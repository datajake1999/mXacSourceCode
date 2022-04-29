/*************************************************************************************
CPhaseModel.cpp - Handles the phase model.

begun 29/3/09 by Mike Rozak.
Copyright 2009 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "m3dwave.h"


/*************************************************************************************
WAVESEGMENTFLOATCompare - Compare two normalized WAVESEGMENTFLOAT and return the
difference in the square of the energy.

inputs
   PWAVESEGMENTFLOAT       pwsfA - First one
   PWAVESEGMENTFLAOT       pwsfB - Second one
   int                     iRotB - How much to rotate B to the right, in samples, before comparing,
                              must be within -2*SRFEATUREPCM and 2*SRFEATUREPCM
   DWORD                   dwSkip - Pass in 1 for maximum accuracy, or 4 for faster (but less accurate) compare
returns
   fp - square of the error
*/
fp WAVESEGMENTFLOATCompare (PWAVESEGMENTFLOAT pwsfA, PWAVESEGMENTFLOAT pwsfB, int iRotB, DWORD dwSkip)
{
   DWORD i;
   double fError = 0;
   float *pafA, *pafB;
   pafA = &pwsfA->afPCM[0];
   pafB = &pwsfB->afPCM[0];
   double fA, fB;
   for (i = 0; i < SRFEATUREPCM; i += dwSkip) {
      fA = pafA[i];
      fB = pafB[((int)i + 4*SRFEATUREPCM - iRotB)%(int)SRFEATUREPCM];

      fA -= fB;
      fError += fA * fA;
   } // i

   return fError;
}



/*************************************************************************************
WAVESEGMENTFLOATCompareBest - Find the best rotation of wave segment B that
produces the least error.

inputs
   PWAVESEGMENTFLOAT       pwsfA - First one
   PWAVESEGMENTFLAOT       pwsfB - Second one
   DWORD                   *pdwRotB - Filled in with the optimium rotation (rotating B to the right)
                           Can be NULL.
returns
   fp - square of the best error (lowest score)
*/
fp WAVESEGMENTFLOATCompareBest (PWAVESEGMENTFLOAT pwsfA, PWAVESEGMENTFLOAT pwsfB, DWORD *pdwRotB)
{
   // quickly find the minimum score
   DWORD i;
   DWORD dwSkip = 4;
   fp fErr;
   fp fErrBest = 0;
   DWORD dwRotRoughBest = 0;
   for (i = 0; i < SRFEATUREPCM; i += dwSkip) {
      fErr = WAVESEGMENTFLOATCompare (pwsfA, pwsfB, i, dwSkip);

      if (!i || (fErr < fErrBest)) {
         dwRotRoughBest = i;
         fErrBest = fErr;
      }
   } // i

   // do more accurate search
   int iStart = (int)dwRotRoughBest - (int)dwSkip * 2;
   int iStop = (int)dwRotRoughBest + (int)dwSkip * 2;
   int iCur;
   fErrBest = 0;
   int iRotBest = 0;
   for (iCur = iStart; iCur <= iStop; iCur++) {
      fErr = WAVESEGMENTFLOATCompare (pwsfA, pwsfB, iCur, 1);

      if ((iCur == iStart) || (fErr < fErrBest)) {
         iRotBest = iCur;
         fErrBest = fErr;
      }
   } // iCur

   // found it
   if (pdwRotB)
      *pdwRotB = (DWORD) (SRFEATUREPCM * 4 + iRotBest) % SRFEATUREPCM;

   return fErrBest;
}


/*************************************************************************************
WAVESEGMENTFLOATRotAndAdd - Rotate B right by the given number of samples, scale,
and add to A.

inputs
   PWAVESEGMENTFLOAT       pwsfA - First one
   PWAVESEGMENTFLAOT       pwsfB - Second one
   DWORD                   dwRotB - How many samples to rotate to the right
   float                   fScale - How much to scale B before adding to A
returns
   none
*/
void WAVESEGMENTFLOATRotAndAdd (PWAVESEGMENTFLOAT pwsfA, PWAVESEGMENTFLOAT pwsfB,
                                DWORD dwRotB, float fScale)
{
   DWORD i;
   float *pafA = &pwsfA->afPCM[0];
   float *pafB = &pwsfB->afPCM[0];
   fp fB;
   for (i = 0; i < SRFEATUREPCM; i++, pafA++) {
      fB = pafB[(i + SRFEATUREPCM - dwRotB) % SRFEATUREPCM];
      *pafA = *pafA + fB * fScale;
   } // i
}


