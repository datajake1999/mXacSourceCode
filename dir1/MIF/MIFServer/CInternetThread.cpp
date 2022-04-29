/*************************************************************************************
CInternetThread.cpp - Code for managing the internet communications thread.

begun 29/2/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

// #define USEIPV6      // BUGFIX - Need this to include winsock2

#ifdef USEIPV6
#include <winsock2.h>
#endif

#include <windows.h>
//#include <winsock2.h>
#include <objbase.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifserver.h"
#include "resource.h"


#ifdef USEIPV6
#include <Ws2tcpip.h>
#endif


#ifndef USEIPV6
/*
 * WinSock 2 extension -- manifest constants for shutdown()
 */
#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02
#endif


#define MAXRECEIVESIZE        8000000  // won't receive any more than 1 megabyte of data
   // BUGFIX - Upped to 8 meg of data so that can receive large images

#define LISTENBACKLOG         32       // backlog of connections
#define MAXUSERSLOGGEDON      2045     // maximum users logged onto the system
         // BUGFIX - Was 1024, but increase since will put protection elsewhere too

// CIRCUMREALITYCONNECT - info about connection
typedef struct {
   DWORD             dwID;       // ID
   PCCircumrealityPacket       pPacket;    // socket to use
} CIRCUMREALITYCONNECT, *PCIRCUMREALITYCONNECT;

// CONREMOVE - Connection remove information
typedef struct {
   DWORD             dwID;       // ID of connection to remove
   DWORD             dwTicks;    // number of ticks (loops of threadproc) before can remove
} CONREMOVE, *PCONREMOVE;

// COMPTHREADINFO - for compressor thread proc
typedef struct {
   DWORD             dwThread;   // thread number
   PCInternetThread  pIT;        // internet thread
} COMPTHREADINFO, *PCOMPTHREADINFO;

// CONNECTERR - Store connection error information
typedef struct {
   DWORD             dwID;       // ID for connection
   int               iErr;       // error number returned
   WCHAR             szErr[256]; // error
} CONNECTERR, *PCONNECTERR;

/*************************************************************************************
CInternetThread::Constructor and destructor
*/
CInternetThread::CInternetThread (void)
{
   InitializeCriticalSection (&m_CritSec);

   m_dwCurSocketID = 1;
   m_hCIRCUMREALITYCONNECT.Init (sizeof(CIRCUMREALITYCONNECT));
   m_lConnectNew.Init (sizeof(DWORD));
   m_lConnectMessage.Init (sizeof(DWORD));
   m_lConnectErr.Init (sizeof(CONNECTERR));
   m_lConnectRemove.Init (sizeof(CONREMOVE));
   m_hThread = NULL;
   m_hSignalFromThread = NULL;
   m_hSignalToThread = NULL;
   m_fConnected = FALSE;
   m_szConnectFile[0] = 0;
   m_fConnectRemote = FALSE;
   m_dwConnectPort = 0;
   m_hWndNotify = NULL;
   m_dwMessage = NULL;
   m_hWnd = NULL;
   m_dwConnectCanDelete = 0;
   m_dwMaxConnections = MAXUSERSLOGGEDON;

   m_iNetworkSent = m_iNetworkReceived = 0;

   m_dwCompNum = HowManyProcessors();
   m_dwCompNum = max (1, m_dwCompNum);
   InitializeCriticalSection (&m_CritSecComp);
   m_lPCCircumrealityPacketComp.Init (sizeof(PCCircumrealityPacket));
   memset (&m_aPCCircumrealityPacketComp, 0, sizeof(m_aPCCircumrealityPacketComp));
   memset (&m_ahSignalToThreadComp, 0, sizeof(m_ahSignalToThreadComp));
   memset (&m_ahSignalFromThreadComp, 0, sizeof(m_ahSignalFromThreadComp));
   memset (&m_afNotifyWhenDoneComp, 0, sizeof(m_afNotifyWhenDoneComp));
   memset (&m_ahThreadComp, 0, sizeof(m_ahThreadComp));
   m_fWantToQuitComp = FALSE;

   InitializeCriticalSection (&m_CritSecIPBLACKLIST);
   m_lIPBLACKLIST.Init (sizeof(IPBLACKLIST));
}

CInternetThread::~CInternetThread (void)
{
   if (m_hThread)
      Disconnect();

   // the act of disconnecting should free everything up
   DeleteCriticalSection (&m_CritSec);
   DeleteCriticalSection (&m_CritSecComp);
   DeleteCriticalSection (&m_CritSecIPBLACKLIST);
}


/*************************************************************************************
InternetThreadProc - Thread that handles the internet.
*/
DWORD WINAPI InternetThreadProc(LPVOID lpParameter)
{
   PCInternetThread pThread = (PCInternetThread) lpParameter;

   pThread->ThreadProc ();

   return 0;
}

/*************************************************************************************
CompressorThreadProc - Thread that handles the internet.
*/
DWORD WINAPI CompressorThreadProc(LPVOID lpParameter)
{
   PCOMPTHREADINFO p = (PCOMPTHREADINFO)lpParameter;
   p->pIT->CompressorThreadProc (p->dwThread);

   return 0;
}



/************************************************************************************
ServerRequestWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK ServerRequestWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCInternetThread p = (PCInternetThread) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCInternetThread p = (PCInternetThread) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCInternetThread) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CInternetThread::WaitUntilSafeToDeletePacket - Makes sure its safe to delete
a packet and that the compressor threads dont have their hands on it

NOTE: This tells the packet not to add itself to the list anymore.

inputs
   PCCircumrealityPacket       pPacket - Packet to check
*/
void CInternetThread::WaitUntilSafeToDeletePacket (PCCircumrealityPacket pPacket)
{
   pPacket->CompressInSeparateThread (&m_CritSecComp, NULL, NULL);

retry:
   EnterCriticalSection (&m_CritSecComp);

   DWORD i;
   // loop through the queue of packets and remove a match
   PCCircumrealityPacket *ppp = (PCCircumrealityPacket*)m_lPCCircumrealityPacketComp.Get(0);
   for (i = m_lPCCircumrealityPacketComp.Num()-1; i < m_lPCCircumrealityPacketComp.Num(); i--)
      if (ppp[i] == pPacket) {
         // found, so remove
         m_lPCCircumrealityPacketComp.Remove (i);
         ppp = (PCCircumrealityPacket*)m_lPCCircumrealityPacketComp.Get(0);
      }

   // make sure it's not being worked on now
   for (i = 0; i < m_dwCompNum; i++)
      if (m_aPCCircumrealityPacketComp[i] == pPacket) {
         // being worked on
         m_afNotifyWhenDoneComp[i] = TRUE;
         SetEvent (m_ahSignalToThreadComp[i]);
         LeaveCriticalSection (&m_CritSecComp);

         WaitForSingleObject (m_ahSignalFromThreadComp[i], INFINITE);

         goto retry;
      }

   // if gets all the way here then packet no longer on list, so all clear
   LeaveCriticalSection (&m_CritSecComp);
}

