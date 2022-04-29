/*************************************************************************************
CVoiceDisguise - Object for voice disguising.

begun 2/8/05 by Mike Rozak.
Copyright 2005 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <crtdbg.h>
#include <float.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "m3dwave.h"


// BUGFIX - Don't bend phases when do voice disguise
// #define VDHACK_BENDPHASES              // if turned on then bend phases

#define VOICEDISGUISEPAGE_STARTWIZARD        1        // start of the microphone and voice disguise setup wizard

#define VOICEDISGUISEPAGE_TYPESOFMICS        1000     // types of microphones
#define VOICEDISGUISEPAGE_TESTMIC            1010     // to test that microphone is recording ok
#define VOICEDISGUISEPAGE_NOSOUNDCARD        1015     // no sound card
#define VOICEDISGUISEPAGE_RECORDNEXT         1020     // will be recording something
#define VOICEDISGUISEPAGE_RECORDSAMPLE       1030     // will be recording something
#define VOICEDISGUISEPAGE_RECORDSAMPLEPLAYBACK 1040   // play back what just recorded

#define VOICEDISGUISEPAGE_STARTDISGUISE      2000     // beginning of starting disguise

#define VOICEDISGUISEPAGE_STARTDISGUISEMIMIC 2005     // beginning of starting disguise, mimic
#define VOICEDISGUISEPAGE_LOADMIMICAUDIO     2010     // load mimic audio
#define VOICEDISGUISEPAGE_ENTERTRANSCRIPTION 2020     // have user type in transcription
#define VOICEDISGUISEPAGE_RECORDORIG         2030     // record the original voice
#define VOICEDISGUISEPAGE_CLEARSETTINGS      2040     // ask if user wishes to clear existing settings
#define VOICEDISGUISEPAGE_ADJUSTPITCH        2050     // adjust your pitch
#define VOICEDISGUISEPAGE_MOVEMARKERS        2060     // move time-slice markers around
#define VOICEDISGUISEPAGE_OCTAVESTRETCHES    2070     // stretch and scale the octaves
#define VOICEDISGUISEPAGE_FINETUNEPHONEME    2080     // review individual phonemes
#define VOICEDISGUISEPAGE_PHONEMEDATABASE    2090     // delete phonemes in database
#define VOICEDISGUISEPAGE_EXTRASETTINGS      2100     // extra settings
#define VOICEDISGUISEPAGE_SAVE               2110     // allow to save after disguise completed

#define VOICEDISGUISEPAGE_STARTDISGUISEQUICK 2300     // start of quick disguise
#define VOICEDISGUISEPAGE_RECORDORIGQUICK    2310     // record the original voice
#define VOICEDISGUISEPAGE_QUICKOPTIONS       2320     // options for quick

#define VOICEDISGUISEPAGE_NOOFFENSIVE        3000     // no offensive langauge
#define VOICEDISGUISEPAGE_VOICECHATTUTORIAL  3010     // voice chat tutorial

static PWSTR gpszFinished = L"Finished";


#define NUMWAVEMARKERS               8    // 12 markers to place

static COLORREF gaColorMarker[NUMWAVEMARKERS] = {
   RGB(0xff,0,0), RGB(0,0xff,0), RGB(0,0,0xff),
   RGB(0x80,0x80,0), RGB(0,0x80,0x80), RGB(0x80,0,0x80),
   RGB(0x80,0,0), RGB(0,0x80,0) // , RGB(0,0,0x80),
   // RGB(0x80,0x80,0), RGB(0,0x80,0x80), RGB(0x80,0,0x80)
   };


/****************************************************************************
OctaveNormalize - Accepts a DISGUISEINFO and figures out the scaling
to apply to each octave. This is necessary for interpolating octaves
so they all have the same weighting.

inputs
   PDISGUISEINFO     pDI - Octave info here
   DWORD             dwUnvoiced - 0 for voiced, 1 for unvoiced
returns
   fp - Scaling to multiply by each pDI->afOctaveBands[voiced][x].
*/
fp OctaveNormalize (PDISGUISEINFO pDI, DWORD dwUnvoiced)
{
   fp fSum = 0;
   DWORD i;
   for (i = 0; i < SROCTAVE; i++)
      fSum += pDI->afOctaveBands[dwUnvoiced][i];
   return fSum ? ((fp)SROCTAVE / fSum) : 1.0;
}


/****************************************************************************
VolumeNormalize - Accepts a DISGUISEINFO and figures out the scaling
to apply to each voiced/unvoiced volume array.
This is necessary to ensure that volume settings neigher increase nor
decrease the overall volume. (Or at least approximately)

inputs
   PDISGUISEINFO     pDI - Octave info here
   DWORD             dwUnvoiced - 0 if voiced, 1 if unvoiced
returns
   int - Number of dB to ADD to the existing values. Will normalize the scaling
*/
int VolumeNormalize (PDISGUISEINFO pDI, DWORD dwUnvoiced)
{
   fp fSum = 0, fGoal = DbToAmplitude (0);
   DWORD i;
   for (i = 0; i < SROCTAVE; i++)
      fSum += DbToAmplitude (pDI->abVolume[dwUnvoiced][i]);
   fSum /= (fp)SROCTAVE;
   fSum = fGoal / (fSum ? fSum : 1.0);
#ifdef _DEBUG
   if ((fSum < 0.99) || (fSum > 1.01))
      i++;
#endif
   return (int)AmplitudeToDb (fSum*fGoal);   // need to multiply by fGoal so comes out as 0
}

/****************************************************************************
VDStartWizardPage
*/
BOOL VDStartWizardPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Voice disguising";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFENTIREWIZARD")) {
            p->pszSubString = pv->m_dwFlags ? L"<comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFENTIREWIZARD")) {
            p->pszSubString = pv->m_dwFlags ? L"</comment>" : L"";
            return TRUE;
         }
      }
      break;



   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"save")) {
            if (!pv->Save (pPage->m_pWindow->m_hWnd))
               pPage->MBWarning (L"The file couldn't be saved.", L"You might wish to resave using a different name.");

            // stay in the same page
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"load")) {
            if (pv->ChangedQuery ()) {
               if (IDYES != pPage->MBYesNo (L"Are you sure you wish to load in new voice diguise settings?",
                  L"You current settings will be lost."))
                  return TRUE;
            }


            if (!pv->Load (pPage->m_pWindow->m_hWnd))
               pPage->MBWarning (L"The file couldn't be loaded.");

            // stay in the same page
            return TRUE;
         }
      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
VDTypesOfMicsPage
*/
BOOL VDTypesOfMicsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Types of microphones";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
VDNoSoundCardPage
*/
BOOL VDNoSoundCardPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"No sound card";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
VDRecordNextPage
*/
BOOL VDRecordNextPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Try recording something";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
VDRecordSamplePlaybackPage
*/
BOOL VDRecordSamplePlaybackPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"play")) {
            if (!pv->m_pWaveScratch)
               pPage->MBWarning (L"Recording missing.", L"The test recording seems to be missing. Press back to re-record.");
            else
               pv->m_pWaveScratch->QuickPlay ();

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Playback sample recording";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
VDStartDisguiseMimicPage
*/
BOOL VDStartDisguiseMimicPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Voice disguising, mimic";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
VDStartDisguisePage
*/
BOOL VDStartDisguisePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if ((p->psz[0] == L'p') && (p->psz[1] == L':')) {
            // verify that the player wants to procede
            if (pv->ChangedQuery ()) {
               if (IDYES != pPage->MBYesNo (L"Are you sure you wish to change your voice diguise settings?",
                  L"You have already made some voice diguise changes. "
                  L"You may wish to go back and save your voice disguise, since if you procede you may lose those changes."))
                  return TRUE;
            }
            break;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"nextnosave")) {
            // player pressed "no digsuie"
            if (pv->ChangedQuery ()) {
               if (IDYES != pPage->MBYesNo (L"Are you sure you wish to change your voice diguise settings?",
                  L"You have already made some voice diguise changes. "
                  L"You may wish to go back and save your voice disguise, since if you procede you may lose those changes."))
                  return TRUE;
            }

            pv->Clear();

            // else, next
            pPage->Exit (pv->m_dwFlags ? L"finished" : L"p:3000");
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Voice disguising";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}





/****************************************************************************
VDLoadMimicAudioPage
*/
BOOL VDLoadMimicAudioPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"skip")) {
            // make sure no voice
            if (pv->m_pWaveMimic)
               delete pv->m_pWaveMimic;
            pv->m_pWaveMimic = NULL;

            pPage->Exit (L"p:2020");
            return TRUE;
         }

         if (!_wcsicmp (psz, L"open")) {
            // open dialog...
            WCHAR szFile[256];
            szFile[0] = 0;
            SetLastDirectory (gszAppDir); // so opens in the MIF directory
            if (!WaveFileOpen (pPage->m_pWindow->m_hWnd, FALSE, szFile, NULL))
               return TRUE;   // cancelled

            // clear out previous one
            if (pv->m_pWaveMimic)
               delete pv->m_pWaveMimic;
            pv->m_pWaveMimic = new CM3DWave;
            if (!pv->m_pWaveMimic)
               return TRUE;   // shouldnt happen

            // try loading it
            char szaTemp[256];
            WideCharToMultiByte (CP_ACP, 0, szFile, -1, szaTemp, sizeof(szaTemp), 0, 0);
            if (!pv->m_pWaveMimic->Open(NULL, szaTemp)) {
               pPage->MBWarning (L"The wave file could not be opened.",
                  L"It may not have been a proper wave file.");
               return TRUE;
            }

            // sampling rate
            if (pv->m_pWaveMimic->m_dwSamplesPerSec < 22050) {
               if (IDYES != pPage->MBYesNo (L"The wave file is recorded at less than 22 kHz. Do you wish to use it?",
                  L"You can use it, but the higher frequencies of voiced and some parts of the 's' sound "
                  L"will be lost."))
                  return TRUE;
            }

            PWSTR pszSpoken = (PWSTR) (pv->m_pWaveMimic ? pv->m_pWaveMimic->m_memSpoken.p : NULL);
            if (pszSpoken) {
               MemZero (&pv->m_memSpoken);
               MemCat (&pv->m_memSpoken, pszSpoken);
            }
            pPage->Exit (L"p:2020");
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Select a voice to mimic";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
VDEnterTranscriptionPage
*/
BOOL VDEnterTranscriptionPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PWSTR pszSpoken = (PWSTR) pv->m_memSpoken.p;
         PCEscControl pControl = pPage->ControlFind (L"transcript");
         if (pszSpoken && pszSpoken[0] && pControl)
            pControl->AttribSet (Text(), pszSpoken);

         pControl = pPage->ControlFind (L"play");
         if (pControl)
            pControl->Enable (pv->m_pWaveMimic ? TRUE : FALSE);  // disable if no wave

         pControl = pPage->ControlFind (L"nextfile");
         if (pControl)
            pControl->Enable (pv->m_pTTS ? FALSE : TRUE);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"transcript")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pv->m_memSpoken.Required (dwNeed))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR) pv->m_memSpoken.p,
               (DWORD)pv->m_memSpoken.m_dwAllocated, &dwNeed);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"play")) {
            if (pv->m_pWaveMimic)
               pv->m_pWaveMimic->QuickPlay ();
            return TRUE;
         }

         if (!_wcsicmp (psz, L"next")) {
            // get the spoken text
            PWSTR pszSpoken = (PWSTR) pv->m_memSpoken.p;
            if (!pszSpoken || !pszSpoken[0]) {
               pPage->MBWarning (L"You need to type in the sentence that was spoken in the recording.");
               return TRUE;
            }

            // create a recording
            if (pv->m_pWaveOrig)
               delete pv->m_pWaveOrig;
            pv->m_pWaveOrig = new CM3DWave;
            if (!pv->m_pWaveOrig)
               return TRUE;   // shouldnt happen

            // make sure its 22khz mono
            pv->m_pWaveOrig->ConvertSamplesAndChannels (22050, 1, NULL);

            // set the text to be spoken
            MemZero (&pv->m_pWaveOrig->m_memSpoken);
            MemCat (&pv->m_pWaveOrig->m_memSpoken, pszSpoken);

            // if use TTS then convert now
            if (pv->m_pTTS) {
               CProgress Progress;
               Progress.Start (pPage->m_pWindow->m_hWnd, "Speaking", TRUE);
               if (!pv->m_pTTS->SynthGenWave (pv->m_pWaveOrig, pv->m_pWaveOrig->m_dwSamplesPerSec, pszSpoken, FALSE, 1 /*iTTSQuality*/, TRUE /* fDisablePCM */, &Progress))
                  return TRUE;   // error. shouldnt happen
            }

            // move on
            pPage->Exit (L"p:2030");

            return TRUE;
         }
         if (!_wcsicmp (psz, L"nextfile")) {
            // get the spoken text
            PWSTR pszSpoken = (PWSTR) pv->m_memSpoken.p;
            if (!pszSpoken || !pszSpoken[0]) {
               pPage->MBWarning (L"You need to type in the sentence that was spoken in the recording.");
               return TRUE;
            }

            // open dialog...
            WCHAR szFile[256];
            szFile[0] = 0;
            SetLastDirectory (gszAppDir); // so opens in the MIF directory
            if (!WaveFileOpen (pPage->m_pWindow->m_hWnd, FALSE, szFile, NULL))
               return TRUE;   // cancelled

            // clear out previous one
            if (pv->m_pWaveOrig)
               delete pv->m_pWaveOrig;
            pv->m_pWaveOrig = new CM3DWave;
            if (!pv->m_pWaveOrig)
               return TRUE;   // shouldnt happen

            // try loading it
            char szaTemp[256];
            WideCharToMultiByte (CP_ACP, 0, szFile, -1, szaTemp, sizeof(szaTemp), 0, 0);
            if (!pv->m_pWaveOrig->Open(NULL, szaTemp)) {
               pPage->MBWarning (L"The wave file could not be opened.",
                  L"It may not have been a proper wave file.");
               return TRUE;
            }

            // sampling rate
            if (pv->m_pWaveOrig->m_dwSamplesPerSec < 22050) {
               if (IDYES != pPage->MBYesNo (L"The wave file is recorded at less than 22 kHz. Do you wish to use it?",
                  L"You can use it, but the higher frequencies of voiced and some parts of the 's' sound "
                  L"will be lost."))
                  return TRUE;
            }

            // set the text to be spoken
            MemZero (&pv->m_pWaveOrig->m_memSpoken);
            MemCat (&pv->m_pWaveOrig->m_memSpoken, pszSpoken);

            // move on
            pPage->Exit (L"p:2040");

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Type in a transcription";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}





/****************************************************************************
VDStartDiguiseQuickPage
*/
BOOL VDStartDiguiseQuickPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PWSTR pszSpoken = (PWSTR) pv->m_memSpoken.p;
         PCEscControl pControl = pPage->ControlFind (L"transcript");
         if (pszSpoken && pszSpoken[0] && pControl)
            pControl->AttribSet (Text(), pszSpoken);

         pControl = pPage->ControlFind (L"nextfile");
         if (pControl)
            pControl->Enable (pv->m_pTTS ? FALSE : TRUE);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"transcript")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pv->m_memSpoken.Required (dwNeed))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR) pv->m_memSpoken.p,
               (DWORD)pv->m_memSpoken.m_dwAllocated, &dwNeed);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"next")) {
            // get the spoken text
            PWSTR pszSpoken = (PWSTR) pv->m_memSpoken.p;
            if (!pszSpoken || !pszSpoken[0]) {
               pPage->MBWarning (L"You need to type in the sentence that you will speak.");
               return TRUE;
            }

            // create a recording
            if (pv->m_pWaveOrig)
               delete pv->m_pWaveOrig;
            pv->m_pWaveOrig = new CM3DWave;
            if (!pv->m_pWaveOrig)
               return TRUE;   // shouldnt happen

            // make sure its 22khz mono
            pv->m_pWaveOrig->ConvertSamplesAndChannels (22050, 1, NULL);

            // set the text to be spoken
            MemZero (&pv->m_pWaveOrig->m_memSpoken);
            MemCat (&pv->m_pWaveOrig->m_memSpoken, pszSpoken);

            // if use TTS then convert now
            if (pv->m_pTTS) {
               CProgress Progress;
               Progress.Start (pPage->m_pWindow->m_hWnd, "Speaking", TRUE);
               if (!pv->m_pTTS->SynthGenWave (pv->m_pWaveOrig, pv->m_pWaveOrig->m_dwSamplesPerSec, pszSpoken, FALSE, 1 /*iTTSQuality*/, TRUE /* fDisablePCM */, &Progress))
                  return TRUE;   // error. shouldnt happen
            }

            // move on
            pPage->Exit (L"p:2310");

            return TRUE;
         }
         if (!_wcsicmp (psz, L"nextfile")) {
            // get the spoken text
            PWSTR pszSpoken = (PWSTR) pv->m_memSpoken.p;
            if (!pszSpoken || !pszSpoken[0]) {
               pPage->MBWarning (L"You need to type in the sentence that you will speak.");
               return TRUE;
            }

            // open dialog...
            WCHAR szFile[256];
            szFile[0] = 0;
            SetLastDirectory (gszAppDir); // so opens in the MIF directory
            if (!WaveFileOpen (pPage->m_pWindow->m_hWnd, FALSE, szFile, NULL))
               return TRUE;   // cancelled

            // clear out previous one
            if (pv->m_pWaveOrig)
               delete pv->m_pWaveOrig;
            pv->m_pWaveOrig = new CM3DWave;
            if (!pv->m_pWaveOrig)
               return TRUE;   // shouldnt happen

            // try loading it
            char szaTemp[256];
            WideCharToMultiByte (CP_ACP, 0, szFile, -1, szaTemp, sizeof(szaTemp), 0, 0);
            if (!pv->m_pWaveOrig->Open(NULL, szaTemp)) {
               pPage->MBWarning (L"The wave file could not be opened.",
                  L"It may not have been a proper wave file.");
               return TRUE;
            }

            // sampling rate
            if (pv->m_pWaveOrig->m_dwSamplesPerSec < 22050) {
               if (IDYES != pPage->MBYesNo (L"The wave file is recorded at less than 22 kHz. Do you wish to use it?",
                  L"You can use it, but the higher frequencies of voiced and some parts of the 's' sound "
                  L"will be lost."))
                  return TRUE;
            }

            // set the text to be spoken
            MemZero (&pv->m_pWaveOrig->m_memSpoken);
            MemCat (&pv->m_pWaveOrig->m_memSpoken, pszSpoken);

            // move on
            pPage->Exit (L"p:2320");

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Voice disguising, quick";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
VDRecordOrigPage
*/
BOOL VDRecordOrigPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Voice has been synthesized";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
VDClearSettingsPage
*/
BOOL VDClearSettingsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // always reset the markers here
         DWORD dwWave, i;
         for (dwWave = 0; dwWave < 2; dwWave++) {
            PCM3DWave pWave = dwWave ? pv->m_pWaveOrig : pv->m_pWaveMimic;
            double fSamples = pWave ? pWave->m_dwSamples : 1;
            PCListFixed pl = dwWave ? &pv->m_lWAVEVIEWMARKEROrig : &pv->m_lWAVEVIEWMARKERMimic;

            pl->Init (sizeof(WAVEVIEWMARKER));
            WAVEVIEWMARKER Mark;
            for (i = 0; i < NUMWAVEMARKERS; i++) {
               Mark.cColor = gaColorMarker[i];
               Mark.dwSample = (DWORD) (((double)i + 0.5) / (double)NUMWAVEMARKERS * fSamples);
               pl->Add (&Mark);
            };
         } // dwWave

         pv->m_lPHONEMEDISGUISETemp.Clear();
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"discard")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to discard your previous changes?",
               L"You might wish to save them to disk first."))
               return TRUE;

            // clear
            pv->Clear();

            // make sure to calculate pitches
            if (pv->m_pWaveOrig)
               pv->m_pWaveOrig->CalcSRFeaturesIfNeeded (WAVECALC_VOICECHAT, pPage->m_pWindow->m_hWnd);
            if (pv->m_pWaveMimic)
               pv->m_pWaveMimic->CalcSRFeaturesIfNeeded (WAVECALC_VOICECHAT, pPage->m_pWindow->m_hWnd);

            pPage->Exit (L"p:2050");
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      // just capture to calc features
      if (pv->m_pWaveOrig)
         pv->m_pWaveOrig->CalcSRFeaturesIfNeeded (WAVECALC_VOICECHAT, pPage->m_pWindow->m_hWnd);
      if (pv->m_pWaveMimic)
         pv->m_pWaveMimic->CalcSRFeaturesIfNeeded (WAVECALC_VOICECHAT, pPage->m_pWindow->m_hWnd);
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Keep your old settings?";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
VDDefPageWithWaves - Default page to call if have waves
*/
BOOL VDDefPageWithWaves (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      // enable/disable the play mimic
      PCEscControl pControl;
      if (pControl = pPage->ControlFind (L"playmimic"))
         pControl->Enable (pv->m_pWaveMimic ? TRUE : FALSE);

      // update all the displays
      pv->DialogSRFEATUREViewUpdate (pPage);

      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"playorig")) {
            // stop the others
            if (pv->m_pWaveMimic)
               pv->m_pWaveMimic->QuickPlayStop ();
            if (pv->m_pWaveScratch)
               pv->m_pWaveScratch->QuickPlayStop ();
            if (pv->m_pWaveOrig)
               pv->m_pWaveOrig->QuickPlayStop ();

            // play
            pv->m_pWaveOrig->QuickPlay ();
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"playmod")) {
            // stop the others
            if (pv->m_pWaveMimic)
               pv->m_pWaveMimic->QuickPlayStop ();
            if (pv->m_pWaveScratch)
               pv->m_pWaveScratch->QuickPlayStop ();
            if (pv->m_pWaveOrig)
               pv->m_pWaveOrig->QuickPlayStop ();

            // if can't find the srfeatureN control then dont include the temp entries
            PCEscControl pControl = pPage->ControlFind (L"srfeature0");

            // play
            pv->DialogPlaySample (pPage->m_pWindow->m_hWnd, pControl ? TRUE : FALSE);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"playmimic")) {
            // stop the others
            if (pv->m_pWaveMimic)
               pv->m_pWaveMimic->QuickPlayStop ();
            if (pv->m_pWaveScratch)
               pv->m_pWaveScratch->QuickPlayStop ();
            if (pv->m_pWaveOrig)
               pv->m_pWaveOrig->QuickPlayStop ();

            // play
            if (pv->m_pWaveMimic)
               pv->m_pWaveMimic->QuickPlay ();
            return TRUE;
         }
      }
      break;


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if ((p->psz[0] == L'p') && (p->psz[2] == L':')) {
            // trap this
            BOOL fBad = FALSE;
            BOOL fOrig = TRUE;
            BOOL fDisguise = FALSE;
            PCListFixed pl;
            switch (p->psz[1]) {
            case L'o':
               pl = &pv->m_lWAVEVIEWMARKEROrig;
               break;
            case L'd':
               fDisguise = TRUE;
               pl = &pv->m_lWAVEVIEWMARKEROrig;
               break;
            case L'm':
               fOrig = FALSE;
               pl = &pv->m_lWAVEVIEWMARKERMimic;

               // if there isn't any mimic wave then beep
               if (!pv->m_pWaveMimic) {
                  pPage->m_pWindow->Beep(ESCBEEP_DONTCLICK);
                  return TRUE;
               }
               break;
            default:
               fBad = TRUE;
               break; // dont want this
            } // swtich
            if (fBad)
               break;

            DWORD dwNum = (DWORD)_wtoi(p->psz + 3);


            // play a sample
            PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get (dwNum);
            if (!pPD)
               return TRUE; // error
            pv->DialogPlaySample (pPD, fOrig, fDisguise);

            //PWAVEVIEWMARKER pwvm = (PWAVEVIEWMARKER)pl->Get(dwNum);
            //PWAVEVIEWMARKER pwvm2 = (PWAVEVIEWMARKER)pv->m_lWAVEVIEWMARKERMimic.Get(dwNum);
            //pv->DialogPlaySample (pwvm->dwSample, fOrig, fDisguise, fDisguise ? pwvm2->dwSample : (DWORD)-1);

            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszPhoneName = L"phonename";
         DWORD dwPhoneNameLen = (DWORD)wcslen(pszPhoneName);
         if (!_wcsnicmp(p->pszSubName, pszPhoneName, dwPhoneNameLen)) {
            DWORD dwNum = (DWORD)_wtoi(p->pszSubName + dwPhoneNameLen);
            PPHONEMEDISGUISE ppd = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(dwNum);
            p->pszSubString = ppd->szName;
            return TRUE;
         }
      }
      break;

   } // switch

   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
