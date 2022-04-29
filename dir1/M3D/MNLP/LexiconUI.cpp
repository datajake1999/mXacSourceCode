/*************************************************************************************
LexiconUI.cpp - Displays some UI for lexicon

begun 19/9/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "mnlp.h"


/* globals */
static PCMLexicon gpLexEdit = NULL; // lexicon that editing
static PCVoiceFile gpVoiceEdit = NULL; // lexicon that editing
static PCTTSWork gpTTSEdit = NULL; // tts editing
static PCMTTS gpTTSDerivedEdit = NULL; // tts editing

/****************************************************************************
LexiconPage
*/
static BOOL LexiconPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         if (!_wcsicmp(p->psz, L"newmaster") || !_wcsicmp(p->psz, L"newex")) {
            WCHAR szFile[256];
            szFile[0] = 0;

            if (!MLexiconOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), TRUE))
               return TRUE;

            // see if exists...
            PCMLexicon pLex;
            pLex = MLexiconCacheOpen (szFile, FALSE);
            if (pLex) {
               if (IDYES != pPage->MBYesNo (L"The lexicon already exists. Are you sure you wish to overwrite it?",
                  L"Pressing Yes will clear out the existing lexicon."))
                  return TRUE;

               // else, note that edditing
               pLex->Clear();
               pLex->m_fDirty = TRUE;
               gpLexEdit = pLex;

               if (!_wcsicmp(p->psz, L"newex"))
                  pLex->MasterLexSet (L"");

               break;   // normal exit
            } // if already have

            // try creating
            pLex = MLexiconCacheOpen (szFile, TRUE);
            if (!pLex)
               return TRUE;   // error
            pLex->m_fDirty = TRUE;  // so will save

            if (!_wcsicmp(p->psz, L"newex"))
               pLex->MasterLexSet (L"");

            // note that editing
            gpLexEdit = pLex;
            break;
         } // if new master
         else if (!_wcsicmp(p->psz, L"editmaster") || !_wcsicmp(p->psz, L"editex")) {
            WCHAR szFile[256];
            szFile[0] = 0;

            if (!MLexiconOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            // see if exists...
            PCMLexicon pLex;
            pLex = MLexiconCacheOpen (szFile, FALSE);
            if (!pLex) {
               pPage->MBWarning (L"The lexicon file couldn't be opened.",
                  L"It may not be a proper lexicon.");
               return TRUE;
            }

            // note that editing
            gpLexEdit = pLex;
            break;
         } // if new master
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Lexicon";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
LexiconRoot - Pulls up the root page for the lexicon.

inputs
   PCEscWindow          pWindow - Window
returns
   BOOL - TRUE if user pressed back, FALSE if something else
*/
BOOL LexiconRoot (PCEscWindow pWindow)
{
   PWSTR pszRet;

redo:
   gpLexEdit = NULL;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXICON, LexiconPage, NULL);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;

   if ((!_wcsicmp(pszRet, L"newmaster") || !_wcsicmp(pszRet, L"editmaster") || !_wcsicmp(pszRet, L"newex") || !_wcsicmp(pszRet, L"editex")) && gpLexEdit) {
      BOOL fRet = gpLexEdit->DialogMain (pWindow);

      // free up
      MLexiconCacheClose (gpLexEdit);
      gpLexEdit = NULL;
      if (fRet)
         goto redo;
      else
         return FALSE;
   }

   // else
   return FALSE;
}