/*************************************************************************************
CInternetThread::WndProc - Manages the window calls for the server window.
This basically accepts data requests and creates new connections.

BUGBUG - At some point will want to limit number of connectison (through
internet and/or window) based on registration key
*/
LRESULT CInternetThread::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_COPYDATA:
      {
         // incoming data... message 42 => new connection. Data is the hWnd
         // to send to
         PCOPYDATASTRUCT pcs = (PCOPYDATASTRUCT) lParam;
         if ((pcs->dwData != 42) || (pcs->cbData != sizeof(HWND)))
            return 0;   // error
         HWND hWndSend = *((HWND*)pcs->lpData);

         // create the socket
         CIRCUMREALITYCONNECT mc;
         memset (&mc, 0 ,sizeof(mc));
         mc.pPacket = new CCircumrealityPacket;
         if (!mc.pPacket)
            return TRUE;   // error
         mc.pPacket->m_iMaxQueuedReceive = MAXRECEIVESIZE;
         mc.pPacket->CompressInSeparateThread (&m_CritSecComp, &m_lPCCircumrealityPacketComp, &m_ahSignalToThreadComp[0]);
         if (!mc.pPacket->Init (SOCKET_ERROR, "ServerPacket")) {
            delete mc.pPacket;
            return TRUE;
         }
         mc.pPacket->WindowSentToSet (hWndSend);

         // add to list
         EnterCriticalSection (&m_CritSec);
         DWORD dwNumConnect = m_hCIRCUMREALITYCONNECT.Num();
         mc.dwID = m_dwCurSocketID;
         m_hCIRCUMREALITYCONNECT.Add (mc.dwID, &mc);
         m_lConnectNew.Add (&mc.dwID);  // so can send back that new one added
         m_dwCurSocketID++;
         LeaveCriticalSection (&m_CritSec);

         // send a version number to client's window so knows to talk back
         DWORD dwVersion = CLIENTVERSIONNEED;
         mc.pPacket->PacketSend (CIRCUMREALITYPACKET_CLIENTVERSIONNEED, &dwVersion, sizeof(dwVersion));

         // send the server load
         CIRCUMREALITYSERVERLOAD sl;
         memset (&sl, 0, sizeof(sl));
         sl.dwConnections = dwNumConnect;
         sl.dwMaxConnections = m_dwMaxConnections;
         mc.pPacket->PacketSend (CIRCUMREALITYPACKET_SERVERLOAD, &sl, sizeof(sl));

         // tell main app that should check for new stuff
         if (m_hWndNotify)
            PostMessage (m_hWndNotify, m_dwMessage, 0, 0);
      }
      return TRUE;
   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CInternetThread::CompressorThreadProc - This internal function handles the thread procedure.
It first creates the packet sending object and tries to log on. If it fails
it sets m_fConnected to FALSE and sets a flag. If it succedes it sets m_fConnected
to true, and loops until m_hSignalToThread is set.
*/
void CInternetThread::CompressorThreadProc (DWORD dwThread)
{
   while (TRUE) {
      EnterCriticalSection (&m_CritSecComp);

      // if app wants a signal then do that
      if (m_afNotifyWhenDoneComp[dwThread]) {
         m_afNotifyWhenDoneComp[dwThread] = FALSE;
         SetEvent (m_ahSignalFromThreadComp[dwThread]);
      }

      // if want to shut down then exit
      if (m_fWantToQuitComp) {
         LeaveCriticalSection (&m_CritSecComp);
         return;
      }

      // if nothing in the buffer then wait
      if (!m_lPCCircumrealityPacketComp.Num()) {
         LeaveCriticalSection (&m_CritSecComp);
         WaitForSingleObject (m_ahSignalToThreadComp[dwThread], INFINITE);
         continue;
      }

      // else, take first item
      m_aPCCircumrealityPacketComp[dwThread] = *((PCCircumrealityPacket*)m_lPCCircumrealityPacketComp.Get(0));
      m_lPCCircumrealityPacketComp.Remove (0);
      LeaveCriticalSection (&m_CritSecComp);

      // compress this
      m_aPCCircumrealityPacketComp[dwThread]->CompCompressAndSend (FALSE);

      // done
      EnterCriticalSection (&m_CritSecComp);
      m_aPCCircumrealityPacketComp[dwThread] = NULL;

      // if app wants a signal then do that
      if (m_afNotifyWhenDoneComp[dwThread]) {
         m_afNotifyWhenDoneComp[dwThread] = FALSE;
         SetEvent (m_ahSignalFromThreadComp[dwThread]);
      }

      LeaveCriticalSection (&m_CritSecComp);
   } // while TRUE
}


