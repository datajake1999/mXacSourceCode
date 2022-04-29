/***************************************************************************
VoiceChat.cpp - Code for doing voice chat.

begun 21/8/2005
Copyright 2005 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <mmreg.h>
#include <msacm.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"


//#define SIZESCALEBY
//#ifdef SIZESCALEBY
//   #define SIZESCALE(x)    ((x)*4/3+1)     // so take less data
//#else
//   #define SIZESCALE(x)    (x)   // no change
//#endif

// BUGFIX - redo SIZESCALE so that if it's header 1 or 2 then skip more,
// but if 3rd header then less (so get higher fidelity). y is the blocktype
#define SIZESCALE(x,y)     ((y <= 2) ? ((x)*4/3+1) : (x))

#define OCTAVEMAXSCALE     2     // how accurate octave maximum is stored

// VOICECHATHDR - Header of voice chat info
typedef struct {
   DWORD          dwSize;        // size of the header, voice disguise wave string, and all the data
   WORD           wID;           // unique ID for the data format

   WORD           wNumBlocks;    // number of blocks of VOICECHATBLOCK to follow
   WORD           wBlockSize;    // size of each block in bytes

   // voice disguise info
   BYTE           bNonIntHarmonics; // voice disguise's non-integer harmonics values. Use 64 for 1.0.
   char           cPitchBase;    // pitch of the wave base for voice diguise's wave. -128 if value is 0.
   // VoiceDisguise wave string - NULL terminated unicode string for voice disguise wave
   // Array of wNumBlocks compressed VOICECHATBLOCKn structures
} VOICECHATHDR, *PVOICECHATHDR;


#define VCH_BASEFREQ       150   // base frequency used in the header and octaves


// VOICECHATBLOCK1 - Block of med-quality voicechat information
#define VCB1_OCTAVES       SROCTAVE
#define VCB1_PHASES        8        // store 8 phase values
//#define VCB1_PHASES        62        // store 8 phase values .. tried, but doesn't make much difference to quality
typedef struct {
   // must be the same in all SIZEOFVOICECHATBLOCK
   char           cPitch;        // pitch, in quarter-tones above/below 150 Hz. 12x4=48 per octave
   char           cPeak;         // peak dB
   BYTE           bCopies;       // number of copies of this

   // changes for each SIZEOFVOICECHATBLOCK
   BYTE           abPhase[VCB1_PHASES/2];   // number of phases to store, nibble each
   BYTE           abOctave[VCB1_OCTAVES]; // high nibble is number of dB (/2) below peak is max for octave,
                                 // low nibble is percent of noise in the octave (0=none, 15=all noise)

   // Raw data. One nibble for each amplitude delta in dB (/2). Keeping every 4, then 3, then 2,
   // then 1 points
} VOICECHATBLOCK1, *PVOICECHATBLOCK1;

//#ifdef SIZESCALEBY
   #define SIZEOFVOICECHATBLOCK1    (sizeof(VOICECHATBLOCK1)+25)  // so know ahead of time
//#else
//   #define SIZEOFVOICECHATBLOCK1    (sizeof(VOICECHATBLOCK1)+44)  // so know ahead of time
//#endif


// VOICECHATBLOCK2 - Block of low-quality voicechat information
#define VCB2_OCTAVES       (SROCTAVE-2)   // toss out lowest and highest octaves
typedef struct {
   // must be the same in all SIZEOFVOICECHATBLOCK
   char           cPitch;        // pitch, in quarter-tones above/below 150 Hz. 12x4=48 per octave
   char           cPeak;         // peak dB
   BYTE           bCopies;       // number of copies of this

   // changes for each SIZEOFVOICECHATBLOCK
   BYTE           abOctave[VCB2_OCTAVES]; // high nibble is number of dB (/2) below peak is max for octave,
                                 // low nibble is percent of noise in the octave (0=none, 15=all noise)

   // Raw data. One nibble for each amplitude delta in dB (/2). Keeping every 4, then 3, then 2,
   // then 1 points
} VOICECHATBLOCK2, *PVOICECHATBLOCK2;
//#ifdef SIZESCALEBY
   #define SIZEOFVOICECHATBLOCK2    (sizeof(VOICECHATBLOCK2)+18)  // so know ahead of time
//#else
//   #define SIZEOFVOICECHATBLOCK2    (sizeof(VOICECHATBLOCK2)+31)  // so know ahead of time
//#endif


// VOICECHATBLOCK3 - Block of high-quality voice chat
#define VCB3_OCTAVES          SROCTAVE    // all octaves
#define VCB3_PHASES           16        // store 16 phase values
typedef struct {
   // must be the same in all SIZEOFVOICECHATBLOCK
   char           cPitch;        // pitch, in quarter-tones above/below 150 Hz. 12x4=48 per octave
   char           cPeak;         // peak dB
   BYTE           bCopies;       // number of copies of this

   // changes for each SIZEOFVOICECHATBLOCK
   BYTE           abPhase[VCB3_PHASES];   // number of phases to store, nibble each
         // BUGFIX - Make abPhase 8-bits instead of 4
   BYTE           abOctave[VCB3_OCTAVES]; // high nibble is number of dB (/2) below peak is max for octave,
                                 // low nibble is percent of noise in the octave (0=none, 15=all noise)

   // Raw data. One nibble for each amplitude delta in dB (/2). Keeping every 4, then 3, then 2,
   // then 1 points
} VOICECHATBLOCK3, *PVOICECHATBLOCK3;

//#ifdef SIZESCALEBY
//   #define SIZEOFVOICECHATBLOCK3    (sizeof(VOICECHATBLOCK3)+25)  // so know ahead of time
//#else
   #define SIZEOFVOICECHATBLOCK3    (sizeof(VOICECHATBLOCK3)+44)  // so know ahead of time
//#endif

// CLUSTERINFO
typedef struct {
   fp          fDiffFromRight;   // how similar this is to the one to the right (skipping removed features)
   fp          fDiffFromLeft;    // likewise, difference from left
   DWORD       dwIndexRight;     // right index
   DWORD       dwIndexLeft;      // left index
   DWORD       dwCount;          // number of times this is used, no more than 255
} CLUSTERINFO, *PCLUSTERINFO;


// viseme sounds - One for each english phoneme. From SpeechRecog.cpp OutputDebugstring
// NOTE: Ignoring deltas for viseme sounds
static fp gafVisemeSounds[41][SROCTAVE][2] = {
	10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	283.475, 0.490397, 369.416, 1.74983, 789.095, 5.44689, 667.516, 6.92857, 150.698, 4.40703, 105.569, 6.10328, 30.0242, 5.21569, 
	302.785, 0.812923, 377.905, 3.41761, 564.692, 6.87408, 286.467, 5.46038, 297.607, 9.33055, 160.714, 10.3413, 44.7508, 8.57482, 
	270.482, 2.42417, 367.172, 9.84634, 546.966, 14.8439, 302.758, 11.614, 118.105, 10.9096, 97.3569, 17.0688, 35.2929, 15.0778, 
	278.358, 0.483008, 410.026, 1.79021, 823.792, 5.22365, 472.035, 6.04792, 89.0498, 2.72572, 72.8822, 3.59653, 27.2523, 3.65477, 
	289.05, 0.464288, 382.28, 1.93172, 746.063, 5.48327, 520.445, 7.03535, 171.527, 4.32993, 103.883, 4.89546, 32.5977, 5.03986, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	285.284, 0.708539, 363.922, 2.91389, 626.906, 6.21556, 422.71, 5.98783, 231.49, 8.04308, 136.819, 8.9632, 39.8054, 8.94127, 
	156.715, 2.32354, 191.601, 9.0342, 131.102, 13.559, 62.9921, 10.6466, 37.4221, 7.71026, 27.7903, 8.85544, 14.1389, 9.02839, 
	37.1835, 2.62786, 43.0873, 6.67821, 27.3161, 10.2689, 14.4202, 19.9434, 70.4718, 264.4, 128.983, 526.421, 51.6316, 519.605, 
	110.588, 4.09555, 112.68, 11.2231, 59.7466, 12.441, 27.4872, 9.93912, 41.1091, 27.3303, 38.2919, 40.1674, 14.9676, 29.8766, 
	185.777, 2.03754, 220.162, 8.15854, 143.406, 9.36482, 53.8334, 6.68729, 59.5551, 10.2316, 76.563, 24.4881, 27.9523, 20.286, 
	293.322, 1.21162, 404.879, 4.73334, 623.88, 10.0423, 280.524, 8.22909, 261.008, 12.901, 143.986, 13.5869, 44.6205, 12.5348, 
	250.55, 1.77179, 425.624, 5.45274, 438.052, 8.7952, 339.302, 11.2822, 115.241, 7.01287, 32.3581, 3.90953, 19.5629, 5.33587, 
	297.42, 0.798147, 498.301, 3.05538, 488.083, 3.37658, 157.712, 2.0295, 332.34, 7.46903, 150.9, 6.7743, 46.9777, 6.8495, 
	39.0418, 3.15571, 43.2989, 8.27624, 39.1118, 18.6923, 20.6867, 28.5217, 22.4974, 52.6998, 41.9802, 119.987, 22.5004, 135.587, 
	147.829, 3.22954, 195.278, 13.2297, 116.557, 14.8778, 54.7227, 15.4035, 51.3261, 20.3673, 41.2083, 26.2992, 19.7017, 23.3464, 
	169.924, 2.089, 160.633, 4.79658, 96.7412, 7.23297, 40.6687, 9.06837, 69.2059, 21.2471, 57.7266, 24.7682, 22.5301, 17.7018, 
	282.336, 2.43886, 467.827, 9.04841, 326.289, 8.50537, 104.987, 5.45839, 190.191, 12.457, 115.528, 19.3274, 36.1346, 15.29, 
	291.125, 1.48789, 495.059, 5.25697, 176.939, 4.51661, 52.8142, 2.15329, 204.239, 7.46527, 118.475, 8.26317, 34.5382, 7.80785, 
	84.3568, 5.01823, 105.316, 9.85277, 51.3651, 7.78714, 20.7334, 11.4312, 82.5462, 168.523, 110.086, 339.146, 45.2367, 337.046, 
	51.4228, 3.25228, 55.9636, 8.73347, 49.2152, 15.5773, 27.1874, 23.2444, 25.7938, 41.4223, 34.5165, 75.1899, 14.4608, 60.5512, 
	236.666, 0.82025, 369.06, 3.29147, 481.743, 8.54977, 199.333, 7.22148, 65.9634, 2.97172, 72.9638, 4.70948, 19.9802, 3.67504, 
	294.235, 0.57835, 324.274, 1.88118, 149.555, 2.21327, 107.221, 2.339, 56.6731, 1.62799, 33.6665, 1.67296, 17.6007, 1.90851, 
	274.982, 0.759909, 289.277, 2.16359, 123.489, 1.79244, 70.4897, 1.76477, 61.4912, 2.49293, 36.3601, 2.85138, 17.2749, 2.45717, 
	216.441, 1.60746, 229.608, 3.74605, 95.7614, 3.29233, 50.2259, 2.62599, 46.8384, 4.05648, 30.0055, 4.30374, 14.7236, 4.6347, 
	299.251, 0.766588, 499.547, 3.26574, 700.329, 5.51564, 337.439, 4.59873, 106.558, 2.59865, 73.9684, 3.11875, 29.17, 3.65482, 
	279.413, 0.510713, 453.109, 2.03696, 729.429, 3.07175, 319.34, 3.18552, 159.09, 4.0507, 85.7364, 4.09846, 33.9877, 4.4794, 
	74.0658, 3.23754, 74.1428, 8.40407, 66.9751, 15.2242, 36.3403, 17.7744, 23.7546, 17.3803, 24.2519, 24.7369, 10.5417, 20.2716, 
	271.192, 1.20872, 465.384, 4.3033, 578.96, 8.10341, 415.357, 11.5086, 164.197, 5.95316, 43.0322, 2.79133, 22.9963, 4.07549, 
	18.7599, 2.86439, 18.676, 4.7699, 14.0984, 8.54069, 7.49338, 13.425, 25.5224, 89.6629, 236.044, 1043.08, 102.296, 953.125, 
	28.1237, 2.9575, 31.2876, 5.77209, 18.3958, 9.55235, 11.2955, 22.5322, 88.8468, 375.4, 179.269, 768.712, 82.8162, 853.971, 
	64.9421, 3.59293, 73.5458, 9.30186, 50.4391, 13.6324, 24.4097, 15.7073, 37.0302, 58.1749, 53.92, 138.263, 21.3573, 117.721, 
	25.611, 3.5147, 24.3675, 6.92601, 18.2653, 11.1369, 10.1066, 13.2395, 15.3164, 34.172, 51.766, 150.166, 19.8271, 129.871, 
	322.559, 2.12717, 684.923, 10.1106, 648.172, 10.4732, 319.42, 8.53461, 148.353, 7.18017, 95.4164, 5.21025, 36.5783, 5.926, 
	306.557, 0.892033, 591.555, 3.30693, 212.814, 2.89717, 111.438, 2.33641, 102.824, 3.24568, 56.4657, 4.94556, 25.9596, 5.04509, 
	124.873, 3.80611, 142.028, 12.6834, 112.17, 15.3837, 57.3563, 14.2103, 37.6604, 15.0445, 43.1638, 29.5698, 23.1319, 34.6449, 
	265.101, 0.153462, 483.588, 1.40913, 403.347, 3.24474, 152.422, 4.2739, 48.7748, 0.699401, 34.6926, 0.618041, 19.6781, 1.05147, 
	320.86, 0.218056, 503.456, 1.2635, 166.814, 1.01711, 56.814, 0.801013, 183.966, 4.17755, 103.177, 3.54127, 36.1199, 3.14647, 
	19.6671, 6.76945, 18.4665, 8.71649, 10.53, 8.58808, 6.276, 11.5435, 26.4624, 82.859, 211.057, 819.376, 84.8295, 700.188, 
	79.2285, 4.68591, 103.388, 7.92643, 61.4538, 8.56581, 29.1336, 16.2915, 101.291, 235.171, 130.361, 444.79, 61.1223, 504.693
};


/***************************************************************************
VisemeSoundCalcEnergy - Calculates the energy of a viseme sound.

inputs
   fp          *paf - Array of SROCTAVE*2 fp's
returns
   fp - Energy
*/
fp VisemeSoundCalcEnergy (fp *paf)
{
   DWORD i;
   double fSum = 0;
   for (i = 0; i < SROCTAVE*2; i++)
      fSum += (double)paf[i] * (double)paf[i];
   fSum = sqrt(fSum);

   return (fp) fSum;
}


