/*********************************************************************************
DLLMain.cpp - Main gateway for DLL.

begun 4/20/2000
Copyright 2000 by Mike Rozak
All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <crtdbg.h>
#include "resleak.h"
#include "escarpment.h"

extern CListVariable     glistJPEG;     // list of JPEG resource to file
extern CListVariable     glistBMP;      // list of bitmap resource to file
extern CListVariable     glistMML;     // list of MML resource to file
extern CMem           gMemChime;           // memory to store current chime in
extern CBTree  gtreeControlCallback;
extern CListFixed    glistGDIObject;
extern CListFixed gListFontCache;   // list of font GFONTCACHE
extern CRITICAL_SECTION gcsEscInit;
extern CRITICAL_SECTION gcsEscMultiThreaded;

HINSTANCE      ghInstance;

BOOL WINAPI DllMain(  HINSTANCE hinstDLL,  // handle to DLL module
  DWORD fdwReason,     // reason for calling function
  LPVOID lpvReserved)   // reserved)
{
   ghInstance = hinstDLL;

   switch (fdwReason) {
   case DLL_PROCESS_ATTACH:
      {
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

         InitializeCriticalSection (&gcsEscInit);
         InitializeCriticalSection (&gcsEscMultiThreaded);

         EscMallocInit ();

         EscAtomicStringInit();
         EscMultiThreadedInit();
      }
      break;
   case DLL_PROCESS_DETACH:
      {
         // show leaks
         PCBitmapCache p = EscBitmapCache ();
         if (p) {
            p->CacheCleanUp(TRUE);
            p->ClearCompletely();
         }
         RLEND();

         glistJPEG.ClearCompletely();
         glistBMP.ClearCompletely();
         glistMML.ClearCompletely();
         gMemChime.ClearCompletely();
         gtreeControlCallback.ClearCompletely();
#ifdef _DEBUG
         glistGDIObject.ClearCompletely();
#endif // _DEBUG
         gListFontCache.ClearCompletely();

         EscMultiThreadedEnd();
         EscAtomicStringEnd();

   #ifdef _DEBUG
         _CrtCheckMemory ();
   #endif // DEBUG

         EscMallocEnd();

         DeleteCriticalSection (&gcsEscInit);
         DeleteCriticalSection (&gcsEscMultiThreaded);

         EscOutputDebugStringClose ();
      }
      break;
   }

   return TRUE;
}


#if 0 // replaced by ESCMALLOC
/***************************************************************************
Code to detect memoyr leaks
*/
void *MyMalloc (unsigned int iSize)
{
   return EscMalloc (iSize);
}

void MyFree (void *pMem)
{
   EscFree (pMem);
}

void *MyRealloc (void *pMem, unsigned int iSize)
{
   return EscRealloc (pMem, iSize);
}
#endif // 0

// BUGFIX - when use the debug samplecontrols.exe with debug escarpment.dll
// get asserts on object deletion. Need to find way to tell VC++ to use
// the app's constructor & destructor.
// I had to fix this by not having the EXE do a new and the DLL then deleting it.