/*************************************************************************************
CInternetThread::ThreadProc - This internal function handles the thread procedure.
It first creates the packet sending object and tries to log on. If it fails
it sets m_fConnected to FALSE and sets a flag. If it succedes it sets m_fConnected
to true, and loops until m_hSignalToThread is set.
*/
void CInternetThread::ThreadProc (void)
{
   m_fConnected = FALSE;

   // try to connect with winsock
   m_iSocket = INVALID_SOCKET;
   memset (&m_qwConnectIP, 0, sizeof(m_qwConnectIP));
   int iRet;
   if (m_fConnectRemote) {
      WSADATA wsData;
      m_iWinSockErr = WSAStartup (MAKEWORD( 2, 2 ), &wsData);
      if (m_iWinSockErr) {
         m_fConnected = FALSE;
         SetEvent (m_hSignalFromThread);
         return;
      }

      char szTemp[256];
      szTemp[0] = 0;
      gethostname (szTemp, sizeof(szTemp));

#ifdef USEIPV6
      ADDRINFO *AI;
      ADDRINFO hints;
      memset (&hints, 0, sizeof(hints));
      hints.ai_flags = AI_PASSIVE;
      char szPortNumber[64];
      sprintf (szPortNumber, "%d", (int)m_dwConnectPort);   // always use port 25
      int iError = getaddrinfo(szTemp, szPortNumber, &hints, &AI);
      if (iError ) {
         m_iWinSockErr = WSAGetLastError ();
         WSACleanup ();
         m_fConnected = FALSE;
         SetEvent (m_hSignalFromThread);
         return;
      }
      memset (&m_qwConnectIP, 0, sizeof(m_qwConnectIP));
      if (AI->ai_addrlen <= sizeof(m_qwConnectIP))
         memcpy (&m_qwConnectIP, AI->ai_addr, AI->ai_addrlen);
#else // !USEIPV6

      struct hostent *host;
      host = gethostbyname (szTemp);
      if (!host) {
         m_iWinSockErr = WSAGetLastError ();
         WSACleanup ();
         m_fConnected = FALSE;
         SetEvent (m_hSignalFromThread);
         return;
      }
      m_qwConnectIP = *((DWORD*)host->h_addr_list[0]);
#endif


#ifdef USEIPV6
      m_iSocket = socket (AI->ai_family, SOCK_STREAM, IPPROTO_TCP);
#else
      m_iSocket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
      if (m_iSocket == INVALID_SOCKET) {
         m_iWinSockErr = WSAGetLastError ();
         WSACleanup ();
         m_fConnected = FALSE;
         SetEvent (m_hSignalFromThread);
         return;
      }

      // make sure can re-use
      int iVal = 1;
      iRet = setsockopt (m_iSocket, SOL_SOCKET, SO_REUSEADDR, (char*) &iVal, sizeof(iVal));

      // set socket as non blocking
      DWORD dwBlock = TRUE;
      iRet = ioctlsocket (m_iSocket, FIONBIO, &dwBlock );

      // BUGFIX - So doesn't "hang up" on user too quickly
      // receive timeout
      int iRequired;
      DWORD dwVal;
      iRequired = sizeof(dwVal);
      if (!getsockopt (m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char*) &dwVal, &iRequired)) {
         dwVal = max(dwVal, WINSOCK_SENDRECEIVETIMEOUT);
         iRet = setsockopt (m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char*) &dwVal, sizeof(dwVal));

#ifdef _DEBUG
         iRequired = sizeof(dwVal);
         dwVal = 0;
         getsockopt (m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char*) &dwVal, &iRequired);
#endif
      }


      // send timeout
      iRequired = sizeof(dwVal);
      if (!getsockopt (m_iSocket, SOL_SOCKET, SO_SNDTIMEO, (char*) &dwVal, &iRequired)) {
         dwVal = max(dwVal, WINSOCK_SENDRECEIVETIMEOUT);
         iRet = setsockopt (m_iSocket, SOL_SOCKET, SO_SNDTIMEO, (char*) &dwVal, sizeof(dwVal));

#ifdef _DEBUG
         iRequired = sizeof(dwVal);
         dwVal = 0;
         getsockopt (m_iSocket, SOL_SOCKET, SO_SNDTIMEO, (char*) &dwVal, &iRequired);
#endif
      }

#ifdef USEIPV6
      // SOCKADDR_STORAGE sa;
      // memset (&sa, 0, sizeof(sa));

      if (bind (m_iSocket, AI->ai_addr, (int) AI->ai_addrlen)) {
#else
      struct sockaddr_in sa;
      memset (&sa, 0, sizeof(sa));
      sa.sin_family = AF_INET;
      sa.sin_port = htons ((WORD)m_dwConnectPort);
      sa.sin_addr.s_addr = htonl (INADDR_ANY); //m_dwConnectIP; // htonl (INADDR_ANY);
         // BUGFIX - Try changing back to INADD_ANY since don't seem to work remotely
      if (bind (m_iSocket, (struct sockaddr*)&sa, sizeof(sa))) {
#endif
         m_iWinSockErr = WSAGetLastError ();
         shutdown (m_iSocket, SD_BOTH);
         closesocket (m_iSocket);
         WSACleanup ();

         m_fConnected = FALSE;
         SetEvent (m_hSignalFromThread);
         return;
      }

      // listen
      if (listen (m_iSocket, LISTENBACKLOG)) {
         m_iWinSockErr = WSAGetLastError ();
         shutdown (m_iSocket, SD_BOTH);
         closesocket (m_iSocket);
         WSACleanup ();

         m_fConnected = FALSE;
         SetEvent (m_hSignalFromThread);
         return;
      }

      // get the address
#ifdef USEIPV6
      memset (&m_qwConnectIP, 0, sizeof(m_qwConnectIP));
      if (AI->ai_addrlen <= sizeof(m_qwConnectIP))
         memcpy (&m_qwConnectIP, AI->ai_addr, AI->ai_addrlen);
#else
      sa.sin_family = AF_INET;
      int iNameLen = sizeof(sa);
      if (!getsockname (m_iSocket, (struct sockaddr*)&sa, &iNameLen))
         m_qwConnectIP = (sa.sin_addr.s_addr == htonl (INADDR_ANY)) ? m_qwConnectIP : sa.sin_addr.s_addr;
      else
         m_qwConnectIP = 0;   // unknown
#endif
   }

   // create a window for the server's "request connect"...
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = ServerRequestWndProc;
   wc.lpszClassName = "CircumrealityServerRequest"; // NOTE: Server class
   RegisterClass (&wc);
   char szaConnectFile[512];
   WideCharToMultiByte (CP_ACP, 0, m_szConnectFile, -1, szaConnectFile, sizeof(szaConnectFile), 0,0);
   m_hWnd = CreateWindow (
      wc.lpszClassName, szaConnectFile,  // NOTE: Server name
      0, 0,0,0,0,
      NULL, NULL, ghInstance, (PVOID) this);
   if (!m_hWnd) {
      if (m_fConnectRemote) {
         shutdown (m_iSocket, SD_BOTH);
         closesocket (m_iSocket);
         WSACleanup ();
      }

      m_fConnected = FALSE;
      SetEvent (m_hSignalFromThread);
      return;
   }

   // else, it went through...
   m_fConnected = TRUE;
   SetEvent (m_hSignalFromThread);

   // get the time, in millseconds
   LARGE_INTEGER     liPerCountFreq, liCount;
   __int64  iTime;
   QueryPerformanceFrequency (&liPerCountFreq);

   // create all the compressor threads
   DWORD i, dwID;
   COMPTHREADINFO aCTI[MAXRAYTHREAD];
   for (i = 0; i < m_dwCompNum; i++) {
      m_ahSignalToThreadComp[i] = CreateEvent (NULL, FALSE, FALSE, NULL);
      m_ahSignalFromThreadComp[i] = CreateEvent (NULL, FALSE, FALSE, NULL);
      aCTI[i].dwThread = i;
      aCTI[i].pIT = this;
      m_ahThreadComp[i] = CreateThread (NULL, 0, ::CompressorThreadProc, &aCTI[i], 0, &dwID);
         // NOTE: Not using ESCTHREADCOMMITSIZE because want to be as stable as possible
         // assume created

      // make sure has decent thread priority so doesn't fall behing
      SetThreadPriority (m_ahThreadComp[i], VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));
   } // i

   // wait, either taking message from the queue, or an event, or doing processing
   MSG msg;
   BOOL fIncoming;
   timeval tv;
   memset (&tv, 0, sizeof(tv));  // so don't wait any time
   struct fd_set WinSockSet;
