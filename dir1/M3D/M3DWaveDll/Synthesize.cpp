/***************************************************************************
Synthesize.cpp - C++ object that takes speech recognition features and
pitch, and resynthesizes a voice.

begun 21/8/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <crtdbg.h>
//#include <mmreg.h>
//#include <msacm.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"

// #define OLDPSOLA              // use old (not officially correct) PSOLA code
// #define UNSKEWEDHANNING       // use skewed hanning window

// things to turn on/off
#define FEATUREHACK_INVERSEFFTSPECTRUM       // generate with inverse FFT
   // BUGFIX - reenabled

#ifdef _DEBUG
// #define FEATUREHACK_COPYWAVEEXACTLY          // to test, copything the wave exactly without any psola or phase shift
#endif

#define PSOLASTRECTCHAMOUNT               0.025    // determines threshhold at which "PSOLA" algorthm
                                                   // will shift from simple PCM time-domain stretch into
                                                   // purse PSOLA. In octaves
      // BUGFIX - Through experimentation, came up with 0.1. 0.0 sounds muffled. 0.3 sound clear, but
      // starts getting the pitch-shift artifacts



#define SUBHARMONICS 3        // centeral harmonic, and one above/below
#define HARMONICS    (100*SUBHARMONICS)
   // BUGFIX - Upped harmonics to 100 since trying to make noise with them, was 50

#define FFTSAMPLES         (SRFEATUREPCM * 4)
            // BUGFIX - Was using SRFEATUREPCM*2 == 512, which is too few because
            // when synthesize from PCM want more points so don't get aliasing


/***************************************************************************
CVoiceSynthesize::GenerateOctaveToWavelength - This fills in a table of SRDATAPOINTS entries,
corresponding to the data in the SRFEATURE, listing the wavelengths that each
of the pitches corresponds to. Wavelengths are given in samples

inputs
   DWORD       dwSamplesPerSec - from CM3DWave
   float       *pafWavelength - Filled in with SRDATAPOINTS entries
*/
void CVoiceSynthesize::GenerateOctaveToWavelength (DWORD dwSamplesPerSec, float *pafWavelength)
{
   DWORD i;
   for (i = 0; i < SRDATAPOINTS; i++) {
      pafWavelength[i] = pow (2, (float)i / SRPOINTSPEROCTAVE) * (float)SRBASEPITCH;
      pafWavelength[i] = (float)dwSamplesPerSec / pafWavelength[i];
   } // i
}

/***************************************************************************
CVoiceSynthesize::GetFundamental - Gets the fundmental pitch to be used
for the given SR sample. This does any pitch modification on the way.

inputs
   PCM3DWave      pWave - Wave to use
   DWORD          dwSRSample - Index into the speech recognition data, 0..pWave->m_dwSRSamples
returns
   fp - Fundemental pitch
*/
fp CVoiceSynthesize::GetFundamental (PCM3DWave pWave, DWORD dwSRSample)
{
   // get the pitch for this point
   double fPitchSample = (double)dwSRSample * (double)pWave->m_dwSRSkip / (double)pWave->m_adwPitchSkip[PITCH_F0];
   DWORD dwPitchSample = (DWORD) fPitchSample;
   dwSRSample = min(dwSRSample, pWave->m_dwSRSamples-1); // so dont go beyond end
   fPitchSample -= dwPitchSample;
   double fFund;
   if (dwPitchSample+1 < pWave->m_adwPitchSamples[PITCH_F0])
      fFund = (1.0 - fPitchSample) * pWave->m_apPitch[PITCH_F0][dwPitchSample].fFreq +
         fPitchSample * pWave->m_apPitch[PITCH_F0][dwPitchSample+1].fFreq;
   else
      fFund = pWave->m_apPitch[PITCH_F0][pWave->m_adwPitchSamples[PITCH_F0]-1].fFreq;
   if (fFund < CLOSE)
      return (fp) fFund;  // error

   return (fp) fFund;
}


/***************************************************************************
CVoiceSynthesize::InverseFFTSpectrum - Takes informaton from CalcVolumeAndPitch,
and creates an inverse FFT to fill in a buffer with the voiced audio.
See if this gets rid of some of the phase problems.

ONLY call this if phaselock is on

inputs
   PSRFEATURE     pSRF - Used to get the phase information
   DWORD          dwHarmonics - As per CalcVolumeAndPitch(). This only cares
                  about the main harmonic. The 1st sub-harmonic is used for noise
   DWORD          *pdawHarmVolume - Filled in with the harmonic's volume. Multiply
                  this by sin(x) and divide by 0x10000 to get the actual level.
                  From CalcVolumeAndPitch
   DWORD          dwSamplesPerSec - Sampling rate
   fp             fPitch - Pitch of wave at this point. Used to determine nyquist cutoff
   BOOL           fNoiseForFFT - If TRUE then noise will be generated in this FFT.
   PWAVESEGMENTFLOAT pWSF - If this exists, then phase will be gotten from this
   float          *pafWave - Filled with the wave, contains FFTSAMPLES samples. Must be power of 2
   PCSinLUT       pSinLUT - SinLUT for FFT
   PCMem          pMemFFTScratch - For FFT
returns
   none
*/
void CVoiceSynthesize::InverseFFTSpectrum (PSRFEATURE pSRF, DWORD dwHarmonics, DWORD *padwHarmVolume,
                                           DWORD dwSamplesPerSec, fp fPitch, BOOL fNoiseForFFT,
                                           PWAVESEGMENTFLOAT pWSF, float *pafWave,
                                           PCSinLUT pSinLUT, PCMem pMemFFTScratch)
{
   // nyquist limit
   fp fNyquist = (fp)dwSamplesPerSec / 2.0 / fPitch;
   DWORD dwNyquist = floor (fNyquist);
   dwNyquist = min(dwNyquist, dwHarmonics / SUBHARMONICS);  // so dont excede amount of data

   // fill in information for FFT
   DWORD i;
   memset (pafWave,0, FFTSAMPLES * sizeof(float));
   for (i = 0; i < dwNyquist; i++) {
      DWORD dwVolume = padwHarmVolume[i*SUBHARMONICS];
      DWORD dwVolumeNoise = fNoiseForFFT ? padwHarmVolume[i*SUBHARMONICS+1] : 0;
      if (!dwVolume && !dwVolumeNoise)
         continue;   // nothing

      fp fVolume = (fp)(dwVolume / 0x10000);
      fp fVolumeNoise = (fp)(dwVolumeNoise / 0x10000);

      // if there's a sample wave to use for phase, then use that
      if (pWSF) {
         DWORD dwHarmWSF = min(i+1, SRFEATUREPCM/2-1);
         fp fTotal = sqrt(pWSF->afPCM[dwHarmWSF*2+0] * pWSF->afPCM[dwHarmWSF*2+0] +
            pWSF->afPCM[dwHarmWSF*2+1] * pWSF->afPCM[dwHarmWSF*2+1]);
         if (fTotal) {
            fVolume /= fTotal;
            pafWave[(i+1)*2 + 0] = pWSF->afPCM[dwHarmWSF*2+0] * fVolume;
            pafWave[(i+1)*2 + 1] = pWSF->afPCM[dwHarmWSF*2+1] * fVolume;
         }
         else {
            pafWave[(i+1)*2 + 0] = fVolume;  // since unknown
            pafWave[(i+1)*2 + 1] = 0.0;
         }
      }
      else {
         // agle
         fp fAngle = (i < SRPHASENUM) ? ((fp)pSRF->abPhase[i] / 256.0 * 2.0 * PI) : 0;

         pafWave[(i+1)*2 + 0] = sin(fAngle) * fVolume;
         pafWave[(i+1)*2 + 1] = cos(fAngle) * fVolume;
      }

      // noise
      if (fVolumeNoise) {
         fp fAngle = randf (0.0, 2.0 * PI);
         pafWave[(i+1)*2 + 0] += sin(fAngle) * fVolumeNoise;
         pafWave[(i+1)*2 + 1] += cos(fAngle) * fVolumeNoise;
      }
   } // i

   // invert fft
   FFTRecurseReal (pafWave - 1, FFTSAMPLES, -1, pSinLUT, pMemFFTScratch);
}


/***************************************************************************
CVoiceSynthesize::CalcVolumeAndPitch - Calculates the volume and pitch for each harmonic
at a given SR window.


inputs
   PCM3DWave      pWave - Wave to use
   DWORD          dwSRSample - Index into the speech recognition data, 0..pWave->m_dwSRSamples
   PSRFEATURE     pSRFeature - SR feature information to use
   DWORD          dwHarmonics - Number of harmonics to calculate
                  NOTE: This includes SUBHARMONICS, so actually dwHarmonics/SUBHARMONICS
                  harmonics, the rest are used to generate noise. Actual harmonics start
                  at elem 0 and skip every SUBHARMONICS elems

                  If fNoiseForFFT, then (SUBHARMONICS*n + 1) have the noise energy.

   fp             fHarmonicDelta - Amount that increase the frequency of the harmonic, in
                  fundemental units. Default is 1.0, but can change to make strange voices
   BOOL           fNoiseOnly - if TRUE then only the noise portion of the voice is generated,
                  else it's all generated
   BOOL           fNoiseForFFT - If TRUE then the noise will be generated by a FFT
   BOOL           fEnablePCM - If TRUE then enable PCM replacement.

   float          *pafWavelength - Output from GenerateOctaveToWavelength
   DWORD          *padwHarmAngleDelta - Filled in with an array of dwHarmonics
                  DWORDs. This will contain, for each harmonic, the delta in
                  angle (where angle is 0 to 0xffffffff instead of 0 to 2PI) for
                  every sample.
   DWORD          *pdawHarmVolume - Filled in with the harmonic's volume. Multiply
                  this by sin(x) and divide by 0x10000 to get the actual level
   float          *pfPCMEnergy - PCM energy, for including PCM.
returns
   BOOL - TRUE if success
*/
BOOL CVoiceSynthesize::CalcVolumeAndPitch (PCM3DWave pWave, DWORD dwSRSample,
                                           PSRFEATURE pSRFeature, DWORD dwHarmonics, fp fHarmonicDelta, BOOL fNoiseOnly,
                                           BOOL fNoiseForFFT, BOOL fEnablePCM,
                         float *pafWavelength, DWORD *padwHarmAngleDelta, DWORD *padwHarmVolume, float *pfPCMEnergy)
{
   *pfPCMEnergy = 0.0;  // clear for now
   double fFund = GetFundamental (pWave, dwSRSample);

   // BUGFIX - Found that female voice doesn't have enough noise (For s's, sh's, f's), because of the way
   // that generate noise from sum of sine wave. Therefore, compensiate noise scaling
   // Using 125.0 because that's the frequency I spoke at, and at which the noise resynthesis is
   // reasonably accurate
   // BUGFIX - If < 125 hz then don't scale unvoiced down, since problem for blizzard2007 voice, losing unvoiced
   double fNoiseScale;
   if (fNoiseForFFT)
      fNoiseScale = 1.0;
   else {
      fNoiseScale = fFund ? sqrt(max(fFund / 125.0, 1.0)) : 1;
      fNoiseScale = max(fNoiseScale, .1); // so not too extreme
      fNoiseScale = min(fNoiseScale, 10);
   }

   // detetermine if doing any harmonics scaling
   DWORD i;

#if 0 // convert voiced to noise
   SRFEATURE srfTemp;
   memcpy (srfTemp.abPhase, pSRFeature->abPhase, sizeof(srfTemp.abPhase));
   for (i = 0; i < SRDATAPOINTS; i++) {
      srfTemp.acVoiceEnergy[i] = AmplitudeToDb(DbToAmplitude (pSRFeature->acNoiseEnergy[i]) + DbToAmplitude (pSRFeature->acVoiceEnergy[i]));
      srfTemp.acNoiseEnergy[i] = -127;
   }
   pSRFeature = &srfTemp;
#endif // 0

   // loop through all the harmonics
   double fFreq, fWL;
   DWORD dwLastWLIndex = 0;
   for (i = 0, fFreq = fFund; i < dwHarmonics / SUBHARMONICS; i++) {
      // convert this to wavelength in samples
      fWL = (double)pWave->m_dwSamplesPerSec / fFreq;

      // find the index to use, since wavelength only decreases as go up the index
      // look for the highest one and use the value before that
      while ((dwLastWLIndex < SRDATAPOINTS) && (fWL < pafWavelength[dwLastWLIndex]))
         dwLastWLIndex++;

      // interpolate to figure out volume...
      char acInterp[2];
      double afAmp[2];
      DWORD dwNoise;
      // BUGFIX - Calculate the amplitudes for both noise and voiced and do
      // some wierd stuff to create noise from sinewave sytnhesis
      for (dwNoise = 0; dwNoise < 2; dwNoise++) {
         char *pac = (dwNoise ?
            &pSRFeature->acNoiseEnergy[0] :
            &pSRFeature->acVoiceEnergy[0]);

         fp fAlpha = 0;
         if (!dwLastWLIndex) {
            // BUGFIX - If Lower than 100 hz then have to gradually decrease volume for another octave, before zeroing
            // slow code... if less than limits of what we calculated, then gradually interolate 0 value
            if ((pac[0] > -110) && (fFreq < SRBASEPITCH)) {
               double fOctave = (fFreq > 0) ? (log(fFreq / (fp)SRBASEPITCH) / log((fp)2)) : -10;
               if (fOctave > -1)
                  acInterp[0] = acInterp[1] = AmplitudeToDb (DbToAmplitude (pac[0]) * (1.0 + fOctave));
               else
                  acInterp[0] = acInterp[1] =  -127;  // quiet
            }
            else
               acInterp[0] = acInterp[1] = pac[0];
         }
         else if (dwLastWLIndex >= SRDATAPOINTS)
            acInterp[0] = acInterp[1] = -127;   // quiet
         else {
            acInterp[0] = pac[dwLastWLIndex-1];
            acInterp[1] = pac[dwLastWLIndex];
            fAlpha = (fWL - pafWavelength[dwLastWLIndex]) /
               (pafWavelength[dwLastWLIndex-1] - pafWavelength[dwLastWLIndex]);
         }

         // volume needs to be counter-adjusted by SR weight
         DWORD dwInterp;
         int iDBAdjust1, iDBAdjust2;
         dwInterp = (dwLastWLIndex ? (dwLastWLIndex-1) : 0);
         iDBAdjust1 = ((int)dwInterp - (int)SRDATAPOINTS/2) * 6 / SRPOINTSPEROCTAVE;
         iDBAdjust2 = ((int)dwLastWLIndex - (int)SRDATAPOINTS/2) * 6 / SRPOINTSPEROCTAVE;
         if ((int)acInterp[0] - iDBAdjust1 > -127)
            acInterp[0] -= (char)iDBAdjust1;
         else
            acInterp[0] = -127;
         if ((int)acInterp[1] - iDBAdjust2 > -127)
            acInterp[1] -= (char)iDBAdjust2;
         else
            acInterp[1] = -127;

         // calculate the ampltidde
         afAmp[dwNoise] = fAlpha * (float)DbToAmplitude(acInterp[0]) + (1.0 - fAlpha) * (float)DbToAmplitude(acInterp[1]);
      }


      // if go beyond the end then use 0 amplitude
      BOOL fEOF = (dwSRSample >= pWave->m_dwSRSamples);
      if (fEOF)
         afAmp[0] = afAmp[1] = 0;

#ifdef SRFEATUREINCLUDEPCM
      // steal away some of the amplitude since will be going to PCM
      DWORD dwHarmPCM = i + 1;
      if (fEnablePCM && (dwHarmPCM < (DWORD) pSRFeature->bPCMHarmNyquist) && (dwHarmPCM > (DWORD)pSRFeature->bPCMHarmFadeStart)) {
         fp fTakeAway;
         if (dwHarmPCM >= (DWORD)pSRFeature->bPCMHarmFadeFull)
            fTakeAway = 1.0;
         else
            fTakeAway = (fp)(dwHarmPCM - (DWORD)pSRFeature->bPCMHarmFadeStart) / (fp)(pSRFeature->bPCMHarmFadeFull - pSRFeature->bPCMHarmFadeStart);

         // put into PCM energy and take away from this
         *pfPCMEnergy += fTakeAway * (afAmp[0] + afAmp[1]);
         afAmp[0] *= (1.0 - fTakeAway);
         afAmp[1] *= (1.0 - fTakeAway);
      }
#endif

      // special considerations...
      // if the wavelength < 2 samples then it exceeds the nyquist limit, so cap
      // it at 2 samples and set the amplitude to 0
      if (fWL <= 2.0) {
         fWL = 2.0;
         afAmp[0] = afAmp[1] = 0;
      }
      double fAmp;
      double fSub = fFund / (double)max(1,(SUBHARMONICS-1));
      double fNoiseAmt = afAmp[1] / max(afAmp[0] + afAmp[1], CLOSE);

      // BUGFIX - scale afAmp for noise, by noisescale
      afAmp[1] *= fNoiseScale;

      // figure out subharmonics scaling
      double fHarmScaleAmt;
      fHarmScaleAmt = 1.0;

      // The more noise, the greater the spread of subharmonics
      fSub *= fNoiseAmt;
         // BUGFIX - put back in so if get noised in a voiced section it isn't bad
      double fOffset = fSub * (-(double)(SUBHARMONICS-2) / 2.0);
      double fAmt;
      BOOL fVoiced;;
      DWORD j;
      for (j = 0; j < SUBHARMONICS; j++) {
         fVoiced = (j == 0);
         // noise is divided amongst all the harmonics, while voiced is only
         // present in the central harminc
         //fAmp = fVoiced ? afAmp[0] : (afAmp[1] / pow(SUBHARMONICS-1, fNoiseAmt));
            // BUGFIX - Added sqrt() to SUBHARMONICS to preserve energy... at least seems to work
            // BUGFIX - Try not dividing by harmonics at all
         fAmp = fVoiced ? afAmp[0] : afAmp[1];

         // if the amplitude is higher than can store than cap it
         fAmp = min(fAmp, 0xffff);

         // convert this to wavelength in samples
         fWL = fFreq + (fVoiced ? 0 : (fOffset + fSub * (double)(j-1)));
         fAmt = fSub / 2;  // since can only go up/down by half
         if (fVoiced) {
            // randomize voiced, but not as much
            //double fRand = (fAmp - afAmp[0]) / max(fAmp,CLOSE);
            //fAmt *= fRand;
            fAmt = 0; // BUGFIX - Set to 0 fWL / 100.0;  // BUGFIX - allow to move a bit so sounds a bit more natural
         }
         //if (!fVoiced)
         if (fAmt)
            fWL += randf(-fAmt, fAmt);
         fWL = (double)pWave->m_dwSamplesPerSec / fWL;

         // if them main harmonic then scale it by whatever it's supposed to be
         if (!j) {
            fAmp *= fHarmScaleAmt;

            // if only genrating noise then set amp to 0
            if (fNoiseOnly)
               fAmp = 0;
         }

         padwHarmAngleDelta[i*SUBHARMONICS+j] = (DWORD)((double)0x10000 * (double)0x10000 / fWL);
         padwHarmVolume[i*SUBHARMONICS+j] = (DWORD)((double)0x10000 * fAmp);
      } // j

      // increase frequency
      fFreq += (fFreq / (fp)(i+1)) * fHarmonicDelta;
   } // i

#ifdef SRFEATUREINCLUDEPCM
   // scale this by the feature's scale indicator
   *pfPCMEnergy *= pSRFeature->fPCMScale;
#endif

   return TRUE;
}


