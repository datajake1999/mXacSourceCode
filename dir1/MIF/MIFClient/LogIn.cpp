/*************************************************************************************
LogIn.cpp - Code for handling the long procedure

begun 3/9/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
//#include <winsock2.h>
//#include <winerror.h>
#include <objbase.h>
#include <dsound.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

PWSTR DeepThoughtGenerate (void);

static LANGID        gLangID = 1033;    // default language ID
//static DWORD         gdwShardIndex = 0;   // shard index used by default
static DWORD         *gpdwIP = NULL;
static DWORD         gdwLoginTimes = 0;   // number of times user has logged in
static DWORD         gdwLabComputer = 0;   // set to 1 if this is a lab computer, instead of a personal computer

static char *gpszLangID = "DefLangID";
static char *gpszShardIndex = "ShardIndex";
static WCHAR *gpszUserName = L"UserName";
static WCHAR *gpszCPUSpeed = L"CPUSpeedNew";
static WCHAR *gpszFirstTime = L"FirstTime";
static WCHAR *gpszLoginTimes = L"LoginTimes";
static WCHAR *gpszLabComputer = L"LabComputer";
static int     giVScroll = 0; // scroll for registartion page
static CListFixed glSERVERLOADQUERY;    // list of players in the servers
static BOOL gfHasRemindedToday = FALSE;   // set to TRUE if has checked for remind player to turn off

#define NUMMRU       5     // number of files in MRU

// LOGONPAGEINFO
typedef struct {
   PWSTR             pszCircumreality;        // current CRF/CRK file
   PWSTR             pszUserName;   // user name
   PWSTR             pszPassword;   // password
   PWSTR             pszPassword2;  // 2nd time ask for password
   BOOL              *pfNewAccount; // Set to TRUE if new account
   BOOL              fNeedVerify;   // set to TRUE if should verify PW again
} LOGONPAGEINFO, *PLOGONPAGEINFO;


#define MAXCPUPROCESSORS      (MAXRAYTHREAD*2)     // max number processors
// PROCSPEEDTEST - information for testing processor speed
typedef struct {
   DWORD             dwID;          // thread ID
   HANDLE            hThread;       // thread
   CRITICAL_SECTION  *pCS;          // critical section
   BOOL              fWantToQuit;   // set to TRUE when want to quit
   DWORD             *pdwCount;     // counter to increment
} PROCSPEEDTEST, *PPROCSPEEDTEST;




int AccountNewAskFile (PCEscWindow pWindow, BOOL fSkipIfOnlyOneFile);
int AccountNewDisplay (PCEscWindow pWindow, PCResTitleInfoSet pInfoSet, BOOL fCircumreality, PWSTR pszFile);
BOOL MNLPRun (PCWSTR pszDir, PCWSTR pszFile, HWND hWnd, BOOL fTestOnly);


/*******************************************************************
DetermineCPUSpeedThreadProc - Thread proc
*/
static DWORD WINAPI DetermineCPUSpeedThreadProc(LPVOID lpParameter)
{
   PPROCSPEEDTEST pPST = (PPROCSPEEDTEST)lpParameter;
   CMatrix m1, m2, m3, m4;
   CPoint p1, p2;

   DWORD i, j;
   BOOL fWantToQuit = FALSE;
   while (!fWantToQuit) {
      for (i = 0; i < 100; i++) {
         m1.Rotation ((fp)i, (fp)i/2, (fp)i/4);
         m1.Invert4 (&m2);
         m3.Translation (0.34, 12.43, 3.34);

         m1.ToXYZLLT (&p1, &p2.p[0], &p2.p[1], &p2.p[2]);

         for (j = 0; j < 10; j++) {
            m4.Multiply (&m1, &m3);
            p1.MultiplyLeft (&m4);
            m4.Add (&m1);
            m4.MultiplyLeft (&m2);
            p2.Add (&p1);

            m1.Copy (&m4);
         } // j
      } // i

      // increase the counter
      EnterCriticalSection (pPST->pCS);
      (*pPST->pdwCount) += 1;
      if (pPST->fWantToQuit)
         fWantToQuit = TRUE;
      LeaveCriticalSection (pPST->pCS);
   } // while TRUE

   return 0;
}

/****************************************************************************
DetermineCPUSpeed - Function to determine the CPU speed.

returns
   fp - Speed (log), where 0.0 = speed of a dual-code 3ghz intel processor, 1.0 = one twice as fast,
         2.0 = 4x as fast
*/
fp DetermineCPUSpeed (void)
{
   PROCSPEEDTEST aPST[MAXCPUPROCESSORS];
   memset (aPST, 0, sizeof(aPST));

   DWORD dwProcessors = HowManyProcessors (FALSE);
   dwProcessors = min(dwProcessors, MAXCPUPROCESSORS);

   HANDLE hThreadCur = GetCurrentThread ();
   int iThreadPriCur = GetThreadPriority (hThreadCur);
   SetThreadPriority (hThreadCur, VistaThreadPriorityHack(THREAD_PRIORITY_HIGHEST));

   CRITICAL_SECTION cs;
   InitializeCriticalSection (&cs);

   DWORD dwStart = GetTickCount ();

   DWORD i;
   DWORD dwCount = 0;
   for (i = 0; i < dwProcessors; i++) {
      aPST[i].pdwCount = &dwCount;
      aPST[i].fWantToQuit = FALSE;
      aPST[i].pCS = &cs;

      aPST[i].hThread = CreateThread (NULL, ESCTHREADCOMMITSIZE, DetermineCPUSpeedThreadProc, &aPST[i], 0, &aPST[i].dwID);
      SetThreadPriority (aPST[i].hThread, VistaThreadPriorityHack(THREAD_PRIORITY_HIGHEST));
   } // i

   // second
   Sleep (1000);

   DWORD dwEnd = GetTickCount() - dwStart;

   // shut down all the threads
   EnterCriticalSection (&cs);
   DWORD dwFinalTime = dwCount;
   for (i = 0; i < dwProcessors; i++)
      aPST[i].fWantToQuit = TRUE;
   LeaveCriticalSection (&cs);
   for (i = 0; i < dwProcessors; i++) {
      WaitForSingleObject (aPST[i].hThread, INFINITE);
      CloseHandle (aPST[i].hThread);
   }

   DeleteCriticalSection (&cs);
   SetThreadPriority (hThreadCur, iThreadPriCur);

   fp fSpeed = (fp)dwFinalTime * 1000.0 / (fp)dwEnd / 3500.0;
      // NOTE: Calculated 3500.0 with experimentation
   fSpeed = max(fSpeed, 0.0001);
   fSpeed = log(fSpeed) / log(2.0);

   return fSpeed;
}


/****************************************************************************
CircumrealityRegBase - return the base registry key
*/
char *CircumrealityRegBase (void)
{
   return "Software\\mXac\\Circumreality";
}

/****************************************************************************
CircumrealityMRUListEnum - Fills in the MRU list, loading it in.

inputs
   PCListVariable       pMRU - Filled in with a list of WCHAR strings for the
                        MRU list.
*/
void CircumrealityMRUListEnum (PCListVariable pMRU)
{
   pMRU->Clear();
   char szTemp[32], szName[512];
   WCHAR szwTemp[512];

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   DWORD i;
   for (i = 0; i < NUMMRU; i++) {
      DWORD dwSize, dwType;
      dwSize = sizeof(szName);
      szName[0] = 0;

      sprintf (szTemp, "MRU%d", (int) i);

      if (ERROR_SUCCESS != RegQueryValueEx (hKey, szTemp, NULL, &dwType, (LPBYTE) szName, &dwSize))
         continue;
      if (dwType != REG_SZ)
         continue;
      if (!szName[0])
         continue;

      // add it
      MultiByteToWideChar (CP_ACP, 0, szName, -1, szwTemp, sizeof(szwTemp)/sizeof(WCHAR));
      pMRU->Add (szwTemp, (wcslen(szwTemp)+1) * sizeof(WCHAR));
   }

   RegCloseKey (hKey);
}


/****************************************************************************
CircumrealityMRUListAdd - Adds an item to the top of the MRU list.

inputs
   char     *pszFile - File
   BOOL     fOnlyRemove - If TRUE, only remove from the MRU list (if it was
            there). Don't add back in at the top
returns
   BOOL - TRUE if was already on the list, FALSE if it wasnt and was added
*/
BOOL CircumrealityMRUListAdd (WCHAR *pszFile, BOOL fOnlyRemove = FALSE)
{
   CListVariable lMRU;
   CircumrealityMRUListEnum (&lMRU);

   // if name already exsits then remove it
   DWORD i;
   WCHAR *psz;
   BOOL fRet = FALSE;
   for (i = lMRU.Num()-1; i < lMRU.Num(); i--) {
      psz = (WCHAR*) lMRU.Get(i);
      if (!_wcsicmp(psz, pszFile)) {
         lMRU.Remove (i);
         fRet = TRUE;
      }
   }

   // add it
   if (!fOnlyRemove)
      lMRU.Insert (0, pszFile, (wcslen(pszFile)+1)*sizeof(WCHAR));

   // delete old ones
   while (lMRU.Num() > NUMMRU)
      lMRU.Remove (NUMMRU);

   // write to the registry
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return fRet;

   char szTemp[32], szaTemp[512];
   for (i = 0; i < NUMMRU; i++) {
      psz = (WCHAR*) lMRU.Get(i);
      if (!psz)
         psz = L"";
      WideCharToMultiByte (CP_ACP, 0, psz, -1, szaTemp, sizeof(szaTemp), 0, 0);

      sprintf (szTemp, "MRU%d", (int) i);
      RegSetValueEx (hKey, szTemp, 0, REG_SZ, (BYTE*) &szaTemp[0], (DWORD)strlen(szaTemp)+1);
   }


   RegCloseKey (hKey);
   return fRet;
}


/****************************************************************************
BeginsWithHTTP: Checks to see if a name begins with http: or https:

returns
   DWORD - Number of characters in the HTTP string, includeing slashes
*/
DWORD BeginsWithHTTP (PWSTR psz)
{
   PWSTR pszHTTP = L"http://", pszHTTPS = L"https://";
   DWORD dwHTTPLen = (DWORD)wcslen(pszHTTP), dwHTTPSLen = (DWORD)wcslen(pszHTTPS);
   if (!_wcsnicmp(psz, pszHTTP, dwHTTPLen))
      return dwHTTPLen;
   else if (!_wcsnicmp(psz, pszHTTPS, dwHTTPSLen))
      return dwHTTPSLen;
   else
      return 0;
}

/****************************************************************************
AddressToIP - Converts a string ("123.43.54.34") to an IP address.

inputs
   PWSTR          psz - String, with "123.544.34.534"
   DWORD          *pdwIP - Filled with the IP address if success
returns
   BOOL - TRUE if success, FALSE if it's not an IP address
*/
BOOL AddressToIP (PWSTR psz, DWORD *pdwIP)
{
   char szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);
   *pdwIP = inet_addr (szTemp);

   return (*pdwIP != INADDR_NONE);
}

/****************************************************************************
NameToIP - Converts a domain name to an IP address.

inputs
   PWSTR          psz - String. "www.mxac.com.au"
   DWORD          *pdwIP - Filled with the IP address if success
   int            *piErr - Filled with the error, if returns FALSE
   WCHAR          pszErr - Filled with the error (should be 512 chars)
returns
   BOOL - TRUE if success, FALSE if it's not an IP address
*/
WCHAR szMicrosoftCom[] = L"www.Microsoft.com";

BOOL NameToIP (PWSTR psz, DWORD *pdwIP, int *piErr, PWSTR pszErr)
{
   pszErr[0] = 0;
   *piErr = 0;
   char szTemp[256];
   struct hostent *host;
   WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);

   host = gethostbyname (szTemp);
   if (!host) {
      *piErr = WSAGetLastError ();

      swprintf (pszErr, L"The server may be down since can't contact %s."
         L"\r\nTechnical: gethostbyname(%s) returns %d", psz, psz, (int)*piErr);

      goto trymicrosoft;
   }
   *pdwIP = *((DWORD*)host->h_addr_list[0]);

   if (*pdwIP != INADDR_NONE)
      return TRUE;

   swprintf (pszErr, L"The server may be down since can't contact %s."
      L"\r\nTechnical: pdwIP == INADDR_NONE", psz);

trymicrosoft:
   // if already trying microsoft.com, don't recurse
   if (!_wcsicmp(psz, szMicrosoftCom))
      return FALSE;

   // else, try Microsoft
   DWORD dwIPMicrosoft;
   int iErrorMicrosoft;
   WCHAR szErrorMicrosoft[512];
   if (!NameToIP (szMicrosoftCom, &dwIPMicrosoft, &iErrorMicrosoft, szErrorMicrosoft))
      swprintf (pszErr, L"CircumReality can't access the internet. "
         L"First see if you can view some web pages, like www.Microsoft.com. If you can't, then you need to turn on your internet. "
         L"If you can, then your firewall software is blocking the CircumReality application from accessing the Internet; you'll need to tell your firewall to let CircumReality use the internet."
         L"\r\nTechnical: Can't even access the IP address of www.Microsoft.com.");

   return FALSE;
}


/****************************************************************************
MRUPage
*/
BOOL MRUPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
#if 0  // dont use
      #define  NOTEBASE       62
      #define  VOLUME         64
         // load tts in befor chime
         //EscSpeakTTS ();

         ESCMIDIEVENT aChime[] = {
            {0, MIDIINSTRUMENT (0, 72+3)}, // flute
            {0, MIDINOTEON (0, NOTEBASE+0,VOLUME)},
            {300, MIDINOTEOFF (0, NOTEBASE+0)},
            {0, MIDINOTEON (0, NOTEBASE-1,VOLUME)},
            {300, MIDINOTEOFF (0, NOTEBASE-1)},
            {0, MIDINOTEON (0, NOTEBASE+0,VOLUME)},
            {300, MIDINOTEOFF (0, NOTEBASE+0)},
            {100, MIDINOTEON (0, NOTEBASE+6,VOLUME)},
            {200, MIDINOTEOFF (0, NOTEBASE+6)},
            {100, MIDINOTEON (0, NOTEBASE-6,VOLUME)},
            {750, MIDINOTEOFF (0, NOTEBASE-6)}
         };
         EscChime (aChime, sizeof(aChime) / sizeof(ESCMIDIEVENT));