#ifdef USEIPV6
   SOCKADDR_STORAGE WinSockAddr;
   struct sockaddr_in6 *pWSA = (struct sockaddr_in6*) &WinSockAddr;
#else
   struct sockaddr_in WinSockAddr;
#endif
   while (TRUE) {
      // handle message queue
      while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
         TranslateMessage (&msg);
         DispatchMessage(&msg);
      }

      // get the time in milliseconds
      QueryPerformanceCounter (&liCount);
      iTime = *((__int64*)&liCount) / ( *((__int64*)&liPerCountFreq) / 1000);

      // loop through all the existing messages and see if anything
      EnterCriticalSection (&m_CritSec);

      // BUGFIX - remove any connections on the to-remove list
      PCONREMOVE pacr = (PCONREMOVE)m_lConnectRemove.Get(0);
      for (i = m_lConnectRemove.Num()-1; i < m_lConnectRemove.Num(); i--) {
         if (pacr[i].dwTicks)
            pacr[i].dwTicks--;
         if (pacr[i].dwTicks)
            continue;   // not ready to delete

         if (m_dwConnectCanDelete)
            continue;   // cant actually delete at this moment because
                        // connectionget was called

         DWORD dwIndex = m_hCIRCUMREALITYCONNECT.FindIndex (pacr[i].dwID);
         if (dwIndex == -1)
            continue; // no longer there
         PCIRCUMREALITYCONNECT pmc = (PCIRCUMREALITYCONNECT)m_hCIRCUMREALITYCONNECT.Get (dwIndex);
         if (!pmc)
            continue; // no longer there

         WaitUntilSafeToDeletePacket (pmc->pPacket);
         delete pmc->pPacket;
         m_hCIRCUMREALITYCONNECT.Remove (dwIndex);

         // remove from to-remove list
         m_lConnectRemove.Remove (i);
         pacr = (PCONREMOVE)m_lConnectRemove.Get(0);
      } // i, remove


      // BUGBUG - if there are too many users logged in, this just ignores
      // the selection. At some point if too many users logged on, connect
      // them and send message that no more allowed?

      // see if there are any new connection requests, etc.
      if (m_fConnectRemote) while (m_hCIRCUMREALITYCONNECT.Num() < m_dwMaxConnections) {
               // NOTE: Don't care about thread protection for m_dwMaxConnections
         FD_ZERO (&WinSockSet);
         FD_SET (m_iSocket, &WinSockSet);
         if (select (0, &WinSockSet, 0, 0, &tv) <= 0)
            break;
         if (!FD_ISSET(m_iSocket, &WinSockSet))
            break;

         iRet = sizeof(WinSockAddr);
         SOCKET sNew = accept (m_iSocket, (sockaddr*) &WinSockAddr, &iRet);
         if (sNew == INVALID_SOCKET)
            break;   // not valid

         // if it's blacklisted then close right away
         EnterCriticalSection (&m_CritSecIPBLACKLIST);
         BOOL fBlocked = FALSE;
         PIPBLACKLIST pIBL = (PIPBLACKLIST) m_lIPBLACKLIST.Get(0);
         for (i = 0; i < m_lIPBLACKLIST.Num(); i++, pIBL++) {
#ifdef USEIPV6
            if (memcmp(&pWSA->sin6_addr, &pIBL->iAddr, sizeof(pWSA->sin6_addr)))
#else
            if (WinSockAddr.sin_addr.s_addr != pIBL->iAddr)
#endif
               continue;   // no match

            // else, match
            if (pIBL->fScore < 1.0)
               continue;   // skip, since not fully blocked

            // BUGFIX - If this IP has been temporarily blacklisted long enough then
            // un-ban
            FILETIME ft;
            __int64 iTime;
            GetSystemTimeAsFileTime (&ft);
            iTime = *((__int64*)&ft);
            if (iTime >= pIBL->iFileTime) {
               pIBL->fScore = 0.0;  // reset the score to 0
               continue;
            }

            // else, blocked
            fBlocked = TRUE;
            break;
         } // i
         LeaveCriticalSection (&m_CritSecIPBLACKLIST);
         if (fBlocked) {
            iRet = shutdown (sNew, SD_BOTH);
            iRet = closesocket (sNew);
            continue;
         }

         // create the new connetion
         // create the socket
         CIRCUMREALITYCONNECT mc;
         memset (&mc, 0 ,sizeof(mc));
         mc.pPacket = new CCircumrealityPacket;
         if (!mc.pPacket) {
            shutdown (m_iSocket, SD_BOTH);
            closesocket (m_iSocket);
            break;   // error
         }
         mc.pPacket->m_iMaxQueuedReceive = MAXRECEIVESIZE;
         mc.pPacket->CompressInSeparateThread (&m_CritSecComp, &m_lPCCircumrealityPacketComp, &m_ahSignalToThreadComp[0]);
#ifdef USEIPV6
         sockaddr_in6 si;
         memset (&si, 0, sizeof(si));
         si.sin6_addr = pWSA->sin6_addr;
         si.sin6_family = AF_INET6;
         char szHostIP[256];
         getnameinfo ((const struct sockaddr FAR*) &si, sizeof(si), szHostIP, sizeof(szHostIP) / sizeof(szHostIP[0]), NULL, 0, NI_NUMERICHOST);
            // do this to store away IP address
         if (!mc.pPacket->Init (sNew, szHostIP)) {
#else
         in_addr ia;
         ia.S_un.S_addr = WinSockAddr.sin_addr.s_addr;
            // do this to store away IP address
         if (!mc.pPacket->Init (sNew, inet_ntoa(ia))) {
#endif
            delete mc.pPacket;
            shutdown (m_iSocket, SD_BOTH);
            closesocket (m_iSocket);
            break;
         }

         DWORD dwNumConnect = m_hCIRCUMREALITYCONNECT.Num();
         // add to list
         mc.dwID = m_dwCurSocketID;
         m_hCIRCUMREALITYCONNECT.Add (mc.dwID, &mc);
         m_lConnectNew.Add (&mc.dwID);  // so can send back that new one added
         m_dwCurSocketID++;

         // send a version number to client's window so knows to talk back
         DWORD dwVersion = CLIENTVERSIONNEED;
         mc.pPacket->PacketSend (CIRCUMREALITYPACKET_CLIENTVERSIONNEED, &dwVersion, sizeof(dwVersion));

         // send the server load
         CIRCUMREALITYSERVERLOAD sl;
         memset (&sl, 0, sizeof(sl));
         sl.dwConnections = dwNumConnect;
         sl.dwMaxConnections = m_dwMaxConnections;
         mc.pPacket->PacketSend (CIRCUMREALITYPACKET_SERVERLOAD, &sl, sizeof(sl));

         // tell main app that should check for new stuff
         if (m_hWndNotify)
            PostMessage (m_hWndNotify, m_dwMessage, 0, 0);
      } // if can have more connections
      
      
      BOOL fAnyIncoming = FALSE;
      DWORD dwGroup;
      for (dwGroup = 0; dwGroup < m_hCIRCUMREALITYCONNECT.Num(); dwGroup += FD_SETSIZE) {
         // fill in internet check
         FD_ZERO (&WinSockSet);
         DWORD dwMaxGroup = min(dwGroup + FD_SETSIZE, m_hCIRCUMREALITYCONNECT.Num());
         SOCKET iSockThis;
         for (i = dwGroup; i < dwMaxGroup; i++) {
            PCIRCUMREALITYCONNECT pmc = (PCIRCUMREALITYCONNECT)m_hCIRCUMREALITYCONNECT.Get(i);
            iSockThis = pmc->pPacket->SocketGet();
            if (iSockThis == INVALID_SOCKET)
               continue;   // not valid
            FD_SET (iSockThis, &WinSockSet);
         } // i

         if (m_fConnectRemote)
            select (0, &WinSockSet, 0, 0, &tv);

         for (i = 0; i < dwMaxGroup; i++) {
            PCIRCUMREALITYCONNECT pmc = (PCIRCUMREALITYCONNECT)m_hCIRCUMREALITYCONNECT.Get(i);

            // get information about the process to keep timers
            CIRCUMREALITYPACKETINFO infoStart, infoEnd;
            pmc->pPacket->InfoGet(&infoStart);

            // try sending out ata
            // NOTE: because compressing data as a separate block, and have 4 kb blocks,
            // will max out sending 100 * 4 = 400 kbytes/sec to a specific client, and
            // will make sure that all clients serviced equally
            DWORD dwLeft;
            int iSent;
            iSent = pmc->pPacket->CompressedDataToWinsock (FALSE, &dwLeft);
            if ((iSent > 0) || dwLeft) // if any data left to be sent or any send, add back to compress queue
               pmc->pPacket->AddToSeparateThreadQueue();
            if (iSent == SOCKET_ERROR) {
               CONNECTERR ce;
               PCWSTR pszErr = NULL;
               memset (&ce, 0, sizeof(ce));
               ce.dwID = pmc->dwID;
               ce.iErr = pmc->pPacket->GetFirstError (&pszErr);
               if (pszErr && ((wcslen(pszErr)+1)*sizeof(WCHAR) < sizeof(ce.szErr)))
                  wcscpy (ce.szErr, pszErr);

               m_lConnectErr.Add (&ce);  // error
               fAnyIncoming = TRUE;
            }

            // see if any new messages and handle
            int iConnectErr;
            if (iConnectErr = pmc->pPacket->MaintinenceSendIfCan (iTime)) {
               CONNECTERR ce;
               PCWSTR pszErr = NULL;
               memset (&ce, 0, sizeof(ce));
               ce.dwID = pmc->dwID;
               ce.iErr = pmc->pPacket->GetFirstError (&pszErr);
               if (pszErr && ((wcslen(pszErr)+1)*sizeof(WCHAR) < sizeof(ce.szErr)))
                  wcscpy (ce.szErr, pszErr);

               m_lConnectErr.Add (&ce);  // error
               fAnyIncoming = TRUE;
            }
            
            // don't bother receiving if it's an internet connection and
            // this wasn't marked as recievable
            iSockThis = pmc->pPacket->SocketGet();
#if 0
            // just to test that works
            if ((iSockThis != INVALID_SOCKET) && FD_ISSET(iSockThis, &WinSockSet))
               iRet = 1;
#endif
            if ((iSockThis == INVALID_SOCKET) || FD_ISSET(iSockThis, &WinSockSet)) {
               if (iConnectErr = pmc->pPacket->MaintinenceReceiveIfCan (iTime, &fIncoming)) {
                  CONNECTERR ce;
                  PCWSTR pszErr = NULL;
                  memset (&ce, 0, sizeof(ce));
                  ce.dwID = pmc->dwID;
                  ce.iErr = pmc->pPacket->GetFirstError (&pszErr);
                  if (pszErr && ((wcslen(pszErr)+1)*sizeof(WCHAR) < sizeof(ce.szErr)))
                     wcscpy (ce.szErr, pszErr);

                  m_lConnectErr.Add (&ce);  // error
                  fAnyIncoming = TRUE;
               }
            }
            else
               fIncoming = FALSE;

            // ping if necessary
            if (iConnectErr = pmc->pPacket->MaintinenceCheckForSilence (iTime)) {
               CONNECTERR ce;
               PCWSTR pszErr = NULL;
               memset (&ce, 0, sizeof(ce));
               ce.dwID = pmc->dwID;
               ce.iErr = pmc->pPacket->GetFirstError (&pszErr);
               if (pszErr && ((wcslen(pszErr)+1)*sizeof(WCHAR) < sizeof(ce.szErr)))
                  wcscpy (ce.szErr, pszErr);

               m_lConnectErr.Add (&ce);  // error
               fAnyIncoming = TRUE;
            }

            if (fIncoming) {
               m_lConnectMessage.Add (&pmc->dwID);
               fAnyIncoming = TRUE;
            }

            // get information
            pmc->pPacket->InfoGet(&infoEnd);
            m_iNetworkSent += infoEnd.iSendBytesComp - infoStart.iSendBytesComp;
            m_iNetworkReceived += infoEnd.iReceiveBytesComp - infoStart.iReceiveBytesComp;
         } // i
      } // dwGroup

      LeaveCriticalSection (&m_CritSec);

      // post message if anything new
      if (fAnyIncoming && m_hWndNotify)
         PostMessage (m_hWndNotify, m_dwMessage, 0, 0);

      // wait for a signalled semaphore, or 50 millisec, so can see if any new message
      // BUGFIX - Changed from 50 ms to 10 ms, since also holding up
      // timing posts
      // BUGFIX - Changed to 5 milliseconds so get faster data throughput and response times
      if (WAIT_OBJECT_0 == WaitForSingleObject (m_hSignalToThread, 5))
         break;   // just received notification that should quit

      // else, 50 milliseconds has ellapsed, so repeat and see if any new messages
      // in queue
   }

   // delete all the sockets
   EnterCriticalSection (&m_CritSec);

   // BUGFIX - Don't delete sockets if still waiting
   while (m_dwConnectCanDelete) {
      LeaveCriticalSection (&m_CritSec);
      Sleep (10);
      EnterCriticalSection (&m_CritSec);
   }

   for (i = 0; i < m_hCIRCUMREALITYCONNECT.Num(); i++) {
      PCIRCUMREALITYCONNECT pmc = (PCIRCUMREALITYCONNECT)m_hCIRCUMREALITYCONNECT.Get(i);
      WaitUntilSafeToDeletePacket (pmc->pPacket);
      delete pmc->pPacket;
   }
   m_hCIRCUMREALITYCONNECT.Clear();
   LeaveCriticalSection (&m_CritSec);

   // destroy all the compressor threads
   m_fWantToQuitComp = TRUE;
   for (i = 0; i < m_dwCompNum; i++) {
      SetEvent (m_ahSignalToThreadComp[i]);
      WaitForSingleObject (m_ahThreadComp[i], INFINITE);
      DeleteObject (m_ahThreadComp[i]);

      DeleteObject (m_ahSignalToThreadComp[i]);
      DeleteObject (m_ahSignalFromThreadComp[i]);
   } // i

   // all done with window
   DestroyWindow (m_hWnd);

   // shut down winsock
   if (m_fConnectRemote) {
      int iRet;
      iRet = shutdown (m_iSocket, SD_BOTH);
      iRet = closesocket (m_iSocket);
      iRet = WSACleanup ();
   }
}


