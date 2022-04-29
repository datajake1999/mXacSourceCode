/*************************************************************************************
CInternetThread.cpp - Code for managing the internet communications thread.

begun 29/2/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <crtdbg.h>
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


// THREADLOWMSG - Message to low pri thread
typedef struct {
   DWORD          dwType;  // message type, such as CIRCUMREALITYPACKET_CLIENTREPLYWANTRENDER
   PCMem          pMem;    // memory associated with it
} THREADLOWMSG, *PTHREADLOWMSG;


/****************************************************************************
RunCircumrealityServer - Runs the Circumreality server

inputs
   char           pszFile - Circumreality file to run
   HWND           hWnd - Window
returns
   BOOL - TRUE if success, FALSE if failed
*/
BOOL RunCircumrealityServer (char *pszFile)
{
   // create the filename
   char szRun[1024];
   char szCurDir[256];
   strcpy (szRun, "\"");
   DWORD dwLen = (DWORD) strlen(szRun);
   strcpy (szCurDir, gszAppDir);
   strcat (szRun, gszAppDir);
   strcat (szRun, "CircumrealityWorldSim.exe");
#ifdef _DEBUG        // to test
   strcpy (szCurDir, "z:\\bin");
   strcpy (szRun + dwLen, "z:\\mif\\mifserver\\debug\\circumrealityworldsim.exe");
#endif
   strcat (szRun, "\"");
   if (pszFile) {
      strcat (szRun, " -0 "); // the -0 indicates locally run
      strcat (szRun, pszFile);
   }

   PROCESS_INFORMATION pi;
   STARTUPINFO si;
   memset (&si, 0, sizeof(si));
   si.cb = sizeof(si);
   memset (&pi, 0, sizeof(pi));
   if (!CreateProcess (NULL, szRun, NULL, NULL, NULL, NULL, NULL, szCurDir, &si, &pi))
      return FALSE;

   // wait until all loaded
   WaitForInputIdle (pi.hProcess, 10000);

   // Close process and thread handles. 
   CloseHandle( pi.hProcess );
   CloseHandle( pi.hThread );

   return TRUE;
}



/****************************************************************************
RunCircumrealityClient - Runs the client

inputs
   HWND           hWnd - Window
returns
   BOOL - TRUE if success, FALSE if failed
*/
BOOL RunCircumrealityClient (void)
{
   // create the filename
   char szRun[1024];
   char szCurDir[256];
   strcpy (szRun, "\"");
   DWORD dwLen = (DWORD) strlen(szRun);
   strcpy (szCurDir, gszAppDir);
   strcat (szRun, gszAppPath);
   strcat (szRun, "\"");

   PROCESS_INFORMATION pi;
   STARTUPINFO si;
   memset (&si, 0, sizeof(si));
   si.cb = sizeof(si);
   memset (&pi, 0, sizeof(pi));
   if (!CreateProcess (NULL, szRun, NULL, NULL, NULL, NULL, NULL, szCurDir, &si, &pi))
      return FALSE;

   // wait until all loaded
   // NOTE: Don't wait if doing this
   // WaitForInputIdle (pi.hProcess, 10000);

   // Close process and thread handles. 
   CloseHandle( pi.hProcess );
   CloseHandle( pi.hThread );

   return TRUE;
}





/*************************************************************************************
CInternetThread::Constructor and destructor
*/
CInternetThread::CInternetThread (void)
{
   m_pPacket = NULL;
   m_hThread = NULL;
   m_lTHREADLOWMSG.Init (sizeof(THREADLOWMSG));
   m_hThreadLow = NULL;
   m_hSignalToThreadLow = NULL;
   m_hSignalFromThread = NULL;
   m_hSignalToThread = NULL;
   m_fConnected = FALSE;
   m_szConnectFile[0] = 0;
   m_fConnectRemote = FALSE;
   m_dwConnectIP = m_dwConnectPort = 0;
   m_hWndNotify = NULL;
   m_dwMessage = NULL;
   m_szError[0] = 0;
   m_fShuttingDown = FALSE;

   InitializeCriticalSection (&m_CritSec);

}