/***************************************************************************
VisemeSoundCompare - COmpares two viseme sounds for a SR score. Lower
is better.

inputs
   fp          *pafA - Array of SROCTAVE*2
   fp          fAEnergy - Energy from VisemeSoundCalcEnergy()
   fp          *pafB - Array of SROCTAVE*2
   fp          fBEnergy - Energy from VisemeSoundCalcEnergy()
returns
   fp - Low score is better
*/
fp VisemeSoundCompare (fp *pafA, fp fAEnergy, fp *pafB, fp fBEnergy)
{
   // if either is 0, then a very high score
   if (!fAEnergy || !fBEnergy)
      return 10000;

   // debels difference, but no more than noise
   fp fDb = fabs(log10(fAEnergy / fBEnergy) * 20.0);
   fDb = min(fDb, (fp)(-(SRNOISEFLOOR)));

   // dot product
   double fDot = 0;
   DWORD i;
   for (i = 0; i < SROCTAVE*2; i++)
      fDot += (double)pafA[i] * (double)pafB[i];
   fDot /= (double)fAEnergy * (double)fBEnergy; // so normalize
   fDot = 1.0 - fDot;   // so if no difference then 0
#ifdef _DEBUG
   if ((fDot < 0) || (fDot > 1))
      OutputDebugString ("\r\nVisemeSoundCompare out of range");
#endif

   fDb += fDot * SRCOMPAREWEIGHT;

   return fDb;
}


/***************************************************************************
VisemeSoundAggregate - Given a wave file, between two samples, this
aggregates all the energy into one bin.

inputs
   PCM3DWave         pWave - Wave file
   DWORD             dwSampleStart - Start sample
   DWORD             dwSampleEnd - End sample
   fp                *paf - Points to an array of SROCTAVE*2 entries to be filled in
returns
   none
*/
void VisemeSoundAggregate (PCM3DWave pWave, DWORD dwSampleStart, DWORD dwSampleEnd, fp *paf)
{
   double af[SROCTAVE][2];
   memset (af, 0, sizeof(af));
   if (pWave->m_dwSRSkip) {
      dwSampleStart /= pWave->m_dwSRSkip;
      dwSampleEnd /= pWave->m_dwSRSkip;
   }
   else
      dwSampleStart = dwSampleEnd = 0; // since none
   dwSampleStart = min(dwSampleStart, pWave->m_dwSRSamples);
   dwSampleEnd = min(dwSampleEnd, pWave->m_dwSRSamples);

   // sum
   DWORD i, k;
   DWORD dwCount = 0;
   for (i = dwSampleStart; i < dwSampleEnd; i++) {
      for (k = 0; k < SRDATAPOINTS; k++) {
         af[k / SRPOINTSPEROCTAVE][0] += (double) DbToAmplitude (pWave->m_paSRFeature[i].acVoiceEnergy[k]);
         af[k / SRPOINTSPEROCTAVE][1] += (double) DbToAmplitude (pWave->m_paSRFeature[i].acNoiseEnergy[k]);
      } // k
      dwCount += SRPOINTSPEROCTAVE;
   } // i

   if (!dwCount)
      dwCount = 1;   // so dont divide by 0
   for (i = 0; i < SROCTAVE*2; i++)
      paf[i] = (fp) (((double*)&af[0][0])[i] / (double)dwCount);
}


/***************************************************************************
VisemeDetermineEnglishPhone - Determines the english phoneme in a wave
by looking at a given sample range

inputs
   PCM3DWave         pWave - Wave
   DWORD             dwSampleStart - Start sample;
   DWORD             dwSampleEnd - End sample
   fp                *pafEnergy - Energy for the 41 english phones in gafVisemeSounds
returns
   DWORD - English phoneme thats spoken
*/
DWORD VisemeDetermineEnglishPhone (PCM3DWave pWave, DWORD dwSampleStart, DWORD dwSampleEnd, fp *pafEnergy)
{
   fp afAudio[SROCTAVE*2];
   VisemeSoundAggregate (pWave, dwSampleStart, dwSampleEnd, afAudio);
   fp fAudioEnergy = VisemeSoundCalcEnergy (afAudio);
   if (!fAudioEnergy)
      return 0;   // must be silence

   // loop and find the best
   DWORD i;
   DWORD dwBest = (DWORD)-1;
   fp fBestScore = 1000000;
   for (i = 0; i < sizeof(gafVisemeSounds) / sizeof(gafVisemeSounds[0]); i++) {
      if (!pafEnergy[i])
         continue;   // no data

      fp fCompare = VisemeSoundCompare (afAudio, fAudioEnergy, &gafVisemeSounds[i][0][0], pafEnergy[i]);
      if ((dwBest == (DWORD)-1) || (fCompare < fBestScore)) {
         dwBest = i;
         fBestScore = fCompare;
      }
   } // i

   if (dwBest == (DWORD)-1)
      return 0;   // assume silence

   return dwBest;
}


/***************************************************************************
VisemeFillInPhonemes - Given a wave, this fills in the guessed phonemes
for the wave.

inputs
   PCM3DWave         pWave - Wave
returns
   none
*/