/*************************************************************************************
CInternetThread::Connect- This is an initialization function. It tries to connect
to the given server. If it fails it returns FALSE. If it succedes, it returns
TRUE and sets up a thread that constantly updates m_pPacket with the latest info.

NOTE: Call this from the MAIN thread.

inputs
   PWSTR          pszFile - Circumreality file that's being run
   BOOL           fRemote - If TRUE then should have a remote connection
   DWORD          dwPort - Port, if remote
   HWND           hWndNotify - When a new message comes in, this will receive a post
                     indicating there's a new message.
   DWORD          dwMessage - This is the WM_ message that's posted to indicate the
                     new message.
   int            *piWinSockErr - If returnes with en error, this is the WinSock error.
returns
   BOOL - TRUE if connected and thread created. FALSE if failed to connect and
      no thread created.
*/
BOOL CInternetThread::Connect (PWSTR pszFile, BOOL fRemote, DWORD dwPort, HWND hWndNotify, DWORD dwMessage,
                               int *piWinSockErr)
{
   *piWinSockErr = m_iWinSockErr = 0;

   if (m_hThread)
      return FALSE;  // cant call a second time

   m_hWndNotify = hWndNotify;
   m_dwMessage = dwMessage;
   m_fConnected = FALSE;
   wcscpy (m_szConnectFile, pszFile);
   m_fConnectRemote = fRemote;
   m_dwConnectPort = dwPort;

   // create all the events
   m_hSignalFromThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hSignalToThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   DWORD dwID;
   m_hThread = CreateThread (NULL, 0, InternetThreadProc, this, 0, &dwID);
      // NOTE: Not using ESCTHREADCOMMITSIZE because want to be as stable as possible
   if (!m_hThread) {
      CloseHandle (m_hSignalFromThread);
      CloseHandle (m_hSignalToThread);
      m_hSignalFromThread = m_hSignalToThread = NULL;
      return FALSE;
   }

   // lower thread priority for rendering so audio wont break up.
   SetThreadPriority (m_hThread, VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));   // BUGFIX - was below normal, changed to normal

   // wait for the signal from thread, to know if initialization succeded
   WaitForSingleObject (m_hSignalFromThread, INFINITE);
   if (!m_fConnected) {
      // error. which means thread it shutting down. wait for it
      WaitForSingleObject (m_hThread, INFINITE);
      CloseHandle (m_hThread);
      CloseHandle (m_hSignalFromThread);
      CloseHandle (m_hSignalToThread);
      m_hSignalFromThread = m_hSignalToThread = NULL;
      m_hThread = NULL;
      *piWinSockErr = m_iWinSockErr;
      return FALSE;
   }

   // else, all ok
   return TRUE;
}


