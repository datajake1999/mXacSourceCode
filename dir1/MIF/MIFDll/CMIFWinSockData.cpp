/*************************************************************************************
CMIFWinSockData - Simulate WinSock data ports.

begun 28/2/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\buildnum.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "resource.h"


/*
 * WinSock 2 extension -- manifest constants for shutdown()
 */
#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02


static PSTR gpszWinSockClassName = "CircumrealityWinSockSim";
#define WORDALIGN(x)             (((x+1)/2)*2)

/************************************************************************************
CCircumrealityWinSockData::Constructor and destructor
*/
CCircumrealityWinSockData::CCircumrealityWinSockData (void)
{
   m_dwLag = 0;
   m_dwBaud = 28000; // typical modem
   m_hWnd = m_hWndSendTo = NULL;
   m_iErrFirst = m_iErrLast = 0;
   m_pszErrFirst = m_pszErrLast = NULL;
   m_dwSendPeriod = 25;

   m_dwLastDataSent = 0;

   // BUGFIX - increase data flow
   m_dwLag = 0;   // was 250
   m_dwBaud *= 1000;

   m_dwSendSize = (m_dwLag+500) * (m_dwBaud/8) / 1000;
   m_dwMaxChanBufSize = WORDALIGN(m_dwSendSize / 2);
   m_dwMaxChanBufSize = min(m_dwMaxChanBufSize, 0x4000); // never too large that will cause word problems
      // BUGFIX - m_dwMaxChanBufSize because way user PeekMessage() and timer-delay
      // in thread-queues, this needs to be larger
   m_lSendAt.Init (sizeof(DWORD));

   m_iSocket = INVALID_SOCKET;
}

CCircumrealityWinSockData::~CCircumrealityWinSockData (void)
{
   int iRet;
   if (m_iSocket != INVALID_SOCKET) {
      iRet = shutdown (m_iSocket, SD_BOTH);
      iRet = closesocket (m_iSocket);
   }

   if (m_hWnd)
      DestroyWindow (m_hWnd);
}

