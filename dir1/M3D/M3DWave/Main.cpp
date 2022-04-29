/*************************************************************************************
Main.cpp - Entry code for the M3D wave.

begun 1/5/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <zmouse.h>
#include <commctrl.h>
#include <objbase.h>
#include <initguid.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\buildnum.h"
#include <crtdbg.h>

char gszFileWantToOpen[256] = "";   // want to open this file
HINSTANCE      ghInstance;
char           gszAppDir[256];
char           gszAppPath[256];     // application path
CMem           gMemTemp; // temporary memoty
PCWaveView     gpWaveView = NULL;

/*****************************************************************************
M3DMainLoop - DOes the main display loop for 3DOB

inputs
   LPSTR       lpCmdLine - command line
   int         nShowCmd - Show
*/
int M3DMainLoop (LPSTR lpCmdLine, int nShowCmd)
{
   // get the name
   GetModuleFileName (ghInstance, gszAppPath, sizeof(gszAppPath));
   strcpy (gszAppDir, gszAppPath);
   char  *pCur;
   for (pCur = gszAppDir + strlen(gszAppDir); pCur >= gszAppDir; pCur--)
      if (*pCur == '\\') {
         pCur[1] = 0;
         break;
      }
#ifdef _DEBUG
   // Get current flag
   int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

   // Turn on leak-checking bit
   tmpFlag |= _CRTDBG_LEAK_CHECK_DF; // | _CRTDBG_CHECK_ALWAYS_DF;
   //tmpFlag |= _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_DELAY_FREE_MEM_DF;

   // BUGFIX - Turn off the high values so dont check for leak every 10K
   // Do this to make things faster
   // tmpFlag &= ~(_CRTDBG_CHECK_EVERY_1024_DF);

   // Set flag to the new value
   _CrtSetDbgFlag( tmpFlag );

   // test
   //char *p;
   //p = (char*)MYMALLOC (42);
   // p[43] = 0;
#endif // _DEBUG


   // call this so scrollbars look better
   InitCommonControls ();

   // initialize
   EscInitialize (L"mikerozak@bigpond.com", 2511603559, 0);

   EscMallocOptimizeMemory (TRUE);  // for optimized memory usage
      // slightly faster with FALSE passed on, but < 1% and uses more memory

   // FUTURERELEASE - ASPHelpInit ();

   // init beep window
   BeepWindowInit ();

   if (lpCmdLine && lpCmdLine[0])
      strcpy (gszFileWantToOpen, lpCmdLine);
   else
      gszFileWantToOpen[0] = 0;

   // start up with blank world, or open a file
open:
   PCM3DWave pWave;
   pWave = new CM3DWave;
   if (gszFileWantToOpen[0] == '*')
      gszFileWantToOpen[0] = 0;  // special signal sent for file new
   if (gszFileWantToOpen[0]) {
      BOOL fRet;
      {
         CProgress Progress;
         Progress.Start (GetDesktopWindow(), "Loading wave...");
         fRet = pWave->Open (&Progress, gszFileWantToOpen);
      }

      // if didn't open then new
      if (!fRet) {
         pWave->New();
         EscMessageBox (GetDesktopWindow(), ASPString(),
            L"The wave file couldn't be opened.",
            L"It may not exist, or may not be a proper wave file. A new file will be created instead.",
            MB_ICONEXCLAMATION | MB_OK);
      }
      else
         MRUListAdd (gszFileWantToOpen);
   }
   gszFileWantToOpen[0] = 0;

   gpWaveView = new CWaveView;
   gpWaveView->Init (pWave);

   // window loop
   MSG msg;
   while( GetMessage( &msg, NULL, 0, 0 ) ) {
      TranslateMessage( &msg );
      DispatchMessage( &msg );
      // FUTURERELEASE - help page ASPHelpCheckPage ();
   }
 
   if (gszFileWantToOpen[0])
      goto open;

   // free lexicon and tts
   TTSCacheShutDown();
   MLexiconCacheShutDown (TRUE);

   ButtonBitmapReleaseAll ();
   // FUTURERELEASE - end help ASPHelpEnd ();


   // dlete beep window
   BeepWindowEnd ();


   EscUninitialize();

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

#if 0 // to verify 64 bit
   char szTemp[64];
   sprintf (szTemp, "Sizeof(pvoid) = %d", (int)sizeof(PVOID));
   MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // 0

   return M3DMainLoop (lpCmdLine, nShowCmd);
}