/****************************************************************************
SRTrainPage
*/
static BOOL SRTrainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         if (!_wcsicmp(p->psz, L"newsrtrain")) {
            WCHAR szFile[256];
            szFile[0] = 0;

            if (!VoiceFileOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), TRUE))
               return TRUE;

            // see if exists...
            PCVoiceFile pVoice;
            pVoice = new CVoiceFile;
            if (pVoice && pVoice->Open (szFile)) {
               delete pVoice;
               pVoice = NULL;

               if (IDYES != pPage->MBYesNo (L"The speech recognition training file already exists. Are you sure you wish to overwrite it?",
                  L"Pressing Yes will clear out the existing voice file."))
                  return TRUE;

               // else, note that edditing
               gpVoiceEdit = new CVoiceFile;
               if (!gpVoiceEdit)
                  return TRUE;
               wcscpy (gpVoiceEdit->m_szFile, szFile);

               break;   // normal exit
            } // if already have
            else if (pVoice)
               delete pVoice;

            // try creating
            gpVoiceEdit = new CVoiceFile;
            if (!gpVoiceEdit)
               return TRUE;
            wcscpy (gpVoiceEdit->m_szFile, szFile);
            break;
         } // if new master
         else if (!_wcsicmp(p->psz, L"editsrtrain")) {
            WCHAR szFile[256];
            szFile[0] = 0;

            if (!VoiceFileOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            // see if exists...
            gpVoiceEdit = new CVoiceFile;
            if (!gpVoiceEdit)
               return TRUE;
            if (!gpVoiceEdit->Open (szFile)) {
               delete gpVoiceEdit;
               gpVoiceEdit = NULL;
               pPage->MBWarning (L"The speech recognition training  file couldn't be opened.",
                  L"It may not be a proper speech recognition training file.");
               return TRUE;
            }

            // done
            break;
         } // if new master
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Speech recognition training";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
SRTrainRoot - Pulls up the root page for the lexicon.

inputs
   PCEscWindow          pWindow - Window
returns
   BOOL - TRUE if user pressed back, FALSE if something else
*/
BOOL SRTrainRoot (PCEscWindow pWindow)
{
   PWSTR pszRet;

redo:
   gpVoiceEdit = NULL;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSRTRAIN, SRTrainPage, NULL);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;

   if ((!_wcsicmp(pszRet, L"newsrtrain") || !_wcsicmp(pszRet, L"editsrtrain")) && gpVoiceEdit) {
      BOOL fRet = gpVoiceEdit->DialogMain (pWindow);

      // save
      gpVoiceEdit->Save(NULL);

      // free up
      delete gpVoiceEdit;
      gpVoiceEdit = NULL;
      if (fRet)
         goto redo;
      else
         return FALSE;
   }

   // else
   return FALSE;
}