#endif // 0
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         PWSTR psz = p->psz;
         if (!psz)
            break;

         if ((psz[0] == L's') && (psz[1] == L'l') && (psz[2] == L':')) {
            // ask for number of players
            DWORD dwIndex = (DWORD)_wtoi(psz + 3);

            CListVariable lMRU;
            CircumrealityMRUListEnum (&lMRU);
            psz = (WCHAR*) lMRU.Get(dwIndex);
            if (!psz || !psz[0])
               return TRUE;

            // get the node
            BOOL fCircumreality;
            PCResTitleInfoSet pSet = PCResTitleInfoSetFromFile (psz, &fCircumreality);
            if (!pSet)
               return TRUE;
            PCResTitleInfo pti = pSet->Find (gLangID);
            if (!pti) {
               delete pSet;
               return TRUE;
            }

            SERVERLOADQUERY slq;
            memset (&slq, 0, sizeof(slq));
            CListFixed lSLQ;
            lSLQ.Init (sizeof(SERVERLOADQUERY));

            // loop over the info
            DWORD i;
            for (i = 0; i < pti->m_lPCResTitleInfoShard.Num(); i++) {
               PCResTitleInfoShard *ppis = (PCResTitleInfoShard*)pti->m_lPCResTitleInfoShard.Get(i);
               PCResTitleInfoShard pis = ppis[0];

               PWSTR pszDomain = (PWSTR)pis->m_memAddr.p;
               if (wcslen(pszDomain)+1 >= sizeof(slq.szDomain)/sizeof(WCHAR))
                  continue;   // name too long
               wcscpy (slq.szDomain, pszDomain);
               slq.dwPort = pis->m_dwPort;
               lSLQ.Add (&slq);
            }
            delete pSet;

            ServerLoadQuery ((PSERVERLOADQUERY) lSLQ.Get(0), lSLQ.Num(), pPage->m_pWindow->m_hWnd);


            // sum all the numbers up
            PSERVERLOADQUERY pslq = (PSERVERLOADQUERY)lSLQ.Get(0);
            CIRCUMREALITYSERVERLOAD ServerLoad;
            memset (&ServerLoad, 0, sizeof(ServerLoad));
            for (i = 0; i < lSLQ.Num(); i++, pslq++) {
               ServerLoad.dwConnections += pslq->ServerLoad.dwConnections;
               ServerLoad.dwMaxConnections += pslq->ServerLoad.dwMaxConnections;
            }

            WCHAR szTemp[256];
            if (ServerLoad.dwMaxConnections)
               swprintf (szTemp, L"The world has %d players out of a maximum of %d.",
                  (int) ServerLoad.dwConnections, (int) ServerLoad.dwMaxConnections);
            else
               wcscpy (szTemp, L"The world couldn't be contacted to retrieve the number of players.");
            pPage->MBInformation (szTemp);

            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszMRU = L"mru";
         DWORD dwMRULen = (DWORD)wcslen(pszMRU);

         if (!wcsncmp(psz, pszMRU, dwMRULen)) {
            CListVariable lMRU;
            CircumrealityMRUListEnum (&lMRU);
            PWSTR pszFile = (PWSTR) lMRU.Get (_wtoi(psz + dwMRULen));
            if (!pszFile)
               return TRUE;

            // else keep
            wcscpy ((PWSTR)pPage->m_pUserData, pszFile);
            pPage->Exit (L"openfile");
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"QUOTE")) {
            p->pszSubString = DeepThoughtGenerate();
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"VERSIONNUM")) {
            WCHAR szTemp[64];
            MultiByteToWideChar (CP_ACP, 0, BUILD_NUMBER, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MRULIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            WCHAR *psz;
            CListVariable lMRU;
            CircumrealityMRUListEnum (&lMRU);
            BOOL fAdded = FALSE;
            for (i = 0; i < lMRU.Num(); i++) {
               psz = (WCHAR*) lMRU.Get(i);
               if (!psz || !psz[0])
                  continue;

               // get the node
               BOOL fCircumreality;
               PCResTitleInfoSet pSet = PCResTitleInfoSetFromFile (psz, &fCircumreality);
               if (!pSet)
                  continue;
               PCResTitleInfo pInfo = pSet->Find (gLangID);
               if (!pInfo) {
                  delete pSet;
                  continue;
               }


               MemCat (&gMemTemp,
                  L"<xChoiceButton style=righttriangle name=mru");
               MemCat (&gMemTemp, (int)i);
               
               // BUGFIX - Make 1st option defcontrol
               if (!i)
                  MemCat (&gMemTemp, L" defcontrol=true accel=enter");

               MemCat (&gMemTemp,
                  L">"
                  L"<bold>");
               PWSTR pszName = (PWSTR)pInfo->m_memName.p;

               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp,
                  L"</bold>");
               MemCat (&gMemTemp,
                  L" (");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp,
                  L")<br/>");
               MemCatSanitize (&gMemTemp, (PWSTR)pInfo->m_memDescShort.p);
               MemCat (&gMemTemp,
                  fCircumreality ? L"<br/><font color=#00ff00>(No Internet, single player)</font>" :
                     L"<br/><font color=#ff0000>(Internet required, multiple players)</font>");
               MemCat (&gMemTemp,
                  L"</xChoiceButton>");

               // how many players?
               if (!fCircumreality) {
                  MemCat (&gMemTemp,
                     L"<align align=right><a href=sl:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp,
                     L">");
                  MemCat (&gMemTemp, L"How many people are playing?");
                  MemCat (&gMemTemp,
                     L"</a><br/></align>");
               }

               pszName = (PWSTR)pInfo->m_memWebSite.p;
               if (BeginsWithHTTP(pszName)) {
                  MemCat (&gMemTemp,
                     L"<p align=right><a href=\"");
                  MemCatSanitize (&gMemTemp, pszName);
                  MemCat (&gMemTemp,
                     L"\">");
                  MemCatSanitize (&gMemTemp, pszName);
                  MemCat (&gMemTemp,
                     L"</a></p>");
               }

               delete pSet;
               fAdded = TRUE;
            }

            if (fAdded)
               MemCat (&gMemTemp, L"<xbr/>");

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return FALSE;
   // BUGFIX - Was return DefPage (pPage, dwMessage, pParam);, but has enter as accel
}




#if 0 // no longer use EULA
/****************************************************************************
EULAPage
*/
BOOL EULAPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCResTitleInfoSet pSet = (PCResTitleInfoSet)pPage->m_pUserData;
   PCResTitleInfo pInfo = pSet->Find (gLangID);

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"langid");
         if (pControl) {
            CMem mem;

            // clear the existing combo
            pControl->Message (ESCM_COMBOBOXRESETCONTENT);

            MemZero (&mem);

            DWORD i;
            DWORD dwSel = 0;
            PCResTitleInfo *ppr = (PCResTitleInfo*)pSet->m_lPCResTitleInfo.Get(0);
            for (i = 0; i < pSet->m_lPCResTitleInfo.Num(); i++) {
               PCResTitleInfo pt = ppr[i];

               if (pInfo->m_lid == pt->m_lid)
                  dwSel = i;
               DWORD dwIndex = MIFLLangFind (pt->m_lid);
               PWSTR psz = MIFLLangGet (dwIndex, NULL);
               if (!psz)
                  psz = L"Unknown";
               
               MemCat (&mem, L"<elem name=");
               MemCat (&mem, (int)i);
               MemCat (&mem, L"><bold>");
               MemCatSanitize (&mem, psz);
               MemCat (&mem, L"</bold>");
               MemCat (&mem, L"</elem>");
            }

            ESCMCOMBOBOXADD lba;
            memset (&lba, 0,sizeof(lba));
            lba.pszMML = (PWSTR)mem.p;

            pControl->Message (ESCM_COMBOBOXADD, &lba);

            pControl->AttribSetInt (CurSel(), (int)dwSel);
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p || !p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"langid")) {
            DWORD dwNum = p->pszName ? _wtoi(p->pszName) : 0;
            PCResTitleInfo *ppr = (PCResTitleInfo*)pSet->m_lPCResTitleInfo.Get(dwNum);
            if (!ppr)
               return TRUE;   // nothing there
            if (ppr[0]->m_lid == gLangID)
               return TRUE;   // no change

            // else, changed
            gLangID = ppr[0]->m_lid;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"EULA")) {
            p->pszSubString = (PWSTR) pInfo->m_memEULA.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"NAME")) {
            p->pszSubString = (PWSTR) pInfo->m_memName.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DESCLONG")) {
            p->pszSubString = (PWSTR) pInfo->m_memDescLong.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WEBSITE")) {
            MemZero (&gMemTemp);

            PWSTR pszName = (PWSTR)pInfo->m_memWebSite.p;
            if (!BeginsWithHTTP(pszName)) {
               MemCat (&gMemTemp,
                  L"<a color=#8080ff href=\"");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp,
                  L"\">");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp,
                  L"</a>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return FALSE;
   // BUGFIX - Was return DefPage (pPage, dwMessage, pParam);, but has enter as accel
}
#endif


/****************************************************************************
ShardPage
*/
BOOL ShardPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCResTitleInfo pInfo = (PCResTitleInfo)pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         PWSTR psz = p->psz;
         if (!psz)
            break;

         if ((psz[0] == L's') && (psz[1] == L'l') && (psz[2] == L':')) {
            // ask for number of players
            DWORD dwIndex = (DWORD)_wtoi(psz + 3);

            PCResTitleInfoShard *pps = (PCResTitleInfoShard*) pInfo->m_lPCResTitleInfoShard.Get(dwIndex);
            if (!pps)
               return TRUE;   // shouldnt happen

            PCResTitleInfoShard pis = pps[0];

            SERVERLOADQUERY slq;
            memset (&slq, 0, sizeof(slq));
            slq.dwPort = pis->m_dwPort;

            PWSTR pszDomain = (PWSTR)pis->m_memAddr.p;
            if (wcslen(pszDomain)+1 < sizeof(slq.szDomain)/sizeof(WCHAR))
               wcscpy (slq.szDomain, pszDomain);

            ServerLoadQuery (&slq, 1, pPage->m_pWindow->m_hWnd);


            WCHAR szTemp[256];
            if (slq.ServerLoad.dwMaxConnections)
               swprintf (szTemp, L"The world has %d players out of a maximum of %d.",
                  (int) slq.ServerLoad.dwConnections, (int) slq.ServerLoad.dwMaxConnections);
            else
               wcscpy (szTemp, L"The world couldn't be contacted to retrieve the number of players.");
            pPage->MBInformation (szTemp);

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"SHARDS")) {
            MemZero (&gMemTemp);

            DWORD i;
            PCResTitleInfoShard *pps = (PCResTitleInfoShard*) pInfo->m_lPCResTitleInfoShard.Get(0);

            for (i = 0; i < pInfo->m_lPCResTitleInfoShard.Num(); i++) {
               PCResTitleInfoShard ps = pps[i];


               MemCat (&gMemTemp,
                  L"<xChoiceButton style=righttriangle href=");
               MemCat (&gMemTemp, (int)i);
               //if (i == gdwShardIndex)
               //   MemCat (&gMemTemp, L" defcontrol=true");
               MemCat (&gMemTemp,
                  L">"
                  L"<bold>");
               PWSTR pszName = (PWSTR)ps->m_memName.p;

               //if (i == gdwShardIndex)
               //   MemCat (&gMemTemp, L"<big><font color=#80ff80>");
               MemCatSanitize (&gMemTemp, pszName);
               //if (i == gdwShardIndex)
               //   MemCat (&gMemTemp, L"</font></big>");
               MemCat (&gMemTemp,
                  L"</bold><br/>");
               MemCatSanitize (&gMemTemp, (PWSTR)ps->m_memDescShort.p);
               MemCat (&gMemTemp,
                  L"</xChoiceButton>");

               // how many people playing
               MemCat (&gMemTemp, L"<small><p align=right><a href=sl:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L">How many people are playing?</a></p></small>");
            }


            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return FALSE;
   // BUGFIX - Was return DefPage (pPage, dwMessage, pParam);, but has enter as accel
}


#if 0 // no longer used
/****************************************************************************
LogonMegaUser - Fills in the string with the megafile used for the user
password.

inputs
   PWSTR          pszCircumreality - Circumreality file name
   PWSTR          pszMegaUser - Filled in with the megafile for the user
*/
void LogonMegaUser (PWSTR pszCircumreality, PWSTR pszMegaUser)
{
   // initialize name
   wcscpy (pszMegaUser, pszCircumreality);
   DWORD dwLen = wcslen(pszMegaUser);
   if ((dwLen < 4) || (pszMegaUser[dwLen-4] != L'.'))
      return;  // shouldnt happen
   pszMegaUser[dwLen-4] = 0;   // remote suffix
   wcscat (pszMegaUser, L".mf3");
}
#endif // 0

static PWSTR gpszPassword = L"Password";

#if 0 // no longer used
/****************************************************************************
LogonPasswordGet - Gets the currently saved password.

inputs
   PWSTR          pszCircumreality - Circumreality file name
   PWSTR          pszUserName - User name
   PWSTR          pszPassword - Filled with the password (64 chars)
returns
   BOOL - TRUE if managed to fill pszPassword, FALSE if couldnt
*/
BOOL LogonPasswordGet (PWSTR pszCircumreality, PWSTR pszUserName, PWSTR pszPassword)
{
   CMegaFile mf;
   WCHAR szMega[256];
   LogonMegaUser (pszCircumreality, szMega);
   if (!mf.Init (szMega, &GUID_MegaUserCache, FALSE))
      return FALSE;

   wcscpy (szMega, pszUserName);
   wcscat (szMega, L"\\Password");
   PCMMLNode2 pNode = MMLFileOpen (&mf, szMega, &GUID_MegaUserCache);
   if (!pNode)
      return FALSE;

   PWSTR psz = MMLValueGet (pNode, gpszPassword);
   if (psz)
      wcscpy (pszPassword, psz);
   else
      pszPassword[0] = 0;
   delete pNode;

   return TRUE;
}

/****************************************************************************
LogonPasswordSet - Sets the password for a user.

inputs
   PWSTR          pszCircumreality - Circumreality file name
   PWSTR          pszUserName - User name
   PWSTR          pszPassword - Password to set (64 chars)
returns
   BOOL - TRUE if managed to fill pszPassword, FALSE if couldnt
*/
BOOL LogonPasswordSet (PWSTR pszCircumreality, PWSTR pszUserName, PWSTR pszPassword)
{
   CMegaFile mf;
   WCHAR szMega[256];
   LogonMegaUser (pszCircumreality, szMega);
   if (!mf.Init (szMega, &GUID_MegaUserCache))
      return FALSE;

   wcscpy (szMega, pszUserName);
   wcscat (szMega, L"\\Password");
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return FALSE;
   pNode->NameSet (gpszPassword);
#ifndef USEPASSWORDSTORE
   pszPassword = L"JUNK";   // obscure so don't cant read from file
#endif
   if (pszPassword)
      MMLValueSet (pNode, gpszPassword, pszPassword);
   MMLFileSave (&mf, szMega, &GUID_MegaUserCache, pNode);
   delete pNode;

   return TRUE;
}
#endif // 0

#if 0 // no longer used
/****************************************************************************
LogOnPage
*/
BOOL LogOnPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PLOGONPAGEINFO plpi = (PLOGONPAGEINFO) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), plpi->pszUserName);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         DWORD dwNeed;
         if (!_wcsicmp(psz, L"name")) {
            p->pControl->AttribGet (Text(), plpi->pszUserName, 64*sizeof(WCHAR), &dwNeed);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"password")) {
            p->pControl->AttribGet (Text(), plpi->pszPassword, 64*sizeof(WCHAR), &dwNeed);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"connect") || !_wcsicmp(psz, L"newaccount")) {
            BOOL fNewAccount = !_wcsicmp(psz, L"newaccount");

            // make sure name is valid and have password
            DWORD i;
            for (i = 0; plpi->pszUserName[i]; i++) {
               if (!i && !iswalpha(plpi->pszUserName[i])) {
                  pPage->MBWarning (L"The first character of the name must be a letter.");
                  return TRUE;
               }
               if (!iswalnum(plpi->pszUserName[i])) {
                  pPage->MBWarning (L"You can only use letters and numbers in the name.");
                  return TRUE;
               }
            } // i
            if (fNewAccount && (i < 6)) {
               pPage->MBWarning (L"The name must be at least 6 characters long.");
               return TRUE;
            }

            // check the password
            if (wcslen(plpi->pszPassword) < 6) {
               pPage->MBWarning (L"Your password must be at least 6 characters long.");
               return TRUE;
            }

            // see if have information
            WCHAR szPWCur[64];
            BOOL fHavePW = LogonPasswordGet (plpi->pszCircumreality, plpi->pszUserName, szPWCur);
            (*plpi->pfNewAccount) = fNewAccount;
            plpi->fNeedVerify = !fHavePW; // if no PW then will need to ask and set
#ifdef USEPASSWORDSTORE
            if (fHavePW && wcscmp(plpi->pszPassword, szPWCur)) {
               if (fNewAccount)
                  pPage->MBWarning (L"That user account already exists, but with a different password.",
                     L"You cannot create a new one with the same name.");
               else
                  pPage->MBWarning (L"The password is wrong.",
                     L"The password you typed doesn't match the one on record.");
               return TRUE;
            }
#endif

            pPage->Exit (psz);
            return TRUE;
         }
      }
      break;
   } // switch

   return FALSE;
   // BUGFIX - Was return DefPage (pPage, dwMessage, pParam);, but has enter as accel
}
#endif // 0


