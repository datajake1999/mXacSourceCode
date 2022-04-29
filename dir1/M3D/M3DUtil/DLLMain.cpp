/*********************************************************************************
DLLMain.cpp - Main gateway for DLL.

begun 4/20/2000
Copyright 2000 by Mike Rozak
All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <crtdbg.h>
#include <objbase.h>
#include <initguid.h>
#include "escarpment.h"
#include "..\m3d.h"

void BuiltInRead (void);

extern CRITICAL_SECTION     gcsResLeak;    // for leak detection

HINSTANCE      ghInstance;
char           gszAppDir[256];
char           gszAppPath[256];     // application path
CMem           gMemTemp; // temporary memoty



BOOL WINAPI DllMain(  HINSTANCE hinstDLL,  // handle to DLL module
  DWORD fdwReason,     // reason for calling function
  LPVOID lpvReserved)   // reserved)
{
   ghInstance = hinstDLL;

   switch (fdwReason) {
   case DLL_PROCESS_ATTACH:
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


         // make sure built-in are read so no thread races
         BuiltInRead ();

#ifdef _DEBUG
         InitializeCriticalSection (&gcsResLeak);
#endif // _DEBUG

#ifdef _DEBUG
         // Get current flag
         int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

         // Turn on leak-checking bit
         tmpFlag |= _CRTDBG_LEAK_CHECK_DF;// | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF;
         //tmpFlag |=  _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF;

         // tmpFlag = LOWORD(tmpFlag) | (_CRTDBG_CHECK_EVERY_1024_DF << 4); // BUGFIX - So dont check for memory overwrites that often, make things faster

         // Set flag to the new value
         _CrtSetDbgFlag( tmpFlag );

         // test
         //char *p;
         //p = (char*)MYMALLOC (42);
         // p[43] = 0;
#endif // _DEBUG
      }
      break;
   case DLL_PROCESS_DETACH:
      {

   #ifdef _DEBUG
         _CrtCheckMemory ();
   #endif // DEBUG

#ifdef _DEBUG
         DeleteCriticalSection (&gcsResLeak);
#endif // _DEBUG
      }
      break;
   }

   return TRUE;
}
// BUGFIX - when use the debug samplecontrols.exe with debug escarpment.dll
// get asserts on object deletion. Need to find way to tell VC++ to use
// the app's constructor & destructor.
// I had to fix this by not having the EXE do a new and the DLL then deleting it.