/***************************************************************************
CVoiceSynthesize::SynthesizeVoiced - Takes a wave with valid SRFEATURE and pitch entries, and
synthesizes the voiced area. This adds to the existing amplitudes, so the
caller should have zeroed out the audio first.

inputs
   PCM3DWave         pWave - Wave to use
   BOOL              fNoiseOnly - If TRUE then generate the noise only... do this
                     if appending onto voice-from-wave
   PCListFixed       plWAVESEGMENTFLOAT - If not using PSOLA, can pass in an array of FFTs for each of the
                     SRFEATUREs and will use that to get phase. If using psola, this is igored, so pass
                     in NULL.
   BOOL              fEnablePCM - Normally TRUE, but might be FALSE if whispering.
                     Will automatically be set to FALSE if non-harmonic voice.
   PCProgressSocket  pProgress - Progress
   PCProgressWaveSample pProgressWave - This is an accurate progress bar used
                  so that will be able to play out TTS while it's being generated.
returns
   BOOL - TRUE if success. FALSE if can't find srfeature or pitch
*/
BOOL CVoiceSynthesize::SynthesizeVoiced (PCM3DWave pWave, BOOL fNoiseOnly, PCListFixed plWAVESEGMENTFLOAT, BOOL fEnablePCM, PCProgressSocket pProgress,
                                         PCProgressWaveSample pProgressWave)
{
   PSRFEATURE pSR = pWave->m_paSRFeature;
   DWORD dwNumSR = pWave->m_dwSRSamples;
   DWORD dwSRSkip = pWave->m_dwSRSkip;
   PWVPITCH pPitch = pWave->m_apPitch[PITCH_F0];
   DWORD dwNumPitch = pWave->m_adwPitchSamples[PITCH_F0];
   DWORD dwPitchSkip = pWave->m_adwPitchSkip[PITCH_F0];
   DWORD i, j, k;
   if (!pSR || !dwNumSR || !pPitch || !dwNumPitch)
      return FALSE;

   // create a cos-square interpolation, hanning window
   CMem memHanning;
   if (!memHanning.Required (dwSRSkip * sizeof(fp)))
      return FALSE;
   fp *pafHanning = (fp*)memHanning.p;
   for (i = 0; i < dwSRSkip; i++) {
      pafHanning[i] = HanningWindow ((fp)i / (fp)dwSRSkip / 2.0 + 0.5);
      // fp fCos = cos((fp)i / (fp)dwSRSkip * PI / 2.0);
      // pafHanning[i] = fCos * fCos;
   } // i

   // calculate the wavelengths
   float afWavelength[SRDATAPOINTS];
   GenerateOctaveToWavelength (pWave->m_dwSamplesPerSec, afWavelength);

   BOOL fUsePhaseLock = (fabs(m_fHarmonicSpacing-1) < CLOSE);
   if (!fUsePhaseLock)
      fEnablePCM = FALSE;

   // fEnablePCM = FALSE; // to test

   BOOL fNoiseForFFT = FALSE;
#ifdef FEATUREHACK_INVERSEFFTSPECTRUM
   if (fUsePhaseLock)
      fNoiseForFFT = TRUE;
#endif

   // figure out some starting volume and pitch values
   DWORD adwAngleDelta[2][HARMONICS];
   DWORD adwAmp[2][HARMONICS];
   SRFEATURE aSRFeature[2];
   float afPCMEnergy[2];   // energy for each
   aSRFeature[1] = pWave->m_paSRFeature[0];
   CalcVolumeAndPitch (pWave, 0, &aSRFeature[1], HARMONICS, m_fHarmonicSpacing, fNoiseOnly, fNoiseForFFT, fEnablePCM,
      afWavelength, adwAngleDelta[1], adwAmp[1], &afPCMEnergy[1]);

#ifdef FEATUREHACK_INVERSEFFTSPECTRUM
   float afFFTWave[2][FFTSAMPLES];
   CSinLUT SinLUT;
   CMem memFFTScratch;
   if (fUsePhaseLock)
      InverseFFTSpectrum (pWave->m_paSRFeature + 0, HARMONICS, adwAmp[1], pWave->m_dwSamplesPerSec,
         pWave->PitchAtSample(PITCH_F0, 0 * pWave->m_dwSRSkip,0), fNoiseForFFT,
         plWAVESEGMENTFLOAT ? (PWAVESEGMENTFLOAT) plWAVESEGMENTFLOAT->Get(0) : NULL,
         afFFTWave[1], &SinLUT, &memFFTScratch);
#endif

   // initial phase
   DWORD adwPhase[HARMONICS];
   memset (adwPhase, 0, sizeof(adwPhase));
   DWORD dwPhaseFundamentalNoAngle = 0;

   // memory to store sum
   CMem memBuf;
   if (!memBuf.Required (sizeof(double)*pWave->m_dwSRSkip + sizeof(DWORD)*pWave->m_dwSRSkip))
      return FALSE;
   double *pafBuf = (double*)memBuf.p;
   DWORD *padwFundPhase = (DWORD*) (pafBuf + pWave->m_dwSRSkip);

   // loop
   int aiAngleDeltaDelta[HARMONICS];
   int aiAmpDelta[HARMONICS];
   DWORD adwPhaseLock[max(HARMONICS,SRPHASENUM)];   // BUGFIX - Was just SRPHASENUM, but need to randomize high-level ones
   DWORD adwPhaseLockDelta[max(HARMONICS,SRPHASENUM)];
   DWORD dwSide;
#if 0
   fUsePhaseLock = FALSE;
#endif
   for (i = 0; i < dwNumSR; i++) {
      // BUGFIX - Don't send progress until 1/3 second in, so wont get
      // hiccup when doing real-time playback

      // BUGFIX - Make this 1/4 of a second, from 1/3

      if ((i > pWave->m_dwSRSAMPLESPERSEC/4) && !(i%10)) {
         if (pProgress)
            pProgress->Update ((fp)i / (fp)dwNumSR);
         if (pProgressWave)
            if (!pProgressWave->Update (pWave, min(i * dwSRSkip,pWave->m_dwSamples), min(dwNumSR * dwSRSkip,pWave->m_dwSamples)))
               return FALSE;
      }

      // move all the calculated pitch and amplitude down...
      memcpy (adwAngleDelta[0], adwAngleDelta[1], HARMONICS * sizeof(DWORD));
      memcpy (adwAmp[0], adwAmp[1], HARMONICS * sizeof(DWORD));
      aSRFeature[0] = aSRFeature[1];
      afPCMEnergy[0] = afPCMEnergy[1];

      // get new entries
      if (i+1 < dwNumSR) {
         aSRFeature[1] = pWave->m_paSRFeature[i+1];

#ifdef _DEBUG
         // to see what the net result is
         pWave->m_paSRFeature[i+1] = aSRFeature[1];
#endif
      }
      CalcVolumeAndPitch (pWave, i+1, &aSRFeature[1],
         HARMONICS, m_fHarmonicSpacing, fNoiseOnly, fNoiseForFFT, fEnablePCM, afWavelength, adwAngleDelta[1], adwAmp[1], &afPCMEnergy[1]);

#ifdef FEATUREHACK_INVERSEFFTSPECTRUM
      if (fUsePhaseLock) {
         memcpy (afFFTWave[0], afFFTWave[1], FFTSAMPLES * sizeof(float));
         InverseFFTSpectrum (pWave->m_paSRFeature + (i+1), HARMONICS, adwAmp[1], pWave->m_dwSamplesPerSec,
            pWave->PitchAtSample(PITCH_F0, (i+1) * pWave->m_dwSRSkip,0), fNoiseForFFT,
            plWAVESEGMENTFLOAT ? (PWAVESEGMENTFLOAT) plWAVESEGMENTFLOAT->Get(i+1) : NULL,
            afFFTWave[1], &SinLUT, &memFFTScratch);
      }
#endif

      // calculate the delta of these...
      for (j = 0; j < HARMONICS; j++) {
         aiAngleDeltaDelta[j] = ((int)adwAngleDelta[1][j] - (int)adwAngleDelta[0][j]) / (int)dwSRSkip;
         aiAmpDelta[j] = ((int)adwAmp[1][j] - (int)adwAmp[0][j]) / (int)dwSRSkip;
      } // j

      // calculate the phase lock... which is meant to keep harmonics in phase
      // with the fundamental
      if (fUsePhaseLock) for (k = 0; k < max(SRPHASENUM, HARMONICS); k++) {
         // BUGFIX - If phase is higher than SRPHASENUM then loop the last few phases so that
         // get semi-random phases on top
         DWORD dwPhaseIndex = k;
         //if (dwPhaseIndex >= SRPHASENUM)
#define PHASELOOP                (SRPHASENUM/4)
         //   dwPhaseIndex = ((dwPhaseIndex - SRPHASENUM) % PHASELOOP) + (SRPHASENUM-PHASELOOP);

         DWORD dwStart = (DWORD)aSRFeature[0].abPhase[dwPhaseIndex] << 24;
         DWORD dwEnd = (DWORD)aSRFeature[1].abPhase[dwPhaseIndex] << 24;
            // BUGFIX - Was using pWave->m_paSRFeature, but used aSRFeature instead so could tweak
         __int64 iStart = (__int64)dwStart;
         __int64 iEnd = (__int64)dwEnd;
         
         // keep phase shift as small as possible, so if more than .5 apart
         // then move the lower one up one phase unit...
         if ((max(dwStart,dwEnd) - min(dwStart,dwEnd)) > 0x80000000) {
            if (dwStart < dwEnd)
               iStart += (__int64)0x100000000;
            else
               iEnd += (__int64)0x100000000;
         }

         adwPhaseLock[k] = dwStart;
         adwPhaseLockDelta[k] = (DWORD)(int)((iEnd - iStart) / (__int64)dwSRSkip);
      } // k

      // clear the buffer
      memset (pafBuf, 0, sizeof(double)*pWave->m_dwSRSkip);

      // loop over all the harmonics
      for (k = 0; k < HARMONICS; k++) {
         // if totally quiet ignore
         if (k && (adwAmp[0][k] == 0) && (aiAmpDelta[k] == 0))
            continue;

         // loop over all the points
         for (j = 0; j < dwSRSkip; j++) {
            if (k == 0)
               dwPhaseFundamentalNoAngle += adwAngleDelta[0][k];

            // calculate the sample
            DWORD dwPhase;
            if (fUsePhaseLock && /*k &&*/ !(k%SUBHARMONICS) && (k < max(HARMONICS,SRPHASENUM) * SUBHARMONICS)) {
                     // BUGFIX - No check for k so can have phase for fundamental
               DWORD dwHarm = k / SUBHARMONICS;
               dwPhase = padwFundPhase[j] * (dwHarm+1);
               dwPhase += adwPhaseLock[dwHarm];   // increase by the phase lock

               adwPhaseLock[dwHarm] += adwPhaseLockDelta[dwHarm];
            }
            else {
               dwPhase = adwPhase[k];

               // increase the phase
               adwPhase[k] += adwAngleDelta[0][k];
            }

#ifdef SRFEATUREINCLUDEPCM
            // NOTE: This does NOT use PSOLA. It's not really worth fixing either since this
            // is mostly a test case. However, because pitch error assumes PSOLA, synthesizing
            // using this will produce wierd speech

            // include PCM
            if (!k && (afPCMEnergy[0] || afPCMEnergy[1])) {
               double fOffset = (double)(dwPhase >> 16) / (double)0x10000 * (double)SRFEATUREPCM;
               DWORD dwLeft = (DWORD)fOffset;
               DWORD dwRight = (dwLeft+1) % SRFEATUREPCM;
               fOffset -= (double)dwLeft;
               fp fOneMinusOffset = 1.0 - fOffset;

               fp fAlpha = 1.0 - pafHanning[j]; // (fp)j / (fp)dwSRSkip;

               // combine two PCMs, ramping in energy
               for (dwSide = 0; dwSide < 2; dwSide++) {
                  // if no energy then no contribution
                  if (!afPCMEnergy[dwSide])
                     continue;

                  // interp points
#ifdef SRFEATUREINCLUDEPCM_SHORT
                  fp fInterp = (fp)aSRFeature[dwSide].asPCM[dwLeft] * fOneMinusOffset +
                     (fp)aSRFeature[dwSide].asPCM[dwRight] * fOffset;
#else
                  fp fInterp = (fp)aSRFeature[dwSide].acPCM[dwLeft] * fOneMinusOffset +
                     (fp)aSRFeature[dwSide].acPCM[dwRight] * fOffset;
#endif
                  pafBuf[j] += fInterp * (dwSide ? fAlpha : (1.0 - fAlpha)) * afPCMEnergy[dwSide];
               } // dwSide
            } // if fundamental
#endif

#ifdef FEATUREHACK_INVERSEFFTSPECTRUM
            if (!k && fUsePhaseLock) {
               double fOffset = (double)(dwPhase >> 16) / (double)0x10000 * (double)FFTSAMPLES;
               DWORD dwLeft = (DWORD)fOffset;
               DWORD dwRight = (dwLeft+1) % FFTSAMPLES;
               fOffset -= (double)dwLeft;
               fp fOneMinusOffset = 1.0 - fOffset;

               fp fAlpha = 1.0 - pafHanning[j]; // (fp)j / (fp)dwSRSkip;

               // combine two PCMs, ramping in energy
               for (dwSide = 0; dwSide < 2; dwSide++) {
                  // interp points
                  fp fInterp = afFFTWave[dwSide][dwLeft] * fOneMinusOffset +
                     afFFTWave[dwSide][dwRight] * fOffset;
                  pafBuf[j] += fInterp * (dwSide ? fAlpha : (1.0 - fAlpha));
               } // dwSide
            }

            // if voiced audio and using phase lock, then dont bother synthsizing futher
            if (k && !(k % SUBHARMONICS) && fUsePhaseLock)
               continue;   // don't do non-phase

            // if generating noise in FFT then ampltidue is 0
            if (fNoiseForFFT && (k % SUBHARMONICS))
               continue;
#endif

            double fSin = (double)SineLUT (dwPhase);

#if 0 // def _DEBUG
            fSin = sin((double)dwPhase / (double)0x10000 / (double)0x10000 * 2.0 * PI) * (double)0x10000;
#endif


#ifdef FEATUREHACK_INVERSEFFTSPECTRUM
            if ((k || !fUsePhaseLock) && adwAmp[0][k])   // so dont add
#endif
               pafBuf[j] += fSin / (double)0x10000 * (double)adwAmp[0][k] / (double)0x10000;

            // store away fundemantel phase
            if (fUsePhaseLock && !k)
               padwFundPhase[j] = dwPhaseFundamentalNoAngle;

            // finally, increase angle and amplitude by delta
            adwAngleDelta[0][k] = (DWORD)((int)adwAngleDelta[0][k] + aiAngleDeltaDelta[k]);

            // if noise and using noise FFT then skip the rest
            adwAmp[0][k] = (DWORD)((int)adwAmp[0][k] + aiAmpDelta[k]);
         } // j
      } // k

      // write out the samples
      for (j = 0; j < dwSRSkip; j++) {
         // wriet out the sample
         double fTemp;
         DWORD dwSample = (i * dwSRSkip) + j;
         if (dwSample >= pWave->m_dwSamples)
            break;   // went beyond end
         dwSample *= pWave->m_dwChannels;
         for (k = 0; k < pWave->m_dwChannels; k++) {
            fTemp = pafBuf[j] + (double)pWave->m_psWave[dwSample + k];
            fTemp = min(fTemp, 32767);
            fTemp = max(fTemp, -32768);
            pWave->m_psWave[dwSample+k] = (short)fTemp;
         } // k
      } // j
   } // i

   // BUGFIX - Final update
   if (pProgress)
      pProgress->Update ((fp)i / (fp)dwNumSR);
   if (pProgressWave)
      if (!pProgressWave->Update (pWave, min(i * dwSRSkip,pWave->m_dwSamples), min(dwNumSR * dwSRSkip,pWave->m_dwSamples)))
         return FALSE;

   return TRUE;
}