/*************************************************************************************
CInternetThread::Disconnect - Call this to cancel a connection made by Connect().
This is autoamtically called if the CInternetThread object is deleted.
It also deletes the thread it creates.
*/
BOOL CInternetThread::Disconnect (void)
{
   if (!m_hThread)
      return FALSE;

   // signal
   SetEvent (m_hSignalToThread);

   // wait
   WaitForSingleObject (m_hThread, INFINITE);

   // delete all
   CloseHandle (m_hThread);
   CloseHandle (m_hSignalFromThread);
   CloseHandle (m_hSignalToThread);
   m_hSignalFromThread = m_hSignalToThread = NULL;
   m_hThread = NULL;

   return TRUE;
}


/*************************************************************************************
CInternetThread::ConnectionCreatedGet - Gets the ID of a newly created Connection. This
ID is removed from the list of newly created Connections.

Basically, this should be called if a message is received from the thread window,
so the calling app can know that a new connection has been established

returns
   DWORD       - ID of a newly created Connection, or -1 if no new ones have been created
*/
DWORD CInternetThread::ConnectionCreatedGet (void)
{
   DWORD dwID = -1;

   EnterCriticalSection (&m_CritSec);
   if (m_lConnectNew.Num()) {
      dwID = *((DWORD*)m_lConnectNew.Get(0));
      m_lConnectNew.Remove (0);
   }

   LeaveCriticalSection (&m_CritSec);

   return dwID;
}