void VisemeFillInPhones (PCM3DWave pWave)
{
   pWave->m_lWVPHONEME.Clear();

   // how many samples in a phoneme
   DWORD dwVisemePhoneSize = pWave->m_dwSamplesPerSec / 10;     // 1/5 of a sec
   dwVisemePhoneSize = max(dwVisemePhoneSize, 1);

   // determine the energy for each of the standard visemes
   fp afVisemeEnergy[sizeof(gafVisemeSounds) / sizeof(gafVisemeSounds[0])];
   DWORD i;
   for (i = 0; i < sizeof(afVisemeEnergy) / sizeof(fp); i++)
      afVisemeEnergy[i] = VisemeSoundCalcEnergy (&gafVisemeSounds[i][0][0]);

   WVPHONEME wp;
   memset (&wp, 0, sizeof(wp));
   for (i = 0; i < pWave->m_dwSamples; i += dwVisemePhoneSize) {
      // determine best english phone
      DWORD dwBest = VisemeDetermineEnglishPhone (pWave, i, i + dwVisemePhoneSize, afVisemeEnergy);

      // get the english phone for this
      PLEXENGLISHPHONE pep = MLexiconEnglishPhoneGet (dwBest);
      if (!pep)
         continue;   // shouldnt happen

      // make up a phoneme
      wp.dwEnglishPhone = dwBest;
      wp.dwSample = i;
      memcpy (wp.awcNameLong, pep->szPhoneLong, sizeof(wp.awcNameLong));
      pWave->m_lWVPHONEME.Add (&wp);
   } // i

   // final silence
   wp.dwEnglishPhone = 0;
   wp.dwSample = pWave->m_dwSamples;
   memset (wp.awcNameLong, 0, sizeof(wp.awcNameLong));
   wcscpy (wp.awcNameLong, L"<s>");
   pWave->m_lWVPHONEME.Add (&wp);
}


/***************************************************************************
VoiceChatDetermineError - Determines error between two.

inputs
   fp                fMaxEnergy - Maximum energy encountered
   PSRFEATURESMALL        pSRF1 - Item one
   PSRFEATURESMALL        pSRF2 - Item two
returns
   fp - Error value. Higher values mean more different
*/
#define UNVOICEDWEIGHT     12

fp VoiceChatDetermineError (fp fMaxEnergy, PSRFEATURESMALL pSRF1, PSRFEATURESMALL pSRF2)
{
   // return SRFEATURECompareAbsolute (pSRF1, pSRF2);

   // BUGFIX - Convert to SRFeatureSMALL, causing a blurring
   SRFEATURESMALL asr[2];
   asr[0] = *pSRF1;
   asr[1] = *pSRF2;

   // BUGFIX - Lower the volume of the unvoiced so that a difference in
   // voiced volume is much more important
   DWORD i;
   for (i = 0; i < SRDATAPOINTSSMALL; i++) {
      // NOTE: ignoring acNoiseEnergyDelta and acVoiceEnergyDelta
      if (asr[0].acNoiseEnergyMain[i] > SRABSOLUTESILENCE + UNVOICEDWEIGHT)
         asr[0].acNoiseEnergyMain[i] -= UNVOICEDWEIGHT;
      else
         asr[0].acNoiseEnergyMain[i] = SRABSOLUTESILENCE;

      if (asr[1].acNoiseEnergyMain[i] > SRABSOLUTESILENCE + UNVOICEDWEIGHT)
         asr[1].acNoiseEnergyMain[i] -= UNVOICEDWEIGHT;
      else
         asr[1].acNoiseEnergyMain[i] = SRABSOLUTESILENCE;
   } // i

   fp fEnergy1 = SRFEATUREEnergySmall (TRUE, &asr[0], FALSE, TRUE);
   fp fEnergy2 = SRFEATUREEnergySmall (TRUE, &asr[1], FALSE, TRUE);
   fp fCompare = SRFEATURECompareSmall (TRUE, &asr[0], /* fEnergy1,*/ &asr[1] /*, fEnergy2*/);
   fCompare = max(fCompare, 0);
   fCompare *= 80; // BUGFIX - Provide a 40 "db" range. BUGFIX - Was 40, changed to 80

   // compare the difference
   fp fDb;
   if (fEnergy1 && fEnergy2)
      fDb = log10(fEnergy1 / fEnergy2) * 20.0; // convert to db
   else
      fDb = 60;
   fDb = fabs(fDb);
   fDb = min (fDb, 20); // max 40 db penalty. BUGFIX - Changed to 20

   // the quieter it gets, the less important the comparison
   fp fMax = max(fEnergy1, fEnergy2);
   fMax = max(fMax, 1); // so at least some
   fMax = log10(fMax / fMaxEnergy) * 20.0;
   fMax += 20;    // so can have full range of 40 dB. BUGFIX - Max 20 dB
   fMax = max(fMax, 0);

   // POTENTIAL - this fMax test might be good to use for speech recogniton
   // since takes into account overall volume.
   // NOTE: THought about putting this into full SR but decided not to
   // since it would mostly help with silence, and I dont think thats an issue

   return (fCompare + fDb) * fMax;
}

/***************************************************************************
VoiceChatDetermineClosest - Looks through all the SR features and determines
which ones are most like one another. Those are grouped together.

inputs
   PSRFEATURESMALL        psrf - features
   DWORD             dwNum - Number
   DWORD             dwNumWant - At the end of the day, want the wave
                     to be composed of this many SRFEATUREs (which means
                     group dwNum-dwNumWant srfeatures into their neighbors
   PCLUSTERINFO      pci - Also dwNum elements. This is filled in with
                     bCount values indicating how often each unit is repeated
returns
   DWORD - New number of srfeatures after merges
*/
DWORD VoiceChatDetermineClosest (PSRFEATURESMALL psrf, DWORD dwNum, DWORD dwNumWant, PCLUSTERINFO pci)
{
   // fill in with initial values
   DWORD i;
   memset (pci, 0, sizeof(*pci)*dwNum);
   fp fMaxEnergy = 1;   // so not 0
   for (i = 0; i < dwNum; i++) {
      // max energy
      fp fEnergy = SRFEATUREEnergySmall (TRUE, psrf + i, FALSE, TRUE);
      fMaxEnergy = max(fMaxEnergy, fEnergy);
   } // i
   for (i = 0; i < dwNum; i++) {
      pci[i].dwCount = 1;
      pci[i].dwIndexRight = (i+1 < dwNum) ? (i+1) : (DWORD)-1;
      pci[i].dwIndexLeft = i ? (i-1) : (DWORD)-1;

      // skip if just before end
      if (i+1 >= dwNum)
         continue;

      // calc energy
      pci[i].fDiffFromRight = VoiceChatDetermineError (fMaxEnergy, psrf + i, psrf + (i+1));
      pci[i+1].fDiffFromLeft = pci[i].fDiffFromRight;

   } // i

   // repeat
   DWORD dwCombined = 0;
   while (dwNumWant + dwCombined < dwNum) {
      // find the one with the least difference to the left or right to combine
      fp fBestScore = 1000000000;
      DWORD dwBest = (DWORD)-1;
      BOOL fBestToRight = FALSE;
      for (i = 0; i < dwNum; i++) {
         // if already subsumed then ignore
         if (!pci[i].dwCount)
            continue;

         // if already have a high count > 3 then ignore
         if (pci[i].dwCount > 3)
            continue;

         // look to the right and see if decent score
         if ((pci[i].fDiffFromRight < fBestScore) && (pci[i].dwIndexRight != (DWORD)-1) &&
            (pci[pci[i].dwIndexRight].dwCount + pci[i].dwCount < 255) ) {
            fBestScore = pci[i].fDiffFromRight;
            dwBest = i;
            fBestToRight = TRUE;
         }

         // look left
         if ((pci[i].fDiffFromLeft < fBestScore) && (pci[i].dwIndexLeft != (DWORD)-1) &&
            (pci[pci[i].dwIndexLeft].dwCount + pci[i].dwCount < 255) ) {
            fBestScore = pci[i].fDiffFromLeft;
            dwBest = i;
            fBestToRight = FALSE;
         }
      }
      if (dwBest == (DWORD)-1)
         break;   // cant find anything to compress

      // else, merge in
      DWORD dwMergeInto = fBestToRight ? pci[dwBest].dwIndexRight : pci[dwBest].dwIndexLeft;
      pci[dwMergeInto].dwCount += pci[dwBest].dwCount;
      pci[dwBest].dwCount = 0; // since could only be one before
      dwCombined++;  // keep track number of times combined things
      if (fBestToRight) {
         pci[dwBest].dwIndexRight = dwMergeInto;
         pci[dwMergeInto].dwIndexLeft = dwBest; // BUGFIX - so causes new error determination... pci[dwBest].dwIndexLeft;
         // NOTE: Entries between dwBest and dwMergeInfo dont matter for left/right index storage
         // because those wont ever be referenced again, given will have dwCount=0
      }
      else {
         pci[dwBest].dwIndexLeft = dwMergeInto;
         pci[dwMergeInto].dwIndexRight = dwBest; // BUGFIX - so causes new error determination pci[dwBest].dwIndexRight;
      }

      // find any other features that used best as left or right and update
      for (i = 0; i < dwNum; i++) {
         if (!pci[i].dwCount)
            continue;
         if (pci[i].dwIndexRight == dwBest) {
            pci[i].dwIndexRight = pci[dwBest].dwIndexRight;
            if (pci[i].dwIndexRight != (DWORD)-1)
               pci[i].fDiffFromRight = VoiceChatDetermineError (fMaxEnergy, psrf + i, psrf + pci[i].dwIndexRight);
         }
         if (pci[i].dwIndexLeft == dwBest) {
            pci[i].dwIndexLeft = pci[dwBest].dwIndexLeft;
            if (pci[i].dwIndexLeft != (DWORD)-1)
               pci[i].fDiffFromLeft = VoiceChatDetermineError (fMaxEnergy, psrf + i, psrf + pci[i].dwIndexLeft);
         }
      } // i
   } // repeat

   // done
   return dwNum - dwCombined;
}