/****************************************************************************
Password2Page
*/
BOOL Password2Page (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PLOGONPAGEINFO plpi = (PLOGONPAGEINFO) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         DWORD dwNeed;
         if (!_wcsicmp(psz, L"password")) {
            p->pControl->AttribGet (Text(), plpi->pszPassword2, 64*sizeof(WCHAR), &dwNeed);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"connect")) {
            // make sure password is valid
            if (wcscmp(plpi->pszPassword2, plpi->pszPassword)) {
               pPage->MBWarning (L"The password don't match.");
               return TRUE;
            }

            // else works
            pPage->Exit (L"connect");
            return TRUE;
         }
      }
      break;
   } // switch

   return FALSE;
   // BUGFIX - Was return DefPage (pPage, dwMessage, pParam);, but has enter as accel
}


/****************************************************************************
IPAddrPage
*/
BOOL IPAddrPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"ok")) {
            WCHAR szTemp[128];
            PCEscControl pControl = pPage->ControlFind (L"ipaddr");
            DWORD dwNeed;
            szTemp[0] = 0;
            pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

            if (!AddressToIP (szTemp, gpdwIP)) {
               pPage->MBWarning (L"The IP address you entered is not valid.");
               return TRUE;
            }
            pPage->Exit (L"ok");
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"WEBSITE")) {
            p->pszSubString = (PWSTR) pPage->m_pUserData;
            return TRUE;
         }
      }
      break;
   };


   return FALSE;
   // BUGFIX - Was return DefPage (pPage, dwMessage, pParam);, but has enter as accel
}




/****************************************************************************
ReadMePage
*/
BOOL ReadMePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"VERSIONNUM")) {
            WCHAR szTemp[64];
            MultiByteToWideChar (CP_ACP, 0, BUILD_NUMBER, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   } // switch

   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
MRUPageDisplay - Pulls up the MRU page and gets the CRF or MFI file that want
to use.

inputs
   PCEscWindow       pWindow - Window to use
   PWSTR             pszFile - Filled with the file name. Should be 256 chars long
returns
   BOOL - TRUE if success, FALSE if should close
*/
BOOL MRUPageDisplay (PCEscWindow pWindow, PWSTR pszFile)
{
   PWSTR pszRet;
redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLMRUPAGE, MRUPage, pszFile);

   if (pszRet && !_wcsicmp(pszRet, L"open")) {
      // NOTE: Always use the application directory
      SetLastDirectory (gszAppDir);

      PWSTR psz = pszFile;
      psz[0] = 0;
      if (!CircumrealityFileOpen (pWindow, FALSE, gLangID, psz, FALSE))
         goto redo;   // failed to open

      return TRUE;
   }
   if (pszRet && !_wcsicmp(pszRet, L"readme")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLREADME, ReadMePage);
      if (pszRet && !_wcsicmp(pszRet, L"back"))
         goto redo;
      // else, should close
      return FALSE;
   }
   if (pszRet && !_wcsicmp(pszRet, L"register")) {
      if (RegisterPageDisplay(pWindow))
         goto redo;
      else
         return FALSE;
   }

   return (pszRet && !_wcsicmp(pszRet, L"openfile"));
      // NOTE: I dont think "openfile" is used anymore
}



/****************************************************************************
LargeTTSPage
*/
BOOL LargeTTSPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;
         if (p->psz && !_wcsicmp(p->psz, L"playhq")) {
            PlaySound (NULL, ghInstance, SND_PURGE);  // stop playing existing
            PlaySound (MAKEINTRESOURCE (IDR_WAVEVOICESAMPLELARGE), ghInstance, SND_ASYNC | SND_RESOURCE);
            return TRUE;
         }
         if (p->psz && !_wcsicmp(p->psz, L"playlq")) {
            PlaySound (NULL, ghInstance, SND_PURGE);  // stop playing existing
            PlaySound (MAKEINTRESOURCE (IDR_WAVEVOICESAMPLESMALL), ghInstance, SND_ASYNC | SND_RESOURCE);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"TTSREGISTERED")) {
            BOOL fCheck;
            LargeTTSRequirements (&fCheck, NULL, NULL, NULL, NULL);
            p->pszSubString = fCheck ?
               L"<font color=#008000>You have paid</font>" :
               L"<font color=#800000>You have NOT paid</font>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TTSCIRC64")) {
            BOOL fCheck;
            LargeTTSRequirements (NULL, NULL, &fCheck, NULL, NULL);
            p->pszSubString = fCheck ?
               L"<font color=#008000>You are running it</font>" :
               L"<font color=#800000>You should download the 64-bit version</font>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TTSWIN64")) {
            BOOL fCheck;
            LargeTTSRequirements (NULL, &fCheck, NULL, NULL, NULL);
            p->pszSubString = fCheck ?
               L"<font color=#008000>You are running it</font>" :
               L"<font color=#800000>You are NOT running Windows 64</font>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TTSDUALCORE")) {
            BOOL fCheck;
            LargeTTSRequirements (NULL, NULL, NULL, &fCheck, NULL);
            p->pszSubString = fCheck ?
               L"<font color=#008000>Your computer is fast enough</font>" :
               L"<font color=#800000>Your computer is NOT fast enough</font>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TTSRAM")) {
            BOOL fCheck;
            LargeTTSRequirements (&fCheck, NULL, NULL, NULL, NULL);
            p->pszSubString = fCheck ?
               L"<font color=#008000>Your computer has enough memory</font>" :
               L"<font color=#800000>Your computer does NOT have enough memory</font>";
            return TRUE;
         }
      }
      break;
   } // switch

   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
LargeTTSPageDisplay - Displays a page recommending that the user downloads
the large tts voices.

inputs
   PCEscWindow       pWindow - Window to use
returns
   BOOL - TRUE if success, FALSE if should close
*/
BOOL LargeTTSPageDisplay (PCEscWindow pWindow)
{
   // find all of the .tts voices in the current directory
   BOOL fFoundLarge = FALSE;
   WIN32_FIND_DATA FindFileData;
   char szDir[256];
   strcpy (szDir, gszAppDir);
   strcat (szDir, "*.tts");

   // make a list of wave files in the directory
   HANDLE hFind;

   hFind = FindFirstFile(szDir, &FindFileData);
   if (hFind != INVALID_HANDLE_VALUE) {
      while (TRUE) {
         // look for large TTS... make sure have at least one > 10 MB
         if (FindFileData.nFileSizeHigh || (FindFileData.nFileSizeLow > MAXTTSSIZE))
            fFoundLarge = TRUE;

         if (!FindNextFile (hFind, &FindFileData))
            break;
      }

      FindClose(hFind);
   } // if not invalid

   if (fFoundLarge)
      return TRUE;   // already have large

   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLARGETTSPAGE, LargeTTSPage, NULL);

   return (pszRet && !_wcsicmp(pszRet, L"next"));
}



/****************************************************************************
NewPasswordFilePage
*/
BOOL NewPasswordFilePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWSTR pszFile = (PWSTR)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"next")) {
            WCHAR szName[64];
            PCEscControl pControl = pPage->ControlFind (L"name");
            if (!pControl)
               return TRUE;
            szName[0] = 0;
            DWORD dwNeeded;
            pControl->AttribGet (Text(), szName, sizeof(szName), &dwNeeded);

            // remove bad characters
            RemoveIllegalFilenameChars (szName);
            pControl->AttribSet (Text(), szName);  // so user can seenew name
            if (!szName[0]) {
               pPage->MBWarning (L"You must first type in a name.");
               return TRUE;
            }

            // create the filename
            AppDataDirGet (pszFile);
            // MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, pszFile, 128);
            wcscat (pszFile, szName);
            wcscat (pszFile, L".crp");

            // see if exists
            FILE *f;
            if (f = _wfopen (pszFile, L"rb")) {
               fclose (f);
               pszFile[0] = 0;

               pPage->MBWarning (L"The password file already exists.");

               return FALSE;
            }
            
            // else, done
            pPage->Exit (L"next");
            return TRUE;
         } // if next

      }
      break;

   } // switch

   return DefPage (pPage, dwMessage, pParam);
}



/*****************************************************************************
NewPasswordFilePageDisplay - Displays the user name page and returns a filename
that the user selectes.

inputs
   PCEscWindow       pWindow - Window to use
   PWSTR             pszFile - Will be filled in. Must be at least 256 characters.
returns
   BOOL - TRUE if success (but if !pszFile[0] then go back), FALSE if should close
*/
BOOL NewPasswordFilePageDisplay (PCEscWindow pWindow, PWSTR pszFile)
{
   // clear
   pszFile[0] = 0;

   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNEWPASSWORDFILE, NewPasswordFilePage, pszFile);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"next"))
      return TRUE;
   else if (!_wcsicmp(pszRet, L"back"))
      return TRUE;

   return FALSE;
}



#if 0 // no longer used
/****************************************************************************
NewPasswordAskPage
*/
BOOL NewPasswordAskPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWSTR pszFile = (PWSTR)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"next")) {
            // get the passwords
            WCHAR szPassword1[64], szPassword2[64];
            PCEscControl pControl = pPage->ControlFind (L"password1");
            if (!pControl)
               return TRUE;
            szPassword1[0] = 0;
            DWORD dwNeeded;
            pControl->AttribGet (Text(), szPassword1, sizeof(szPassword1), &dwNeeded);

            // BUGFIX - No limits on passwords
            // must be at least 6 characters
            // if (wcslen(szPassword1) < 6) {
            //    pPage->MBWarning (L"Your password must be at least six characters.");
            //   return TRUE;
            //}

            // second password must match
            pControl = pPage->ControlFind (L"password2");
            if (!pControl)
               return TRUE;
            szPassword2[0] = 0;
            pControl->AttribGet (Text(), szPassword2, sizeof(szPassword2), &dwNeeded);
            if (wcscmp(szPassword1, szPassword2)) {
               pPage->MBWarning (L"Your second password doesn't match your first.");
               return TRUE;
            }

            // make sure CAN'T open existing
            DWORD dwRet = PasswordFileSet (pszFile, szPassword1, FALSE);
            if (dwRet != 2) {
               pPage->MBWarning (L"The password file already exists. You can't create a new one with the same name.");
               pPage->Exit (L"back");
               return TRUE;
            }

            // else, try to create it
            bugbug
            dwRet = PasswordFileSet (pszFile, szPassword1, TRUE);
            if (dwRet) {
               pPage->MBWarning (L"The password file couldn't be created.",
                  L"You may not have permission to write to the disk or some other problem. Try a new name.");
               pPage->Exit (L"back");
               return TRUE;
            }

            // else, it worked
            pPage->Exit (L"next");
            return TRUE;
         } // if next

      }
      break;

   } // switch

   return DefPage (pPage, dwMessage, pParam);
}
#endif


/****************************************************************************
UserPasswordAskPage
*/
BOOL UserPasswordAskPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWSTR pszFile = (PWSTR)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"next")) {
            // get the passwords
            WCHAR szPassword[64];
            PCEscControl pControl = pPage->ControlFind (L"password");
            if (!pControl)
               return TRUE;
            szPassword[0] = 0;
            DWORD dwNeeded;
            pControl->AttribGet (Text(), szPassword, sizeof(szPassword), &dwNeeded);

            // must be at least 6 characters
            if (!szPassword[0]) {
               pPage->MBWarning (L"Your must type in a password.");
               return TRUE;
            }

            // try to load the file
            DWORD dwRet = PasswordFileSet (pszFile, szPassword, FALSE);
            switch (dwRet) {
            case 0:  // worked
               pPage->Exit (L"next");
               return TRUE;

            case 3:
               pPage->MBWarning (L"The password isn't correct.");
               pPage->Exit (RedoSamePage());
               return TRUE;

            case 1:
            case 2:
            case 4:
            default:
               pPage->MBWarning (L"The password file couldn't be loaded for some unknown reason.");
               pPage->Exit (L"back");
               return TRUE;
            } // switch

         } // if next

      }
      break;

   } // switch

   return DefPage (pPage, dwMessage, pParam);
}



