/*************************************************************************************
CServerLoadThread.cpp - Connect to a server and see what the server load is.

begun 4/11/06 by Mike Rozak.
Copyright 2006 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"


/*
 * WinSock 2 extension -- manifest constants for shutdown()
 */
#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02






/*************************************************************************************
CServerLoadThread::Constructor and destructor
*/
CServerLoadThread::CServerLoadThread (void)
{
   m_pPacket = NULL;
   m_hThread = NULL;
   m_hSignalFromThread = NULL;
   m_hSignalToThread = NULL;
   m_fConnected = FALSE;
   m_szDomain[0] = 0;
   m_dwConnectIP = m_dwConnectPort = 0;
   m_pServerLoad = NULL;
   m_hSignalFinal = NULL;
   m_szError[0] = 0;

   InitializeCriticalSection (&m_CritSec);

}

CServerLoadThread::~CServerLoadThread (void)
{
   if (m_hThread)
      Disconnect();

   DeleteCriticalSection (&m_CritSec);
   // the act of disconnecting should free everything up
}


/*************************************************************************************
InternetThreadProc - Thread that handles the internet.
*/
static DWORD WINAPI InternetThreadProc(LPVOID lpParameter)
{
   PCServerLoadThread pThread = (PCServerLoadThread) lpParameter;

   pThread->ThreadProc ();

   return 0;
}