#if 0 // dead code
/***************************************************************************
CVoiceSynthesize::SynthesizeNoisePacket - Synthesizes a packet of noise from the given SRFEATURE.

inputs
   PCM3DWave      pWave - Wave. Information like samples per sec used for this
   PSRFEATURE     pSRFeature - SR feature to use
   float          *pafWavelength - Output from GenerateOctaveToWavelength
   float          *pafNoise - Filled in with 2*pWave->m_dwSRSkip samples.. no windowing applied
returns
   none
*/
void CVoiceSynthesize::SynthesizeNoisePacket (PCM3DWave pWave, PSRFEATURE pSRFeature, float *pafWavelength,
                            float *pafNoise)
{
   // allocate enough memory for a FFT
   DWORD dwNoiseNum = pWave->m_dwSRSkip*2;
   DWORD dwWindow;
   CMem memFFT;
   for (dwWindow = 2; dwWindow < dwNoiseNum; dwWindow *= 2);
   dwWindow *= 2;
   if (!memFFT.Required (dwWindow * sizeof(float)))
      return;
   float *pafFFT = (float*)memFFT.p;

   // fill this in
   DWORD i;
   DWORD dwLastWLIndex = 0;
   pafFFT[0] = pafFFT[1] = 0; // always 0 and bottom
   for (i = 1; i < dwWindow/2; i++) {
      // figure out this wavelength
      fp fWL = (fp)dwNoiseNum / i;

      // find the index to use, since wavelength only decreases as go up the index
      // look for the highest one and use the value before that
      while ((dwLastWLIndex < SRDATAPOINTS) && (fWL < pafWavelength[dwLastWLIndex]))
         dwLastWLIndex++;

      // interpolate to figure out volume...
      char acInterp[2];
      double fAmp;
      fp fAlpha = 0;
      if (!dwLastWLIndex)
         acInterp[0] = acInterp[1] = pSRFeature->acNoiseEnergy[0];
      else if (dwLastWLIndex >= SRDATAPOINTS)
         acInterp[0] = acInterp[1] = -127;   // quiet
      else {
         acInterp[0] = pSRFeature->acNoiseEnergy[dwLastWLIndex-1];
         acInterp[1] = pSRFeature->acNoiseEnergy[dwLastWLIndex];
         fAlpha = (fWL - pafWavelength[dwLastWLIndex]) /
            (pafWavelength[dwLastWLIndex-1] - pafWavelength[dwLastWLIndex]);
      }

      // volume needs to be counter-adjusted by SR weight
      DWORD dwInterp;
      dwInterp = (dwLastWLIndex ? (dwLastWLIndex-1) : 0);
      if (acInterp[0] > -127 + SRDATAPOINTS/2*6/SRPOINTSPEROCTAVE)
         acInterp[0] -= (char)(((int)dwInterp - (int)SRDATAPOINTS/2) * 6 / SRPOINTSPEROCTAVE);
      else
         acInterp[0] = -127;
      if (acInterp[1] > -127 + SRDATAPOINTS/2*6/SRPOINTSPEROCTAVE)
         acInterp[1] -= (char)(((int)dwLastWLIndex - (int)SRDATAPOINTS/2) * 6 / SRPOINTSPEROCTAVE);
      else
         acInterp[1] = -127;

      // calculate the ampltidde
      fAmp = fAlpha * DbToAmplitude(acInterp[0]) + (1.0 - fAlpha) * DbToAmplitude(acInterp[1]);

      // nyquist limit
      if (fWL <= 2.0)
         fAmp = 0;

      // figure out a random angle
      DWORD dwAngle = (DWORD)(rand()%256) << 24;

      // fill in
      pafFFT[i*2+0] = (float)SineLUT(dwAngle) / (float)0x10000 * fAmp;
      pafFFT[i*2+1] = (float)SineLUT(dwAngle + 0x40000000) / (float)0x10000 * fAmp; // so have cos
   } // i


   // do invert FFT to get back to frequency domain
   FFTCalc (pafFFT - 1, dwWindow, -1);

   // go through and stretch the values out into pafNoise
   for (i = 0; i < dwNoiseNum; i++) {
      fp fIndex = (fp)i / (fp)dwNoiseNum * (fp)dwWindow;
      DWORD dwSample = (DWORD)fIndex;
      DWORD dwSample2 = (dwSample+1)%dwWindow;
      fIndex -= dwSample;

      // interpolate
      pafNoise[i] = (1.0 - fIndex) * pafFFT[dwSample] + fIndex * pafFFT[dwSample2];
   } // i
}

/***************************************************************************
CVoiceSynthesize::SynthesizeNoise - Takes a wave with valid SRFEATURE and
synthesizes the noise area. This adds to the existing amplitudes, so the
caller should have zeroed out the audio first.

inputs
   PCM3DWave         pWave - Wave to use
   PCProgressSocket  pProgress - Progress
returns
   BOOL - TRUE if success. FALSE if can't find srfeature or pitch
*/
BOOL CVoiceSynthesize::SynthesizeNoise (PCM3DWave pWave, PCProgressSocket pProgress)
{
   PSRFEATURE pSR = pWave->m_paSRFeature;
   DWORD dwNumSR = pWave->m_dwSRSamples;
   DWORD dwSRSkip = pWave->m_dwSRSkip;
   if (!pSR || !dwNumSR)
      return FALSE;

   // calculate the wavelengths
   float afWavelength[SRDATAPOINTS];
   GenerateOctaveToWavelength (pWave->m_dwSamplesPerSec, afWavelength);

   // create a window
   CMem memWindow;
   DWORD dwNoiseNum = pWave->m_dwSRSkip*2;
   if (!memWindow.Required (dwNoiseNum * sizeof(float) *2))
      return FALSE;
   float *pafWindow = (float*) memWindow.p;
   float *pafMerge = pafWindow + dwNoiseNum;
   CreateFFTWindow (3, pafWindow, dwNoiseNum, FALSE);

   DWORD i, j, k;
   for (i = 0; i < dwNumSR; i++) {
      if (pProgress && !(i % 10))
         pProgress->Update ((fp)i / (fp)dwNumSR);

      // synthesize
      SynthesizeNoisePacket (pWave, &pWave->m_paSRFeature[i], afWavelength, pafMerge);

      // scale by the window
      for (j = 0; j < dwNoiseNum; j++)
         pafMerge[j] *= pafWindow[j];

      // add in...
      // loop over all the points
      for (j = 0; j < dwNoiseNum; j++) {
         // what sample?
         int iSample = (int)i * (int)dwSRSkip + (int)j - (int)dwNoiseNum/2;
         if (iSample < 0)
            continue;
         if (iSample >= (int)pWave->m_dwSamples)
            break;

         // wriet out the sample
         double fTemp;
         iSample *= (int)pWave->m_dwChannels;
         for (k = 0; k < pWave->m_dwChannels; k++) {
            fTemp = pafMerge[j] + (double)pWave->m_psWave[iSample + (int)k];
            fTemp = min(fTemp, 32767);
            fTemp = max(fTemp, -32768);
            pWave->m_psWave[iSample+(int)k] = (short)fTemp;
         } // k
      } // j
   } // i

   return TRUE;
}
#endif // 0


/*********************************************************************************
CVoiceSynthesize::Constructor and destructor
*/
CVoiceSynthesize::CVoiceSynthesize (void)
{
   // reset all the settings
   m_fHarmonicSpacing = 1;
   m_fWavePitch = 0;
}

CVoiceSynthesize::~CVoiceSynthesize (void)
{
}



/*********************************************************************************
CVoiceSynthesize::CloneTo - Copies all the customization information to another
version of the object
*/
BOOL CVoiceSynthesize::CloneTo (CVoiceSynthesize *pTo)
{
   pTo->m_fHarmonicSpacing = m_fHarmonicSpacing;

   pTo->m_fWavePitch = m_fWavePitch;

   return TRUE;
}

/*********************************************************************************
CVoiceSynthesize::Clone - Standard API
*/
CVoiceSynthesize *CVoiceSynthesize::Clone (void)
{
   PCVoiceSynthesize pv = new CVoiceSynthesize;
   if (!pv)
      return NULL;
   if (!CloneTo(pv)) {
      delete pv;
      return NULL;
   }
   return pv;
}