/************************************************************************************
WinSockWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK WinSockWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCCircumrealityWinSockData p = (PCCircumrealityWinSockData) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCCircumrealityWinSockData p = (PCCircumrealityWinSockData) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCCircumrealityWinSockData) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/************************************************************************************
CCircumrealityWinSockData::Init - Call this to initialze the object to act as either
a server socket, or a client socket.

inputs
   SOCKET         iSocket - This is the socket to use, or INVALID_SOCKET if no
                  socket and use the window name instead. NOTE: This socket
                  will be freed when the object is deleted
   char           *pszNameThis - Window name to use for the end of the socket.
                     Only used if iSocket == INVALID_SOCKET
   char           *pszNameServer - Name for the socket to connect to. If this is NULL
                     then this socket will merely sit around and wait for a connection
                     to happen. If the server doesn't exist this will error out
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityWinSockData::Init (SOCKET iSocket, char *pszNameThis, char *pszNameServer)
{
   if (iSocket != INVALID_SOCKET) {
      m_iSocket = iSocket;

      // BUGFIX - default send/receive buffers neeed to be enlarged so that can uploade/download
      // voices and models quicly
      // NOTE: Not sure if this is really working because doesn't seem to make things faster
      int iSize = 0;
      int iRequired, iRet;

      iRequired = sizeof(int);
      if (!getsockopt (iSocket, SOL_SOCKET, SO_RCVBUF, (char*) &iSize, &iRequired)) {
         iSize = max(iSize, 0x10000);
         iRet = setsockopt (iSocket, SOL_SOCKET, SO_RCVBUF, (char*) &iSize, sizeof(iSize));

#ifdef _DEBUG
         iRequired = sizeof(int);
         iSize = 0;
         getsockopt (iSocket, SOL_SOCKET, SO_RCVBUF, (char*) &iSize, &iRequired);
#endif
      }

      iRequired = sizeof(int);
      if (!getsockopt (iSocket, SOL_SOCKET, SO_SNDBUF, (char*) &iSize, &iRequired)) {
         iSize = max(iSize, 0x10000);
         iRet = setsockopt (iSocket, SOL_SOCKET, SO_SNDBUF, (char*) &iSize, sizeof(iSize));

#ifdef _DEBUG
         iRequired = sizeof(int);
         iSize = 0;
         getsockopt (iSocket, SOL_SOCKET, SO_SNDBUF, (char*) &iSize, &iRequired);
#endif
      }


      // receive timeout
      DWORD dwVal;
      iRequired = sizeof(dwVal);
      if (!getsockopt (iSocket, SOL_SOCKET, SO_RCVTIMEO, (char*) &dwVal, &iRequired)) {
         dwVal = max(dwVal, WINSOCK_SENDRECEIVETIMEOUT);
         iRet = setsockopt (iSocket, SOL_SOCKET, SO_RCVTIMEO, (char*) &dwVal, sizeof(dwVal));

#ifdef _DEBUG
         iRequired = sizeof(dwVal);
         dwVal = 0;
         getsockopt (iSocket, SOL_SOCKET, SO_RCVTIMEO, (char*) &dwVal, &iRequired);
#endif
      }


      // send timeout
      iRequired = sizeof(dwVal);
      if (!getsockopt (iSocket, SOL_SOCKET, SO_SNDTIMEO, (char*) &dwVal, &iRequired)) {
         dwVal = max(dwVal, WINSOCK_SENDRECEIVETIMEOUT);
         iRet = setsockopt (iSocket, SOL_SOCKET, SO_SNDTIMEO, (char*) &dwVal, sizeof(dwVal));

#ifdef _DEBUG
         iRequired = sizeof(dwVal);
         dwVal = 0;
         getsockopt (iSocket, SOL_SOCKET, SO_SNDTIMEO, (char*) &dwVal, &iRequired);
#endif
      }

      // diable nagle aglorthm
      BOOL fBool;
      iRequired = sizeof(fBool);
      if (!getsockopt (iSocket, SOL_SOCKET, TCP_NODELAY, (char*) &fBool, &iRequired)) {
         fBool = TRUE;
         iRet = setsockopt (iSocket, SOL_SOCKET, TCP_NODELAY, (char*) &fBool, sizeof(fBool));

#ifdef _DEBUG
         iRequired = sizeof(fBool);
         fBool = 82;
         getsockopt (iSocket, SOL_SOCKET, TCP_NODELAY, (char*) &fBool, &iRequired);
#endif
      }

      return TRUE;
   }

   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = WinSockWndProc;
   wc.lpszClassName = gpszWinSockClassName;
   RegisterClass (&wc);

   m_hWnd = CreateWindow (
      gpszWinSockClassName, pszNameThis,
      0,
      0,0,0,0,
      NULL, NULL, ghInstance, (PVOID) this);

   // if not looking for a server then done
   if (!pszNameServer)
      return TRUE;

   // else, try to connect
   m_hWndSendTo = FindWindow (gpszWinSockClassName, pszNameServer);
   if (!m_hWndSendTo)
      return FALSE;

   // done
   return TRUE;
}



/************************************************************************************
CCircumrealityWinSockData::WindowGet - Gets the window used by the winsock data.

returns
   HWND - This window
*/
HWND CCircumrealityWinSockData::WindowGet (void)
{
   if (m_iSocket != INVALID_SOCKET)
      return NULL;

   return m_hWnd;
}

/************************************************************************************
CCircumrealityWinSockData::WindowSendToSet - Sets the window that will be sending to.

inputs
   HWND     hWnd - Send to
returns
   none
*/
void CCircumrealityWinSockData::WindowSentToSet (HWND hWnd)
{
   if (m_iSocket != INVALID_SOCKET)
      return;

   m_hWndSendTo = hWnd;
}