/*************************************************************************************
CircumrealityPasswordFileOpenDialog - Dialog box for opening a .crf file

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL CircumrealityPasswordFileOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
{
   OPENFILENAMEW   ofn;
   WCHAR  szTemp[256];
   szTemp[0] = 0;
   // WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
   wcscpy (szTemp, pszFile);
   memset (&ofn, 0, sizeof(ofn));
   
   // BUGFIX - Set directory
   WCHAR szInitial[MAX_PATH];
   // strcpy (szInitial, gszAppDir);
   AppDataDirGet (szInitial);
   GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = L"Circumreality user password file (*.crp)\0*.crp\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp) / sizeof(WCHAR);
   ofn.lpstrTitle = fSave ? L"Create a new password file" :
      L"Open a password file";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = L"crp";
   // nFileExtension 

   if (fSave) {
      if (!GetSaveFileNameW(&ofn))
         return FALSE;
   }
   else {
      if (!GetOpenFileNameW(&ofn))
         return FALSE;
   }

   // BUGFIX - Save diretory
   wcscpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // copy over
   wcscpy (pszFile, ofn.lpstrFile);
   // MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, pszFile, dwChars);
   return TRUE;
}


/****************************************************************************
PlayerInfoPage
*/
BOOL PlayerInfoPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCPasswordFile pPF = (PCPasswordFile) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         if (pControl = pPage->ControlFind( L"email"))
            pControl->AttribSet (Text(), pPF->m_szEmail);
         if (pControl = pPage->ControlFind (L"prefname"))
            pControl->AttribSet (Text(), pPF->m_szPrefName);
         if (pControl = pPage->ControlFind (L"desclink"))
            pControl->AttribSet (Text(), pPF->m_szDescLink);
         if (pControl = pPage->ControlFind (L"prefdesc"))
            pControl->AttribSet (Text(), (PWSTR)pPF->m_memPrefDesc.p);

         if (pControl = pPage->ControlFind (L"exposetimezone"))
            pControl->AttribSetBOOL (Checked(), pPF->m_fExposeTimeZone);
         if (pControl = pPage->ControlFind (L"disabletutorial"))
            pControl->AttribSetBOOL (Checked(), pPF->m_fDisableTutorial);

         if (pPF->m_dwAge)
            DoubleToControl (pPage, L"age", pPF->m_dwAge);
         if (pPF->m_dwHoursPlayed)
            DoubleToControl (pPage, L"hoursplayed", pPF->m_dwHoursPlayed);

         if (pControl = pPage->ControlFind (L"rendercache")) {
            pControl->AttribSetBOOL (Checked(), gfRenderCache);
            
            if (!RegisterMode())
               pControl->Enable (FALSE);
         }
         ComboBoxSet (pPage, L"prefgender", (DWORD)(pPF->m_iPrefGender + 1));
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         DWORD dwNeed;

         if (!_wcsicmp(psz, L"prefdesc")) {
            WCHAR szDesc[2048];
            szDesc[0] = 0;
            p->pControl->AttribGet (Text(), szDesc, sizeof(szDesc), &dwNeed);
            MemZero (&pPF->m_memPrefDesc);
            MemCat (&pPF->m_memPrefDesc, szDesc);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"email")) {
            p->pControl->AttribGet (Text(), pPF->m_szEmail, sizeof(pPF->m_szEmail), &dwNeed);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"prefname")) {
            p->pControl->AttribGet (Text(), pPF->m_szPrefName, sizeof(pPF->m_szPrefName), &dwNeed);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"desclink")) {
            p->pControl->AttribGet (Text(), pPF->m_szDescLink, sizeof(pPF->m_szDescLink), &dwNeed);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"age")) {
            pPF->m_dwAge = (DWORD) DoubleFromControl (pPage, psz);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"hoursplayed")) {
            pPF->m_dwHoursPlayed = (DWORD) DoubleFromControl (pPage, psz);
            return TRUE;
         }
      }

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"exposetimezone")) {
            pPF->m_fExposeTimeZone = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"disabletutorial")) {
            pPF->m_fDisableTutorial = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"rendercache")) {
            gfRenderCache = p->pControl->AttribGetBOOL (Checked());
            WriteRegRenderCache (gfRenderCache);
            return TRUE;
         }
      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p || !p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"prefgender")) {
            int iVal = p->pszName ? ((int)_wtoi(p->pszName) - 1) : 0;
            if (iVal == pPF->m_iPrefGender)
               return TRUE;

            pPF->m_iPrefGender = iVal;
            break;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"IFSIMPLEUI")) {
            p->pszSubString = pPF->m_fSimplifiedUI ? L"<comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSIMPLEUI")) {
            p->pszSubString = pPF->m_fSimplifiedUI ? L"</comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RENDERCACHE")) {
            int iRegister = RegisterMode ();
            if (iRegister > 0)
               p->pszSubString = L"";  // since registered
            else if (iRegister < 0)
               p->pszSubString = gpszSettingsTempEnabled;
            else
               p->pszSubString = gpszSettingsDisabled;
            return TRUE;
         }
      }
      break;

   } // switch

   return DefPage (pPage, dwMessage, pParam);
}



/*****************************************************************************
PlayerInfoPageDisplay - Displays a page asking for the user info.

inputs
   PCEscWindow       pWindow - Window to use
   BOOL              fSimpleUI - If TRUE then show the simplified UI for new players.
returns
   int - +1 if selected next, -1 if back, 0 if close
*/
BOOL PlayerInfoPageDisplay (PCEscWindow pWindow, BOOL fSimplifiedUI)
{
   // make sure to cache and create a clone
   PCPasswordFile pPF = PasswordFileCache(FALSE);
   if (!pPF)
      return 0;
   PCPasswordFile pClone = pPF->Clone();
   PasswordFileRelease ();
   if (!pClone)
      return 0;

   PWSTR pszRet;
   pClone->m_fSimplifiedUI = fSimplifiedUI;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPLAYERINFO, PlayerInfoPage, pClone);

   pPF = PasswordFileCache(FALSE);
   if (!pPF) {
      pClone->m_fDirty = FALSE;
      delete pClone;
      return 0;
   }

   // copy the clone info back over
   wcscpy (pPF->m_szEmail, pClone->m_szEmail);
   wcscpy (pPF->m_szPrefName, pClone->m_szPrefName);
   wcscpy (pPF->m_szDescLink, pClone->m_szDescLink);
   MemZero (&pPF->m_memPrefDesc);
   MemCat (&pPF->m_memPrefDesc, (PWSTR)pClone->m_memPrefDesc.p);
   pPF->m_dwAge = pClone->m_dwAge;
   pPF->m_dwHoursPlayed = pClone->m_dwHoursPlayed;
   pPF->m_iPrefGender = pClone->m_iPrefGender;
   pPF->m_fExposeTimeZone = pClone->m_fExposeTimeZone;
   pPF->m_fDisableTutorial = pClone->m_fDisableTutorial;
   pPF->m_fDirty = TRUE;   // so save

   PasswordFileRelease();

   pClone->m_fDirty = FALSE;  // so wont auto save
   delete pClone;

   if (!pszRet)
      return 0;
   if (!_wcsicmp(pszRet, L"next"))
      return 1;
   else if (!_wcsicmp(pszRet, L"back"))
      return -1;
   else
      return 0;
}

/****************************************************************************
UserPasswordFilePage
*/
BOOL UserPasswordFilePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWSTR pszFile = (PWSTR)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         
         if (pControl = pPage->ControlFind (L"usingpersonal"))
            pControl->AttribSetBOOL (Checked(), !gdwLabComputer);
         if (pControl = pPage->ControlFind (L"usinglab"))
            pControl->AttribSetBOOL (Checked(), gdwLabComputer);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"openfile")) {
            // get the filename
            WCHAR szFile[256];
            wcscpy (szFile, L"fl:");
            if (!CircumrealityPasswordFileOpenDialog(pPage->m_pWindow->m_hWnd,
               szFile + wcslen(szFile), sizeof(szFile)/sizeof(WCHAR) - (DWORD)wcslen(szFile), FALSE))
               return TRUE;

            // exit with this
            pPage->Exit (szFile);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"createfile")) {
            // get the filename
            WCHAR szFile[256];
            szFile[0] = 0;
            if (!CircumrealityPasswordFileOpenDialog(pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), TRUE))
               return TRUE;

            // make sure doesn't exist
            FILE *f;
            if (f = _wfopen (szFile, L"rb")) {
               fclose (f);
               pszFile[0] = 0;

               pPage->MBWarning (L"The password file already exists.");

               return FALSE;
            }

            // else, choose this
            pPage->Exit (L"createfile");
            wcscpy (pszFile, szFile);
            return TRUE;
         } // createfile
         else if (!_wcsicmp(psz, L"usingpersonal") || !_wcsicmp(psz, L"usinglab")) {
            gdwLabComputer = p->pControl->AttribGetBOOL (Checked());
            if (!_wcsicmp(psz, L"usingpersonal"))
               gdwLabComputer = !gdwLabComputer;

            // save option to registry
            HKEY hKey;
            DWORD dwDisp;
            RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
               KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
            if (hKey) {
               RegSetValueExW (hKey, gpszLabComputer, 0, REG_DWORD, (LPBYTE) &gdwLabComputer, sizeof(gdwLabComputer));
               RegCloseKey (hKey);
            }

            pPage->Exit(RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"QUOTE")) {
            p->pszSubString = DeepThoughtGenerate();
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFPERSONAL")) {
            p->pszSubString = gdwLabComputer ? L"<comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFPERSONAL")) {
            p->pszSubString = gdwLabComputer ? L"</comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFLAB")) {
            p->pszSubString = (!gdwLabComputer) ? L"<comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFLAB")) {
            p->pszSubString = (!gdwLabComputer) ? L"</comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ONDISKLIST")) {
            MemZero (&gMemTemp);

            // enumerate all the files
            //WCHAR szDir[256], szDirOrig[256];
            //MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szDir, 128);
            // wcscpy (szDirOrig, szDir);
            WCHAR szDir[MAX_PATH], szDirOrig[MAX_PATH];
            AppDataDirGet (szDir);
            wcscpy (szDirOrig, szDir);
            wcscat (szDir, L"*.crp");
            HANDLE hFind;
            WIN32_FIND_DATAW FindFileData;

            hFind = FindFirstFileW(szDir, &FindFileData);
            DWORD dwCount = 0;
            if (hFind != INVALID_HANDLE_VALUE) {
               while (TRUE) {
                  DWORD dwLen =(DWORD)wcslen(FindFileData.cFileName);
                  if ((dwLen >= 4) && !_wcsicmp(FindFileData.cFileName + (dwLen-4), L".crp")) {

                     // add this
                     MemCat (&gMemTemp,
                        L"<xChoiceButton style=righttriangle color=#a080ff ");
                     if (!dwCount)
                        MemCat (&gMemTemp, L"defcontrol=true accel=enter ");
                     MemCat (&gMemTemp, L"href=\"fl:");
                     MemCatSanitize (&gMemTemp, szDirOrig);
                     MemCatSanitize (&gMemTemp, FindFileData.cFileName);
                     MemCat (&gMemTemp, L"\"><bold>");
                     FindFileData.cFileName[dwLen-4] = 0;   // to get rid of extension
                     MemCatSanitize (&gMemTemp, FindFileData.cFileName);
                     MemCat (&gMemTemp, L"</bold><br/>Use this password file.</xChoiceButton>");

                     dwCount++;
                  }

                  if (!FindNextFileW (hFind, &FindFileData))
                     break;
               }

               FindClose(hFind);
            }

            p->pszSubString = (PWSTR)gMemTemp.p;

            return TRUE;
         }
      }
      break;
   } // switch

   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
UserPasswordFilePageDisplay - Displays a page asking for the user password file.

NOTE: If there is already a user file then this merely returns TRUE.
A user file might already exist because the user double-clicked a password file.

inputs
   PCEscWindow       pWindow - Window to use
   PWSTR             pszFile - Should check this to see if a .crp user file has
                        been double-clicked, in which case autologon.
   BOOL              *pfNewPlayer - This function will set to TRUE if a new
                        password file has been created.
returns
   BOOL - TRUE if success, FALSE if should close
*/
BOOL UserPasswordFilePageDisplay (PCEscWindow pWindow, PWSTR pszFile, BOOL *pfNewPlayer)
{
   *pfNewPlayer = FALSE;

   WCHAR szFile[256];
   PCPasswordFile pPF = PasswordFileCache(FALSE);
   if (pPF) {
      PasswordFileRelease ();
      return TRUE;
   }

   // allow user to double-click on .crp file to run it
   DWORD dwLen = pszFile ? (DWORD)wcslen(pszFile) : 0;
   if ((dwLen >= 4) && !_wcsicmp(pszFile + (dwLen-4), L".crp") && (dwLen+1 < sizeof(szFile)/sizeof(WCHAR)) ) {
      wcscpy (szFile, pszFile);
      pszFile[0] = 0;
      goto redoaskpassword;
   }

   static BOOL fTryToFind = TRUE;   // so only try once

redo:
   PWSTR pszRet;
   szFile[0] = 0;

   // if this isn't a lab computer, and there are no password files, then automatically create
   // enumerate all the files
   WCHAR szDir[MAX_PATH], szDirOrig[MAX_PATH];
   AppDataDirGet (szDir);
   wcscpy (szDirOrig, szDir);
   wcscat (szDir, L"*.crp");
   HANDLE hFind;
   WIN32_FIND_DATAW FindFileData;

   hFind = fTryToFind ? FindFirstFileW(szDir, &FindFileData) : INVALID_HANDLE_VALUE;
   DWORD dwCount = 0;
   BOOL fFoundExistingFile = FALSE;
   if (hFind != INVALID_HANDLE_VALUE) {
      while (TRUE) {
         DWORD dwLen =(DWORD)wcslen(FindFileData.cFileName);
         if ((dwLen >= 4) && !_wcsicmp(FindFileData.cFileName + (dwLen-4), L".crp")) {

            fFoundExistingFile = TRUE;
            break;
         }

         if (!FindNextFileW (hFind, &FindFileData))
            break;
      }

      FindClose(hFind);
   } // if files


   if (!gdwLabComputer && !fFoundExistingFile && fTryToFind)
      pszRet = L"createdisk"; // so will create a new user
   else
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLUSERPASSWORDFILE, UserPasswordFilePage, szFile);
   fTryToFind = FALSE;  // so don't get in infinte loop

   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"next"))
      return TRUE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   else if (!_wcsicmp(pszRet, L"readme")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLREADME, ReadMePage);
      if (pszRet && !_wcsicmp(pszRet, L"back"))
         goto redo;
      // else, should close
      return FALSE;
   }
   else if ((pszRet[0] == L'f') && (pszRet[1] == L'l') && (pszRet[2] == L':')) {
      // copy it
      wcscpy (szFile, pszRet + 3);

      // ask the password
redoaskpassword:

      // try to load with empty password first
      DWORD dwRet = PasswordFileSet (szFile, L"", FALSE);
      if (dwRet != 0) {
         PWSTR pszRet = pWindow->PageDialog (ghInstance, IDR_MMLUSERPASSWORDASK, UserPasswordAskPage, szFile);
         if (!pszRet)
            return FALSE;
         if (!_wcsicmp(pszRet, L"back"))
            goto redo;
         else if (!_wcsicmp(pszRet, RedoSamePage()))
            goto redoaskpassword;

         // if not next then exit
         if (_wcsicmp(pszRet, L"next"))
            return FALSE;
      }

      // else, succeded
      return TRUE;
   }
   else if (!_wcsicmp(pszRet, L"createdisk") || !_wcsicmp(pszRet, L"createfile")) {
      // if "createfile" then pszFile already filled
      BOOL fCreateDisk = !_wcsicmp(pszRet, L"createdisk");
redocreatedisk:
      if (fCreateDisk) {
         if (!NewPasswordFilePageDisplay (pWindow, szFile))
            return FALSE;

         if (!szFile[0])
            goto redo;
      } //

      // ask for the password
      // BUGFIX - DON'T ask for a password. Default to no password
      // make sure CAN'T open existing
      DWORD dwRet = PasswordFileSet (szFile, L"", FALSE);
      if (dwRet != 2) {
         EscMessageBox (pWindow->m_hWnd, gpszCircumrealityClient,
            L"The password file already exists. You can't create a new one with the same name.",
            NULL, MB_OK);
         if (fCreateDisk)
            goto redocreatedisk;
         else
            goto redo;
      }

      // else, try to create it
      dwRet = PasswordFileSet (szFile, L"", TRUE);
      if (dwRet) {
         EscMessageBox (pWindow->m_hWnd, gpszCircumrealityClient,
            L"The password file couldn't be created.",
            L"You may not have permission to write to the disk or some other problem. Try a new name.",
            MB_OK);
         if (fCreateDisk)
            goto redocreatedisk;
         else
            goto redo;
      }

#if 0 // no longer used
      PWSTR pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNEWPASSWORDASK, NewPasswordAskPage, szFile);
      if (!pszRet)
         return FALSE;
      if (!_wcsicmp(pszRet, L"back")) {
         if (fCreateDisk)
            goto redocreatedisk;
         else
            goto redo;
      }
      // if not next then exit
      if (_wcsicmp(pszRet, L"next"))
         return FALSE;