VDAdjustPitchPage
*/
BOOL VDAdjustPitchPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // calculate recommended values
         fp fPitchOrig;
         if (pv->m_pTTS)
            fPitchOrig = pv->m_pTTS->AvgPitchGet ();
         else if (pv->m_pWaveOrig)
            fPitchOrig = pv->m_pWaveOrig->PitchOverRange (PITCH_F0, 0, pv->m_pWaveOrig->m_dwSamples, 0,
               NULL, NULL, NULL, FALSE);
         else
            fPitchOrig = 100;

         fp fPitchMimic;
         if (pv->m_pWaveMimic)
            fPitchMimic = pv->m_pWaveMimic->PitchOverRange (PITCH_F0, 0, pv->m_pWaveMimic->m_dwSamples, 0,
               NULL, NULL, NULL, FALSE);
         else
            fPitchMimic = fPitchOrig;

         fp fVarOrig;
         if (pv->m_pWaveOrig)
            fVarOrig = pv->CalcPitchVariation (pv->m_pWaveOrig, fPitchOrig);
         else
            fVarOrig = 1;

         fp fVarMimic;
         if (pv->m_pWaveMimic)
            fVarMimic = pv->CalcPitchVariation (pv->m_pWaveMimic, fPitchMimic);
         else
            fVarMimic = fVarOrig;

         if (!pv->m_fPitchOrig) {
            pv->m_fPitchOrig = fPitchOrig;
            pv->m_fPitchScale = fPitchMimic / fPitchOrig;
            pv->m_fPitchVariationScale = fVarOrig ? (fVarMimic / fVarOrig) : 1.0;
         }

         DoubleToControl (pPage, L"pitchorig", pv->m_fPitchOrig);
         DoubleToControl (pPage, L"pitchscale", pv->m_fPitchScale);
         DoubleToControl (pPage, L"pitchvariationscale", pv->m_fPitchVariationScale);

         PCEscControl pControl;
         if (pControl = pPage->ControlFind (L"pitchmusical"))
            pControl->AttribSetBOOL (Checked(), pv->m_fPitchMusical);

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // just get all the values
         pv->m_fPitchOrig = DoubleFromControl (pPage, L"pitchorig");
         pv->m_fPitchScale = DoubleFromControl (pPage, L"pitchscale");
         pv->m_fPitchVariationScale = DoubleFromControl (pPage, L"pitchvariationscale");
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"pitchmusical")) {
            pv->m_fPitchMusical = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Adjust pitch";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PITCHRECOMMEND")) {
            WCHAR szTemp[32];

            fp fPitchOrig;
            if (pv->m_pTTS)
               fPitchOrig = pv->m_pTTS->AvgPitchGet ();
            else if (pv->m_pWaveOrig)
               fPitchOrig = pv->m_pWaveOrig->PitchOverRange (PITCH_F0, 0, pv->m_pWaveOrig->m_dwSamples, 0,
                  NULL, NULL, NULL, FALSE);
            else
               fPitchOrig = 100;

            swprintf (szTemp, L"%.2f", (double)fPitchOrig);

            MemZero (&gMemTemp);
            MemCat (&gMemTemp ,szTemp);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"SCALERECOMMEND")) {
            WCHAR szTemp[32];

            fp fPitchOrig;
            if (pv->m_pTTS)
               fPitchOrig = pv->m_pTTS->AvgPitchGet ();
            else if (pv->m_pWaveOrig)
               fPitchOrig = pv->m_pWaveOrig->PitchOverRange (PITCH_F0, 0, pv->m_pWaveOrig->m_dwSamples, 0,
                  NULL, NULL, NULL, FALSE);
            else
               fPitchOrig = 100;

            fp fPitchMimic;
            if (pv->m_pWaveMimic)
               fPitchMimic = pv->m_pWaveMimic->PitchOverRange (PITCH_F0, 0, pv->m_pWaveMimic->m_dwSamples, 0,
                  NULL, NULL, NULL, FALSE);
            else
               fPitchMimic = fPitchOrig;

            swprintf (szTemp, L"%.2f", (double)fPitchMimic / (double)fPitchOrig);

            MemZero (&gMemTemp);
            MemCat (&gMemTemp ,szTemp);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"VARIATIONRECOMMEND")) {
            WCHAR szTemp[32];

            fp fPitchOrig;
            if (pv->m_pTTS)
               fPitchOrig = pv->m_pTTS->AvgPitchGet ();
            else if (pv->m_pWaveOrig)
               fPitchOrig = pv->m_pWaveOrig->PitchOverRange (PITCH_F0, 0, pv->m_pWaveOrig->m_dwSamples, 0,
                  NULL, NULL, NULL, FALSE);
            else
               fPitchOrig = 100;

            fp fVarOrig;
            if (pv->m_pWaveOrig)
               fVarOrig = pv->CalcPitchVariation (pv->m_pWaveOrig, fPitchOrig);
            else
               fVarOrig = 1;

            fp fVarMimic;
            if (pv->m_pWaveMimic)
               fVarMimic = pv->CalcPitchVariation (pv->m_pWaveMimic,
                  pv->m_pWaveMimic->PitchOverRange (PITCH_F0, 0, pv->m_pWaveMimic->m_dwSamples, 0,
                     NULL, NULL, NULL, FALSE));
            else
               fVarMimic = fVarOrig;

            swprintf (szTemp, L"%.2f", fVarOrig ? ((double)fVarMimic / (double)fVarOrig) : 1.0);

            MemZero (&gMemTemp);
            MemCat (&gMemTemp ,szTemp);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return VDDefPageWithWaves (pPage, dwMessage, pParam);
}


/****************************************************************************
VDQuickOptionsPage
*/
BOOL VDQuickOptionsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // calculate the srfeatures in the original
         if (pv->m_pWaveOrig)
            pv->m_pWaveOrig->CalcSRFeaturesIfNeeded (WAVECALC_VOICECHAT, pPage->m_pWindow->m_hWnd);

         // calculate recommended values
         fp fPitchOrig;
         if (pv->m_pTTS)
            fPitchOrig = pv->m_pTTS->AvgPitchGet ();
         else if (pv->m_pWaveOrig)
            fPitchOrig = pv->m_pWaveOrig->PitchOverRange (PITCH_F0, 0, pv->m_pWaveOrig->m_dwSamples, 0,
               NULL, NULL, NULL, FALSE);
         else
            fPitchOrig = 100;
         pv->m_fPitchOrig = fPitchOrig;

         // set the scrollbar
         fp fPitch = max(pv->m_fPitchScale, .1);
         fPitch = log(fPitch) / log((fp)2) * 100.0;
         fPitch = floor(fPitch);
         fPitch = max(fPitch, -200);
         fPitch = min(fPitch, 200);
         PCEscControl pControl = pPage->ControlFind (L"pitch");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)fPitch);

         // stretch
         fp fStretch = pv->m_DisguiseInfo.afOctaveBands[0][1];
            // NOTE - Stretch is only concerned with voiced
         fStretch = floor(fStretch * 100);
         fStretch = max(fStretch, 80);
         fStretch = min(fStretch, 120);
         pControl = pPage->ControlFind (L"stretch");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)fStretch);

         // volume delta
         int iDelta;
         DWORD dwNoise;
         WCHAR szTemp[64];
         for (dwNoise = 0; dwNoise < 2; dwNoise++) {
            iDelta = (int)pv->m_DisguiseInfo.abVolume[dwNoise][1] - (int)pv->m_DisguiseInfo.abVolume[dwNoise][0];
            iDelta = max(iDelta, -6);
            iDelta = min(iDelta, 6);
            swprintf (szTemp, L"volume%d", (int)dwNoise);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)iDelta);
         }
      }
      break;


   case ESCN_SCROLL:
   // case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"pitch")) {
            pv->m_fPitchScale = pow(2.0, (fp)p->iPos / 100.0);
            pPage->Message (ESCM_USER+430);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"stretch")) {
            fp fStretch = (fp)p->iPos / 100.0;
            pv->m_DisguiseInfo.afOctaveBands[0][0] = 1.0;
               // NOTE: Stretched is only doing voiced
            DWORD i;
            for (i = 1; i < SROCTAVE; i++)
               pv->m_DisguiseInfo.afOctaveBands[0][i] = pv->m_DisguiseInfo.afOctaveBands[0][i-1] * fStretch;
            for (i = 0; i < SROCTAVE; i++)
               pv->m_DisguiseInfo.afOctaveBands[1][i] = 1;  // no stretching unvoiced

            pPage->Message (ESCM_USER+430);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"volume0") || !_wcsicmp(psz, L"volume1")) {
            DWORD dwNoise = _wcsicmp(psz, L"volume0") ? 1 : 0;
            char cDelta = (char)p->iPos;

            pv->m_DisguiseInfo.abVolume[dwNoise][0] = -cDelta * (SROCTAVE/2);

            DWORD i;
            for (i = 1; i < SROCTAVE; i++)
               pv->m_DisguiseInfo.abVolume[dwNoise][i] =
                  pv->m_DisguiseInfo.abVolume[dwNoise][i-1] + cDelta;

            pPage->Message (ESCM_USER+430);
            return TRUE;
         }
      }
      break;

   case ESCM_USER+430:  // play
      // stop the others
      if (pv->m_pWaveMimic)
         pv->m_pWaveMimic->QuickPlayStop ();
      if (pv->m_pWaveScratch)
         pv->m_pWaveScratch->QuickPlayStop ();
      if (pv->m_pWaveOrig)
         pv->m_pWaveOrig->QuickPlayStop ();

      // play
      pv->DialogPlaySample (pPage->m_pWindow->m_hWnd, FALSE);
      return TRUE;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Voice disguise options, quick";
            return TRUE;
         }
      }
      break;
   };


   return VDDefPageWithWaves (pPage, dwMessage, pParam);
}

/****************************************************************************
VDMoveMarkersPage
*/
BOOL VDMoveMarkersPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // find the mimic wave and display that
         PCEscControl pControl;
         ESCMWAVEVIEW wv;
         ESCMWAVEVIEWMARKERSSET ms;
         memset (&wv, 0, sizeof(wv));
         memset (&ms, 0, sizeof(ms));
         if (pControl = pPage->ControlFind (L"mimic")) {
            wv.pWave = pv->m_pWaveMimic;
            ms.dwNum = pv->m_lWAVEVIEWMARKERMimic.Num();
            ms.paMarker = (PWAVEVIEWMARKER) pv->m_lWAVEVIEWMARKERMimic.Get(0);
            pControl->Message (ESCM_WAVEVIEW, &wv);
            pControl->Message (ESCM_WAVEVIEWMARKERSSET, &ms);
         }
         if (pControl = pPage->ControlFind (L"orig")) {
            wv.pWave = pv->m_pWaveOrig;
            ms.dwNum = pv->m_lWAVEVIEWMARKEROrig.Num();
            ms.paMarker = (PWAVEVIEWMARKER) pv->m_lWAVEVIEWMARKEROrig.Get(0);
            pControl->Message (ESCM_WAVEVIEW, &wv);
            pControl->Message (ESCM_WAVEVIEWMARKERSSET, &ms);
         }

         // set up the PHONEMEDISGUISE if not set up
         pv->m_lPHONEMEDISGUISETemp.Clear(); // clear out the current temporary phonemes
         while (pv->m_lPHONEMEDISGUISETemp.Num() < NUMWAVEMARKERS) {
            PHONEMEDISGUISE pd;
            DWORD dwNum = pv->m_lPHONEMEDISGUISETemp.Num();
            memset (&pd, 0, sizeof(pd));
            swprintf (pd.szName, L"New %d", (int)dwNum+1);
            PWAVEVIEWMARKER pwm = (PWAVEVIEWMARKER) pv->m_lWAVEVIEWMARKERMimic.Get(dwNum);

            fp fTemp = pd.fPitchMimic;
            pd.fMimic = pv->DialogSRFEATUREGet (pwm->dwSample, FALSE, FALSE, &fTemp, &pd.srfMimic);
            pd.fPitchMimic = fTemp;

            pwm = (PWAVEVIEWMARKER) pv->m_lWAVEVIEWMARKEROrig.Get(dwNum);
            fTemp = pd.fPitchOrig;
            pv->DialogSRFEATUREGet (pwm->dwSample, TRUE, FALSE, &fTemp, &pd.srfOrig);
            pd.fPitchOrig = fTemp;
            pd.DI = pv->m_DisguiseInfo;

            // add
            pv->m_lPHONEMEDISGUISETemp.Add (&pd);
         } // while

         // fill in the names
         WCHAR szTemp[64];
         PPHONEMEDISGUISE ppd = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(0);
         DWORD i;
         for (i = 0; i < NUMWAVEMARKERS; i++, ppd++) {
            swprintf (szTemp, L"phonename%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSet (Text(), ppd->szName);
         }
      }
      break;

   case ESCN_WAVEVIEWMARKERCHANGED:
      {
         PESCNWAVEVIEWMARKERCHANGED p = (PESCNWAVEVIEWMARKERCHANGED)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp (psz, L"mimic") || !_wcsicmp(psz, L"orig")) {
            PCM3DWave pWave;
            PCListFixed pl;
            if (!_wcsicmp (psz, L"mimic")) {
               pWave = pv->m_pWaveMimic;
               pl = &pv->m_lWAVEVIEWMARKERMimic;
            }
            else {
               pWave = pv->m_pWaveOrig;
               pl = &pv->m_lWAVEVIEWMARKEROrig;
            }

            PWAVEVIEWMARKER pMark = (PWAVEVIEWMARKER) pl->Get(p->dwNum);
            if (!pMark)
               return TRUE;
            pMark->dwSample = p->dwSample;

            // update the temporary entry
            PPHONEMEDISGUISE ppd = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(p->dwNum);
            BOOL fOrig = (pWave == pv->m_pWaveOrig);
            fp fTemp = fOrig ? ppd->fPitchOrig : ppd->fPitchMimic;
            pv->DialogSRFEATUREGet (pMark->dwSample, fOrig, FALSE,
               &fTemp,
               fOrig ? &ppd->srfOrig : &ppd->srfMimic);
            *(fOrig ? &ppd->fPitchOrig : &ppd->fPitchMimic) = fTemp;
            // NOTE: Dont need to fill in the ppd->fMimic flag because will still be valid/invalid

            // see if can get the phoneme
            WCHAR szPhone[16];
            if (pWave->PhonemeAtTime (pMark->dwSample, szPhone)) {
               // set the phoneme name since know it
               WCHAR szTemp[64];
               PCEscControl pControl;
               swprintf (szTemp, L"phonename%d", (int)p->dwNum);
               if (pControl = pPage->ControlFind (szTemp))
                  pControl->AttribSet (Text(), szPhone);

               PPHONEMEDISGUISE ppd = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(p->dwNum);
               wcscpy (ppd->szName, szPhone);
            }

            pv->DialogPlaySampleSnip (pMark->dwSample, (pWave == pv->m_pWaveOrig));

            // update UI of amplitudes
            pv->DialogSRFEATUREViewUpdate (pPage, p->dwNum);

            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszPhoneName = L"phonename";
         DWORD dwPhoneNameLen = (DWORD)wcslen(pszPhoneName);
         if (!_wcsnicmp(psz, pszPhoneName, dwPhoneNameLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwPhoneNameLen);
            PPHONEMEDISGUISE ppd = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(dwNum);
            DWORD dwNeed;
            p->pControl->AttribGet (Text(), ppd->szName, sizeof(ppd->szName), &dwNeed);
            return TRUE;
         }
      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"mimiccombo")) {
            int iVal = p->pszName ? _wtoi(p->pszName) : 0;
            PCEscControl pControl = pPage->ControlFind (L"mimic");
            int iCur = pControl ? pControl->AttribGetInt (L"display") : 0;
            if (iVal == iCur)
               break;   // no change

            if (pControl)
               pControl->AttribSetInt (L"display", iVal);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"origcombo")) {
            int iVal = p->pszName ? _wtoi(p->pszName) : 0;
            PCEscControl pControl = pPage->ControlFind (L"orig");
            int iCur = pControl ? pControl->AttribGetInt (L"display") : 0;
            if (iVal == iCur)
               break;   // no change

            if (pControl)
               pControl->AttribSetInt (L"display", iVal);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Move markers";
            return TRUE;
         }
      }
      break;
   };


   return VDDefPageWithWaves (pPage, dwMessage, pParam);
}