/************************************************************************************
CCircumrealityWinSockData::WndProc - Handles window proc for the code. This ends up creating
a timer that sends/receives data, simulating winsock
*/
LRESULT CCircumrealityWinSockData::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      SetTimer (hWnd, 42, m_dwSendPeriod, NULL);
      return 0;

   case WM_DESTROY:
      KillTimer (hWnd, 42);
      break;

   case WM_TIMER:
      {
         // send some data
         if (!m_lSend.Num() || !m_hWndSendTo)
            return 0;

         if (!IsWindow (m_hWndSendTo)) {
            // called if the destination window disappears
            m_iErrLast = WSAESHUTDOWN;
            m_pszErrLast = L"WndProc::!IsWindow";

            if (!m_iErrFirst) {
               m_iErrFirst = m_iErrLast;
               m_pszErrFirst = m_pszErrLast;
            }
            return 0;
         }

         DWORD dwTime = GetTickCount();
         DWORD *pdw;
         while (m_lSend.Num()) {
            pdw = (DWORD*)m_lSendAt.Get(0);
            if (*pdw > dwTime)
               break;   // dont send this yet

            // else, send it
            COPYDATASTRUCT cds;
            cds.cbData = (DWORD) m_lSend.Size(0);
            cds.dwData = 42;
            cds.lpData = m_lSend.Get(0);
            SendMessage (m_hWndSendTo, WM_COPYDATA, (WPARAM)hWnd,
               (LPARAM)&cds);

            // delete data
            m_lSend.Remove (0);
            m_lSendAt.Remove (0);
         }
      }
      break;

   case WM_COPYDATA:
      {
         // incoming data
         PCOPYDATASTRUCT pcs = (PCOPYDATASTRUCT) lParam;
         if (pcs->dwData != 42)
            return 0;   // error

         if (m_hWndSendTo == NULL)
            m_hWndSendTo = (HWND)wParam;

         if (!m_memReceive.Required (m_memReceive.m_dwCurPosn + pcs->cbData))
            return TRUE;   // error
         memcpy ((PBYTE)m_memReceive.p + m_memReceive.m_dwCurPosn,
            pcs->lpData, pcs->cbData);
         m_memReceive.m_dwCurPosn += pcs->cbData;
      }
      return TRUE;
   }

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/************************************************************************************
CCircumrealityWinSockData::Receive - Reads data thats queued up from the remove machine.

inputs
   PVOID          pData - To fill in
   DWORD          dwSize - Bytes available
returns
   int - Number of bytes actually filled in. SOCKET_ERROR if an error has occured,
      in which case GetLastError() should be called for the error
*/
int CCircumrealityWinSockData::Receive (PVOID pData, DWORD dwSize)
{
   if (m_iSocket != INVALID_SOCKET) {
      int iRet = recv (m_iSocket, (char*)pData, (int)dwSize, 0);
      if (iRet == SOCKET_ERROR) {
         m_iErrLast = WSAGetLastError ();
         m_pszErrLast = L"Receive2::recv()";

         // if begnin error then ignore
         if (m_iErrLast == WSAEWOULDBLOCK) {
            m_iErrLast = 0;
            m_pszErrLast = NULL;
            return 0;
         }

         if (!m_iErrFirst) {
            m_iErrFirst = m_iErrLast;
            m_pszErrFirst = m_pszErrLast;
         }
      }
      else {
         m_iErrLast = 0; // since no error
         m_pszErrLast = NULL;
      }
      return iRet;
   }


   // if error then fail
   if (m_iErrLast)
      return SOCKET_ERROR;

   // get data
   size_t dwData = min(m_memReceive.m_dwCurPosn, dwSize);
   if (!dwData)
      return 0;   // fast
   memcpy (pData, m_memReceive.p, dwData);
   memmove (m_memReceive.p, (PBYTE)m_memReceive.p + dwData, m_memReceive.m_dwCurPosn - dwData);
   m_memReceive.m_dwCurPosn -= dwData;
   return (int) dwData;
}