/***************************************************************************
CVoiceSynthesize::SynthesizeFromWave - Does the synthesis but synthesizes it
from a wave file instead of from the voice box.

inputs
   PCM3DWave         pWave - Wave to use
   PCM3DWave         pSourceWave - Source wave file to synthesize from
   PCProgressSocket  pProgress - Progress
returns
   BOOL - TRUE if success. FALSE if can't find srfeature or pitch, or cant
      load in the wave file.
*/
BOOL CVoiceSynthesize::SynthesizeFromWave (PCM3DWave pWave, PCM3DWave pSourceWave, PCProgressSocket pProgress)
{
   if (!pSourceWave)
      return FALSE;
   if (!pSourceWave->RequireWave (NULL))
      return FALSE;

   DWORD i, j, dwChan;

   // make sure it's the right sampling rate
   PCM3DWave pUse = NULL;
   if ((pSourceWave->m_dwChannels == pWave->m_dwChannels) && (pSourceWave->m_dwSamplesPerSec == pWave->m_dwSamplesPerSec))
      pUse = pSourceWave;
   else {
      // need to downsample
      pUse = pSourceWave->Clone();
      if (!pUse) {
         return FALSE;
      }
      pUse->ConvertSamplesAndChannels (pWave->m_dwSamplesPerSec, pWave->m_dwChannels, NULL);
   }

   // fill the current buffer with this wave looped
   double fPhase, fPitch, fPitchDelta;
   fPhase = 0;
   for (i = 0; i < pWave->m_dwSRSamples; i++) {
      if (m_fWavePitch > CLOSE) {
         // get pitch in hz
         double fPitchStart, fPitchEnd;
         fPitchStart = GetFundamental (pWave, i);
         fPitchEnd = GetFundamental (pWave, min(i+1,pWave->m_dwSRSamples-1));
         fPitchStart = max(fPitchStart, CLOSE);
         fPitchEnd = max(fPitchEnd, CLOSE);

         // conver this to number of samples to skip ahead each time
         fPitchStart = fPitchStart / m_fWavePitch;
         fPitchEnd = fPitchEnd / m_fWavePitch;

         fPitch = fPitchStart;
         fPitchDelta = (fPitchEnd - fPitchStart) / (double) pWave->m_dwSRSkip;
      }
      else {
         fPitch = 1;
         fPitchDelta = 0;
      }

      // fill in
      for (j = 0; j < pWave->m_dwSRSkip; j++) {
         // do modulo
         while (fPhase >= (double)pUse->m_dwSamples)
            fPhase -= (double)pUse->m_dwSamples;

         // over the channels
         DWORD dwSamp = (DWORD)fPhase;
         DWORD dwNext = (dwSamp+1) % pUse->m_dwSamples;
         fp fAlpha = (fp)fPhase - dwSamp;
         for (dwChan = 0; dwChan < pWave->m_dwChannels; dwChan++) {
            fp fValue = (1.0 - fAlpha) * (fp)pUse->m_psWave[dwSamp*pUse->m_dwChannels + dwChan] +
               fAlpha * pUse->m_psWave[dwNext*pUse->m_dwChannels + dwChan];
            DWORD dwTo = (i * pWave->m_dwSRSkip)+j;
            if (dwTo >= pWave->m_dwSamples)
               break;
            pWave->m_psWave[dwTo*pWave->m_dwChannels] =
               (int) fValue;
         } // dwChan

         // increase
         fPhase += fPitch;
         fPitch += fPitchDelta;
      } // j
   } // i

   // release the cache
   if (pUse != pSourceWave)
      delete pUse;

   // fill in the filter information
   CMem memFilter;
   if (!memFilter.Required (sizeof(fp)*SRDATAPOINTS + sizeof(DWORD)*pWave->m_dwSRSamples +
      sizeof(fp)*SRDATAPOINTS*pWave->m_dwSRSamples)) {
         return FALSE;
      }
   fp *pafFreq, *pafVol;
   DWORD *padwSample;
   pafFreq = (fp*) memFilter.p;
   pafVol = pafFreq + SRDATAPOINTS;
   padwSample = (DWORD*) (pafVol + SRDATAPOINTS*pWave->m_dwSRSamples);
   for (i = 0; i < SRDATAPOINTS; i++)
      pafFreq[i] = SRBASEPITCH * pow((fp)2.0, (fp)i / (fp)SRPOINTSPEROCTAVE);
   SRFEATURE SRFeature;

   for (i = 0; i < pWave->m_dwSRSamples; i++) {
      padwSample[i] = i * pWave->m_dwSRSkip;

      // tweak the feature according to settings
      SRFeature = pWave->m_paSRFeature[i];

      for (j = 0; j < SRDATAPOINTS; j++) {
         char cAmp = SRFeature.acVoiceEnergy[j];
         int iAdjust = ((int)j - (int)SRDATAPOINTS/2) * 6 / SRPOINTSPEROCTAVE;
         if ((int)cAmp - iAdjust > -127)
            cAmp -= (char)iAdjust;
         else
            cAmp = -127;

         pafVol[i * SRDATAPOINTS + j] = (fp)DbToAmplitude(cAmp) / 32678.0 * 10.0; // BUGFIX - Multiply by 20 as fuge factor
      }
   }

   // filter the lot
   pWave->Filter (SRDATAPOINTS, pafFreq, pWave->m_dwSRSamples, padwSample,
      pafVol, pProgress, FALSE);

   return TRUE;
}


/***************************************************************************
CVoiceSynthesize::SynthesizeFromSRFEATURE - This resythesizes the wave from its existing SRFEATUREs
and pitch information.

NOTE: This doesnt affect undo or set the dirty flag.

inputs
   int               iTTSQuality - TTS quality to use
   PCM3DWave         pWave - Wave
   PPSOLASTRUCT      paPS - Array of PSOLASTRUCT. If not NULL then will synthesize using PSOLA.
                        If NULL then do normal additive sine-wave synthesis
   DWORD             dwNumPS - Number of entries in paPS
   fp                fFormantShift - Number of octaves to shift formant up/down. Only used for PSOLA
   PCListFixed       plWAVESEGMENTFLOAT - If not using PSOLA, can pass in an array of FFTs for each of the
                     SRFEATUREs and will use that to get phase. If using psola, this is igored, so pass
                     in NULL.
   BOOL              fEnablePCM - If TRUE (default), then allow PCM synthesis. Might
                     turn to FALSE if doing whisper.
   PCProgressSocket  pProgress - Progress
   PCWave            pWaveBase - Wave to use as a basis for playback
   BOOL              fClearSRFEATURE - If TRUE then clera the existing SRFEATURES (since
                     they're technically not valid). If FALSE leave them there
   PCProgressWaveSample pProgressWave - This is an accurate progress bar used
                  so that will be able to play out TTS while it's being generated.
returns
   BOOL - TRUE if success. Will fail if doesnt have SRFeatures or pitch
*/
BOOL CVoiceSynthesize::SynthesizeFromSRFeature (int iTTSQuality, PCM3DWave pWave, PPSOLASTRUCT paPS, DWORD dwNumPS, fp fFormantShift,
                                                PCListFixed plWAVESEGMENTFLOAT,
                                                BOOL fEnablePCM, PCProgressSocket pProgress,
                                                PCM3DWave pWaveBase,
                                                BOOL fClearSRFEATURE, PCProgressWaveSample pProgressWave)
{
   // if non-integer harmincs then can't do psola
   if (m_fHarmonicSpacing != 1.0)
      paPS = NULL;

   // if have a base waveform for special effect than can't use PSOLA
   if (pWaveBase)
      paPS = NULL;

   // potentially synthsize with psola
   BOOL fDoingPSOLA = FALSE;
   if (paPS && dwNumPS)
      fDoingPSOLA = TRUE;

#if 0 // to test out that not distorting when synthesize
   DWORD i, j;
   for (i = 0; i < pWave->m_adwPitchSamples[PITCH_F0]; i++)
      pWave->m_apPitch[PITCH_F0][i].fFreq = (fp) i / (fp)pWave->m_adwPitchSamples[PITCH_F0] * 250 + 50;

   for (i = 0; i < pWave->m_dwSRSamples; i++) {
      PSRFEATURE psrf = pWave->m_paSRFeature + i;

      psrf->fPCMScale = 0;
      psrf->bPCMHarmFadeFull = psrf->bPCMHarmFadeStart = psrf->bPCMHarmNyquist = 0;

      for (j = 0; j < SRDATAPOINTS; j++)
         psrf->acVoiceEnergy[j] = psrf->acNoiseEnergy[j] = -127;

      // sine sweep
      fp fFreq = pWave->PitchAtSample (i * pWave->m_dwSRSkip, 0);
      fFreq = log(fFreq / SRBASEPITCH) / log(2.0) * SRPOINTSPEROCTAVE;
      int iFreq = (int)fFreq;
      int iAmp = -13;
      iAmp -= (SRDATAPOINTS/2 - iFreq) * 6 / SRPOINTSPEROCTAVE;   // 6 db per octave
      
      int iLoop;
      for (iLoop = iFreq - 2; iLoop <= iFreq+2; iLoop++)
         if ((iLoop >= 0) && (iLoop < SRDATAPOINTS))
            psrf->acVoiceEnergy[iLoop] = (char)iAmp;
   } // i
#endif

   // zero out the wave
   memset (pWave->m_psWave, 0, pWave->m_dwSamples * pWave->m_dwChannels * sizeof(short));

   BOOL fWave, fTryWave;
   fTryWave = fWave = (pWaveBase ? TRUE : FALSE);
   if (fTryWave && pProgress)
      pProgress->Push (0, .5);

   // call synthesize from wave since will fail if no wave specificed
   fWave = SynthesizeFromWave (pWave, pWaveBase, pProgress);

   if (fTryWave && pProgress) {
      pProgress->Pop();
      pProgress->Push (.5, 1);
   }

   if (!SynthesizeVoiced (pWave, fWave, plWAVESEGMENTFLOAT, fEnablePCM, pProgress, fDoingPSOLA ? NULL : pProgressWave))
      return FALSE;

   if (fTryWave && pProgress)
      pProgress->Pop();

   if (fDoingPSOLA) {
      if (!PSOLASTRUCTNonContiguous (paPS, dwNumPS, fFormantShift, pWave, iTTSQuality))
         return FALSE;

      // update progress
      if (pProgress)
         pProgress->Update (1.0);
      if (pProgressWave)
         pProgressWave->Update (pWave, pWave->m_dwSamples, pWave->m_dwSamples);
   }
   // Synthesize unvoiced
   // BUGFIX  - Disable since can create the noise through addtiive sinewave synthesis
   //if (!SynthesizeNoise (pWave, pProgress)) {
   //   return FALSE;
   //}

   // clear the pitch and SR feature info since it's no longer valid
   if (fClearSRFEATURE)
      pWave->ReleaseCalc();

   return TRUE;

}


/***************************************************************************
PitchAtSRFEATURE - Get the pitch at a given SRFEATURE

inputs
   fp                fSRFEATURE - SRFEATURE index. Can be negative or excede length of data
   BOOL              fFromWave - If TRUE then get it from pWave, else from PTTSFEATURECOMPEXTRA
   PCM3DWave         pWave - Where to get pitch from (if fFromWave is true)
   PTTSFEATURECOMPEXTRA pTFCE - Where to get pitch from (if fFromWave is FALSE)
   DWORD             dwNumTFCE - Number of elements in TFCE
returns
   fp - Pitch in Hz
*/
fp PitchAtSRFEATURE (double fSRFEATURE, BOOL fFromWave, PCM3DWave pWave,
                                       PTTSFEATURECOMPEXTRA pTFCE, DWORD dwNumTFCE)
{
   // make sure not less than 0
   fSRFEATURE = max(fSRFEATURE, 0.0);

   // from the wave
   if (fFromWave) {
      if (pWave->m_adwPitchSamples[PITCH_F0])
         return pWave->PitchAtSample (PITCH_F0, fSRFEATURE * (double)pWave->m_adwPitchSkip[PITCH_F0], 0); // don't bother testing limits because does so
      else
         return 100; // error
   }

   // else, from TFCE
   if (!dwNumTFCE)
      return 100; // erro
   DWORD dwStart = (DWORD)fSRFEATURE;
   fSRFEATURE -= (double)dwStart;
   dwStart = min(dwStart, dwNumTFCE-1);
   DWORD dwEnd = min(dwStart+1, dwNumTFCE-1);

   return (1.0 - fSRFEATURE) * pTFCE[dwStart].fPitchF0All + fSRFEATURE * pTFCE[dwEnd].fPitchF0All;
}