/*************************************************************************************
WAVESEGMENTFLOATNormalize - Normalize so total energy = 1.0

inputs
   PWAVESEGMENTFLOAT       pwsf - Wave
*/
void WAVESEGMENTFLOATNormalize (PWAVESEGMENTFLOAT pwsf)
{
   double fSum = 0.0;
   DWORD i;

   for (i = 0; i < SRFEATUREPCM; i++)
      fSum += (double)pwsf->afPCM[i] * (double)pwsf->afPCM[i];
   fSum = sqrt(fSum);
   if (fSum < EPSILON)
      return;  // can't normalize

   fSum = 1.0 / fSum;
   for (i = 0; i < SRFEATUREPCM; i++)
      pwsf->afPCM[i] *= fSum;
}

/*************************************************************************************
CPhaseModel::Constructor and destructor
*/
CPhaseModel::CPhaseModel (void)
{
   DWORD i;
   for (i = 0; i < PHASEMODELPITCH; i++) {
      m_alPHASEMODELEXAMPLE[i].Init (sizeof(PHASEMODELEXAMPLE));
      m_alPHASEMODELEXAMPLEToTrain[i].Init (sizeof(PHASEMODELEXAMPLE));
      m_alPHASEMODELEXAMPLECALC[i].Init (sizeof(PHASEMODELEXAMPLECALC));
   }

   memset (&m_adwTrainedTimes, 0, sizeof(m_adwTrainedTimes));
}

CPhaseModel::~CPhaseModel (void)
{
   // do nothing for now
}



/*************************************************************************************
CPhaseModel::CloneTo - Standard API
*/
BOOL CPhaseModel::CloneTo (CPhaseModel *pTo)
{
   // NOT TESTED
   memcpy (pTo->m_adwTrainedTimes, m_adwTrainedTimes, sizeof(m_adwTrainedTimes));

   DWORD i;
   for (i = 0; i < PHASEMODELPITCH; i++) {
      pTo->m_alPHASEMODELEXAMPLE[i].Init (sizeof(PHASEMODELEXAMPLE), m_alPHASEMODELEXAMPLE[i].Get(0), m_alPHASEMODELEXAMPLE[i].Num());
      pTo->m_alPHASEMODELEXAMPLEToTrain[i].Init (sizeof(PHASEMODELEXAMPLE), m_alPHASEMODELEXAMPLEToTrain[i].Get(0), m_alPHASEMODELEXAMPLEToTrain[i].Num());
      pTo->m_alPHASEMODELEXAMPLECALC[i].Init (sizeof(PHASEMODELEXAMPLECALC), m_alPHASEMODELEXAMPLECALC[i].Get(0), m_alPHASEMODELEXAMPLECALC[i].Num());
   }

   return TRUE;
}