/****************************************************************************
VDOctaveStretchesPage
*/
BOOL VDOctaveStretchesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set the combobox
         if (pv->m_dwFineTunePhoneme != (DWORD)-1)
            ComboBoxSet (pPage, L"modnumber", pv->m_dwFineTunePhoneme);

         // set the checkbox
         PCEscControl pControl;
         if (pControl = pPage->ControlFind (L"scalevoiced"))
            pControl->AttribSetBOOL (Checked(), TRUE);

         // sliders
         pPage->Message (ESCM_USER+185);
         pPage->Message (ESCM_USER+186);

      }
      break;

   case ESCM_USER+185:  // scrollbars for non-volume
      {
         // which disguise info
         PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(pv->m_dwFineTunePhoneme);
         PDISGUISEINFO pDI = pPD ? &pPD->DI : &pv->m_DisguiseInfo;

         PCEscControl pControl;
         // do all the stretch sliders, and peaks and valleys
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < SROCTAVE; i++) {
            swprintf (szTemp, L"octaveband%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(100.0 - pDI->afOctaveBands[0][i] * 50.0));
                  // NOTE: Can only stretch voiced

            swprintf (szTemp, L"peak%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(200.0 - pDI->afEmphasizePeaks[0][i] * 100.0));
                  // NOTE: Can only emphasize voiced
         } // i
      }
      return TRUE;

   case ESCM_USER+186:  // scrollbars for volume
      {
         // which disguise info
         PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(pv->m_dwFineTunePhoneme);
         PDISGUISEINFO pDI = pPD ? &pPD->DI : &pv->m_DisguiseInfo;

         // this sets up the scrollbars for the volume based on
         // whether scalevoiced is set
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"scalevoiced");
         DWORD dwUnvoiced = pControl ? (pControl->AttribGetBOOL(Checked()) ? 0 : 1) : 0;

         // set
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < SROCTAVE; i++) {
            swprintf (szTemp, L"scale%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), -(int)pDI->abVolume[dwUnvoiced][i]);
         } // i
      }
      return TRUE;



   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"scalevoiced")) {
            pPage->Message (ESCM_USER+186);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"next")) {
            // add the phonemes to the permanent database from the temp one
            PPHONEMEDISGUISE pPDTemp = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(0);
            PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISE.Get(0);

            DWORD i, j;
            for (i = 0; i < pv->m_lPHONEMEDISGUISETemp.Num(); i++, pPDTemp++) {
               // make sure doesn't already exist
               for (j = 0; j < pv->m_lPHONEMEDISGUISE.Num(); j++)
                  if (!memcmp (pPDTemp, &pPD[j], sizeof(*pPDTemp)))
                     break;
               if (j < pv->m_lPHONEMEDISGUISE.Num())
                  continue;   // already there, so dont add

               // else, add
               pv->m_lPHONEMEDISGUISE.Add (pPDTemp);
               pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISE.Get(0);  // in case realloc
            } // i


            // go to the database
            pPage->Exit (L"p:2090");
            return TRUE;
         }
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;
         PWSTR psz;
         int iVal;
         fp fVal;
         psz = p->pControl->m_pszName;
         iVal = (int)p->pControl->AttribGetInt (Pos());
         fVal = (fp)(100 - iVal) / 50.0;

         // which disguise info
         PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(pv->m_dwFineTunePhoneme);
         PDISGUISEINFO pDI = pPD ? &pPD->DI : &pv->m_DisguiseInfo;

         PWSTR pszOctaveBand = L"octaveband", pszScale = L"scale", pszPeak = L"peak";
         DWORD dwOctaveBandLen = (DWORD)wcslen(pszOctaveBand), dwScaleLen = (DWORD)wcslen(pszScale),
            dwPeakLen = (DWORD)wcslen(pszPeak);

         if (!_wcsnicmp (psz, pszOctaveBand, dwOctaveBandLen)) {
            DWORD dwIndex = _wtoi (psz + dwOctaveBandLen);
            pDI->afOctaveBands[0][dwIndex] = fVal;
               // NOTE: Can only affect voiced

            if (pDI == &pv->m_DisguiseInfo) {
               // update all temporary disguises to the new value
               PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(0);
               DWORD i;
               for (i = 0; i < pv->m_lPHONEMEDISGUISETemp.Num(); i++, pPD++)
                  pPD->DI = *pDI;
            }

            // update all the sliders
            pv->DialogSRFEATUREViewUpdate (pPage, pv->m_dwFineTunePhoneme);

            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszPeak, dwPeakLen)) {
            DWORD dwIndex = _wtoi (psz + dwPeakLen);
            pDI->afEmphasizePeaks[0][dwIndex] = 2.0 - (fp)iVal / 100.0;
               // NOTE: Can only emphasize voiced

            if (pDI == &pv->m_DisguiseInfo) {
               // update all temporary disguises to the new value
               PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(0);
               DWORD i;
               for (i = 0; i < pv->m_lPHONEMEDISGUISETemp.Num(); i++, pPD++)
                  pPD->DI = *pDI;
            }


            // update all the sliders
            pv->DialogSRFEATUREViewUpdate (pPage, pv->m_dwFineTunePhoneme);

            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszScale, dwScaleLen)) {
            DWORD dwIndex = _wtoi (psz + dwScaleLen);

            PCEscControl pControl;
            pControl = pPage->ControlFind (L"scalevoiced");
            DWORD dwUnvoiced = pControl ? (pControl->AttribGetBOOL(Checked()) ? 0 : 1) : 0;

            pDI->abVolume[dwUnvoiced][dwIndex] = -(char)iVal;

            if (pDI == &pv->m_DisguiseInfo) {
               // update all temporary disguises to the new value
               PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(0);
               DWORD i;
               for (i = 0; i < pv->m_lPHONEMEDISGUISETemp.Num(); i++, pPD++)
                  pPD->DI = *pDI;
            }


            // update all the sliders
            pv->DialogSRFEATUREViewUpdate (pPage, pv->m_dwFineTunePhoneme);

            return TRUE;
         }
         else
            break;
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"modnumber")) {
            DWORD dwNum = p->pszName ? _wtoi (p->pszName) : 0;
            if (dwNum == pv->m_dwFineTunePhoneme)
               return TRUE;   // no change

            // change
            pv->m_dwFineTunePhoneme = dwNum;

            // update all the sliders
            pPage->Message (ESCM_USER+185);
            pPage->Message (ESCM_USER+186);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = (pv->m_dwFineTunePhoneme != (DWORD)-1) ? L"Fine-tune phonemes" : L"Reshaping the audio";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"COMBOELEM")) {
            MemZero (&gMemTemp);

            // all the temporary phonemes
            DWORD i;
            PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISETemp.Get(0);
            for (i = 0; i < pv->m_lPHONEMEDISGUISETemp.Num(); i++, pPD++) {
               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pPD->szName[0] ? pPD->szName : L"Unnamed");
               MemCat (&gMemTemp, L"</elem>");
            } // i
            

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return VDDefPageWithWaves (pPage, dwMessage, pParam);
}




/****************************************************************************
VDPhonemeDatabasePage
*/
BOOL VDPhonemeDatabasePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // loop through and set all the displays
         DWORD i;
         WCHAR szTemp[64];
         PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISE.Get(0);
         DWORD dwNum = pv->m_lPHONEMEDISGUISE.Num();
         ESCMSRFEATUREVIEW fv;
         memset (&fv, 0, sizeof(fv));
         for (i = 0; i < dwNum; i++, pPD++) {
            // fill i nthe structure
            fv.cColor = RGB(0x60, 0x60, 0x60);
            memcpy (&fv.afOctaveBands, pPD->DI.afOctaveBands, sizeof(fv.afOctaveBands));
            fv.fShowBlack = TRUE;
            fv.fShowWhite = pPD->fMimic;
            fv.srfWhite = pPD->srfMimic;
            pv->ModifySRFEATURE (&pPD->srfOrig, NULL, &fv.srfBlack, NULL, pPD->fPitchOrig, &pPD->DI, FALSE);

            // need to scale the mimic so same energy as orig
            if (fv.fShowWhite) {
               fp fEnergyOrig = SRFEATUREEnergy (FALSE, &pPD->srfOrig);
               fp fEnergyMimic = SRFEATUREEnergy (FALSE, &fv.srfWhite);
               fEnergyMimic = max(fEnergyMimic, 1);   // so no divide by 0
               SRFEATUREScale (&fv.srfWhite, fEnergyOrig / fEnergyMimic);
            }

            // finhd the control
            swprintf (szTemp, L"phoneme%d", (int)i);
            PCEscControl pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Message (ESCM_SRFEATUREVIEW, &fv);
         } // i
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if ((p->psz[0] == L'd') && (p->psz[1] == L'l') && (p->psz[2] == L':')) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to permanently delete this phoneme?"))
               return TRUE;

            DWORD dwNum = _wtoi (p->psz + 3);
            pv->m_lPHONEMEDISGUISE.Remove(dwNum);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Phoneme database";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PHONELIST")) {
            MemZero (&gMemTemp);

            // all the temporary phonemes
            DWORD i;
            PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) pv->m_lPHONEMEDISGUISE.Get(0);
            DWORD dwNum = pv->m_lPHONEMEDISGUISE.Num();

#define COLUMNS         3

            for (i = 0; i < pv->m_lPHONEMEDISGUISE.Num(); i++, pPD++) {
               if ((i % COLUMNS) == 0)
                  MemCat (&gMemTemp, L"<tr>");

               MemCat (&gMemTemp, L"<td><a href=dl:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pPD->szName[0] ? pPD->szName : L"Unnamed");
               MemCat (&gMemTemp, L"</a><br/><srfeatureview width=100% height=25% name=phoneme");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></td>");

               if ((i % COLUMNS) == (COLUMNS-1))
                  MemCat (&gMemTemp, L"</tr>");
            } // i

            // finish off columns
            while (i % COLUMNS) {
               MemCat (&gMemTemp, L"<td/>");

               if ((i % COLUMNS) == (COLUMNS-1))
                  MemCat (&gMemTemp, L"</tr>");
               i++;
            }
            

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return VDDefPageWithWaves (pPage, dwMessage, pParam);
}


/****************************************************************************
VDExtraSettingsPage
*/
BOOL VDExtraSettingsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // do all the voiced to noise sliders
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < SROCTAVE; i++) {
            swprintf (szTemp, L"convert%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(100.0 - pv->m_afVoicedToUnvoiced[i] * 100.0));
         } // i

         // overall volumes
         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"overall%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), -(int)pv->m_abOverallVolume[i]);
         } // i

         DoubleToControl (pPage, L"nonharmonic", pv->m_fNonIntHarmonics);
         DoubleToControl (pPage, L"wavepitch", pv->m_fWaveBasePitch);

         if (pControl = pPage->ControlFind (L"wavebase"))
            pControl->AttribSet (Text(), (PWSTR)pv->m_memWaveBase.p);

         // set the combobox
         DWORD dwWant = 0;
         for (i = 0; i < pv->m_dwNumWAVEBASECHOICE; i++)
            if (!_wcsicmp(pv->m_paWAVEBASECHOICE[i].szFile, (PWSTR)pv->m_memWaveBase.p)) {
               dwWant = i+1;
               break;
            }
         ComboBoxSet (pPage, L"wavebasecombo", dwWant);

      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"waveopen")) {
            // clear the cache
            if (pv->m_pWaveCache) {
               // since just cleared wavebase
               WaveCacheRelease (pv->m_pWaveCache);
               pv->m_pWaveCache = NULL;
            }

            // open dialog...
            WCHAR szFile[256];
            szFile[0] = 0;
            SetLastDirectory (gszAppDir); // so opens in the MIF directory
            MemZero (&pv->m_memWaveBase);
            if (WaveFileOpen (pPage->m_pWindow->m_hWnd, FALSE, szFile, NULL))
               MemCat (&pv->m_memWaveBase, szFile);
            // else, will be 0

            PCEscControl pControl;
            if (pControl = pPage->ControlFind (L"wavebase"))
               pControl->AttribSet (Text(), (PWSTR)pv->m_memWaveBase.p);

            return TRUE;
         }
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;
         PWSTR psz;
         int iVal;
         fp fVal;
         psz = p->pControl->m_pszName;
         iVal = (int)p->pControl->AttribGetInt (Pos());
         fVal = (fp)(100 - iVal) / 100.0;

         // which disguise info
         PWSTR pszConvert = L"convert", pszOverAll = L"overall";
         DWORD dwConvertLen = (DWORD)wcslen(pszConvert), dwOverAllLen = (DWORD)wcslen(pszOverAll);

         if (!_wcsnicmp (psz, pszConvert, dwConvertLen)) {
            DWORD dwIndex = _wtoi (psz + dwConvertLen);
            pv->m_afVoicedToUnvoiced[dwIndex] = fVal;

            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszOverAll, dwOverAllLen)) {
            DWORD dwIndex = _wtoi (psz + dwOverAllLen);

            pv->m_abOverallVolume[dwIndex] = -(char)iVal;
            return TRUE;
         }
         else
            break;
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"nonharmonic")) {
            pv->m_fNonIntHarmonics = DoubleFromControl (pPage, psz);
            pv->m_fNonIntHarmonics = max(pv->m_fNonIntHarmonics, CLOSE);
            return TRUE;
         }
         if (!_wcsicmp(psz, L"wavepitch")) {
            pv->m_fWaveBasePitch = DoubleFromControl (pPage, psz);
            pv->m_fWaveBasePitch = max(pv->m_fWaveBasePitch, 0);
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"wavebasecombo")) {
            DWORD dwNum = p->pszName ? _wtoi (p->pszName) : 0;

            // find current
            DWORD dwWant = 0;
            DWORD i;
            for (i = 0; i < pv->m_dwNumWAVEBASECHOICE; i++)
               if (!_wcsicmp(pv->m_paWAVEBASECHOICE[i].szFile, (PWSTR)pv->m_memWaveBase.p)) {
                  dwWant = i+1;
                  break;
               }


            if (dwNum == dwWant)
               return TRUE;   // no change

            // clear the cache
            if (pv->m_pWaveCache) {
               // since just cleared wavebase
               WaveCacheRelease (pv->m_pWaveCache);
               pv->m_pWaveCache = NULL;
            }

            // open dialog...
            MemZero (&pv->m_memWaveBase);
            if (dwNum) {
               MemCat (&pv->m_memWaveBase, pv->m_paWAVEBASECHOICE[dwNum-1].szFile);
               pv->m_fWaveBasePitch = pv->m_paWAVEBASECHOICE[dwNum-1].fPitch;
            }


            PCEscControl pControl;
            if (pControl = pPage->ControlFind (L"wavebase"))
               pControl->AttribSet (Text(), (PWSTR)pv->m_memWaveBase.p);

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Extra settings";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFBASEVOICE")) {
            p->pszSubString = (pv->m_dwNumWAVEBASECHOICE || !pv->m_paWAVEBASECHOICE) ?
               L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFBASEVOICE")) {
            p->pszSubString = (pv->m_dwNumWAVEBASECHOICE || !pv->m_paWAVEBASECHOICE) ?
               L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFANYFILE")) {
            p->pszSubString = (!pv->m_paWAVEBASECHOICE) ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFANYFILE")) {
            p->pszSubString = (!pv->m_paWAVEBASECHOICE) ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFCOMBO")) {
            p->pszSubString = pv->m_paWAVEBASECHOICE ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFCOMBO")) {
            p->pszSubString = pv->m_paWAVEBASECHOICE ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"COMBOELEM")) {
            MemZero (&gMemTemp);

            // all the temporary phonemes
            DWORD i;
            for (i = 0; i < pv->m_dwNumWAVEBASECHOICE; i++) {
               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int)i+1);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pv->m_paWAVEBASECHOICE[i].szName);
               MemCat (&gMemTemp, L"</elem>");
            } // i
            

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

      }
      break;
   };


   return VDDefPageWithWaves (pPage, dwMessage, pParam);
}





/****************************************************************************
VDSavePage
*/
BOOL VDSavePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"save") || !_wcsicmp(psz, L"nextnosave")) {
            if (!_wcsicmp (psz, L"save")) {
               if (!pv->Save (pPage->m_pWindow->m_hWnd)) {
                  pPage->MBWarning (L"The file couldn't be saved.", L"You might wish to resave using a different name.");
                  return TRUE;
               }
            }

            // else, next
            pPage->Exit (pv->m_dwFlags ? L"finished" : L"p:3000");
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Save settings";
            return TRUE;
         }
      }
      break;
   };


   return VDDefPageWithWaves (pPage, dwMessage, pParam);
}

/****************************************************************************
VDNoOffensivePage
*/
BOOL VDNoOffensivePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"No offensive language";
            return TRUE;
         }
      }
      break;
   };


   return VDDefPageWithWaves (pPage, dwMessage, pParam);
}



/****************************************************************************
VDVoiceChatTutorialPage
*/
BOOL VDVoiceChatTutorialPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Voice chat tutorial";
            return TRUE;
         }
      }
      break;
   };


   return VDDefPageWithWaves (pPage, dwMessage, pParam);
}


/*************************************************************************************
CVoiceDisguise::Constructor
*/
CVoiceDisguise::CVoiceDisguise (void)
{
   m_lPageHistory.Init (sizeof(DWORD));
   m_pWaveScratch = NULL;
   m_pWaveMimic = NULL;
   m_pWaveOrig = NULL;
   m_pWaveCache = NULL;
   MemZero (&m_memSourceTTSFile);
   m_fAgreeNoOffensive = FALSE;
   m_dwFlags = 0;
   m_pWaveTest = m_pWaveTestOriginal = NULL;

   Clear();
}

/*************************************************************************************
CVoiceDisguise::Destructor
*/
CVoiceDisguise::~CVoiceDisguise (void)
{
   if (m_pWaveScratch)
      delete m_pWaveScratch;
   if (m_pWaveMimic)
      delete m_pWaveMimic;
   if (m_pWaveOrig)
      delete m_pWaveOrig;
   if (m_pWaveCache)
      WaveCacheRelease (m_pWaveCache);
   if (m_pWaveTest)
      delete m_pWaveTest;
}


