/*************************************************************************************
Main.cpp - Entry code for the M3D wave.

begun 19/9/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
//#include <zmouse.h>
#include <commctrl.h>
#include <objbase.h>
#include <shlobj.h>
#include <initguid.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\buildnum.h"
#include <crtdbg.h>
#include "mnlp.h"

#undef GetSystemMetrics

char gszFileWantToOpen[256] = "";   // want to open this file
HINSTANCE      ghInstance;
char           gszAppDir[256];
char           gszAppPath[256];     // application path
CMem           gMemTemp; // temporary memoty
PCWaveView     gpWaveView = NULL;
static PWSTR   gpszCircumrealityVoices = L"CircumRealityTTSVoices";
static PWSTR   gpszmXac = L"mXac";

LANGID         glidNewVoice = 1033; // default language ID for new voice
WCHAR          gszNewVoiceName[256]; // default name of the new voice
DWORD          gdwNewVoiceGender = 1;  // default new voice gender
DWORD          gdwNewVoiceAge = 2;     // default new voice age

BOOL MNLPAppDataDirGet (PWSTR psz);

/****************************************************************************
MainPage
*/
BOOL MainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"mXac NLP Editor";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
SimplePage
*/
BOOL SimplePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS)pParam;
         if (!p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"waveedit")) {
            if (!WaveEditorRun (NULL, NULL, pPage->m_pWindow->m_hWnd))
               pPage->MBError (L"For some unknown reason, the wave editor didn't run.",
                  L"Contact Mike@mXac.com.au about this.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"registersapi")) {
            CMTTS TTS;

            // set the last directory to the SAPI voices directory so that
            // open file dialog will go there
            WCHAR szDir[256];
            MNLPAppDataDirGet (szDir);
            SetLastDirectory  (szDir);

            // get the filename
            WCHAR szFile[256];
            szFile[0] = 0;

            if (!TTSFileOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            // open it
            if (!TTS.Open (szFile, TRUE)) {
               pPage->MBError (L"The text-to-speech file couldn't be opened.", szFile);
               return TRUE;
            }


            if (!TTS.SAPIRegister()) {
               WCHAR szTemp[1024], szDir[256];
               MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szDir, sizeof(szDir)/sizeof(WCHAR));

               swprintf (szTemp,
                  L"Microsoft Windows prevents \"ordinary\" applications from registering SAPI voices. "
                  L"You'll need shut down this application (MNLP.exe), and re-run it using \"Run as Administrator\". "
                  L"\r\n"
                  L"You can do this by going to the application install directory, %s, "
                  L"and right-clicking on MNLP.exe and selecting \"Run as Administrator\"."
                  L"\r\n"
                  L"Once MNLP.exe is running, press the \"I want to use a voice with a SAPI application\" "
                  L"button again.",
                  szDir);

               pPage->MBWarning (L"The voice couldn't be registered with SAPI.", szTemp);
            }
            else
               pPage->MBInformation (L"You can now use the voice from SAPI applications.");
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"mXac NLP Editor - Create your own text-to-speech voice";
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"HAVEENOUGHMEMORY")) {
            MemZero (&gMemTemp);

            DWORD dwRet = TTSWorkEnoughMemory (1000);

            if (dwRet & 0x04) {
               if (dwRet & 0x01)
                  MemCat (&gMemTemp, L"<font color=#800000><bold>You aren't running 64-bit Windows.</bold></font> ");
               else
                  MemCat (&gMemTemp, L"You are running 64-bit Windows, but <font color=#800000><bold>you must install the 64-bit version of CircumReality from www.CircumReality.com</bold></font> to create a large voice. ");
            }
            else
               MemCat (&gMemTemp, L"You are running 64-bit Windows. ");

            if (dwRet & 0x02)
               MemCat (&gMemTemp, L"<font color=#800000><bold>You don't have enough memory for a large voice.</bold></font> ");
            else
               MemCat (&gMemTemp, L"You have enough memory for a large voice.");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"EXISTINGFILES")) {
            MemZero (&gMemTemp);

            WCHAR szDir[256];
            if (!MNLPAppDataDirGet (szDir))
               return FALSE;

            WCHAR szFind[256];
            WCHAR szFile[256];
            wcscpy (szFind, szDir);
            wcscat (szFind, L"*");

            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW (szFind, &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
               while (TRUE) {
                  // must be directory
                  if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                     goto nextfile;
                  if (fd.cFileName[0] == L'.')
                     goto nextfile;

                  // see if can open file
                  wcscpy (szFile, szDir);
                  wcscat (szFile, fd.cFileName);
                  wcscat (szFile, L"\\Voice.mtv");

                  PCTTSWork pWork = new CTTSWork;
                  if (!pWork)
                     goto nextfile;
                  if (!pWork->Open (szFile)) {
                     delete pWork;
                     goto nextfile;
                  }
                  delete pWork;

                  // add
                  MemCat (&gMemTemp, L"<xChoiceButton href=\"edit:");
                  MemCatSanitize (&gMemTemp, szFile);
                  MemCat (&gMemTemp, L"\"><bold>Add more recordings to ");
                  MemCatSanitize (&gMemTemp, fd.cFileName);
                  MemCat (&gMemTemp, L"</bold><br/>Add some more recordings to a "
                     L"text-to-speech voice that you have already been working on."
                     L"<p/><italic>This file is saved in ");
                  MemCatSanitize (&gMemTemp, szFile);
                  MemCat (&gMemTemp,
                     L"</italic></xChoiceButton>");

nextfile:
                  if (!FindNextFileW (hFind, &fd))
                     break;
               } // while TRUE
               FindClose (hFind);
            } // if find
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}