#endif

      int iRet = PlayerInfoPageDisplay (pWindow, TRUE);
      if (iRet < 0)
         goto redo;
      else if (!iRet)
         return FALSE;
      else {
         *pfNewPlayer = TRUE;
         return TRUE;
      }
   }

   return FALSE;
}



/****************************************************************************
AccountEditPage
*/
BOOL AccountEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCPasswordFileAccount pPFA = (PCPasswordFileAccount) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         if (pControl = pPage->ControlFind (L"name"))
            pControl->AttribSet (Text(), (PWSTR) pPFA->m_memName.p);
         if (pControl = pPage->ControlFind (L"user"))
            pControl->AttribSet (Text(), pPFA->m_szUser);
         if (pControl = pPage->ControlFind (L"password"))
            pControl->AttribSet (Text(), pPFA->m_szPassword);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         DWORD dwNeed;
         if (!_wcsicmp(psz, L"name")) {
            WCHAR szTemp[128];
            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);
            MemZero (&pPFA->m_memName);
            MemCat (&pPFA->m_memName, szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"user")) {
            p->pControl->AttribGet (Text(), pPFA->m_szUser, sizeof(pPFA->m_szUser), &dwNeed);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"password")) {
            p->pControl->AttribGet (Text(), pPFA->m_szPassword, sizeof(pPFA->m_szPassword), &dwNeed);
            return TRUE;
         }
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"delete")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this password?",
               L"Deleting the password won't affect the account stored in the world, but "
               L"you won't be able to access it without the password."))
               return TRUE;

            // else, delete it
            pPage->Exit (L"delete");
            return TRUE;
         } // if delete

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         if (!_wcsicmp(p->pszSubName, L"ACCOUNTNAME")) {
            PWSTR psz = (PWSTR)pPFA->m_memFileName.p;
            if (!psz[0])
               psz = (PWSTR)pPFA->m_memName.p;

            p->pszSubString = psz;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DESCSHORT")) {
            PCResTitleInfo pInfo = pPFA->m_pCResTitleInfoSet ? pPFA->m_pCResTitleInfoSet->Find (gLangID) : NULL;
            if (!pInfo)
               return FALSE;

            p->pszSubString = (PWSTR) pInfo->m_memDescShort.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DESCLONG")) {
            PCResTitleInfo pInfo = pPFA->m_pCResTitleInfoSet ? pPFA->m_pCResTitleInfoSet->Find (gLangID) : NULL;
            if (!pInfo)
               return FALSE;

            p->pszSubString = (PWSTR) pInfo->m_memDescLong.p;
            return TRUE;
         }

      }
      break;

   } // switch

   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
AccountEditPageDisplay - Displays a page for editing an account

inputs
   PCEscWindow       pWindow - Window to use
   DWORD             dwID - Unique ID
returns
   BOOL - TRUE pressed back, FALSE if closed
*/
BOOL AccountEditPageDisplay (PCEscWindow pWindow, DWORD dwID)
{
   PCPasswordFile pPF = PasswordFileCache(FALSE);
   if (!pPF)
      return TRUE;

   DWORD dwIndex = pPF->AccountFind (dwID);
   if (dwIndex == (DWORD)-1) {
      PasswordFileRelease();
      return TRUE;   // shouldnt happen
   }

   PCPasswordFileAccount pPFA = pPF->AccountGet (dwIndex);
   if (!pPFA) {
      PasswordFileRelease();
      return TRUE;   // shouldnt happen
   }

   // clone this
   pPFA = pPFA->Clone();
   pPFA->m_pCPasswordFile = NULL;   // so no link to that

   PasswordFileRelease();

   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLACCOUNTEDIT, AccountEditPage, pPFA);
   if (pszRet && !_wcsicmp(pszRet, L"delete")) {
      delete pPFA;

      // delete this
      pPF = PasswordFileCache(FALSE);
      if (!pPF)
         return TRUE;

      dwIndex = pPF->AccountFind (dwID);
      if (dwIndex == (DWORD)-1) {
         PasswordFileRelease();
         return TRUE;   // shouldnt happen
      }

      pPF->AccountRemove (dwIndex);

      PasswordFileRelease();
      return TRUE;
   }

   // save changes
   BOOL fRet = pszRet && !_wcsicmp(pszRet, L"back");

   pPF = PasswordFileCache(FALSE);
   if (!pPF)
      return fRet;

   PCPasswordFileAccount pPFANew = pPF->AccountGet (pPF->AccountFind (dwID));
   if (!pPFANew) {
      PasswordFileRelease();
      return fRet;   // shouldnt happen
   }

   // copy over
   wcscpy (pPFANew->m_szPassword, pPFA->m_szPassword);
   wcscpy (pPFANew->m_szUser, pPFA->m_szUser);
   MemZero (&pPFANew->m_memName);
   MemCat (&pPFANew->m_memName, (PWSTR)pPFA->m_memName.p);

   delete pPFA;
   pPFANew->Dirty();

   PasswordFileRelease();


   return fRet;
}




/****************************************************************************
ChangePasswordPage
*/
BOOL ChangePasswordPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"next")) {
            // get the passwords
            WCHAR szPassword0[64], szPassword1[64], szPassword2[64];
            PCEscControl pControl;
            DWORD dwNeeded;

            // get the old password
            pControl = pPage->ControlFind (L"password0");
            if (!pControl)
               return TRUE;
            szPassword0[0] = 0;
            pControl->AttribGet (Text(), szPassword0, sizeof(szPassword0), &dwNeeded);
            // BUGFIX - Old password can no be blank
            //if (!szPassword0[0]) {
            //   pPage->MBWarning (L"You must type in your old password.");
            //   return TRUE;
            //}


            // get the new one
            pControl = pPage->ControlFind (L"password1");
            if (!pControl)
               return TRUE;
            szPassword1[0] = 0;
            pControl->AttribGet (Text(), szPassword1, sizeof(szPassword1), &dwNeeded);

            // must be at least 6 characters
            // BUGFIX - Don't require passwords to be any number of characters
            //if (wcslen(szPassword1) < 6) {
            //   pPage->MBWarning (L"Your new password must be at least six characters.");
            //   return TRUE;
            //}

            // second password must match
            pControl = pPage->ControlFind (L"password2");
            if (!pControl)
               return TRUE;
            szPassword2[0] = 0;
            pControl->AttribGet (Text(), szPassword2, sizeof(szPassword2), &dwNeeded);
            if (wcscmp(szPassword1, szPassword2)) {
               pPage->MBWarning (L"Your second password doesn't match your first.");
               return TRUE;
            }

            // verify that passwords match
            PCPasswordFile pPF = PasswordFileCache (FALSE);
            if (!pPF)
               return TRUE;   // shouldnt happen

            if (wcscmp (pPF->m_szPassword, szPassword0)) {
               pPage->MBWarning (L"The \"current password\" that you typed in is incorrect.");
               PasswordFileRelease();
               return TRUE;
            }

            // change
            pPF->PasswordChange (szPassword1);
            PasswordFileRelease();

            // remember new password
            WCHAR szFile[256];
            PasswordFileGet (szFile, NULL);
            PasswordFileSet (szFile, szPassword1, FALSE);

            pPage->MBInformation (L"Your password has been changed. Don't forget it!");
            pPage->Exit (L"back");
            return TRUE;
         } // if next

      }
      break;

   } // switch

   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
DeletePasswordFilePage
*/
BOOL DeletePasswordFilePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"next")) {
            // get the passwords
            WCHAR szPassword0[64];
            PCEscControl pControl;
            DWORD dwNeeded;

            // get the old password
            pControl = pPage->ControlFind (L"password0");
            if (!pControl)
               return TRUE;
            szPassword0[0] = 0;
            pControl->AttribGet (Text(), szPassword0, sizeof(szPassword0), &dwNeeded);
            // BUGFIX - Allow NULL passwords
            //if (!szPassword0[0]) {
            //   pPage->MBWarning (L"You must type in your password before you can delete your password file.");
            //   return TRUE;
            //}


            // verify that passwords match
            PCPasswordFile pPF = PasswordFileCache (FALSE);
            if (!pPF)
               return TRUE;   // shouldnt happen

            if (wcscmp (pPF->m_szPassword, szPassword0)) {
               pPage->MBWarning (L"The password that you typed in is incorrect.");
               PasswordFileRelease();
               return TRUE;
            }

            // release
            PasswordFileRelease();

            // verify
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to permenantly delete your password file?")) {
               pPage->Exit (L"back");
               return TRUE;
            }

            // delete it
            WCHAR szFile[256];
            PasswordFileGet (szFile, NULL);
            DeleteFileW (szFile);
            PasswordFileSet (L"", L"", FALSE);

            // inform the user
            pPage->MBInformation (L"Your password file has been deleted.",
               L"Circumreality will now exit.");

            // exit
            pPage->Exit (L"[exit]");
            return TRUE;
         } // if next

      }
      break;

   } // switch

   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