/*************************************************************************************
CVoiceDisguise::Dialog - Displays a voice disguise dialog.

inputs
   PCEscWindow          pWindow - Window to use
   PCMTTS               pTTS - If this is not NULL, then its the TTS whose voice
                        is to be disguised.
   DWORD                dwNumWAVEBASECHOICE - Number of choices in paWAVEBASECHOICE.
   PWAVEBASECHOICE      paWAVEBASECHOICE - List of choices for waves the user can select.
                        If this is NULL, the user has access to a file open dialog.
                        If non-NULL, a list of these choices is presented. If non-NULL and
                        dwNumWAVEBASECHOICE==0 then the list won't be shown.
   DWORD                dwFlags - Flags
                              0x00 - Wizard for mic setup
                              0x01 - Setup for voice modification in wave or TTS
returns
   BOOL - TRUE if the user pressed back, FALSE if closed
*/
BOOL CVoiceDisguise::Dialog (PCEscWindow pWindow, PCMTTS pTTS,
                             DWORD dwNumWAVEBASECHOICE, PWAVEBASECHOICE paWAVEBASECHOICE,
                             DWORD dwFlags)
{
   m_pWindow = pWindow;
   m_pTTS = pTTS;
   m_dwNumWAVEBASECHOICE = dwNumWAVEBASECHOICE;
   m_paWAVEBASECHOICE = paWAVEBASECHOICE;
   m_dwFlags = dwFlags;
   BOOL fRet = TRUE;

   WCHAR szLink[256];
   PWSTR pszRet;
   DWORD dwPage = VOICEDISGUISEPAGE_STARTWIZARD;

   m_lPageHistory.Init (sizeof(DWORD), &dwPage, 1);

   while (m_lPageHistory.Num()) {
      // pull off the last page
      dwPage = *((DWORD*)m_lPageHistory.Get(m_lPageHistory.Num()-1));

      PESCPAGECALLBACK pCallback = NULL;
      DWORD dwResource = 0;
      switch (dwPage) {
      case VOICEDISGUISEPAGE_STARTWIZARD:
         pCallback = VDStartWizardPage;
         dwResource = IDR_MMLVDSTARTWIZARD;
         break;

      case VOICEDISGUISEPAGE_TYPESOFMICS:
         pCallback = VDTypesOfMicsPage;
         dwResource = IDR_MMLVDTYPESOFMICS;
         break;

      case VOICEDISGUISEPAGE_TESTMIC:
         // special code to just display the test mic page
         {
            CM3DWave Wave;

            // want 22 khz mono
            Wave.ConvertSamplesAndChannels (22050, 1, NULL);

            PCM3DWave pRet = Wave.RecordAdvanced (m_pWindow, IDR_MMLTESTMIC, ghInstance, NULL, szLink);
            if (pRet)
               delete pRet;   // shouldnt happen

            // deal with no microphone... will need to wipe off this back from the back
            if (!_wcsicmp(szLink, L"NoSoundCard")) {
               // remove the last page (this one)
               m_lPageHistory.Remove (m_lPageHistory.Num()-1);

               swprintf (szLink, L"p:%d", (int) VOICEDISGUISEPAGE_NOSOUNDCARD);
            }

            pszRet = szLink;
            goto diddialogalready;
         }
         break;

      case VOICEDISGUISEPAGE_NOSOUNDCARD:
         pCallback = VDNoSoundCardPage;
         dwResource = IDR_MMLVDNOSOUNDCARD;
         break;

      case VOICEDISGUISEPAGE_RECORDNEXT:
         pCallback = VDRecordNextPage;
         dwResource = IDR_MMLVDRECORDNEXT;
         break;

      case VOICEDISGUISEPAGE_RECORDSAMPLE:
         // special code to just display the test mic page
         {
            if (m_pWaveScratch)
               delete m_pWaveScratch;
            m_pWaveScratch = NULL;
            m_pWaveScratch = new CM3DWave;
            if (!m_pWaveScratch)
               break;   // shouldnt happen

            // want 22 khz mono
            m_pWaveScratch->ConvertSamplesAndChannels (22050, 1, NULL);

            PCM3DWave pRet = m_pWaveScratch->RecordAdvanced (m_pWindow, NULL, NULL, NULL, szLink);
            delete m_pWaveScratch;
            m_pWaveScratch = pRet;

            if (!m_pWaveScratch)
               pszRet = Back();
            else {
               swprintf (szLink, L"p:%d", (int)VOICEDISGUISEPAGE_RECORDSAMPLEPLAYBACK);
               pszRet = szLink;
            }
            goto diddialogalready;
         }
         break;

      case VOICEDISGUISEPAGE_RECORDSAMPLEPLAYBACK:
         pCallback = VDRecordSamplePlaybackPage;
         dwResource = IDR_MMLVDRECORDSAMPLEPLAYBACK;
         break;

      case VOICEDISGUISEPAGE_STARTDISGUISEMIMIC:
         pCallback = VDStartDisguiseMimicPage;
         dwResource = IDR_MMLVDSTARTDISGUISEMIMIC;
         break;

      case VOICEDISGUISEPAGE_STARTDISGUISE:
         pCallback = VDStartDisguisePage;
         dwResource = IDR_MMLVDSTARTDISGUISE;
         break;

      case VOICEDISGUISEPAGE_LOADMIMICAUDIO:
         pCallback = VDLoadMimicAudioPage;
         dwResource = IDR_MMLVDLOADMIMICAUDIO;
         break;

      case VOICEDISGUISEPAGE_ENTERTRANSCRIPTION:
         pCallback = VDEnterTranscriptionPage;
         dwResource = IDR_MMLVDENTERTRANSCRIPTION;
         break;

      case VOICEDISGUISEPAGE_STARTDISGUISEQUICK:
         pCallback = VDStartDiguiseQuickPage;
         dwResource = IDR_MMLVDSTARTDISGUISEQUICK;
         break;

      case VOICEDISGUISEPAGE_QUICKOPTIONS:
         pCallback = VDQuickOptionsPage;
         dwResource = IDR_MMLQUICKOPTIONS;
         break;

      case VOICEDISGUISEPAGE_RECORDORIG:
      case VOICEDISGUISEPAGE_RECORDORIGQUICK:
         if (m_pTTS) {
            pCallback = VDRecordOrigPage;
            dwResource = (dwPage == VOICEDISGUISEPAGE_RECORDORIG) ? IDR_MMLVDRECORDORIG : IDR_MMLVDRECORDORIGQUICK;
         }
         else {
            PCM3DWave pRet = m_pWaveOrig->RecordAdvanced (m_pWindow, NULL, NULL, NULL, szLink);
            delete m_pWaveOrig;
            m_pWaveOrig = pRet;

            // set the text to speak
            if (m_pWaveOrig) {
               MemZero (&m_pWaveOrig->m_memSpoken);
               MemCat (&m_pWaveOrig->m_memSpoken, (PWSTR)m_memSpoken.p);
            }

            if (!m_pWaveOrig)
               pszRet = Back();
            else {
               // set the text to speak
               MemZero (&m_pWaveOrig->m_memSpoken);
               MemCat (&m_pWaveOrig->m_memSpoken, (PWSTR)m_memSpoken.p);

               swprintf (szLink, L"p:%d", (int)((dwPage == VOICEDISGUISEPAGE_RECORDORIG) ?
                  VOICEDISGUISEPAGE_CLEARSETTINGS : VOICEDISGUISEPAGE_QUICKOPTIONS));
               pszRet = szLink;
            }
            goto diddialogalready;
         }
         break;

      case VOICEDISGUISEPAGE_CLEARSETTINGS:
         pCallback = VDClearSettingsPage;
         dwResource = ChangedQuery() ? IDR_MMLVDCLEARSETTINGS : IDR_MMLVDCLEARSETTINGSNOT;
         break;

      case VOICEDISGUISEPAGE_ADJUSTPITCH:
         pCallback = VDAdjustPitchPage;
         dwResource = IDR_MMLVDADJUSTPITCH;
         break;

      case VOICEDISGUISEPAGE_MOVEMARKERS:
         pCallback = VDMoveMarkersPage;
         dwResource = IDR_MMLVDMOVEMARKERS;
         break;

      case VOICEDISGUISEPAGE_OCTAVESTRETCHES:
         m_dwFineTunePhoneme = (DWORD)-1;
         pCallback = VDOctaveStretchesPage;
         dwResource = IDR_MMLVDOCTAVESTRETCHES;
         break;

      case VOICEDISGUISEPAGE_FINETUNEPHONEME:
         m_dwFineTunePhoneme = 0;
         pCallback = VDOctaveStretchesPage;  // same page code as octave stretches
         dwResource = IDR_MMLVDFINETUNEPHONEME;
         break;

      case VOICEDISGUISEPAGE_PHONEMEDATABASE:
         pCallback = VDPhonemeDatabasePage;
         dwResource = IDR_MMLVDPHONEMEDATABASE;
         break;

      case VOICEDISGUISEPAGE_EXTRASETTINGS:
         pCallback = VDExtraSettingsPage;
         dwResource = IDR_MMLVDEXTRASETTINGS;
         break;

      case VOICEDISGUISEPAGE_SAVE:
         pCallback = VDSavePage;
         dwResource = IDR_MMLVDSAVE;
         break;

      case VOICEDISGUISEPAGE_NOOFFENSIVE:
         pCallback = VDNoOffensivePage;
         dwResource = IDR_MMLVDNOOFFENSIVE;
         break;

      case VOICEDISGUISEPAGE_VOICECHATTUTORIAL:
         m_fAgreeNoOffensive = TRUE;
         pCallback = VDVoiceChatTutorialPage;
         dwResource = IDR_MMLVDVOICECHATTUTORIAL;
         break;

      } // switch dwPage

      if (!pCallback || !dwResource) {
         fRet = FALSE;  // bad page
         break;
      }

      // call it
      pszRet = m_pWindow->PageDialog (ghInstance, dwResource, pCallback, this);

diddialogalready:
      // if error then exit
      if (!pszRet) {
         fRet = FALSE;
         break;
      }

      // compare to "p:"
      if ((pszRet[0] == L'p') && (pszRet[1] == L':')) {
         dwPage = _wtoi (pszRet + 2);
         m_lPageHistory.Add (&dwPage);
         continue;
      }

      // back
      if (!_wcsicmp (pszRet, Back())) {
         // remove the last page
         m_lPageHistory.Remove (m_lPageHistory.Num()-1);
         continue;
      }

      // finishd
      if (!_wcsicmp (pszRet, gpszFinished))
         break;

      // redo same page
      if (!_wcsicmp (pszRet, RedoSamePage()))
         continue;   // will repeat
      
      // else, dont understand
      fRet = FALSE;
      break;
   } // while have pages

   return fRet;   // pressed back or exit
}


/*************************************************************************************
CVoiceDisguise::Clear - Clears all the settings to their default values.
*/
void CVoiceDisguise::Clear (void)
{
   m_fPitchOrig = 0;
   m_fPitchScale = 1.0;
   m_fPitchVariationScale = 1.0;
   m_fPitchMusical = FALSE;

   DWORD i, j;
   for (j = 0; j < 2; j++) for (i = 0; i < SROCTAVE; i++)
      m_DisguiseInfo.afOctaveBands[j][i] = m_DisguiseInfo.afEmphasizePeaks[j][i] = 1.0;
   memset (m_DisguiseInfo.abVolume, 0, sizeof(m_DisguiseInfo.abVolume));

   m_lPHONEMEDISGUISE.Init (sizeof(PHONEMEDISGUISE));
   m_lPHONEMEDISGUISETemp.Init (sizeof(PHONEMEDISGUISE));

   memset (m_afVoicedToUnvoiced, 0, sizeof(m_afVoicedToUnvoiced));   // 0.0 voiced to unvoiced
   memset (m_abOverallVolume, 0, sizeof(m_abOverallVolume));   // 0 db
   m_fNonIntHarmonics = 1;
   m_fWaveBasePitch = 0;
   MemZero (&m_memWaveBase);  // no string
   if (m_pWaveCache) {
      // since just freed m_memWaveBase
      WaveCacheRelease (m_pWaveCache);
      m_pWaveCache = NULL;
   }

   m_lMixPCVoiceDisguise.Init (sizeof(PCVoiceDisguise));
   m_lMixWeight.Init (sizeof(fp));
}


/*************************************************************************************
CVoiceDisguise::CloneTo - Copies the voice settings to a new object

inputs
   PCVoiceDisguise      pTo - Copy to
returns
   BOOL - TRUE if success
*/
BOOL CVoiceDisguise::CloneTo (CVoiceDisguise *pTo)
{
   pTo->m_fPitchOrig = m_fPitchOrig;
   pTo->m_fPitchScale = m_fPitchScale;
   pTo->m_fPitchVariationScale = m_fPitchVariationScale;
   pTo->m_fPitchMusical = m_fPitchMusical;

   pTo->m_DisguiseInfo = m_DisguiseInfo;

   pTo->m_lPHONEMEDISGUISE.Init (sizeof(PHONEMEDISGUISE), m_lPHONEMEDISGUISE.Get(0),
      m_lPHONEMEDISGUISE.Num());
   pTo->m_lPHONEMEDISGUISETemp.Init (sizeof(PHONEMEDISGUISE), m_lPHONEMEDISGUISETemp.Get(0),
      m_lPHONEMEDISGUISETemp.Num());

   memcpy (pTo->m_afVoicedToUnvoiced, m_afVoicedToUnvoiced, sizeof(m_afVoicedToUnvoiced));
   memcpy (pTo->m_abOverallVolume, m_abOverallVolume, sizeof(m_abOverallVolume));
   pTo->m_fNonIntHarmonics = m_fNonIntHarmonics;
   MemZero (&pTo->m_memWaveBase);
   MemCat (&pTo->m_memWaveBase, (PWSTR)m_memWaveBase.p);
   pTo->m_fWaveBasePitch = m_fWaveBasePitch;

   // copy the mixes
   pTo->m_lMixPCVoiceDisguise.Init (sizeof(PCVoiceDisguise), m_lMixPCVoiceDisguise.Get(0), m_lMixPCVoiceDisguise.Num());
   pTo->m_lMixPCVoiceDisguise.Init (sizeof(fp), m_lMixWeight.Get(0), m_lMixWeight.Num());

   return TRUE;
}




/*************************************************************************************
CVoiceDisguise::ChangedQuery - Returns TRUE if the object's settings have changed
from their original values.

returns
   BOOL - TRUE if they've changed, FALSE if not.
*/
BOOL CVoiceDisguise::ChangedQuery (void)
{
   CVoiceDisguise Compare; // blank voice disguise object to compare to

   if (Compare.m_fPitchOrig != m_fPitchOrig)
      return TRUE;
   if (Compare.m_fPitchScale != m_fPitchScale)
      return TRUE;
   if (Compare.m_fPitchVariationScale != m_fPitchVariationScale)
      return TRUE;
   if (Compare.m_fPitchMusical != m_fPitchMusical)
      return TRUE;

   DWORD i, j;
   for (j = 0; j < 2; j++) for (i = 0; i < SROCTAVE; i++) {
      if (Compare.m_DisguiseInfo.afOctaveBands[j][i] != m_DisguiseInfo.afOctaveBands[j][i])
         return TRUE;
      if (Compare.m_DisguiseInfo.afEmphasizePeaks[j][i] != m_DisguiseInfo.afEmphasizePeaks[j][i])
         return TRUE;
      if (Compare.m_afVoicedToUnvoiced[i] != m_afVoicedToUnvoiced[i])
         return TRUE;
   }
   if (memcmp (Compare.m_DisguiseInfo.abVolume, m_DisguiseInfo.abVolume, sizeof(m_DisguiseInfo.abVolume)))
      return TRUE;

   // compare phoneme disguises
   if (m_lPHONEMEDISGUISE.Num())
      return TRUE;

   if (memcmp (Compare.m_abOverallVolume, m_abOverallVolume, sizeof(m_abOverallVolume)))
      return TRUE;

   if (Compare.m_fNonIntHarmonics != m_fNonIntHarmonics)
      return TRUE;
   if (Compare.m_fWaveBasePitch != m_fWaveBasePitch)
      return TRUE;

   if (_wcsicmp ((PWSTR)Compare.m_memWaveBase.p, (PWSTR)m_memWaveBase.p))
      return TRUE;

   // NOTE: ignoring mixes

   return FALSE;
}