/***************************************************************************
VoiceChatDeCompressBlock - DeCompresses an invidual block.

inputs
   PVOICECHATBLOCK1  pBlock - Block to fill in. This could also be type 2.
   DWORD             dwBlockType - 1 for type 1, 2 for type 2
   PSRFEATURE        pSRF - data to decompress into
returns
   fp - Pitch, in Hz
*/
fp VoiceChatDeCompressBlock (PVOICECHATBLOCK1 pBlock, DWORD dwBlockType, PSRFEATURE pSRF)
{
   PVOICECHATBLOCK2 pBlock2 = (PVOICECHATBLOCK2) pBlock;
   PVOICECHATBLOCK3 pBlock3 = (PVOICECHATBLOCK3) pBlock;

   // fill in the energy per octave, as well as the limits
   DWORD i, dwCopyTo, dwNum;
   char acPeak[SROCTAVE];
   BYTE abNoise[SROCTAVE];
   PBYTE pbCur;
   DWORD dwPhases = 0;
   PBYTE pabPhases = NULL;
   BOOL fPhaseIsByte = FALSE;
   switch (dwBlockType) {
   case 1:
      dwCopyTo = 0;
      dwNum = VCB1_OCTAVES;
      pbCur = &pBlock->abOctave[0];
      dwPhases = VCB1_PHASES;
      pabPhases = &pBlock->abPhase[0];
      break;
   case 2:
      dwCopyTo = 1;
      dwNum = VCB2_OCTAVES;
      pbCur = &pBlock2->abOctave[0];
      break;
   case 3:
      dwCopyTo = 0;
      dwNum = VCB3_OCTAVES;
      pbCur = &pBlock3->abOctave[0];
      dwPhases = VCB3_PHASES;
      pabPhases = &pBlock3->abPhase[0];
      fPhaseIsByte = TRUE;
      break;
   default:
      return 0; // error
   } // dwBlockType
   memset (acPeak, 0, sizeof(acPeak));
   memset (abNoise, 0, sizeof(abNoise));
   for (i = 0; i < dwNum; i++, pbCur++) {
      int iCur = (int)pBlock->cPeak - (int)(char)(*pbCur >> 4)*OCTAVEMAXSCALE;
      iCur = max(iCur, -128);
      iCur = min(iCur, 127);
      acPeak[i+dwCopyTo] = (char)iCur;
      abNoise[i+dwCopyTo] = *pbCur & 0x0f;
   }

   DWORD dwStart, dwEnd;
   switch (dwBlockType) {
   case 1:  // block type 1
      pbCur = (PBYTE) (pBlock+1);
      dwStart = 0;
      dwEnd = sizeof(pBlock->abOctave) * SRPOINTSPEROCTAVE + dwStart;
      break;
   case 2:  // block type 2
      pbCur = (PBYTE) (pBlock2+1);
      dwStart = SRPOINTSPEROCTAVE;
      dwEnd = sizeof(pBlock2->abOctave) * SRPOINTSPEROCTAVE + dwStart;
      break;
   case 3:  // block type 3
      pbCur = (PBYTE) (pBlock3+1);
      dwStart = 0;
      dwEnd = sizeof(pBlock3->abOctave) * SRPOINTSPEROCTAVE + dwStart;
      break;
   default:
      return 0;  // shouldnt happen
   } // switch

   // figure out the boundaries
   DWORD adwBoundary[3];
   adwBoundary[0] = (dwStart *3 + dwEnd) / 4;
   adwBoundary[1] = (dwStart + dwEnd) / 2;
   adwBoundary[2] = (dwStart + dwEnd*3) / 4;

   // where to copy to
   char acEnergy[SRDATAPOINTS];
   for (i = 0; i < SRDATAPOINTS; i++)
      acEnergy[i] = 127;   // so know that haven't written

   DWORD dwFreq = dwStart;
   DWORD dwSize;
   int iAverage;
   BOOL fLowNibble = TRUE;
   while (dwFreq < dwEnd) {
      // figure out the size
      if (dwFreq < adwBoundary[0])
         dwSize = SIZESCALE(4, dwBlockType);
      else if (dwFreq < adwBoundary[1])
         dwSize = SIZESCALE(3, dwBlockType);
      else if (dwFreq < adwBoundary[2])
         dwSize = SIZESCALE(2, dwBlockType);
      else
         dwSize = SIZESCALE(1, dwBlockType);

      // write the value out
      if (fLowNibble) {
         iAverage = *pbCur & 0x0f;
         fLowNibble = FALSE;
      }
      else {
         iAverage = (*pbCur >> 4);
         fLowNibble = TRUE;
         pbCur++;
      }


      // new value
      iAverage = (int)acPeak[dwFreq / SRPOINTSPEROCTAVE] - iAverage*2;
      iAverage = max(iAverage, -128);
      iAverage = min(iAverage, 126);   // dont let get to 127

      // what's the location to write into?
      DWORD dwWriteTo = dwFreq + (dwSize/2);

      // wite it
      if (dwWriteTo < SRDATAPOINTS)
         acEnergy[dwWriteTo] = (char)iAverage;

      // increase
      dwFreq += dwSize;
   } // dwFreq < dwEnd

   // loop through an fill in gaps
   DWORD dwLast = (DWORD)-1;
   DWORD j;
   for (i = 0; i < SRDATAPOINTS; i++) {
      if (acEnergy[i] == 127)
         continue;   // nothing here

      // else found point... if no delta between now and last then skip
      if (i == dwLast+1) {
         dwLast = i;
         continue;
      }

      // if no last then fill all values till now with that
      if (dwLast == (DWORD)-1) {
         for (j = 0; j < i; j++)
            acEnergy[j] = acEnergy[i];
         dwLast = i;
         continue;
      }

      // else, need to blend
      for (j = dwLast+1; j < i; j++) {
         // BUGFIX - Do in energy, not dB
         iAverage = DbToAmplitude(acEnergy[dwLast]) * (int)(i-j) + DbToAmplitude(acEnergy[i]) * (int)(j-dwLast);
         iAverage /= (int)(i - dwLast);
         acEnergy[j] = AmplitudeToDb(iAverage);
      } // j
      dwLast = i;

   } // i
   for (i = dwLast+1; i < SRDATAPOINTS; i++)
      acEnergy[i] = SRABSOLUTESILENCE;  // zero out

   // set the phases.. and blank out everything
   //memset (pSRF->abPhase, 0, sizeof(pSRF->abPhase));
   memset (pSRF, 0, sizeof(*pSRF));

   // BUGFIX - random phase
   switch (dwBlockType) {
   case 1:
   case 2:
   default:
      for (i = VCB1_PHASES; i < SRPHASENUM; i++)
         pSRF->abPhase[i] = (BYTE)rand();
      break;
   case 3:  // higher quality
      for (i = VCB3_PHASES; i < SRPHASENUM; i++)
         pSRF->abPhase[i] = (BYTE)rand();
      break;
   }

   for (i = 0; i < dwPhases; i++) {
      BYTE bPhase;
      if (fPhaseIsByte) {
         bPhase = *pabPhases;
         pabPhases++;
      }
      else if (i % 2) {
         bPhase = *pabPhases & 0xf0;
         pabPhases++;
      }
      else
         bPhase = *pabPhases << 4;

      pSRF->abPhase[i+1] = bPhase;
   } // i

   // fill in the srdatapoint
   for (i = 0; i < SRDATAPOINTS; i++) {
      fp fBlend = (fp)i / (fp)SRPOINTSPEROCTAVE - 0.5;
      int iLow = floor(fBlend);
      int iHigh = iLow+1;
      iLow = max(iLow,0);
      iLow = min(iLow,(int)SROCTAVE-1);
      iHigh = max(iHigh,0);
      iHigh = min(iHigh,(int)SROCTAVE-1);
      fBlend -= (fp)iLow;
      fp fNoiseRatio = (fp)(int)abNoise[iLow] * (1.0-fBlend) + (fp)(int)abNoise[iHigh] * fBlend;
      fNoiseRatio /= 15.0;

      fp fTotal = DbToAmplitude (acEnergy[i]);
      pSRF->acVoiceEnergy[i] = AmplitudeToDb (fTotal * (1.0 - fNoiseRatio));
      pSRF->acNoiseEnergy[i] = AmplitudeToDb (fTotal * fNoiseRatio);
   } // i

   return exp ((fp)(int)pBlock->cPitch * (fp)(log((fp)2) / 48.0)) * VCH_BASEFREQ;
}