/***************************************************************************
PSOLAGeneratePitchPoints - Generates pitch points.

inputs
   BOOL              fSrc - If TRUE generate source pitch points (using pWave).
                           If FALSE generate destination pitch points (using pFTCE
                           in paPS)
   PPSOLASTRUCT      paPS - Array of PSOLASTRUCT. All in the same data stream
                              and contiguous
   DWORD             dwNum - Number of entries in paPS
   PCM3DWave         pWave - Ulimate destination wave. Used to get destination pitch.
   fp                fPhaseAngle - Offset of the first bit of audio (from the source).
                              0 = none, -1 = full wavelength to negative. Use this
                              to create wave with different phase angled for better blending
   BOOL              fSilenceToLeft - TRUE if there's supposed to be silence to the left (before wave)
   BOOL              fSilenceToRight - TRUE if there's supposed to be silence to the right (after dwSamples)
   PCListFixed       plPSOLAPOINT - Initialized and filled with sorted list of PSOLAPOINT
returns
   BOOL - TRUE if success
*/
BOOL PSOLAGeneratePitchPoints (BOOL fSrc, PPSOLASTRUCT paPS, DWORD dwNum,
                                                 PCM3DWave pWave, fp fPhaseAngle,
                                                 BOOL fSilenceToLeft, BOOL fSilenceToRight, PCListFixed plPSOLAPOINT)
{
   plPSOLAPOINT->Init (sizeof(PSOLAPOINT));

   double fSampleCur = 0;
   DWORD dwSamplesPerSec = fSrc ? paPS[0].dwSamplesPerSec : pWave->m_dwSamplesPerSec;
   DWORD dwSRSkip = fSrc ? paPS[0].dwSRSkip : pWave->m_dwSRSkip;
   int iSRFEATUREStart = fSrc ? (int)paPS[0].dwFeatureStartSrc : paPS[0].iFeatureStartDest;
   int iSRFEATUREEnd = fSrc ? (int)paPS[dwNum-1].dwFeatureEndSrc : paPS[dwNum-1].iFeatureEndDest;

   // subtract initial phaseangle
   fSampleCur = iSRFEATUREStart * (int)dwSRSkip;
   double fPitch;
   if (fPhaseAngle) {
      fPitch = PitchAtSRFEATURE (iSRFEATUREStart, !fSrc, pWave, paPS[0].paTFCE, paPS[0].dwNumSRFEATURE);
      fPitch = max(fPitch, CLOSE);
      fp fWavelength = (fp)dwSamplesPerSec / fPitch;
      fSampleCur += fWavelength * fPhaseAngle;
   }

   // figure out max sample
   double fSampleMax = iSRFEATUREEnd * (int)dwSRSkip;

   // figure out wavelength at left/right
   double fWavelengthLeft = 0, fWavelengthRight = 0;
   fPitch = PitchAtSRFEATURE (iSRFEATUREStart, !fSrc, pWave, paPS[0].paTFCE, paPS[0].dwNumSRFEATURE);
   fWavelengthLeft = (fp)dwSamplesPerSec / max (fPitch, CLOSE);
   fPitch = PitchAtSRFEATURE (iSRFEATUREEnd, !fSrc, pWave, paPS[0].paTFCE, paPS[0].dwNumSRFEATURE);
   fWavelengthRight = (fp)dwSamplesPerSec / max (fPitch, CLOSE);

   PSOLAPOINT PP;
   memset (&PP, 0, sizeof(PP));

   // loop until
   PPSOLAPOINT pPP;
   DWORD i;
   while (TRUE) {
      // see if should stop
      if (plPSOLAPOINT->Num()) {
         pPP = (PPSOLAPOINT) plPSOLAPOINT->Get(plPSOLAPOINT->Num()-1);
         if (pPP->fSample > fSampleMax)
            break;
      }

      // BUGFIX - If no silence to right, then fSampleMax should be adhered to
      if (!fSilenceToRight && (fSampleCur >= fSampleMax - fWavelengthRight * 2.0))  // BUGFIX - Include right wavelength so can double in pitch without clicks
         break;

      // BUGFIX - If no silence to the left, and this is less than 0, then
      // just skip
      if (!fSilenceToLeft && (fSampleCur < 0.0 + fWavelengthLeft * 2.0))   // BUGFIX - Include left wavelength so can double in pitch without clicks
         goto nextpitchpoint;

      // write the current point in
      PP.fSample = fSampleCur;
      PP.fSRFEATURE = fSampleCur / (double)dwSRSkip;
      // find out the PSOLASTRUCT it's in
      for (i = 0; i < dwNum; i++) {
         int iFeatureStart = fSrc ? (int) paPS[i].dwFeatureStartSrc : paPS[i].iFeatureStartDest;
         int iFeatureEnd = fSrc ? (int) paPS[i].dwFeatureEndSrc : paPS[i].iFeatureEndDest;
         int iSampleStart = iFeatureStart * (int)dwSRSkip;
         int iSampleEnd = iFeatureEnd * (int)dwSRSkip;

         // if after this
         if (fSampleCur >= (double)iSampleEnd) {
            // if there's still more to go then go to next i
            if (i+1 < dwNum)
               continue;

            // else, fall through
         }

         // NOTE: If fSampleCur < iSampleStart, which should only happen for initial fPhaseAngle,
         // then the following code works

         PP.fPSOLASTRUCT = (double)i + (fSampleCur - (double)iSampleStart) / (double)(iSampleEnd - iSampleStart);

         PP.fAppended = paPS[i].fAppended;

         plPSOLAPOINT->Add (&PP);
         break;
      } // i

nextpitchpoint:
      // will have added PSOLAPOINT by now

      // increase but four quarter-pitches, to try to get more accuate
#define WAVELENGTHFRAGMENTS       8    // number of divisions of wavelength, for more accurate audio
      for (i = 0; i < WAVELENGTHFRAGMENTS; i++) {
         fPitch = PitchAtSRFEATURE (fSampleCur / (double)dwSRSkip, !fSrc, pWave, paPS[0].paTFCE, paPS[0].dwNumSRFEATURE);
         fPitch = max(fPitch, CLOSE);  // so no divide by 0
         fSampleCur += (double)dwSamplesPerSec / fPitch / (fp)WAVELENGTHFRAGMENTS;
      }
   } // while TRUE

   if (plPSOLAPOINT->Num() < 2)
      return FALSE;

   return TRUE;
}


/***************************************************************************
PSOLAAverageSample - Weighted average of a point

inputs
   short       *pasWave - Wave
   DWORD       dwSamples - Number of samples in the wave
   int         iCenter - Center point
   DWORD       dwHalfWindow - Width of half a window
returns
   fp - Weighted average around iCenter
*/
fp PSOLAAverageSample (short *pasWave, DWORD dwSamples, int iCenter, DWORD dwHalfWindow)
{
   fp fSum = 0;
   fp fWeightSum = 0;
   fp fWeight;
   int iOffset, iCur;
   for (iOffset = -(int)dwHalfWindow; iOffset <= (int)dwHalfWindow; iOffset++) {
      iCur = iOffset + iCenter;
      if ((iCur < 0) || (iCur >= (int)dwSamples))
         continue;

      fWeight = (fp)(dwHalfWindow + 1 - abs(iOffset));
      fSum += (fp)pasWave[iCur] * fWeight;
      fWeightSum += fWeight;
   } // iOffset

   if (fWeightSum)
      return fSum / fWeightSum;
   else
      return 0.0; // cant calc
}

/***************************************************************************
PSOLAGeneratePitchPointsWithEpochs - Generates pitch points but changing
the phase angle to get the best epochs.

inputs
   BOOL              fSrc - If TRUE generate source pitch points (using pWave).
                           If FALSE generate destination pitch points (using pFTCE
                           in paPS)
   PPSOLASTRUCT      paPS - Array of PSOLASTRUCT. All in the same data stream
                              and contiguous

                              Uses fAppended to determine which blocks are important.
                              The ones with !fAppended are tested.

   DWORD             dwNum - Number of entries in paPS
   PCM3DWave         pWave - Ulimate destination wave. Used to get destination pitch.
   BOOL              fSilenceToLeft - TRUE if there's supposed to be silence to the left (before wave)
   BOOL              fSilenceToRight - TRUE if there's supposed to be silence to the right (after dwSamples)
   int               iTTSQuality - How accurate to calculate epochs
   PCListFixed       plPSOLAPOINT - Initialized and filled with sorted list of PSOLAPOINT
returns
   BOOL - TRUE if success
*/
BOOL PSOLAGeneratePitchPointsWithEpochs (BOOL fSrc, PPSOLASTRUCT paPS, DWORD dwNum,
                                                 PCM3DWave pWave,
                                                 BOOL fSilenceToLeft, BOOL fSilenceToRight,
                                                 int iTTSQuality, PCListFixed plPSOLAPOINT)
{
   // how many passes - based on TTS quality
   DWORD dwPasses = 4 << max(iTTSQuality, 0);

   DWORD dwPass, i;
   fp fBestPhase = -1000;
   fp fBestScore = 0;
   for (dwPass = 0; dwPass < dwPasses; dwPass++) {
      fp fPhase = -(fp)dwPass / (fp)dwPasses;

      if (!PSOLAGeneratePitchPoints (fSrc, paPS, dwNum, pWave, fPhase, fSilenceToLeft, fSilenceToRight, plPSOLAPOINT))
         return FALSE;

      // determine the average absoluate ampltiude
      PPSOLAPOINT ppp = (PPSOLAPOINT)plPSOLAPOINT->Get(0);
      fp fSum = 0;
      DWORD dwSumCount = 0;
      for (i = 0; i < plPSOLAPOINT->Num(); i++, ppp++) {
         if (ppp->fAppended)
            continue;   // appended, so skip since not relevent

         // find the approximate wavelength
         fp fWavelength = 0;
         DWORD dwWavelengthCount = 0;
         if (i) {
            fWavelength += ppp->fSample - ppp[-1].fSample;
            dwWavelengthCount++;
         }
         if ((i+1) < plPSOLAPOINT->Num()) {
            fWavelength += ppp[1].fSample - ppp->fSample;
            dwWavelengthCount++;
         }
         if (dwWavelengthCount)
            fWavelength /= (fp)dwWavelengthCount;
         fWavelength = max(fWavelength, 0.0);
         fWavelength /= (fp)dwPasses;
         DWORD dwWavelength = (DWORD)(fWavelength+0.5);

         
         fSum += fabs(PSOLAAverageSample (paPS[0].pasWave, paPS[0].dwNumSRFEATURE * paPS[0].dwSRSkip,
            (int)(ppp->fSample + 0.5), dwWavelength));
         dwSumCount++;
      } // i

      // average
      if (dwSumCount)
         fSum /= (fp)dwSumCount;

      // keep best?
      if ((fBestPhase <= -2.0) || (fSum > fBestScore)) {
         fBestPhase = fPhase;
         fBestScore = fSum;
      }

   } // dwPass

   // if didn't find a best then fail, shouldnt happen
   if (fBestPhase <= -2.0)
      return FALSE;

   // else, regenerate and use that
   return PSOLAGeneratePitchPoints (fSrc, paPS, dwNum, pWave, fBestPhase, fSilenceToLeft, fSilenceToRight, plPSOLAPOINT);
}


/***************************************************************************
PSOLAPOINTClosest - Finds the closes PSOLAPOINT

inputs
   double            fPSOLASTRUCT - Integer and fractional PSOLAPOINT location to look for
   PPSOLAPOINT       paPSOLAPOINT - Array of PSOLAPOINT
   DWORD             dwNum - Number of elements in paPSOLAPOINT
returns
   DWORD - Index in paPSOLAPOINT for the the most accurate. -1 if error
*/
DWORD PSOLAPOINTClosest (double fPSOLASTRUCT, PPSOLAPOINT paPSOLAPOINT, DWORD dwNum)
{
   if (!dwNum)
      return (DWORD)-1;

   // find closest
   double fClosest = fabs(paPSOLAPOINT[0].fPSOLASTRUCT - fPSOLASTRUCT);
   DWORD dwClosest = 0;

   DWORD i;
   double fDist;
   for (i = 1; i < dwNum; i++) {
      fDist = fabs (paPSOLAPOINT[i].fPSOLASTRUCT - fPSOLASTRUCT);
      if (fDist < fClosest) {
         fClosest = fDist;
         dwClosest = i;
      }
      else
         break;   // since going futher away now
   } // i

   return dwClosest;
}

/***********************************************************************************
HanningWindowSkew - Does a hanning window, but skews it

inputs
   fp          fAlpha - From 0.0 to 1.0 to cover an entire wavlength.
returns
   fp - Hanning window, from 0.0 to 1.0. fAlpha = 0 return 0, fAlpha = 0.5 returns 1, fAlpha = 1 returns 0
*/
__inline fp HanningWindowSkew (fp fAlpha)
{
#ifdef UNSKEWEDHANNING
   return HanningWindow (fAlpha);
#else
   BOOL fInvert;
   if (fAlpha >= 0.5) {
      fInvert = FALSE;
      fAlpha = (fAlpha - 0.5) * 2.0;
   }
   else {
      fInvert = TRUE;
      fAlpha *= 2.0;
   }

   fAlpha = pow(fAlpha, 2.0); // so stretch out to the right

   fAlpha = 0.5 + fAlpha / 2.0;  // so back to hanning window

   fAlpha = HanningWindow (fAlpha);
   return (fInvert ? (1.0 - fAlpha) : fAlpha);
#endif
}