/*************************************************************************************
CPhaseModel::CloneTo - Standard API
*/
CPhaseModel *CPhaseModel::Clone (void)
{
   // NOT TESTED
   PCPhaseModel pNew = new CPhaseModel;
   if (!pNew)
      return NULL;
   if (!CloneTo(pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}


/*************************************************************************************
CPhaseModel::MMLToBinary - Like MMLTo, except this fills in a binary buffer
with information.

inputs
   PCMem       pmem - Memory to write to. Start at m_dwCurPosn and add on. Should
                     update m_dwCurPosn in the process
returns
   DWORD - Size of memory added, or 0 if error
*/
size_t CPhaseModel::MMLToBinary (PCMem pMem)
{
   // make sure committed
   if (!CommitTraining())
      return 0;

   // how much is needed
   size_t dwNeed = sizeof(PHASEMODELHEADER);
   DWORD i;
   for (i = 0; i < PHASEMODELPITCH; i++)
      dwNeed += sizeof(PHASEMODELEXAMPLE) * m_alPHASEMODELEXAMPLE[i].Num();

   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return 0;   // error
   PPHASEMODELHEADER pph = (PPHASEMODELHEADER) ((PBYTE)pMem->p + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwNeed;

   memset (pph, 0, sizeof(pph));
   pph->dwTotal = (DWORD)dwNeed;
   memcpy (pph->adwTrainedTimes, m_adwTrainedTimes, sizeof(m_adwTrainedTimes));
   PPHASEMODELEXAMPLE ppe = (PPHASEMODELEXAMPLE) (pph + 1);
   for (i = 0; i < PHASEMODELPITCH; i++) {
      pph->adwSize[i] = sizeof(PHASEMODELEXAMPLE) * m_alPHASEMODELEXAMPLE[i].Num();
      memcpy (ppe, m_alPHASEMODELEXAMPLE[i].Get(0), pph->adwSize[i]);
      ppe += m_alPHASEMODELEXAMPLE[i].Num();
   } // i

   return dwNeed;
}


/*************************************************************************************
CPhaseModel::MMLFromBinary - Reads triphone information from binary memory.

inputs
   PVOID             pvMem - Memory to read from
   DWORD             dwSize - Number of bytes. This is the same value as returned from MMLToBinary()
returns
   size_t - Number of bytes read. 0 if error
*/
size_t CPhaseModel::MMLFromBinary (PBYTE pabMem, size_t dwLeft)
{
   if (dwLeft < sizeof(PHASEMODELHEADER))
      return 0;
   PPHASEMODELHEADER pph = (PPHASEMODELHEADER) pabMem;
   if (dwLeft < pph->dwTotal)
      return 0;

   // verify the header
   DWORD i;
   DWORD dwTotal = sizeof(PHASEMODELHEADER);
   for (i = 0; i < PHASEMODELPITCH; i++)
      dwTotal += pph->adwSize[i];
   if (dwTotal != pph->dwTotal)
      return 0;

   // get
   memcpy (m_adwTrainedTimes, pph->adwTrainedTimes, sizeof(pph->adwTrainedTimes));
   PPHASEMODELEXAMPLE ppe = (PPHASEMODELEXAMPLE) (pph + 1);
   for (i = 0; i < PHASEMODELPITCH; i++) {
      if (pph->adwSize[i] % sizeof(PHASEMODELEXAMPLE))
         return 0;
      m_alPHASEMODELEXAMPLE[i].Init (sizeof(PHASEMODELEXAMPLE), ppe, pph->adwSize[i] / sizeof(PHASEMODELEXAMPLE));
      ppe += pph->adwSize[i] / sizeof(PHASEMODELEXAMPLE);

      m_alPHASEMODELEXAMPLECALC[i].Clear();
      m_alPHASEMODELEXAMPLEToTrain[i].Clear();

   } // i

   return pph->dwTotal;
}


/*************************************************************************************
CPhaseModel::PitchAsFp - Given the pitch (in hz) of the frame, and the average pitch
of the voice, this determines which pitch bin to place it in.

inputs
   fp          fPitch - Pitch in hz of the frame
   fp          fAvgPitch - Average pitch, in hz, of the voice
returns
   fp - Pitch bin to put it in, from 0..PHASEMODELPITCH-1. This may be a fractional
      result, used for interpolation
*/
fp CPhaseModel::PitchAsFp (fp fPitch, fp fAvgPitch)
{
   fp fBin = log(max(fPitch, CLOSE) / max(fAvgPitch,CLOSE)) / log((fp)2.0);
   fBin /= PHASEMODELPITCH_DELTA;
   fBin += (PHASEMODELPITCH - 1.0) / 2.0;
   fBin = max(fBin, 0.0);
   fBin = min(fBin, (fp)(PHASEMODELPITCH-1));

   return fBin;
}


/*************************************************************************************
CPhaseModel::Pitch - Given the pitch (in hz) of the frame, and the average pitch
of the voice, this determines which pitch bin to place it in.

inputs
   fp          fPitch - Pitch in hz of the frame
   fp          fAvgPitch - Average pitch, in hz, of the voice
returns
   DWORD - Pitch bin to put it in, from 0..PHASEMODELPITCH-1
*/
DWORD CPhaseModel::Pitch (fp fPitch, fp fAvgPitch)
{
   return (DWORD) floor(PitchAsFp (fPitch, fAvgPitch) + 0.5);
}


/*************************************************************************************
CPhaseModel::Train - Trains a srfeature.

inputs
   DWORD          dwPitch - From Pitch()
   PSRFEAURESMALL pSRFS - Speech recogntion feature small
   PSRFEATURE     pSRF - Used for acPCM
returns
   BOOL - TRUE if success
*/
BOOL CPhaseModel::Train (DWORD dwPitch, PSRFEATURESMALL pSRFS, PSRFEATURE pSRF)
{
   PHASEMODELEXAMPLE pme;
   memset (&pme, 0, sizeof(pme));
   pme.SRFS = *pSRFS;
#ifdef SRFEATUREINCLUDEPCM_SHORT
   _ASSERTE (sizeof(pme.asPCM) == sizeof(pSRF->asPCM));
   memcpy (pme.asPCM, pSRF->asPCM, sizeof(pme.asPCM));
#else
   _ASSERTE (sizeof(pme.acPCM) == sizeof(pSRF->acPCM));
   memcpy (pme.acPCM, pSRF->acPCM, sizeof(pme.acPCM));
#endif

   if (m_alPHASEMODELEXAMPLEToTrain[dwPitch].Num() >= PHASEMODELPITCH_MAXEXAMPLES) {
      if (!CommitTraining (dwPitch))
         return FALSE;
   }

   // add this
   m_alPHASEMODELEXAMPLEToTrain[dwPitch].Required (PHASEMODELPITCH_MAXEXAMPLES);
   m_alPHASEMODELEXAMPLEToTrain[dwPitch].Add (&pme);

   return TRUE;
}


/*************************************************************************************
CPhaseModel::CommitTraining - Commit to training.

inputs
   DWORD          dwPitch - From 0 .. PHASEMODELPITCH-1, or -1 for all
returns
   BOOL - TRUE if success
*/
BOOL CPhaseModel::CommitTraining (DWORD dwPitch)
{
   DWORD dwStart = dwPitch;
   DWORD dwEnd = dwPitch+1;
   if (dwPitch == (DWORD)-1) {
      dwStart = 0;
      dwEnd = PHASEMODELPITCH;
   }
   DWORD i;
   for (dwPitch = dwStart; dwPitch < dwEnd; dwPitch++) {
      PCListFixed plTrained = &m_alPHASEMODELEXAMPLE[dwPitch];
      PCListFixed plToTrain = &m_alPHASEMODELEXAMPLEToTrain[dwPitch];

      if (!plToTrain->Num())
         continue; // empty, so nothing

      // if less than the maximum then train up to that
      if (plTrained->Num() < PHASEMODELPITCH_MAXEXAMPLES) {
         DWORD dwToCopy = min(PHASEMODELPITCH_MAXEXAMPLES - plTrained->Num(), plToTrain->Num());

         for (i = 0; i < dwToCopy; i++) {
            DWORD dwIndex = rand() % plToTrain->Num();
            plTrained->Add (plToTrain->Get(dwIndex));
            plToTrain->Remove (dwIndex);
         } // i
         m_adwTrainedTimes[dwPitch] += dwToCopy;
      }

      // if nothing to train then done
      if (!plToTrain->Num() || !plTrained->Num())
         continue; // empty, so nothing

      // how many do I want to keep from the training
      fp fKeep = (fp)plToTrain->Num() / (fp) (plToTrain->Num() + m_adwTrainedTimes[dwPitch]) *
         (fp)plTrained->Num();

      // round up on keep?
      DWORD dwKeep = (DWORD)floor(fKeep);
      fKeep -= (fp)dwKeep;
      if ((fKeep >= EPSILON) && (randf(0.0, 1.0) < fKeep))
         dwKeep++;
      dwKeep = min (dwKeep, plToTrain->Num());

      m_adwTrainedTimes[dwPitch] += plToTrain->Num();
      for (; dwKeep; dwKeep--) {
         DWORD dwIndex = (DWORD)(rand() % plToTrain->Num());
         DWORD dwIndexTo = (DWORD)(rand() % plTrained->Num());
         plTrained->Set(dwIndexTo, plToTrain->Get(dwIndex));
         plToTrain->Remove (dwIndex);
      } // dwKeep
      plToTrain->Clear();

      // training calc is invalid
      m_alPHASEMODELEXAMPLECALC[dwPitch].Clear();
   } // dwPitch

   return TRUE;
}


/*************************************************************************************
CPhaseModel::CalcPHASEMODELEXAMPLE - Calulcate the phase model examples if not
already calculated.

inputs
   DWORD          dwPitch - From 0 .. PHASEMODELPITCH-1
returns
   PCListFixed - List of PHASEMODELEXAMPLECALC, or error if cant create
*/
PCListFixed CPhaseModel::CalcPHASEMODELEXAMPLE (DWORD dwPitch)
{
   DWORD i, j, dwNum;
   PHASEMODELEXAMPLECALC pmc;
   PPHASEMODELEXAMPLE ppme;
   PCListFixed plPHASEMODELEXAMPLECALC = &m_alPHASEMODELEXAMPLECALC[dwPitch];
   // if right number of entries in each, then ok
   dwNum = m_alPHASEMODELEXAMPLE[dwPitch].Num();
   if (dwNum == plPHASEMODELEXAMPLECALC->Num())
      return plPHASEMODELEXAMPLECALC;

   plPHASEMODELEXAMPLECALC->Clear();
   plPHASEMODELEXAMPLECALC->Required (dwNum);

   ppme = (PPHASEMODELEXAMPLE) m_alPHASEMODELEXAMPLE[dwPitch].Get(0);
   for (i = 0; i < dwNum; i++, ppme++) {
      pmc.fEnergySRFS = SRFEATUREEnergySmall(TRUE, &ppme->SRFS, FALSE, TRUE);
      for (j = 0; j < SRFEATUREPCM; j++)
#ifdef SRFEATUREINCLUDEPCM_SHORT
         pmc.WSF.afPCM[j] = ppme->asPCM[j];
#else
         pmc.WSF.afPCM[j] = ppme->acPCM[j];
#endif
      WAVESEGMENTFLOATNormalize (&pmc.WSF);
      plPHASEMODELEXAMPLECALC->Add (&pmc);
   } // i

   return plPHASEMODELEXAMPLECALC;
}


/*************************************************************************************
CPhaseModel::FillOut - If any phase model has less than PHASEMODELPITCH_MAXEXAMPLES entries,
then the number of entries is expanded. Use this before finishing building a TTS model
to make sure that there are enough phases.
*/
void CPhaseModel::FillOut (void)
{
   // make sure all the training is committed
   CommitTraining ();

   DWORD dwPitch;
   BOOL fLookDown;
   int aiLook[2], aiLookIndex[2];
   DWORD adwInLook[2];
   DWORD dwLookPasses;
   for (dwPitch = 0; dwPitch < PHASEMODELPITCH; dwPitch++) {
      fLookDown = TRUE;  // first get sample from down
      aiLook[0] = -1;
      aiLook[1] = PHASEMODELPITCH+1;
      dwLookPasses = 0;

      while (m_alPHASEMODELEXAMPLE[dwPitch].Num() < PHASEMODELPITCH_MAXEXAMPLES) {
         // if both looks are out of range then another passes
         if ((aiLook[0] < 0) && (aiLook[1] >= PHASEMODELPITCH)) {
            // might reset
            dwLookPasses++;
            if (dwLookPasses > PHASEMODELPITCH_MAXEXAMPLES+1)
               break;   // won't find anything else to add

            // else, reset the pass
            aiLook[0] = aiLook[1] = (int)dwPitch;
            adwInLook[0] = adwInLook[1] = 0;
            aiLookIndex[0] = aiLookIndex[1] = PHASEMODELPITCH_MAXEXAMPLES;
         }

         // go to the next index
         aiLookIndex[fLookDown]++;
         if (aiLookIndex[fLookDown] >= (int)adwInLook[fLookDown]) {
            // go to next entry
            if (fLookDown)
               aiLook[fLookDown]++;
            else
               aiLook[fLookDown]--;

            // starting again at zero index
            aiLookIndex[fLookDown] = 0;

            if ((aiLook[fLookDown] < 0) || (aiLook[fLookDown] >= PHASEMODELPITCH))
               // beyond end of range, so nothing in here
               adwInLook[fLookDown] = 0;
            else // , entry
               adwInLook[fLookDown] = m_alPHASEMODELEXAMPLE[aiLook[fLookDown]].Num();
            if (!adwInLook[fLookDown]) {
               fLookDown = !fLookDown;
               continue;   // nothing in here
            }

            // if get here, know aiLookIndex[fLookDown] < adwInLook[fLookDown]
         } // if look index is beyond end of list

         // add this
         m_alPHASEMODELEXAMPLE[dwPitch].Add (
            m_alPHASEMODELEXAMPLE[aiLook[fLookDown]].Get(aiLookIndex[fLookDown]) );

         // flip direction
         fLookDown = !fLookDown;
      } // while not enough
   } // dwPitch
}


/*************************************************************************************
CPhaseModel::MatchingExampleFindSRFEATURESMALL - Find the best matching examples
based on comparisons of SRFEATURESMALL.

inputs
   PSRFEATURESMALL   pSRFS - Feature
   fp                fEnergySRFS - Energy of the feature
   DWORD             dwPitch - From 0 .. PHASEMODELPITCH-1
   PCListFixed       plPWAVESEGMENTFLOAT - Initializes and filled in with a list of wave segments,
                     ordered from the best match to the worst.
returns
   BOOL - TRUE if success
*/

// MEFSEARCH - For the search
typedef struct {
   fp                fScore;  // score;
   PVOID             pData;   // data
} MEFSEARCH, *PMEFSEARCH;


static int _cdecl MEFSEARCHSortScore (const void *elem1, const void *elem2)
{
   MEFSEARCH *pdw1, *pdw2;
   pdw1 = (MEFSEARCH*) elem1;
   pdw2 = (MEFSEARCH*) elem2;

   if (pdw1->fScore > pdw2->fScore)
      return 1;
   else if (pdw1->fScore < pdw2->fScore)
      return -1;
   else
      return 0;
}

BOOL CPhaseModel::MatchingExampleFindSRFEATURESMALL (PSRFEATURESMALL pSRFS, fp fEnergySRFS,
                                                     DWORD dwPitch, PCListFixed plPWAVESEGMENTFLOAT)
{
   CListFixed lMEFSEARCH;
   lMEFSEARCH.Init (sizeof(MEFSEARCH));
   plPWAVESEGMENTFLOAT->Init (sizeof(PWAVESEGMENTFLOAT));

   PCListFixed plPMEC = CalcPHASEMODELEXAMPLE (dwPitch);
   if (!plPMEC)
      return FALSE;
   PPHASEMODELEXAMPLECALC ppec = (PPHASEMODELEXAMPLECALC) plPMEC->Get(0);
   PPHASEMODELEXAMPLE ppe = (PPHASEMODELEXAMPLE) m_alPHASEMODELEXAMPLE[dwPitch].Get(0);
   MEFSEARCH ms;
   memset (&ms, 0, sizeof(ms));
   DWORD i;
   for (i = 0; i < plPMEC->Num(); i++, ppec++, ppe++) {
      ms.fScore = SRFEATURECompareSmall (TRUE, pSRFS, /* fEnergySRFS,*/ &ppe->SRFS/*,  ppec->fEnergySRFS*/);
      ms.pData = &ppec->WSF;
      lMEFSEARCH.Add (&ms);
   } // i

   // sort this list
   PMEFSEARCH pms = (PMEFSEARCH) lMEFSEARCH.Get(0);
   qsort (pms, lMEFSEARCH.Num(), sizeof(MEFSEARCH), MEFSEARCHSortScore);

   // write out
   PWAVESEGMENTFLOAT pws;
   for (i = 0; i < lMEFSEARCH.Num(); i++, pms++) {
      pws = (PWAVESEGMENTFLOAT) pms->pData;
      plPWAVESEGMENTFLOAT->Add (&pws);
   }

   return TRUE;
}


/*************************************************************************************
CPhaseModel::MatchingExampleFindPCM - Find the best matching examples
based on comparisons of PCM.

inputs
   PWAVESEGMENTFLOAT pWSF - Normalize wave segment to match
   DWORD             dwPitch - From 0 .. PHASEMODELPITCH-1
   PCListFixed       plPWAVESEGMENTFLOAT - Initializes and filled in with a list of wave segments,
                     ordered from the best match to the worst.
returns
   BOOL - TRUE if success
*/
BOOL CPhaseModel::MatchingExampleFindPCM (PWAVESEGMENTFLOAT pWSF, DWORD dwPitch, PCListFixed plPWAVESEGMENTFLOAT)
{
   CListFixed lMEFSEARCH;
   lMEFSEARCH.Init (sizeof(MEFSEARCH));
   plPWAVESEGMENTFLOAT->Init (sizeof(PWAVESEGMENTFLOAT));

   PCListFixed plPMEC = CalcPHASEMODELEXAMPLE (dwPitch);
   if (!plPMEC)
      return FALSE;
   PPHASEMODELEXAMPLECALC ppec = (PPHASEMODELEXAMPLECALC) plPMEC->Get(0);
   PPHASEMODELEXAMPLE ppe = (PPHASEMODELEXAMPLE) m_alPHASEMODELEXAMPLE[dwPitch].Get(0);
   MEFSEARCH ms;
   memset (&ms, 0, sizeof(ms));
   DWORD i;
   for (i = 0; i < plPMEC->Num(); i++, ppec++, ppe++) {
      ms.fScore = WAVESEGMENTFLOATCompareBest (pWSF, &ppec->WSF, NULL);
      ms.pData = &ppec->WSF;
      lMEFSEARCH.Add (&ms);
   } // i

   // sort this list
   PMEFSEARCH pms = (PMEFSEARCH) lMEFSEARCH.Get(0);
   qsort (pms, lMEFSEARCH.Num(), sizeof(MEFSEARCH), MEFSEARCHSortScore);

   // write out
   PWAVESEGMENTFLOAT pws;
   for (i = 0; i < lMEFSEARCH.Num(); i++, pms++) {
      pws = (PWAVESEGMENTFLOAT) pms->pData;
      plPWAVESEGMENTFLOAT->Add (&pws);
   }

   return TRUE;
}

/*************************************************************************************
CPhaseModel::MatchingExampleFind - Find a matching example and generate the phase.

inputs
   PSRFEATURESMALL   pSRFS - Feature
   fp                fEnergySRFS - Energy of the feature
   fp                fPitch - Pitch in Hz
   fp                fAvgPitch - Average pitch for the voice, in Hz
   CPhaseModel       *pPMB - B phoneme to include in this. Can be NULL.
   fp                fWeightB - Weight of B, from 0.0 to 1.0.
   PWAVESEGMENTFLOAT pwsf - Where to fill in.
   BOOL              fPreviousValid - Set to TRUE if previous (pwsf[-1]) is valid,
                     and will try to match up against previous phase
returns
   BOOL - TRUE if success
*/

// MEFWEIGHTS - List with weights
typedef struct {
   fp                fWeight;    // final weighting
   WAVESEGMENTFLOAT  WSF;        // normalized wave segment
} MEFWEIGHTS, *PMEFWEIGHTS;

BOOL CPhaseModel::MatchingExampleFind (PSRFEATURESMALL pSRFS, fp fEnergySRFS, fp fPitch,
                                       fp fPitchAvg,
                                       CPhaseModel *pPMB, fp fWeightB,
                                       PWAVESEGMENTFLOAT pwsf, BOOL fPreviousValid)
{
   // wipe out the phase just in case
   memset (pwsf, 0, sizeof(*pwsf));

   if (!pPMB)
      fWeightB = 0.0;

   // find the pitch
   fp fPitchBin = PitchAsFp (fPitch, fPitchAvg);
   DWORD dwPitchBin = floor(fPitchBin);
   DWORD dwPitchBin2 = min(dwPitchBin+1, PHASEMODELPITCH-1);
   fPitchBin -= (fp)dwPitchBin;

   // fill in lists for closest SRFEATURE matches
   CListFixed alMatch[8];
   MatchingExampleFindSRFEATURESMALL (pSRFS, fEnergySRFS, dwPitchBin, &alMatch[0]);
   MatchingExampleFindSRFEATURESMALL (pSRFS, fEnergySRFS, dwPitchBin2, &alMatch[1]);

   // fill in lists for closest phase matches to previous
   if (fPreviousValid) {
      MatchingExampleFindPCM (pwsf - 1, dwPitchBin, &alMatch[2]);
      MatchingExampleFindPCM (pwsf - 1, dwPitchBin2, &alMatch[3]);
   }

   if (pPMB && fWeightB) {
      pPMB->MatchingExampleFindSRFEATURESMALL (pSRFS, fEnergySRFS, dwPitchBin, &alMatch[4]);
      pPMB->MatchingExampleFindSRFEATURESMALL (pSRFS, fEnergySRFS, dwPitchBin2, &alMatch[5]);

      // fill in lists for closest phase matches to previous
      if (fPreviousValid) {
         pPMB->MatchingExampleFindPCM (pwsf - 1, dwPitchBin, &alMatch[6]);
         pPMB->MatchingExampleFindPCM (pwsf - 1, dwPitchBin2, &alMatch[7]);
      }
   }

   // create a list of all the matches along with weights
   MEFWEIGHTS mw;
   CListFixed lMEFWEIGHTS;
   lMEFWEIGHTS.Init (sizeof(MEFWEIGHTS));
   memset (&mw, 0, sizeof(mw));
   DWORD i, dwSource;
   PWAVESEGMENTFLOAT *ppwsf;
   for (dwSource = 0; dwSource < sizeof(alMatch) / sizeof(alMatch[0]); dwSource++) {
      ppwsf = (PWAVESEGMENTFLOAT*)alMatch[dwSource].Get(0);
      for (i = 0; i < min(PHASEMODELPITCH_MAXEXAMPLESTOP, alMatch[dwSource].Num()); i++) {
         mw.WSF = *(ppwsf[i]);
         mw.fWeight = (fp)(PHASEMODELPITCH_MAXEXAMPLESTOP-i) / (fp)PHASEMODELPITCH_MAXEXAMPLESTOP;

         // lower weights for the phase contiguity part
         if ((dwSource % 4) >= 2)
            mw.fWeight *= 1.0;   // NOTE: Not lowering at the moment

         // take into account pitch to choose
         if (dwSource % 2)
            mw.fWeight *= fPitchBin;
         else
            mw.fWeight *= (1.0 - fPitchBin);

         // take into account second phoneme
         if (dwSource >= 4)
            mw.fWeight *= fWeightB;
         else
            mw.fWeight *= 1.0 - fWeightB;

         // if no weight, then don't add
         if (mw.fWeight <= EPSILON)
            continue;

         lMEFWEIGHTS.Add (&mw);
      }
   } // dwSource

   // randomize this list
   PMEFWEIGHTS pmw = (PMEFWEIGHTS) lMEFWEIGHTS.Get(0);
   PMEFWEIGHTS pmw2;
   for (i = 0; i < lMEFWEIGHTS.Num(); i++) {
      DWORD dwIndex = (DWORD)rand() % lMEFWEIGHTS.Num();
      mw = pmw[i];
      pmw[i] = pmw[dwIndex];
      pmw[dwIndex] = mw;
   } // i

   if (!lMEFWEIGHTS.Num())
      return FALSE;  // can't figure out a phase from this because no values

   // combine together
   DWORD dwRotB;
   fp fErr;
   while (lMEFWEIGHTS.Num() > 1) {
      for (i = 0; i+1 < lMEFWEIGHTS.Num(); i++) {  // intentionally i++
         pmw = (PMEFWEIGHTS) lMEFWEIGHTS.Get(i);
         pmw2 = pmw+1;

         // how much need to rotate B to get a match
         fErr = WAVESEGMENTFLOATCompareBest (&pmw->WSF, &pmw2->WSF, &dwRotB);

         // combine and scale
         WAVESEGMENTFLOATRotAndAdd (&pmw->WSF, &pmw2->WSF, dwRotB,
            pmw2->fWeight / max(pmw->fWeight,CLOSE));

         // normalize
         WAVESEGMENTFLOATNormalize (&pmw->WSF);

         // combine weights
         pmw->fWeight += pmw2->fWeight;

         // remove second one
         lMEFWEIGHTS.Remove (i+1);
      } // i, combining
   } // while have

   // when get here will only have one left
   pmw = (PMEFWEIGHTS) lMEFWEIGHTS.Get(0);
   
   // see if want to rotate it
   if (fPreviousValid) {
      fErr = WAVESEGMENTFLOATCompareBest (pwsf - 1, &pmw->WSF, &dwRotB);

      // combine and scale
      // NOTE: pwsf is already 0
      WAVESEGMENTFLOATRotAndAdd (pwsf, &pmw->WSF, dwRotB, 1.0);

      // need to normalize
      // NOTE: Don't need to normalize
      // WAVESEGMENTFLOATNormalize (pwsf);
   }
   else
      // just copy over
      pwsf[0] = pmw->WSF;

   return TRUE;
}