CInternetThread::~CInternetThread (void)
{
   if (m_hThread)
      Disconnect();

   DeleteCriticalSection (&m_CritSec);
   // the act of disconnecting should free everything up
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
InternetThreadProcLow - Thread that handles the internet.
*/
DWORD WINAPI InternetThreadProcLow(LPVOID lpParameter)
{
   PCInternetThread pThread = (PCInternetThread) lpParameter;

   pThread->ThreadProcLow ();

   return 0;
}



/*************************************************************************************
CInternetThread::ThreadProcLow - For low-priority thread
*/
void CInternetThread::ThreadProcLow (void)
{
   THREADLOWMSG tlm;

   while (TRUE) {
      // wait for an event
      WaitForSingleObject (m_hSignalToThreadLow, INFINITE);

      EnterCriticalSection (&m_CritSec);
      if (m_fShuttingDown) {
         // shutting down
         LeaveCriticalSection (&m_CritSec);
         break;
      }

      // check for queue
      if (!m_lTHREADLOWMSG.Num()) {
         // empty
         LeaveCriticalSection (&m_CritSec);
         continue;
      }
      tlm = *((PTHREADLOWMSG)m_lTHREADLOWMSG.Get(0));
      m_lTHREADLOWMSG.Remove (0);

      // leave the thread to do the rest of the processing
      LeaveCriticalSection (&m_CritSec);

      PCMem pMem;

      switch (tlm.dwType) {
      case CIRCUMREALITYPACKET_CLIENTREPLYWANTTTS:
         {
            // get the memory
            pMem = tlm.pMem;
            //pMem = m_pPacket->PacketGetMem ();
            if (!pMem)
               break;

            // NOTE: For TTS, don't check QueuedToSend() because data not big enough

            // ignore this message is not registered
            // ignore this message if have internet turned off
            if (!gfRenderCache || !RegisterMode()) {
               delete pMem;
               break;
            }

            PCIRCUMREALITYPACKETCLIENTREPLYWANT pw;
            if (pMem->m_dwCurPosn < sizeof(*pw)) {
               // shouldnt happen
               delete pMem;
               break;
            }

            // make sure memory large enough
            pw = (PCIRCUMREALITYPACKETCLIENTREPLYWANT) pMem->p;
            if ((pMem->m_dwCurPosn < sizeof(*pw) + pw->dwStringSize) || (pw->dwStringSize < sizeof(WCHAR)) ) {
               delete pMem;
               break;
            }

            // if server doesn't want it then done
            if (!pw->fWant) {
               delete pMem;
               break;
            }

            // make sure string OK
            PWSTR pszString = (PWSTR)(pw+1);
            if (pszString[pw->dwStringSize/sizeof(WCHAR)-1]) {
               // not null terminated
               delete pMem;
               break;
            }

            // send it
            gpMainWindow->SendWaveToServer (pszString, pw->dwQuality);

            // done
            delete pMem;
         }
         break;

      case CIRCUMREALITYPACKET_CLIENTREPLYWANTRENDER:
         {
            // get the memory
            pMem = tlm.pMem;
            // pMem = m_pPacket->PacketGetMem ();
            if (!pMem)
               break;

            // if already have data queued to send, then ignore this message since
            // dont want to overload the queue
            // BUGFIX - Was ignoring if anything queued, but now let queue up
            // to 2 meg
            if (m_pPacket && (m_pPacket->QueuedToSend(PACKCHAN_RENDER) > MAXQUEUETOSEND_RENDER)) {
               delete pMem;
               break;
            }

            // ignore this message is not registered
            // ignore this message if have internet turned off
            if (!gfRenderCache || !RegisterMode()) {
               delete pMem;
               break;
            }

            PCIRCUMREALITYPACKETCLIENTREPLYWANT pw;
            if (pMem->m_dwCurPosn < sizeof(*pw)) {
               // shouldnt happen
               delete pMem;
               break;
            }

            // make sure memory large enough
            pw = (PCIRCUMREALITYPACKETCLIENTREPLYWANT) pMem->p;
            if ((pMem->m_dwCurPosn < sizeof(*pw) + pw->dwStringSize) || (pw->dwStringSize < sizeof(WCHAR)) ) {
               delete pMem;
               break;
            }

            // if server doesn't want it then done
            if (!pw->fWant) {
               delete pMem;
               break;
            }

            // make sure string OK
            PWSTR pszString = (PWSTR)(pw+1);
            if (pszString[pw->dwStringSize/sizeof(WCHAR)-1]) {
               // not null terminated
               delete pMem;
               break;
            }


            // make sure pr->dwQuality is what we currently have set
            int iResolution;
            int iQualityMono = gpMainWindow->QualityMonoGet(&iResolution);
            if (iQualityMono < 0) {
               delete pMem;
               break;
            }
            if ((DWORD)iQualityMono != pw->dwQuality) {
               delete pMem;
               break;
            }


            // load in the high quality version, if not that, assume that should send
            // in the low-quality version
            // BUGFIX - Only save highest quality
            DWORD dwQuality;
            if ((gpMainWindow->m_iResolution == gpMainWindow->m_iResolutionLow) && (gpMainWindow->m_dwShadowsFlags == gpMainWindow->m_dwShadowsFlagsLow) && (gpMainWindow->m_fLipSync == gpMainWindow->m_fLipSyncLow))
               dwQuality = 0;
            else
               dwQuality = 1;

            CImageStore ImageStore;
            CMem memBinary;
            //for (dwQuality = NUMIMAGECACHE-1; dwQuality < NUMIMAGECACHE; dwQuality--) {
            // try to load from megafile
            if (!ImageStore.Init (&gpMainWindow->m_amfImages[dwQuality], pszString))
               break;

            // if loaded, get the binary
            if (!ImageStore.ToBinary (TRUE, &memBinary)) {
               memBinary.m_dwCurPosn = 0;
               break;
            }

               // else, saved it to binary
               //break;
            //}

            // dont send image unless resolution exactly matches one
            // of the images for the quality setting
            fp fScale = ResolutionToRenderScale (iResolution);
            DWORD dwAspect, dwWidth, dwHeight;
            // look through all the aspect ratios, including 10
            for (dwAspect = 0; dwAspect < 11; dwAspect++) {
               RenderSceneAspectToPixelsInt  ((int)dwAspect, fScale, &dwWidth, &dwHeight);
               if ((dwWidth == ImageStore.Width()) && (dwHeight == ImageStore.Height()))
                  break;
            } // dwAspect
            // NOTE: Because of this test, things like books (with text), won't ever be
            // cached because their resolution is automatically bumped up
            if (dwAspect >= 11) {
               // resolution doesn't match, so don't upload it, just in case
               delete pMem;
               break;
            }

            if (memBinary.m_dwCurPosn) {
               CMem memAll;
               size_t dwNeed = sizeof(CIRCUMREALITYPACKETSEVERCACHE) + pw->dwStringSize + memBinary.m_dwCurPosn;
               if (memAll.Required (dwNeed) && m_pPacket) {
                  PCIRCUMREALITYPACKETSEVERCACHE prw = (PCIRCUMREALITYPACKETSEVERCACHE) memAll.p;
                  memset (prw, 0, sizeof(*prw));
                  prw->dwQuality = pw->dwQuality;
                  prw->dwStringSize = pw->dwStringSize;
                  prw->dwDataSize = (DWORD) memBinary.m_dwCurPosn;

                  memcpy (prw+1, pszString, prw->dwStringSize);
                  memcpy ((PBYTE)(prw+1) + prw->dwStringSize, memBinary.p, prw->dwDataSize);

                  m_pPacket->PacketSend (CIRCUMREALITYPACKET_SERVERCACHERENDER, prw, (DWORD) dwNeed);

#ifdef _DEBUG
                  OutputDebugStringW (L"\r\nCInternetThread::Sending ServerCacheRender message.");
#endif
               }
            } // if have image to write

            delete pMem;
         }
         break;

      default:
         if (tlm.pMem)
            delete tlm.pMem;
         break;
      } // switch


   } // while TRUE

   // done
   return;
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

   HWND hWndServer = NULL;
   SOCKET m_iSocket = INVALID_SOCKET;
   char szaConnectFile[512] = "";
   int iRet;
   if (m_fConnectRemote) {
      // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
      // WSADATA wsData;
      m_iWinSockErr = 0;
      // m_iWinSockErr = WSAStartup (MAKEWORD( 2, 2 ), &wsData);
      //if (m_iWinSockErr) {
      //   swprintf (m_szError,
      //      L"You may have the internet turned off on your computer, or your firewall may be preventing CircumReality from connectting."
      //      L"\r\nTechnical: WSAStartup() failed, returing %d.", (int)m_iWinSockErr);
      //   SetEvent (m_hSignalFromThread);
      //   return;
      //}

      m_iSocket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (m_iSocket == INVALID_SOCKET) {
         m_iWinSockErr = WSAGetLastError ();

         swprintf (m_szError,
            L"You may have the internet turned off on your computer, or your firewall may be preventing CircumReality from connectting."
            L"\r\nTechnical: socket() failed, returing %d.", (int)m_iWinSockErr);

         // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
         // WSACleanup ();
         SetEvent (m_hSignalFromThread);
         return;
      }

      // int iSize = 0;
      int iRequired; // , iRet;
      DWORD dwVal;

      // BUGFIX - Try making reusable, and changing send/receive timeout to see if not
      // working because of problem with mom's computer

      // make sure reusable
#ifdef _DEBUG
      iRequired = sizeof(dwVal);
      getsockopt (m_iSocket, SOL_SOCKET, SO_REUSEADDR, (char*) &dwVal, &iRequired);
#endif
      dwVal = 1;
      setsockopt (m_iSocket, SOL_SOCKET, SO_REUSEADDR, (char*) &dwVal, sizeof(dwVal));

      // receive timeout
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


      struct sockaddr_in WinSockAddr;
      memset (&WinSockAddr, 0, sizeof(WinSockAddr));
      WinSockAddr.sin_family = AF_INET;
      WinSockAddr.sin_port = htons((WORD)m_dwConnectPort);
      WinSockAddr.sin_addr.s_addr = m_dwConnectIP;
      DWORD dwConnectAttempts;
#define CONNECTATTEMPTS       5
      for (dwConnectAttempts = 0; dwConnectAttempts < CONNECTATTEMPTS; dwConnectAttempts++) {
         m_iWinSockErr = 0;

         if (!connect (m_iSocket, (sockaddr*) &WinSockAddr, sizeof(WinSockAddr)))
            break;   // succeded

         m_iWinSockErr = WSAGetLastError ();

         // sleep
         Sleep (500);

         // retry
      } // dwConnectattemps
      if (dwConnectAttempts >= CONNECTATTEMPTS) {
         swprintf (m_szError,
            L"You may have the internet turned off on your computer, or your firewall may be preventing CircumReality from connectting."
            L"\r\nTechnical: connect(%d, %d)-multipass failed, returing %d.", (int)m_dwConnectPort, (int)m_dwConnectIP, (int)m_iWinSockErr);

         shutdown (m_iSocket, SD_BOTH);
         closesocket (m_iSocket);
         // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
         // WSACleanup ();

         SetEvent (m_hSignalFromThread);
         return;
      }

#if 0 // old code, replaced by connection attempts
      if (connect (m_iSocket, (sockaddr*) &WinSockAddr, sizeof(WinSockAddr))) {
         m_iWinSockErr = WSAGetLastError ();

         // BUGFIX - To see why connect is failing

         swprintf (m_szError,
            L"You may have the internet turned off on your computer, or your firewall may be preventing CircumReality from connectting."
            L"\r\nTechnical: connect(%d, %d) failed, returing %d.", (int)m_dwConnectPort, (int)m_dwConnectIP, (int)m_iWinSockErr);

         shutdown (m_iSocket, SD_BOTH);
         closesocket (m_iSocket);
         // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
         // WSACleanup ();

         SetEvent (m_hSignalFromThread);
         return;
      }
#endif // 0

      // set socket as non blocking
      DWORD dwBlock = TRUE;
      iRet = ioctlsocket (m_iSocket, FIONBIO, &dwBlock );
   }
   else {
      WideCharToMultiByte (CP_ACP, 0, m_szConnectFile, -1, szaConnectFile, sizeof(szaConnectFile), 0, 0);

      // find the server window
      // do this by finding the window, if cant then execute and wait
      DWORD i;
      hWndServer = NULL;
      for (i = 0; i < 50; i++) {
         hWndServer = FindWindow ("CircumrealityServerRequest", szaConnectFile);
         if (hWndServer)
            break;

         // if can't find an it's the first time around then run
         if (!i)
            if (!RunCircumrealityServer (szaConnectFile))
               break;

         // wait a short bit
         Sleep (100);
      }
      if (!hWndServer) {
         SetEvent (m_hSignalFromThread);
         return;
      }
   }

   m_pPacket = new CCircumrealityPacket;
   if (!m_pPacket) {
      if (m_fConnectRemote) {
         shutdown (m_iSocket, SD_BOTH);
         closesocket (m_iSocket);
         // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
         // WSACleanup ();
      }
      SetEvent (m_hSignalFromThread);
      return;
   }
   if (!m_pPacket->Init (m_iSocket, (m_iSocket == INVALID_SOCKET) ? szaConnectFile : NULL)) {
      if (m_fConnectRemote) {
         shutdown (m_iSocket, SD_BOTH);
         closesocket (m_iSocket);
         // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
         // WSACleanup ();
      }

      swprintf (m_szError,
         L"You may have the internet turned off on your computer, or your firewall may be preventing CircumReality from connectting."
         L"\r\nTechnical: m_pPacket->Init(%d) failed.", (int)m_iSocket);

      delete m_pPacket;
      m_pPacket = NULL;
      m_fConnected = FALSE;
      SetEvent (m_hSignalFromThread);
      return;
   }

   if (hWndServer) {
      // send message off to server with packet
      HWND hWndPacket = m_pPacket->WindowGet();
      COPYDATASTRUCT cds;
      cds.cbData = sizeof(hWndPacket);
      cds.dwData = 42;  // so know right message
      cds.lpData = &hWndPacket;
      SendMessage (hWndServer, WM_COPYDATA, (WPARAM) hWndPacket, (LPARAM) &cds);
   }

   // else, it went through...
   m_fConnected = TRUE;
   SetEvent (m_hSignalFromThread);

   // sent a ping just to introduce self
   // NOTE: Don't need to send ping now because server will do it
   // m_pPacket->PacketSend (CIRCUMREALITYPACKET_PINGSEND, NULL, 0);

   // get the time, in millseconds
   LARGE_INTEGER     liPerCountFreq, liCount;
   __int64  iTime;
   QueryPerformanceFrequency (&liPerCountFreq);

   // when first start up, send a message listing all cached files
   {
      CListVariable lEnumName;
      CListFixed  lEnumInfo;
      DWORD i;
      CMem mem;
      gpMainWindow->m_mfFiles.Enum (&lEnumName, &lEnumInfo);
      PMFFILEINFO pmi = (PMFFILEINFO)lEnumInfo.Get(0);

      for (i = 0; i < lEnumName.Num(); i++, pmi++) {
         // get the name
         PWSTR psz = (PWSTR)lEnumName.Get(i);

#if 0 // def _DEBUG
         if (!_wcsicmp(psz, L"LibraryUser")) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"LibraryUser loaded time: %d %d", (int) pmi->iTimeModify.dwHighDateTime, (int)pmi->iTimeModify.dwLowDateTime);
            OutputDebugStringW (szTemp);
         }
#endif

         // if this is the cached TTS list then dont send down the delte
         // list because dont want to delete
         if (!_wcsicmp(psz, gpszRecentTTSFile))
            continue;

         // if this file ends with any of then names of the files that meant
         // to get from the source, then dont send a request for the date.
         // That way user can have really large TTS voice as a substitute,
         // and server will never know
         DWORD dwLen = (DWORD)wcslen(psz);
         DWORD j;
         for (j = 0; j < gpMainWindow->m_lDontDownloadFile.Num(); j++) {
            DWORD dwDLen = (DWORD)gpMainWindow->m_lDontDownloadFile.Size(j) / sizeof(WCHAR) - 1;
            if (dwDLen > dwLen)
               continue;   // too small

            PWSTR pszD = (PWSTR)gpMainWindow->m_lDontDownloadFile.Get(j);
            if (_wcsicmp (pszD, psz + (dwLen - dwDLen)))
               continue;   // no match
            if ((dwDLen < dwLen) && (psz[dwLen - dwDLen - 1] != L'\\'))
               continue;   // no backslash

            // else, match
            break;
         } // j
         if (j < gpMainWindow->m_lDontDownloadFile.Num())
            continue;

         // add the file time
         size_t dwNeed = mem.m_dwCurPosn + sizeof(FILETIME);
         if ((dwNeed > mem.m_dwAllocated) && !mem.Required (dwNeed))
            break;   // error, shouldnt happen
         memcpy ((PBYTE)mem.p + mem.m_dwCurPosn, &pmi->iTimeModify, sizeof(FILETIME));
         mem.m_dwCurPosn = dwNeed;

         // add the file name
         mem.StrCat (psz, (DWORD)wcslen(psz)+1);
      } // i

      // send this
      if (!m_pPacket->PacketSend (CIRCUMREALITYPACKET_FILEDATEQUERY, mem.p, (DWORD)mem.m_dwCurPosn)) {
         int iErr;
         PCWSTR pszErr;
         iErr = m_pPacket->GetFirstError (&pszErr);
         gpMainWindow->PacketSendError(iErr, pszErr);
      }
   }

   // send another packet with the unique ID
   {
      HRESULT hRes;
      GUID gMAC;
      WCHAR szUniqueID[6*2+1];
      hRes = UuidCreateSequential (&gMAC);
      if (hRes == RPC_S_OK)
         MMLBinaryToString (&gMAC.Data4[2], 6, szUniqueID);
      else
         szUniqueID[0] = 0;   // NULL terminate, since dont know
      if (!m_pPacket->PacketSend (CIRCUMREALITYPACKET_UNIQUEID, szUniqueID, (DWORD)wcslen(szUniqueID) * sizeof(WCHAR))) {
         int iErr;
         PCWSTR pszErr;
         iErr = m_pPacket->GetFirstError (&pszErr);
         gpMainWindow->PacketSendError(iErr, pszErr);
      }
   }


   // wait, either taking message from the queue, or an event, or doing processing
   MSG msg;
   BOOL fIncoming;
   BOOL fLoggingOff = FALSE;
   DWORD dwLogOffLeft = 100;  // once decide to close down, give 1 second to actually do so
   while (TRUE) {
      // handle message queue
      while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
         TranslateMessage (&msg);
         DispatchMessage(&msg); 
      }

      // get the time in milliseconds
      QueryPerformanceCounter (&liCount);
      iTime = *((__int64*)&liCount) / ( *((__int64*)&liPerCountFreq) / 1000);

      // see if any new messages and handle
      m_pPacket->MaintinenceSendIfCan (iTime);
      m_pPacket->MaintinenceReceiveIfCan (iTime, &fIncoming);
      if (m_pPacket->MaintinenceCheckForSilence (iTime)) {
         fIncoming = TRUE;

         // BUGFIX - alert main window that silence
         if (gpMainWindow) {
            int iErr;
            PCWSTR pszErr;
            iErr = m_pPacket->GetFirstError (&pszErr);
            gpMainWindow->PacketSendError(iErr, pszErr);
         }
      }

      // loop through all the incoming messages and handle
      while (TRUE) {
         PCMem pMem;
         DWORD dwType = m_pPacket->PacketGetType();
         if (dwType == -1)
            break;

         switch (dwType) {
         case CIRCUMREALITYPACKET_MMLIMMEDIATE:
         case CIRCUMREALITYPACKET_MMLQUEUE:
            {
#ifdef _DEBUG
//               PCMem pMem = m_pPacket->PacketGetMem ();
//               PWSTR psz2 = (PWSTR) pMem->p;
#endif

               PCMMLNode2 pNode = m_pPacket->PacketGetMML();
               if (!pNode)
                  break;

               // BUGFIX - PacketGetMML() now returns several packets.
               // loop through all the sub-packets
               PWSTR psz;
               PCMMLNode2 pSub;
               while (pNode->ContentNum()) {
                  pSub = NULL;
                  pNode->ContentEnum (0, &psz, &pSub);
                  if (!pSub) {
                     pNode->ContentRemove (0);
                     continue;
                  }
                  pNode->ContentRemove (0, FALSE);

                  // try this (it may not work well)
                  // when get a new message in, look for links for files. Add
                  // them to the cache list if they don't exist
                  CBTree tFiles;
                  DWORD i;
                  ExtractFilesFromNode (pSub, &tFiles, FALSE, 0);
                  for (i = 0; i < tFiles.Num(); i++) {
                     PWSTR psz = tFiles.Enum (i);

#if 0 // def _DEBUG
                     if (!_wcsicmp(psz, L"LibraryUser"))
                        i += 0;
#endif

                     // BUGFIX - DON'T do file request for .tts and .mlx here because may be
                     // auto-muting, and don't want to load the TTS voices in now since may
                     // load them even though won't need them
                     size_t dwLen = wcslen(psz);
                     if ((dwLen >= 4) && (!_wcsicmp(psz + (dwLen - 4), L".tts") || !_wcsicmp(psz + (dwLen - 4), L".mlx")) )
                        continue;   // skip this

                     if (!gpMainWindow->m_mfFiles.Exists (psz))
                        FileRequest (psz, FALSE);
                  } // i

                  // if this is a BinaryDataRefresh message then get the file and
                  // clear out some information. Then, pass it one
                  if (!_wcsicmp(CircumrealityBinaryDataRefresh(), pSub->NameGet())) {
                     PWSTR psz = MMLValueGet (pSub, L"File");
                     if (psz)
                        BinaryDataRefresh (psz);
                  } // refresh

                  // change of plans from before...
                  // call the audio thread and pass this in. The audio thread's
                  // RequestAudio() will be smart enough to forward it on
                  LARGE_INTEGER iTime;
                  QueryPerformanceCounter (&iTime);
                  if (gpMainWindow->m_pAT)
                     gpMainWindow->m_pAT->RequestAudio (pSub, iTime, (dwType == CIRCUMREALITYPACKET_MMLQUEUE), TRUE);
                  else
                     delete pSub;
               } // dwPack
               delete pNode;

            }
            break;

         case CIRCUMREALITYPACKET_CLIENTVERSIONNEED:
            {
               // got message about version number
               pMem = m_pPacket->PacketGetMem();
               if (!pMem)
                  break;
               if (pMem->m_dwCurPosn < sizeof(DWORD)) {
                  delete pMem;   // shouldnt happen
                  break;
               }
               DWORD dw = *((DWORD*)pMem->p);
               delete pMem;
               if (m_dwMessage)
                  PostMessage (m_hWndNotify, m_dwMessage, dwType, (LPARAM) dw);
            }
            break;

         case CIRCUMREALITYPACKET_FILEDATA:
            {
               // get the memory
               pMem = m_pPacket->PacketGetMem ();
               if (!pMem) {
                  break;
               }
               if (pMem->m_dwCurPosn < sizeof(FILETIME)+sizeof(WCHAR)) {
                  // shouldnt happen
                  delete pMem;
                  break;
               }

               // save the file to the cache
               PFILETIME pft =(PFILETIME)pMem->p;
               PWSTR psz = (PWSTR)(pft+1);
               DWORD dwLen = ((DWORD)wcslen(psz)+1)*sizeof(WCHAR) + sizeof(FILETIME);

#ifdef _DEBUG
               OutputDebugString ("\r\nReceiving ");
               OutputDebugStringW (psz);
#endif
               if (dwLen < pMem->m_dwCurPosn) {
                  // limit total file size for megafile
                  gpMainWindow->m_mfFiles.LimitSize (giTotalPhysicalMemory / 2, 60 * 24);
                        // BUGFIX - Limit cached files to 1/2 of physical memory

#if 0 // def _DEBUG
                  if (!_wcsicmp(psz, L"LibraryUser")) {
                     WCHAR szTemp[64];
                     swprintf (szTemp, L"LibraryUser loaded time: %d %d", (int) pft->dwHighDateTime, (int)pft->dwLowDateTime);
                     OutputDebugStringW (szTemp);
                  }
#endif

                  if (!gpMainWindow->m_mfFiles.Save (psz, (PBYTE)pMem->p + dwLen,
                     pMem->m_dwCurPosn - dwLen, pft, pft))
                     dwLen = (DWORD)pMem->m_dwCurPosn; // so will mark as being bad
               }

               // remove the file from the list and/or put on the bad list
               EnterCriticalSection (&m_CritSec);
               DWORD i;
               for (i = 0; i < m_lFileRequest.Num(); i++)
                  if (!_wcsicmp(psz, (PWSTR)m_lFileRequest.Get(i))) {
                     m_lFileRequest.Remove (i);
                     break;
                  }
               if (dwLen >= pMem->m_dwCurPosn)
                  m_tFileBad.Add (psz, 0, 0);   // bad file, so note this
               LeaveCriticalSection (&m_CritSec);
                     

               // done
               delete pMem;
            }
            break;

         // case CIRCUMREALITYPACKET_CLIENTCACHEFAILTTS:
            // BUGFIX - Disabling this so just deletes, since shouldnt get from the
            // server and don't know what to do with

         case CIRCUMREALITYPACKET_CLIENTCACHETTS:
         case CIRCUMREALITYPACKET_CLIENTCACHERENDER:
         case CIRCUMREALITYPACKET_CLIENTCACHEFAILRENDER:
            {
               // get the memory
               pMem = m_pPacket->PacketGetMem ();
               if (!pMem)
                  break;

               // remove from list of download requests
               PCIRCUMREALITYPACKETCLIENTCACHE pcc = (PCIRCUMREALITYPACKETCLIENTCACHE) pMem->p;
               if (pMem->m_dwCurPosn < sizeof(*pcc)) {
                  delete pMem;
                  break;
               }
               if ((pMem->m_dwCurPosn < sizeof(*pcc) + pcc->dwStringSize + pcc->dwDataSize) || (pcc->dwStringSize < sizeof(WCHAR)) ) {
                  delete pMem;
                  break;
               }

               // make sure string OK
               PWSTR pszString = (PWSTR)(pcc+1);
               if (pszString[pcc->dwStringSize/sizeof(WCHAR)-1]) {
                  // not null terminated
                  delete pMem;
                  break;
               }

               // remove from the list
               BOOL fIncludeMem = FALSE;
               if (pcc->dwDataSize)
                  fIncludeMem = TRUE;

               // if not paid or feature turned off, then ignore by setting fIncludeMem = FALSE
               if (!gfRenderCache || !RegisterMode())
                  fIncludeMem = FALSE;

               if (dwType == CIRCUMREALITYPACKET_CLIENTCACHETTS)
                  gpMainWindow->ReceivedTTSCache (pMem->p, pMem->m_dwCurPosn);
               else
                  gpMainWindow->ReceivedDownloadRequest (pszString, fIncludeMem ? pMem : NULL);

               delete pMem;
            }
            break;

         case CIRCUMREALITYPACKET_CLIENTREPLYWANTTTS:
         case CIRCUMREALITYPACKET_CLIENTREPLYWANTRENDER:
            {
               // get the memory
               pMem = m_pPacket->PacketGetMem ();
               if (!pMem)
                  break;

               // add this to the queue
               EnterCriticalSection (&m_CritSec);
               if (m_fShuttingDown)
                  delete pMem;
               else {
                  THREADLOWMSG tlm;
                  memset (&tlm, 0, sizeof(tlm));
                  tlm.dwType = dwType;
                  tlm.pMem = pMem;
                  m_lTHREADLOWMSG.Add (&tlm);
                  SetEvent (m_hSignalToThreadLow);
               }
               LeaveCriticalSection (&m_CritSec);
            }
            break;

#if 0 // deal with elsewhere
         case CIRCUMREALITYPACKET_CLIENTREPLYWANTTTS:
            {
               // get the memory
               pMem = m_pPacket->PacketGetMem ();
               if (!pMem)
                  break;


               // NOTE: For TTS, don't check QueuedToSend() because data not big enough

               // ignore this message is not registered
               // ignore this message if have internet turned off
               if (!gfRenderCache || !RegisterMode()) {
                  delete pMem;
                  break;
               }

               PCIRCUMREALITYPACKETCLIENTREPLYWANT pw;
               if (pMem->m_dwCurPosn < sizeof(*pw)) {
                  // shouldnt happen
                  delete pMem;
                  break;
               }

               // make sure memory large enough
               pw = (PCIRCUMREALITYPACKETCLIENTREPLYWANT) pMem->p;
               if ((pMem->m_dwCurPosn < sizeof(*pw) + pw->dwStringSize) || (pw->dwStringSize < sizeof(WCHAR)) ) {
                  delete pMem;
                  break;
               }

               // if server doesn't want it then done
               if (!pw->fWant) {
                  delete pMem;
                  break;
               }

               // make sure string OK
               PWSTR pszString = (PWSTR)(pw+1);
               if (pszString[pw->dwStringSize/sizeof(WCHAR)-1]) {
                  // not null terminated
                  delete pMem;
                  break;
               }

               // send it
               gpMainWindow->SendWaveToServer (pszString, pw->dwQuality);

               // done
               delete pMem;
            }
            break;
#endif // 0

#if 0 // handled in low-pri thread
         case CIRCUMREALITYPACKET_CLIENTREPLYWANTRENDER:
            {
               // get the memory
               pMem = m_pPacket->PacketGetMem ();
               if (!pMem)
                  break;

               // if already have data queued to send, then ignore this message since
               // dont want to overload the queue
               // BUGFIX - Was ignoring if anything queued, but now let queue up
               // to 2 meg
               if (m_pPacket && (m_pPacket->QueuedToSend(PACKCHAN_RENDER) > MAXQUEUETOSEND_RENDER)) {
                  delete pMem;
                  break;
               }

               // ignore this message is not registered
               // ignore this message if have internet turned off
               if (!gfRenderCache || !RegisterMode()) {
                  delete pMem;
                  break;
               }

               PCIRCUMREALITYPACKETCLIENTREPLYWANT pw;
               if (pMem->m_dwCurPosn < sizeof(*pw)) {
                  // shouldnt happen
                  delete pMem;
                  break;
               }

               // make sure memory large enough
               pw = (PCIRCUMREALITYPACKETCLIENTREPLYWANT) pMem->p;
               if ((pMem->m_dwCurPosn < sizeof(*pw) + pw->dwStringSize) || (pw->dwStringSize < sizeof(WCHAR)) ) {
                  delete pMem;
                  break;
               }

               // if server doesn't want it then done
               if (!pw->fWant) {
                  delete pMem;
                  break;
               }

               // make sure string OK
               PWSTR pszString = (PWSTR)(pw+1);
               if (pszString[pw->dwStringSize/sizeof(WCHAR)-1]) {
                  // not null terminated
                  delete pMem;
                  break;
               }


               // make sure pr->dwQuality is what we currently have set
               int iResolution;
               int iQualityMono = gpMainWindow->QualityMonoGet(&iResolution);
               if (iQualityMono < 0) {
                  delete pMem;
                  break;
               }
               if ((DWORD)iQualityMono != pw->dwQuality) {
                  delete pMem;
                  break;
               }


               // load in the high quality version, if not that, assume that should send
               // in the low-quality version
               // BUGFIX - Only save highest quality
               DWORD dwQuality;
               if ((gpMainWindow->m_iResolution == gpMainWindow->m_iResolutionLow) && (gpMainWindow->m_dwShadowsFlags == gpMainWindow->m_dwShadowsFlagsLow) && (gpMainWindow->m_fLipSync == gpMainWindow->m_fLipSyncLow))
                  dwQuality = 0;
               else
                  dwQuality = 1;

               CImageStore ImageStore;
               CMem memBinary;
               //for (dwQuality = NUMIMAGECACHE-1; dwQuality < NUMIMAGECACHE; dwQuality--) {
               // try to load from megafile
               if (!ImageStore.Init (&gpMainWindow->m_amfImages[dwQuality], pszString))
                  break;

               // if loaded, get the binary
               if (!ImageStore.ToBinary (TRUE, &memBinary)) {
                  memBinary.m_dwCurPosn = 0;
                  break;
               }

                  // else, saved it to binary
                  //break;
               //}

               // dont send image unless resolution exactly matches one
               // of the images for the quality setting
               fp fScale = ResolutionToRenderScale (iResolution);
               DWORD dwAspect, dwWidth, dwHeight;
               // look through all the aspect ratios, including 10
               for (dwAspect = 0; dwAspect < 11; dwAspect++) {
                  RenderSceneAspectToPixelsInt  ((int)dwAspect, fScale, &dwWidth, &dwHeight);
                  if ((dwWidth == ImageStore.Width()) && (dwHeight == ImageStore.Height()))
                     break;
               } // dwAspect
               // NOTE: Because of this test, things like books (with text), won't ever be
               // cached because their resolution is automatically bumped up
               if (dwAspect >= 11) {
                  // resolution doesn't match, so don't upload it, just in case
                  delete pMem;
                  break;
               }

               if (memBinary.m_dwCurPosn) {
                  CMem memAll;
                  size_t dwNeed = sizeof(CIRCUMREALITYPACKETSEVERCACHE) + pw->dwStringSize + memBinary.m_dwCurPosn;
                  if (memAll.Required (dwNeed) && m_pPacket) {
                     PCIRCUMREALITYPACKETSEVERCACHE prw = (PCIRCUMREALITYPACKETSEVERCACHE) memAll.p;
                     memset (prw, 0, sizeof(*prw));
                     prw->dwQuality = pw->dwQuality;
                     prw->dwStringSize = pw->dwStringSize;
                     prw->dwDataSize = (DWORD) memBinary.m_dwCurPosn;

                     memcpy (prw+1, pszString, prw->dwStringSize);
                     memcpy ((PBYTE)(prw+1) + prw->dwStringSize, memBinary.p, prw->dwDataSize);

                     m_pPacket->PacketSend (CIRCUMREALITYPACKET_SERVERCACHERENDER, prw, (DWORD) dwNeed);

#ifdef _DEBUG
                     OutputDebugStringW (L"\r\nCInternetThread::Sending ServerCacheRender message.");
#endif
                  }
               } // if have image to write

               delete pMem;
            }
            break;
#endif // 0

         case CIRCUMREALITYPACKET_VOICECHAT:
            {
               pMem = new CMem;
               PCMMLNode2 pNode = m_pPacket->PacketGetMML (0, pMem);
               if (!pNode) {
                  delete pMem;
                  break;
               }

               PCMMLNode2 pSub = NULL;
               PWSTR psz;
               pNode->ContentEnum (0, &psz, &pSub);
               pNode->ContentRemove (0, FALSE);
               delete pNode;
               pNode = pSub;

               // send down to be spoken
               if (pNode && gpMainWindow && gpMainWindow->m_pAT) {
                  // log this
                  gpMainWindow->TranscriptString (2, NULL, NULL, NULL, NULL, pNode, pMem);

                  LARGE_INTEGER iTime;
                  QueryPerformanceCounter (&iTime);
                  gpMainWindow->m_pAT->VoiceChat (pNode, pMem, iTime, TRUE);
               }
               else {
                  if (pMem)
                     delete pMem;
                  if (pNode)
                     delete pNode;
               }
            }
            break;

         case CIRCUMREALITYPACKET_FILEDELETE:
            {
               // get the memory
               pMem = m_pPacket->PacketGetMem ();
               if (!pMem)
                  break;

               PWSTR psz = (PWSTR)pMem->p;
               DWORD dwLeft = (DWORD)pMem->m_dwCurPosn / sizeof(WCHAR);

               BOOL fDeleted = FALSE;
               while (dwLeft) {
                  // remember start
                  PWSTR pszStart = psz;

                  // find the end
                  for (; psz[0] && dwLeft; psz++, dwLeft--);
                  if (!dwLeft)
                     break;   // shouldnt happen. probablly hacker
                  psz++;
                  dwLeft--;

                  // delete this
                  gpMainWindow->m_mfFiles.Delete (pszStart);

                  // BUGFIX - Because "LibraryUser" always seems to get modified by this, if
                  // it is changed, then still delete, but DON'T clear the image cache
                  if (!_wcsicmp(pszStart, L"LibraryUser"))
                     continue;

                  fDeleted = TRUE;
               }

               // if delete any files then want to clear image cache
               if (fDeleted) {
                  // load resolution info that was there
                  __int64 iSize;
                  PVOID prfi = gpMainWindow->m_amfImages[0].Load (gpszResolutionFileInfo, &iSize);

                  DWORD dwQuality;
                  for (dwQuality = 0; dwQuality < NUMIMAGECACHE; dwQuality++)
                     gpMainWindow->m_amfImages[dwQuality].Clear();

                  // resave the image info
                  if (prfi) {
                     gpMainWindow->m_amfImages[0].Save (gpszResolutionFileInfo, prfi, iSize);
                     MegaFileFree (prfi);
                  }
               }

               // free up the memory
               delete pMem;

               // the first time that get this will need to send a message
               // to the image and audio processing queues to have it start
               gpMainWindow->m_pRT->CanLoad3D();
               gpMainWindow->m_pAT->CanLoadTTS();

            }
            break;

         default:
            // shouldnt happen
            pMem = m_pPacket->PacketGetMem ();
            if (pMem)
               delete pMem;
            break;

         }

      }

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

   if (m_fConnectRemote) {
      shutdown (m_iSocket, SD_BOTH);
      closesocket (m_iSocket);
      // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
      // WSACleanup ();
   }
}