/***************************************************************************
VoiceChatCompressBlock - Compresses an invidual block.

inputs
   fp                fPitch - pitch
   PSRFEATURE        pSRF - data to compress
   PVOICECHATBLOCK1  pBlock - Block to fill in. This could also be type 2.
   DWORD             dwBlockType - 1 for type 1, 2 for type 2, 3 for type 3
returns
   none
*/
void VoiceChatCompressBlock (fp fPitch, PSRFEATURE pSRF, PVOICECHATBLOCK1 pBlock,
                             DWORD dwBlockType)
{

   DWORD i;
#if 0 // to test
   SRFEATURE srfTemp;
   pSRF = &srfTemp;
   memset (&srfTemp, 0, sizeof(srfTemp));
   for (i = 0; i < SRDATAPOINTS; i++) {
      srfTemp.acNoiseEnergy[i] = -128;
      srfTemp.acVoiceEnergy[i] = (i % 6) ? -128 : -20;
   } // i
#endif

   PVOICECHATBLOCK2 pBlock2 = (PVOICECHATBLOCK2) pBlock;
   PVOICECHATBLOCK3 pBlock3 = (PVOICECHATBLOCK3) pBlock;
   PBYTE pbCur;
   DWORD dwStart, dwEnd;
   DWORD dwPhases = 0;
   PBYTE pabPhases = NULL;
   BOOL fPhaseIsByte = FALSE;
#ifdef _DEBUG
   DWORD dwLeft;
#endif
   switch (dwBlockType) {
   case 1:  // block type 1
      pbCur = (PBYTE) (pBlock+1);
      dwStart = 0;
      dwEnd = sizeof(pBlock->abOctave) * SRPOINTSPEROCTAVE + dwStart;
      dwPhases = VCB1_PHASES;
      pabPhases = &pBlock->abPhase[0];
#ifdef _DEBUG
      dwLeft = SIZEOFVOICECHATBLOCK1 - sizeof(*pBlock);
#endif
      break;

   case 2:  // block type 2
      pbCur = (PBYTE) (pBlock2+1);
      dwStart = SRPOINTSPEROCTAVE;
      dwEnd = sizeof(pBlock2->abOctave) * SRPOINTSPEROCTAVE + dwStart;
#ifdef _DEBUG
      dwLeft = SIZEOFVOICECHATBLOCK2 - sizeof(*pBlock2);
#endif
      break;

   case 3:  // block type 3, high quality
      pbCur = (PBYTE) (pBlock3+1);
      dwStart = 0;
      dwEnd = sizeof(pBlock3->abOctave) * SRPOINTSPEROCTAVE + dwStart;
      dwPhases = VCB3_PHASES;
      pabPhases = &pBlock3->abPhase[0];
      fPhaseIsByte = TRUE;
#ifdef _DEBUG
      dwLeft = SIZEOFVOICECHATBLOCK3 - sizeof(*pBlock3);
#endif
      break;
   default:
      return;  // shouldnt happen
   } // switch

   // figure out the boundaries
   DWORD adwBoundary[3];
   adwBoundary[0] = (dwStart *3 + dwEnd) / 4;
   adwBoundary[1] = (dwStart + dwEnd) / 2;
   adwBoundary[2] = (dwStart + dwEnd*3) / 4;

   DWORD dwFreq = dwStart;
   DWORD dwSize;
   int iAverage;
   
   // determine the energy to be placed in each bin
   char acEnergies[SRDATAPOINTS];
   DWORD dwNumEnergies = 0;
   char acPeak[SROCTAVE];  // peak in each octave
   for (i = 0; i < SROCTAVE; i++)
      acPeak[i] = -128;
   // convert to energy and figure out how much energy is in each octave
   fp afEnergy[2][SROCTAVE];     // [x][y], x = 0 for voiced, 1 for unvoiced, y are the octave numbers
   memset (afEnergy, 0, sizeof(afEnergy));
   char cMax = -128;
   while (dwFreq < dwEnd) {
      // figure out the size
      if (dwFreq < adwBoundary[0])
         dwSize = SIZESCALE(4, dwBlockType);
      else if (dwFreq < adwBoundary[1])
         dwSize = SIZESCALE(3, dwBlockType);
      else if (dwFreq < adwBoundary[2])
         dwSize = SIZESCALE(2, dwBlockType);
      else
         dwSize = SIZESCALE(1, dwBlockType);

      DWORD dwOctToUse = dwFreq / SRPOINTSPEROCTAVE;

      // get value
      if (dwFreq + dwSize - 1 < dwEnd) {
         // BUGFIX - Wasn't converting to energy before averageing, which was wrong
         DWORD dwCount;
         iAverage = 0;
         for (dwCount = 0; dwCount < dwSize; dwCount++) {
            // individual energies
            fp fVoiced = DbToAmplitude (pSRF->acVoiceEnergy[dwFreq + dwCount]);
            fp fNoise = DbToAmplitude (pSRF->acNoiseEnergy[dwFreq + dwCount]);

            afEnergy[0][dwOctToUse] += fVoiced;
            afEnergy[1][dwOctToUse] += fNoise;

            iAverage += (int)(fVoiced + fNoise);
         }
         iAverage /= (int) dwCount;
         iAverage = AmplitudeToDb (iAverage);
      }
      else
         iAverage = SRABSOLUTESILENCE;  // beyond range


      // store this
      acEnergies[dwNumEnergies] = (char)iAverage;

      // store the peak away
      acPeak[dwOctToUse] = max(acPeak[dwOctToUse], acEnergies[dwNumEnergies]);
      cMax = max(cMax, acEnergies[dwNumEnergies]);

      // remember how many have
      dwNumEnergies++;

      // increase
      dwFreq += dwSize;
   } // while dwFreq < dwEnd



   // loop and figure out ratio of noise to voiced, as well as max per octave
   BYTE abOctave[SROCTAVE];
   for (i = 0; i < SROCTAVE; i++) {
      // determine how much energy
      if (!afEnergy[1][i])
         abOctave[i] = 0;  // all voiced
      else if (!afEnergy[0][i])
         abOctave[i] = 15; // all noise
      else {
         fp fSum = afEnergy[0][i] + afEnergy[1][i];
         fp fRatio = afEnergy[1][i] / fSum;
         fRatio = fRatio * 15.0 + 0.5;
         fRatio = max(fRatio, 0);
         fRatio = min(fRatio, 15);
         abOctave[i] = (BYTE)fRatio;
      }

      // determine how much this is below the peak value
      int iBelow = ((int)cMax - (int)acPeak[i]) / OCTAVEMAXSCALE;   // scaling of dB
      iBelow = max(iBelow, 0);
      iBelow = min(iBelow, 15);
      abOctave[i] |= ((BYTE)iBelow << 4);

      // store this back in
      acPeak[i] = cMax - (char)(iBelow*OCTAVEMAXSCALE);
   } // i

   // fill in the structure
   fPitch = max(fPitch, 1);
   fp f = log((fp)fPitch / (fp)VCH_BASEFREQ) / (log((fp)2) / 48.0);
   f = max(f, -127);
   f = min(f, 127);
   pBlock->cPeak = cMax;
   pBlock->cPitch = (char)(int)f;
   switch (dwBlockType) {
   case 1:  // block type 1
      memcpy (pBlock->abOctave, abOctave, sizeof(pBlock->abOctave));
      break;
   case 2:  // block type 2
      memcpy (pBlock2->abOctave, abOctave+1, sizeof(pBlock2->abOctave));
      break;
   case 3:  // block type 3
      memcpy (pBlock3->abOctave, abOctave, sizeof(pBlock3->abOctave));
      break;
   default:
      return;  // shouldnt happen
   } // switch

   // fill in the phases
   for (i = 0; i < dwPhases; i++) {
      BYTE bPhase = pSRF->abPhase[i+1];   // skip 0 since always 0
      if (fPhaseIsByte) {
         *pabPhases = bPhase;
         pabPhases++;
      }
      else { // nibble
         bPhase += (1<<4)/2;   // BUGFIX: to round off
         bPhase = bPhase >> 4;
         if (i % 2) {
            *pabPhases = *pabPhases | (bPhase << 4);
            pabPhases++;
         }
         else
            *pabPhases = bPhase;
      }
   } // i

   dwFreq = dwStart;
   BOOL fLowNibble = TRUE;
   for (i = 0; (i < dwNumEnergies) && (dwFreq < dwEnd); i++) {  // intentional test in middle
      // figure out the size
      if (dwFreq < adwBoundary[0])
         dwSize = SIZESCALE(4, dwBlockType);
      else if (dwFreq < adwBoundary[1])
         dwSize = SIZESCALE(3, dwBlockType);
      else if (dwFreq < adwBoundary[2])
         dwSize = SIZESCALE(2, dwBlockType);
      else
         dwSize = SIZESCALE(1, dwBlockType);

      // get value
      iAverage = acEnergies[i];

      // do the delta
      iAverage = ((int)acPeak[dwFreq / SRPOINTSPEROCTAVE] - iAverage)/2;
      iAverage = max(iAverage, 0);
      iAverage = min(iAverage, 15);

      // write the value out
      if (fLowNibble) {
         *pbCur = (BYTE)iAverage;
         fLowNibble = FALSE;
      }
      else {
         *pbCur = *pbCur | ((BYTE)iAverage << 4);
         fLowNibble = TRUE;
         pbCur++;
#ifdef _DEBUG
         dwLeft--;
#endif
      }

      // increase
      dwFreq += dwSize;
   };
#ifdef _DEBUG
   if (!fLowNibble)
      dwLeft--;
#endif

   // make sure nothing left, and haven't gone negative
   _ASSERTE (dwLeft == 0);
}