/*************************************************************************************
CVoiceDisguise::Clone - Clones the voice disguise object

returns
   PCVoiceDisguise - New object
*/
CVoiceDisguise *CVoiceDisguise::Clone (void)
{
   PCVoiceDisguise pNew = new CVoiceDisguise;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


/*************************************************************************************
CVoiceDisguise::ModifySRFEATURE - Modifies an individual SRFEATURE according to
the current voice disguise mods.

inputs
   PSRFEATURE        pOrig - Original feture
   PSRDETAILEDPHASE  pSDPOrig - Original detailed phase
   PSRFEATURE        pMod - Filled in with the modified SRFEATURE. pMod != pOrig
   PSRDETAILEDPHASE  pSDPMod -Filled in with modified detailed phase, pSDPOrig != pSDPMod
   fp                fPitchOrig - Original pitch
   PDISGUISEINFO     pDI - Disguise info to use. If NULL then use m_DisguiseInfo
   BOOL              fIncludeExtra - If TRUE include extra modifications (like
                     converting voiced energy to noise.) If FALSE dont
returns
   fp - Modified pitch
*/
fp CVoiceDisguise::ModifySRFEATURE (PSRFEATURE pOrig, PSRDETAILEDPHASE pSDPOrig,
                                    PSRFEATURE pMod, PSRDETAILEDPHASE pSDPMod, fp fPitchOrig,
                                    PDISGUISEINFO pDI, BOOL fIncludeExtra)
{
   if (!pDI)
      pDI = &m_DisguiseInfo;

   // copy over detailed phase now because don't modify the harmonic info every
   if (pSDPMod && pSDPOrig)
      *pSDPMod = *pSDPOrig;

   // if there isn't any real change then trivial return, saving CPU
   DWORD dwUnvoiced, i, j;
   for (j = 0; j < 2; j++) {
      for (i = 0; i < SROCTAVE; i++) {
         if (fabs(pDI->afOctaveBands[j][i] - 1.0) > CLOSE)
            break;
         if (fabs(pDI->afEmphasizePeaks[j][i] - 1.0) > CLOSE)
            break;
         if ((pDI->abVolume[j][i] < -1) || (pDI->abVolume[j][i] > 1))
            break;
         if (!j && fIncludeExtra && (fabs(m_afVoicedToUnvoiced[i]) > CLOSE))	// BUGFIX - was fabs(m_afVoicedToUnvoiced[i] > CLOSE)
            break;
      } // i
      if (i < SROCTAVE)
         break;

      if ((m_abOverallVolume[j] < -1) || (m_abOverallVolume[j] > 1))
         break;
   } // j
   if (j >= 2) {
      *pMod = *pOrig;
      // already copied over: *pSDPMod = *pSDPOrig;
      goto done;
   }

   // copy original over to a scratch location
   SRFEATURE srf;
   srf = *pOrig;

   // create a smoothed out version so can emphasize peaks and valleys
   int iOffset;
   SRFEATURE srfSmooth, srfRoughish;
      // NOTE: Do roughish so don't make huge spikes in frequency response
#define WINDOWSIZE         SROCTAVE
#define SMALLWINDOWSIZE    SROCTAVE/3
#define SUPERSAMPLE        PHASEDETAILED        // amount to stretch out

   // Assume SUPERSAMPLE == PHASEDETAILED for phase stretch
   _ASSERTE (SUPERSAMPLE ==PHASEDETAILED);

   fp    afEnergy[2][SRDATAPOINTS*SUPERSAMPLE];
   for (i = 0; i < SRDATAPOINTS; i++) {
      afEnergy[0][i] = DbToAmplitude (srf.acVoiceEnergy[i]);
      afEnergy[1][i] = DbToAmplitude (srf.acNoiseEnergy[i]);
   } // i
   for (j = 0; j < 2; j++) {
      DWORD dwWindow = j ? WINDOWSIZE : SMALLWINDOWSIZE;
      PSRFEATURE psrf = j ? &srfSmooth : &srfRoughish;
      for (i = 0; i < SRDATAPOINTS; i++) {
         // blend in
         fp fSumVoiced = 0, fSumNoise = 0;
         fp fCount = 0;
         for (iOffset = -(int)dwWindow; iOffset <= (int)dwWindow; iOffset++) {
            int iCur = iOffset + (int)i;
            if ((iCur < 0) || (iCur >= SRDATAPOINTS))
               continue; // out of range

            fp fWeight = (int)dwWindow + 1 - abs(iOffset);
            fCount += fWeight;
            fSumVoiced += afEnergy[0][iCur] * fWeight;
            fSumNoise += afEnergy[1][iCur] * fWeight;
         } // iOffset
         fSumVoiced /= fCount;
         fSumNoise /= fCount;

         // wrte out
         // dont have to worry about it exceeding bounds
         psrf->acVoiceEnergy[i] = AmplitudeToDb (fSumVoiced);
         psrf->acNoiseEnergy[i] = AmplitudeToDb (fSumNoise);
      } // i
   } // j



   // do the volume adjustments... while at it caluclate the band width
   fp afBandWidth[2][SRDATAPOINTS*SUPERSAMPLE];
   fp afBandWidthSum[2];
   afBandWidthSum[0] = afBandWidthSum[1] = 0;
   for (dwUnvoiced = 0; dwUnvoiced < 2; dwUnvoiced++) {
      char *pcSRF = dwUnvoiced ? &srf.acNoiseEnergy[0] : &srf.acVoiceEnergy[0];
      char *pcSmooth = dwUnvoiced ? &srfSmooth.acNoiseEnergy[0] : &srfSmooth.acVoiceEnergy[0];
      char *pcRoughish = dwUnvoiced ? &srfRoughish.acNoiseEnergy[0] : &srfRoughish.acVoiceEnergy[0];
      int iVolumeNorm = VolumeNormalize (pDI, dwUnvoiced);

      for (i = 0; i < SRDATAPOINTS; i++) {
         fp fOctave = (fp)i / (fp)SRDATAPOINTS * (fp) SROCTAVE - 0.5;
         int iOctaveLow = (int)floor(fOctave);
         int iOctaveHigh = iOctaveLow+1;
         iOctaveLow = max(iOctaveLow, 0);
         iOctaveLow = min(iOctaveLow, SROCTAVE-1);
         iOctaveHigh = max(iOctaveHigh, 0);
         iOctaveHigh = min(iOctaveHigh, SROCTAVE-1);
         fOctave -= iOctaveLow;

         // just once in dwVoiced, calculate the band width
         // BUGFIX - Calculate for both voiced and unvoiced
         //if (!dwUnvoiced) {
         fp fWidth = (fp)pDI->afOctaveBands[dwUnvoiced][iOctaveLow] * (1.0 - fOctave) +
            (fp)pDI->afOctaveBands[dwUnvoiced][iOctaveHigh] * fOctave;
         for (j = 0; j < SUPERSAMPLE; j++)
            afBandWidth[dwUnvoiced][i*SUPERSAMPLE+j] = fWidth;
         afBandWidthSum[dwUnvoiced] += fWidth*SUPERSAMPLE;
         //}

         // determine emphasize adjustment
         fp fValue = (fp)pDI->afEmphasizePeaks[dwUnvoiced][iOctaveLow] * (1.0 - fOctave) +
            (fp)pDI->afEmphasizePeaks[dwUnvoiced][iOctaveHigh] * fOctave;
         fValue *= fValue; // pow 2
         fp fOrig = DbToAmplitude (pcSRF[i]);
         fp fSmooth = DbToAmplitude (pcSmooth[i]);
         fp fRoughish = DbToAmplitude (pcRoughish[i]);
         fValue = (fp)(fSmooth + fOrig - fRoughish) +
            fValue * (fp)(fRoughish - fSmooth);
         fValue = max(fValue, 0);
         fValue = (int)AmplitudeToDb (fValue);

         // determine volume adjustment
         fValue += (fp)((int)pDI->abVolume[dwUnvoiced][iOctaveLow] + iVolumeNorm) * (1.0 - fOctave) +
            (fp)((int)pDI->abVolume[dwUnvoiced][iOctaveHigh] + iVolumeNorm) * fOctave;


         // write out
         int iDelta = (int)floor(fValue + 0.5); // rounded
         iDelta = max(iDelta, SRABSOLUTESILENCE);
         iDelta = min(iDelta, SRMAXLOUDNESS);
         pcSRF[i] = (char)iDelta;
      } // i
   } // dwUnvoiced

   // convert from voiced to unvoiced
   for (i = 0; i < SRDATAPOINTS; i++) {
      fp fOctave = (fp)i / (fp)SRDATAPOINTS * (fp) SROCTAVE - 0.5;
      int iOctaveLow = (int)floor(fOctave);
      int iOctaveHigh = iOctaveLow+1;
      iOctaveLow = max(iOctaveLow, 0);
      iOctaveLow = min(iOctaveLow, SROCTAVE-1);
      iOctaveHigh = max(iOctaveHigh, 0);
      iOctaveHigh = min(iOctaveHigh, SROCTAVE-1);
      fOctave -= iOctaveLow;

      fp fValue = m_afVoicedToUnvoiced[iOctaveLow] * (1.0 - fOctave) +
         m_afVoicedToUnvoiced[iOctaveHigh] * fOctave;
      if (fValue < EPSILON)
         continue;   // too low

      fp fVoiced = DbToAmplitude (srf.acVoiceEnergy[i]);
      fp fUnvoiced = DbToAmplitude (srf.acNoiseEnergy[i]);
      fUnvoiced += fVoiced * fValue;
      fVoiced *= (1.0 - fValue);
      srf.acVoiceEnergy[i] = AmplitudeToDb (fVoiced);
      srf.acNoiseEnergy[i] = AmplitudeToDb (fUnvoiced);
   } // i

   // overall increase/decrease volume
   int aiOverall[2];
   for (i = 0; i < 2; i++) {
      aiOverall[i] = (int) m_abOverallVolume[i];
      if (aiOverall[i] <= -23)
         aiOverall[i] = SRABSOLUTESILENCE; // so totally mute
   } // i
   for (i = 0; i < SRDATAPOINTS; i++) {
      int iTemp;

      // voiced
      iTemp = (int)srf.acVoiceEnergy[i] + aiOverall[0];
      iTemp = max(iTemp, SRABSOLUTESILENCE);
      iTemp = min(iTemp, SRMAXLOUDNESS);
      srf.acVoiceEnergy[i] = (char)iTemp;

      // unvoiced
      iTemp = (int)srf.acNoiseEnergy[i] + aiOverall[1];
      iTemp = max(iTemp, SRABSOLUTESILENCE);
      iTemp = min(iTemp, SRMAXLOUDNESS);
      srf.acNoiseEnergy[i] = (char)iTemp;
   } // i

   // stretch this out
   for (dwUnvoiced = 0; dwUnvoiced < 2; dwUnvoiced++) {
      char *pc = dwUnvoiced ? srf.acNoiseEnergy : srf.acVoiceEnergy;

      for (i = 0; i < SRDATAPOINTS*SUPERSAMPLE; i++) {
         fp fIndex = (fp)i / (fp)SUPERSAMPLE - 0.5;
         int iLow = (int)floor(fIndex);
         int iHigh = iLow+1;
         iLow = max(iLow, 0);
         iLow = min(iLow, SRDATAPOINTS-1);
         iHigh = max(iHigh, 0);
         iHigh = min(iHigh, SRDATAPOINTS-1);
         fIndex -= iLow;

         fp fLow = DbToAmplitude (pc[iLow]);
         fp fHigh = DbToAmplitude (pc[iHigh]);
         afEnergy[dwUnvoiced][i] = fLow * (1.0 - fIndex) + fHigh * (fIndex);
      } // i

      // normalize the band width
      if (!afBandWidthSum[dwUnvoiced])
         afBandWidthSum[dwUnvoiced] = 1;

      afBandWidthSum[dwUnvoiced] = (fp)SRDATAPOINTS / afBandWidthSum[dwUnvoiced];
      for (i = 0; i < SRDATAPOINTS*SUPERSAMPLE; i++) {
         afBandWidth[dwUnvoiced][i] *= afBandWidthSum[dwUnvoiced];
         if (i)
            afBandWidth[dwUnvoiced][i] += afBandWidth[dwUnvoiced][i-1]; // so now is increment
         else
            afBandWidth[dwUnvoiced][i] = 0;  // so starts at 0
      }
   } // dwUnvoiced


#if 0 // to test and make sure phase affected
   // want to test by hardcoding bands for pSDPOrig and making sure bent
   for (i = 0; i < SRDATAPOINTSDETAILED; i++) {
      fp fPhase = (fp)i / (fp)SRDATAPOINTSDETAILED * 2.0 * PI * 5.0;
      pSDPOrig->afVoicedPhase[i][0] = sin(fPhase);
      pSDPOrig->afVoicedPhase[i][1] = cos(fPhase);
      if (i < SRPHASENUM)
         pSDPMod->afHarmPhase[i][0] = pSDPMod->afHarmPhase[i][1] = 0;
   }
#endif // 0

   // apply band to phase
   DWORD dwCur;
   DWORD dwStart, dwEnd;
   DWORD dwCount; // BUGFIX - Was =0

#ifdef VDHACK_BENDPHASES
   if (pSDPOrig && pSDPMod) {
      dwCur = 0;
      fp fSumSin, fSumCos;

      for (dwStart = 0; (dwStart < SRDATAPOINTS * SUPERSAMPLE) && (dwCur < SRDATAPOINTSDETAILED); ) {
         // figure out all the points that fit within dwCur
         fp fLimit =(fp)(dwCur + 1);
         fSumSin = fSumCos = 0;
         dwCount = 0;
         for (dwEnd = dwStart; dwEnd < (SRDATAPOINTS*SUPERSAMPLE); dwEnd++) {
            if (afBandWidth[0][dwEnd] * SUPERSAMPLE >= fLimit)
               break;   // point no longer within cur

            fSumSin += pSDPOrig->afVoicedPhase[dwEnd][0];
            fSumCos += pSDPOrig->afVoicedPhase[dwEnd][1];
            dwCount++;
         }

         // if dwEnd <= dwStart then no points assigned to this,
         // so duplicate the current one
         if (dwEnd <= dwStart) {
            pSDPMod->afVoicedPhase[dwCur][0] = pSDPOrig->afVoicedPhase[dwStart][0];
            pSDPMod->afVoicedPhase[dwCur][1] = pSDPOrig->afVoicedPhase[dwStart][1];
            dwCur++;
            continue;
         }

         // NOTE - Keeping phase interp bend on since have lots of detail
#define DOINTERPBENDPHASE // - Specifically not using since blurs
#ifndef DOINTERPBENDPHASE // non-interpolation
         // BUGFIX - take out interpolation so get crisper "image"
         if (dwCount > 1) {
            fSumSin = pSDPOrig->afVoicedPhase[(dwStart+dwEnd)/2][0];
            fSumCos = pSDPOrig->afVoicedPhase[(dwStart+dwEnd)/2][1];
            dwCount = 1;
         }
#endif // DOINTERPBEND


         // else, use sum
         pSDPMod->afVoicedPhase[dwCur][0] = fSumSin / (fp)dwCount;
         pSDPMod->afVoicedPhase[dwCur][1] = fSumCos / (fp)dwCount;
         dwCur++;
         dwStart = dwEnd;
      }
      // if dwCur < SRDATAPOINTS then fill in the remaining
      for (; dwCur < SRDATAPOINTSDETAILED; dwCur++) {
         pSDPMod->afVoicedPhase[dwCur][0] = pSDPOrig->afVoicedPhase[SRDATAPOINTS * SUPERSAMPLE - 1][0];
         pSDPMod->afVoicedPhase[dwCur][1] = pSDPOrig->afVoicedPhase[SRDATAPOINTS * SUPERSAMPLE - 1][1];
      }
   } // band to phase
#endif //  VDHACK_BENDPHASES

   // apply band to new wave
   for (dwUnvoiced = 0; dwUnvoiced < 2; dwUnvoiced++) {
      fp fSum;
      dwCur = 0;

      for (dwStart = 0; (dwStart < SRDATAPOINTS * SUPERSAMPLE) && (dwCur < SRDATAPOINTS); ) {
         // figure out all the points that fit within dwCur
         fp fLimit =(fp)(dwCur + 1);
         fSum = 0;
         dwCount = 0;
         for (dwEnd = dwStart; dwEnd < (SRDATAPOINTS*SUPERSAMPLE); dwEnd++) {
            if (afBandWidth[dwUnvoiced][dwEnd] >= fLimit)
               break;   // point no longer within cur

            fSum += afEnergy[dwUnvoiced][dwEnd];
            dwCount++;
            // fUnvoiced += afEnergy[1][dwEnd];
         }

         // if dwEnd <= dwStart then no points assigned to this,
         // so duplicate the current one
         if (dwEnd <= dwStart) {
            if (dwUnvoiced)
               pMod->acNoiseEnergy[dwCur] = AmplitudeToDb (afEnergy[dwUnvoiced][dwStart]);
            else
               pMod->acVoiceEnergy[dwCur] = AmplitudeToDb (afEnergy[dwUnvoiced][dwStart]);
            dwCur++;
            continue;
         }

// #define DOINTERPBEND - Specifically not using since blurs
#ifndef DOINTERPBEND // non-interpolation
         // BUGFIX - take out interpolation so get crisper "image"
         if (dwCount > 1) {
            fSum = afEnergy[dwUnvoiced][(dwStart+dwEnd)/2];
            dwCount = 1;
         }
#endif // DOINTERPBEND


         // else, use sum
         if (dwUnvoiced)
            pMod->acNoiseEnergy[dwCur] = AmplitudeToDb (fSum / (fp)dwCount);
         else
            pMod->acVoiceEnergy[dwCur] = AmplitudeToDb (fSum / (fp)dwCount);
         dwCur++;
         dwStart = dwEnd;
      }
      // if dwCur < SRDATAPOINTS then fill in the remaining
      for (; dwCur < SRDATAPOINTS; dwCur++) {
         if (dwUnvoiced)
            pMod->acNoiseEnergy[dwCur] = AmplitudeToDb (afEnergy[dwUnvoiced][SRDATAPOINTS * SUPERSAMPLE - 1]);
         else
            pMod->acVoiceEnergy[dwCur] = AmplitudeToDb (afEnergy[dwUnvoiced][SRDATAPOINTS * SUPERSAMPLE - 1]);
      }
   } // dwUnvoiced

   // copy over phase
   memcpy (pMod->abPhase, pOrig->abPhase, sizeof(pMod->abPhase));

done:
   // do pitch modifications
   fPitchOrig = max(fPitchOrig, 1); // at least some pitch
   fp fOrigMod = m_fPitchOrig;
   if (!fOrigMod)
      fOrigMod = 1; // so have something
   if (!m_fPitchScale)
      m_fPitchScale = 1;
   fp fDelta = log(fPitchOrig / fOrigMod) * m_fPitchVariationScale;
   fPitchOrig = fOrigMod * m_fPitchScale * exp(fDelta);

   // make it musical
   if (m_fPitchMusical) {
#define AFREQ           440.0       // frequency of a to use
      fp fNote = log(fPitchOrig / (fp) AFREQ) / (log((fp)2) / 12);
      fNote = floor(fNote + 0.5);   // round to nearest
      fPitchOrig = exp(fNote * (log((fp)2) / (fp)12)) * AFREQ;
   }

   return fPitchOrig;
}


/*************************************************************************************
CVoiceDisguise::ModifySRFEATUREInWave - Modifies all the SRFEATUREs in a wave.

inputs
   PCM3DWave         pWave - Wave to modify
   PSRDETAILEDPHASE  paSRDP - If not NULL, this is the detauled phase to use. If NULL, then the detauled phase
                     is automatically generated. Must have pWave->m_dwSRSamples elements
   fp                *pfFormantShift - If not NULL, filled in with the formant shift generated by the
                     disguise, so can approximate in psola.
   DWORD             dwStartFeature - Starting feature number. defaults to 0
   DWORD             dwEndFeature - End feature number. Defaults to (DWORD)-1
returns
   BOOL - TRUE if success. FAIL if no SRFEATURE or pitch in the wave
*/
BOOL CVoiceDisguise::ModifySRFEATUREInWave (PCM3DWave pWave, PSRDETAILEDPHASE paSRDP, fp *pfFormantShift,
                                            DWORD dwStartFeature, DWORD dwEndFeature)
{
   if (pfFormantShift)
      *pfFormantShift = 0; // to zero out

   if (!pWave->m_dwSRSamples || !pWave->m_adwPitchSamples[PITCH_F0] || (pWave->m_dwSRSkip != pWave->m_adwPitchSkip[PITCH_F0]))
      return FALSE;

   CMem memDetailed;
   DWORD i;

   // if no detailed phase then make some
   if (!paSRDP) {
      if (!memDetailed.Required (pWave->m_dwSRSamples * sizeof(SRDETAILEDPHASE)))
         return FALSE;

      paSRDP = (PSRDETAILEDPHASE) memDetailed.p;
      for (i = 0; i < pWave->m_dwSRSamples; i++)
         SRDETAILEDPHASEFromSRFEATURE (pWave->m_paSRFeature + i, pWave->PitchAtSample (PITCH_F0, i * pWave->m_dwSRSkip, 0), paSRDP + i);
   }
   dwEndFeature = min(dwEndFeature, pWave->m_dwSRSamples);

#ifdef _DEBUG
   DWORD dwTimeCur, dwTimeStart = GetTickCount();
   WCHAR szTemp[128];
#endif

   // figure out the disguise info
   CListFixed lDI;
   BOOL fRet = DetermineDISGUISEINFOForWave (dwEndFeature - dwStartFeature, pWave, pWave->m_paSRFeature + dwStartFeature,
      (PWVPHONEME) pWave->m_lWVPHONEME.Get(0), pWave->m_lWVPHONEME.Num(),
      pWave->m_dwSRSkip, dwStartFeature * pWave->m_dwSRSkip,
      &lDI);
#ifdef _DEBUG
   dwTimeCur = GetTickCount();
   swprintf (szTemp, L"\r\nDetermineDISGUISEINFOForWave = %d ms", (int)(dwTimeCur - dwTimeStart));
   dwTimeStart = dwTimeCur;
   OutputDebugStringW (szTemp);
#endif
   if (!fRet)
      return fRet;

   // potentially mix in other waves
   PDISGUISEINFO pDI = (PDISGUISEINFO)lDI.Get(0);
   DWORD dwUnvoiced;
   if (m_lMixPCVoiceDisguise.Num()) {
      CListFixed lDI2;
      fp *pafWeight = (fp*)m_lMixWeight.Get(0);
      PCVoiceDisguise *ppVD = (PCVoiceDisguise*) m_lMixPCVoiceDisguise.Get(0);

      // loop
      DWORD i, j, k;
      for (i = 0; i < m_lMixWeight.Num(); i++) {
         PDISGUISEINFO pDI2, pDIOrig;
         if (i) {
            if (!ppVD[i-1]->DetermineDISGUISEINFOForWave (dwEndFeature - dwStartFeature, pWave, pWave->m_paSRFeature + dwStartFeature,
               (PWVPHONEME) pWave->m_lWVPHONEME.Get(0), pWave->m_lWVPHONEME.Num(),
               pWave->m_dwSRSkip, dwStartFeature * pWave->m_dwSRSkip,
               &lDI2))
               return FALSE; // error
            if (lDI2.Num() != lDI.Num())
               return FALSE;  // shouldnt happen
            pDI2 = (PDISGUISEINFO)lDI2.Get(0);
         }
         else
            pDI2 = pDI; // since will modify what have

         // scale everything by the weight
         fp fScale = pafWeight[i];
         for (j = 0, pDIOrig = pDI; j < lDI.Num(); j++, pDI2++, pDIOrig++) {
            for (dwUnvoiced = 0; dwUnvoiced < 2; dwUnvoiced++) {
               for (k = 0; k < SROCTAVE; k++) {
                  pDI2->afEmphasizePeaks[dwUnvoiced][k] *= fScale;
                  pDI2->afOctaveBands[dwUnvoiced][k] *= fScale;

                  // add them in
                  if (i) {
                     pDIOrig->afEmphasizePeaks[dwUnvoiced][k] += pDI2->afEmphasizePeaks[dwUnvoiced][k];
                     pDIOrig->afOctaveBands[dwUnvoiced][k] += pDI2->afOctaveBands[dwUnvoiced][k];
                  }

                  // do the volume as special to minimize induced error
                  //for (dwVoiced = 0; dwVoiced < 2; dwVoiced++) {
                  fp fCur = i ? DbToAmplitude (pDIOrig->abVolume[dwUnvoiced][k]) : 0;
                  fCur += DbToAmplitude (pDI2->abVolume[dwUnvoiced][k]) * fScale;
                  pDIOrig->abVolume[dwUnvoiced][k] = AmplitudeToDb (fCur);
                  //} // dwVoiced
               } // k
            } // dwUnvoiced
         } // j, over all disguise
      } // i
   } // if other siguise info

#ifdef _DEBUG
   dwTimeCur = GetTickCount();
   swprintf (szTemp, L"\r\nMixInOtherWaves = %d ms", (int)(dwTimeCur - dwTimeStart));
   dwTimeStart = dwTimeCur;
   OutputDebugStringW (szTemp);
#endif

   // so can keep track of disguise
   DISGUISEINFO DISum;
   memset (&DISum, 0, sizeof(DISum));

   ApplyDISGUISEINFOToWave (dwEndFeature - dwStartFeature, pWave, pWave->m_paSRFeature + dwStartFeature,
      paSRDP + dwStartFeature, pWave->m_apPitch[PITCH_F0] + dwStartFeature, pDI, &DISum);
#ifdef _DEBUG
   dwTimeCur = GetTickCount();
   swprintf (szTemp, L"\r\nApplyDISGUISEINFOToWave = %d ms", (int)(dwTimeCur - dwTimeStart));
   dwTimeStart = dwTimeCur;
   OutputDebugStringW (szTemp);
#endif

   // rewrite phase
   for (i = dwStartFeature; i < dwEndFeature; i++)
      SRDETAILEDPHASEToSRFEATURE (paSRDP + i, pWave->PitchAtSample (PITCH_F0, i * pWave->m_dwSRSkip, 0), pWave->m_paSRFeature + i);


   // figure out the formant shift
#define FORMANTSHIFTOCTAVE          4  // use 4th octave for formant shift
   fp fBelow = 0, fAbove = 0;
   for (i = 0; i < SROCTAVE; i++)
      if (i < FORMANTSHIFTOCTAVE)
         fBelow += DISum.afOctaveBands[0][i];
      else
         fAbove += DISum.afOctaveBands[0][i];
   fp fSum = fBelow + fAbove;
   if (fSum && pfFormantShift) {
      fSum = (fp)SROCTAVE / fSum; // so sums up to right number of octaves
      fBelow *= fSum;
      *pfFormantShift = fBelow - FORMANTSHIFTOCTAVE;
   }

   return TRUE;
}


/***************************************************************************
CVoiceDisguise::SynthesizeFromSRFEATURE - This resythesizes the wave from its existing SRFEATUREs
and pitch information.

NOTE: This doesnt affect undo or set the dirty flag.

inputs
   int               iTTSQuality - TTS quality to use
   PCM3DWave         pWave - Wave
   PPSOLASTRUCT      paPS - Array of PSOLASTRUCT. If not NULL then will synthesize using PSOLA.
                        If NULL then do normal additive sine-wave synthesis
   DWORD             dwNumPS - Number of entries in paPS
   fp                fFormantShift - Formant shift to use for PSOLA. 0 is default. Overwritten unless fSkipModify is set.
   PCListFixed       plWAVESEGMENTFLOAT - If not using PSOLA, can pass in an array of FFTs for each of the
                     SRFEATUREs and will use that to get phase. If using psola, this is igored, so pass
                     in NULL.
   PSRDETAILEDPHASE  paSRDP - If not NULL, this is the detauled phase to use. If NULL, then the detauled phase
                     is automatically generated. Must have pWave->m_dwSRSamples elements
   BOOL              fEnablePCM - Set to TRUE (default) if PCM synthesis should be allowed. FALSE
                     if shouldnt, such as for whisper.
   PCProgressSocket  pProgress - Progress
   BOOL              fClearSRFEATURE - If TRUE then clera the existing SRFEATURES (since
                     they're technically not valid). If FALSE leave them there
   BOOL              fSkipModify - If TRUE then skip the modifySRFEATUREInWave(). Usually FALSE
   PCProgressWaveSample pProgressWave - This is an accurate progress bar used
                  so that will be able to play out TTS while it's being generated.
returns
   BOOL - TRUE if success. Will fail if doesnt have SRFeatures or pitch
*/
BOOL CVoiceDisguise::SynthesizeFromSRFeature (int iTTSQuality, PCM3DWave pWave, PPSOLASTRUCT paPS, DWORD dwNumPS, fp fFormantShift,
                                              PCListFixed plWAVESEGMENTFLOAT,
                                              PSRDETAILEDPHASE paSRDP, BOOL fEnablePCM, PCProgressSocket pProgress,
                                                BOOL fClearSRFEATURE, BOOL fSkipModify, PCProgressWaveSample pProgressWave)
{
   CVoiceSynthesize vs;
   CMem memDetailed;

   if (!fSkipModify) {
      if (!ModifySRFEATUREInWave (pWave, paSRDP, &fFormantShift))
         return FALSE;
   }

   // load in the cache
   vs.m_fHarmonicSpacing = m_fNonIntHarmonics;
   PWSTR pszWave = (PWSTR)m_memWaveBase.p;
   DWORD dwLen = (DWORD)wcslen(pszWave);
   WCHAR szTemp[256];
   if (dwLen+1 < sizeof(szTemp)/sizeof(WCHAR)) {
      wcscpy (szTemp, pszWave);
      vs.m_fWavePitch = m_fWaveBasePitch;

      // if we have a wave then try to load it
      if (szTemp[0] && !m_pWaveCache) {
         PWSTR pszSourceFile = (PWSTR)m_memSourceTTSFile.p;
         if (pszSourceFile)
            ResolvePathIfNotExist (szTemp, pszSourceFile);
         m_pWaveCache = WaveCacheOpen (szTemp);
      }
   }

   return vs.SynthesizeFromSRFeature (iTTSQuality, pWave, paPS, dwNumPS, fFormantShift,
      plWAVESEGMENTFLOAT, fEnablePCM, pProgress, m_pWaveCache,
      fClearSRFEATURE, pProgressWave);
}


/*************************************************************************************
CVoiceDisguise::DialogPlaySample - Copies the orig wave, applied the given features,
converts to audio, and plays it.

inputs
   HWND        hWnd - To show progress.
   BOOL        fIncludeTemp - If TRUE then include temporary PHONEMEDISGUISE
returns
   BOOL - TRUE if success
*/
BOOL CVoiceDisguise::DialogPlaySample (HWND hWnd, BOOL fIncludeTemp)
{
   // stop all
   if (m_pWaveOrig)
      m_pWaveOrig->QuickPlayStop();
   if (m_pWaveMimic)
      m_pWaveMimic->QuickPlayStop();
   if (m_pWaveScratch)
      m_pWaveScratch->QuickPlayStop();

   if (!m_pWaveOrig)
      return FALSE;
   if (m_pWaveScratch)
      delete m_pWaveScratch;
   m_pWaveScratch = m_pWaveOrig->Clone ();
   if (!m_pWaveScratch)
      return FALSE;

   CProgress Progress;
   Progress.Start (hWnd, "Processing...");

   DWORD dwTruncTo;
   if (fIncludeTemp) {
      dwTruncTo = m_lPHONEMEDISGUISE.Num();
      PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) m_lPHONEMEDISGUISETemp.Get(0);
      DWORD i;
      for (i = 0; i < m_lPHONEMEDISGUISETemp.Num(); i++, pPD++)
         m_lPHONEMEDISGUISE.Add (pPD);
   }

   BOOL fRet = SynthesizeFromSRFeature (4, m_pWaveScratch, NULL, 0, NULL, NULL, NULL, TRUE, &Progress, FALSE);

   if (fIncludeTemp)
      m_lPHONEMEDISGUISE.Truncate (dwTruncTo);

#ifdef _DEBUG
   strcpy (m_pWaveScratch->m_szFile, "c:\\test.wav");
   m_pWaveScratch->Save (TRUE, NULL);
#endif

   m_pWaveScratch->QuickPlay ();
   return fRet;
}



