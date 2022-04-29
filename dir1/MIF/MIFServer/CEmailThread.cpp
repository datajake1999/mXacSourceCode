/*************************************************************************************
CEmailThread.cpp - Used to send E-mail to a pop server.

begun 5/10/05 by Mike Rozak.
Copyright 2005 by Mike Rozak. All rights reserved
*/

// #define USEIPV6      // BUGFIX - Need this to include winsock2

#ifdef USEIPV6
#include <winsock2.h>
#endif

#include <windows.h>
#include <objbase.h>
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



/*
 * WinSock 2 extension -- manifest constants for shutdown()
 */
#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02

/*************************************************************************************
MegaFileThreadProc - Thread that handles the internet.
*/
DWORD WINAPI EmailThreadProc(LPVOID lpParameter)
{
   PCEmailThread pThread = (PCEmailThread) lpParameter;

   pThread->ThreadProc ();

   return 0;
}


/*************************************************************************************
CEmailThread::Constructor and destructor
*/
CEmailThread::CEmailThread (void)
{
   m_fWantToQuit = FALSE;

   m_hSignalFromThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hSignalToThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   InitializeCriticalSection (&m_CritSec);

   DWORD dwID;
   m_hThread = CreateThread (NULL, 0, EmailThreadProc, this, 0, &dwID);
      // NOTE: Not using ESCTHREADCOMMITSIZE because want to be as stable as possible

   // lower thread priority for rendering so audio wont break up.
   if (m_hThread)
      SetThreadPriority (m_hThread, VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL));

}

CEmailThread::~CEmailThread (void)
{
   if (m_hThread) {
      EnterCriticalSection (&m_CritSec);
      m_fWantToQuit = TRUE;
      SetEvent (m_hSignalToThread);
      LeaveCriticalSection (&m_CritSec);
      WaitForSingleObject (m_hThread, INFINITE);

      // delete
      DeleteObject (m_hThread);
   }
   if (m_hSignalFromThread)
      DeleteObject (m_hSignalFromThread);
   if (m_hSignalToThread)
      DeleteObject (m_hSignalToThread);

   // NOTE: By time get here, queue will have been emptied

   DeleteCriticalSection (&m_CritSec);
}


/*************************************************************************************
CEmailThread::Mail - Sends mail using the thread proc.

inputs
   PWSTR       pszDomain - Where this is being sent from, such as "bigpond.com"
   PWSTR       pszSMTPServer - Server name, such as "mail.bigpond.com"
   PWSTR       pszEmailTo - Email address, such as "mike@mxac.com.au"
   PWSTR       pszEmailFrom - Email address of sender, such as "autoreply@mxac.com.au"
   PWSTR       pszNameFrom - Name of sender, such as "Auto reply"
   PWSTR       pszSubject - Message subject
   PWSTR       pszMessage - Message to send
   PWSTR       pszAuthUser - Authorization user. Can be NULL
   PWSTR       pszAuthPassword - Authorization password. Can be NULL.
returns
   BOOL - TRUE indicatest that it was added to the queue, NOT that it was successful
*/
BOOL CEmailThread::Mail (PWSTR pszDomain, PWSTR pszSMTPServer, PWSTR pszEmailTo, PWSTR pszEmailFrom,
   PWSTR pszNameFrom, PWSTR pszSubject, PWSTR pszMessage,
   PWSTR pszAuthUser, PWSTR pszAuthPassword)
{
   EnterCriticalSection (&m_CritSec);
   m_lSMTPServer.Add (pszSMTPServer, (wcslen(pszSMTPServer)+1)*sizeof(WCHAR));
   m_lEmailTo.Add (pszEmailTo, (wcslen(pszEmailTo)+1)*sizeof(WCHAR));
   m_lEmailFrom.Add (pszEmailFrom, (wcslen(pszEmailFrom)+1)*sizeof(WCHAR));
   m_lNameFrom.Add (pszNameFrom, (wcslen(pszNameFrom)+1)*sizeof(WCHAR));
   m_lMessage.Add (pszMessage, (wcslen(pszMessage)+1)*sizeof(WCHAR));
   m_lSubject.Add (pszSubject, (wcslen(pszSubject)+1)*sizeof(WCHAR));
   m_lDomain.Add (pszDomain, (wcslen(pszDomain)+1)*sizeof(WCHAR));
   if (!pszAuthUser)
      pszAuthUser = L"";
   m_lAuthUser.Add (pszAuthUser, (wcslen(pszAuthUser)+1)*sizeof(WCHAR));
   if (!pszAuthPassword)
      pszAuthPassword = L"";
   m_lAuthPassword.Add (pszAuthPassword, (wcslen(pszAuthPassword)+1)*sizeof(WCHAR));

   if (m_hSignalToThread)
      SetEvent (m_hSignalToThread);
   LeaveCriticalSection (&m_CritSec);
   return TRUE;
}