MainPlayerPage
*/
BOOL MainPlayerPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // enable MNLP button
         PCEscControl pControl = pPage->ControlFind (L"recordtts");
         if (!MNLPRun (NULL, NULL, pPage->m_pWindow->m_hWnd, TRUE) && pControl)
            pControl->Enable (FALSE);

         // scroll to right position
         if (giVScroll > 0) {
            pPage->VScroll (giVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            giVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         // remind player to turn off tutorial?
         if (!gfHasRemindedToday) {
            gfHasRemindedToday = TRUE;

            PCPasswordFile pPF = PasswordFileCache (FALSE);
            if (pPF) {
               BOOL fShouldRemind = FALSE;
               if (!pPF->m_fDisableTutorial) {
                  if (pPF->m_dwTimesSinceRemind)
                     pPF->m_dwTimesSinceRemind--;
                  else {
                     pPF->m_dwTimesSinceRemind = DEFAULTTIMESSINCEREMIND;
                     fShouldRemind = TRUE;
                  }

                  // set dirty, since chaning flag
                  pPF->Dirty ();
               } // if disable tutorial

               if (fShouldRemind) {
                  if (IDYES == pPage->MBYesNo (L"Do you wish to disable the tutorial in any new worlds that you enter?",
                     L"If you have already gone through the tutorial in one world, you probably don't need to experience "
                     L"it in others. Pressing \"Yes\" will automatically disable the tutorial in "
                     L"any NEW worlds that you enter. (Worlds that you have already visited will be unchanged.)"))
                     pPF->m_fDisableTutorial = TRUE;
                        // NOTE: - Dont need to turn set dirty because already set
               } // if shouldremind

               // save this
               PasswordFileRelease();
            } // if have cache
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p || !p->pControl)
            break;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp (psz, L"recordtts")) {
            if (!MNLPRun(NULL, NULL, pPage->m_pWindow->m_hWnd, FALSE))
               pPage->MBError (L"The application, MNLP.exe, couldn't be run.");
            return TRUE;
         }
         if (!_wcsicmp (psz, L"checkplayers")) {
            DWORD i, j;
            PCPasswordFile pPF = PasswordFileCache (FALSE);
            if (!pPF)
               return TRUE;

            SERVERLOADQUERY slq;
            memset (&slq, 0, sizeof(slq));
            CListFixed lSLQ;
            lSLQ.Init (sizeof(SERVERLOADQUERY));

            for (j = 0; j < pPF->AccountNum(); j++) {
               PCPasswordFileAccount pPFA = pPF->AccountGet(j);
               if (!pPFA)
                  continue;

               if (((PWSTR)pPFA->m_memFileName.p)[0])
                  continue;   // not a link

               PCResTitleInfo pti = pPFA->m_pCResTitleInfoSet->Find (gLangID);
               if (!pti)
                  continue;

               PCResTitleInfoShard *ppis = (PCResTitleInfoShard*)pti->m_lPCResTitleInfoShard.Get(pPFA->m_dwShard);
               if (!ppis)
                  continue;

               PCResTitleInfoShard pis = ppis[0];

               PWSTR pszDomain = (PWSTR)pis->m_memAddr.p;
               if (wcslen(pszDomain)+1 >= sizeof(slq.szDomain)/sizeof(WCHAR))
                  continue;

               wcscpy (slq.szDomain, pszDomain);
               slq.dwPort = pis->m_dwPort;
               slq.pUserData = (PVOID)(size_t) pPFA->m_dwUniqueID;

               lSLQ.Add (&slq);
            } // j

            PasswordFileRelease();

            PSERVERLOADQUERY pSLQ = (PSERVERLOADQUERY)lSLQ.Get(0);
            ServerLoadQuery (pSLQ, lSLQ.Num(), pPage->m_pWindow->m_hWnd);


            for (j = 0; j < lSLQ.Num(); j++, pSLQ++) {
               // replace existing
               PSERVERLOADQUERY pSLQGlobal = (PSERVERLOADQUERY) glSERVERLOADQUERY.Get(0);
               for (i = 0; i < glSERVERLOADQUERY.Num(); i++, pSLQGlobal++)
                  if (pSLQGlobal->pUserData == pSLQ->pUserData) {
                     *pSLQGlobal = *pSLQ;
                     break;
                  }
               if (i >= glSERVERLOADQUERY.Num())
                  glSERVERLOADQUERY.Add (pSLQ);
            } // j


            pPage->Exit (RedoSamePage());
            return TRUE;
         } // checkplayers
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         PWSTR psz = p->psz;
         if (!psz)
            break;

         if ((psz[0] == L's') && (psz[1] == L'l') && (psz[2] == L':')) {
            // ask for number of players
            DWORD dwID = (DWORD)_wtoi(psz + 3);
            DWORD i;

            PCPasswordFile pPF = PasswordFileCache (FALSE);
            if (!pPF)
               return TRUE;
            PCPasswordFileAccount pPFA = pPF->AccountGet(pPF->AccountFind(dwID));
            if (!pPFA || !pPFA->m_pCResTitleInfoSet) {
               PasswordFileRelease();
               return TRUE;   // shouldnt happen
            }

            PCResTitleInfo pti = pPFA->m_pCResTitleInfoSet->Find (gLangID);
            if (!pti) {
               PasswordFileRelease();
               return TRUE;
            }

            SERVERLOADQUERY slq;
            memset (&slq, 0, sizeof(slq));
            CListFixed lSLQ;
            lSLQ.Init (sizeof(SERVERLOADQUERY));

            PCResTitleInfoShard *ppis = (PCResTitleInfoShard*)pti->m_lPCResTitleInfoShard.Get(pPFA->m_dwShard);
            if (ppis) {
               PCResTitleInfoShard pis = ppis[0];

               PWSTR pszDomain = (PWSTR)pis->m_memAddr.p;
               if (wcslen(pszDomain)+1 < sizeof(slq.szDomain)/sizeof(WCHAR)) {
                  wcscpy (slq.szDomain, pszDomain);
                  slq.dwPort = pis->m_dwPort;
                  lSLQ.Add (&slq);
               }
            }
            PasswordFileRelease();

            PSERVERLOADQUERY pSLQ = (PSERVERLOADQUERY)lSLQ.Get(0);
            ServerLoadQuery (pSLQ, lSLQ.Num(), pPage->m_pWindow->m_hWnd);


            WCHAR szTemp[256];
            if (pSLQ && pSLQ->ServerLoad.dwMaxConnections)
               swprintf (szTemp, L"The world has %d players out of a maximum of %d.",
                  (int) pSLQ->ServerLoad.dwConnections, (int) pSLQ->ServerLoad.dwMaxConnections);
            else
               wcscpy (szTemp, L"The world couldn't be contacted to retrieve the number of players.");
            pPage->MBInformation (szTemp);

            if (pSLQ) {
               pSLQ->pUserData = (PVOID)(size_t)dwID;

               // replace existing
               PSERVERLOADQUERY pSLQGlobal = (PSERVERLOADQUERY) glSERVERLOADQUERY.Get(0);
               for (i = 0; i < glSERVERLOADQUERY.Num(); i++, pSLQGlobal++)
                  if (pSLQGlobal->pUserData == pSLQ->pUserData) {
                     *pSLQGlobal = *pSLQ;
                     break;
                  }
               if (i >= glSERVERLOADQUERY.Num())
                  glSERVERLOADQUERY.Add (pSLQ);
            }

            pPage->Exit (RedoSamePage());

            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         if (!_wcsicmp(p->pszSubName, L"WIN64WARNING")) {
            // if already on 64-bit then don't bother
            if (sizeof(PVOID) > sizeof(DWORD))
               return FALSE;

            LPFN_ISWOW64PROCESS 
            fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
            GetModuleHandle("kernel32"),"IsWow64Process");

            // if not wow64 then done
            if (!fnIsWow64Process)
               return FALSE;
            BOOL fProcess;
            if (!fnIsWow64Process (GetCurrentProcess(),&fProcess))
               return FALSE;

            if (!fProcess)
               return FALSE;

            // else, warn user that would be faster on 64-bit windows
            p->pszSubString =
               L"<p align=center><table width=50% bordercolor=#c0c0c0>"
               L"<xtrheader>CircumReality could be faster</xtrheader>"
               L"<tr><td>"
               L"<colorblend color=#ffffff posn=background transparent=0.25/>"
               L"If you visit the CircumReality website <a href=\"http://www.CircumReality.com/Download.htm\">http://www.CircumReality.com/Download.htm</a>, "
               L"download, and install the <bold>64-bit version</bold> of CircumReality, it will run slightly faster "
               L"on your computer. (You are currently running the 32-bit version of CircumReality.)"
               L"</td></tr>"
               L"</table></p>";

            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ACCOUNTS")) {
            PCPasswordFile pPF = PasswordFileCache (FALSE);
            if (!pPF)
               return FALSE;
            if (!pPF->AccountNum()) {
               PasswordFileRelease();
               return FALSE;
            }

            MemZero (&gMemTemp);
            MemCat (&gMemTemp,
               L"<p align=center><table width=50% bordercolor=#c0c0c0>"
               L"<xtrheader>Return to a world you have already visited...</xtrheader>");

            DWORD i, j;
            pPF->AccountSort();
            __int64 iMinutesToday = DFDATEToMinutes (DFDATEToday(TRUE));
            BOOL fFoundLink = FALSE;

            for (i = 0; i < pPF->AccountNum(); i++) {
               PCPasswordFileAccount pPFA = pPF->AccountGet (i);
               if (!pPFA)
                  continue;

               PCResTitleInfo pInfo = NULL;
               if (pPFA->m_pCResTitleInfoSet)
                  pInfo = pPFA->m_pCResTitleInfoSet->Find (gLangID);

               MemCat (&gMemTemp,
                  L"<tr><td width=3>"
                  L"<colorblend color=#ffffff posn=background transparent=0.25/>"
                  L"<xChoiceButton style=righttriangle href=pl:");
               MemCat (&gMemTemp, (int)pPFA->m_dwUniqueID);

               // BUGFIX - Make 1st option defcontrol
               if (!i)
                  MemCat (&gMemTemp, L" defcontrol=true accel=enter");

               MemCat (&gMemTemp,
                  L">Visit <bold>");

               MemCatSanitize (&gMemTemp, (PWSTR) pPFA->m_memName.p);
               MemCat (&gMemTemp,
                  L"</bold>");
               MemCat (&gMemTemp,
                  L"<br/><small>");
               if (pInfo)
                  MemCatSanitize (&gMemTemp, (PWSTR)pInfo->m_memDescShort.p);
               MemCat (&gMemTemp,
                  L"</small></xChoiceButton>");

               MemCat (&gMemTemp,
                  L"<p align=right><small>");

               // web page
               PWSTR pszName = (PWSTR)pInfo->m_memWebSite.p;
               if (BeginsWithHTTP(pszName)) {
                  MemCat (&gMemTemp,
                     L"<a color=#8080ff href=\"");
                  MemCatSanitize (&gMemTemp, pszName);
                  MemCat (&gMemTemp,
                     L"\">");
                  MemCatSanitize (&gMemTemp, pszName);
                  MemCat (&gMemTemp,
                     L"</a><br/>");
               }

               MemCat (&gMemTemp,
                  ((PWSTR)pPFA->m_memFileName.p)[0] ?
                     L"<font color=#008000>Single player world (Internet <bold>not</bold> needed)</font><br/>" :
                     L"<font color=#800000>Multiplayer world (<bold>Internet</bold> required)</font><br/>");

               // see if there is any server information
               if (!((PWSTR)pPFA->m_memFileName.p)[0]) {
                  fFoundLink = TRUE;
                  PSERVERLOADQUERY pSLQ = (PSERVERLOADQUERY) glSERVERLOADQUERY.Get(0);
                  for (j = 0; j < glSERVERLOADQUERY.Num(); j++, pSLQ++) {
                     if ((DWORD)(size_t)pSLQ->pUserData == pPFA->m_dwUniqueID)
                        break;
                  } // j

                  if (j < glSERVERLOADQUERY.Num()) {
                     if (pSLQ->ServerLoad.dwMaxConnections) {
                        MemCat (&gMemTemp, (int)pSLQ->ServerLoad.dwConnections);
                        MemCat (&gMemTemp, L" players out of a maximum of ");
                        MemCat (&gMemTemp, (int)pSLQ->ServerLoad.dwMaxConnections);
                     }
                     else
                        MemCat (&gMemTemp, L"The world couldn't be contacted to retrieve the number of players.");
                  }
                  else {
                     // option for how many players
                     MemCat (&gMemTemp, L"<a href=sl:");
                     MemCat (&gMemTemp, (int)pPFA->m_dwUniqueID);
                     MemCat (&gMemTemp, L">How many people are playing?</a>");
                  }
                  MemCat (&gMemTemp,
                     L"<br/>");
               }

               MemCat (&gMemTemp,
                  L"</small></p>"
                  L"</td>"
                  L"<td width=1>"
                  L"<colorblend color=#ffffff posn=background transparent=0.25/>"
                  L"<small>");

               // see if its expired
               __int64 iAge = iMinutesToday - DFDATEToMinutes (pPFA->m_dwLastUsed);
               iAge /= (60 * 24);
               BOOL fExpired = (pInfo && pInfo->m_dwDelUnusedUsers && (iAge > (__int64)pInfo->m_dwDelUnusedUsers));

               MemCat (&gMemTemp, (int)DAYFROMDFDATE(pPFA->m_dwLastUsed));
               MemCat (&gMemTemp, L" - ");

               PWSTR apszMonth[12] = {L"Jan", L"Feb", L"Mar", L"Apr",
                  L"May", L"Jun", L"Jul", L"Aug",
                  L"Sep", L"Oct", L"Nov", L"Dec"};

               MemCat (&gMemTemp, apszMonth[MONTHFROMDFDATE(pPFA->m_dwLastUsed) - 1]);
               MemCat (&gMemTemp, L" - ");
               MemCat (&gMemTemp, (int)YEARFROMDFDATE(pPFA->m_dwLastUsed));

               if (fExpired)
                  MemCat (&gMemTemp,
                     L"<p/><font color=#800000>This account may have been automatically "
                     L"deleted because it hasn't been used for awhile.</font>");

               MemCat (&gMemTemp, L"</small></td></tr>");

            } // i

            MemCat (&gMemTemp,
               L"<tr><td>"
               L"<colorblend color=#ffffff posn=background transparent=0.25/>"
               L"<small>"
               L"Click on the world's name to visit "
               L"using your <bold>primary character</bold>. If you wish to play "
               L"with a secondary character, <bold>hold down the CTRL key</bold> when "
               L"you click. "
               L"To see or modify the account's user name or password, <bold>hold "
               L"down SHIFT</bold> when you click on the play button.</small></td></tr>");

            // option for how many players only available if have URL link in one
            if (fFoundLink)
               MemCat (&gMemTemp,
                  L"<tr><td>"
                  L"<colorblend color=#ffffff posn=background transparent=0.25/>"
                  L"<xchoicebutton name=checkplayers>"
                  L"<bold>How many people are in each of the worlds?</bold><br/>"
                  L"Press this to see how many players there are in each of the listed worlds."
                  L"</xchoicebutton></td></tr>");

            MemCat (&gMemTemp,
               L"</table></p>");

            p->pszSubString = (PWSTR)gMemTemp.p;

            // release the password file
            PasswordFileRelease();
            return TRUE;
         }
      }
      break;
   } // switch

   return DefPage (pPage, dwMessage, pParam);
}


/******************************************************************************
MNLPRun - Runs the wave editor.

inputs
   PCWSTR            pszDir - Directory. Can be null
   PCWSTR            pszFile - File. Can be null if !pszDir.
   HWND              hWnd - To run from
   BOOL              fTestOnly - If TRUE, testing to see if MNLP.exe exists.
returns
   BOOL - TRUE if success
*/
BOOL MNLPRun (PCWSTR pszDir, PCWSTR pszFile, HWND hWnd, BOOL fTestOnly)
{
   char szFile[512];
   if (pszDir) {
      WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szFile, sizeof(szFile), 0 ,0);
      strcat (szFile, "\\");
   }
   else
      szFile[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szFile + strlen(szFile), sizeof(szFile) - (DWORD)strlen(szFile), 0 ,0);

   // create the filename
   char szRun[1024];
   char szCurDir[256];
   DWORD dwLen;
   strcpy (szRun, "\"");
   dwLen = (DWORD) strlen(szRun);
   strcpy (szCurDir, gszAppDir);
   strcat (szRun, gszAppDir);
   strcat (szRun, "mnlp.exe");

   if (fTestOnly) {
      FILE *pf = fopen (szRun + 1 /* to skip the quote */, "rb");
      if (pf) {
         fclose (pf);
         return TRUE;
      }
      else
         return FALSE;
   }

   strcat (szRun, "\"");
   if (szFile[0]) {
      strcat (szRun, " ");
      strcat (szRun, szFile);
   }

   PROCESS_INFORMATION pi;
   STARTUPINFO si;
   memset (&si, 0, sizeof(si));
   si.cb = sizeof(si);
   memset (&pi, 0, sizeof(pi));
   if (!CreateProcess (NULL, szRun, NULL, NULL, NULL, NULL, NULL, szCurDir, &si, &pi))
      return FALSE;

   // Close process and thread handles. 
   CloseHandle( pi.hProcess );
   CloseHandle( pi.hThread );

   return TRUE;
}