/*********************************************************************************
MNLPLangComboBoxSet - Fills a combobox with a list of languages, and sets the
given language.

inputs
   PCEscPage      pPage - Page
   PWSTR          pszControl - Control
   LANGID         lid - Language
returns
   LANGID - 0 if error. Otherwise language ID for default
*/
LANGID MNLPLangComboBoxSet (PCEscPage pPage, PWSTR pszControl, LANGID lid)
{
   CMem mem;

   // clear the existing combo
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return 0;
   pControl->Message (ESCM_COMBOBOXRESETCONTENT);

   // get all the languages
   CListFixed lLangID;
   CListVariable lLangName;
   TTSWorkLangEnum (&lLangID, &lLangName);

   MemZero (&mem);

   DWORD i;
   LANGID *pl = (LANGID*)lLangID.Get(0);
   DWORD dwSel = 0;
   LANGID lidRet = lid;
   for (i = 0; i < lLangID.Num(); i++) {
      if (!i)
         lidRet = pl[i];

      if (lid == pl[i]) {
         dwSel = i;
         lidRet = pl[i];
      }
      PWSTR psz = (PWSTR)lLangName.Get(i);
      
      MemCat (&mem, L"<elem name=");
      MemCat (&mem, (int)pl[i]);
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

   return lidRet;
}




/************************************************************************************
MNLPAppDataDirGet - Gets the application data directory, and makes sure it's created.

inputs
   PWSTR       psz - String to fill in. Must be MAX_PATH. Will be of the form, "c:\\path\\"
returns
   BOOL - TRUE if success
*/
BOOL MNLPAppDataDirGet (PWSTR psz)
{
   if (!SUCCEEDED(SHGetFolderPathW (NULL, CSIDL_FLAG_CREATE | CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, psz)))
      return FALSE;

   // append CircumReality
   if (wcslen(psz) + wcslen(gpszCircumrealityVoices) + wcslen(gpszmXac) + 4 > MAX_PATH)
      return FALSE;
   wcscat (psz, L"\\");
   wcscat (psz, gpszmXac);
   // make sure directory exists
   CreateDirectoryW (psz, NULL);

   wcscat (psz, L"\\");
   wcscat (psz, gpszCircumrealityVoices);

   // make sure directory exists
   CreateDirectoryW (psz, NULL);

   // add another backslash just to make easier
   wcscat (psz, L"\\");

#if 0 // to test
   EscMessageBox (NULL, gpszCircumrealityVoices,  L"Where the files are.", psz, MB_OK);
#endif

   return TRUE;
}


/************************************************************************************
NewVoiceVerifyName - Verifies the name of the new voice

inputs
   PWSTR          pszName - Name of the voice, like "Mike Rozak"
   PWSTR          pszFile - Filled with the filename, like "c:\\mXac\\MyDir\\Voice.mtv"
returns
   DWORD - 0 if success, 1 if failed because can't create directory, 2 if failed because file alredy exists
*/
DWORD NewVoiceVerifyName (PWSTR pszName, PWSTR pszFile)
{
   // must have something
   if (!pszName[0])
      return 1;

   WCHAR szRoot[256];
   if (!MNLPAppDataDirGet (szRoot))
      return 1;

   // see about directory
   wcscpy (pszFile, szRoot);
   wcscat (pszFile, pszName);

   // make sure directory exists
   CreateDirectoryW (pszFile, NULL);

   // full name
   wcscat (pszFile, L"\\Voice.mtv");

   // see if the file already exists
   FILE *pf = _wfopen (pszFile, L"rb");
   if (pf) {
      fclose (pf);
      return 2;
   }

   // try create hack file
   pf = _wfopen (pszFile, L"wb");
   if (pf) {
      fclose (pf);
      return 0;
   }

   // else, can't write
   return 1;
}


/****************************************************************************
NewVoicePage
*/
BOOL NewVoicePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         glidNewVoice = MNLPLangComboBoxSet (pPage, L"language", glidNewVoice);
         ComboBoxSet (pPage, L"gender", gdwNewVoiceGender);
         ComboBoxSet (pPage, L"age", gdwNewVoiceAge);

         PCEscControl pControl;

         if (pControl = pPage->ControlFind (L"voicename"))
            pControl->AttribSet (Text(), gszNewVoiceName);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE)pParam;
         if (!p->pControl->m_pszName)
            break;
         DWORD dwVal = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;

         if (!_wcsicmp (p->pControl->m_pszName, L"language")) {
            if ((LANGID)dwVal == glidNewVoice)
               return TRUE;   // already done

            // else change
            glidNewVoice = (LANGID)dwVal;
            return TRUE;
         }
         if (!_wcsicmp (p->pControl->m_pszName, L"gender")) {
            if (dwVal == gdwNewVoiceGender)
               return TRUE;   // already done

            // else change
            gdwNewVoiceGender = dwVal;
            return TRUE;
         }
         if (!_wcsicmp (p->pControl->m_pszName, L"age")) {
            if (dwVal == gdwNewVoiceAge)
               return TRUE;   // already done

            // else change
            gdwNewVoiceAge = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (!_wcsicmp (p->pControl->m_pszName, L"voicename")) {
            DWORD dwNeeded;
            p->pControl->AttribGet (Text(), gszNewVoiceName, sizeof(gszNewVoiceName), &dwNeeded);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS)pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about "newvoice"
         if (_wcsicmp(p->pControl->m_pszName, L"newvoice"))
            break;

         // make sure can create the directory
         WCHAR szFile[256];
         DWORD dwRet = NewVoiceVerifyName (gszNewVoiceName, szFile);
         if (dwRet) {
            if (dwRet == 1)
               pPage->MBWarning (L"The file for the voice can't be created.",
                  L"You may have typed in an illegal character in the name. Try using only letters and numbers.");
            else
               pPage->MBWarning (L"You already have a voice using that name.");

            return TRUE;
         }

         // actually create data and fill in information
         PCTTSWork pWork = new CTTSWork;
         if (!pWork)
            return TRUE;   // shouldnt happen

         // set some values
         MemZero (&pWork->m_memSAPIName);
         MemCat (&pWork->m_memSAPIName, gszNewVoiceName);
         MemZero (&pWork->m_memSAPIToken);
         MemCat (&pWork->m_memSAPIToken, gszNewVoiceName);
         pWork->m_dwAge = gdwNewVoiceAge;
         pWork->m_dwGender = gdwNewVoiceGender;
         pWork->m_LangID = glidNewVoice;

         // lexicon
         WCHAR szTemp[256];
         if (!TTSWorkDefaultFileName (glidNewVoice, NULL, TWDF_LEXICON, szTemp))
            return TRUE;   // shouldnt happen because already verified
         pWork->LexiconSet (szTemp);

         // file
         if (!TTSWorkDefaultFileName (glidNewVoice, NULL,
            (!pWork->m_dwGender && (pWork->m_dwAge >= 1)) ? TWDF_SRMALE : TWDF_SRFEMALE, szTemp))
            return TRUE;   // shouldnt happen because already verified
         wcscpy (pWork->m_szSRFile, szTemp);

         // save. Should work because verified can save
         pWork->Save (szFile);

         delete pWork;

         // return edit:
         wcscpy (szTemp, L"edit:");
         wcscat (szTemp, szFile);
         pPage->Exit (szTemp);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Start recording my voice";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
AboutPage - Search page callback. It handles standard operations
*/
BOOL AboutPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"About mXac NLP Editor";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DFVERSION")) {
            static WCHAR szVersion[16];   // for about box
            MultiByteToWideChar (CP_ACP, 0, BUILD_NUMBER, -1, szVersion, sizeof(szVersion)/2);
            p->pszSubString = szVersion;
            return TRUE;
         }
      }
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}



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

   {
      CEscWindow cWindow;
      RECT r;
      int iWidth = GetSystemMetrics (SM_CXSCREEN);
      int iHeight = GetSystemMetrics (SM_CYSCREEN);
      r.left = iWidth / 6;
      r.right = iWidth * 5 / 6;
      r.top = iHeight / 10;
      r.bottom = iHeight * 9 / 10;

      cWindow.Init (ghInstance, NULL, 0/*EWS_FIXEDSIZE*/, &r);
      PWSTR pszRet;

      while (TRUE) {
         pszRet = cWindow.PageDialog (ghInstance, IDR_MMLSIMPLE, SimplePage, NULL);
         if (!pszRet)
            break;

doediting:
         // handle "edit:"
         if (!_wcsnicmp (pszRet, L"edit:", 5)) {
            WCHAR szFile[256];
            wcscpy (szFile, pszRet + 5);

            // BUGFIX - clear the TTS cache since if anything was cached it may
            // be made valid by editing tts
            TTSCacheShutDown ();

            // else, note that edditing
            PCTTSWork pWork = new CTTSWork;
            if (!pWork)
               continue;
            wcscpy (pWork->m_szFile, szFile);
            if (!pWork->Open (szFile)) {
               delete pWork;
               pWork = NULL;
               continue;   // shouldnt happen
            }

            BOOL fRet = pWork->DialogMain (&cWindow, TRUE);

            // save
            pWork->Save(NULL);

            // free up
            delete pWork;
            pWork = NULL;
            if (fRet)
               continue;
            else
               break;
         } // if edit:

         else if (!_wcsicmp(pszRet, L"newvoice")) {
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLNEWVOICE, NewVoicePage, NULL);
            if (!pszRet)
               break;

            if (!_wcsnicmp(pszRet, L"edit:", 5)) {
               goto doediting;
            }

            if (!_wcsicmp(pszRet, Back()))
               continue;

            // else done
            break;
         }

         else if (!_wcsicmp(pszRet, L"advancedstuff")) {
            while (TRUE) {

               pszRet = cWindow.PageDialog (ghInstance, IDR_MMLMAIN, MainPage, NULL);
      again:
               if (!pszRet)
                  break;

               if (!_wcsicmp(pszRet, L"about")) {
                  pszRet = cWindow.PageDialog (ghInstance, IDR_MMLABOUT, AboutPage, NULL);
                  if (pszRet && !_wcsicmp(pszRet, Back()))
                     continue;
                  goto again;
               }
               else if (!_wcsicmp(pszRet, L"lexicon")) {
                  if (LexiconRoot (&cWindow))
                     continue;
                  else
                     break;
               }
               else if (!_wcsicmp(pszRet, L"tts")) {
                  if (TTSRoot (&cWindow))
                     continue;
                  else
                     break;
               }
               else if (!_wcsicmp(pszRet, L"srtrain")) {
                  if (SRTrainRoot (&cWindow))
                     continue;
                  else
                     break;
               }

               // else done
               break;
            }
         if (pszRet && !_wcsicmp(pszRet, Back()))
            continue;
         else
            break;
      } // advanced stuff

      // else done
      break;
   } // while (TRUE) for simple
   }


   // shut down the lexicons
   TTSCacheShutDown();
   MLexiconCacheShutDown (TRUE);

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

   return M3DMainLoop (lpCmdLine, nShowCmd);
}