/*************************************************************************************
CEmailThread::ReceiveLine - Keeps receiving until a line comes in.

inputs
   int         iSocket - socket
   PCMem       pMem - Filled with the string, incluing terminating \r\n
returns
   BOOL - TRUE if success, FALSE if failure
*/
BOOL CEmailThread::ReceiveLine (int iSocket, PCMem pMem)
{
   if (!m_memRecieve.Required(2048))
      return FALSE;
   DWORD i;

   while (TRUE) {
      PSTR psz = (PSTR) m_memRecieve.p;
      DWORD dwLen = (DWORD)m_memRecieve.m_dwCurPosn;

      // look for \r\n
      for (i = 1; i < dwLen; i++)
         if ((psz[i] == '\n') && (psz[i-1] == '\r')) {
            // copy over string
            if (!pMem->Required (i+2))
               return FALSE;
            memcpy (pMem->p, psz, i+1);
            ((PSTR)pMem->p)[i+1] = 0;

            // remove from buffer
            memmove (psz, psz + (i+1), m_memRecieve.m_dwCurPosn - (i+1));
            m_memRecieve.m_dwCurPosn -= (i+1);

            return TRUE;
         }

      // else, cant find \r\n, so get buffer
      int iRet = recv (iSocket, psz + m_memRecieve.m_dwCurPosn, (DWORD)(m_memRecieve.m_dwAllocated - m_memRecieve.m_dwCurPosn), 0);
      if ((iRet <= 0) || (iRet == SOCKET_ERROR))
         return FALSE;  // not receiving any more
      m_memRecieve.m_dwCurPosn += (DWORD)iRet;
   } // while TRUE
}



/*************************************************************************************
CEmailThread::EncodeMessageText - Cleans up the message text.

inputs
   PWSTR          pszOrig - Original message
   PCMem          pMem - Filled with encoded version
returns
   BOOL - TRUE if success
*/
BOOL CEmailThread::EncodeMessageText (PWSTR pszOrig, PCMem pMem)
{
   pMem->m_dwCurPosn = 0;

   // find a line
   DWORD i, dwLen, dwLastSpace;
   BOOL fNewline;

   while (TRUE) {
      fNewline = FALSE;
      dwLastSpace = (DWORD)-1;
      for (i = 0; ; i++) {
         // check for end of all data
         if (!pszOrig[i]) {
            dwLen = i;
            break;
         }

         // if too long then break
         if (i > 65) {
            if ((dwLastSpace != (DWORD)-1) || (dwLastSpace > i/2)) {
               // break at the last space
               dwLen = dwLastSpace;
               i = dwLastSpace + 1; // so go back to that
               fNewline = TRUE;
               break;
            }
            else {
               // break here
               dwLen = i;
               fNewline = TRUE;
               break;
            }
         }

         // if space then note
         if (iswspace (pszOrig[i]))
            dwLastSpace = i;

         // check for \r\n
         if (pszOrig[i] == '\r') {
            dwLen = i;
            if (pszOrig[i+1] == '\n')
               i++;
            i++;
            fNewline = TRUE;
            break;
         }
         if (pszOrig[i] == '\n') {
            dwLen = i;
            if (pszOrig[i+1] == '\r')
               i++;
            i++;
            fNewline = TRUE;
            break;
         }
      } // i

      // if there's any text add that
      if (dwLen) {
         // if starts with period, prepend another period
         if (pszOrig[i] == L'.')
            pMem->CharCat (L'.');

         pMem->StrCat (pszOrig, dwLen);
      }
      if (fNewline)
         pMem->StrCat (L"\r\n", 2);

      // advance
      pszOrig += i;
      if (!pszOrig[0])
         break;
   } // while TRUE

   pMem->CharCat (0);   // null terminate

   return TRUE;

}