/*************************************************************************************
CVoiceDisguise::DialogPlaySample - Plays a sample of the given SRFEATUREs.

inputs
   PSRFEATURE        pSRFEATURE - To play
   fp                fPitch - To play at
returns
   BOOL - TRUE if success
*/
BOOL CVoiceDisguise::DialogPlaySample (PSRFEATURE pSRFEATURE, fp fPitch)
{
#define SECONDSOFSAMPLE       1

   // if it's the same as last time then no change
   BOOL fTheSame = m_pWaveScratch ? TRUE : FALSE;
   if (fTheSame && (m_pWaveScratch->m_dwSamples != m_pWaveScratch->m_dwSamplesPerSec * SECONDSOFSAMPLE))
      fTheSame = FALSE;
   if (fTheSame && (!m_pWaveScratch->m_adwPitchSamples[PITCH_F0] || !m_pWaveScratch->m_dwSRSamples))
      fTheSame = FALSE;
   DWORD i;
   if (fTheSame) for (i = 0; i < m_pWaveScratch->m_adwPitchSamples[PITCH_F0]; i++)
      if (m_pWaveScratch->m_apPitch[PITCH_F0][i].fFreq != fPitch) {
         fTheSame = FALSE;
         break;
      }
   if (fTheSame) for (i = 0; i < m_pWaveScratch->m_dwSRSamples; i++)
      if (memcmp (&m_pWaveScratch->m_paSRFeature[i], pSRFEATURE, sizeof(*pSRFEATURE))) {
         fTheSame = FALSE;
         break;
      }

   // if it's the same and is playing then do nothing
   //if (fTheSame && (m_pWaveScratch->QuickPlayQuery() >= 2))
   //   return TRUE;   // no point playing since the same thing

   // if not the same then redo
   PCM3DWave pNew = NULL;
   if (!fTheSame) {
      pNew = new CM3DWave;
      if (!pNew)
         return FALSE;

      // make 22 kHz
      pNew->ConvertSamplesAndChannels (22050, 1, NULL);

      // create number of samples
      pNew->BlankWaveToSize (SECONDSOFSAMPLE * pNew->m_dwSamplesPerSec, TRUE);
      pNew->BlankSRFeatures ();

      // fill in
      DWORD i;
      for (i = 0; i < pNew->m_adwPitchSamples[PITCH_F0]; i++)
         pNew->m_apPitch[PITCH_F0][i].fFreq = fPitch;
      for (i = 0; i < pNew->m_dwSRSamples; i++)
         pNew->m_paSRFeature[i] = *pSRFEATURE;

      CVoiceSynthesize vs;
      vs.SynthesizeFromSRFeature (4, pNew, NULL, 0, TRUE, NULL, FALSE, NULL);

      // recopy over, since synthesis seems to change
      for (i = 0; i < pNew->m_adwPitchSamples[PITCH_F0]; i++)
         pNew->m_apPitch[PITCH_F0][i].fFreq = fPitch;
      for (i = 0; i < pNew->m_dwSRSamples; i++)
         pNew->m_paSRFeature[i] = *pSRFEATURE;
   } // if not the same
   else
      pNew = NULL;

   // stop all
   if (m_pWaveOrig)
      m_pWaveOrig->QuickPlayStop();
   if (m_pWaveMimic)
      m_pWaveMimic->QuickPlayStop();
   if (m_pWaveScratch)
      m_pWaveScratch->QuickPlayStop();

   if (pNew) {
      if (m_pWaveScratch)
         delete m_pWaveScratch;
      m_pWaveScratch = pNew;
   }

   m_pWaveScratch->QuickPlay ();
   return TRUE;
}



/*************************************************************************************
CVoiceDisguise::DialogSRFEATUREGet - Gets a SRFEATURE, and potetnially disguises it.
Used for displaying

inputs
   DWORD          dwSample - Sample number in the wave to play
   BOOL           fOrig - If TRUE then play from the original, else mimic
   BOOL           fApplyDisguise - If TRUE then apply the disguise to the voice
   fp             *pfPitch - Filled with the pitch of the feature
   PSRFEATURE     pSRFEATURE - Filled with the feature
returns
   BOOL - TRUE if success
*/
BOOL CVoiceDisguise::DialogSRFEATUREGet (DWORD dwSample, BOOL fOrig, BOOL fApplyDisguise,
                                         fp *pfPitch, PSRFEATURE pSRFEATURE)
{
   // wave
   *pfPitch = 1;
   PCM3DWave pWave = fOrig ? m_pWaveOrig : m_pWaveMimic;
   if (!pWave)
      return FALSE;

   // pitch sample
   DWORD dwPitch = dwSample / pWave->m_adwPitchSkip[PITCH_F0];
   if (dwPitch >= pWave->m_adwPitchSamples[PITCH_F0])
      return FALSE;
   *pfPitch = pWave->m_apPitch[PITCH_F0][pWave->m_dwChannels*dwPitch].fFreq;

   // if beyond range then error
   dwSample /= pWave->m_dwSRSkip;
   if (dwSample >= pWave->m_dwSRSamples)
      return FALSE;

   // average adjacent features together to reduce noisyness
#define NEIGHBORFEAT    ((int)(pWave->m_dwSRSAMPLESPERSEC/50))
   int iWindow;
   fp afSum[2][SRDATAPOINTS];
   DWORD i;
   memset (afSum, 0, sizeof(afSum));
   fp fCount = 0;
   fp fWeight;
   for (iWindow = -NEIGHBORFEAT; iWindow <= NEIGHBORFEAT; iWindow++) {
      int iLook = (int)dwSample + iWindow;
      if ((iLook < 0) || (iLook >= (int)pWave->m_dwSRSamples))
         continue;

      fWeight = NEIGHBORFEAT + 1 - abs(iWindow);
      fCount += fWeight;

      for (i = 0; i < SRDATAPOINTS; i++) {
         afSum[0][i] += DbToAmplitude (pWave->m_paSRFeature[iLook].acVoiceEnergy[i]) * fWeight;
         afSum[1][i] += DbToAmplitude (pWave->m_paSRFeature[iLook].acNoiseEnergy[i]) * fWeight;
      } // i
   } // iWindow
   fCount = 1.0 / fCount;  // to use as scale
   SRFEATURE srf;
   for (i = 0; i < SRDATAPOINTS; i++) {
      srf.acVoiceEnergy[i] = AmplitudeToDb (afSum[0][i] * fCount);
      srf.acNoiseEnergy[i] = AmplitudeToDb (afSum[1][i] * fCount);
   } // i


   PSRFEATURE psrOrig = &srf; // &pWave->m_paSRFeature[dwSample];
   if (fApplyDisguise)
      *pfPitch = ModifySRFEATURE (psrOrig, NULL, pSRFEATURE, NULL, *pfPitch, NULL, FALSE);
   else
      *pSRFEATURE = *psrOrig;

   return TRUE;
}


#if 0 // no longer used
/*************************************************************************************
CVoiceDisguise::DialogPlaySample - Plays a sample of one of the waves.

inputs
   DWORD          dwSample - Sample number in the wave to play
   BOOL           fOrig - If TRUE then play from the original, else mimic
   BOOL           fApplyDisguise - If TRUE then apply the disguise to the voice
   DWORD           dwSamplePitch - If this isn't -1 then this pitch is taken from the mimic
                     voice.
returns
   BOOL - TRUE if success
*/
BOOL CVoiceDisguise::DialogPlaySample (DWORD dwSample, BOOL fOrig, BOOL fApplyDisguise,
                                       DWORD dwSamplePitch)
{
   // if nothing to mimic then must get pitch from the original
   if ((dwSamplePitch != (DWORD)-1) && !m_pWaveMimic)
      dwSamplePitch = (DWORD)-1;

   // wave
   SRFEATURE sr;
   fp fPitch;
   if (!DialogSRFEATUREGet (dwSample, fOrig, fApplyDisguise, &fPitch, &sr))
      return FALSE;

   // pitch
   PCM3DWave pWave = fOrig ? m_pWaveOrig : m_pWaveMimic;
   if (!pWave)
      return FALSE;
   if (dwSamplePitch != (DWORD)-1) {
      dwSamplePitch = dwSamplePitch / m_pWaveMimic->m_adwPitchSkip[PITCH_F0];
      if (dwSamplePitch >= m_pWaveMimic->m_adwPitchSamples[PITCH_F0])
         return FALSE;
      fPitch = m_pWaveMimic->m_apPitch[PITCH_F0][dwSamplePitch].fFreq;
   }

   // play
   return DialogPlaySample (&sr, fPitch);
}
#endif // 0

/*************************************************************************************
CVoiceDisguise::DialogPlaySample - Plays a sample of one of the waves.

inputs
   PPHONEMEDISGUISE pPD - Phoneme disguise to play
   BOOL           fOrig - If TRUE then play from the original, else mimic
   BOOL           fApplyDisguise - If TRUE then apply the disguise to the voice (for the original)
returns
   BOOL - TRUE if success
*/
BOOL CVoiceDisguise::DialogPlaySample (PPHONEMEDISGUISE pPD, BOOL fOrig, BOOL fApplyDisguise)
{
   SRFEATURE sr;
   fp fPitch;
   if (fOrig) {
      if (fApplyDisguise) {
         fPitch = ModifySRFEATURE (&pPD->srfOrig, NULL, &sr, NULL, pPD->fPitchOrig, &pPD->DI, FALSE);
         if (pPD->fMimic)
            fPitch = pPD->fPitchMimic;
      }
      else {
         sr = pPD->srfOrig;
         fPitch = pPD->fPitchOrig;
      }
   }
   else {
      if (!pPD->fMimic)
         return FALSE;  // cant play since nothing stored
      sr = pPD->srfMimic;
      fPitch = pPD->fPitchMimic;
   }

   // play
   return DialogPlaySample (&sr, fPitch);
}


/*************************************************************************************
CVoiceDisguise::DialogPlaySampleSnip - Plays a sample of one of the waves, just
a short snippet

inputs
   DWORD          dwSample - Sample number in the wave to play
   BOOL           fOrig - If TRUE then play from the original, else mimic
returns
   BOOL - TRUE if success
*/
BOOL CVoiceDisguise::DialogPlaySampleSnip (DWORD dwSample, BOOL fOrig)
{
   PCM3DWave pWave = fOrig ? m_pWaveOrig : m_pWaveMimic;
   if (!pWave)
      return FALSE;

   // stop all
   if (m_pWaveOrig)
      m_pWaveOrig->QuickPlayStop();
   if (m_pWaveMimic)
      m_pWaveMimic->QuickPlayStop();
   if (m_pWaveScratch)
      m_pWaveScratch->QuickPlayStop();

   int iSnipLength = pWave->m_dwSamplesPerSec / 20;   // 1/20th of a sec to either side
   int iStart = (int)dwSample - iSnipLength;
   int iEnd = (int)dwSample + iSnipLength;
   iStart = max(iStart, 0);
   iEnd = min(iEnd, (int)pWave->m_dwSamples);
   
   if (iEnd <= iStart)
      return FALSE;

   pWave->QuickPlay ((DWORD)iStart, (DWORD)iEnd);

   return TRUE;
}

/*************************************************************************************
CVoiceDisguise::CalcPitchVariation - Determines how much the pitch varies.

inputs
   PCM3DWave         pWave - WAve
   fp                fCenterPitch - Central pitch, non-zero
returns
   fp - Number indicating how much it varies
*/
fp CVoiceDisguise::CalcPitchVariation (PCM3DWave pWave, fp fCenterPitch)
{
   if (!pWave->m_adwPitchSamples[PITCH_F0])
      return 0;   // no variation

   DWORD i;
   double fSum = 0, fStrength = 0;
   for (i = 0; i < pWave->m_adwPitchSamples[PITCH_F0]; i++) {
      fp fPitch = pWave->m_apPitch[PITCH_F0][i*pWave->m_dwChannels].fFreq;
      fPitch = max(fPitch, 1);
      fPitch = log (fPitch / fCenterPitch);
      fPitch = fabs(fPitch);

      fStrength += (double)(pWave->m_apPitch[PITCH_F0][i*pWave->m_dwChannels].fStrength+1);
      fSum += (double) fPitch*
         (double)(pWave->m_apPitch[PITCH_F0][i*pWave->m_dwChannels].fStrength+1);
      // BUGFIX - Adding 1 to strength so always have at least something
   }
   if (fStrength)
      fSum /= fStrength;

   return (fp)fSum;
}



/*************************************************************************************
CVoiceDisguise::DialogSRFEATUREViewUpdate - Updates a single SRFEATURE view control.

inputs
   PCEscPage   pPage - Page
   DWORD       dwNum - Feature number, 0 ..NUMWAVEMARKERS-1.
               If this is -1 then update all of them
returns
   none
*/
void CVoiceDisguise::DialogSRFEATUREViewUpdate (PCEscPage pPage, DWORD dwNum)
{
   if (dwNum >= m_lPHONEMEDISGUISETemp.Num()) {
      // show all
      DWORD i;
      DWORD dwNum = m_lPHONEMEDISGUISETemp.Num();
      for (i = 0; i < dwNum; i++)
         DialogSRFEATUREViewUpdate (pPage, i);
      return;
   }

   // else, updating one
   PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) m_lPHONEMEDISGUISETemp.Get(dwNum);

   // fill i nthe structure
   ESCMSRFEATUREVIEW fv;
   memset (&fv, 0, sizeof(fv));
   fv.cColor = gaColorMarker[dwNum];
   memcpy (&fv.afOctaveBands, pPD->DI.afOctaveBands, sizeof(fv.afOctaveBands));
   fv.fShowBlack = TRUE;
   fv.fShowWhite = pPD->fMimic;
   fv.srfWhite = pPD->srfMimic;
   ModifySRFEATURE (&pPD->srfOrig, NULL, &fv.srfBlack, NULL, pPD->fPitchOrig, &pPD->DI, FALSE);

   // need to scale the mimic so same energy as orig
   if (fv.fShowWhite) {
      fp fEnergyOrig = SRFEATUREEnergy (FALSE, &pPD->srfOrig);
      fp fEnergyMimic = SRFEATUREEnergy (FALSE, &fv.srfWhite);
      fEnergyMimic = max(fEnergyMimic, 1);   // so no divide by 0
      SRFEATUREScale (&fv.srfWhite, fEnergyOrig / fEnergyMimic);
   }


   // finhd the control
   WCHAR szTemp[64];
   swprintf (szTemp, L"srfeature%d", (int)dwNum);
   PCEscControl pControl = pPage->ControlFind (szTemp);
   if (pControl)
      pControl->Message (ESCM_SRFEATUREVIEW, &fv);
}