/***************************************************************************
PSOLASTRUCTContiguous - Synthesizes a contiguous
list of PSOLASTRUCT into memory.

inputs
   PPSOLASTRUCT      paPS - Array of PSOLASTRUCT. All in the same data stream
                              and contiguous
   DWORD             dwNum - Number of entries in paPS
   fp                fFormantShift - Number of octaves to shift formant up/down
   PCM3DWave         pWave - Ulimate destination wave. Used to get destination pitch.
   fp                fPhaseAngle - Offset of the first bit of audio (from the source).
   BOOL              fSilenceToLeft - TRUE if there's supposed to be silence to the left (before wave)
   BOOL              fSilenceToRight - TRUE if there's supposed to be silence to the right (after dwSamples)
   PCListFixed       plPSOLAPOINTSrc - If the source points were already calculated then pass in a pointer
                        to the list. Else pass in NULL.
                        If this is an empty list, it will be filled in, and can be re-used next time.
   int               iTTSQuality - TTS quality
   PCMem             pMem - Filled with the PCM, as per paPS. m_dwCurPosn is
                              filled with # samples * sizeof(short). If this is NULL then no memory is filled in,
                              and only the error is calculated
   DWORD             *pdwError - If not NULL, filled with the error... trying to minimize the number of
                              times that psola repeats (or skips) individual wavelengths
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL PSOLASTRUCTContiguous (PPSOLASTRUCT paPS, DWORD dwNum, fp fFormantShift,
                                             PCM3DWave pWave, fp fPhaseAngle, BOOL fSilenceToLeft, BOOL fSilenceToRight,
                                             PCListFixed plPSOLAPOINTSrc, int iTTSQuality, PCMem pMem,
                                             DWORD *pdwError)
{
   if (pdwError)
      *pdwError = 0;

   // how much memory need
   int iFeatureStartDestTotal = paPS[0].iFeatureStartDest;
   int iFeatureEndDestTotal = paPS[dwNum-1].iFeatureEndDest;
   int iFeatureDestTotal = (iFeatureEndDestTotal - iFeatureStartDestTotal);
   if (iFeatureDestTotal <= 0)
      return FALSE;
   short *pasWave = NULL;
   float *pafWave = NULL;
   CMem memFloatWave;
   if (pMem) {
      DWORD dwNeed = (DWORD)iFeatureDestTotal * pWave->m_dwSRSkip * sizeof(short);
      if (!pMem->Required (dwNeed))
         return FALSE;
      pMem->m_dwCurPosn = dwNeed;

      pasWave = (short*)pMem->p;
      memset (pasWave, 0, dwNeed);

      dwNeed = (DWORD)iFeatureDestTotal * pWave->m_dwSRSkip * sizeof(float);
      if (!memFloatWave.Required (dwNeed))
         return FALSE;
      memFloatWave.m_dwCurPosn = dwNeed;

      pafWave = (float*)memFloatWave.p;
      memset (pafWave, 0, dwNeed);
   }

   // wavelength scale
   double fWavelengthScale = 1;
   if (paPS[0].dwSamplesPerSec != pWave->m_dwSamplesPerSec)
      fWavelengthScale = (double)paPS[0].dwSamplesPerSec / (double)pWave->m_dwSamplesPerSec;

   DWORD i;
#ifdef FEATUREHACK_COPYWAVEEXACTLY
   DWORD j;
   // make sure the features all match exact length
   for (i = 0; i < dwNum; i++)
      _ASSERTE ((int)(paPS[i].dwFeatureEndSrc - paPS[i].dwFeatureStartSrc) == (paPS[i].iFeatureEndDest - paPS[i].iFeatureStartDest) );

   for (i = 0; i < (DWORD)iFeatureDestTotal * pWave->m_dwSRSkip; i++) {
      double fLocOrig = (double)i * fWavelengthScale;

      // NOTE: Not blending
      for (j = 0; j < dwNum; j++) {
         double fSamples = (paPS[j].dwFeatureEndSrc - paPS[j].dwFeatureStartSrc) * paPS[j].dwSRSkip;
         if (fLocOrig >= fSamples) {
            // beyond edge of this
            fLocOrig -= fSamples;
            continue;
         }

         // else, found
         break;
      } // j
      if (pasWave) {
         if (j < dwNum)
            pasWave[i] = paPS[j].pasWave[paPS[j].dwFeatureStartSrc * paPS[j].dwSRSkip + (DWORD)fLocOrig];
         else
            pasWave[i] = 0;   // beyond edge
      }
   } // i
   return TRUE;
#endif

   // generate pitch points
   CListFixed lPSOLAPOINTSrc, lPSOLAPOINTDest;
   if (!plPSOLAPOINTSrc) {
      plPSOLAPOINTSrc = &lPSOLAPOINTSrc;
      if (!PSOLAGeneratePitchPointsWithEpochs (TRUE, paPS, dwNum, pWave, fSilenceToLeft, fSilenceToRight, iTTSQuality, plPSOLAPOINTSrc))
         return FALSE;
   }
   else if (!plPSOLAPOINTSrc->Num()) {
      if (!PSOLAGeneratePitchPointsWithEpochs (TRUE, paPS, dwNum, pWave, fSilenceToLeft, fSilenceToRight, iTTSQuality, plPSOLAPOINTSrc))
         return FALSE;
   }
   PPSOLAPOINT pPPSrc = (PPSOLAPOINT)plPSOLAPOINTSrc->Get(0);
   DWORD dwSrcNum = plPSOLAPOINTSrc->Num();
   if (!PSOLAGeneratePitchPoints (FALSE, paPS, dwNum, pWave, fPhaseAngle, TRUE, TRUE, &lPSOLAPOINTDest))
      return FALSE;
   PPSOLAPOINT pPPDest = (PPSOLAPOINT)lPSOLAPOINTDest.Get(0);
   DWORD dwDestNum = lPSOLAPOINTDest.Num();

   // fill in between the destination PSOLAPOINTs
   DWORD dwCloseLast = (DWORD)-1;
   fp fWavelengthScaleOrig = fWavelengthScale;
   if (fFormantShift)
      fWavelengthScale *= pow((double)2.0, (double)fFormantShift);

   BOOL fFormantShifting = (fabs(fFormantShift) > CLOSE);

   // wipe out the closest count. Only need to do for the source
   for (i = 0; i < dwSrcNum; i++)
      pPPSrc[i].dwTimesUsed = 0;

   int iSampleStartDest = iFeatureStartDestTotal * (int)pWave->m_dwSRSkip;
   int iSampleEndDest = iSampleStartDest + iFeatureDestTotal * (int)pWave->m_dwSRSkip;
   int iSamplesInSrc = paPS[0].dwNumSRFEATURE * (int)paPS[0].dwSRSkip;
#ifdef OLDPSOLA
   for (i = 0; i+1 < dwDestNum; i++, pPPDest++) {
      // find closest points in the src
      DWORD dwCloseA = (dwCloseLast == (DWORD)-1) ? PSOLAPOINTClosest (pPPDest[0].fPSOLASTRUCT, pPPSrc, dwSrcNum) : dwCloseLast;
      //if (dwCloseA == dwSrcNum-1) {
      //   // can't have A closest to the last one
      //   if (!dwSrcNum)
      //      continue;   // shouldnt happen
      //   dwCloseA--;
      //}
      DWORD dwCloseB = PSOLAPOINTClosest (pPPDest[1].fPSOLASTRUCT, pPPSrc, dwSrcNum);
      //if (!dwCloseB)
      //   dwCloseB++; // can't use first src point for B
      dwCloseLast = dwCloseB;
      if ((dwCloseA == (DWORD)-1) || (dwCloseB == (DWORD)-1))
         continue;   // shouldnt happen


      // remember how many times used
      if (dwCloseA < dwSrcNum)
         pPPSrc[dwCloseA].dwTimesUsed++;
      if (dwCloseB < dwSrcNum)
         pPPSrc[dwCloseB].dwTimesUsed++;

      // if don't want any audio then don't bother going further
      if (!pafWave)
         continue;

      int iSampleCur = ceil(pPPDest[0].fSample);
      double fWavelength = pPPDest[1].fSample - pPPDest[0].fSample;

      // calculate how much to do basic PCM shift
      DWORD dwRight;
      fp afWavelengthScale[2];
      afWavelengthScale[0] = afWavelengthScale[1] = 1.0;

      if (!fFormantShifting) {
         // start at left at look right
         fp fWavelengthSrc;
         for (dwRight = 0; dwRight < 2; dwRight++) {
            if (dwRight) {
               // on right point looking left
               if (dwCloseB)
                  fWavelengthSrc = pPPSrc[dwCloseB].fSample - pPPSrc[dwCloseB-1].fSample;
               else if (dwCloseB+1 < dwSrcNum)
                  fWavelengthSrc = pPPSrc[dwCloseB+1].fSample - pPPSrc[dwCloseB].fSample;
               else
                  fWavelengthSrc = 1;  // error. shouldnt happen
            }
            else {
               // on left point looking right
               if (dwCloseA+1 < dwSrcNum)
                  fWavelengthSrc = pPPSrc[dwCloseA+1].fSample - pPPSrc[dwCloseA].fSample;
               else if (dwCloseA)
                  fWavelengthSrc = pPPSrc[dwCloseA].fSample - pPPSrc[dwCloseA-1].fSample;
               else
                  fWavelengthSrc = 1;  // error
            }

            fWavelengthSrc /= (fWavelength * fWavelengthScale); // since multiply sample dest by this
            fWavelengthSrc = log (fWavelengthSrc) / log(2.0);  // so know difference in octaves

            // BUGFIX - Use minimum instead of the clean slope
            // afWavelengthScale[dwRight] = fabs(fWavelengthSrc);
            // afWavelengthScale[dwRight] = PSOLASTRECTCHAMOUNT / (PSOLASTRECTCHAMOUNT + afWavelengthScale[dwRight]);
            // afWavelengthScale[dwRight] *= fWavelengthSrc;   // so back in octave
            afWavelengthScale[dwRight] = fWavelengthSrc;
            afWavelengthScale[dwRight] = min(afWavelengthScale[dwRight], PSOLASTRECTCHAMOUNT);
            afWavelengthScale[dwRight] = max(afWavelengthScale[dwRight], -PSOLASTRECTCHAMOUNT);

            afWavelengthScale[dwRight] = pow(2.0, afWavelengthScale[dwRight]);   // so in scaling factor
         } // dwRight
      } // if not formant shifting

      for (; (double) iSampleCur <= pPPDest[1].fSample; iSampleCur++) { // think can use <, but being safe
         // make sure in range
         if ((iSampleCur < iSampleStartDest) || (iSampleCur >= iSampleEndDest))
            continue;

         double fAlpha = ((double)iSampleCur - pPPDest[0].fSample) / fWavelength;

         double fSampleValue = 0;
         for (dwRight = 0; dwRight < 2; dwRight++) {  // dwRight==1 => on right side working left
            DWORD dwClose = dwRight ? dwCloseB : dwCloseA;
            double fSampleSrc = (dwRight ? ((fAlpha - 1.0) * fWavelength) : (fAlpha * fWavelength));
            fSampleSrc *= fWavelengthScale;
            if (!fFormantShifting)
               fSampleSrc *= afWavelengthScale[dwRight];
            fSampleSrc += pPPSrc[dwClose].fSample;

            int iSampleSrc = floor(fSampleSrc);
            fSampleSrc -= (double)iSampleSrc;

            fp fSrcLeft;
            if ((iSampleSrc >= 0) && (iSampleSrc < iSamplesInSrc))
               fSrcLeft = paPS[0].pasWave[iSampleSrc];
            else
               fSrcLeft = 0;
            iSampleSrc++;
            fp fSrcRight;
            if ((iSampleSrc >= 0) && (iSampleSrc < iSamplesInSrc))
               fSrcRight = paPS[0].pasWave[iSampleSrc];
            else
               fSrcRight = 0;

            double fSampleSrcValue = (1.0 - fSampleSrc) * (double)fSrcLeft + fSampleSrc * (double)fSrcRight;

            double fHanning = HanningWindowSkew ( dwRight ? (fAlpha / 2.0) : (fAlpha / 2.0 + 0.5) );

            fSampleValue += fSampleSrcValue * fHanning;

         } // dwRight

         //fSampleValue = max(fSampleValue, -32768);
         //fSampleValue = min(fSampleValue, 32867);
         pafWave[iSampleCur - iSampleStartDest] = (short)fSampleValue;

      } // i
   } // destination
#else // !OLDPSOLA
   // This uses the official psola algorithm, not my own modification. See if there's any difference

   for (i = 0; i < dwDestNum; i++, pPPDest++) {
      // find closest points in the src
      DWORD dwCloseA = PSOLAPOINTClosest (pPPDest[0].fPSOLASTRUCT, pPPSrc, dwSrcNum);
      if (dwCloseA == (DWORD)-1)
         continue;   // shouldnt happen


      // remember how many times used
      if (dwCloseA < dwSrcNum)
         pPPSrc[dwCloseA].dwTimesUsed++;

      // if don't want any audio then don't bother going further
      if (!pafWave || !dwSrcNum)
         continue;

      // wavelength from the source
      DWORD dwCloseANext = min(dwCloseA+1, dwSrcNum-1);
      DWORD dwCloseAPrev = (dwCloseA ? (dwCloseA-1) : 0);
      fp fWavelengthSrc;
      if (dwCloseANext > dwCloseAPrev) {
         fWavelengthSrc = pPPSrc[dwCloseANext].fSample - pPPSrc[dwCloseAPrev].fSample;
         fWavelengthSrc /= (fp)(dwCloseANext - dwCloseAPrev);
      }
      else
         fWavelengthSrc = pWave ? (pWave->m_dwSamplesPerSec / 100) : 100;   // arbitrary number

      fp fWavelengthDst = fWavelengthSrc / fWavelengthScaleOrig;
      fWavelengthSrc *= fWavelengthScale / fWavelengthScaleOrig;
         // BUGFIX - Put in fWavelengthScaleOrig so that when downsample sounds ok
      // old version
      // fp fWavelengthDst = fWavelengthSrc / fWavelengthScale;

      int iSampleDestCur = ceil(pPPDest[0].fSample - fWavelengthDst);

      for (; (double) iSampleDestCur < pPPDest[0].fSample + fWavelengthDst; iSampleDestCur++) { // think can use <, but being safe
         // make sure in range
         if ((iSampleDestCur < iSampleStartDest) || (iSampleDestCur >= iSampleEndDest))
            continue;

         double fAlpha = ((double)iSampleDestCur - pPPDest[0].fSample) / fWavelengthDst;
         fAlpha = max(fAlpha, -1.0);
         fAlpha = min(fAlpha, 1.0);
         double fHanning;
         fHanning = HanningWindowSkew(fAlpha / 2.0 + 0.5);

         double fSampleSrc = fAlpha * fWavelengthSrc + pPPSrc[dwCloseA].fSample;

         int iSampleSrc = floor(fSampleSrc);
         fSampleSrc -= (double)iSampleSrc;

         fp fSrcLeft;
         if ((iSampleSrc >= 0) && (iSampleSrc < iSamplesInSrc))
            fSrcLeft = paPS[0].pasWave[iSampleSrc];
         else
            fSrcLeft = 0;
         iSampleSrc++;
         fp fSrcRight;
         if ((iSampleSrc >= 0) && (iSampleSrc < iSamplesInSrc))
            fSrcRight = paPS[0].pasWave[iSampleSrc];
         else
            fSrcRight = 0;

         double fSampleSrcValue = (1.0 - fSampleSrc) * (double)fSrcLeft + fSampleSrc * (double)fSrcRight;

         fSampleSrcValue *= fHanning;

         // add to existing wave
         pafWave[iSampleDestCur - iSampleStartDest] += fSampleSrcValue;
      } // i
   } // destination
#endif // !OLDPSOLA

   // copy pasWave
   fp f;
   if (pasWave) for (i = 0; i < (DWORD) iFeatureDestTotal * pWave->m_dwSRSkip; i++) {
      f = pafWave[i];
      f += 0.5; // to round
      f = max(f, -32767);
      f = min(f, 32767);
      pasWave[i] = (short) f;
   } // i

   // figure out the error
   DWORD dwError = 0;
   for (i = 0; i < dwSrcNum; i++)
      if (!pPPSrc[i].fAppended)
         dwError += (DWORD)( ((int)pPPSrc[i].dwTimesUsed - 2) * ((int)pPPSrc[i].dwTimesUsed - 2) );
   if (pdwError)
      *pdwError = dwError;

   return TRUE;
}


/***************************************************************************
CVoiceSynthesize::PSOLASTRUCTContiguousExtra - Synthesizes a contiguous
list of PSOLASTRUCT into memory. This also adds EXTRA data to the left
and right to enable a smooth blend.

inputs
   PPSOLASTRUCT      paPS - Array of PSOLASTRUCT. All in the same data stream
                              and contiguous
   DWORD             dwNum - Number of entries in paPS
   fp                fFormantShift - Number of octaves to shift formant up/down
   PCM3DWave         pWave - Ulimate destination wave. Used to get destination pitch.
   fp                fPhaseAngle - Offset of the first bit of audio (from the source).
                              0 = none, -1 = full wavelength to negative. Use this
                              to create wave with different phase angled for better blending
   PCListFixed       plPSOLAPOINTSrc - If the source points were already calculated then pass in a pointer
                        to the list. Else pass in NULL.
                        If this is an empty list, it will be filled in, and can be re-used next time.
   int               iTTSQuality - Quality to synthesize as
   PCMem             pMem - Filled with the PCM, as per paPS. m_dwCurPosn is
                              filled with # samples * sizeof(short). If NULL, then only padwError will be calculated
   DWORD             *pdwSampleStart - Filled with the sample number (indexed into pMam's shorts)
                              where the requested data really starts. Everything before this
                              is padded to maximize the blend
   DWORD             *pdwSampleEnd - Filled with the sample number where the original data really
                              ends. Everything after is padding.
   BOOL              *pfSilenceToLeft - Initially fill with TRUE if there's supposed to be silence to the left (before wave).
                              May be modified in place if prepend audio.
   BOOL              *pfSilenceToRight - Initially fill with TRUE if there's supposed to be silence to the right (after dwSamples)
                              May be modified in place if prepend audio.
   DWORD             *padwError - If not NULL, this is filled in with the PSOLA error... how much PSOLA ends up distorting
                              the original signal.
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CVoiceSynthesize::PSOLASTRUCTContiguousExtra (PPSOLASTRUCT paPS, DWORD dwNum, fp fFormantShift,
                                             PCM3DWave pWave, fp fPhaseAngle,
                                             PCListFixed plPSOLAPOINTSrc, int iTTSQuality, PCMem pMem,
                                             DWORD *pdwSampleStart, DWORD *pdwSampleEnd,
                                             BOOL *pfSilenceToLeft, BOOL *pfSilenceToRight,
                                             DWORD *padwError)
{
   if (padwError)
      *padwError = 0;

#ifdef FEATUREHACK_COPYWAVEEXACTLY
   fPhaseAngle = 0;
#endif

   // fill in, just in case
   *pdwSampleStart = *pdwSampleEnd = 0;

   // figure out the ideal padding, in SRFEATURES
   DWORD dwIdealPad = paPS[0].dwSamplesPerSec / paPS[0].dwSRSkip / 10;   // one tenth of a second ideal
   dwIdealPad = max(dwIdealPad, 1);

   // real padding
   DWORD dwPadLeft = min (paPS[0].dwFeatureStartSrc, dwIdealPad);
   DWORD dwPadRight = min (paPS[dwNum-1].dwNumSRFEATURE - paPS[dwNum-1].dwFeatureEndSrc, dwIdealPad);

   // create a list with padding
   CListFixed lPSOLASTRUCT;
   lPSOLASTRUCT.Init (sizeof(PSOLASTRUCT), paPS, dwNum);

   // fill in appended
   PPSOLASTRUCT paPSThis;
   DWORD i;
   paPSThis = (PPSOLASTRUCT) lPSOLASTRUCT.Get(0);
   for (i = 0; i < lPSOLASTRUCT.Num(); i++)
      paPSThis[i].fAppended = FALSE;

   PSOLASTRUCT PS;
   if (dwPadLeft) {
      PS = paPS[0];
      PS.dwFeatureEndSrc = PS.dwFeatureStartSrc;
      PS.dwFeatureStartSrc -= dwPadLeft;
      PS.iFeatureEndDest = PS.iFeatureStartDest;
      PS.iFeatureStartDest -= (int)dwPadLeft;
      PS.fAppended = TRUE;
      lPSOLASTRUCT.Insert (0, &PS);

      // if had thought there was silence to the left, there certainly wasn't
      *pfSilenceToLeft = FALSE;
   }
   if (dwPadRight) {
      PS = paPS[dwNum-1];
      PS.dwFeatureStartSrc = PS.dwFeatureEndSrc;
      PS.dwFeatureEndSrc += dwPadRight;
      PS.iFeatureStartDest = PS.iFeatureEndDest;
      PS.iFeatureEndDest += (int)dwPadRight;
      PS.fAppended = TRUE;
      lPSOLASTRUCT.Add (&PS);

      // if had thought there was silence to the right, there certainly wasn't
      *pfSilenceToRight = FALSE;
   }

#ifndef FEATUREHACK_COPYWAVEEXACTLY // if doing hack then DON'T do this
   // BUGFIX - Add extra padding of last wavelength looped over and over
   // just in case
   if (!*pfSilenceToLeft) {
      paPSThis = (PPSOLASTRUCT) lPSOLASTRUCT.Get(0);
      PS = paPSThis[0];
      PS.dwFeatureEndSrc = PS.dwFeatureStartSrc;
      // leave PS.dwFeatureStartSrc as the same
      PS.iFeatureEndDest = PS.iFeatureStartDest;
      PS.iFeatureStartDest -= (int)dwIdealPad;
      PS.fAppended = TRUE;
      lPSOLASTRUCT.Insert (0, &PS);

      dwPadLeft += dwIdealPad;
   }
   if (!*pfSilenceToRight && lPSOLASTRUCT.Num()) {
      paPSThis = (PPSOLASTRUCT) lPSOLASTRUCT.Get(0);
      PS = paPSThis[lPSOLASTRUCT.Num()-1];
      PS.dwFeatureStartSrc = PS.dwFeatureEndSrc;
      // leave PS.dwFeatureEndSrc as the same
      PS.iFeatureStartDest = PS.iFeatureEndDest;
      PS.iFeatureEndDest += (int)dwIdealPad;
      PS.fAppended = TRUE;
      lPSOLASTRUCT.Add (&PS);

      dwPadRight += dwIdealPad;
   }
#endif


   // fill in
   *pdwSampleStart = dwPadLeft * pWave->m_dwSRSkip;
   *pdwSampleEnd = (DWORD)(paPS[dwNum-1].iFeatureEndDest - paPS[0].iFeatureStartDest + (int)dwPadLeft) * pWave->m_dwSRSkip;
      // intenitonally NOT adding dwPadRight to this one

   return PSOLASTRUCTContiguous ((PPSOLASTRUCT)lPSOLASTRUCT.Get(0), lPSOLASTRUCT.Num(), fFormantShift, pWave,
      fPhaseAngle, *pfSilenceToLeft, *pfSilenceToRight, plPSOLAPOINTSrc, iTTSQuality, pMem, padwError);
}


/***************************************************************************
CVoiceSynthesize::PSOLAFillInSilence - Blend and fill in with silence.

inputs
   PCM3DWave         pWave - Wave
   DWORD             dwSampleStart - Start sample
   DWORD             dwSampleEnd - End sample
returns
   none
*/
void CVoiceSynthesize::PSOLAFillInSilence (PCM3DWave pWave, DWORD dwSampleStart, DWORD dwSampleEnd)
{
   DWORD dwBlendHalf = pWave->m_dwSamplesPerSec / 20; // blend over 1/20th of a second

   // blend down to silence
   DWORD i, dwChannel;
   for (i = 0; i < dwBlendHalf; i++) {
      int iCur = (int)i - (int)dwBlendHalf + (int)dwSampleStart;
      if ((iCur < 0) || (iCur >= (int)pWave->m_dwSamples))
         continue;   // out of range

      short *ps = &pWave->m_psWave[(DWORD)iCur * pWave->m_dwChannels];
      for (dwChannel = 0; dwChannel < pWave->m_dwChannels; dwChannel++, ps++)
         ps[0] = (short)((int)ps[0] * (int)(dwBlendHalf - i) / (int)dwBlendHalf);
   } // i

   // just zero out the rest quickly
   dwSampleEnd = min(dwSampleEnd, pWave->m_dwSamples);
   if (dwSampleStart < dwSampleEnd)
      memset (pWave->m_psWave + dwSampleStart * pWave->m_dwChannels, 0, (dwSampleEnd - dwSampleStart) * pWave->m_dwChannels * sizeof(short));
}