/*************************************************************************************
StringToBase64 - Converts an ANSI string to base 64.

inputs
   char     *psz - Original string
   PCMem    pMem - Filled with the base-64 ANSI string. NULL terminated
returns
   none
*/
void StringToBase64 (char *psz, PCMem pMem)
{
   pMem->m_dwCurPosn = 0;

   char *pszBase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

   DWORD dwLen = (DWORD)strlen(psz);
   DWORD i;
   char *pszAdd;
   for (i = 0; i < dwLen; i += 3) {
      DWORD dwVal =
         ((DWORD)(BYTE)psz[i] << 16) |
         ((DWORD)(BYTE)psz[min(i+1,dwLen-1)] << 8) |
         ((DWORD)(BYTE)psz[min(i+2,dwLen-1)]);

      if (!pMem->Required (pMem->m_dwCurPosn + 4))
         return;
      pszAdd = (char*)pMem->p + pMem->m_dwCurPosn;

      // add to memory
      pszAdd[0] = pszBase64[(dwVal >> 18) & 0x3f];
      pszAdd[1] = pszBase64[(dwVal >> 12) & 0x3f];
      pszAdd[2] = (i+1 < dwLen) ? pszBase64[(dwVal >> 6) & 0x3f] : '=';
      pszAdd[3] = (i+2 < dwLen) ? pszBase64[(dwVal >> 0) & 0x3f] : '=';
      pMem->m_dwCurPosn += 4;
   } // i

   // final NULL
   if (!pMem->Required (pMem->m_dwCurPosn + 1))
      return;
   pszAdd = (char*)pMem->p + pMem->m_dwCurPosn;
   pszAdd[0] = 0;
}