/*************************************************************************************
CVoiceDisguise::CreateNoiseAndGarbage - This creates the noise and garbage models
for the voice.

inputs
   DWORD                dwNum - Number of existing disguises
   PPHONEMEDISGUISE     paPD - Array of dwNum phoneme disguises. The paPD[i].srfSmall
                        should already be filled in and normalized to 10000
   PPHONEMEDISGUISE     pPDNoise - Filled in with the noise model
   PPHONEMEDISGUISE     pPDGarbage - Filled in with the garbage model
returns
   none
*/
void CVoiceDisguise::CreateNoiseAndGarbage (DWORD dwNum, PPHONEMEDISGUISE paPD,
                                            PPHONEMEDISGUISE pPDNoise, PPHONEMEDISGUISE pPDGarbage)
{
   // fill in some info
   memset (pPDNoise, 0, sizeof(pPDNoise));
   memset (pPDGarbage, 0, sizeof(pPDGarbage));
   wcscpy (pPDNoise->szName, L"Noise");
   wcscpy (pPDGarbage->szName, L"Garbage");

   // loop through existing diguises and figure out the average stretch information
   fp afAvgOctave[2][SROCTAVE];
   fp afAvgPeak[2][SROCTAVE];
   fp afAvgScale[2][SROCTAVE];
   fp afGarbage[2][2][SRDATAPOINTSSMALL];
   memset (afAvgOctave, 0, sizeof(afAvgOctave));
   memset (afAvgPeak, 0, sizeof(afAvgPeak));
   memset (afAvgScale, 0, sizeof(afAvgScale));
   memset (afGarbage, 0, sizeof(afGarbage));
   DWORD i, j, dwUnvoiced;
   int iVolumeNorm;
   for (i = 0; i < dwNum; i++) {
      for (dwUnvoiced = 0; dwUnvoiced < 2; dwUnvoiced++) {
         // determine the sum of the octave bands, so that will noramlize to SROCTAVE
         fp fSum = OctaveNormalize (&paPD[i].DI, dwUnvoiced);
         // below code is replaced by above
         //fp fSum = 0;
         //for (j = 0; j < SROCTAVE; j++)
         //   fSum += paPD[i].DI.afOctaveBands[j];
         //if (fSum)
         //   fSum = (fp)SROCTAVE / fSum;

         iVolumeNorm = VolumeNormalize (&paPD[i].DI, dwUnvoiced);
         //aiVolumeNorm[1] = VolumeNormalize (&paPD[i].DI, 1);

         for (j = 0; j < SROCTAVE; j++) {
            afAvgOctave[dwUnvoiced][j] += paPD[i].DI.afOctaveBands[dwUnvoiced][j] * fSum;
            afAvgPeak[dwUnvoiced][j] += paPD[i].DI.afEmphasizePeaks[dwUnvoiced][j];
            afAvgScale[dwUnvoiced][j] += (fp)((int)paPD[i].DI.abVolume[dwUnvoiced][j] + iVolumeNorm);
            //afAvgScale[1][j] += (fp)((int)paPD[i].DI.abVolume[1][j] + aiVolumeNorm[1]);
         } // j
      } // dwUnvoiced

      // figure out the garbage value
      for (j = 0; j < SRDATAPOINTSSMALL; j++) {
         afGarbage[0][0][j] += DbToAmplitude (paPD[i].srfSmall.acVoiceEnergyMain[j]);
         afGarbage[0][1][j] += DbToAmplitude (paPD[i].srfSmall.acNoiseEnergyMain[j]);
         afGarbage[1][0][j] += DbToAmplitude (paPD[i].srfSmall.acVoiceEnergyDelta[j]);
         afGarbage[1][1][j] += DbToAmplitude (paPD[i].srfSmall.acNoiseEnergyDelta[j]);
      }
   } // i

   // if there are less than 16 points, average in with the master setting
#define MINENTRIES      16
   if (dwNum < MINENTRIES) {

      for (dwUnvoiced = 0; dwUnvoiced < 2; dwUnvoiced++) {
         fp fSum = OctaveNormalize (&m_DisguiseInfo, dwUnvoiced);
         // below code is replaced by above
         //fp fSum = 0;
         //for (j = 0; j < SROCTAVE; j++)
         //   fSum += m_DisguiseInfo.afOctaveBands[j];
         //if (fSum)
         //   fSum = (fp)SROCTAVE / fSum;

         int iVolumeNorm;
         iVolumeNorm = VolumeNormalize (&m_DisguiseInfo, dwUnvoiced);
         //aiVolumeNorm[1] = VolumeNormalize (&m_DisguiseInfo, 1);

         for (j = 0; j < SROCTAVE; j++) {
            afAvgOctave[dwUnvoiced][j] += m_DisguiseInfo.afOctaveBands[dwUnvoiced][j] * fSum * (fp)(MINENTRIES-dwNum);
            afAvgPeak[dwUnvoiced][j] += m_DisguiseInfo.afEmphasizePeaks[dwUnvoiced][j] * (fp)(MINENTRIES-dwNum);
            afAvgScale[dwUnvoiced][j] += (fp)((int)m_DisguiseInfo.abVolume[dwUnvoiced][j] + iVolumeNorm) * (fp)(MINENTRIES-dwNum);
            //afAvgScale[1][j] += (fp)((int)m_DisguiseInfo.abVolume[1][j] + aiVolumeNorm[1]) * (fp)(MINENTRIES-dwNum);
         } // j
      } // dwUnvoiced
   }

   // figure out scale
   fp fScale = 1.0 / (fp) max(MINENTRIES, dwNum);
   for (dwUnvoiced = 0; dwUnvoiced < 2; dwUnvoiced++)
      for (j = 0; j < SROCTAVE; j++) {
         pPDNoise->DI.afOctaveBands[dwUnvoiced][j] = pPDGarbage->DI.afOctaveBands[dwUnvoiced][j] =
            afAvgOctave[dwUnvoiced][j] * fScale;
         pPDNoise->DI.afEmphasizePeaks[dwUnvoiced][j] = pPDGarbage->DI.afEmphasizePeaks[dwUnvoiced][j] =
            afAvgPeak[dwUnvoiced][j] * fScale;

         fp f = floor(afAvgScale[dwUnvoiced][j] * fScale + 0.5);
         f = min(f, SRMAXLOUDNESS);
         f = max(f, SRABSOLUTESILENCE);
         pPDNoise->DI.abVolume[dwUnvoiced][j] = pPDGarbage->DI.abVolume[dwUnvoiced][j] = (char)(int)f;

         //f = floor(afAvgScale[1][j] * fScale + 0.5);
         //f = min(f, SRMAXLOUDNESS);
         //f = max(f, SRABSOLUTESILENCE);
         //pPDNoise->DI.abVolume[1][j] = pPDGarbage->DI.abVolume[1][j] = (char)(int)f;
      } // j

   // fill in the noise energy levels
   // since dB already zeroed, just calc energy
   fp fEnergy = SRFEATUREEnergySmall (TRUE, &pPDNoise->srfSmall, FALSE, TRUE);
   SRFEATUREScale (&pPDNoise->srfSmall, PHONESAMPLENORMALIZED / fEnergy);

   // fill in the garbage energy levels
   if (dwNum) {
      for (j = 0; j < SRDATAPOINTSSMALL; j++) {
         pPDGarbage->srfSmall.acVoiceEnergyMain[j] = AmplitudeToDb (afGarbage[0][0][j] / (fp)dwNum);
         pPDGarbage->srfSmall.acNoiseEnergyMain[j] = AmplitudeToDb (afGarbage[0][1][j] / (fp)dwNum);
         pPDGarbage->srfSmall.acVoiceEnergyDelta[j] = AmplitudeToDb (afGarbage[1][0][j] / (fp)dwNum);
         pPDGarbage->srfSmall.acNoiseEnergyDelta[j] = AmplitudeToDb (afGarbage[1][1][j] / (fp)dwNum);
      } // j
      fEnergy = SRFEATUREEnergySmall (TRUE, &pPDGarbage->srfSmall, FALSE, TRUE);
      if (fEnergy)
         SRFEATUREScale (&pPDGarbage->srfSmall, PHONESAMPLENORMALIZED / fEnergy);
   }
   else
      // dont have any information to make up garbage, so use noise
      memcpy (&pPDGarbage->srfSmall, &pPDNoise->srfSmall, sizeof(pPDNoise->srfSmall));
}



/*************************************************************************************
CVoiceDisguise::ApplyDISGUISEINFOToWave - Given an array of DISGUISEINFO
(from DetermineDISGUISEINFOForWave()) and SRFEATUREs, this modifies the
SRFEATUREs according to the disguise info.

inputs
   DWORD          dwNum - Number of features and disguiseinfo
   PCM3DWave      pWave - Wave used. Gets m_dwSRSAMPLESPERSEC from here
   PSRFEATURE     pasrf - Array of dwNum SRFEATUREs to modify
   PSRDETAILEDPHASE  paSRDP - Detailed phase to use. Must have dwNum entries
   PWVPITCH       paPitch - Array of dwNum pitch to modify.
   PDISGUISEINFO  paDI - Array of disguise info to use. They are blended left & right
                     somewhat to minimize abrupt changes
   PDISGUISEINFO  pDISum - If this is not NULL, then pDISum->afOctaveBands() are increased as they're used
                     so can use for psola. Values must have already been initialized
returns
   none
*/
void CVoiceDisguise::ApplyDISGUISEINFOToWave (DWORD dwNum, PCM3DWave pWave,
                                              PSRFEATURE pasrf, PSRDETAILEDPHASE paSRDP, PWVPITCH paPitch, PDISGUISEINFO paDI,
                                              PDISGUISEINFO pDISum)
{
   DWORD i, j, dwUnvoiced;
   int iWindow;
   DISGUISEINFO di;
   SRFEATURE srf;
   SRDETAILEDPHASE sdp;
   fp afAvgVolume[2][SROCTAVE];
   for (i = 0; i < dwNum; i++) {
      // sero
      memset (&di, 0, sizeof(di));
      memset (afAvgVolume, 0, sizeof(afAvgVolume));
      fp fCount = 0;

#define AVERAGEDISTB     ((int)(pWave->m_dwSRSAMPLESPERSEC/10))  // average score 2 units away to minimize jumpiness

      for (iWindow = -AVERAGEDISTB; iWindow <= AVERAGEDISTB; iWindow++) {
         int iLoc = iWindow + (int)i;
         if ((iLoc < 0) || (iLoc >= (int)dwNum))
            continue;   // out of range

         fp fScore = AVERAGEDISTB + 1 - abs(iWindow);
         fCount += fScore;
         for (dwUnvoiced = 0; dwUnvoiced < 2; dwUnvoiced++)
            for (j = 0; j < SROCTAVE; j++) {
               di.afOctaveBands[dwUnvoiced][j] += paDI[iLoc].afOctaveBands[dwUnvoiced][j] * fScore;
                  // ok to average these since will have been normalized by DetermineDISGUISEINFOForWave()
               di.afEmphasizePeaks[dwUnvoiced][j] += paDI[iLoc].afEmphasizePeaks[dwUnvoiced][j] * fScore;
               afAvgVolume[dwUnvoiced][j] += (fp)(int)paDI[iLoc].abVolume[dwUnvoiced][j] * fScore;
               //afAvgVolume[1][j] += (fp)(int)paDI[iLoc].abVolume[1][j] * fScore;
                  // likewise, volume should have been normalized
            } // j
      } // iWindow
      fCount = 1.0 / fCount;

      // scale
      for (dwUnvoiced = 0; dwUnvoiced < 2; dwUnvoiced++)
         for (j = 0; j < SROCTAVE; j++) {
            di.afOctaveBands[dwUnvoiced][j] *= fCount;
            di.afEmphasizePeaks[dwUnvoiced][j] *= fCount;
            di.abVolume[dwUnvoiced][j] = (char)(int)floor(afAvgVolume[dwUnvoiced][j] * fCount + 0.5);
            //di.abVolume[1][j] = (char)(int)floor(afAvgVolume[1][j] * fCount + 0.5);

            // to keep track so can apply to PSOLA
            if (pDISum)
               pDISum->afOctaveBands[dwUnvoiced][j] += di.afOctaveBands[dwUnvoiced][j];
         } // j

      // apply the change
      srf = pasrf[i];
      sdp = paSRDP[i];
      paPitch[i].fFreq = ModifySRFEATURE (&srf, &sdp, pasrf+i, paSRDP + i, paPitch[i].fFreq, &di, TRUE);
   } // i
}

// BUGFIX - Getting internal compiler error. Try to work around
// #pragma optimize ("", off)
/*************************************************************************************
CVoiceDisguise::DetermineDISGUISEINFOForWave - This accepts an array of SRFEATURE
from a wave, and figures out the best diguise info to use for each SRFEATURE.

inputs
   DWORD          dwNum - Number of SRFEATUREs
   PCM3DWavw      pWave - Get m_dwSRSAMPLESPERSEC from
   PSRFEATURE     psrf - SRFEATUREs from the wave
   PWVPHONEME     paWVPHONEME - Phonemes from the wave, if they exist. Or NULL.
   DWORD          dwNumWVPHONEME - Number of phonemes in the wave
   DWORD          dwFeatureSize - Number of samples per feature
   DWORD          dwPhonemeOffset - Offset of the first feature into the phonemes (if skipped some)
   PCListFixed    plDISGUISEINFO - Initialized to sizeof(DISGUISEINFO) and filled with
                  an array of DISGUISEINFO structures, one for each feature
returns
   BOOL - TRUE if success
*/
BOOL CVoiceDisguise::DetermineDISGUISEINFOForWave (DWORD dwNum, PCM3DWave pWave, PSRFEATURE psrf,
                                                   PWVPHONEME paWVPHONEME, DWORD dwNumWVPHONEME,
                                                   DWORD dwFeatureSize, DWORD dwPhonemeOffset,
                                                   PCListFixed plDISGUISEINFO)
{
   plDISGUISEINFO->Init (sizeof(DISGUISEINFO));

   // make sure the energies are calculated for all the phoneme disguises
   DWORD i;
   PPHONEMEDISGUISE pPD = (PPHONEMEDISGUISE) m_lPHONEMEDISGUISE.Get(0);
   DWORD dwNumDisguise = m_lPHONEMEDISGUISE.Num();
   fp fEnergy;
   CListFixed lEnergy;
   lEnergy.Init (sizeof(fp));
   lEnergy.Required (dwNumDisguise+2);
   for (i = 0; i < dwNumDisguise; i++) {
      SRFEATUREConvert (&pPD[i].srfOrig, i ? &pPD[i-1].srfOrig : NULL, &pPD[i].srfSmall);
      fEnergy = SRFEATUREEnergySmall (TRUE, &pPD[i].srfSmall, FALSE, TRUE);
      if (!fEnergy) {
         // make noise
         memset (&pPD[i].srfSmall, 0, sizeof(SRFEATURESMALL));
         fEnergy = SRFEATUREEnergySmall (TRUE, &pPD[i].srfSmall, FALSE, TRUE);
      }
      SRFEATUREScale (&pPD[i].srfSmall, PHONESAMPLENORMALIZED / fEnergy);

      // store this energy
      fEnergy = SRFEATUREEnergySmall (TRUE, &pPD[i].srfSmall, FALSE, TRUE);
      lEnergy.Add (&fEnergy);
   } // i

   // also calculate noise and garbage
   PHONEMEDISGUISE pdNoise, pdGarbage;
   CreateNoiseAndGarbage (dwNumDisguise, pPD, &pdNoise, &pdGarbage);
   DWORD dwNumDisguiseInList = dwNumDisguise;
   dwNumDisguise += 2;  // two more to disguise

   // store the energy for noise and garbage
   fEnergy = SRFEATUREEnergySmall (TRUE, &pdNoise.srfSmall, FALSE, TRUE);
   lEnergy.Add (&fEnergy);
   fEnergy = SRFEATUREEnergySmall (TRUE, &pdGarbage.srfSmall, FALSE, TRUE);
   lEnergy.Add (&fEnergy);

   // loop through all the disguised phonemes and see how well they match
   CMem memEnergy;
   if (!memEnergy.Required (dwNum * dwNumDisguise * sizeof(fp)))
      return FALSE;
   fp *pfEnergy = (fp*)memEnergy.p;
   fp *pafEnergyCalc = (fp*)lEnergy.Get(0);
   DWORD j;
   SRFEATURESMALL srfSmall;
   fp afSmall[2][2][SRDATAPOINTSSMALL];
   if (dwNumDisguiseInList) for (i = 0; i < dwNum; i++) {
      // average neighboring features so less noise involved
      int iWindow;
      fp fCount = 0;
      memset (afSmall, 0, sizeof(afSmall));
      for (iWindow = -NEIGHBORFEAT; iWindow <= NEIGHBORFEAT; iWindow++) {
         int iLook = (int)i + iWindow;
         if ((iLook < 0) || (iLook >= (int)dwNum))
            continue;

         fp fWeight = NEIGHBORFEAT + 1 - abs(iWindow);
         fCount += fWeight;

         // convert and average
         SRFEATUREConvert (psrf + iLook, iLook ? &psrf[iLook-1] : NULL, &srfSmall);
         for (j = 0; j < SRDATAPOINTSSMALL; j++) {
            afSmall[0][0][j] += DbToAmplitude (srfSmall.acVoiceEnergyMain[j]) * fWeight;
            afSmall[0][1][j] += DbToAmplitude (srfSmall.acNoiseEnergyMain[j]) * fWeight;
            afSmall[1][0][j] += DbToAmplitude (srfSmall.acVoiceEnergyDelta[j]) * fWeight;
            afSmall[1][1][j] += DbToAmplitude (srfSmall.acNoiseEnergyDelta[j]) * fWeight;
         } // j
      } // iWindow
      fCount = 1.0 / fCount;  // to couteract scaling
      for (j = 0; j < SRDATAPOINTSSMALL; j++) {
         srfSmall.acVoiceEnergyMain[j] = AmplitudeToDb (afSmall[0][0][j] * fCount);
         srfSmall.acNoiseEnergyMain[j] = AmplitudeToDb (afSmall[0][1][j] * fCount);
         srfSmall.acVoiceEnergyDelta[j] = AmplitudeToDb (afSmall[1][0][j] * fCount);
         srfSmall.acNoiseEnergyDelta[j] = AmplitudeToDb (afSmall[1][1][j] * fCount);
      } // j

      // convert to small and get enerfy
      fEnergy = SRFEATUREEnergySmall (TRUE, &srfSmall, FALSE, TRUE);
      if (!fEnergy) {
         // make noise
         memset (&srfSmall, 0, sizeof(SRFEATURESMALL));
         fEnergy = SRFEATUREEnergySmall (TRUE, &srfSmall, FALSE, TRUE);
      }
      SRFEATUREScale (&srfSmall, PHONESAMPLENORMALIZED / fEnergy);
      fEnergy = SRFEATUREEnergySmall (TRUE, &srfSmall, FALSE, TRUE);   // so have it again for more accurate

      for (j = 0; j < dwNumDisguise; j++) {
         PPHONEMEDISGUISE pd;
         if (j < dwNumDisguiseInList)
            pd = pPD + j;
         else if (j == dwNumDisguiseInList)
            pd = &pdNoise;
         else
            pd = &pdGarbage;

         // energy difference?
         pfEnergy[i*dwNumDisguise+j] = 1.0 - SRFEATURECompareSmall (TRUE, &srfSmall, fEnergy, &pd->srfSmall, pafEnergyCalc[j]);
            // do 1.0 - so that best match will be 1.0
      } // j
   } // i

   // now, loop through and determine which are the best features
#define TOPN            2  // how many to keep
#define AVERAGEDIST     ((int)(pWave->m_dwSRSAMPLESPERSEC/10))  // average score 2 units away to minimize jumpiness
   fp afBest[TOPN];  // scores of the best
   DWORD adwBest[TOPN];  // index for the best
   DWORD dwKept, k;
   fp afScale[2][SROCTAVE];
   int iAverage;
   DISGUISEINFO di;
   PWVPHONEME pCur = NULL;
   PWVPHONEME pNext = dwNumWVPHONEME ? &paWVPHONEME[0] : NULL;
   int iCurPhoneme = -1;
   DWORD dwSample = dwPhonemeOffset;
   CListFixed lValidPhones;
   lValidPhones.Init (sizeof(DWORD));
   for (i = 0; i < dwNum; i++, dwSample += dwFeatureSize) {
      fp *pafCur = pfEnergy + (i * dwNumDisguise);
      dwKept = 0;

      // see about jumping to the next phoneme
      while (pNext) {
         if (dwSample < pNext->dwSample)
            break;   // still on the current one

         // else, advanced
         pCur = pNext;
         iCurPhoneme++;
         if (iCurPhoneme+1 < (int)dwNumWVPHONEME)
            pNext++;
                  // BUGFIX - Added +1 to iCurPhoneme because was crashing
         else
            pNext = NULL;  // no more

         // set the list of valid phonemes based on this
         lValidPhones.Init (sizeof(DWORD));
         WCHAR szPhone[16];
         szPhone[sizeof(pCur->awcNameLong)/sizeof(WCHAR)] = 0;
         memcpy (szPhone, pCur->awcNameLong, sizeof(pCur->awcNameLong));
         for (j = 0; szPhone[j]; j++)
            if ((szPhone[j] >= L'0') && (szPhone[j] <= L'9'))
               break;
         szPhone[j] = 0;   // to get rid of digits
         DWORD dwLen = (DWORD)wcslen (szPhone);

         // run through all the phonemes in the list and add them
         // if their name matches
         for (j = 0; j < dwNumDisguiseInList; j++) {
            if (_wcsnicmp (szPhone, pPD[j].szName, dwLen))
               continue;   // no match

            // else, match IF there's a number or a NULL
            if (!pPD[j].szName[dwLen] || ((pPD[j].szName[dwLen] >= L'0') && (pPD[j].szName[dwLen] <= L'9')))
               lValidPhones.Add (&j);
         } // j

      } // i
      DWORD *padwValidPhones = (DWORD*)lValidPhones.Get(0);
      DWORD dwNumValidPhones = lValidPhones.Num();

      // if no phonemes then trivial
      if (!dwNumDisguiseInList) {
         afBest[0] = 1;
         afBest[1] = 0;
         adwBest[0] = 0;
         adwBest[1] = 1;
         dwKept = 2;
      }
      else for (j = 0; j < dwNumDisguise; j++) {
         fp fScore = 0;
         fp fCount = 0;
         for (iAverage = -AVERAGEDIST; iAverage <= AVERAGEDIST; iAverage++) {
            int iLook = iAverage + (int)i;
            if ((iLook < 0) || (iLook >= (int)dwNum))
               continue;   // out of range
            int iScale = AVERAGEDIST + 1 - abs(iAverage);
            fScore += pafCur[iAverage * (int)dwNumDisguise + (int)j] * (fp)iScale;
            fCount += (fp)iScale;
         } // iAverage
         fScore /= fCount;

         // if there are and valid phones, then see if this matches any
         // if it doesn't then score=0
         if (dwNumValidPhones) {
            for (k = 0; k < dwNumValidPhones; k++)
               if (padwValidPhones[k] == j)
                  break;
            if (k >= dwNumValidPhones)
               fScore = 0; // not on the valid list, so 0 score
         }

         // see if matches
         if (!dwKept) {
            // empty, so fill in
            afBest[0] = fScore;
            adwBest[0] = j;
            dwKept++;
            continue;
         }

         // loop down, and find to insert before
         for (k = dwKept-1; k < dwKept; k--)
            if (fScore <= afBest[k])
               break; // insert after this
         k++;  // convert insert after to insert before
         if (k >= TOPN)
            continue;   // too big
         if (dwKept >= TOPN)
            dwKept = TOPN-1;   // forget that had the lowest one
         memmove (afBest + (k+1), afBest + k, (dwKept - k) * sizeof(afBest[0]));
         memmove (adwBest + (k+1), adwBest + k, (dwKept - k) * sizeof(adwBest[0]));
         afBest[k] = fScore;
         adwBest[k] = j;
         dwKept++;
      } // j, over all diguises

      // there will be at least 2 best (silence and garbage), and maybe more...
      // take the delta between the best since that affects the weighting
      // of final
      fp fSum = 0;
      dwKept--;
      for (j = 0; j < dwKept; j++) {
         afBest[j] = afBest[j] - afBest[dwKept];
         if (!afBest[j])
            afBest[j] = EPSILON;   // at least some change
         fSum += afBest[j];
      }

      // normalize to one
      fSum = 1.0 / fSum;   // wont be 0 because used epsilon
      fSum /= 2.0; // BUGFIX - Make them sum to 1/2 so can average in garbage's (aka: total average) weight
      for (j = 0; j < dwKept; j++)
         afBest[j] *= fSum;
      // add in garbage
      afBest[dwKept] = 0.5;
      adwBest[dwKept] = dwNumDisguiseInList+1;
      dwKept++;

      // produce the averaged info
      memset (&di, 0, sizeof(di));
      memset (afScale, 0, sizeof(afScale));

#if 0 // def _DEBUG
      OutputDebugString ("\r\n");
#endif
      DWORD dwUnvoiced;
      // loop throug all the best ones
      for (j = 0; j < dwKept; j++) {
         PPHONEMEDISGUISE pd;
         if (adwBest[j] < dwNumDisguiseInList)
            pd = pPD + adwBest[j];
         else if (adwBest[j] == dwNumDisguiseInList)
            pd = &pdNoise;
         else
            pd = &pdGarbage;

#if 0 // def _DEBUG
         WCHAR szTemp[64];
         swprintf (szTemp, L"%s (%.2f) ", pd->szName, (double)afBest[j]);
         OutputDebugStringW (szTemp);
#endif


         // average in to octave bands
         for (dwUnvoiced = 0; dwUnvoiced < 2; dwUnvoiced++) {
            // determine the sum of the octaves
            fSum = OctaveNormalize (&pd->DI, dwUnvoiced);
            // below code is replaced by above
            //fSum = 0;
            //for (k = 0; k < SROCTAVE; k++)
            //   fSum += pd->DI.afOctaveBands[k];
            //if (fSum)
            //   fSum = (fp)SROCTAVE / fSum;

            int iVolumeNorm;
            iVolumeNorm = VolumeNormalize (&pd->DI, dwUnvoiced);
            //aiVolumeNorm[1] = VolumeNormalize (&pd->DI, 1);

            for (k = 0; k < SROCTAVE; k++) {
               di.afOctaveBands[dwUnvoiced][k] += pd->DI.afOctaveBands[dwUnvoiced][k] * fSum * afBest[j];
               di.afEmphasizePeaks[dwUnvoiced][k] += pd->DI.afEmphasizePeaks[dwUnvoiced][k] * afBest[j];
               afScale[dwUnvoiced][k] += (fp)((int)pd->DI.abVolume[dwUnvoiced][k] + iVolumeNorm) * afBest[j];
               //afScale[1][k] += (fp)((int)pd->DI.abVolume[1][k] + aiVolumeNorm[1]) * afBest[j];
            } // k
         } // dwUnvoiced
      } // j

      // fill dB scale
      for (k = 0; k < SROCTAVE; k++) {
         fp f = floor(afScale[0][k] + 0.5);
         f = min(f, SRMAXLOUDNESS);
         f = max(f, SRABSOLUTESILENCE);
         di.abVolume[0][k] = (char)(int)f;

         f = floor(afScale[1][k] + 0.5);
         f = min(f, SRMAXLOUDNESS);
         f = max(f, SRABSOLUTESILENCE);
         di.abVolume[1][k] = (char)(int)f;
      } // j

      // add
      plDISGUISEINFO->Add (&di);
   } // i

   // done
   return TRUE;
}
// #pragma optimize ("", on)