/****************************************************************************
TTSPage
*/
static BOOL TTSPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         if (!_wcsicmp(p->psz, L"newmaster")) {
            WCHAR szFile[256];
            szFile[0] = 0;

            if (!TTSWorkFileOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), TRUE))
               return TRUE;

            // see if exists...
            PCTTSWork pTTS;
            pTTS = new CTTSWork;
            if (pTTS && pTTS->Open (szFile)) {
               delete pTTS;
               pTTS = NULL;

               if (IDYES != pPage->MBYesNo (L"The TTS project file already exists. Are you sure you wish to overwrite it?",
                  L"Pressing Yes will clear out the existing TTS project file."))
                  return TRUE;

               // else, note that edditing
               gpTTSEdit = new CTTSWork;
               if (!gpTTSEdit)
                  return TRUE;
               wcscpy (gpTTSEdit->m_szFile, szFile);

               break;   // normal exit
            } // if already have
            else if (pTTS)
               delete pTTS;

            // try creating
            gpTTSEdit = new CTTSWork;
            if (!gpTTSEdit)
               return TRUE;
            wcscpy (gpTTSEdit->m_szFile, szFile);
            break;
         } // if new master
         else if (!_wcsicmp(p->psz, L"editmaster")) {
            WCHAR szFile[256];
            szFile[0] = 0;

            if (!TTSWorkFileOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            // see if exists...
            gpTTSEdit = new CTTSWork;
            if (!gpTTSEdit)
               return TRUE;
            if (!gpTTSEdit->Open (szFile)) {
               delete gpTTSEdit;
               gpTTSEdit = NULL;
               pPage->MBWarning (L"The TTS project file couldn't be opened.",
                  L"It may not be a proper TTS project file.");
               return TRUE;
            }

            // done
            break;
         } // if new master
         else if (!_wcsicmp(p->psz, L"newex")) {
            WCHAR szFile[256];
            szFile[0] = 0;

            if (!TTSFileOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), TRUE))
               return TRUE;

            // see if exists...
            PCMTTS pTTS;
            pTTS = new CMTTS;
            if (pTTS && pTTS->Open (szFile, TRUE)) {
                     // BUGBUG - opening as thread safe, for test purposes. Doesn't really matter but
                     // could just as well be set to FALSE
               delete pTTS;
               pTTS = NULL;

               if (IDYES != pPage->MBYesNo (L"The TTS voice file already exists. Are you sure you wish to overwrite it?",
                  L"Pressing Yes will clear out the existing TTS voice file."))
                  return TRUE;

               // else, note that edditing
               gpTTSDerivedEdit = new CMTTS;
               if (!gpTTSDerivedEdit)
                  return TRUE;
               wcscpy (gpTTSDerivedEdit->m_szFile, szFile);
               gpTTSDerivedEdit->IsDerivedSet(TRUE);

               break;   // normal exit
            } // if already have
            else if (pTTS)
               delete pTTS;

            // try creating
            gpTTSDerivedEdit = new CMTTS;
            if (!gpTTSDerivedEdit)
               return TRUE;
            wcscpy (gpTTSDerivedEdit->m_szFile, szFile);
            gpTTSDerivedEdit->IsDerivedSet(TRUE);
            break;
         } // if new derived
         else if (!_wcsicmp(p->psz, L"editex")) {
            WCHAR szFile[256];
            szFile[0] = 0;

            if (!TTSFileOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            // see if exists...
            gpTTSDerivedEdit = new CMTTS;
            if (!gpTTSDerivedEdit)
               return TRUE;
            if (!gpTTSDerivedEdit->Open (szFile, TRUE)) {
                     // BUGBUG - opening as thread safe, for test purposes. Doesn't really matter but
                     // could just as well be set to FALSE
               delete gpTTSDerivedEdit;
               gpTTSDerivedEdit = NULL;
               pPage->MBWarning (L"The TTS voice file couldn't be opened.",
                  L"It may not be a proper TTS voice file.");
               return TRUE;
            }

            if (!gpTTSDerivedEdit->IsDerivedGet()) {
               delete gpTTSDerivedEdit;
               gpTTSDerivedEdit = NULL;
               pPage->MBWarning (L"This isn't a modified TTS voice.",
                  L"It is a master TTS voice and can't be edited.");
               return TRUE;
            }

            // done
            break;
         } // if new derived
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Text-to-speech";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
TTSRoot - Pulls up the root page for the lexicon.

inputs
   PCEscWindow          pWindow - Window
returns
   BOOL - TRUE if user pressed back, FALSE if something else
*/
BOOL TTSRoot (PCEscWindow pWindow)
{
   PWSTR pszRet;

   // BUGFIX - clear the TTS cache since if anything was cached it may
   // be made valid by editing tts
   TTSCacheShutDown ();

redo:
   gpTTSEdit = NULL;
   gpTTSDerivedEdit = NULL;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTTS, TTSPage, NULL);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;

   if ((!_wcsicmp(pszRet, L"newmaster") || !_wcsicmp(pszRet, L"editmaster")) && gpTTSEdit) {
      BOOL fRet = gpTTSEdit->DialogMain (pWindow, FALSE);

      // save
      gpTTSEdit->Save(NULL);

      // free up
      delete gpTTSEdit;
      gpTTSEdit = NULL;
      if (fRet)
         goto redo;
      else
         return FALSE;
   }
   else if ((!_wcsicmp(pszRet, L"newex") || !_wcsicmp(pszRet, L"editex")) && gpTTSDerivedEdit) {
      BOOL fRet = gpTTSDerivedEdit->DialogDerivedMain (pWindow);

      // save
      gpTTSDerivedEdit->Save(NULL);

      // free up
      delete gpTTSDerivedEdit;
      gpTTSDerivedEdit = NULL;
      if (fRet)
         goto redo;
      else
         return FALSE;
   }

   // else
   return FALSE;
}