/***************************************************************************
VoiceChatCompress - Compresses a wave.

inputs
   DWORD             dwQuality - One of VCH_ID_XXX
   PCM3DWave         pWave - Wave. This data is modified somehwhat as the
                     remove background noise and remove sub-voice frequenceis
                     are called. This should be mono, ideally 22 kHz.
   PCVoiceDisguise   pVoiceDisguise - Voice disguise used. The non-harmonic
                     setting and wave are gotten from here. Can be NULL.
                     This will also apply the voice disguise to the voice.
   PCMem             pMem - Filled with compressed wave, starting at
                     m_dwCurPosn.
returns
   BOOL - TRUE if success
*/
BOOL VoiceChatCompress (DWORD dwQuality, PCM3DWave pWave, PCVoiceDisguise pVoiceDisguise, PCMem pMem)
{
#if 0 // def _DEBUG  // to test
   static iFile = 0;
   char szTemp[256];
   sprintf (szTemp, "c:\\VoiceChatCompress%.3d.wav", iFile);
   HMMIO hmmio = mmioOpen (szTemp, NULL, MMIO_WRITE | MMIO_CREATE | MMIO_EXCLUSIVE );
   pWave->Save (FALSE, NULL, hmmio);
   mmioClose (hmmio, 0);
   iFile++;
#endif

   // force voice chat to 100 samples per sec, not 200
   if (pWave->m_dwSRSAMPLESPERSEC != VOICECHATSAMPLESPERSEC) {
      pWave->m_dwSRSAMPLESPERSEC = VOICECHATSAMPLESPERSEC;
      pWave->m_dwSRSamples = 0;
      memset (pWave->m_adwPitchSamples, 0, sizeof(pWave->m_adwPitchSamples));
   }

   // reduce sub-voice frequenceis
   pWave->FXRemoveDCOffset (TRUE, NULL);
   // BUGFIX - No noise reduction, since may be causing problems... pWave->FXNoiseReduce (NULL);

   // calculate the pitch and SR features
   if (!pWave->m_adwPitchSamples[PITCH_F0])
      pWave->CalcPitch (WAVECALC_VOICECHAT, NULL);  // BUGFIX - Dont use fast pitch since not accurate enough
   if (!pWave->m_dwSRSamples)
      pWave->CalcSRFeatures (WAVECALC_VOICECHAT, NULL);   // BUGFIX - not doing fast SR now
   if (!pWave->m_dwSRSamples)
      return FALSE;  // nothing to compress

   DWORD dwBlockSize, dwBlockType;
   DWORD dwBlocksWant;
   BOOL fFastPitchSR;
   switch (dwQuality) {
   case VCH_ID_VERYBEST:
      dwBlockType = 3;
      dwBlockSize = SIZEOFVOICECHATBLOCK3;
      fFastPitchSR = FALSE;
      dwBlocksWant = pWave->m_dwSRSamples * 2 / 3;
      break;
   case VCH_ID_BEST:
      dwBlockType = 1;
      dwBlockSize = SIZEOFVOICECHATBLOCK1;
      fFastPitchSR = FALSE;
      dwBlocksWant = pWave->m_dwSRSamples * 2 / 3;
      break;
   case VCH_ID_MED:
      dwBlockType = 1;
      dwBlockSize = SIZEOFVOICECHATBLOCK1;
      fFastPitchSR = TRUE;
      dwBlocksWant = pWave->m_dwSRSamples / 2;
      break;
   case VCH_ID_LOW:
      dwBlockType = 2;
      dwBlockSize = SIZEOFVOICECHATBLOCK2;
      fFastPitchSR = TRUE;
      dwBlocksWant = pWave->m_dwSRSamples / 2;
      break;
   case VCH_ID_VERYLOW:
      dwBlockType = 2;
      dwBlockSize = SIZEOFVOICECHATBLOCK2;
      fFastPitchSR = TRUE;
      dwBlocksWant = pWave->m_dwSRSamples / 4;
      break;
   default:
      return FALSE; // error
   } // switch quality
   dwBlocksWant = max(dwBlocksWant, 1);

   // calculate the voice disguise
   if (pVoiceDisguise)
      pVoiceDisguise->ModifySRFEATUREInWave (pWave, NULL, NULL);

   // BUGFIX - Convert to SRFEATURESMALL and blur left/right a bit
   CMem memBlur;
   if (!memBlur.Required (pWave->m_dwSRSamples * sizeof(SRFEATURESMALL)))
      return FALSE;
   PSRFEATURESMALL pSmall = (PSRFEATURESMALL)memBlur.p;
   SRFEATURE srTemp;
   fp afWeight[3];
   DWORD i,j;
   for (i = 0; i < pWave->m_dwSRSamples; i++) {
      // find range
      int iMin = (int)i-1;
      int iMax = (int)i+2;
      if (iMin < 0)
         iMin = 0;
      iMax = min(iMax, (int)pWave->m_dwSRSamples);
      DWORD dwNum = (DWORD)(iMax - iMin);

      fp fWeight = 1.0 / (fp)dwNum;
      for (j = 0; j < dwNum; j++)
         afWeight[j] = fWeight;
      SRFEATUREInterpolate (pWave->m_paSRFeature + iMin, dwNum, afWeight, &srTemp);

      // convert to SRFEATURESMALL
      SRFEATUREConvert (&srTemp, NULL, &pSmall[i]);
   } // i

   // loop through all blocks and find closest matches
   CMem memCluster;
   if (!memCluster.Required (pWave->m_dwSRSamples * sizeof(CLUSTERINFO)))
      return FALSE;
   PCLUSTERINFO pci = (PCLUSTERINFO) memCluster.p;
   DWORD dwBlocks = VoiceChatDetermineClosest (pSmall, pWave->m_dwSRSamples, dwBlocksWant, pci);
   if (!dwBlocks)
      return FALSE;  // shouldnt happen
   if (dwBlocks > 0xffff)
      return FALSE;  // too large

   // figure out how much will need
   PWSTR pszWave = pVoiceDisguise ? (PWSTR)pVoiceDisguise->m_memWaveBase.p : L"";
   DWORD dwLen = (DWORD)wcslen(pszWave);
   DWORD dwNeed = sizeof(VOICECHATHDR) + (dwLen+1)*sizeof(WCHAR) + dwBlocks * dwBlockSize;
   dwNeed = (dwNeed + 3) & ~0x03;   // DWORD align
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return FALSE;  // cant allocate
   PVOICECHATHDR pVCH = (PVOICECHATHDR) ((PBYTE)pMem->p + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwNeed;

   // fill in the header
   pVCH->dwSize = dwNeed;
   pVCH->wBlockSize = (WORD)dwBlockSize;
   pVCH->wID = (WORD)dwQuality;
   pVCH->wNumBlocks = (WORD)dwBlocks;

   if (pVoiceDisguise) {
      fp f = pVoiceDisguise->m_fNonIntHarmonics * 64.0;
      f = max(f, 0);
      f = min(f,255);
      pVCH->bNonIntHarmonics = (BYTE)f;

      f = pVoiceDisguise->m_fWaveBasePitch;
      if (f > 0) {
         f = log(pVoiceDisguise->m_fWaveBasePitch / (fp)VCH_BASEFREQ) / (log((fp)2) / 48.0);
         f = max(f, -127);
         f = min(f, 127);
      }
      else
         f = -128;
      pVCH->cPitchBase = (char)(int)f;
   }
   else {
      pVCH->bNonIntHarmonics = 64;
      pVCH->cPitchBase = -128;
   }

   // copy the string
   PWSTR pszCopyWaveName = (PWSTR) (pVCH + 1);
   memcpy (pszCopyWaveName, pszWave, (dwLen+1)*sizeof(WCHAR));

   // more data
   PBYTE pData = (PBYTE) (pszCopyWaveName + (dwLen+1));

   DWORD dwCur;
#ifdef _DEBUG
   DWORD dwTotal = 0, dwUsed = 0;
   for (i = 0; i < pWave->m_dwSRSamples; i++) {
      dwTotal += pci[i].dwCount;
      if (pci[i].dwCount)
         dwUsed++;
   }
#endif

   // loop over all the blocks and compress each block
   for (i = dwCur = 0; i < pWave->m_dwSRSamples; i++) {
      if (!pci[i].dwCount)
         continue;   // skipped

      // else, copy this
      fp fPitch = pWave->PitchAtSample (PITCH_F0, i * pWave->m_dwSRSkip, 0);
      fPitch = max(fPitch, 50);
         // BUGFIX - absolute minimum pitch, otherwise unvoiced "s" at end of voice chat doesn't
         // work properly
      VoiceChatCompressBlock (fPitch, pWave->m_paSRFeature + i, (PVOICECHATBLOCK1) pData, dwBlockType);
      ((PVOICECHATBLOCK1)pData)->bCopies = (BYTE)pci[i].dwCount;

      // increase
      pData += dwBlockSize;
      dwCur++;
   } // i

   return TRUE;
}


/***************************************************************************
VoiceChatInterpolate - Interpolates all the points between one SRFEature and
the next

inputs
   PCM3DWave         pWave - Wave
   PSRFEATURE        pLastSRF - Last SR feature. Can be NULL
   DWORD             dwLast - Last index. Can be (DWORD)-1
   fp                fLastPitch - Last pitch.
   PSRFEATURE        pCurSRF - Current SR feature. Can be NULL;
   DWORD             dwCur - Current one. CANNOT be -1. Can be m_dwNumSRFEatures
   fp                fCurPitch - Current pitch
returns
   none
*/
void VoiceChatInterpolate (PCM3DWave pWave,
                           PSRFEATURE pLastSRF, DWORD dwLast, fp fLastPitch,
                           PSRFEATURE pCurSRF, DWORD dwCur, fp fCurPitch)
{
   // if no last then use current
   if (!pLastSRF) {
      dwLast = (DWORD)-1;  // to make sure
      fLastPitch = fCurPitch;
      pLastSRF = pCurSRF;

      if (!pLastSRF)
         return;  // erorr, cant go from nothing to nothing
   }
   if (!pCurSRF) {
      pCurSRF = pLastSRF;
      fCurPitch = fLastPitch;
      if (!pCurSRF)
         return;  // error, cant go from nothing to nothing
   }

   // loop
   DWORD j;
   // else, blend
   for (j = dwLast+1; j < dwCur; j++) {
      fp fAlpha = (fp)(dwCur-j) / (fp)(dwCur - dwLast);

      SRFEATUREInterpolate (pCurSRF, pLastSRF, 1.0 - fAlpha, pWave->m_paSRFeature + j);

#ifdef _DEBUG
      pWave->m_paSRFeature[j].abPhase[0] = 55;
#endif

      // pitch
      if (j < pWave->m_adwPitchSamples[PITCH_F0]) {
         pWave->m_apPitch[PITCH_F0][j].fFreq = fCurPitch * (1.0 - fAlpha) + fLastPitch * fAlpha;
         pWave->m_apPitch[PITCH_F0][j].fStrength = 1;
      }
   } // j
}


/***************************************************************************
VoiceChatDeCompress - Decompresses to a wave.

inputs
   PBYTE             pbData - Data to use
   DWORD             dwSize - Number of bytes available
   PCM3DWave         pWave - This wave is wiped out and filled in with data.
                        If this is NULL then just doing an integrity check.
                        The wave is automatically converted to 22 kHz
   PCVoiceDisguise   pVoiceDisguise - Voice disguise used. The non-harmonic
                     setting and wave are written. Can be NULL.
   PCListVariable    plValidWaves - This is a list of valid wave files. If the
                     given wavefile is not empty-string and it's NOT on this
                     list then an error is returned. If this is NULL then
                     it's not checked.
   PCProgressWaveSample pProgressWave - This is an accurate progress bar used
                  so that will be able to play out TTS while it's being generated.
returns
   DWORD - Number of bytes of data to use. Returns 0 if error
*/
DWORD VoiceChatDeCompress (PBYTE pbData, DWORD dwSize, PCM3DWave pWave, PCVoiceDisguise pVoiceDisguise,
                           PCListVariable plValidWaves, PCProgressWaveSample pProgressWave)
{
   // force voice chat to 100 samples per sec, not 200
   if (pWave && (pWave->m_dwSRSAMPLESPERSEC != VOICECHATSAMPLESPERSEC)) {
      pWave->m_dwSRSAMPLESPERSEC = VOICECHATSAMPLESPERSEC;
      pWave->m_dwSRSamples = 0;
      memset (pWave->m_adwPitchSamples, 0, sizeof(pWave->m_adwPitchSamples));
   }

   // make sure have enough memory
   if (dwSize < sizeof(VOICECHATHDR) + sizeof(WCHAR))
      return 0;
   PVOICECHATHDR pVCH = (PVOICECHATHDR)pbData;
   if ( (pVCH->dwSize > dwSize) || (pVCH->dwSize < sizeof(VOICECHATHDR) + sizeof(WCHAR)) )
      return 0;

   DWORD dwBlockSize, dwBlockType;
   switch (pVCH->wID) {
   case VCH_ID_VERYBEST:
      dwBlockType = 3;
      dwBlockSize = SIZEOFVOICECHATBLOCK3;
      break;
   case VCH_ID_BEST:
   case VCH_ID_MED:
      dwBlockType = 1;
      dwBlockSize = SIZEOFVOICECHATBLOCK1;
      break;
   case VCH_ID_LOW:
   case VCH_ID_VERYLOW:
      dwBlockType = 2;
      dwBlockSize = SIZEOFVOICECHATBLOCK2;
      break;
   default:
      return 0;   // error
   } // switch ID

   // veritfy block size
   if ((DWORD)pVCH->wBlockSize != dwBlockSize)
      return 0;

   // get the string and make sure it's null terminated
   PWSTR pszWave = (PWSTR)(pVCH + 1);
   PWSTR pszCur;
   int iLeft = (int)pVCH->dwSize - (int)sizeof(VOICECHATHDR);
   for (pszCur = pszWave; (iLeft >= sizeof(WCHAR)) && *pszCur; pszCur++, iLeft -= sizeof(WCHAR));
   if (iLeft < sizeof(WCHAR))
      return 0;   // error
   // else, know have the end
   pszCur++;
   iLeft -= sizeof(WCHAR);

   // data begins from here
   PBYTE pbCur = (PBYTE)pszCur;
   if ((DWORD)iLeft < dwBlockSize * (DWORD)pVCH->wNumBlocks)
      return 0;   // error

   // if there's no blocks then error
   if (!pVCH->wNumBlocks)
      return 0;

   // calc total number of blocks
   DWORD dwFinalNumBlocks = 0;
   pbCur = (PBYTE)pszCur;
   DWORD i;
   for (i = 0; i < (DWORD)pVCH->wNumBlocks; i++, pbCur += pVCH->wBlockSize) {
      BYTE bCopies = ((PVOICECHATBLOCK1)pbCur)->bCopies;
      if (!bCopies)
         return FALSE;  // error
      
      dwFinalNumBlocks += (DWORD)bCopies;
   }
   pbCur = (PBYTE)pszCur;

   // may need to validate that wave file is valid name
   if (plValidWaves && pszWave[0]) {
      for (i = 0; i < plValidWaves->Num(); i++)
         if (!_wcsicmp (pszWave, (PWSTR)plValidWaves->Get(i)))
            break;
      if (i >= plValidWaves->Num())
         return 0;   // error
   }

   // if there's no wave then integrity check is done
   if (!pWave)
      return pVCH->dwSize;

   // how many samples will we need
   DWORD dwBlocks = dwFinalNumBlocks;

   // clear out
   pWave->BlankWaveToSize (0, TRUE);
   pWave->ConvertSamplesAndChannels (22050, 1, NULL); // 22 kHz
   DWORD dwSRSkip = pWave->m_dwSamplesPerSec / pWave->m_dwSRSAMPLESPERSEC;  // 1/100th of a second;
   DWORD dwSamples = dwSRSkip * (dwBlocks-1);
   pWave->BlankWaveToSize (dwSamples, TRUE);

   _ASSERTE (dwBlocks == pWave->m_dwSRSamples);
   _ASSERTE (dwBlocks == pWave->m_adwPitchSamples[PITCH_F0]);

   // fill in the features and pitch
   PSRFEATURE psrfLast = NULL;
   DWORD dwLast = (DWORD)-1;
   fp fPitchLast = 0;
   DWORD dwLoc = 0;
   for (i = 0; i < (DWORD)pVCH->wNumBlocks; i++, pbCur += dwBlockSize) {
      // figure out where to place this... generally in the center
      PVOICECHATBLOCK1 pVCB = (PVOICECHATBLOCK1) pbCur;
      DWORD dwFeature = dwLoc + (DWORD)pVCB->bCopies/2;
      PSRFEATURE psrfThis = &pWave->m_paSRFeature[dwFeature];

      fp fPitch = VoiceChatDeCompressBlock (pVCB, dwBlockType,
         &pWave->m_paSRFeature[dwFeature]);

      // write the pitch
      if (dwFeature < pWave->m_adwPitchSamples[PITCH_F0]) {
         pWave->m_apPitch[PITCH_F0][dwFeature].fFreq = fPitch;
         pWave->m_apPitch[PITCH_F0][dwFeature].fStrength = 1;
      }


      // interpolate
      VoiceChatInterpolate (pWave, psrfLast, dwLast, fPitchLast,
         psrfThis, dwFeature, fPitch);

      // remember last
      fPitchLast = fPitch;
      psrfLast = psrfThis;
      dwLast = dwFeature;
      dwLoc += (DWORD)pVCB->bCopies;
   } // i

   // interpolate to the end
   if (dwLast+1 < pWave->m_dwSRSamples)
      VoiceChatInterpolate (pWave, psrfLast, dwLast, fPitchLast,
         NULL, pWave->m_dwSRSamples, 0);


   // disguise
   BOOL fCreated = FALSE;
   if (!pVoiceDisguise) {
      pVoiceDisguise = new CVoiceDisguise;
      if (!pVoiceDisguise)
         return 0; // error
      fCreated = TRUE;
   }
   
   // fill in the harmonics, etc.
   pVoiceDisguise->m_fNonIntHarmonics = (fp)pVCH->bNonIntHarmonics / 64.0;
   if (pVCH->cPitchBase == -128)
      pVoiceDisguise->m_fWaveBasePitch = 0;
   else
      pVoiceDisguise->m_fWaveBasePitch = exp ((fp)(int)pVCH->cPitchBase * (log((fp)2) / (fp)48.0)) * VCH_BASEFREQ;
   pszCur = (PWSTR)pVoiceDisguise->m_memWaveBase.p;
   if (_wcsicmp(pszCur, pszWave)) {
      // new wave
      if (pVoiceDisguise->m_pWaveCache) {
         // since just cleared wavebase
         WaveCacheRelease (pVoiceDisguise->m_pWaveCache);
         pVoiceDisguise->m_pWaveCache = NULL;
      }

      MemZero (&pVoiceDisguise->m_memWaveBase);
      MemCat (&pVoiceDisguise->m_memWaveBase, pszWave);
      // else, will be 0
   }

   // figure out phonemes
   // BUGFIX - Move ahead so that will get phonemes done before sends audio out
   VisemeFillInPhones (pWave);

   // NOTE: Enabling PCM here, even though shouldn't for whisper, but doesn't
   // matter since PCM not transmitted
   BOOL fRet = pVoiceDisguise->SynthesizeFromSRFeature (4, pWave, NULL, 0, NULL, NULL, NULL, TRUE, NULL,
#ifdef _DEBUG
         FALSE
#else
         TRUE
#endif
         , TRUE, pProgressWave);
      // NOTE: Skipping modification because assume it was already diguised
      // NOTE: clearing sr feature at the moment
   if (fCreated)
      delete pVoiceDisguise;

#if 0// def _DEBUG  // to test
   static iFile = 0;
   char szTemp[256];
   sprintf (szTemp, "c:\\VoiceChatDeCompress%.3d.wav", iFile);
   HMMIO hmmio = mmioOpen (szTemp, NULL, MMIO_WRITE | MMIO_CREATE | MMIO_EXCLUSIVE );
   pWave->Save (FALSE, NULL, hmmio);
   mmioClose (hmmio, 0);
   iFile++;
#endif

   return fRet ? pVCH->dwSize : 0;
}

/***************************************************************************
VoiceChatWhisperify - Converts voice to whisper.

inputs
   PBYTE             pbData - Data to use
   DWORD             dwSize - Number of bytes available
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL VoiceChatWhisperify (PBYTE pbData, DWORD dwSize)
{
   // verify that valid
   if (!VoiceChatDeCompress (pbData, dwSize, NULL, NULL, NULL))
      return FALSE;

   // find actual data

   // make sure have enough memory
   PVOICECHATHDR pVCH = (PVOICECHATHDR)pbData;

   DWORD dwBlockType;
   switch (pVCH->wID) {
   case VCH_ID_VERYBEST:
      dwBlockType = 3;
      break;
   case VCH_ID_BEST:
   case VCH_ID_MED:
      dwBlockType = 1;
      break;
   case VCH_ID_LOW:
   case VCH_ID_VERYLOW:
      dwBlockType = 2;
      break;
   default:
      return FALSE;   // error
   } // switch ID

   // get the string and make sure it's null terminated
   PWSTR pszWave = (PWSTR)(pVCH + 1);
   DWORD dwLen = (DWORD)wcslen(pszWave);   // since know its valid

   // data begins from here
   PBYTE pbCur = (PBYTE)(pszWave + (dwLen+1));
   DWORD i, j;
   int aiMax[SROCTAVE];
   for (i = 0; i < (DWORD) pVCH->wNumBlocks; i++, pbCur += pVCH->wBlockSize) {
      PVOICECHATBLOCK1 pBlock = (PVOICECHATBLOCK1) pbCur;
      PVOICECHATBLOCK2 pBlock2 = (PVOICECHATBLOCK2) pbCur;
      PVOICECHATBLOCK3 pBlock3 = (PVOICECHATBLOCK3) pbCur;

      PBYTE pb;
      DWORD dwNum, dwOffset;
      switch (dwBlockType) {
      case 1:
         pb = &pBlock->abOctave[0];
         dwNum = sizeof(pBlock->abOctave);
         dwOffset = 0;
         break;
      case 2:
         pb = &pBlock2->abOctave[0];
         dwNum = sizeof(pBlock2->abOctave);
         dwOffset = 1;
         break;
      case 3:
         pb = &pBlock3->abOctave[0];
         dwNum = sizeof(pBlock3->abOctave);
         dwOffset = 0;
         break;
      default:
         return FALSE;
      }

      DWORD dwWhisperStart = SROCTAVE-4 - dwOffset;
      DWORD dwWhisperStop = SROCTAVE-2 - dwOffset;
      int iNewPeak = SRABSOLUTESILENCE;
      for (j = 0; j < dwNum; j++) {
         // figure out the voiced and unvoiced energy
         char cBelow = -(char)(pb[j]>>4)*OCTAVEMAXSCALE;
         fp fUnvoicedWeight = (fp)(pb[j]&0x0f) / 15.0;
         fp fSum = DbToAmplitude (cBelow);
         fp fVoiced = fSum * (1.0 - fUnvoicedWeight);
         fp fNoise = fSum * fUnvoicedWeight;

         // convert from voiced to noise
         if (j >= dwWhisperStop)
            fNoise += fVoiced;
         else if (j >= dwWhisperStart) {
            fp fAlpha = ((fp)(j - dwWhisperStart)+0.5) / (fp)(dwWhisperStop-dwWhisperStart);
            fNoise += fVoiced * fAlpha;
         }

         aiMax[j] = (int)AmplitudeToDb (fNoise) + (int)pBlock->cPeak; // now ignoring voiced
         iNewPeak = max(iNewPeak, aiMax[j]);
      }
      
      // change to use newpeak
      if (pBlock->cPeak != (char)iNewPeak)
         pBlock->cPeak = (char)iNewPeak;
      for (j = 0; j < dwNum; j++) {
         aiMax[j] = iNewPeak - aiMax[j];
         aiMax[j] /= 2;
         aiMax[j] = max(aiMax[j], 0);
         aiMax[j] = min(aiMax[j], 15);
         pb[j] = ((BYTE)aiMax[j] << 4) | 0x0f; // mark as totally unvoiced
      } // j
   } // i

   return TRUE;
}


/***************************************************************************
VoiceChatRandomize - Randomize the voice to simulate a different language

inputs
   PBYTE             pbData - Data to use
   DWORD             dwSize - Number of bytes available
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL VoiceChatRandomize (PBYTE pbData, DWORD dwSize)
{
   // verify that valid
   if (!VoiceChatDeCompress (pbData, dwSize, NULL, NULL, NULL))
      return FALSE;

   // find actual data

   // make sure have enough memory
   PVOICECHATHDR pVCH = (PVOICECHATHDR)pbData;
   // get the string and make sure it's null terminated
   PWSTR pszWave = (PWSTR)(pVCH + 1);
   DWORD dwLen = (DWORD)wcslen(pszWave);   // since know its valid

   // data begins from here
   PBYTE pbCur = (PBYTE)(pszWave + (dwLen+1));


   // swap back to front
   BYTE abTemp[max(SIZEOFVOICECHATBLOCK1,SIZEOFVOICECHATBLOCK3)];
   if (sizeof(abTemp) < pVCH->wBlockSize)
      return FALSE;  // cant randomize because blocks too large

   DWORD i;
   for (i = 0; i < (DWORD)pVCH->wNumBlocks/2; i++) {
      memcpy (abTemp, pbCur + i * (DWORD)pVCH->wBlockSize, pVCH->wBlockSize);
      memcpy (pbCur + i * (DWORD)pVCH->wBlockSize, pbCur + ((DWORD)pVCH->wNumBlocks - i - 1) * (DWORD)pVCH->wBlockSize, pVCH->wBlockSize);
      memcpy (pbCur + ((DWORD)pVCH->wNumBlocks - i - 1) * (DWORD)pVCH->wBlockSize, abTemp, pVCH->wBlockSize);
   } // i


   return TRUE;
}



/***************************************************************************
VoiceChatValidateMaxLength - Validates, and makes sure the voice chat
audio isn't too long.

inputs
   PBYTE             pbData - Data to use
   DWORD             dwSize - Number of bytes available
   DWORD             dwMax - Max length in seconds
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL VoiceChatValidateMaxLength (PBYTE pbData, DWORD dwSize, DWORD dwMax)
{
   // verify that valid
   if (!VoiceChatDeCompress (pbData, dwSize, NULL, NULL, NULL))
      return FALSE;

   PVOICECHATHDR pVCH = (PVOICECHATHDR)pbData;
   PWSTR pszWave = (PWSTR)(pVCH + 1);
   DWORD dwLen = (DWORD)wcslen(pszWave);   // since know its valid

   // data begins from here
   PBYTE pbCur = (PBYTE)(pszWave + (dwLen+1));


   // calc total number of blocks
   DWORD dwFinalNumBlocks = 0;
   DWORD i;
   for (i = 0; i < (DWORD)pVCH->wNumBlocks; i++, pbCur += pVCH->wBlockSize) {
      BYTE bCopies = ((PVOICECHATBLOCK1)pbCur)->bCopies;
      if (!bCopies)
         return FALSE;  // error
      
      dwFinalNumBlocks += (DWORD)bCopies;
   }

   // make sure not too long
   if (dwFinalNumBlocks > dwMax * VOICECHATSAMPLESPERSEC)   // NOTE: Assuming that max detail, so low-quality can be 2x as long
      return FALSE;

   return TRUE;
}

/***************************************************************************
VoiceChatStream - This handles a stream of voice data coming in.
It looks for decent silence points between words and cuts out the beginning
of the wave. This can then be cut and passed into a separate thread
for compression.

inputs
   PCM3DWave         pWave - Wave that audio keeps getting added to. This
                     may have data removed from the start.
   PCListFixed       plEnergy - Should initially be inialized to sizeof(fp). This
                     will be modofied by VoiceChatStream to monitor the
                     energy of the data and look for a good place to slice it
                     off and add it to the stream.
   BOOL              fHavePreviouslySent - Set to TRUE if data was previously sliced
                     of the beginning of this wave, FALSE if it's still the first
                     time. The first slice must be longer than later onces to
                     minimize the amount of pauses at the destination.
returns
   PCM3DWave - Slice of the wave, or NULL if nothing could be passed through
*/
PCM3DWave VoiceChatStream (PCM3DWave pWave, PCListFixed plEnergy, BOOL fHavePreviouslySent)
{
   // force voice chat to 100 samples per sec, not 200
   if (pWave->m_dwSRSAMPLESPERSEC != VOICECHATSAMPLESPERSEC) {
      pWave->m_dwSRSAMPLESPERSEC = VOICECHATSAMPLESPERSEC;
      pWave->m_dwSRSamples = 0;
      memset (pWave->m_adwPitchSamples, 0, sizeof(pWave->m_adwPitchSamples));
   }

   // figure out the block size... which is two SR windows worth so that SR features will
   // be nicely aligned
   DWORD dwWindow = (pWave->m_dwSamplesPerSec / pWave->m_dwSRSAMPLESPERSEC) * 2;
   
   // quick exit... if no new energy then done
   if (pWave->m_dwSamples / dwWindow <= plEnergy->Num())
      return NULL;

   // calculate the new energy
   DWORD i;
   while (plEnergy->Num() < pWave->m_dwSamples / dwWindow) {
      // calculate the energy
      double fSum = 0;
      DWORD dwMax = (plEnergy->Num()+1) * dwWindow;
      for (i = plEnergy->Num() * dwWindow; i < dwMax; i++)
         fSum += (double)pWave->m_psWave[i * pWave->m_dwChannels] * (double)pWave->m_psWave[i * pWave->m_dwChannels];
      fSum = sqrt(fSum / (double)dwWindow);
         // BUGFIX - had dwWindow outside the sqrt, which is wrong

      // add to list
      fp fValue = (fp) fSum;
      plEnergy->Add (&fValue);
   } // while

   // at what window do we start looking for viable data
   // start at 1 second
   DWORD dwStart = pWave->m_dwSamplesPerSec / dwWindow * 2 / 3; // 1/3  second
      // BUGFIX - Make this 1/3 of a second, to try to get as short as possible. Was 1/2 second
      // BUGFIX - Make this 2/3 of a second since getting too choppy with short values. Was 1/3 second
   if (!fHavePreviouslySent)
      dwStart *= 2;  // if havent previously sent then double to 1 seconds
   DWORD dwForced = pWave->m_dwSamplesPerSec / dwWindow * 2 + dwStart;   // max duration is 2 seconds beyond start

   // if don't have this many points yet then done
   DWORD dwNum = plEnergy->Num();
   fp *paf = (fp*)plEnergy->Get(0);
   if (dwStart >= dwNum)
      return NULL;

   // else, look, finding the minimum energy
   DWORD dwMin = dwStart;
   for (i = dwStart+1; i < dwNum; i++)
      if (paf[i] < paf[dwMin])
         dwMin = i;

   // how much energy is acceptable
   fp fCutAt;
   if (dwNum <= dwStart)
      fCutAt = 0; // shouldnt happen, but just in case
   else if (dwNum >= dwForced)
      fCutAt = 1000000; // basically, always get cut
   else {
      fCutAt = (fp)(dwNum - dwStart) / (fp)(dwForced - dwStart);
      fCutAt *= fCutAt; // so square
      fCutAt *= 10000;  // since 10000 is the expected energy of full voiced
   }

   // if not enough energy then stop here
   if (paf[dwMin] > fCutAt)
      return NULL;

#ifdef _DEBUG
   fp fMax = 0;
   for (i = 0; i < dwMin; i++)
      fMax = max(fMax, paf[i]);
#endif

   // else, need to slice out
   dwMin++; // so that end on a silence
   PCM3DWave pNew = pWave->Copy (0, dwMin * dwWindow);
   if (!pNew)
      return NULL;   // error
   pWave->ReplaceSection (0, dwMin * dwWindow, NULL);

   // clear out energy
   memmove (paf, paf+dwMin, (dwNum-dwMin)*sizeof(fp));
   plEnergy->Truncate (dwNum-dwMin);

#ifdef _DEBUG
   char szTemp[64];
   sprintf (szTemp, "\r\nLength=%f sec", (double)(dwMin*dwWindow) / (double)pWave->m_dwSamplesPerSec);
   OutputDebugString (szTemp);
#endif

   return pNew;
}


