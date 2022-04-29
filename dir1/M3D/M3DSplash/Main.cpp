/***************************************************************************************
Main.cpp - Main code for M3D splash window

begun 18/5/03 by Mike Rozak
Copyright 2003 by Mike ROzak. All rights reserved.
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


// globals
CMem              gMemTemp;
HINSTANCE         ghInstance;
char           gszAppDir[256];
char           gszAppPath[256];     // application path
PSTR           gpszTutorial = "-tutorial";

/****************************************************************************
RunThis - Runs a program.

inputs
   char           pszFile - File to run. Can also be gpszTutorial. If NULL then empty command line
   HWND           hWnd - Window
   DWORD          dwRun - 0 for .m3d, 1 for .wav. Usually determined from pszFile, but if no FILE then should set,
                     2 for nlp editor
returns
   BOOL - TRUE if success, FALSE if failed
*/
BOOL RunThis (char *pszFile, HWND hWnd, DWORD dwRun = 0)
{
   DWORD dwLen = pszFile ? (DWORD)strlen(pszFile) : 0;
   if ((dwLen >= 4) && !_stricmp(pszFile + (dwLen-4), ".wav"))
      dwRun = 1;  // for m3dwave.exe
   // FUTURE RELEASE - Also option for splicer

   // create the filename
   char szRun[512];
   char szCurDir[256];
   strcpy (szRun, "\"");
   dwLen = (DWORD) strlen(szRun);
   strcpy (szCurDir, gszAppDir);
   strcat (szRun, gszAppDir);
   switch (dwRun) {
      case 0:  /// m3d file
         strcat (szRun, "m3d.exe");
         break;
      case 1:  // wave file
         strcat (szRun, "m3dwave.exe");
         break;
      case 2:  // nlp edtior
         strcat (szRun, "mnlp.exe");
         break;
   }
#if 0
   strcpy (szCurDir, "z:\\bin");
   switch (dwRun) {
   case 0: // m3d
      strcpy (szRun + dwLen, "z:\\m3d\\m3d\\debug\\m3d.exe");
      break;
   case 1:  // m3d wave
      strcpy (szRun + dwLen, "z:\\m3d\\m3dwave\\debug\\m3dwave.exe");
      break;
   }
#endif
   strcat (szRun, "\"");
   if (pszFile) {
      strcat (szRun, " ");
      strcat (szRun, pszFile);
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



/****************************************************************************
SplashPage
*/
BOOL SplashPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
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

         // BUGFIX - Forece to go through tutorial
#if 0
         PCEscControl pControl;
         if (gdwButtonLevelShown < 15) {
            pControl = pPage->ControlFind (L"sample");
            if (pControl)
               pControl->Enable (FALSE);
            pControl = pPage->ControlFind (L"open");
            if (pControl)
               pControl->Enable (FALSE);
            pControl = pPage->ControlFind (L"new");
            if (pControl)
               pControl->Enable (FALSE);
         }
#endif // 0
         // speak
         //EscSpeak (L"Welcome to " APPLONGNAMEW ".");
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p || !p->psz)
            break;
         if ((p->psz[0] == L'm') && (p->psz[1] == L':')) {
            CListVariable gMRU;
            char *psz;
            MRUListEnum (&gMRU);
            psz = (char*)gMRU.Get(_wtoi(p->psz + 2));
            if (!psz)
               return TRUE;

            if (!RunThis (psz, pPage->m_pWindow->m_hWnd))
               return TRUE;
            break;
         }
         else if (!_wcsicmp(p->psz, L"tutorial")) {
            // bring up tutorial
            if (!RunThis (gpszTutorial, pPage->m_pWindow->m_hWnd))
               return TRUE;
            // ASPHelp (IDR_HTUTORIALSTART);
            break;
         }
         else if (!_wcsicmp (p->psz, L"new")) {
            if (!RunThis (NULL, pPage->m_pWindow->m_hWnd, 0))
               return TRUE;
            break;
         }
         else if (!_wcsicmp (p->psz, L"newwave")) {
            if (!RunThis (NULL, pPage->m_pWindow->m_hWnd, 1))
               return TRUE;
            break;
         }
         else if (!_wcsicmp (p->psz, L"nlp")) {
            if (!RunThis (NULL, pPage->m_pWindow->m_hWnd, 2))
               return TRUE;
            break;
         }
         else if (!_wcsicmp (p->psz, L"open") || !_wcsicmp (p->psz, L"sample") ) {
            BOOL fSample = !_wcsicmp (p->psz, L"sample");

            OPENFILENAME   ofn;
            char  szTemp[256];
            szTemp[0] = 0;
            memset (&ofn, 0, sizeof(ofn));
   
            // BUGFIX - Set directory
            char szInitial[256];
            if (fSample)
               strcpy (szInitial, gszAppDir);
            else
               GetLastDirectory(szInitial, sizeof(szInitial));
            ofn.lpstrInitialDir = szInitial;

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;
               ofn.lpstrFilter =
                  "Supported files\0*." M3DFILEEXT ";*.wav\0"
                  APPLONGNAME " file (*." M3DFILEEXT ")\0*." M3DFILEEXT "\0"
                  "Audio file (*.wav)\0*.wav\0"
                  "\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Open " APPLONGNAME " file";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = M3DFILEEXT;
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               return TRUE;   // failed to specify file so go back

            if (!fSample) {
               // BUGFIX - Save diretory
               strcpy (szInitial, ofn.lpstrFile);
               szInitial[ofn.nFileOffset] = 0;
               SetLastDirectory(szInitial);
            }

            // copy the file name to the global
            if (!RunThis (szTemp, pPage->m_pWindow->m_hWnd))
               return TRUE;

            break;
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
         else if (!_wcsicmp(p->pszSubName, L"MRULIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            char *psz;
            WCHAR szTemp[512];
            CListVariable gMRU;
            MRUListEnum (&gMRU);
            for (i = 0; i < gMRU.Num(); i++) {
               psz = (char*) gMRU.Get(i);
               if (!psz)
                  continue;

               MemCat (&gMemTemp, L"<li><a color=#c0c0ff href=m:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">");
               MultiByteToWideChar (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp)/2);
               PWSTR pwsz;
               for (pwsz = szTemp; wcschr(pwsz, L'\\'); pwsz = wcschr(pwsz, L'\\')+1);
               MemCatSanitize (&gMemTemp, pwsz);
               MemCat (&gMemTemp, L"<xHoverHelpShort>");
               MemCatSanitize (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"</xHoverHelpShort>");
               MemCat (&gMemTemp, L"</a></li>");
            }
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/*****************************************************************************
Splash - Brings up the splash screen, which asks the user they want to do.
Based upon user input sets globals so taht files will be loaded.

inputs
   none
returns
   BOOL - TRUE if continue on, FALSE if exit
*/
BOOL Splash (void)
{
   CEscWindow cWindow;

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

   cWindow.Init (ghInstance, NULL, EWS_FIXEDSIZE | EWS_NOTITLE | EWS_NOVSCROLL, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPRESPLASH, DefPage);
   if (!pszRet || _wcsicmp(pszRet, L"next"))
      return FALSE;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLSPLASH, SplashPage);
   return TRUE;
}


/*****************************************************************************
WinMain - DOes the main display loop for 3DOB

inputs
   LPSTR       lpCmdLine - command line
   int         nShowCmd - Show
*/
int WINAPI WinMain(
  HINSTANCE hInstance,      // handle to current instance
  HINSTANCE hPrevInstance,  // handle to previous instance
  LPSTR lpCmdLine,          // command line
  int nCmdShow              // show state
)
{
   ghInstance = hInstance;
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


   // initialize
   EscInitialize (L"mikerozak@bigpond.com", 2511603559, 0);

   // BUGFIX - If not a variant on WinNT then bring up warning since not going to
   // be testing on win98
   OSVERSIONINFO vi;
   memset (&vi, 0, sizeof(vi));
   vi.dwOSVersionInfoSize = sizeof(vi);
   GetVersionEx (&vi);
   if (vi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
      EscMessageBox (NULL, APPLONGNAMEW,
         L"A newer version of windows is recommended.",
         APPSHORTNAMEW L" should work on the version of windows you're using, but "
         L"since it hasn't been extensively tested on that version, we recommend "
         L"using a more recent version of Windows.",
         MB_OK);
   }

   // BUGFIX - Test bit plane
   // warn the user if they don't have their monitor at 24 bits
   HDC   hDC;
   hDC = GetDC (NULL);
   DWORD dwBits;
   dwBits = GetDeviceCaps (hDC, BITSPIXEL);
   ReleaseDC (NULL, hDC);
   if (dwBits < 24) {
      EscMessageBox (NULL, APPLONGNAMEW,
         L"Your display is not set to 24-bit color.",
         L"This will cause degredation in "
         L"the images " APPSHORTNAMEW L" produces. You might want to changed to 24 bit color "
         L"using the Display control panel.",
         MB_OK);
   }


   // init beep window
   //BeepWindowInit ();

   Splash ();

   // dlete beep window
   //BeepWindowEnd ();


   EscUninitialize();

#ifdef _DEBUG
   _CrtCheckMemory ();
#endif // DEBUG

   return 0;
}