/*************************************************************************************
CInternetThread::Connect- This is an initialization function. It tries to connect
to the given server. If it fails it returns FALSE. If it succedes, it returns
TRUE and sets up a thread that constantly updates m_pPacket with the latest info.

NOTE: Call this from the MAIN thread.

inputs
   PWSTR          pszFile - File (CRF or CRK) that using.
   BOOL           fRemote - If TRUE using a remote connection
   DWORD          dwIP - IP address to use (if remote)
   DWORD          dwPort - Port to use (if remote)
   HWND           hWndNotify - When a new message comes in, this will receive a post
                     indicating there's a new message. wParam will be the message iD,
                     lParam will be a pNode that MUST be freed
   DWORD          dwMessage - This is the WM_ message that's posted to indicate the
                     new message.
   int            *piWinSockErr - Where the winsock error will be written if there's an error
                  If there's an error, m_szError will be filled in.
returns
   BOOL - TRUE if connected and thread created. FALSE if failed to connect and
      no thread created.
*/
BOOL CInternetThread::Connect (PWSTR pszFile, BOOL fRemote, DWORD dwIP, DWORD dwPort, HWND hWndNotify, DWORD dwMessage,
                               int *piWinSockErr)
{
   m_szError[0] = 0;

   if (m_hThread)
      return FALSE;  // cant call a second time

   *piWinSockErr = m_iWinSockErr = 0;

   m_hWndNotify = hWndNotify;
   m_dwMessage = dwMessage;
   m_fConnected = FALSE;

   wcscpy (m_szConnectFile, pszFile);
   m_fConnectRemote = fRemote;
   m_dwConnectIP = dwIP;
   m_dwConnectPort = dwPort;

   // create the low priority thread
   DWORD dwID;
   m_hSignalToThreadLow = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hThreadLow = CreateThread (NULL, ESCTHREADCOMMITSIZE, InternetThreadProcLow, this, 0, &dwID);
   if (!m_hThreadLow) {
      CloseHandle (m_hSignalToThreadLow);
      m_hSignalToThreadLow = NULL;
      return FALSE;
   }
   SetThreadPriority (m_hThread, VistaThreadPriorityHack(THREAD_PRIORITY_LOWEST));

   // create all the events
   m_hSignalFromThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hSignalToThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hThread = CreateThread (NULL, ESCTHREADCOMMITSIZE, InternetThreadProc, this, 0, &dwID);
   if (!m_hThread) {
      // shut down the low-pri thread
      EnterCriticalSection (&m_CritSec);
      m_fShuttingDown = TRUE;
      LeaveCriticalSection (&m_CritSec);

      SetEvent (m_hSignalToThreadLow);
      WaitForSingleObject (m_hThreadLow, INFINITE);
      CloseHandle (m_hThreadLow);
      CloseHandle (m_hSignalToThreadLow);

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
      EnterCriticalSection (&m_CritSec);
      m_fShuttingDown = TRUE;
      LeaveCriticalSection (&m_CritSec);

      // free low pri thread
      SetEvent (m_hSignalToThreadLow);
      WaitForSingleObject (m_hThreadLow, INFINITE);
      CloseHandle (m_hThreadLow);
      CloseHandle (m_hSignalToThreadLow);


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

   EnterCriticalSection (&m_CritSec);
   m_fShuttingDown = TRUE;
   LeaveCriticalSection (&m_CritSec);

   // free low-pri thread
   SetEvent (m_hSignalToThreadLow);
   WaitForSingleObject (m_hThreadLow, INFINITE);
   CloseHandle (m_hThreadLow);
   CloseHandle (m_hSignalToThreadLow);
   
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

   // free up message queue for low-pri thread
   DWORD i;
   EnterCriticalSection (&m_CritSec);
   PTHREADLOWMSG ptlm = (PTHREADLOWMSG) m_lTHREADLOWMSG.Get(0);
   for (i = 0; i < m_lTHREADLOWMSG.Num(); i++, ptlm++) {
      if (ptlm->pMem)
         delete ptlm->pMem;
   } // i
   m_lTHREADLOWMSG.Clear();
   LeaveCriticalSection (&m_CritSec);

   return TRUE;
}



/*************************************************************************************
CInternetThread::FileRequest - Requsts that a file be downloaded.

inputs
   PWSTR          pszFile - File
   BOOL           fIgnoreDir - If TRUE then ignore the directory.
                              Used for M3D's library files
returns
   BOOL - TRUE if message was send, FALSE if it wasn't because server has already
         replied saying that file no longer exists
*/
BOOL CInternetThread::FileRequest (PWSTR pszFile, BOOL fIgnoreDir)
{
   BOOL fRet= FALSE;

   EnterCriticalSection (&m_CritSec);

   DWORD i;

   // if it matches one of the files that we are getting from the application
   // directory then pretend its sent
   DWORD dwLen = (DWORD)wcslen(pszFile);
   for (i = 0; i < gpMainWindow->m_lDontDownloadFile.Num(); i++) {
      // compare size
      DWORD dwSize = (DWORD)gpMainWindow->m_lDontDownloadFile.Size(i) / sizeof(WCHAR) - 1;
      if (dwSize > dwLen)
         continue; // couldnt be this one

      // compare string
      PWSTR psz = (PWSTR) gpMainWindow->m_lDontDownloadFile.Get(i);
      if (_wcsicmp(psz, pszFile + (dwLen - dwSize)))
         continue; // mistmatched name
      if ((dwSize < dwLen) && (pszFile[dwLen-dwSize-1] != L'\\'))
         continue; // backslash problems

#if 0 // def _DEBUG
      if (!_wcsicmp(pszFile, L"LibraryUser"))
         fRet = fRet;
#endif

      // obviously, wouldnt be here if didnt exist, so try to copy over
      if (gpMainWindow->m_mfFiles.SaveAs (pszFile, (PWSTR)gpMainWindow->m_lDontDownloadOrig.Get(i))) {
         fRet = TRUE;
         goto done;
      }
   } // i

   // if it's already on the bad tree then error
   if (m_tFileBad.Find (pszFile))
      goto done;

   // if it's already on the list of requested files then error
   for (i = 0; i < m_lFileRequest.Num(); i++)
      if (!_wcsicmp(pszFile, (PWSTR)m_lFileRequest.Get(i))) {
         fRet = TRUE;
         goto done;  // already on requested list, so just wait
      }

   // else, request
   _ASSERTE (m_pPacket);
   fRet = m_pPacket ? m_pPacket->PacketSend (
      fIgnoreDir ? CIRCUMREALITYPACKET_FILEREQUESTIGNOREDIR : CIRCUMREALITYPACKET_FILEREQUEST,
      pszFile, ((DWORD)wcslen(pszFile)+1)*sizeof(WCHAR)) : FALSE;
   if (fRet)
      m_lFileRequest.Add (pszFile, (wcslen(pszFile)+1)*sizeof(WCHAR));

done:
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}



/*************************************************************************************
CInternetThread::FileCheckRequest - Sees if a request has for a file has been handled.

inputs
   PWSTR          pszFile - File
returns
   BOOL - TRUE if the request has been handled, FALSE if it's still pending
*/
BOOL CInternetThread::FileCheckRequest (PWSTR pszFile)
{
   BOOL fRet= FALSE;

   EnterCriticalSection (&m_CritSec);

   // if it's already on the bad tree then error
   if (m_tFileBad.Find (pszFile)) {
      fRet = TRUE;
      goto done;
   }

   // else, not marked as bad, so we're still kicking
   // NOTE: If it had been handled then the a function calling FileCheckRequest()
   // would have called into the megafile() to find the file first
   fRet = FALSE;
#if 0 // dead code
   // if it's already on the list of requested files then error
   DWORD i;
   for (i = 0; i < m_lFileRequest.Num(); i++)
      if (!_wcsicmp(pszFile, (PWSTR)m_lFileRequest.Get(i))) {
         fRet = FALSE;
         goto done;  // already on requested list, so just wait
      }

   // else, it's not on the bad list and not on the to-be-send list, so
   // it must be done
   fRet= TRUE;
#endif // 0

done:
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}



/*************************************************************************************
CInternetThread::BinaryDataRefresh - Called when receives a binary data refresh
message.

This:
   - Removes the bad files
   - Deletes the file as its stored
   - Looks through all the pre-rendered images and deletes ones refrencing this file

inputs
   PWSTR          pszFile - File
returns
   none
*/
void CInternetThread::BinaryDataRefresh (PWSTR pszFile)
{
#if 0 // def _DEBUG
   DWORD k;
   if (!_wcsicmp(pszFile, L"LibraryUser"))
      k = 1;
#endif

   EnterCriticalSection (&m_CritSec);
   m_tFileBad.Remove (pszFile);
   gpMainWindow->m_mfFiles.Delete (pszFile);
   // NOTE: Not clearing the image cache since just .jpg images shouldnt be cached
   LeaveCriticalSection (&m_CritSec);

   // need to sanitize this string since that's the way it will be written
   CMem memSan;
   MemZero (&memSan);
   MemCatSanitize (&memSan, pszFile);
   PWSTR pszFile2 = (PWSTR)memSan.p;

   // enumerate all the files with this in it
   CListVariable lv;
   DWORD i, dwQuality;
   for (dwQuality = 0; dwQuality < NUMIMAGECACHE; dwQuality++) {
      gpMainWindow->m_amfImages[dwQuality].Enum (&lv, NULL);
      for (i = 0; i < lv.Num(); i++) {
         PWSTR psz = (PWSTR)lv.Get(i);
         if (MyStrIStr (psz, pszFile) || MyStrIStr (psz, pszFile2))
            gpMainWindow->m_amfImages[dwQuality].Delete (psz);
      } // i
   } // dwQuality
}