/*************************************************************************************
CEmailThread::ThreadProc - Processes the thread
*/
void CEmailThread::ThreadProc (void)
{
   BOOL fInitialized = FALSE;
   CMem memANSI, memReceive, memScratch, memEncode, memLog, memUni;
   int iWinSockErr, iSocket;
   PSTR pasz;

   while (TRUE) {
      EnterCriticalSection (&m_CritSec);
      if (!m_lSMTPServer.Num()) {
         // nothing left
         if (m_fWantToQuit) {
            LeaveCriticalSection (&m_CritSec);
            break;
         }

         // else, wait
         LeaveCriticalSection (&m_CritSec);
         WaitForSingleObject (m_hSignalToThread, INFINITE);
         continue;
      }

      // else, have something to send

      // remember the params
      PWSTR pszSMTPServer = (PWSTR)m_lSMTPServer.Get(0);
      PWSTR pszEmailTo = (PWSTR)m_lEmailTo.Get(0);
      PWSTR pszEmailFrom = (PWSTR)m_lEmailFrom.Get(0);
      PWSTR pszNameFrom = (PWSTR)m_lNameFrom.Get(0);
      PWSTR pszMessage = (PWSTR)m_lMessage.Get(0);
      if (!EncodeMessageText (pszMessage, &memEncode)) {
         m_lSMTPServer.Remove (0);
         m_lEmailTo.Remove (0);
         m_lEmailFrom.Remove (0);
         m_lNameFrom.Remove (0);
         m_lMessage.Remove (0);
         m_lSubject.Remove (0);
         m_lDomain.Remove (0);
         m_lAuthUser.Remove (0);
         m_lAuthPassword.Remove (0);
         LeaveCriticalSection (&m_CritSec);
         continue;
      }
      pszMessage = (PWSTR)memEncode.p;

#ifdef _DEBUG
      OutputDebugStringW (L"\r\nEmail to:");
      OutputDebugStringW (pszEmailTo);
      OutputDebugStringW (L"\r\nText:");
      OutputDebugStringW ((PWSTR)memEncode.p);
#endif

      PWSTR pszSubject = (PWSTR)m_lSubject.Get(0);
      PWSTR pszDomain = (PWSTR)m_lDomain.Get(0);
      PWSTR pszAuthUser = (PWSTR)m_lAuthUser.Get(0);
      PWSTR pszAuthPassword = (PWSTR)m_lAuthPassword.Get(0);

      // log this
      MemZero (&memLog);
      MemCat (&memLog, L"Send mail:");
      MemCat (&memLog, L"\r\nDomain = ");
      MemCat (&memLog, pszDomain ? pszDomain : L"");
      MemCat (&memLog, L"\r\nSMTPServer = ");
      MemCat (&memLog, pszSMTPServer ? pszSMTPServer : L"");
      MemCat (&memLog, L"\r\nEmailTo = ");
      MemCat (&memLog, pszEmailTo ? pszEmailTo : L"");
      MemCat (&memLog, L"\r\nEmailFrom = ");
      MemCat (&memLog, pszEmailFrom ? pszEmailFrom : L"");
      MemCat (&memLog, L"\r\nNameFrom = ");
      MemCat (&memLog, pszNameFrom ? pszNameFrom : L"");
      MemCat (&memLog, L"\r\nSubject = ");
      MemCat (&memLog, pszSubject ? pszSubject : L"");
      MemCat (&memLog, L"\r\nMessage = ");
      MemCat (&memLog, pszMessage ? pszMessage : L"");
      MemCat (&memLog, L"\r\nAuthUser = ");
      MemCat (&memLog, pszAuthUser ? pszAuthUser : L"");
      MemCat (&memLog, L"\r\nAuthPassword = ");
      MemCat (&memLog, pszAuthPassword ? pszAuthPassword : L"");
      if (gpMainWindow && gpMainWindow->m_pTextLog)
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);

      DWORD dwLen = (DWORD)wcslen(pszSMTPServer) + 1 +
         (DWORD)wcslen(pszEmailTo) + 1 +
         (DWORD)wcslen(pszEmailFrom) + 1 +
         (DWORD)wcslen(pszNameFrom) + 1 +
         (DWORD)wcslen(pszMessage) + 1 +
         (DWORD)wcslen(pszSubject) + 1 +
         (DWORD)wcslen(pszDomain) + 1 +
         (DWORD)wcslen(pszAuthUser) + 1 +
         (DWORD)wcslen(pszAuthPassword) + 1
         ;

      if (!memANSI.Required (dwLen * sizeof(WCHAR))) {
         m_lSMTPServer.Remove (0);
         m_lEmailTo.Remove (0);
         m_lEmailFrom.Remove (0);
         m_lNameFrom.Remove (0);
         m_lMessage.Remove (0);
         m_lSubject.Remove (0);
         m_lDomain.Remove (0);
         m_lAuthUser.Remove (0);
         m_lAuthPassword.Remove (0);
         LeaveCriticalSection (&m_CritSec);
         continue;
      }


      // convert to ANSI
      PSTR paszSMTPServer, paszEmailTo, paszEmailFrom, paszNameFrom, paszMessage, paszDomain, paszSubject,
         paszAuthUser, paszAuthPassword;
      paszSMTPServer = (PSTR)memANSI.p;
      WideCharToMultiByte (CP_ACP, 0, pszSMTPServer, -1, paszSMTPServer, (DWORD)memANSI.m_dwAllocated, 0, 0);
      paszEmailTo = paszSMTPServer + (strlen(paszSMTPServer)+1);
      WideCharToMultiByte (CP_ACP, 0, pszEmailTo, -1, paszEmailTo, (DWORD)memANSI.m_dwAllocated, 0, 0);
      paszEmailFrom = paszEmailTo + (strlen(paszEmailTo)+1);
      WideCharToMultiByte (CP_ACP, 0, pszEmailFrom, -1, paszEmailFrom, (DWORD)memANSI.m_dwAllocated, 0, 0);
      paszNameFrom = paszEmailFrom + (strlen(paszEmailFrom)+1);
      WideCharToMultiByte (CP_ACP, 0, pszNameFrom, -1, paszNameFrom, (DWORD)memANSI.m_dwAllocated, 0, 0);
      paszMessage = paszNameFrom + (strlen(paszNameFrom)+1);
      WideCharToMultiByte (CP_ACP, 0, pszMessage, -1, paszMessage, (DWORD)memANSI.m_dwAllocated, 0, 0);
      paszDomain = paszMessage + (strlen(paszMessage)+1);
      WideCharToMultiByte (CP_ACP, 0, pszDomain, -1, paszDomain, (DWORD)memANSI.m_dwAllocated, 0, 0);
      paszSubject = paszDomain + (strlen(paszDomain)+1);
      WideCharToMultiByte (CP_ACP, 0, pszSubject, -1, paszSubject, (DWORD)memANSI.m_dwAllocated, 0, 0);
      paszAuthUser = paszSubject + (strlen(paszSubject)+1);
      WideCharToMultiByte (CP_ACP, 0, pszAuthUser, -1, paszAuthUser, (DWORD)memANSI.m_dwAllocated, 0, 0);
      paszAuthPassword = paszAuthUser + (strlen(paszAuthUser)+1);
      WideCharToMultiByte (CP_ACP, 0, pszAuthPassword, -1, paszAuthPassword, (DWORD)memANSI.m_dwAllocated, 0, 0);

      // clear out item and leave critical section
      m_lSMTPServer.Remove (0);
      m_lEmailTo.Remove (0);
      m_lEmailFrom.Remove (0);
      m_lNameFrom.Remove (0);
      m_lMessage.Remove (0);
      m_lSubject.Remove (0);
      m_lDomain.Remove (0);
      m_lAuthUser.Remove (0);
      m_lAuthPassword.Remove (0);
      LeaveCriticalSection (&m_CritSec);

      // if haven't initialized then do so
      if (!fInitialized) {
         WSADATA wsData;
         iWinSockErr = WSAStartup (MAKEWORD( 2, 2 ), &wsData);
         if (iWinSockErr) {
#ifdef _DEBUG
            OutputDebugStringW (L"\r\nFailed to initalize winsock");
#endif
            if (gpMainWindow && gpMainWindow->m_pTextLog)
               gpMainWindow->m_pTextLog->Log (L"Send mail: Failed to initalize winsock");
            continue;   // error
         }
         fInitialized = TRUE;
      }

#ifdef USEIPV6
      ADDRINFO *AI;
      char szPortNumber[64];
      sprintf (szPortNumber, "%d", 25);   // always use port 25
      int iError = getaddrinfo(paszSMTPServer, szPortNumber, NULL, &AI);
      if (iError ) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nFailed to getaddrinfo");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Failed to getaddrinfo");
      }
