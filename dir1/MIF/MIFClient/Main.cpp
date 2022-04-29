/*************************************************************************************
Main.cpp - Entry code for the M3D wave.

begun 28/2/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <dsound.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "resource.h"
#include "..\buildnum.h"
#include <crtdbg.h>
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "mifclient.h"


HINSTANCE      ghInstance;
char           gszAppDir[256];
char           gszAppPath[256];     // application path
CMem           gMemTemp; // temporary memoty
BOOL           gfQuitBecauseUserClosed;   // set to TRUE if the close was the user's doing

PCMainWindow   gpMainWindow = NULL;
static PSTR    gpszMonitorUseSecond = "MonitorUseSecond";

DWORD          gdwMonitorNum = 0;         // number of monitors attached to the system
BOOL           gfMonitorUseSecond = FALSE;   // if TRUE the use the second monitor
RECT           grMonitorSecondRect = {0,0,0,0};        // rectangle for the secondary monitor

__int64        giTotalPhysicalMemory = LIMITSIZE;

PCResTitleInfoSet gpLinkWorld = NULL;      // where to link to

/*************************************************************************
MonitorInfoFill - Call this to fill in the monitor information,
gdwMonitorNum, grMonitorSecondRect.

inputs
   BOOL           fSecondTime - Set this if is being called a second time.
*/
void MonitorInfoFill (BOOL fSecondTime)
{
   gdwMonitorNum = 1;   // just in case
   gfMonitorUseSecond = FALSE;

   FillXMONITORINFO ();
   PCListFixed plMI = ReturnXMONITORINFO ();
   PXMONITORINFO pMI = (PXMONITORINFO) plMI->Get(0);

   // find the secondary monitor
   DWORD i;
   for (i = 0; i < plMI->Num(); i++, pMI++) {
      if (pMI->fPrimary)
         continue;

      // else, secondary
      gdwMonitorNum++;
      if (gdwMonitorNum == 2)
         grMonitorSecondRect = pMI->rWork;
   } // i

   // exit now if only one monitor; not using second one
   if ((gdwMonitorNum <= 1) || !RegisterMode())
      return;

   // load from registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   gfMonitorUseSecond = (BOOL)111;    // default to FALSE since user may not have second monitor on
   if (hKey) {
      DWORD dwSize, dwType;
      dwSize = sizeof(gfMonitorUseSecond);
      RegQueryValueEx (hKey, gpszMonitorUseSecond, NULL, &dwType, (LPBYTE) &gfMonitorUseSecond, &dwSize);
      RegCloseKey (hKey);
   }
   if ((DWORD)gfMonitorUseSecond == 111) {
      int iRet = fSecondTime ? IDNO : EscMessageBox (NULL, gpszCircumrealityClient,
         L"Do you want to use both your monitors for the game?",
         L"You seem to have two monitors attached to your computers. Circumreality can use both of "
         L"them, displaying the main image in one, and miscellaneous information in the "
         L"second. If you press, \"Yes\", make sure your second monitor is turned on.",
         MB_YESNO);
      gfMonitorUseSecond = (iRet == IDYES);
   }

}


/*************************************************************************
MonitorInfoSave - Saves the monitor info to the registry
*/
void MonitorInfoSave (void)
{
   // dont bother saving if only one monitor
   if ((gdwMonitorNum <= 1) || !RegisterMode())
      return;

   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      RegSetValueEx (hKey, gpszMonitorUseSecond, 0, REG_DWORD, (LPBYTE) &gfMonitorUseSecond, sizeof(gfMonitorUseSecond) );
      RegCloseKey (hKey);
   }
}