/*****************************************************************************
MainPlayerPageDisplay - Displays a main player page

inputs
   PCEscWindow       pWindow - Window to use
   PWSTR             pszFile - Used to determine if file and to open it up
   PCResTitleInfoSet *ppInfoSet - Point this to the location of the global where
                     the InfoSet from the previous session was stored. This might
                     point to something that's NULL. If used, this will be deleted
                     and set to NULL.
   BOOL              *pfNewPlayer - Should initially be set to TRUE if it's a new
                     player. If so, will automatically skip the main screen and
                     go right to the "open file" screen and then set this to FALSE.
   DWORD             *pdwUniqueID - Filled in with the the ID to play (if returns 1)
   BOOL              *pfQuickLogon - If TRUE then quick logon. FALSE then ask for character name.
returns
   int -    0 for exit the app, 1 for play with *pdwUniqueID being filled in
*/
int MainPlayerPageDisplay (PCEscWindow pWindow, PWSTR pszFile, PCResTitleInfoSet *ppInfoSet,
                           BOOL *pfNewPlayer, DWORD *pdwUniqueID, BOOL *pfQuickLogon)
{
   *pfQuickLogon = TRUE;

   BOOL fSkipIfOnlyOneFile = FALSE;
   DWORD i;
   DWORD dwLen = pszFile ? (DWORD)wcslen(pszFile) : 0;
   PCResTitleInfoSet pInfoSet = NULL;;
   BOOL fCircumreality;
   BOOL fLoadedFromFile = FALSE;
   DWORD dwMatchID = (DWORD)-1;
   if (ppInfoSet && *ppInfoSet) {
      // load from previous instance
      pInfoSet = *ppInfoSet;
      *ppInfoSet = NULL;
      fLoadedFromFile = NULL;
      pszFile[0] = 0;   // since will ignore whatever's there
      fCircumreality = FALSE;
   }
   else if ((dwLen > 4) && !pInfoSet) {
      pInfoSet = PCResTitleInfoSetFromFile (pszFile, &fCircumreality);
      fLoadedFromFile = pInfoSet ? TRUE : FALSE;
   }

   if (pInfoSet) {
      // opened the file

      PCPasswordFile pPF = PasswordFileCache (FALSE);
      if (!pPF)
         goto noload;

      // loop
      for (i = 0; i < pPF->AccountNum(); i++) {
         PCPasswordFileAccount pPFA = pPF->AccountGet (i);
         if (!pPFA)
            continue;

         // find language
         if (!pPFA->m_pCResTitleInfoSet)
            continue;   // shouldnt happen

         PCResTitleInfo pInfo = pPFA->m_pCResTitleInfoSet->Find (gLangID);
         if (!pInfo)
            continue;   // shouldnt happen
         PCResTitleInfo pInfo2 = pInfoSet->Find (gLangID);
         if (!pInfo2)
            continue;   // shouldnt happen

         // if this is a circumreality file (.crf) then check against
         // match in filename
         if (fCircumreality) {
            if (!_wcsicmp(pszFile, (PWSTR)pPFA->m_memFileName.p)) {
               // found it
               dwMatchID = pPFA->m_dwUniqueID;
               break;
            }
         }
         else {
            if (((PWSTR)pPFA->m_memFileName.p)[0])
               continue;   // is a file

            // else, compare "file names"
            if (!_wcsicmp ((PWSTR)pInfo->m_memFileName.p, (PWSTR)pInfo2->m_memFileName.p)) {
               // found it
               dwMatchID = pPFA->m_dwUniqueID;
               break;
            }
         }
      } // i

      // if didn't find a match then should make one
      if (dwMatchID == (DWORD)-1) {
         int iRet = AccountNewDisplay (pWindow, pInfoSet, fCircumreality, pszFile);
         // NOTE: If press back, will just end up and main display
         if (iRet >= 0)
            dwMatchID = (DWORD)iRet;
      }

      PasswordFileRelease ();
   }
noload:
   if (pInfoSet)
      delete pInfoSet;

   // clear out pszFile so its empty
   if (pszFile)
      pszFile[0] = 0;

   if (dwMatchID != (DWORD)-1) {
      // play this
      *pdwUniqueID = dwMatchID;
      return 1;
   }


   // clear server load list
   if (!glSERVERLOADQUERY.Num())
      glSERVERLOADQUERY.Init (sizeof(SERVERLOADQUERY));

redo:
   PWSTR pszRet;
   WCHAR szFile[256];
   szFile[0] = 0;
   giVScroll = 0;

   if (*pfNewPlayer) {
      *pfNewPlayer = FALSE;
      fSkipIfOnlyOneFile = TRUE;
      goto newworld;
   }

redowithscroll:
   fSkipIfOnlyOneFile = FALSE;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLMAINPLAYERPAGE, MainPlayerPage, NULL);
   giVScroll = pWindow->m_iExitVScroll;

   if (!pszRet)
      return 0;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redowithscroll;
   else if ((pszRet[0] == L'p') && (pszRet[1] == L'l') && (pszRet[2] == L':')) {
      BOOL fControl = (GetKeyState (VK_CONTROL) < 0);
      BOOL fShift = (GetKeyState (VK_SHIFT) < 0);
      DWORD dwID = (DWORD)_wtoi (pszRet + 3);

      if (fShift) {
         BOOL fRet = AccountEditPageDisplay (pWindow, dwID);
         if (fRet)
            goto redo;
         else
            return 0;
      }

      // have ID to play, so use that
      *pdwUniqueID = dwID;
      *pfQuickLogon = !fControl;
      return 1;
   }
   else if (!_wcsicmp(pszRet, L"readme")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLREADME, ReadMePage);
      if (pszRet && !_wcsicmp(pszRet, L"back"))
         goto redo;
      // else, should close
      return 0;
   }
   else if (!_wcsicmp(pszRet, L"register")) {
      if (RegisterPageDisplay(pWindow))
         goto redo;
      else
         return 0;
   }
   else if (!_wcsicmp(pszRet, L"playerinfo")) {
      if (PlayerInfoPageDisplay(pWindow, FALSE))
         goto redo;
      else
         return 0;
   }
   else if (!_wcsicmp(pszRet, L"changepassword")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLCHANGEPASSWORD, ChangePasswordPage);
      if (pszRet && !_wcsicmp(pszRet, L"back"))
         goto redo;

      // else, should close
      return 0;
   }
   else if (!_wcsicmp(pszRet, L"delete")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLDELETEPASSWORDFILE, DeletePasswordFilePage);
      if (pszRet && !_wcsicmp(pszRet, L"back"))
         goto redo;

      // else, should close
      return 0;
   }
   else if (!_wcsicmp(pszRet, L"newworld")) {
newworld:
      int iRet = AccountNewAskFile (pWindow, fSkipIfOnlyOneFile);
      if (iRet < 0) switch (iRet) {
      case -1: // back
         goto redo;
      case -2: // cancel
      default:
         return 0;   // close
      } // switch

      // need to go onto playing this new world (instead of repeating)
      *pdwUniqueID = (DWORD)iRet;
      return 1;
   }
   else if (!_wcsicmp(pszRet, L"newworldmanual")) {
      int iRet = AccountNewAskFile (pWindow, fSkipIfOnlyOneFile);
      if (iRet < 0) switch (iRet) {
      case -1: // back
         goto redo;
      case -2: // cancel
      default:
         return 0;   // close
      } // switch

      // change some values from default
      PCPasswordFile pPF = PasswordFileCache (FALSE);
      if (pPF) {
         PCPasswordFileAccount pPFA = pPF->AccountGet(pPF->AccountFind((DWORD)iRet));
         if (pPFA) {
            wcscpy (pPFA->m_szUser, L"Unknown");
            wcscpy (pPFA->m_szPassword, L"Unknown");
            pPFA->m_fNewUser = FALSE;
            pPFA->Dirty();
         }
         PasswordFileRelease();
      }

      // need to go onto UI settings (instead of repeating)
      BOOL fRet = AccountEditPageDisplay (pWindow, (DWORD)iRet);
      if (fRet)
         goto redo;
      else
         return 0;
   }
   else
      return 0;
}


/*****************************************************************************
ShardPageDisplay - Asks the user what shard they wish to use. If the user
doesn't need to be asked then this merely returns shard -1.

inputs
   PCEscWindow    pWindow - Window to display it on
   PCResTitleInfoSet pInfoSet - Title info
   BOOL           fCirumreality - TRUE if it's a .crf (and shards won't be used). FALSE if it's a .crk.
returns
   int - 0+ to indicate a shard number. -1 for doesn't need shard (or only one shard, so no UI), -2 for back. -3 for close
*/
int ShardPageDisplay (PCEscWindow pWindow, PCResTitleInfoSet pInfoSet, BOOL fCircumreality)
{
   if (fCircumreality)
      return -1;

   
   PCResTitleInfo pInfo = pInfoSet->Find (gLangID);
   if (!pInfo)
      return -1;

   if (pInfo->m_lPCResTitleInfoShard.Num() < 2)
      return -1;

   // else, ask

   PWSTR pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSHARDPAGE, ShardPage, pInfo);
   if (!pszRet)
      return -3;
   if (!_wcsicmp(pszRet, L"back"))
      return -2;
   if (iswdigit (pszRet[0])) {
      DWORD dwNum = _wtoi(pszRet);
      if (dwNum < pInfo->m_lPCResTitleInfoShard.Num())
         return (int)dwNum;
   }
   return -3;  // unknown
}

/*****************************************************************************
AccountNewDisplay - This adds a new password account given
a PCResTitleInfoSet. If necessary, this pulls up the ShardPage selection.

inputs
   PCEscWindow             pWindow - Window to potentially display stuff on.
   PCResTitleInfoSet       pInfoSet - This is cloned.
   BOOL                    fCirumreality - TRUE if it's a .crf (and shards won't be used). FALSE if it's a .crk.
   PWSTR                   pszFile - File name originally from. Only needed if it's a .crf. .crk (link) files dont use
returns
   int - Unique ID of the account if 0+. -1 for back. -2 for close.
*/
int AccountNewDisplay (PCEscWindow pWindow, PCResTitleInfoSet pInfoSet, BOOL fCircumreality, PWSTR pszFile)
{
   int iShard = ShardPageDisplay (pWindow, pInfoSet, fCircumreality);
   if (iShard < 0) switch (iShard) {
   case -1:
      iShard = 0;
      break;
   case -2:
      return -1;  // back
      break;
   case -3: // close
   default:
      return -2;  // close
      break;
   }

   PCPasswordFile pPF = PasswordFileCache (FALSE);
   if (!pPF)
      return -2;  // shouldnt happen

   WCHAR szFile[256];
   PasswordFileGet (szFile, NULL);

   DWORD dwID = pPF->AccountNew (pInfoSet, (DWORD)iShard, gLangID, szFile, fCircumreality ? pszFile : NULL);

   PasswordFileRelease();

   if (dwID == (DWORD)-1)
      return -1;  // error
   return (int) dwID;
}



/*****************************************************************************
AccountNewFromFile - Create a new account from a file.

inputs
   PCEscWindow             pWindow - Window to potentially display stuff on.
   PWSTR                   pszFile - File name originally from. Only needed if it's a .crf. .crk (link) files dont use
returns
   int - Unique ID of the account if 0+. -1 for back. -2 for close.
*/
int AccountNewFromFile (PCEscWindow pWindow, PWSTR pszFile)
{
   BOOL fCircumreality;
   PCResTitleInfoSet pInfoSet = PCResTitleInfoSetFromFile (pszFile, &fCircumreality);
   if (!pInfoSet)
      // couldnt open for whatever reason
      return -1;

   int iRet = AccountNewDisplay (pWindow, pInfoSet, fCircumreality, pszFile);
   delete pInfoSet;

   return iRet;
}


/*****************************************************************************
AccountNewAskFile - Create a new account by asking for the file.

inputs
   PCEscWindow             pWindow - Window to potentially display stuff on.
   BOOL                    fSkipIfOnlyOneFile - If TRUE and there's only one file in the
                           list, then that file will automatically be used.
returns
   int - Unique ID of the account if 0+. -1 for back. -2 for close.
*/
int AccountNewAskFile (PCEscWindow pWindow, BOOL fSkipIfOnlyOneFile)
{
   WCHAR    szFile[256];
   szFile[0] = 0;
reopen:
   if (!CircumrealityFileOpen (pWindow, FALSE, gLangID, szFile, fSkipIfOnlyOneFile))
      return -1;  // went back

   int iRet = AccountNewFromFile (pWindow, szFile);
   if (iRet == -1) {
      fSkipIfOnlyOneFile = FALSE;   // just in case didn't work
      goto reopen;
   }

   // else
   return iRet;
}


/****************************************************************************
ReLogonPage
*/
BOOL ReLogonPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCResTitleInfoSet pInfoSet = (PCResTitleInfoSet) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"LINKWORLD")) {
            if (!pInfoSet)
               break;

            PCResTitleInfo pInfo = pInfoSet->Find (gLangID);
            if (!pInfo)
               break;

            p->pszSubString = (PWSTR)pInfo->m_memName.p;
            return TRUE;
         }
      }
      break;

   } // switch

   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
LogIn - Brings up the pages that allow to log in.

inputs
   PWSTR       pszFile - Buffer for the file name. Must be 256 chars long. This
                  should originally be filled in with either the file passed into
                  the command line, or "" if none was.
   PCResTitleInfoSet *ppLinkWorld - Where the link for information is stored.
   PWSTR       pszUserName - Filled with the user name to logon with. 64 chars long
   PWSTR       pszPassword - Filled with the password to logon with. 64 chars long.
   BOOL        *pfNewAccount - Filled with TRUE if user indicates to create new account
   BOOL        *pfRemote - Filled in with TRUE if using remote access, FALSE if
                  connecting to local system
   DWORD       *pdwIP - Filled in with the IP address of the server to use,
                  if using remote access
   DWORD       *pdwPort - Filled in with the port to use, if using remote access
   BOOL        *pfQuickLogon - If set to TRUE then logon with few questions. FALSE then
                  ask for specific character, etc.
   PCMem       pMemJPEGBack - Filled in with the background jpeg. m_dwCurPosn = size
returns
   BOOL - TRUE if continue on, FALSE if exit