/***************************************************************************
CVoiceSynthesize::PSOLAFillInPCM - Writes the PCM into the wave.

inputs
   PCM3DWave         pWave - Wave to write to
   short             *pasFrom - Where to get wave from
   DWORD             dwSampleNum - Number of samples in pasFrom
   DWORD             dwSampleStart - Starting sample to copy from, in pasFrom. May copy a bit before to do blend
   DWORD             dwSampleEnd - Official end sample in pasFrom, but will copy all the way to dwSampleNum
   int               iSampleStartWave - Where dwSampleStart maps to in the wave
   int               *piSampleLastWrite - Filled in the end location of where copied to into pWave, based on dwSampleEnd
   int               *piSampleLastWriteUpTo - Where actually filled up to
returns
   none
*/
void CVoiceSynthesize::PSOLAFillInPCM (PCM3DWave pWave, short *pasFrom, DWORD dwSampleNum, DWORD dwSampleStart,
                                       DWORD dwSampleEnd, int iSampleStartWave, int *piSampleLastWrite, int *piSampleLastWriteUpTo)
{
   *piSampleLastWrite = iSampleStartWave + (int)(dwSampleEnd - dwSampleStart);
   *piSampleLastWriteUpTo = iSampleStartWave + (int)(dwSampleNum - dwSampleStart);

   // blend with preexisting wave
   DWORD dwBlendHalf = pWave->m_dwSamplesPerSec / 40; // blend over 1/20th of a second

   // if there isn't any audio before this, then at the start of a wave, so offset the blend so starts at this wave
   if (dwSampleStart < dwBlendHalf) {
      DWORD dwOffset = dwBlendHalf - dwSampleStart;
      dwSampleStart += dwOffset;
      iSampleStartWave += (int)dwOffset;
   }
   DWORD i, dwChannel;
   short *psWave;
   short sValueFrom;
   int iWaveCur;
   for (i = 0; i < 2 * dwBlendHalf; i++) {
      iWaveCur = iSampleStartWave + (int)i - (int)dwBlendHalf;
      if ((iWaveCur < 0) || (iWaveCur >= (int)pWave->m_dwSamples))
         continue;   // out of range

      int iFromCur = (int)dwSampleStart + (int)i - (int)dwBlendHalf;
      sValueFrom = ((iFromCur >= 0) && (iFromCur <= (int)dwSampleNum)) ? pasFrom[iFromCur] : 0;

      psWave = pWave->m_psWave + (DWORD)iWaveCur * pWave->m_dwChannels;
      for (dwChannel = 0; dwChannel < pWave->m_dwChannels; dwChannel++, psWave++)
         psWave[0] = (short)(((int)psWave[0] * (int)(2 * dwBlendHalf - i) + (int)sValueFrom * (int)i) / (int)(2 * dwBlendHalf));
   } // i

   // copy over the remainder
   dwSampleStart += dwBlendHalf;
   iSampleStartWave += (int)dwBlendHalf;
   for (i = dwSampleStart; i < dwSampleNum; i++) {
      iWaveCur = (int)i - (int)dwSampleStart + iSampleStartWave;
      if (iWaveCur < 0)
         continue;   // out of range
      if (iWaveCur >= (int)pWave->m_dwSamples)
         break;   // reached the end

      sValueFrom = pasFrom[i];

      psWave = pWave->m_psWave + (DWORD)iWaveCur * pWave->m_dwChannels;
      for (dwChannel = 0; dwChannel < pWave->m_dwChannels; dwChannel++, psWave++)
         psWave[0] = sValueFrom;
   } // i
}


/***************************************************************************
CVoiceSynthesize::PSOLAVolumeAdjust - Adjust the volume of the PCM
so it's the same as the SRFEATUREs in the wave.

inputs
   PCM3DWave      pWave - Wave with SRFEATUREs and pitch
   fp             *pafSRFEnergy - Energy in pWave, from PSOLAEnergyInOrig()
   short          *pasPCM - PCM samples to write into wave. This volume is modified in place
   DWORD          dwSamples - Number of samples in pasWave
   int            iSampleStart - pasWave[0] is to be copied to this sample in the wave
   BOOL           fSilenceToLeft - TRUE if there's supposed to be silence to the left (before wave)
   BOOL           fSilenceToRight - TRUE if there's supposed to be silence to the right (after dwSamples)
returns
   none
*/
void CVoiceSynthesize::PSOLAVolumeAdjust (PCM3DWave pWave, fp *pafSRFEnergy, short *pasPCM, DWORD dwSamples, int iSampleStart,
                                          BOOL fSilenceToLeft, BOOL fSilenceToRight)
{
   DWORD dwEnergyBins = dwSamples / pWave->m_dwSRSkip + 2;
   CMem memEnergy;
   if (!memEnergy.Required (dwEnergyBins * 4 * sizeof(fp)))
      return;
   fp *pafEnergyWave = (fp*)memEnergy.p;
   fp *pafEnergyPCM = pafEnergyWave + dwEnergyBins;
   fp *pafEnergyWaveFiltered = pafEnergyPCM + dwEnergyBins;
   fp *pafEnergyPCMFiltered = pafEnergyWaveFiltered + dwEnergyBins;

   int iFeatureStart = (int)floor((double)iSampleStart / (double)pWave->m_dwSRSkip);
   int iFeatureStartAtSample = iFeatureStart * (int)pWave->m_dwSRSkip;
   int iSampleOffset = iSampleStart - iFeatureStartAtSample;
   // int iFeatureEnd = iFeatureStart + (int)dwEnergyBins;

   // calculate the energy
   DWORD i, j;
   int iCur;
   for (i = 0; i < dwEnergyBins; i++) {
      // from refernce wave
      iCur = (int)i + iFeatureStart;
      if ((iCur < 0) || (iCur >= (int)pWave->m_dwSRSamples))
         pafEnergyWave[i] = 0;
      else
         pafEnergyWave[i] = pafSRFEnergy[iCur]; // SRFEATUREEnergy (FALSE, pWave->m_paSRFeature + iCur);

      // and from the PCM
      fp fPitch = pWave->PitchAtSample (PITCH_F0, iCur * (int)pWave->m_dwSRSkip, 0);
      fPitch = max(fPitch, CLOSE);
      fp fWavelength = (fp)pWave->m_dwSamplesPerSec / fPitch;
      DWORD dwWavelength = (DWORD)(fWavelength + 0.5);
      dwWavelength = max(dwWavelength, 1);

      double fEnergy = 0;
      int iCenterWave = (int)i * (int)pWave->m_dwSRSkip - iSampleOffset;
      if (!fSilenceToLeft && (iCenterWave < (int)dwWavelength/2))
         iCenterWave = (int)dwWavelength/2;  // to make sure that don't take energy off the wave
      if (!fSilenceToRight && (iCenterWave > (int)dwSamples - (int)dwWavelength/2))
         iCenterWave = (int)dwSamples - (int)dwWavelength/2;
      for (j = 0; j < dwWavelength; j++) {
         iCur = iCenterWave + (int)j - (int)dwWavelength/2;
         if ((iCur < 0) || (iCur >= (int)dwSamples))
            continue;   // out of range
         fEnergy += (double)pasPCM[iCur] * (double)pasPCM[iCur];
      } // j
      fEnergy = sqrt(fEnergy);
      fEnergy /= (double)dwWavelength;
      pafEnergyPCM[i] = fEnergy;
   } // i

   // go through and blur
   DWORD dwBlurHalf = pWave->m_dwSamplesPerSec / pWave->m_dwSRSkip / 40; // 1/20th of a second
   dwBlurHalf = max(dwBlurHalf, 1);
   DWORD dwBlurPCM;
   int iBlur;
   double afEnergyAverage[2];
   for (dwBlurPCM = 0; dwBlurPCM < 2; dwBlurPCM++) {
      fp *pafEnergyOrig = dwBlurPCM ? pafEnergyPCM : pafEnergyWave;
      fp *pafEnergyFiltered = dwBlurPCM ? pafEnergyPCMFiltered : pafEnergyWaveFiltered;
      afEnergyAverage[dwBlurPCM] = 0;
      for (i = 0; i < dwEnergyBins; i++) {
         DWORD dwCount = 0;
         double fSum = 0;
         DWORD dwWeight;
         for (iBlur = -(int)dwBlurHalf; iBlur <= (int)dwBlurHalf; iBlur++) {
            iCur = iBlur + (int)i;
            if ((iCur < 0) || (iCur >= (int) dwEnergyBins))
               continue;   // out of range, so don't count

            dwWeight = dwBlurHalf + 1 - (DWORD)abs(iBlur);
            dwCount += dwWeight;
            fSum += (double)dwWeight * pafEnergyOrig[iCur];
         } // iBlur

         // write it
         fSum /= (double)dwCount;
         pafEnergyFiltered[i] = max(fSum, CLOSE);
         
         // keep track of the average energy
         afEnergyAverage[dwBlurPCM] += pafEnergyFiltered[i];
      } // i

      afEnergyAverage[dwBlurPCM] /= (double)dwEnergyBins;
   } // dwBlurPCM

   // figure out maximum and minmum scale
   fp fScaleAverage = afEnergyAverage[0] / afEnergyAverage[1];
   fp fScaleMax = fScaleAverage * 4.0;
   fp fScaleMin = fScaleAverage / 4.0;

   // determine the scaling, writing back into pafEnergyPCM
   fp fScale;
   for (i = 0; i < dwEnergyBins; i++) {
      fScale = pafEnergyWaveFiltered[i] / pafEnergyPCMFiltered[i];
      fScale = max (fScale, fScaleMin);
      fScale = min (fScale, fScaleMax);

      // fScale *= 0.04; // needed this for SRFEATUREEnergy(), but not using anymore
      pafEnergyPCM[i] = fScale;
   } // i

   // scale the PCM
   for (i = 0; i < dwSamples; i++) {
      int iSampleWithOffset = (int)i - iSampleOffset;
      fp fEnergyBin = (fp)iSampleWithOffset / (fp)pWave->m_dwSRSkip;

      int iEnergyBin = floor (fEnergyBin);
      fEnergyBin -= (fp)iEnergyBin;
      int iEnergyBinPlusOne = iEnergyBin+1;
      iEnergyBin = max(iEnergyBin, 0);
      iEnergyBin = min(iEnergyBin, (int)dwEnergyBins-1);
      iEnergyBinPlusOne = max(iEnergyBinPlusOne, 0);
      iEnergyBinPlusOne = min(iEnergyBinPlusOne, (int)dwEnergyBins-1);

      fScale = (1.0 - fEnergyBin) * pafEnergyPCM[iEnergyBin] + fEnergyBin * pafEnergyPCM[iEnergyBinPlusOne];

      fScale *= (fp)pasPCM[i];
      fScale = max(fScale, -32768);
      fScale = min(fScale, 32767);
      pasPCM[i] = (short)fScale;
   } // i
}