static PWSTR gpszPitchOrig = L"PitchOrig";
static PWSTR gpszPitchScale = L"PitchScale";
static PWSTR gpszPitchVariationScale = L"PitchVariationScale";
static PWSTR gpszPitchMusical = L"PitchMusical";
static PWSTR gpszWaveBasePitch = L"WaveBasePitch";
static PWSTR gpszNonIntHarmonics = L"NonIntHarmonics";
static PWSTR gpszVoicedToUnvoiced = L"VoicedToUnvoiced";
static PWSTR gpszOverallVolume = L"OverallVolume";
static PWSTR gpszDisguiseInfo = L"DisguiseInfo";
static PWSTR gpszPhonemeDisguise = L"PhonemeDisguise2";  // BUGFIX - Changed name so wont have problems when reload with new DISGUISEINFO structure
static PWSTR gpszWaveBase = L"WaveBase";
static PWSTR gpszVoiceDisguise = L"VoiceDisguise";
static PWSTR gpszAgreeNoOffensive = L"AgreeNoOffensive";

/*************************************************************************************
CVoiceDisguise::MMLTo - Standard API
*/
PCMMLNode2 CVoiceDisguise::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszVoiceDisguise);

   MMLValueSet (pNode, gpszPitchOrig, m_fPitchOrig);
   MMLValueSet (pNode, gpszPitchScale, m_fPitchScale);
   MMLValueSet (pNode, gpszPitchVariationScale, m_fPitchVariationScale);
   MMLValueSet (pNode, gpszPitchMusical, (int)m_fPitchMusical);
   MMLValueSet (pNode, gpszAgreeNoOffensive, (int)m_fAgreeNoOffensive);
   MMLValueSet (pNode, gpszWaveBasePitch, m_fWaveBasePitch);
   MMLValueSet (pNode, gpszNonIntHarmonics, m_fNonIntHarmonics);
   MMLValueSet (pNode, gpszVoicedToUnvoiced, (PBYTE)&m_afVoicedToUnvoiced[0], sizeof(m_afVoicedToUnvoiced));
   MMLValueSet (pNode, gpszOverallVolume, (PBYTE)&m_abOverallVolume[0], sizeof(m_abOverallVolume));
   MMLValueSet (pNode, gpszDisguiseInfo, (PBYTE)&m_DisguiseInfo, sizeof(m_DisguiseInfo));
   if (m_lPHONEMEDISGUISE.Num()) // BUGFIX - so no 0-length
      MMLValueSet (pNode, gpszPhonemeDisguise, (PBYTE)m_lPHONEMEDISGUISE.Get(0), m_lPHONEMEDISGUISE.Num()*sizeof(PHONEMEDISGUISE));
   PWSTR psz = (PWSTR)m_memWaveBase.p;
   if (psz[0])
      MMLValueSet (pNode, gpszWaveBase, psz);

   // NOTE: ignoring mixes

   return pNode;
}


/*************************************************************************************
CVoiceDisguise::MMLFrom - Standard API
*/
BOOL CVoiceDisguise::MMLFrom (PCMMLNode2 pNode)
{
   // clear out. also releases wave cache
   Clear ();

   m_fPitchOrig = MMLValueGetDouble (pNode, gpszPitchOrig, m_fPitchOrig);
   m_fPitchScale = MMLValueGetDouble (pNode, gpszPitchScale, m_fPitchScale);
   m_fPitchVariationScale = MMLValueGetDouble (pNode, gpszPitchVariationScale, m_fPitchVariationScale);
   m_fPitchMusical = (BOOL) MMLValueGetInt (pNode, gpszPitchMusical, (int)m_fPitchMusical);
   m_fAgreeNoOffensive = (BOOL) MMLValueGetInt (pNode, gpszAgreeNoOffensive, (int)m_fAgreeNoOffensive);
   m_fWaveBasePitch = MMLValueGetDouble (pNode, gpszWaveBasePitch, m_fWaveBasePitch);
   m_fNonIntHarmonics = MMLValueGetDouble (pNode, gpszNonIntHarmonics, m_fNonIntHarmonics);
   MMLValueGetBinary (pNode, gpszVoicedToUnvoiced, (PBYTE)&m_afVoicedToUnvoiced[0], sizeof(m_afVoicedToUnvoiced));
   MMLValueGetBinary (pNode, gpszOverallVolume, (PBYTE)&m_abOverallVolume[0], sizeof(m_abOverallVolume));

   // So can load in old versions
   size_t dwSize;
   DWORD i;
   dwSize = MMLValueGetBinary (pNode, gpszDisguiseInfo, (PBYTE)&m_DisguiseInfo, sizeof(m_DisguiseInfo));
   if (dwSize == sizeof(DISGUISEINFOOLD)) {
      DISGUISEINFOOLD old;
      memcpy (&old, &m_DisguiseInfo, sizeof(old));
      for (i = 0; i < SROCTAVE; i++) {
         m_DisguiseInfo.abVolume[0][i] = old.abVolume[0][1];
         m_DisguiseInfo.abVolume[1][i] = old.abVolume[1][1];

         m_DisguiseInfo.afEmphasizePeaks[0][i] = old.afEmphasizePeaks[i];
         m_DisguiseInfo.afEmphasizePeaks[1][i] = 1.0;

         m_DisguiseInfo.afOctaveBands[0][i] = old.afOctaveBands[i];
         m_DisguiseInfo.afOctaveBands[1][i] = 1.0;
      } // i
   }

   CMem mem;
   dwSize = MMLValueGetBinary (pNode, gpszPhonemeDisguise, &mem);
   m_lPHONEMEDISGUISE.Init (sizeof(PHONEMEDISGUISE), mem.p, (DWORD)dwSize / sizeof(PHONEMEDISGUISE));

   PWSTR psz = MMLValueGet (pNode, gpszWaveBase);
   MemZero (&m_memWaveBase);
   if (psz)
      MemCat (&m_memWaveBase, psz);

   // NOTE: ignoring mixes

   return TRUE;
}


/*************************************************************************************
CVoiceDisguise::Load - Load information in from a file (providing dialog box)

inputs
   HWND        hWnd - Window to pull up dialog on
returns
   BOOL - TRUE if loaded in new info, FALSE if not
*/
BOOL CVoiceDisguise::Load (HWND hWnd)
{
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   memset (&ofn, 0, sizeof(ofn));
   
   // BUGFIX - Set directory
   char szInitial[256];
   strcpy (szInitial, gszAppDir);
   GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "Voice disguise settings (*.vds)\0*.vds\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = "Open voice disguise settings";
   ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = "vds";
   // nFileExtension 

   if (!GetOpenFileName(&ofn))
      return FALSE;

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // try opening
   WCHAR szw[256];
   MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, szw, sizeof(szw)/sizeof(WCHAR));

   PCMMLNode2 pNode = MMLFileOpen (szw, &GUID_SynthParam, NULL, FALSE, TRUE /* bypass global megafile */);
   if (!pNode)
      return FALSE;

   // dont allow load to bypass the agreement
   BOOL fAgree = m_fAgreeNoOffensive;
   if (!MMLFrom (pNode)) {
      m_fAgreeNoOffensive = fAgree;
      delete pNode;
      return FALSE;
   }
   m_fAgreeNoOffensive = fAgree;

   delete pNode;
   return TRUE;
}




/*************************************************************************************
CVoiceDisguise::Save - Saves information in from a file (providing dialog box)

inputs
   HWND        hWnd - Window to pull up dialog on
returns
   BOOL - TRUE if saved in new info, FALSE if not
*/
BOOL CVoiceDisguise::Save (HWND hWnd)
{
   // save UI
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   memset (&ofn, 0, sizeof(ofn));

   // BUGFIX - Set directory
   char szInitial[256];
   GetLastDirectory(szInitial, sizeof(szInitial));

   ofn.lpstrInitialDir = szInitial;
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "Voice disguise settings (*.vds)\0*.vds\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = "Save voice disguise file";
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = "vds";
   // nFileExtension 
   if (!GetSaveFileName(&ofn))
      return TRUE;   // user pressed cancel, but dont display an error

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // try opening
   WCHAR szw[256];
   MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, szw, sizeof(szw)/sizeof(WCHAR));

   PCMMLNode2 pNode = MMLTo();
   if (!pNode)
      return FALSE;
   BOOL fRet;
   fRet = MMLFileSave (szw, &GUID_SynthParam, pNode, 1, NULL, TRUE /* bypass global megagile*/);
   delete pNode;
   return fRet;
}




/****************************************************************************
VoiceSynSettingsPage
*/
static BOOL VoiceSynSettingsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceDisguise pv = (PCVoiceDisguise) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!_wcsicmp(psz, L"test")) {
            if (!pv->m_pWaveTestOriginal)
               return TRUE;

            if (pv->m_pWaveTest) {
               pv->m_pWaveTest->QuickPlayStop();
               delete pv->m_pWaveTest;
            }
            pv->m_pWaveTest = pv->m_pWaveTestOriginal->Clone();
            if (!pv->m_pWaveTest)
               return TRUE;

            CProgress Progress;
            Progress.Start (pPage->m_pWindow->m_hWnd, "Generating voice...", TRUE);
            pv->SynthesizeFromSRFeature (4, pv->m_pWaveTest, NULL, 0, NULL, NULL, NULL, TRUE, &Progress);
            pv->m_pWaveTest->QuickPlay();
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Voice synthesis settings";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/*********************************************************************************
CVoiceDisguise::DialogModifyWave - Pulls up a dialog that lets the user modify
a wave in the wave editor.

inputs
   PCEscWindow       pWindow - Window to display the settings in
   PCM3DWave         pWave - Wave to use as a sample wave to modify and play
returns
   DWORD - 2 if user press OK (which means do synthesis, but not actually
            done by this call), 1 if pressed back,
            0 if pressed cancel
*/
DWORD CVoiceDisguise::DialogModifyWave (PCEscWindow pWindow, PCM3DWave pWave)
{
   PWSTR pszRet;

   // make a copy of the wave so can modify
   pWave->CalcSRFeaturesIfNeeded(WAVECALC_VOICECHAT, pWindow->m_hWnd);
   pWave->CalcPitchIfNeeded (WAVECALC_VOICECHAT, pWindow->m_hWnd);
   m_pWaveTestOriginal = pWave;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLVOICESYNSETTINGS, VoiceSynSettingsPage, this);

   if (m_pWaveTest) {
      delete m_pWaveTest;
      m_pWaveTest = NULL;
   }

   if (!pszRet)
      return 0;
   if (!_wcsicmp(pszRet, L"disguise")) {
      BOOL fRet = Dialog (pWindow, NULL, 0, NULL, 0x01);
      if (fRet)
         goto redo;
      else
         return 0;   // cancel
   }

   if (!_wcsicmp(pszRet, L"doeffect"))
      return 2;
   if (!_wcsicmp(pszRet, Back()))
      return 1;
   return 0;   // some other cancel
}


/*********************************************************************************
CVoiceDisguise::MixTogether - Mixes a number of voice disguises together into
this voice disguise.

inputs
   DWORD          dwNum - Number
   PCVoiceDisguise *papVD - Pointer to an array of dwNum voice disguises.
                     These objects (except the first one) should remain valid
                     until the voice disguise is freed, since they will
                     be referenced
   fp             *pafWeight - Pointer to an array of dwNum voice disuise weights. Should
                  sum to 1.0.
returns
   BOOL - TRUE if success
*/
BOOL CVoiceDisguise::MixTogether (DWORD dwNum, PCVoiceDisguise *papVD, fp *pafWeight)
{
   // clone the first one to this
   if (!papVD[0]->CloneTo (this))
      return FALSE;

   if (dwNum < 2)
      return TRUE;   // quick exit

   // scale the current scores by the weight
   DWORD i, j;
   fp afOverallVolume[2];
   fp fScale = pafWeight[0];  // assuming that everything sums to 1
   m_fPitchOrig *= fScale;
   m_fPitchScale *= fScale;
   m_fPitchVariationScale *= fScale;
   m_fNonIntHarmonics *= fScale;
   for (j = 0; j < SROCTAVE; j++)
      m_afVoicedToUnvoiced[j] *= fScale;
   for (j = 0; j < 2; j++)
      afOverallVolume[j] = DbToAmplitude (m_abOverallVolume[j]) * fScale;

   // loop over all the others
   for (i = 1; i < dwNum; i++) {
      fScale = pafWeight[i];
      m_fPitchOrig += papVD[i]->m_fPitchOrig * fScale;
      m_fPitchScale += papVD[i]->m_fPitchScale * fScale;
      m_fPitchVariationScale += papVD[i]->m_fPitchVariationScale * fScale;
      m_fNonIntHarmonics += papVD[i]->m_fNonIntHarmonics * fScale;
      for (j = 0; j < SROCTAVE; j++)
         m_afVoicedToUnvoiced[j] += papVD[i]->m_afVoicedToUnvoiced[j] * fScale;
      for (j = 0; j < 2; j++)
         afOverallVolume[j] += DbToAmplitude (papVD[i]->m_abOverallVolume[j]) * fScale;
   } // i

   // fill in overall volume
   for (j = 0; j < 2; j++)
      m_abOverallVolume[j] = AmplitudeToDb(afOverallVolume[j]);

   // remember these so can average blends together
   m_lMixPCVoiceDisguise.Init (sizeof(PCVoiceDisguise), papVD+1, dwNum-1);
   m_lMixWeight.Init (sizeof(fp), pafWeight, dwNum);  // remember this weight too

   return TRUE;
}

// BUGBUG - eventually need setup to install some sample files that people can
// disguise their voice to

