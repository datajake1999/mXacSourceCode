/***************************************************************************
RecBatch.cpp - Code to do batch recording

begun 20/9/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
//#include <mmreg.h>
//#include <msacm.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"

typedef struct {
   PWSTR             pszText;    // text spoken
   PWSTR             pszFile;    // file to edit
   PCM3DWave         pOrig;      // pointer to the original wave
   PCM3DWave         pSynth;     // pointer to the synthesized wave
   BOOL              fDontReview;   // if TRUE, dont review any more recordings
} RECREV, *PRECREV;

/****************************************************************************
RecReviewPage
*/
static BOOL RecReviewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PRECREV pRR = (PRECREV) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"playorig")) {
            pRR->pSynth->QuickPlayStop();
            pRR->pOrig->QuickPlay ();
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"edit")) {
            CWaveDirInfo wdi;
            wdi.EditFile (NULL, pRR->pszFile, pPage->m_pWindow->m_hWnd);

            pPage->MBInformation (L"The wave editor has been run.",
               L"Press OK when you finish editing so that your recording session can continue.");
            pPage->Exit (L"keep");
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"dontreviewmore")) {
            pRR->fDontReview = TRUE;
            pPage->Exit (L"keep");
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         PWSTR psz = p->psz;

         if (!_wcsicmp(psz, L"playsynth")) {
            pRR->pOrig->QuickPlayStop();
            pRR->pSynth->QuickPlay();
            return TRUE;
         }
         else if ((psz[0] == L'w') && (psz[1] == L':')) {
            DWORD dwWord = _wtoi(psz+2);
            PWVWORD pww1 = (PWVWORD) pRR->pOrig->m_lWVWORD.Get(dwWord);
            PWVWORD pww2 = (PWVWORD) pRR->pOrig->m_lWVWORD.Get(dwWord+1);
            pRR->pSynth->QuickPlayStop();
            pRR->pOrig->QuickPlay (pww1 ? pww1->dwSample : 0,
               pww2 ? pww2->dwSample : pRR->pOrig->m_dwSamples);
            return TRUE;
         }
         else if ((psz[0] == L'p') && (psz[1] == L':')) {
            DWORD dwPhoneme = _wtoi(psz+2);
            PWVPHONEME pww1 = (PWVPHONEME) pRR->pOrig->m_lWVPHONEME.Get(dwPhoneme);
            PWVPHONEME pww2 = (PWVPHONEME) pRR->pOrig->m_lWVPHONEME.Get(dwPhoneme+1);
            pRR->pSynth->QuickPlayStop();
            pRR->pOrig->QuickPlay (pww1 ? pww1->dwSample : 0,
               pww2 ? pww2->dwSample : pRR->pOrig->m_dwSamples);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Review recording";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TEXT")) {
            p->pszSubString = pRR->pszText;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WORDWAVE")) {
            MemZero (&gMemTemp);

            PCListVariable plWord = &pRR->pOrig->m_lWVWORD;
            BOOL fFirst = TRUE;
            DWORD i;
            for (i = 0; i < plWord->Num(); i++) {
               PWVWORD pwv = (PWVWORD) plWord->Get(i);
               if (!pwv)
                  continue;
               PWSTR psz = (PWSTR)(pwv+1);
               if (!psz[0])
                  continue;   // blank

               // first time
               if (!fFirst)
                  MemCat (&gMemTemp, L" ");
               else
                  fFirst = FALSE;

               // add link
               MemCat (&gMemTemp, L"<a href=w:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</a>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PHONEWAVE")) {
            MemZero (&gMemTemp);

            PCListFixed plPhone = &pRR->pOrig->m_lWVPHONEME;
            BOOL fFirst = TRUE;
            DWORD i;
            for (i = 0; i < plPhone->Num(); i++) {
               WCHAR sz[16];
               PWVPHONEME pwv = (PWVPHONEME) plPhone->Get(i);
               if (!pwv)
                  continue;
               memset (&sz, 0 ,sizeof(sz));
               memcpy (sz, pwv->awcNameLong, sizeof(pwv->awcNameLong));

               if (!_wcsicmp(sz, L"<s>"))
                  continue;   // skip silence

               // first time
               if (!fFirst)
                  MemCat (&gMemTemp, L" ");
               else
                  fFirst = FALSE;

               // add link
               MemCat (&gMemTemp, L"<a href=p:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, sz);
               MemCat (&gMemTemp, L"</a>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}




/***************************************************************************
RecBatch - Record a batch of files.

inputs
   DWORD             dwCalcFor - As per other dwCalcFor
   PCEscWindow       pWindow - To show record info in. If this is NULL then hWnd must be filled in
   PCWSTR            pszVoiceFile - Voice file path to use for segmentation.
   BOOL              fRequireVoiceFile - If TRUE then the voice file must be loadable
                     and it must have the necessary phonemes trained, if FALSE then
                     don't require it, but warn user
   BOOL              fUIToReview - If TRUE show the UI to review for TTS.
                     if FALSE, no UI so use for SR. Must be FALSE if pWindow == NULL
   PCWaveToDo        pWaveToDo - List of sentences to record
   PCWaveDirInfo     pWaveDirInfo - Where to save the file
   PCWSTR            pszMasterFile - Master file name, to use if need root directory
                        when invent file name
   PCWSTR            pszStartName - Root identifier of file such as "speechrecog"
   HWND              hWnd - If !pWindow, then this is the window to display the record info off of
returns
   DWORD - Number of sentneces recorded
*/
DWORD RecBatch (DWORD dwCalcFor, PCEscWindow pWindow, PCWSTR pszVoiceFile, BOOL fRequireVoiceFile,
                BOOL fUIToReview, PCWaveToDo pWaveToDo, PCWaveDirInfo pWaveDirInfo,
                PCWSTR pszMasterFile, PCWSTR pszStartName, HWND hWnd)
{
   // if nothing in batch then done
   if (pWindow)
      hWnd = pWindow->m_hWnd;
   else if (!hWnd)
      return 0;   // didnt record

   if (!pWaveToDo->m_lToDo.Num()) {
      EscMessageBox (hWnd, ASPString(),
         L"You don't have any sentence in the to-do list.",
         NULL,
         MB_ICONEXCLAMATION | MB_OK);
      return 0;
   }

   // load in voice if can
   BOOL fSRValid = FALSE;
   CVoiceFile VF;
   if (pszVoiceFile && pszVoiceFile[0] && VF.Open ((WCHAR*)pszVoiceFile))
      fSRValid = TRUE;
   if (!fSRValid) {
      if (fRequireVoiceFile) {
         EscMessageBox (hWnd, ASPString(),
            L"The speech recognition training file didn't load. You must have one to record.",
            (PWSTR) (pszVoiceFile ? pszVoiceFile : L""),
            MB_ICONEXCLAMATION | MB_OK);
         return 0;
      }
      else {
         if (pszVoiceFile && (IDYES != EscMessageBox (hWnd, ASPString(),
            L"The speech recognition training file didn't load. You don't need one to continue, but it's recommended. "
            L"Do you want to continue?",
            (PWSTR) (pszVoiceFile ? pszVoiceFile : L""),
            MB_ICONEXCLAMATION | MB_YESNO)) )
            return 0;
      }
   }
   else {
      fSRValid = (VF.TrainVerifyAllTrained(NULL) == 0);

      if (!fSRValid && fRequireVoiceFile) {
         EscMessageBox (hWnd, ASPString(),
            L"The speech recognition training file doesn't have training for all the phonemes. "
            L"You must have all the phonemes trained to record.",
            (PWSTR) (pszVoiceFile ? pszVoiceFile : L""),
            MB_ICONEXCLAMATION | MB_OK);
         return 0;
      }
      else if (!fSRValid) {
         if (IDYES != EscMessageBox (hWnd, ASPString(),
            L"The speech recognition training doesn't have training for all the phonemes so it can't be used. You don't need one to continue, but it's recommended. "
            L"Do you want to continue?",
            (PWSTR) (pszVoiceFile ? pszVoiceFile : L""),
            MB_ICONEXCLAMATION | MB_YESNO))
            return 0;
      }
   }

   // loop through all the sentences
   DWORD dwRecorded = 0;
   BOOL fWantQuit  = FALSE;
   while (pWaveToDo->m_lToDo.Num()) {
      PWSTR psz = (PWSTR) pWaveToDo->m_lToDo.Get(0);

      // invent a name for the file
      WCHAR szFile[256], szSpeaker[256];
      if (!pWaveDirInfo->InventFileName (pszMasterFile, pszStartName, szFile,
         sizeof(szFile)/sizeof(WCHAR), szSpeaker, sizeof(szSpeaker)/sizeof(WCHAR))) {
            EscMessageBox (hWnd, ASPString(),
               L"A file for the recording couldn't be created.",
               NULL,
               MB_ICONEXCLAMATION | MB_OK);
            return 0;
         }

      // create the wave
      PCM3DWave pWave = new CM3DWave;
      if (!pWave)
         continue;

      // set right sampling rate, etc.
      WAVEFORMATEX wfex;
      memset (&wfex, 0, sizeof(wfex));
      wfex.cbSize = 0;
      wfex.wFormatTag = WAVE_FORMAT_PCM;
      wfex.nChannels = 1;
      wfex.nSamplesPerSec = 22050;
      wfex.wBitsPerSample = 16;
      wfex.nBlockAlign  = wfex.nChannels * wfex.wBitsPerSample / 8;
      wfex.nAvgBytesPerSec = wfex.nBlockAlign * wfex.nSamplesPerSec;
      pWave->ConvertWFEX (&wfex, NULL);
      
      // set the text and speaker
      if (pWave->m_memSpeaker.Required((wcslen(szSpeaker)+1)*sizeof(WCHAR)))
         wcscpy ((PWSTR)pWave->m_memSpeaker.p, szSpeaker);
      if (pWave->m_memSpoken.Required((wcslen(psz)+1)*sizeof(WCHAR)))
         wcscpy ((PWSTR)pWave->m_memSpoken.p, psz);
      //if (pWave->m_memSoundsLike.Required((wcslen(psz)+1)*sizeof(WCHAR)))
      //   wcscpy ((PWSTR)pWave->m_memSoundsLike.p, psz);

      // record
      PCM3DWave pNew;
      pNew = pWave->Record (hWnd, NULL);
      if (!pNew) {
         delete pWave;
         break;
      }
      if (!pNew->m_dwSamples) {
         delete pNew;
         delete pWave;
         break;
      }

      // set speaker in pnew
      if (pNew->m_memSpeaker.Required((wcslen(szSpeaker)+1)*sizeof(WCHAR)))
         wcscpy ((PWSTR)pNew->m_memSpeaker.p, szSpeaker);
      if (pNew->m_memSpoken.Required((wcslen(psz)+1)*sizeof(WCHAR)))
         wcscpy ((PWSTR)pNew->m_memSpoken.p, psz);
      //if (pNew->m_memSoundsLike.Required((wcslen(psz)+1)*sizeof(WCHAR)))
      //   wcscpy ((PWSTR)pNew->m_memSoundsLike.p, psz);

      // do speech recognition on it
      PCM3DWave pSynth = NULL;
      if (fSRValid) {
         CProgress Progress;
         Progress.Start (hWnd, "Analyzing...", TRUE);

         if (fUIToReview)
            Progress.Push (0, .8);

         // calculate pitch
         Progress.Push (0, .33);
         pNew->CalcPitch (dwCalcFor, &Progress);
         Progress.Pop ();

         // calculate SR features
         Progress.Push (.33, .5);
         pNew->CalcSRFeaturesIfNeeded (dwCalcFor, hWnd, &Progress);
         Progress.Pop ();

         Progress.Push (.5, 1);
         VF.Recognize (psz, pNew, FALSE, &Progress);
         Progress.Pop();

         if (fUIToReview && (pSynth = pNew->Clone())) {
            Progress.Pop ();
            Progress.Push (.8, 1);

            // make copy
            CVoiceSynthesize VS;
            VS.SynthesizeFromSRFeature (4, pSynth, NULL, 0, 0.0, NULL, TRUE, &Progress);
         }
      }

      // save just in case edit
      WideCharToMultiByte (CP_ACP, 0, szFile, -1, pNew->m_szFile, sizeof(pNew->m_szFile), 0, 0);
      // BUGFIX - So can bulk record while have megafile open
      HMMIO hmmio = mmioOpen (pNew->m_szFile, NULL, MMIO_WRITE | MMIO_CREATE | MMIO_EXCLUSIVE );
      if (!pNew->Save (TRUE, NULL, hmmio))
         EscMessageBox (hWnd, ASPString(),
            L"The wave file failed to save.",
            szFile,
            MB_ICONEXCLAMATION | MB_OK);
      if (hmmio)
         mmioClose (hmmio, 0);

      // UI to review wave
      BOOL fReRecord = FALSE;
      if (fUIToReview && pWindow) {
         RECREV rr;
         memset (&rr, 0, sizeof(rr));
         rr.pOrig = pNew;
         rr.pSynth = pSynth;
         rr.pszText = psz;
         rr.pszFile = szFile;

         PWSTR pszRet;
         pszRet = pWindow->PageDialog (ghInstance, IDR_MMLRECREVIEW, RecReviewPage, &rr);
         if (!pszRet) {
            DeleteFile (pNew->m_szFile);
            fWantQuit = TRUE;
         }
         else if (!_wcsicmp(pszRet, Back()) || !_wcsicmp(pszRet, L"rerecord")) {
            DeleteFile (pNew->m_szFile);
            fReRecord = TRUE;
         }
         else if (_wcsicmp(pszRet, L"keep")) {
            DeleteFile (pNew->m_szFile);
            fWantQuit = TRUE;
         }

         // if marked as no more SR then do so
         if (rr.fDontReview) {
            fUIToReview = FALSE;
            fSRValid = FALSE; // so dont try SR
         }
      }

      // finally, save
      if (pSynth)
         delete pSynth;
      delete pWave;
      delete pNew;

      // add to list
      if (!fWantQuit && !fReRecord) {
         dwRecorded++;

         PCWaveFileInfo PWFI = new CWaveFileInfo;
         if (PWFI) {
            PWFI->SetText (szFile, psz, szSpeaker);
            pWaveDirInfo->m_lPCWaveFileInfo.Add (&PWFI);
         }

         // remove from to do list
         pWaveToDo->m_lToDo.Remove (0);
      }

      if (fWantQuit)
         break;
   } // while sentences

   return dwRecorded;
}

/***************************************************************************
RecBatch2 - Record a batch of files, different UI.

inputs
   DWORD             dwCalcFor - As per other dwCalcFor
   HWND              hWnd - The window to display off of. if pWindow, ignored
   PCWSTR            pszVoiceFile - Voice file path to use for segmentation.
   BOOL              fRequireVoiceFile - If TRUE then the voice file must be loadable
                     and it must have the necessary phonemes trained, if FALSE then
                     don't require it, but warn user
   PCListVariable    plWaveToDo - List of sentences to record
   PWSTR             pszDir - Directory to save in.
   PCWSTR            pszStartName - Root identifier of file such as "speechrecog"
returns
   DWORD - Number of sentneces recorded
*/
DWORD RecBatch2 (DWORD dwCalcFor, HWND hWnd, PCWSTR pszVoiceFile, BOOL fRequireVoiceFile,
                PCListVariable plWaveToDo, PWSTR pszDir,
                PCWSTR pszStartName)
{
   CWaveToDo wtd;
   DWORD i;
   for (i = 0; i < plWaveToDo->Num(); i++)
      wtd.AddToDo ((PWSTR)plWaveToDo->Get(i));

   // clean diectory name
   WCHAR szDir[512];
   wcscpy (szDir, pszDir);
   DWORD dwLen = (DWORD)wcslen (szDir);
   if (dwLen && (szDir[dwLen-1] == L'\\'))
      szDir[dwLen-1] = 0;  // so no extra backslash

   CWaveDirInfo wdi;
   wcscpy (wdi.m_szDir, pszDir);
   wdi.SyncWithDirectory (NULL, TRUE);

   // make up a fake master name
   WCHAR szMaster[512];
   wcscpy (szMaster, szDir);
   wcscat (szMaster, L"\\Unknown.exe");

   return RecBatch (dwCalcFor, NULL, pszVoiceFile, fRequireVoiceFile, FALSE,
      &wtd, &wdi, szMaster, pszStartName, hWnd); 
}