#else // !USEIPV6
      struct hostent *host;
      host = gethostbyname (paszSMTPServer);
      if (!host) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nFailed to gethostbyname");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Failed to gethostbyname");
         continue;
      }
      DWORD dwIP = *((DWORD*)host->h_addr_list[0]);
#endif


#ifdef USEIPV6
      iSocket = socket (AI->ai_family, SOCK_STREAM, IPPROTO_TCP);
#else
      iSocket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
      if (iSocket == INVALID_SOCKET) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nInvalid socket");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Invalid socket");
         continue;   // couldnt send
      }



#ifdef USEIPV6
      //SOCKADDR_STORAGE WinSockAddr;
      //memset (&WinSockAddr, 0, sizeof(WinSockAddr));
      if (connect (iSocket, AI->ai_addr, (int) AI->ai_addrlen)) {
#else
      struct sockaddr_in WinSockAddr;
      memset (&WinSockAddr, 0, sizeof(WinSockAddr));
      WinSockAddr.sin_family = AF_INET;
      WinSockAddr.sin_port = htons(25);   // always port 25
      WinSockAddr.sin_addr.s_addr = dwIP;
      if (connect (iSocket, (sockaddr*) &WinSockAddr, sizeof(WinSockAddr))) {
#endif
         int iError = WSAGetLastError ();
         WCHAR szTemp[64];

         shutdown (iSocket, SD_BOTH);
         closesocket (iSocket);
#ifdef _DEBUG
         swprintf (szTemp, L"\r\nconnect failed %d", (int)iError);
         OutputDebugStringW (szTemp);
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog) {
            swprintf (szTemp, L"Send mail: connect failed %d", (int)iError);
            gpMainWindow->m_pTextLog->Log (szTemp);
         }
         continue;
      }

      // get first line
      if (!ReceiveLine (iSocket, &memReceive)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nReceiveLine failed : 1");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: ReceiveLine failed : 1");
         goto done;
      }
      pasz = (PSTR)memReceive.p;
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: ReceiveLine = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }

      // send hello
      PSTR pszHello = "helo ";
      PSTR pszRN = "\r\n";
      if (!memScratch.Required (strlen(pszHello) + strlen(pszRN) + strlen(paszDomain) + 1))
         goto done;
      pasz = (PSTR)memScratch.p;
      strcpy (pasz, pszHello);
      strcat (pasz, paszDomain);
      strcat (pasz, pszRN);
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(pasz) != send (iSocket, pasz, (DWORD)strlen(pasz), 0))
         goto done;

      // wait for a line to come in, but ignore it
      if (!ReceiveLine (iSocket, &memReceive)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nReceiveLine failed : 2");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: ReceiveLine failed : 2");
         goto done;
      }
      pasz = (PSTR)memReceive.p;
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: ReceiveLine = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }

      // send login authorization
      if (paszAuthUser[0]) {
         // send "auth login"
         pasz = "AUTH LOGIN\r\n";
         if (gpMainWindow && gpMainWindow->m_pTextLog) {
            MemZero (&memLog);
            MemCat (&memLog, L"SendMail: send = ");
            memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
            MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
            MemCat (&memLog, (PWSTR)memUni.p);
            gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
         }
         if (strlen(pasz) != send (iSocket, pasz, (DWORD)strlen(pasz), 0))
            goto done;

         // wait for a line to come in
redousername:
         if (!ReceiveLine (iSocket, &memReceive)) {
   #ifdef _DEBUG
            OutputDebugStringW (L"\r\nReceiveLine failed : 2a");
   #endif
            if (gpMainWindow && gpMainWindow->m_pTextLog)
               gpMainWindow->m_pTextLog->Log (L"Send mail: ReceiveLine failed : 2a");
            goto done;
         }
         pasz = (PSTR)memReceive.p;
         if (gpMainWindow && gpMainWindow->m_pTextLog) {
            MemZero (&memLog);
            MemCat (&memLog, L"SendMail: ReceiveLine = ");
            memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
            MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
            MemCat (&memLog, (PWSTR)memUni.p);
            gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
         }

#ifdef _DEBUG
         StringToBase64 ("Username:", &memScratch);
         StringToBase64 ("Password:", &memScratch);
#endif

         // if didn't get the right prompt ("Username") then error
         char *pszResponse = "VXNlcm5hbWU6";
         if (!strstr (pasz, pszResponse)) {
            Sleep (10);
            goto redousername;   // may not have responsed in time, and may have gotten other bits
         }
         //{
         //   if (gpMainWindow && gpMainWindow->m_pTextLog)
         //      gpMainWindow->m_pTextLog->Log (L"Send mail: No request for user name");
         //   goto done;
         //}

         // send the user name
         StringToBase64 (paszAuthUser, &memScratch);
         if (!memScratch.Required (strlen((char*)memScratch.p) + strlen(pszRN) + 1))
            goto done;
         pasz = (PSTR)memScratch.p;
         strcat (pasz, pszRN);
         if (gpMainWindow && gpMainWindow->m_pTextLog) {
            MemZero (&memLog);
            MemCat (&memLog, L"SendMail: send = ");
            memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
            MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
            MemCat (&memLog, (PWSTR)memUni.p);
            gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
         }
         if (strlen(pasz) != send (iSocket, pasz, (DWORD)strlen(pasz), 0))
            goto done;

         // wait for a line to come in
redouserpassword:
         if (!ReceiveLine (iSocket, &memReceive)) {
   #ifdef _DEBUG
            OutputDebugStringW (L"\r\nReceiveLine failed : 2b");
   #endif
            if (gpMainWindow && gpMainWindow->m_pTextLog)
               gpMainWindow->m_pTextLog->Log (L"Send mail: ReceiveLine failed : 2b");
            goto done;
         }
         pasz = (PSTR)memReceive.p;
         if (gpMainWindow && gpMainWindow->m_pTextLog) {
            MemZero (&memLog);
            MemCat (&memLog, L"SendMail: ReceiveLine = ");
            memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
            MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
            MemCat (&memLog, (PWSTR)memUni.p);
            gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
         }

         // if didn't get the right prompt ("Password") then error
         pszResponse = "UGFzc3dvcmQ6";
         if (!strstr (pasz, pszResponse)) {
            Sleep (10);
            goto redouserpassword;   // may not have responsed in time, and may have gotten other bits
         }
         //{
         //   if (gpMainWindow && gpMainWindow->m_pTextLog)
         //      gpMainWindow->m_pTextLog->Log (L"Send mail: No request for password");
         //   goto done;
         //}

         // send the password
         StringToBase64 (paszAuthPassword, &memScratch);
         if (!memScratch.Required (strlen((char*)memScratch.p) + strlen(pszRN) + 1))
            goto done;
         pasz = (PSTR)memScratch.p;
         strcat (pasz, pszRN);
         if (gpMainWindow && gpMainWindow->m_pTextLog) {
            MemZero (&memLog);
            MemCat (&memLog, L"SendMail: send = ");
            memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
            MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
            MemCat (&memLog, (PWSTR)memUni.p);
            gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
         }
         if (strlen(pasz) != send (iSocket, pasz, (DWORD)strlen(pasz), 0))
            goto done;

         // wait for a line to come in
redouseraccept:
         if (!ReceiveLine (iSocket, &memReceive)) {
   #ifdef _DEBUG
            OutputDebugStringW (L"\r\nReceiveLine failed : 2c");
   #endif
            if (gpMainWindow && gpMainWindow->m_pTextLog)
               gpMainWindow->m_pTextLog->Log (L"Send mail: ReceiveLine failed : 2c");
            goto done;
         }
         pasz = (PSTR)memReceive.p;
         if (gpMainWindow && gpMainWindow->m_pTextLog) {
            MemZero (&memLog);
            MemCat (&memLog, L"SendMail: ReceiveLine = ");
            memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
            MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
            MemCat (&memLog, (PWSTR)memUni.p);
            gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
         }

         // if didn't get the right prompt ("235") then error
         pszResponse = "235";
         if (!strstr (pasz, pszResponse)) {
            Sleep (10);
            goto redouseraccept;   // may not have responsed in time, and may have gotten other bits
         }
         //{
         //   if (gpMainWindow && gpMainWindow->m_pTextLog)
         //      gpMainWindow->m_pTextLog->Log (L"Send mail: Password not accepted");
         //   goto done;
         //}

      } // if authenticiation

      // mail from
      PSTR pszMailFrom1 = "MAIL FROM: <";
      PSTR pszMailFrom2 = ">";
      if (!memScratch.Required (strlen(pszMailFrom1) + strlen(pszMailFrom2) + strlen(pszRN) + strlen(paszEmailFrom) + 1))
         goto done;
      pasz = (PSTR)memScratch.p;
      strcpy (pasz, pszMailFrom1);
      strcat (pasz, paszEmailFrom);
      strcat (pasz, pszMailFrom2);
      strcat (pasz, pszRN);
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(pasz) != send (iSocket, pasz, (DWORD)strlen(pasz), 0))
         goto done;

      // wait for a line to come in, but ignore it
      if (!ReceiveLine (iSocket, &memReceive)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nReceiveLine failed : 3");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: ReceiveLine failed : 3");
         goto done;
      }
      pasz = (PSTR)memReceive.p;
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: ReceiveLine = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }

      // mail to
      PSTR pszMailTo1 = "RCPT TO: <";
      PSTR pszMailTo2 = ">";
      if (!memScratch.Required (strlen(pszMailTo1) + strlen(pszMailTo2) + strlen(pszRN) + strlen(paszEmailTo) + 1))
         goto done;
      pasz = (PSTR)memScratch.p;
      strcpy (pasz, pszMailTo1);
      strcat (pasz, paszEmailTo);
      strcat(pasz, pszMailTo2);
      strcat (pasz, pszRN);
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(pasz) != send (iSocket, pasz, (DWORD)strlen(pasz), 0)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nSend failed : 1");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Send failed : 1");
         goto done;
      }

      // wait for a line to come in, but ignore it
      if (!ReceiveLine (iSocket, &memReceive)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nReceiveLine failed : 4");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: ReceiveLine failed : 4");
         goto done;
      }
      pasz = (PSTR)memReceive.p;
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: ReceiveLine = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }

      // send data
      PSTR pszData = "DATA\r\n";
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(pszData)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pszData, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(pszData) != send (iSocket, pszData, (DWORD)strlen(pszData), 0)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nSend failed : 2");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Send failed : 2");
         goto done;
      }

      // send to
      PSTR pszTo = "To: ";
      if (!memScratch.Required (strlen(pszTo) + strlen(pszRN) + strlen(paszEmailTo) + 1))
         goto done;
      pasz = (PSTR)memScratch.p;
      strcpy (pasz, pszTo);
      strcat (pasz, paszEmailTo);
      strcat (pasz, pszRN);
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(pasz) != send (iSocket, pasz, (DWORD)strlen(pasz), 0)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nSend failed : 3");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Send failed : 3");
         goto done;
      }

      // send from
      PSTR pszFrom1 = "From: \"";
      PSTR pszFrom2 = "\" <";
      PSTR pszFrom3 = ">";
      if (!memScratch.Required (strlen(pszFrom1) + strlen(pszFrom2) + strlen(pszFrom3) + strlen(pszRN) + strlen(paszEmailFrom) + strlen(paszNameFrom) + 1))
         goto done;
      pasz = (PSTR)memScratch.p;
      strcpy (pasz, pszFrom1);
      strcat (pasz, paszNameFrom);
      strcat (pasz, pszFrom2);
      strcat (pasz, paszEmailFrom);
      strcat (pasz, pszFrom3);
      strcat (pasz, pszRN);
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(pasz) != send (iSocket, pasz, (DWORD)strlen(pasz), 0)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nSend failed : 4");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Send failed : 4");
         goto done;
      }

      // send subject
      PSTR pszSubjectLine = "Subject: ";
      if (!memScratch.Required (strlen(pszSubjectLine) + strlen(pszRN) + strlen(paszSubject) + 1))
         goto done;
      pasz = (PSTR)memScratch.p;
      strcpy (pasz, pszSubjectLine);
      strcat (pasz, paszSubject);
      strcat (pasz, pszRN);
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(pasz) != send (iSocket, pasz, (DWORD)strlen(pasz), 0)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nSend failed : 5");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Send failed : 5");
         goto done;
      }

      // send date
      SYSTEMTIME st;
      char szTemp[256];
      PSTR paszMonth[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov"};
      memset (&st, 0, sizeof(st));
      GetSystemTime (&st);
      sprintf (szTemp, "Date: %d %s %d %d%d:%d%d:%d%d +0000\r\n",
         (int) st.wDay, paszMonth[max(1,st.wMonth)-1], (int)st.wYear,
         (int)(st.wHour / 10), (int)(st.wHour % 10),
         (int)(st.wMinute / 10), (int)(st.wMinute % 10),
         (int)(st.wSecond / 10), (int)(st.wSecond % 10));
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(szTemp)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, szTemp, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(szTemp) != send (iSocket, szTemp, (DWORD)strlen(szTemp), 0)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nSend failed : 6");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Send failed : 6");
         goto done;
      }


      // send message
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(paszMessage)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, paszMessage, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(paszMessage) != send (iSocket, paszMessage, (DWORD)strlen(paszMessage), 0)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nSend failed : 7");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Send failed : 7");
         goto done;
      }

      // final \r\n
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(pszRN)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pszRN, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(pszRN) != send (iSocket, pszRN, (DWORD)strlen(pszRN), 0)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nSend failed : 8");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Send failed : 8");
         goto done;
      }

      // send follow-up period
      PSTR pszPeriod = "\r\n.\r\n";
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(pszPeriod)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pszPeriod, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(pszPeriod) != send (iSocket, pszPeriod, (DWORD)strlen(pszPeriod), 0)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nSend failed : 9");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Send failed : 9");
         goto done;
      }

      // wait for a line to come in, but ignore it
      if (!ReceiveLine (iSocket, &memReceive)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nReceiveLine failed : 5");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: ReceiveLine failed : 5");
         goto done;
      }
      pasz = (PSTR)memReceive.p;
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: ReceiveLine = ");
         memUni.Required ((strlen(pasz)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pasz, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }

      // send quit
      PSTR pszQuit = "QUIT\r\n";
      if (gpMainWindow && gpMainWindow->m_pTextLog) {
         MemZero (&memLog);
         MemCat (&memLog, L"SendMail: send = ");
         memUni.Required ((strlen(pszQuit)+1)*sizeof(WCHAR));
         MultiByteToWideChar (CP_ACP, 0, pszQuit, -1, (PWSTR)memUni.p, (DWORD)memUni.m_dwAllocated/sizeof(WCHAR));
         MemCat (&memLog, (PWSTR)memUni.p);
         gpMainWindow->m_pTextLog->Log ((PWSTR)memLog.p);
      }
      if (strlen(pszQuit) != send (iSocket, pszQuit, (DWORD)strlen(pszQuit), 0)) {
#ifdef _DEBUG
         OutputDebugStringW (L"\r\nSend failed : 10");
#endif
         if (gpMainWindow && gpMainWindow->m_pTextLog)
            gpMainWindow->m_pTextLog->Log (L"Send mail: Send failed : 10");
         goto done;
      }

#ifdef _DEBUG
      OutputDebugStringW (L"\r\nSent successfully");
#endif
      if (gpMainWindow && gpMainWindow->m_pTextLog)
         gpMainWindow->m_pTextLog->Log (L"Send mail: Sent successfully");

done:
      shutdown (iSocket, SD_BOTH);
      closesocket (iSocket);
   } // while TRUE

   if (fInitialized)
      WSACleanup();
}