/*************************************************************************************
CServerLoadThread::ThreadProc - This internal function handles the thread procedure.
It first creates the packet sending object and tries to log on. If it fails
it sets m_fConnected to FALSE and sets a flag. If it succedes it sets m_fConnected
to true, and loops until m_hSignalToThread is set.
*/
void CServerLoadThread::ThreadProc (void)
{
   m_fConnected = FALSE;

   // must have a domain, and not with *, or with http:
   if (!m_szDomain[0] || (m_szDomain[0] == L'*') || BeginsWithHTTP (m_szDomain)) {
      // error
      m_iWinSockErr = -1;  // unknwon
      SetEvent (m_hSignalFromThread);
      return;
   }

   SOCKET m_iSocket = INVALID_SOCKET;
   int iRet;

   // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
   m_iWinSockErr = 0;
   //WSADATA wsData;
   //m_iWinSockErr = WSAStartup (MAKEWORD( 2, 2 ), &wsData);
   //if (m_iWinSockErr) {
   //   SetEvent (m_hSignalFromThread);
   //   return;
   //}

   // set an event saying that started, so that wont wait while get domain locaiton
   m_fConnected = TRUE;
   SetEvent (m_hSignalFromThread);

   // convert to ANSI
   //char szaDomain[512];
   //WideCharToMultiByte (CP_ACP, 0, m_szDomain, -1, szaDomain, sizeof(szaDomain), 0, 0);

   if (!AddressToIP (m_szDomain, &m_dwConnectIP)) {
      //if (!m_szDomain[0] || (m_szDomain[0] == L'*')) {
      //   // error
      //   m_iWinSockErr = -1;  // unknwon
      //   WSACleanup ();
      //   SetEvent (m_hSignalFinal);
      //   return;
      //}

      if (!NameToIP(m_szDomain, &m_dwConnectIP, &m_iWinSockErr, m_szError)) {
         // error
         // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
         // WSACleanup ();
         SetEvent (m_hSignalFinal);
         return;
      }
#ifdef _DEBUG
      else
         m_iWinSockErr = 0;
#endif

   } // if not an IP address
#ifdef _DEBUG
   else
      m_iWinSockErr = 0;
#endif


   m_iSocket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if (m_iSocket == INVALID_SOCKET) {
      m_iWinSockErr = WSAGetLastError ();
      // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
      // WSACleanup ();
      SetEvent (m_hSignalFinal);
      return;
   }

   struct sockaddr_in WinSockAddr;
   memset (&WinSockAddr, 0, sizeof(WinSockAddr));
   WinSockAddr.sin_family = AF_INET;
   WinSockAddr.sin_port = htons((WORD)m_dwConnectPort);
   WinSockAddr.sin_addr.s_addr = m_dwConnectIP;
   if (connect (m_iSocket, (sockaddr*) &WinSockAddr, sizeof(WinSockAddr))) {
      m_iWinSockErr = WSAGetLastError ();
      shutdown (m_iSocket, SD_BOTH);
      closesocket (m_iSocket);
      // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
      // WSACleanup ();

      SetEvent (m_hSignalFinal);
      return;
   }

   // set socket as non blocking
   DWORD dwBlock = TRUE;
   iRet = ioctlsocket (m_iSocket, FIONBIO, &dwBlock );

   m_pPacket = new CCircumrealityPacket;
   if (!m_pPacket) {
      shutdown (m_iSocket, SD_BOTH);
      closesocket (m_iSocket);
      // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
      // WSACleanup ();

      SetEvent (m_hSignalFinal);
      return;
   }
   if (!m_pPacket->Init (m_iSocket, NULL)) {
      shutdown (m_iSocket, SD_BOTH);
      closesocket (m_iSocket);
      // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
      // WSACleanup ();

      delete m_pPacket;
      m_pPacket = NULL;
      m_fConnected = FALSE;
      SetEvent (m_hSignalFinal);
      return;
   }

   // else, it went through...
   // since called above m_fConnected = TRUE;
   // since called above SetEvent (m_hSignalFromThread);

   // sent a ping just to introduce self
   // NOTE: Don't need to send ping now because server will do it
   // m_pPacket->PacketSend (CIRCUMREALITYPACKET_PINGSEND, NULL, 0);

   // get the time, in millseconds
   LARGE_INTEGER     liPerCountFreq, liCount;
   __int64  iTime;
   QueryPerformanceFrequency (&liPerCountFreq);


   // wait, either taking message from the queue, or an event, or doing processing
   BOOL fIncoming;
   BOOL fLoggingOff = FALSE;
   DWORD dwLogOffLeft = 100;  // once decide to close down, give 1 second to actually do so
   DWORD dwTime = GetTickCount();
   while (TRUE) {
      // get the time in milliseconds
      QueryPerformanceCounter (&liCount);
      iTime = *((__int64*)&liCount) / ( *((__int64*)&liPerCountFreq) / 1000);

      // see if any new messages and handle
      m_pPacket->MaintinenceSendIfCan (iTime);
      m_pPacket->MaintinenceReceiveIfCan (iTime, &fIncoming);
      if (m_pPacket->MaintinenceCheckForSilence (iTime))
         fIncoming = TRUE;

      // if have been connected for more thant 5 seconds and no results then start logoff
      if ((GetTickCount() > dwTime + 5000) && !fLoggingOff) {
         fLoggingOff = TRUE;
         if (m_pPacket)
            m_pPacket->PacketSend (CIRCUMREALITYPACKET_CLIENTLOGOFF, NULL, 0, 0, 0);
      }

      // loop through all the incoming messages and handle
      while (TRUE) {
         PCMem pMem;
         DWORD dwType = m_pPacket->PacketGetType();
         if (dwType == -1)
            break;

         switch (dwType) {

         case CIRCUMREALITYPACKET_SERVERLOAD:
            {
               // got message about version number
               pMem = m_pPacket->PacketGetMem();
               if (!pMem)
                  break;
               if (pMem->m_dwCurPosn < sizeof(CIRCUMREALITYSERVERLOAD)) {
                  delete pMem;   // shouldnt happen
                  break;
               }

               PCIRCUMREALITYSERVERLOAD psl = (PCIRCUMREALITYSERVERLOAD) pMem->p;
               memcpy (m_pServerLoad, psl, sizeof(*m_pServerLoad));
               delete pMem;

               // initiate shut-down, informing server
               if (!fLoggingOff) {
                  fLoggingOff = TRUE;
                  if (m_pPacket)
                     m_pPacket->PacketSend (CIRCUMREALITYPACKET_CLIENTLOGOFF, NULL, 0, 0, 0);
               }

            }
            break;


         default:
            // dont care about other packets
            pMem = m_pPacket->PacketGetMem ();
            if (pMem)
               delete pMem;
            break;

         }

      } // while TRUE

      if (fLoggingOff) {
         // if logging off, spend 1 second finishing sending connections
         Sleep (10);
         dwLogOffLeft--;
         if (!dwLogOffLeft)
            break;
      }
      else {
         // wait for a signalled semaphore, or 50 millisec, so can see if any new message
         // BUGFIX - Changed to 10 ms so would be faster
         if (WAIT_OBJECT_0 == WaitForSingleObject (m_hSignalToThread, 10)) {
            // send a packet to server so knows logging off
            if (m_pPacket)
               m_pPacket->PacketSend (CIRCUMREALITYPACKET_CLIENTLOGOFF, NULL, 0, 0, 0);
               // NOTE: Dont bother checking for error
            fLoggingOff = TRUE;   // just received notification that should quit
         }
      }

      // else, 50 milliseconds has ellapsed, so repeat and see if any new messages
      // in queue
   }

   // all done
   delete m_pPacket;
   m_pPacket = NULL;

   shutdown (m_iSocket, SD_BOTH);
   closesocket (m_iSocket);
   // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
   // WSACleanup ();

   // will need to set semaphore that done
   SetEvent (m_hSignalFinal);
}