/*************************************************************************
AllowToDoubleClick - Writes the registry entries so a user can fp click

inputs
   char           *pszSuffix - Suffix, either "crf" or "crk"
   char           *pszFileName - Long file name, such as "Circumreality internet"
returns
   BOOL - TRUE if set, FALSE if already set
*/
BOOL AllowToDoubleClick (char *pszSuffix, char *pszFileName)
{
   char  szTempDoc[64];
   sprintf (szTempDoc, "%sDocoument.1", pszSuffix);

   HKEY  hInfo = NULL;
   DWORD dwDisp;
   char  szTemp[256], szTemp2[256];

   // GetModuleFileName (ghInstance, gszAppPath, sizeof(gszAppPath));

   // verify that it's not already written
   sprintf (szTemp, "\"%s\" %%1", gszAppPath);
   sprintf (szTemp2, "%sDocoument.1\\shell\\open\\command", pszSuffix);
   RegOpenKeyEx (HKEY_CLASSES_ROOT, szTemp2, 0,
      KEY_READ, &hInfo);
   if (hInfo) {
      char  szTemp2[256];
      szTemp2[0] = 0;
      DWORD dwType;
      dwDisp = sizeof(szTemp2);
      RegQueryValueEx (hInfo, NULL, 0, &dwType, (BYTE*) szTemp2, &dwDisp);
      RegCloseKey (hInfo);

      if (!_stricmp(szTemp, szTemp2))
         return FALSE;
   }
   // else, not there, so write

   // so can fp click .M3DFILEEXT
   sprintf (szTemp2, ".%s", pszSuffix);
   RegCreateKeyEx (HKEY_CLASSES_ROOT, szTemp2, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szTempDoc, (DWORD)strlen(szTempDoc)+1);
      RegCloseKey (hInfo);
   }

   RegCreateKeyEx (HKEY_CLASSES_ROOT, szTempDoc, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) pszFileName, (DWORD)strlen(pszFileName)+1);
      RegCloseKey (hInfo);
   }

   sprintf (szTemp, "%s,0", gszAppPath);
   sprintf (szTemp2, "%sDocoument.1\\DefaultIcon", pszSuffix);
   RegCreateKeyEx (HKEY_CLASSES_ROOT, szTemp2, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szTemp, (DWORD)strlen(szTemp)+1);
      RegCloseKey (hInfo);
   }

   sprintf (szTemp2, "%sDocoument.1\\protocol\\StdFileEditing\\Server", pszSuffix);
   RegCreateKeyEx (HKEY_CLASSES_ROOT, szTemp2, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) gszAppPath, (DWORD)strlen(gszAppPath)+1);
      RegCloseKey (hInfo);
   }

   sprintf (szTemp, "\"%s\" %%1", gszAppPath);
   sprintf (szTemp2, "%sDocoument.1\\shell\\open\\command", pszSuffix);
   RegCreateKeyEx (HKEY_CLASSES_ROOT, szTemp2, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szTemp, (DWORD)strlen(szTemp)+1);
      RegCloseKey (hInfo);
   }

   return TRUE;
}