/*************************************************************************************
CInternetThread::ConnectionReceivedGet - Gets the ID of a Connection that has just
received data. This ID is removed from the list of Connections containing received data.

Basically, this should be called if a message is received from the thread window,
so the calling app can know that new information exists to be processed. The
calling app should read as as much input data as possible from the socket.

returns
   DWORD       - ID of a a message Connection, or -1 if no new ones have been created
*/
DWORD CInternetThread::ConnectionReceivedGet (void)
{
   DWORD dwID = -1;

   EnterCriticalSection (&m_CritSec);
   while (m_lConnectMessage.Num()) {
      // get the ID
      dwID = *((DWORD*)m_lConnectMessage.Get(0));
      m_lConnectMessage.Remove (0);

      // make sure it still has data
      PCIRCUMREALITYCONNECT pmc = (PCIRCUMREALITYCONNECT)m_hCIRCUMREALITYCONNECT.Find (dwID);
      if (!pmc) {
         dwID = -1;
         continue;   // no longer exists
      }
      if (pmc->pPacket->PacketGetType (0) == -1) {
         dwID = -1;
         continue;   // no more data
      }

      // else, this is ok
      break;
   }
   LeaveCriticalSection (&m_CritSec);

   return dwID;
}




/*************************************************************************************
CInternetThread::ConnectionErrGet - Gets the ID of a Connection that has just
received an error. This basically means that it should be disconnected.

Basically, this should be called if a message is received from the thread window,
so the calling app can know that new information exists to be processed. If
ConnectionErrGet() returns a value then the connection should be killed because it's
now reporting errors.

inputs
   int         *piErr - If not NULL, filled with error number
   PWSTR       pszErr - If not NULL, filled in with error string. Must 256 chars

returns
   DWORD       - ID of an err Connection, or -1 if no new ones have been created
*/
DWORD CInternetThread::ConnectionErrGet (int *piErr, PWSTR pszErr)
{
   DWORD dwID = -1;
   CONNECTERR ce;
   memset (&ce, 0, sizeof(ce));

   EnterCriticalSection (&m_CritSec);
   while (m_lConnectErr.Num()) {
      // get the ID
      PCONNECTERR pce = (PCONNECTERR)m_lConnectErr.Get(0);
      ce = *pce;
      dwID = pce->dwID;
      m_lConnectErr.Remove (0);

      // make sure it still has data
      PCIRCUMREALITYCONNECT pmc = (PCIRCUMREALITYCONNECT)m_hCIRCUMREALITYCONNECT.Find (dwID);
      if (!pmc) {
         dwID = -1;
         continue;   // no longer exists
      }
      // BUGFIX - remove
      //if (pmc->pPacket->PacketGetType (0) == -1) {
      //   dwID = -1;
      //   continue;   // no more data
      //}

      // else, this is ok
      break;
   }
   LeaveCriticalSection (&m_CritSec);

   if (piErr)
      *piErr = ce.iErr;
   _ASSERTE (sizeof(ce.szErr) <= 256 * sizeof(WCHAR));
   if (pszErr)
      wcscpy (pszErr, ce.szErr);
   return dwID;
}

/*************************************************************************************
CInternetThread::ConnectionRemove - Deletes a Connection based on its ID.
NOTE: This may not happen right away. THe connection to be removed is added to
the list and will be deleted eventually.

inputs
   DWORD          dwID - Connection to remove
returns
   BOOL - TRUE if found
*/
BOOL CInternetThread::ConnectionRemove (DWORD dwID)
{
   BOOL fRet = FALSE;

   EnterCriticalSection (&m_CritSec);
   // make sure it still has data
   DWORD dwIndex = m_hCIRCUMREALITYCONNECT.FindIndex (dwID);
   if (dwIndex != -1) {
      fRet = TRUE;
      CONREMOVE cr;
      cr.dwID = dwID;
      cr.dwTicks = 200; // give the connection 2 seconds to send remaining bits out
      m_lConnectRemove.Add (&cr);
   }
   LeaveCriticalSection (&m_CritSec);

   return fRet;
}


/*************************************************************************************
CInternetThread::ConnectionsLimit - Limit the number of connections. Any new connections
will be denied if there are more than this.

inputs
   DWORD          dwNum - Number of connections.
returns
   BOOL - TRUE if set. FALSE if too few/many.
*/
BOOL CInternetThread::ConnectionsLimit (DWORD dwNum)
{
   BOOL fRet = FALSE;

   // too few or too many
   if (!dwNum || (dwNum > MAXUSERSLOGGEDON))
      return FALSE;

   EnterCriticalSection (&m_CritSec);

   m_dwMaxConnections = dwNum;

   LeaveCriticalSection (&m_CritSec);

   return TRUE;
}