/************************************************************************************
CCircumrealityWinSockData::Send - Sends data to winsock.

inputs
   PVOID             pData - Data to send
   DWORD             dwSize - Size of data
returns
   int - Number of bytes of data actually send, or SOCKET_ERROR if an error has occured.
      Often times not as much data will be sent as is provided
*/
int CCircumrealityWinSockData::Send (PVOID pData, DWORD dwSize)
{
   if (m_iSocket != INVALID_SOCKET) {
      int iRet = send (m_iSocket, (char*)pData, (int)dwSize, 0);
      if (iRet == SOCKET_ERROR) {
         m_iErrLast = WSAGetLastError ();
         m_pszErrLast = L"Send::send()";

         // if begnin error then ignore
         if (m_iErrLast == WSAEWOULDBLOCK) {
            m_iErrLast = 0;
            m_pszErrLast = NULL;
            return 0;
         }

         if (!m_iErrFirst) {
            m_iErrFirst = m_iErrLast;
            m_pszErrFirst = m_pszErrLast;
         }

      }
      else {
         m_iErrLast = 0; // since no error
         m_pszErrLast = NULL;
      }
      return iRet;
   }

   // BUGFIX - Cant really send until receive a ping back from main server, so if
   // m_hWndSendTo is still null then just return 0
   if (!m_hWndSendTo)
      return 0;

   if (!IsWindow (m_hWndSendTo)) {
      // send-to window no longer exists
      m_iErrLast = WSAESHUTDOWN;
      m_pszErrLast = L"Send::!IsWindow";

      if (!m_iErrFirst) {
         m_iErrFirst = m_iErrLast;
         m_pszErrFirst = m_pszErrLast;
      }

      return SOCKET_ERROR;
   }

   // figure out how much data already have
   DWORD dwHave;
   DWORD i;
   dwHave = 0;
   for (i = 0; i < m_lSend.Num(); i++)
      dwHave += (DWORD)m_lSend.Size(i);
   if (dwHave >= m_dwSendSize)
      return 0;   // cand send anymore
   DWORD dwSend = m_dwSendSize - dwHave;
   if (dwSize > dwSend)
      dwSize = dwSend;
   if (!dwSize)
      return 0;   // nothing sent

   // how much data in a block (send ever 100 ms)
   DWORD dwBlock = m_dwBaud / 8 * m_dwSendPeriod / 1000;
   dwBlock = max(dwBlock, 1);

   // while have data send it out
   DWORD dwTime = GetTickCount();
   DWORD dwSent = 0;
   while (dwSize) {
      // how much in this one...
      DWORD dwThis = min(dwSize, dwBlock);

      // add to queue
      m_lSend.Add (pData, dwThis);

      // figure out time from that
      DWORD dwWhen, dwSendTime;
      dwWhen = max(m_dwLastDataSent, dwTime);
      dwSendTime = dwThis * 1000 / (m_dwBaud / 8) + 1;
      dwWhen += dwSendTime;
      m_dwLastDataSent = dwWhen;
      dwWhen += m_dwLag;   // so take into account lag
      m_lSendAt.Add (&dwWhen);


      // note that sent
      dwSize -= dwThis;
      dwSent += dwThis;
      pData = (PVOID)((PBYTE)pData + dwThis);
   }

   return (int)dwSent;
}


/************************************************************************************
CCircumrealityWinSockData::GetLastError - Returns the last error reported by winsock data

inputs
   PCWSTR      *ppszErr - Filled in with the error
*/
int CCircumrealityWinSockData::GetLastError (PCWSTR *ppszErr)
{
   *ppszErr = m_pszErrLast;
   return m_iErrLast;
}


/************************************************************************************
CCircumrealityWinSockData::GetFirstError - Returns the First error reported by winsock data

inputs
   PCWSTR      *ppszErr - Filled in with the error
*/
int CCircumrealityWinSockData::GetFirstError (PCWSTR *ppszErr)
{
   *ppszErr = m_pszErrFirst;
   return m_iErrFirst;
}

// BUGBUG - make sure that don't send/receive any more than 10 kb/sec from a client
// so they don't hog all resources