*/
BOOL LogIn (PWSTR pszFile, PCResTitleInfoSet *ppLinkWorld, PWSTR pszUserName, PWSTR pszPassword, BOOL *pfNewAccount,
            BOOL *pfRemote, DWORD *pdwIP, DWORD *pdwPort, BOOL *pfQuickLogon, PCMem pMemJPEGBack)
{
   pMemJPEGBack->m_dwCurPosn = 0;

   *pfQuickLogon = TRUE;

   CEscWindow cWindow;

   pszUserName[0] = 0;
   pszPassword[0] = 0;
   *pfNewAccount = FALSE;
   *pfRemote = FALSE;
   *pdwIP = 0;
   *pdwPort = 0;
   BOOL fCircumreality = FALSE;
   BOOL fManyShards = FALSE;
   PWSTR pszRet;

   BOOL fRet = TRUE;
   PCResTitleInfoSet pInfoSet = NULL;
   PCResTitleInfo pInfo = NULL;
   CListVariable lMRU;
   RECT r;
   FillXMONITORINFO ();
   PCListFixed pListXMONITORINFO;
   pListXMONITORINFO = ReturnXMONITORINFO();
   PXMONITORINFO p;
   p = NULL;
   DWORD i;
   for (i = 0; i < pListXMONITORINFO->Num(); i++) {
      p = (PXMONITORINFO) pListXMONITORINFO->Get(i);
      if (p->fPrimary)
         break;
   }
   if (p)
      r = p->rWork;
   else {
#undef GetSystemMetrics
      r.left = r.top = 0;
      r.right = GetSystemMetrics (SM_CXSCREEN);
      r.bottom = GetSystemMetrics (SM_CYSCREEN);
   }

   cWindow.Init (ghInstance, NULL, EWS_FIXEDSIZE | EWS_NOTITLE/* | EWS_NOVSCROLL*/, &r);
   EscWindowFontScaleByScreenSize (&cWindow);

   // get the default langid
   gLangID = GetSystemDefaultLangID();

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   DWORD dwFirstTime = TRUE;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      DWORD dw = 1033;
      DWORD dwSize = sizeof(dw);
      DWORD dwType;
      if (ERROR_SUCCESS == RegQueryValueEx (hKey, gpszLangID, NULL, &dwType, (LPBYTE) &dw, &dwSize))
         gLangID = (LANGID)dw;

      //gdwShardIndex = 0;
      //dwSize = sizeof(gdwShardIndex);
      //if (ERROR_SUCCESS != RegQueryValueEx (hKey, gpszShardIndex, NULL, &dwType, (LPBYTE) &gdwShardIndex, &dwSize))
      //   gdwShardIndex = 0;

      pszUserName[0] = 0;
      dwSize = 64 * sizeof(WCHAR);
      if (ERROR_SUCCESS != RegQueryValueExW (hKey, gpszUserName, NULL, &dwType, (LPBYTE) pszUserName, &dwSize))
         pszUserName[0] = 0;

      // see if already used
      dwSize = sizeof(dwFirstTime);
      if (ERROR_SUCCESS != RegQueryValueExW (hKey, gpszFirstTime, NULL, &dwType, (LPBYTE) &dwFirstTime, &dwSize)) {
         dwFirstTime = FALSE;
         RegSetValueExW (hKey, gpszFirstTime, 0, REG_DWORD, (LPBYTE) &dwFirstTime, sizeof(dwFirstTime));
         dwFirstTime = TRUE;
      }
      else
         dwFirstTime = FALSE;

      // login times
      dwSize = sizeof(gdwLoginTimes);
      if (ERROR_SUCCESS != RegQueryValueExW (hKey, gpszLoginTimes, NULL, &dwType, (LPBYTE) &gdwLoginTimes, &dwSize))
         gdwLoginTimes = 0;
      gdwLoginTimes++;
      RegSetValueExW (hKey, gpszLoginTimes, 0, REG_DWORD, (LPBYTE) &gdwLoginTimes, sizeof(gdwLoginTimes));

      // login times
      dwSize = sizeof(gdwLabComputer);
      if (ERROR_SUCCESS != RegQueryValueExW (hKey, gpszLabComputer, NULL, &dwType, (LPBYTE) &gdwLabComputer, &dwSize))
         gdwLabComputer = 0;

      // if this is the first time logging on then determine then CPU speed
      if (ERROR_SUCCESS != RegQueryValueExW (hKey, gpszCPUSpeed, NULL, &dwType, (LPBYTE) &giCPUSpeed, &dwSize)) {
         giCPUSpeed = (int)(DetermineCPUSpeed ()*2.0 + 10.5 + 1.0) - 11 - 1 /* for SSE2 optimizations */;
            // BUGFIX - Was -10, but changed to -11 so that my old dual-core would have a CPU speed of 0
            // => 3 ghz dual core to 1
            // BUGFIX - changed so giCPUSpeed is 0 for 3 ghz dual-core pentium D
            // BUGFIX - Subtract 1 since used new SSE2 optimizations and produced faster code

         RegSetValueExW (hKey, gpszCPUSpeed, 0, REG_DWORD, (LPBYTE) &giCPUSpeed, sizeof(giCPUSpeed));
      }
   }

   // keep global with info about if using render cache
   gfRenderCache = GetRegRenderCache ();


   // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
   // start up sockets
   //WSADATA wsData;
   //int wsErr;
   //wsErr = WSAStartup (MAKEWORD( 2, 2 ), &wsData);
   BOOL fSecondTimeThrough = FALSE;

   if (!_wcsicmp(pszFile, L"***")) {
      // this is the signal that the main window was shut down and we
      // may wish to log in again
      pszFile[0] = 0;   // so dont loop back
      fSecondTimeThrough = TRUE;

      pszRet = cWindow.PageDialog (ghInstance,
         *ppLinkWorld ? IDR_MMLLINKWORLDPAGE :  IDR_MMLRELOGONPAGE,
         ReLogonPage,
         *ppLinkWorld ? *ppLinkWorld : pInfoSet);
      if (!pszRet || _wcsicmp(pszRet, L"yes")) {
         fRet = FALSE;
         goto done;
      }
   }

   // if not paid fully then registration page
   // BUGFIX - Only show registration page after play a few times
   if (!fSecondTimeThrough && (gdwLoginTimes >= 3) && (RegisterMode() != 1)) {
      if (!RegisterPageDisplay (&cWindow)) {
         fRet = FALSE;
         goto done;
      }
   }

   // large TTS page
   // recommend large TTS voice on 8th, 16th, 32nd, 64th, 128th, etc. time
   // BUGFIX - Only do large TTS page if have paid
   BOOL fRegistered, fWin64, fCirc64, fDualCore, fRAM;
   LargeTTSRequirements (&fRegistered, &fWin64, &fCirc64, &fDualCore, &fRAM);
   if (!fSecondTimeThrough && (gdwLoginTimes > 7) && !(gdwLoginTimes & (gdwLoginTimes-1))
      && fRegistered && fWin64 && fCirc64 && fDualCore && fRAM )
         if (!LargeTTSPageDisplay (&cWindow)) {
            fRet = FALSE;
            goto done;
         }

   // user password
   BOOL fNewPlayer = FALSE;
   if (!UserPasswordFilePageDisplay (&cWindow, pszFile, &fNewPlayer)) {
      fRet = FALSE;
      goto done;
   }

reopen:
   // main player page
   DWORD dwUniqueID;
   int iRet = MainPlayerPageDisplay (&cWindow, pszFile, ppLinkWorld, &fNewPlayer, &dwUniqueID, pfQuickLogon);
   switch (iRet) {
   case 1:  // open world
      break;

   case 0:  // exit
   default:
      fRet = FALSE;
      goto done;

   } // switch


   // cache some o this info
   if (pInfoSet) {
      delete pInfoSet;
      pInfoSet = NULL;
   }
   PasswordFileUniqueIDSet (dwUniqueID);
   PCPasswordFileAccount pPFA = PasswordFileAccountCache ();
   DWORD dwShardIndex = 0;
   fCircumreality = FALSE;
   if (pPFA) {
      pInfoSet = pPFA->m_pCResTitleInfoSet ? pPFA->m_pCResTitleInfoSet->Clone() : NULL;
      dwShardIndex = pPFA->m_dwShard;

      // while at it, copy/construct file name
      PWSTR psz = (PWSTR)pPFA->m_memFileName.p;
      if (psz[0]) {
         fCircumreality = TRUE;

         // have a file
         if (wcslen(psz)+1 < 256)
            wcscpy (pszFile, psz);
         else {
            // error
            delete pInfoSet;
            pInfoSet = NULL;
         }
      }
      else {
         // this is a link, so make a phantom file so that will create all the
         // right temporary directories and stuff
         PCResTitleInfo pInfo = pInfoSet->Find (gLangID);
         if (pInfo) {
            WCHAR szTemp[256];
            MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szTemp, sizeof(szTemp));

            // clear illegal characters
            RemoveIllegalFilenameChars ((PWSTR) pInfo->m_memFileName.p);

            DWORD dwLen = (DWORD)wcslen((PWSTR)pInfo->m_memFileName.p) + (DWORD)wcslen(szTemp) + 5;
            if (dwLen + 1 < 256) {
               // create the phantom file name
               wcscpy (pszFile, szTemp);
               wcscat (pszFile, (PWSTR)pInfo->m_memFileName.p);
               wcscat (pszFile, L".crk");
            }
            else {
               // error, too long
               delete pInfoSet;
               pInfoSet = NULL;
            }
         } // if pInfo
      }

      // release
      PasswordFileRelease ();
   } // if managed to cache

   if (!pInfoSet) {
      WCHAR szTemp[512];
      swprintf (szTemp, L"The file, %s, could not be opened.", pszFile);
      EscMessageBox (cWindow.m_hWnd, gpszCircumrealityClient, szTemp, NULL, MB_OK);
      pszFile[0] = 0;
      goto reopen;
   }

#if 0 // no longer need eula
   // see if need EULA
   CircumrealityMRUListEnum (&lMRU);
   BOOL fNeedEULA = TRUE;
   for (i = 0; i < lMRU.Num(); i++)
      if (!_wcsicmp((PWSTR)lMRU.Get(i), pszFile)) {
         fNeedEULA = FALSE;
         break;
      }

redoEULA:
   // display EULA and info
   if (fNeedEULA) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLEULAPAGE, EULAPage, pInfoSet);
      if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
         goto redoEULA;
      if (!pszRet || _wcsicmp(pszRet, L"yes")) {
         pszFile[0] = 0;
         goto reopen;
      }
   }
#endif
   pInfo = pInfoSet->Find (gLangID);
   if (!pInfo) {
      pszFile[0] = 0;
      goto reopen;   // not likely to happen
   }

   // get the image
   if (pMemJPEGBack->Required (pInfo->m_memJPEGBack.m_dwCurPosn)) {
      memcpy (pMemJPEGBack->p, pInfo->m_memJPEGBack.p, pInfo->m_memJPEGBack.m_dwCurPosn);
      pMemJPEGBack->m_dwCurPosn = pInfo->m_memJPEGBack.m_dwCurPosn;
   }

   // add this to MRU
   // not anymore CircumrealityMRUListAdd (pszFile);

redoShard:
   // select shard
   *pfRemote = !fCircumreality;
   fManyShards = FALSE;
   if (!fCircumreality) {
      PCResTitleInfoShard ps = *((PCResTitleInfoShard*)pInfo->m_lPCResTitleInfoShard.Get(dwShardIndex));

      // return port info
      *pdwPort = ps->m_dwPort;

      // figure out if need to ask more UI, etc.
      PWSTR psz = (PWSTR) ps->m_memAddr.p;
      if (BeginsWithHTTP (psz)) {
         *pdwIP = INADDR_NONE;
         gpdwIP = pdwIP;
         pszRet = cWindow.PageDialog (ghInstance, IDR_MMLIPADDRPAGE, IPAddrPage, psz);
         if (!pszRet || _wcsicmp(pszRet, L"ok") || (*pdwIP == INADDR_NONE)) {
            if (fManyShards)
               goto redoShard;
            else {
               pszFile[0] = 0;
               goto reopen;
            }
         }
      }
      else if (AddressToIP (psz, pdwIP)) {
         // do nothing
      }
      else {
         int iErr;
         WCHAR szTemp[512];
         if (NameToIP (psz, pdwIP, &iErr, szTemp))
            goto logonPage;   // succeded


         switch (iErr) {
         case WSAENETDOWN:
         case WSANO_RECOVERY:
         default:
            wcscat (szTemp, L"The network connection seems to be down. Try connecting "
               L"onto the Internet and trying again.");
            break;

         case WSAHOST_NOT_FOUND:
         case WSATRY_AGAIN:
         case WSAEFAULT:
            swprintf (szTemp + wcslen(szTemp), L"The shard's Internet address, %s, could not be found.",
               psz);
            break;
         }
         EscMessageBox (cWindow.m_hWnd, gpszCircumrealityClient, szTemp, NULL, MB_OK);

         if (fManyShards)
            goto redoShard;
         else {
            pszFile[0] = 0;
            goto reopen;
         }
      }
   } // if use remote


logonPage:
   // actually log on
   pPFA = PasswordFileAccountCache ();
   if (!pPFA) {
      // shouldnt happen
      pszFile[0] = 0;
      goto reopen;
   }


   // copy over stuff
   wcscpy (pszUserName, pPFA->m_szUser);
   wcscpy (pszPassword, pPFA->m_szPassword);
   *pfNewAccount = pPFA->m_fNewUser;

   // remember when last used
   pPFA->m_dwLastUsed = DFDATEToday (TRUE);
   pPFA->m_fNewUser = FALSE;
   pPFA->Dirty();

   PasswordFileRelease();

   fRet = TRUE;


done:
   if (pInfoSet)
      delete pInfoSet;

   // BUGFIX - Moved WSAStartup and WSACleanup to main.cpp to see about problems with mom's computer
   // cleanup winsock for now
   // WSACleanup ();

   // save some registry entries
   if (hKey) {
      DWORD dw = gLangID;
      RegSetValueEx (hKey, gpszLangID, 0, REG_DWORD, (BYTE*) &dw, sizeof(dw));
      // RegSetValueEx (hKey, gpszShardIndex, 0, REG_DWORD, (BYTE*) &gdwShardIndex, sizeof(gdwShardIndex));
      RegSetValueExW (hKey, gpszUserName, 0, REG_SZ, (LPBYTE) pszUserName, ((DWORD)wcslen(pszUserName)+1)*sizeof(WCHAR));

      RegCloseKey (hKey);
   }

   return fRet;
}


typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

LPFN_ISWOW64PROCESS 
fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
GetModuleHandle("kernel32"),"IsWow64Process");
 
BOOL IsWow64()
{
    BOOL bIsWow64 = FALSE;
 
    if (NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
        {
            // handle error
        }
    }
    return bIsWow64;
}

/*****************************************************************************
LargeTTSRequirements - Checks the computer to see if it meets the large-voice
TTS requirements.

inputs
   BOOL           *pfRegistered - Filled with TRUE if the player has paid. Can be NULL
   BOOL           *pfWin64 - Filled with TRUE if this is windows 64. Can be NULL.
   BOOL           *pfCirc64 - Filled with TRUE if running 64-bit circumreality. Can be NULL.
   BOOL           *pfDualCore - Filled with TRUE if running a dual-core computer. Can be NULL
   BOOL           *pfRAM - Filled with TRUE if have 2 gig of ram. Can be NULL
returns
   none
*/
void LargeTTSRequirements (BOOL *pfRegistered, BOOL *pfWin64, BOOL *pfCirc64,
                           BOOL *pfDualCore, BOOL *pfRAM)
{
   if (pfRegistered)
      *pfRegistered = (RegisterMode() > 0);

   if (pfWin64) {
      if (sizeof(PVOID) > sizeof(DWORD))
         *pfWin64 = TRUE;  // must be running on 64 since have 64-bit version
      else {
         *pfWin64 = IsWow64();
      }
   } // pfWin64

   if (pfCirc64)
      *pfCirc64 = (sizeof(PVOID) > sizeof(DWORD));

   if (pfDualCore)
      *pfDualCore = (HowManyProcessors() >= 4);
         // BUGFIX - Was dual-core, but upped to quad core

   if (pfRAM)
      *pfRAM = (giTotalPhysicalMemory >= 4000000000);
         // BUGFIX - Was 2 gig, but upped to 4 gig
}