/*************************************************************************************
CInternetThread::ConnectionGet - Given an ID, this returns a pointer to the
Connection's CCircumrealityPacket object... which can be used to send and receive data.

NOTE: If this connection is on the to-delete list then NULL will be returned.

You MUST call ConnectionGetRelease() after done with the pointer to ensure that
it can be released.

inputs
   DWORD          dwID - Connection to remove
returns
   PCCircumrealityPackeet - Object, or NULL if cant find
*/
PCCircumrealityPacket CInternetThread::ConnectionGet (DWORD dwID)
{
   PCCircumrealityPacket pRet = NULL;

   EnterCriticalSection (&m_CritSec);

   // if it's on to-delete list then don't allow to find. need
   // this so dont get race condition of deleting it, then inadvertantly
   // getting it after the delete
   DWORD dwDel, i;
   if (dwDel = m_lConnectRemove.Num()) {
      PCONREMOVE pcr = (PCONREMOVE)m_lConnectRemove.Get(0);
      for (i = 0; i < dwDel; i++, pcr++)
         if (pcr->dwID == dwID) {
            LeaveCriticalSection (&m_CritSec);
            return NULL;
         }
   }
   // make sure it still has data
   PCIRCUMREALITYCONNECT pmc = (PCIRCUMREALITYCONNECT)m_hCIRCUMREALITYCONNECT.Find (dwID);
   if (pmc)
      pRet = pmc->pPacket;

   // BUGFIX - Only increase connection can delete if pRet, since seem
   // to have m_dwConnectCanDelete leak
   if (pRet)
      m_dwConnectCanDelete++;
   LeaveCriticalSection (&m_CritSec);

   return pRet;
}



/*************************************************************************************
CInternetThread::ConnectionGetRelease - Releases the "dont delete" flag set
by caling ConnectionGet()

inputs
returns
*/
void CInternetThread::ConnectionGetRelease (void)
{
   EnterCriticalSection (&m_CritSec);
   m_dwConnectCanDelete--;
   LeaveCriticalSection (&m_CritSec);
}



/*************************************************************************************
CInternetThread::ConnectionEnum - Fills in a list with all the connection IDs.


inputs
   PCListFixed       plEnum - Initialized to sizeof(DWORD), and filled in with list
returns
   none
*/
void CInternetThread::ConnectionEnum (PCListFixed plEnum)
{
   plEnum->Init (sizeof(DWORD));

   EnterCriticalSection (&m_CritSec);
   DWORD i;
   for (i = 0; i < m_hCIRCUMREALITYCONNECT.Num(); i++) {
      PCIRCUMREALITYCONNECT pmc = (PCIRCUMREALITYCONNECT)m_hCIRCUMREALITYCONNECT.Get (i);
      plEnum->Add (&pmc->dwID);
   }
   LeaveCriticalSection (&m_CritSec);
}


/*************************************************************************************
CInternetThread::PerformanceNetwork - Returns the number of bytes sent/received

inputs
   __int64        *piSent - Number of bytes sent
   __int64        *piReceived - Number of bytes received
returns
   none
*/
void CInternetThread::PerformanceNetwork (__int64 *piSent, __int64 *piReceived)
{
   EnterCriticalSection (&m_CritSec);
   if (piSent)
      *piSent = m_iNetworkSent;
   if (piReceived)
      *piReceived = m_iNetworkReceived;
   LeaveCriticalSection (&m_CritSec);
}



/*************************************************************************************
CInternetThread::IPBlacklist - Blacklists an IP address. This does NOT kick off
anyone using that IP address though.

Thread safe

inputs
   PWSTR          pszIP - IP address
   fp             fp - Amount to blacklist. This is summed with the current blacklist
                     amount. If the sum is >= 1.0 then it's blacklisted
   fp             fDays - Number of days to ban the IP address from (as long as on)
returns
   BOOL - TRUE if success
*/
BOOL CInternetThread::IPBlacklist (PWSTR pszIP, fp fpAmount, fp fDays)
{
   char szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, pszIP, -1, szTemp, sizeof(szTemp), 0, 0);
#ifdef USEIPV6
   ADDRINFO *AI;
   ADDRINFO hints;
   memset (&hints, 0, sizeof(hints));
   hints.ai_flags = AI_NUMERICHOST;
   getaddrinfo (szTemp, NULL, &hints, &AI);
   if (!AI)
      return FALSE;
#else
   unsigned long iNet = inet_addr (szTemp);
   if (iNet == INADDR_NONE)
      return FALSE;
#endif

   // BUGFIX - Ban until a certain date
   FILETIME ft;
   __int64 iTime;
   GetSystemTimeAsFileTime (&ft);
   iTime = *((__int64*)&ft);
   fDays *= 24.0 * 60.0 * 60.0;  // to minutes
   iTime += (__int64) fDays * 10000000.0 /* 100 nanoseconds */;

   EnterCriticalSection (&m_CritSecIPBLACKLIST);
   DWORD i;
   PIPBLACKLIST pIBL = (PIPBLACKLIST) m_lIPBLACKLIST.Get(0);
   for (i = 0; i < m_lIPBLACKLIST.Num(); i++, pIBL++) {
#ifdef USEIPV6
      if (AI->ai_addrlen > sizeof(pIBL->iAddr))
         continue;   // error
      if (!memcmp (AI->ai_addr, &pIBL->iAddr, AI->ai_addrlen))
         break;
#else
      if (pIBL->iAddr == iNet)
         break;
#endif
   }
   if (i < m_lIPBLACKLIST.Num()) {
      pIBL->fScore += fpAmount;
      pIBL->iFileTime = iTime;
   }
   else {
      IPBLACKLIST ipb;
      memset (&ipb, 0, sizeof(ipb));
      ipb.fScore = fpAmount;
      ipb.iFileTime = iTime;
#ifdef USEIPV6
      memset (&ipb.iAddr, 0, sizeof(ipb.iAddr));
      if (AI->ai_addrlen <= sizeof(ipb.iAddr))
         memcpy (&ipb.iAddr, AI->ai_addr, AI->ai_addrlen);
#else
      ipb.iAddr = iNet;
#endif
      m_lIPBLACKLIST.Add (&ipb);
   }
   LeaveCriticalSection (&m_CritSecIPBLACKLIST);

   return TRUE;
}

// BUGBUG - may have some problems when dying connections are allowed to send
// last bits of data. I don't think there will be, but watch out for this.