/*************************************************************************************
CServerLoadThread::Connect- This is an initialization function. It tries to connect
to the given server. If it fails it returns FALSE. If it succedes, it returns
TRUE and sets up a thread that constantly updates m_pPacket with the latest info.

NOTE: Call this from the MAIN thread.

inputs
   PWSTR          pszDomain - This is either http://xxx, or an IP address in a string.
   DWORD          dwPort - Port to use (if remote)
   PCIRCUMREALITYSERVERLOAD pServerLoad - Filled in with the server load information
   HANDLE         hSignalDone - Set when pServerLoad is filled in
   int            *piWinSockErr - Where the winsock error will be written if there's an error
returns
   BOOL - TRUE if connected and thread created, and will set hSignalDone when
      pServerLoad is filled in or failed. FALSE if failed to connect and
      no thread created.
*/
BOOL CServerLoadThread::Connect (PWSTR pszDomain, DWORD dwPort, PCIRCUMREALITYSERVERLOAD pServerLoad,
                                 HANDLE hSignalDone, int *piWinSockErr)
{
   m_szError[0] = 0;
   if (m_hThread)
      return FALSE;  // cant call a second time

   m_hSignalFinal = hSignalDone;
   m_pServerLoad = pServerLoad;
   memset (m_pServerLoad, 0, sizeof(*m_pServerLoad));

   *piWinSockErr = m_iWinSockErr = 0;

   m_fConnected = FALSE;

   wcscpy (m_szDomain, pszDomain);
   // m_dwConnectIP = dwIP;
   m_dwConnectPort = dwPort;

   // create all the events
   m_hSignalFromThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hSignalToThread = CreateEvent (NULL, FALSE, FALSE, NULL);

   DWORD dwID;
   m_hThread = CreateThread (NULL, ESCTHREADCOMMITSIZE, InternetThreadProc, this, 0, &dwID);
   if (!m_hThread) {
      CloseHandle (m_hSignalFromThread);
      CloseHandle (m_hSignalToThread);
      m_hSignalFromThread = m_hSignalToThread = NULL;
      return FALSE;
   }
   SetThreadPriority (m_hThread, VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));

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
CServerLoadThread::Disconnect - Call this to cancel a connection made by Connect().
This is autoamtically called if the CServerLoadThread object is deleted.
It also deletes the thread it creates.
*/
BOOL CServerLoadThread::Disconnect (void)
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
ServerLoadQuery - Connects to a collection of servers and tests to see
the server loads.

inputs
   PSERVERLOADQUERY     paQuery - Array of dwNum query structures. The
                        paQuery->ServerLoad sub-structures will be filled in,
                        although if there's an error it might be all zeros.
   DWORD                dwNum - Number of structures
   HWND                 hWndProgress - Window to show progress bar on top of. IF NULL then no progress
returns
   none - Returns when finished.
*/
#define MAXQUERIES         10    // dont have any more than 10 threads at once

void ServerLoadQuery (PSERVERLOADQUERY paQuery, DWORD dwNum, HWND hWndProgress)
{
   CProgress Progress;
   if (hWndProgress)
      Progress.Start (hWndProgress, "Checking world populations...", TRUE);

   HANDLE   ahSignal[MAXQUERIES];
   PCServerLoadThread aSLT[MAXQUERIES];
   DWORD dwWaiting = 0;
   DWORD dwCur = 0;
   
   // create all the events
   DWORD i;
   int iWinSockErr;
   memset (aSLT, 0, sizeof(aSLT));
   for (i = 0; i < MAXQUERIES; i++)
      ahSignal[i] = CreateEvent (NULL, FALSE, FALSE, NULL);

   while ((dwCur < dwNum) || dwWaiting) {
      // if we can send some out then do so
      while ((dwCur < dwNum) && (dwWaiting < MAXQUERIES)) {
         // find the bin
         for (i = 0; i < MAXQUERIES; i++)
            if (!aSLT[i])
               break;
         if (i >= MAXQUERIES)
            break;   // shouldnt happen

         // reset
         ResetEvent (ahSignal[i]);

         // create new server object
         aSLT[i] = new CServerLoadThread();
         if (!aSLT[i])
            break;   // error. shouldnt hapepn
         if (!aSLT[i]->Connect (paQuery[dwCur].szDomain, paQuery[dwCur].dwPort, &paQuery[dwCur].ServerLoad,
            ahSignal[i], &iWinSockErr)) {
               // error
               memset (&paQuery[dwCur].ServerLoad, 0, sizeof(paQuery[dwCur].ServerLoad));
               dwCur++;

               delete aSLT[i];
               aSLT[i] = NULL;
               continue;
            }

         // else, need to wait
         dwCur++;
         dwWaiting++;
      }

      // see if any events signaled
      for (i = 0; i < MAXQUERIES; i++) {
         if (!aSLT[i])
            continue;
   
         if (WaitForSingleObject (ahSignal[i], 0) != WAIT_OBJECT_0)
            continue;   // not signaled

         // else, done
         delete aSLT[i];
         aSLT[i] = NULL;
         dwWaiting--;
      } // i

      if (hWndProgress)
         Progress.Update ((fp)(dwCur - dwWaiting) / (fp)dwNum);

      // sleep for a short bit before retesting
      Sleep (10);
   } // while have stuff out

   // free handles
   for (i = 0; i < MAXQUERIES; i++)
      CloseHandle (ahSignal[i]);

}