/***************************************************************************
CVoiceSynthesize::PSOLAEnergyInOrig - Calculated the energy in the
original wave. Used for PSOLAVolumeAdjust.

inputs
   PCM3DWave         pWave - WAve. Must have PCM.
   PCMem             pMem - Allocated and filled with one fp per SRFEATURE
returns
   BOOL - TRUE if success
*/
BOOL CVoiceSynthesize::PSOLAEnergyInOrig (PCM3DWave pWave, PCMem pMem)
{
   if (!pMem->Required (pWave->m_dwSRSamples * sizeof(fp)))
      return FALSE;
   fp *paf = (fp*)pMem->p;

   // calculate the energy
   DWORD i, j;
   int iCur;
   for (i = 0; i < pWave->m_dwSRSamples; i++) {
      // and from the PCM
      fp fPitch = pWave->PitchAtSample (PITCH_F0, i * pWave->m_dwSRSkip, 0);
      fPitch = max(fPitch, CLOSE);
      fp fWavelength = (fp)pWave->m_dwSamplesPerSec / fPitch;
      DWORD dwWavelength = (DWORD)(fWavelength + 0.5);
      dwWavelength = max(dwWavelength, 1);

      double fEnergy = 0;
      int iCenterWave = (int)i * (int)pWave->m_dwSRSkip;
      for (j = 0; j < dwWavelength; j++) {
         iCur = iCenterWave + (int)j - (int)dwWavelength/2;
         if ((iCur < 0) || (iCur >= (int)pWave->m_dwSamples))
            continue;   // out of range
         short fValue = pWave->m_psWave[(DWORD)iCur*pWave->m_dwChannels];
         fEnergy += (double)fValue * (double)fValue;
      } // j
      fEnergy = sqrt(fEnergy);
      fEnergy /= (double)dwWavelength;
      paf[i] = fEnergy;
   } // i

   return TRUE;
}


/***************************************************************************
CVoiceSynthesize::PSOLABlendQuality - Returns a number for the blend
quality. Higher is better.

inputs
   PCM3DWave         pWave - Wave
   short             *pasPCM - PCM
   DWORD             dwSamples - Number of samples in PCM
   int               iCenterWave - Center sample in the wave
   int               iCenterPCM - Center sample in the PCM
returns
   double - High score is better
*/
double CVoiceSynthesize::PSOLABlendQuality (PCM3DWave pWave, short *pasPCM, DWORD dwSamples, int iCenterWave, int iCenterPCM)
{
   fp fPitch = pWave->PitchAtSample (PITCH_F0, iCenterWave, 0);
   fPitch = max(fPitch, CLOSE);
   fp fWavelength = (fp)pWave->m_dwSamplesPerSec / fPitch;
   DWORD dwWavelength = (DWORD)(fWavelength + 0.5);
   dwWavelength = max(dwWavelength, 1);

   double fSum = 0;
   DWORD i;
   int iCurWave = iCenterWave - (int)dwWavelength/2;
   int iCurPCM = iCenterPCM - (int)dwWavelength/2;
   for (i = 0; i < dwWavelength; i++, iCurWave++, iCurPCM++) {
      int sWave = ((iCurWave >= 0) && (iCurWave < (int)pWave->m_dwSamples)) ?
         pWave->m_psWave[iCurWave * (int)pWave->m_dwChannels] : 0;
      int sPCM = ((iCurPCM >= 0) && (iCurPCM < (int)dwSamples)) ?
         pasPCM[iCurPCM] : 0;

      fSum += (double)(sWave * sPCM);
   } // i

   // fSum = sqrt(fSum);
   fSum /= (double)fWavelength;

   return fSum;
}


/***************************************************************************
CVoiceSynthesize::PSOLAAutoCorrelate - Finds the best offset for autocorrelation
between the two waves.

inputs
   PCM3DWave         pWave - Wave
   short             *pasPCM - PCM
   DWORD             dwSamples - Number of samples in PCM
   int               iCenterWave - Center sample in the wave
   int               iCenterPCM - Center sample in the PCM
   DWORD             dwFractionOfWavelength - What fraction of a wavelength to scan through
   double            *pfScore - Filled in with the best score. High is better.
returns
   int iOffset - Offset the PCM by this much
*/
int CVoiceSynthesize::PSOLAAutoCorrelate (PCM3DWave pWave, short *pasPCM, DWORD dwSamples, int iCenterWave,
                                          int iCenterPCM, DWORD dwFractionOfWavelength, double *pfScore)
{
   fp fPitch = pWave->PitchAtSample (PITCH_F0, iCenterWave, 0);
   fPitch = max(fPitch, CLOSE);
   fp fWavelength = (fp)pWave->m_dwSamplesPerSec / fPitch / (fp)dwFractionOfWavelength;
   DWORD dwWavelength = (DWORD)(fWavelength + 0.5);
   dwWavelength = max(dwWavelength, 1);

   DWORD i;
   int iBest = -1000000000;
   double fBest = 0;
   double fScore;
   int iCur;
   for (i = 0; i < dwWavelength; i++) {
      iCur = (int)i - (int)dwWavelength/2;
      fScore = PSOLABlendQuality (pWave, pasPCM, dwSamples, iCenterWave, iCenterPCM - iCur);

      if ((iBest == -1000000000) || (fScore > fBest)) {
         iBest = iCur;
         fBest = fScore;
      }
   } // i

   *pfScore = fBest;
   return iBest;
}




/***************************************************************************
CVoiceSynthesize::PSOLASTRUCTNonContiguous - Synthesizes using the PSOLASTRUCT,
which aren't necessarily contiguous.

inputs
   PPSOLASTRUCT      paPS - Array of PSOLASTRUCT.
   DWORD             dwNum - Number of entries in paPS
   fp                fFormantShift - Number of octaves to shift formants up/down.
   PCM3DWave         pWave - Where to fill in. This should already have the SRFEATUREs and pitch
                           filled in. The wave should have space for PCM allocated. Should have
                           PCM in the original wave that's used to match up energy for new.
   int               iTTSQuality - Quality to use
returns
   BOOL - TRUE if success, FALSE if error
*/
// #define AUTOCORRELATIONATTEMPTS       4     // create for phase variations of each wave

BOOL CVoiceSynthesize::PSOLASTRUCTNonContiguous (PPSOLASTRUCT paPS, DWORD dwNum, fp fFormantShift, PCM3DWave pWave,
                                                 int iTTSQuality)
{
   CMem memEnergy;
   if (!PSOLAEnergyInOrig (pWave, &memEnergy))
      return FALSE;

   // zero out the wave
   memset (pWave->m_psWave, 0, pWave->m_dwSamples * pWave->m_dwChannels * sizeof(short));

#ifdef _DEBUG
   int iCurBlock = -1;
#endif

   // loop over contiguous segments
   DWORD i;
   // CMem amemWave[AUTOCORRELATIONATTEMPTS];
   DWORD dwSampleStart, dwSampleEnd;
   int iSampleLastWrite = 0;  // sample that was last written
   int iSampleLastWriteUpTo = 0; // sample that was last written up to
   while (dwNum) {
#ifdef _DEBUG
      iCurBlock++;
#endif

      // if not contiguous then zero
      int iWriteTo = paPS[0].iFeatureStartDest * (int)pWave->m_dwSRSkip;
      BOOL fWasSilence = FALSE;
      if (!iWriteTo || (iWriteTo != iSampleLastWrite)) {
         PSOLAFillInSilence (pWave, max(iSampleLastWrite, 0), max(iSampleLastWriteUpTo, 0));
         fWasSilence = TRUE;
      }

      // find contiguous
      for (i = 1; i < dwNum; i++) {
         // if change wave then stop
         if (paPS[i-1].pasWave != paPS[i].pasWave)
            break;

         // if start != end then stop
         if (paPS[i-1].iFeatureEndDest != paPS[i].iFeatureStartDest)
            break;
         if (paPS[i-1].dwFeatureEndSrc != paPS[i].dwFeatureStartSrc)
            break;
      } // i
      DWORD dwNumInBlock = i;

#if 0 // def _DEBUG // to test
      if ((iCurBlock < 1) || (iCurBlock > 1)) {
         paPS += dwNumInBlock;
         dwNum -= dwNumInBlock;
         continue;
      }
#endif

      BOOL fSilenceToLeft = FALSE;
      BOOL fSilenceToRight = FALSE;
      if ((dwNumInBlock >= dwNum) || (paPS[dwNumInBlock-1].iFeatureEndDest != paPS[dwNumInBlock].iFeatureStartDest))
         fSilenceToRight = TRUE;
      if (fWasSilence)
         fSilenceToLeft = TRUE;

      DWORD dwError;
      // synth different phase offsets and see which is best
      //DWORD dwBest = (DWORD)-1;
      //int iBestOffset = 0;
      //double fBest = 0;
      //double fScore;
      int iOffset;

#ifdef _DEBUG
      OutputDebugStringW (L"\r\n\r\nPSOLASTRUCTNonContiguous errors: ");
#endif

      // loop around, trying to find the PSOLA arrangement with the least error
      CListFixed lPSOLAPOINTSrc; // intitially blank

      // how many passes - based on TTS quality
      DWORD dwPasses = 2 << max(iTTSQuality, 0);
      BOOL fSilenceToLeftTemp;
      BOOL fSilenceToRightTemp;
      DWORD dwBestAttempt = (DWORD)-1;
      DWORD dwBestError = 0;
      DWORD dwAttempt;
      for (dwAttempt = 0; dwAttempt < dwPasses; dwAttempt++) {
         fSilenceToLeftTemp = fSilenceToLeft;
         fSilenceToRightTemp = fSilenceToRight;

         if (!PSOLASTRUCTContiguousExtra (paPS, dwNumInBlock, fFormantShift, pWave,
               -(fp)dwAttempt / (fp)dwPasses, &lPSOLAPOINTSrc, iTTSQuality,
               NULL, &dwSampleStart, &dwSampleEnd,
               &fSilenceToLeftTemp, &fSilenceToRightTemp, &dwError))
            continue;

#ifdef _DEBUG
         WCHAR szTemp[64];
         swprintf (szTemp, L"%d ", (int)dwError);
         OutputDebugStringW (szTemp);
#endif

         if ((dwBestAttempt == (DWORD)-1) || (dwError < dwBestError)) {
            dwBestAttempt = dwAttempt;
            dwBestError = dwError;
         }
      } // dwAttempt
      if (dwBestAttempt == (DWORD)-1) {
         // shouldnt happen
         paPS += dwNumInBlock;
         dwNum -= dwNumInBlock;
         continue;
      }

      // DWORD adwSampleStart[AUTOCORRELATIONATTEMPTS], adwSampleEnd[AUTOCORRELATIONATTEMPTS];
      short *pasWave;
      DWORD dwSamples;

      // now, actually calculate the audio
      CMem memWave;

      fSilenceToLeftTemp = fSilenceToLeft;
      fSilenceToRightTemp = fSilenceToRight;
      dwAttempt = dwBestAttempt;
      if (!PSOLASTRUCTContiguousExtra (paPS, dwNumInBlock, fFormantShift, pWave,
            -(fp)dwAttempt / (fp)dwPasses, &lPSOLAPOINTSrc, iTTSQuality,
            &memWave, &dwSampleStart, &dwSampleEnd,
            &fSilenceToLeftTemp, &fSilenceToRightTemp, &dwError)) {

         // shouldnt happen
         paPS += dwNumInBlock;
         dwNum -= dwNumInBlock;
         continue;
      }

#ifdef _DEBUG
         WCHAR szTemp[64];
         swprintf (szTemp, L"\r\nPSOLASTRUCTContiguousExtra using %d ", (int)dwError);
         OutputDebugStringW (szTemp);
#endif

      pasWave = (short*)memWave.p;
      dwSamples = (DWORD)memWave.m_dwCurPosn / sizeof(short);

      // adjust the volume
      PSOLAVolumeAdjust (pWave, (fp*)memEnergy.p, pasWave, dwSamples, iWriteTo - (int)dwSampleStart,
         fSilenceToLeftTemp, fSilenceToRightTemp);

      fp fScore;
      if (fWasSilence)
         iOffset = 0;
      else
         // else, noise, so try different offsets
         iOffset = PSOLAAutoCorrelate (pWave, pasWave, dwSamples, iWriteTo, (int)dwSampleStart,
            1, &fScore);

      // dwSampleStart = adwSampleStart[dwBest];
      // dwSampleEnd = adwSampleEnd[dwBest];
      pasWave = (short*)memWave.p;
      dwSamples = (DWORD)memWave.m_dwCurPosn / sizeof(short);


      // fill in the PCM
      int iWriteUpToOld = iSampleLastWriteUpTo;
      PSOLAFillInPCM (pWave, pasWave, dwSamples, dwSampleStart, dwSampleEnd,
         iWriteTo + iOffset, &iSampleLastWrite, &iSampleLastWriteUpTo);
      iSampleLastWrite -= iOffset;  // to counteract effects of iWriteTo
      iSampleLastWriteUpTo = max(iSampleLastWriteUpTo, iWriteUpToOld);

      // advance
      paPS += dwNumInBlock;
      dwNum -= dwNumInBlock;
   } // dwNum

   // silence out the ending
   PSOLAFillInSilence (pWave, max(iSampleLastWrite,0), max(iSampleLastWriteUpTo,0));

   return TRUE;
}