/*****************************************************************************
CircumrealityMainLoop - DOes the main display loop for 3DOB

inputs
   LPSTR       lpCmdLine - command line
   int         nShowCmd - Show
*/
int CircumrealityMainLoop (LPSTR lpCmdLine, int nShowCmd)
{
#if 0 // def _DEBUG // test malloc
   DWORD i;
   for (i = 0; i < 100; i++)
      if (!ESCMALLOC(100 * 1000000))
         break;
#endif

#if 0 // to test ServerLoadQuery()
   SERVERLOADQUERY aSLQ[5];
   memset (aSLQ, 0, sizeof(aSLQ));
   DWORD i;
   for (i = 0; i < 5; i++) {
      aSLQ[i].dwPort = 4000;
   } // i
   wcscpy (aSLQ[0].szDomain, L"144.135.186.245");
   wcscpy (aSLQ[1].szDomain, L"127.0.0.1");
   wcscpy (aSLQ[2].szDomain, L"http://www.microsoft.com");
   wcscpy (aSLQ[3].szDomain, L"127.0.0.1");
   wcscpy (aSLQ[4].szDomain, L"http://www.mxac.com.au");

   ServerLoadQuery (aSLQ, 5, NULL);
#endif // 0

   // get the name
   GetModuleFileName (ghInstance, gszAppPath, sizeof(gszAppPath));
   strcpy (gszAppDir, gszAppPath);
   char  *pCur;
   for (pCur = gszAppDir + strlen(gszAppDir); pCur >= gszAppDir; pCur--)
      if (*pCur == '\\') {
         pCur[1] = 0;
         break;
      }

   // BUGFIX - If has "/Commit" then run EXE again, but don't wait
   if (!_stricmp (lpCmdLine, "/Commit")) {
      RunCircumrealityClient ();
      return 0;
   }

   // allow to double click in explorer
   if (AllowToDoubleClick ("crp", "Circumreality user password file")) {
      AllowToDoubleClick ("crf", "Circumreality file");
      AllowToDoubleClick ("crk", "Circumreality link");
   }

#ifdef _DEBUG
   // Get current flag
   int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

   // Turn on leak-checking bit
   tmpFlag |= _CRTDBG_LEAK_CHECK_DF; // | _CRTDBG_CHECK_ALWAYS_DF;
   // BUGBUG - disable this, as it should be tmpFlag |= _CRTDBG_CHECK_ALWAYS_DF; // | _CRTDBG_DELAY_FREE_MEM_DF;
      // BUGBUG - really slow so find the memory problem

   // BUGFIX - Turn off the high values so dont check for leak every 10K
   // Do this to make things faster
   tmpFlag &= ~(_CRTDBG_CHECK_EVERY_1024_DF);

   // Set flag to the new value
   _CrtSetDbgFlag( tmpFlag );

   // test
   //char *p;
   //p = (char*)MYMALLOC (42);
   // p[43] = 0;
#endif // _DEBUG


   // initialize
   EscInitialize (L"mikerozak@bigpond.com", 2511603559, 0);
   MIDIRemapSet ((HWND)-1, 0);
   CMem memJPEGBack;

   WSADATA wsData;
   int iWinSockErr = WSAStartup (MAKEWORD( 2, 2 ), &wsData);
   if (iWinSockErr) {
      WCHAR szTemp[512];
      swprintf (szTemp,
         L"You may have the internet turned off on your computer, or your firewall may be preventing CircumReality from connectting."
         L"\r\nTechnical: WSAStartup() failed, returing %d.", (int)iWinSockErr);
      
      EscMessageBox (NULL, gpszCircumrealityClient, L"Error starting the internet.", szTemp, MB_OK);
      goto exit;
   }

   MonitorInfoFill (FALSE);

   WCHAR szFile[256] = L"";
   WCHAR szUserName[64], szPassword[64];
   BOOL fNewAccount;
   MultiByteToWideChar (CP_ACP, 0, lpCmdLine, -1, szFile, sizeof(szFile)/sizeof(WCHAR));

   BOOL fRemote, fQuickLogon;
   DWORD dwIP, dwPort;
relogon:
   memJPEGBack.m_dwCurPosn = 0;
   if (!LogIn (szFile, &gpLinkWorld, szUserName, szPassword, &fNewAccount, &fRemote, &dwIP, &dwPort, &fQuickLogon, &memJPEGBack))
      goto exit;

   // delete the link world if there is one now
   if (gpLinkWorld) {
      delete gpLinkWorld;
      gpLinkWorld = NULL;
   }

   gfQuitBecauseUserClosed = FALSE;
   gpMainWindow = new CMainWindow;
   if (!gpMainWindow)
      goto exit;
   if (!gpMainWindow->Init (szFile, szUserName, szPassword, fNewAccount, fRemote, dwIP, dwPort, fQuickLogon, 
      &memJPEGBack)) {
      // dont do since moved loc BeepWindowInit ();   // so will be initailized when end
      delete gpMainWindow;
      gpMainWindow = NULL;
      goto exit;
   }

   // init beep window
   // BUGFIX - Move dinitializing beep window until after
   BeepWindowInit ();

   // window loop
   MSG msg;
   HACCEL hAccel = LoadAccelerators (ghInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));
   while( GetMessage( &msg, NULL, 0, 0 ) ) {
      // translate accelerators
      if (hAccel && gpMainWindow && gpMainWindow->m_hWndPrimary &&
         TranslateAccelerator (gpMainWindow->m_hWndPrimary, hAccel, &msg))
            continue;

      // translate the push to talk keys
      if (
         ((msg.message == WM_KEYDOWN) || (msg.message == WM_KEYUP)) &&
         gpMainWindow && gpMainWindow->m_hWndPrimary &&
         gpMainWindow->m_VoiceChatInfo.m_fAllowVoiceChat && !gpMainWindow->m_fTempDisablePTT &&
         gpMainWindow->m_VoiceDisguise.m_fAgreeNoOffensive && RegisterMode()) {

            // test virtual key
            //UINT uVal = MapVirtualKey (msg.wParam, 2);
            //char c = (char)uVal;
            //if ((c == '`') || (c == '~')) {
            if (msg.wParam == VK_CONTROL) {
               // trap this and start/stop recording
               if (msg.message == WM_KEYUP)
                  gpMainWindow->VoiceChatStop ();
               else
                  gpMainWindow->VoiceChatStart ();
               // dont trap... continue;   // trapped
            }
      }

      TranslateMessage( &msg );
      DispatchMessage( &msg );
   }
   if (gpMainWindow)
      delete gpMainWindow;
   gpMainWindow = NULL;
   MIDIRemapSet ((HWND)-1, 0);   // since no more main window

   // delete beep window
   BeepWindowEnd ();
 
   if (!gfQuitBecauseUserClosed) {
      // go back to the login
      wcscpy (szFile, L"***");  // so knows restarting
      goto relogon;
   }

