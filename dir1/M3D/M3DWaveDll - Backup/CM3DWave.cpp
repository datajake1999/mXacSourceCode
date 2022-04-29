/***************************************************************************
CM3DWave.cpp - C++ object for handling wave data.

begun 2/5/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <mmreg.h>
#include <msacm.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"


#define FEATUREHACKS          // Turn on hacks for extracting SRFEATURE

#ifdef FEATUREHACKS
#define FEATUREHACK_PHASEFROMONETRY    // get phases from best try, NOT combination of several. Do this since trying to
                                       // use all harmonics at once for FFT when resynthesizing
                                       // BUGFIX - have re-enabled

#define FEATUREHACK_RENORMALIZETOWAVE  // after have genrated SRFeatures, synthesize and then normalize according to original wave
#define FEATUREHACK_NARROWERFORMANT    // also apply rule to narrow formant
#define FEATUREHACK_SHARPENFORMANTSINFEAT    // sharpens the formants in the feature set
#define FEATUREHACK_UPSAMPLE           // upsample first
#define FEATUREHACK_NOISEAFFECTEDBYNEIGHBORS   // noise calculation affected by neighbors noise
#define FEATUREHACK_NORMALIZEVOLUME    // normalize left/and right volumes
#define FEATUREHACK_AVERAGEALLPHASES   // average all phases together
#define FEATUREHACK_SUMPHASEOVERWAVELENGTH   // sum features over attempts together so more stable
#define FEATUREHACK_AVERAGEALL         // average amplitudes over time
#define FEATUREHACK_NOISETOVOICE       // convert noise to voice
#define FEATUREHACK_ALLNOISEORALLVOICE // modifies the harmonics to produce all noise or all voice
#define FEATUREHACK_REDUCEQUIET        // reduce quiet areas to silence
#define FEATUREHACK_REDUCEQUIETSCALE   // scaled reduce quiet based on energy of spectrum. Must have FEATUREHACK_REDUCEQUIET on
#define FEATUREHACK_FINETUNEPITCH      // fine-tune pitch
#define FEATUREHACK_PITCHANALYSISADJUSTVOLUME   // pitch anaylysis adjusts for volume
#define FEATUREHACK_BLURPHASEDETAILED  // blur phase using detailed phase
// disabled because don't need now (added FEATUREHACK_ALLNOISEORALLVOICE), and causing problems with noise converted to voice #define FEATUREHACK_NOISEATHIGHFREQ    // encourage noise at high frequencies
// disabled because uneven volume adjust per voice #define FEATUREHACK_SCALEENERGYTOFFT   // recscale the energy to FFT, in case made errors
// disabled because doesn't work well #define FEATUREHACK_SECONDPITCHDETECT     // second pitch detect
// disabled becayse doesn't work well #define FEATUREHACK_SHARPENFORMANTSINFFT    // sharpens the formants
#endif

#ifdef FEATUREHACK_PHASEFROMONETRY
// Need to turn off averaging of phases
#undef FEATUREHACK_AVERAGEALLPHASES
#undef FEATUREHACK_SUMPHASEOVERWAVELENGTH
#endif

// #ifdef FEATUREHACK_NOISEATHIGHFREQ
// #define ENCOURAGENOISE(noise,freq)     ((noise) * (fp)(freq) / (fp)SRDATAPOINTS * 2.0)
   // noise = noise energy, freq = 0..SRDATAPOINTS-1, returns weighted
// #else
// #define ENCOURAGENOISE(noise,freq)     (noise)
// #endif

static PCM3DWave gpWaveQuickPlay = NULL;        // wave that is currently quickplaying

#define FCC_WAVE              mmioFOURCC('W', 'A', 'V', 'E')
#define FCC_FMT               mmioFOURCC('f', 'm', 't', ' ')
#define FCC_PHONEME           mmioFOURCC('m', 'p', 'h', '3')
#define FCC_PHONEMEOLD2       mmioFOURCC('m', 'p', 'h', '2')
#define FCC_PHONEMEOLD        mmioFOURCC('m', 'p', 'h', 'n')
#define FCC_WORDSEG           mmioFOURCC('m', 'w', 'r', 'd')
#define FCC_SPOKEN            mmioFOURCC('m', 's', 'k', 'n')
#define FCC_SPEAKER           mmioFOURCC('m', 's', 'p', 'k')
#define FCC_SOUNDSLIKE        mmioFOURCC('m', 's', 'n', 'l')
#define FCC_DATA              mmioFOURCC('d', 'a', 't', 'a')

#ifdef SRFEATUREINCLUDEPCM

#ifdef SRFEATUREINCLUDEPCM_SHORT
#define FCC_SRFEATURE         ((m_dwSRSAMPLESPERSEC == 200) ? mmioFOURCC('m', 's', 'f', '6') : mmioFOURCC('m', 's', 'f', '6'))   // BUGFIX - Updated SR features so change file
#else
#define FCC_SRFEATURE         ((m_dwSRSAMPLESPERSEC == 200) ? mmioFOURCC('m', 's', 'f', '5') : mmioFOURCC('m', 's', 'f', '6'))   // BUGFIX - Updated SR features so change file
#endif

#define FCC_SRFEATUREOLD         ((m_dwSRSAMPLESPERSEC == 200) ? mmioFOURCC('m', 's', 'f', '2') : mmioFOURCC('m', 's', 'f', '4'))   // BUGFIX - Updated SR features so change file
#else
#define FCC_SRFEATURE         ((m_dwSRSAMPLESPERSEC == 200) ? mmioFOURCC('m', 's', 'f', '2') : mmioFOURCC('m', 's', 'f', '4'))   // BUGFIX - Updated SR features so change file
#endif

#define FCC_PITCH             ((m_dwSRSAMPLESPERSEC == 200) ? mmioFOURCC('m', 'p', 't', '2') : mmioFOURCC('m', 'p', 't', 'c'))
#define FCC_PITCHSUB           ((m_dwSRSAMPLESPERSEC == 200) ? mmioFOURCC('m', 'p', 's', '2') : mmioFOURCC('m', 'p', 's', 'u'))
      // BUGFIX - If SAMPLESPERSEC set to 200 then use different RIFF chunk


#define  RECORDBUF         8        // number of record buffers
#define  RECORDWIDTH       300      // number of pixels across for scope
#define  RECORDHEIGHT      200      // number of pixels UD for scope

#define CALCSRFEATUREPCMMAX      (SRFEATUREPCM*4)     // maximum number that can have

typedef struct {
   PCM3DWave         pM3DWave;     // wave object
   PCEscPage         pPage;         // page that's displaying it
   HWAVEIN           hWaveIn;       // recording from
   WAVEHDR           aWaveHdr[RECORDBUF]; // wave headers
   DWORD             dwWaveOut;     // number of wave headers out and recording
   BOOL              fStopped;      // set to TRUE when stopped and exiting
   BOOL              fRecording;    // set to TRUE if recording
   BOOL              fClippedDuringRecord;    // set to TRUE if clipped during record
   HBITMAP           hBit;          // bitmap displaying the wave
   HWND              hWndDC;        // window to get the DC from
   short             asCache[RECORDWIDTH];   // cache of last bitmap drawn
   DWORD             dwWindowSize;  // for FFT window
   float             *pfWindow;     // window
   float             *pfFFTScratch; // scratch for fft
   PCListVariable    plAudio;       // store audio in
   BOOL              fTrimSilence;  // set to TRUE if should trim silence
   BOOL              fNormalize;    // set to TRUE if should normalize
   BOOL              fNoiseReduce;  // set to TRUE if should noise reduce
   BOOL              fRemoveDC;     // set to TRUE if shoudl remove dc offset
   DWORD             dwReplace;     // 0 = selection, 1 = entire wave, 2 = add to end
   BOOL              fShowReplaceButtons; // if TRUE show the replace buttonse, else dont
   CRITICAL_SECTION  csStopped;     // critical section arond stopped
   DWORD             dwTimerID;     // timer for recording
} RECORDINFO, *PRECORDINFO;

// PITCHHYP - Pitch hypothesis
#define PITCHHYPHISTORY       (SRSAMPLESPERSEC/2)    // around 1/2 of of a second
typedef struct {
   BYTE           abUsed[PITCHHYPHISTORY];      // 0 for none, 1 for highest energy band, 2 for second highest
   double         fScore;                       // score
   fp             fLastKnownPitch;              // last known pitch, or 0 if none
   DWORD          dwLastKnownPitchTime;         // number of ticks since last known pitch
} PITCHHYP, *PPITCHHYP;

// PITCHDETECT - Storing pitch detect information away
typedef struct {
   DWORD          dwSamplesPerSec;              // sampling rate
   DWORD          dwWindowSize;                 // window size
   PCListFixed    plPITCHDETECTVALUE;           // pitch detection values that have been calculated
} PITCHDETECT, *PPITCHDETECT;

// PITCHDETECTVALUE - Value of pitch detection
#define PITCHDETECTVALUEBULGEMAXHALF         16
#define PITCHDETECTVALUEBULGEMAX             (PITCHDETECTVALUEBULGEMAXHALF*2+1)
#define MAXPITCHCANFIND       600      // wont find anything higher than 600 hz
#define MINPITCHCANFIND       50       // lowest pitch can find
   // BUGFIX - Was 25, but way too low to actually detect, so causing error with new pitch detect
// BUGFIX - Put in STARTHARMONICS and MAXHARMONICS so would work better with low pitch sounds
#define STARTHARMONICS     5     // after 5 harmonics starts to rolloff
#define MAXHARMONICS       (STARTHARMONICS*2)   // after 10 harmonics 0
#define PITCHDETECTDELTA      (25)
#define PITCHDETECTMAXFREQ    (MAXPITCHCANFIND * 4)   // BUGFIX - Not using MAXHARMONICS since taking different approach
#define PITCHDETECTMAXHARMONICS  50    // maximum harmonics for new ptich detect
#define ANALYZEPITCHWINDOWSCALE     4  // how large a window to use

typedef struct {
   fp             fCenterFFT;             // index into Wave.FFT where should be centered
   fp             fBulgeSum;              // sum of the bulges
   fp             afBulge[PITCHDETECTVALUEBULGEMAX];     // values after passed in sin() * 32767. Center at PITCHDETECTVALUEBULGEMAXHALF
} PITCHDETECTVALUE, *PPITCHDETECTVALUE;

// ADPCMBLOCKHEADER - For storing a block of ADPCM
typedef struct {
   BYTE           bSamples;         // number of samples
   BYTE           bScale;           // scaling factor
} ADPCMBLOCKHEADER, *PADPCMBLOCKHEADER;

// ADPCMHEADER - Header info for ADPCM
typedef struct {
   DWORD          dwSamples;        // number of samples stored
   DWORD          dwBlocks;         // number of blocks
} ADPCMHEADER, *PADPCMHEADER;

static CRITICAL_SECTION gcsPitchDetect;         // critical section for pitch detect
static CListFixed    glPITCHDETECT;             // array of pitch detect info

void EnergyCalcPerOctave (float *pafVoiced, float *pafUnvoiced, float *pafEnergy);
void NarrowerFormants (float *pafVoiced, float *pafUnvoiced);

/****************************************************************************
PitchDetectCacheInit - Initialize the pitch detection information. Called
from DLLMain()
*/
void PitchDetectCacheInit (void)
{
   InitializeCriticalSection (&gcsPitchDetect);

   glPITCHDETECT.Init (sizeof(PITCHDETECT));
}

/****************************************************************************
PitchDetectCacheEnd - Frees the pitch detection information. Called
from DLLMain()
*/
void PitchDetectCacheEnd (void)
{
   PPITCHDETECT ppd = (PPITCHDETECT)glPITCHDETECT.Get(0);
   DWORD i;
   for (i = 0; i < glPITCHDETECT.Num(); i++, ppd++)
      if (ppd->plPITCHDETECTVALUE)
         delete ppd->plPITCHDETECTVALUE;

   DeleteCriticalSection (&gcsPitchDetect);
}


/****************************************************************************
PitchDetectGetPITCHDETECTVALUE - Get a list of pitch-detect values given
the window and samples per sec

inputs
   DWORD       dwSamplesPerSec - Sampling rate
   DWORD       dwWindowSize - Window size
   float       *pafWindow - Window, of dwWindowSize elements to use
returns
   PCListFixed - List of PITCHDETECTVALUE, or NULL if can't find. Do NOT delete
                  or change the contents
*/
PCListFixed PitchDetectGetPITCHDETECTVALUE (DWORD dwSamplesPerSec, DWORD dwWindowSize, float *pafWindow)
{
   PCListFixed plRet;

   EnterCriticalSection (&gcsPitchDetect);
   DWORD i;
   PPITCHDETECT ppd = (PPITCHDETECT)glPITCHDETECT.Get(0);
   for (i = 0; i < glPITCHDETECT.Num(); i++, ppd++)
      if ((ppd->dwSamplesPerSec == dwSamplesPerSec) && (ppd->dwWindowSize == dwWindowSize)) {
         plRet = ppd->plPITCHDETECTVALUE;
         LeaveCriticalSection (&gcsPitchDetect);
         return plRet;
      }

   // else, need to create
   PCListFixed plPDV = new CListFixed;
   CMem memFFTScratch, memFFT;
   CSinLUT SinLUT;
   if (!plPDV || !memFFT.Required (dwWindowSize / 2 * sizeof(float)) ) {
      if (plPDV)
         delete plPDV;
      LeaveCriticalSection (&gcsPitchDetect);
      return NULL;
   }
   plPDV->Init (sizeof(PITCHDETECTVALUE));

   fp fPitch;
   float *pafFFT = (float*)memFFT.p;
   CM3DWave Wave;
   Wave.ConvertSamplesAndChannels (dwSamplesPerSec, 1, NULL);
   if (!Wave.BlankWaveToSize (dwWindowSize, TRUE)) {
      delete plPDV;
      LeaveCriticalSection (&gcsPitchDetect);
      return NULL;
   }
   PITCHDETECTVALUE pdv;
   memset (&pdv, 0, sizeof(pdv));
   for (fPitch = MINPITCHCANFIND; fPitch < PITCHDETECTMAXFREQ; fPitch += PITCHDETECTDELTA) {
      fp fWavelength = (fp)dwSamplesPerSec / fPitch;

      for (i = 0; i < dwWindowSize; i++)
         Wave.m_psWave[i] = (short)(sin((fp)i / fWavelength * PI * 2.0) * 32767.0);

      // fo the FFT
      Wave.FFT (dwWindowSize, (int)dwWindowSize/2, 0, pafWindow, pafFFT, &SinLUT,
         &memFFTScratch, NULL, 0);

      // figure out the expected center
      pdv.fCenterFFT = (fp)dwWindowSize / fWavelength;
      pdv.fBulgeSum = 0;
      pafFFT[0] = 0; // so DC offset doesn't translate
      for (i = 0; i < PITCHDETECTVALUEBULGEMAX; i++) {
         fp fOffset = pdv.fCenterFFT - (fp)PITCHDETECTVALUEBULGEMAXHALF + (fp)i;
         fOffset = max(fOffset, 0);
         fOffset = min(fOffset, (fp)dwWindowSize/2.0);

         DWORD dwLeft = min((DWORD)fOffset, dwWindowSize/2-1);
         DWORD dwRight = min(dwLeft+1, dwWindowSize/2-1);
         fOffset -= (fp)dwLeft;

         pdv.afBulge[i] = (1.0 - fOffset) * pafFFT[dwLeft] + fOffset * pafFFT[dwRight];
         pdv.fBulgeSum += pdv.afBulge[i];
      } // i

      // add
      plPDV->Add (&pdv);

   } // fPitch

   // add this
   PITCHDETECT pd;
   memset (&pd, 0, sizeof(pd));
   pd.dwSamplesPerSec = dwSamplesPerSec;
   pd.dwWindowSize = dwWindowSize;
   pd.plPITCHDETECTVALUE = plPDV;
   glPITCHDETECT.Add (&pd);

   LeaveCriticalSection (&gcsPitchDetect);

   return plPDV;
}


/****************************************************************************
CM3DWave::Constructor and destructor
*/
CM3DWave::CM3DWave (void)
{
   m_dwSRSAMPLESPERSEC = SRSAMPLESPERSEC;

   m_hWaveOut = NULL;
   m_pmemWaveOut = NULL;
   m_dwQuickPlayTimer = 0;
   m_fQuickPlayDone = FALSE;

   m_dwSRFeatChunkStart = 0;
   m_dwSRFeatChunkSize =0;
   memset (m_apmemSRFeatTemp, 0, sizeof(m_apmemSRFeatTemp));
   memset (m_adwSRFeatTemp, 0, sizeof(m_adwSRFeatTemp));
   m_pmemSR = NULL;

   m_pmemWave = NULL;
   m_pAnimWave = NULL;
   m_pUserData = NULL;
   m_lWVPHONEME.Init (sizeof(WVPHONEME));
   New ();
}

CM3DWave::~CM3DWave (void)
{
   // stop playing
   QuickPlayStop();

   // free up existing info
   ReleaseWave();
}


/****************************************************************************
CM3DWave::New - Frees up the existing data (does NOT check for m_fDirty) and
fills it with a new wave

inputs
   DWORD          dwSamplesPerSec - Sampling rate
   DWORD          dwChannels - Channels
   DWORD          dwSamples - Samples to start out with. NOTE: The memory is left
                  unitialized
*/
BOOL CM3DWave::New (DWORD dwSamplesPerSec, DWORD dwChannels, DWORD dwSamples)
{
   // free up existing data
   ReleaseWave ();

   // text
   MemZero (&m_memSpoken);
   DefaultSpeakerGet (&m_memSpeaker);
   
   m_szFile[0] = 0;
   m_fDirty = FALSE;
   m_dwSamplesPerSec = dwSamplesPerSec;
   m_dwSamples = dwSamples;
   m_dwChannels = dwChannels;
   m_lWVPHONEME.Clear();
   m_lWVWORD.Clear();

   // BUGFIX - Make sure these are zeroed out
   m_dwSRSkip = 0;
   memset (m_adwPitchSkip, 0, sizeof(m_adwPitchSkip));

   WAVEFORMATEX wfex;
   memset (&wfex, 0, sizeof(wfex));
   wfex.cbSize = 0;
   wfex.wFormatTag = WAVE_FORMAT_PCM;
   wfex.nChannels = m_dwChannels;
   wfex.nSamplesPerSec = m_dwSamplesPerSec;
   wfex.wBitsPerSample = 16;
   wfex.nBlockAlign  = wfex.nChannels * wfex.wBitsPerSample / 8;
   wfex.nAvgBytesPerSec = wfex.nBlockAlign * wfex.nSamplesPerSec;
   if (!m_memWFEX.Required (sizeof(wfex)))
      return FALSE;
   memcpy (m_memWFEX.p, &wfex, sizeof(wfex));

   m_pmemWave = new CMem;
   if (!m_pmemWave)
      return FALSE;
   DWORD dwNeed;
   dwNeed = dwSamples * m_dwChannels * sizeof(short);
   if (!m_pmemWave->Required (dwNeed))
      return FALSE;
   m_psWave = (short*) m_pmemWave->p;
   // dont init: memset (m_pszWave, 0, dwNeed);

   return TRUE;
}


/****************************************************************************
CM3DWave::PhonemeRemoveDup - Internal function that removes duplicate phonemes
occurring one after the other.

inputs
   BOOL     fRemoveOnlySilence - If TRUE then remove only duplicate silence.
            If false then remove all duplicates.
*/
void CM3DWave::PhonemeRemoveDup (BOOL fRemoveOnlySilence)
{
   DWORD i;
   PCWSTR pszSilence = MLexiconEnglishPhoneSilence();
   PWVPHONEME pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
   for (i = m_lWVPHONEME.Num() - 1; i && (i < m_lWVPHONEME.Num()); i--) {
      // if smae phoneme remove
      if (!_wcsnicmp(pp[i].awcNameLong, pp[i-1].awcNameLong, sizeof(pp[i].awcNameLong)/sizeof(WCHAR))) {
         // make sure silence
         if (!fRemoveOnlySilence || !_wcsnicmp(pp[i].awcNameLong, pszSilence, wcslen(pszSilence))) {
            m_lWVPHONEME.Remove(i);
            pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
            // BUGFIX - Dont do: i--;
            continue;
         }
      }

      if (pp[i].dwSample <= pp[i-1].dwSample) {
         // happening at the same time
         m_lWVPHONEME.Remove(i-1);
         pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
         // BUGFIX - Dont to: i--;
         continue;
      }
   } // i

   // if the first phoneme is silence then remove that since redundant
   if (m_lWVPHONEME.Num() && !_wcsnicmp (pp[0].awcNameLong, pszSilence, sizeof(pp[0].awcNameLong)/sizeof(WCHAR)))
      m_lWVPHONEME.Remove(0);

}


/****************************************************************************
CM3DWave::PhonemeDelete - Internal function that deletes the phonemes between
time start and time end, moving all phonemes after time end up.

inputs
   DWORD          dwStart - start time
   DWORD          dwEnd - end time
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::PhonemeDelete (DWORD dwStart, DWORD dwEnd)
{
   DWORD i;
   PWVPHONEME pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
   for (i = 0; i < m_lWVPHONEME.Num(); i++) {
      if (pp[i].dwSample <= dwStart)
         continue; // too low
      if (pp[i].dwSample > dwEnd) {
         // just decrease location
         pp[i].dwSample -= (dwEnd - dwStart);
         continue;
      }

      // else, deleting the section that contains the phoneme, so se it's time to dwStart
      pp[i].dwSample = dwStart;

      // if this time matches the last time then get rid of the previous one
      if (i && (pp[i].dwSample <= pp[i-1].dwSample)) {
         m_lWVPHONEME.Remove (i-1);
         i--;
         pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
         continue;
      }
   } // i

   // get rid of duplicates
   PhonemeRemoveDup ();

   return TRUE;
}

/****************************************************************************
CM3DWave::PhonemeInsert - Inserts a number of phonemes.

inputs
   DWORD          dwInsert - Intertion time
   DWORD          dwTime - Amount of time to insert
   DWORD          dwFrom - Subtract this time from the phoneems in plWVPHONEMEFrom
   PCListFixed    plWVPHONEMEFrom - List to get phonemes from
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::PhonemeInsert (DWORD dwInsert, DWORD dwTime, DWORD dwFrom, PCListFixed plWVPHONEMEFrom)
{
   // if there are no phonemes make a silenct phoneme
   WVPHONEME wp;
   memset (&wp, 0, sizeof(wp));
   if (!m_lWVPHONEME.Num()) {
      PCWSTR pszSilence = MLexiconEnglishPhoneSilence();
      memcpy (wp.awcNameLong, pszSilence, min(wcslen(pszSilence)*sizeof(WCHAR),sizeof(wp.awcNameLong)));
      wp.dwSample = 0;
      m_lWVPHONEME.Add (&wp);
   }

   // move time points
   DWORD i, dwInsertBefore;
   PWVPHONEME pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
   dwInsertBefore = -1;
   for (i = 0; i < m_lWVPHONEME.Num(); i++) {
      if (pp[i].dwSample < dwInsert)
         continue;

      // else, after
      pp[i].dwSample += dwTime;

      // remember this if it's the first sample to insert before
      if (dwInsertBefore == -1)
         dwInsertBefore = i;
   }

   // duplicate the phoneme just before dwInsert before to dwInsert before so
   // that will get phonemes split
   if ((dwInsertBefore != -1) && dwInsertBefore) {
      // dont worry about duplicates because they'll be removed
      wp = pp[dwInsertBefore-1];
      wp.dwSample = dwInsert + dwTime;
      m_lWVPHONEME.Insert (dwInsertBefore, &wp);
   }
   else if (dwInsertBefore == 0) {
      // inserting before silence
      memcpy (wp.awcNameLong, MLexiconEnglishPhoneSilence(), sizeof(wp.awcNameLong));
      wp.dwSample = dwInsert + dwTime;
      m_lWVPHONEME.Insert (dwInsertBefore, &wp);
   }
   else {   // dwInsertBefore=-1
      // adding onto the end, so need to put silence afterwards
      memcpy (wp.awcNameLong, MLexiconEnglishPhoneSilence(), sizeof(wp.awcNameLong));
      wp.dwSample = dwInsert + dwTime;
      m_lWVPHONEME.Add (&wp);
      dwInsertBefore = m_lWVPHONEME.Num()-1;
   }

   // insert a silence before dwInsertBefore because that is implied
   memcpy (wp.awcNameLong, MLexiconEnglishPhoneSilence(), sizeof(wp.awcNameLong));
   wp.dwSample = dwInsert;
   if (dwInsertBefore != -1) {
      m_lWVPHONEME.Insert (dwInsertBefore, &wp);
      dwInsertBefore++; // so inserting after silnece
   }
   else
      // must be inserting adding onto the end, so add silence at insertion point
      m_lWVPHONEME.Add (&wp);

   // insert all the phonemes
   pp = (PWVPHONEME) plWVPHONEMEFrom->Get(0);
   for (i = 0; i < plWVPHONEMEFrom->Num(); i++) {
      wp = pp[i];
      if (wp.dwSample < dwFrom)
         wp.dwSample = 0;   // copying before this, so scrunch up in beginning
      else
         wp.dwSample -= dwFrom;
      if (wp.dwSample >= dwTime)
         break;   // occurs after the time want to insert, so might as well stop here
      wp.dwSample += dwInsert;

      if (dwInsertBefore != -1) {
         m_lWVPHONEME.Insert (dwInsertBefore, &wp);
         dwInsertBefore++; // so inserting after silnece
      }
      else
         m_lWVPHONEME.Add (&wp);
   }

   // finally
   PhonemeRemoveDup();
   return TRUE;
}

/****************************************************************************
CM3DWave::PhonemeStretch - Stretches the phoneme time up/down by the
given percent

inputs
   double         fStretch - if 1.0 no change, else 2.0 double the time, etc.
returns
   none
*/
void CM3DWave::PhonemeStretch (double fStretch)
{
   if (fStretch == 1.0)
      return;  // no change
   DWORD i;
   PWVPHONEME pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
   for (i = 0; i < m_lWVPHONEME.Num(); i++) {
      pp[i].dwSample = (DWORD)((double)pp[i].dwSample * fStretch);
   }

   // finally
   PhonemeRemoveDup();
}


/****************************************************************************
CM3DWave::DefaultWFEXGet - Reads the default WFEX from the registry.

inputs
   PWAVEFORMATEX     pwfex - Filled with wfex.
   DWORD             dwSize - Size of pwfex buffer. Should be reasonably large
                        so can contain extra compression information (save 500-1000bytes)
returns
   none
*/
static char gszKeyWFEXDefault[] = "WFEXDefault";
void CM3DWave::DefaultWFEXGet (PWAVEFORMATEX pwfex, DWORD dwSize)
{
   DWORD dwKey;
   dwKey = 0;

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      goto fromdefault;

   DWORD dw, dwType;
   LONG lRet;
   dw = dwSize;
   lRet = RegQueryValueEx (hKey, gszKeyWFEXDefault, NULL, &dwType, (LPBYTE) pwfex, &dw);
   RegCloseKey (hKey);

   if ((lRet == ERROR_SUCCESS) && (dw >= sizeof(WAVEFORMATEX)) && (dwType == REG_BINARY))
      return;

fromdefault:
   // else, set up default
   memset (pwfex, 0, sizeof(*pwfex));
   pwfex->wFormatTag = WAVE_FORMAT_PCM;
   pwfex->nChannels = 1;
   pwfex->nSamplesPerSec = 22050;
   pwfex->wBitsPerSample = 16;
   pwfex->nBlockAlign  = pwfex->nChannels * pwfex->wBitsPerSample / 8;
   pwfex->nAvgBytesPerSec = pwfex->nBlockAlign * pwfex->nSamplesPerSec;
}


/****************************************************************************
CM3DWave::DefaultWFEXSet - Sets the default WFEX in the registry

inputs
   PWAVEFORMATEX     pwfex - To Write out
*/
void CM3DWave::DefaultWFEXSet (PWAVEFORMATEX pwfex)
{
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   RegSetValueEx (hKey, gszKeyWFEXDefault, 0, REG_BINARY, (BYTE*) pwfex,
      sizeof(WAVEFORMATEX) + pwfex->cbSize);

   RegCloseKey (hKey);
}

/****************************************************************************
CM3DWave::DefaultSpeakerGet - Reads the default Speaker from the registry.

inputs
   PCMem       pMem - Memory to receive the default speaker string (unicode)
returns
   none
*/
static char gszKeySpeakerDefault[] = "SpeakerDefault";
void CM3DWave::DefaultSpeakerGet (PCMem pMem)
{
   DWORD dwKey;
   dwKey = 0;
   MemZero (pMem);

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   DWORD dw, dwType;
   LONG lRet;
   char szTemp[256];
   WCHAR szw[256];
   szTemp[0] = 0;
   dw = sizeof(szTemp);
   lRet = RegQueryValueEx (hKey, gszKeySpeakerDefault, NULL, &dwType, (LPBYTE) szTemp, &dw);
   RegCloseKey (hKey);

   if ((lRet == ERROR_SUCCESS) && (dwType == REG_SZ)) {
      MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw));
      MemCat (pMem, szw);
   }
   // else just leave blank
}


/****************************************************************************
CM3DWave::DefaultSpeakerSet - Sets the default Speaker in the registry

inputs
   PCMem       pMem -memory contianing default string
*/
void CM3DWave::DefaultSpeakerSet (PCMem pMem)
{
   DWORD dwLen = (DWORD)wcslen((PWSTR) pMem->p);
   char szTemp[256];
   if (dwLen > sizeof(szTemp)/2-1)
      return;  // too long
   WideCharToMultiByte (CP_ACP, 0, (PWSTR)pMem->p, -1, szTemp, sizeof(szTemp), 0, 0);

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   RegSetValueEx (hKey, gszKeySpeakerDefault, 0, REG_SZ, (BYTE*) szTemp,
      (DWORD)strlen(szTemp)+1);

   RegCloseKey (hKey);
}

/****************************************************************************
CM3DWave::New - Like the other New() except that it creates based on
a WAVEFORMATEX.

inputs
   PWAVEFORMATEX     pwfex - Wave format to use. If NULL then read the default
      wfex from the registry
   DWORD             dwSamples - Samples
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::New (PWAVEFORMATEX pwfex, DWORD dwSamples)
{
   BYTE abTemp[1000];
   if (!pwfex) {
      pwfex = (PWAVEFORMATEX) &abTemp[0];
      DefaultWFEXGet (pwfex, sizeof(abTemp));
   }
   if (!New (pwfex->nSamplesPerSec, pwfex->nChannels, dwSamples))
      return FALSE;

   DWORD dwNeed;
   dwNeed = sizeof(WAVEFORMATEX) + pwfex->cbSize;
   if (!m_memWFEX.Required (dwNeed))
      return FALSE;
   memcpy (m_memWFEX.p, pwfex, dwNeed);

   return TRUE;
}


/****************************************************************************
CM3DWave::Clone - Duplicates the wave object and all its contents
*/
PCM3DWave CM3DWave::Clone (void)
{
   PCM3DWave pNew = Copy (0, m_dwSamples);
   if (!pNew)
      return NULL;

   pNew->m_dwSRSAMPLESPERSEC = m_dwSRSAMPLESPERSEC;

   // BUGFIX - also clone the SR features and whatnot so dont need to recalculate
   if (m_dwEnergySamples && pNew->m_memEnergy.Required (m_dwEnergySamples * m_dwChannels * sizeof(WORD))) {
      pNew->m_dwEnergySamples = m_dwEnergySamples;
      pNew->m_dwEnergySkip = m_dwEnergySkip;
      pNew->m_pwEnergy = (WORD*) pNew->m_memEnergy.p;
      memcpy (pNew->m_pwEnergy, m_pwEnergy, m_dwEnergySamples * m_dwChannels * sizeof(WORD));
   }
   DWORD dwPitchSub;
   for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++)
      if (m_adwPitchSamples[dwPitchSub] && pNew->m_amemPitch[dwPitchSub].Required (m_adwPitchSamples[dwPitchSub] * m_dwChannels * sizeof(WVPITCH))) {
         pNew->m_adwPitchSamples[dwPitchSub] = m_adwPitchSamples[dwPitchSub];
         pNew->m_adwPitchSkip[dwPitchSub] = m_adwPitchSkip[dwPitchSub];
         pNew->m_afPitchMaxStrength[dwPitchSub] = m_afPitchMaxStrength[dwPitchSub];
         pNew->m_apPitch[dwPitchSub] = (WVPITCH*)pNew->m_amemPitch[dwPitchSub].p;
         memcpy (pNew->m_apPitch[dwPitchSub], m_apPitch[dwPitchSub], m_adwPitchSamples[dwPitchSub] * m_dwChannels * sizeof(WVPITCH));
      }
   if (m_pmemSR) {
      if (!pNew->m_pmemSR)
         pNew->m_pmemSR = new CMem;
      if (!pNew->m_pmemSR) {
         delete pNew;
         return FALSE;
      }
      if (m_dwSRSamples && pNew->m_pmemSR->Required (m_dwSRSamples *sizeof (SRFEATURE))) {
         pNew->m_paSRFeature = (PSRFEATURE)pNew->m_pmemSR->p;
         memcpy (pNew->m_paSRFeature, m_paSRFeature, m_dwSRSamples *sizeof (SRFEATURE));
      }
   }
   else {
      if (pNew->m_pmemSR)
         delete pNew->m_pmemSR;
      pNew->m_pmemSR = NULL;
      pNew->m_paSRFeature = NULL;
   }
   pNew->m_dwSRSamples = m_dwSRSamples;
   pNew->m_dwSRSkip = m_dwSRSkip;

   return pNew;
}

/****************************************************************************
CM3DWave::RequireWave - This checks to make sure the wave data is loaded.
If it isn't then the wave data is loaded from the file. If it cant be loaded
then an error is returned.

inputs
   PCProgressSocket        pProgress - Used if the wave data is loaded
*/
BOOL CM3DWave::RequireWave (PCProgressSocket pProgress)
{
   if (m_pmemWave)
      return TRUE;

   // else try and load
   char szFile[256];
   strcpy (szFile, m_szFile);
   return Open (pProgress, szFile, TRUE);
}


/****************************************************************************
CM3DWave::ReleaseCalc - Release calculated information, such as the energy
or pitch calculated throughout the wave.
*/
void CM3DWave::ReleaseCalc (void)
{
   // free up wave used for animation
   if (m_pAnimWave) {
      delete m_pAnimWave;
      m_pAnimWave = NULL;
   }

   // clear energy
   m_dwEnergySamples = 0;
   m_dwEnergySkip = 0;
   m_pwEnergy = NULL;

   // clear pitch
   memset (m_adwPitchSamples, 0, sizeof(m_adwPitchSamples));
   memset (m_adwPitchSkip, 0, sizeof(m_adwPitchSkip));
   memset (m_apPitch, 0, sizeof(m_apPitch));

   m_dwSRSamples = m_dwSRSkip = 0;
   m_paSRFeature = NULL;
   ReleaseSRFeatures(); // BUGFIX - make sure not leaking
}


/****************************************************************************
CM3DWave::ReleaseSRFeatures - Release the SR features.

returns
   BOOL - TRUE if there was SR data, FALSE if not
*/
BOOL CM3DWave::ReleaseSRFeatures (void)
{
   DWORD i;
   for (i = 0; i < SRFEATCACHE; i++)
      if (m_apmemSRFeatTemp[i]) {
         delete m_apmemSRFeatTemp[i];
         m_apmemSRFeatTemp[i] = NULL;
      }

   if (!m_pmemSR)
      return FALSE;

   delete m_pmemSR;
   m_paSRFeature = NULL;
   m_pmemSR = NULL;
   return TRUE;
}


/****************************************************************************
CM3DWave::CacheSRFeatures - This temporarily fills in some memory with some
feature location info (or uses the main feature info) if it's there.

inputs
   DWORD       dwStart - Start feature number
   DWORD       dwEnd - End feature number (exclusive)
returns
   PSRFEATURE - Cached SRFEATURE starting at dwStart. This memory will
               become invalid when CacheSRFeatures() is called again.
*/
PSRFEATURE CM3DWave::CacheSRFeatures (DWORD dwStart, DWORD dwEnd)
{
   if ((dwStart > dwEnd) || (dwEnd > m_dwSRSamples))
      return NULL; // error

   // if already have memory then use that
   if (m_paSRFeature)
      return m_paSRFeature + dwStart;

   // else, if already cached internally use that
   DWORD i;
   for (i = 0; i < SRFEATCACHE; i++)
      if (m_apmemSRFeatTemp[i] && (dwStart >= m_adwSRFeatTemp[i]) &&
         (m_adwSRFeatTemp[i] + m_apmemSRFeatTemp[i]->m_dwCurPosn/sizeof(SRFEATURE) >= dwEnd)) {

            // move this to the top
            if (i >= 5) {
               PCMem pmemTemp = m_apmemSRFeatTemp[i];
               DWORD dwTemp = m_adwSRFeatTemp[i];
               memmove (m_apmemSRFeatTemp + 1, m_apmemSRFeatTemp, sizeof(m_apmemSRFeatTemp[0])*i);
               memmove (m_adwSRFeatTemp + 1, m_adwSRFeatTemp, sizeof(m_adwSRFeatTemp[0])*i);
               m_apmemSRFeatTemp[0] = pmemTemp;
               m_adwSRFeatTemp[0] = dwTemp;
               i = 0;
            }
            return (PSRFEATURE)m_apmemSRFeatTemp[i]->p + (dwStart - m_adwSRFeatTemp[i]);
         }

   // if not SR features in file then error
   if (!m_dwSRFeatChunkStart)
      return NULL;

   // if get here, couldn't find feature, so waste and old one and add this
   if (m_apmemSRFeatTemp[SRFEATCACHE-1])
      delete m_apmemSRFeatTemp[SRFEATCACHE-1];
   memmove (m_apmemSRFeatTemp + 1, m_apmemSRFeatTemp, sizeof(m_apmemSRFeatTemp[0])*(SRFEATCACHE-1));
   memmove (m_adwSRFeatTemp + 1, m_adwSRFeatTemp, sizeof(m_adwSRFeatTemp[0])*(SRFEATCACHE-1));

   // else, load
   m_apmemSRFeatTemp[0] = new CMem;
   if (!m_apmemSRFeatTemp[0])
      return NULL;

   DWORD dwNeed = (dwEnd - dwStart) * sizeof(SRFEATURE);
   HMMIO hmmio = NULL;
   if (!m_apmemSRFeatTemp[0]->Required(dwNeed))
      goto error;
   m_apmemSRFeatTemp[0]->m_dwCurPosn = dwNeed;
   m_adwSRFeatTemp[0] = dwStart;

   // read file
   hmmio = mmioOpen (m_szFile, NULL, MMIO_READ );
   if (!hmmio)
      goto error;
   mmioSeek (hmmio, m_dwSRFeatChunkStart + dwStart * sizeof(SRFEATURE), SEEK_SET);
   mmioRead (hmmio, (HPSTR) m_apmemSRFeatTemp[0]->p, dwNeed);
   mmioClose (hmmio, 0);

   return (PSRFEATURE) m_apmemSRFeatTemp[0]->p;

error:
   delete m_apmemSRFeatTemp[0];
   m_apmemSRFeatTemp[0] = NULL;

   if (hmmio)
      mmioClose (hmmio, 0);
   return NULL;
}


/****************************************************************************
CM3DWave::ReleaseWave - Frees up the loaded wave data. Do this is just needed
the wave data loaded for a bit, but dont wish to hog up memory.

NOTE: Also frees up the srfeatures, etc.

returns
   BOOL - TRUE if there was wave data, FALSE if there wasnt any
*/
BOOL CM3DWave::ReleaseWave (void)
{
   ReleaseCalc (); // while at it free up the calculated info

   if (!m_pmemWave)
      return FALSE;

   delete m_pmemWave;
   m_pmemWave = NULL;
   m_psWave = NULL;
   return TRUE;
}

/****************************************************************************
CM3DWave::Open - Opens a file and reads in all the data. If there is an existing
file then that one is freed (unless the dirty flag is set, in which case an error
is returned.)

inputs
   PCProgressSocket        pProgress - Used to show percent complete. THis can be NULL
   char                    *pszFile - File to open with. The name will be copied to m_szFile
   BOOL                    fLoadWave - If TRUE the the wave is loaded, else it isn't and only the info is loaded
   HMMIO                   hmmio - If not-NULL then this is used to load the data from instead of
                           the file name.
   DWORD                   dwForceSamplesPerSec - If not 0, then force the sampling
                           rate to this. This force will cause the acm to do up/down sampling.
   PCProgressWaveSample    pProgressWave - This is a more accurate load progress that is
                           used so can dynamically play wave while loading. If NULL not called
returns
   BOOL - TRUE if success
*/
#ifdef _DEBUG
// WVPHONEME - Phoneme information stores in wave
typedef struct {
   DWORD       dwSample;      // sample that the phoneme starts at
   CHAR        acName[4];     // phoneme name, followed by zero's, such as "ae\0\0"
} WVPHONEMEOLD, *PWVPHONEMEOLD;
#endif

BOOL CM3DWave::Open (PCProgressSocket pProgress, char* szFile, BOOL fLoadWave, HMMIO hmmio,
                     DWORD dwForceSamplesPerSec, PCProgressWaveSample pProgressWave)
{
   if (m_fDirty)
      return FALSE;

   m_dwSRFeatChunkStart = m_dwSRFeatChunkSize = 0;

   // free up existing info
   ReleaseWave();
   m_lWVPHONEME.Clear();
   m_lWVWORD.Clear();
   MemZero (&m_memSpeaker);
   MemZero (&m_memSpoken);

   // open header
   // use multimedia functions to open
   WAVEFORMATEX   *pWFEX = NULL;
   BOOL fOpened = FALSE;
   MMIOINFO mmio;
   PMEGAFILE pMegaFile = NULL;
   if (!hmmio) {
      if (MegaFileInUse()) {
         pMegaFile = MegaFileOpen (szFile);
         if (!pMegaFile)
            return FALSE;

         memset (&mmio, 0, sizeof(mmio));
         mmio.pchBuffer = (HPSTR) pMegaFile->pbMem;
         mmio.fccIOProc = FOURCC_MEM;
         mmio.cchBuffer = (DWORD) pMegaFile->iMemSize;
         hmmio = mmioOpen (NULL, &mmio, MMIO_READ);
      }
      else
         hmmio = mmioOpen (szFile, NULL, MMIO_READ );
      fOpened = TRUE;
   }
   if (!hmmio) {
      if (pMegaFile)
         MegaFileClose (pMegaFile);
      return FALSE;  // error
   }
   strcpy (m_szFile, szFile);

   // find the wave chunk
   MMCKINFO mmckinfoParent;
   MMCKINFO mmckinfoSubchunk;
   // Locate a "RIFF" chunk with a "WAVE" form type to make 
   // sure the file is a waveform-audio file. 
   mmckinfoParent.fccType = FCC_WAVE; 
   if (mmioDescend(hmmio, &mmckinfoParent, NULL, MMIO_FINDRIFF)) {
      if (fOpened)
         mmioClose (hmmio, 0);
      if (pMegaFile)
         MegaFileClose (pMegaFile);
      return FALSE;
   }

   // find the format chunk
   mmckinfoSubchunk.ckid = FCC_FMT; 
   if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK)) {
      if (fOpened)
         mmioClose (hmmio, 0);
      if (pMegaFile)
         MegaFileClose (pMegaFile);
      return FALSE;
   }


   // read in the data
   if (!m_memWFEX.Required (max(sizeof(WAVEFORMATEX), mmckinfoSubchunk.cksize))) {
      if (fOpened)
         mmioClose (hmmio, 0);
      if (pMegaFile)
         MegaFileClose (pMegaFile);
      return FALSE;
   }
   pWFEX = (PWAVEFORMATEX) m_memWFEX.p;
   memset (pWFEX, 0, sizeof(WAVEFORMATEX));  // in case using old style, smaller structures
   mmioRead (hmmio, (HPSTR) pWFEX, mmckinfoSubchunk.cksize);
   DWORD dwSamplesPerSecOrig = pWFEX->nSamplesPerSec;
   m_dwSamplesPerSec = pWFEX->nSamplesPerSec;
   m_dwChannels = pWFEX->nChannels;

   // if want to force sampling rate, but no real change then just set to 0
   if (dwForceSamplesPerSec && (dwForceSamplesPerSec == m_dwSamplesPerSec))
      dwForceSamplesPerSec = 0;
   // BUGFIX - If forcing samples per sec then do as post processing, otherwise ACM can't always do conversion
#ifdef OLDFORCE
   if (dwForceSamplesPerSec)
      m_dwSamplesPerSec = dwForceSamplesPerSec;
#endif

   // ascend
   mmioAscend(hmmio, &mmckinfoSubchunk, 0);

   // find the phoneme chunk
   long lSeek;
   lSeek = mmioSeek (hmmio, 0, SEEK_CUR);
   mmckinfoSubchunk.ckid = FCC_PHONEME; 
   if (!mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK)) {
      CMem memTemp;
      if (!memTemp.Required (mmckinfoSubchunk.cksize)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }
      mmioRead (hmmio, (HPSTR) memTemp.p, mmckinfoSubchunk.cksize);
      m_lWVPHONEME.Init (sizeof(WVPHONEME), memTemp.p, mmckinfoSubchunk.cksize /
         sizeof(WVPHONEME));
      mmioAscend(hmmio, &mmckinfoSubchunk, 0);
   }
   mmioSeek (hmmio, lSeek, SEEK_SET);

   // if there aren't any phonemes, then see out old ones
   mmckinfoSubchunk.ckid = FCC_PHONEMEOLD2; 
   if (!mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK)) {
      CMem memTemp;
      if (!memTemp.Required (mmckinfoSubchunk.cksize)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }
      mmioRead (hmmio, (HPSTR) memTemp.p, mmckinfoSubchunk.cksize);

      DWORD i;
      m_lWVPHONEME.Init (sizeof(WVPHONEME));
      PWVPHONEMEOLD2 pOld2 = (PWVPHONEMEOLD2) memTemp.p;
      WVPHONEME wp;
      memset (&wp, 0, sizeof(wp));
      for (i = 0; i < mmckinfoSubchunk.cksize / sizeof(WVPHONEMEOLD2); i++, pOld2++) {
         wp.dwEnglishPhone = pOld2->dwEnglishPhone;
         wp.dwSample = pOld2->dwSample;
         memcpy (wp.awcNameLong, pOld2->awcName, sizeof(pOld2->awcName));

         m_lWVPHONEME.Add (&wp);
      }

      mmioAscend(hmmio, &mmckinfoSubchunk, 0);
   }
   mmioSeek (hmmio, lSeek, SEEK_SET);

   // seek out th eold phonemes
#ifdef _DEBUG
   if (!m_lWVPHONEME.Num()) {
      lSeek = mmioSeek (hmmio, 0, SEEK_CUR);
      mmckinfoSubchunk.ckid = FCC_PHONEMEOLD; 
      if (!mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK)) {
         CMem memTemp;
         if (!memTemp.Required (mmckinfoSubchunk.cksize)) {
            if (fOpened)
               mmioClose (hmmio, 0);
            if (pMegaFile)
               MegaFileClose (pMegaFile);
            return FALSE;
         }
         mmioRead (hmmio, (HPSTR) memTemp.p, mmckinfoSubchunk.cksize);
         PWVPHONEMEOLD pwo = (PWVPHONEMEOLD) memTemp.p;

         DWORD i;
         WVPHONEME wp;
         memset (&wp, 0, sizeof(wp));
         for (i = 0; i < mmckinfoSubchunk.cksize / sizeof(WVPHONEMEOLD); i++, pwo++) {
            wp.dwSample = pwo->dwSample;
            wp.awcNameLong[0] = (WCHAR)(BYTE)pwo->acName[0];
            wp.awcNameLong[1] = (WCHAR)(BYTE)pwo->acName[1];
            wp.awcNameLong[2] = (WCHAR)(BYTE)pwo->acName[2];
            wp.awcNameLong[3] = (WCHAR)(BYTE)pwo->acName[3];
            wp.dwEnglishPhone = 0;
            m_lWVPHONEME.Add (&wp);
         } // i

         mmioAscend(hmmio, &mmckinfoSubchunk, 0);
      }
      mmioSeek (hmmio, lSeek, SEEK_SET);
   }
#endif

   // find word chunk
   lSeek = mmioSeek (hmmio, 0, SEEK_CUR);
   mmckinfoSubchunk.ckid = FCC_WORDSEG; 
   if (!mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK)) {
      CMem memTemp;
      if (!memTemp.Required (mmckinfoSubchunk.cksize)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }
      mmioRead (hmmio, (HPSTR) memTemp.p, mmckinfoSubchunk.cksize);

      m_lWVWORD.Clear();
      DWORD dwCur, i;
      for (dwCur = 0; dwCur+sizeof(WVWORD) < mmckinfoSubchunk.cksize; ) {
         PWVWORD pw = (PWVWORD) ((PBYTE)memTemp.p + dwCur);
         PWSTR psz = (PWSTR)(pw+1);

         BOOL fFoundNULL = FALSE;
         for (i = 0; dwCur + i*sizeof(WCHAR) + sizeof(WVWORD) < mmckinfoSubchunk.cksize; i++)
            if (!psz[i]) {
               fFoundNULL = TRUE;
               break;
            }
         if (!fFoundNULL)
            break;    // error
         i++;  // so include null

         // else, found
         m_lWVWORD.Add (pw, i * sizeof(WCHAR) + sizeof(WVWORD));
         dwCur += i*sizeof(WCHAR)+sizeof(WVWORD);
      } // dwCur

      mmioAscend(hmmio, &mmckinfoSubchunk, 0);
   }
   mmioSeek (hmmio, lSeek, SEEK_SET);


#ifdef SRFEATUREINCLUDEPCM
   // see if can load in old format
   lSeek = mmioSeek (hmmio, 0, SEEK_CUR);
   mmckinfoSubchunk.ckid = FCC_SRFEATUREOLD; 
   if (!dwForceSamplesPerSec && !mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK)) {
      if (!m_pmemSR)
         m_pmemSR = new CMem;
      DWORD dwFeatures = mmckinfoSubchunk.cksize / sizeof(SRFEATUREOLD);

      if (!m_pmemSR || !m_pmemSR->Required (dwFeatures * sizeof(SRFEATURE))) {
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }

      CMem memLoad;
      if (!memLoad.Required (mmckinfoSubchunk.cksize)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }
      PSRFEATUREOLD pSRFOld = (PSRFEATUREOLD)memLoad.p;

      m_dwSRSkip = m_dwSamplesPerSec / m_dwSRSAMPLESPERSEC;  // 1/100th of a second
      m_dwSRSamples = dwFeatures;
      m_paSRFeature = (PSRFEATURE) m_pmemSR->p;

      // set the location
      // dont do for back compat: m_dwSRFeatChunkStart = (DWORD) mmioSeek (hmmio, 0, SEEK_CUR);

      mmioRead (hmmio, (HPSTR) pSRFOld, mmckinfoSubchunk.cksize);

      DWORD i;
      memset (m_paSRFeature, 0, dwFeatures * sizeof(SRFEATURE));
      for (i = 0; i < m_dwSRSamples; i++)
         memcpy (m_paSRFeature + i, pSRFOld + i, sizeof(SRFEATUREOLD));

      // set the size
      // don't do for backwards compat: m_dwSRFeatChunkSize = (DWORD) mmioSeek (hmmio, 0, SEEK_CUR) - m_dwSRFeatChunkStart;

      mmioAscend(hmmio, &mmckinfoSubchunk, 0);
   }
   mmioSeek (hmmio, lSeek, SEEK_SET);
#endif // SRFEATUREINCLUDEPCM

   // srfeature
   lSeek = mmioSeek (hmmio, 0, SEEK_CUR);
   mmckinfoSubchunk.ckid = FCC_SRFEATURE; 
   if (!dwForceSamplesPerSec && !mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK)) {
      if (!m_pmemSR)
         m_pmemSR = new CMem;
      if (!m_pmemSR || !m_pmemSR->Required (mmckinfoSubchunk.cksize)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }

      m_dwSRSkip = m_dwSamplesPerSec / m_dwSRSAMPLESPERSEC;  // 1/100th of a second
      m_dwSRSamples = mmckinfoSubchunk.cksize / sizeof(SRFEATURE);
      m_paSRFeature = (PSRFEATURE) m_pmemSR->p;

      // set the location
      m_dwSRFeatChunkStart = (DWORD) mmioSeek (hmmio, 0, SEEK_CUR);

      mmioRead (hmmio, (HPSTR) m_paSRFeature, mmckinfoSubchunk.cksize);

      // set the size
      m_dwSRFeatChunkSize = (DWORD) mmioSeek (hmmio, 0, SEEK_CUR) - m_dwSRFeatChunkStart;

      mmioAscend(hmmio, &mmckinfoSubchunk, 0);
   }
   mmioSeek (hmmio, lSeek, SEEK_SET);

   // pitch
   DWORD dwPitchSub;
   for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++) {
      lSeek = mmioSeek (hmmio, 0, SEEK_CUR);
      mmckinfoSubchunk.ckid = dwPitchSub ? FCC_PITCHSUB: FCC_PITCH; 
      if (!dwForceSamplesPerSec && !mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK)) {
         if (!m_amemPitch[dwPitchSub].Required (mmckinfoSubchunk.cksize)) {
            if (fOpened)
               mmioClose (hmmio, 0);
            if (pMegaFile)
               MegaFileClose (pMegaFile);
            return FALSE;
         }

         m_adwPitchSkip[dwPitchSub] = m_dwSamplesPerSec / m_dwSRSAMPLESPERSEC;  // 1/100th of a second
         m_adwPitchSamples[dwPitchSub] = mmckinfoSubchunk.cksize / sizeof(WVPITCH);
         m_apPitch[dwPitchSub] = (PWVPITCH) m_amemPitch[dwPitchSub].p;

         mmioRead (hmmio, (HPSTR) m_apPitch[dwPitchSub], mmckinfoSubchunk.cksize);

         DWORD i;
         m_afPitchMaxStrength[dwPitchSub] = 0;
         for (i = 0; i < m_adwPitchSamples[dwPitchSub]; i++)
            m_afPitchMaxStrength[dwPitchSub] = max(m_afPitchMaxStrength[dwPitchSub], m_apPitch[dwPitchSub][i].fStrength);

         mmioAscend(hmmio, &mmckinfoSubchunk, 0);
      }
      mmioSeek (hmmio, lSeek, SEEK_SET);
   } // dwPitchSub

   // find all the text chunks
   DWORD i;
   for (i = 0; i < 2; i++) {
      PCMem pMem;
      FOURCC fcc;
      switch (i) {
         case 0:
            pMem = &m_memSpoken;
            fcc = FCC_SPOKEN;
            break;
         case 1:
            pMem = &m_memSpeaker;
            fcc = FCC_SPEAKER;
            break;
         //case 2:
         //   pMem = &m_memSoundsLike;
         //   fcc = FCC_SOUNDSLIKE;
         //   break;
      }
      // find the spoken text
      lSeek = mmioSeek (hmmio, 0, SEEK_CUR);
      mmckinfoSubchunk.ckid = fcc; 
      if (!mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK)) {
         if (!pMem->Required (mmckinfoSubchunk.cksize + 4)) {
            if (fOpened)
               mmioClose (hmmio, 0);
            if (pMegaFile)
               MegaFileClose (pMegaFile);
            return FALSE;
         }
         memset (pMem->p, 0, mmckinfoSubchunk.cksize + 4);
         mmioRead (hmmio, (HPSTR) pMem->p, mmckinfoSubchunk.cksize);
         mmioAscend(hmmio, &mmckinfoSubchunk, 0);
      }
      mmioSeek (hmmio, lSeek, SEEK_SET);
   } // i


   // Find the data subchunk. The current file position should be at 
   // the beginning of the data chunk; however, you should not make 
   // this assumption. Use mmioDescend to locate the data chunk. 
   mmckinfoSubchunk.ckid = FCC_DATA; 
   if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK)) {
      if (fOpened)
         mmioClose (hmmio, 0);
      if (pMegaFile)
         MegaFileClose (pMegaFile);
      return FALSE;
   }

   // how large is the data?
   DWORD dwDataSize;
   dwDataSize = mmckinfoSubchunk.cksize;
   m_dwSamples = (DWORD)((__int64) dwDataSize * (__int64) pWFEX->nSamplesPerSec / (__int64) pWFEX->nAvgBytesPerSec);
      // this is an approx
#ifdef OLDFORCE
   if (dwForceSamplesPerSec)
      m_dwSamples = (DWORD)((__int64)m_dwSamples * (__int64) dwForceSamplesPerSec /
         (__int64)dwSamplesPerSecOrig);
#endif

   // update the progress wave
#ifdef OLDFORCE
   if (pProgressWave) {
#else
   if (!dwForceSamplesPerSec && pProgressWave) {
#endif
      if (!pProgressWave->Update (this, 0, m_dwSamples)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }
   }

   // if we dont want to load in the wave data then we're all done
   if (!fLoadWave)
      goto alldone;

   // allocate approximately enough data, plus some
   if (!m_pmemWave)
      m_pmemWave = new CMem;
   if (!m_pmemWave) {
      if (fOpened)
         mmioClose (hmmio, 0);
      if (pMegaFile)
         MegaFileClose (pMegaFile);
      return FALSE;
   }
   DWORD dwExtra;
   dwExtra = 24;  // BUGFIX - Allocate a few extra because when loaded in 16 khz wave
                  // and converted to 32 khz, was having problems because off by
                  // 2 bytes
   if (pWFEX->wFormatTag != WAVE_FORMAT_PCM) {
      dwExtra = 1024 + m_dwSamples/20;      // just in case conversion is wrong
      if (pProgressWave)
         dwExtra *= 2;  // since wont be able to add on
   }

   if (!m_pmemWave->Required ((m_dwSamples + dwExtra) * m_dwChannels * sizeof(short))) {
      if (fOpened)
         mmioClose (hmmio, 0);
      if (pMegaFile)
         MegaFileClose (pMegaFile);
      return FALSE;
   }
   m_psWave = (short*) m_pmemWave->p;  // so when make callback can access

   // allocate enough memory for load buffer
   DWORD dwSrcBufSize = pWFEX->nAvgBytesPerSec / 2;
   dwSrcBufSize = max(dwSrcBufSize, (DWORD)pWFEX->nBlockAlign*2);
   dwSrcBufSize = max(dwSrcBufSize, 1000);

   // BUGFIX - Made SRCBUF smaller so will compress and decompress in smaller
   // chunks, for on-the-fly in MIFCLIENT. Was 100K
   BOOL fAbort = FALSE;
#ifdef OLDFORCE
   if ((pWFEX->wFormatTag != WAVE_FORMAT_PCM) || (pWFEX->wBitsPerSample != 16) || dwForceSamplesPerSec) {
#else
   if ((pWFEX->wFormatTag != WAVE_FORMAT_PCM) || (pWFEX->wBitsPerSample != 16)) {
#endif
      CMem memSrc;
      DWORD dwBlock = (dwSrcBufSize / pWFEX->nBlockAlign) * pWFEX->nBlockAlign;
      if (!memSrc.Required (dwBlock)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }

      // needs to get converted
      WAVEFORMATEX   WFEXNew;
      memset (&WFEXNew, 0, sizeof(WFEXNew));
      WFEXNew.nChannels = pWFEX->nChannels;
      WFEXNew.nSamplesPerSec = pWFEX->nSamplesPerSec;
#ifdef OLDFORCE
      if (dwForceSamplesPerSec)
         WFEXNew.nSamplesPerSec = dwForceSamplesPerSec;
#endif
      WFEXNew.wFormatTag = WAVE_FORMAT_PCM;
      WFEXNew.wBitsPerSample = 16;
      WFEXNew.nBlockAlign = (WFEXNew.wBitsPerSample * WFEXNew.nChannels) / 8;
      WFEXNew.nAvgBytesPerSec = WFEXNew.nBlockAlign * WFEXNew.nSamplesPerSec;

      // open the converter
      HACMSTREAM  hStream;
      hStream = NULL;
      MMRESULT mm;
      mm = acmStreamOpen (&hStream, NULL, pWFEX, &WFEXNew, NULL, NULL, 0,
         ACM_STREAMOPENF_NONREALTIME);
      if (!hStream) {
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }

      // figure out the destination stream size
      DWORD dwDestSize;
      dwDestSize = 0;
      if (acmStreamSize (hStream, dwBlock, &dwDestSize, ACM_STREAMSIZEF_SOURCE)) {
         acmStreamClose (hStream, 0);
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }

      // allocate the memory
      CMem memDest;
      if (!memDest.Required (dwDestSize)) {
         acmStreamClose (hStream, 0);
         if (fOpened)
            mmioClose (hmmio, 0);
         if (pMegaFile)
            MegaFileClose (pMegaFile);
         return FALSE;
      }
      BYTE *pNew;
      pNew = (BYTE*) memDest.p;

      // prepare the stream header
      ACMSTREAMHEADER hdr;
      memset (&hdr, 0, sizeof(hdr));
      hdr.cbStruct = sizeof(hdr);
      hdr.pbSrc = (LPBYTE) memSrc.p;
      hdr.cbSrcLength = dwBlock;
      hdr.pbDst = pNew;
      hdr.cbDstLength = dwDestSize;
      mm = acmStreamPrepareHeader (hStream, &hdr, 0);

      DWORD dwCurPosn;
      dwCurPosn = 0;
      while (dwDataSize) {
         DWORD dwLoad = min(dwDataSize, dwBlock);
         mmioRead (hmmio, (PSTR) memSrc.p, dwLoad);
         dwDataSize -= dwLoad;

         // convert
         hdr.cbSrcLength = dwLoad;
         hdr.cbDstLengthUsed = 0;
         mm = acmStreamConvert (hStream, &hdr,
            ((dwDataSize + dwLoad == mmckinfoSubchunk.cksize) ? ACM_STREAMCONVERTF_START : 0) |
            (dwDataSize ? ACM_STREAMCONVERTF_BLOCKALIGN : ACM_STREAMCONVERTF_END));

         // if there isn't enough memory in the buffer then allocate more
         // BUGFIX - If pProgressWave then can't add on any
         if (!pProgressWave && (m_pmemWave->m_dwAllocated < dwCurPosn + hdr.cbDstLengthUsed)) {
            m_pmemWave->Required (dwCurPosn + hdr.cbDstLengthUsed);
            m_psWave = (short*) m_pmemWave->p; // so when make callback can access
         }
         if (m_pmemWave->m_dwAllocated >= dwCurPosn + hdr.cbDstLengthUsed) {
            memcpy ((PBYTE) m_pmemWave->p + dwCurPosn, hdr.pbDst, hdr.cbDstLengthUsed);
            dwCurPosn += hdr.cbDstLengthUsed;
         }

         // do percent complete
         if (pProgress && mmckinfoSubchunk.cksize)
            pProgress->Update ((fp)(mmckinfoSubchunk.cksize - dwDataSize) / (fp)mmckinfoSubchunk.cksize);

         // update the progress wave
#ifdef OLDFORCE
         if (pProgressWave) {
#else
         if (!dwForceSamplesPerSec && pProgressWave) {
#endif
            if (!pProgressWave->Update (this, dwCurPosn/sizeof(short)/m_dwChannels, m_dwSamples)) {
               fAbort = TRUE;
               break;
            }
         }

      }

      // unprepare the header
      mm = acmStreamUnprepareHeader (hStream, &hdr, 0);

      // close the stream
      acmStreamClose (hStream, 0);

      // update the number of samples
      m_dwSamples = dwCurPosn / (m_dwChannels * sizeof(short));
   }
   else {
      // else, 16-bit PCM so just load in
      DWORD dwCurPosn;
      dwCurPosn = 0;
      while (dwDataSize) {
         DWORD dwLoad = min(dwDataSize, dwSrcBufSize);
         mmioRead (hmmio, (HPSTR)((PBYTE)m_pmemWave->p + dwCurPosn), dwLoad);
         dwDataSize -= dwLoad;
         dwCurPosn += dwLoad;

         // do percent complete
         if (pProgress && mmckinfoSubchunk.cksize)
            pProgress->Update ((fp)(mmckinfoSubchunk.cksize - dwDataSize) / (fp)mmckinfoSubchunk.cksize);

         // update the progress wave
#ifdef OLDFORCE
         if (pProgressWave) {
#else
         if (!dwForceSamplesPerSec && pProgressWave) {
#endif
            if (!pProgressWave->Update (this, dwCurPosn/sizeof(short)/m_dwChannels, m_dwSamples)) {
               fAbort = TRUE;
               break;
            }
         }
      }
   }

   if (fAbort) {
      if (fOpened)
         mmioClose (hmmio, 0);
      if (pMegaFile)
         MegaFileClose (pMegaFile);
      return FALSE;
   }


   // finished
alldone:
   if (fOpened)
      mmioClose (hmmio, 0);
   if (pMegaFile)
      MegaFileClose (pMegaFile);
   m_fDirty = FALSE;

#ifndef OLDFORCE
   // if force samples per sec then conver
   if (dwForceSamplesPerSec)
      ConvertSamplesPerSec (dwForceSamplesPerSec, NULL);
#endif

   if (pProgress)
      pProgress->Update (1);
   return TRUE;
}

/****************************************************************************
CM3DWave::Save - Writes the file out using the current file name

inputs
   BOOL                    fIncludeSRFeature - If TRUE then save the SRFeatures
                           If FALSE then dont bother savint them
   PCProgressSocket        pProgress - To show progress bar. NULL if error
   HMMIO                   hmmio - If not-NULL then this is used to load the data from instead of
                           the file name.
*/
BOOL CM3DWave::Save (BOOL fIncludeSRFeature, PCProgressSocket pProgress, HMMIO hmmio)
{
   if (!m_pmemWave)
      return FALSE;  // no wave data loaded

   m_dwSRFeatChunkStart = m_dwSRFeatChunkSize = 0;

   // if the samples per sec or channels have changed AND this is not PCM then
   // need to do some trickiness
   PWAVEFORMATEX pWFEX = (PWAVEFORMATEX) m_memWFEX.p;

   if ((m_dwSamplesPerSec != pWFEX->nSamplesPerSec) || (m_dwChannels != pWFEX->nChannels)) {
      if (pWFEX->wFormatTag == WAVE_FORMAT_PCM) {
         pWFEX->nChannels = m_dwChannels;
         pWFEX->nBlockAlign = pWFEX->nChannels * pWFEX->wBitsPerSample / 8;
         pWFEX->nSamplesPerSec = m_dwSamplesPerSec;
         pWFEX->nAvgBytesPerSec = pWFEX->nSamplesPerSec * pWFEX->nBlockAlign;
      }
      else {
         BYTE abHuge[10000];  // enough for destination format
         if (acmFormatSuggest (NULL, pWFEX, (PWAVEFORMATEX) &abHuge[0],
            sizeof(abHuge), ACM_FORMATSUGGESTF_NCHANNELS | ACM_FORMATSUGGESTF_NSAMPLESPERSEC |
            ACM_FORMATSUGGESTF_WBITSPERSAMPLE | ACM_FORMATSUGGESTF_WFORMATTAG))
            return FALSE;  // error convering format
         
         pWFEX = (PWAVEFORMATEX) &abHuge[0];
         if (!m_memWFEX.Required (sizeof(WAVEFORMATEX) + pWFEX->cbSize))
            return FALSE;  // too large
         memcpy (m_memWFEX.p, pWFEX, sizeof(WAVEFORMATEX) + pWFEX->cbSize);
         pWFEX = (PWAVEFORMATEX) m_memWFEX.p;
      } // not PCM
   } // if change in samples per sec


   // open header
   // use multimedia functions to open
   BOOL fOpened;
   fOpened = FALSE;
   if (!hmmio) {
      // NOTE: If using megafile, dont let save
      if (MegaFileInUse())
         return FALSE;

      hmmio = mmioOpen (m_szFile, NULL, MMIO_WRITE | MMIO_CREATE | MMIO_EXCLUSIVE );
      fOpened = TRUE;
   }
   if (!hmmio)
      return FALSE;  // error

   // creat the main chunk
   MMCKINFO mmckinfoMain;
   memset (&mmckinfoMain, 0, sizeof(mmckinfoMain));
   mmckinfoMain.fccType = FCC_WAVE;
   if (mmioCreateChunk (hmmio, &mmckinfoMain, MMIO_CREATERIFF)) {
      if (fOpened)
         mmioClose (hmmio, 0);
      return FALSE;
   }


   // create the format
   MMCKINFO mmckinfoFmt;
   memset (&mmckinfoFmt, 0, sizeof(mmckinfoFmt));
   mmckinfoFmt.ckid = FCC_FMT;
   if (mmioCreateChunk (hmmio, &mmckinfoFmt, 0)) {
      if (fOpened)
         mmioClose (hmmio, 0);
      return FALSE;
   }

   // write the header
   mmioWrite (hmmio, (char*) pWFEX, sizeof(WAVEFORMATEX) + pWFEX->cbSize);

   // ascent the format
   mmioAscend (hmmio, &mmckinfoFmt, 0);


   // write the phoneme chunk
   if (m_lWVPHONEME.Num()) {
      memset (&mmckinfoFmt, 0, sizeof(mmckinfoFmt));
      mmckinfoFmt.ckid = FCC_PHONEME;
      if (mmioCreateChunk (hmmio, &mmckinfoFmt, 0)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         return FALSE;
      }

      // write the header
      mmioWrite (hmmio, (char*) m_lWVPHONEME.Get(0), m_lWVPHONEME.Num()*sizeof(WVPHONEME));

      // ascent the format
      mmioAscend (hmmio, &mmckinfoFmt, 0);
   }

   // write out the words
   if (m_lWVWORD.Num()) {
      memset (&mmckinfoFmt, 0, sizeof(mmckinfoFmt));
      mmckinfoFmt.ckid = FCC_WORDSEG;
      if (mmioCreateChunk (hmmio, &mmckinfoFmt, 0)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         return FALSE;
      }

      // allocate enough memory
      CMem mem;
      DWORD i;
      size_t dwNeed = 0;
      PWVWORD pw;
      PWSTR psz;
      for (i = 0; i < m_lWVWORD.Num(); i++) {
         pw = (PWVWORD)m_lWVWORD.Get(i);
         psz = (PWSTR)(pw+1);
         dwNeed += sizeof(WVWORD) + sizeof(WCHAR) * (wcslen(psz)+1);
      } // i
      if (!mem.Required (dwNeed)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         return FALSE;
      }
      PBYTE pb = (PBYTE) mem.p;
      size_t dw;
      for (i = 0; i < m_lWVWORD.Num(); i++) {
         pw = (PWVWORD)m_lWVWORD.Get(i);
         psz = (PWSTR)(pw+1);
         dw = sizeof(WVWORD) + sizeof(WCHAR) * (wcslen(psz)+1);
         memcpy (pb, pw, dw);
         pb += dw;
      } // i

      // write the header
      mmioWrite (hmmio, (char*) mem.p, (DWORD) dwNeed);

      // ascent the format
      mmioAscend (hmmio, &mmckinfoFmt, 0);
   }

   // write the sr feature chunk
   // BUGFIX - Must be right SRSkip
   if (fIncludeSRFeature && m_dwSRSamples && (m_dwSRSkip == m_dwSamplesPerSec / SRSAMPLESPERSEC)) {
      memset (&mmckinfoFmt, 0, sizeof(mmckinfoFmt));
      mmckinfoFmt.ckid = FCC_SRFEATURE;
      if (mmioCreateChunk (hmmio, &mmckinfoFmt, 0)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         return FALSE;
      }

      // set the location
      m_dwSRFeatChunkStart = (DWORD) mmioSeek (hmmio, 0, SEEK_CUR);

      // write the header
      mmioWrite (hmmio, (char*) m_paSRFeature, m_dwSRSamples * sizeof(SRFEATURE));

      // find out how large it is
      m_dwSRFeatChunkSize = (DWORD) mmioSeek (hmmio, 0, SEEK_CUR) - m_dwSRFeatChunkStart;

      // ascent the format
      mmioAscend (hmmio, &mmckinfoFmt, 0);
   }

   // write the pitch chunk
   // BUGFIX - Only write out pitch if it's the standard samples per sec.
   DWORD dwPitchSub;
   for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++) {
      if (fIncludeSRFeature && m_adwPitchSamples[dwPitchSub] && (m_adwPitchSkip[dwPitchSub] == m_dwSamplesPerSec / SRSAMPLESPERSEC)) {
         memset (&mmckinfoFmt, 0, sizeof(mmckinfoFmt));
         mmckinfoFmt.ckid = dwPitchSub ? FCC_PITCHSUB : FCC_PITCH;
         if (mmioCreateChunk (hmmio, &mmckinfoFmt, 0)) {
            if (fOpened)
               mmioClose (hmmio, 0);
            return FALSE;
         }

         // write the header
         mmioWrite (hmmio, (char*) m_apPitch[dwPitchSub], m_adwPitchSamples[dwPitchSub] * sizeof(WVPITCH));

         // ascent the format
         mmioAscend (hmmio, &mmckinfoFmt, 0);
      }
   } // dwPitchSub

   // write all the text chunks
   DWORD i;
   for (i = 0; i < 2; i++) {
      PCMem pMem;
      FOURCC fcc;
      switch (i) {
         case 0:
            pMem = &m_memSpoken;
            fcc = FCC_SPOKEN;
            break;
         case 1:
            pMem = &m_memSpeaker;
            fcc = FCC_SPEAKER;
            break;
         //case 2:
         //   pMem = &m_memSoundsLike;
         //   fcc = FCC_SOUNDSLIKE;
         //   break;
      }

      DWORD dwLen;
      dwLen = (DWORD)wcslen((PWSTR)pMem->p);
      if (!dwLen)
         continue;   // nothing to write

      // write out
      memset (&mmckinfoFmt, 0, sizeof(mmckinfoFmt));
      mmckinfoFmt.ckid = fcc;
      if (mmioCreateChunk (hmmio, &mmckinfoFmt, 0)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         return FALSE;
      }

      // write the header
      mmioWrite (hmmio, (char*) pMem->p, (dwLen+1) * sizeof(WCHAR));

      // ascent the format
      mmioAscend (hmmio, &mmckinfoFmt, 0);
   }

   // create the data chunk
   MMCKINFO mmckinfoData;
   memset (&mmckinfoData, 0, sizeof(mmckinfoData));
   mmckinfoData.ckid = FCC_DATA;
   if (mmioCreateChunk (hmmio, &mmckinfoData, 0)) {
      if (fOpened)
         mmioClose (hmmio, 0);
      return FALSE;
   }

#define SRCBUF       100000         // 100K
   // allocate enough memory for save buffer
   if ((pWFEX->wFormatTag != WAVE_FORMAT_PCM) || (pWFEX->wBitsPerSample != 16)) {
      CMem memSrc;
      if (!memSrc.Required (SRCBUF)) {
         if (fOpened)
            mmioClose (hmmio, 0);
         return FALSE;
      }

      // needs to get converted
      WAVEFORMATEX   WFEXOld;
      memset (&WFEXOld, 0, sizeof(WFEXOld));
      WFEXOld.nChannels = m_dwChannels;
      WFEXOld.nSamplesPerSec = m_dwSamplesPerSec;
      WFEXOld.wFormatTag = WAVE_FORMAT_PCM;
      WFEXOld.wBitsPerSample = 16;
      WFEXOld.nBlockAlign = (WFEXOld.wBitsPerSample * WFEXOld.nChannels) / 8;
      WFEXOld.nAvgBytesPerSec = WFEXOld.nBlockAlign * WFEXOld.nSamplesPerSec;

      DWORD dwBlock;
      dwBlock = (SRCBUF / WFEXOld.nBlockAlign) * WFEXOld.nBlockAlign;

      // open the converter
      HACMSTREAM  hStream;
      hStream = NULL;
      MMRESULT mm;
      mm = acmStreamOpen (&hStream, NULL, &WFEXOld, pWFEX, NULL, NULL, 0,
         ACM_STREAMOPENF_NONREALTIME);
      if (!hStream) {
         if (fOpened)
            mmioClose (hmmio, 0);
         return FALSE;
      }

      // figure out the destination stream size
      DWORD dwDestSize;
      dwDestSize = 0;
      if (acmStreamSize (hStream, dwBlock, &dwDestSize, ACM_STREAMSIZEF_SOURCE)) {
         acmStreamClose (hStream, 0);
         if (fOpened)
            mmioClose (hmmio, 0);
         return FALSE;
      }

      // allocate the memory
      CMem memDest;
      if (!memDest.Required (dwDestSize)) {
         acmStreamClose (hStream, 0);
         if (fOpened)
            mmioClose (hmmio, 0);
         return FALSE;
      }
      BYTE *pNew;
      pNew = (BYTE*) memDest.p;

      // prepare the stream header
      ACMSTREAMHEADER hdr;
      memset (&hdr, 0, sizeof(hdr));
      hdr.cbStruct = sizeof(hdr);
      hdr.pbSrc = (LPBYTE) memSrc.p;
      hdr.cbSrcLength = dwBlock;
      hdr.pbDst = pNew;
      hdr.cbDstLength = dwDestSize;
      mm = acmStreamPrepareHeader (hStream, &hdr, 0);

      DWORD dwCurPosn, dwDataSize;
      dwCurPosn = 0;
      dwDataSize = m_dwSamples * m_dwChannels * sizeof(short);
      while (dwDataSize) {
         DWORD dwLoad = min(dwDataSize, dwBlock);
         memcpy (memSrc.p, (PBYTE)m_pmemWave->p + dwCurPosn, dwLoad);

         // convert
         hdr.cbSrcLength = dwLoad;
         hdr.cbDstLengthUsed = 0;
         mm = acmStreamConvert (hStream, &hdr,
            (dwCurPosn ? 0 : ACM_STREAMCONVERTF_START) |
            ((dwDataSize > dwLoad) ? ACM_STREAMCONVERTF_BLOCKALIGN : ACM_STREAMCONVERTF_END));
         dwDataSize -= hdr.cbSrcLengthUsed;
         dwCurPosn += hdr.cbSrcLengthUsed;

         // write it out
         mmioWrite (hmmio, (char*) hdr.pbDst, hdr.cbDstLengthUsed);

         // do percent complete
         if (pProgress && (dwDataSize+dwCurPosn))
            pProgress->Update ((fp)dwCurPosn / (fp)(dwDataSize+dwCurPosn));
      }

      // unprepare the header
      mm = acmStreamUnprepareHeader (hStream, &hdr, 0);

      // close the stream
      acmStreamClose (hStream, 0);
   }
   else {
      // else, 16-bit PCM so just write it out
      DWORD dwCurPosn, dwDataSize;
      dwCurPosn = 0;
      dwDataSize = m_dwSamples * m_dwChannels * sizeof(short);
      while (dwDataSize) {
         DWORD dwLoad = min(dwDataSize, SRCBUF);
         mmioWrite (hmmio, (HPSTR)((PBYTE)m_pmemWave->p + dwCurPosn), dwLoad);
         dwDataSize -= dwLoad;
         dwCurPosn += dwLoad;

         // do percent complete
         if (pProgress && (dwDataSize+dwCurPosn))
            pProgress->Update ((fp)dwCurPosn / (fp)(dwDataSize+dwCurPosn));
      }
   }



   // will need to ascend data
   mmioAscend (hmmio, &mmckinfoData, 0);

   // ascent main riff
   mmioAscend (hmmio, &mmckinfoMain, 0);

   // finished
   if (fOpened)
      mmioClose (hmmio, 0);

   // finally
   if (pProgress)
      pProgress->Update (1);
   m_fDirty = FALSE;
   return TRUE;
}

/****************************************************************************
CM3DWave::PaintPhonemes - This draws the phonems onto the HDC.

inputs
   HDC            hDC - To draw on
   RECT           *pr - Rectangle on the HDC
   double         fLeft, fRight - Left and right sides of HDC correspond to these sample
                  numbers. The samples may extend beyond the wave
   PCMLexicon     pLex - Lexicon to use for the phoneme sample text
returns
   none
*/
void CM3DWave::PaintPhonemes (HDC hDC, RECT *pr, double fLeft, double fRight,
                              PCMLexicon pLex)
{
   if ((pr->right - pr->left <= 0) || (pr->bottom - pr->top <= 0))
      return;  // nothing to draw
   if (fRight <= fLeft + EPSILON)
      return;  // cant to this either

   // create a bitmap to draw this in
   HBITMAP hBit;
   HDC hDCDraw;
   hBit = CreateCompatibleBitmap (hDC, pr->right - pr->left, pr->bottom - pr->top);
   hDCDraw = CreateCompatibleDC (hDC);
   SelectObject (hDCDraw, hBit);

   // blank backgroudn
   RECT rDrawMain;
   rDrawMain.top = rDrawMain.left = 0;
   rDrawMain.right = pr->right - pr->left;
   rDrawMain.bottom = pr->bottom - pr->top;
   HBRUSH hBrush;
   hBrush = CreateSolidBrush (RGB(0xf0, 0xf0, 0xff));
   FillRect (hDCDraw, &rDrawMain, hBrush);
   DeleteObject (hBrush);

   // font
   HFONT hFont;
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -10; 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   hFont = CreateFontIndirect (&lf);
   HFONT hFontOld;
   hFontOld = (HFONT) SelectObject (hDCDraw, hFont);
   SetBkMode (hDCDraw, TRANSPARENT);
   SetTextColor (hDCDraw, RGB(0,0,0));

   // line for separator
   SelectObject (hDCDraw, GetStockObject (BLACK_PEN));


   // loop through all the phonemes
   int i;
   PWVPHONEME pp;
   DWORD dwNum;
   dwNum = m_lWVPHONEME.Num();
   pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
   for (i = -1; i < (int)dwNum; i++) {
      // start and end pixel for phoneme, as well as phoneme descriptor
      double fStart, fEnd;
      WCHAR szPhone[max(8, sizeof(pp[i].awcNameLong)/sizeof(WCHAR))];
      PLEXPHONE plp;
      szPhone[0] = 0;
      if (i < 0) {
         fStart = 0;
         fEnd = (dwNum ? pp[0].dwSample : m_dwSamples);
         wcscpy (szPhone, MLexiconEnglishPhoneSilence());
         plp = pLex ? pLex->PhonemeGet((WCHAR*)pLex->PhonemeSilence()) : NULL;
      }
      else {
         fStart = pp[i].dwSample;
         fEnd = ((i+1 < (int)dwNum) ? pp[i+1].dwSample : m_dwSamples);
         memset (szPhone, 0, sizeof(szPhone));
         memcpy (szPhone, pp[i].awcNameLong, sizeof(pp[i].awcNameLong));
         plp = pLex ? pLex->PhonemeGet (szPhone) : NULL;
      }

      // if out of view then dont bother
      if ((fEnd <= fLeft) || (fStart >= fRight))
         continue;

      // convert this to pixels
      fEnd = (fEnd - fLeft) / (fRight - fLeft) * (double)(rDrawMain.right - rDrawMain.left);
      fStart = (fStart - fLeft) / (fRight - fLeft) * (double)(rDrawMain.right - rDrawMain.left);

      // keep within screen boundaries, but out just enough so edge line doenst draw
      fEnd = min(fEnd, rDrawMain.right+2);
      fStart = max(fStart, rDrawMain.left - 2);

      // draw lines
      MoveToEx (hDCDraw, (int)fStart, rDrawMain.top, NULL);
      LineTo (hDCDraw, (int)fStart, rDrawMain.bottom+1);
      MoveToEx (hDCDraw, (int)fEnd, rDrawMain.top, NULL);
      LineTo (hDCDraw, (int)fEnd, rDrawMain.bottom+1);

      // figure out text strings
      char szShort[12], szLong[30];
      WideCharToMultiByte (CP_ACP, 0, szPhone, -1, szShort, sizeof(szShort), 0, 0);
      strcpy (szLong, szShort);
      if (plp && plp->szSampleWord[0]) {
         strcat (szLong, " - ");
         WideCharToMultiByte (CP_ACP, 0, plp->szSampleWord, -1, szLong + strlen(szLong),
            sizeof(szLong)-(int)strlen(szLong), 0, 0);
      }

      // see if they fit
      RECT rSizeS, rSizeL, rDraw;
      rSizeS = rDrawMain;
      rSizeS.left = 0;
      rSizeS.right = 1000;
      rSizeL = rSizeS;
      DrawText (hDCDraw, szShort, -1, &rSizeS, DT_CALCRECT | DT_SINGLELINE);
      DrawText (hDCDraw, szLong, -1, &rSizeL, DT_CALCRECT | DT_SINGLELINE);

      // how much space
      int iLen;
      rDraw = rDrawMain;
      rDraw.left = (int)fStart;
      rDraw.right = (int)fEnd;
      iLen = rDraw.right - rDraw.left - 2;
      if (rSizeL.right < iLen)
         DrawText (hDCDraw, szLong, -1, &rDraw, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
      else if (rSizeS.right < iLen)
         DrawText (hDCDraw, szShort, -1, &rDraw, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
   }

   // free font
   SelectObject (hDCDraw, hFontOld);
   DeleteObject (hFont);

   // draw
   BitBlt (hDC, pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top,
      hDCDraw, 0, 0, SRCCOPY);
   DeleteDC (hDCDraw);
   DeleteObject (hBit);
}

/****************************************************************************
CM3DWave::PaintWave - This draws the wave onto a HDC.

inputs
   HDC            hDC - To draw on
   RECT           *pr - Rectangle on the HDC
   double         fLeft, fRight - Left and right sides of HDC correspond to these sample
                  numbers. The samples may extend beyond the wave
   double         fTop, fBottom - Top and bottom of HDC correspond from these values,
                  from 32767 to -32768
   DWORD          dwChannel - Channel to draw, or -1 for combined
   BOOL           fEverySample - If TRUE account for every sample
returns
   none
*/
void CM3DWave::PaintWave (HDC hDC, RECT *pr, double fLeft, double fRight,
                          double fTop, double fBottom, DWORD dwChannel, BOOL fEverySample)
{
   if ((pr->right - pr->left <= 0) || (pr->bottom - pr->top <= 0))
      return;  // nothing to draw
   if ((fRight <= fLeft + EPSILON) || (fTop < fBottom + EPSILON))
      return;  // cant to this either

   // create a bitmap to draw this in
   HBITMAP hBit;
   HDC hDCDraw;
   hBit = CreateCompatibleBitmap (hDC, pr->right - pr->left, pr->bottom - pr->top);
   hDCDraw = CreateCompatibleDC (hDC);
   SelectObject (hDCDraw, hBit);

   // blank backgroudn
   RECT rDraw;
   rDraw.top = rDraw.left = 0;
   rDraw.right = pr->right - pr->left;
   rDraw.bottom = pr->bottom - pr->top;
   FillRect (hDCDraw, &rDraw, (HBRUSH)GetStockObject (WHITE_BRUSH));

   HPEN hPen, hPenOld;
   COLORREF crPen;
   crPen = RGB(0x40, 0x40, 0x40);
   if ((dwChannel < m_dwChannels) && (m_dwChannels >= 2))
      switch (dwChannel % 6) {
      case 0:
         crPen = RGB(0x80,0,0);
         break;
      case 1:
         crPen = RGB(0,0x80,0);
         break;
      case 2:
         crPen = RGB(0,0,0x80);
         break;
      case 3:
         crPen = RGB(0x80,0x80,0);
         break;
      case 4:
         crPen = RGB(0,0x80,0x80);
         break;
      case 5:
         crPen = RGB(0x80,0,0x80);
         break;
      }
   hPen = CreatePen (PS_SOLID, 0, crPen);
   hPenOld = (HPEN) SelectObject (hDCDraw, hPen);

#define SUPER        10
   int x, is, isMax;
   double fCur, fCurSuper, fDelta, fDeltaSuper;
   DWORD dwChanStart, dwChanEnd, dwChan;
   short sMin, sMax, sCur;
   DWORD dwSuper;
   dwSuper = SUPER;
   fp fScale;
   fScale =  1.0 / (fTop - fBottom) * (fp) rDraw.bottom;
   fDelta = (fRight - fLeft) / (double) rDraw.right;
   
   // if want every sample then increase supersampling
   if (fEverySample) {
      dwSuper = (DWORD) (fDelta + 1);
   }

   fDeltaSuper = fDelta / (fp) dwSuper;
   isMax = (fDelta < 1) ? 1 : dwSuper;
   dwChanStart = (dwChannel < m_dwChannels) ? dwChannel : 0;
   dwChanEnd = (dwChannel < m_dwChannels) ? (dwChannel+1) : m_dwChannels;
   for (x = 0, fCur = fLeft; x < rDraw.right; x++, fCur += fDelta) {
      sMax = -32768;
      sMin = 32767;

      // supersample
      for (is = 0, fCurSuper = fCur; is < isMax; is++, fCurSuper += fDeltaSuper) {
         if ((fCurSuper < 0) || (fCurSuper >= m_dwSamples))
            continue;   // beyond range of wave

         for (dwChan = dwChanStart; dwChan < dwChanEnd; dwChan++) {
            sCur = m_psWave[(DWORD)fCurSuper * m_dwChannels + dwChan];
            sMin = min(sMin, sCur);
            sMax = max(sMax, sCur);
         } // dwchan
      } // is

      // draw it
      if (sMax < sMin)
         continue;   // beyond range of wave
      sMax = max(sMax, 0);
      sMin = min(sMin, 0);

      // translate min and max to a line
      int iStart, iEnd;
      iStart = (int) ((fTop - (double)sMax) * fScale);
      iEnd = (int) ((fTop - (double)sMin) * fScale);
      MoveToEx (hDCDraw, x, iStart, NULL);
      LineTo (hDCDraw, x, iEnd);
   } // x


   // finally, horizontal line
   SelectObject (hDCDraw, GetStockObject (BLACK_PEN));
   int iCenter;
   iCenter = (int) (fTop * fScale);
   MoveToEx (hDCDraw, 0, iCenter, NULL);
   LineTo (hDCDraw, rDraw.right, iCenter);


   // draw
   BitBlt (hDC, pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top,
      hDCDraw, 0, 0, SRCCOPY);
   SelectObject (hDCDraw, hPenOld);
   DeleteObject (hPen);
   DeleteDC (hDCDraw);
   DeleteObject (hBit);
}


/****************************************************************************
CM3DWave::WindowSize - Returns an ideal window size, in samples
*/
DWORD CM3DWave::WindowSize (void)
{
   DWORD dw;
   for (dw = 4; dw < m_dwSamplesPerSec / 40; dw *= 2);
      // BUGFIX - Changed from /40 to /20 so pitch detect would work better
      // BUGFIX - Back to /40 so smaller time slices

   return dw;
}

/****************************************************************************
CM3DWave::FFT - Does an FFT on the given point.

inputs
   DWORD          dwWindowSize - Size of the window (in samples)
   int            iCenter - Center sample
   DWORD          dwChannel - Channel that want, or -1 for all channels
   float          *pafWindow - Window function, from CreateFFTWindow
   float          *pafFFT - Filled with the FFT. dwWindowSize/2 must be available
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
   double         *pfEnergy - If not NULL, will also fill in the energy of the
                  windowed area
   DWORD          dwFilterHalf - If not 0, then does a tent filter, with a width
                  of dwFilterHalf*2+1.
returns
   none
*/
void CM3DWave::FFT (DWORD dwWindowSize, int iCenter, DWORD dwChannel,
                    float *pafWindow, float *pafFFT, PCSinLUT pLUT, PCMem pMemFFTScratch,
                    double *pfEnergy, DWORD dwFilterHalf)
{
   // allocate enough memory for the FFT
   CMem memFFT;
   float *pafFFT2;
   if (!memFFT.Required (dwWindowSize * sizeof(float)))
      return;
   pafFFT2 = (float*) memFFT.p;

   int i, iStart;
   DWORD dwChanStart, dwChanEnd, dwScale, dwChan;
   dwChanStart = (dwChannel < m_dwChannels) ? dwChannel : 0;
   dwChanEnd = (dwChannel < m_dwChannels) ? (dwChannel+1) : m_dwChannels;
   dwScale = dwChanEnd - dwChanStart;
   iStart = iCenter - (int)dwWindowSize/2;
   double fWindowSum;
   if (pfEnergy) {
      *pfEnergy = 0;
      fWindowSum = 0;
   }
   for (i = iStart; i < iStart + (int)dwWindowSize; i++) {
      short sValue;
      sValue = 0;
      if ((i >= 0) && (i < (int)m_dwSamples)) {
         for (dwChan = dwChanStart; dwChan < dwChanEnd; dwChan++)
            sValue += m_psWave[i*m_dwChannels + dwChan] / (short)dwScale;
      } // i

      // store awway, along with the window
      pafFFT2[i-iStart] = (float)sValue * pafWindow[i-iStart];

      if (pfEnergy) {
         *pfEnergy += pafFFT2[i-iStart] * pafFFT2[i-iStart];
         fWindowSum += pafWindow[i-iStart];
      }
   }
   if (pfEnergy)
      *pfEnergy = sqrt(*pfEnergy) / fWindowSum;

   // do the FFT
   FFTRecurseReal (pafFFT2 - 1, dwWindowSize, 1, pLUT, pMemFFTScratch);

   // scale and write away
   float   fScale = 2.0 / (float) dwWindowSize;// * 32768.0 / 32768.0;
   float   fScaleSqr = fScale * fScale;
   dwWindowSize /= 2;
   for (i = 0; i < (int)dwWindowSize; i++)
      pafFFT[i] = (float) sqrt(
         (pafFFT2[i*2] * pafFFT2[i*2] + pafFFT2[i*2+1] * pafFFT2[i*2+1]) * fScaleSqr );

   // filter it?
   if (dwFilterHalf) {
      // copy original up
      memcpy (pafFFT2, pafFFT, dwWindowSize * sizeof(float));

      for (i = 0; i < (int)dwWindowSize; i++) {
         int iOffset, iCur, iWeight;
         float fSum = 0;
         DWORD dwCount = 0;
         for (iOffset = -(int)dwFilterHalf; iOffset <= (int)dwFilterHalf; iOffset++) {
            iCur = (int)i + iOffset;
            if ((iCur < 0) || (iCur >= (int)dwWindowSize))
               continue;   // off the edge

            iWeight = (int)dwFilterHalf + 1 - abs(iOffset);
            fSum += (float)iWeight * pafFFT2[iCur];
            dwCount += (DWORD)iWeight;
         } // iOffset
         if (dwCount)
            fSum /= (float)dwCount;

         pafFFT[i] = fSum;
      } // i
   }

   // done
}


/****************************************************************************
CreateLUTForFFT - Create a color look-up table for FFT.

inputs
   PBYTE    pabRed, pabGreen, pabBlue - Pointer to an array of COLORLUT bytes.
*/
#define  COLORLUT    2048
static void CreateLUTForFFT (PBYTE pabRed, PBYTE pabGreen, PBYTE pabBlue)
{

   // generate the color table by interpolating from 5 points at
   // 0x00, 0x40, 0x80, 0xc0, and 0xff

#define LUTNUM    6
   DWORD adwColors[LUTNUM] = {RGB(0,0,0), RGB(0,0,128), RGB(128,0,255), RGB(255,128,128), RGB(128,255,0),
      RGB(0xff,0xff,0xff)};

   BYTE  abInterpRed[LUTNUM];
   BYTE  abInterpGreen[LUTNUM];
   BYTE  abInterpBlue[LUTNUM];
   DWORD i, dwIndex, dwOffset;
   for (i = 0; i < LUTNUM; i++) {
      DWORD dwVal = adwColors[i];

      abInterpRed[i] = GetRValue(dwVal);
      abInterpGreen[i] = GetGValue(dwVal);
      abInterpBlue[i] = GetBValue (dwVal);
   }
   for (i = 0; i < COLORLUT; i++) {
      if (i < COLORLUT/(LUTNUM-1)) {
         dwOffset = i;
         dwIndex = 0;
      }
      else if (i < COLORLUT*2/(LUTNUM-1)) {
         dwOffset = i - COLORLUT/(LUTNUM-1);
         dwIndex = 1;
      }
      else if (i < COLORLUT*3/(LUTNUM-1)) {
         dwOffset = i - COLORLUT*2/(LUTNUM-1);
         dwIndex = 2;
      }
      else if (i < COLORLUT*4/(LUTNUM-1)) {
         dwOffset = i - COLORLUT*3/(LUTNUM-1);
         dwIndex = 3;
      }
      else {
         dwOffset = i - COLORLUT*4/(LUTNUM-1);
         dwIndex = 4;
      }

      pabRed[i] = (BYTE) ((abInterpRed[dwIndex] * (COLORLUT/(LUTNUM-1) - dwOffset) +
         abInterpRed[dwIndex+1] * dwOffset) / (COLORLUT/(LUTNUM-1)));
      pabGreen[i] = (BYTE) ((abInterpGreen[dwIndex] * (COLORLUT/(LUTNUM-1) - dwOffset) +
         abInterpGreen[dwIndex+1] * dwOffset) / (COLORLUT/(LUTNUM-1)));
      pabBlue[i] = (BYTE) ((abInterpBlue[dwIndex] * (COLORLUT/(LUTNUM-1) - dwOffset) +
         abInterpBlue[dwIndex+1] * dwOffset) / (COLORLUT/(LUTNUM-1)));
   }

}


/****************************************************************************
CM3DWave::PaintFFT - This draws the spectrogram onto the HDC. This can also
paint the frequency analysis.

inputs
   HDC            hDC - To draw on
   RECT           *pr - Rectangle on the HDC
   double         fLeft, fRight - Left and right sides of HDC correspond to these sample
                  numbers. The samples may extend beyond the wave
   double         fTop, fBottom - Top and bottom of HDC correspond from these values,
                  from 0 to 1
   DWORD          dwChannel - Channel to draw, or -1 for combined
   HWND           hWnd - If needs to caluclate pitch will use this to display progress bar
   DOWRD          dwPaintWhat -
                        0 = FFT
                        1 = frequncy analysis
                        2 = Stretch FFT
                        3 = Stretch FFT, fit to octave (SR format)
                        4 = SR features (combined)
                        5 = FFT, but smaller window size so that is faster and displays
                           well in shrunk-down animation window
                        6 = SR features (voiced)
                        7 = SR features (noise)
                        8 = phase
                        9 = PCM
                        10 = phase (pitch aligned)
   fp             *pfFreqRange - Fills this in with the frequency range in Hz
returns
   none
*/
void CM3DWave::PaintFFT (HDC hDC, RECT *pr, double fLeft, double fRight,
                          double fTop, double fBottom, DWORD dwChannel, HWND hWnd,
                          DWORD dwPaintWhat, fp *pfFreqRange)
{
   if ((pr->right - pr->left <= 0) || (pr->bottom - pr->top <= 0))
      return;  // nothing to draw
   if ((fRight <= fLeft + EPSILON) || (fTop > fBottom - EPSILON))
      return;  // cant to this either

   switch (dwPaintWhat) {
   case 2:  // stretch fft
   case 3:  // stretch fft fit octave
   // do we need frequency
      CalcPitchIfNeeded (WAVECALC_SEGMENT, hWnd);
      break;
   case 4:  // sr all
   case 6:  // sr voiced
   case 7:  // sr unvoiced
   case 8:  // sr phase
   case 9:  // PCM
      // BUGFIX - Calc SR features if not there
      CalcSRFeaturesIfNeeded (WAVECALC_SEGMENT, hWnd);
      break;
   case 10: // phase by pitch
      // BUGFIX - Calc SR features if not there
      CalcSRFeaturesIfNeeded (WAVECALC_SEGMENT, hWnd);

      // NOTE: Just to make sure that pitch is calculated
      CalcPitchIfNeeded (WAVECALC_SEGMENT, hWnd);
      break;
   }

   // calculate the window size
   DWORD dwWindowSize = WindowSize();
   switch (dwPaintWhat) {
   case 1:  // frequency analysis
      dwWindowSize *= ANALYZEPITCHWINDOWSCALE;   // so can better calc pitch
      break;
   case 5:  // FFT, smaller window so faster
      dwWindowSize /= 8;
      dwWindowSize = max(4,dwWindowSize);
      break;
   }

   BYTE  abRed[COLORLUT], abGreen[COLORLUT], abBlue[COLORLUT];
   CreateLUTForFFT (abRed, abGreen, abBlue);

   // do this - not sure why..
   // BUGFIX - Take out dwWindowSize /= 2;

   // what draw?
   RECT rDraw;
   rDraw.top = rDraw.left = 0;
   rDraw.right = pr->right - pr->left;
   rDraw.bottom = pr->bottom - pr->top;

   HBITMAP  hBit, hBitOld;
   hBit = hBitOld = NULL;
   hBit = CreateCompatibleBitmap (hDC, rDraw.right, rDraw.bottom);
   BOOL  fWriteToMemory;
   fWriteToMemory = FALSE;
   BITMAP   bm;
   GetObject (hBit, sizeof(bm), &bm);
   HDC hDCDraw;
   hDCDraw = CreateCompatibleDC (hDC);

   // memory for image
   CMem memImage;
   PBYTE pb;
   if ((bm.bmPlanes == 1) && (bm.bmBitsPixel == 24)) {
      DeleteObject (hBit);

      fWriteToMemory = TRUE;
      memset (&bm, 0, sizeof(bm));
      bm.bmWidth = (DWORD)rDraw.right;
      bm.bmHeight = (DWORD)rDraw.bottom;
      bm.bmWidthBytes = (DWORD)rDraw.right * 3;
      if (bm.bmWidthBytes % 2)
         bm.bmWidthBytes++;
      bm.bmPlanes = 1;
      bm.bmBitsPixel = 24;
      if (!memImage.Required (bm.bmWidthBytes * (DWORD)rDraw.bottom))
         return;   // error
      bm.bmBits = memImage.p;
      pb = (BYTE*) bm.bmBits;
   }
   else
      SelectObject (hDCDraw, hBit);

   // make the window
   CMem memWindow;
   if (!memWindow.Required (dwWindowSize * sizeof(float)))
      return;  // error
   float *pafWindow;
   pafWindow = (float*) memWindow.p;
   CreateFFTWindow (3, pafWindow, dwWindowSize);

   // enough memory to store the FFT
   CMem memFFT;
   if (!memFFT.Required (max(SRDATAPOINTSDETAILED,max(SRFEATUREPCM,dwWindowSize/2)) * sizeof(float)))
         // BUGFIX - so enough memory
         // BUGFIX - Was just SRDATAPOINTS, not SRDATAPOINTSDETAILED
      return; // error
   float *pafFFT;
   pafFFT= (float*) memFFT.p;

   int x, y;
   double fCur, fDelta, fCurY, fDeltaY;
   float fVal;
   int index;
   DWORD dwDrawWindow;
#define SUPERSAMPPITCH     4     // supersample pitch by this much
      // BUGFIX - Was 2, but upped to 4 when reduced window size

   switch (dwPaintWhat) {
   case 1:  // frequency analysis
      dwDrawWindow = 500 * dwWindowSize * SUPERSAMPPITCH / m_dwSamplesPerSec;
      dwDrawWindow = min(dwWindowSize/4, dwDrawWindow);   // only lowest frequency limits
      break;
   case 3:  // stretch FFT to fit octave
   case 4:  // sr all
   case 6:  // sr voiced
   case 7:  // sr unvoiced
      dwDrawWindow = SRDATAPOINTS;
      break;
   case 8:  // sr phase
      dwDrawWindow = SRPHASENUM;
      break;
   case 9:  // PCM
      dwDrawWindow = SRFEATUREPCM;
      break;
   case 10: // phase by pitch
      dwDrawWindow = SRDATAPOINTSDETAILED ;
      break;
   default:
      dwDrawWindow = dwWindowSize / 2;
      break;
   }

   if (pfFreqRange) {
      *pfFreqRange = (fp)dwDrawWindow / (fp)dwWindowSize * (fp)m_dwSamplesPerSec;

      switch (dwPaintWhat) {
      case 1:  // frequency analysis
         *pfFreqRange = *pfFreqRange / (fp)SUPERSAMPPITCH;
         break;
      case 3:  // stretch FFT to fit octave
      case 4:  // sr all
      case 6:  // sr voiced
      case 7:  // sr unvoiced
      case 10: // phase by pitch
         *pfFreqRange = SROCTAVE;
         break;
      case 8:  // phase
         *pfFreqRange = SRPHASENUM;
         break;
      case 9:
         *pfFreqRange = SRFEATUREPCM;
         break;
      }
   }

   fDelta = (fRight - fLeft) / (double) rDraw.right;
   fDeltaY = (fBottom - fTop) / (double) rDraw.bottom * (fp)dwDrawWindow;
   CMem memPitchSpectrum;
   CSinLUT SinLUT;
   CMem memFFTScratch;

   // BUGFIX - If doing pitch spectrum then all in one block
   if (dwPaintWhat == 1)
      AnalyzePitchSpectrumBlock (rDraw.right, fLeft, fDelta, dwChannel, &memPitchSpectrum, &dwDrawWindow, &dwWindowSize, NULL);

#define NUMMAXTOSHOW       3
   for (x = 0, fCur = fLeft; x < rDraw.right; x++, fCur += fDelta) {
      DWORD adwMax[3];
      switch (dwPaintWhat) {
      case 1:  // frequency analysis
         {
            pafFFT = (float*)memPitchSpectrum.p + (x * dwDrawWindow);
            //AnalyzePitchSpectrum ((int)fCur, dwChannel, dwWindowSize, pafWindow, dwDrawWindow,
            //   1.0 / (fp) SUPERSAMPPITCH, pafFFT, &memPitchSpectrum, &SinLUT, &memFFTScratch);

            // figure out the max
            DWORD adwMinLow[NUMMAXTOSHOW], adwMinHigh[NUMMAXTOSHOW];
            DWORD k, dwOrd, m;
            for (k = 0; k < NUMMAXTOSHOW; k++) {
               adwMinLow[k] = adwMinHigh[k] = 0;
               adwMax[k] = -1;
            }

            for (dwOrd = 0; dwOrd < NUMMAXTOSHOW; dwOrd++) {
               for (y = 0; y < (int)dwDrawWindow; y++) {
                  for (m = 0; m < dwOrd; m++)
                     if (((DWORD)y >= adwMinLow[m]) && ((DWORD)y <= adwMinHigh[m]))
                        break;
                  if (m < dwOrd)
                     continue;     // skip

                  if (pafFFT[y] < 1)
                     continue;   // too quiet
                  if ((adwMax[dwOrd] ==-1) || (pafFFT[y] > pafFFT[adwMax[dwOrd]]))
                     adwMax[dwOrd] = y;
               } // y
               if (adwMax[dwOrd] == (DWORD)-1)
                  adwMax[dwOrd] = 0;

               // find the local minima below dwMax
               adwMinLow[dwOrd] = 0;
               if (adwMax[dwOrd]) for (k = adwMax[dwOrd]-1; k; k--)
                  if (!pafFFT[k] || (pafFFT[k] < pafFFT[k-1])) {
                     adwMinLow[dwOrd] = k;
                     break;
                  }

               // find the local minima above dwMax
               adwMinHigh[dwOrd] = dwDrawWindow-1;
               for (k = adwMax[dwOrd]+1; k+1 < dwDrawWindow; k++)
                  if (!pafFFT[k] || (pafFFT[k] < pafFFT[k+1])) {
                     adwMinHigh[dwOrd] = k;
                     break;
                  }

               adwMinHigh[dwOrd] = (WORD) adwMinHigh[dwOrd] +2;
               adwMinLow[dwOrd] = (adwMinLow[dwOrd] > 2) ? (adwMinLow[dwOrd]-2) : 0;
            } // dwOrd, ordinals of max


         }
         break;
      case 2:  // stretch fft
         StretchFFT (dwWindowSize/2, (int)fCur, dwChannel, pafFFT, NULL, NULL, 0, 0, 0,
            &SinLUT, &memFFTScratch);
         break;
      case 3:  // stretch fft fit octave
         StretchFFT (dwDrawWindow, (int)fCur, dwChannel, pafFFT, NULL, NULL, SRBASEPITCH,
            SRPOINTSPEROCTAVE, SRPOINTSPEROCTAVE/2,
            &SinLUT, &memFFTScratch);
         break;
      case 4:  // sr all
      case 6:  // sr voiced
      case 7:  // sr unvoiced
         // find the SR data
         if (!m_dwSRSamples) {
            memset (pafFFT, 0, dwDrawWindow * sizeof(float));
         }
         else {
            // have data
            double fLoc = fCur / (double) m_dwSRSkip;
            fLoc = min(fLoc, (double)m_dwSRSamples-1);
            fLoc = max(0,fLoc);

            DWORD dwLoc;
            dwLoc = (DWORD) fLoc;
            DWORD i;
            for (i = 0; i < SRDATAPOINTS; i++) {
               fVal = 0;
               if (dwPaintWhat != 7)
                  fVal += pow(10, (fp)m_paSRFeature[dwLoc].acVoiceEnergy[i] / 20.0);
               if (dwPaintWhat != 6)
                  fVal += pow(10, (fp)m_paSRFeature[dwLoc].acNoiseEnergy[i] / 20.0);  // convert from dB
               fVal *= 32767.0;
               pafFFT[i] = fVal;
            }
         }
         break;
      case 8:  // phase
         if (!m_dwSRSamples) {
            memset (pafFFT, 0, dwDrawWindow * sizeof(float));
         }
         else {
            // have data
            double fLoc = fCur / (double) m_dwSRSkip;
            fLoc = min(fLoc, (double)m_dwSRSamples-1);
            fLoc = max(0,fLoc);

            DWORD dwLoc;
            dwLoc = (DWORD) fLoc;
            DWORD i;
            for (i = 0; i < SRPHASENUM; i++)
               pafFFT[i] = (fp)m_paSRFeature[dwLoc].abPhase[i] / 255.0 * 3.0;
         }
         break;
      case 10:  // phase by pitch
         if (!m_dwSRSamples || !m_adwPitchSamples[PITCH_F0] || (m_dwSRSamples != m_adwPitchSamples[PITCH_F0])) {
            memset (pafFFT, 0, dwDrawWindow * sizeof(float));
         }
         else {
            // have data
            double fLoc = fCur / (double) m_dwSRSkip;
            fLoc = min(fLoc, (double)m_dwSRSamples-1);
            fLoc = max(0,fLoc);

            DWORD dwLoc;
            dwLoc = (DWORD) fLoc;

            SRDETAILEDPHASE SDP;
            SRDETAILEDPHASEFromSRFEATURE (m_paSRFeature + dwLoc, max(m_apPitch[PITCH_F0][dwLoc].fFreq, CLOSE), &SDP);

            DWORD i;
            for (i = 0; i < SRDATAPOINTSDETAILED; i++) {
               pafFFT[i] = atan2(SDP.afVoicedPhase[i][0], SDP.afVoicedPhase[i][1]);
               pafFFT[i] = myfmod(pafFFT[i] / (2.0 * PI), 1.0) * 3.0;
            }
         }
         break;
      case 9:  // PCM
         if (!m_dwSRSamples) {
            memset (pafFFT, 0, dwDrawWindow * sizeof(float));
         }
         else {
            // have data
            double fLoc = fCur / (double) m_dwSRSkip;
            fLoc = min(fLoc, (double)m_dwSRSamples-1);
            fLoc = max(0,fLoc);

            DWORD dwLoc;
            dwLoc = (DWORD) fLoc;
            DWORD i;
            for (i = 0; i < SRFEATUREPCM; i++)
#ifdef SRFEATUREINCLUDEPCM_SHORT
               pafFFT[i] = (fp)abs(m_paSRFeature[dwLoc].asPCM[i]) * m_paSRFeature[dwLoc].fPCMScale * 10.0 /* typically 1/10th full scale */ * 5000.0 /* tyical harm energy */;
#else
               pafFFT[i] = (fp)abs(m_paSRFeature[dwLoc].acPCM[i]) * m_paSRFeature[dwLoc].fPCMScale * 10.0 /* typically 1/10th full scale */ * 5000.0 /* tyical harm energy */;
#endif
         }
         break;
      default:
         FFT (dwWindowSize, (int)fCur, dwChannel, pafWindow, pafFFT, &SinLUT, &memFFTScratch, NULL, 0);
         break;
      } // dwPaintWhat

      fCurY = (1.0 - fTop) * (fp)dwDrawWindow;
      for (y = 0; y < rDraw.bottom; y++, fCurY -= fDeltaY) {
         index = (int) fCurY;
         index = max(0,index);
         index = min((int)dwDrawWindow-1,index);
         fVal = pafFFT[index];
         BYTE ab[3];

         if ((dwPaintWhat == 8) || (dwPaintWhat == 10)) {
            // paint phase as a rainbow
            ab[0] = ab[1] = ab[2] = 0;
            if (fVal <= 1) {
               ab[0] = (BYTE)(255 - fVal * 255);
               ab[1] = 255 - ab[0];
            }
            else if (fVal <= 2) {
               fVal -= 1;
               ab[1] = (BYTE)(255 - fVal * 255);
               ab[2] = 255 - ab[1];
            }
            else {   // fVal<=3
               fVal -= 2;
               ab[2] = (BYTE)(255 - fVal * 255);
               ab[0] = 255 - ab[2];
            }
         }
         else if (dwPaintWhat == 9) {
            // painting PCM
            fVal /= 32768.0;  // max range
            fVal *= COLORLUT;
            fVal = max(fVal, 0);
            fVal = min(fVal, COLORLUT-1);
            WORD  bVal;
            bVal = (WORD) fVal;

            ab[0] = abRed[bVal];
            ab[1] = abGreen[bVal];
            ab[2] = abBlue[bVal];
         }
         else {
            // scale convertions
            fVal = max(1.0, fVal);
            fVal = log10 (fVal / 32767.0) * 20;
            // fVal = max(fVal, SRNOISEFLOOR); // to test min
            fVal = (fVal / 96 + 1); // so it ranges between 0 and 1

            // convert to word
            WORD  bVal;
            fVal = fVal * COLORLUT;
            fVal = min(fVal, COLORLUT-1);
            fVal = max(fVal, 0);
            bVal = (WORD) fVal;

            ab[0] = abRed[bVal];
            ab[1] = abGreen[bVal];
            ab[2] = abBlue[bVal];
         }

         if (dwPaintWhat == 1) {
            if (index == adwMax[0])
               // highest white
               ab[0] = ab[1] = ab[2] = 0xff;
            else if (index == adwMax[1]) {
               // second highest
               ab[0] = ab[1] = 0xff;
               ab[2] = 0;
            }
            else if (index == adwMax[2]) {
               // third highest
               ab[0] = 0xff;
               ab[1] = ab[2] = 0;
            }
         }

         if (fWriteToMemory) {
            DWORD dwBitsIndex;
            dwBitsIndex = (DWORD)x*3 + (DWORD)y * bm.bmWidthBytes;
            pb[dwBitsIndex] = ab[2]; // blue
            pb[dwBitsIndex+1] =  ab[1];
            pb[dwBitsIndex+2] = ab[0];
         }
         else
            SetPixel (hDCDraw, x, y, RGB(ab[0], ab[1], ab[2]));

      } // y
   } // x

   if (fWriteToMemory) {
      // create the bitmap
      hBit = CreateBitmapIndirect (&bm);
      if (!hBit)
         return;
      SelectObject (hDCDraw, hBit);
   }

   // create a bitmap to draw this in
   BitBlt (hDC, pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top,
      hDCDraw, 0, 0, SRCCOPY);
   DeleteDC (hDCDraw);
   DeleteObject (hBit);
}


/****************************************************************************
CM3DWave::ReplaceSection - Replaces all the audio from dwStart to dwEnd
with the audio from pWave. This returns an error if the data format is incompatile
or something.

inputs
   DWORD          dwStart - Start sample
   DWORD          dwEnd - End sample
   PCM3DWave      pWave - Copy contents of this over start to end of wave
                        This can be NULL, in which case the info is deleted
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::ReplaceSection (DWORD dwStart, DWORD dwEnd, PCM3DWave pWave)
{
   // if wrong format error
   if (pWave)
      if ((pWave->m_dwChannels != m_dwChannels) || (pWave->m_dwSamplesPerSec != pWave->m_dwSamplesPerSec))
         return FALSE;

   // destroys calculated info
   ReleaseCalc ();

   dwEnd = min(dwEnd, m_dwSamples);
   dwStart = min(dwStart, dwEnd);

   // how much need
   DWORD dwNeed;
   DWORD dwSamples;
   dwSamples = pWave ? pWave->m_dwSamples : 0;
   dwNeed = (dwSamples + m_dwSamples - (dwEnd - dwStart)) * (m_dwChannels * sizeof(short));
   if (!m_pmemWave->Required (dwNeed))
      return FALSE;  // error
   m_psWave = (short*) m_pmemWave->p;

   // move up
   memmove (m_psWave + (dwStart + dwSamples) * m_dwChannels, m_psWave + dwEnd * m_dwChannels,
      (m_dwSamples - dwEnd) * m_dwChannels * sizeof(short));
   if (pWave)
      memcpy (m_psWave + dwStart * m_dwChannels, pWave->m_psWave,
         pWave->m_dwSamples * m_dwChannels * sizeof(short));

   // remove phonemes and then insert
   PhonemeDelete (dwStart, dwEnd);
   WordDelete (dwStart, dwEnd);
   if (pWave) {
      PhonemeInsert (dwStart, pWave->m_dwSamples, 0, &pWave->m_lWVPHONEME);
      WordInsert (dwStart, pWave->m_dwSamples, 0, &pWave->m_lWVWORD);
   }

   // update samples
   m_dwSamples = dwNeed / (m_dwChannels * sizeof(short));

   return TRUE;
}


/****************************************************************************
CM3DWave::Copy - Clones the object, but only copies some of the samples.

inputs
   DWORD          dwStart - Start sample
   DWORD          dwEnd - End sample
returns
   PCM3DWave - Wave
*/
PCM3DWave CM3DWave::Copy (DWORD dwStart, DWORD dwEnd)
{
   PCM3DWave pNew = new CM3DWave;
   if (!pNew)
      return NULL;

   dwEnd = min(dwEnd, m_dwSamples);
   dwStart = min(dwStart, dwEnd);

   strcpy (pNew->m_szFile, m_szFile);
   pNew->m_fDirty = m_fDirty;
   pNew->m_dwSamplesPerSec = m_dwSamplesPerSec;
   pNew->m_dwSamples = dwEnd - dwStart;
   pNew->m_dwChannels = m_dwChannels;

   // text
   MemZero(&pNew->m_memSpoken);
   MemCat (&pNew->m_memSpoken, (PWSTR)m_memSpoken.p);
   MemZero(&pNew->m_memSpeaker);
   MemCat (&pNew->m_memSpeaker, (PWSTR)m_memSpeaker.p);
   //MemZero(&pNew->m_memSoundsLike);
   //MemCat (&pNew->m_memSoundsLike, (PWSTR)m_memSoundsLike.p);

   PWAVEFORMATEX pwfex;
   pwfex = (PWAVEFORMATEX) m_memWFEX.p;
   if (!pNew->m_memWFEX.Required (sizeof(WAVEFORMATEX) + pwfex->cbSize)) {
      delete pNew;
      return FALSE;
   }
   memcpy (pNew->m_memWFEX.p, m_memWFEX.p, sizeof(WAVEFORMATEX) + pwfex->cbSize);

   if (m_pmemWave) {
      DWORD dwCopy = pNew->m_dwSamples;
      DWORD dwSize = dwCopy * m_dwChannels * sizeof(short);
      if (!pNew->m_pmemWave) {
         pNew->m_pmemWave = new CMem;
         if (!pNew->m_pmemWave) {
            delete pNew;
            return FALSE;
         }
      }
      if (!pNew->m_pmemWave->Required (dwSize)) {
         delete pNew;
         return FALSE;
      }
      memcpy (pNew->m_pmemWave->p, (short*)m_pmemWave->p + dwStart * m_dwChannels, dwSize);

      pNew->m_psWave = (short*) pNew->m_pmemWave->p;
   }
   else {
      if (pNew->m_pmemWave)
         delete pNew->m_pmemWave;
      pNew->m_pmemWave = NULL;
      pNew->m_psWave = NULL;
   }

   // copy the phonemes
   pNew->m_lWVPHONEME.Clear();
   pNew->m_lWVWORD.Clear();
   pNew->PhonemeInsert (0, dwEnd - dwStart, dwStart, &m_lWVPHONEME);
   pNew->WordInsert (0, dwEnd - dwStart, dwStart, &m_lWVWORD);

   return pNew;
}

/****************************************************************************
CM3DWave::MakePCM - Convert the waveformatex info to just PCM so that wont
compress when writing to clipboard
*/
void CM3DWave::MakePCM (void)
{
   PWAVEFORMATEX pwfex = (PWAVEFORMATEX) m_memWFEX.p;
   pwfex->cbSize = 0;
   pwfex->wFormatTag = WAVE_FORMAT_PCM;
   pwfex->nChannels = m_dwChannels;
   pwfex->nSamplesPerSec = m_dwSamplesPerSec;
   pwfex->wBitsPerSample = 16;
   pwfex->nBlockAlign  = pwfex->nChannels * pwfex->wBitsPerSample / 8;
   pwfex->nAvgBytesPerSec = pwfex->nBlockAlign * pwfex->nSamplesPerSec;
}


/****************************************************************************
CM3DWave::Decimate - Convers from the sampling rate dwOrig to the new
sampling rate dwNew by skipping (or duplicating) samples.

inputs
   DWORD          dwOrig - Original sampling rate
   DWORD          dwNew - New sampling rate
   BOOL           fInterp - If TRUE then interpolate, else just copy sample
   PCProgressSocket     pProgress - Progress bar. MIght be NULL
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::Decimate (DWORD dwOrig, DWORD dwNew, BOOL fInterp, PCProgressSocket pProgress)
{
   // figure out how large dest needs to be
   DWORD dwNewSamp = (DWORD)((__int64)m_dwSamples * (__int64)dwNew / (__int64)dwOrig);

   if (dwOrig == dwNew)
      return TRUE;   // no change

   // destroys calculated info
   ReleaseCalc ();

   // create new memory with enough space
   PCMem pNew;
   short *psNew;
   pNew = new CMem;
   if (!pNew)
      return FALSE;
   DWORD dwNeed;
   dwNeed  = dwNewSamp * m_dwChannels * sizeof(short);
   if (!pNew->Required (dwNeed)) {
      delete pNew;
      return FALSE;
   }

   // loop
   double fCur, fInc, f;
   fInc = (double) dwOrig / (double)dwNew;
   DWORD i, j, dwChan;
   for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
      psNew = (short*) pNew->p;

      for (i = 0, fCur = 0; i < dwNewSamp; i++, fCur += fInc, psNew += m_dwChannels) {
         // progress
         if (((i % 100000) == 0) && pProgress)
            pProgress->Update ((fp) i / (fp) m_dwSamples);

         j = (DWORD) fCur;
         if (!fInterp || (j+1 >= m_dwSamples)) {
            psNew[dwChan] = m_psWave[j*m_dwChannels + dwChan];
            continue;
         }

         // else, interpolate
         f = fCur - floor(fCur);
         psNew[dwChan] = (short)((1.0 - f) * (double)m_psWave[j*m_dwChannels + dwChan] +
            f * (double)m_psWave[(j+1)*m_dwChannels + dwChan]);
      } // i
   } // dwChan


   // finally copy over
   delete m_pmemWave;
   m_pmemWave = pNew;
   m_psWave = (short*) pNew->p;
   m_dwSamples = dwNewSamp;
   // NOTE: Doesnt actually change sampling rate stored away

   // decimate phonemes
   PhonemeStretch ((double)dwNew / (double)dwOrig);
   WordStretch ((double)dwNew / (double)dwOrig);

   return TRUE;
}


/****************************************************************************
CM3DWave::ConvertChannels - Converts the wave to the given number of channels

NOTE: This doesnt ajust the WFEX because not sure if it's PCM or not. To
make sure the WFEX is adjusted, call ConvertWFEX()

inputs
   DWORD       dwChannels - New number of channels
   PCProgressSocket     pProgress - Progress bar. MIght be NULL
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::ConvertChannels (DWORD dwChannels, PCProgressSocket pProgress)
{
   if (dwChannels == m_dwChannels)
      return TRUE;

   // destroys calculated info
   ReleaseCalc ();

   // create new memory with enough space
   PCMem pNew;
   pNew = new CMem;
   if (!pNew)
      return FALSE;
   DWORD dwNeed;
   dwNeed  = m_dwSamples * dwChannels * sizeof(short);
   if (!pNew->Required (dwNeed)) {
      delete pNew;
      return FALSE;
   }

   DWORD i, j, k;
   short *psNew;
   psNew = (short*) pNew->p;
   int iSum, iCount;
   for (i = 0; i < m_dwSamples; i++) {
      // progress
      if (((i % 100000) == 0) && pProgress)
         pProgress->Update ((fp) i / (fp) m_dwSamples);

      for (j = 0; j < dwChannels; j++) {
         iSum = iCount = 0;
         for (k = j % m_dwChannels; k < m_dwChannels; k += dwChannels) {
            iSum += m_psWave[i * m_dwChannels + (k % m_dwChannels)];
            iCount++;
         } // k
         if (iCount >= 2)
            iSum /= iCount;
         iSum = max(iSum, -32768);
         iSum = min(iSum, 32767);
         psNew[i * dwChannels + j] = (short)iSum;
      } // j
   } // i


   // finally copy over
   delete m_pmemWave;
   m_pmemWave = pNew;
   m_psWave = (short*) pNew->p;
   m_dwChannels = dwChannels;
   // NOTE: Not adjusting WFEX

   return TRUE;
}


/****************************************************************************
CM3DWave::ConvertSamplesPerSec - Converts the wave to the given sampling rate

NOTE: This doesnt ajust the WFEX because not sure if it's PCM or not. To
make sure the WFEX is adjusted, call ConvertWFEX()

inputs
   DWORD       dwSamplesPerSec - New sampling rate
   PCProgressSocket     pProgress - Progress bar. MIght be NULL
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::ConvertSamplesPerSec (DWORD dwSamplesPerSec, PCProgressSocket pProgress)
{
   if (dwSamplesPerSec == m_dwSamplesPerSec)
      return TRUE;

   // destroys calculated info
   ReleaseCalc ();

   // find the min and max frequencies
   DWORD dwMin, dwMax;
   dwMin = min(dwSamplesPerSec, m_dwSamplesPerSec);
   dwMax = max(dwSamplesPerSec, m_dwSamplesPerSec);

   // if they're integer multiples then note
   BOOL fInteger;
   BOOL fRet;
   fInteger = ((DWORD)(dwMax / dwMin) * dwMin == dwMax);

   // un-decimate the original wave to the max frequency. If the max frequency is not
   // an integer multiple of the minimum one then go 2x as far
   DWORD dwUp;
   dwUp = dwMax * (fInteger ? 1 : 2);
   if (pProgress)
      pProgress->Push (0, .1);
   fRet = Decimate (m_dwSamplesPerSec, dwUp, FALSE, pProgress);
   if (pProgress)
      pProgress->Pop();
   if (!fRet)
      return fRet;
   m_dwSamplesPerSec = dwUp;
   
   // filter
   if (pProgress)
      pProgress->Push (.1, .9);
   fp afFreqBand[2], afAmplify[2];
   DWORD dwTimeBand;
   afFreqBand[1] = (fp)dwMin / 2.0; // if above this then set to 0
   afFreqBand[0] = afFreqBand[1] * .99;   // BUGFIX - Was .9
   dwTimeBand = 0;
   afAmplify[0] = 1;
   afAmplify[1] = 0;
   fRet = Filter (2, afFreqBand, 1, &dwTimeBand, afAmplify, pProgress);
   if (pProgress)
      pProgress->Pop();
   if (!fRet)
      return fRet;
   
   // decimate down to the final, with interp
   if (pProgress)
      pProgress->Push (.9, 1);
   fRet = Decimate (m_dwSamplesPerSec, dwSamplesPerSec, TRUE, pProgress);
   if (pProgress)
      pProgress->Pop();
   if (!fRet)
      return fRet;
   m_dwSamplesPerSec = dwSamplesPerSec;

   // NOTE: Not adjusting WFEX

   return TRUE;
}

/****************************************************************************
CM3DWave::ConvertWFEX - Called to convert the wave to a new WFEX. This will
do channel conversion, sampling rate conversion, and store away the WFEX.

inputs
   PWAVEFORMATEX        pwfex - New WFEX
   PCProgressSocket     pProgress - Progress bar. MIght be NULL
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::ConvertWFEX (PWAVEFORMATEX pwfex, PCProgressSocket pProgress)
{
   BOOL fRet;

   if (pwfex->nChannels != m_dwChannels) {
      if (pProgress)
         pProgress->Push (0, .1);

      fRet = ConvertChannels (pwfex->nChannels, pProgress);

      if (pProgress)
         pProgress->Pop ();
      if (!fRet)
         return fRet;
   }

   if (pwfex->nSamplesPerSec != m_dwSamplesPerSec) {
      if (pProgress)
         pProgress->Push (.1, 1);

      fRet = ConvertSamplesPerSec (pwfex->nSamplesPerSec, pProgress);

      if (pProgress)
         pProgress->Pop ();
      if (!fRet)
         return fRet;
   }

   // copy over new wave format
   if (!m_memWFEX.Required (pwfex->cbSize + sizeof (WAVEFORMATEX)))
      return FALSE;
   memcpy (m_memWFEX.p, pwfex, pwfex->cbSize + sizeof(WAVEFORMATEX));

   if (pProgress)
      pProgress->Update (1);

   return TRUE;
}


/****************************************************************************
CM3DWave::ConvertSamplesAndChannels - Convert to a PCM form with the given
number of samples and channels. Useful for the blipboard functions to ensure
they're the same sampling rate and number of channels.

inputs
   DWORD                dwSamplesPerSec - Sampling rate to use
   DWORD                dwChannels - channels to use
   PCProgressSocket     pProgress - Progress bar. MIght be NULL
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::ConvertSamplesAndChannels (DWORD dwSamplesPerSec, DWORD dwChannels, PCProgressSocket pProgress)
{
   WAVEFORMATEX wfex;
   memset (&wfex, 0, sizeof(wfex));
   wfex.cbSize = 0;
   wfex.wFormatTag = WAVE_FORMAT_PCM;
   wfex.nChannels = dwChannels;
   wfex.nSamplesPerSec = dwSamplesPerSec;
   wfex.wBitsPerSample = 16;
   wfex.nBlockAlign  = wfex.nChannels * wfex.wBitsPerSample / 8;
   wfex.nAvgBytesPerSec = wfex.nBlockAlign * wfex.nSamplesPerSec;

   return ConvertWFEX (&wfex, pProgress);
}


/****************************************************************************
CM3DWave::Filter - Filters the wave.

inputs
   DWORD          dwFreqBands - Number of frequency band points.
   fp             *pafFreqBand - Pointer to an array of dwFreq band that control
                     the frequency (in Hz) that the band is at. These must be
                     sorted in ascending order. If a frequency appears
                     below the lowest frequency listed it will be treated as the lowest.
                     Similarly for highest.
   DWORD          dwTimeBands - Number of points over time.
   DWORD          *padwTimeBand - Pointer to an array of dwTimeBand that controls
                     the time of the band. These must be increasing order. If audio
                     occurs before the time it will be treated if it occurred at the
                     first time band, and similarly for the highest
   fp             *pafAmplify - Array of dwTimeBands x dwFreqBands floats. Each one
                     represents the amplitude multiply (1.0 = no change) for the
                     frequency
   PCProgressSocket pProgress - Progress bar
   BOOL           fReleaseCalc - If TRUE then release calculated information because
                     filtering will change it. If FALSE keep.
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::Filter (DWORD dwFreqBands, fp *pafFreqBands, DWORD dwTimeBands, DWORD *padwTimeBands,
                       fp *pafAmplify, PCProgressSocket pProgress, BOOL fReleaseCalc)
{
   // destroys calculated info
   if (fReleaseCalc)
      ReleaseCalc ();

   // figure out window size
   DWORD dwWindowSize = WindowSize () * 4; // / 2;   // scale down so not so much CPU
      // BUGFIX - Was /2 to reduce CPU, but changed to 4* using for sub-voice filtering,
      // want to be more accurate, keep full windowsize
      // BUGFIX - Was /4, but made /2 so could get more accurate filtering
   dwWindowSize = max(dwWindowSize, 4);   // at least some window
   DWORD dwHalfWindow;
   dwHalfWindow = dwWindowSize / 2;

   // allocate some memory
   CMem memFreq, memWindow, memFFT, memCurFilter;
   fp *pafFreq, *pafCurFilter;
   float *pafWindow, *pafFFT;
   if (!memFreq.Required (dwFreqBands * sizeof(fp)))
      return FALSE;
   if (!memCurFilter.Required (dwFreqBands * sizeof(fp)))
      return FALSE;
   if (!memWindow.Required (dwWindowSize * sizeof(float)))
      return FALSE;
   if (!memFFT.Required (dwWindowSize * sizeof(float)))
      return FALSE;
   pafFreq = (fp*) memFreq.p;
   pafWindow = (float*) memWindow.p;
   pafFFT = (float*) memFFT.p;
   pafCurFilter = (fp*) memCurFilter.p;

   // convert frequency bands to FFT frequencies
   DWORD i;
   for (i = 0; i < dwFreqBands; i++)
      pafFreq[i] = pafFreqBands[i] / ((fp) m_dwSamplesPerSec / 2.0) * (fp)dwWindowSize;

   // make the window... apply sqrt() since end up applying window twice
   DWORD dwChan, j;
   CreateFFTWindow (3, pafWindow, dwWindowSize);
   for (i = 0; i < dwWindowSize; i++)
      pafWindow[i] = sqrt(pafWindow[i]);

   // allocate space to transfer to
   // create new memory with enough space
   PCMem pNew;
   pNew = new CMem;
   if (!pNew)
      return FALSE;
   DWORD dwNeed;
   dwNeed  = m_dwSamples * m_dwChannels * sizeof(short);
   if (!pNew->Required (dwNeed)) {
      delete pNew;
      return FALSE;
   }
   short *psNew;
   psNew = (short*) pNew->p;
   memset (psNew, 0, dwNeed);


   CMem memFFTScratch;
   CSinLUT SinLUT;

   // loop
   int iSamp, iSum;
   for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
      if (pProgress)
         pProgress->Push ((fp)dwChan / (fp)m_dwChannels, (fp)(dwChan+1) / (fp)m_dwChannels);

      DWORD dwLastBand = 0;

      for (i = 0; i < m_dwSamples + dwHalfWindow; i += dwHalfWindow) {

         // progress
         if (pProgress && ((i % 1000) == 0))
            pProgress->Update ((fp) i / (fp) (m_dwSamples+dwHalfWindow));

         // calculate the filter for here
         if (i < padwTimeBands[0]) {
            for (j = 0; j < dwFreqBands; j++)
               pafCurFilter[j] = pafAmplify[0 * dwFreqBands + j];
         }
         else if (i >= padwTimeBands[dwTimeBands-1]) {
            for (j = 0; j < dwFreqBands; j++)
               pafCurFilter[j] = pafAmplify[(dwTimeBands-1) * dwFreqBands + j];
         }
         else {
            // interpolate
            DWORD dwBand;
            DWORD dwNext;
            for (dwBand = dwLastBand; dwBand+1 < dwTimeBands; dwBand++) {
               if ((i >= padwTimeBands[dwBand]) && (i <= padwTimeBands[dwBand+1]))  // BUGFIX - Was i >=, which was wrong
                  break;
            }
            dwLastBand = dwBand = min(dwBand, dwTimeBands-1);
            dwNext = min(dwBand+1, dwTimeBands-1);

            fp fAlpha;
            if (dwBand != dwNext)
               fAlpha = ((fp)i - (fp)padwTimeBands[dwBand]) / (fp)(padwTimeBands[dwBand+1]-padwTimeBands[dwBand]);
            else
               fAlpha = 0;

            for (j = 0; j < dwFreqBands; j++)
               pafCurFilter[j] = (1.0 - fAlpha) * pafAmplify[dwBand*dwFreqBands+j] +
                  fAlpha * pafAmplify[dwNext*dwFreqBands+j];
         }

         // get all the values so can pass into fft
         for (j = 0; j < dwWindowSize; j++) {
            iSamp = (int)i + (int)j - (int)dwHalfWindow;
            if ((iSamp >= 0) && (iSamp < (int)m_dwSamples))
               pafFFT[j] = (float) m_psWave[iSamp * (int)m_dwChannels + (int)dwChan] * pafWindow[j];
            else
               pafFFT[j] = 0;
         } // j

         // fft it
         FFTRecurseReal (pafFFT - 1, dwWindowSize, 1, &SinLUT, &memFFTScratch);

         // filter
         fp f, fAlpha, fDelta;
         DWORD dwBand;
         dwBand = 0;
         for (j = 0; j < dwWindowSize; j++) {
            f = (DWORD)(j / 2) * 2; // because in FFT get combined sin and cos
            if (f <= pafFreq[0]) {
               pafFFT[j] *= pafCurFilter[0];
               continue;
            }
            else if (f >= pafFreq[dwFreqBands-1]) {
               pafFFT[j] *= pafCurFilter[dwFreqBands-1];
               continue;
            }

            // else, interpolte
            while ((dwBand+1 < dwFreqBands) && (f >= pafFreq[dwBand+1]))
               dwBand++;
            fDelta = pafFreq[dwBand+1] - pafFreq[dwBand];
            if (fDelta <= EPSILON)
               continue;   // error
            fAlpha = (f - pafFreq[dwBand]) / fDelta;
            pafFFT[j] *= (1.0 - fAlpha) * pafCurFilter[dwBand] +
               fAlpha * pafCurFilter[dwBand+1];
         } // j

         // un-fft it
         FFTRecurseReal (pafFFT - 1, dwWindowSize, -1, &SinLUT, &memFFTScratch);

         // return it
         for (j = 0; j < dwWindowSize; j++) {
            iSamp = (int)i + (int)j - (int)dwHalfWindow;
            if ((iSamp < 0) || (iSamp >= (int)m_dwSamples))
               continue;

            iSum = (int) (pafFFT[j] * pafWindow[j] / (float)dwWindowSize);
            iSum += psNew[iSamp * (int)m_dwChannels + (int)dwChan];
            iSum = max(iSum, -32768);
            iSum = min(iSum, 32767);
            psNew[iSamp * (int)m_dwChannels + (int)dwChan] = (short)iSum;
         } // j
         
      } // i

      if (pProgress)
         pProgress->Pop();
   } // channel

   // finally copy over
   delete m_pmemWave;
   m_pmemWave = pNew;
   m_psWave = (short*) pNew->p;

   return TRUE;

}

/****************************************************************************
CM3DWave::CalcEnergy - Calculates the enegy for the wave. THis ends up filling
in m_dwEnergySamples, m_dwEnergySkip, m_pwEnergy, and m_memEnergy.

inputs
   PCProgressSocket     pProgress - Progress bar
returns
   BOOL - TRUE if successful
*/
BOOL CM3DWave::CalcEnergy (PCProgressSocket pProgress)
{
   // how much need
   DWORD dwWindowSize = WindowSize() / 4; // BUGFIX - Make window size / 4, was /2, but need more accurate time
   m_dwEnergySkip = dwWindowSize / 2;
   m_dwEnergySamples = m_dwSamples / m_dwEnergySkip + 1;
   if (!m_memEnergy.Required (m_dwEnergySamples * m_dwChannels * sizeof(WORD))) {
      m_dwEnergySamples = 0;
      return FALSE;
   }
   m_pwEnergy = (WORD*) m_memEnergy.p;


   // create the window
   CMem memWindow;
   float *pafWindow;
   if (!memWindow.Required (dwWindowSize * sizeof(float)))
      return FALSE;
   pafWindow = (float*) memWindow.p;
   CreateFFTWindow (3, pafWindow, dwWindowSize);

   // loop
   DWORD dwChan, i, j;
   int iSamp;
   for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
      if (pProgress)
         pProgress->Push ((fp)dwChan / (fp)m_dwChannels, (fp)(dwChan+1)/(fp)m_dwChannels);

      for (i = 0; i < m_dwEnergySamples; i++) {
         if (pProgress && ((i%10000) == 0))
            pProgress->Update ((fp)i / (fp)m_dwEnergySamples);

         // BUGFIX - Remove DC offset in energy calculations since really messes up
         fp fDC;
         DWORD dwDCCount;
         fp fEnergy, fVal;
         dwDCCount = 0;
         fDC = 0;
         for (j = 0; j < dwWindowSize; j++) {
            iSamp = ((int)i - 1) * (int) m_dwEnergySkip + (int)j; // so centerd on the sample
            if ((iSamp < 0) || (iSamp >= (int)m_dwSamples))
               continue;
            fVal = m_psWave[iSamp * (int)m_dwChannels + (int)dwChan];
            fDC += fVal;
            dwDCCount++;
         }
         if (dwDCCount)
            fDC /= (fp)dwDCCount;

         // calculate the energy
         fEnergy = 0;
         for (j = 0; j < dwWindowSize; j++) {
            iSamp = ((int)i - 1) * (int) m_dwEnergySkip + (int)j; // so centerd on the sample
            if ((iSamp < 0) || (iSamp >= (int)m_dwSamples))
               continue;
            fVal = (fp) m_psWave[iSamp * (int)m_dwChannels + (int)dwChan] - fDC;
            fVal *= fVal;
            fVal *= pafWindow[j];
            fEnergy += fVal;
         } // j

         fEnergy = sqrt(fEnergy);
         fEnergy /= sqrt((fp)dwWindowSize);
         fEnergy *= 2;  // so if is max (32767), ends up going from 0 to 0xffff
         fEnergy = min(fEnergy, (fp)0xffff);
         m_pwEnergy[i*m_dwChannels+dwChan] = (WORD)fEnergy;
      } // i

      if (pProgress)
         pProgress->Pop();
   } // dwChan

   return TRUE;
}


/****************************************************************************
CM3DWave::PaintEnergy - This draws the energy graph onto a HDC.

inputs
   HDC            hDC - To draw on
   RECT           *pr - Rectangle on the HDC
   double         fLeft, fRight - Left and right sides of HDC correspond to these sample
                  numbers. The samples may extend beyond the wave
   double         fTop, fBottom - Top and bottom of HDC correspond from these values,
                  from 1 to 0
   DWORD          dwChannel - Channel to draw, or -1 for combined
returns
   none
*/
void CM3DWave::PaintEnergy (HDC hDC, RECT *pr, double fLeft, double fRight,
                          double fTop, double fBottom, DWORD dwChannel)
{
   if ((pr->right - pr->left <= 0) || (pr->bottom - pr->top <= 0))
      return;  // nothing to draw
   if ((fRight <= fLeft + EPSILON) || (fTop > fBottom - EPSILON))
      return;  // cant to this either

   if (!m_dwEnergySamples) {
      if (!CalcEnergy (NULL))
         return;
   }

   // convert sample numbers
   fLeft /= (double)m_dwEnergySkip;
   fRight /= (double)m_dwEnergySkip;

   // create a bitmap to draw this in
   HBITMAP hBit;
   HDC hDCDraw;
   hBit = CreateCompatibleBitmap (hDC, pr->right - pr->left, pr->bottom - pr->top);
   hDCDraw = CreateCompatibleDC (hDC);
   SelectObject (hDCDraw, hBit);

   // blank backgroudn
   RECT rDraw;
   rDraw.top = rDraw.left = 0;
   rDraw.right = pr->right - pr->left;
   rDraw.bottom = pr->bottom - pr->top;
   HBRUSH hBrush;
   hBrush = CreateSolidBrush (RGB(0xff, 0xf0, 0xf0));
   FillRect (hDCDraw, &rDraw, hBrush);
   DeleteObject (hBrush);

   HPEN hPen, hPenOld;
   COLORREF crPen;

   int x;
   double fCur, fDelta;
   fp fVal;
   DWORD dwChanStart, dwChanEnd, dwChan;
   WORD wCur1, wCur2;
   fp fScale;
   fScale =  1.0 / (fTop - fBottom) * (fp) rDraw.bottom;
   fDelta = (fRight - fLeft) / (double) rDraw.right;
   dwChanStart = (dwChannel < m_dwChannels) ? dwChannel : 0;
   dwChanEnd = (dwChannel < m_dwChannels) ? (dwChannel+1) : m_dwChannels;
   for (dwChan = dwChanStart; dwChan < dwChanEnd; dwChan++) {
      crPen = RGB(0x40, 0x40, 0x40);
      if (m_dwChannels >= 2)
         switch (dwChan % 6) {
         case 0:
            crPen = RGB(0x80,0,0);
            break;
         case 1:
            crPen = RGB(0,0x80,0);
            break;
         case 2:
            crPen = RGB(0,0,0x80);
            break;
         case 3:
            crPen = RGB(0x80,0x80,0);
            break;
         case 4:
            crPen = RGB(0,0x80,0x80);
            break;
         case 5:
            crPen = RGB(0x80,0,0x80);
            break;
         }
      hPen = CreatePen (PS_SOLID, 0, crPen);
      hPenOld = (HPEN) SelectObject (hDCDraw, hPen);

      for (x = 0, fCur = fLeft; x < rDraw.right; x++, fCur += fDelta) {
         int iSamp;
         iSamp = (int)floor(fCur);
         if (iSamp < (int)m_dwEnergySamples)
            wCur1 = m_pwEnergy[(DWORD)iSamp * m_dwChannels + dwChan];
         else
            wCur1 = 0;
         if (iSamp+1 < (int)m_dwEnergySamples)
            wCur2 = m_pwEnergy[(DWORD)(iSamp+1) * m_dwChannels + dwChan];
         else
            wCur2 = 0;

         // interpolate
         fVal = fCur - iSamp;
         fVal = (1.0 - fVal) * (fp)wCur1 + fVal * (fp)wCur2;

         // scale convertions
         fVal = max(1.0, fVal);
         fVal = log10 (fVal / (fp)0xffff) * 20;
         fVal = (fVal / 96 + 1); // so it ranges between 0 and 1
         fVal = max(fVal, 0);
         fVal = 1.0 - fVal;   // so quiet on bottom

         // translate min and max to a line
         int iY;
         iY = (int)((fTop - fVal) * fScale) + rDraw.top;
         if (x)
            LineTo (hDCDraw, x, iY);
         else
            MoveToEx (hDCDraw, x, iY, NULL);
      } // x

      SelectObject (hDCDraw, hPenOld);
      DeleteObject (hPen);
   } // dwChan



   // draw
   BitBlt (hDC, pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top,
      hDCDraw, 0, 0, SRCCOPY);
   DeleteDC (hDCDraw);
   DeleteObject (hBit);
}


/****************************************************************************
CM3DWave::PaintPitch - This draws the Pitch graph onto a HDC.

inputs
   HDC            hDC - To draw on
   RECT           *pr - Rectangle on the HDC
   double         fLeft, fRight - Left and right sides of HDC correspond to these sample
                  numbers. The samples may extend beyond the wave
   double         fTop, fBottom - Top and bottom of HDC correspond from these values,
                  from 1 to 0
   DWORD          dwChannel - Channel to draw, or -1 for combined
   HWND           hWnd - To display progress bar for painting pitch on
returns
   none
*/
void CM3DWave::PaintPitch (HDC hDC, RECT *pr, double fLeft, double fRight,
                          double fTop, double fBottom, DWORD dwChannel, HWND hWnd)
{
   if ((pr->right - pr->left <= 0) || (pr->bottom - pr->top <= 0))
      return;  // nothing to draw
   if ((fRight <= fLeft + EPSILON) || (fTop > fBottom - EPSILON))
      return;  // cant to this either
   fp fLeftOrig = fLeft;
   fp fRightOrig = fRight;

   CalcPitchIfNeeded (WAVECALC_SEGMENT, hWnd);
   // BUGBUG - will need to calc pitchsub too

   // create a bitmap to draw this in
   HBITMAP hBit;
   HDC hDCDraw;
   hBit = CreateCompatibleBitmap (hDC, pr->right - pr->left, pr->bottom - pr->top);
   hDCDraw = CreateCompatibleDC (hDC);
   SelectObject (hDCDraw, hBit);

   // blank backgroudn
   RECT rDraw;
   rDraw.top = rDraw.left = 0;
   rDraw.right = pr->right - pr->left;
   rDraw.bottom = pr->bottom - pr->top;
   HBRUSH hBrush;
   hBrush = CreateSolidBrush (RGB(0xf0, 0xf0, 0xff));
   FillRect (hDCDraw, &rDraw, hBrush);
   DeleteObject (hBrush);

   DWORD dwPitchSub;
   for (dwPitchSub = PITCH_NUM-1; dwPitchSub < PITCH_NUM; dwPitchSub--) {
      if (!m_adwPitchSamples[dwPitchSub])
         continue;

      HPEN hPenOld;

      // create all the pens
      HPEN     ahPenColor[16];
      BYTE bc;
      DWORD dwLastPen, dwPen, i;
      for (i = 0; i < 16; i++) {
         bc = (15 - i) * 15;  // note: not using 16 because lines too dim
         ahPenColor[i] = CreatePen (PS_SOLID, i/4+1,
            dwPitchSub ? RGB (0xff, bc, bc) : RGB(bc,bc,bc));
      }
      hPenOld = (HPEN) SelectObject (hDCDraw, ahPenColor[dwLastPen = 0]);

      // convert sample numbers
      fLeft = fLeftOrig / (double)m_adwPitchSkip[dwPitchSub];
      fRight = fRightOrig / (double)m_adwPitchSkip[dwPitchSub];

      int x;
      double fCur, fDelta;
      fp fAlpha, fFreq, fStrength;
      DWORD dwChanStart, dwChanEnd, dwChan;
      float afCurFreq[2];
      float afCurStrength[2];
      fp fScale;
      fScale =  1.0 / (fBottom - fTop) * (fp) rDraw.bottom;
      fDelta = (fRight - fLeft) / (double) rDraw.right;
      dwChanStart = (dwChannel < m_dwChannels) ? dwChannel : 0;
      dwChanEnd = (dwChannel < m_dwChannels) ? (dwChannel+1) : m_dwChannels;
      for (dwChan = dwChanStart; dwChan < dwChanEnd; dwChan++) {
         for (x = 0, fCur = fLeft; x < rDraw.right; x++, fCur += fDelta) {
            int iSamp;
            iSamp = (int)floor(fCur);
            if (iSamp < (int)m_adwPitchSamples[dwPitchSub]) {
               afCurFreq[0] = m_apPitch[dwPitchSub][(DWORD)iSamp * m_dwChannels + dwChan].fFreq;
               afCurStrength[0] = m_apPitch[dwPitchSub][(DWORD)iSamp * m_dwChannels + dwChan].fStrength;
            }
            else {
               afCurFreq[0] = 0;
               afCurStrength[0] = 0;
            }
            if (iSamp+1 < (int)m_adwPitchSamples[dwPitchSub]) {
               afCurFreq[1] = m_apPitch[dwPitchSub][(DWORD)(iSamp+1) * m_dwChannels + dwChan].fFreq;
               afCurStrength[1] = m_apPitch[dwPitchSub][(DWORD)(iSamp+1) * m_dwChannels + dwChan].fStrength;
            }
            else {
               afCurFreq[1] = 0; // BUGFIX - Was using [0]
               afCurStrength[1] = 0;
            }

            // interpolate
            fAlpha = fCur - iSamp;
            fFreq = (1.0 - fAlpha) * afCurFreq[0] + fAlpha * afCurFreq[1];
            fStrength = (1.0 - fAlpha) * afCurStrength[0] + fAlpha * afCurStrength[1];

            // scale convertions
            fFreq = fFreq / ((fp)m_dwSamplesPerSec/4);   // BUGFIX - Using /4 since using it elswhere
            fFreq = 1.0 - fFreq;

            // pen
            dwPen = (DWORD)(sqrt(fStrength / m_afPitchMaxStrength[dwPitchSub]) * 16.0);
            dwPen = min(dwPen, 15);
            if (dwPen != dwLastPen) {
               dwLastPen = dwPen;
               SelectObject (hDCDraw, ahPenColor[dwLastPen]);
            }
            // translate min and max to a line
            int iY;
            iY = (int)((fFreq - fTop) * fScale) + rDraw.top;
            if (x)
               LineTo (hDCDraw, x, iY);
            else
               MoveToEx (hDCDraw, x, iY, NULL);
         } // x

      } // dwChan
      SelectObject (hDCDraw, hPenOld);
      for (i = 0; i < 16; i++)
         DeleteObject (ahPenColor[i]);
   } // dwPitchSub


   // draw
   BitBlt (hDC, pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top,
      hDCDraw, 0, 0, SRCCOPY);
   DeleteDC (hDCDraw);
   DeleteObject (hBit);
}

/****************************************************************************
CM3DWave::FXVolume - Affect the volume, with fade in or out.

inputs
   fp       fVolStart - Volume scaling at the start (1.0 = no change)
   fp       fVolEnd - Volume scaling at the end (1.0 = no change)
   PCProgressSocket pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXVolume (fp fVolStart, fp fVolEnd, PCProgressSocket pProgress)
{
   if (!m_dwSamples)
      return TRUE;   // no change
   ReleaseCalc();

   // find delta
   double fDelta, fCur;
   fDelta = (double)(fVolEnd - fVolStart) / (double) m_dwSamples;
   fCur = fVolStart;

   // loop
   DWORD i, j;
   fp fVal;
   for (i = 0; i < m_dwSamples; i++, fCur += fDelta) {
      if (((i%100000) == 0) && pProgress)
         pProgress->Update ((fp)i / (fp)m_dwSamples);

      for (j = 0; j < m_dwChannels; j++) {
         fVal = m_psWave[i*m_dwChannels+j];
         fVal *= fCur;
         fVal = max(fVal, -32768);
         fVal = min(fVal, 32767);
         m_psWave[i*m_dwChannels+j] = (short)fVal;
      } // j
   } // j

   return TRUE;
}

/****************************************************************************
CM3DWave::FindMax - Returns the maximum (in abs terms) sample.

returns
   short - Maximum sample. This will always be positive
*/
short CM3DWave::FindMax (void)
{
   short sMax = 0;
   short sCur;
   DWORD i;

   for (i = 0; i < m_dwSamples * m_dwChannels; i++) {
      sCur = m_psWave[i];
      if (sCur < 0)
         sCur *= -1;
      sMax = max(sMax, sCur);
   }

   return sMax;
}


/****************************************************************************
CM3DWave::FXReverse - Time-reverses the entire wave

inputs
   PCProgressSocket pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXReverse (PCProgressSocket pProgress)
{
   if (!m_dwSamples)
      return TRUE;   // no change
   ReleaseCalc();

   // loop
   DWORD i, j;
   short sTemp;
   for (i = 0; i < m_dwSamples / 2; i++) {
      if (((i%100000) == 0) && pProgress)
         pProgress->Update ((fp)i / (fp)m_dwSamples);

      for (j = 0; j < m_dwChannels; j++) {
         sTemp = m_psWave[i*m_dwChannels+j];
         m_psWave[i*m_dwChannels+j] = m_psWave[(m_dwSamples-i-1)*m_dwChannels+j];
         m_psWave[(m_dwSamples-i-1)*m_dwChannels+j] = sTemp;
      } // j
   } // j

   // clear phonemes since not worth the pain of reversing
   m_lWVPHONEME.Clear();
   m_lWVWORD.Clear();

   return TRUE;
}



/****************************************************************************
CM3DWave::FXRemoveDCOffset - Removes the DC offset (and low frequencies)


inputs
   BOOL              fAgressive - If TRUE them agressize and also removed sub-voice
                        frequencies.
   PCProgressSocket pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXRemoveDCOffset (BOOL fAgressive, PCProgressSocket pProgress)
{
   fp afFreqBand[2], afAmplify[2];
   DWORD dwTimeBand;
   afFreqBand[1] = fAgressive ? 25 : 12.5; // 50 hz
      // BUGFIX - Removing too much sub-voice frequencies when loading in TTS,
      // so changed to 1/4 the amount
   afFreqBand[0] = afFreqBand[1] * .66;
   dwTimeBand = 0;
   afAmplify[0] = 0;
   afAmplify[1] = 1;
   return Filter (2, afFreqBand, 1, &dwTimeBand, afAmplify, pProgress);
}



/****************************************************************************
CM3DWave::FXSwapChannels - Rotates channel 0 to channel 1, etc.

inputs
   PCProgressSocket pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXSwapChannels (PCProgressSocket pProgress)
{
   if (!m_dwSamples || (m_dwChannels < 2))
      return TRUE;   // no change
   ReleaseCalc();

   // loop
   DWORD i, j;
   short sTemp;
   for (i = 0; i < m_dwSamples; i++) {
      if (((i%100000) == 0) && pProgress)
         pProgress->Update ((fp)i / (fp)m_dwSamples);

      sTemp = m_psWave[i*m_dwChannels+(m_dwChannels-1)];
      for (j = 0; j+1 < m_dwChannels; j++)
         m_psWave[i*m_dwChannels + j+1] = m_psWave[i*m_dwChannels];
      m_psWave[i*m_dwChannels + 0] = sTemp;
   } // j

   return TRUE;
}


/****************************************************************************
CM3DWave::CalcSilenceEnergy - Looks through the wave and determines at what
point (or below) there's silence. This is done by one of two methods, finding
the maximum volume and taking 30(?) dB below that, the other is finding the
quietest area and using 6 dB above to represent silnce.

returns
   WORD - Energy for silence, from 0 to 0xffff. Same units as m_pwEnergy
*/
WORD CM3DWave::CalcSilenceEnergy (void)
{
   // make sure have energy
   if (!m_dwEnergySamples) {
      if (!CalcEnergy (NULL))
         return 0;
   }

   // find the max and the min
   WORD wMax, wMin;
   wMax = 0;
   wMin = 0xffff;
   DWORD i;
   for (i = 0; i < m_dwEnergySamples * m_dwChannels; i++) {
      wMax = max(wMax, m_pwEnergy[i]);
      wMin = min(wMin, m_pwEnergy[i]);
   }

   // take 36 db below max
   wMin *= 2;  // each *2 is 6DB
   wMin = min(wMin, wMax / 8);   // BUGFIX - minimum must be at least 18dB lower that max
   wMax /= (2 * 2 * 2 * 2 * 2 * 2);   // each *2 is 6 dB

   if (wMin >= wMax)
      return wMin;   // since at least need something
   else
      return wMax/2 + wMin/2; // average
}

/****************************************************************************
CM3DWave::FXNoiseReduce - Reduces the amount of noise.

inputs
   PCProgressSocket pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXNoiseReduce (PCProgressSocket pProgress)
{
   // get noise level
   WORD wNoise = CalcSilenceEnergy ();
   if (!wNoise)
      return FALSE;  // no noise to reduce to

   // create the window
   DWORD dwWindowSize, dwHalfWindow;
   dwWindowSize = WindowSize ();
   dwHalfWindow = dwWindowSize / 2;
   CMem memWindow;
   float *pafWindow;
   if (!memWindow.Required (dwWindowSize * sizeof(float)))
      return FALSE;
   pafWindow = (float*) memWindow.p;
   CreateFFTWindow (3, pafWindow, dwWindowSize);
   DWORD i;
   for (i = 0; i < dwWindowSize; i++)
      pafWindow[i] = sqrt(pafWindow[i]);  // do this so filters ok

   // allocate memory to store the FFT, and the noise FFT
   CMem memFFT, memNoise;
   float *pafFFT, *pafNoise;
   if (!memFFT.Required (dwWindowSize * sizeof(float)))
      return FALSE;
   pafFFT = (float*) memFFT.p;
   if (!memNoise.Required (dwHalfWindow * sizeof(float)))
      return FALSE;
   pafNoise = (float*) memNoise.p;

   CMem memFFTScratch;
   CSinLUT SinLUT;

   // figure out the average noise
   DWORD dwCount, dwChan, j;
   int iSamp;
   dwCount = 0;
   memset (pafNoise, 0, sizeof(float)*dwHalfWindow);
   for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
      for (i = 0; i < m_dwEnergySamples; i++) {
         if (m_pwEnergy[i*m_dwChannels+dwChan] > wNoise)
            continue;   // too loud, isn't noise

         // fill in all the values for FFT
         for (j = 0; j < dwWindowSize; j++) {
            iSamp = (int)(i * m_dwEnergySkip) + (int)j - (int) dwHalfWindow;
            if ((iSamp < 0) || (iSamp >= (int)m_dwSamples)) {
               pafFFT[j] = 0;
               continue;
            }

            // else, get from value
            pafFFT[j] = m_psWave[iSamp * (int)m_dwChannels + (int)dwChan];
         } // j

         // do the FFT
         FFTRecurseReal (pafFFT - 1, dwWindowSize, 1, &SinLUT, &memFFTScratch);

         // throw out the phase
         dwCount++;
         for (j = 0; j < dwHalfWindow; j++)
            pafNoise[j] += sqrt(pafFFT[j*2+0]*pafFFT[j*2+0] + pafFFT[j*2+1]*pafFFT[j*2+1]);
      } // i
   } // dwChan

   // scale the count out of the noise
   if (!dwCount)
      return FALSE;  // no silent bits
   for (j = 0; j < dwHalfWindow; j++) {
      pafNoise[j] /= (fp) dwCount;
      pafNoise[j] *= 2.0;  // so eliminate 2x the average for noise
   }

   // allocate space to transfer to
   // create new memory with enough space
   PCMem pNew;
   pNew = new CMem;
   if (!pNew)
      return FALSE;
   DWORD dwNeed;
   dwNeed  = m_dwSamples * m_dwChannels * sizeof(short);
   if (!pNew->Required (dwNeed)) {
      delete pNew;
      return FALSE;
   }
   short *psNew;
   psNew = (short*) pNew->p;
   memset (psNew, 0, dwNeed);


   // loop
   int iSum;
   for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
      if (pProgress)
         pProgress->Push ((fp)dwChan / (fp)m_dwChannels, (fp)(dwChan+1) / (fp)m_dwChannels);

      for (i = 0; i < m_dwSamples + dwHalfWindow; i += dwHalfWindow) {

         // progress
         if (pProgress && ((i % 1000) == 0))
            pProgress->Update ((fp) i / (fp) (m_dwSamples+dwHalfWindow));

         // get all the values so can pass into fft
         for (j = 0; j < dwWindowSize; j++) {
            iSamp = (int)i + (int)j - (int)dwHalfWindow;
            if ((iSamp >= 0) && (iSamp < (int)m_dwSamples))
               pafFFT[j] = (float) m_psWave[iSamp * (int)m_dwChannels + (int)dwChan] * pafWindow[j];
            else
               pafFFT[j] = 0;
         } // j

         // fft it
         FFTRecurseReal (pafFFT - 1, dwWindowSize, 1, &SinLUT, &memFFTScratch);

         // remove silence
         fp fEnergy;
         for (j = 0; j < dwHalfWindow; j++) {
            // calculate the energy in this bin
            fEnergy = sqrt(pafFFT[j*2+0]*pafFFT[j*2+0] + pafFFT[j*2+1]*pafFFT[j*2+1]);
            if (fEnergy <= pafNoise[j] + EPSILON) {
               // completely below noise level, so eliminate
               pafFFT[j*2+0] = pafFFT[j*2+1] = 0;
               continue;
            }

            // else, just scale energy by amount that reduce
            fEnergy = (fEnergy - pafNoise[j]) / fEnergy;
            pafFFT[j*2+0] *= fEnergy;
            pafFFT[j*2+1] *= fEnergy;
         }

         // un-fft it
         FFTRecurseReal (pafFFT - 1, dwWindowSize, -1, &SinLUT, &memFFTScratch);

         // return it
         for (j = 0; j < dwWindowSize; j++) {
            iSamp = (int)i + (int)j - (int)dwHalfWindow;
            if ((iSamp < 0) || (iSamp >= (int)m_dwSamples))
               continue;

            iSum = (int) (pafFFT[j] * pafWindow[j] / (float)dwWindowSize);
            iSum += psNew[iSamp * (int)m_dwChannels + (int)dwChan];
            iSum = max(iSum, -32768);
            iSum = min(iSum, 32767);
            psNew[iSamp * (int)m_dwChannels + (int)dwChan] = (short)iSum;
         } // j
         
      } // i

      if (pProgress)
         pProgress->Pop();
   } // channel

   // finally copy over
   delete m_pmemWave;
   m_pmemWave = pNew;
   m_psWave = (short*) pNew->p;

   // reset calculations
   ReleaseCalc();

   return TRUE;

}

/****************************************************************************
CM3DWave::FXTrimSilence - Remove silence from the beginning and end of a recording.


inputs
   PCProgressSocket pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXTrimSilence (PCProgressSocket pProgress)
{
   // get noise level
   WORD wNoise = CalcSilenceEnergy ();
   if (!wNoise || !m_dwEnergySamples)
      return FALSE;  // no noise to reduce to
   if (wNoise < 0x8000)
      wNoise *= 2;   // be extra agressive around edge

   // find the silence on the end
   DWORD i, dwChan;
   BOOL fNotSilent;
   for (i = m_dwEnergySamples - 1; i < m_dwEnergySamples; i--) {
      fNotSilent = FALSE;
      for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
         if (m_pwEnergy[i*m_dwChannels + dwChan] > wNoise) {
            fNotSilent = TRUE;
            break;
         }
      } // dwChan
      if (fNotSilent)
         break;
   } // i

   // if ended up breaking less than dwEnergySamples - 1 then remove from end
   if (i+1 < m_dwEnergySamples) {
      DWORD dwOrigSamples = m_dwSamples;

      m_dwSamples = min(m_dwSamples, (i+1) * m_dwEnergySkip); // remove the end

      // remove phonemes
      if (dwOrigSamples > m_dwSamples) {
         PhonemeDelete (m_dwSamples, dwOrigSamples);
         WordDelete (m_dwSamples, dwOrigSamples);
      }
   }

   // see how far can get before reach non-silent part
   for (i = 0; i < m_dwEnergySamples; i++) {
      fNotSilent = FALSE;
      for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
         if (m_pwEnergy[i*m_dwChannels + dwChan] > wNoise) {
            fNotSilent = TRUE;
            break;
         }
      } // dwChan
      if (fNotSilent)
         break;
   } // i

   // if i > 0 then have point
   if (i > 0) {
      DWORD dwRemoveTo = (i-1) * m_dwEnergySkip;
      dwRemoveTo = min(dwRemoveTo, m_dwSamples);

      // move memory
      memmove (m_psWave, m_psWave + dwRemoveTo * m_dwChannels,
         (m_dwSamples - dwRemoveTo) * m_dwChannels * sizeof(short));
      m_dwSamples -= dwRemoveTo;

      // remove phonemes
      PhonemeDelete (0, dwRemoveTo);
      WordDelete (0, dwRemoveTo);
   }

   // fade in and out on the very edges
   fp fCur, fDelta;
   fCur = 0;
   fDelta = 1.0 / (fp)m_dwEnergySkip;
   for (i = 0; i < m_dwEnergySkip; i++, fCur += fDelta) {
      for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
         if (i >= m_dwSamples)
            continue;   // whats left is too small

         // on the start
         m_psWave[i] = (short)((fp)m_psWave[i] * fCur);
         m_psWave[m_dwSamples-i-1] = (short)((fp)m_psWave[m_dwSamples-i-1]*fCur);
      } // dwChan
   } // i

   // reset calculations
   ReleaseCalc();

   return TRUE;

}





/****************************************************************************
CM3DWave::FXFrequency - Does effects based on frequency

inputs
   DWORD          dwEffect - One of the following
                     0 - No phase
                     1 - Swap phase
                     2 - Drop every other frequency (and good for stereo)
                     3 - Flip freq from top to bottom
                     4 - Double frequencies
                     5 - Halve frequencies
                     6 - Like 2 except larger chunks
   PCProgressSocket pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXFrequency (DWORD dwEffect, PCProgressSocket pProgress)
{
   // create the window
   DWORD dwWindowSize, dwHalfWindow;
   dwWindowSize = WindowSize ();
   dwHalfWindow = dwWindowSize / 2;
   CMem memWindow;
   float *pafWindow;
   if (!memWindow.Required (dwWindowSize * sizeof(float)))
      return FALSE;
   pafWindow = (float*) memWindow.p;
   CreateFFTWindow (3, pafWindow, dwWindowSize);
   DWORD i;
   for (i = 0; i < dwWindowSize; i++)
      pafWindow[i] = sqrt(pafWindow[i]);  // do this so filters ok

   // allocate memory to store the FFT, and the noise FFT
   CMem memFFT;
   float *pafFFT;
   if (!memFFT.Required (dwWindowSize * sizeof(float)))
      return FALSE;
   pafFFT = (float*) memFFT.p;


   // allocate space to transfer to
   // create new memory with enough space
   PCMem pNew;
   pNew = new CMem;
   if (!pNew)
      return FALSE;
   DWORD dwNeed;
   dwNeed  = m_dwSamples * m_dwChannels * sizeof(short);
   if (!pNew->Required (dwNeed)) {
      delete pNew;
      return FALSE;
   }
   short *psNew;
   psNew = (short*) pNew->p;
   memset (psNew, 0, dwNeed);


   CMem memFFTScratch;
   CSinLUT SinLUT;

   // loop
   int iSum;
   DWORD dwChan, j;
   int iSamp;
   for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
      if (pProgress)
         pProgress->Push ((fp)dwChan / (fp)m_dwChannels, (fp)(dwChan+1) / (fp)m_dwChannels);

      for (i = 0; i < m_dwSamples + dwHalfWindow; i += dwHalfWindow) {

         // progress
         if (pProgress && ((i % 1000) == 0))
            pProgress->Update ((fp) i / (fp) (m_dwSamples+dwHalfWindow));

         // get all the values so can pass into fft
         for (j = 0; j < dwWindowSize; j++) {
            iSamp = (int)i + (int)j - (int)dwHalfWindow;
            if ((iSamp >= 0) && (iSamp < (int)m_dwSamples))
               pafFFT[j] = (float) m_psWave[iSamp * (int)m_dwChannels + (int)dwChan] * pafWindow[j];
            else
               pafFFT[j] = 0;
         } // j

         // fft it
         FFTRecurseReal (pafFFT - 1, dwWindowSize, 1, &SinLUT, &memFFTScratch);

         fp fEnergy;
         switch (dwEffect) {
         default:
         case 0: // No phase
            for (j = 0; j < dwHalfWindow; j++) {
               // calculate the energy in this bin
               fEnergy = sqrt(pafFFT[j*2+0]*pafFFT[j*2+0] + pafFFT[j*2+1]*pafFFT[j*2+1]);

               pafFFT[j*2+0] = fEnergy;
               pafFFT[j*2+1] = 0;
            }
            break;

         case 1: // Swap phase
            for (j = 0; j < dwHalfWindow; j++) {
               fEnergy = pafFFT[j*2+0];
               pafFFT[j*2+0] = pafFFT[j*2+1];
               pafFFT[j*2+1] = fEnergy;
            }
            break;

         case 2: // Drop every other frequency (and good for stereo)
            for (j = 0; j < dwHalfWindow; j++) {
               if ((j + dwChan)%2 == 0) {
                  pafFFT[j*2+0] = pafFFT[j*2+1] = 0;
               }
            }
            break;

         case 6: // Drop every other frequency (and good for stereo)
            for (j = 0; j < dwHalfWindow; j++) {
               if ((j/4 + dwChan)%2 == 0) {
                  pafFFT[j*2+0] = pafFFT[j*2+1] = 0;
               }
            }
            break;

         case 3: // Flip freq from top to bottom
            for (j = 0; j < dwHalfWindow; j++) {
               fEnergy = pafFFT[j];
               pafFFT[j] = pafFFT[dwWindowSize-1-j];
               pafFFT[dwWindowSize-1-j] = fEnergy;
            }
            break;

         case 4: // Double frequencies
            for (j = dwHalfWindow-1; j < dwHalfWindow; j--) {
               pafFFT[j*2+0] = pafFFT[(j/2)*2 + 0];
               pafFFT[j*2+1] = pafFFT[(j/2)*2 + 1];
            }
            break;

         case 5: // Halve frequencies
            for (j = 0; j < dwHalfWindow; j++) {
               if (j*2 >= dwHalfWindow) {
                  pafFFT[j*2+0] = pafFFT[j*2+1] = 0;
                  continue;
               }
               pafFFT[j*2+0] = pafFFT[(j*2)*2 + 0];
               pafFFT[j*2+1] = pafFFT[(j*2)*2 + 1];
            }
            break;
         } // switch

         // un-fft it
         FFTRecurseReal (pafFFT - 1, dwWindowSize, -1, &SinLUT, &memFFTScratch);

         // return it
         for (j = 0; j < dwWindowSize; j++) {
            iSamp = (int)i + (int)j - (int)dwHalfWindow;
            if ((iSamp < 0) || (iSamp >= (int)m_dwSamples))
               continue;

            iSum = (int) (pafFFT[j] * pafWindow[j] / (float)dwWindowSize);
            iSum += psNew[iSamp * (int)m_dwChannels + (int)dwChan];
            iSum = max(iSum, -32768);
            iSum = min(iSum, 32767);
            psNew[iSamp * (int)m_dwChannels + (int)dwChan] = (short)iSum;
         } // j
         
      } // i

      if (pProgress)
         pProgress->Pop();
   } // channel

   // finally copy over
   delete m_pmemWave;
   m_pmemWave = pNew;
   m_psWave = (short*) pNew->p;

   // reset calculations
   ReleaseCalc();

   return TRUE;

}



/****************************************************************************
CM3DWave::FXBlend - Blends the end portion of the wave in with the beginngin

inputs
   fp             fLoop - What percentage of the wave to blend in. Max .5.
   PCProgressSocket pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXBlend (fp fLoop, PCProgressSocket pProgress)
{
   DWORD dwLoop;
   dwLoop = (DWORD) (fLoop * (fp)m_dwSamples);
   dwLoop = min(dwLoop, m_dwSamples/2);
   if (!m_dwSamples || !dwLoop)
      return TRUE;   // no change
   ReleaseCalc();

   // remove phonemes at end
   PhonemeDelete (m_dwSamples-dwLoop, m_dwSamples);
   WordDelete (m_dwSamples-dwLoop, m_dwSamples);

   // reduce the number of samples total
   m_dwSamples -= dwLoop;

   // loop
   DWORD i, j;
   fp f, fAlpha;
   for (i = 0; i < dwLoop; i++) {
      if (((i%100000) == 0) && pProgress)
         pProgress->Update ((fp)i / (fp)dwLoop);

      fAlpha = (fp) i / (fp)dwLoop;
      for (j = 0; j < m_dwChannels; j++) {
         f = (fp) m_psWave[i*m_dwChannels+j] * fAlpha +
            (fp)m_psWave[(i+m_dwSamples)*m_dwChannels+j] * (1.0 - fAlpha);
         f = max(f, -32768);
         f = min(f, 32767);
         m_psWave[i*m_dwChannels+j] = (short)f;
      }; // j
   } // i

   return TRUE;
}





/****************************************************************************
CM3DWave::FXAcousticCompress - Does acoustic compression, basically converting
all samples to -1 to 1 range, and then raising them to the give power. Therefore,
to do traditional compression, power should be < 1, such as .5.

inputs
   fp             fPower - Power to raise audio to
   PCProgressSocket pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXAcousticCompress (fp fPower, PCProgressSocket pProgress)
{
   if (!m_dwSamples)
      return TRUE;   // no change
   ReleaseCalc();

   // loop
   DWORD i, j;
   fp f;
   short s;
   for (i = 0; i < m_dwSamples; i++) {
      if (((i%100000) == 0) && pProgress)
         pProgress->Update ((fp)i / (fp)m_dwSamples);

      for (j = 0; j < m_dwChannels; j++) {
         s = m_psWave[i*m_dwChannels+j];
         f = fabs((fp)s) / 32768.0;
         f = pow(f, fPower);
         f = f * 32767.0 * ((s < 0) ? -1 : 1);
         m_psWave[i*m_dwChannels+j] = (short)f;
      }; // j
   } // i

   return TRUE;
}

/****************************************************************************
CM3DWave::FXGenerateEcho - This is used to create the wave that is convolved
with the main wave. It CLEARS the existing wave and replaces it with the echo
information.

inputs
   PFXECHOPARAM         pEcho - Echo information
returns
   BOOL - TRUE when finished
*/
BOOL CM3DWave::FXGenerateEcho (PFXECHOPARAM pEcho)
{
   // clear wave
   if (!New (m_dwSamplesPerSec, m_dwChannels, (DWORD)(pEcho->fTime * (fp)m_dwSamplesPerSec)))
      return FALSE;
   memset (m_psWave, 0, m_dwSamples * m_dwChannels * sizeof(short));

   // see rand
   srand (pEcho->dwSeed);

   // put a spike in for original sound
   DWORD i;
   for (i = 0; i < m_dwChannels; i++)
      m_psWave[i] = 32767;

   // send out a bounce for each channel
   DWORD dwSurf;
   for (i = 0; i < m_dwChannels; i++)
      for (dwSurf = 0; dwSurf < pEcho->dwSurfaces; dwSurf++)
         EchoBounce (20, i, 0, 1.0 / (fp)pEcho->dwSurfaces, 1.0, pEcho);

   return TRUE;
}

/****************************************************************************
CM3DWave::EchoBounce - Internal function that is called recurseively to simulate
echo bounces.

NOTE: This only bounces off one surface

inputs
   DWORD                dwCount - Decreases for each echo. When it reaches 0 doesnt excho
   DWORD                dwChan - Channel
   DWORD                dwSample - Sample that audio starts from
   fp                   fScale - Amount to scale in strength
   fp                   fSpreadOut - Amount that it's spread out. 1.0 for not. # increases
                           NOT used because sharpness disabled
   PFXECHOPARAM         pEcho - Echo information
returns
   none
*/
void CM3DWave::EchoBounce (DWORD dwCount, DWORD dwChan, DWORD dwSample, fp fScale, fp fSpreadOut, PFXECHOPARAM pEcho)
{
   if ((dwSample >= m_dwSamples) || (fScale < 1.0 / 10000.0) || !dwCount)
      return;  // too quiet

   // loop over all the surfaces
   DWORD dwHit;
   fp f;
   // figure out how long until it hits there
   fp fTime = randf (1.001 - pEcho->fDelayVar, 0.999 + pEcho->fDelayVar) * pEcho->fDelay;

   // first bounce is only half the time, assuming around center of room
   if (!dwSample)
      fTime /= 2;

   fTime *= (fp)m_dwSamplesPerSec;
   dwHit = dwSample + (DWORD)fTime;
   if (dwHit >= m_dwSamples)
      return; // wont bounce in time

   // figure out the intensity when it hits
   fp fIntensity;
   fIntensity = fScale * (1.0 - pEcho->fDecay) * randf(1 - pEcho->fDelayVar*.8, 1); // randf() provides some randomness
   if (fIntensity < 1.0 / 10000.0)
      return; // too quiet

#if 0 // dont use anymore
   // figure out decay
   fp fDecay;
   fDecay = pow(1.0 - min(pEcho->fSharpness,.99),
      1.0 / (fSpreadOut * (fp)m_dwSamplesPerSec / 22050.0));

   // create the impulse and add
   fp fImpulse;
   DWORD dwHitOrig;
   fImpulse = fIntensity * 32767;
   dwHitOrig = dwHit;
   for (; (fImpulse >= 1) && (dwHit < m_dwSamples); dwHit++, fImpulse *= fDecay) {
      f = m_psWave[dwHit * m_dwChannels + dwChan];
      f += fImpulse;
      f = min(f,32767);
      m_psWave[dwHit * m_dwChannels + dwChan] = (short)f;
   } // dwHit
   dwHit = dwHitOrig;
#endif

   fp fImpulse;
   fImpulse = fIntensity * 32767;
   f = m_psWave[dwHit * m_dwChannels + dwChan];
   f += fImpulse;
   f = min(f,32767);
   m_psWave[dwHit * m_dwChannels + dwChan] = (short)f;

   // repeat
   DWORD i;
   DWORD dwNum;
   dwNum = pEcho->dwSurfaces - 1;
   if (dwNum)
      fIntensity /= (fp)dwNum;
   for (i = 0; i < dwNum; i++)
      EchoBounce (dwCount-1, dwChan, dwHit, fIntensity,
         fSpreadOut /*+ (1.0 - pEcho->fSharpness)*/, pEcho);
}

/****************************************************************************
CM3DWave::Convolve - Convolves the current wave with the wave, pWave.

inputs
   PCM3DWave      pWith - Convolve with this wave
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXConvolve (PCM3DWave pWith, PCProgressSocket pProgress)
{
   if (pWith->m_dwChannels != m_dwChannels)
      return FALSE;

   // allocate space to transfer to
   // create new memory with enough space
   PCMem pNew;
   pNew = new CMem;
   if (!pNew)
      return FALSE;
   DWORD dwNeed, dwNewSamples;
   dwNewSamples = m_dwSamples + pWith->m_dwSamples;
   dwNeed  = dwNewSamples * m_dwChannels * sizeof(short);
   if (!pNew->Required (dwNeed)) {
      delete pNew;
      return FALSE;
   }
   short *psNew;
   psNew = (short*) pNew->p;
   memset (psNew, 0, dwNeed);


   // loop, filling in new smaples
   DWORD i, j, dwChan;
   fp fSum;
   int iSamp;
   short sVal;
   for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
      if (pProgress)
         pProgress->Push ((fp)dwChan / (fp)m_dwChannels, (fp)(dwChan+1)/(fp)m_dwChannels);

      for (i = 0; i < dwNewSamples; i++) {
         if (((i%5000) == 0) && pProgress)
            pProgress->Update ((fp)i / (fp)dwNewSamples);

         fSum = 0;

         for (j = 0; j < pWith->m_dwSamples; j++) {
            sVal = pWith->m_psWave[j*m_dwChannels+dwChan];
            if (!sVal)
               continue;

            iSamp = (int)i - (int)j;
            if ((iSamp < 0) || (iSamp >= (int)m_dwSamples))
               continue;

            fSum += (fp)m_psWave[iSamp*(int)m_dwChannels+(int)dwChan] *
               (fp)sVal / 32768.0;
         } // j

         fSum = max(fSum, -32768);
         fSum = min(fSum, 32767);
         psNew[i*m_dwChannels+dwChan] = (short)fSum;
      } // i

      if (pProgress)
         pProgress->Pop();
   } // dwChan



   // finally copy over
   delete m_pmemWave;
   m_pmemWave = pNew;
   m_psWave = (short*) pNew->p;
   m_dwSamples = dwNewSamples;

   // reset calculations
   ReleaseCalc();

   return TRUE;

}


/****************************************************************************
CM3DWave::FXSine - Draw a sine wave.

inputs
   fp       fStart - Starting pitch
   fp       fEnd - Ending pitch
   DWORD    dwShape - 0 for sine, 1 for triangle, 2 for sawtooth, 3 for sqaure
   DWORD    dwChannel - Channel to use. 0 for all, 1 for left, 2 for right, etc.
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXSine (fp fStart, fp fEnd, DWORD dwShape, DWORD dwChannel, PCProgressSocket pProgress)
{
   // reset calculations
   ReleaseCalc();

   // loop
   DWORD i, j;
   fp fPitch, fAlpha, fPhase;
   fPhase = 0;
   for (i = 0; i < m_dwSamples; i++) {
      if (((i%100000) == 0) && pProgress)
         pProgress->Update ((fp)i / (fp)m_dwSamples);

      // figure out pitch here
      fAlpha = (fp)i / (fp)m_dwSamples;
      fPitch = (1.0 - fAlpha) * fStart + fAlpha * fEnd;

      // figure out phase
      fPhase += fPitch / (fp)m_dwSamplesPerSec;
      fPhase = fmod(fPhase, 1);  // only keep the fractional part

      // what's the value, from -1 to 1
      fp fVal;


      switch (dwShape) {
         default:
         case 0: // sine
            fVal = sin(fPhase * 2.0 * PI);
#if 0 // def _DEBUG  // create a complex wave for testing voice analysis
            DWORD k, dwBits;
            fVal = 0;
            for (j = 1; j < 40; j++) {
               // determine the number of bits
               k = j;
               // for (dwBits = 0; k; dwBits++, k /= 2);
               dwBits = (j == 1) ? 1 : ((j < 8) ? 2 : 4);

               fVal += cos (fPhase * 2.0 * PI * (fp)j) / (fp)(dwBits*dwBits*dwBits) / 4.0;
            } // j
#endif // _DEBUG
            break;
         case 1:  // tirangle
            if (fPhase <= .5)
               fVal = 1.0 - 4 * fPhase;
            else
               fVal = 4*fPhase - 3;
            break;
         case 2:  // sawtooth
            fVal = fPhase * 2.0 - 1;
            break;
         case 3: // sqaure
            fVal = (fPhase < .5) ? 1 : -1;
            break;
      } // dwShape
      
      if (!dwChannel)
         for (j = 0; j < m_dwChannels; j++)
            m_psWave[i*m_dwChannels+j] = (short)(fVal * 32767.0);
      else if (dwChannel <= m_dwChannels)
            m_psWave[i*m_dwChannels+(dwChannel-1)] = (short)(fVal * 32767.0);
   } // i

   // clear out the phonemes
   m_lWVPHONEME.Clear();
   m_lWVWORD.Clear();

#if 0 // to test def _DEBUG
   fp fPitchFound, fDelta;
   CalcPitchIfNeeded (WAVECALC_SEGMENT, NULL);
   fPitchFound = PitchOverRange (m_dwSamples*0/4, m_dwSamples*4/4, 0, NULL, &fDelta);
#endif

   return TRUE;
}

/****************************************************************************
CombScoreNew - New comb score that uses PitchDetectGetPITCHDETECTVALUE values.

inputs
   DWORD       dwFFTSize - Number of points in the FFT energy. Implied a window size was twice this.
   float       *pafFFT - FFT point, starting with 0=DC offset energy, 1=x hz, 2 = 2x hz, etc.
   DWORD       dwSamplesPerSec - Samples per second in the save
   fp          fPitch - Pitch in Hz
   PCListFixed plPITCHDETECTVALUE - List from pitchdetect value
   PCMem       pMemScrtatch - Scratch memory
returns
   fp - Score. 0+
*/
static __inline fp CombScoreNew (DWORD dwFFTSize, float *pafFFT, DWORD dwSamplesPerSec, fp fPitch, PCListFixed plPITCHDETECTVALUE,
                        PCMem pMemScratch)
{
   pafFFT[0] = 0; // so ignore DC offset

   // figure out the FFT location for each of the harmonics
   DWORD i, j, dwLeft, dwRight;
   fp afPitchHarmonic[PITCHDETECTMAXHARMONICS], afWavelength[PITCHDETECTMAXHARMONICS], afCenterFFT[PITCHDETECTMAXHARMONICS], afPeak[PITCHDETECTMAXHARMONICS];
   DWORD dwWindowSize = dwFFTSize * 2;
   fp fAlpha;
   fp fWavelengthMax = (fp)dwSamplesPerSec / (fp)PITCHDETECTMAXFREQ;
   fp fCenterFFTMax = (fp)dwWindowSize / fWavelengthMax;
   DWORD dwMaxPeak = min((DWORD)fCenterFFTMax, dwFFTSize);
   for (i = 0; i < PITCHDETECTMAXHARMONICS; i++) {
      afPitchHarmonic[i] = fPitch * (fp)(i+1);
      afWavelength[i] = (fp)dwSamplesPerSec / afPitchHarmonic[i];
      afCenterFFT[i] = (fp)dwWindowSize / afWavelength[i];

      if (afPitchHarmonic[i] >= PITCHDETECTMAXFREQ) {
         afPeak[i] = 0;
         continue;
      }

      fAlpha = afCenterFFT[i];
      dwLeft = min ((DWORD)fAlpha, dwFFTSize-1);
      dwRight = min(dwLeft+1, dwFFTSize-1);
      fAlpha -= (fp)dwLeft;
      afPeak[i] = (1.0 - fAlpha) * pafFFT[dwLeft] + fAlpha * pafFFT[dwRight];
   } // i

   // do sum "blurring" (using max's) of harmonics so that won't do pitch doubling or pitch halving
#define PEAKBLURAMT     2
   fp afPeakBlur[PITCHDETECTMAXHARMONICS];
   memset (afPeakBlur, 0, sizeof(afPeakBlur));
   for (i = 0; i < PITCHDETECTMAXHARMONICS; i++) {
      int iTent;
      for (iTent = -(int)i; iTent <= PEAKBLURAMT; iTent++) {
         int iIndex = iTent + (int)i;
         if ((iIndex < 0) || (iIndex >= PITCHDETECTMAXHARMONICS))
            continue;

         if (iTent <= 0) {
            // gradual ramp down to first harminc
            fAlpha = (fp)((int) i + 1 + iTent) / (fp)(i+1);
            fAlpha /= 2.0; // so not as steep
         }
         else
            fAlpha = 0;

         // steeper ramp nearby
         if (abs(iTent) <= PEAKBLURAMT)
            fAlpha = max(fAlpha, (fp)(PEAKBLURAMT + 1 - abs(iTent)) / (fp)(PEAKBLURAMT+1));

         fAlpha *= afPeak[i];

         afPeakBlur[iIndex] = max(afPeakBlur[iIndex], fAlpha);
      } // iTent
   } // i

   // come up with a theoretical model
   if (!pMemScratch->Required (dwFFTSize * sizeof(float)))
      return 0;
   float *pafScratch = (float*)pMemScratch->p;
   memset (pafScratch, 0, dwFFTSize * sizeof(float));

   PPITCHDETECTVALUE pPDVLeft, pPDVRight;
   PITCHDETECTVALUE pdv;
   int iLeft, iRight;
   fp fScale;
   double fErrorBlur = 0;
   for (i = 0; i < PITCHDETECTMAXHARMONICS; i++) {
      // if zero then don't bother
      if (!afPeak[i])
         continue;

      // what's the index into plPITCHDETECTVALUE
      fAlpha = (afPitchHarmonic[i] - MINPITCHCANFIND) / PITCHDETECTDELTA;
      fAlpha = max(fAlpha, 0.0);
      fAlpha = min(fAlpha, (fp)plPITCHDETECTVALUE->Num() - 1);

      dwLeft = min((DWORD)fAlpha, plPITCHDETECTVALUE->Num()-1);
      dwRight = min(dwLeft+1, plPITCHDETECTVALUE->Num()-1);
      fAlpha -= (fp)dwLeft;

      pPDVLeft = (PPITCHDETECTVALUE)plPITCHDETECTVALUE->Get(dwLeft);
      pPDVRight = (PPITCHDETECTVALUE)plPITCHDETECTVALUE->Get(dwRight);

      for (j = 0; j < PITCHDETECTVALUEBULGEMAX; j++)
         pdv.afBulge[j] = (1.0 - fAlpha) * pPDVLeft->afBulge[j] + fAlpha * pPDVLeft->afBulge[j];

      if (afPeakBlur[i] > afPeak[i]) {
         // expecting at least some sound here but not getting any, so error
         pdv.fBulgeSum = (1.0 - fAlpha) * pPDVLeft->fBulgeSum + fAlpha * pPDVLeft->fBulgeSum;
         fScale = afPeakBlur[i] - afPeak[i];
         fScale *= pdv.fBulgeSum / max(pdv.afBulge[PITCHDETECTVALUEBULGEMAXHALF], CLOSE);
         fErrorBlur += fScale * fScale;
      }

      // figure out the scale
      fScale = afPeak[i] / max(pdv.afBulge[PITCHDETECTVALUEBULGEMAXHALF], CLOSE);
      
      // add in
      for (j = 0; j < PITCHDETECTVALUEBULGEMAX; j++) {
         fAlpha = afCenterFFT[i] + (fp)j - (fp)PITCHDETECTVALUEBULGEMAXHALF;
         iLeft = (int) floor(fAlpha);
         iRight = iLeft+1;
         fAlpha -= (fp)iLeft;

         fp fEnergy = fScale * pdv.afBulge[j];

         if ((iLeft >= 1) && (iLeft < (int)dwFFTSize))
            pafScratch[iLeft] += (1.0 - fAlpha) * fEnergy;
         if ((iRight >= 1) && (iRight < (int)dwFFTSize))
            pafScratch[iRight] += fAlpha * fEnergy;
      } // j
   } // i

   // figure out the energy for voiced vs. unvoiced
   double fVoiced = 0, fUnvoiced = 0;
   for (i = 0; i < dwMaxPeak; i++) {   // BUGFIX - Was dwFFTSize, but want to limit to around 2k
      fVoiced += pafFFT[i] * pafFFT[i];
      fUnvoiced += (pafScratch[i] - pafFFT[i]) * (pafScratch[i] - pafFFT[i]);
#if 0 // not used
      if (pafFFT[i] <= pafScratch[i]) {
         fVoiced += pafFFT[i];   // min(pafFFT[i], pafScratch[i])
         fUnvoiced += pafScratch[i] - pafFFT[i];   // BUGFIX - count any mismatch as noise
      }
      else {
         fVoiced += pafScratch[i];
         fUnvoiced += pafFFT[i] - pafScratch[i];
      }
#endif
   } // i
   fVoiced = sqrt(fVoiced);
   fUnvoiced = sqrt(fUnvoiced);
   fErrorBlur = sqrt(fErrorBlur);

   fVoiced -= fUnvoiced;
   fVoiced -= fErrorBlur / 3.0; //  3.0 seems to work well
   fVoiced = max(fVoiced, 0);

   // BUGFIX - If below lower-pitch test then zero
   if (fPitch < MINPITCHCANFIND)
      fVoiced = 0;   // absolute 0
   else if (fPitch < 2*MINPITCHCANFIND)
      fVoiced *= (fPitch - MINPITCHCANFIND) / MINPITCHCANFIND; // so get ramp

   return (fp)fVoiced;
}


/****************************************************************************
CombScore - Runs a comb through the FFT energies and returns a score for
the given frequency.

inputs
   DWORD       dwFFTSize - Number of points in the FFT energy.
   float       *pafFFT - FFT point, starting with 0=DC offset energy, 1=x hz, 2 = 2x hz, etc.
   DWORD       dwIgnoreBelow - Ignore all frequencies in the FFT below this mark... do this
               so that DC offset and breathiness are ignored
   float       fWaveLen - Frequency to test for (in FFT units). This must be 2 or more to avoid nyquist
   BOOL        fCalcValues - If TRUE then need to calculate the values for pafValues, else
                  they're already calculated
   float       *pafValues - Potentially precalculated values
   PCMem       pMemScratch - Some more scratch memory
returns
   float - score. 0+
*/

#define PREVIOUSPITCHDETECT      // experiment with one one didn't work

// #define OLDAPPROACH - new approach seems to work!
   // NOTE: The new approach doesn't work well with a pure sine wave sweep from 100 to 400 hz,
   // while the old one does. The new approach seems better with voices
#ifndef OLDAPPROACH
static __inline float CombScore (DWORD dwFFTSize, float *pafFFT, DWORD dwIgnoreBelow, float fWaveLen,
                                 BOOL fCalcValues, float *pafValues, PCMem pMemScratch)
{
   // BUGFIX - This new version attempts to get rid of detecting high-pitched formants
   // as being harmonics

   float fHalf = fWaveLen/2;
   float fPiOverWaveLen = PI / fWaveLen;
   float fOneOverWaveLen = 1.0 / fWaveLen;
   float fi, fSin;
   float fStartHarmonics = ((float)STARTHARMONICS + 0.5) * fWaveLen;
   float fMaxHarmonics = ((float)MAXHARMONICS + 0.5) * fWaveLen;
   DWORD dwMaxHarmonics = (DWORD) ceil(fMaxHarmonics);
   dwMaxHarmonics = min(dwFFTSize, dwMaxHarmonics+1);   // no point going so far that will alwayss be 0

   float *pafRampOnly = pafValues;
   float *pafRampSin = pafValues + dwFFTSize;

   // calculate values
   DWORD i;
   if (fCalcValues) for (i = dwIgnoreBelow; i < dwMaxHarmonics; i++) {
      fi = (fp) i - fHalf;
      if (fi <= 0) {
         pafRampOnly[i] = 1.0;
         pafRampSin[i] = 0;   // sin is zeroed
         continue;
      }

      // calculate the weighting factor
      if ((float)i <= fStartHarmonics)
         pafRampOnly[i] = 1.0;
      else if ((float)i >= fMaxHarmonics)
         pafRampOnly[i] = 0.0;
      else 
         pafRampOnly[i] = ((float)i - fMaxHarmonics) / (fStartHarmonics - fMaxHarmonics);

      // calculate the since
      fSin = sin(fi * fPiOverWaveLen);
      // BUGFIX - try removing one so not as pointy for blizzard 2007 voice: fSin *= fSin;
      fSin *= fSin;  // again, to make pointier

      // fSin *= fSin;  // again, to make pointier
      // fSin *= fSin;  // again, to make pointier
      pafRampSin[i] = fSin * pafRampOnly[i];
   } // i

   // NOTE: Ideal code would be to reconstruct the harmonics using additive sine wave
   // sampling, then apply window and FFT it. Compare these results against what have and
   // use as a difference, but too slow

   // find the total score for each node of the sine, as well as total weighting
   float afEnergyHarmonic[MAXHARMONICS], afSumRampSin[MAXHARMONICS];
   if (!pMemScratch->Required (dwFFTSize * 2 * sizeof(float)))
      return FALSE;
   float *pafScratch = (float*)pMemScratch->p;
   float *pafRampOrig = pafScratch;
   float *pafIdeal = pafScratch + dwFFTSize;
   memset (afEnergyHarmonic, 0, sizeof(afEnergyHarmonic));
   memset (afSumRampSin, 0, sizeof(afSumRampSin));
   for (i = dwIgnoreBelow; i < dwMaxHarmonics; i++) {
      fi = (fp) (i - fHalf) * fOneOverWaveLen;
      if (fi <= 0) {
         pafRampOrig[i] = 0;
         continue;
      }
      DWORD dwHarmonic = (DWORD) floor(fi);
      if (dwHarmonic >= MAXHARMONICS) {
         pafRampOrig[i] = 0;
         continue;
      }

      pafRampOrig[i] = pafFFT[i] * pafRampSin[i];
      afSumRampSin[dwHarmonic] += pafRampSin[i]; // sin * weight
      afEnergyHarmonic[dwHarmonic] += pafRampOrig[i];  // energy * weight * sin
   } // i
#if 0 // def _DEBUG
   float afEnergyHarmonicOrig[MAXHARMONICS];
   memcpy (afEnergyHarmonicOrig, afEnergyHarmonic, sizeof(afEnergyHarmonicOrig));
#endif
   for (i = 0; i < MAXHARMONICS; i++)
      if (afSumRampSin[i])
         afEnergyHarmonic[i] /= afSumRampSin[i];

   // figure out the ideal
#ifndef PREVIOUSPITCHDETECT
   fp fError = 0, fBestCase = 0; //, fSumRampOnly = 0;
#else
   fp fLengthReal = 0, fLengthIdeal = 0, fDotProd = 0;
#endif
   for (i = dwIgnoreBelow; i < dwMaxHarmonics; i++) {
      fi = (fp) (i - fHalf) * fOneOverWaveLen;

      if (fi <= 0)
         pafIdeal[i] = 0; // below pitch, so shouldnt be any
      else {
         DWORD dwHarmonic = (DWORD) floor(fi);

         // basically assume that the energy is evenly distributed around the harmonic
         if (dwHarmonic < MAXHARMONICS)
            pafIdeal[i] = pafRampSin[i] /* sin */ * afEnergyHarmonic[dwHarmonic];
         else
            pafIdeal[i] = 0;
      }

#ifndef PREVIOUSPITCHDETECT
      // keep track of an error between the ideal and original
      fError += (pafIdeal[i] - pafRampOrig[i]) * (pafIdeal[i] - pafRampOrig[i]);
      fBestCase += pafIdeal[i] * pafIdeal[i];
      // fSumRampOnly += pafRampOnly[i] * pafRampOnly[i];
#else // PREVIOUSPITCHDETECT
      // keep track of how much the ideal varies from the real
      fLengthReal += pafRampOrig[i] * pafRampOrig[i];
      fLengthIdeal += pafIdeal[i] * pafIdeal[i];
      fDotProd += pafRampOrig[i] * pafIdeal[i];
#endif
   } // i

#ifndef PREVIOUSPITCHDETECT
   fBestCase = sqrt(fBestCase);
   fError = sqrt(fError);
   // fSumRampOnly = sqrt(fSumRampOnly);

   return max(fBestCase - fError, 0.0) / (fp)dwFFTSize * 100.0;
         // * 100.0 is just to make sure bright on the UI histogram

#else
   if (fLengthReal && fLengthIdeal)
      fDotProd /= (sqrt(fLengthReal) * sqrt(fLengthIdeal));

   // find out how
   // now, go through again, predicting an ideal wave at that point vs. what
   // really is
   fp fTotal = 0, fGood = 0;
   for (i = dwIgnoreBelow; i < dwMaxHarmonics; i++) {
      // ideal
      // fp fIdeal = pafIdeal[i];

      // determine what this value is after weighting
      fp fReal = pafFFT[i] * pafRampOnly[i]; // energy * weight * sin

      // and what the value should be when multiplyed by sin (good part)
      fp fTimesSin = pafRampOrig[i];

      // do the dot product
      fTotal += fReal;
      fGood += fTimesSin; // min(fTimesSin, fIdeal); // fTimesSin - fabs(fTimesSin - fIdeal);
         // NOTE: Reducing good by error with what consider ideal
   } // i
   fGood = fGood - (fTotal - fGood);   // subtract the bad (fTotal-fGood) from the good
   fGood *= fDotProd;
         // NOTE: Trying to reduce amount of good if not an exact match of what expect
   fGood /= sqrt((fp)dwFFTSize);
   return max(fGood, 0);
#endif
}



#else // OLDAPPROACH
static __inline float CombScore (DWORD dwFFTSize, float *pafFFT, DWORD dwIgnoreBelow, float fWaveLen,
                                 BOOL fCalcValues, float *pafValues, PCMem pMemScratch/*not used*/)
{
   float fHalf = fWaveLen/2;
   float fPiOverWaveLen = PI / fWaveLen;
   float fi, fSin;
   float fStartHarmonics = ((float)STARTHARMONICS + 0.5) * fWaveLen;
   float fMaxHarmonics = ((float)MAXHARMONICS + 0.5) * fWaveLen;
   DWORD dwMaxHarmonics = (DWORD) ceil(fMaxHarmonics);
   dwMaxHarmonics = min(dwFFTSize, dwMaxHarmonics+1);   // no point going so far that will alwayss be 0

   DWORD i;
   // note: ignore pafFFT[0] because DC offset and shouldnt affect one way or the other
   fp fTotal, fGood;
   fGood = fTotal = 0;
   for (i = dwIgnoreBelow; i < dwMaxHarmonics /* was dwFFTSize*/; i++) {
      fi = (fp) i - fHalf;
      if (fi <= 0) {
         fTotal += pafFFT[i];
            // NOTE: Don't need to multiply by overall weighting
         continue;
      }

      // else, use sin2
      if (fCalcValues) {
         // calculate the weighting factor
         if ((float)i <= fStartHarmonics)
            pafValues[i] = 1.0;
         else if ((float)i >= fMaxHarmonics)
            pafValues[i] = 0.0;
         else 
            pafValues[i] = ((float)i - fMaxHarmonics) / (fStartHarmonics - fMaxHarmonics);

         // calculate the since
         fSin = sin(fi * fPiOverWaveLen);
         fSin *= fSin;
         pafValues[i+dwFFTSize] = fSin * pafValues[i];
      }
      else
         fSin = pafValues[i+dwFFTSize];

      fTotal += pafFFT[i] * pafValues[i];
      fGood += pafFFT[i] * fSin;
   } // i

   fGood = fGood - (fTotal - fGood);   // subtract the bad (fTotal-fGood) from the good
   // BUGFIX - Remove fGood /= (float) dwFFTSize;   // so find average
   // BUGFIX - Remove, since technically isnt proper thing to do - fGood += fTotal/16;   // BUGFIX - Eliminating too much

   // BUGFIX - dont do average, but divide by sqrt
   fGood /= sqrt(dwFFTSize);
   return max(fGood, 0);
}


#endif // OLDAPPROACH



/****************************************************************************
CM3DWave::AnalyzePitchSpectrum - Takes an FFT around the given sample
and fills a pitch-spectrum analsis memory.

inputs
   int         iSample - Sample to analyze around
   DWORD       dwChannel - Channel. if -1 then average all channels
   DWORD       dwWindowSize - Size of the window, in pixels (to analyze)
   float       *pafWindow - Pointer to memory containing the window generated by CreateFFTWindow()
   DWORD       dwResultSize - Number of samples for the result. Must be <= dwWindowSize/4.
                  Use a lower value if only care about lower frequencies
   float       fWavelenInc - Amount wavelength is increased for every result. If it's 1.0
                  then do one wavelength per FFT band. If 0.5 then do 2 wavelengths per FFT band.
   float       *pafResult - Filled with dwResultSize entries for frequency.
   PCMem       pMem - Temp. This will be filled in with some calculated values by
               this function. It speeds up pitch detection when used over a range of samples.
               On the first call pMem->m_dwCurPosn must be 0
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
   PCListFixed plPITCHDETECTVALUE - From PitchDetectGetPITCHDETECTVALUE()
returns
   BOOL - TRUE if success
*/
#define ANALYZEPITCHSPECTRUM_IDEALENERGY     250.0
#define ANALYZEPITCHSPECTRUM_MINENERGY       0.1         // stop scaling energy when quieter than this


#define ANALYZESKIP        4        // first pass does one every 4

BOOL CM3DWave::AnalyzePitchSpectrum (int iSample, DWORD dwChannel, DWORD dwWindowSize,
                                     float *pafWindow, DWORD dwResultSize, float fWavelenInc, float *pafResult,
                                     PCMem pMem, PCSinLUT pLUT, PCMem pMemFFTScratch,
                                     PCListFixed plPITCHDETECTVALUE)
{
   // scratch memory to hold FFT
   CMem memFFT, memScratch;
   float *pfFFT;
   DWORD dwHalfWindow = dwWindowSize/2;
   if (!memFFT.Required (dwHalfWindow *sizeof(float)))
      return FALSE;
   pfFFT = (float*) memFFT.p;

   // ignore any frequencies below 50 hz since they're likely to be DC offset
   // or breath
   DWORD dwIgnoreBelow = dwWindowSize * 10 / m_dwSamplesPerSec + 1;
   // BUGFIX - Ignore below was 100 hz. changed to 50 hz so could get low voices
   // BUGFIX - Changed back to 100 hz.


   // get the fft. Resulting points will be in units that passed in
   double fEnergy;
   FFT (dwWindowSize, iSample, dwChannel, pafWindow, pfFFT, pLUT, pMemFFTScratch, &fEnergy, 0);

#ifdef FEATUREHACK_PITCHANALYSISADJUSTVOLUME
   // BUGFIX - Scale the pitch-detect score by the invert of the energy
   fEnergy /= ANALYZEPITCHSPECTRUM_IDEALENERGY;
   fEnergy = max(fEnergy, ANALYZEPITCHSPECTRUM_MINENERGY);
   fEnergy = pow(fEnergy, 0.75);   // so still de-weights
   fEnergy = 1.0 / fEnergy;   // so change to a scale
#else
   fEnergy = 1.0;
#endif
   

#if 0 // hack to test
   memset (pfFFT, 0, dwHalfWindow * sizeof(float));
   pfFFT[100] = 32767;
#endif // 0

   // need to caluclate?
   DWORD dwFFTSize = 2 * dwResultSize;
   BOOL fCalcValues = (pMem->m_dwCurPosn == 0);
   if (fCalcValues) {
      DWORD dwNeed = dwFFTSize * dwResultSize * 2 * sizeof(float);
      if (!pMem->Required (dwNeed))
         return FALSE;
      pMem->m_dwCurPosn = dwNeed;
   }
   float *pafMem = (float*)pMem->p;

   // do a comb sweep
   DWORD i, dwPass;
   float fCur;
   fp fCutoff = 0;
   // BUGFIX - Two pass, first is very rough detail, second fills in more dense areas
   for (dwPass = 0; dwPass < 2; dwPass++) {
      DWORD dwInc = dwPass ? 1 : ANALYZESKIP;
      fp fWavelenIncPass = fWavelenInc * (fp)dwInc;
      fp fMax = 0;

      for (i = 0, fCur = 0; i < dwResultSize; i += dwInc, fCur += fWavelenIncPass) {
         if (fCur < 2) {
            pafResult[i] = 0; // since cant analyze this
            continue;
         }

         if (dwPass) {
            // if this is the second pass, see what the adjacent values are
            DWORD dwMod = i % ANALYZESKIP;
            if (!dwMod)
               continue;   // already calculated this

            // if left is loud enough, calc own score
            DWORD dwLeft = i - dwMod;
            fp fLeft = pafResult[dwLeft];

            // if right is loud enough then calc own score
            DWORD dwRight = dwLeft + ANALYZESKIP;
            fp fRight = (dwRight < dwResultSize) ? pafResult[dwRight] : 0;

            // if neither left nor right is high enough then interp
            if ((fLeft <= fCutoff) && (fRight <= fCutoff)) {
               pafResult[i] = ((fp)(ANALYZESKIP - dwMod) * fLeft + (fp)dwMod * fRight) / (fp)ANALYZESKIP;
               continue;
            }

         }

         // BUGFIX - new comb score
         fp fPitchCur = fCur * (fp)m_dwSamplesPerSec / (fp)dwWindowSize;
         pafResult[i] = CombScoreNew (dwWindowSize/2, pfFFT, m_dwSamplesPerSec, fPitchCur,
            plPITCHDETECTVALUE, &memScratch) * fEnergy;
      
         // BUGFIX - This was the old code
         // pafResult[i] = CombScore (dwFFTSize, pfFFT, dwIgnoreBelow, fCur,
         //   fCalcValues, pafMem + i * dwFFTSize * 2, &memScratch) * fEnergy;
         // BUGFIX - Pass in 2 * dwResultSize instead of dwWIndowSize/2... no
         // need to check higher frequencies

         fMax = max(fMax, pafResult[i]);
      } // i

      fCutoff = fMax / 4.0;   // for the next pass, the cutoff is 1/4 of the maximum
   } // dwPass


   return TRUE;
}






typedef struct {
   DWORD       dwStart;          // starting point (inclusive)
   DWORD       dwEnd;            // ending point (inclusive)
   float       fScore;           // sum of all scores
} WVPATCH, *PWVPATCH;

/***********************************************************************
WVPATCHSort */
static DWORD gdwMinPatchLen;
static int _cdecl WVPATCHSort (const void *elem1, const void *elem2)
{
   WVPATCH *pdw1, *pdw2;
   pdw1 = (WVPATCH*) elem1;
   pdw2 = (WVPATCH*) elem2;

   BOOL f1TooShort, f2TooShort;
   f1TooShort = ((pdw1->dwEnd- pdw1->dwStart + 1) < gdwMinPatchLen);
   f2TooShort = ((pdw2->dwEnd- pdw2->dwStart + 1) < gdwMinPatchLen);
   if (f1TooShort && !f2TooShort)
      return 1;   // elem 2 occurs before 1
   else if (!f1TooShort && f2TooShort)
      return -1;  // elem 1 occurs before 1

   // else, compare energy
   if (pdw1->fScore < pdw2->fScore)
      return 1;   // elem 2 occurs before 1
   else if (pdw1->fScore > pdw2->fScore)
      return -1;  // elem 1 occurs before 2
   else
      return 0;
}

/****************************************************************************
CM3DWave::CalcPitchExpandHyp - Expand the hypothesis list.

inputs
   PCListFixed          plOrig - Original hypthesis, PITCHHYP. If empty, a blank one is created
   PCListFixed          plNew - Initialized to sizeof(PITCHHYP), and fill with hypothesis
   PWVPITCHEXTRA        pExtra - Extra one that looking at. Can be NULL, in which case just copies
   fp                   fNothingScore - Score if choose nothing
   fp                   fChangePenalty - Penalty if outside minimum
   fp                   fPitchChangePenalty - Penalty for changing a full octave in 1 segment
returns
   BYTE - Value that got rid of. 0 for ignore pitch, 1 for highest, 2 for second highest, etc.
            From PITCHHYPHISTORY calls ago.
*/
static int _cdecl PITCHHYPSort (const void *elem1, const void *elem2)
{
   PITCHHYP *pdw1, *pdw2;
   pdw1 = (PITCHHYP*) elem1;
   pdw2 = (PITCHHYP*) elem2;

   // else, compare energy
   if (pdw1->fScore < pdw2->fScore)
      return 1;   // elem 2 occurs before 1
   else if (pdw1->fScore > pdw2->fScore)
      return -1;  // elem 1 occurs before 2
   else
      return 0;
}

static int _cdecl PITCHHYPSort2 (const void *elem1, const void *elem2)
{
   PITCHHYP *pdw1, *pdw2;
   pdw1 = (PITCHHYP*) elem1;
   pdw2 = (PITCHHYP*) elem2;

   // need to worry about last element since causes state change
   int iRet = (int)pdw1->abUsed[PITCHHYPHISTORY-1] - (int)pdw2->abUsed[PITCHHYPHISTORY-1];
   if (iRet)
      return iRet;

   // since last known pitch time is a state changing effect, keep
   iRet = (int)pdw1->dwLastKnownPitchTime - (int)pdw2->dwLastKnownPitchTime;
   if (iRet)
      return iRet;

   // since last known pitch affects state, deal with
   if (pdw1->fLastKnownPitch < pdw2->fLastKnownPitch)
      return 1;   // elem 2 occurs before 1
   else if (pdw1->fLastKnownPitch > pdw2->fLastKnownPitch)
      return -1;  // elem 1 occurs before 2

   // else, compare score
   if (pdw1->fScore < pdw2->fScore)
      return 1;   // elem 2 occurs before 1
   else if (pdw1->fScore > pdw2->fScore)
      return -1;  // elem 1 occurs before 2
   else
      return 0;
}

BYTE CM3DWave::CalcPitchExpandHyp (PCListFixed plOrig, PCListFixed plNew,
                                   PWVPITCHEXTRA pExtra, fp fNothingScore, fp fChangePenalty,
                                   fp fPitchChangePenalty)
{
   // if empty then start with empty one
   PITCHHYP ph;
   if (!plOrig->Num()) {
      memset (&ph, 0, sizeof(ph));
      plOrig->Init (sizeof(ph), &ph, 1);
   }

   // assume that the original is sorted by score
   // since will be adding one entry, and thus removing one from the beginning,
   // rember the entry value for the top, and remove any others
   PPITCHHYP pphOrig = (PPITCHHYP) plOrig->Get(0);
   BYTE bTop = pphOrig[0].abUsed[0];
   DWORD i;
   for (i = plOrig->Num()-1; i >= 1; i--)
      if (pphOrig[i].abUsed[0] != bTop) {
         // different history, so delete
         plOrig->Remove (i);
         pphOrig = (PPITCHHYP) plOrig->Get(0);
      }

   // sort, looking for duplicates
   qsort (plOrig->Get(0), plOrig->Num(), sizeof(PITCHHYP), PITCHHYPSort2);
   for (i = plOrig->Num()-1; i >= 1; i--)
      if (
         (pphOrig[i].abUsed[PITCHHYPHISTORY-1] == pphOrig[i-1].abUsed[PITCHHYPHISTORY-1]) &&
         (pphOrig[i].dwLastKnownPitchTime == pphOrig[i-1].dwLastKnownPitchTime) &&
         (pphOrig[i].fLastKnownPitch == pphOrig[i-1].fLastKnownPitch) ) {
         // same, sol delete the lower since will have lower score
         plOrig->Remove (i);
         pphOrig = (PPITCHHYP) plOrig->Get(0);
      }

   // init other list
   plNew->Init (sizeof(ph));

   // expand
   DWORD j; //, k;
   pphOrig = (PPITCHHYP) plOrig->Get(0);
   fp fLogScale = (fp) m_dwSamplesPerSec / (fp)m_adwPitchSkip[PITCH_F0] / 200.0;
   for (i = 0; i < plOrig->Num(); i++, pphOrig++) {
      memcpy (&ph.abUsed[0], pphOrig->abUsed + 1, sizeof(ph.abUsed)-sizeof(BYTE));

      if (!pExtra) {
         // end of data
         ph.abUsed[PITCHHYPHISTORY-1] = 0;
         ph.fScore = pphOrig->fScore;
         ph.fLastKnownPitch = pphOrig->fLastKnownPitch;
         ph.dwLastKnownPitchTime = pphOrig->dwLastKnownPitchTime+1;
         plNew->Add (&ph);
         continue;
      }

      // loop over possibilities
      for (j = 0; j <= NUMWVMATCH; j++) { // intentionally <=
         ph.fScore = pphOrig->fScore;
         ph.fLastKnownPitch = pphOrig->fLastKnownPitch;
         ph.dwLastKnownPitchTime = pphOrig->dwLastKnownPitchTime+1;
         fp fPitchThis = 0;
         BOOL fUsePitchPenalty = FALSE;

         if (j >= NUMWVMATCH) {
            // choosing nothing
            ph.abUsed[PITCHHYPHISTORY-1] = 0;
            ph.fScore += fNothingScore;
            if (ph.abUsed[PITCHHYPHISTORY-2])
               ph.fScore -= fChangePenalty;  // since changed
         }
         else if (!ph.abUsed[PITCHHYPHISTORY-2]) {
            if (!pExtra->afStrength[j])
               continue;   // must have some strength to count

            ph.abUsed[PITCHHYPHISTORY-1] = j+1;
            ph.fScore -= fChangePenalty;  // since changed
            ph.fScore += pExtra->afStrength[j];
            fPitchThis = pExtra->afFreq[j];
            fUsePitchPenalty = TRUE;
         }
         else {   // was pitch before, and keeping pitch now
            if (!pExtra->afStrength[j])
               continue;   // must have some strength to count

            ph.abUsed[PITCHHYPHISTORY-1] = j+1;
            ph.fScore += pExtra->afStrength[j];
            fPitchThis = pExtra->afFreq[j];

#if 0
            // if minima/maximum dont add up then penalty
            BYTE bPrev = ph.abUsed[PITCHHYPHISTORY-2]-1;
            int aiRange[2][2]; // , aiRangeExtreme[2][2];
            aiRange[0][0] = (int)pExtra[0].awMaximaStart[j];
            aiRange[0][1] = (int)pExtra[0].awMaximaEnd[j];
            aiRange[1][0] = (int)pExtra[-1].awMaximaStart[bPrev];
            aiRange[1][1] = (int)pExtra[-1].awMaximaEnd[bPrev];

#if 0
            // calculate extreme ranges
            for (k = 0; k < 2; k++) {
               int iDist = aiRange[k][1] - aiRange[k][0];
               iDist = max(iDist, 1);
               aiRangeExtreme[k][0] = aiRange[k][0] - iDist;
               aiRangeExtreme[k][1] = aiRange[k][1] + iDist;
            }

            if ( (aiRangeExtreme[0][0] >= aiRangeExtreme[1][1] + 4) ||
               (aiRangeExtreme[0][1] + 4 <= aiRangeExtreme[1][0]) )
                  ph.fScore -= fChangePenalty * 3;  // since changed a lot
            else 
#endif // 0
            if ( (aiRange[0][0] >= aiRange[1][1] + 4) ||
               (aiRange[0][1] + 4 <= aiRange[1][0]) )
#endif // 0
                  fUsePitchPenalty = TRUE;
         }

#if 0// def _DEBUG
         BOOL fDebugged = FALSE;
         if ((fPitchThis == 40.0) && (ph.fLastKnownPitch == 20.0)) {
            WCHAR szTemp[256];
            swprintf (szTemp, L"\r\n\tScore=%g now=%d last=%d PitchTime=%d",
               (double)ph.fScore, (int)ph.abUsed[PITCHHYPHISTORY-1], (int)ph.abUsed[PITCHHYPHISTORY-1],
               (int)ph.dwLastKnownPitchTime);
            OutputDebugStringW (szTemp);
            fDebugged = TRUE;
         }
#endif

         // if have new pitch
         if (fPitchThis) {
            // if ther was an odler pitch then error
            if (ph.fLastKnownPitch && fUsePitchPenalty) {
               fp fLog = fabs(log((double)fPitchThis / (double)ph.fLastKnownPitch)) / log((double)2);
               fLog *= (fp)fLogScale / (fp)ph.dwLastKnownPitchTime;   // over time
               fLog -= .10;   // allow minor changes
                  // BUGFIX - making non-linear because really want to dissuade instant jumps
                  // BUGFIX - Uppsed from 0.05 to 0.1

               if (fLog > 0)
                  ph.fScore -= fLog * fPitchChangePenalty;
            }

            // remember this
            ph.fLastKnownPitch = fPitchThis;
            ph.dwLastKnownPitchTime = 0;
         }
#if 0 // def _DEBUG
         if (fDebugged) {
            WCHAR szTemp[256];
            swprintf (szTemp, L"\r\n\t\tScore=%g",
               (double)ph.fScore);
            OutputDebugStringW (szTemp);
         }
#endif
         // add this
         plNew->Add (&ph);
      } // j
   } // i

   // sort by score, and remove lowest
   qsort (plNew->Get(0), plNew->Num(), sizeof(PITCHHYP), PITCHHYPSort);
   pphOrig = (PPITCHHYP) plNew->Get(0);

#if 0 // def _DEBUG
      WCHAR szTemp[64];
      swprintf (szTemp, L"\r\nPitch score = %g", pphOrig[0].fScore);
      OutputDebugStringW (szTemp);
#endif

   // trim
#define MAXPITCHHYP        200
   if (plNew->Num() > MAXPITCHHYP)
      plNew->Truncate (MAXPITCHHYP);  // no more than 100 hypothesis

   return bTop;
}


/****************************************************************************
CM3DWave::AnalyzePitchSpectrumBlock - Analyzes a block of pitch spectrums.
This is better since it also multiplies by the average of all the spectrums
and hones in on the typical pich better.

inputs
   DWORD       dwSamples - Number of samples to calc
   double      fSampleStart - Starting sample
   double      fSampleDelta - How much to increase the sample by each time

   DWORD       dwChannel - Channel. if -1 then average all channels
   PCMem       pMemBlock - Allocated to an array of floats * dwSamples * dwResultSize, and
                  fill with the pitch spectrums, starting at fStartSample,
                  increasing by fSampleDelta each time
   DWORD       *pdwResultSize - Filled in with the number of samples for the result. Must be <= dwWindowSize/4.
                  Use a lower value if only care about lower frequencies
   DWORD       *pdwWindowSize - Filled with the window size
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::AnalyzePitchSpectrumBlock (DWORD dwSamples, double fSampleStart, double fSampleDelta,
                                           DWORD dwChannel,
                                           PCMem pMemBlock, DWORD *pdwResultSize, DWORD *pdwWindowSize,
                                           PCProgressSocket pProgress)
{
   // how much need
   DWORD dwWindowSize = WindowSize() * ANALYZEPITCHWINDOWSCALE; // BUGFIX - Make larger window so more accurate calcing pitch
   DWORD dwResultSize;
   dwResultSize = 500 * dwWindowSize * SUPERSAMPPITCH / m_dwSamplesPerSec;
   dwResultSize = min(dwWindowSize/4, dwResultSize);   // only lowest frequency limits
   *pdwResultSize = dwResultSize;
   *pdwWindowSize = dwWindowSize;

   float fWavelenInc = 1.0 / (fp) SUPERSAMPPITCH;
      // Amount wavelength is increased for every result. If it's 1.0
      // then do one wavelength per FFT band. If 0.5 then do 2 wavelengths per FFT band.

   // make sure enough memory
   if (!pMemBlock->Required (dwSamples * dwResultSize * sizeof(float)))
      return FALSE;
   float *paf = (float*)pMemBlock->p;

   CSinLUT SinLUT;
   CMem memFFTScratch;

   // memory for doubles sum
   CMem memSum, memTemp;
   if (!memSum.Required (dwResultSize * sizeof(double) + dwResultSize * sizeof(float)))
      return FALSE;
   double *pafSum = (double*) memSum.p;
   float *pafResult = (float*) (pafSum + dwResultSize);
   memset (memSum.p, 0, memSum.m_dwAllocated);

   // create the window
   CMem memWindow;
   float *pafWindow;
   if (!memWindow.Required (dwWindowSize * sizeof(float))) {
      m_adwPitchSamples[PITCH_F0] = 0;
      return FALSE;
   }
   pafWindow = (float*) memWindow.p;
   CreateFFTWindow (3, pafWindow, dwWindowSize);

   // get the right pitch-detect precalc for this wave
   PCListFixed plPITCHDETECTVALUE = PitchDetectGetPITCHDETECTVALUE (m_dwSamplesPerSec, dwWindowSize, pafWindow);
   if (!plPITCHDETECTVALUE) {
      m_adwPitchSamples[PITCH_F0] = 0;
      return FALSE;
   }

   // loop
   DWORD i, j;
   for (i = 0; i < dwSamples; i++, fSampleStart += fSampleDelta) {
      if (pProgress && !(i % 16))
         pProgress->Update ((fp)i / (fp)dwSamples);

      if (!AnalyzePitchSpectrum ((int)fSampleStart, dwChannel, dwWindowSize,
         pafWindow, dwResultSize, fWavelenInc, pafResult, &memTemp, &SinLUT, &memFFTScratch, plPITCHDETECTVALUE))
         return FALSE;

      // sum
      for (j = 0; j < dwResultSize; j++)
         pafSum[j] += pafResult[j];
      
      // copy over
      memcpy (paf + (i * dwResultSize), pafResult, dwResultSize * sizeof(float));
   } // i

   // find the max
   double fMax = 0;
   for (j = 0; j < dwResultSize; j++) {
      pafSum[j] = sqrt(pafSum[j]); // so not too overpowering
      fMax = max(fMax, pafSum[j]);
   }

   // scale sums
   if (fMax)
      fMax = 1.0 / fMax;
   for (j = 0; j < dwResultSize; j++)
      pafSum[j] *= fMax;

   // scale all the samples
   for (i = 0; i < dwSamples; i++)
      for (j = 0; j < dwResultSize; j++)
         paf[i*dwResultSize + j] *= pafSum[j];
   
   return TRUE;
}



/****************************************************************************
CM3DWave::CalcPitchInternalA - Internal function for calculating pitch.

inputs
   BOOL                 fFastPitch - If TRUE then fast pitch calculation, half the data points
   PCProgressSocket     pProgress - Progress bar
returns
   BOOL - TRUE if successful
*/
BOOL CM3DWave::CalcPitchInternalA (BOOL fFastPitch, PCProgressSocket pProgress)
{
   // BUGFIX - Make pitchskip same as SR skip
   m_adwPitchSkip[PITCH_F0] = m_dwSamplesPerSec / m_dwSRSAMPLESPERSEC;
   if (fFastPitch)
      m_adwPitchSkip[PITCH_F0] *= 2;
   //m_adwPitchSkip[PITCH_F0] = WindowSize() / 4;
   m_adwPitchSamples[PITCH_F0] = m_dwSamples / m_adwPitchSkip[PITCH_F0] + 1;
   // BUGFIX - Since changed scale, change definition m_fPitchMaxStrength = 500.0;
   //m_fPitchMaxStrength = .1;
   m_afPitchMaxStrength[PITCH_F0] = 2000;   // BUGFIX - Moved back to 2000
      // BUXFIX - Was EPSILON, but tests have shown that is around 2000, so
      // want something reasonable in case total silence
   if (!m_amemPitch[PITCH_F0].Required (m_adwPitchSamples[PITCH_F0] * m_dwChannels * sizeof(WVPITCH))) {
      m_adwPitchSamples[PITCH_F0] = 0;
      return FALSE;
   }
   m_apPitch[PITCH_F0] = (PWVPITCH) m_amemPitch[PITCH_F0].p;

   // create some scratch memory that store extra pitch info
   CMem memExtra;
   PWVPITCHEXTRA pExtra;
   if (!memExtra.Required (m_adwPitchSamples[PITCH_F0] * sizeof (WVPITCHEXTRA))) {
      m_adwPitchSamples[PITCH_F0] = 0;
      return FALSE;
   }
   pExtra = (PWVPITCHEXTRA) memExtra.p;

   fp fFreqRange = 0;



   // list of patches
   CListFixed lPatch;
   lPatch.Init (sizeof(WVPATCH));
   CSinLUT SinLUT;
   CMem memFFTScratch;

   // loop
   DWORD i, dwChan, j, dwResultSize, dwWindowSize;
   CMem memPitchSpectrum;
   for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
      if (pProgress)
         pProgress->Push ((fp)dwChan / (fp)m_dwChannels, (fp)(dwChan+1)/(fp)m_dwChannels);

      if (pProgress)
         pProgress->Push (0, .7);

      if (!AnalyzePitchSpectrumBlock (m_adwPitchSamples[PITCH_F0], 0, m_adwPitchSkip[PITCH_F0], dwChan,
         &memPitchSpectrum, &dwResultSize, &dwWindowSize, pProgress))
            return FALSE;

      fFreqRange = (fp)dwResultSize / (fp)dwWindowSize * (fp)m_dwSamplesPerSec;
      fFreqRange = fFreqRange / (fp)SUPERSAMPPITCH;

      float *pafResult = (float*)memPitchSpectrum.p;

      for (i = 0; i < m_adwPitchSamples[PITCH_F0]; i++, pafResult += dwResultSize) {


         // find the best one and second best
         for (j = 0; j < NUMWVMATCH; j++) {
            // loop over spectrum and find the maximum
            DWORD dwMax = 0;
            DWORD k, dwMinLow, dwMinHigh;
            for (k = 1; k < dwResultSize; k++)
               if (pafResult[k] > pafResult[dwMax])
                  dwMax = k;

            // find the local minima below dwMax
            dwMinLow = 0;
            if (dwMax) for (k = dwMax-1; k; k--)
               if (!pafResult[k] || (pafResult[k] < pafResult[k-1])) {
                  dwMinLow = k;
                  break;
               }

            // find the local minima above dwMax
            dwMinHigh = dwResultSize-1;
            for (k = dwMax+1; k+1 < dwResultSize; k++)
               if (!pafResult[k] || (pafResult[k] < pafResult[k+1])) {
                  dwMinHigh = k;
                  break;
               }

            // store this away
            pExtra[i].afStrength[j] = pafResult[dwMax];
            pExtra[i].afFreq[j] = (WORD) dwMax;
            pExtra[i].awMaximaEnd[j] = (WORD) dwMinHigh +2;
            pExtra[i].awMaximaStart[j] = (WORD) (dwMinLow > 2) ? (dwMinLow-2) : 0;
               // BUGFIX - Add 2 to minhigh and subtract from minlow to make forgiving

#if 0 // moved fine-tuning pitch below
            // BUGFIX - fine-tune the pitch here
            // NOTE: Use narrower beam since will fine-tune more later
            fp fVoiced, fNoise;
            pExtra[i].afFreq[j] = FineTunePitch ((int)i * (int)m_adwPitchSkip[PITCH_F0], dwChan,
               (fp)dwMax * .99, (fp)dwMax / .99, &fVoiced, &fNoise, &SinLUT, &memFFTScratch);
            // BUGFIX - remove since messing up new code with hyps: pExtra[i].afStrength[j] = fVoiced;
            // old pExtra[i].afStrength[j] = fVoiced / max(fVoiced+fNoise,CLOSE);
            // BUGFIX - Should probably change strength (and the minimum strength above)
            // to use only the voiced component since I think will be more accurate
#endif

            // NOTE: Could fine-tune a bit later and have slightly faster pitch detect,
            // but doing it now since this should make pitch detect slightly more accurate

            // BUGFIX - moved later
            m_afPitchMaxStrength[PITCH_F0] = max (m_afPitchMaxStrength[PITCH_F0], pExtra[i].afStrength[j]);

            // wipe out the value between high and low so that when do next maximum
            // wont include anything from this local maximum
            //for (k = dwMinLow; k <= dwMinHigh; k++)
            for (k = pExtra[i].awMaximaStart[j]; k <= min(pExtra[i].awMaximaEnd[j], dwResultSize-1); k++)
               pafResult[k] = 0;
         } // j, over best and second best
         pExtra[i].fUsed = FALSE;   // for later

      } // i

      // hypotheisis
      CListFixed alHyp[2];
      DWORD dwOld;
      BYTE bUsed;
      for (i = 0; i < m_adwPitchSamples[PITCH_F0] + PITCHHYPHISTORY; i++) {
         // expand
         dwOld = i % 2;
         bUsed = CalcPitchExpandHyp (&alHyp[dwOld], &alHyp[!dwOld],
            (i < m_adwPitchSamples[PITCH_F0]) ? &pExtra[i] : NULL,
            m_afPitchMaxStrength[PITCH_F0] * 0.00,   // nothing is worth something
                  // BUGFIX - reduced to 1% to try to get very quiet. Was 2%
                  // BUGFIX - reduced from 0.01 to 0.00
            m_afPitchMaxStrength[PITCH_F0] * 0.125,  // penalty for swapping
               // BUGFIX - reduced from 0.25 to 0.125 when reduced nothing to 1%
            m_afPitchMaxStrength[PITCH_F0] * 25.0);  // penalty for octave change in 1/200th of a secon
               // BUGFIX - reduced from *50.0 to *25.0

#ifdef _DEBUG
         if (i < m_adwPitchSamples[PITCH_F0])
            _ASSERTE(!pExtra[i].fUsed);
#endif

         if (bUsed && (i >= PITCHHYPHISTORY)) {

            DWORD dwOffset = i - PITCHHYPHISTORY;
            if (dwOffset >= m_adwPitchSamples[PITCH_F0])
               break;   // shouldnt happen

            // keep the one selected
            if (bUsed > 1) {
               pExtra[dwOffset].afFreq[0] = pExtra[dwOffset].afFreq[bUsed-1];
               pExtra[dwOffset].afStrength[0] = pExtra[dwOffset].afStrength[bUsed-1];
            }

            // BUGFIX - moved fine-tuning pitch here. slightly faster
            fp fVoiced, fNoise;
            pExtra[dwOffset].afFreq[0] = FineTunePitch ((int)dwOffset * (int)m_adwPitchSkip[PITCH_F0], dwChan,
               pExtra[dwOffset].afFreq[0] * .99, pExtra[dwOffset].afFreq[0] / .99, &fVoiced, &fNoise, &SinLUT, &memFFTScratch);

            pExtra[dwOffset].fUsed = TRUE;

#if 0 // def _DEBUG
            WCHAR szTemp[256];
            swprintf (szTemp, L"\r\nTime=%d, pitch=%g, bUsed=%d", (int)dwOffset, (double)pExtra[dwOffset].afFreq[bUsed-1], (int)bUsed-1);
            OutputDebugStringW (szTemp);
#endif
         }

      } // i

#if 0 // old code
      // two passes
      DWORD dwPass;
      // BUGFIX - Only do one pass
      for (dwPass = 0; dwPass < 1 /*NUMWVMATCH*/; dwPass++) {
         // on the first pass, look at the most strongest test and use that
         // on the second pass, ignore those bins that have already been filled with
         // the strongest and try the next strongest

         // make a list of all patches (sequences of continuosu pitch without breaks
         lPatch.Clear();
         WVPATCH p;
         memset (&p, 0, sizeof(p));
         p.dwEnd = -1;
         for (i = 0; i < m_adwPitchSamples[PITCH_F0]; i++) {
            // if a subsequent pass, copy the parameters up into strongest
            if (dwPass && !pExtra[i].fUsed) {
               pExtra[i].afStrength[0] = pExtra[i].afStrength[dwPass];
               pExtra[i].afFreq[0] = pExtra[i].afFreq[dwPass];
               pExtra[i].awMaximaEnd[0] = pExtra[i].awMaximaEnd[dwPass];
               pExtra[i].awMaximaStart[0] = pExtra[i].awMaximaStart[dwPass];
            }

            // if there's nothing there then start new patch and done with it
            if (p.dwEnd == -1) {
               if (pExtra[i].fUsed)
                  continue;   // if already used then skip

               p.dwStart = p.dwEnd = i;
               p.fScore = pExtra[i].afStrength[0];
               continue;
            }

            // else, contining. However, if the point has already been used then
            // just end here
            if (pExtra[i].fUsed) {
               lPatch.Add (&p);
               p.dwEnd = -1;
               continue;
            }

            // else, see if this is a break in the sequence
            if ((pExtra[i].awMaximaStart[0] >= pExtra[p.dwEnd].awMaximaEnd[0]) ||
               (pExtra[i].awMaximaEnd[0] <= pExtra[p.dwEnd].awMaximaStart[0])) {

                  // it's a break, so start a new patch
                  lPatch.Add (&p);
                  p.dwStart = p.dwEnd = i;
                  p.fScore = pExtra[i].afStrength[0];
                  continue;
               }
            
            // else, add on
            p.dwEnd = i;
            p.fScore += pExtra[i].afStrength[0];
         } // i
         if (p.dwEnd != -1)
            lPatch.Add (&p);

         // sort all the patches so the highest scores are fist
         gdwMinPatchLen = m_dwSamplesPerSec / 20 / m_adwPitchSkip[PITCH_F0] + 1; // minimum is 1/20th second
         qsort (lPatch.Get(0), lPatch.Num(), sizeof(WVPATCH), WVPATCHSort);

         // loop through all the patches
         PWVPATCH pp;
         fp fAvgScore;
         pp = (PWVPATCH) lPatch.Get(0);

         // BUGFIX - Get the score of the median patch
         PWVPATCH ppMedian = pp + (lPatch.Num()/2);
         fp fScoreMedian = ppMedian->fScore / (fp)(ppMedian->dwEnd - ppMedian->dwStart + 1);
         fScoreMedian = (fScoreMedian + m_afPitchMaxStrength[PITCH_F0] / 10.0) / 2.0; // average with weighting of max strength
         for (i = 0; i < lPatch.Num(); i++, pp++) {
            // if the average score is less than 1/10th then maximum score then skip
            fAvgScore = pp->fScore / (fp)(pp->dwEnd - pp->dwStart + 1);
            if (fAvgScore < (m_afPitchMaxStrength[PITCH_F0] / 50.0))   // BUGFIX - Make 1/50th... // BUGFIX - Make /100, was /50
               continue;

            // if this is too short continue
            if ((pp->dwEnd - pp->dwStart + 1) < gdwMinPatchLen)
               break;   // stop here since they're all too short after this

            // if this is isolated, not touching other patches, then just add
            BOOL fTouchLeft, fTouchRight;
            fTouchLeft = (pp->dwStart && pExtra[pp->dwStart-1].fUsed);
            fTouchRight = ((pp->dwEnd+1 < m_adwPitchSamples[PITCH_F0]) &&pExtra[pp->dwEnd+1].fUsed);

            // if it touches on the left see if there's a smooth transition now since
            // may be in the second pass
            // BUGFIX - Make more forgiving by adding +4
            if (fTouchLeft &&
               (pExtra[pp->dwStart].awMaximaStart[0] < pExtra[pp->dwStart-1].awMaximaEnd[0] + 4) &&
               (pExtra[pp->dwStart].awMaximaEnd[0] + 4 > pExtra[pp->dwStart-1].awMaximaStart[0]))
                  fTouchLeft = FALSE;  // pretend it doesnt touch
            if (fTouchRight &&
               (pExtra[pp->dwEnd].awMaximaStart[0] < pExtra[pp->dwEnd+1].awMaximaEnd[0] + 4) &&
               (pExtra[pp->dwEnd].awMaximaEnd[0] + 4 > pExtra[pp->dwEnd+1].awMaximaStart[0]))
                  fTouchRight = FALSE;  // pretend it doesnt touch

            // BUGFIX - If this is greater than the median score (from the first pass) then always use
            if (!dwPass && (fAvgScore >= fScoreMedian) && (fTouchRight || fTouchLeft))
               fTouchRight = fTouchLeft = FALSE;

            // if they touch either side then dont add
            if (fTouchLeft || fTouchRight)
               continue;

            // else, patch totally isolated, or ok to connect
            for (j = pp->dwStart; j <= pp->dwEnd; j++)
               pExtra[j].fUsed = TRUE;
         } // i
      } // dwPass
#endif // 0

      // tansfer over directly
      for (i = 0; i < m_adwPitchSamples[PITCH_F0]; i++) {
         if (!pExtra[i].fUsed)
            continue;   // not strong enough to use, so go back in second pass
         m_apPitch[PITCH_F0][i*m_dwChannels+dwChan].fFreq = (float)pExtra[i].afFreq[0] /
            (float)dwResultSize * fFreqRange;

         m_apPitch[PITCH_F0][i*m_dwChannels+dwChan].fStrength = pExtra[i].afStrength[0];
      }

      // loop through a second pass filling in all the unsused
      for (i = 0; i < m_adwPitchSamples[PITCH_F0]; i++) {
         if (pExtra[i].fUsed)
            continue;   // already set

         // else, unknown, so loop from here until find pitch
         for (j = i+1; j < m_adwPitchSamples[PITCH_F0]; j++)
            if (pExtra[j].fUsed)
               break;

         // start and end frequency
         float fStart, fEnd;
         if ((i == 0) && (j >= m_adwPitchSamples[PITCH_F0]))
            fStart = fEnd = 0;   // all silence
         else {
            fStart = (i == 0) ? 0 : m_apPitch[PITCH_F0][(i-1)*m_dwChannels+dwChan].fFreq;
            fEnd = (j >= m_adwPitchSamples[PITCH_F0]) ? fStart : m_apPitch[PITCH_F0][j*m_dwChannels+dwChan].fFreq;
            if (i == 0)
               fStart = fEnd; // so if nothing to left, keep constant pitch
         }

         // interpolate
         float fDelta;
         fDelta = (fEnd - fStart) / (float)(j - i + 1);
         fStart += fDelta;
         for (j = i; j < m_adwPitchSamples[PITCH_F0]; j++, fStart += fDelta) {
            if (pExtra[j].fUsed)
               break;

            // else, set
            m_apPitch[PITCH_F0][j*m_dwChannels+dwChan].fFreq = fStart;
            m_apPitch[PITCH_F0][j*m_dwChannels+dwChan].fStrength = 0;  // since guessing
         }

         // reset i
         i = j-1;
      }

      if (pProgress) {
         pProgress->Pop();
         pProgress->Push (.7, 1);
      }
      // fine-tune pitch
      for (j = 0; j < m_adwPitchSamples[PITCH_F0]; j++) {
         if (pProgress && ((j%100) == 0))
            pProgress->Update ((fp)j / (fp)m_adwPitchSamples[PITCH_F0]);

         // BUGFIX - nothing higher than MAXPITCHCANFIND hz... otherwise calculating
         // SRfeatures gets VERY slow
         if (m_apPitch[PITCH_F0][j*m_dwChannels+dwChan].fFreq >= MAXPITCHCANFIND)
            m_apPitch[PITCH_F0][j*m_dwChannels+dwChan].fFreq = MAXPITCHCANFIND;
         else if (m_apPitch[PITCH_F0][j*m_dwChannels+dwChan].fFreq <= MINPITCHCANFIND)
            m_apPitch[PITCH_F0][j*m_dwChannels+dwChan].fFreq = MINPITCHCANFIND;

         fp fWavelen = (fp)m_dwSamplesPerSec / m_apPitch[PITCH_F0][j*m_dwChannels+dwChan].fFreq;
         fWavelen = FineTunePitch ((int)j * (int)m_adwPitchSkip[PITCH_F0], dwChan,
            (DWORD)(fWavelen * .95), (DWORD)(fWavelen / .95), NULL, NULL, &SinLUT, &memFFTScratch);
               // BUGFIX - Finetune was using .9, changed to .95 so slightly more accurate
         fWavelen = (fp)m_dwSamplesPerSec / fWavelen;
         m_apPitch[PITCH_F0][j*m_dwChannels+dwChan].fFreq = fWavelen;
      }

      // two pops
      if (pProgress) {
         pProgress->Pop();
         pProgress->Pop();
      }
   } // dwChan

   return TRUE;
}


/****************************************************************************
CM3DWave::CalcPitchInternalB - Internal function for calculating pitch.

inputs
   BOOL                 fFastPitch - If TRUE then fast pitch calculation, half the data points
   PCProgressSocket     pProgress - Progress bar
returns
   BOOL - TRUE if successful
*/
BOOL CM3DWave::CalcPitchInternalB (BOOL fFastPitch, PCProgressSocket pProgress)
{
   DWORD i, dwChan;

   // if not doing fast pitch then done
   if (!fFastPitch)
      return TRUE;

   // else, lengthen
   m_adwPitchSkip[PITCH_F0] /= 2;
   DWORD dwOldSamples = m_adwPitchSamples[PITCH_F0];
   m_adwPitchSamples[PITCH_F0] = m_dwSamples / m_adwPitchSkip[PITCH_F0] + 1;

   if (!m_amemPitch[PITCH_F0].Required (m_adwPitchSamples[PITCH_F0] * m_dwChannels * sizeof(WVPITCH))) {
      m_adwPitchSamples[PITCH_F0] = 0;
      return FALSE;
   }
   m_apPitch[PITCH_F0] = (PWVPITCH) m_amemPitch[PITCH_F0].p;

   // loop backwards filling in
   for (i = m_adwPitchSamples[PITCH_F0]-1; i < m_adwPitchSamples[PITCH_F0]; i--) for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
      DWORD dwOld = i/2;

      // if an even number then exact match
      if (!(i%2)) {
         if (i != min(dwOld,dwOldSamples-1))
            m_apPitch[PITCH_F0][i*m_dwChannels+dwChan] = m_apPitch[PITCH_F0][min(dwOld,dwOldSamples-1)*m_dwChannels+dwChan];
         continue;
      }

      // else, odd, so interpolate
      DWORD dwOldNext = dwOld+1;
      dwOld = min(dwOld, dwOldSamples-1);
      dwOldNext = min(dwOldNext, dwOldSamples-1);

      fp fNewPitch = (m_apPitch[PITCH_F0][dwOld*m_dwChannels+dwChan].fFreq + m_apPitch[PITCH_F0][dwOldNext*m_dwChannels+dwChan].fFreq) / 2;
      fp fNewStrength = (m_apPitch[PITCH_F0][dwOld*m_dwChannels+dwChan].fStrength + m_apPitch[PITCH_F0][dwOldNext*m_dwChannels+dwChan].fStrength) / 2;

      // weite
      m_apPitch[PITCH_F0][i*m_dwChannels+dwChan].fFreq = fNewPitch;
      m_apPitch[PITCH_F0][i*m_dwChannels+dwChan].fStrength = fNewStrength;
   } // i

   return TRUE;
}

/****************************************************************************
CM3DWave::CalcPitch - Calculates the pitch for the wave. THis ends up filling
in m_adwPitchSamples[PITCH_F0], m_adwPitchSkip[PITCH_F0], m_pwPitch, and m_amemPitch[PITCH_F0].

inputs
   DWORD                dwCalcFor - WAVECALC_XXX
   PCProgressSocket     pProgress - Progress bar
returns
   BOOL - TRUE if successful
*/
BOOL CM3DWave::CalcPitch (DWORD dwCalcFor, PCProgressSocket pProgress)
{
   // make sure sub-pitch is cleared
   PitchSubClear();

   BOOL fFastPitch = FALSE;
      // If TRUE then fast pitch calculation, of only half
      // the data points, and then rebuild into smaller sets


   switch (dwCalcFor) {
   case WAVECALC_TRANSPROS:
   case WAVECALC_VOICECHAT:
   case WAVECALC_TTS_PARTIALPCM:
   case WAVECALC_TTS_FULLPCM:
   case WAVECALC_SEGMENT:
      // no change
      break;
   }


#ifdef _DEBUG
   DWORD dwStart =GetTickCount();
#endif

   // calcualte pitch
   if (!CalcPitchInternalA (fFastPitch, pProgress))
      return FALSE;

   // if fast pitch then re-lengthen
   if (!CalcPitchInternalB (fFastPitch, pProgress))
      return FALSE;

#ifdef _DEBUG
   char szTemp[64];
   sprintf (szTemp, "\r\nPitch time=%d", (int)GetTickCount()-dwStart);
   OutputDebugString (szTemp);
#endif

   // sub-pitch
   PitchSubCalc (this);

   return TRUE;

}


/****************************************************************************
CM3DWave::CalcPitchIfNeeded - If there's no pitch information this calculates
it, showing the progress bar.

inputs
   DWORD       dwCalcFor - WAVECALC_XXX
   HWND        hWnd - To show progress bar on
returns
   none
*/
void CM3DWave::CalcPitchIfNeeded (DWORD dwCalcFor, HWND hWnd)
{
   if (!m_adwPitchSamples[PITCH_F0]) {
      CProgress Progress;
      Progress.Start (hWnd, "Calculating pitch...");
      CalcPitch (dwCalcFor, &Progress);
   }

   // calc PITCH_SUB?
   if (!m_adwPitchSamples[PITCH_SUB])
      PitchSubCalc (this);
}


/****************************************************************************
CM3DWave::CalcSRFeaturesIfNeeded - If there's no pitch information this calculates
it, showing the progress bar.

inputs
   DWORD       dwCalcFor - WAVECALC_XXX
   HWND        hWnd - To show progress bar on
   PCProgressSocket pProgress - Progress bar to use. If NULL creates own progress bar
returns
   none
*/
void CM3DWave::CalcSRFeaturesIfNeeded (DWORD dwCalcFor, HWND hWnd, PCProgressSocket pProgress)
{
   if (!m_dwSRSamples) {
      CProgress Progress;
      if (!pProgress)
         Progress.Start (hWnd, "Calculating voice features...", TRUE);
      CalcSRFeatures (dwCalcFor, pProgress ? pProgress : &Progress);
   }
}

/****************************************************************************
CM3DWave::FXTimeStretch - Stretches or shrinks time without shifting pitch.

inputs
   fp             fStretch - Amount to stretch by. 2.0 = takes 2x as long, etc.
   PCProgressSocket pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXTimeStretch (fp fStretch, PCProgressSocket pProgress)
{
   DWORD dwNeed, dwNewSamples;
   dwNewSamples = (DWORD) ((double)m_dwSamples * (double)fStretch);
   dwNeed  = dwNewSamples * m_dwChannels * sizeof(short);

   // also create memory to store weight
   CMem memWeight;
   WORD *pawWeight;
   if (!memWeight.Required (dwNewSamples * sizeof(WORD)))
      return FALSE;
   pawWeight = (WORD*)memWeight.p;

   // allocate space to transfer to
   // create new memory with enough space
   PCMem pNew;
   pNew = new CMem;
   if (!pNew)
      return FALSE;
   if (!pNew->Required (dwNeed)) {
      delete pNew;
      return FALSE;
   }
   short *psNew;
   psNew = (short*) pNew->p;
   memset (psNew, 0, dwNeed);

   // progress
   BOOL fCalcPitch;
   fCalcPitch = !m_adwPitchSamples[PITCH_F0];
   if (fCalcPitch) {
      if (pProgress)
         pProgress->Push (0, .8);

      // calculate the pitch
      CalcPitch (WAVECALC_SEGMENT, pProgress);

      if (pProgress)
         pProgress->Pop ();

      if (!m_adwPitchSamples[PITCH_F0])
         return FALSE;

      if (pProgress)
         pProgress->Push (.8, 1);
   }

   // loop through all the channels
   DWORD dwChan, dwWindowSize;
   int iPitch;
   double fPitch, fAlpha;
   int iFrom, iTo, iWindow, iSamp;
   short sSamp;
   WORD wScore;
   dwWindowSize = WindowSize();
   DWORD j;
   DWORD dwCount;
   for (dwChan = 0; dwChan < m_dwChannels; dwChan++) {
      // progress
      if (pProgress)
         pProgress->Push ((fp)dwChan / (fp)m_dwChannels, (fp)(dwChan+1) / (fp)m_dwChannels);

      // reset the scale
      memset (pawWeight, 0, dwNewSamples * sizeof(WORD));

      // start copying from and copying to
      iFrom = 0;
      iTo = 0;
      dwCount = 0;
      while ((DWORD)iTo < dwNewSamples + dwWindowSize) {
         // progress
         if (pProgress && ((dwCount % 10000) == 0))
            pProgress->Update ((fp)iTo / (fp)dwNewSamples);
         dwCount++;

         // find the pitch where we're copying from
         fPitch = (double) iFrom / (double) m_adwPitchSkip[PITCH_F0];
         iPitch = (int) fPitch;
         fAlpha = fPitch - iPitch;
         fPitch = (1.0 - fAlpha) * m_apPitch[PITCH_F0][min((DWORD)iPitch,m_adwPitchSamples[PITCH_F0]-1)*m_dwChannels+dwChan].fFreq +
            fAlpha * m_apPitch[PITCH_F0][min((DWORD)iPitch+1,m_adwPitchSamples[PITCH_F0]-1)*m_dwChannels+dwChan].fFreq;
         fPitch = max(fPitch, 1);   // at least 1 hz
         fPitch = (fp)m_dwSamplesPerSec / fPitch;
         iPitch = (int)(fPitch+0.5);  // need as an integer
         iPitch = max(iPitch, 1);   // so at least on
         iWindow = iPitch * 2;

         while (iTo <= (double)iFrom * fStretch) {
            // duplicate this
            for (j = 0; j < (DWORD)iWindow; j++) {
               // get the original sample
               iSamp = iFrom - iWindow/2 + (int) j;
               if ((iSamp >= 0) && (iSamp < (int)m_dwSamples))
                  sSamp = m_psWave[iSamp * (int)m_dwChannels + (int)dwChan];
               else
                  sSamp = 0;

               // find out where it goes
               iSamp = iTo - iWindow/2 + (int)j;
               if ((iSamp < 0) || (iSamp >= (int)dwNewSamples))
                  continue;

               // how much is it weighted
               if (j < (DWORD)iWindow/2)
                  wScore = (WORD) ((DWORD) 0xfff * (DWORD) j / (DWORD)((iWindow-1)/2));
               else
                  wScore = (WORD) ((DWORD) 0xfff * (DWORD) ((DWORD)iWindow-j-1) / (DWORD)((iWindow-1)/2));


               // if nothing there then just copy over
               int iIndex;
               iIndex = iSamp*(int)m_dwChannels+dwChan;
               if (!pawWeight[iSamp]) {
                  pawWeight[iSamp] = wScore;
                  psNew[iIndex] = sSamp;
                  continue;
               }
               else if (pawWeight[iSamp] == 0xffff)
                  continue;   // already 100% so do nothing

               // else, average in with what's there
               int iTemp;
               iTemp = ((int)psNew[iIndex] * (int)pawWeight[iSamp] +
                  (int)sSamp * (int) wScore) / ((int)pawWeight[iSamp]+(int)wScore);
               iTemp = max(iTemp, -32768);
               iTemp = min(iTemp, 32767);
               psNew[iIndex] = (short) iTemp;
               pawWeight[iSamp] = 0xffff; // so dont try to average in more.. shouldnt happen
            }

            // increase To location
            iTo += iPitch;
         } // while copying to is behind

         // continue, advance the from location by iPitch
         iFrom += iPitch;
      } // while

      if (pProgress)
         pProgress->Pop();
   } // dwChan

   // finally copy over
   delete m_pmemWave;
   m_pmemWave = pNew;
   m_psWave = (short*) pNew->p;
   m_dwSamples = dwNewSamples;

   // stretch phonemes
   PhonemeStretch (fStretch);
   WordStretch (fStretch);

   // reset calculations
   ReleaseCalc();

   if (fCalcPitch && pProgress)
      pProgress->Pop();

   return TRUE;
}


#define NOISEEXTRA      2     // do this much larger a FFT when detecting noise  

/****************************************************************************
CM3DWave::FineTunePitch - This is a slow process that fine-tunes the pitch at
a specific point of time.

inputs
   int            iCenter - Center sample
   DWORD          dwChannel - Channel that want, or -1 for all channels
   DWORD          dwMinWaveLen - minimum wavelength that can use, in samples
   DWORD          dwMaxWaveLen - maximum wavelength that can use, in sample
   fp             *pfVoiced - Filled with the total energy in voiced
   fp             *pfNoise - Filled with the total energy in noise
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
returns
   fp - Optimium wavelength (in samples). Because NOISEEXTRA is used, this may
         end up being fractional
*/
fp CM3DWave::FineTunePitch (int iCenter, DWORD dwChannel, DWORD dwMinWaveLen, DWORD dwMaxWaveLen,
                            fp *pfVoiced, fp*pfNoise, PCSinLUT pLUT, PCMem pMemFFTScratch)
{
   // increase min and max wavelen
   dwMinWaveLen *= NOISEEXTRA;
   dwMaxWaveLen *= NOISEEXTRA;

   if (!m_dwSamples)
      return 0;   // error

   // BUGFIX - as an optimization, calculate the energy in the signal... if it's
   // very quiet then ignore. Otherwise if total silence takes a LONG time to
   // calculate since sometimes wavelengths are 1000 samples
   double fEnergy = 0;
   DWORD dwCount = 0;
   int iCur;
   for (iCur = iCenter - (int)dwMaxWaveLen; iCur < iCenter + (int)dwMaxWaveLen; iCur++) {
      if ((iCur < 0) || (iCur >= (int) m_dwSamples))
         continue; // out of range

      double f = m_psWave[iCur*(int)m_dwChannels + (int)dwChannel];
      fEnergy += f*f;
      dwCount++;
   } // iCur
   if (dwCount)
      fEnergy = sqrt(fEnergy / (double)dwCount);
#define MINENERGY       100
#define MAXENERGY       5000
#define MAXDIVISIONACCURACY      400
   DWORD dwWLDelta = dwMaxWaveLen - dwMinWaveLen;
   if (fEnergy <= MINENERGY)
      dwWLDelta /= 2;   // only try a few points
   else if (fEnergy >= MAXENERGY)
      dwWLDelta = 1; // every one since loud
   else {
      fp fAlpha = log10(MAXENERGY / fEnergy) * 20.0;  // to dB below max
      fAlpha = (1.0 + (fAlpha / 10.0)) * (fp)dwMaxWaveLen / (fp)MAXDIVISIONACCURACY;  // skip 1 PLUS 1 per 10 dB below
      dwWLDelta = (DWORD)fAlpha;
   }
   dwWLDelta = max(dwWLDelta, dwMaxWaveLen / MAXDIVISIONACCURACY); // dont try to get too accurate
   dwWLDelta = min(dwWLDelta, (dwMaxWaveLen-dwMinWaveLen)/2);  // always at least 2 points
   dwWLDelta = max(dwWLDelta, 1);

   // figure out a window size
   DWORD dwWindowSize;
   for (dwWindowSize = 8; dwWindowSize < dwMaxWaveLen; dwWindowSize *= 2);

   // allocate enough for this
   CMem memFFT;
   if (!memFFT.Required (dwWindowSize * sizeof(float) + dwWindowSize * sizeof(short)))
      return 0;   // error
   float *pafFFT = (float*) memFFT.p;
   short *psVal = (short*) (pafFFT + dwWindowSize);

   // loop over all pitches
   DWORD dwWL;
   fp fBestNoise = 0, fBestVoiced=0;
   DWORD dwBestNoise = -1;
   dwMinWaveLen = max(dwMinWaveLen, 1);   // BUGFIX - so dont get divide by 0
   for (dwWL = dwMinWaveLen; dwWL <= dwMaxWaveLen; dwWL += dwWLDelta) {
      // get all the values
      int i, iSamp;
      DWORD dwChanStart, dwChanEnd, dwChan, dwScale;
      dwChanStart = (dwChannel < m_dwChannels) ? dwChannel : 0;
      dwChanEnd = (dwChannel < m_dwChannels) ? (dwChannel+1) : m_dwChannels;
      dwScale = dwChanEnd - dwChanStart;
      int iWindow = (int)dwWL;
      for (i = 0; i < iWindow; i++) {
         psVal[i] = 0;
         iSamp = iCenter - iWindow/2 + i;
         if ((iSamp < 0) || (iSamp >= (int) m_dwSamples))
            continue;   // 0

         for (dwChan = dwChanStart; dwChan < dwChanEnd; dwChan++)
            psVal[i] += m_psWave[iSamp*(int)m_dwChannels + (int)dwChan] / (short)dwScale;
      }

      // stretch this out
      for (i = 0; i < (int)dwWindowSize; i++) {
         fp fAlpha = (fp) i / (fp)dwWindowSize * (fp) iWindow;
         DWORD dwLow = (DWORD)fAlpha;
         DWORD dwHigh = (dwLow+1)%(DWORD)iWindow;
         fAlpha -= dwLow;

         pafFFT[i] = (1.0 - fAlpha) * (fp)psVal[dwLow] + fAlpha * (fp)psVal[dwHigh];
      }

      // do FFT
      // do the FFT
      FFTRecurseReal (pafFFT - 1, dwWindowSize, 1, pLUT, pMemFFTScratch);

      // BUGFIX - Always make 0hz = 0
      pafFFT[0] = pafFFT[1] = 0;

      // find the error
      fp fVoiced = 0, fNoise = 0;
      // BUGFIX - Normally would loop to dwWindowSize/2 since that would cover
      // all the energy in the wave, but because only want to get the most accurate
      // in the voiced region do windowsize / 8 (only use bottom quarter)
      for (i = 0; i < (int) dwWindowSize/8; i++) {
         // how much energy
         fp fEnergy = pafFFT[i*2+0] * pafFFT[i*2+0] + pafFFT[i*2+1] * pafFFT[i*2+1];
            // BUGFIX - Move sqrt() to later

         // dont count the data that's supposed to be
         if (i % NOISEEXTRA)
            fNoise += fEnergy;
         else
            fVoiced += fEnergy;
      } // i

      // BUGFIX - Was just summing the sqrt() of energy in individual cells...
      // need to do so for all cells
      // BUGFIX - Scale by the size of the window - which is left in i
      fNoise = sqrt(fNoise) / (fp)i;
      fVoiced = sqrt(fVoiced) / (fp)i;

      // only really care about difference, as little noise as possible
      if ((dwBestNoise == -1) || ((fNoise - fVoiced) < (fBestNoise - fBestVoiced))) {
         dwBestNoise = dwWL;
         fBestNoise = fNoise;
         fBestVoiced = fVoiced;
      }
   } // dwWL

   // return the best noise
   if (pfNoise)
      *pfNoise = fBestNoise;
   if (pfVoiced)
      *pfVoiced = fBestVoiced;
   return (fp)dwBestNoise / (fp)NOISEEXTRA;
}


/****************************************************************************
CM3DWave::StretchFFT - This uses the m_apPitch[PITCH_F0] information (which means pitch
MUST be calculated before this) and gets the pitch at sample iCenter. This
is used to determine a set of samples. They are pulled out and stretched (interpolated)
to fit into the next largest FFT block. This is then analyzed. The result is then
interpolated into pafFFT (keeping pitch in tact), so you end up with a FFT that's
pitch independent.

inputs
   DWORD          dwWindowSize - Size of the window (in samples)
   int            iCenter - Center sample
   DWORD          dwChannel - Channel that want, or -1 for all channels
   float          *pafFFTVoiced - Filled with the FFT. dwWindowSize must be available
   float          *pafFFTNoise - If non-NULL then the function splits the sound into
                  voiced (stored in pafFFTVoiced) and unvoiced (stored in pafFFTNoise).
                  If NULL then all the audio is together.
   float          fZeroOctave - If this is 0 then pitch will go up linearly in pafFFT.
                  Otherwise, will go up exponentially, using fZeroOctave as the base
                  pitch
   float          *pafFFTPhase - If non-NULL then filled with the phase of the first
                  SRPHASENUM harmonics. Only the voiced harmonics, every NOISEEXTRA,
                  are actually calculated

                  NOTE: Because of new code will never get called with pafFFTPhase of non-NULL

   DWORD          dwDivsPerOctave - Used if fZeroOctave non-zero, this indicates the
                  number of steps per octaves. Thus, there will be dwWindowSize / dwDivsPerOctave
                  octaves covered
   DWORD          dwBlur - Amount of blurring, 1 means none. Higher numbers cause blurring
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
returns
   none
*/
void CM3DWave::StretchFFT (DWORD dwWindowSize, int iCenter, DWORD dwChannel, float *pafFFTVoiced,
                           float *pafFFTNoise, float *pafFFTPhase, float fZeroOctave, DWORD dwDivsPerOctave, DWORD dwBlur,
                           PCSinLUT pLUT, PCMem pMemFFTScratch)
{
   BOOL fExponent = (fZeroOctave ? TRUE : FALSE);

   // in this case, need a channel for pitch, so pick one
   DWORD dwChanPitch = dwChannel;
   if (dwChanPitch >= m_dwChannels)
      dwChanPitch = 0;

   // figure out the pitch
   double fPitch, fAlpha, fOrigPitch;
   int iPitch;
   fPitch = (double) iCenter / (double) m_adwPitchSkip[PITCH_F0];
   iPitch = (int) fPitch;
   fAlpha = fPitch - iPitch;
   fOrigPitch = fPitch = (1.0 - fAlpha) * m_apPitch[PITCH_F0][min((DWORD)iPitch,m_adwPitchSamples[PITCH_F0]-1)*m_dwChannels+dwChanPitch].fFreq +
      fAlpha * m_apPitch[PITCH_F0][min((DWORD)iPitch+1,m_adwPitchSamples[PITCH_F0]-1)*m_dwChannels+dwChanPitch].fFreq;
   fPitch = max(fPitch, 1);   // at least 1 hz
   fPitch = (fp)m_dwSamplesPerSec / fPitch;

   // if using noise then go for really accurate pitch calculations
   fp fMoveNoiseToVoiced = 0;  // 0 if no change, 1 if move all noise to voiced, -1 if all voiced to noise
   if (pafFFTNoise) {
      fp fBestNoise, fBestVoiced, fSum;
      fp fRange;
      // BUGFIX - Don't bother fine-tuning if the pitch skip is the same as the SR skip
      // because will have already done fine adjustments there
      fRange = (m_adwPitchSkip[PITCH_F0] == m_dwSRSkip) ? .99 : .9;
      fPitch = FineTunePitch (iCenter, dwChannel, (DWORD)(fPitch *fRange), (DWORD)(fPitch / fRange),
         &fBestVoiced, &fBestNoise, pLUT, pMemFFTScratch);

      fSum = fBestVoiced + fBestNoise;
      fSum = max(fSum, CLOSE);
      fMoveNoiseToVoiced = (fBestVoiced / fSum) * 2.0 - 1;

      // BUGFIX - Try to make this more extreme
      if (fMoveNoiseToVoiced > 0)
         fMoveNoiseToVoiced = sqrt(fMoveNoiseToVoiced);
      else
         fMoveNoiseToVoiced = -sqrt(-fMoveNoiseToVoiced);

      //fMoveNoiseToVoiced = 0; // BUGFIX - Take out move noisetovoiced since reduced quality
   }

   iPitch = (int)(fPitch+0.5);  // need as an integer
   iPitch = max(iPitch, 1);   // so at least on

   // if have noise then double the size of the window...
   int iWindow;
   iWindow = iPitch;
   if (pafFFTNoise) {
      iWindow = (int) (fPitch * NOISEEXTRA + 0.5);
      iWindow = max(iWindow, NOISEEXTRA);
   }

   // find a window that's large enough for all these
   DWORD dwRealWindowSize;
   for (dwRealWindowSize = 2; dwRealWindowSize < (DWORD)iWindow; dwRealWindowSize *= 2);

   // allocate enough memory for the FFT and temporarily copy samples
   CMem memFFT;
   float *pafFFT2, *pafFFTSeperate;
   short *psVal;
   if (!memFFT.Required (max(dwWindowSize,dwRealWindowSize) * sizeof(float) + (DWORD)iWindow*sizeof(short) +
      dwRealWindowSize * sizeof(float)))
      return;
      // do the max(dwWindowSize..) so that enough space for scratch later on
   pafFFT2 = (float*) memFFT.p;
   psVal = (short*) (pafFFT2+max(dwRealWindowSize,dwWindowSize));
   pafFFTSeperate = (float*) (psVal + iWindow);

   // get all the values
   int i, iSamp;
   DWORD dwChanStart, dwChanEnd, dwChan, dwScale;
   dwChanStart = (dwChannel < m_dwChannels) ? dwChannel : 0;
   dwChanEnd = (dwChannel < m_dwChannels) ? (dwChannel+1) : m_dwChannels;
   dwScale = dwChanEnd - dwChanStart;
   for (i = 0; i < iWindow; i++) {
      psVal[i] = 0;
      iSamp = iCenter - iWindow/2 + i;
      if ((iSamp < 0) || (iSamp >= (int) m_dwSamples))
         continue;   // 0

      for (dwChan = dwChanStart; dwChan < dwChanEnd; dwChan++)
         psVal[i] += m_psWave[iSamp*(int)m_dwChannels + (int)dwChan] / (short)dwScale;
   }

   // stretch this out
   for (i = 0; i < (int)dwRealWindowSize; i++) {
      fp fAlpha = (fp) i / (fp)dwRealWindowSize * (fp) iWindow;
      DWORD dwLow = (DWORD)fAlpha;
      DWORD dwHigh = (dwLow+1)%(DWORD)iWindow;
      fAlpha -= dwLow;

      pafFFT2[i] = (1.0 - fAlpha) * (fp)psVal[dwLow] + fAlpha * (fp)psVal[dwHigh];
   }

   // was just in here to test
   //if (pafFFTNoise) {
   //   CreateFFTWindow (3, pafFFTSeperate, dwRealWindowSize);
   //   for (i = 0; i < (int) dwRealWindowSize; i++)
   //      pafFFT2[i] *= pafFFTSeperate[i];
   //}

   // do the FFT
   FFTRecurseReal (pafFFT2 - 1, dwRealWindowSize, 1, pLUT, pMemFFTScratch);

   // BUGFIX - Always make 0hz = 0
   pafFFT2[0] = pafFFT2[1] = 0;  // BUGFIX - Was seting pafFFT[1] = 0

   // calculate the phase
   if (pafFFTPhase) {
      for (i = 0; i < SRPHASENUM; i++) {
         DWORD dwElem = (i+1)*2*NOISEEXTRA;
         if (dwElem >= dwRealWindowSize) {
            pafFFTPhase[i] = 0;
            continue;
         }

         pafFFTPhase[i] = atan2(pafFFT2[dwElem], pafFFT2[dwElem+1]);
      } // i

      for (i = SRPHASENUM-1; (i >= 1) && (i < SRPHASENUM); i--) {
         pafFFTPhase[i] -= pafFFTPhase[0] * (float)(i+1);
         //pafFFTPhase[i] -= pafFFTPhase[i-1] * (float)(i+1) / (float)i; - dont really work
         pafFFTPhase[i] = myfmod(pafFFTPhase[i], 2.0 * PI);
      }
      pafFFTPhase[0] = 0;  // always
   } // pafFFTPhase

   // scale and write away
   float   fScale = 2.0 / (float) dwRealWindowSize;// * 32768.0 / 32768.0;
   float   fScaleSqr = fScale * fScale;
   dwRealWindowSize /= 2;  // make it easier later so know much much data have to interpolate
   // dont do this since changed definition of param dwWindowSize /= 2;   // make it easier later on
   for (i = 0; i < (int)dwRealWindowSize; i++)
      pafFFT2[i] = (float) sqrt(
         (pafFFT2[i*2] * pafFFT2[i*2] + pafFFT2[i*2+1] * pafFFT2[i*2+1]) * fScaleSqr );
   
   // if we are looking for noise then isolate the noise (even-numbers) from the
   // voices (odd-numbers). Fills into pafFFTSeperate, where noise at start and voiced
   // at end
   if (pafFFTNoise) {
      // BUGFIX - know that voiced is higher, unvoiced is lower, so if get high frequences
      // that think are voiced, make them unvoiced, and vice versa
      fp fNoiseStart = 10.0 / (fp)iPitch * dwRealWindowSize; // noise starts about 8x the fundamental
      fp fVoiceEnd = 30.0 / (fp)iPitch * dwRealWindowSize;   // voice ends about 24x the fundamental
      fp fNoiseVoiceMid = (fNoiseStart + fVoiceEnd) / 2.0;

      // BUGFIX - blend together frequencies since high noise areas cause problems
      // with synthesis
      DWORD j;
      memcpy (pafFFT2 + dwRealWindowSize, pafFFT2, sizeof(pafFFT2[0]) * dwRealWindowSize);
      for (j = 0; j < NOISEEXTRA; j++)
         for (i = 0; i < (int)dwRealWindowSize / NOISEEXTRA; i++) {
            int iRange = i / 20; // average over a range depending upon the frquency,
                                 // the higher the frequency th emore the blurring
                     // BUGFIX - Changed from 20 to 10 when switch to traingle window
                     // BUGFIX - Change back to /20 so get mroe detail
            int iCur;
            DWORD dwCount = 0;
            DWORD dwWeight;
            fp fBlend = 0;
            for (iCur = i-iRange; iCur <= i + iRange; iCur++) {
               dwWeight = (DWORD)(iRange + 1 - abs(i - iCur));
               if ((iCur < 0) || (iCur >= (int)dwRealWindowSize/NOISEEXTRA))
                  continue;
               fBlend += pafFFT2[dwRealWindowSize + iCur * NOISEEXTRA + j] * (float)dwWeight;
               dwCount += dwWeight;
            }

            // average three together and put back
            if (dwCount >= 2)
               fBlend /= (fp)dwCount;
            pafFFT2[i * NOISEEXTRA + j] = fBlend; // BUGFIX - Remove because duplicate / (fp)dwCount;
         } //i,j

      float fNoise, fVoiced;
      for (i = 0; i < (int)dwRealWindowSize; i+=NOISEEXTRA) {
         if (NOISEEXTRA == 2)
            fNoise = pafFFT2[i+1];
         else if (NOISEEXTRA == 3)
            //fNoise = max(pafFFT2[i+1], pafFFT2[i+2]);
            fNoise = (pafFFT2[i+1] + pafFFT2[i+2]) / 2;
         else if (NOISEEXTRA == 4)
            //fNoise = max(max(pafFFT2[i+1], pafFFT2[i+2]), pafFFT2[i+3]);
            fNoise = (pafFFT2[i+1] + pafFFT2[i+2] + pafFFT2[i+3]) / 3;
         else if (NOISEEXTRA == 5)
            //fNoise = max (max(max(pafFFT2[i+1], pafFFT2[i+2]), pafFFT2[i+3]), pafFFT2[i+4]);
            fNoise = (pafFFT2[i+1] + pafFFT2[i+2] + pafFFT2[i+3] + pafFFT2[i+4]) / 4;
         fVoiced = pafFFT2[i];


         // BUGFIX - If sound clip is mostly noise then push more over towards noise
         // but if moustly voice then push towards voiced
         if (fMoveNoiseToVoiced > 0) {
            fVoiced += fMoveNoiseToVoiced * fNoise;
            fNoise *= (1.0 - fMoveNoiseToVoiced);
         }
         else if (fMoveNoiseToVoiced < 0) {
            fNoise -= fMoveNoiseToVoiced * fVoiced;
            fVoiced *= (1.0 + fMoveNoiseToVoiced);
         }

#if 0 // BUGFIX - Took out because actually made quality slightly worse
         // BUGFIX - if we are in the voice only region then put in voice, if in
         // the noise-only region then put in noise, else put some in each
         if (i <= fNoiseStart) {
            fVoiced += fNoise;
            fNoise = 0;
         }
         else if (i >= fVoiceEnd) {
            fNoise += fVoiced;
            fVoiced = 0;
         }
         else {
            // blend
            fp fAlpha;
            if (i <= fNoiseVoiceMid) {
               // move noise towards voice side...
               fAlpha = ((fp)i - fNoiseStart) / (fNoiseVoiceMid - fNoiseStart);
               fVoiced += fNoise * (1.0 - fAlpha);
               fNoise *= fAlpha;
            }
            else {
               // move voiced towards noise side
               fAlpha = ((fp)i - fNoiseVoiceMid) / (fVoiceEnd - fNoiseVoiceMid);
               fNoise += fVoiced * fAlpha;
               fVoiced *= (1.0 - fAlpha);
            }
         } // move force voiced and noise settings
#endif // 0


         if (fVoiced > fNoise) {
            // noise channel less than voiced channel, so have some voiced
            pafFFTSeperate[i/NOISEEXTRA + dwRealWindowSize/NOISEEXTRA] =  fVoiced - fNoise;
            pafFFTSeperate[i/NOISEEXTRA] = fNoise * NOISEEXTRA;
               // BUGFIX - Was just setting to fNOise, but this makes things wrong
         }
         else {
            // else, noise channel is more than voiced channel, so assume its all noise
            pafFFTSeperate[i/NOISEEXTRA] = fNoise * (NOISEEXTRA-1) + fVoiced;
               // BUGFIX - Was using ) / NOISEEXTRA - this is wrong
            pafFFTSeperate[i/NOISEEXTRA + dwRealWindowSize/NOISEEXTRA] = 0;
         }
      } // i

      // halve the real window size so calculatiosn ok
      dwRealWindowSize /= NOISEEXTRA;


      // BUGFIX - What's happening in the voice is that the pitch of higher harmonics
      // is changing. The result is that FFT thinks there's more noise at higher
      // frequencies thatn there really is. Therefreo, do this hack to calculate
      // the noise and lower frequencies, and then move some voiced to noise
      fp fNoiseLow = 0, fVoicedLow = 0;
      fp fAmt;
      for (i = 0; i < 5; i++) {
         fNoiseLow += pafFFTSeperate[i];
         fVoicedLow += pafFFTSeperate[dwRealWindowSize+i];
      }
      fNoiseLow /= (fp)i;
      fVoicedLow /= (fp)i;
      fVoicedLow = fVoicedLow / max(fNoiseLow+fVoicedLow,CLOSE);

      // move fAmt to extremes
      if (fVoicedLow < .5)
         fVoicedLow = pow(fVoicedLow*2, 2) * .5;
      else
         fVoicedLow = sqrt(fVoicedLow*2 - 1) / 2 + .5;

#define FFTERRSTART  30  // starts around 5th harmonic
#define FFTCANTTELL  60 // by the time on the 50th harmonic is probably noise or cant tell
         // BUGFIX - Catches more at 80
      for (i = 0; i < (int)dwRealWindowSize; i++) {
         if (i < FFTCANTTELL) {
            fAmt = pafFFTSeperate[i] * fVoicedLow;   // amount to move over

            // if between EERRSTART adn CANTTELL then interpolate
            if (i > FFTERRSTART)
               fAmt = fAmt * (1.0 - (fp)(i - FFTERRSTART) / (fp)(FFTCANTTELL - FFTERRSTART));
            // else, keep whole thing

            pafFFTSeperate[i] -= fAmt;
            pafFFTSeperate[dwRealWindowSize+i] += fAmt;
         }

         if ((i > FFTERRSTART) && (fVoicedLow < .5)) {
            fAmt = pafFFTSeperate[i] * (1.0 - fVoicedLow*2);   // amount to move over
               // BUGFIX - Changed dwRealWindowSize to i

            // if between EERRSTART adn CANTTELL then interpolate
            if (i < FFTCANTTELL)
               fAmt = fAmt * (fp)(i - FFTERRSTART) / (fp)(FFTCANTTELL - FFTERRSTART);
            // else, keep whole thing

            pafFFTSeperate[i] += fAmt;
            pafFFTSeperate[dwRealWindowSize+i] -= fAmt;
         }
      } // i

      // conversely, if mostly noise then move from voiced to noise
   } // if noise


   // figure out what one unit of frequency counts for
   fp fSrcUnit, fDstUnit;  // how many Hz each is
   fp fDstToSrc;  // multiply by destination units to get source units
   fp f, f1, f2;
   DWORD dwIndex;
   fDstUnit = (fp)m_dwSamplesPerSec / 2.0 / (fp) dwWindowSize;
   //fSrcUnit = (fp)m_dwSamplesPerSec / 2.0 / (fp) dwRealWindowSize * ((fp) iPitch / (fp) dwRealWindowSize);
   fSrcUnit = (fp)m_dwSamplesPerSec / (fp) dwRealWindowSize / ((fp) iPitch / (fp) dwRealWindowSize);
   fDstToSrc = fDstUnit / fSrcUnit;
   //fDstToSrc = 1.0 / fDstToSrc;

   DWORD dwNoise;
   for (dwNoise = 0; dwNoise < (DWORD) (pafFFTNoise ? 2 : 1); dwNoise++) {
      float *pafFFTFrom, *pafFFTTo;
      if (pafFFTNoise) {
         pafFFTFrom = pafFFTSeperate + (dwNoise ? 0 : dwRealWindowSize);
         pafFFTTo = dwNoise ? pafFFTNoise : pafFFTVoiced;
      }
      else {
         pafFFTFrom = pafFFT2;
         pafFFTTo = pafFFTVoiced;
      }

      // fill in what want
      for (i = 0; i < (int)dwWindowSize; i++) {
         if (fExponent) {
            f = fZeroOctave * pow((fp)2.0, (fp)((fp)i / (fp)dwDivsPerOctave));
            f /= fSrcUnit;
         }
         else
            f = (fp) i * fDstToSrc;
         dwIndex = (DWORD) f;
         f -= dwIndex;

         f1 = (dwIndex < dwRealWindowSize) ? pafFFTFrom[dwIndex] : 0;
         f2 = (dwIndex+1 < dwRealWindowSize) ? pafFFTFrom[dwIndex+1] : 0;

         pafFFTTo[i] = (1.0 - f) * f1 + f * f2;
      }

      if (dwBlur >= 2) {
         // move energies up
         // NOTE: intentionally copy to pafFFT2
         memcpy (pafFFT2, pafFFTTo, dwWindowSize * sizeof(float));

         // calculate blur
         float fSum;
         DWORD j;
         DWORD dwCount;
         for (i = 0; i < (int)dwWindowSize; i++) {
            fSum = 0;
            dwCount = 0;
            for (j = 0; j < dwBlur; j++) {
               iSamp = (int)i + (int)j - (int)dwBlur/2;
               if ((iSamp < 0) || (iSamp >= (int) dwWindowSize))
                  continue;
               fSum += pafFFT2[iSamp];
               dwCount++;
            } // j
            fSum /= (float) dwCount;
            pafFFTTo[i] = fSum;
         } // i
      }
   }  // dwNoise

   // done
}



/****************************************************************************
CM3DWave::BlankSRFeatures - Fill in with blank SR features. Used when lookin
at SR training data
*/
BOOL CM3DWave::BlankSRFeatures (void)
{
   m_dwSRSkip = m_dwSamplesPerSec / m_dwSRSAMPLESPERSEC;  // 1/100th of a second
   m_dwSRSamples = m_dwSamples / m_dwSRSkip + 1;

   // how much need?
   DWORD dwNeed;
   dwNeed = m_dwSRSamples * sizeof (SRFEATURE);
   // old codedwNeed = m_dwSRSamples * (SRDATAPOINTS + 1) * sizeof(char);
   if (!m_pmemSR)
      m_pmemSR = new CMem;
   if (!m_pmemSR || !m_pmemSR->Required (dwNeed)) {
      m_dwSRSamples = 0;
      return FALSE;
   }
   m_paSRFeature = (PSRFEATURE) m_pmemSR->p;
   memset (m_paSRFeature, 0, dwNeed);

   return TRUE;
}


// FSSTACK - Feature stack
typedef struct {
   float          afVoiced[SRDATAPOINTS];    // voiced data
   float          afUnvoiced[SRDATAPOINTS];  // unvoiced
   float          afPhase[SRPHASENUM];       // phase angle
   float          afPhaseSin[SRPHASENUM];    // sine of phase, times voiced energy
   float          afPhaseCos[SRPHASENUM];    // cos of phase, times voiced energy
   fp             fPitch;     // pitch, in Hz
   fp             fEnergy;    // energy including voiced and unvoiced
   float          afEnergy[SROCTAVE+1];   // energy per octave
   float          afUnvoicedPercent[SROCTAVE+1];   // percentage of the energy that's voiced
} FSSTACK, *PFSSTACK;

/****************************************************************************
CM3DWave::CalcSRFeaturesStack - Pushes another value onto the stack,
potentially causing some info to be written.

inputs
   fp                   fPitch - Pitch in Hz at this point
   float                *pafFFTVoiced - Voiced. SRDATAPOINTS. Can be NULL
   float                *pafFFTUnvoiced - Unvoiced. SRDATAPOINTS. Can be NULL.
   float                *pafFFTPhase - Phase. SRPHASENUM
   PCMem                pMemStack - Temporary stack memory.
   int                  *piStack - Initially filled with current stack location (intiialize
                                    to 0), and then will be incremeented each time
   PCSinLUT             pSinLUT - used for FFT
   PCMem                pMemFFTScratch - Used for FFT
returns
   none
*/

#define MIDSRFSTACK        (SRSAMPLESPERSEC / 32)
#define SRFSTACKELEMS      (MIDSRFSTACK*2+2)     // elements

void CM3DWave::CalcSRFeaturesStack (fp fPitch, float *pafFFTVoiced, float *pafFFTUnvoiced, float *pafFFTPhase,
                                    PCMem pMemStack, int *piStack, PCSinLUT pSinLUT, PCMem pMemFFTScratch)
{
   // if first time then add some bits
   int iStack = *piStack;
   if (!iStack) {
      if (!pMemStack->Required(sizeof(FSSTACK)*SRFSTACKELEMS))
         return;  // error. not likely
      pMemStack->m_dwCurPosn = sizeof(FSSTACK) * (SRFSTACKELEMS-1);
      memset (pMemStack->p, 0, pMemStack->m_dwCurPosn); // so nothing
   }

   // if have enough data then peel off the stack
   // float *paf, *pafUse;
   DWORD dwNoise, j;
   float fVal;
   FSSTACK fsCleaned;
   PFSSTACK pfs, pfsPrior, pfsNext;
   if (pMemStack->m_dwCurPosn >= sizeof(FSSTACK)*SRFSTACKELEMS) {
      int iWriteTo = iStack - MIDSRFSTACK - 2;
      if ((iWriteTo >= 0) && (iWriteTo < (int)m_dwSRSamples) ) {
         // copy the central one to the cleaned bin
         pfs = (PFSSTACK)pMemStack->p + MIDSRFSTACK;
         pfsPrior = pfs-1;
         pfsNext = pfs+1;
         memcpy (&fsCleaned, pfs, sizeof(fsCleaned));

         // do cleanup of signal

         // BUGFIX - Put removenoisySRFEATURE before adjust for attack
         // so that t's at start of word won't be turned into voiced

         // remove single noisy units
         RemoveNoisySRFEATURE (
            pfs->afVoiced, pfs->afUnvoiced, pfs->afEnergy, pfs->afUnvoicedPercent,
            pfsPrior->afVoiced, pfsPrior->afUnvoiced, pfsPrior->afEnergy, pfsPrior->afUnvoicedPercent,
            pfsNext->afVoiced, pfsNext->afUnvoiced, pfsNext->afEnergy, pfsNext->afUnvoicedPercent,
            fsCleaned.afVoiced, fsCleaned.afUnvoiced);


         // find out maximum change in amplitude, and if there's enough then
         // call the FFT calcs
         fp fDeltaMin = 1.0;
         fp fMaxComp = max(pfs->fEnergy, pfsPrior->fEnergy);
         fp fMinComp = min(pfs->fEnergy, pfsPrior->fEnergy);
         if (fMaxComp > CLOSE)
            fDeltaMin = min(fDeltaMin, fMinComp / fMaxComp);
         fMaxComp = max(pfs->fEnergy, pfsNext->fEnergy);
         fMinComp = min(pfs->fEnergy, pfsNext->fEnergy);
         if (fMaxComp > CLOSE)
            fDeltaMin = min(fDeltaMin, fMinComp / fMaxComp);

#define MAXATTACKTHRESH       0.80      // if this much of an energy change
#define MINATTACKTHRESH       0.25      // when at this point, treat as 100% attack
            // BUGFIX - Changed from .9 and .8, .5 to .25, to minimize effects with voiced sections, tradeoff with plosives
         if (fDeltaMin < MAXATTACKTHRESH) {
            fp fAmt = 1.0 - (fDeltaMin - MINATTACKTHRESH) / (MAXATTACKTHRESH - MINATTACKTHRESH);
            fAmt = min(fAmt, 1.0);
            fAmt = max(fAmt, 0.0);
            RescaleSRDATAPOINTBySmallFFT (iWriteTo, fsCleaned.afVoiced, fsCleaned.afUnvoiced,
               fAmt,
               pSinLUT, pMemFFTScratch);
         }


         // tighten formants
         NarrowerFormants (fsCleaned.afVoiced, fsCleaned.afUnvoiced);

         // transfer the signal over, index of 1
         PSRFEATURE psrf = &m_paSRFeature[iWriteTo];

         // convert to DB
         for (dwNoise = 0; dwNoise < 2; dwNoise++) {
            float *pafUse = dwNoise ? &fsCleaned.afUnvoiced[0] : &fsCleaned.afVoiced[0];
            for (j = 0; j < SRDATAPOINTS; j++) {
               fVal = pafUse[j];

               // convert to dB
               fVal = max(0.0001, fVal);  // BUGFIX - Was .01
               fVal = log10 (fVal / (fp)0x8000) * 20.0;

               // make sure ranges between 127 and -127
               fVal = min(fVal, 127);

               // make sure not any less than 50 db down
               fVal = max(fVal, SRABSOLUTESILENCE);  // max 96 db down
                  // BUGFIX - Was max -96. Changed to -110

               // write out
               if (dwNoise)
                  psrf->acNoiseEnergy[j] = (char)fVal;
               else
                  psrf->acVoiceEnergy[j] = (char)fVal;
            } // j
         }  // dwNoise

         // determine the phase
         for (j = 0; j < SRPHASENUM; j++) {
            // phase width is larger at low harmonics
            DWORD dwPhaseWidth = (SRPHASENUM-j) * SRSAMPLESPERSEC / 32 * m_dwSRSkip / (m_dwSamplesPerSec / SRSAMPLESPERSEC) / SRPHASENUM;
            dwPhaseWidth = (DWORD) floor(
               (fp)dwPhaseWidth
               / max(fsCleaned.fPitch, 1.0) *
               100.0 + 0.5);
                  // BUGFIX - Phase width is a function of fundamental pitch
            if (!dwPhaseWidth)
               continue;   // since wont make a difference
            dwPhaseWidth = min(dwPhaseWidth, MIDSRFSTACK);

            // loop
            int iOffset;
            fp fSumCos = 0, fSumSin = 0;
            for (iOffset = -(int)dwPhaseWidth; iOffset <= (int)dwPhaseWidth; iOffset++) {
               fp fWeight = dwPhaseWidth + 1 - (DWORD)abs(iOffset);
               fSumSin += pfs[iOffset].afPhaseSin[j] * fWeight;
               fSumCos += pfs[iOffset].afPhaseCos[j] * fWeight;
            } // iOffset

            // atan2 - so reconstruct
            fsCleaned.afPhase[j] = atan2(fSumSin, fSumCos);
         } // j

         // phase to bytes
         for (j = 0; j < SRPHASENUM; j++)
            psrf->abPhase[j] = (BYTE)myfmod(fsCleaned.afPhase[j] / 2.0 / PI * 256.0, 256.0);
            // BUGFIX - Was 255. Should be 256
      } // if iStack >= xxx

      // move down
      memmove (pMemStack->p, (PBYTE)pMemStack->p + sizeof(FSSTACK), sizeof(FSSTACK) * (SRFSTACKELEMS-1));
      pMemStack->m_dwCurPosn = sizeof(FSSTACK)*(SRFSTACKELEMS-1);
   }

   // append
   pfs = (PFSSTACK)pMemStack->p + pMemStack->m_dwCurPosn / sizeof(FSSTACK);
   memset (pfs, 0, sizeof(*pfs));
   if (pafFFTVoiced)
      memcpy (pfs->afVoiced, pafFFTVoiced, SRDATAPOINTS * sizeof(float));
   if (pafFFTUnvoiced)
      memcpy (pfs->afUnvoiced, pafFFTUnvoiced, SRDATAPOINTS * sizeof(float));
   if (pafFFTPhase)
      memcpy (pfs->afPhase, pafFFTPhase, SRPHASENUM * sizeof(float));
   pfs->fPitch = fPitch;
   pfs->fEnergy = 0;
   DWORD i;
   for (i = 0; i < SRDATAPOINTS; i++)
      pfs->fEnergy += pfs->afVoiced[i] + pfs->afUnvoiced[i];
   float afVoiced[SROCTAVE+1], afUnvoiced[SROCTAVE+1];
   EnergyCalcPerOctave (pfs->afVoiced, NULL, afVoiced);
   EnergyCalcPerOctave (pfs->afUnvoiced, NULL, afUnvoiced);
   for (i = 0; i < SROCTAVE+1; i++) {
      pfs->afEnergy[i] = afVoiced[i] + afUnvoiced[i];
      if (pfs->afEnergy[i])
         pfs->afUnvoicedPercent[i] = afUnvoiced[i] / pfs->afEnergy[i];
      else
         pfs->afUnvoicedPercent[i] = 0;   // 0, since preferring voiced
   } // i

   // determine the sin and cos of the phase, scaled by the amount of voiced
   for (i = 0; i < SRPHASENUM; i++) {
      fp fFreq = (fp)(i+1) * fPitch;
      fFreq = max(fFreq, 1) / (fp)SRBASEPITCH;
      fFreq = log(fFreq) / log(2.0) * (fp)SRPOINTSPEROCTAVE;
      int iFreq = floor(fFreq + 0.5);
      iFreq = max(iFreq, 0);
      iFreq = min(iFreq, SRDATAPOINTS-1);

      fp fEnergy = pfs->afVoiced[iFreq] + pfs->afUnvoiced[iFreq]/ 10.0;
            // some noise contribution

      pfs->afPhaseSin[i] = sin(pfs->afPhase[i]) * fEnergy;
      pfs->afPhaseCos[i] = cos(pfs->afPhase[i]) * fEnergy;
   } // i

   pMemStack->m_dwCurPosn += sizeof(FSSTACK);

   // icrease staack
   *piStack = *piStack + 1;
}


/****************************************************************************
PhaseRotateSoZeroFundamental - Rotate all the phase angles so
the fundamental is at 0.

inputs
   float       *pafPhase - Array of SRPHASENUM phases
returns
   none
*/
void PhaseRotateSoZeroFundamental (float *pafPhase)
{
   // adjust all phases so they're relative to the fundemental
   DWORD i;
   for (i = 1; i < SRPHASENUM; i++) {
      pafPhase[i] -= pafPhase[0] * (fp)(i+1);  // subtract phase of fundamental
      pafPhase[i] = myfmod(pafPhase[i], 2.0 * PI);
   }
   pafPhase[0] = 0;  // set phase fundamental to 0
}

/****************************************************************************
SRFPHASEEXTRACTError - Calculate the error between two SRFPHASEEXTRACT

inputs
   PSRFPHASEEXTRACT        pA - A. Use the energies from this one
   PSRFPHASEEXTRACT        pB - Use only the phase from this
   int                     iWhat - 0 compares all, -1 compared only fundamental,
                           1 compares everything except fundamental
returns
   fp - Difference
*/
fp SRFPHASEEXTRACTError (PSRFPHASEEXTRACT pA, PSRFPHASEEXTRACT pB, int iWhat)
{
   DWORD dwStart = (iWhat <= 0) ? 0 : 1;
   DWORD dwEnd = (iWhat >= 0) ? SRFFPHASEEXTRACT_NUM : 1;
   DWORD i;

   fp fTotal = 0;
   for (i = dwStart; i < dwEnd; i++) {
      fp fErr = fabs(pA->afPhase[i] - pB->afPhase[i]);
      fErr = myfmod (fErr, 2.0 * PI);
      if (fErr > PI)
         fErr = 2.0 * PI - fErr;

      fTotal += fErr * pA->afEnergy[i] / (fp)(i+1);
            // use divide, since lower phases more important to the sound
   } // i

   return fTotal;
}


/****************************************************************************
SRFPHASEEXTRACTRotate - Rotates all the phases by the given phase angle.

inputs
   PSRFPHASEEXTRACT        pSRFPhase - To rotate
   fp                      fAngle - 0 to 2PI, or negative
returns
   none
*/
void SRFPHASEEXTRACTRotate (PSRFPHASEEXTRACT pSRFPhase, fp fAngle)
{
   DWORD i;
   for (i = 0; i < SRFFPHASEEXTRACT_NUM; i++) {
      pSRFPhase->afPhase[i] += fAngle * (fp)(i+1);
      pSRFPhase->afPhase[i] = myfmod(pSRFPhase->afPhase[i], 2.0 * PI);
   }
}



/****************************************************************************
SRFPHASEEXTRACTAdjustFundamental - Adjusts the fundamental phase since
there may be an error in calculating it.

inputs
   PSRFPHASEEXTRACT        pPrev - Previous phase, and location of the phase.
   PSRFPHASEEXTRACT        pCur - Current phase. The fundamental is modified in place
returns
   none
*/
void SRFPHASEEXTRACTAdjustFundamental (PSRFPHASEEXTRACT pPrev, PSRFPHASEEXTRACT pCur)
{
   // must have valid wavelengths
   if (!pPrev->fWavelength || !pCur->fWavelength)
      return;

   // predict what should be
   SRFPHASEEXTRACT SRFPredict;
   SRFPredict = *pPrev;
   fp fDist = (pCur->fCenter - pPrev->fCenter);
   fDist /= (pCur->fWavelength + pPrev->fWavelength) / 2.0;
   fDist *= 2.0 * PI;   // amount to rotate
   SRFPHASEEXTRACTRotate (&SRFPredict, fDist);

   // since prediction will never match what actually observed,
   // calculate several predictions
#define PREDICTHYPNUM      6
   SRFPHASEEXTRACT aSRFPredictHyp[PREDICTHYPNUM];
   fp afPredictHypError[PREDICTHYPNUM];
   fp afPredictHypErrorCur[PREDICTHYPNUM];
   DWORD i, j;
   DWORD dwCur;
   fp fAngleRot;
   for (i = dwCur = 0; dwCur < PREDICTHYPNUM; i++) {
      // alternate angles
      for (j = 0; (j <= i) && (dwCur < PREDICTHYPNUM); j++, dwCur++) {
         aSRFPredictHyp[dwCur] = SRFPredict;
         memcpy (aSRFPredictHyp[dwCur].afEnergy, pCur->afEnergy, sizeof(pCur->afEnergy));   // use energy from the current one

         // so can rotate the predicted angle to match the current angle
         fAngleRot = pCur->afPhase[i] - aSRFPredictHyp[dwCur].afPhase[i];
         fAngleRot = myfmod (fAngleRot + PI, 2.0 * PI) -PI;
         fAngleRot /= (fp)(i+1);
         fAngleRot += (fp)j / (fp)(i+1) * 2.0 * PI;   // since several copies of each

         SRFPHASEEXTRACTRotate (&aSRFPredictHyp[dwCur], fAngleRot);
         
         // determine an error between the predicted and the hypothesized predicted
         afPredictHypError[dwCur] = SRFPHASEEXTRACTError (&aSRFPredictHyp[dwCur], &SRFPredict, 0);

         // also, determine the error between the hypothesis and the current
         // phase, excluding the fundamental, which is suspect
         afPredictHypErrorCur[dwCur] = SRFPHASEEXTRACTError (&aSRFPredictHyp[dwCur], pCur, 1);
      } // j
   } // i

   // loop over a number of possible phase angles for the fundamental
   // and see which has the lowest overall error
   fp fError, fErrorOrig;
#define FUNDAMENTALHYPNUM        32
   SRFPHASEEXTRACT SRFFundamentalHyp = *pCur;
   fp fErrorBest = 0;
   DWORD dwBest = (DWORD)-1;
   for (i = 0; i < FUNDAMENTALHYPNUM; i++) {
      SRFFundamentalHyp.afPhase[0] = pCur->afPhase[0] + (fp)i / (fp) FUNDAMENTALHYPNUM * 2.0 * PI;

      // whats the error for this
      fErrorOrig = SRFPHASEEXTRACTError (&SRFFundamentalHyp, pCur, -1);
         // only compare fundamental, since its the only thing that has changed

      // loop over all prection hypothesis
      for (j = 0; j < PREDICTHYPNUM; j++) {
         // start error out based on comparing fundamental hyp with what did before
         fError = SRFPHASEEXTRACTError (&aSRFPredictHyp[j], &SRFFundamentalHyp, -1) + afPredictHypErrorCur[j];

         // add the error between the fundamtenal hypothesis and the measured
         fError += fErrorOrig;

         // if this is the best then keep it
         if ((dwBest == (DWORD)-1) || (fError < fErrorBest)) {
            fErrorBest = fError;
            dwBest = i; // only care about the fundamental
         }
      } // j
   } // i


   // write out the best andle for the fundamental
   pCur->afPhase[0] = myfmod(pCur->afPhase[0] + (fp)dwBest / (fp) FUNDAMENTALHYPNUM * 2.0 * PI, 2.0 * PI);

#if 0
   WCHAR szTemp[64];
   swprintf (szTemp, L"\r\nBest fund phase rotate = %d", (int)dwBest);
   OutputDebugStringW (szTemp);
#endif
}


#if 0 // DEAD code, rewritten beloe
/****************************************************************************
CM3DWave::CalcSRFeatures - Calculates the SR features.

inputs
   BOOL                 fFastSR - Set to TRUE if want fast SR calculation. FALSE for slow but accurate.
                        In fast SR calculation, only the every other even one will be calculated.
   PCProgressSocket     pProgress - Progress bar
returns
   BOOL - TRUE if successful
*/
BOOL CM3DWave::CalcSRFeatures (BOOL fFastSR, PCProgressSocket pProgress)
{
   if (!BlankSRFeatures())
      return FALSE;

   // if fastSR then double skip and reduce samples
   DWORD dwFullSamples = m_dwSRSamples;
   if (fFastSR) {
      m_dwSRSkip *= 2;
      m_dwSRSamples = m_dwSamples / m_dwSRSkip + 1;
   }

   if (!m_dwSamples)
      return FALSE;

   // see if need pitch
   BOOL fNeedPitch;
   fNeedPitch = (m_adwPitchSamples[PITCH_F0] ? FALSE : TRUE);
   if (fNeedPitch) {
      if (pProgress)
         pProgress->Push (0, .5);
      CalcPitch (dwCalcFor, pProgress);
         // BUGFIX - Dont use fast calcpitch because not accurate enough
      if (pProgress) {
         pProgress->Pop();
         pProgress->Push (.5, 1);
      }
   }

   CMem memOldPitch;
   DWORD dwOldPitchSkip;
   DWORD dwOldPitchSamples;
   DWORD i, j;
   if (m_dwSRSkip != m_adwPitchSkip[PITCH_F0]) {
      // know that we're skipping every other sample for sr features, but still need
      // a full pitch cyle for max detail
      
      // copy over the old pitch
      DWORD dwNeed = m_adwPitchSamples[PITCH_F0] * m_dwChannels * sizeof(WVPITCH);
      if (!memOldPitch.Required (dwNeed))
         return FALSE;
      memcpy (memOldPitch.p, m_apPitch[PITCH_F0], dwNeed);
      memOldPitch.m_dwCurPosn = dwNeed;
      dwOldPitchSkip = m_adwPitchSkip[PITCH_F0];
      dwOldPitchSamples = m_adwPitchSamples[PITCH_F0];

      // produce new pitch points
      CMem memTemp;
      dwNeed = m_dwSRSamples * m_dwChannels * sizeof(WVPITCH);
      if (!memTemp.Required (dwNeed))
         return FALSE;
      PWVPITCH pwp = (PWVPITCH)memTemp.p;
      for (i = 0; i < m_dwSRSamples; i++) for (j = 0; j < m_dwChannels; j++, pwp++) {
         pwp->fFreq = PitchAtSample (i * m_dwSRSkip, j);
         pwp->fStrength = 1;
      }

      // copy over
      if (!m_amemPitch[PITCH_F0].Required (dwNeed))
         return FALSE;
      m_apPitch[PITCH_F0] = (PWVPITCH) m_amemPitch[PITCH_F0].p;
      m_adwPitchSkip[PITCH_F0] = m_dwSRSkip;
      m_adwPitchSamples[PITCH_F0] = m_dwSRSamples;
   }

   // allocate sratch for float results
   CMem memFFT;
   float *pafFFTVoiced, *pafFFTNoise, *pafFFTPhase;
   if (!memFFT.Required ((SRDATAPOINTS * sizeof(float) * 2 + SRPHASENUM *sizeof(float)) * 3))
      return FALSE;
   pafFFTVoiced = (float*) memFFT.p;
   pafFFTNoise = pafFFTVoiced + SRDATAPOINTS * 3;
   pafFFTPhase = pafFFTNoise + SRDATAPOINTS * 3;
   memset (pafFFTVoiced, 0, memFFT.m_dwAllocated); // zero it all out since assume 0's
   float afFinalEnergy[2][SRDATAPOINTS];

   int iStack = 0; // startinc stack
   CMem  memStack; // for stack

   // attempted improvements for phase
   SRFPHASEEXTRACT aSRFPhase[3];
   memset (aSRFPhase, 0, sizeof(aSRFPhase));
#if 0
   fp afPhaseIIR[2][SRPHASENUM];
   memset (afPhaseIIR, 0, sizeof(afPhaseIIR));
#endif // 0

   CSinLUT SinLUT, SinLUT2;
   CMem memFFTScratch;
   CListFixed lfScratch;
   CListVariable lvScratch;
   // loop
   DWORD dwNoise;
   fp fVal;
   for (i = 0; i < m_dwSRSamples; i++) {
      if (((i%100) == 0) && pProgress)
         pProgress->Update ((fp)i / (fp)m_dwSRSamples);

      // this code does blurring in time domain so get smoother signal
      if (!i) {
         // get three windows...
         // first window is 0 because negative in wave

         // second window is point 0
         SuperMagicFFT (i, pafFFTVoiced+1*SRDATAPOINTS, pafFFTNoise+1*SRDATAPOINTS,
            pafFFTPhase+1*SRPHASENUM, NULL, &SinLUT, &memFFTScratch, &lfScratch, &lvScratch,
            &aSRFPhase[1]);
         //StretchFFT (SRDATAPOINTS, i * m_dwSRSkip, 0,
         //   pafFFTVoiced + 1 * SRDATAPOINTS,
         //   pafFFTNoise + 1 * SRDATAPOINTS,
         //   pafFFTPhase + 1 * SRPHASENUM,
         //   SRBASEPITCH, SRPOINTSPEROCTAVE,
         //   0); // BUGFIX - Remove blurring because doing it automatically SRPOINTSPEROCTAVE/2);
      }
      else {
         // move windows back
         memmove (pafFFTVoiced, pafFFTVoiced + SRDATAPOINTS, SRDATAPOINTS * sizeof(float)*2);
         memmove (pafFFTNoise, pafFFTNoise + SRDATAPOINTS, SRDATAPOINTS * sizeof(float)*2);
         memmove (pafFFTPhase, pafFFTPhase + SRPHASENUM, SRPHASENUM * sizeof(float)*2);
         memmove (&aSRFPhase[0], &aSRFPhase[1], sizeof(aSRFPhase[0])*2);

         // fill in
      }

      // fill in third window
      // NOTE: always using channel 0
      SuperMagicFFT (i+1, pafFFTVoiced + 2 * SRDATAPOINTS, pafFFTNoise + 2 * SRDATAPOINTS,
         pafFFTPhase + 2 * SRPHASENUM, pafFFTPhase + SRPHASENUM, &SinLUT, &memFFTScratch, &lfScratch, &lvScratch,
         &aSRFPhase[2]);
      //StretchFFT (SRDATAPOINTS, (i+1) * m_dwSRSkip, 0,
      //   pafFFTVoiced + 2 * SRDATAPOINTS,
      //   pafFFTNoise + 2 * SRDATAPOINTS,
      //   pafFFTPhase + 2 * SRPHASENUM,
      //   SRBASEPITCH, SRPOINTSPEROCTAVE,
      //   0); // BUGFIX - Remove blurring because doing it automatically SRPOINTSPEROCTAVE/2);

      // figure out the highest noise
      DWORD k;
      fp fNoiseZero = 0;
      fp fNoiseZeroUse;
      for (k = 0; k < 3; k++) for (j = 0; j < SRDATAPOINTS; j++) {
         fNoiseZero = max(fNoiseZero, pafFFTNoise[k*SRDATAPOINTS+j]);
         fNoiseZero = max(fNoiseZero, pafFFTVoiced[k*SRDATAPOINTS+j]);
      }
      fNoiseZero /= 150; // so anything this far below the peak is removed
         // BUGFIX - Was 300, but changed to 150 since seemed to work better with blizzard2007 voice
      fNoiseZero = max(fNoiseZero, 25);  // always at least a certain amount
         // BUGFIX - Up fNoiseZero from 25 to 50, and /300 to /100
         // BUGFIX - Changed from 50 back to 25 because of blizzard2007

      for (dwNoise = 0; dwNoise < 2; dwNoise++) {
         fNoiseZeroUse = fNoiseZero * (dwNoise ? 2 : 1);
         // fNoiseZeroUse = 0; // BUGFIX - Had in, but when changed to latest technique
               // of supersampling no longer need
            // BUGFIX - Disabled fNoiseZeroUse=0 since improved quality for blizzard 2007 voice
         float *pafFFTUse = (dwNoise ? pafFFTNoise : pafFFTVoiced);
         for (j = 0; j < SRDATAPOINTS; j++) {
            // average three adjacent values so smoother signal
            // BUGFIX - Do triangle filter
#if 0 // BUGFIX - Dont average over time. Dont need when changed to supersampling technique
            if (dwNoise) {
               fVal = 0;
               for (k = 0; k < 3; k++)
                  fVal += (pafFFTUse[k*SRDATAPOINTS+j]);// *((k == 1) ? 2 : 1);
               fVal /= 3.0; // 4.0;   // average out
            }
            else {
               fVal = 0;
               for (k = 0; k < 3; k++)
                  fVal += (pafFFTUse[k*SRDATAPOINTS+j])*((k == 1) ? 2 : 1);
               fVal /= 4.0;   // average out
               //fVal = pafFFTUse[1*SRDATAPOINTS+j];  // to test w/out blurring
            }
#else
            fVal = pafFFTUse[1*SRDATAPOINTS+j];
#endif // 0

            // BUGFIX - Increase the "volume" in the higher frequencies because higher
            // frequencies represent a larger slice of the FFT, and if dont do
            // this actually misrepresenting the energy
            // old code after logfVal += ((fp)j - (fp)SRDATAPOINTS/2) / SRPOINTSPEROCTAVE * 6.0;
               // 6.0 = 6 db per octave, since 6db = 2x volume
            // BUGFIX - Moved this before do conversion to dB because it's
            // easier to do noise reduction when in linear
            fp fScaleVal = pow (2, ((fp)j - (fp)SRDATAPOINTS/2) / SRPOINTSPEROCTAVE);
            fVal *= fScaleVal;

            // BUGFIX - Reduce noise
            if (fVal < fNoiseZeroUse)
               fVal = 0;
            else if (fVal < 2*fNoiseZeroUse) {
               fVal = (fVal - fNoiseZeroUse) * 2;   // so get smooth ramp
            }

#if 0
            // BUGFIX - use fNoiseZeroUsex4 to get rid of spuroious noise bits
            // if this is spurrious noise, isolated, then set to 0
            // BUGFIX - Put in fScaleVal since wasn't in, causing high freq to be cut out
            if ((pafFFTUse[0*SRDATAPOINTS+j] < 2*fNoiseZeroUse / fScaleVal) &&
               (pafFFTUse[1*SRDATAPOINTS+j] < 4*fNoiseZeroUse / fScaleVal) &&
               (pafFFTUse[2*SRDATAPOINTS+j] < 2*fNoiseZeroUse / fScaleVal))
               fVal = 0;
#endif // 0
            // store away
            afFinalEnergy[dwNoise][j] = fVal;
         } // j
      } // dwNoise

      // BUGFIX - fix the fundamental phase in case is not extracted properly,
      // as in the case of blizzard 2007 voice. Doesn't seem to help much
      _ASSERTE (pafFFTPhase[SRPHASENUM+0] == aSRFPhase[1].afPhase[0]);
      SRFPHASEEXTRACTAdjustFundamental (&aSRFPhase[0], &aSRFPhase[1]);
      pafFFTPhase[SRPHASENUM+0] = aSRFPhase[1].afPhase[0];  // set new fundamental phase

      // by this point will need to zero out phase to fundamental
      float *pafPhaseCur = pafFFTPhase + SRPHASENUM;
      PhaseRotateSoZeroFundamental (pafPhaseCur);

#if 0
      // do a phase IIR to minimize phase changes
#define IIRFILTERWEIGHT          0.25
      for (j = 0; j < SRPHASENUM; j++) {
         fp fWeight = IIRFILTERWEIGHT; // * (fp)(double)pow((double)j, 0.25);
         afPhaseIIR[0][j] += sin(pafPhaseCur[j]) * fWeight;
         afPhaseIIR[1][j] += cos(pafPhaseCur[j]) * fWeight;

         pafPhaseCur[j] = atan2(afPhaseIIR[0][j], afPhaseIIR[1][j]);

         // renormalize contents of afPhaseIIR. Easiest way is
         // to call sin and cos again
         afPhaseIIR[0][j] = sin(pafPhaseCur[j]);
         afPhaseIIR[1][j] = cos(pafPhaseCur[j]);
      } // j
#endif // 0

      // add this to the stack
      fp fPitch = PitchAtSample (i * m_dwSRSkip, 0);
      CalcSRFeaturesStack (fPitch, afFinalEnergy[0], afFinalEnergy[1], pafPhaseCur, &memStack, &iStack,
         &SinLUT2, &memFFTScratch);

#if 0 // no longer used because in stack
      // convert to DB
      for (dwNoise = 0; dwNoise < 2; dwNoise++) {
         for (j = 0; j < SRDATAPOINTS; j++) {
            fVal = afFinalEnergy[dwNoise][j];

            // convert to dB
            fVal = max(0.0001, fVal);  // BUGFIX - Was .01
            fVal = log10 (fVal / (fp)0x8000) * 20.0;

            // make sure ranges between 127 and -127
            fVal = min(fVal, 127);

            // make sure not any less than 50 db down
            fVal = max(fVal, SRABSOLUTESILENCE);  // max 96 db down
               // BUGFIX - Was max -96. Changed to -110

            // write out
            if (dwNoise)
               m_paSRFeature[i].acNoiseEnergy[j] = (char)fVal;
            else
               m_paSRFeature[i].acVoiceEnergy[j] = (char)fVal;
         } // j
      }  // dwNoise

      // phase
      for (j = 0; j < SRPHASENUM; j++)
         m_paSRFeature[i].abPhase[j] = (BYTE)(pafFFTPhase[SRPHASENUM+j] / 2.0 / PI * 256.0);
         // BUGFIX - Was 255. Should be 256

#endif // 0
   } // i


   // add some silences to the stack to fill out the data
   for (i = 0; i < SRFSTACKELEMS-1; i++)
      CalcSRFeaturesStack (0, NULL, NULL, NULL, &memStack, &iStack,
         &SinLUT2, &memFFTScratch);

   // BUGFIX - go over phases and average them
#define STOPBLENDINGPHASE     (SRPHASENUM/4)
   CMem memPhase;
   DWORD dwPhaseSize = sizeof(m_paSRFeature[0].abPhase);
   if (memPhase.Required (m_dwSRSamples * dwPhaseSize)) {
      PBYTE pabPhase = (PBYTE)memPhase.p;
      fp fAngle;
      // copy over
      for (i = 0; i < m_dwSRSamples; i++)
         memcpy (pabPhase + (i * dwPhaseSize), m_paSRFeature[i].abPhase, dwPhaseSize);

      for (i = 0; i < m_dwSRSamples; i++) for (j = 0; j < SRPHASENUM; j++) {
         // average
         int iTent;
         int iMax;
         if (j < STOPBLENDINGPHASE) {
            iMax = (STOPBLENDINGPHASE - j) * (int)m_dwSRSAMPLESPERSEC / 32 / STOPBLENDINGPHASE;
                  // 1/32th of a second either side
            iMax = max(iMax, 1);
         }
         else {
            m_paSRFeature[i].abPhase[j] = pabPhase[i*dwPhaseSize + j];
            continue;
         }
         fp fX = 0, fY = 0;
         for (iTent = -iMax; iTent <= (int)iMax; iTent++) {
            int iCur = (int) i + iTent;
            if ((iCur < 0) || (iCur >= (int)m_dwSRSamples))
               continue; // out of range

            fAngle = (fp)pabPhase[(DWORD)iCur*dwPhaseSize + j] / 256.0 * 2.0 * PI;
            fX += sin(fAngle) * (fp)(iMax + 1 - abs(iTent));
            fY += cos(fAngle) * (fp)(iMax + 1 - abs(iTent));
         } // iTent

         fAngle = atan2(fX, fY);
         fAngle = myfmod(fAngle, 2.0 * PI);
         m_paSRFeature[i].abPhase[j] = (BYTE)(fAngle / 2.0 / PI * 256.0);
      } // i, j
   } // if phase

   if (fNeedPitch && pProgress)
      pProgress->Pop();

#if 0 // BUFIX - This new code was pre supersampling, and isn't needed now
   // BUGFIX - Code to get rid of spikes that appear in the signal because of
   // bad pitch problems
   CMem memSRCopy;
   if (!memSRCopy.Required (m_dwSRSamples * sizeof(SRFEATURE)))
      return FALSE;
   PSRFEATURE pBlur = (PSRFEATURE) memSRCopy.p;

   // blur all the samples so spikes become real obvious
   for (i = 0; i < m_dwSRSamples; i++) {
      SRFEATURE srTemp;
      srTemp = m_paSRFeature[i];

      for (dwNoise = 0; dwNoise < 2; dwNoise++) {
         for (j = 0; j < SRDATAPOINTS; j++) {
            int iSum = 0;
            DWORD dwCount = 0;
            DWORD dwWeight;
            int k;
#define BLURVERT     (SRDATAPOINTS/20)
            for (k = (int)j - BLURVERT; k < (int)j + BLURVERT; k++) {
               if ((k < 0) || (k >= SRDATAPOINTS))
                  continue;
               dwWeight = ((DWORD) k < j) ? (j-(DWORD)k) : ((DWORD)k - j);
               dwWeight = BLURVERT+1-dwWeight;
               iSum += (int)(dwNoise ? srTemp.acNoiseEnergy[k] : srTemp.acVoiceEnergy[k]) *
                  (int)dwWeight;
               dwCount += dwWeight;
            }
            if (dwCount)
               iSum /= (int)dwCount;
            iSum = max(iSum, -127);
            iSum = min(iSum, 127);
            char *pcMod = dwNoise ? &pBlur[i].acNoiseEnergy[j] : &pBlur[i].acVoiceEnergy[j];
            pcMod[0] = max(pcMod[0], (char)iSum);
               // not quite the same is blurring, since keep the maxes the same
         } // j
      } // dwNoise
   } // i

   // loop through again and find any areas where it seems like have veritcal
   // spike and smooth it out
   for (i = 0; i < m_dwSRSamples; i++) {
      for (dwNoise = 0; dwNoise < 2; dwNoise++) {
         int iNoiseFloor = 50 * (dwNoise ? 1 : 2);

         for (j = 0; j < SRDATAPOINTS; j++) {
            // find the minimum given surrounding areas
#define MINWINDOW    (m_dwSRSAMPLESPERSEC/100)
            int k;
            BOOL fFound = FALSE;
            char cMin = 0;
            char cVal;
            //if (dwNoise)
            //   cMin = pBlur[i].acNoiseEnergy[j];   // dont bother with looking for spikes with noise since not a problem
            //else
            for (k = (int)i-MINWINDOW; k <= (int)i+MINWINDOW; k++) {
               if ((k < 0) || (k >= (int)m_dwSRSamples))
                  continue;
               cVal = dwNoise ? pBlur[k].acNoiseEnergy[j] : pBlur[k].acVoiceEnergy[j];
               if (fFound)
                  cMin = min(cMin, cVal);
               else {
                  cMin = cVal;
                  fFound = TRUE;
               }
            } // k

            // modify the original so it's no louder than the blurred version
            char *pcOrig = dwNoise ? &m_paSRFeature[i].acNoiseEnergy[j] : &m_paSRFeature[i].acVoiceEnergy[j];
            pcOrig[0] = min(pcOrig[0], cMin);

            // while at it, do a noise reduction
#define NOISEFLOOR      iNoiseFloor
            int iAmp = DbToAmplitude (pcOrig[0]);
            if (iAmp < 2 * NOISEFLOOR) {
               if (iAmp < NOISEFLOOR)
                  iAmp = 0;
               else if (iAmp < 2*NOISEFLOOR) {
                  iAmp = (iAmp - NOISEFLOOR) * 2;   // so get smooth ramp
               }

               fVal = max(0.0001, iAmp);  // BUGFIX - Was .01
               fVal = log10 (fVal / (fp)0x8000) * 20.0;

               // make sure ranges between 127 and -127
               fVal = min(fVal, 127);

               // make sure not any less than 50 db down
               fVal = max(fVal, SRABSOLUTESILENCE);  // max 96 db down
               pcOrig[0] = (char)(int)fVal;
            }
         } // j
      } // dwNoise
   } // i
#endif // 0


   // restore original pitch
   if (memOldPitch.m_dwCurPosn) {
      // know that m_amemPitch[PITCH_F0] will be large enough
      memcpy (m_amemPitch[PITCH_F0].p, memOldPitch.p, memOldPitch.m_dwCurPosn);
      m_adwPitchSkip[PITCH_F0] = dwOldPitchSkip;
      m_adwPitchSamples[PITCH_F0] = dwOldPitchSamples;
   }

   // if not doing fast pitch then done
   if (!fFastSR) {
      // BUGFIX - Extend the pitch
      FXSRFEATUREExtend ();

      return TRUE;
   }
   // restore pitch
   // not used anymore CalcPitchInternalB (fFastSR, pProgress);

   // else, lengthen
   m_dwSRSkip /= 2;
   DWORD dwOldSamples = m_dwSRSamples;
   m_dwSRSamples = dwFullSamples;

   // loop backwards filling in
   for (i = m_dwSRSamples-1; i < m_dwSRSamples; i--) {
      DWORD dwOld = i/2;

      // if an even number then exact match
      if (!(i%2)) {
         if (i != min(dwOld,dwOldSamples-1))
            m_paSRFeature[i] = m_paSRFeature[min(dwOld,dwOldSamples-1)];
         continue;
      }

      // else, theoretically need to interpolate, but since
      // only using this for voice compression, which skips every other one anyway,
      // just duplicate
      DWORD dwOldNext = dwOld+1;
      dwOld = min(dwOld, dwOldSamples-1);
      dwOldNext = min(dwOldNext, dwOldSamples-1);

      if (i != dwOld)
         m_paSRFeature[i] = m_paSRFeature[min(dwOld,dwOldSamples-1)];
   } // i

   // BUGFIX - Extend the pitch
   FXSRFEATUREExtend ();

   return TRUE;
}
#endif // 0


/****************************************************************************
CM3DWave::PhonemeGetRidOfShortSilence - Gets rid of very short silence phonemes.
*/
void CM3DWave::PhonemeGetRidOfShortSilence (void)
{
   DWORD i;
   PWVPHONEME pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
   PCWSTR pszSilence = MLexiconEnglishPhoneSilence();
   for (i = 0; i+1 < m_lWVPHONEME.Num(); i++) {
      if (_wcsnicmp (pp[i].awcNameLong, pszSilence, sizeof(pp[i].awcNameLong)/sizeof(WCHAR)))
         continue;   // only care about silnce

      // if too short eliminate
      if (pp[i+1].dwSample - pp[i].dwSample < m_dwSamplesPerSec / 20) {
         // get tird of
         m_lWVPHONEME.Remove (i);
         i--;  // do conunteract increase
         pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
         continue;
      }
   }
}


/****************************************************************************
CM3DWave::PhonemeAtTime - Given a sample number, fills in a string
with the phoneme name.

inputs
   DWORD          dwSample - Sample
   PWSTR          psz - Filled in with the phoneme name (if found). Must be 9 chars long
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::PhonemeAtTime (DWORD dwSample, PWSTR psz)
{
   PWVPHONEME pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
   DWORD dwNum = m_lWVPHONEME.Num();
   if (!dwNum)
      return FALSE;

   // loop until find phoneme
   DWORD i;
   for (i = 0; i < dwNum+1; i++) {
      if (i >= dwNum)
         break;   // always <= this

      if (dwSample < pp[i].dwSample)
         break;   // less than this
   }
   if (!i)
      return FALSE;  // too soon

   // else
   psz[sizeof(pp[i-1].awcNameLong)/sizeof(WCHAR)] = 0;
   memcpy (psz, pp[i-1].awcNameLong, sizeof(pp[i-1].awcNameLong));
   return TRUE;
}


/****************************************************************************
CM3DWave::PhonemeAtTime - Given a sample number, this returns information
about the phonemes to the left and right, the amount of (linear) blend,
and the mouth shape.

inputs
   DWORD          dwSample - Sample to look at
   PSTR           *ppszLeft - Filled with the left phoneme. Note: Only 4 chars valid, so may not be null terminated
   PSTR           *ppszRight - Filled with the right phoneme Note: Only 4 chars valid, so may not be null terminated
   fp             *pfAlpha - Filled with the alpha blend, 0 is left, 1 is right
   fp             *pfLateralTension - Filled with how open the are laterally, 0=rest, 1=full lateral tension,
                        -1 is puckered
   fp             *pfVerticalOpen - Filled with how much opened vertically, 0= closed, 1=fullopen
   fp             *pfTongueForward - Filled with amount that the tip of tongue is forward. 0 = touching teeth, 1 = way back in soft palette
   fp             *pfTongueUp - Filled with amount that tongue up. 1 = touch roof of mouth, .5 = bottom of top teeth, 0 is against bottom
returns
   BOOL - TRUE if succes. FALSE if fails because there aren't any phonemes
*/
BOOL CM3DWave::PhonemeAtTime (DWORD dwSample, PWSTR *ppszLeft, PWSTR *ppszRight, fp *pfAlpha,
                              fp *pfLateralTension, fp*pfVerticalOpen,
                              fp *pfTeethTop, fp* pfTeethBottom,
                              fp *pfTongueForward, fp *pfTongueUp)
{
   PWVPHONEME pp = (PWVPHONEME) m_lWVPHONEME.Get(0);
   DWORD dwNum = m_lWVPHONEME.Num();
   if (!dwNum)
      return FALSE;

   // loop until find phoneme
   DWORD i;
   DWORD dwSampStart, dwSampEnd;
   int iLeft, iRight;
   PWSTR pszLeft, pszRight;
   PCWSTR pszSilence = MLexiconEnglishPhoneSilence();
   for (i = 0; i < dwNum+1; i++) {
      iLeft = (int)i-1;
      iRight = (int)i;

      if (iLeft < 0)
         dwSampStart = 0;
      else if (iLeft < (int)dwNum)
         dwSampStart = pp[iLeft].dwSample;
      else
         dwSampStart = m_dwSamples;

      if (iRight < 0)
         dwSampEnd = 0;
      else if (iRight < (int)dwNum)
         dwSampEnd = pp[iRight].dwSample;
      else
         dwSampEnd = m_dwSamples;

      if ((dwSample >= dwSampStart) && (dwSample <= dwSampEnd))
         break;
   }
   dwSampEnd = max(dwSampEnd, dwSampStart+2);   // so no divide by zeros
   if (i >= dwNum+1) {
      // before the start/end, so mouth closed
      iLeft = -1;
      iRight = (int)dwNum+2; // good enough to cause the effect
   }

   // figure out a maximum opening and closing time
   DWORD dwMaxOpen;
   dwMaxOpen = m_dwSamplesPerSec / 10;

   // amount to interpolate by
   DWORD dwDelta, dwInto;
   BOOL fInterpToLeft;
   fp fAlpha;
   dwDelta = dwSampEnd - dwSampStart;
   dwInto = dwSample - dwSampStart;
   fInterpToLeft = TRUE;
   if (dwDelta <= 2 * dwMaxOpen) {
      // create a peak at the center of the phoneme, fAlpha=1 when his phoneme the
      // strongest, and 0 when not visible
      fAlpha = (fp)dwInto / (fp)dwDelta;
      fAlpha += .5;
      if (fAlpha > 1.0) {
         fAlpha = 2.0 - fAlpha;
         fInterpToLeft = FALSE;
      }
   }
   else if (dwInto < dwMaxOpen) {
      // ramping up
      fAlpha = (fp)dwInto / (fp)dwMaxOpen / 2.0 + .5;
   }
   else if (dwSampEnd - dwSample < dwMaxOpen) {
      // ramping down
      fInterpToLeft = FALSE;
      fAlpha = (fp)(dwSampEnd - dwSample) / (fp)dwMaxOpen / 2.0 + .5;
   }
   else {
      // stay at peak
      fAlpha = 1; // max
   }


   // figure out what phoneme is to the left
   DWORD dwLeft, dwRight;
   if (fInterpToLeft) {
      dwLeft = (iLeft-1 >= 0) ? pp[iLeft-1].dwEnglishPhone : 0;   // know that 0 is silence
      dwRight = ((iLeft >= 0) && (iLeft < (int)dwNum)) ? pp[iLeft].dwEnglishPhone : 0;
   }
   else {
      fAlpha = 1.0 - fAlpha;  // since will have main target on left
      dwLeft = ((iLeft >= 0) && (iLeft < (int)dwNum)) ? pp[iLeft].dwEnglishPhone : 0;
      dwRight = (iRight < (int)dwNum) ? pp[iRight].dwEnglishPhone : 0;
   }
   if (dwLeft >= MLexiconEnglishPhoneNum())
      dwLeft = 0;
   if (dwRight >= MLexiconEnglishPhoneNum())
      dwRight = 0;
   pszLeft = MLexiconEnglishPhoneGet(dwLeft)->szPhoneLong;
   pszRight = MLexiconEnglishPhoneGet(dwRight)->szPhoneLong;

   // get the two phoneme
   PLEXENGLISHPHONE pLeft, pRight;
   pLeft = MLexiconEnglishPhoneGet(dwLeft);
   pRight = MLexiconEnglishPhoneGet(dwRight);
   if (!pLeft || !pRight)
      return FALSE;  // invalid phoneme so dont know

   // if the phonemes that are supposed to be fixed then do so
   if (fInterpToLeft) {
      // phoneme that want would be on right
      if (pRight->dwShape & PIS_KEEPSHUT)
         fAlpha = 1; // max
      else if (pLeft->dwShape & PIS_KEEPSHUT)
         fAlpha = (fAlpha - .5) * 2;
   }
   else {
      // phoneme that want would be on left
      if (pLeft->dwShape & PIS_KEEPSHUT)
         fAlpha = 0; // max
      else if (pRight->dwShape & PIS_KEEPSHUT)
         fAlpha = 1 - ((1 - fAlpha) - .5) * 2;
   }
   if (ppszLeft)
      *ppszLeft = pszLeft;
   if (ppszRight)
      *ppszRight = pszRight;
   if (pfAlpha)
      *pfAlpha = fAlpha;

   // figure out the values for the phoneme
   fp afLatTension[2];
   fp afVertOpen[2];
   fp afTeethTop[2];
   fp afTeethBottom[2];
   fp afTongueUp[2];
   fp afTongueForward[2];
   for (i = 0; i < 2; i++) {
      PLEXENGLISHPHONE ppi = i ? pRight : pLeft;

      switch (ppi->dwShape & PIS_LATTEN_MASK) {
      case PIS_LATTEN_PUCKER:
         afLatTension[i] = -1;
         break;
      default:
      case PIS_LATTEN_REST:
         afLatTension[i] = 0;
         break;
      case PIS_LATTEN_SLIGHT:
         afLatTension[i] = .5;
         break;
      case PIS_LATTEN_MAX:
         afLatTension[i] = 1;
         break;
      }

      switch (ppi->dwShape & PIS_VERTOPN_MASK) {
      case PIS_VERTOPN_CLOSED:
         afVertOpen[i] = 0;
         break;
      default:
      case PIS_VERTOPN_SLIGHT:
         afVertOpen[i] = .33;
         break;
      case PIS_VERTOPN_MID:
         afVertOpen[i] = .66;
         break;
      case PIS_VERTOPN_MAX:
         afVertOpen[i] = 1;
         break;
      }

      switch (ppi->dwShape & PIS_TEETHTOP_MASK) {
      case PIS_TEETHTOP_MID:
         afTeethTop[i] = .5;
         break;
      case PIS_TEETHTOP_FULL:
         afTeethTop[i] = 1;
         break;
      default:
         afTeethTop[i] = 0;
         break;
      }
      switch (ppi->dwShape & PIS_TEETHBOT_MASK) {
      case PIS_TEETHBOT_MID:
         afTeethBottom[i] = .5;
         break;
      case PIS_TEETHBOT_FULL:
         afTeethBottom[i] = 1;
         break;
      default:
         afTeethBottom[i] = 0;
         break;
      }

      switch (ppi->dwShape & PIS_TONGUETOP_MASK) {
      case PIS_TONGUETOP_ROOF:
         afTongueUp[i] = 1;
         break;
      case PIS_TONGUETOP_TEETH:
         afTongueUp[i] = .5;
         break;
      default:
      case PIS_TONGUETOP_BOTTOM:
         afTongueUp[i] = 0;
         break;
      }


      switch (ppi->dwShape & PIS_TONGUEFRONT_MASK) {
      case PIS_TONGUEFRONT_TEETH:
         afTongueForward[i] = 1;
         break;
      case PIS_TONGUEFRONT_BEHINDTEETH:
         afTongueForward[i] = .5;
         break;
      default:
      case PIS_TONGUEFRONT_PALATE:
         afTongueForward[i] = 0;
         break;
      }

   }

   // interpolate
   if (pfLateralTension)
      *pfLateralTension = (1.0 - fAlpha) * afLatTension[0] + fAlpha * afLatTension[1];
   if (pfVerticalOpen)
      *pfVerticalOpen = (1.0 - fAlpha) * afVertOpen[0] + fAlpha * afVertOpen[1];
   if (pfTeethTop)
      *pfTeethTop = (1.0 - fAlpha) * afTeethTop[0] + fAlpha * afTeethTop[1];
   if (pfTeethBottom)
      *pfTeethBottom = (1.0 - fAlpha) * afTeethBottom[0] + fAlpha * afTeethBottom[1];
   if (pfTongueForward)
      *pfTongueForward = (1.0 - fAlpha) * afTongueForward[0] + fAlpha * afTongueForward[1];
   if (pfTongueUp)
      *pfTongueUp = (1.0 - fAlpha) * afTongueUp[0] + fAlpha * afTongueUp[1];

   return TRUE;
}


/****************************************************************************
CM3DWave::WaveBufferGet - Used for animation, this gets a buffer from the wave.

This is different than the usual way because:
   1) If the wave data isn't loaded already it's laoded now.
   2) If the requested sampling rate or channels is different it fills in m_pAnimWave
      with an up/downsampled version.
   3) If the request is for data before or after the wave then it's filled with 0's
         (Unless it's completely before or after, in which case FALSE is returned.)
   4) If the looped flag it set then looping is assumed for the data
   5) If the dwMaxSamples flag is set then looping (and the rest) will stop after max-samples
      is reached

inputs
   DWORD             dwSamplesPerSec - SAmpling rate that want
   DWORD             dwChannels - Channels that want
   int               iTimeSec - Time in seconds (0 sec = start of data) to get
   int               iTimeFrac - Fractional time (in samples) to get.. Use iTimeSec and iTimeFrac
                           so can accurately measure time
   BOOL              fLoop - If TRUE then the wave will be treated as if it loops (starting at time=0)
                     until dwMaxSAmples
   DWORD             dwMaxSamples - This defines the ceiling of the maximum number of samples that
                     the wave will claim to have. If < m_dwSamples (adjusted for frequency)
                     it limits the duration of the wave to shorter than the original. If >= m_dwSamples
                     it does nothing EXCEPT if the fLoop setting is on. Use -1 for all
   DWORD             dwSamples - Number of samples that wants (starting at iTimeSec*dwSamplesPerSec + iTimeFrac
   short             *pasSamp - This is filled with the dwSAmples * dwChannels entries. If this is
                     before or after the wave data then it will contain 0 for silence. If all
                     the requested data is before/after the wave data then function will return FALSE
                     without doing anything
returns
   BOOL - TRUE if filled in pasSamp. FALSE if didn't - perhaps because requested audio out of range
*/
BOOL CM3DWave::WaveBufferGet (DWORD dwSamplesPerSec, DWORD dwChannels, int iTimeSec, int iTimeFrac,
                              BOOL fLoop, DWORD dwMaxSamples, DWORD dwSamples, short *pasSamp)
{
   // require that the wave is loaded
   if (!RequireWave (NULL))
      return FALSE;

   // if it's not downsampled then do so
   PCM3DWave pWave;
   pWave = this;
   if ((m_dwSamplesPerSec != dwSamplesPerSec) || (m_dwChannels != dwChannels)) {
      // need to downsample
      if (!m_pAnimWave || (m_pAnimWave->m_dwSamplesPerSec != dwSamplesPerSec) ||
         (m_pAnimWave->m_dwChannels != dwChannels)) {
            // create new one
            if (m_pAnimWave)
               delete m_pAnimWave;
            m_pAnimWave = Clone();
            if (!m_pAnimWave)
               return FALSE;
            if (!m_pAnimWave->ConvertSamplesAndChannels (dwSamplesPerSec, dwChannels, NULL)) {
               delete m_pAnimWave;
               m_pAnimWave = NULL;
               return FALSE;
            }
         }
      pWave = m_pAnimWave;
   }

   // next step, potentially limit maximum wave length
   if (!fLoop)
      dwMaxSamples = min(dwMaxSamples, pWave->m_dwSamples);

   // figure out start and end samples to copy
   __int64 i64Start, i64End;
   i64Start = (__int64)iTimeSec * (__int64)dwSamplesPerSec + (__int64)iTimeFrac;
   i64End = i64Start + (__int64)dwSamples;

   // if ends before the wave starts then nothing
   // if starts after it ends then nothing
   if ((i64End <= 0) || (i64Start >= (__int64)dwMaxSamples))
      return FALSE;

   // since the samples wanted overlap with the wave, assume they'll fit into an
   // int
   int iStart, iEnd;
   iStart = (int)i64Start;
   iEnd = (int)i64End;

   // fast copy
   if ((iStart >= 0) && (iEnd <= (int)pWave->m_dwSamples)) {
      memcpy (pasSamp, pWave->m_psWave + iStart*(int)dwChannels,
         (int)dwChannels * (iEnd - iStart) * sizeof(short));
      return TRUE;
   }

   // slow copy over
   int iSamp;
   for (; iStart < iEnd; iStart++, pasSamp += dwChannels) {
      if ((iStart < 0) || (iStart >= (int)dwMaxSamples)) {
         // beyond the limit, so set to zero
         memset (pasSamp, 0, dwChannels*sizeof(short));
         continue;
      }

      // if beyond end of existing data then modulo
      iSamp = iStart;
      if (iSamp >= (int)pWave->m_dwSamples)
         iSamp = iSamp % (int)pWave->m_dwSamples;

      memcpy (pasSamp, pWave->m_psWave + iSamp*(int)dwChannels, dwChannels*sizeof(short));
   }

   return TRUE;
}



/****************************************************************************
CM3DWave::QuickPlayQuery - Returns 2 if this wave is playing, 1 if any
wave is playing, 0 if nothing is playing
*/
int CM3DWave::QuickPlayQuery (void)
{
   if (m_hWaveOut)
      return 2;
   else if (gpWaveQuickPlay)
      return 1;
   else
      return 0;
}


/****************************************************************************
CM3DWave::QuickPlayStop - Call this to force the quick-play to stop
*/
void CM3DWave::QuickPlayStop (void)
{
   if (m_hWaveOut) {
      MMRESULT mm;
      mm = waveOutReset (m_hWaveOut);
      mm = waveOutUnprepareHeader (m_hWaveOut, &m_whWaveOut, sizeof(m_whWaveOut));
      mm = waveOutClose (m_hWaveOut);
      m_hWaveOut = NULL;
      gpWaveQuickPlay = NULL; // nothign quickplaying
   }

   if (m_pmemWaveOut) {
      delete m_pmemWaveOut;
      m_pmemWaveOut = NULL;
   }

   if (m_dwQuickPlayTimer) {
      KillTimer (NULL, m_dwQuickPlayTimer);
      m_dwQuickPlayTimer = NULL;
   }
}


/**********************************************************************************
CM3DWave::PlayCallback - Handles a call from the waveOutOpen function
*/
static void CALLBACK WavePlayCallback (
  HWAVEOUT hwo,       
  UINT uMsg,         
  DWORD_PTR dwInstance,  
  DWORD_PTR dwParam1,    
  DWORD_PTR dwParam2     
)
{
   PCM3DWave pwv = (PCM3DWave) dwInstance;
   pwv->PlayCallback (hwo, uMsg, dwParam1, dwParam2);
}

static VOID CALLBACK WaveTimerFunc(
  HWND hwnd,         // handle to window
  UINT uMsg,         // WM_TIMER message
  UINT_PTR idEvent,  // timer identifier
  DWORD dwTime       // current system time
)
{
   // called to stop playing
   if (gpWaveQuickPlay && gpWaveQuickPlay->m_fQuickPlayDone)
      gpWaveQuickPlay->QuickPlayStop();
}

void CM3DWave::PlayCallback (HWAVEOUT hwo,UINT uMsg,         
  DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
   // do something with the buffer
   if (uMsg == MM_WOM_DONE) {
      PWAVEHDR pwh = (WAVEHDR*) dwParam1;

      // note that we're done
      m_fQuickPlayDone = TRUE;
   }
}



/****************************************************************************
CM3DWave::QuickPlay - Call this to quickly play the wave (or part of it)
as a sample. This doesn't provide any playback position information though.

inputs
   DWORD             dwStart - Start sample
   DWORD             dwEnd - End sample, or -1 if play to end of wave
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::QuickPlay (DWORD dwStart, DWORD dwEnd)
{
   // stop playing if already playing
   QuickPlayStop();

   if (gpWaveQuickPlay)
      return FALSE;

   // copy over audio
   RequireWave (NULL);
   dwEnd = min(dwEnd, m_dwSamples);
   if (dwStart >= dwEnd)
      return FALSE;  // too small
   DWORD dwSamples = dwEnd - dwStart;
   DWORD dwSize = dwSamples * m_dwChannels * sizeof(short);
   if (!m_pmemWaveOut)
      m_pmemWaveOut = new CMem;
   if (!m_pmemWaveOut)
      return FALSE;
   if (!m_pmemWaveOut->Required (dwSize))
      return FALSE;
   memcpy (m_pmemWaveOut->p, m_psWave + (dwStart * m_dwChannels), dwSize);

   // open
   // create the wave
   MMRESULT mm;
   WAVEFORMATEX WFEX;
   memset (&WFEX, 0, sizeof(WFEX));
   WFEX.cbSize = 0;
   WFEX.wFormatTag = WAVE_FORMAT_PCM;
   WFEX.nChannels = m_dwChannels;
   WFEX.nSamplesPerSec = m_dwSamplesPerSec;
   WFEX.wBitsPerSample = 16;
   WFEX.nBlockAlign  = WFEX.nChannels * WFEX.wBitsPerSample / 8;
   WFEX.nAvgBytesPerSec = WFEX.nBlockAlign * WFEX.nSamplesPerSec;
   mm = waveOutOpen (&m_hWaveOut, WAVE_MAPPER, &WFEX, (DWORD_PTR) ::WavePlayCallback,
      (DWORD_PTR) this, CALLBACK_FUNCTION);
   if (mm) {
      m_hWaveOut = NULL;
      return FALSE;
   }

   // prepare the headers
   memset (&m_whWaveOut, 0, sizeof(m_whWaveOut));
   m_whWaveOut.dwBufferLength = dwSize;
   m_whWaveOut.lpData = (PSTR) (m_pmemWaveOut->p);
   mm = waveOutPrepareHeader (m_hWaveOut, &m_whWaveOut, sizeof(m_whWaveOut));
   if (mm) {
      waveOutClose (m_hWaveOut);
      m_hWaveOut = NULL;
      return FALSE;
   }

   gpWaveQuickPlay = this;
   m_fQuickPlayDone = FALSE;

   // because stopping in a callbakc hangs the system, set a timer
   m_dwQuickPlayTimer = SetTimer (NULL, NULL, 100, WaveTimerFunc);

   // play
   mm = waveOutWrite (m_hWaveOut, &m_whWaveOut, sizeof(m_whWaveOut));
   waveOutRestart (m_hWaveOut);

   return TRUE;
}


/****************************************************************************
CM3DWave::PitchSubClear - Clears the sub-pitch. Call if phonemes, words, or
pitch are changed.
*/
void CM3DWave::PitchSubClear (void)
{
}


/****************************************************************************
CM3DWave::PitchAtSample - Given a sample (which may be a fp value), this
returns the pitch (in hZ)

inputs
   DWORD       dwPitchSub - PITCH_F0 or PITCH_SUB
   fp          fSample - potentially fractional
   DWORD       dwChannel - channel
returns
   fp - Pitch in Hz
*/
fp CM3DWave::PitchAtSample (DWORD dwPitchSub, fp fSample, DWORD dwChannel)
{
   // NOTE: Code copied to Monotone() to make it faster

   double fAlpha, fOrigPitch;
   int iSample;
   fSample /= (fp)m_adwPitchSkip[dwPitchSub];
   fSample = max(0, fSample);   // always min 0
   iSample = (int) fSample;
   fAlpha = fSample - iSample;
   fOrigPitch = (1.0 - fAlpha) * m_apPitch[dwPitchSub][min((DWORD)iSample,m_adwPitchSamples[dwPitchSub]-1)*m_dwChannels+dwChannel].fFreq +
      fAlpha * m_apPitch[dwPitchSub][min((DWORD)iSample+1,m_adwPitchSamples[dwPitchSub]-1)*m_dwChannels+dwChannel].fFreq;
   fOrigPitch = max(fOrigPitch, 1);   // at least 1 hz

   return fOrigPitch;
}


/****************************************************************************
CM3DWave::StretchFFTFloat - This uses the m_apPitch[PITCH_F0] information (which means pitch
MUST be calculated before this) and gets the pitch at sample fCenter. This
is used to determine a set of samples. They are pulled out and stretched (interpolated)
to fit into the next largest FFT block. This is then analyzed. The result is then
interpolated into pafFFT (keeping pitch in tact), so you end up with a FFT that's
pitch independent.

This is used to get more accurate voiced/unvoiced determination.

inputs
   fp             fCenter - Center sample
   DWORD          dwChannel - Channel that want, or -1 for all channels
   DWORD          dwFFTSize - Stretch FFT out to this size. Will alternate between
                     sin() and cos() elements of FFT. Since the original signal
                     will be stretched from a single pitch period, every 2 elements
                     will be one harmonic.
   float          *pafFFT - Filled with the FFT. dwWindowSize must be available
   fp             fScaleLeft - Amount to scale incoming signal amplitude on the left
   fp             fScaleRight - Amount to scale incoming signal amplitude on right
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
   PCListFixed    plfCache - Cache used bu StretcjFFTFloat. Automatically initialzed to right size of 0 length
   PCListVariable plvCache - Cached used by StrettchFFTFloat.
returns
   none
*/

// STRETCHFFTFLOATCACHE
typedef struct {
   fp             fCenter;       // Center value
   DWORD          dwChannel;     // cahnnel
   DWORD          dwFFTSize;     // FFT size
   fp             fScaleLeft;    // cached left
   fp             fScaleRight;   // cached right
} STRETCHFFTFLOATCACHE, *PSTRETCHFFTFLOATCACHE;

void CM3DWave::StretchFFTFloat (fp fCenter, DWORD dwChannel, DWORD dwFFTSize, float *pafFFT,
                                fp fScaleLeft, fp fScaleRight, PCSinLUT pLUT, PCMem pMemFFTScratch,
                                PCListFixed plfCache, PCListVariable plvCache)
{
   // in this case, need a channel for pitch, so pick one
   DWORD dwChanPitch = dwChannel;
   if (dwChanPitch >= m_dwChannels)
      dwChanPitch = 0;

   // figure out the pitch in the center, left, and right
   double fWLCenter, fWLLeft, fWLRight;
   fWLCenter = (double)m_dwSamplesPerSec / PitchAtSample (PITCH_F0, fCenter, dwChanPitch);
   fWLLeft = (double)m_dwSamplesPerSec / PitchAtSample (PITCH_F0, fCenter - fWLCenter/2, dwChanPitch);
   fWLRight = (double)m_dwSamplesPerSec / PitchAtSample (PITCH_F0, fCenter + fWLCenter/2, dwChanPitch);

   // BUGFIX - Look in the cache
   DWORD i;
   PSTRETCHFFTFLOATCACHE psfc = (PSTRETCHFFTFLOATCACHE) plfCache->Get(0);
   for (i = 0; i < plfCache->Num(); i++, psfc++) {
      if ((psfc->dwChannel == dwChanPitch) && (psfc->dwFFTSize == dwFFTSize) &&
         (fabs(psfc->fCenter - fCenter) < fWLCenter/10) &&
         (min(fScaleLeft, psfc->fScaleLeft) / max(fScaleLeft, psfc->fScaleLeft) > 0.95) &&
         (min(fScaleRight, psfc->fScaleRight) / max(fScaleRight, psfc->fScaleRight) > 0.95) ) {

            // found match
            memcpy (pafFFT, plvCache->Get(i), plvCache->Size(i));
            return;
         }
   } // i

   // fill in the FFT
   memset (pafFFT, 0, dwFFTSize * sizeof(float));
   fp fAlpha;
   for (i = 0; i < dwFFTSize; i++) {
      fAlpha = (fp)i / (fp)dwFFTSize;
      fp fWLAtI = (1.0 - fAlpha) * fWLLeft + fAlpha * fWLRight;
      fp fSample = fCenter + (fAlpha - 0.5) * fWLAtI;
      if ((fSample < 0) || (fSample+1 >= m_dwSamples))
         continue;
      DWORD dwLeft, dwRight;
      dwLeft = (DWORD)fSample;
      dwRight = min(dwLeft+1, m_dwSamples-1);
      fSample -= dwLeft;

      // interpolate
      pafFFT[i] = (1.0 - fSample) * (float)m_psWave[dwLeft*m_dwChannels+dwChanPitch] +
         fSample * m_psWave[dwRight*m_dwChannels+dwChanPitch];

      // scale...
      pafFFT[i] *= ((1.0 - fAlpha) * fScaleLeft + fAlpha * fScaleRight);
   }

   // do the FFT
   FFTRecurseReal (pafFFT - 1, dwFFTSize, 1, pLUT, pMemFFTScratch);

   float   fScale = 2.0 / (float) dwFFTSize;// * 32768.0 / 32768.0;
   for (i = 0; i < dwFFTSize; i++)
      pafFFT[i] *= fScale;

   // Always make 0hz = 0
   pafFFT[0] = pafFFT[1] = 0;

   // done

#if 0 // BUGFIX - Don't cache because need supermagic FFT to reduce noise
   // cache this
   if (!plfCache->Num())
      plfCache->Init (sizeof(STRETCHFFTFLOATCACHE));

   // remove if too many
   while (plfCache->Num() > 10) {
      plfCache->Remove (0);
      plvCache->Remove (0);
   }

   // add
   STRETCHFFTFLOATCACHE sfc;
   memset (&sfc, 0, sizeof(sfc));
   sfc.dwChannel = dwChanPitch;
   sfc.dwFFTSize = dwFFTSize;
   sfc.fCenter = fCenter;
   sfc.fScaleLeft = fScaleLeft;
   sfc.fScaleRight = fScaleRight;
   plfCache->Add (&sfc);
   plvCache->Add (pafFFT, sizeof(float) * dwFFTSize);
#endif
}



/****************************************************************************
CM3DWave::StretchFFTSlide - Slides
the time left/right to make as on-phase as possible. Note: Hack test to
see what phase problems are.

NOTE - Acoustically I can't hear anything, but visually it looks a bit better
when this function is called.

inputs
   fp             fCenter - Center sample
   DWORD          dwChannel - Channel that want, or -1 for all channels
   DWORD          dwFFTSize - Stretch FFT out to this size. Will alternate between
                     sin() and cos() elements of FFT. Since the original signal
                     will be stretched from a single pitch period, every 2 elements
                     will be one harmonic.
   float          *pafFFT - Filled with the FFT. dwWindowSize must be available
   fp             fScaleLeft - Amount to scale incoming signal amplitude on the left
   fp             fScaleRight - Amount to scale incoming signal amplitude on right
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
returns
   fp - New fCenter
*/
fp CM3DWave::StretchFFTSlide (fp fCenter, DWORD dwChannel)
{
#define SLIDEDETAIL     256
   static BOOL sfStretchSin = FALSE;
   static fp safStretchSin[SLIDEDETAIL];

   DWORD i, dwPasses;
   if (!sfStretchSin) {
      for (i = 0; i < SLIDEDETAIL; i++)
         safStretchSin[i] = sin((fp)i / (fp)SLIDEDETAIL * 2.0 * PI);
      sfStretchSin = TRUE;
   }
   // in this case, need a channel for pitch, so pick one
   DWORD dwChanPitch = dwChannel;
   if (dwChanPitch >= m_dwChannels)
      dwChanPitch = 0;

   // figure out the pitch in the center, left, and right
   fp fWLCenter;

   // loop over a wavelength
   for (dwPasses = 0; dwPasses < 2; dwPasses++) {
      fWLCenter = (double)m_dwSamplesPerSec / PitchAtSample (PITCH_F0, fCenter, dwChanPitch);

      double fSin = 0, fCos = 0;
      for (i = 0; i < SLIDEDETAIL; i++) {
         fp fSample = fCenter + (((fp)i / (fp)SLIDEDETAIL) - 0.5) * fWLCenter;
         fp fValue;
         if ((fSample < 0) || (fSample+1 >= m_dwSamples))
            fValue = 0;
         else {
            DWORD dwLeft, dwRight;
            dwLeft = (DWORD)fSample;
            dwRight = min(dwLeft+1, m_dwSamples-1);
            fSample -= dwLeft;

            // interpolate
            fValue = (1.0 - fSample) * (float)m_psWave[dwLeft*m_dwChannels+dwChanPitch] +
               fSample * (float) m_psWave[dwRight*m_dwChannels+dwChanPitch];
         }

         // fp fAngle = (fp)i / (fp)SLIDEDETAIL * 2.0 * PI;
         fSin += safStretchSin[i] * fValue;  // sin
         fCos += safStretchSin[(i+SLIDEDETAIL/4)%SLIDEDETAIL] * fValue;  // cos
      } // i

      // repeat without the phase
      fp fPhase = atan2(fCos, fSin);
      if (fPhase >= PI)
         fPhase -= 2.0 * PI;  // just to make sure
      fp fOffset = fPhase / 2.0 / PI * fWLCenter;
      fCenter -= fOffset;
   } // dwPasses


   return fCenter;
}

/****************************************************************************
FFTCalcEnergy - Given a pointer to FFT results, this calculates the energy.

inputs
   float       *pafFFT - FFT
   DWORD       dwFFTSize - size of FFT
returns
   fp - energy
*/
fp FFTCalcEnergy (float *pafFFT, DWORD dwFFTSize)
{
   fp fEnergy  = 0;
   DWORD i;
   for (i = 0; i < dwFFTSize; i++)
      fEnergy += pafFFT[i] * pafFFT[i];
   return sqrt(fEnergy);
}

/****************************************************************************
FFTScale - Scales the FFT by the given amount.

inputs
   float       *pafFFT - FFT
   DWORD       dwFFTSize - size of FFT
   fp          fScale - amount to scale by
*/
void FFTScale (float *pafFFT, DWORD dwFFTSize, fp fScale)
{
   DWORD i;
   for (i = 0; i < dwFFTSize; i++)
      pafFFT[i] *= fScale;
}

/****************************************************************************
FFTConvertToEnergyAndPhase - This goes through an FFT, which alternates
between sin and cos values, and rewrites it with energy and phase information.
So even numbers are written with energy, and odd with phease

inputs
   float       *pafFFT - FFT
   DWORD       dwFFTSize - size of FFT
*/
void FFTConvertToEnergyAndPhase (float *pafFFT, DWORD dwFFTSize)
{
   DWORD i;
   for (i = 0; i < dwFFTSize; i += 2) {
      fp fEnergy = sqrt(pafFFT[i]*pafFFT[i] + pafFFT[i+1]*pafFFT[i+1]);
      fp fPhase = atan2(pafFFT[i], pafFFT[i+1]);
      pafFFT[i] = fEnergy;
      pafFFT[i+1] = fPhase;
   } // i

   // NOTE: The following code moved to PhaseRotateSoZeroFundamental()
#if 0
   // adjust all phases so they're relative to the fundemental
   dwFFTSize /= 2;
   for (i = 2; i < dwFFTSize; i++) {
      pafFFT[i*2+1] -= pafFFT[1*2+1] * (float)i;  // subtract phase of fundamental
      pafFFT[i*2+1] = myfmod(pafFFT[i*2+1], 2.0 * PI);
   }
   pafFFT[1*2+1] = 0;  // set fundamental to 0
#endif // 0
   pafFFT[0*2+1] = 0;   // since shouldnt exist
}

/****************************************************************************
FFTBlur - Blurs an FFT (especially at higher frequencies) so it takes
into account some of the noisness of FFTs.

NOTE: Assumes the FFT has been converted to energy and phase, so it only
deals with the energy (even) values.

inputs
   float       *pafFFT - FFT
   DWORD       dwFFTSize - size of FFT
   DWORD       dwAmount - Usually 20 so blurs with 1/20th the current band number
   float       *pafScratch - Scratch space, must be dwFFTSize.
*/
void FFTBlur (float *pafFFT, DWORD dwFFTSize, DWORD dwAmount, float *pafFFTScratch)
{
   // copy to scratch
   memcpy (pafFFTScratch, pafFFT, dwFFTSize * sizeof(float));

   DWORD i;
   dwFFTSize /= 2;
   for (i = 0; i < dwFFTSize; i++) {
      DWORD dwWindow = i / dwAmount;
      DWORD dwCount = 0;
      DWORD dwWeight;
      fp fSum = 0;
      fp fVal;
      int iCur;
      for (iCur = (int)i-(int)dwWindow; iCur <= (int)i+(int)dwWindow; iCur++) {
         if ((iCur < 0) || (iCur >= (int)dwFFTSize))
            fVal = 0;
         else
            fVal = pafFFTScratch[iCur*2];

         dwWeight = dwWindow + 1 - (DWORD)abs(iCur - (int)i);
         fSum += fVal * (fp)dwWeight;
         dwCount += dwWeight;
      }
      if (dwCount >= 2)
         fSum /= (fp)dwCount;

      pafFFT[i*2] = fSum;
   } // i
}

/****************************************************************************
FFTToSRDATAPOINT - Takes an FFT and stretches it out so it fills in an
array of SRDATAPOINT fields.

NOTE: The FFT must have been converted to energy-only.

inputs
   float       *pafFFT - FFT
   DWORD       dwFFTSize - size of FFT
   fp          fPitch - Pitch of the original wave, in Hz (and hence the fundamental)
   float       *pafSRD - Filled with SRDATAPOINT energy values
*/
void FFTToSRDATAPOINT (float *pafFFT, DWORD dwFFTSize, fp fPitch, float *pafSRD)
{
   DWORD i;
   DWORD dwBasePitch;
   for (i = 0; i < SRDATAPOINTS; i++) {
      // what is this in the FFT
      fp fFreq = SRBASEPITCH * pow ((fp)2.0, (fp)((fp)i / SRPOINTSPEROCTAVE));
      fp fAlpha = fFreq / fPitch;
      dwBasePitch = (DWORD)fAlpha;
      fAlpha -= dwBasePitch;
      if (dwBasePitch+1 >= dwFFTSize) {
         pafSRD[i] = 0;
         continue;
      }

      // interpolate energy
      pafSRD[i] = (1.0 - fAlpha) * pafFFT[dwBasePitch] + fAlpha * pafFFT[dwBasePitch+1];
   } // i
}

/****************************************************************************
CM3DWave::CalcWavelengthEnergy - Calculates the average energy (per sample) for a range of
of samples.

inputs
   int            iCenter - Center sample
   int            iSamples - Number of samples
returns
   fp - Average energy
*/
fp CM3DWave::CalcWavelengthEnergy (int iCenter, int iSamples)
{
   fp f, fSum = 0;
   int i;
   int iSamp;
   for (i = 0; i < iSamples; i++) {
      iSamp = iCenter - iSamples/2 + i;
      if ((iSamp < 0) || (iSamp >= (int)m_dwSamples))
         continue;

      // else
      f = m_psWave[iSamp * (int)m_dwChannels+0];
      fSum += f*f;
   } // i

   return sqrt(fSum) / (fp)iSamples;
}

/****************************************************************************
CM3DWave::MagicFFT - This is a magic FFT that figures out a) the harmonics
of a wave at a given point, and b) the noise at a given point.

NOTE: Only does this on channel 0
NOTE: Assumes that the pitch is 100% accurate at dwSRSample.

inputs
   fp             fSRSample - Speech recognition sample
   float          *pafFFTVoiced - Filled in with SRDATAPOINTS for energy of voiced
   float          *pafFFTNoise - Filled in with SRDATAPOINTS for energy of unvoiced
   float          *pafFFTPhase - Filled in with SRPHASENUM harmonics. Only the voiced harmonics, every NOISEEXTRA,
                  are actually calculated
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
   PCListFixed    plfCache - Cache used bu StretcjFFTFloat. Automatically initialzed to right size of 0 length
   PCListVariable plvCache - Cached used by StrettchFFTFloat.
   BOOL           fCanSlide - If TRUE then can slide the center point to maximize the phase accuracy
   PSRFPHASEEXTRACT  pSRFPhase - If not NULL, this is filled in with the phase information adjustment.
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::MagicFFT (fp fSRSample, float *pafFFTVoiced, float *pafFFTNoise, float *pafFFTPhase,
                         PCSinLUT pLUT, PCMem pMemFFTScratch,
                         PCListFixed plfCache, PCListVariable plvCache,
                         BOOL fCanSlide, PSRFPHASEEXTRACT pSRFPhase)
{
   // BUGFIX - Modified so fSRSample is a floating point value so can do some
   // stocahsitc sampling

   if (pSRFPhase)
      memset (pSRFPhase, 0, sizeof(*pSRFPhase));

   // just to make sure
   if ((fSRSample >= (fp)m_dwSRSamples) || (fSRSample >= (fp)m_adwPitchSamples[PITCH_F0]) ||
      (m_dwSRSkip != m_adwPitchSkip[PITCH_F0]) || !m_paSRFeature || !m_apPitch[PITCH_F0])
      return FALSE;

   // BUGFIX - slide the center to exact phase match
   fp fCenterSample = fSRSample * (fp)m_dwSRSkip;
   if (fCanSlide)
      fCenterSample = StretchFFTSlide (fCenterSample, 0);

   // create a FFT window large enough for the pitch
   fp fPitch = PitchAtSample (PITCH_F0, fCenterSample /* BUGFIX - was fSRSample * (fp)m_adwPitchSkip[PITCH_F0]*/, 0);
   fPitch = max(fPitch, 1);
   fp fPitchOrig = fPitch;
   fPitch = (fp)m_dwSamplesPerSec / fPitch;

   // BUGFIX - slide to fLeftSAmple and fRightSample
   fp fLeftSample, fRightSample;
   if (fCanSlide) {
      fp fTry;
      fLeftSample = StretchFFTSlide (fTry = fCenterSample - fPitch, 0);
      fLeftSample = max(fLeftSample, fTry - fPitch/10);  // only allow to slide so much
      fLeftSample = min(fLeftSample, fTry + fPitch/10);  // only allow to slide so much
      fRightSample = StretchFFTSlide (fTry = fCenterSample + fPitch, 0);
      fRightSample = max(fRightSample, fTry - fPitch/10);  // only allow to slide so much
      fRightSample = min(fRightSample, fTry + fPitch/10);  // only allow to slide so much
   }
   else {
      fLeftSample = fCenterSample - fPitch;
      fRightSample = fCenterSample + fPitch;
   }
   DWORD dwWindowSize;
   for (dwWindowSize = 2; dwWindowSize < fPitch; dwWindowSize *= 2);

   // allocate enough memory for 3 ffts
   CMem memFFT;
   if (!memFFT.Required (4 * dwWindowSize * sizeof(float) + 6 * SRDATAPOINTS * sizeof(float) +
      dwWindowSize/2 * 2 * sizeof(float)))
      return FALSE;
   float *pafFFTLeft = (float*)memFFT.p;
   float *pafFFTCenter = pafFFTLeft + dwWindowSize;
   float *pafFFTRight = pafFFTCenter + dwWindowSize;
   float *pafFFTScratch = pafFFTRight + dwWindowSize;
   float *pafSRDLeft = pafFFTScratch + dwWindowSize;
   float *pafSRDCenter = pafSRDLeft + SRDATAPOINTS;
   float *pafSRDRight = pafSRDCenter + SRDATAPOINTS;
   float *pafSRDPhaseLeft = pafSRDRight + SRDATAPOINTS;
   float *pafSRDPhaseCenter = pafSRDPhaseLeft + SRDATAPOINTS;
   float *pafSRDPhaseRight = pafSRDPhaseCenter + SRDATAPOINTS;
   float *pafNoise = pafSRDPhaseCenter + SRDATAPOINTS;
   float *pafVoiced = pafNoise + dwWindowSize/2;

   // figure out the volume levels at 5 points, at edges of adjacent pitch
   // periods and in absolute center
   DWORD i;
   fp afEnergy[5];
   int iPitch = (int)ceil(fPitch);
   afEnergy[0] = CalcWavelengthEnergy ((int)(fLeftSample - fPitch/2), iPitch);
      // BUGFIX - was fCenterSample - 3.0/2.0*fPitch
   afEnergy[1] = CalcWavelengthEnergy ((int)(fCenterSample + fLeftSample)/2, iPitch);
      // BUGFIX - was fCenterSample - 1.0/2.0*fPitch
   afEnergy[2] = CalcWavelengthEnergy ((int)fCenterSample, iPitch);
   afEnergy[3] = CalcWavelengthEnergy ((int)(fCenterSample + fRightSample)/2, iPitch);
      // BUGFIX - was fCenterSample + 1.0/2.0*fPitch
   afEnergy[4] = CalcWavelengthEnergy ((int)(fRightSample + fPitch/2), iPitch);
      // BUGFIX - was fCenterSample + 3.0*2.0*fPitch <<< Note... * is a bug too
   afEnergy[2] = max(afEnergy[2], CLOSE);
   for (i = 0; i < 5; i++) // make sure at least some energy, but dont allow to be too far off
      afEnergy[i] = max(afEnergy[i], afEnergy[2]/2);  // dont scale too much

   // do the FFT - NOTE: this is clever because it automatically takes pitch change
   // into account.
   StretchFFTFloat (fLeftSample, 0, dwWindowSize, pafFFTLeft,
      afEnergy[2] / afEnergy[0], afEnergy[2] / afEnergy[1], pLUT, pMemFFTScratch,
      plfCache, plvCache);
   StretchFFTFloat (fCenterSample, 0, dwWindowSize, pafFFTCenter,
      afEnergy[2] / afEnergy[1], afEnergy[2] / afEnergy[3], pLUT, pMemFFTScratch,
      plfCache, plvCache);
   StretchFFTFloat (fRightSample, 0, dwWindowSize, pafFFTRight,
      afEnergy[2] / afEnergy[3], afEnergy[2] / afEnergy[4], pLUT, pMemFFTScratch,
      plfCache, plvCache);

   // figure out the energies and normalize the left and right so dealing
   // with same energy levels, in case audio suddenly getting quieter or louder
   fp fEnergyLeft = FFTCalcEnergy (pafFFTLeft, dwWindowSize);
   fp fEnergyCenter = FFTCalcEnergy (pafFFTCenter, dwWindowSize);
   fp fEnergyRight = FFTCalcEnergy (pafFFTRight, dwWindowSize);
   fEnergyLeft = max(fEnergyLeft, CLOSE);
   fEnergyCenter = max(fEnergyCenter, CLOSE);
   fEnergyRight = max(fEnergyRight, CLOSE);
   FFTScale (pafFFTLeft, dwWindowSize, fEnergyCenter / fEnergyLeft);
   FFTScale (pafFFTRight, dwWindowSize, fEnergyCenter / fEnergyRight);

   // convert these into energy and phase information...
   FFTConvertToEnergyAndPhase (pafFFTLeft, dwWindowSize);
   FFTConvertToEnergyAndPhase (pafFFTCenter, dwWindowSize);
   FFTConvertToEnergyAndPhase (pafFFTRight, dwWindowSize);

   // fill in the phase structure
   if (pSRFPhase) {
      for (i = 0; i < SRFFPHASEEXTRACT_NUM; i++) {
         pSRFPhase->afEnergy[i] = pafFFTCenter[(i+1)*2];
         pSRFPhase->afPhase[i] = pafFFTCenter[(i+1)*2+1];
      } // i
      pSRFPhase->fCenter = fCenterSample;
      pSRFPhase->fWavelength = fPitch;
   }

   // blur the FFT so have a better sense of noise
   // BUGFIX - Make blur 20
#if 0 // BUGFIX - remove because no longer needed once have supersampling on
   FFTBlur (pafFFTLeft, dwWindowSize, 20, pafFFTScratch);
   FFTBlur (pafFFTCenter, dwWindowSize, 20, pafFFTScratch);
   FFTBlur (pafFFTRight, dwWindowSize, 20, pafFFTScratch);
#endif

   // fill in the phase information
   for (i = 0; i < SRPHASENUM; i++) {
      if (i+1 >= dwWindowSize/2) {
         pafFFTPhase[i] = 0;
         continue;
      }

      pafFFTPhase[i] = pafFFTCenter[(i+1)*2+1];
      // BUGFIX - No need to do the following because removed it when converted
      // to phase in first place
      //pafFFTPhase[i] -= pafFFTCenter[1*2+1] * (float)(i+1); // subtract phase of fundamental
      //pafFFTPhase[i] = myfmod(pafFFTPhase[i], 2.0 * PI);
   }

   // BUGFIX - Cutoff used to be 7, or 10, but changed to depend in the pitch
   DWORD dwCutoff = (DWORD)(7.0 / (fp) fPitchOrig * 200.0);
   dwCutoff = max(dwCutoff, 1);
      // using 7.0, becuase worked well for female voices with pitch around 200
      // thus, male voice with 1/3 the pitch would be 21

   // loop through the harmonics and figure out how much is noise and how much is voiced
   fp fNoiseLow = 0, fVoicedLow = 0, fEnergyAbove = 0;
   for (i = 1; i < dwWindowSize/2; i++) {
      if (i > dwCutoff) { // BUGFIX - changed 10 to 7 because caused problems for female voices
         // calculate the energy above
         fEnergyAbove += pafFFTCenter[i*2];
         continue;
      }

      // energy
      fp fLeft = pafFFTLeft[i*2];
      fp fRight = pafFFTRight[i*2];
      fp fCenter = pafFFTCenter[i*2];

      // figure out in phase and out of phase
      fp fPhaseLeft, fPhaseRight;
      if (fLeft) {
         fPhaseLeft = fabs(pafFFTLeft[i*2+1] - pafFFTCenter[i*2+1]);
         if (fPhaseLeft > PI)
            fPhaseLeft = 2.0*PI - fPhaseLeft;  // so get smallest phase
      }
      else {
         fPhaseLeft = PI;
         fLeft = CLOSE; // so have something
      }
      if (fRight) {
         fPhaseRight = fabs(pafFFTRight[i*2+1] - pafFFTCenter[i*2+1]);
         if (fPhaseRight > PI)
            fPhaseRight = 2.0*PI - fPhaseRight;  // so get smallest phase
      }
      else {
         fPhaseRight = PI;
         fRight = CLOSE; // so have something
      }

      // average phase
      fp fAvg = (fPhaseLeft * fLeft + fPhaseRight * fRight) / (fLeft + fRight);
      fp fNoise = fCenter * sin(fAvg / 2);
      fp fVoiced = fCenter - fNoise;

      fNoiseLow += fNoise;
      fVoicedLow += fVoiced;
   }
   fNoiseLow += fEnergyAbove/4; // energy above 1st 10 harmonics more likely to be noise
         // just played with /x to get to work
   fVoicedLow = fVoicedLow / max(fNoiseLow+fVoicedLow,CLOSE);

   // move fAmt to extremes
#define RATIOCUT           .2    // BUGFIX - move cut ratio lower to encourage voiced, just played with value to get to work
   // BUGFIX - RATIOCUT was .2, changed to .5, since with new supersampling works better
   // BUGFIX - Change to .2 (from .5) since having problems with blizzard male voice
#define RATIOPOW           1     // BUGFIX - was 4, but works better (or alsmost same) as 1 when
                                 // have supersampling turned on
   fp fPhaseForgive;
   if (fVoicedLow < RATIOCUT)
      fPhaseForgive = (pow(fVoicedLow/RATIOCUT, RATIOPOW)-1) * PI;  // will cause negative values which encourages rest to be noisy
   else
      fPhaseForgive = pow((fVoicedLow-RATIOCUT) / (1.0 - RATIOCUT), 1.0 / RATIOPOW) * PI;

   // figure out the voiced and unvoiced energy
   for (i = 0; i < dwWindowSize/2; i++) {
      // get the left, right, and center energy
      fp fLeft, fRight, fCenter;
      fLeft = pafFFTLeft[i*2];
      fCenter = pafFFTCenter[i*2];
      fRight = pafFFTRight[i*2];

      // figure out the phase difference between the left/right and center
      fp fPhaseLeft, fPhaseRight;
      if (fLeft) {
         fPhaseLeft = fabs(pafFFTLeft[i*2+1] - pafFFTCenter[i*2+1]);
         if (fPhaseLeft > PI)
            fPhaseLeft = 2.0*PI - fPhaseLeft;  // so get smallest phase
      }
      else {
         fPhaseLeft = PI;
         fLeft = CLOSE; // so have something
      }
      if (fRight) {
         fPhaseRight = fabs(pafFFTRight[i*2+1] - pafFFTCenter[i*2+1]);
         if (fPhaseRight > PI)
            fPhaseRight = 2.0*PI - fPhaseRight;  // so get smallest phase
      }
      else {
         fPhaseRight = PI;
         fRight = CLOSE;   // so have something
      }

      // If phases are close will want to reduce the phase so
      // that is forgiving of some phase shifts...
      if (fPhaseLeft <= fPhaseForgive)
         fPhaseLeft = 0;
      else
         fPhaseLeft = (fPhaseLeft - fPhaseForgive) / (PI - fPhaseForgive) * PI;
      if (fPhaseRight <= fPhaseForgive)
         fPhaseRight = 0;
      else
         fPhaseRight = (fPhaseRight - fPhaseForgive) / (PI - fPhaseForgive) * PI;

      // figure the amount in phase
      fp fOutPhaseLeft, fOutPhaseRight;
      fOutPhaseLeft = sin(fPhaseLeft/2);
      fOutPhaseRight = sin(fPhaseRight/2);

      // noise is the amount out of phase
      fp fNoise = (fOutPhaseLeft*fLeft + fOutPhaseRight*fRight)/(fLeft+fRight) * fCenter;
      
      // voiced in the in phase stuff
      fp fVoiced = fCenter - fNoise;

      // write out
      pafVoiced[i] = fVoiced;
      pafNoise[i] = fNoise;
   } // i


   // convert these into a log scale
   FFTToSRDATAPOINT (pafVoiced, dwWindowSize/2, (fp)m_dwSamplesPerSec / fPitch,
      pafFFTVoiced);
   FFTToSRDATAPOINT (pafNoise, dwWindowSize/2, (fp)m_dwSamplesPerSec / fPitch,
      pafFFTNoise);

   return TRUE;
}


/****************************************************************************
PhaseDelta - Determine the difference between two phases, from -PI to PI

inputs
   fp             fPhaseA - One phase
   fp             fPhaseB - Other phase
returns
   fp - Delta
*/
__inline fp PhaseDelta (fp fPhaseA, fp fPhaseB)
{
   fp fDist1 = fabs(fPhaseA - fPhaseB);
   fp fDist2 = (fPhaseA < fPhaseB) ? (fPhaseA + 2*PI - fPhaseB) : (fPhaseB + 2*PI - fPhaseA);
   return min(fDist1, fDist2);
}


/****************************************************************************
CM3DWave::SuperMagicFFT - This is like MagicFFT except that it supersamples
the point to get rid of the annyoing noise spikes that appear.

inputs
   fp             fSRSample - Speech recognition sample (to center around)
   float          *pafFFTVoiced - Filled in with SRDATAPOINTS for energy of voiced
   float          *pafFFTNoise - Filled in with SRDATAPOINTS for energy of unvoiced
   float          *pafFFTPhase - Filled in with SRPHASENUM harmonics. Only the voiced harmonics, every NOISEEXTRA,
                  are actually calculated
   float          *pafFFTPhaseLast - Filled in with SRPHASENUM phase from the last time. This can be NULL.
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
   PCListFixed    plfCache - Cache used bu StretcjFFTFloat. Automatically initialzed to right size of 0 length
   PCListVariable plvCache - Cached used by StrettchFFTFloat.
   PSRFPHASEEXTRACT  pSRFPhase - If not NULL, this is filled in with the phase information adjustment.
returns
   BOOL - TRUE if success
*/
static int _cdecl floatSort (const void *elem1, const void *elem2)
{
   float *pdw1, *pdw2;
   pdw1 = (float*) elem1;
   pdw2 = (float*) elem2;

   if (pdw1[0] < pdw2[0])
      return -1;
   else if (pdw1[0] > pdw2[0])
      return 1;
   else
      return 0;
}

BOOL CM3DWave::SuperMagicFFT (fp fSRSample, float *pafFFTVoiced, float *pafFFTNoise, float *pafFFTPhase,
                              float *pafFFTPhaseLast,
                              PCSinLUT pLUT, PCMem pMemFFTScratch,
                              PCListFixed plfCache, PCListVariable plvCache,
                              PSRFPHASEEXTRACT pSRFPhase)
{
   // zero out
   memset (pafFFTVoiced, 0, SRDATAPOINTS * sizeof(float));
   memset (pafFFTNoise, 0, SRDATAPOINTS * sizeof(float));
   memset (pafFFTPhase, 0, SRPHASENUM * sizeof(float));
   if (pSRFPhase)
      memset (pSRFPhase, 0, sizeof(*pSRFPhase));

#define SUPERSAMPLE     8 // BUGFIX - OK with supersample 5, but better with 15
#define SUPERSAMPLEPLUSONE (SUPERSAMPLE+1)
   // BUGFIX - Lowered supersample to 8 (from 15) since dont need as much with FixHighFrequencyBlurriness
   // allocate spare memory
   CMem mem;
   if (!mem.Required (sizeof(float) * SUPERSAMPLEPLUSONE * SRDATAPOINTS*2 + sizeof(float)*SRPHASENUM))
      return FALSE;
   float *pfTempVoiced = (float*)mem.p;
   float *pfTempNoise = pfTempVoiced + SRDATAPOINTS * SUPERSAMPLEPLUSONE;
   float *pfTempPhase = pfTempNoise + SRDATAPOINTS * SUPERSAMPLEPLUSONE;

   // NOTE necessary because done above:
   // if (pafFFTPhase)
   //   memset (pafFFTPhase, 0, SRPHASENUM*sizeof(float));

   DWORD i, j;
   //fp fPrevPhase;
   BOOL fFilledInPhase = FALSE;
   for (i = 0; i < SUPERSAMPLEPLUSONE; i++) {
      DWORD dwSample = i;
      BOOL fSlide = FALSE;
      if (i >= SUPERSAMPLE) {
         dwSample = SUPERSAMPLE/2;
         fSlide = TRUE;
      }

      fp fToUse = fSRSample + ((fp)dwSample - (fp)(SUPERSAMPLE-1)/2) / (fp)SUPERSAMPLE;

      if (!MagicFFT (fToUse, pfTempVoiced + SRDATAPOINTS*i,
         pfTempNoise + SRDATAPOINTS*i,
         pfTempPhase, pLUT, pMemFFTScratch, plfCache, plvCache,
         fSlide, fSlide ? pSRFPhase : NULL))
         continue;

      // take the phase from the central one
      // BUGFIX - Take the phase that changes the least from the previous phases
#define OLDPHASECODE
#ifdef OLDPHASECODE
      if (fSlide)
         memcpy (pafFFTPhase, pfTempPhase, SRPHASENUM*sizeof(float));
#else
      if (pafFFTPhase) {
         if (fFilledInPhase)
            for (j = 0; j < SRPHASENUM; j++) {
               fPrevPhase = pafFFTPhaseLast ? pafFFTPhaseLast[j] : 0;
               if (PhaseDelta(fPrevPhase, pfTempPhase[j]) < PhaseDelta(fPrevPhase, pafFFTPhase[j]))
                  pafFFTPhase[j] = pfTempPhase[j];
            } // j
         else {
            memcpy (pafFFTPhase, pfTempPhase, SRPHASENUM*sizeof(float));
            fFilledInPhase = TRUE;
         }
      }

#endif // OLDPHASECODE
   } // i

   // try to find the "sharpest" set of formants and use that for
   // the energy signature
   double afSharpSum[SUPERSAMPLEPLUSONE], afSharpSumSquare[SUPERSAMPLEPLUSONE];
   DWORD dwSharpest = 0;
   for (i = 0; i < SUPERSAMPLEPLUSONE; i++) {
      afSharpSum[i] = afSharpSumSquare[i] = 0;
      for (j = 0; j < SRDATAPOINTS; j++) {
         double fSum = pfTempNoise[i*SRDATAPOINTS+j] + pfTempVoiced[i*SRDATAPOINTS+j];
         afSharpSumSquare[i] += fSum * fSum;
         afSharpSum[i] += fSum;
      } // j

      afSharpSum[i] *= afSharpSum[i];  // so keep right units/weighting
      if (afSharpSum[i] > CLOSE)
         afSharpSumSquare[i] /= afSharpSum[i];
      else
         afSharpSumSquare[i] = 0;

      // see if this is the sharpest
      if (afSharpSumSquare[i] > afSharpSumSquare[0])
         dwSharpest = i;
   } // i

   float afTemp[SUPERSAMPLEPLUSONE];

   // sort so know lowest to highest score
   // got decent results when took lowest energy of the 15 tests, except that
   // sounded muted. if take median, sounds a bit noisy in the upper register.
   // therefore, so some hackish stuff and take lower ranks for higher frequencies
   // (when using voice), and higher ones for noise.
   DWORD dwNoise;
   DWORD dwRank;
#define VOICEDCUTOFF       (SRDATAPOINTS - 2*SRPOINTSPEROCTAVE)
#define MEDIAN             (SUPERSAMPLEPLUSONE/2)
   for (i = 0; i < SRDATAPOINTS; i++) {
      // BUGFIX - Must keep energy constant!
      fp afAdjusted[2];
      fp fSumNoiseVoice = 0;

      for (dwNoise = 0; dwNoise < 2; dwNoise++) {
         float *pafSrc = (dwNoise ? pfTempNoise : pfTempVoiced);

         for (j = 0; j < SUPERSAMPLEPLUSONE; j++) {
            afTemp[j] = pafSrc[j*SRDATAPOINTS + i];

            // fSumNoiseVoice += afTemp[j];
         }
         qsort (afTemp, SUPERSAMPLEPLUSONE, sizeof(float), floatSort);

         dwRank = MEDIAN;

         // for voiced, biased on lower frequencies, noise biased on higher
         if (dwNoise) {
            if (i < VOICEDCUTOFF)
               dwRank = 0;
            else
               dwRank = (i - VOICEDCUTOFF) * MEDIAN/2 // BUGFIX - keep noise low
                  / (SRDATAPOINTS - VOICEDCUTOFF);
         }
#if 0 // BUGFIX - disabled because shouldnt need the cutoff with voiced noe
         else {
            if (i < VOICEDCUTOFF)
               dwRank = MEDIAN;
            else
               dwRank = MEDIAN - 1 - (i - VOICEDCUTOFF) * MEDIAN
                  / (SRDATAPOINTS - VOICEDCUTOFF);
         }
#endif // 0

         afAdjusted[dwNoise] = afTemp[dwRank];
      } // dwNoise

      // BUGFIX - readjust total energy so not actually losing any!
      // Was just keeping one value before and losing energy in some cases!
      //fSumNoiseVoice /= (fp)SUPERSAMPLEPLUSONE;
      // BUGFIX - fSumNoiseVoice used from the sharpest. This way get sharper formants
      fSumNoiseVoice = pfTempNoise[dwSharpest*SRDATAPOINTS + i] + pfTempVoiced[dwSharpest*SRDATAPOINTS + i];
      
      // make sure something
      fp fSumFinal = afAdjusted[0] + afAdjusted[1];
      fSumFinal = max(fSumFinal, CLOSE);

      pafFFTVoiced[i] = afAdjusted[0] / fSumFinal * fSumNoiseVoice;
      pafFFTNoise[i] = afAdjusted[1] / fSumFinal * fSumNoiseVoice;
   }

   // BUGFIX - Adjust the total energy so get crisper signal
   FixHighFrequencyBlurriness (fSRSample, pafFFTVoiced, pafFFTNoise, pLUT, pMemFFTScratch);

   return TRUE;
}

/****************************************************************************
CM3DWave::SRFEATUREStretch - This stretches out the wave, kind of. It stretches
out the sr features and pitch. It blanks out the audio and the energy.

inputs
   fp          fStretch - Amount to stretch it out
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FXSRFEATUREStretch (fp fStretch)
{
   fStretch = max(fStretch, .01);

   // must have something
   if (!m_dwSRSamples || !m_adwPitchSamples[PITCH_F0])
      return FALSE;

   // initalize the list of SRfeatures and pitch
   CListFixed lSRFEATURE, lPitch;
   lSRFEATURE.Init (sizeof(SRFEATURE), m_paSRFeature, m_dwSRSamples);
   lPitch.Init (sizeof(WVPITCH), m_apPitch[PITCH_F0], m_adwPitchSamples[PITCH_F0]);
   PSRFEATURE pSROld = (PSRFEATURE) lSRFEATURE.Get(0);
   PWVPITCH pPitchOld = (PWVPITCH) lPitch.Get(0);

   // zero out energy
   m_dwEnergySamples = 0;

   // when this is stretched out how many samples should have
   m_dwSamples = (DWORD)(double)m_dwSamples * (double)fStretch;
   if (!m_pmemWave->Required (m_dwSamples * m_dwChannels * sizeof(short)))
      return FALSE;
   m_psWave = (short*) m_pmemWave->p;
   memset (m_psWave, 0, m_dwSamples * m_dwChannels * sizeof(short));

   // allocate SR and pitch samples
   DWORD dwNeed;
   m_dwSRSamples = (m_dwSamples / m_dwSRSkip) + 1;
   dwNeed = m_dwSRSamples * sizeof (SRFEATURE);
   if (!m_pmemSR)
      m_pmemSR = new CMem;
   if (!m_pmemSR || !m_pmemSR->Required (dwNeed)) {
      m_dwSRSamples = 0;
      return FALSE;
   }
   m_paSRFeature = (PSRFEATURE) m_pmemSR->p;
   memset (m_paSRFeature, 0, dwNeed);
   m_adwPitchSamples[PITCH_F0] = m_dwSRSamples;
   dwNeed = m_adwPitchSamples[PITCH_F0] * m_dwChannels * sizeof(WVPITCH);
   if (!m_amemPitch[PITCH_F0].Required (dwNeed)) {
      m_adwPitchSamples[PITCH_F0] = 0;
      return FALSE;
   }
   m_apPitch[PITCH_F0] = (PWVPITCH) m_amemPitch[PITCH_F0].p;
   memset (m_apPitch[PITCH_F0], 0 ,dwNeed);

   // interpolate SR features
   DWORD i;
   for (i = 0; i < m_dwSRSamples; i++) {
      fp fOrig = (fp)i / fStretch;
      DWORD dwLeft = (DWORD)fOrig;
      DWORD dwRight = min(dwLeft+1, lSRFEATURE.Num()-1);
      fOrig -= (fp)dwLeft;
      dwLeft = min(dwLeft, lSRFEATURE.Num()-1);

      SRFEATUREInterpolate (pSROld+dwLeft, pSROld+dwRight, 1.0 - fOrig, m_paSRFeature + i);
   }


   // interpoalte pitch
   for (i = 0; i < m_adwPitchSamples[PITCH_F0]; i++) {
      fp fOrig = (fp)i / fStretch;
      DWORD dwLeft = (DWORD)fOrig;
      DWORD dwRight = min(dwLeft+1, lPitch.Num()-1);
      fOrig -= (fp)dwLeft;
      dwLeft = min(dwLeft, lPitch.Num()-1);

      m_apPitch[PITCH_F0][i].fFreq = (1.0 - fOrig) * pPitchOld[dwLeft].fFreq + fOrig * pPitchOld[dwRight].fFreq;
      m_apPitch[PITCH_F0][i].fStrength = (1.0 - fOrig) * pPitchOld[dwLeft].fStrength + fOrig * pPitchOld[dwRight].fStrength;
   }

#if 0 // def _DEBUG
   // this is a hack to test functionality
   SRDETAILEDPHASE SDP;
   for (i = 0; i < m_dwSRSamples; i++) {
      SRDETAILEDPHASEFromSRFEATURE (m_paSRFeature + i, m_apPitch[PITCH_F0][i].fFreq, &SDP);
      m_apPitch[PITCH_F0][i].fFreq *= 2.0; // will need to try pitch doubling
      SRDETAILEDPHASEToSRFEATURE (&SDP, m_apPitch[PITCH_F0][i].fFreq, m_paSRFeature + i);
   } // i
#endif

   return TRUE;
}


/****************************************************************************
CM3DWave::PaintWords - This draws the phonems onto the HDC.

inputs
   HDC            hDC - To draw on
   RECT           *pr - Rectangle on the HDC
   double         fLeft, fRight - Left and right sides of HDC correspond to these sample
                  numbers. The samples may extend beyond the wave
returns
   none
*/
void CM3DWave::PaintWords (HDC hDC, RECT *pr, double fLeft, double fRight)
{
   if ((pr->right - pr->left <= 0) || (pr->bottom - pr->top <= 0))
      return;  // nothing to draw
   if (fRight <= fLeft + EPSILON)
      return;  // cant to this either

   // create a bitmap to draw this in
   HBITMAP hBit;
   HDC hDCDraw;
   hBit = CreateCompatibleBitmap (hDC, pr->right - pr->left, pr->bottom - pr->top);
   hDCDraw = CreateCompatibleDC (hDC);
   SelectObject (hDCDraw, hBit);

   // blank backgroudn
   RECT rDrawMain;
   rDrawMain.top = rDrawMain.left = 0;
   rDrawMain.right = pr->right - pr->left;
   rDrawMain.bottom = pr->bottom - pr->top;
   HBRUSH hBrush;
   hBrush = CreateSolidBrush (RGB(0xf0, 0xf0, 0xff));
   FillRect (hDCDraw, &rDrawMain, hBrush);
   DeleteObject (hBrush);

   // font
   HFONT hFont;
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -10; 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   hFont = CreateFontIndirect (&lf);
   HFONT hFontOld;
   hFontOld = (HFONT) SelectObject (hDCDraw, hFont);
   SetBkMode (hDCDraw, TRANSPARENT);
   SetTextColor (hDCDraw, RGB(0,0,0));

   // line for separator
   SelectObject (hDCDraw, GetStockObject (BLACK_PEN));


   // loop through all the words
   int i;
   PWVWORD pp, pp2;
   DWORD dwNum;
   dwNum = m_lWVWORD.Num();
   for (i = -1; i < (int)dwNum; i++) {
      // start and end pixel for word, as well as phoneme descriptor
      double fStart, fEnd;
      PWSTR psz = NULL;
      if (i < 0) {
         fStart = 0;
         pp = (PWVWORD) m_lWVWORD.Get(0);
         fEnd = (dwNum ? pp->dwSample : m_dwSamples);
         psz = NULL;
      }
      else {
         pp = (PWVWORD) m_lWVWORD.Get(i);
         pp2 = (PWVWORD) m_lWVWORD.Get(i+1);
         fStart = pp->dwSample;
         fEnd = ((i+1 < (int)dwNum) ? pp2->dwSample : m_dwSamples);
         psz = (PWSTR)(pp+1);;
      }

      // if out of view then dont bother
      if ((fEnd <= fLeft) || (fStart >= fRight))
         continue;

      // convert this to pixels
      fEnd = (fEnd - fLeft) / (fRight - fLeft) * (double)(rDrawMain.right - rDrawMain.left);
      fStart = (fStart - fLeft) / (fRight - fLeft) * (double)(rDrawMain.right - rDrawMain.left);

      // keep within screen boundaries, but out just enough so edge line doenst draw
      fEnd = min(fEnd, rDrawMain.right+2);
      fStart = max(fStart, rDrawMain.left - 2);

      // draw lines
      MoveToEx (hDCDraw, (int)fStart, rDrawMain.top, NULL);
      LineTo (hDCDraw, (int)fStart, rDrawMain.bottom+1);
      MoveToEx (hDCDraw, (int)fEnd, rDrawMain.top, NULL);
      LineTo (hDCDraw, (int)fEnd, rDrawMain.bottom+1);

      // figure out text strings
      char szLong[256];
      szLong[0] = 0;
      if (psz)
         WideCharToMultiByte (CP_ACP, 0, psz, -1, szLong, sizeof(szLong), 0, 0);

      // see if they fit
      RECT rDraw;
      int iLen;
      rDraw = rDrawMain;
      rDraw.left = (int)fStart;
      rDraw.right = (int)fEnd;
      iLen = rDraw.right - rDraw.left - 2;
      DrawText (hDCDraw, szLong, -1, &rDraw, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
   }

   // free font
   SelectObject (hDCDraw, hFontOld);
   DeleteObject (hFont);

   // draw
   BitBlt (hDC, pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top,
      hDCDraw, 0, 0, SRCCOPY);
   DeleteDC (hDCDraw);
   DeleteObject (hBit);
}


/****************************************************************************
CM3DWave::WordDelete - Internal function that deletes the Words between
time start and time end, moving all Words after time end up.

inputs
   DWORD          dwStart - start time
   DWORD          dwEnd - end time
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::WordDelete (DWORD dwStart, DWORD dwEnd)
{
   DWORD i;
   for (i = 0; i < m_lWVWORD.Num(); i++) {
      PWVWORD pp = (PWVWORD) m_lWVWORD.Get(i);
      if (pp->dwSample <= dwStart)
         continue; // too low
      if (pp->dwSample > dwEnd) {
         // just decrease location
         pp->dwSample -= (dwEnd - dwStart);
         continue;
      }

      // else, deleting the section that contains the word, so se it's time to dwStart
      pp->dwSample = dwStart;

      // if this time matches the last time then get rid of the previous one
      PWVWORD pp2;
      if (i)
         pp2 = (PWVWORD) m_lWVWORD.Get(i-1);
      if (i && (pp->dwSample <= pp2->dwSample)) {
         m_lWVWORD.Remove (i-1);
         i--;
         continue;
      }
   } // i

   // get rid of duplicates
   WordRemoveDup ();

   return TRUE;
}

/****************************************************************************
CM3DWave::WordInsert - Inserts a number of words.

inputs
   DWORD          dwInsert - Intertion time
   DWORD          dwTime - Amount of time to insert
   DWORD          dwFrom - Subtract this time from the phoneems in plWVWORDFrom
   PCListViarable    plWVWORDFrom - List to get WORDs from
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::WordInsert (DWORD dwInsert, DWORD dwTime, DWORD dwFrom, PCListVariable plWVWORDFrom)
{
   BYTE     abHuge[512];
   PWVWORD  pNewWord = (PWVWORD)&abHuge[0];
   PWSTR    pNewString = (PWSTR)(pNewWord+1);

   // if there are no words make a silenct word
   if (!m_lWVWORD.Num()) {
      pNewWord->dwSample = 0;
      pNewString[0] = 0;
      m_lWVWORD.Add (pNewWord, sizeof(WVWORD) + sizeof(WCHAR)*(wcslen(pNewString)+1) );
   }

   // move time points
   DWORD i, dwInsertBefore;
   dwInsertBefore = -1;
   for (i = 0; i < m_lWVWORD.Num(); i++) {
      PWVWORD pp = (PWVWORD) m_lWVWORD.Get(i);
      if (pp->dwSample < dwInsert)
         continue;

      // else, after
      pp->dwSample += dwTime;

      // remember this if it's the first sample to insert before
      if (dwInsertBefore == -1)
         dwInsertBefore = i;
   }

   // duplicate the WORD just before dwInsert before to dwInsert before so
   // that will get WORDs split
   if ((dwInsertBefore != -1) && dwInsertBefore) {
      // dont worry about duplicates because they'll be removed
      PWVWORD pp = (PWVWORD) m_lWVWORD.Get(dwInsertBefore-1);
      memcpy (abHuge, pp, m_lWVWORD.Size(dwInsertBefore-1));
      pNewWord->dwSample = dwInsert + dwTime;
      m_lWVWORD.Insert (dwInsertBefore, pNewWord,  sizeof(WVWORD) + sizeof(WCHAR)*(wcslen(pNewString)+1));
   }
   else if (dwInsertBefore == 0) {
      // inserting before silence
      pNewString[0] = 0;
      pNewWord->dwSample = dwInsert + dwTime;
      m_lWVWORD.Insert (dwInsertBefore, pNewWord,  sizeof(WVWORD) + sizeof(WCHAR)*(wcslen(pNewString)+1));
   }
   else {   // dwInsertBefore=-1
      // adding onto the end, so need to put silence afterwards
      pNewString[0] = 0;
      pNewWord->dwSample = dwInsert + dwTime;
      m_lWVWORD.Add (pNewWord,  sizeof(WVWORD) + sizeof(WCHAR)*(wcslen(pNewString)+1));
      dwInsertBefore = m_lWVWORD.Num()-1;
   }

   // insert a silence before dwInsertBefore because that is implied
   pNewString[0] = 0;
   pNewWord->dwSample = dwInsert;
   if (dwInsertBefore != -1) {
      m_lWVWORD.Insert (dwInsertBefore, pNewWord,  sizeof(WVWORD) + sizeof(WCHAR)*(wcslen(pNewString)+1));
      dwInsertBefore++; // so inserting after silnece
   }
   else
      // must be inserting adding onto the end, so add silence at insertion point
      m_lWVWORD.Add (pNewWord,  sizeof(WVWORD) + sizeof(WCHAR)*(wcslen(pNewString)+1));

   // insert all the WORDs
   for (i = 0; i < plWVWORDFrom->Num(); i++) {
      PWVWORD pp = (PWVWORD) plWVWORDFrom->Get(i);
      memcpy (abHuge, pp, plWVWORDFrom->Size(i));
      if (pNewWord->dwSample < dwFrom)
         pNewWord->dwSample = 0;   // copying before this, so scrunch up in beginning
      else
         pNewWord->dwSample -= dwFrom;
      if (pNewWord->dwSample >= dwTime)
         break;   // occurs after the time want to insert, so might as well stop here
      pNewWord->dwSample += dwInsert;

      if (dwInsertBefore != -1) {
         m_lWVWORD.Insert (dwInsertBefore, pNewWord,  sizeof(WVWORD) + sizeof(WCHAR)*(wcslen(pNewString)+1));
         dwInsertBefore++; // so inserting after silnece
      }
      else
         m_lWVWORD.Add (pNewWord,  sizeof(WVWORD) + sizeof(WCHAR)*(wcslen(pNewString)+1));
   }

   // finally
   WordRemoveDup();
   return TRUE;
}

/****************************************************************************
CM3DWave::WordStretch - Stretches the word time up/down by the
given percent

inputs
   double         fStretch - if 1.0 no change, else 2.0 double the time, etc.
returns
   none
*/
void CM3DWave::WordStretch (double fStretch)
{
   if (fStretch == 1.0)
      return;  // no change
   DWORD i;
   for (i = 0; i < m_lWVWORD.Num(); i++) {
      PWVWORD pp = (PWVWORD) m_lWVWORD.Get(i);
      pp->dwSample = (DWORD)((double)pp->dwSample * fStretch);
   }

   // finally
   WordRemoveDup();
}


/****************************************************************************
CM3DWave::WordRemoveDup - Internal function that removes duplicate words
occurring one after the other.
*/
void CM3DWave::WordRemoveDup (void)
{
   DWORD i;
   for (i = m_lWVWORD.Num() - 1; i && (i < m_lWVWORD.Num()); i--) {
      PWVWORD pp = (PWVWORD) m_lWVWORD.Get(i);
      PWVWORD pp2 = (PWVWORD) m_lWVWORD.Get(i-1);

      // if smae WORD remove
      if (!_wcsicmp ((PWSTR)(pp+1), (PWSTR)(pp2+1))) {
         m_lWVWORD.Remove(i);
         // BUGFIX - Dont do: i--;
         continue;
      }

      if (pp->dwSample <= pp2->dwSample) {
         // happening at the same time
         m_lWVWORD.Remove(i-1);
         // BUGFIX - Dont to: i--;
         continue;
      }
   } // i

   // if the first WORD is silence then remove that since redundant
   PWVWORD pp = (PWVWORD) m_lWVWORD.Get(0);
   if (m_lWVWORD.Num() && !(((PWSTR)(pp+1))[0]))
      m_lWVWORD.Remove(0);

}


/**********************************************************************************
CM3DWave::RecordBitmap - Creates a bitmap based on the wave data.

inputs
   short          *psWave - Wave data
   DWORD          dwSamples - Number of samples
   DWORD          dwChannels - Number of channels
   HDC            hDC - DC
   short          *psCache - Cache of RECORDWIDTH samples, from previous use
   PVOID          priVoid - Recordingo
returns
   HBITMAP - bitmap
*/
HBITMAP CM3DWave::RecordBitmap (short *pszWave, DWORD dwSamples, DWORD dwChannels, HDC hDC, short *psCache,
                                 PVOID priVoid)
{
   PRECORDINFO pri = (PRECORDINFO) priVoid;
   HBITMAP hBit = CreateCompatibleBitmap (hDC, RECORDWIDTH, RECORDHEIGHT);
   if (!hBit)
      return NULL;

   CMem memFFTScratch;
   CSinLUT SinLUT;

   HDC hDCDraw;
   hDCDraw = CreateCompatibleDC (hDC);
   SelectObject (hDCDraw, hBit);

   // see if it clips
   BOOL fClip;
   DWORD i;
   fClip = FALSE;
   for (i = 0; i < dwSamples * dwChannels; i++)
      if ((pszWave[i] > 30000) || (pszWave[i] < -30000))
         fClip = TRUE;

   RECT r;
   r.top = r.left = 0;
   r.right = RECORDWIDTH;
   r.bottom = RECORDHEIGHT;
   if (fClip) {
      HBRUSH hbr = CreateSolidBrush (RGB(0x80,0,0));
      FillRect (hDCDraw, &r, hbr);
      DeleteObject (hbr);

      if (pri->fRecording)
         pri->fClippedDuringRecord = TRUE;
   }
   else
      FillRect (hDCDraw, &r, (HBRUSH) GetStockObject (BLACK_BRUSH));


   // do FFT?
   if (pri->dwWindowSize <= dwSamples) {
      int i;
      DWORD dwScale, dwChan;
      dwScale = dwChannels;
      for (i = 0; i < (int)pri->dwWindowSize; i++) {
         short sValue;
         sValue = 0;
         for (dwChan = 0; dwChan < dwChannels; dwChan++)
            sValue += pszWave[i*dwChannels + dwChan] / (short)dwScale;

         // store awway, along with the window
         pri->pfFFTScratch[i] = (float)sValue * pri->pfWindow[i];
      }

      // do the FFT
      FFTRecurseReal (pri->pfFFTScratch - 1, pri->dwWindowSize, 1, &SinLUT, &memFFTScratch);

      // scale and write away
      float   fScale = 2.0 / (float) pri->dwWindowSize;// * 32768.0 / 32768.0;
      float   fScaleSqr = fScale * fScale;
      float fVal;
      HPEN hPen, hPenOld;
      hPen = CreatePen (PS_SOLID, 0, RGB(0,0,0x80));
      hPenOld = (HPEN) SelectObject (hDCDraw, hPen);
      for (i = 0; i < ((int)pri->dwWindowSize/2); i++) {
         if (i >= RECORDWIDTH)
            break;

         fVal = (float) sqrt(
            (pri->pfFFTScratch[i*2] * pri->pfFFTScratch[i*2] + pri->pfFFTScratch[i*2+1] * pri->pfFFTScratch[i*2+1]) * fScaleSqr );
         fVal = max(.10, fVal);
         fVal = log10 (fVal / 32767.0) * 20;
         fVal = (fVal / 96 + 1); // so it ranges between 0 and 1
         fVal = (1.0 - fVal) * RECORDHEIGHT;

         // draw box
         MoveToEx (hDCDraw, i, (int) fVal, NULL);
         LineTo (hDCDraw, i, RECORDHEIGHT+1);
      }
      SelectObject (hDCDraw, hPenOld);
      DeleteObject (hPen);

   }

   DWORD dwSkip;
   CListFixed lCross;
   dwSkip = 0;
   lCross.Init (sizeof(DWORD));
   for (i = 1; i+RECORDWIDTH < dwSamples; i++) {
      if ((pszWave[i*dwChannels] > 0) && (pszWave[(i-1)*dwChannels] < 0))
         lCross.Add (&i);
   }
   if (lCross.Num()) {
      // find the best score
      DWORD *padw = (DWORD*) lCross.Get(0);
      DWORD dwBest = -1;
      DWORD j;
      int iBestScore = -1, iScore;
      for (i = 0; i < lCross.Num(); i++, padw++) {
         // score
         iScore = 0;
         for (j = 0; j < RECORDWIDTH; j++) {
            iScore += abs((int) pszWave[(padw[0]+j)*dwChannels] - (int) psCache[j]);
         } // j

         if ((dwBest == -1) || (iScore < iBestScore)) {
            dwBest = i;
            iBestScore = iScore;
            dwSkip = padw[0];
         }
      } // i
   } // lcross.num

   // draw wave
   HPEN hPen, hPenOld;
   short sVal;
   int iY;
   DWORD dwChan;
   for (dwChan = 0; dwChan < dwChannels; dwChan++) {
      COLORREF crPen;
      crPen = RGB(0xff, 0xff, 0xff);
      if ((dwChan < dwChannels) && (dwChannels >= 2))
         switch (dwChan % 6) {
         case 0:
            crPen = RGB(0xff,0,0);
            break;
         case 1:
            crPen = RGB(0,0xff,0);
            break;
         case 2:
            crPen = RGB(0,0,0xff);
            break;
         case 3:
            crPen = RGB(0xff,0xff,0);
            break;
         case 4:
            crPen = RGB(0,0xff,0xff);
            break;
         case 5:
            crPen = RGB(0xff,0,0xff);
            break;
         }

      hPen = CreatePen (PS_SOLID, 2, crPen);
      hPenOld = (HPEN) SelectObject (hDCDraw, hPen);

      for (i = 0; i < RECORDWIDTH; i++) {
         sVal = pszWave[(dwSkip+i) * dwChannels + dwChan];

         // keep track of this for next time
         if (!dwChan)
            psCache[i] = sVal;

         iY = (((int) sVal + 32768) * RECORDHEIGHT) / 0x10000;
         if (i)
            LineTo (hDCDraw, i, iY);
         else
            MoveToEx (hDCDraw, i, iY, NULL);
      } // i
      SelectObject (hDCDraw, hPenOld);
      DeleteObject (hPen);
   } // dwChan

   // if clipped during recording show text to indicate so
   if (pri->fClippedDuringRecord || pri->fRecording) {
      // draw the text
      HFONT hFont, hFontOld;
      LOGFONT  lf;
      memset (&lf, 0, sizeof(lf));
      lf.lfHeight = -MulDiv(24, GetDeviceCaps(hDCDraw, LOGPIXELSY), 72); 
      lf.lfCharSet = DEFAULT_CHARSET;
      lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
      strcpy (lf.lfFaceName, "Arial");
      hFont = CreateFontIndirect (&lf);
      hFontOld = (HFONT) SelectObject (hDCDraw, hFont);
      SetTextColor (hDCDraw, RGB(0xff,0xff,0));
      SetBkMode (hDCDraw, TRANSPARENT);
      DrawText(hDCDraw,
         pri->fClippedDuringRecord ? "Clipped while recording!" : "Recording...",
         -1, &r, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
      SelectObject (hDCDraw, hFontOld);
      DeleteObject (hFont);
   }

   DeleteDC (hDCDraw);
   return hBit;
}


/**********************************************************************************
CM3DWave::RecordCallback - Handles a call from the waveInOpen function
*/
static void CALLBACK RecordCallback (
  HWAVEIN hwi,       
  UINT uMsg,         
  DWORD_PTR dwInstance,  
  DWORD_PTR dwParam1,    
  DWORD_PTR dwParam2     
)
{
   PRECORDINFO pri = (PRECORDINFO) dwInstance;
   if (pri->pM3DWave)
      pri->pM3DWave->RecordCallback (pri, hwi, uMsg, dwParam1, dwParam2);
}
void CM3DWave::RecordCallback (PVOID  priVoid, HWAVEIN hwi,UINT uMsg,         
  DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
   PRECORDINFO pri = (PRECORDINFO) priVoid;

   // do something with the buffer
   if (uMsg == MM_WIM_DATA) {
      // BUGFIX - Wrap around critical section so dont reenter self
      EnterCriticalSection (&pri->csStopped);

      if (!pri->fStopped) {
         PWAVEHDR pwh = (WAVEHDR*) dwParam1;
         // Update the bitmap
         if (pri->hBit)
            DeleteObject (pri->hBit);
         pri->hBit = NULL;
         HDC hDC;
         hDC = GetDC (pri->hWndDC);
         pri->hBit = RecordBitmap ((short*) pwh->lpData, pwh->dwBytesRecorded / 2 / m_dwChannels,
            m_dwChannels, hDC, pri->asCache, pri);
         ReleaseDC (pri->hWndDC, hDC);

         // store away the audio
         if (pri->fRecording && pwh->dwBytesRecorded) {
            pri->plAudio->Add (pwh->lpData, pwh->dwBytesRecorded);
         }

         // send it out again
         waveInAddBuffer (pri->hWaveIn, pwh, sizeof(WAVEHDR));
      }
      else {
#if 0 // def _DEBUG
         OutputDebugString ("WOC ");
#endif
         pri->dwWaveOut--;
      }

      // BUGFIX - Wrap around critical section so dont reenter self
      LeaveCriticalSection (&pri->csStopped);

   }
}


/*********************************************************************************
RecordPage */

static BOOL RecordPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PRECORDINFO pri = (PRECORDINFO) pPage->m_pUserData;
   static WCHAR sTextureTemp[16];

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // remember this
         pri->pPage = pPage;

         // BUGFIX - Move the start to here. Otherwise seems that sometimes the
         // scope bitmap doesnt exist
         waveInStart (pri->hWaveIn);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"trimsilence");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pri->fTrimSilence);
         pControl = pPage->ControlFind (L"normalize");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pri->fNormalize);
         pControl = pPage->ControlFind (L"noisereduce");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pri->fNoiseReduce);
         pControl = pPage->ControlFind (L"removedc");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pri->fRemoveDC);

         pControl = NULL;
         if (pri->dwReplace == 0)
            pControl = pPage->ControlFind (L"replacesel");
         else if (pri->dwReplace == 1)
            pControl = pPage->ControlFind (L"replaceall");
         else if (pri->dwReplace == 2)
            pControl = pPage->ControlFind (L"replaceend");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // create a timer
         pri->dwTimerID = pPage->m_pWindow->TimerSet (100, pPage);
      }
      break;

   case ESCM_TIMER:
      // display this
      EnterCriticalSection (&pri->csStopped);
      if (pri->hBit) {
         WCHAR szTemp[32];
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"image");
         swprintf (szTemp, L"%lx", (__int64) pri->hBit);
         if (pControl)
            pControl->AttribSet (L"hbitmap", szTemp);
      }
      LeaveCriticalSection (&pri->csStopped);
      break;

   case ESCM_DESTRUCTOR:
      // kill the timer
      pPage->m_pWindow->TimerKill (pri->dwTimerID);

      // BUGFIX - Wrap around critical section so dont reenter self
      EnterCriticalSection (&pri->csStopped);
      pri->fStopped = TRUE;
      LeaveCriticalSection (&pri->csStopped);

#if 0 // def _DEBUG
      OutputDebugString ("\r\nWaveInReset: ");
#endif
      waveInReset (pri->hWaveIn);
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // absorb this, but send stop message
         // BUGFIX - Wrap around critical section so dont reenter self
         EnterCriticalSection (&pri->csStopped);
         pri->fStopped = TRUE;
         LeaveCriticalSection (&pri->csStopped);
#if 0 // def _DEBUG
         OutputDebugString ("\r\nWaveInReset: ");
#endif
         waveInReset (pri->hWaveIn);

         // BUGFIX - If it has a "p:" at the start then don't absorb, since is
         // used elswhere
         if (p->psz && (p->psz[0] == L'p')  && (p->psz[1] == L':'))
            break;
      }
      return TRUE;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"oops")) {
            EnterCriticalSection (&pri->csStopped);
            pri->fRecording = FALSE;
            pri->plAudio->Clear();
            pri->fClippedDuringRecord = FALSE;  // just so can restart
            LeaveCriticalSection (&pri->csStopped);

            p->pControl->Enable (FALSE);

         }
         else if (!_wcsicmp(psz, L"record")) {
            PCEscControl pOops = pPage->ControlFind (L"oops");

            EnterCriticalSection (&pri->csStopped);
            if (!pri->fRecording) {
               // not recording so start
               pri->fRecording = TRUE;
               pri->fClippedDuringRecord = FALSE;  // just so can restart
               LeaveCriticalSection (&pri->csStopped);

               if (pOops)
                  pOops->Enable (TRUE);
            }
            else {
               // BUGFIX - Wrap around critical section so dont reenter self
               pri->fStopped = TRUE;
               pri->fRecording = FALSE;
               LeaveCriticalSection (&pri->csStopped);
#if 0 // def _DEBUG
               OutputDebugString ("\r\nWaveInReset: ");
#endif
               waveInReset (pri->hWaveIn);

               if (pOops)
                  pOops->Enable (FALSE);

               if (pri->fClippedDuringRecord) {
                  if (IDYES != pPage->MBYesNo (L"The audio clipped while recording.",
                     L"The recording won't sound as good as it could. Are you sure you "
                     L"wish to continue?")) {

                     // BUGFIX - Wrap around critical section so dont reenter self
                     EnterCriticalSection (&pri->csStopped);
                     pri->fStopped = FALSE;

                     pri->plAudio->Clear();
                     pri->fClippedDuringRecord = FALSE;  // just so can restart
                     LeaveCriticalSection (&pri->csStopped);

                     DWORD i;
                     MMRESULT mm;
                     for (i = 0; i < RECORDBUF; i++) {
                        pri->dwWaveOut++;
                        mm = waveInAddBuffer (pri->hWaveIn, &pri->aWaveHdr[i], sizeof(WAVEHDR));
                     }
                     waveInStart (pri->hWaveIn);

                     return TRUE;
                     }
               }
               pPage->Exit (Back());
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"trimsilence")) {
            pri->fTrimSilence = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"normalize")) {
            pri->fNormalize = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"noisereduce")) {
            pri->fNoiseReduce = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"removedc")) {
            pri->fRemoveDC = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"replacesel")) {
            pri->dwReplace = 0;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"replaceall")) {
            pri->dwReplace = 1;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"replaceend")) {
            pri->dwReplace = 2;
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Record";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            EnterCriticalSection (&pri->csStopped);
            swprintf (sTextureTemp, L"%lx", (_int64) pri->hBit);
            LeaveCriticalSection (&pri->csStopped);
            p->pszSubString = sTextureTemp;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSHOWREPLACE")) {
            p->pszSubString = pri->fShowReplaceButtons ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSHOWREPLACE")) {
            p->pszSubString = pri->fShowReplaceButtons ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RECSTRING")) {
            PWSTR psz = (PWSTR) pri->pM3DWave->m_memSpoken.p;
            if (psz && psz[0])
               p->pszSubString = psz;
            else
               p->pszSubString = L"(Nothing specified)";
            return TRUE;
         }
      }
      break;

   }

   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CM3DWave::RecordAdvanced - Pulls up the record UI, but allows you
to provide your own CEscWindow, as well as resource for the page display.

inputs
   PCEscWindow    pWindow - Window to display in
   DWORD          dwResource - Resource number to use
   HINSTANCE      hInstance - Instance to get the resource from
   DWORD          *pdwReplace - Filled in with the replace settings chosen by
                  the user. 0 to replace the selection, 1 to add to the end of the
                  wave, 2 for the entire wave.  Can be NULL, in which case
                  no record-over options.
   PWSTR          pszLink - The link that was returned by the record
                  dialog is copied into here. Can be NULL.
returns
   PCM3DWave - Recorded wave. If none was recorded, returns NULL.
*/
PCM3DWave CM3DWave::RecordAdvanced (PCEscWindow pWindow, DWORD dwResource, HINSTANCE hInstance,
                                    DWORD *pdwReplace, PWSTR pszLink)
{
   // if resource not set then initialize
   if (!dwResource) {
      dwResource = IDR_MMLRECORD;
      hInstance = ghInstance;
   }

   // clear out
   if (pszLink)
      pszLink[0] = 0;
   if (pdwReplace)
      *pdwReplace = 0;

   // allocate a structure that has all the record info ifn it
   RECORDINFO ri;
   CListVariable lAudio;
   memset (&ri, 0, sizeof(ri));
   ri.pM3DWave = this;
   ri.hWndDC = pWindow->m_hWnd;
   ri.dwWindowSize = WindowSize();
   ri.plAudio = &lAudio;
   ri.fTrimSilence = ri.fNormalize = ri.fNoiseReduce = ri.fRemoveDC = TRUE;
   ri.dwReplace = 2;
   ri.fShowReplaceButtons = (pdwReplace ? TRUE : FALSE);
   InitializeCriticalSection (&ri.csStopped);

   // create the space for the window
   CMem  memWindow;
   if (!memWindow.Required (ri.dwWindowSize * sizeof(float) * 3)) {
      DeleteCriticalSection (&ri.csStopped);
      return NULL;
   }
   ri.pfWindow = (float*) memWindow.p;
   ri.pfFFTScratch = ri.pfWindow + ri.dwWindowSize;
   CreateFFTWindow (3, ri.pfWindow, ri.dwWindowSize);

   // allocate memory to write to
   CMem  memWave;
   DWORD dwSingleBuf;
   dwSingleBuf = (DWORD) (m_dwSamplesPerSec / 8) * m_dwChannels * sizeof(short);
   if (!memWave.Required (dwSingleBuf * RECORDBUF)) {
      DeleteCriticalSection (&ri.csStopped);
      return NULL;  // unexpected error
   }
   memset (memWave.p, 0, dwSingleBuf * RECORDBUF);


   // open the wave device
   MMRESULT mm;
   WAVEFORMATEX WFEX;
   memset (&WFEX, 0, sizeof(WFEX));
   WFEX.cbSize = 0;
   WFEX.wFormatTag = WAVE_FORMAT_PCM;
   WFEX.nChannels = m_dwChannels;
   WFEX.nSamplesPerSec = m_dwSamplesPerSec;
   WFEX.wBitsPerSample = 16;
   WFEX.nBlockAlign  = WFEX.nChannels * WFEX.wBitsPerSample / 8;
   WFEX.nAvgBytesPerSec = WFEX.nBlockAlign * WFEX.nSamplesPerSec;
   mm = waveInOpen (&ri.hWaveIn, WAVE_MAPPER, &WFEX, (DWORD_PTR) ::RecordCallback,
      (DWORD_PTR) &ri, CALLBACK_FUNCTION);
   if (mm) {
      if (pszLink)
         wcscpy (pszLink, L"NoSoundCard");

      DeleteCriticalSection (&ri.csStopped);
      return NULL;
   }

   // prepare all the headers
   DWORD i;
   for (i = 0; i < RECORDBUF; i++) {
      ri.aWaveHdr[i].dwBufferLength = dwSingleBuf;
      ri.aWaveHdr[i].lpData = (LPSTR) ((PBYTE) memWave.p + i * dwSingleBuf);
      mm = waveInPrepareHeader (ri.hWaveIn, &ri.aWaveHdr[i], sizeof(WAVEHDR));
   }

   // create the bitmap
   HDC hDC;
   hDC = GetDC (ri.hWndDC);
   ri.hBit = RecordBitmap ((short*) ri.aWaveHdr[0].lpData, ri.aWaveHdr[0].dwBufferLength / 2 / m_dwChannels,
      m_dwChannels, hDC, ri.asCache, &ri);
   if (!ri.hBit)
      ri.hBit = NULL;
   ReleaseDC (ri.hWndDC, hDC);

   // add all the buffers
   for (i = 0; i < RECORDBUF; i++) {
      ri.dwWaveOut++;
      mm = waveInAddBuffer (ri.hWaveIn, &ri.aWaveHdr[i], sizeof(WAVEHDR));
   }

   // BUGFIX - Move the start until after window diplayed


   // display the window
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (hInstance, dwResource, RecordPage, &ri);
   if (pszRet && pszLink)
      wcscpy (pszLink, pszRet);





   // set some flags so if get callback wont respond
   ri.pM3DWave = NULL;
   ri.pPage = NULL;
   if (ri.hBit) {
      DeleteObject (ri.hBit);
      ri.hBit = NULL;
   }

   // unprepare all the headers
   for (i = 0; i < RECORDBUF; i++)  // BUGFIX - missing loop
      waveInUnprepareHeader (ri.hWaveIn, &ri.aWaveHdr[i], sizeof(WAVEHDR));
   waveInClose (ri.hWaveIn);

   // how large
   size_t dwSize;
   dwSize = 0;
   for (i = 0; i < lAudio.Num(); i++)
      dwSize += lAudio.Size(i);
   if (!dwSize) {
      DeleteCriticalSection (&ri.csStopped);
      return NULL;  // nothing recorded
   }

   // create a wave object and fill it up
   PCM3DWave pNewWave;
   pNewWave = new CM3DWave;
   if (!pNewWave) {
      DeleteCriticalSection (&ri.csStopped);
      return NULL;  // error
   }
   if (!pNewWave->New (m_dwSamplesPerSec, m_dwChannels, (DWORD)dwSize / (sizeof(short)*m_dwChannels))) {
      delete pNewWave;
      DeleteCriticalSection (&ri.csStopped);
      return NULL;  // error
   }

   // copy over
   PBYTE pTo;
   pTo = (PBYTE) pNewWave->m_psWave;
   for (i = 0; i < lAudio.Num(); i++) {
      dwSize = lAudio.Size(i);
      memcpy (pTo, lAudio.Get(i), dwSize);
      pTo += dwSize;
   }

   // free up list contents
   lAudio.Clear();

   if (ri.fNormalize || ri.fNoiseReduce || ri.fTrimSilence || ri.fRemoveDC) {
      CProgress Progress;
      Progress.Start (pWindow->m_hWnd, "Processing...");

      if (ri.fRemoveDC) {
         Progress.Push (0, .3);
         pNewWave->FXRemoveDCOffset (TRUE, &Progress);
         Progress.Pop();
      }

      if (ri.fNormalize) {
         Progress.Push (.4, .5);
         short sMax = pNewWave->FindMax();
         fp f;
         sMax = max(1,sMax);
         f = 32767.0 / (fp)sMax;
         pNewWave->FXVolume (f, f, &Progress);
         Progress.Pop();
      }

      if (ri.fNoiseReduce) {
         Progress.Push (.5, .9);
         pNewWave->FXNoiseReduce (&Progress);
         Progress.Pop();
      }

      if (ri.fTrimSilence) {
         Progress.Push (.9, 1);
         pNewWave->FXTrimSilence (&Progress);
         Progress.Pop();
      }
   }

   if (pdwReplace)
      *pdwReplace = ri.dwReplace;
   DeleteCriticalSection (&ri.csStopped);
   return pNewWave;
}


/**********************************************************************************
CM3DWave::Record - Brings up the record UI dialog and records the audio.

inputs
   HWND           hWnd - to display on top of
   DWORD          *pdwReplace - Filled in with the replace settings chosen by
                  the user. 0 to replace the selection, 1 to add to the end of the
                  wave, 2 for the entire wave.  Can be NULL, in which case
                  no record-over options.
returns
   PCM3DWave - Recorded wave
*/
PCM3DWave CM3DWave::Record (HWND hWnd, DWORD *pdwReplace)
{
   // display the window
   CEscWindow cWindow;
   RECT r;
   WCHAR szLink[256];
   DialogBoxLocation2 (hWnd, &r);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT, &r);
   PCM3DWave pRet = RecordAdvanced (&cWindow, IDR_MMLRECORD, ghInstance, pdwReplace, szLink);

   if (!_wcsicmp(szLink, L"NoSoundCard"))
      EscMessageBox (hWnd, ASPString(),
         L"Recording couldn't be started.",
         L"You may not have a sound card, or it might be used by another application.",
         MB_ICONEXCLAMATION | MB_OK);
      
   return pRet;
}


/**********************************************************************************
CM3DWave::Allocate - Allocate enough memory for the required number of samples.
This also affects pitch and srfeatures. If the wave is expanded, then the end-bits
will be garbage.

inputs
   DWORD          dwSamples - Number of samples
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::Allocate (DWORD dwSamples)
{
   DWORD dwNeed;
   dwNeed = dwSamples * m_dwChannels * sizeof(short);
   if (!m_pmemWave->Required (dwNeed))
      return FALSE;
   m_psWave = (short*) m_pmemWave->p;
   m_dwSamples = dwSamples;

   // SRFeatures get expanded if they exist
   if (m_paSRFeature && m_pmemSR) {
      // m_dwSRSkip already calculated
      m_dwSRSamples = m_dwSamples / m_dwSRSkip + 1;

      dwNeed = m_dwSRSamples * sizeof (SRFEATURE);
      if (!m_pmemSR->Required (dwNeed)) {
         m_dwSRSamples = 0;
         m_paSRFeature = NULL;
         return FALSE;
      }
      m_paSRFeature = (PSRFEATURE) m_pmemSR->p;
   }

   DWORD dwPitchSub;
   for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++)
      if (m_apPitch[dwPitchSub]) {
         m_adwPitchSamples[dwPitchSub] = m_dwSamples / m_dwSRSkip + 1;;
         // m_adwPitchSkip[PITCH_F0] should already be set
         dwNeed = m_adwPitchSamples[dwPitchSub] * m_dwChannels * sizeof(WVPITCH);
         if (!m_amemPitch[dwPitchSub].Required (dwNeed)) {
            m_adwPitchSamples[dwPitchSub] = 0;
            m_apPitch[dwPitchSub] = NULL;
            return FALSE;
         }
         m_apPitch[dwPitchSub] = (PWVPITCH) m_amemPitch[dwPitchSub].p;
      }

   // no energy samples
   m_dwEnergySamples = 0;

   // leave m_lWVPHONEME and m_lWVWORD

   return TRUE;
}

/**********************************************************************************
CM3DWave::BlankWaveToSize - This blanks out the entire wave, extending (or shrinking)
its size to the given number of samples. This is used to produce a SR voice.

inputs
   DWORD          dwSamples - Number of samples
   BOOL           fBlankPitch - If TRUE then blank the pitch information too
returns
   BOOL - TRUE if sueccess
*/
BOOL CM3DWave::BlankWaveToSize (DWORD dwSamples, BOOL fBlankPitch)
{
   DWORD dwNeed;
   dwNeed = dwSamples * m_dwChannels * sizeof(short);
   if (!m_pmemWave->Required (dwNeed))
      return FALSE;
   m_psWave = (short*) m_pmemWave->p;
   m_dwSamples = dwSamples;
   memset (m_psWave, 0, dwNeed);

   BlankSRFeatures ();

   // pitch
   DWORD dwPitchSub;
   if (fBlankPitch) for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++) {
      m_adwPitchSamples[dwPitchSub] = m_dwSRSamples;
      m_adwPitchSkip[dwPitchSub] = m_dwSRSkip;
      dwNeed = m_adwPitchSamples[dwPitchSub] * m_dwChannels * sizeof(WVPITCH);
      if (!m_amemPitch[dwPitchSub].Required (dwNeed)) {
         m_adwPitchSamples[dwPitchSub] = 0;
         return FALSE;
      }
      m_apPitch[dwPitchSub] = (PWVPITCH) m_amemPitch[dwPitchSub].p;
      memset (m_apPitch[dwPitchSub], 0 ,dwNeed);
   }

   // no energy samples
   m_dwEnergySamples = 0;

   // phoneme...
   m_lWVPHONEME.Clear();
   m_lWVWORD.Clear();

   return TRUE;
}



/**********************************************************************************
CM3DWave::AppendPCMAudio - Appends PCM audio onto the end of the wave.
It must be the same sampling rate and channels of the original, and 16-bit.

inputs
   PVOID          pPCM - PCM data
   DWORD          dwSamples - Number of samples
   BOOL           fAlsoSRPitch - If TRUE, SR and pitch will be allocated, but blank
returns
   BOOL - TRUE if sueccess
*/
BOOL CM3DWave::AppendPCMAudio (PVOID pPCM, DWORD dwSamples, BOOL fAlsoSRPitch)
{
   DWORD dwNeed;
   dwNeed = (dwSamples + m_dwSamples) * m_dwChannels * sizeof(short);
   if (!m_pmemWave->Required (dwNeed))
      return FALSE;
   m_psWave = (short*) m_pmemWave->p;
   if (pPCM)
      memcpy (m_psWave + m_dwSamples*m_dwChannels, pPCM, dwSamples * m_dwChannels * sizeof(short));
   else
      memset (m_psWave + m_dwSamples*m_dwChannels, 0, dwSamples * m_dwChannels * sizeof(short));
   m_dwSamples += dwSamples;

   // no energy samples or SR, pitch
   m_dwEnergySamples = 0;

   DWORD dwPitchSub;
   if (fAlsoSRPitch) {
      for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++) {
         DWORD dwOldPitch = m_adwPitchSamples[dwPitchSub] * m_dwChannels * sizeof(WVPITCH);
         m_adwPitchSamples[dwPitchSub] = m_dwSamples / m_adwPitchSkip [dwPitchSub]+ 1;
         DWORD dwNewPitch = m_adwPitchSamples[dwPitchSub] * m_dwChannels * sizeof(WVPITCH);
         if (!m_amemPitch[dwPitchSub].Required (dwNewPitch))
            return FALSE;
         if (dwNewPitch > dwOldPitch)
            memset ((PBYTE)m_amemPitch[dwPitchSub].p + dwOldPitch, 0, dwNewPitch - dwOldPitch);
         m_apPitch[dwPitchSub] = (PWVPITCH) m_amemPitch[dwPitchSub].p;
      } // dwPitchSub

      // SR
      DWORD dwOldSR = m_dwSRSamples * sizeof(SRFEATURE);

      m_dwSRSamples = m_dwSamples / m_dwSRSkip + 1;
      DWORD dwNewSR = m_dwSRSamples * sizeof(SRFEATURE);

      if (!m_pmemSR)
         m_pmemSR = new CMem;
      if (!m_pmemSR)
         return FALSE;
      if (!m_pmemSR->Required (dwNewSR))
         return FALSE;
      if (dwNewSR > dwOldSR)
         memset ((PBYTE)m_pmemSR->p + dwOldSR, 0, dwNewSR - dwOldSR);
      m_paSRFeature = (PSRFEATURE) m_pmemSR->p;

   }
   else {
      m_dwSRSamples = 0;
      memset (m_adwPitchSamples, 0, sizeof(m_adwPitchSamples));
      
      // phoneme...
      m_lWVPHONEME.Clear();
      m_lWVWORD.Clear();
   }

   return TRUE;
}


/**********************************************************************************
CM3DWave::AppendWave - Appends one wave onto to the end of another.

inputs
   PCM3DWave         pAppend - Append this onto the end of the existing wave.
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::AppendWave (PCM3DWave pWave)
{
   // make sure same sampling rate and channels
   if ((pWave->m_dwSamplesPerSec != m_dwSamplesPerSec) || (pWave->m_dwChannels != m_dwChannels))
      return FALSE;

   // if the srskip isn't set then do thiat
   if (pWave->m_dwSRSkip)
      m_dwSRSkip = pWave->m_dwSRSkip;
   DWORD dwPitchSub;
   for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++)
      if (pWave->m_adwPitchSkip[dwPitchSub])
         m_adwPitchSkip[dwPitchSub] = pWave->m_adwPitchSkip[dwPitchSub];
   if (pWave->m_dwSRSAMPLESPERSEC)
      m_dwSRSAMPLESPERSEC = pWave->m_dwSRSAMPLESPERSEC;

   // round m_dwSamples dwon to m_dwSRSkip
   DWORD dwRound = m_dwSamples % m_dwSRSkip;
   if (dwRound)
      m_dwSamples -= dwRound;

   // allocate
   DWORD dwSamplesOrig = m_dwSamples;
   DWORD dwSRSamplesOrig = dwSamplesOrig / m_dwSRSkip;
   DWORD adwPitchSamplesOrig[PITCH_NUM];
   for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++)
      adwPitchSamplesOrig[dwPitchSub] = dwSamplesOrig / m_adwPitchSkip[dwPitchSub];
   if (pWave->m_paSRFeature) {
      if (!m_pmemSR) {
         m_pmemSR = new CMem;
         if (!m_pmemSR || !m_pmemSR->Required (sizeof(SRFEATURE)))
            return FALSE;
         m_paSRFeature = (PSRFEATURE) m_pmemSR->p;
      }
   }
   else {
      if (m_pmemSR)
         delete m_pmemSR;
      m_paSRFeature = NULL;
      m_dwSRSamples = 0;
   }
   for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++)
      if (pWave->m_apPitch[dwPitchSub]) {
         if (!m_amemPitch[dwPitchSub].Required (sizeof(WVPITCH)))
            return FALSE;
         m_apPitch[dwPitchSub] = (PWVPITCH) m_amemPitch[dwPitchSub].p;

         if (m_adwPitchSamples[dwPitchSub] <= 1)
            m_afPitchMaxStrength[dwPitchSub] = 0;
      }
      else {
         m_apPitch[dwPitchSub] = NULL;
         m_adwPitchSamples[dwPitchSub] = 0;
      }
   if (!Allocate (m_dwSamples + pWave->m_dwSamples))
      return FALSE;

   // copy over
   memcpy (m_psWave + (dwSamplesOrig * m_dwChannels), pWave->m_psWave, pWave->m_dwSamples * m_dwChannels * sizeof(short));
   if (m_paSRFeature && pWave->m_paSRFeature)
      memcpy (m_paSRFeature + dwSRSamplesOrig, pWave->m_paSRFeature, pWave->m_dwSRSamples * sizeof(SRFEATURE));
   for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++)
      if (m_apPitch[dwPitchSub] && pWave->m_apPitch[dwPitchSub]) {
         memcpy (m_apPitch[dwPitchSub] + adwPitchSamplesOrig[dwPitchSub] * m_dwChannels,
            pWave->m_apPitch[dwPitchSub], pWave->m_adwPitchSamples[dwPitchSub] * m_dwChannels * sizeof(WVPITCH));
         m_afPitchMaxStrength[dwPitchSub] = max(m_afPitchMaxStrength[dwPitchSub], pWave->m_afPitchMaxStrength[dwPitchSub]);

         // BUGFIX - Make sure the pitch of the previous sample is the same as the newly added sample
         // so don't get sudden pitch sweep
         // BUGFIX - Don't need to do this
         //if (dwPitchSamplesOrig)
         //   memcpy (m_apPitch[PITCH_F0] + ((dwPitchSamplesOrig-1)*m_dwChannels),
         //      m_apPitch[PITCH_F0] + dwPitchSamplesOrig * m_dwChannels,
         //      m_dwChannels * sizeof(PWVPITCH));
      }

   // words
   DWORD i;
   PWVWORD pw;
   for (i = 0; i < m_lWVWORD.Num(); i++) {
      pw = (PWVWORD) m_lWVWORD.Get(i);
      if (pw->dwSample >= dwSamplesOrig) {
         m_lWVWORD.Remove (i);
         i--;
         continue;
      }
   }
   DWORD dwSampleOrig;
   for (i = 0; i < pWave->m_lWVWORD.Num(); i++) {
      pw = (PWVWORD) pWave->m_lWVWORD.Get(i);
      dwSampleOrig = pw->dwSample;
      pw->dwSample += dwSamplesOrig;
      m_lWVWORD.Add (pw, pWave->m_lWVWORD.Size(i));
      pw->dwSample = dwSampleOrig;
   } // i

   // phonemes
   PWVPHONEME pp;
   for (i = 0; i < m_lWVPHONEME.Num(); i++) {
      pp = (PWVPHONEME) m_lWVPHONEME.Get(i);
      if (pp->dwSample >= dwSamplesOrig) {
         m_lWVPHONEME.Remove (i);
         i--;
         continue;
      }
   } // i
   for (i = 0; i < pWave->m_lWVPHONEME.Num(); i++) {
      pp = (PWVPHONEME) pWave->m_lWVPHONEME.Get(i);
      dwSampleOrig = pp->dwSample;
      pp->dwSample += dwSamplesOrig;
      m_lWVPHONEME.Add (pp);
      pp->dwSample = dwSampleOrig;
   } // i

   return TRUE;
}

/****************************************************************************
CM3DWave::CalcSREnergyRange - Calculates the enegy for the wave over a range of
samples, but uses the SRFEATURE information for the energy

inputs
   DWORD             dwStart - Start sample
   DWORD             dwEnd - End sample
   BOOL              fPyshco - If TRUE, calculating the psychoacoustic energy
returns
   fp - Energy (per sample)
*/
fp CM3DWave::CalcSREnergyRange (DWORD dwStart, DWORD dwEnd, BOOL fPsycho)
{
   if (!m_dwSRSkip)
      return 0;

   // NOTE: This will crash if m_paSRFeature is NULL, but I don't expect it to happen
   // in test, but to make sure.. Shuldnt break here
   if (!m_paSRFeature)
      return 0;

   dwStart = (dwStart + m_dwSRSkip/2) / m_dwSRSkip;
   dwEnd = (dwEnd + m_dwSRSkip/2) / m_dwSRSkip;
   dwEnd = min(dwEnd, m_dwSRSamples);
   if (dwEnd <= dwStart)
      return 0;

   // next energy
   double fEnergy = 0;
   DWORD i;
   for (i = dwStart; i < dwEnd; i++)
      fEnergy += SRFEATUREEnergy (fPsycho, m_paSRFeature + i);

   fEnergy /= (double)(dwEnd-dwStart);
   return (fp) fEnergy;
}


/****************************************************************************
CM3DWave::CalcEnergyRange - Calculates the enegy for the wave over a range of
samples.

inputs
   DWORD             dwStart - Start sample
   DWORD             dwEnd - End sample
returns
   fp - Energy (per sample)
*/
fp CM3DWave::CalcEnergyRange (DWORD dwStart, DWORD dwEnd)
{
   dwStart = min(dwStart, m_dwSamples);
   dwEnd = min(dwEnd,m_dwSamples);
   dwEnd = max(dwEnd, dwStart);
   if (dwEnd <= dwStart)
      return 0;   // cant calulate this

   // first of all, determine the DC offset, since really mucks up energy
   double fDC = 0;
   DWORD i, j;
   for (i = dwStart; i < dwEnd; i++) for (j = 0; j < m_dwChannels; j++)
      fDC += (double)m_psWave[i*m_dwChannels+j];
   fDC /= ((fp)(dwEnd-dwStart) * (fp)m_dwChannels);

   // next energy
   double fEnergy = 0;
   for (i = dwStart; i < dwEnd; i++) for (j = 0; j < m_dwChannels; j++) {
      fp fVal = (fp)m_psWave[i*m_dwChannels+j] - fDC;
      fEnergy += fVal * fVal;
   }
   fEnergy = sqrt(fEnergy);
   fEnergy /= ((double)(dwEnd-dwStart) * (double)m_dwChannels);
   return (fp) fEnergy;
}


/****************************************************************************
CM3DWave::PitchOverRange - Returns the pitch of a segment of wavelength,
over a range of samples.

inputs
   DWORD          dwPitchSub - PITCH_F0 or PITCH_SUB
   DWORD          dwStartSample - Starting sample
   DWORD          dwEndSample - End sample
   DWORD          dwChannel - channel
   fp             *pfAvgStrength - Filled with the average strength over the range.
                     Can be NULL
   fp             *pfPitchDelta - Filled with the pitch delta between the end and
                     start pitch, generated by doing a linear fit. Ratio of right
                     pitch to left pitch. THis
                     can be NULL.
   fp             *pfPitchBulge - Filled in with the amount to scale the time-centered
                     average by to create a bend in the pitch curve to more
                     accurately represent it. If > 1, creates a bulge, if < 1 creates
                     a valley. (Can be NULL)
   BOOL           fIgnoreStrength - If TRUE (defaults to FALSE) then ignores strength
                     when calcualting range.
returns
   fp - Average pitch, or -1 if error
*/
fp CM3DWave::PitchOverRange (DWORD dwPitchSub, DWORD dwStartSample, DWORD dwEndSample, DWORD dwChannel,
                             fp *pfAvgStrength, fp *pfPitchDelta, fp *pfPitchBulge,
                             BOOL fIgnoreStrength)
{
   if (!m_adwPitchSamples[dwPitchSub] || !m_adwPitchSkip[dwPitchSub])
      return -1;   // error since no pitch information

   DWORD dwOrigStart = dwStartSample;
   DWORD dwOrigEnd = dwEndSample;

   // convert to start sample rounded down, and end up
   dwStartSample /= m_adwPitchSkip[dwPitchSub];
   dwEndSample = dwEndSample / m_adwPitchSkip[dwPitchSub] + 1;
   dwEndSample = min(dwEndSample, m_adwPitchSamples[dwPitchSub]);
   if (dwEndSample <= dwStartSample)
      return -1;   // error

   DWORD i;
   double fSum = 0, fStrength = 0, fStrengthRet = 0, fStrengthThis;
   for (i = dwStartSample; i < dwEndSample; i++) {
      fStrengthThis = m_apPitch[dwPitchSub][i*m_dwChannels+dwChannel].fStrength+1.0;
      fStrengthRet += fStrengthThis;
      fStrengthThis = fIgnoreStrength ? 1 : fStrengthThis;
      fStrength += fStrengthThis;
      fSum += log(max(1.0,m_apPitch[dwPitchSub][i*m_dwChannels+dwChannel].fFreq) / SRBASEPITCH) * fStrengthThis;
      // BUGFIX - Adding 1 to strength so always have at least something
      // BUGFIX - make it log based so more accurate
   }
   if (fStrength)
      fSum /= fStrength;
   fSum = exp(fSum) * SRBASEPITCH;
   if (pfAvgStrength)
      *pfAvgStrength = fStrengthRet / (fp)(dwEndSample - dwStartSample);

   if (pfPitchDelta) {
      // do a linear fit
      double fSlope =0;
      fStrength = 0;
      double fCenter = ((double)dwStartSample + (double)(dwEndSample-1))/2;
      double fX, fTempSlope, fTempStrength;
      for (i = dwStartSample; i < dwEndSample; i++) {
         fX = (double)i - fCenter;
         if (!fX)
            continue;   // right at center point so skip
         fTempSlope = log(max(m_apPitch[dwPitchSub][i*m_dwChannels+dwChannel].fFreq,1) / max(fSum,1)) / fX;

         fTempStrength = fIgnoreStrength ? 1 : (double)(m_apPitch[dwPitchSub][i*m_dwChannels+dwChannel].fStrength+1);
         fTempStrength *= fabs(fX);

         fSlope += fTempStrength * fTempSlope;
         fStrength += fTempStrength;
         // BUGFIX - Adding 1 to strength so always have at least something
      } // i

      if (fStrength)
         fSlope /= fStrength;

      fSlope *= (double)(dwEndSample - dwStartSample - 1);
      *pfPitchDelta = (fp) exp(fSlope);
   } // if pitch delta

   if (pfPitchBulge) {
      // average over center fifth so dont get too much pitch noise
      DWORD dwStart = (dwOrigStart * 3 + dwOrigEnd * 2) / 5;
      DWORD dwEnd = (dwOrigStart * 2 + dwOrigEnd * 3) / 5;
      *pfPitchBulge = PitchOverRange (dwPitchSub, dwStart, dwEnd, dwChannel, NULL, NULL, NULL, FALSE);
      if (*pfPitchBulge <= 0)
         *pfPitchBulge = PitchAtSample (dwPitchSub, (dwOrigStart + dwOrigEnd)/2, dwChannel);
      *pfPitchBulge = *pfPitchBulge / fSum;
   }

   return (fp)fSum;
}


/****************************************************************************
CM3DWave::FixHighFreqBlurriness - This function fixes the problem where
formants are blurrier at higher frequencies because of problems with the
FFT.

It does a windowed FFT over the same sample and derives an energy value
from that. Using the energy it finds, it scales up/down the energy in
the voiced and unvoiced area so it matches with the windowed FFT energy.
Because the energy calculated in lower frequencies is less accurate for the FFT
and more accurate for the FFT of the pitch period (that originall produced
the data), the windowed FFT result is weighted by frequency.

inputs
   fp             fSRSample - Speech recognition sample (to center around)
   float          *pafFFTVoiced - Filled in with SRDATAPOINTS for energy of voiced. Modified
   float          *pafFFTNoise - Filled in with SRDATAPOINTS for energy of unvoiced. Modified
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FixHighFrequencyBlurriness (fp fSRSample, float *pafFFTVoiced, float *pafFFTNoise,
                                           PCSinLUT pLUT, PCMem pMemFFTScratch)
{
   fSRSample *= (fp)m_dwSRSkip;

   // BUGFIX - get the pitch here, and make sure large enough FFT to cover 2 pitches
   fp fPitch = PitchAtSample (PITCH_F0, fSRSample, 0);
   fPitch = max(fPitch, 1);
   DWORD dwMinSize = (DWORD) ((fp)m_dwSamplesPerSec / fPitch * 2.0);
   dwMinSize = max(dwMinSize, m_dwSRSkip);

   // the window size needs to be just larger than a SR window
   DWORD dwWindowSize;
   for (dwWindowSize = 2; dwWindowSize < dwMinSize; dwWindowSize*=2);
   
   // allocate enough space
   CMem mem;
   if (!mem.Required ((dwWindowSize + dwWindowSize/2)*sizeof(float)))
      return FALSE;
   float *pafWindow = (float*)mem.p;
   float *pafFFT = pafWindow + dwWindowSize;

   // create the hanning window
   CreateFFTWindow (3, pafWindow, dwWindowSize);

   // do the FFT
   FFT (dwWindowSize, (int)fSRSample, 0, pafWindow, pafFFT, pLUT, pMemFFTScratch, NULL, 0);
   dwWindowSize /= 2;   // so easier to deal with later


   // figure out the conversion from frequency to window bin
   fp fFreqPerBin = (fp)m_dwSamplesPerSec / 2.0 / (fp)dwWindowSize;

   // determine how much frequency increases every step in SRDATA
   fp fFreqInc = pow (2.0, 1.0 / (fp) SRPOINTSPEROCTAVE);
   fp fFreqLow = (fp)SRBASEPITCH / sqrt(fFreqInc);

   // BUGFIX - increased by an octave
#define ALPHAKEEPALL    3200  // BUGFIX - was 1600, but getting banding
//#define ALPHAKEEPALL    6400
#define ALPHAKEEPNONE   (ALPHAKEEPALL/2)
   // BUGFIX - Scale keep based on pitch so better outcome for low blizzard 2007 voice
   fp fPitchScale = fPitch / 180.0; // since fine-tuned for female voice when increased octave
   fp fAlphaKeepAll = fPitchScale * (fp)ALPHAKEEPALL;
   fp fAlphaKeepNone = fPitchScale * (fp)ALPHAKEEPNONE;

   // loop over all the elements in SRData
   DWORD i, j;
   for (i = 0; i < SRDATAPOINTS; i++) {
      // figure out the next frequency
      fp fFreqNext = fFreqLow * fFreqInc;

      // what are these in terms of FFT window locations
      int iStart, iEnd;
      iStart = (int)(fFreqLow / fFreqPerBin);
      iEnd = (int)(fFreqNext / fFreqPerBin);
      iStart = max(iStart, 0);
      iEnd = max(iEnd, iStart+1);   // so always have something. BUGFIX
      iEnd = min(iEnd, (int) dwWindowSize);

      // update frequency low
      fFreqLow = fFreqNext;

      // if below point that touch then just continue
      if (fFreqLow <= fAlphaKeepNone)
         continue;

      // if beyond range then just set to 0
      if (iEnd >= (int)dwWindowSize) {
         pafFFTVoiced[i] *= 0.5;
            // BUGFIX - Since not affecting voiced 
         pafFFTNoise[i] = 0;
         continue;
      }

      if (iEnd <= iStart)
         continue;   // too small a bin

      // loop over all these points and calculate the energy
      fp fEnergy = 0;
      for (j = (DWORD)iStart; j < (DWORD)iEnd; j++)
         fEnergy = max(pafFFT[j], fEnergy); // BUGFIX * pafFFT[j];
         // fEnergy += pafFFT[j]; // BUGFIX * pafFFT[j];
      // BUGFIX - fEnergy = sqrt(fEnergy);
      // BUGFIX - I think this is wrong... since really want sum of eneergy
      // not, average over area... Causing female voice to be muted... Putting this in makes female sound better
      // fEnergy /= (fp)(iEnd - iStart);
      // fEnergy /= (fp)(iEnd - iStart); // BUGFIX - sqrt((fp)(iEnd - iStart));

      // figure out the combined energy of noise and voiced
      // BUGFIX - fHave scaling was wrong
      //fp fHave = sqrt(pafFFTVoiced[i]*pafFFTVoiced[i] + pafFFTNoise[i]*pafFFTNoise[i]);
      fp fHave = pafFFTVoiced[i] + pafFFTNoise[i];
      fp fVoiced = fabs(pafFFTVoiced[i]) / max(CLOSE, fHave);
      fHave = max(fHave, CLOSE); // so dont get divide by zero

      // figure out how much would need to scale what have in order to match
      // the energy
      fp fScale = fEnergy / fHave;

      // don't allow to scale up by more than a factor of 8, although scaling to
      // nothing is ok
      fScale = min(fScale, 8);

      // weight the scale by the frequency...
      fp fAlpha;
      if (fFreqLow >= fAlphaKeepAll)
         fAlpha  = 1;
      else if (fFreqLow >= fAlphaKeepNone)
         fAlpha = (fFreqLow - fAlphaKeepNone) / (fAlphaKeepAll-fAlphaKeepNone);
      else
         fAlpha = 0;
      fScale = fScale * fAlpha + 1.0 * (1.0 - fAlpha);

      // the more voiced, the less the scale
      // BUGFIX - only do if fScale >= 1. If < 1, then willing to go down to eliminate vocal fry
      if (fScale >= 1)
         fScale = fScale * (1.0 - fVoiced) + ((fScale + 1.0)/2.0) * fVoiced;

      // scale
      //pafFFTVoiced[i] *= (fScale + 1) / 2.0;
         // BUGFIX - reduce voiced effect since seems to hurt more than helps... 
         // BUGFIX - Removed the above because emphasized noise in some cirumstances
      pafFFTVoiced[i] *= fScale;
      pafFFTNoise[i] *= fScale;
   } // i

   // done
   return TRUE;
}



/****************************************************************************
FXSRFEATUREExtendSingle - Extends the top 1/2 octave of noise to the
full range, allowing 16 kHz audio with s's to sound better when sytnhesized
on 22 or 44 khz.

inputs
   PSRFEATURE        pSRF - Feature to extend
   DWORD             dwSamplesPerSec - Samples per second
*/
void FXSRFEATUREExtendSingle (PSRFEATURE pSRF, DWORD dwSamplesPerSec)
{
   // get the nyquist limit
   fp fNyquist = (fp)dwSamplesPerSec / 2.0;
   fNyquist *= 0.95; // toss out very last bit since round down

   // what does this translate in terms of srfeature units
   DWORD dwBoundary = (DWORD)(log((fp)(fNyquist / SRBASEPITCH)) / log((fp)2) * (fp)SRPOINTSPEROCTAVE);
   if (dwBoundary >= SRDATAPOINTS)
      return;  // nothing to change

   // figure out the range to look through
   DWORD dwStart = dwBoundary - SRPOINTSPEROCTAVE;
   DWORD dwLastQuarter = dwBoundary - SRPOINTSPEROCTAVE/2;  // BUGFIX - changed to half
   DWORD dwEnd = dwBoundary;

   // loop
   PSRFEATURE psrf;
   fp fMid = (fp)(dwStart+dwEnd) / 2.0;
   psrf = pSRF;

   // find the average
   fp fAverage = 0, fAverageLast = 0;
   DWORD i;
   BOOL fFound = FALSE;
   fp fEnergy;
   for (i = dwStart; i < dwEnd; i++) {
      fEnergy = DbToAmplitude(psrf->acNoiseEnergy[i]);
      fAverage += fEnergy;
      if (i >= dwLastQuarter)
         fAverageLast += fEnergy;
      if (psrf->acNoiseEnergy[i] > SRABSOLUTESILENCE)
         fFound = TRUE;
   }
   if (!fFound)
      return;   // nothing to extend
   fAverage /= (fp)(dwEnd - dwStart);
   fAverageLast /= (fp)(dwEnd - dwLastQuarter);

   // find the standard deviation
   fp fTotalSlope = 0, fWeight = 0;
   fp fSlope, fDist;
   for (i = dwStart; i < dwEnd; i++) {
      fDist = (fp)i - fMid;
      if (!fDist)
         continue;   // no weight right no top
      
      fSlope = (DbToAmplitude(psrf->acNoiseEnergy[i]) - fAverage) / fDist;

      // weight
      fDist = fabs(fDist);
      fTotalSlope += fSlope * fDist;
      fWeight += fDist;
   } // i
   if (fWeight)
      fTotalSlope /= fDist;
   fTotalSlope /= 8; // so not too extreme

   // determine what the starting value at the limit should be
   // fp fStartAtLimit = fAverage + fTotalSlope * ((fp)dwEnd - fMid);
   fp fStartAtLimit = fAverageLast; // since following slope didn't seem to work well
   
   // always decrease energy by at least 24 dB per octave
   fp fMaxEnergy = fStartAtLimit;
   fp fMaxEnergyChange = pow (2.0, -1.0 / (SRPOINTSPEROCTAVE/4));

   for (i = dwEnd; i < SRDATAPOINTS; i++, fStartAtLimit += fTotalSlope, fMaxEnergy *= fMaxEnergyChange) {
      // no less than 110 db
      fStartAtLimit = max(fStartAtLimit, 0);
      fStartAtLimit = min(fStartAtLimit, fMaxEnergy);
      psrf->acNoiseEnergy[i] = AmplitudeToDb (fStartAtLimit);
   }

   // done
}



/****************************************************************************
CM3DWave::SRFEATUREExtend - Extends the top 1/2 octave of noise to the
full range, allowing 16 kHz audio with s's to sound better when sytnhesized
on 22 or 44 khz.
*/
void CM3DWave::FXSRFEATUREExtend (void)
{
   // if no SR features then ignore
   if (!m_paSRFeature)
      return;


   // loop
   DWORD dwSample;
   PSRFEATURE psrf;
   for (dwSample = 0, psrf = m_paSRFeature; dwSample < m_dwSRSamples; dwSample++, psrf++)
      FXSRFEATUREExtendSingle (psrf, m_dwSamplesPerSec);

   // done
}




/****************************************************************************
CM3DWave::FixedFFTToSRDATAPOINT - This does a FFT around a given SR sample and
fills in an array of pafFFTVoiced with SRDATAPOINTs worth of energy.

inputs
   fp             fSRSample - Speech recognition sample (to center around)
   float          *pafFFTVoiced - Filled in with SRDATAPOINTS for energy of voiced.
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::FixedFFTToSRDATAPOINT (fp fSRSample, float *pafFFTVoiced,
                                           PCSinLUT pLUT, PCMem pMemFFTScratch)
{
   fSRSample *= (fp)m_dwSRSkip;

   // BUGFIX - get the pitch here, and make sure large enough FFT to cover 2 pitches
   DWORD dwMinSize = m_dwSRSkip;

   // the window size needs to be just larger than a SR window
   DWORD dwWindowSize;
   for (dwWindowSize = 2; dwWindowSize < dwMinSize; dwWindowSize*=2);
   
   // allocate enough space
   CMem mem;
   if (!mem.Required ((dwWindowSize + dwWindowSize/2)*sizeof(float)))
      return FALSE;
   float *pafWindow = (float*)mem.p;
   float *pafFFT = pafWindow + dwWindowSize;

   // create the hanning window
   CreateFFTWindow (3, pafWindow, dwWindowSize);
      // NOTE: Could pull out an optimize

   // do the FFT
   FFT (dwWindowSize, (int)fSRSample, 0, pafWindow, pafFFT, pLUT, pMemFFTScratch, NULL, 0);
   dwWindowSize /= 2;   // so easier to deal with later


   // figure out the conversion from frequency to window bin
   fp fFreqPerBin = (fp)m_dwSamplesPerSec / 2.0 / (fp)dwWindowSize;

   // determine how much frequency increases every step in SRDATA
   fp fFreqInc = pow (2.0, 1.0 / (fp) SRPOINTSPEROCTAVE);
   fp fFreqLow = (fp)SRBASEPITCH / sqrt(fFreqInc);

   // loop over all the elements in SRData
   DWORD i, j;
   for (i = 0; i < SRDATAPOINTS; i++) {
      // figure out the next frequency
      fp fFreqNext = fFreqLow * fFreqInc;

      // what are these in terms of FFT window locations
      int iStart, iEnd;
      iStart = (int)(fFreqLow / fFreqPerBin);
      iEnd = (int)(fFreqNext / fFreqPerBin);
      iStart = max(iStart, 0);
      iEnd = max(iEnd, iStart+1);   // so always have something
      iEnd = min(iEnd, (int) dwWindowSize);

      // update frequency low
      fFreqLow = fFreqNext;

      // if beyond range then just set to 0
      if (iEnd >= (int)dwWindowSize) {
         pafFFTVoiced[i]  = 0;
         continue;
      }

      if (iEnd <= iStart) {
         pafFFTVoiced[i] = 0;
         continue;   // too small a bin
      }

      // loop over all these points and calculate the energy
      fp fEnergy = 0;
      for (j = (DWORD)iStart; j < (DWORD)iEnd; j++)
         fEnergy += pafFFT[j] * pafFFT[j];
      fEnergy = sqrt(fEnergy);
      // BUGFIX - I think this is wrong... since really want sum of eneergy
      // not, average over area... Causing female voice to be muted... Putting this in makes female sound better
      // fEnergy /= (fp)(iEnd - iStart);
      fEnergy /= sqrt((fp)(iEnd - iStart));

      fp fScaleVal = pow (2, ((fp)i - (fp)SRDATAPOINTS/2) / SRPOINTSPEROCTAVE);
      pafFFTVoiced[i] = fEnergy * fScaleVal;
   } // i

   // done
   return TRUE;
}

/****************************************************************************
EnergyCalcPerOctave - Calculate the energy centered around each octave.

inputs
   float       *pafVoiced - Voiced energy. SRDATAPOINTS
   float       *pafUnvoiced - Unvoiced energy. Can be NULL.
   float       *pafEnergy - Array of SROCTAVE+1 points (for voiced an unvoiced)
returns
   none
*/
void EnergyCalcPerOctave (float *pafVoiced, float *pafUnvoiced, float *pafEnergy)
{
   memset (pafEnergy, 0, sizeof(*pafEnergy)*(SROCTAVE+1));

   DWORD i;
   for (i = 0; i < SRDATAPOINTS; i++) {
      DWORD dwOctave = i / SRPOINTSPEROCTAVE;
      fp fAlpha = (fp)(i - dwOctave * SRPOINTSPEROCTAVE) / (fp)SRPOINTSPEROCTAVE;
      fp fOneMinus = 1.0 - fAlpha;
      
      float fEThis = pafVoiced[i] + (pafUnvoiced ? pafUnvoiced[i] : 0.0);
      pafEnergy[dwOctave] += fOneMinus * fEThis;
      pafEnergy[dwOctave+1] += fAlpha * fEThis;
   } // i
}


/****************************************************************************
EnergyScalePerOctave - Given an amount to scale per octave, this
scales up the voiced and unvoiced energy

inputs
   float       *pafVoiced - Voiced energy. SRDATAPOINTS
   float       *pafUnvoiced - Unvoiced energy. Can be NULL.
   float       *pafEnergy - Array of SROCTAVE+1 points (for voiced an unvoiced) to scale by
returns
   none
*/
void EnergyScalePerOctave (float *pafVoiced, float *pafUnvoiced, float *pafEnergy)
{
   DWORD i;
   for (i = 0; i < SRDATAPOINTS; i++) {
      DWORD dwOctave = i / SRPOINTSPEROCTAVE;
      fp fAlpha = (fp)(i - dwOctave * SRPOINTSPEROCTAVE) / (fp)SRPOINTSPEROCTAVE;
      fp fOneMinus = 1.0 - fAlpha;

      float fScale = pafEnergy[dwOctave] * fOneMinus + pafEnergy[dwOctave+1] * fAlpha;

      pafVoiced[i] *= fScale;
      if (pafUnvoiced)
         pafUnvoiced[i] *= fScale;
   } // i
}



/****************************************************************************
CM3DWave::RescaleSRDATAPOINTBySmallFFT - This does a small FFT window in order
to get maximum time accuracy, and rescales the SRDATAPOINTs to the new
FFT (limited) to try to catch the change in energy.

inputs
   fp             fSRSample - Speech recognition sample (to center around)
   float          *pafFFTVoiced - Filled in with SRDATAPOINTS for energy of voiced. And modified in place
   float          *pafFFTUnvoiced - Filled in with SRDATAPOINTS for energy of voiced. And modified in place
   float          fWeight - If 1.0, weight is all for the FFT data. If 0, all for the original data
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
returns
   none
*/
void CM3DWave::RescaleSRDATAPOINTBySmallFFT (fp fSRSample, float *pafFFTVoiced,
                                             float *pafFFTUnvoiced, float fWeight, PCSinLUT pLUT, PCMem pMemFFTScratch)
{
   // calculate the FFT
   float afFFT[SRDATAPOINTS];
   FixedFFTToSRDATAPOINT (fSRSample, afFFT, pLUT, pMemFFTScratch);

   // calculate the energy in the FFT and in the original signal
   float afEnergyFFT[SROCTAVE+1], afEnergyOrig[SROCTAVE+1];
   EnergyCalcPerOctave (pafFFTVoiced, pafFFTUnvoiced, afEnergyOrig);
   EnergyCalcPerOctave (afFFT, NULL, afEnergyFFT);

   // figure out the scaling
   DWORD i;
   float fScale;
   for (i = 0; i < SROCTAVE+1; i++) {
      if (afEnergyOrig[i] < CLOSE)
         fScale = 1;
      else
         fScale = afEnergyFFT[i] / afEnergyOrig[i];

      // limit to 24dB
      fScale = max(fScale, 0.12);
      fScale = min(fScale, 8.0);

      // weight, so control how influenctial the FFT is
      fScale = fWeight * fScale + (1.0 - fWeight) * 1.0;

      afEnergyFFT[i] = fScale;
   } // i

   // rescale by this FFT
   EnergyScalePerOctave (pafFFTVoiced, pafFFTUnvoiced, afEnergyFFT);
}


/****************************************************************************
CM3DWave::RemoveNoisySRFEATURE - Given a SRFEATURE (in floats) in the
center, and a left and right, this might remove the noise component
if the left/right are similar energy and are mostly voiced.

inputs
   float       *pafCenterVoiced - Center, with SRDATAPOINTS voiced. This is also modified
   float       *pafCenterUnvoiced - Center, with SRDATAPOINTS voiced. This is also modified
   float       *pafCenterEnergy - SROCTAVE+1 energy values
   float       *pafCenterPercent - Percent of noise. SROCTAVE+1
   float       *pafLeftVoiced - Left voiced
   float       *pafLeftUnvoiced - Unvoiced
   float       *pafLeftEnergy - SROCTAVE+1 energy values
   float       *pafLeftPercent - Percent of noise. SROCTAVE+1
   float       *pafRightVoiced - Right
   float       *pafRightUnvoiced - Right
   float       *pafRightEnergy - SROCTAVE+1 energy values
   float       *pafRightPercent - Percent of noise. SROCTAVE+1
   float       *pafNewVoiced - Fill in with the new voiced
   float       *pafNewUnvoiced - Fill in with the new unvoiced
returns
   none
*/
void CM3DWave::RemoveNoisySRFEATURE (float *pafCenterVoiced, float *pafCenterUnvoiced, float *pafCenterEnergy, float *pafCenterPercent,
                                     float *pafLeftVoiced, float *pafLeftUnvoiced, float *pafLeftEnergy, float *pafLeftPercent,
                                     float *pafRightVoiced, float *pafRightUnvoiced, float *pafRightEnergy, float *pafRightPercent,
                                     float *pafNewVoiced, float *pafNewUnvoiced)
{
   // figure out what percent we want
   float afWantPercent[SROCTAVE+1];
   memcpy (afWantPercent, pafCenterPercent, sizeof(afWantPercent));

   DWORD i, dwLeft;

   // figure out toal energies
   float afEnergyTotal[3];
   memset (afEnergyTotal, 0, sizeof(afEnergyTotal));
   for (i = 0; i < SROCTAVE+1; i++) {
      afEnergyTotal[0] += pafLeftEnergy[i];
      afEnergyTotal[1] += pafCenterEnergy[i];
      afEnergyTotal[2] += pafRightEnergy[i];
   } // i
   for (i = 0; i < 3; i++)
      afEnergyTotal[i] /= (float)(SROCTAVE+1);  // for normalization

   for (i = 0; i < SROCTAVE+1; i++) {
      // if already voiced then ignore
      if (!afWantPercent[i])
         continue;

      // if no energy then contiue
      if (!pafCenterEnergy[i])
         continue;

      for (dwLeft = 0; dwLeft < 2; dwLeft++) {
         // BUGFIX - Instead of taking a percentage of voied, take minimum unvoiced for left/right
         fp fWant = (dwLeft ? pafLeftPercent : pafRightPercent)[i] * (dwLeft ? pafLeftEnergy : pafRightEnergy)[i] / pafCenterEnergy[i];
         if (fWant >= afWantPercent[i])
            continue;   // already higher percentage of noise, so ignore

         // what's the weighting? Include total energy as a factor
         fp fEnergy = (dwLeft ? pafLeftEnergy : pafRightEnergy)[i] + afEnergyTotal[dwLeft ? 0 : 2];
         fp fEnergyMax = max(fEnergy, pafCenterEnergy[i] + afEnergyTotal[1]);
         fp fEnergyMin = min(fEnergy, pafCenterEnergy[i] + afEnergyTotal[1]);
         fp fWeight = fEnergyMax ? (fEnergyMin / fEnergyMax) : 0;
         // take out - fWeight = (fWeight - 0.5) * 2.0; // so need to within 50% of energy to count
         if (fWeight <= 0)
            continue;

         // else, include
         afWantPercent[i] = afWantPercent[i] * (1.0 - fWeight) + fWant * fWeight;
      } // dwLeft
   } // i

   // loop over all the frequencies and apply the noise
   for (i = 0; i < SRDATAPOINTS; i++) {
      DWORD dwOctave = i / SRPOINTSPEROCTAVE;
      fp fAlpha = (fp)(i - dwOctave * SRPOINTSPEROCTAVE) / (fp)SRPOINTSPEROCTAVE;
      fp fOneMinus = 1.0 - fAlpha;

      float fPercent = afWantPercent[dwOctave] * fOneMinus + afWantPercent[dwOctave+1] * fAlpha;

      float fSum = pafCenterVoiced[i] + pafCenterUnvoiced[i];
      pafNewVoiced[i] = fSum * (1.0 - fPercent);
      pafNewUnvoiced[i] = fSum * fPercent;
   } // i

}


#define FORMANTMUCKINGLESSATLOWFREQ    (SRPOINTSPEROCTAVE*2) // do less formant mucking for the bottom two octaves, so don't get as fuzzy males

/****************************************************************************
NarrowerFormants - Creates narrower formants.

inputs
   float       *pafVoiced - Voiced, SRDATAPOINTS. Modified in place.
   float       *pafUnvoiced - Unvoiced, SRDATAPOINTS. Modified in place
*/
void NarrowerFormants (float *pafVoiced, float *pafUnvoiced)
{
   // remember the original energy
   float afEnergyOrig[SROCTAVE+1];
   EnergyCalcPerOctave (pafVoiced, pafUnvoiced, afEnergyOrig);

   // copy these over
   float afCopy[2][SRDATAPOINTS];
   memcpy (afCopy[0], pafUnvoiced, sizeof(afCopy[0]));
   memcpy (afCopy[1], pafVoiced, sizeof(afCopy[1]));

   DWORD i;
   for (i = 0; i < SRDATAPOINTS; i++) {
      // wider at bottom, 1/4 of an octave, while narrow at high frequencies
      DWORD dwWidth = (SRDATAPOINTS - i) * SRPOINTSPEROCTAVE / 2 / SRDATAPOINTS; // + SRPOINTSPEROCTAVE/12;
         // BUGFIX - Instead of 1/4 octave, make 1/2 octave
      
      int iStart = (int)i - (int)dwWidth/2;
      int iEnd = iStart + (int)dwWidth;
      iStart = max(iStart, 0);
      iStart = min(iStart, SRDATAPOINTS-1);
      iEnd = max(iEnd, iStart+1);   // so at least one
      iEnd = min(iEnd, SRDATAPOINTS);

      DWORD dwVoiced;
      int iLoop;
      for (dwVoiced = 0; dwVoiced < 2; dwVoiced++) {
         float *paf = afCopy[dwVoiced];

         // take the minimum over a range
         float f = paf[iStart];  // starting value
         for (iLoop = iStart+1; iLoop < iEnd; iLoop++)
            f = min(f, paf[iLoop]);

         if (dwVoiced)
            pafVoiced[i] = f;
         else
            pafUnvoiced[i] = f;

      } // dwVoiced
   } // i

   // what's the new energy
   float afEnergyNew[SROCTAVE+1];
   EnergyCalcPerOctave (pafVoiced, pafUnvoiced, afEnergyNew);

   // scale so have the same energy as started with
   for (i = 0; i < SROCTAVE+1; i++) {
      if (afEnergyNew[i] > CLOSE)
         afEnergyOrig[i] /= afEnergyNew[i];
      else
         afEnergyOrig[i] = 1.0;
   } // i
   EnergyScalePerOctave (pafVoiced, pafUnvoiced, afEnergyOrig);

   // BUGFIX - decrease effect at lower octives because making for very boomy males
   for (i = 0; i < FORMANTMUCKINGLESSATLOWFREQ; i++) {
      fp fScale = (fp) i / (fp) FORMANTMUCKINGLESSATLOWFREQ;

      pafVoiced[i] = fScale * pafVoiced[i] + (1.0 - fScale) * afCopy[1][i];
      pafUnvoiced[i] = fScale * pafUnvoiced[i] + (1.0 - fScale) * afCopy[0][i];
   } // i

}


/****************************************************************************
CM3DWave::Monotone - Create a new version of this wave that is
entirely monotone. It uses the pitch to stretch/squash the wave
so it's monotone, but not necessarily the same duration.

inputs
   DWORD          dwCalcFor - WAVECALC_XXX
   fp             fPitch - Pitch that want
   DWORD          dwStartSample - Start sample. If -1 then uses 0
   DWORD          dwEndSample - End sample. If -1 then uses m_dwNumSamples
   PCListFixed    plMap - If not NULL, this list is initialized to sizeof(double)
                  and filled with one entry per sample in the NEW wave. THe
                  entry points back to the sample index from the old wave
   PCProgressSocket     pProgress - Progress bar.
returns
   PCM3DWave - New wave, or NULL if error
*/
PCM3DWave CM3DWave::Monotone (DWORD dwCalcFor, fp fPitch, DWORD dwStartSample, DWORD dwEndSample,
                              PCListFixed plMap, PCProgressSocket pProgress)
{
   // do some min and max
   if (dwStartSample == (DWORD)-1)
      dwStartSample = 0;
   if (dwEndSample == (DWORD)-1)
      dwEndSample = m_dwSamples;
   dwEndSample = min(dwEndSample, m_dwSamples);
   if (dwStartSample >= dwEndSample)
      return NULL; // out of bounds

   // require pitch
   BOOL fCalcPitch = FALSE;
   if (!m_adwPitchSamples[PITCH_F0]) {
      fCalcPitch = TRUE;
      if (pProgress)
         pProgress->Push(0, 0.8);
      CalcPitch (dwCalcFor, pProgress);
      if (pProgress)
         pProgress->Pop();

      if (!m_adwPitchSamples[PITCH_F0])
         return NULL;   // error
   }
   // create the new wave
   PCM3DWave pNew = new CM3DWave;
   if (!pNew)
      return NULL;
   pNew->ConvertSamplesAndChannels (m_dwSamplesPerSec, m_dwChannels, NULL);
   pNew->ReleaseCalc (); // while at it free up the calculated info


   // initialize map
   if (plMap) {
      plMap->Init (sizeof(double));
      plMap->Required ((dwEndSample - dwStartSample) * 2);
   }
   double fCurSample;
   double fEndSample = dwEndSample;
   double fdPitch = fPitch;

   double fPitchSkip = m_adwPitchSkip[PITCH_F0];
   double fAlpha, fOrigPitch, fPitchSample;
   int iPitchSample;

   if (pProgress)
      pProgress->Push (fCalcPitch ? 0.8 : 1, 1.0);
   DWORD dwCount;
   for (fCurSample = dwStartSample, dwCount = 0; fCurSample < fEndSample; dwCount++) {
      // progress
      if (pProgress && !(dwCount % 10000))
         pProgress->Update ((fCurSample - (fp)dwStartSample) / (fp)(dwEndSample - dwStartSample));

      // make sure enough memory in new one
      DWORD dwNeed;
      pNew->m_dwSamples++;
      dwNeed = pNew->m_dwSamples * pNew->m_dwChannels * sizeof(short);
      if (dwNeed > pNew->m_pmemWave->m_dwAllocated) {
         if (!pNew->m_pmemWave->Required (dwNeed)) {
            delete pNew;
            return NULL;
         }
         pNew->m_psWave = (short*) pNew->m_pmemWave->p;
      }

      // what are the locations
      DWORD dwOrig = (DWORD)fCurSample;
      DWORD dwOrigNext = min(dwOrig+1, m_dwSamples-1);
      fAlpha = fCurSample - (double)dwOrig;
      short *psOrig = m_psWave + dwOrig*m_dwChannels;
      short *psOrigNext = m_psWave + dwOrigNext*m_dwChannels;
      short *psNew = pNew->m_psWave + (pNew->m_dwSamples-1)*pNew->m_dwChannels;

      // interp
      DWORD dwChannel;
      for (dwChannel = 0; dwChannel < m_dwChannels; dwChannel++, psOrig++, psOrigNext++, psNew++)
         psNew[0] = (double)psOrig[0] * (1.0 - fAlpha) + (double)psOrigNext[0] * fAlpha;

      // write this to the list
      if (plMap)
         plMap->Add (&fCurSample);

      // determine the pitch here
      fPitchSample = fCurSample / fPitchSkip;
      iPitchSample = (int)floor(fPitchSample);
      fAlpha = fPitchSample - iPitchSample;
      fOrigPitch = (1.0 - fAlpha) * m_apPitch[PITCH_F0][min((DWORD)iPitchSample,m_adwPitchSamples[PITCH_F0]-1)*m_dwChannels+0].fFreq +
         fAlpha * m_apPitch[PITCH_F0][min((DWORD)iPitchSample+1,m_adwPitchSamples[PITCH_F0]-1)*m_dwChannels+0].fFreq;
      fOrigPitch = max(fOrigPitch, 1);
      fCurSample += fdPitch / fOrigPitch;
   }
   if (pProgress)
      pProgress->Pop ();

   return pNew;
}



/****************************************************************************
CM3DWave::MonotoneFindOrigSample - Given a sample in the monotone
wave, this finds the original sample using the plMap filled in
by the Monotone() call.

inputs
   double            fSample - Sample looking for in the original wave
   PCListFixed       plMap - List of doubles, from Monotone()
   double            *pfPitch - Filled in with the pitch scaling.
                     can be NULL
returns
   double - Sample in the original
*/
double CM3DWave::MonotoneFindOrigSample (double fSample, PCListFixed plMap,
                                         double *pfPitch)
{
   double *pafMap = (double*)plMap->Get(0);

   // loop
   DWORD dwCur, dw;
   DWORD dwNum = plMap->Num();
   if (dwNum < 2) {
      if (pfPitch)
         *pfPitch = 1;  // since can't tell
      return 0;
   }
   for (dwCur = 1; dwCur < dwNum; dwCur *= 2);

   dw = 0;
   double f;
   for (; dwCur; dwCur /= 2) {
      DWORD dwTry = dw + dwCur;
      if (dwTry >= dwNum)
         continue;   // too high

      // see how pComp stacks up
      f = fSample - pafMap[dwTry];
      if (f == 0) {
         // exact match
         dw = dwTry;
         break;
      }
      else if (f > 0)
         dw = dwTry;
   } // dwCur

   // if beyond end then use that
   dw = min(dw, dwNum-1);
   if (fSample < pafMap[dw])
      dw++;
   dw = min(dw, dwNum-2);  // so can interpolate

   double fDelta = pafMap[dw+1] - pafMap[dw];
   if (fDelta < EPSILON) {
      if (pfPitch)
         *pfPitch = 1;  // since can't tell
      return fDelta;
   }

   double fAlpha = (fSample - pafMap[dw]) / fDelta;
   if (pfPitch)
      *pfPitch = 1.0 / fDelta;
   return (double)dw + fAlpha;
}



/****************************************************************************
CM3DWave::FFTToHarmInfo - Takes a FFT and converts it to a harmonic
info array.

inputs
   DWORD             dwNum - Number of entries in pHI
   PHARMINFO         pHI - To be filled in
   DWORD             dwWindowSize - FFT window size. Ideally, the FFT window size
                     should be 4 * dwNum.
                     2* is because of real-imaginary conversion, 2* is only take even
                     harmonics.
   float             *pafFFT - FFT. FFT should NOT have been scaled
returns
   float - Original phase of the fundamental. Can be NULL.
*/
float CM3DWave::FFTToHarmInfo (DWORD dwNum, PHARMINFO pHI, DWORD dwWindowSize, float *pafFFT)
{
   float   fScale = 2.0 / (float) dwWindowSize;// * 32768.0 / 32768.0;

   // loop through harmonics
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      DWORD dwIndex = i * 4;
      
      // if beyond window then ignore
      if (dwIndex+1 >= dwWindowSize) {
         pHI[i].fVoiced = pHI[i].fUnvoiced = pHI[i].fPhase = 0;
         continue;
      }

      DWORD dwIndexAbove = dwIndex + 2;
      if (dwIndexAbove+1 >= dwWindowSize)
         dwIndexAbove = (DWORD)-1;
      DWORD dwIndexBelow = dwIndex ? (dwIndex - 2) : (DWORD)-1;

      pHI[i].fVoiced = sqrt(pafFFT[dwIndex] * pafFFT[dwIndex] + pafFFT[dwIndex+1] * pafFFT[dwIndex+1]) * fScale;
      pHI[i].fUnvoiced = (
            ((dwIndexAbove != (DWORD)-1) ? sqrt(pafFFT[dwIndexAbove] * pafFFT[dwIndexAbove] + pafFFT[dwIndexAbove+1] * pafFFT[dwIndexAbove+1]) : 0) +
            ((dwIndexBelow != (DWORD)-1) ? sqrt(pafFFT[dwIndexBelow] * pafFFT[dwIndexBelow] + pafFFT[dwIndexBelow+1] * pafFFT[dwIndexBelow+1]) : 0)
         ) * fScale / 2.0;

      // BUGFIX - The way that got noise was wrong, since even if full noise, would still
      // claim half was voiced, so need to correct
      if (pHI[i].fUnvoiced >= pHI[i].fVoiced) {
         // swollow all the voiced
         pHI[i].fUnvoiced += pHI[i].fVoiced;
         pHI[i].fVoiced = 0;
      }
      else {
         // swollow part of the voices
         pHI[i].fVoiced -= pHI[i].fUnvoiced;
         pHI[i].fUnvoiced *= 2.0;
      }

      pHI[i].fPhase = atan2 (pafFFT[dwIndex], pafFFT[dwIndex+1]);

      // adjust so getting phase at center of FFT, not left edge
      // BUGFIX - Wrong, because phase half way in a 2-wavelength FFT is the same
      // as the phase at the start
      // pHI[i].fPhase += (float)i * PI;
   } // i

   // loop through again and readjust phase so that 1st harmonic is a phase of 0.0
   float fRet = pHI[1].fPhase;
   for (i = 1; i < dwNum; i++)
      pHI[i].fPhase -= fRet * (float)i;

   pHI[0].fPhase = pHI[0].fVoiced = pHI[0].fUnvoiced = 0.0; // nothing at bottom
   pHI[0].afVoicedSinCos[0] = pHI[0].afVoicedSinCos[1] = 0;

   return fRet;
}



/****************************************************************************
CM3DWave::HarmInfoToSRFEATUREFLOAT - Takes an array of HARMINFO and fills
in a SRFEATURE.

inputs
   DWORD          dwNum - Number of pHI
   PHARMINFO      pHI - Array of HARMINFO for the harmonics
   fp             fPitch - Pitch in Hz
   PSRFEATUREFLOAT     pSRF - To be filled in
returns
   none
*/
void CM3DWave::HarmInfoToSRFEATUREFLOAT (DWORD dwNum, PHARMINFO pHI, fp fPitch, PSRFEATUREFLOAT pSRF)
{
   DWORD i;
   DWORD dwBasePitch;
   for (i = 0; i < SRDATAPOINTS; i++) {
      // what is this in the FFT
      fp fFreq = SRBASEPITCH * pow ((fp)2.0, (fp)((fp)i / SRPOINTSPEROCTAVE));
      fp fAlpha = fFreq / fPitch;
      dwBasePitch = (DWORD)fAlpha;
      fAlpha -= dwBasePitch;
      if (dwBasePitch+1 >= dwNum) {
         pSRF->afVoiceEnergy[i] = pSRF->afNoiseEnergy[i] = 0;
         continue;
      }

      // scaling factor
      fp fScaleVal = pow ((fp)2.0, ((fp)i - (fp)SRDATAPOINTS/2) / SRPOINTSPEROCTAVE);

      // interpolate energy
      fp fVal;

      fVal = ((1.0 - fAlpha) * pHI[dwBasePitch].fVoiced + fAlpha * pHI[dwBasePitch+1].fVoiced) * fScaleVal;
      pSRF->afVoiceEnergy[i] = fVal;

      fVal = ((1.0 - fAlpha) * pHI[dwBasePitch].fUnvoiced + fAlpha * pHI[dwBasePitch+1].fUnvoiced) * fScaleVal;
      pSRF->afNoiseEnergy[i] = fVal;

   } // i

   // do the phase
   for (i = 0; i < SRPHASENUM; i++) {
      if (i+1 >= dwNum) {
         pSRF->afPhase[i] = 0;
         continue;
      }

      pSRF->afPhase[i] = atan2(pHI[i+1].afVoicedSinCos[0], pHI[i+1].afVoicedSinCos[1]);
      //pSRF->afPhase[i] = pHI[i+1].fPhase;
   }
}

/****************************************************************************
SRFEATUREFLOATDist - Distance between SRFEATUREFLOATs

inputs
   PSRFEATUREFLOAT   pSRFA - Feature one
   PSRFEATUREFLOAT   pSRFB - Feature two
returns
   fp - Distance, from 0 to 1
*/
fp SRFEATUREFLOATDist (PSRFEATUREFLOAT pSRFA, PSRFEATUREFLOAT pSRFB)
{
   double fSumA = 0, fSumB = 0, fDiff = 0;
   DWORD i;
   for (i = 0; i < SRDATAPOINTS; i++) {
      fSumA += pSRFA->afVoiceEnergy[i] + pSRFA->afNoiseEnergy[i];
      fSumB += pSRFB->afVoiceEnergy[i] + pSRFB->afNoiseEnergy[i];

      // BUGFIX - Add voiced and noise together since may not be that accurate yet
      fDiff += fabs(pSRFA->afVoiceEnergy[i] + pSRFA->afNoiseEnergy[i] - pSRFB->afVoiceEnergy[i] - pSRFB->afNoiseEnergy[i]);
   } // i

   fSumA = max(fSumA, fSumB);
   fSumA = max(fSumA, CLOSE);

   return fDiff / fSumA;
}


/****************************************************************************
SRFEATUREFLOATAverage - Do a weighted average

inputs
   DWORD                dwNum - Number
   PSRFEATUREFLOAT      paSRF - Pointer to an array of dwNum entries
   fp                   *pafWeight - Pointer to an array of weights.
   PSRFEATUREFLOAT      pSRFDest - Destination. THe center one has the misc bits, like phase,
                        copied
returns
   none
*/
void SRFEATUREFLOATAverage (DWORD dwNum, PSRFEATUREFLOAT paSRF, fp *pafWeight, PSRFEATUREFLOAT pSRFDest)
{
   memcpy (pSRFDest, paSRF + (dwNum/2), sizeof(*pSRFDest));

   DWORD i, j;
   double fSumVoice, fSumNoise;
   for (i = 0; i < SRDATAPOINTS; i++) {
      fSumVoice = fSumNoise = 0;
      for (j = 0; j < dwNum; j++) {
         if (!pafWeight[j])
            continue;   // dont read if 0 strength, so don't gp fault

         fSumVoice += pafWeight[j] * paSRF[j].afVoiceEnergy[i];
         fSumNoise += pafWeight[j] * paSRF[j].afNoiseEnergy[i];
      } // j

      pSRFDest->afVoiceEnergy[i] = fSumVoice;
      pSRFDest->afNoiseEnergy[i] = fSumNoise;
   } // i
}

/****************************************************************************
CM3DWave::HARMINFOAverageAll - Average all the phases together over a wave and
write them in the paSRF.

inputs
   DWORD             dwSamples - Number of samples
   double            *pafPitch - Listof pitches for each of dwSamples
   PHARMINFO         paHI - Array of dwSamples x SRFEATANAL_HARMONICS entries
   PSRFEATUREFLOAT   paSRF - Array of dwSamples SRFEATUREFLOAT that will have their phase modified
   DWORD             dwCalcSRFeatPCM - Was constant, size of stretched wave
returns
   none
*/

#define SRFEATANAL_HARMONICS           (dwCalcSRFeatPCM/4)
#define SRFEATANAL_HARMONICSMAX        (CALCSRFEATUREPCMMAX/4)

void CM3DWave::HARMINFOAverageAll (DWORD dwSamples, double *pafPitch, PHARMINFO paHI, PSRFEATUREFLOAT paSRF,
                                   DWORD dwCalcSRFeatPCM)
{
   // over all samples
   DWORD i, j;
   for (i = 0; i < dwSamples; i++) {
      for (j = 2; j < SRFEATANAL_HARMONICS; j++) { // start at 2, since 0 is DC offset, and 1 fundamtental
         // phase width is larger at low harmonics
         DWORD dwPhaseWidth = (SRPHASENUM-min(j, SRPHASENUM)) * SRSAMPLESPERSEC / 32 * m_dwSRSkip / (m_dwSamplesPerSec / SRSAMPLESPERSEC) / SRPHASENUM;
         dwPhaseWidth = (DWORD) floor(
            (fp)dwPhaseWidth
            / max(pafPitch[i], 1.0) *
            100.0 + 0.5);
               // BUGFIX - Phase width is a function of fundamental pitch

         // BUGFIX - Reduce phase width a bit more
         dwPhaseWidth = dwPhaseWidth * 2 / 3;

         if (!dwPhaseWidth)
            continue;   // since wont make a difference

         // loop
         int iOffset;
         fp fSumCos = 0, fSumSin = 0;
         for (iOffset = -(int)dwPhaseWidth; iOffset <= (int)dwPhaseWidth; iOffset++) {
            int iCur = (int)i + iOffset;
            if ((iCur < 0) || (iCur >= (int)dwSamples))
               continue;

            fp fWeight = dwPhaseWidth + 1 - (DWORD)abs(iOffset);
            fSumSin += paHI[(DWORD)iCur * SRFEATANAL_HARMONICS + j].afVoicedSinCos[0] * fWeight;
            fSumCos += paHI[(DWORD)iCur * SRFEATANAL_HARMONICS + j].afVoicedSinCos[1] * fWeight;
         } // iOffset

         // write
         if (j-1 < SRPHASENUM)
            paSRF[i].afPhase[j-1] = atan2(fSumSin, fSumCos);
      } // j, over all phases
   } // i
}



/****************************************************************************
SRFEATUREFLOATAverageAll - Loops over a block of SRFEATUREFLOAT and averages
them together if they're already fairly similar.

inputs
   DWORD             dwNum - Number of SRFEATURE
   PSRFEATUREFLOAT   pSRFOrig - Originals
   PSRFEATUREFLOAT   pSRFNew - Filled in with the blurred versions
returns
   none
*/
void SRFEATUREFLOATAverageAll (DWORD dwNum, PSRFEATUREFLOAT pSRFOrig, PSRFEATUREFLOAT pSRFNew)
{
#define SRFFBLURWINDOW     5     // number of samples to blue together, must be odd
#define SRFFBLURWINDOWHALF (SRFFBLURWINDOW/2)
#define TOODIFFERENTTOBLUR 0.5   // too different to blur
   fp afWeight[SRFFBLURWINDOW];
   fp fWeightSum;

   // loop over all SRFEATURE
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {

      // clear the weights
      memset (afWeight, 0, sizeof(afWeight));
      fWeightSum = 0;

      // over left and right
      for (j = 0; j < SRFFBLURWINDOW; j++) {
         int iCur = (int)i + (int)j - (int)SRFFBLURWINDOWHALF;
         if ((iCur < 0) || (iCur >= (int)dwNum))
            continue;   // nothing, and weight already set to 0

         // figure out how similar it is to the center
         fp fDist = (j == SRFFBLURWINDOWHALF) ? 0 : SRFEATUREFLOATDist (pSRFOrig + i, pSRFOrig + iCur);
         if (fDist >= TOODIFFERENTTOBLUR)
            continue;   // dont include
         fDist = 1.0 - (fDist / TOODIFFERENTTOBLUR);

         afWeight[j] = fDist * (fp)(SRFFBLURWINDOWHALF + 1 - abs((int)j - (int)SRFFBLURWINDOWHALF));
         fWeightSum += afWeight[j];
      } // j

      if (fWeightSum)
         fWeightSum = 1.0 / fWeightSum;

      for (j = 0; j < SRFFBLURWINDOW; j++)
         afWeight[j] *= fWeightSum;

      SRFEATUREFLOATAverage (SRFFBLURWINDOW, pSRFOrig + ((int)i - (int)SRFFBLURWINDOWHALF), &afWeight[0], pSRFNew + i);
   } // i, over features
}


/****************************************************************************
SRFEATUREFLOATToSRFEATURE - Converts floating point SRFEATUREFLOAT
   to SRFEATURE

inputs
   PSRFEATUREFLOAT   pSRFFloat - Original
   PSRFEATURE        pSRF - To be filledin
   DWORD             dwSamplesPerSec - Sampling rate. Needed for extending frequences
returns
   none
*/
void SRFEATUREFLOATToSRFEATURE (PSRFEATUREFLOAT pSRFFloat, PSRFEATURE pSRF, DWORD dwSamplesPerSec)
{
   DWORD i;
   for (i = 0; i < SRDATAPOINTS; i++) {
      // scaling factor
      fp fScaleVal = pow ((fp)2.0, ((fp)i - (fp)SRDATAPOINTS/2) / SRPOINTSPEROCTAVE);

      // interpolate energy
      pSRF->acVoiceEnergy[i] = AmplitudeToDb(pSRFFloat->afVoiceEnergy[i]);
      pSRF->acNoiseEnergy[i] = AmplitudeToDb(pSRFFloat->afNoiseEnergy[i]);
   } // i

   // do the phase
   for (i = 0; i < SRPHASENUM; i++) {
      fp fPhase = pSRFFloat->afPhase[i];
      fPhase *= 256.0 / (2.0 * PI);
      fPhase = myfmod(fPhase, 256);

      pSRF->abPhase[i] = (BYTE) (DWORD) fPhase;
   } // i

   // need to fill in high frequency areas that weren't included in
   // original sampling rate
   FXSRFEATUREExtendSingle (pSRF, dwSamplesPerSec);

#ifdef SRFEATUREINCLUDEPCM
   // conver the PCM
   float fMax = 0;
   for (i = 0; i < SRFEATUREPCM; i++)
      fMax = max(fMax, fabs(pSRFFloat->afPCM[i]));
   if (fMax)
#ifdef SRFEATUREINCLUDEPCM_SHORT
      fMax = 32767.0 / fMax; // scale
#else
      fMax = 127.0 / fMax; // scale
#endif
   for (i = 0; i < SRFEATUREPCM; i++)
#ifdef SRFEATUREINCLUDEPCM_SHORT
      pSRF->asPCM[i] = (short)floor(pSRFFloat->afPCM[i] * fMax + 0.5);
#else
      pSRF->acPCM[i] = (char)floor(pSRFFloat->afPCM[i] * fMax + 0.5);
#endif
   pSRF->fPCMScale = fMax ? (1.0 / fMax) : 0;
   pSRF->bPCMHarmFadeFull = pSRFFloat->bPCMHarmFadeFull;
   pSRF->bPCMHarmFadeStart = pSRFFloat->bPCMHarmFadeStart;
   pSRF->bPCMHarmNyquist = pSRFFloat->bPCMHarmNyquist;
   pSRF->bPCMFill = 0;
#endif
}


/****************************************************************************
SRFEATUREFLOATSharpenFormants - Sharpen the formants

inputs
   PSRFEATUREFLOAT      pSRF - Feature to modify
*/

#define FEATFORMANTDECAYLEN      (SRPOINTSPEROCTAVE)
#define FEATFORMANTDECAYLENMIN   (FEATFORMANTDECAYLEN/8)
#define FEATFORMANTSUBTRACTSCALE    0.5      // subtract the filtered amount, scaled by this much
#define FEATFORMANTMAXRESCALE       4.0      // cant rescale any more than this energy
// #define FEATFORMANTDECAY      pow((double)0.01, 1.0 / (double)SRPOINTSPEROCTAVE)   // how quickly to decay
#define FEATFORMANTDECAYINTIAL      (1.0 / (double)SRPOINTSPEROCTAVE)   // initial decay
#define FEATFORMANTDECAYPEROCTAVE   0.5   // how much every octave affects decay amount
#define FEATFORMANTDECAY_MAX  0.001 // if decayed more than this then stop

void SRFEATUREFLOATSharpenFormants (PSRFEATUREFLOAT pSRF)
{
   // figure out energy distribution per octave
   DWORD i;
   fp fEnergy, fOctave;
   int iLeft, iRight;
   double afEnergyOctaveOrig[SROCTAVE];
   memset (afEnergyOctaveOrig, 0, sizeof(afEnergyOctaveOrig));
   for (i = 0; i < SRDATAPOINTS; i++) {
      fEnergy = pSRF->afVoiceEnergy[i] + pSRF->afNoiseEnergy[i];

      // octave
      fOctave = (fp)i /(fp) SRPOINTSPEROCTAVE - 0.5;
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = floor(fOctave);
      iRight = min(iLeft+1, SROCTAVE-1);
      fOctave -= (fp) iLeft;
      afEnergyOctaveOrig[iLeft] += (1.0 - fOctave) * fEnergy;
      afEnergyOctaveOrig[iRight] += fOctave * fEnergy;
   } // i

   for (i = 0; i < SROCTAVE; i++)
      if (afEnergyOctaveOrig[i])
         break;
   if (i >= SROCTAVE)
      return;  // no energy, so dont bother

   // filter
   DWORD dwWindowSize;
   SRFEATUREFLOAT SRFNew;
   for (i = 0; i < SRDATAPOINTS; i++) {
      dwWindowSize = (DWORD)((double)FEATFORMANTDECAYLEN * (1.0 - (double)i / (double)SRDATAPOINTS) ) + FEATFORMANTDECAYLENMIN;

      int iOffset, iCur;
      double fVoiced = 0, fNoise = 0;
      DWORD dwWeightSum = 0, dwWeight;
      for (iOffset = -(int)dwWindowSize; iOffset <= (int)dwWindowSize; iOffset++) {
         iCur = (int)i + iOffset;
         if ((iCur < 0) || (iCur >= SRDATAPOINTS))
            continue;

         // weight
         dwWeight = dwWindowSize + 1 - (DWORD)abs(iOffset);
         fVoiced += (double)dwWeight * pSRF->afVoiceEnergy[iCur];
         fNoise += (double)dwWeight * pSRF->afNoiseEnergy[iCur];
         dwWeightSum += dwWeight;
      } // iOffset
      if (dwWeightSum) {
         fVoiced /= (double)dwWeightSum;
         fNoise /= (double)dwWeightSum;
      }

      SRFNew.afVoiceEnergy[i] = pSRF->afVoiceEnergy[i] * (1.0 + FEATFORMANTSUBTRACTSCALE) - fVoiced * FEATFORMANTSUBTRACTSCALE;
      SRFNew.afNoiseEnergy[i] = pSRF->afNoiseEnergy[i] * (1.0 + FEATFORMANTSUBTRACTSCALE) - fNoise * FEATFORMANTSUBTRACTSCALE;
   } // i


   // calculate the energy in the new
   double afEnergyOctaveNew[SROCTAVE];
   memset (afEnergyOctaveNew, 0, sizeof(afEnergyOctaveNew));
   for (i = 0; i < SRDATAPOINTS; i++) {
      // make sure >= 0
      SRFNew.afVoiceEnergy[i] = max(SRFNew.afVoiceEnergy[i], 0);
      SRFNew.afNoiseEnergy[i] = max(SRFNew.afNoiseEnergy[i], 0);

      fEnergy = SRFNew.afVoiceEnergy[i] + SRFNew.afNoiseEnergy[i];

      // octave
      fOctave = (fp)i /(fp) SRPOINTSPEROCTAVE - 0.5;
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = floor(fOctave);
      iRight = min(iLeft+1, SROCTAVE-1);
      fOctave -= (fp) iLeft;
      afEnergyOctaveNew[iLeft] += (1.0 - fOctave) * fEnergy;
      afEnergyOctaveNew[iRight] += fOctave * fEnergy;
   } // i

   // figure out how much to scale by
   for (i = 0; i < SROCTAVE; i++) {
      if (afEnergyOctaveNew[i])
         afEnergyOctaveOrig[i] = afEnergyOctaveOrig[i] / afEnergyOctaveNew[i];
      else
         afEnergyOctaveOrig[i] = afEnergyOctaveOrig[i] ? 100 : 1.0;

      // maximum energy scale
      afEnergyOctaveOrig[i] = max(afEnergyOctaveOrig[i], 1.0 / FEATFORMANTMAXRESCALE);
      afEnergyOctaveOrig[i] = min(afEnergyOctaveOrig[i], FEATFORMANTMAXRESCALE);
   }

   // scale
   fp fScale;
   for (i = 0; i < SRDATAPOINTS; i++) {
      fOctave = (fp)i /(fp) SRPOINTSPEROCTAVE - 0.5;
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = floor(fOctave);
      iRight = min(iLeft+1, SROCTAVE-1);
      fOctave -= (fp) iLeft;
      fScale = (1.0 - fOctave) * afEnergyOctaveOrig[iLeft] + fOctave * afEnergyOctaveOrig[iRight];

      // don't affect the lower frequencies as much since seem to creating very boomy males
      fp fScale2;
      if (i < FORMANTMUCKINGLESSATLOWFREQ)
         fScale2 = (fp) i / (fp) FORMANTMUCKINGLESSATLOWFREQ;
      else
         fScale2 = 1.0;

      pSRF->afVoiceEnergy[i] = fScale2 * fScale * SRFNew.afVoiceEnergy[i] + (1.0 - fScale2) * pSRF->afVoiceEnergy[i];
      pSRF->afNoiseEnergy[i] = fScale2 * fScale * SRFNew.afNoiseEnergy[i] + (1.0 - fScale2) * pSRF->afNoiseEnergy[i];
   } // i
}

#if 0 // dead code - doesnt work well
/****************************************************************************
SRFEATUREFLOATSharpenFormants - Sharpen the formants

inputs
   PSRFEATUREFLOAT      pSRF - Feature to modify
*/

#define FEATFORMANTDECAYLEN      SRPOINTSPEROCTAVE
// #define FEATFORMANTDECAY      pow((double)0.01, 1.0 / (double)SRPOINTSPEROCTAVE)   // how quickly to decay
#define FEATFORMANTDECAYINTIAL      (1.0 / (double)SRPOINTSPEROCTAVE)   // initial decay
#define FEATFORMANTDECAYPEROCTAVE   0.0   // how much every octave affects decay amount
#define FEATFORMANTDECAY_MAX  0.001 // if decayed more than this then stop

void SRFEATUREFLOATSharpenFormants (PSRFEATUREFLOAT pSRF)
{
   // figure out energy distribution per octave
   DWORD i;
   fp fEnergy, fOctave;
   int iLeft, iRight;
   double afEnergyOctaveOrig[SROCTAVE];
   memset (afEnergyOctaveOrig, 0, sizeof(afEnergyOctaveOrig));
   for (i = 0; i < SRDATAPOINTS; i++) {
      fEnergy = pSRF->afVoiceEnergy[i] + pSRF->afNoiseEnergy[i];

      // octave
      fOctave = (fp)i /(fp) SRPOINTSPEROCTAVE - 0.5;
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = floor(fOctave);
      iRight = min(iLeft+1, SROCTAVE-1);
      fOctave -= (fp) iLeft;
      afEnergyOctaveOrig[iLeft] += (1.0 - fOctave) * fEnergy;
      afEnergyOctaveOrig[iRight] += fOctave * fEnergy;
   } // i

   for (i = 0; i < SROCTAVE; i++)
      if (afEnergyOctaveOrig[i])
         break;
   if (i >= SROCTAVE)
      return;  // no energy, so dont bother

   // copy over SRFEATUREFLOAT to modify
   SRFEATUREFLOAT SRFNew = *pSRF;
   
   // modify
   // fp fDecayRate, fDecay;
   fp fDecayAll, fDecayAllOrig, fDecayAllExtra;
   fp fAlpha, fAlphaDelta;
   DWORD dwDecayLen;
   DWORD dwDist;
   int iCur;
   for (i = 0; i < SRDATAPOINTS; i++) {
      dwDecayLen = (DWORD)((double)FEATFORMANTDECAYLEN * pow(1.0 - FEATFORMANTDECAYPEROCTAVE, (double)i / (double)SRPOINTSPEROCTAVE));
      if (!dwDecayLen)
         continue;

      fDecayAllOrig = FEATFORMANTDECAYINTIAL * pSRF->afVoiceEnergy[i]; // BUGFIX - only decay based on voiced: (pSRF->afVoiceEnergy[i] + pSRF->afNoiseEnergy[i]);

      //fDecayRate = pow (FEATFORMANTDECAY, (double)i / (double)SRPOINTSPEROCTAVE * FEATFORMANTDECAYPEROCTAVE + 1.0);
      //fDecayAll = fDecayAllOrig;
      //fDecayAllExtra = fDecayAll * fDecayRate;

      fAlphaDelta = 1.0 / (fp)dwDecayLen;

      if (fDecayAllOrig) for (
         dwDist = 0, fAlpha = 1.0; // , fDecay = fDecayRate;
         dwDist < dwDecayLen; // (fDecay >= FEATFORMANTDECAY_MAX) && (fDecayAll > EPSILON);
         dwDist++, fAlpha -= fAlphaDelta /*, fDecay *= fDecayRate, fDecayAll *= fDecayRate, fDecayAllExtra *= fDecayRate*/) {

            fDecayAll = fDecayAllOrig * fAlpha;
            fDecayAllExtra = fDecayAllOrig * max(fAlpha-fAlphaDelta, 0.0);

            // for self, just reduce noise
            if (!dwDist) {
               SRFNew.afNoiseEnergy[i] -= fDecayAll;
               continue;
            }

            // decay to the left
            iCur = (int)i - (int)dwDist;
            if (iCur >= 0) {
               SRFNew.afVoiceEnergy[i] -= fDecayAllExtra; // decay, but less
               SRFNew.afNoiseEnergy[i] -= fDecayAll;
            }

            // decay to the right
            iCur = (int)i + (int)dwDist;
            if (iCur < SRDATAPOINTS) {
               SRFNew.afVoiceEnergy[i] -= fDecayAllExtra; // decay, but less
               SRFNew.afNoiseEnergy[i] -= fDecayAll;
            }
      }
         
   } // i

   // calculate the energy in the new
   double afEnergyOctaveNew[SROCTAVE];
   memset (afEnergyOctaveNew, 0, sizeof(afEnergyOctaveNew));
   for (i = 0; i < SRDATAPOINTS; i++) {
      // make sure >= 0
      SRFNew.afVoiceEnergy[i] = max(SRFNew.afVoiceEnergy[i], 0);
      SRFNew.afNoiseEnergy[i] = max(SRFNew.afNoiseEnergy[i], 0);

      fEnergy = SRFNew.afVoiceEnergy[i] + SRFNew.afNoiseEnergy[i];

      // octave
      fOctave = (fp)i /(fp) SRPOINTSPEROCTAVE - 0.5;
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = floor(fOctave);
      iRight = min(iLeft+1, SROCTAVE-1);
      fOctave -= (fp) iLeft;
      afEnergyOctaveNew[iLeft] += (1.0 - fOctave) * fEnergy;
      afEnergyOctaveNew[iRight] += fOctave * fEnergy;
   } // i

   // figure out how much to scale by
   for (i = 0; i < SROCTAVE; i++) {
      if (afEnergyOctaveNew[i])
         afEnergyOctaveOrig[i] = afEnergyOctaveOrig[i] / afEnergyOctaveNew[i];
      else
         afEnergyOctaveOrig[i] = afEnergyOctaveOrig[i] ? 100 : 1.0;

      // maximum energy scale
      afEnergyOctaveOrig[i] = max(afEnergyOctaveOrig[i], 1.0 / 16.0);
      afEnergyOctaveOrig[i] = min(afEnergyOctaveOrig[i], 16.0);
   }

   // scale
   fp fScale;
   for (i = 0; i < SRDATAPOINTS; i++) {
      fOctave = (fp)i /(fp) SRPOINTSPEROCTAVE - 0.5;
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = floor(fOctave);
      iRight = min(iLeft+1, SROCTAVE-1);
      fOctave -= (fp) iLeft;
      fScale = (1.0 - fOctave) * afEnergyOctaveOrig[iLeft] + fOctave * afEnergyOctaveOrig[iRight];

      pSRF->afVoiceEnergy[i] = fScale * SRFNew.afVoiceEnergy[i];
      pSRF->afNoiseEnergy[i] = fScale * SRFNew.afNoiseEnergy[i];
   } // i
}
#endif // 0

// #ifdef FEATUREHACK_NOISEATHIGHFREQ
// #define ENCOURAGENOISE(noise,freq)     ((noise) * (fp)(freq) / (fp)SRDATAPOINTS * 2.0)
   // noise = noise energy, freq = 0..SRDATAPOINTS-1, returns weighted
// #else
// #define ENCOURAGENOISE(noise,freq)     (noise)
/****************************************************************************
EncourageNoise - Encourags noise in high frequences

inputs
   fp                fNoise - Initial noise energy
   fp                fVoice - Initial voice energy
   DWORD             dwPitch - Pitch, from 0..SRDATAPOINTS
returns
   fp - New noise energy. May be more than fNoise, but never more than fNoise + fVoice
*/
__inline fp EncourageNoise (fp fNoise, fp fVoice, DWORD dwPitch)
{
#ifndef FEATUREHACK_NOISEATHIGHFREQ
   return fNoise; // keep the same
#endif

   if (!fNoise)
      return 0;   // no change

   // modify
   fp fScale = (fp)dwPitch / (fp)SRDATAPOINTS;  // so from 0..1
   fScale = fScale * 3.0 - 2.0;  // so at 2/3, is 0   // BUGFIX - Was centered at SRDATAPOINTS/2, but too much noise
   fScale *= 1.5; // BUGFIX - emphasize noise at high freq
   fScale += 1.0; // si at 2/3 pitch, will be 1.0
   if (fScale <= 0)
      return 0.0; // too low, so always voiced

   return min(fScale * fNoise, fNoise + fVoice);
}

/****************************************************************************
SRFEATUREFLOATNoiseVoiceEnergy - Calculates the noise and voice energy

inputs
   PSRFEATUREFLOAT      pSRF - Feature
   double               *pafVoice - Filled in with the voice
   double               *pafNoise - Filled in with the noise
*/
void SRFEATUREFLOATNoiseVoiceEnergy (PSRFEATUREFLOAT pSRF, double *pafVoice, double *pafNoise)
{
   double fNoiseSum = 0, fVoiceSum = 0;
   DWORD i;
   for (i = 0; i < SRDATAPOINTS; i++) {
      fp fNoiseThis = EncourageNoise (pSRF->afNoiseEnergy[i], pSRF->afVoiceEnergy[i], i); // was ENCOURAGENOISE()
      fNoiseSum += fNoiseThis;
      fVoiceSum += pSRF->afVoiceEnergy[i] + pSRF->afNoiseEnergy[i] - fNoiseThis;
   }

   *pafVoice = fVoiceSum;
   *pafNoise = fNoiseSum;
}

/****************************************************************************
SRFEATUREFLOATNoiseToVoice - Convert noise that's much quieter than voice
to voice, and if voice close to noise then convert to noise

inputs
   PSRFEATUREFLOAT      pSRF - Feature to modify
   double               fNoiseSum - Sum of this noise and surrounding ones
   double               fVoiceSum - Sum of this voice and surrounding ones
*/
void SRFEATUREFLOATNoiseToVoice (PSRFEATUREFLOAT pSRF, double fNoiseSum, double fVoiceSum)
{
#define NOISETOVOICEALL          0.5      // if noise is less than this amount of voice energy then convert all
#define NOISETOVOICESTART        (NOISETOVOICEALL*1.5)   // if noise is less than this amount then start converting
#define VOICETONOISEALL          0.15      // if 1.0 - noise is more than this much voice then convert all
#define VOICETONOISESTART        (VOICETONOISEALL * 1.5)  // start to convert

   // BUGFIX - Figure out ratio for entire area
   double afNoiseSum[SROCTAVE], afVoiceSum[SROCTAVE];
   DWORD i, dwOctave;
   memset (afNoiseSum, 0, sizeof(afNoiseSum));
   memset (afVoiceSum, 0, sizeof(afVoiceSum));
   for (i = 0; i < SRDATAPOINTS; i++) {
      dwOctave = i / SRPOINTSPEROCTAVE;
      fp fNoiseThis = EncourageNoise (pSRF->afNoiseEnergy[i], pSRF->afVoiceEnergy[i], i);
      afNoiseSum[dwOctave] += fNoiseThis; // ENCOURAGENOISE (pSRF->afNoiseEnergy[i], i);
      afVoiceSum[dwOctave] += pSRF->afVoiceEnergy[i] + pSRF->afNoiseEnergy[i] - fNoiseThis;
   }
   fp afRatioAll[SROCTAVE];
   fp fRatioAll = fVoiceSum ? (fNoiseSum / fVoiceSum) : (fNoiseSum ? 1.0 : 0.0);
   for (i = 0; i < SROCTAVE; i++) {
      if (afVoiceSum[i])
         afRatioAll[i] = afNoiseSum[i] / afVoiceSum[i];
      else
         afRatioAll[i] = afNoiseSum[i] ? 1.0 : 0.0;

      // averaege in total energy
      afRatioAll[i] = (afRatioAll[i] + fRatioAll*2.0) / 3.0;
   } // i

   float fAlpha;
   for (i = 0; i < SRDATAPOINTS; i++) {
      float fNoise = pSRF->afNoiseEnergy[i];
      float fVoice = pSRF->afVoiceEnergy[i];

      fp fOctave = (fp)i / (fp)SRPOINTSPEROCTAVE - 0.5;
      int iOctave = (int) floor (fOctave);
      int iOctaveNext = iOctave+1;
      fOctave -= (fp)iOctave;
      iOctave = max(iOctave, 0);
      iOctaveNext = max(iOctaveNext, 0);
      iOctave = min(iOctave, SROCTAVE-1);
      iOctaveNext = min(iOctaveNext, SROCTAVE-1);
      float fRatio = (1.0 - fOctave) * afRatioAll[iOctave] + fOctave * afRatioAll[iOctaveNext];  // BUGFIX - so entire wave at once

      //fRatio = fVoice ? (fNoise / fVoice) : 1.0;
      if (fRatio < NOISETOVOICESTART) {
         if (fRatio <= NOISETOVOICEALL) {
            // all to voice
            pSRF->afVoiceEnergy[i] = fVoice + fNoise;
            pSRF->afNoiseEnergy[i] = 0;
            continue;
         }

         // else, partial to voiced
         fAlpha = (fRatio - NOISETOVOICEALL) / (NOISETOVOICESTART - NOISETOVOICEALL);
         pSRF->afVoiceEnergy[i] = fVoice + (1.0 - fRatio) * fNoise;
         pSRF->afNoiseEnergy[i] = fRatio * fNoise;
         continue;
      }

      // ther way around
      fRatio = 1.0 - fRatio;
      if (fRatio < VOICETONOISESTART) {
         if (fRatio <= VOICETONOISEALL) {
            // all to voice
            pSRF->afNoiseEnergy[i] = fVoice + fNoise;
            pSRF->afVoiceEnergy[i] = 0;
            continue;
         }

         // else, partial to voiced
         fAlpha = (fRatio - VOICETONOISEALL) / (VOICETONOISESTART - VOICETONOISEALL);
         pSRF->afNoiseEnergy[i] = fNoise + (1.0 - fRatio) * fVoice;
         pSRF->afVoiceEnergy[i] = fRatio * fVoice;
         continue;
      }
   } // i

}

/****************************************************************************
SRFEATUREFLOATScaleEnergyToFFT - Rescale the energy to that of the FFT,
in case messed up energy at all

inputs
   PSRFEATUREFLOAT      pSRF - Feature, modified in place
   float                *pafEnergyOctave - Energy per octave, from FFT.
                           SROCTAVE entries
returns
   none
*/
void SRFEATUREFLOATScaleEnergyToFFT (PSRFEATUREFLOAT pSRF, float *pafEnergyOctave)
{
   // figure out the amount of energy that currently have
   double afEnergyOctaveCur[SROCTAVE];
   memset (afEnergyOctaveCur, 0, sizeof(afEnergyOctaveCur));
   DWORD i;
   fp fOctave, fEnergy;
   int iLeft, iRight;
   for (i = 0; i < SRDATAPOINTS; i++) {
      // figure out octave
      fOctave = (fp)i / (fp)SRPOINTSPEROCTAVE - 0.5;
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = (int)fOctave;
      iRight = min(iLeft+1, SROCTAVE-1);
      fOctave -= (fp)iLeft;

      // energy
      fEnergy = pSRF->afVoiceEnergy[i] + pSRF->afNoiseEnergy[i];

      // tweak
      // BUGFIX - take out tweak since seems to work best without
      // fp fScaleVal = pow ((fp)2.0, ((fp)i - (fp)SRDATAPOINTS/2) / SRPOINTSPEROCTAVE);
      // fEnergy *= fScaleVal;

      // write away
      afEnergyOctaveCur[iLeft] += (1.0 - fOctave) * fEnergy;
      afEnergyOctaveCur[iRight] += fOctave * fEnergy;
   } // i

#define MAXSCALEFORENERGY        4.0

   // figure out how much to scale
   for (i = 0; i < SROCTAVE; i++) {
      if (afEnergyOctaveCur[i])
         afEnergyOctaveCur[i] = pafEnergyOctave[i] / afEnergyOctaveCur[i];
      else
         afEnergyOctaveCur[i] = 1.0;   // no change

      // dont scale too much
      afEnergyOctaveCur[i] = min(afEnergyOctaveCur[i], MAXSCALEFORENERGY);
      afEnergyOctaveCur[i] = max(afEnergyOctaveCur[i], 1.0 / MAXSCALEFORENERGY);
   } // i

   // scale
   for (i = 0; i < SRDATAPOINTS; i++) {
      // figure out octave
      fOctave = (fp)i / (fp)SRPOINTSPEROCTAVE - 0.5;
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = (int)fOctave;
      iRight = min(iLeft+1, SROCTAVE-1);
      fOctave -= (fp)iLeft;

      // energy
      fEnergy = (1.0 - fOctave) * afEnergyOctaveCur[iLeft] + fOctave * afEnergyOctaveCur[iRight];

      pSRF->afVoiceEnergy[i] *= fEnergy;
      pSRF->afNoiseEnergy[i] *= fEnergy;
   } // i
}

/****************************************************************************
SRFEATUREFLOATReduceQuiet - Quiet areas are reduced to total silence

inputs
   PSRFEATUREFLAOT      pSRF - Feature
returns
   none
*/
void SRFEATUREFLOATReduceQuiet (PSRFEATUREFLOAT pSRF)
{
#define           VOICETOSILENCE          50.0
#define           VOICETOSILENCESTART     (VOICETOSILENCE*2.0)
#define           NOISETOSILENCE          (VOICETOSILENCE*2.0)
#define           NOISETOSILENCESTART     (NOISETOSILENCE*2.0)

#define           QUIETOPTIMALENERGY      150000.0        // optimal energy for VOICETOSILENCE
      // BUGFIX - Was 200000
#define           QUIETSCALEMINENERGY     0.1            // if enery is this low, don't scale any more

   fp fVoiceToSilence = VOICETOSILENCE;
   fp fVoiceToSilenceStart = VOICETOSILENCESTART;
   fp fNoiseToSilence = NOISETOSILENCE;
   fp fNoiseToSilenceStart = NOISETOSILENCESTART;

   DWORD i;
#ifdef FEATUREHACK_REDUCEQUIETSCALE
   // BUGFIX - So that when voice gets quiet doesn't get all muted
   double fSum = 0;
   for (i = 0; i < SRDATAPOINTS; i++)
      fSum += pSRF->afVoiceEnergy[i] + pSRF->afNoiseEnergy[i];

   fSum = fSum / QUIETOPTIMALENERGY;
   fSum = max(fSum, QUIETSCALEMINENERGY);

   fVoiceToSilence *= fSum;
   fVoiceToSilenceStart *= fSum;
   fNoiseToSilence *= fSum;
   fNoiseToSilenceStart *= fSum;
#endif
   
   fp fVoiceSubInv = 1.0 / (fVoiceToSilenceStart - fVoiceToSilence);
   fp fNoiseSubInv = 1.0 / (fNoiseToSilenceStart - fNoiseToSilence);

   for (i = 0; i < SRDATAPOINTS; i++) {
      float fVoice = pSRF->afVoiceEnergy[i], fNoise = pSRF->afNoiseEnergy[i];

      // voiced
      if (fVoice <= fVoiceToSilence)
         fVoice = 0;
      else if (fVoice <= fVoiceToSilenceStart)
         fVoice *= (fVoice - fVoiceToSilence) * fVoiceSubInv;

      // noise
      if (fNoise <= fNoiseToSilence)
         fNoise = 0;
      else if (fNoise <= fNoiseToSilenceStart)
         fNoise *= (fNoise - fNoiseToSilence) * fNoiseSubInv;

      pSRF->afVoiceEnergy[i] = fVoice;
      pSRF->afNoiseEnergy[i] = fNoise;
   } // i
}

/****************************************************************************
FFTSharpenFormants - Sharpens the formants of a FFT. Same FFT
   as passed into FFTToHarmInfo.

inputs
   DWORD          dwWindowSize - Number of elements in the paFFT
   float          *paFFT - FFT, alternating between real and imaginary
   PCMem          pMem - Scratch memory
*/
#define FORMANTDECAY       0.3         // formants assumed to decay this much each step to left/right
#define FORMANTDECAY_MIN   0.001       // if quieter than this, then stop

void FFTSharpenFormants (DWORD dwWindowSize, float *pafFFT, PCMem pMem)
{
   DWORD dwFormants = dwWindowSize/2;
   if (!pMem->Required (dwFormants * sizeof(float) * 2))
      return;  // error, shouldnt happen
   float *pafEnergyOrig = (float*)pMem->p;
   float *pafEnergyNew = pafEnergyOrig + dwFormants;

   // clear 0-wavelength
   pafFFT[0] = pafFFT[1] = 0;

   // loop and calculate the energy
   DWORD i;
   fp fOctave;
   int iLeft, iRight;
   double afEnergyOctaveOrig[SROCTAVE];
   memset (afEnergyOctaveOrig, 0, sizeof(afEnergyOctaveOrig));
   for (i = 0; i < dwFormants; i++) {
      pafEnergyOrig[i] = sqrt(pafFFT[i*2+0] * pafFFT[i*2+0] + pafFFT[i*2+1] * pafFFT[i*2+1]);

      // if i == 0 then not in octave
      if (!i)
         continue;

      // store energy in octave
      fOctave = log((double)i/2.0) / log((double)2.0);
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = (int)floor(fOctave);
      iRight = iLeft+1;
      iLeft = max(iLeft, 0);
      iRight = max(iRight, 0);
      iLeft = min(iLeft, SROCTAVE-1);
      iRight = min(iRight, SROCTAVE-1);
      fOctave -= (fp) iLeft;
      afEnergyOctaveOrig[iLeft] += (1.0 - fOctave) * pafEnergyOrig[i];
      afEnergyOctaveOrig[iRight] += fOctave * pafEnergyOrig[i];
   } // i

   // copy over original energy to new
   memcpy (pafEnergyNew, pafEnergyOrig, dwFormants * sizeof(float));

   // loop over all formants
   DWORD dwDist;
   fp fDecay, fDecayFull;
   int iCur;
   for (i = 1; i < dwFormants; i++) {  // NOTE: Starting at 1
      for (
         dwDist = 1, fDecayFull = FORMANTDECAY * pafEnergyOrig[i], fDecay = FORMANTDECAY;
         fDecay > FORMANTDECAY_MIN;
         dwDist++, fDecay *= FORMANTDECAY, fDecayFull *= FORMANTDECAY) {

            // decay to left
            iCur = (int)i - (int)dwDist;
            if (iCur > 0)  // dont bother modifying 0
               pafEnergyNew[iCur] -= fDecayFull;

            // decay to right
            iCur = (int)i + (int)dwDist;
            if (iCur < (int)dwFormants)
               pafEnergyNew[iCur] -= fDecayFull;
      }
   } // i

   // zero out negative formants and calulate energy per octave
   double afEnergyOctaveNew[SROCTAVE];
   memset (afEnergyOctaveNew, 0, sizeof(afEnergyOctaveNew));
   for (i = 0; i < dwFormants; i++) {
      if (pafEnergyNew[i] <= 0.0) {
         pafEnergyNew[i] = 0.0;
         continue;
      }

      fOctave = log((double)i/2.0) / log((double)2.0);
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = (int)floor(fOctave);
      iRight = iLeft+1;
      iLeft = max(iLeft, 0);
      iRight = max(iRight, 0);
      iLeft = min(iLeft, SROCTAVE-1);
      iRight = min(iRight, SROCTAVE-1);
      fOctave -= (fp) iLeft;
      afEnergyOctaveNew[iLeft] += (1.0 - fOctave) * pafEnergyNew[i];
      afEnergyOctaveNew[iRight] += fOctave * pafEnergyNew[i];
   } // i

   // figure out a scale for energy
   for (i = 0; i < SROCTAVE; i++) {
      if (afEnergyOctaveNew[i])
         afEnergyOctaveOrig[i] = afEnergyOctaveOrig[i] / afEnergyOctaveNew[i];
      else
         afEnergyOctaveOrig[i] = afEnergyOctaveOrig[i] ? 100 : 1.0;

      // maximum energy scale
      afEnergyOctaveOrig[i] = max(afEnergyOctaveOrig[i], 1.0 / 4.0);
      afEnergyOctaveOrig[i] = min(afEnergyOctaveOrig[i], 4.0);
   }

   // scale
   fp fScale;
   for (i = 0; i < dwFormants; i++) {
      if (!i)
         continue;

      fOctave = log((double)i/2.0) / log((double)2.0);
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = (int)floor(fOctave);
      iRight = iLeft+1;
      iLeft = max(iLeft, 0);
      iRight = max(iRight, 0);
      iLeft = min(iLeft, SROCTAVE-1);
      iRight = min(iRight, SROCTAVE-1);
      fOctave -= (fp) iLeft;

      fScale = (1.0 - fOctave) * afEnergyOctaveOrig[iLeft] + fOctave * afEnergyOctaveOrig[iRight];
      fScale *= pafEnergyNew[i];
      if (pafEnergyOrig[i])
         fScale /= pafEnergyOrig[i];
      else
         fScale = 1.0;  // since nothing to scale

      pafFFT[i*2+0] *= fScale;
      pafFFT[i*2+1] *= fScale;
   } // i

   // done
}

/****************************************************************************
CM3DWave::HarmInfoFromStretch - Calculates harmonic info by
stretching from an original wave source.

inputs
   DWORD          dwNum - Number of samples in pasWave
   short          *pasWave - Original PCM
   DWORD          dwCenter - Center sample index into pasWave
   double         fWavelength - Wavelength that looking to FFT
   DWORD          dwNumHI - Number of harmonic info
   PHARMINFO      pHI - array that's filled in
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
   DWORD             dwCalcSRFeatPCM - Was constant, size of stretched wave
returns
   float - Original phase of the fundamental
*/
float CM3DWave::HarmInfoFromStretch (DWORD dwNum, short *pasWave, DWORD dwCenter, double fWavelength,
                                    DWORD dwNumHI, PHARMINFO pHI,
                                    PCSinLUT pLUT, PCMem pMemFFTScratch,
                                    DWORD dwCalcSRFeatPCM)
{
   DWORD dwWindowSize = dwCalcSRFeatPCM*2;   // twice the wavelength
   float afFFT[CALCSRFEATUREPCMMAX*2];  // to store FFT

   // fill in
   double fDelta = fWavelength / (double)(dwWindowSize/2);
   double fCur = (double)dwCenter - fDelta * (double)(dwWindowSize/2);
   DWORD i;
   BOOL fNotZero = FALSE;
   for (i = 0; i < dwWindowSize; i++, fCur += fDelta) {
      int iLeft = (int)floor(fCur);
      int iRight = iLeft+1;
      double fAlpha = fCur - (double)iLeft;
      double fLeft = ((iLeft >= 0) && (iLeft < (int)dwNum)) ? pasWave[iLeft] : 0;
      double fRight = ((iRight >= 0) && (iRight < (int)dwNum)) ? pasWave[iRight] : 0;

      afFFT[i] = (1.0 - fAlpha) * fLeft + fAlpha * fRight;
      if (afFFT[i])
         fNotZero = TRUE;
   } // i

   // optimize: If all zeros then do nothing
   if (!fNotZero) {
      memset (pHI, 0, dwNumHI * sizeof(HARMINFO));
      return 0;
   }

   // do the FFT
   FFTRecurseReal (&afFFT[0] - 1, dwWindowSize, 1, pLUT, pMemFFTScratch);

#ifdef FEATUREHACK_SHARPENFORMANTSINFFT
   FFTSharpenFormants (dwWindowSize, &afFFT[0], pMemFFTScratch);
#endif

   // fill in the harmonics
   return FFTToHarmInfo (dwNumHI, pHI, dwWindowSize, &afFFT[0]);
}



/****************************************************************************
CM3DWave::PCMToSRFEATUREFocusInOnPitch - Focuses in on a pitch

inputs
   fp                fOctaveMin - Minimum octave (inclusive), in octaves
   fp                fOctaveMax - Maximum octave (inclusive), in octaves
   fp                fNoiseMin - Noise at the minimum octave, or -1 if haven't calcualted
   fp                fNoiseCenter - Noise at the center (between min and max), or -1 if haven't calculated
   fp                fNoiseMax - Noise at the maxmimum octave, or -1 if haven't calcualted
   fp                fMinDelta - How much can hone in before stops searching deeper
   DWORD             dwNum - Number of samples
   short             *pasWave - Samples
   DWORD             dwCenter - Center point
   PCSinLUT       pLUT - Sine lookup to use for scratch
   PCMem          pMemFFTScratch - FFT stractch
   DWORD             dwCalcSRFeatPCM - Was constant, size of stretched wave
returns
   fp - Octave with the lowest noise
*/

fp CM3DWave::PCMToSRFEATUREFocusInOnPitch (fp fOctaveMin, fp fOctaveMax, fp fNoiseMin, fp fNoiseCenter, fp fNoiseMax,
                                           fp fMinDelta, DWORD dwNum, short *pasWave, DWORD dwCenter,
                                           PCSinLUT pLUT, PCMem pMemFFTScratch,
                                           DWORD dwCalcSRFeatPCM)
{
#define FOCUSSUBDIVIDE     7     // try 5 differen values and see which is the best. MUST be odd
#define FOCUSSUBDIVIDE_CENTER ((FOCUSSUBDIVIDE-1)/2)  // center

   double afNoise[FOCUSSUBDIVIDE];
   double afOctave[FOCUSSUBDIVIDE];
   double fDelta = (fOctaveMax - fOctaveMin) / (fp)(FOCUSSUBDIVIDE-1);
   double fNoiseCur;
   HARMINFO aHI[SRFEATANAL_HARMONICSMAX];

   // loop over frequences
   DWORD i, j;
   DWORD dwBest = 0;
   for (i = 0; i < FOCUSSUBDIVIDE; i++) {
      afOctave[i] = fOctaveMin + fDelta * (fp)i;

      // see if have already calcualted
      if ((i == 0) && (fNoiseMin >= 0)) {
         afNoise[i] = fNoiseMin;
         continue;
      }
      else if ((i == FOCUSSUBDIVIDE_CENTER) && (fNoiseCenter >= 0)) {
         afNoise[i] = fNoiseCenter;
         continue;
      }
      else if ((i == FOCUSSUBDIVIDE-1) && (fNoiseMax >= 0)) {
         afNoise[i] = fNoiseMax;
         continue;
      }

      // wavelength
      double fWavelength = pow ((double)2.0, (double)afOctave[i]) * (double)dwCalcSRFeatPCM;

      // else, calculate 
      HarmInfoFromStretch (dwNum, pasWave, dwCenter, fWavelength, SRFEATANAL_HARMONICS, &aHI[0], pLUT, pMemFFTScratch,
         dwCalcSRFeatPCM);

      // find the noise
      fNoiseCur = 0;
      for (j = 0; j < SRFEATANAL_HARMONICS; j++)
         fNoiseCur += aHI[j].fUnvoiced;
      afNoise[i] = fNoiseCur;

      // remember which is best
      if (afNoise[i] < afNoise[dwBest])
         dwBest = i;
   } // i

   // if too deep already then done
   if (fDelta <= fMinDelta)
      return afOctave[dwBest];

   // if no noise then already done
   if (afNoise[dwBest] < EPSILON)
      return afOctave[dwBest];

   // else, recurse
   return PCMToSRFEATUREFocusInOnPitch (afOctave[dwBest] - fDelta, afOctave[dwBest] + fDelta,
      dwBest ? afNoise[dwBest-1] : -1, afNoise[dwBest], (dwBest+1 < FOCUSSUBDIVIDE) ? afNoise[dwBest+1] : -1,
      fMinDelta, dwNum, pasWave, dwCenter, pLUT, pMemFFTScratch, dwCalcSRFeatPCM);
}


/****************************************************************************
CM3DWave::PCMToSRFEATURE - Takes an index into a wave's PCM and creates
an SRFEATURE out of it

inputs
   DWORD             dwCalcFor - Use WAVECALC_XXX
   int               iCenter - Center sample
   fp                fPitch - Pitch to use for this. However, assuming
                     this is a Monotone() wave where the new wavelength
                     is dwCalcSRFeatPCM
   DWORD             dwSamplesPerSecOrig - Original samples per second, before upsample
   PSRFEATURE        pSRF - Filled in
   PHARMINFO         paHI - Filled in so can use the phase information later.
                              Must have SRFEATANAL_HARMONICS elements.
   float             *pafEnergyOctave - Array of SROCTAVE that's filled with per-octave
                        energy of voice. Only if FEATUREHACK_SCALEENERGYTOFFT
   PCSinLUT          pLUT - Sine lookup to use for scratch
   PCMem             pMemFFTScratch - FFT stractch
   DWORD             dwCalcSRFeatPCM - Was constant, size of stretched wave
   fp                *pfPitchFineTune - Filled with how much fine-tuned initial pitch up (positive) or down.
returns
   none
*/
void CM3DWave::PCMToSRFEATUREFLOAT (DWORD dwCalcFor, int iCenter, fp fPitch,
                                    DWORD dwSamplesPerSecOrig, PSRFEATUREFLOAT pSRF, PHARMINFO paHI,
                                    float *pafEnergyOctave,
                                    PCSinLUT pLUT, PCMem pMemFFTScratch, DWORD dwCalcSRFeatPCM,
                                    fp *pfPitchFineTune)
{
#ifdef FEATUREHACK_FINETUNEPITCH
#define SRFEATANAL_OCTAVERANGE          0.2          // 10% of an octave in either dir
#define SRFEATANAL_OCTAVERANGEPASSTWO   0.025          // 1.25% of an octave in either dir
#else
#define SRFEATANAL_OCTAVERANGE          0.0
#define SRFEATANAL_OCTAVERANGEPASSTWO   0.0
#endif

#define SRFEATANAL_OCTAVEACCURACY       0.001         // accuracy
#define SRFEATANAL_OCTAVEACCURACYPASSTWO  SRFEATANAL_OCTAVEACCURACY

   *pfPitchFineTune = 1.0; // no fine-une

   BOOL fFullPCM = FALSE;
   BOOL fNoPCMAtAll = FALSE;

   DWORD dwSamples = dwCalcSRFeatPCM * 5; // so can do the windowed FFT later
   short asSamples[CALCSRFEATUREPCMMAX*5];

   DWORD i, j;
   int iCur;
   for (i = 0, iCur = iCenter - (int)dwSamples/2; i < dwSamples; i++, iCur++)
      asSamples[i] = ((iCur >= 0) && (iCur < (int)m_dwSamples)) ? m_psWave[iCur * (int)m_dwChannels] : 0;

   // create the window
   DWORD dwWindowSize = dwCalcSRFeatPCM*2;
   DWORD dwWindowSizeEnergy = dwWindowSize * 2;
   CMem memNorm;
   if (!memNorm.Required (
      max(dwWindowSize,dwWindowSizeEnergy) * sizeof(float) * 2 +
      dwWindowSize * 5 * sizeof(float) + dwWindowSizeEnergy * sizeof(float)))
      return;  // shouldnt happen
   float *pafWindow = (float*)memNorm.p;
   float *pafWindowNoSqrt = pafWindow + max(dwWindowSize, dwWindowSizeEnergy);
   float *pafFFT[4];
   pafFFT[0] = pafWindowNoSqrt + max(dwWindowSize, dwWindowSizeEnergy);
   pafFFT[1] = pafFFT[0] + dwWindowSize;
   pafFFT[2] = pafFFT[1] + dwWindowSize;
   pafFFT[3] = pafFFT[2] + dwWindowSize;  // for energy. center one, but with full window, or energy
   float *pafPCM = pafFFT[3] + dwWindowSizeEnergy;  // for restoring filtering final wave by top harmonics
   float *pafPCMNoSqrt = pafPCM + dwWindowSize;  // for PCM, unfiltered

   DWORD dwOffset;
   fp fEnergy;
   int iLeft, iRight;
#ifdef FEATUREHACK_SCALEENERGYTOFFT
   CreateFFTWindow (3, pafWindow, dwWindowSizeEnergy);

   // do FFT before sqrt-window
   dwOffset = dwSamples / 2 - dwWindowSizeEnergy / 2;
   for (j = 0; j < dwWindowSizeEnergy; j++)
      pafFFT[3][j] = (float)asSamples[dwOffset + j] * pafWindow[j];
   FFTRecurseReal (&pafFFT[3][0] - 1, dwWindowSizeEnergy, 1, pLUT, pMemFFTScratch);

   // figure out energy per octave
   fp fOctave;
   memset (pafEnergyOctave, 0, sizeof(pafEnergyOctave[0]) * SROCTAVE);
   fp fScale = 2.0 / (fp) dwWindowSizeEnergy;// * 32768.0 / 32768.0;
      // NOTE - Can't tweak scale so works equally well for all voices
   for (i = 0; i < dwWindowSizeEnergy/2; i++) {
      if (!i)
         continue;   // DC offset

      fOctave = log((double)i / (fp)(dwWindowSizeEnergy / dwCalcSRFeatPCM) * fPitch / SRBASEPITCH) / log(2.0) - 0.5;
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      iLeft = (int)fOctave;
      iRight = min(iLeft+1, SROCTAVE-1);
      fOctave -= (fp)iLeft;

      fEnergy = sqrt(pafFFT[3][i*2+0] * pafFFT[3][i*2+0] + pafFFT[3][i*2+1] * pafFFT[3][i*2+1]) * fScale;
      pafEnergyOctave[iLeft] += (1.0 - fOctave) * fEnergy;
      pafEnergyOctave[iRight] += fOctave * fEnergy;
   } // i
#endif // FEATUREHACK_SCALEENERGYTOFFT


   // sqrt since will be end up multiplying twice
   CreateFFTWindow (3, pafWindow, dwWindowSize);
   for (i = 0; i < dwWindowSize; i++)
      pafWindow[i] = sqrt(pafWindow[i]);
#ifdef FEATUREHACK_PHASEFROMONETRY
   CreateFFTWindow (3, pafWindowNoSqrt, dwWindowSize, FALSE);
#endif

   // do the 4th one as the center
   dwOffset = dwSamples / 2 - dwWindowSize / 2;
   for (j = 0; j < dwWindowSize; j++) {
      pafPCM[j] = (float)asSamples[dwOffset + j] * pafWindow[j];

      // the extra sample is one to the right or left
      DWORD dwExtraSample = (j + dwCalcSRFeatPCM) % dwWindowSize;
      pafPCMNoSqrt[j] = (float)asSamples[dwOffset + j] * pafWindowNoSqrt[j] +
         (float)asSamples[dwOffset + dwExtraSample] * pafWindowNoSqrt[dwExtraSample];
   }
   FFTRecurseReal (&pafPCM[0] - 1, dwWindowSize, 1, pLUT, pMemFFTScratch);

#ifdef FEATUREHACK_PHASEFROMONETRY
   FFTRecurseReal (&pafPCMNoSqrt[0] - 1, dwCalcSRFeatPCM, 1, pLUT, pMemFFTScratch);
      // intentionally doing single wave
#endif

#ifdef FEATUREHACK_NORMALIZEVOLUME

   // do all the FFTs
   DWORD adwOffset[3];
   for (i = 0; i < 3; i++) {
      dwOffset = adwOffset[i] = dwSamples / 2 - dwWindowSize + dwWindowSize / 2 * i;

      if (i == 1) {
         // already calculated, so just copy over
         memcpy (pafFFT[1], pafPCM, dwWindowSize * sizeof(float));
         continue;
      }

      for (j = 0; j < dwWindowSize; j++)
         pafFFT[i][j] = (float)asSamples[dwOffset + j] * pafWindow[j];

      // fft it
      FFTRecurseReal (&pafFFT[i][0] - 1, dwWindowSize, 1, pLUT, pMemFFTScratch);
   } // i

   // find the energies in bins
   fp fBin; // , fEnergy;
   // int iLeft, iRight;
#define NORMBINSMAX        (CALCSRFEATUREPCMMAX/8) // number of bins for normalized energies
#define NORMBINS        (dwCalcSRFeatPCM/8) // number of bins for normalized energies
   double afEnergySumOrig[3][NORMBINSMAX];
   memset (afEnergySumOrig, 0, sizeof(afEnergySumOrig));
   for (i = 0; i < 3; i++) {
      for (j = 0; j < dwWindowSize / 2; j++) {
         // find the location to add the energy
         fBin = (fp)j / (fp)(dwWindowSize/2) * (fp)NORMBINS - 0.5;
         fBin = max(fBin, 0.0);
         iLeft = (int)fBin;
         iRight = min(iLeft + 1, (int)NORMBINS-1);
         fBin -= (fp)iLeft;

         // add it
         fEnergy = sqrt(pafFFT[i][j*2+0] * pafFFT[i][j*2+0] + pafFFT[i][j*2+1] * pafFFT[i][j*2+1]);
         afEnergySumOrig[i][iLeft] += (1.0 - fBin) * fEnergy;
         afEnergySumOrig[i][iRight] += fBin * fEnergy;
      } // j
   } // i

   // figure out how much should change the volume
   for (i = 0; i < 3; i += 2) {  // to skip the center one
      for (j = 0; j < NORMBINS; j++) {
         if (afEnergySumOrig[i][j])
            afEnergySumOrig[i][j] = afEnergySumOrig[1][j] / afEnergySumOrig[i][j];  // so scale
         else
            afEnergySumOrig[i][j] = 1.0; // dont bother scaling because no energy

         // don't scale too much
         afEnergySumOrig[i][j] = max(afEnergySumOrig[i][j], 1.0 / 4.0);
         afEnergySumOrig[i][j] = min(afEnergySumOrig[i][j], 4.0);
      } // j
   } // i

   // scale left and right
   for (i = 0; i < 3; i += 2) {  // to skip the center one
      for (j = 0; j < dwWindowSize / 2; j++) {
         // find the location to add the energy
         fBin = (fp)j / (fp)(dwWindowSize/2) * (fp)NORMBINS - 0.5;
         fBin = max(fBin, 0.0);
         iLeft = (int)fBin;
         iRight = min(iLeft + 1, (int)NORMBINS-1);
         fBin -= (fp)iLeft;

         // figure out scale
         fp fScale = (1.0 - fBin) * afEnergySumOrig[i][iLeft] + fBin * afEnergySumOrig[i][iRight];
         if (fScale != 1.0) {
            // scale
            pafFFT[i][j*2+0] *= fScale;
            pafFFT[i][j*2+1] *= fScale;
         }
      } // j
   } // i

   // restore the values
   for (i = 0; i < 3; i++)
      FFTRecurseReal (&pafFFT[i][0] - 1, dwWindowSize, -1, pLUT, pMemFFTScratch);

   // write them
   for (i = 0; i < dwSamples; i++) {
      fp fValue = 0;
      for (j = 0; j < 3; j++) {
         // make sure in range
         if ((i < adwOffset[j]) || (i >= adwOffset[j] + dwWindowSize))
            continue;

         fValue += pafFFT[j][i - adwOffset[j]] * pafWindow[i - adwOffset[j]];

      } // j

      fValue /= (fp)dwWindowSize;

      fValue = max(fValue, -32768);
      fValue = min(fValue, 32767);
      asSamples[i] = (short)fValue;
   } // i

   // float   fScale = 2.0 / (float) dwFFTSize;// * 32768.0 / 32768.0;

#endif

   // accuracy and range
   fp fAccuracy = SRFEATANAL_OCTAVEACCURACY;
   fp fRange = SRFEATANAL_OCTAVERANGE;

   switch (dwCalcFor) {
   case WAVECALC_TRANSPROS:
   case WAVECALC_VOICECHAT:
      // faster
      fAccuracy *= 2.0;
      fRange /= 2.0;
      fNoPCMAtAll = TRUE; // BUGFIX - No PCM at all for transplanted prosody and voice chat
      break;

   case WAVECALC_TTS_PARTIALPCM:
      // no change
      break;

   case WAVECALC_SEGMENT:
      fNoPCMAtAll = TRUE; // BUGFIX - No PCM at all for segmenting (also default wave editor)
      // no change
      break;

   case WAVECALC_TTS_FULLPCM:
      fFullPCM = TRUE;
      break;
   }

   // fFullPCM = TRUE;  // to test BUGBUG
   // fNoPCMAtAll = FALSE; // to test BUGBUG

   // try to zoom in on the wavelength with the least noise
   fp fCenter = PCMToSRFEATUREFocusInOnPitch (-fRange/2.0, fRange/2.0,
      -1, -1, -1, fAccuracy,
      dwSamples, &asSamples[0], dwSamples/2, pLUT, pMemFFTScratch, dwCalcSRFeatPCM);

   // different accuracy and range
   fAccuracy = SRFEATANAL_OCTAVEACCURACYPASSTWO;
   fRange = SRFEATANAL_OCTAVERANGEPASSTWO;

   switch (dwCalcFor) {
   case WAVECALC_TRANSPROS:
   case WAVECALC_VOICECHAT:
      // faster
      fAccuracy *= 2.0;
      fRange /= 2.0;
      break;

   case WAVECALC_TTS_PARTIALPCM:
   case WAVECALC_TTS_FULLPCM:
   case WAVECALC_SEGMENT:
      // no change
      break;
   }

   fp fStart = fCenter - fRange/2.0;
   DWORD dwNumTries = (DWORD)(fRange / fAccuracy) + 1;

#define MAXFEATURETRIES       100      // so can keep an array. This should be more than large enough
   _ASSERTE (dwNumTries <= MAXFEATURETRIES);
   dwNumTries = min(dwNumTries, MAXFEATURETRIES);
   fp afPitchTry[MAXFEATURETRIES];
   DWORD adwBestTry[SRFEATANAL_HARMONICSMAX];
   memset (afPitchTry, 0, sizeof(afPitchTry));
   memset (adwBestTry, 0, sizeof(adwBestTry));

#ifdef OLDFEATUREHACK_PHASEFROMONETRY
   // remember all the phases, along with the amount of noise in each attempt
   CMem memPhase;
   DWORD dwNeed = dwNumTries * sizeof(float) * (SRFEATANAL_HARMONICS*3 + 2);
   if (!memPhase.Required (dwNeed))
      return;  // error
   memset (memPhase.p, 0, dwNeed);
   float *pafPhaseNoiseSum = (float*)memPhase.p;
   float *pafPhaseFund = pafPhaseNoiseSum + dwNumTries;
   float *pafPhaseStore = pafPhaseFund + dwNumTries;
#endif

   // loop over a number of different wavelengths
   HARMINFO aHIBest[SRFEATANAL_HARMONICSMAX];
   HARMINFO aHICur[SRFEATANAL_HARMONICSMAX];
   fp fPhaseFund = 0;
   for (i = 0; i < dwNumTries; i++) {
      fp fCurPitch = fStart + fAccuracy * (fp)i;
      afPitchTry[i] = fCurPitch;
      double fWavelength = pow ((double)2.0, (double)fCurPitch) * (double)dwCalcSRFeatPCM;

      // just find one harmonic
      fp fPhaseFundTemp = HarmInfoFromStretch (dwSamples, &asSamples[0], dwSamples/2, fWavelength,
         SRFEATANAL_HARMONICS, &aHICur[0], pLUT, pMemFFTScratch, dwCalcSRFeatPCM);

      // calculate all the phases
      for (j = 0; j < SRFEATANAL_HARMONICS; j++) {
         aHICur[j].afVoicedSinCos[0] = sin(aHICur[j].fPhase) * aHICur[j].fVoiced;
         aHICur[j].afVoicedSinCos[1] = cos(aHICur[j].fPhase) * aHICur[j].fVoiced;
      }

#ifdef OLDFEATUREHACK_PHASEFROMONETRY
      pafPhaseFund[i] = fPhaseFundTemp;
      for (j = 0; j < SRFEATANAL_HARMONICS; j++) {
         // keep sum of unvoiced
         pafPhaseNoiseSum[i] += aHICur[j].fUnvoiced;
         pafPhaseStore[i * SRFEATANAL_HARMONICS * 3 + j * 3 + 0] = aHICur[j].fPhase;
         pafPhaseStore[i * SRFEATANAL_HARMONICS * 3 + j * 3 + 1] = aHICur[j].afVoicedSinCos[0];
         pafPhaseStore[i * SRFEATANAL_HARMONICS * 3 + j * 3 + 2] = aHICur[j].afVoicedSinCos[1];
      } // j
#endif

      // if this is the first time then keep this as the best
      if (!i) {
         memcpy (aHIBest, aHICur, sizeof(aHICur));
         fPhaseFund = fPhaseFundTemp;
         continue;
      }

      // else, take the ones with the lowest noise
      for (j = 0; j < SRFEATANAL_HARMONICS; j++) {
         // sum phases together so more stable
#ifndef FEATUREHACK_PHASEFROMONETRY
#ifdef FEATUREHACK_SUMPHASEOVERWAVELENGTH
         aHIBest[j].afVoicedSinCos[0] += aHICur[j].afVoicedSinCos[0];
         aHIBest[j].afVoicedSinCos[1] += aHICur[j].afVoicedSinCos[1];
#endif
#endif // FEATUREHACK_PHASEFROMONETRY

         if (aHICur[j].fUnvoiced < aHIBest[j].fUnvoiced ) {
            if (j == 1)
               fPhaseFund = fPhaseFundTemp;

            aHIBest[j].fPhase = aHICur[j].fPhase;
            aHIBest[j].fVoiced = aHICur[j].fVoiced;
            aHIBest[j].fUnvoiced = aHICur[j].fUnvoiced;

            adwBestTry[j] = i;

#ifndef FEATUREHACK_PHASEFROMONETRY
#ifndef FEATUREHACK_SUMPHASEOVERWAVELENGTH
            aHIBest[j].afVoicedSinCos[0] = aHICur[j].afVoicedSinCos[0];
            aHIBest[j].afVoicedSinCos[1] = aHICur[j].afVoicedSinCos[1];
#endif
#endif // FEATUREHACK_PHASEFROMONETRY
         }

         //if (aHICur[j].fUnvoiced < aHIBest[j].fUnvoiced)
         //   aHIBest[j].fUnvoiced = aHICur[j].fUnvoiced;
         //if (aHICur[j].fVoiced > aHIBest[j].fVoiced) {
         //   aHIBest[j].fVoiced = aHICur[j].fVoiced;
         //   aHIBest[j].fPhase = aHICur[j].fPhase;
         //}

         //if ((aHICur[j].fUnvoiced / max(aHICur[j].fUnvoiced + aHICur[j].fVoiced, EPSILON)) <
         //    (aHIBest[j].fUnvoiced / max(aHIBest[j].fUnvoiced + aHIBest[j].fVoiced, EPSILON)) )
         //   aHIBest[j] = aHICur[j];
      }
   } // i

   // determine the pitch as accurately as possuble
   double fPitchSum = 0.0;
   double fPitchSumWeight = 0.0;
   for (j = 0; j < SRFEATANAL_HARMONICS; j++) {
      double fEnergy = aHIBest[j].fVoiced + aHIBest[j].fUnvoiced;
      fPitchSum += afPitchTry[adwBestTry[j]] * fEnergy;
      fPitchSumWeight += fEnergy;
   } // i
   if (fPitchSumWeight > CLOSE)  // BUGFIX - Was erroneously testing fEnergy
      *pfPitchFineTune = pow(2.0, fPitchSum / fPitchSumWeight);

#ifdef OLDFEATUREHACK_PHASEFROMONETRY
   // find the best phase
   DWORD dwBestPhase = 0;
   for (i = 1; i < dwNumTries; i++) // intentionally starting at 1
      if (pafPhaseNoiseSum[i] < pafPhaseNoiseSum[dwBestPhase])
         dwBestPhase = i;
   for (j = 0; j < SRFEATANAL_HARMONICS; j++) {
      aHIBest[j].fPhase = pafPhaseStore[dwBestPhase * SRFEATANAL_HARMONICS * 3 + j * 3 + 0];
      aHIBest[j].afVoicedSinCos[0] = pafPhaseStore[dwBestPhase * SRFEATANAL_HARMONICS * 3 + j * 3 + 1];
      aHIBest[j].afVoicedSinCos[1] = pafPhaseStore[dwBestPhase * SRFEATANAL_HARMONICS * 3 + j * 3 + 2];
   } // j
   fPhaseFund = pafPhaseFund[dwBestPhase];
#endif // FEATUREHACK_PHASEFROMONETRY

#ifdef FEATUREHACK_PHASEFROMONETRY
   fPhaseFund = 0.0;
   for (j = 0; j < SRFEATANAL_HARMONICS; j++) {
      aHIBest[j].fPhase = (j*2+1 < dwCalcSRFeatPCM/2) ? atan2(pafPCMNoSqrt[j*2], pafPCMNoSqrt[j*2+1]) : 0;

      if (j == 1)
         fPhaseFund = aHIBest[j].fPhase;

      // remove the funamental phase
      aHIBest[j].fPhase -= fPhaseFund * (float)j;

      // new sincos for phase
      aHIBest[j].afVoicedSinCos[0] = sin(aHIBest[j].fPhase) * aHIBest[j].fVoiced;
      aHIBest[j].afVoicedSinCos[1] = cos(aHIBest[j].fPhase) * aHIBest[j].fVoiced;
   } // j
#endif

   // remove energy above nyquist limit
   fp fNyquist = (fp)dwSamplesPerSecOrig / 2.0 / fPitch;
   for (i = 0; i < SRFEATANAL_HARMONICS; i++)
      if ((fp)i >= fNyquist)
         aHIBest[i].fVoiced = aHIBest[i].fUnvoiced = 0;

#define GOTOVOICEDDIVIDE      0.3         // when to encourage a split between voiced and unvoiced
#define CONVERTNOISETOVOICEBELOW 0.5      // convert noise to voice below this level
#define NOISETENDSTOBEABOVE   0.66        // noise tends to be this high or above

#ifdef FEATUREHACK_ALLNOISEORALLVOICE
   fp fVoicedSum = EPSILON, fUnvoicedSum = EPSILON;
   for (i = 0; i < SRFEATANAL_HARMONICS; i++) {
      fp fOctave = fPitch * (fp)(i+1);
      fOctave = log(fOctave) / log(2.0) - log((fp)SRBASEPITCH) / log(2.0);
      fOctave = max(fOctave, 0);
      fOctave = min(fOctave, SROCTAVE-1);
      fOctave = (fp)fOctave / (fp)(SROCTAVE-1);

      // automatically convert lower-frequency sounds to voiced (if they're noise)
      if ((fOctave < CONVERTNOISETOVOICEBELOW) && aHIBest[i].fUnvoiced) {
         fp fNoiseToVoice = 1.0 - fOctave / CONVERTNOISETOVOICEBELOW;
         fNoiseToVoice = sqrt(fNoiseToVoice);   // very quickly ramp up
         aHIBest[i].fVoiced += aHIBest[i].fUnvoiced * fNoiseToVoice;
         aHIBest[i].fUnvoiced *= (1.0 - fNoiseToVoice);
      }

      // what percentage of the overall audio is voiced
      fp fWeightVoiced;
      if (fOctave > NOISETENDSTOBEABOVE)
         fWeightVoiced = (fOctave - NOISETENDSTOBEABOVE) / (1.0 - NOISETENDSTOBEABOVE) * 0.5 + 0.5;
      else
         fWeightVoiced = fOctave / NOISETENDSTOBEABOVE * 0.5;
      fWeightVoiced = 1.0 - fWeightVoiced;

      fVoicedSum += aHIBest[i].fVoiced * fWeightVoiced;
      fUnvoicedSum += aHIBest[i].fUnvoiced * (1.0 - fWeightVoiced);
   } // i
   fVoicedSum /= (fVoicedSum + fUnvoicedSum);   // so 1.0 = fully voiced, 0.0 = unvoiced

   fp fConvertVoicedToUnvoiced = (fVoicedSum < GOTOVOICEDDIVIDE) ? ((GOTOVOICEDDIVIDE - fVoicedSum) / GOTOVOICEDDIVIDE ) : 0.0;
   fp fConvertUnvoicedToVoiced = (fVoicedSum > GOTOVOICEDDIVIDE) ? ((fVoicedSum - GOTOVOICEDDIVIDE) / (1.0 - GOTOVOICEDDIVIDE) ) : 0.0;
   fConvertVoicedToUnvoiced = pow(fConvertVoicedToUnvoiced, 0.25);  // encourage extremes
   fConvertUnvoicedToVoiced = pow(fConvertUnvoicedToVoiced, 0.25);

   for (i = 0; i < SRFEATANAL_HARMONICS; i++) {
      if (fConvertVoicedToUnvoiced) {
         aHIBest[i].fUnvoiced += aHIBest[i].fVoiced * fConvertVoicedToUnvoiced;
         aHIBest[i].fVoiced *= (1.0 - fConvertVoicedToUnvoiced);
      }
      else if (fConvertUnvoicedToVoiced) {
         aHIBest[i].fVoiced += aHIBest[i].fUnvoiced * fConvertUnvoicedToVoiced;
         aHIBest[i].fUnvoiced *= (1.0 - fConvertUnvoicedToVoiced);
      }
   }

#endif

   // keep the best
   HarmInfoToSRFEATUREFLOAT (SRFEATANAL_HARMONICS, &aHIBest[0], fPitch, pSRF);

   if (paHI)
      memcpy (paHI, &aHIBest[0], sizeof(HARMINFO) * SRFEATANAL_HARMONICS);

#ifdef SRFEATUREINCLUDEPCM
   //scale harmonic ranges a bit so that for higher frequencies, take slightly lower harmonics
   fp fHarmScale = fPitch ? sqrt(100.0 /* hz */ / (double)fPitch) : 1;
   fHarmScale = min(fHarmScale, 4); // dont scale too much
   pSRF->bPCMHarmFadeFull = (BYTE)(SRFEATUREPCM_HARMFULL * fHarmScale);
   pSRF->bPCMHarmFadeStart = (BYTE)(SRFEATUREPCM_HARMSTART * fHarmScale);

   // if full PCM spectrum then set both to 0
   if (fFullPCM)
      pSRF->bPCMHarmFadeFull = pSRF->bPCMHarmFadeStart = 0;

   //fNyquist = (fp)dwSamplesPerSecOrig / 2 / fPitch;
   fNyquist = min(fNyquist, 255);
   fNyquist = max(fNyquist, 1);

   if (fNoPCMAtAll) {
      pSRF->bPCMHarmFadeFull = pSRF->bPCMHarmFadeStart = 255;
      fNyquist = 0;
   }

#define PCMEMPHASIZEHIGHFREQ_SCALE        2.0 // how much to emphasize high frequencies since will be summing over one another
#define PCMEMPHASIZEHIGHFREQ_START        4000  // start at 2000 hz
#define PCMEMPHASIZEHIGHFREQ_STOP         16000  // stop at 16000 hz
      // BUGFIX - Scale was 4.0, changed to 2.0. Start was 2000 and stop 8000, changed to 4000 and 16000
      // so that would be more consistent
         
   pSRF->bPCMHarmNyquist = (BYTE)fNyquist;
   DWORD dwTwoKHz = (DWORD)(PCMEMPHASIZEHIGHFREQ_START / fPitch);
   DWORD dwEightKHz = (DWORD)(PCMEMPHASIZEHIGHFREQ_STOP / fPitch);
   fp fScale;

   // filter out pafPCM so can reconstruct the higher frequences
   for (i = 0; i < dwWindowSize/2; i++) {
      DWORD dwHarm = i / 2;

      // BUGFIX - Amplify energy at high frequencies, because when recombine them
      // together, waveforms will interfere at high frequencies, causing them
      // to sound muted
      if (dwHarm >= dwEightKHz) {
         pafPCM[i*2+0] *= PCMEMPHASIZEHIGHFREQ_SCALE;
         pafPCM[i*2+1] *= PCMEMPHASIZEHIGHFREQ_SCALE;
      }
      else if (dwHarm > dwTwoKHz) {
         fScale = 1.0 + (fp)(dwHarm - dwTwoKHz) / (fp)(dwEightKHz - dwTwoKHz) * (PCMEMPHASIZEHIGHFREQ_SCALE - 1.0);
         pafPCM[i*2+0] *= fScale;
         pafPCM[i*2+1] *= fScale;
      }

      // if out of range then zero
      if (dwHarm >= (DWORD)pSRF->bPCMHarmNyquist) {
         // zero
         pafPCM[i*2+0] = pafPCM[i*2+1] = 0.0;
         continue;
      }

      // else, if should take 100% then skip
      if (dwHarm >= (DWORD)pSRF->bPCMHarmFadeFull)
         continue;

      // BUGFIX - Only set to 0 for <= harmfadestart if also bPCMHarmFadeFull
      if (dwHarm <= (DWORD)pSRF->bPCMHarmFadeStart) {
         // zero
         pafPCM[i*2+0] = pafPCM[i*2+1] = 0.0;
         continue;
      }

      // else, partial energy
      fScale = (fp)(dwHarm - (DWORD)pSRF->bPCMHarmFadeStart) / (fp)(pSRF->bPCMHarmFadeFull - pSRF->bPCMHarmFadeStart);
      pafPCM[i*2+0] *= fScale;
      pafPCM[i*2+1] *= fScale;
   } // i

   // inverse the FFT
   FFTRecurseReal (&pafPCM[0] - 1, dwWindowSize, -1, pLUT, pMemFFTScratch);

   // phase
   fPhaseFund = -fPhaseFund / (2.0 * PI) * (fp)(dwWindowSize/2);
      // use negative to counteract fundamental phase
   fPhaseFund = myfmod(fPhaseFund, dwWindowSize/2);
   DWORD dwPhaseOffset = (DWORD)fPhaseFund;

   // copy the remaining wave to a buffer, taking into account modulo,
   // and the fact that was hanning window
   float afWave[CALCSRFEATUREPCMMAX];
   fScale = 1.0 / (fp)dwWindowSize;
   for (i = 0; i < dwCalcSRFeatPCM; i++) {
      dwOffset = (dwWindowSize / 2 + i + dwPhaseOffset) % dwWindowSize;
      afWave[i] = pafPCM[dwOffset] * pafWindow[dwOffset] * fScale;

      dwOffset = (dwWindowSize / 2 + i + dwPhaseOffset + dwCalcSRFeatPCM) % dwWindowSize;
      afWave[i] += pafPCM[dwOffset] * pafWindow[dwOffset] * fScale;
   }

#if 0 // def _DEBUG // to test copying original PCM back
   FFTRecurseReal (&pafPCMNoSqrt[0] - 1, dwCalcSRFeatPCM, -1, pLUT, pMemFFTScratch);

   fScale = 1.0 / (fp)(dwCalcSRFeatPCM/2);
   for (i = 0; i < dwCalcSRFeatPCM; i++) {
      dwOffset = (i + dwPhaseOffset) % dwCalcSRFeatPCM;
      afWave[i] = pafPCMNoSqrt[dwOffset] * fScale;
   }
#endif

   // determine sum of the harmonic energy
   double fHarmEnergy = 0;
   for (i = (DWORD)pSRF->bPCMHarmFadeStart; i < (DWORD)pSRF->bPCMHarmNyquist; i++) {
      // if gone beyond the limits of the harmonics then stop
      if (i >= SRFEATANAL_HARMONICS)
         break;

      fHarmEnergy += aHIBest[i].fVoiced + aHIBest[i].fUnvoiced;
   } // i
   if (fHarmEnergy > 50.0) // BUGFIX - so not for absolute silence
      fHarmEnergy = 1.0 / fHarmEnergy;
   else
      fHarmEnergy = 0.0;

   // fill in PCM
   for (i = 0; i < SRFEATUREPCM; i++)
      pSRF->afPCM[i] = afWave[ (i * dwCalcSRFeatPCM / SRFEATUREPCM) % dwCalcSRFeatPCM] * fHarmEnergy;

#endif // SRFEATUREINCLUDEPCM
}


/****************************************************************************
CM3DWave::ReNormalizeToWaveSRFEATIRE - Called by ReNormalizeToWave,
this renormalize a single SRFEATURE woth.

inputs
   PCM3DWave      pClone - Cloned and resynthesized wave (at half energy)
   DWORD          dwFeature - SRFEATURE number
   PCSinLUT       pLUT - SinLUT object to use for FFT
   PCMem          pMemScratch - Scratch memory
   PCMem          pMemFFTScratch - Memory to use for FFT
   PCMem          pMemWindow - Window where CreateFFTWindow() puts its stuff
   DWORD          *pdwWindowSize - Should be filled with window size in pMemWindow,
                  and will be modified if new window size
returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::ReNormalizeToWaveSRFEATURE (PCM3DWave pClone, DWORD dwFeature,
                                           PCSinLUT pLUT, PCMem pMemScratch, PCMem pMemFFTScratch, PCMem pMemWindow,
                                           DWORD *pdwWindowSize)
{
   // how large a window is neede
   DWORD dwSample = dwFeature * m_dwSRSkip;
   fp fPitch = PitchAtSample (PITCH_F0, dwSample, 0);
   fPitch = max(fPitch, 30 /* hz */);  // don't let be too low, because makes for really huge window

   fp fWavelength = (fp)m_dwSamplesPerSec / fPitch;
   fp fNeed = fWavelength * 3.0;   // 3.0 => want at least three wavelengths
   fNeed = max(fNeed, 4);
   DWORD dwWindowSize;
   for (dwWindowSize = 8; dwWindowSize < (DWORD)fNeed; dwWindowSize *= 2);
   DWORD dwHalfWindow = dwWindowSize / 2;

   // filter half is two harmincs
   DWORD dwFilterHalf = (DWORD)((fp)dwWindowSize / fWavelength * 2.0 + 1.0 /* for good measure*/);

   float *pafWindow;
   if (dwWindowSize != *pdwWindowSize) {
      if (!pMemWindow->Required (dwWindowSize * sizeof(float)))
         return FALSE;

      pafWindow = (float*)pMemWindow->p;
      CreateFFTWindow (3, pafWindow, dwWindowSize);

      *pdwWindowSize = dwWindowSize;
   }
   else
      pafWindow = (float*)pMemWindow->p;

   if (!pMemScratch->Required (dwWindowSize * 2 * sizeof(float)))
      return FALSE;
   float *pafOrig = (float*)pMemScratch->p;
   float *pafResynth = pafOrig + dwWindowSize;

   // do FFTs on both of them
   FFT (dwWindowSize, dwSample, 0, pafWindow, pafOrig, pLUT, pMemFFTScratch, NULL, dwFilterHalf);
   pClone->FFT (dwWindowSize, dwSample, 0, pafWindow, pafResynth, pLUT, pMemFFTScratch, NULL, dwFilterHalf);

   // frequency of 1st harmonic of window
   fp fFreqWindow = (fp)m_dwSamplesPerSec / (fp)dwWindowSize;
   fp fPitchWindow = 1.0 / fFreqWindow;
   PSRFEATURE pSRF = m_paSRFeature + dwFeature;

   // loop over all the SRDATAPOINTs
   DWORD i;
   fp fScale;
   for (i = 0; i < SRDATAPOINTS; i++) {
      // frequency of the SRDATAPOINT
      fp fFreq = pow ((fp)2.0, (fp)i / (fp)SRPOINTSPEROCTAVE) * SRBASEPITCH;

      // what index to use in the filtered windwos
      fp fIndex = fFreq / fFreqWindow;
      int iIndex = floor(fIndex);
      int iIndex2 = iIndex+1;
      fIndex -= (fp)iIndex;
      iIndex = min(iIndex, (int)dwHalfWindow - 1);
      iIndex2 = min(iIndex2, (int)dwHalfWindow - 1);

      // interpolate the values for both the original and new
      fp fInOrig = pafOrig[iIndex] * (1.0 - fIndex) + pafOrig[iIndex2] * fIndex;
      fp fInResynth = pafResynth[iIndex] * (1.0 - fIndex) + pafResynth[iIndex2] * fIndex;
      fInResynth *= 2.0;   // since had been halved in volume before passing in
      if (!fInOrig || !fInResynth)
         continue;   // 0, cant do anything with

#define MAXDBCHANGEFORRENOMALIZE       12
      // how much to scale
      // no more than 6 db either way
      fScale = log10(fInOrig / fInResynth) * 20.0; // into dB
      fScale = floor (fScale + 0.5);   // round
      fScale = max(fScale, -MAXDBCHANGEFORRENOMALIZE);
      fScale = min(fScale, MAXDBCHANGEFORRENOMALIZE);
      int iScale = (int)fScale;

      int iCur;

      iCur = pSRF->acVoiceEnergy[i];
      iCur += fScale;
      iCur = max(iCur, SRABSOLUTESILENCE);
      iCur = min(iCur, SRMAXLOUDNESS);
      pSRF->acVoiceEnergy[i] = (char)iCur;

      iCur = pSRF->acNoiseEnergy[i];
      iCur += fScale;
      iCur = max(iCur, SRABSOLUTESILENCE);
      iCur = min(iCur, SRMAXLOUDNESS);
      pSRF->acNoiseEnergy[i] = (char)iCur;
   } // i

   // also calc energy for PCM
   double fPCMEnergyOrig, fPCMEnergyResynth;
   fp fPCMHarmFadeStart, fPCMHarmFadeFull;
   if (pSRF->bPCMHarmNyquist) {
      fPCMEnergyOrig = fPCMEnergyResynth = 0;
      fPCMHarmFadeStart = fPitch * (fp)pSRF->bPCMHarmFadeStart / fFreqWindow;
      fPCMHarmFadeFull = fPitch * (fp)pSRF->bPCMHarmFadeFull / fFreqWindow;

      for (i = (int)ceil(fPCMHarmFadeStart); i < dwHalfWindow; i++) {
         if ((fp)i >= fPCMHarmFadeFull) {
            fPCMEnergyOrig += pafOrig[i];
            fPCMEnergyResynth += pafResynth[i];
         }
         else {
            fp fAlpha = ((fp)i - fPCMHarmFadeStart) / (fPCMHarmFadeFull - fPCMHarmFadeStart);
            fPCMEnergyOrig += pafOrig[i] * fAlpha;
            fPCMEnergyResynth += pafResynth[i] * fAlpha;
         }
      } // i

      fPCMEnergyResynth *= 2.0;  // since came in at half energy
   }

   // adjust the PCM scale
   if (pSRF->bPCMHarmNyquist && fPCMEnergyOrig) {
      fScale = fPCMEnergyResynth / fPCMEnergyOrig; // BUGFIX need to scale the opposite
      fScale = max(fScale, 0.5);
      fScale = min(fScale, 2.0);
      pSRF->fPCMScale *= fScale;
   }
   
#if 0 // to test
   pSRF->bPCMHarmFadeFull = pSRF->bPCMHarmFadeStart = pSRF->bPCMHarmNyquist = 0;
   pSRF->fPCMScale = 0;
#endif // 0

   return TRUE;
}



/****************************************************************************
CM3DWave::ReNormalizeToWave - Called by CalcSRFeatures to synthesize the results,
and compare the synthesized results against what should have. Fine-tuning
adjustments are made.

inputs
   none
returns
   none
*/
void CM3DWave::ReNormalizeToWave (void)
{
   // clone this for resynthesis
   PCM3DWave pClone = Clone ();
   if (!pClone)
      return;

   // loop through SRFEATUREs and eliminate PCM AND half all the volume levels,
   // so won't clip when resynthesize
   DWORD i, j;
   PSRFEATURE pSRF = pClone->m_paSRFeature;
   int iCur;
   for (i = 0; i < pClone->m_dwSRSamples; i++, pSRF++) {
      for (j = 0; j < SRDATAPOINTS; j++) {
         // halve voiced
         iCur = pSRF->acVoiceEnergy[j];
         iCur -= 6;
         iCur = max(iCur, SRABSOLUTESILENCE);
         pSRF->acVoiceEnergy[j] = (char)iCur;

         // halve unvoiced
         iCur = pSRF->acNoiseEnergy[j];
         iCur -= 6;
         iCur = max(iCur, SRABSOLUTESILENCE);
         pSRF->acNoiseEnergy[j] = (char)iCur;

         pSRF->bPCMHarmFadeStart = pSRF->bPCMHarmFadeFull = pSRF->bPCMHarmNyquist = 0;
         pSRF->fPCMScale = 0;
      } // j
   } // i

   // resynthesize
   CVoiceSynthesize vs;
   vs.SynthesizeFromSRFeature (0, pClone, NULL, 0, 0.0, NULL, FALSE, NULL);

   // loop and adjust
   CSinLUT SinLUT;
   CMem memScratch, memFFTScratch, memWindow;
   DWORD dwWindowSize = 0;
   for (i = 0; i < m_dwSRSamples; i++)
      ReNormalizeToWaveSRFEATURE (pClone, i, &SinLUT, &memScratch, &memFFTScratch, &memWindow, &dwWindowSize);


   delete pClone;
}



/****************************************************************************
CM3DWave::BlurSRDETAILEDPHASE - Blurs the SRDETAILEDPHASE of a wave
used for making CalcSRFeatures have better phases.
*/

void CM3DWave::BlurSRDETAILEDPHASE (void)
{
   CMem mem;
   if (!mem.Required (m_dwSRSamples * sizeof(SRDETAILEDPHASE)))
      return;  // error
   PSRDETAILEDPHASE pSDP = (PSRDETAILEDPHASE) mem.p;
   memset (pSDP, 0, m_dwSRSamples * sizeof(SRDETAILEDPHASE));

   // created detailed phase
   DWORD i;
   fp fPitch;
   for (i = 0; i < m_dwSRSamples; i++) {
      fPitch = PitchAtSample (PITCH_F0, i * m_dwSRSkip, 0);
      SRDETAILEDPHASEFromSRFEATURE (m_paSRFeature + i, fPitch, pSDP + i);
   } // i

   // go back through and average
   SRDETAILEDPHASE SDP;
   DWORD j;
   fp fBlurWidth, fScale;
   int iOffset, iBlurWidth, iCur;
   for (i = 0; i < m_dwSRSamples; i++) {
      memset (&SDP, 0, sizeof(SDP));

      for (j = 0; j < SRDATAPOINTSDETAILED; j++) {
         // what's the width of blurring?
         fBlurWidth = (fp)j / (fp)SRDATAPOINTSDETAILED;
         fBlurWidth = (1.0 - fBlurWidth) * PHASEBLURWIDTH_BASE + fBlurWidth * PHASEBLURWIDTH_TOP;
         fBlurWidth *= (fp)m_dwSamplesPerSec / (fp)m_dwSRSkip;

         iBlurWidth = (int) (fBlurWidth + 0.5);
         for (iOffset = -iBlurWidth; iOffset <= iBlurWidth; iOffset++) {
            iCur = iOffset + (int)i;
            if ((iCur < 0) || (iCur >= (int)m_dwSRSamples))
               continue;   // out of range

            fScale = (fp)(iBlurWidth + 1 - abs(iOffset));

            SDP.afVoicedPhase[j][0] += pSDP[iCur].afVoicedPhase[j][0] * fScale;
            SDP.afVoicedPhase[j][1] += pSDP[iCur].afVoicedPhase[j][1] * fScale;

            // NOTE: NOT doing afHarmPhase
         } // iOffset
      } // j

      // write this into the wave
      fPitch = PitchAtSample (PITCH_F0, i * m_dwSRSkip, 0);
      SRDETAILEDPHASEToSRFEATURE (&SDP, fPitch, m_paSRFeature + i);
   } // i, all samples
}


/****************************************************************************
CM3DWave::CalcSRFeatures - Calculates the SR features.

inputs
   DWORD                dwCalcFor - Use WAVECALC_XXX
   PCProgressSocket     pProgress - Progress bar
returns
   BOOL - TRUE if successful
*/
BOOL CM3DWave::CalcSRFeatures (DWORD dwCalcFor, PCProgressSocket pProgress)
{
   BOOL fFastSR = FALSE;   // Set to TRUE if want fast SR calculation. FALSE for slow but accurate.
                        // In fast SR calculation, only the every other even one will be calculated.

   if (!BlankSRFeatures())
      return FALSE;

   // if fastSR then double skip and reduce samples
   DWORD dwFullSamples = m_dwSRSamples;
   if (fFastSR) {
      m_dwSRSkip *= 2;
      m_dwSRSamples = m_dwSamples / m_dwSRSkip + 1;
   }

   if (!m_dwSamples)
      return FALSE;

   BOOL fSecondMono = FALSE;
#ifdef FEATUREHACK_SECONDPITCHDETECT
   fSecondMono = TRUE;
#endif

   // see if need pitch
   BOOL fNeedPitch;
   fNeedPitch = (m_adwPitchSamples[PITCH_F0] ? FALSE : TRUE);
   if (fNeedPitch) {
      if (pProgress)
         pProgress->Push (0, fSecondMono ? 0.25 : .5);
      CalcPitch (dwCalcFor, pProgress);
         // BUGFIX - Dont use fast calcpitch because not accurate enough
      if (pProgress) {
         pProgress->Pop();
         pProgress->Push (fSecondMono ? .25 : 0.5, 1);
      }
   }

   PCM3DWave pWaveUp = this;
   DWORD dwCalcSRFeatPCM = SRFEATUREPCM*2;   // so get around 512 samples in window

   switch (dwCalcFor) {
   case WAVECALC_TRANSPROS:
   case WAVECALC_VOICECHAT:
      break;

   case WAVECALC_TTS_PARTIALPCM:
   case WAVECALC_TTS_FULLPCM:
   case WAVECALC_SEGMENT:
#ifdef FEATUREHACK_UPSAMPLE
      if (SRFEATUREPCM < 512)
         dwCalcSRFeatPCM = SRFEATUREPCM*4;  // since upsampling
      pWaveUp = Clone();
      if (!pWaveUp) {
         if (fNeedPitch && pProgress)
            pProgress->Pop();
         return FALSE;
      }
      pWaveUp->ConvertSamplesPerSec (min(max(44100, m_dwSamplesPerSec), m_dwSamplesPerSec * 2), NULL);
      pWaveUp->m_dwSRSkip = pWaveUp->m_dwSamplesPerSec / m_dwSRSAMPLESPERSEC;    // intentionally NOT using pWaveUp->m_dwSRSAMPLESPERSEC
#endif
      break;
   }



   // create a wave that's monotone
   CListFixed lMap, lMap2;
   fp fPitchIfMono = 80.0; // BUGFIX - If making a second monotone then first one to 80 hz so sure can reconize pitch a second time
   fp fPitchConvertTo = (fp) pWaveUp->m_dwSamplesPerSec / (fp)dwCalcSRFeatPCM;
   PCM3DWave pWaveMono = pWaveUp->Monotone (dwCalcFor, fSecondMono ? fPitchIfMono : fPitchConvertTo, (DWORD)-1, (DWORD)-1, &lMap, NULL);
      
   if (!pWaveMono) {
      if (fNeedPitch && pProgress)
         pProgress->Pop();
      if (pWaveUp != this)
         delete pWaveUp;
      return FALSE;
   }
   PCM3DWave pWaveMono2 = NULL;
   if (fSecondMono) {
      if (pProgress)
         pProgress->Push (0, 0.33);

      pWaveMono2 = pWaveMono->Monotone (dwCalcFor, fPitchConvertTo, (DWORD)-1, (DWORD)-1, &lMap2, pProgress);
      if (!pWaveMono2) {
         delete pWaveMono;
         if (fNeedPitch && pProgress)
            pProgress->Pop();
         if (pProgress)
            pProgress->Pop();
         if (pWaveUp != this)
            delete pWaveUp;
         return FALSE;
      }

      if (pProgress) {
         pProgress->Pop ();
         pProgress->Push (0.33, 1.0);
      }
   }




   CSinLUT SinLUT;
   CMem memFFTScratch;
   CMem memSRFEATUREFLOAT;
   if (!memSRFEATUREFLOAT.Required (2 * m_dwSRSamples * sizeof(SRFEATUREFLOAT) + m_dwSRSamples * SRFEATANAL_HARMONICS * sizeof(HARMINFO)
      + m_dwSRSamples * sizeof(double) * 3 + m_dwSRSamples * sizeof(float) * SROCTAVE )) {

      if (pWaveMono)
         delete pWaveMono;
      if (pWaveMono2)
         delete pWaveMono2;
      if (pWaveUp != this)
         delete pWaveUp;
      return FALSE;  // shouldnt happen
   }
   PSRFEATUREFLOAT pSRFFloat1 = (PSRFEATUREFLOAT)memSRFEATUREFLOAT.p;
   PSRFEATUREFLOAT pSRFFloat2 = pSRFFloat1 + m_dwSRSamples;
   PHARMINFO paHI = (PHARMINFO) (pSRFFloat2 + m_dwSRSamples);
   double *pafPitch = (double*) (paHI + (m_dwSRSamples * SRFEATANAL_HARMONICS));
   double *pafNoiseSum = (double*) pafPitch + m_dwSRSamples;
   double *pafVoiceSum = (double*) pafNoiseSum + m_dwSRSamples;
   float *pafEnergyOctave = (float*) (pafVoiceSum + m_dwSRSamples);

   // loop
   double fPitch, fPitch2;
   DWORD i;
   CListFixed lPitchMod;
   lPitchMod.Init (sizeof(fp));
   for (i = 0; i < m_dwSRSamples; i++) {
      if (((i%16) == 0) && pProgress)
         pProgress->Update ((fp)i / (fp)m_dwSRSamples);

      // determine the sample
      double fSampleOrig, fSampleOrig2;
      fSampleOrig = pWaveUp->MonotoneFindOrigSample (i * pWaveUp->m_dwSRSkip, &lMap, &fPitch);
      if (fSecondMono) {
         fSampleOrig2 = pWaveMono->MonotoneFindOrigSample (fSampleOrig, &lMap2, &fPitch2);
         fSampleOrig = fSampleOrig2;
         fPitch *= fPitch2;
      }
      fPitch *= fPitchConvertTo;
      pafPitch[i] = fPitch;

      // analyze
      fp fPitchFineTune;
      if (fSecondMono)
         pWaveMono2->PCMToSRFEATUREFLOAT (dwCalcFor, (int)fSampleOrig, fPitch, m_dwSamplesPerSec, pSRFFloat1 + i, paHI + (i*SRFEATANAL_HARMONICS),
            pafEnergyOctave + (i * SROCTAVE),
            &SinLUT, &memFFTScratch, dwCalcSRFeatPCM, &fPitchFineTune);
               // NOTE: Specifically NOT using pWaveUp->m_dwSamplesPerSec
      else
         pWaveMono->PCMToSRFEATUREFLOAT (dwCalcFor, (int)fSampleOrig, fPitch, m_dwSamplesPerSec, pSRFFloat1 + i, paHI + (i*SRFEATANAL_HARMONICS),
            pafEnergyOctave + (i * SROCTAVE),
            &SinLUT, &memFFTScratch, dwCalcSRFeatPCM, &fPitchFineTune);
               // NOTE: Specifically NOT using pWaveUp->m_dwSamplesPerSec

      // remember what new pitch should be
      fPitch *= fPitchFineTune;
      lPitchMod.Add (&fPitch);
   } // i

   // go back and modify pitch in the main SRFEATURE
   fp *pafPitchMod = (fp*)lPitchMod.Get(0);
   if ((m_dwSRSkip == m_adwPitchSkip[PITCH_F0]) && (m_dwSRSamples == m_adwPitchSamples[PITCH_F0])) for (i = 0; i < m_dwSRSamples; i++) {
      if (i >= m_adwPitchSamples[PITCH_F0])
         continue;   // shouldnt happen
      if (i >= lPitchMod.Num())
         continue;   // shouldnt happen

      m_apPitch[PITCH_F0][i].fFreq = pafPitchMod[i];
   } // i

#ifdef FEATUREHACK_AVERAGEALL
   // blur the waves together as long as they're similar
   SRFEATUREFLOATAverageAll (m_dwSRSamples, pSRFFloat1, pSRFFloat2);
#else
   memcpy (pSRFFloat2, pSRFFloat1, m_dwSRSamples * sizeof(SRFEATUREFLOAT));
#endif

   // calculate noise and voice
   for (i = 0; i < m_dwSRSamples; i++)
      SRFEATUREFLOATNoiseVoiceEnergy (pSRFFloat2 + i, pafVoiceSum + i, pafNoiseSum + i);

   // average phases
#ifdef FEATUREHACK_AVERAGEALLPHASES
   HARMINFOAverageAll (m_dwSRSamples, pafPitch, paHI, pSRFFloat2, dwCalcSRFeatPCM);   // BUGFIX - specifically not using pWaveUp
#endif

   // loop through and convert floats to db
   double fNoiseSum, fVoiceSum;
   for (i = 0; i < m_dwSRSamples; i++) {

      fNoiseSum = fVoiceSum = 0;

#ifdef FEATUREHACK_NOISEAFFECTEDBYNEIGHBORS
#define AFFECTEDBYNEIGHBORS         3        // BUGFIX - Was just 1
      int iOffset;
      for (iOffset = -AFFECTEDBYNEIGHBORS; iOffset <= AFFECTEDBYNEIGHBORS; iOffset++) {
         int iCur = (int)i+iOffset;
         if ((iCur < 0) || (iCur >= (int)m_dwSRSamples))
            continue;

         fp fWeight = (AFFECTEDBYNEIGHBORS + 1 - abs(iCur));
         fNoiseSum += pafNoiseSum[iCur] * fWeight;
         fVoiceSum += pafVoiceSum[iCur] * fWeight;
      } // iOffset

      //fNoiseSum = pafNoiseSum[i] * 2.0;
      //fVoiceSum = pafVoiceSum[i] * 2.0;
      
      //if (i) {
      //   fNoiseSum += pafNoiseSum[i-1];
      //   fVoiceSum += pafVoiceSum[i-1];
      //}
      //if (i+1 < m_dwSRSamples) {
      //   fNoiseSum += pafNoiseSum[i+1];
      //   fVoiceSum += pafVoiceSum[i+1];
      //}
#endif

#ifdef FEATUREHACK_NOISETOVOICE
      // convert noise to voice
      SRFEATUREFLOATNoiseToVoice (pSRFFloat2 + i, fNoiseSum, fVoiceSum);
#endif

#ifdef FEATUREHACK_SHARPENFORMANTSINFEAT
      // sharpen the formants
      SRFEATUREFLOATSharpenFormants (pSRFFloat2 + i);
#endif

#ifdef FEATUREHACK_NARROWERFORMANT
      NarrowerFormants (&pSRFFloat2[i].afVoiceEnergy[0], &pSRFFloat2[i].afNoiseEnergy[0]);
#endif

#ifdef FEATUREHACK_REDUCEQUIET
      // get rid of really quiet stuff
      SRFEATUREFLOATReduceQuiet (pSRFFloat2 + i);
#endif

#ifdef FEATUREHACK_SCALEENERGYTOFFT
      // rescale the energy to FFT
      SRFEATUREFLOATScaleEnergyToFFT (pSRFFloat2 + i, pafEnergyOctave + i * SROCTAVE);
#endif

      // convert to SRFEATURE
      SRFEATUREFLOATToSRFEATURE (pSRFFloat2 + i, m_paSRFeature + i, m_dwSamplesPerSec);
   }

#ifdef FEATUREHACK_BLURPHASEDETAILED
   BlurSRDETAILEDPHASE ();
#endif

#ifdef FEATUREHACK_RENORMALIZETOWAVE
   switch (dwCalcFor) {
   case WAVECALC_TRANSPROS:
   case WAVECALC_VOICECHAT:
      break;

   case WAVECALC_SEGMENT:
   case WAVECALC_TTS_PARTIALPCM:
   case WAVECALC_TTS_FULLPCM:
      // BUGFIX - Renormalize several times instead of just once, slightly better
      // BUGGIX - only renamalize once for (i = 0; i < 3; i++), because was making s's sound strange
         ReNormalizeToWave ();
      break;
   }
#endif

   // finally
   if (fNeedPitch && pProgress)
      pProgress->Pop();
   if (fSecondMono && pProgress)
      pProgress->Pop();
   if (pWaveMono)
      delete pWaveMono;
   if (pWaveMono2)
      delete pWaveMono2;
   if (pWaveUp != this)
      delete pWaveUp;

   if (fFastSR) {

      // else, lengthen
      m_dwSRSkip /= 2;
      DWORD dwOldSamples = m_dwSRSamples;
      m_dwSRSamples = dwFullSamples;

      // loop backwards filling in
      for (i = m_dwSRSamples-1; i < m_dwSRSamples; i--) {
         DWORD dwOld = i/2;

         // if an even number then exact match
         if (!(i%2)) {
            if (i != min(dwOld,dwOldSamples-1))
               m_paSRFeature[i] = m_paSRFeature[min(dwOld,dwOldSamples-1)];
            continue;
         }

         // else, theoretically need to interpolate, but since
         // only using this for voice compression, which skips every other one anyway,
         // just duplicate
         DWORD dwOldNext = dwOld+1;
         dwOld = min(dwOld, dwOldSamples-1);
         dwOldNext = min(dwOldNext, dwOldSamples-1);

         if (i != dwOld)
            m_paSRFeature[i] = m_paSRFeature[min(dwOld,dwOldSamples-1)];
      } // i
   }


#if 0 // def _DEBUG // to test
   CMem mem;
   CTTSWork Work;
   Work.JoinSimulatePSOLAInWave (this, &m_paSRFeature[0], m_dwSRSamples, &mem, 0);
   memcpy (&m_paSRFeature[0], mem.p, m_dwSRSamples * sizeof(SRFEATURE));
//#define JOINPSOLAHARMONICS 128
//   float afVoiced[JOINPSOLAHARMONICS], afNoise[JOINPSOLAHARMONICS];
//   for (i = 0; i < m_dwSRSamples; i++) {
//      fp fPitch = PitchAtSample (i * m_dwSRSkip, 0);
//      SRFEATUREToHarmonics (&m_paSRFeature[i], JOINPSOLAHARMONICS, afVoiced, afNoise, fPitch);
//      SRFEATUREFromHarmonics (JOINPSOLAHARMONICS, afVoiced, afNoise, fPitch, &m_paSRFeature[i]);
//   }
#endif

   return TRUE;
}



/****************************************************************************
SRFEATUREPCMLowPass - Do a quick low-pass filter on the PCM so that alignment
is easier.

inputs
   PSRFEATURE     pSRF - Feature
   char           *pacLowPass - Filled with the low pass size. Must be
                                 SRFEATUREPCM in size
returns
   none
*/
#define LOWPASSSIZE     (SRFEATUREPCM/64)     // to left and right
#ifdef SRFEATUREINCLUDEPCM_SHORT
void SRFEATUREPCMLowPass (PSRFEATURE pSRF, short *pasLowPass)
#else
void SRFEATUREPCMLowPass (PSRFEATURE pSRF, char *pacLowPass)
#endif
{
   DWORD i;
   int iOffset;
   for (i = 0; i < SRFEATUREPCM; i++) {
      int iSum = 0;
      int iSumWeight = 0;
      for (iOffset = -(int)LOWPASSSIZE; iOffset <= (int)LOWPASSSIZE; iOffset++) {
         int iCur = ((int)i + (int)SRFEATUREPCM + (int)iOffset) % SRFEATUREPCM;
         int iWeight = LOWPASSSIZE - abs(iOffset) + 1;

#ifdef SRFEATUREINCLUDEPCM_SHORT
         iSum += (int)pSRF->asPCM[iCur] * iWeight;
#else
         iSum += (int)pSRF->acPCM[iCur] * iWeight;
#endif
         iSumWeight += iWeight;
      } // iOffset

#ifdef SRFEATUREINCLUDEPCM_SHORT
      pasLowPass[i] = (short)(iSum / iSumWeight);
#else
      pacLowPass[i] = (char)(iSum / iSumWeight);
#endif
   } // i
}

/****************************************************************************
SRFEATUREPCMError - Finds the error between two PCM features (ignoring amplification)

inputs
   char     *pacA - version A, SRFEATUREPCM samples
   char     *pacB - Version B, SRFEATUREPCM samples
   DWORD    dwOffsetB - Add this offset to B
returns
   int - iError squared
*/
#ifdef SRFEATUREINCLUDEPCM_SHORT
__int64 SRFEATUREPCMError (short *pasA, short *pasB, DWORD dwOffsetB)
#else
int SRFEATUREPCMError (char *pacA, char *pacB, DWORD dwOffsetB)
#endif
{
   DWORD dwPass1Max = SRFEATUREPCM - dwOffsetB;
   DWORD dwPass2Max = SRFEATUREPCM - dwPass1Max;

   __int64 iError = 0;
   int iDist;
#ifdef SRFEATUREINCLUDEPCM_SHORT
   short *pA = pasA;
   short *pB = pasB + dwOffsetB;
#else
   char *pA = pacA;
   char *pB = pacB + dwOffsetB;
#endif
   DWORD i;
   for (i = 0; i < dwPass1Max; i++, pA++, pB++) {
      iDist = (int)pA[0] - (int)pB[0];
      iError += iDist * iDist;
   }

#ifdef SRFEATUREINCLUDEPCM_SHORT
   pB = pasB;  // start at beginning of B
#else
   pB = pasB;  // start at beginning of B
#endif
   for (i = 0; i < dwPass2Max; i++, pA++, pB++) {
      iDist = (int)pA[0] - (int)pB[0];
      iError += iDist * iDist;
   }

   return iError;
}

/****************************************************************************
SRFEATUREEnergyAtFrequency - Looks in the SRFEATURE's acVoiced spectrum
and determines how much energy is available at a given frequency.

inputs
   PSRFEATURE        pSRF - Feature
   fp                fFreq - Frequency, in Hz
returns  
   fp - Energy
*/
fp SRFEATUREEnergyAtFrequency (PSRFEATURE pSRF, fp fFreq)
{
   fFreq = max(fFreq, 1);

   fp fExtraScale = 1.0;
   fFreq = log(fFreq / (fp)SRBASEPITCH) / log((fp)2) * SRPOINTSPEROCTAVE;
   if (fFreq >= SRDATAPOINTS-1) // BUGFIX - Was SRPHASENUM-1
      fFreq = SRDATAPOINTS-1; // BUGFIX - Was SRPHASENUM-1
   else if (fFreq < 0) {
      if (fFreq > -SRPOINTSPEROCTAVE)
         fExtraScale = (fFreq + SRPOINTSPEROCTAVE) / (fp)SRPOINTSPEROCTAVE;
            // so if below recorded, allow an extra octave lower
      else
         fExtraScale = 0;
      fFreq = 0;
   }

   DWORD dwOffset = (DWORD)fFreq;
   fFreq -= (fp)dwOffset;
   DWORD dwOffset2 = min(dwOffset+1, SRDATAPOINTS-1); // BUGFIX - as SRPHASENUM-1
   int iAdjust = ((int)dwOffset - (int)SRDATAPOINTS/2) * 6 / SRPOINTSPEROCTAVE;
      // BUGFIX - Was SRPHASENUM/2

   int iDb1 = (int)pSRF->acVoiceEnergy[dwOffset] - iAdjust;
   int iDb2 = (int)pSRF->acVoiceEnergy[dwOffset2] - iAdjust;
   iDb1 = max(iDb1, -127);
   iDb2 = max(iDb2, -127);
   iDb1 = min(iDb1, 127);
   iDb2 = min(iDb2, 127);
   fp fEnergy1 = DbToAmplitude (iDb1);
   fp fEnergy2 = DbToAmplitude (iDb2);

   return (fEnergy1 * (1.0 - fFreq) + fEnergy2 * fFreq) * fExtraScale;
}


/****************************************************************************
SRFEATUREVoicedToPCM - Converts a voiced spectrum into PCM.

inputs
   PSRFEATURE     pSRF - Feature
   fp             fPitch - Frequency of fundamental (in Hz)
   char           *pacPCM - Array of SRFEATUREPCM to be filled in with normalized amount
   PCSinLUT       pLUT - Sine lookup table, initialized by this, to use
returns
   none
*/
#define VOICEDTOPCM_HARMONICS       10       // number of harmonics to do

#ifdef SRFEATUREINCLUDEPCM_SHORT
void SRFEATUREVoicedToPCM (PSRFEATURE pSRF, fp fPitch, short *pasPCM, PCSinLUT pLUT)
#else
void SRFEATUREVoicedToPCM (PSRFEATURE pSRF, fp fPitch, char *pacPCM, PCSinLUT pLUT)
#endif
{
   pLUT->Init (SRFEATUREPCM);

   fp afPCM[SRFEATUREPCM];
   memset (afPCM, 0, sizeof(afPCM));

   DWORD dwHarm, i;
   int iNum;
   BYTE bPhase;
   for (dwHarm = 0; dwHarm < VOICEDTOPCM_HARMONICS; dwHarm++) {
      fp fEnergy = SRFEATUREEnergyAtFrequency(pSRF, fPitch * (fp)(dwHarm+1));
      if (!fEnergy)
         continue;

      bPhase = pSRF->abPhase[dwHarm];
      iNum = (int)bPhase * SRFEATUREPCM / 256;
      for (i = 0; i < SRFEATUREPCM; i++, iNum += (int)(dwHarm+1))
         afPCM[i] += fEnergy * (fp)pLUT->SinFast (iNum, SRFEATUREPCM);
   } // dwHarm

   // find fMax
   fp fMax = 0;
   for (i = 0; i < SRFEATUREPCM; i++)
      fMax = max(fabs(afPCM[i]), fMax);

   if (fMax) {
#ifdef SRFEATUREINCLUDEPCM_SHORT
      // NOTE: Not tested
      fMax = 32767.0 / fMax;
      for (i = 0; i < SRFEATUREPCM; i++)
        pasPCM[i] = (short)(afPCM[i] * fMax);
#else
      fMax = 127.0 / fMax;
      for (i = 0; i < SRFEATUREPCM; i++)
        pacPCM[i] = (char)(afPCM[i] * fMax);
#endif
   }
   else {
#ifdef SRFEATUREINCLUDEPCM_SHORT
      // NOTE: Not tested
      memset (pasPCM, 0, sizeof(pasPCM[0]) * SRFEATUREPCM);
#else
      memset (pacPCM, 0, sizeof(pacPCM[0]) * SRFEATUREPCM);
#endif
   }
}


/****************************************************************************
CM3DWave::SRFEATUREAlignPCM - Aligns the PCM so that a FullPCM version
of TTS will sound better.

inputs
   BOOL     fFullPCM - Set to TRUE if PCM is everything, else uses harmonics
   PCMLexicon        pLex - If not NULL, used for phonemes to determine if voiced or not
   PCListFixed       plSRDETAILEDPHASE - Initialized and filled in with sRDETAILEDPHASE,
                        one per pWave->m_dwSRSamples. Can be NULL
returns
   BOOL - TRUE if succes. Will fail if no PCM
*/
BOOL CM3DWave::SRFEATUREAlignPCM (BOOL fFullPCM, PCMLexicon pLex, PCListFixed plSRDETAILEDPHASE)
{
   if (!m_dwSRSamples)
      return FALSE;
   PSRDETAILEDPHASE paSDP = plSRDETAILEDPHASE ? (PSRDETAILEDPHASE) plSRDETAILEDPHASE->Get(0) : NULL;

   // loop
#ifdef SRFEATUREINCLUDEPCM_SHORT
   short asTemp[SRFEATUREPCM];
   short asLowPass[2][SRFEATUREPCM];
#else
   char acTemp[SRFEATUREPCM];
   char acLowPass[2][SRFEATUREPCM];
#endif
   CSinLUT SinLUT;
   DWORD dwLastOffset = 0;
   DWORD i, j;
   DWORD dwBest;
   SRFEATURE SRFTemp;
   __int64 iBestScore, iScore;
   fp fPitch;
   DWORD dwNextPhonemeStart = 0; // when start
   DWORD dwNoAlignStart = (DWORD)-1, dwNoAlignStop = (DWORD)-1;
   PWVPHONEME pwp = (PWVPHONEME) m_lWVPHONEME.Get(0);
   WCHAR szTemp[16];
   for (i = 0; i < m_dwSRSamples; i++, paSDP ? paSDP++ : 0) {
      if (fFullPCM) {
         // lowpass this one
#ifdef SRFEATUREINCLUDEPCM_SHORT
         SRFEATUREPCMLowPass (m_paSRFeature + i, asLowPass[i%2]);
#else
         SRFEATUREPCMLowPass (m_paSRFeature + i, acLowPass[i%2]);
#endif
      }
      else {
         fPitch = PitchAtSample(PITCH_F0, i * m_dwSRSkip, 0);
         if (paSDP) {
            SRDETAILEDPHASEToSRFEATURE (paSDP, fPitch, &SRFTemp);
            memcpy (m_paSRFeature[i].abPhase, SRFTemp.abPhase, sizeof(SRFTemp.abPhase));
         }

#ifdef SRFEATUREINCLUDEPCM_SHORT
         // NOTE: Not tested
         SRFEATUREVoicedToPCM (m_paSRFeature + i, fPitch, asLowPass[i%2], &SinLUT);
#else
         SRFEATUREVoicedToPCM (m_paSRFeature + i, fPitch, acLowPass[i%2], &SinLUT);
#endif
      }


      // if this is the first one, do nothing
      if (!i)
         continue;

      // find the best match
      dwBest = (DWORD)-1;
      iBestScore = 0;
      for (j = 0; j < SRFEATUREPCM; j++) {
#ifdef SRFEATUREINCLUDEPCM_SHORT
         iScore = SRFEATUREPCMError (asLowPass[(i-1)%2], asLowPass[i%2], j);
#else
         iScore = SRFEATUREPCMError (acLowPass[(i-1)%2], acLowPass[i%2], j);
#endif
         if ((dwBest == (DWORD)-1) || (iScore < iBestScore)) {
            // found match
            dwBest = j;
            iBestScore = iScore;

            if (!iBestScore)
               break;   // speed up
         }
      } // j

      // will always have a best
      
      // what's the offset for this
      DWORD dwOffsetThis = (dwLastOffset + SRFEATUREPCM - dwBest) % SRFEATUREPCM;
      
      // BUGFIX - No offset in unvoiced
      if (pLex && m_lWVPHONEME.Num()) {
         // find phoneme
         if (i >= dwNextPhonemeStart) {
            dwNoAlignStart = dwNoAlignStop = (DWORD)-1;

            for (j = m_lWVPHONEME.Num()-1; j < m_lWVPHONEME.Num(); j--)
               if (i >= pwp[j].dwSample / m_dwSRSkip)
                  break;
            if (j >= m_lWVPHONEME.Num())
               // no match, before first phoneme
               dwNextPhonemeStart = pwp[0].dwSample / m_dwSRSkip;
            else {
               dwNextPhonemeStart = (j+1 < m_lWVPHONEME.Num()) ? (pwp[j+1].dwSample) / m_dwSRSkip : (DWORD)-1;

               memset (szTemp, 0, sizeof(szTemp));
               memcpy (szTemp, pwp[j].awcNameLong, sizeof(pwp[j].awcNameLong));
               PLEXPHONE plp = pLex->PhonemeGetUnsort (pLex->PhonemeFindUnsort (szTemp));
               PLEXENGLISHPHONE ple = plp ? MLexiconEnglishPhoneGet(plp->bEnglishPhone) : NULL;
               BOOL fVoiced = (ple->dwCategory & PIC_VOICED) ? TRUE : FALSE;

               if (!fVoiced) {
                  dwNoAlignStart = pwp[j].dwSample / m_dwSRSkip + m_dwSRSAMPLESPERSEC / 100; // don't start right away
                  dwNoAlignStop = dwNextPhonemeStart - m_dwSRSAMPLESPERSEC / 100; // don't start right away
               }
            }
         } // if need new phoneme

         // if this is unvoiced then have dwNoAlignStart and dwNoAlignStop
         // if within range, then update offset
         if ((i >= dwNoAlignStart) && (i < dwNoAlignStop))
            dwOffsetThis = dwLastOffset;  // ignore dwBest
      } // if pLex

      // rotate
      if (dwOffsetThis) {
         // rotate harmonics
         if (!fFullPCM) {
            for (j = 0; j < SRPHASENUM; j++) {
               int iPhase = m_paSRFeature[i].abPhase[j];

               iPhase = iPhase - (int)dwOffsetThis * 256 / SRFEATUREPCM * (int)(j+1) + 0x10000;
               while (iPhase < 0)
                  iPhase += 0x10000;   // so positive

               m_paSRFeature[i].abPhase[j] = (BYTE)iPhase;
            } // j

            if (paSDP) {
               // convert detailed phase
               memcpy (SRFTemp.abPhase, m_paSRFeature[i].abPhase, sizeof(SRFTemp.abPhase));

               SRDETAILEDPHASEFromSRFEATURE (&m_paSRFeature[i], fPitch, paSDP);
            }
         }

         // rotate the PCM
#ifdef SRFEATUREINCLUDEPCM_SHORT
         // NOTE: Not tested
         short *pasSRF = m_paSRFeature[i].asPCM;
         memcpy (asTemp, pasSRF, sizeof(m_paSRFeature->asPCM));
         for (j = 0; j < SRFEATUREPCM; j++, pasSRF++)
            pasSRF[0] = asTemp[(j - dwOffsetThis + SRFEATUREPCM) % SRFEATUREPCM];
#else
         char *pacSRF = m_paSRFeature[i].acPCM;
         memcpy (acTemp, pacSRF, sizeof(m_paSRFeature->acPCM));
         for (j = 0; j < SRFEATUREPCM; j++, pacSRF++)
            pacSRF[0] = acTemp[(j - dwOffsetThis + SRFEATUREPCM) % SRFEATUREPCM];
#endif
      }

#if 0 //  - test temp test to make sure comes out ok
      SRFEATUREVoicedToPCM (m_paSRFeature + i, PitchAtSample(i * m_dwSRSkip, 0), acTemp, &SinLUT);

      m_paSRFeature[i].fPCMScale = 0.004;
      // fill in and rotate - dont need to sine phase modified
      //char *pacSRF = m_paSRFeature[i].acPCM;
      //memcpy (acTemp, pacSRF, sizeof(m_paSRFeature->acPCM));
      //for (j = 0; j < SRFEATUREPCM; j++, pacSRF++)
      //   pacSRF[0] = acTemp[(j - dwOffsetThis + SRFEATUREPCM) % SRFEATUREPCM];

      memcpy (m_paSRFeature[i].acPCM, acTemp, sizeof(m_paSRFeature[i].acPCM));
#endif // 0

      dwLastOffset = dwOffsetThis;
   } // i

   return TRUE;
}



/****************************************************************************
CM3DWave::ADPCMHackCompress - Hack compress the PCM samples of a wave into ADPCM,
and pretend that they're PCM when save. Used for sending TTS to/from the render
cache.

returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::ADPCMHackCompress (void)
{
   if (m_dwChannels != 1)
      return FALSE;

   CMem mem;
   if (!ADPCMCompress (m_psWave, m_dwSamples, &mem))
      return FALSE;

   size_t dwNeed = (mem.m_dwCurPosn + sizeof(DWORD) + sizeof(short)-1)/sizeof(short); // round up
   if (!m_pmemWave->Required (dwNeed * sizeof(short)))
      return FALSE;
   m_psWave = (short*) m_pmemWave->p;
   m_dwSamples = (DWORD) dwNeed;

   DWORD* pdw = (DWORD*)m_psWave;
   *pdw = (DWORD) mem.m_dwCurPosn;
   memcpy (pdw+1, mem.p, mem.m_dwCurPosn);
   return TRUE;
}



/****************************************************************************
CM3DWave::ADPCMHackDecompress - Reverses ADPCMHackCompress().

returns
   BOOL - TRUE if success
*/
BOOL CM3DWave::ADPCMHackDecompress (void)
{
   if (m_dwChannels != 1)
      return FALSE;

   DWORD *pdw = (DWORD*)m_psWave;

   CMem mem;
   if (!ADPCMDecompress (pdw+1, *pdw, &mem))
      return FALSE;

   if (!m_pmemWave->Required (mem.m_dwCurPosn))
      return FALSE;
   m_psWave = (short*) m_pmemWave->p;
   m_dwSamples = (DWORD) mem.m_dwCurPosn / sizeof(short);

   memcpy (m_psWave, mem.p, mem.m_dwCurPosn);
   return TRUE;
}


/****************************************************************************
ADPCMCompressBlock - Compresses a block of samples.

inputs
   short          *pasPCM - PCM to compress
   DWORD          dwSamples - This can't be any more than 256 samples. It has to be at least 1.
   int            iStartSample - Sample value at the start. Use 0 unless previous audio
   int            iStartDelta - Delta at the start. Use 0 unless previous audio
   DWORD          dwScale - Scale factor to use, from 1..256
   PVOID          *pMemory - Memory to write into.
   DWORD          dwSize - Size of the memory, in bytes
   int            *piStartSample - Filled with the start sample for the next block
   int            *piStartDelta - Filled with the start delta for the next block
   __int64        *piError - Fills in the error
returns
   DWORD - Number of bytes used in pbMemory. Or 0 if error.
*/
DWORD ADPCMCompressBlock (short *pasPCM, DWORD dwSamples, int iStartSample, int iStartDelta,
                          DWORD dwScale, PVOID pMemory, DWORD dwSize, int *piStartSample, int *piStartDelta,
                          __int64 *piError)
{
   // check some parameters
   if ((dwSamples < 1) || (dwSamples > 256))
      return FALSE;
   if ((dwScale < 1) || (dwScale > 256))
      return FALSE;
   DWORD dwNeed = sizeof(ADPCMBLOCKHEADER) + dwSamples * sizeof(char);
   if (dwSize < dwNeed)
      return FALSE;

   // where block header is
   PADPCMBLOCKHEADER pBH = (PADPCMBLOCKHEADER)pMemory;
   char *pac = (char*) (pBH+1);
   pBH->bSamples = (BYTE)(dwSamples-1);
   pBH->bScale = (BYTE)(dwScale-1);

   __int64 iError = 0;
   DWORD i;
   int iScale = (int)dwScale;
   int iScaleHalf = iScale/2;
   for (i = 0; i < dwSamples; i++, pac++, pasPCM++) {
      int iMomentum = iStartSample + iStartDelta;
      int iDelta = *pasPCM - iMomentum;
      if (iDelta >= 0)
         iDelta = (iDelta + iScaleHalf) / iScale;
      else {
         // since negative numbers not handled properly
         iDelta = -iDelta;
         iDelta = (iDelta + iScaleHalf) / iScale;
         iDelta = -iDelta;
      }

      iDelta = max(iDelta, -128);
      iDelta = min(iDelta, 127);

      pac[0] = (char)iDelta;

      iDelta *= iScale;
      iStartDelta += iDelta;
      iStartSample += iStartDelta;

      iError += (__int64)(iStartSample - (int)pasPCM[0]) * (__int64)(iStartSample - (int)pasPCM[0]);
   }

   *piStartSample = iStartSample;
   *piStartDelta = iStartDelta;
   *piError = iError;
   return dwNeed;
}



/****************************************************************************
ADPCMDecompressBlock - Decompresses a block of samples.

inputs
   int            iStartSample - Sample value at the start. Use 0 unless previous audio
   int            iStartDelta - Delta at the start. Use 0 unless previous audio
   PVOID          pMemory - Memory with the data
   DWORD          dwSize - Amount of data available, probably won't use it all.
   PCMem          pMemSamples - Memory with up-to-date m_dwCurPosn, that has samples appeneded to
   int            *piStartSample - Filled with the start sample for the next block
   int            *piStartDelta - Filled with the start delta for the next block
returns
   DWORD - Number of bytes used from pMemory. 0 if error.
*/
DWORD ADPCMDecompressBlock (int iStartSample, int iStartDelta, PVOID pMemory, DWORD dwSize,
                            PCMem pMemSamples,
                            int *piStartSample, int *piStartDelta)
{
   // header
   if (dwSize < sizeof(ADPCMBLOCKHEADER))
      return 0;   // error
   PADPCMBLOCKHEADER pBH = (PADPCMBLOCKHEADER)pMemory;
   char *pac = (char*)(pBH+1);
   DWORD dwSamples = (DWORD)pBH->bSamples+1;
   int iScale = (int)((DWORD)pBH->bScale+1);
   DWORD dwNeed = sizeof(ADPCMBLOCKHEADER) + dwSamples * sizeof(char);
   if (dwSize < dwNeed)
      return FALSE;

   // memory for the samples
   if (!pMemSamples->Required(pMemSamples->m_dwCurPosn + dwSamples * sizeof(short)))
      return FALSE;
   short *pasPCM = (short*)((PBYTE)pMemSamples->p + pMemSamples->m_dwCurPosn);
   pMemSamples->m_dwCurPosn += dwSamples * sizeof(short);

   DWORD i;
   for (i = 0; i < dwSamples; i++, pac++, pasPCM++) {
      iStartDelta += (int)pac[0] * iScale;
      iStartSample += iStartDelta;
      if (iStartSample < -32768)
         pasPCM[0] = -32768;
      else if (iStartSample > 32767)
         pasPCM[0] = 32767;
      else
         pasPCM[0] = (short)iStartSample;
   }

   *piStartSample = iStartSample;
   *piStartDelta = iStartDelta;
   return dwNeed;
}

/****************************************************************************
ADPCMCompressBlockOptimum - Determines optimum dwScale for a block.

inputs
   short          *pasPCM - PCM to compress
   DWORD          dwSamples - This can't be any more than 256 samples. It has to be at least 1.
   int            iStartSample - Sample value at the start. Use 0 unless previous audio
   int            iStartDelta - Delta at the start. Use 0 unless previous audio
   PVOID          *pMemory - Memory to write into.
   DWORD          dwSize - Size of the memory, in bytes
   int            *piStartSample - Filled with the start sample for the next block
   int            *piStartDelta - Filled with the start delta for the next block
returns
   DWORD - Number of bytes used in pbMemory. Or 0 if error.
*/
DWORD ADPCMCompressBlockOptimum (short *pasPCM, DWORD dwSamples, int iStartSample, int iStartDelta,
                          PVOID pMemory, DWORD dwSize, int *piStartSample, int *piStartDelta)
{
   __int64 aiError[16];
   DWORD dwBest = (DWORD)-1;
   
   DWORD i;
   DWORD dwScale;
   for (i = 0; i < 16; i++) {
      dwScale = (i+1) * (i+1);

      if (!ADPCMCompressBlock (pasPCM, dwSamples, iStartSample, iStartDelta, dwScale,
         pMemory, dwSize, piStartSample, piStartDelta, &aiError[i]))
         return 0;   // error

      if (dwBest == (DWORD)-1)
         dwBest = i;
      else if (aiError[i] < aiError[dwBest])
         dwBest = i;
   } // i

   // recompress
   dwScale = (dwBest+1) * (dwBest+1);
   return ADPCMCompressBlock (pasPCM, dwSamples, iStartSample, iStartDelta, dwScale,
         pMemory, dwSize, piStartSample, piStartDelta, &aiError[0]);
}


/****************************************************************************
ADPCMCompress - Compresses a range of PCM into memory.

inputs
   short          *pasPCM - PCM to compress
   DWORD          dwSamples - This can't be any more than 256 samples.
   PCMem          pMem - Filled with the memory. m_dwCurPosn set to number of bytes used.
returns
   BOOL - TRUE if success
*/
BOOL ADPCMCompress (short *pasPCM, DWORD dwSamples, PCMem pMem)
{
   pMem->m_dwCurPosn = 0;

   // guestimate memory
   size_t dwNeed = sizeof(ADPCMHEADER) + (dwSamples + 255) / 256 * (256 * sizeof(char) + sizeof(ADPCMBLOCKHEADER));
   if (!pMem->Required (dwNeed))
      return FALSE;

   PADPCMHEADER pAH = (PADPCMHEADER)pMem->p;
   memset (pAH, 0, sizeof(*pAH));
   pAH->dwSamples = dwSamples;
   pMem->m_dwCurPosn = sizeof(*pAH);

   // compress
   int iStartSample = 0;
   int iStartDelta = 0;
   DWORD dwSize;
   while (dwSamples) {
      DWORD dwSamplesThis = min(dwSamples, 256);
      dwNeed = pMem->m_dwCurPosn + 256 * sizeof(char) + sizeof(ADPCMBLOCKHEADER);
      if (!pMem->Required(dwNeed))
         return FALSE;
      pAH = (PADPCMHEADER)pMem->p;  // in case changed location

      dwSize = ADPCMCompressBlockOptimum(pasPCM, dwSamplesThis, iStartSample, iStartDelta,
         (PBYTE)pMem->p + pMem->m_dwCurPosn, (DWORD)(pMem->m_dwAllocated - pMem->m_dwCurPosn),
         &iStartSample, &iStartDelta);
      if (!dwSize)
         return FALSE;

      // move on
      pMem->m_dwCurPosn += dwSize;
      pasPCM += dwSamplesThis;
      dwSamples -= dwSamplesThis;
      pAH->dwBlocks++;
   }

   return TRUE;
}



/****************************************************************************
ADPCMDecompress - Decompresses a range of ADPCM into memory.

inputs
   PVOID          pMemory - Compressed memory
   DWORD          dwSize - Available size, may not use it all
   PCMem          pMemSamples - Start out with m_dwCurPosn = 0. Concatenates samples onto
                     the end of this, updating m_dwCurPosn.
returns
   DWORD - Number of bytes used. 0 if error
*/
DWORD ADPCMDecompress (PVOID pMemory, DWORD dwSize, PCMem pMemSamples)
{
   DWORD dwUsed = 0;
   if (dwSize < sizeof(ADPCMHEADER))
      return 0;
   PADPCMHEADER pAH = (PADPCMHEADER)pMemory;
   dwSize -= sizeof(ADPCMHEADER);
   dwUsed += sizeof(ADPCMHEADER);
   pMemory = (PBYTE)pMemory + sizeof(ADPCMHEADER);

   // make sure memory large enough
   if (!pMemSamples->Required (pAH->dwSamples * sizeof(short)))
      return 0;

   // loop
   DWORD i, dwUsedThis;
   int iStartSample = 0;
   int iStartDelta = 0;
   for (i = 0; i < pAH->dwBlocks; i++) {
      dwUsedThis = ADPCMDecompressBlock (iStartSample, iStartDelta, pMemory, dwSize,
         pMemSamples, &iStartSample, &iStartDelta);
      if (!dwUsedThis)
         return 0;

      dwUsed += dwUsedThis;
      dwSize -= dwUsedThis;
      pMemory = (PBYTE)pMemory + dwUsedThis;
   } // i

   // done
   return dwUsed;
}