exit:
   // free lexicon and tts
   TTSCacheShutDown();
   MLexiconCacheShutDown (TRUE);

   ButtonBitmapReleaseAll ();
   // FUTURERELEASE - end help ASPHelpEnd ();


   if (gpLinkWorld)
      delete gpLinkWorld;


   // end internet
   WSACleanup ();

   EscUninitialize();

   MonitorInfoSave ();

#ifdef _DEBUG
   _CrtCheckMemory ();
#endif // DEBUG
   return 0;
}

/*****************************************************************************
WinMain */
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
   ghInstance = hInstance;

   // if this is a computer with a lot of memory, then use
   // memory manager that doesn't fragment
   MEMORYSTATUSEX ms;
   memset (&ms, 0, sizeof(ms));
   ms.dwLength = sizeof(ms);
   GlobalMemoryStatusEx (&ms);
#if 0 // def _DEBUG // to test
   ms.ullTotalPhys = 0;
#endif
   // BUGFIX - If not 64-bit then optimize memory too
   // BUGFIX - Went to optimized memory if less then 2gig. Changed to 4 gig.
   // BUGFIX - Optimized memory if less than 4 cores (since don't really need)
#if 0 // BUGBUG - try my memory... def _DEBUG  // BUGBUG - to try and find crash
   EscMallocOptimizeMemory (TRUE);
#else
   EscMallocOptimizeMemory (
      (sizeof(PVOID) <= sizeof(DWORD)) ||
      (ms.ullTotalPhys < 4000000000) ||
      (HowManyProcessors() < 4) );
#endif

   giTotalPhysicalMemory = (__int64) ms.ullTotalPhys;

   // BUGFIX - Make sure to set this thread priority so can work around vista hack
   // BUGFIX - So higher thread priority on vista
   // BUGFIX - Turn this off since TTS not getting enough priority compared to renderer
   // hopefully doesn't cause problems when vista is sucking up all cpu
   VistaThreadPriorityHackSet (FALSE);
   // VistaThreadPriorityHackSet (TRUE);
   SetThreadPriority (GetCurrentThread(), VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));  // so doesnt suck up all CPU

   return CircumrealityMainLoop (lpCmdLine, nShowCmd);
}



