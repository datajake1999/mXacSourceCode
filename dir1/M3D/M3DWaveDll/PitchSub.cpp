/***************************************************************************
PitchSub.cpp - C++ object for calculating the fujisaki sub-f0

begun 3/4/2009
Copyright 2009 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <mmreg.h>
#include <msacm.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"


#define PSE_SILENCE_NOEFFECT           0.1         // silence 0.1 sec or less has no effect on slope score
#define PSE_SILENCE_FULLEFFECT         0.5         // silence 0.5 sec has full effect
#define PSE_SILENCE_FULLEFFECT_ERROR   4.00        // at full effect, this is the error

#define PSE_NONSILENCE_ERRORBASE       1.00

#define PSE_MINMAX_TROUGHLOWER         0.0         // trough's lower side is at 0.0 above ideal pitch
#define PSE_MINMAX_TROUGHUPPER         0.75         // trough's high end is half an octave above ideal
#define PSE_MINMAX_TROUGH_ERRORPEROCTAVEABOVE (PSE_NONSILENCE_ERRORBASE)       // for every octave below trough, this error
#define PSE_MINMAX_TROUGH_ERRORPEROCTAVEBELOW (PSE_MINMAX_TROUGH_ERRORPEROCTAVEABOVE*2.0)       // for every octave below trough, this error

#define PSE_SLOPE_OVERPHRASEIDEAL      (-0.25)       // ideal slope over phrase, in octaves per second
#define PSE_SLOPE_ERRORIFNONEORDOUBLE  PSE_NONSILENCE_ERRORBASE  // amount of error if the slope gets to 0, or -0.5

#define PSE_PHRASE_MIN                 0.75        // phrases shouldnt be less than this seconds
#define PSE_PHRASE_MAX                 4.0         // or more this this seconds
#define PSE_PHRASE_ERRORMOREMAX        PSE_NONSILENCE_ERRORBASE  // for every second of duration more than max
#define PSE_PHRASE_ERRORLESSMIN        (PSE_PHRASE_ERRORMOREMAX*4.0)  // for every second of duration less than min

#define PSE_PHRASESEPSILENCE_NOEFFECT  (PSE_SILENCE_FULLEFFECT)   // if the phrase separation >= this number of sec then no effect
#define PSE_PHRASESEPSILENCE_ERROR     (PSE_NONSILENCE_ERRORBASE*2.0)  // error for every second the phrase separator is short

#define PSE_PHRASESEPOCTAVE_IDEAL      (-PSE_SLOPE_OVERPHRASEIDEAL)  // ideal separation in phrases
#define PSE_PHRASESEPOCTAVE_ERRORIFONEORDOUBLE (PSE_NONSILENCE_ERRORBASE*2.0)   // amount of error of not ideal

#define PSE_SLOPESTOTRY                11          // number of slopes to try
#define PSE_MAXPHASELENGTH             20          // phrases can't be any more than 20 syllables
#define PSE_MAXHYP                     1000        // maximum number of hypothesis

// PITCHSUBSLOPE - Information about a slope to add
typedef struct {
   fp                afTime[2];     // start/end time
   fp                afOctave[2];   // start/end octave
} PITCHSUBSLOPE, *PPITCHSUBSLOPE;

// CPitchSubHyp - Hypothesis for slope
class CPitchSubHyp {
public:
   ESCNEWDELETE;

   CPitchSubHyp (void);
   ~CPitchSubHyp (void);
   CPitchSubHyp *Clone (void);
   int Compare (CPitchSubHyp *pHyp, BOOL fIncludeError);

   fp             m_fError;            // total error so far

   BOOL           m_fInSlope;          // set to TRUE if in a slope. FALSE, if outside one
   PITCHSUBSLOPE  m_PSS;               // current slope that in (if m_fInSlope is true)
   fp             m_fErrorPerSec;      // amount of error added per second (if m_fInSlope)
   DWORD          m_dwSlopeEnd;        // where the slope ends

   CListFixed     m_lPITCHSUBSLOPE;    // list of slopes already added

private:
};
typedef CPitchSubHyp *PCPitchSubHyp;


BOOL PitchSubCalc (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec);
fp PitchSubSlopeOffset (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec,
                        fp fSlope);
fp PitchSubError (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec,
                  fp fSlope, fp fOffset);
fp PitchSubErrorDeltaLast (fp fPitchStartThis, fp fPitchEndLast, fp fTime);


/***************************************************************************
CPitchSubHyp::Constructor and destructor
*/
CPitchSubHyp::CPitchSubHyp (void)
{
   m_fError = 0.0;
   m_fInSlope = FALSE;
   m_lPITCHSUBSLOPE.Init (sizeof(PITCHSUBSLOPE));
}

CPitchSubHyp::~CPitchSubHyp (void)
{
   // do nothing
}



/***************************************************************************
CPitchSubHyp::Clone - Clones the hypothesis

returns
   CPitchSubHyp * - Copy
*/
CPitchSubHyp *CPitchSubHyp::Clone (void)
{
   PCPitchSubHyp pNew = new CPitchSubHyp;
   if (!pNew)
      return NULL;

   pNew->m_fError = m_fError;
   pNew->m_fErrorPerSec = m_fErrorPerSec;
   pNew->m_dwSlopeEnd = m_dwSlopeEnd;
   pNew->m_fInSlope = m_fInSlope;
   pNew->m_PSS = m_PSS;
   pNew->m_lPITCHSUBSLOPE.Init (sizeof(PITCHSUBSLOPE), m_lPITCHSUBSLOPE.Get(0), m_lPITCHSUBSLOPE.Num());

   return pNew;
}


/***************************************************************************
PITCHSUBSLOPECompare - Compares

inputs
   PPITCHSUBSLOPE       pA - A
   PPITCHSUBSLOPE       pB - B
returns
   int - 0 if the same, else as per qsort callback
*/
int PITCHSUBSLOPECompare (PPITCHSUBSLOPE pA, PPITCHSUBSLOPE pB)
{
   DWORD i;

   for (i = 0; i < 2; i++) {
      if (pA->afOctave[i] > pB->afOctave[i])
         return 1;
      else if (pA->afOctave[i] < pB->afOctave[i])
         return -1;

      if (pA->afTime[i] > pB->afTime[i])
         return 1;
      else if (pA->afTime[i] < pB->afTime[i])
         return -1;
   } // i

   return 0;
}

/***************************************************************************
CPitchSubHyp::Compare - Compares two hypothesis for the purpose of seeing
if they are the same (except for score).

inputs
   CPitchSubHyp      *pHyp - Other hypothesis
   BOOL              fIncludeError - If TRUE, include error in the sort
returns
   int - 0 if they're the same (except for score). + or - as per qsort callback
*/
int CPitchSubHyp::Compare (CPitchSubHyp *pHyp, BOOL fIncludeError)
{
   if (m_fInSlope != pHyp->m_fInSlope)
      return (int)m_fInSlope - (int)pHyp->m_fInSlope;

   DWORD i;
   int iRet;
   if (m_fInSlope) {  // && pHyp->m_fInSlope
      if (m_fErrorPerSec > pHyp->m_fErrorPerSec)
         return 1;
      else if (m_fErrorPerSec < pHyp->m_fErrorPerSec)
         return -1;

      iRet = PITCHSUBSLOPECompare (&m_PSS, &pHyp->m_PSS);
      if (iRet)
         return iRet;
   } // if both in slope

   // compare history
   if (m_lPITCHSUBSLOPE.Num() > pHyp->m_lPITCHSUBSLOPE.Num())
      return 1;
   else if (m_lPITCHSUBSLOPE.Num() < pHyp->m_lPITCHSUBSLOPE.Num())
      return -1;
   for (i = 0; i < m_lPITCHSUBSLOPE.Num(); i++) {
      iRet = PITCHSUBSLOPECompare (
         (PPITCHSUBSLOPE) m_lPITCHSUBSLOPE.Get(i),
         (PPITCHSUBSLOPE) pHyp->m_lPITCHSUBSLOPE.Get(i));
      if (iRet)
         return iRet;
   } // i
   
   if (fIncludeError) {
      if (pHyp->m_fError > m_fError)
         return 1;
      else if (pHyp->m_fError < m_fError)
         return -1;
   }

   return 0;
}


/***************************************************************************
PitchSubHypothesizePhrase - Hypothesize that a phrase exists.

inputs
   PPITCHSUBSYLLABLE       ppss - List of sub-syllables, one per phoneme.
                           Their fPitchSub will be filled in
   DWORD                   dwNum - Number of syb-syllables.
   DWORD                   dwSamplesPerSec - Sampling rate
   DWORD                   dwPhraseStart - Phrase start, from ppss
   DWORD                   dwPhraseEnd - Phrase end, from ppss, exclusive
   PCListFixed             plNotInSlope - List of PCPitchSubHyp that aren't in a slope.
                              DO NOT MODIFY. This must have at least one entry
   PCListFixed             plHyp - Append new PCPitchSubHyp to this for any new hypothsis created
returns
   BOOL - TRUE if success
*/
BOOL PitchSubHypothesizePhrase (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec,
                                DWORD dwPhraseStart, DWORD dwPhraseEnd,
                                PCListFixed plNotInSlope, PCListFixed plHyp)
{
   PCPitchSubHyp *ppPSH = (PCPitchSubHyp*) plNotInSlope->Get(0);

   // special case. If this is a length of one AND it's silence, then pass all through
   DWORD i;
   PCPitchSubHyp pNew;
   if ((dwPhraseEnd - dwPhraseStart == 1) && (ppss[dwPhraseStart].fSilence)) {
      for (i = 0; i < plNotInSlope->Num(); i++) {
         pNew = ppPSH[i]->Clone();
         if (!pNew)
            return FALSE;
         plHyp->Add (&pNew);
      } // i

      return TRUE;
   } // if silence

   // if there's silence at the beginning or end of this phrase then stop here.
   if (ppss[dwPhraseStart].fSilence || ppss[dwPhraseEnd-1].fSilence)
      return TRUE;

   // if this ends mid-word then end here.
   // Don't need to check if starts mid-word, though because won't ever happen
   if ((dwPhraseEnd < dwNum) && (ppss[dwPhraseEnd-1].dwWordNum == ppss[dwPhraseEnd].dwWordNum))
      return TRUE;

   // see what the duration of this phrase is
   fp fDuration = (fp)(ppss[dwPhraseEnd-1].dwEnd - ppss[dwPhraseStart].dwStart) / (fp)dwSamplesPerSec;
   fp fTimeStart = (fp)ppss[dwPhraseStart].dwStart / (fp)dwSamplesPerSec;
   fp fTimeEnd = (fp)ppss[dwPhraseEnd-1].dwEnd / (fp)dwSamplesPerSec;
   fp fSlopeIdeal = PSE_SLOPE_OVERPHRASEIDEAL / fDuration;

   // loop over possible slopes and generate
   DWORD dwSlope, dwHyp;
   for (dwSlope = 0; dwSlope < PSE_SLOPESTOTRY; dwSlope++) {
      fp fSlopeToTry = fSlopeIdeal * 2.0 * (fp)dwSlope / (PSE_SLOPESTOTRY-1);

      // what's the offset
      fp fOffset = PitchSubSlopeOffset (ppss + dwPhraseStart, dwPhraseEnd - dwPhraseStart, dwSamplesPerSec, fSlopeToTry);
      fp fPitchEnd = fOffset + fDuration * fSlopeToTry;

      // see what the error is
      fp fErrorSlope = PitchSubError (ppss + dwPhraseStart, dwPhraseEnd - dwPhraseStart, dwSamplesPerSec,
                  fSlopeToTry, fOffset);
      
      // loop over all the hypothesis when done and return the best one
      PCPitchSubHyp pHypBest = NULL;
      fp fHypBestError = 0.0;
      fp fHypBestErrorDelta = 0.0;

      for (dwHyp = 0; dwHyp < plNotInSlope->Num(); dwHyp++) {
         PCPitchSubHyp pHyp = ppPSH[dwHyp];
         
         fp fErrorHyp = pHyp->m_fError + fErrorSlope;
         fp fErrorDelta = 0;
         if (pHyp->m_lPITCHSUBSLOPE.Num()) {
            PPITCHSUBSLOPE pPSS = (PPITCHSUBSLOPE)pHyp->m_lPITCHSUBSLOPE.Get(pHyp->m_lPITCHSUBSLOPE.Num()-1);
            fErrorDelta = PitchSubErrorDeltaLast (fOffset, pPSS->afOctave[1], fTimeStart - pPSS->afTime[1]);

            fErrorHyp += fErrorDelta;
         } // if previous slope

         // if find a best then remember
         if (!pHypBest || (fErrorHyp < fHypBestError)) {
            pHypBest = pHyp;
            fHypBestError = fErrorHyp;
            fHypBestErrorDelta = fErrorDelta;
         }
      }  // dwHyp
      if (!pHypBest)
         continue;   // shouldnt happen

      // if get here will have found a best hyp, so add that
      pNew = pHypBest->Clone();
      pNew->m_fError += fHypBestErrorDelta;
      pNew->m_fErrorPerSec = fErrorSlope / fDuration;
      pNew->m_fInSlope = TRUE;
      pNew->m_dwSlopeEnd = dwPhraseEnd;
      pNew->m_PSS.afOctave[0] = fOffset;
      pNew->m_PSS.afOctave[1] = fPitchEnd;
      pNew->m_PSS.afTime[0] = fTimeStart;
      pNew->m_PSS.afTime[1] = fTimeEnd;
      plHyp->Add (&pNew);
   } // dwSlope

   return TRUE;
}



/***************************************************************************
PitchSubHypothesizePhrases - Hypothesize all possible phrases from here.

inputs
   PPITCHSUBSYLLABLE       ppss - List of sub-syllables, one per phoneme.
                           Their fPitchSub will be filled in
   DWORD                   dwNum - Number of syb-syllables.
   DWORD                   dwSamplesPerSec - Sampling rate
   DWORD                   dwPhraseStart - Phrase start, from ppss
   PCListFixed             plNotInSlope - List of PCPitchSubHyp that aren't in a slope.
                              DO NOT MODIFY. This must have at least one entry
   PCListFixed             plHyp - Append new PCPitchSubHyp to this for any new hypothsis created
returns
   BOOL - TRUE if success
*/
BOOL PitchSubHypothesizePhrases (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec,
                                DWORD dwPhraseStart,
                                PCListFixed plNotInSlope, PCListFixed plHyp)
{
   DWORD dwPhraseEnd;
   for (dwPhraseEnd = dwPhraseStart+1; (dwPhraseEnd <= dwNum) && (dwPhraseEnd <= dwPhraseStart + PSE_MAXPHASELENGTH); dwPhraseEnd++) {
      if (!PitchSubHypothesizePhrase (ppss, dwNum, dwSamplesPerSec, dwPhraseStart, dwPhraseEnd, plNotInSlope, plHyp))
         return FALSE;
   }

   return TRUE;
}


/***************************************************************************
PitchSubHypothesizeExpand - Expands the hypothesis

inputs
   PPITCHSUBSYLLABLE       ppss - List of sub-syllables, one per phoneme.
                           Their fPitchSub will be filled in
   DWORD                   dwNum - Number of syb-syllables.
   DWORD                   dwSamplesPerSec - Sampling rate
   DWORD                   dwPhraseStart - Phrase start, from ppss
   PCListFixed             plHypCur - Current list of hypothesis, PCPitchSubHyp
   PCListFixed             plHypNew - Where to fill in new hypothesis, PCPitchSubHyp
returns
   BOOL - TRUE if success
*/
BOOL PitchSubHypothesizeExpand (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec,
                                DWORD dwPhraseStart,
                                PCListFixed plHypCur, PCListFixed plHypNew)
{
   // loop through current. Continuing those that are still in a phrase, and
   // remembering those not in a phrase
   CListFixed lNotInSlope;
   lNotInSlope.Init (sizeof(PCPitchSubHyp));
   lNotInSlope.Required (plHypCur->Num());
   DWORD dwHyp;
   PCPitchSubHyp *ppPSH = (PCPitchSubHyp*) plHypCur->Get(0);
   fp fDuration = (fp)(ppss[dwPhraseStart].dwEnd - ppss[dwPhraseStart].dwStart) / (fp)dwSamplesPerSec;
   PCPitchSubHyp pHyp;
   for (dwHyp = 0; dwHyp < plHypCur->Num(); dwHyp++) {
      pHyp = ppPSH[dwHyp];

      // if not in a slope, remember this so can expand later
      if (!pHyp->m_fInSlope) {
         lNotInSlope.Add (&pHyp);
         continue;
      }

      // else, expand those in a slope
      pHyp = pHyp->Clone();
      if (!pHyp)
         return FALSE;
      plHypNew->Add (&pHyp);
      pHyp->m_fError += pHyp->m_fErrorPerSec * fDuration;
      if (dwPhraseStart + 1 >= pHyp->m_dwSlopeEnd) {
         // end this
         pHyp->m_lPITCHSUBSLOPE.Add (&pHyp->m_PSS);
         pHyp->m_fInSlope = FALSE;
      }
   } // dwHyp

   // expand possible slopes
   DWORD dwNumOld = plHypNew->Num();
   if (!PitchSubHypothesizePhrases (ppss, dwNum, dwSamplesPerSec, dwPhraseStart, &lNotInSlope, plHypNew))
      return FALSE;

   // loop through the ones just added and update error
   ppPSH = (PCPitchSubHyp*) plHypNew->Get(0);
   for (dwHyp = dwNumOld; dwHyp < plHypNew->Num(); dwHyp++) {
      pHyp = ppPSH[dwHyp];

      // only care if in slope
      if (!pHyp->m_fInSlope)
         continue;

      // else, expand those in a slope
      pHyp->m_fError += pHyp->m_fErrorPerSec * fDuration;
      if (dwPhraseStart + 1 >= pHyp->m_dwSlopeEnd) {
         // end this
         pHyp->m_lPITCHSUBSLOPE.Add (&pHyp->m_PSS);
         pHyp->m_fInSlope = FALSE;
      }
   } // dwHyp

   return TRUE;
}


/***************************************************************************
PitchSubHypothesizePrune - Prune the hypothesis.

Returns the score sorted by best hypothesis

inputs
   PCListFixed plHyp - List of hypothesis
returns
   BOOL - TRUE if success
*/

static int _cdecl PCPitchSubHypIdentSort (const void *elem1, const void *elem2)
{
   PCPitchSubHyp *pdw1, *pdw2;
   pdw1 = (PCPitchSubHyp*) elem1;
   pdw2 = (PCPitchSubHyp*) elem2;

   return pdw1[0]->Compare (pdw2[0], TRUE);
}


static int _cdecl PCPitchSubHypErrorSort (const void *elem1, const void *elem2)
{
   PCPitchSubHyp *pdw1, *pdw2;
   pdw1 = (PCPitchSubHyp*) elem1;
   pdw2 = (PCPitchSubHyp*) elem2;

   if (pdw1[0]->m_fError > pdw2[0]->m_fError)
      return 1;
   else if (pdw1[0]->m_fError < pdw2[0]->m_fError)
      return -1;
   else
      return 0;
}

BOOL PitchSubHypothesizePrune (PCListFixed plHyp)
{
   PCPitchSubHyp *ppPSH = (PCPitchSubHyp*) plHyp->Get(0);

   // sort to find identical
   qsort (ppPSH, plHyp->Num(), sizeof(PCPitchSubHyp), PCPitchSubHypIdentSort);
   DWORD dwHyp;
   for (dwHyp = plHyp->Num()-2; dwHyp < plHyp->Num(); dwHyp--) {
      if (ppPSH[dwHyp]->Compare (ppPSH[dwHyp+1], FALSE))
         continue;   // different

      // delete one
      // NOTE: Not tested because never got here
      if (ppPSH[dwHyp]->m_fError < ppPSH[dwHyp+1]->m_fError) {
         delete ppPSH[dwHyp+1];
         plHyp->Remove (dwHyp+1);
      }
      else {
         delete ppPSH[dwHyp];
         plHyp->Remove (dwHyp);
      }
      ppPSH = (PCPitchSubHyp*) plHyp->Get(0);   // since may have changed
   } // dwHyp

   // sort by score
   qsort (ppPSH, plHyp->Num(), sizeof(PCPitchSubHyp), PCPitchSubHypErrorSort);
   if (plHyp->Num() > PSE_MAXHYP) {
      for (dwHyp = PSE_MAXHYP; dwHyp < plHyp->Num(); dwHyp++)
         delete ppPSH[dwHyp];
      plHyp->Truncate (PSE_MAXHYP);
   }

   return TRUE;
}



/***************************************************************************
PitchSubSlopeOffset - Given a slope, in octaves/second, determine what the
offset at the start of the slope is.

inputs
   PPITCHSUBSYLLABLE       ppss - List of sub-syllables, one per phoneme.
                           Their fPitchSub will be filled in
   DWORD                   dwNum - Number of syb-syllables.
   DWORD                   dwSamplesPerSec - Sampling rate
   fp                      fSlope - In octaves/second
returns
   fp - Offset for the slope, in octaves. Returns -1000 if can't calculate
*/
static int _cdecl fpSort (const void *elem1, const void *elem2)
{
   fp *pdw1, *pdw2;
   pdw1 = (fp*) elem1;
   pdw2 = (fp*) elem2;

   if (*pdw2 > *pdw1)
      return -1;
   else if (*pdw2 < *pdw1)
      return 1;
   else
      return 0;
}

fp PitchSubSlopeOffset (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec,
                        fp fSlope)
{
   // loop through all the minimums and calculate the location
   CListFixed lOctave;
   lOctave.Init (sizeof(fp));
   lOctave.Required (dwNum);

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      // skip silence
      if (ppss[i].fSilence)
         continue;

      // else, have a point
      fp fX = (fp)(ppss[i].dwMid - ppss[0].dwStart) / (fp)dwSamplesPerSec;
      fp fY = ppss[i].fPitchLowest;
      fY -= fX * fSlope;

      lOctave.Add (&fY);
   } // i
   
   if (!lOctave.Num())
      return -1000.0;

   // sort
   fp *paf = (fp*)lOctave.Get(0);
   dwNum = lOctave.Num();
   qsort (paf, dwNum, sizeof(fp), fpSort);

   // average lowest 1/3
   fp fSum = 0;
   dwNum = max((dwNum+2)/3, 1);
   for (i = 0; i < dwNum; i++)
      fSum += paf[i];
   fSum /= (fp) dwNum;

   return fSum;
}



/***************************************************************************
PitchSubError - Determine the error in a slope.

inputs
   PPITCHSUBSYLLABLE       ppss - List of sub-syllables, one per phoneme.
                           Their fPitchSub will be filled in
   DWORD                   dwNum - Number of syb-syllables.
   DWORD                   dwSamplesPerSec - Sampling rate
   fp                      fSlope - In octaves/second
   fp                      fOffset - From PitchSubSlopeOffset()
returns
   fp - Error. Lower is better
*/
fp PitchSubError (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec,
                  fp fSlope, fp fOffset)
{
   fp fTotal = 0.0;

   // per syllable
   DWORD i, dwMin;
   fp fDuration;
   for (i = 0; i < dwNum; i++) {
      fDuration = (fp)(ppss[i].dwEnd - ppss[i].dwStart) / (fp)dwSamplesPerSec;

      // silence adds an error
      if (ppss[i].fSilence) {
         fDuration -= PSE_SILENCE_NOEFFECT;
         if (fDuration <= 0.0)
            continue;

         fDuration /= (PSE_SILENCE_FULLEFFECT - PSE_SILENCE_NOEFFECT);
         fDuration *= PSE_SILENCE_FULLEFFECT_ERROR;

         fTotal += fDuration;
         continue;
      }

      // loop through min and max
      for (dwMin = 0; dwMin < 2; dwMin++) {
         fp fValue = dwMin ? ppss[i].fPitchLowest : ppss[i].fPitchHighest;
         fp fX = (fp)(ppss[i].dwMid - ppss[0].dwStart) / (fp)dwSamplesPerSec;
         fp fIdeal = fX * fSlope + fOffset;

         // error
         fValue -= fIdeal;
         if (fValue < PSE_MINMAX_TROUGHLOWER) {
            fValue = PSE_MINMAX_TROUGHLOWER - fValue;
            fTotal += fDuration / 2.0 * fValue * PSE_MINMAX_TROUGH_ERRORPEROCTAVEBELOW;
         }
         else if (fValue > PSE_MINMAX_TROUGHUPPER) {
            fValue -= PSE_MINMAX_TROUGHUPPER;
            fTotal += fDuration / 2.0 * fValue * PSE_MINMAX_TROUGH_ERRORPEROCTAVEABOVE;
         }
      } // dwMin
   } // i

   // overall slope
   fDuration = (ppss[dwNum-1].dwEnd - ppss[0].dwStart) / (fp)dwSamplesPerSec;
   fp fSlopeOverAll = fSlope * fDuration;
   fSlopeOverAll = fabs((fSlopeOverAll - PSE_SLOPE_OVERPHRASEIDEAL)/PSE_SLOPE_OVERPHRASEIDEAL);
   fTotal += fSlopeOverAll * PSE_SLOPE_ERRORIFNONEORDOUBLE;

   // duration
   if (fDuration < PSE_PHRASE_MIN) {
      fDuration = PSE_PHRASE_MIN - fDuration;
      fTotal += fDuration * PSE_PHRASE_ERRORLESSMIN;
   }
   else if (fDuration > PSE_PHRASE_MAX) {
      fDuration -= PSE_PHRASE_MAX;
      fTotal += fDuration * PSE_PHRASE_ERRORMOREMAX;
   }

   return fTotal;
}


/***************************************************************************
PitchSubErrorDeltaLast - How much error is added given where this
slope starts (pitch), and the last one ended.

inputs
   fp       fPitchStartThis - Starting pitch (in octaves) of this
   fp       fPitchEndLast - Ending pitch of last
   fp       fTime - Time (in seconds) between the start of this and end of last
returns
   fp - Additional error
*/
fp PitchSubErrorDeltaLast (fp fPitchStartThis, fp fPitchEndLast, fp fTime)
{
   fp fTotal = 0.0;

   // if too short a time, then penalty
   if (fTime < PSE_PHRASESEPSILENCE_NOEFFECT)
      fTotal += (PSE_PHRASESEPSILENCE_NOEFFECT - fTime) * PSE_PHRASESEPSILENCE_ERROR;

   // if too much of a delta then error
   fPitchStartThis = (fPitchStartThis - fPitchEndLast) - PSE_PHRASESEPOCTAVE_IDEAL;
   fPitchStartThis = fabs(fPitchStartThis / PSE_PHRASESEPOCTAVE_IDEAL);
   fTotal += fPitchStartThis * PSE_PHRASESEPOCTAVE_ERRORIFONEORDOUBLE * 1.0 /*sec */;

   return fTotal;
}




/***************************************************************************
PitchSubCalc - Calculates pitchsub, from a wave file.

PITCH_F0, m_lWVPHONEME, and m_lWVWORD must all be filled in.

Fills in all the pitch for PITCH_SUB.

inputs
   PCM3DWave         pWave - Wave
returns
   BOOL - TRUE if success
*/
BOOL PitchSubCalc (PCM3DWave pWave)
{
   if (!pWave->m_apPitch[PITCH_F0] || !pWave->m_adwPitchSamples[PITCH_F0] |
      !pWave->m_lWVPHONEME.Num() || !pWave->m_lWVWORD.Num())
      return FALSE;

   // fill in a structure with all syllable info, so can send down
   CListFixed lPITCHSUBSYLLABLE;
   lPITCHSUBSYLLABLE.Init (sizeof(PITCHSUBSYLLABLE));
   PITCHSUBSYLLABLE pss;
   memset (&pss, 0, sizeof(pss));
   DWORD i, j;
   PWVPHONEME pwp = (PWVPHONEME)pWave->m_lWVPHONEME.Get(0);
   PWVWORD pww, pww2;
   for (i = 0; i < pWave->m_lWVPHONEME.Num(); i++, pwp++) {
      DWORD dwPhonemeStart = min(pwp->dwSample, pWave->m_dwSamples);
      DWORD dwPhonemeEnd = ((i+1) < pWave->m_lWVPHONEME.Num()) ? min(pwp[1].dwSample,pWave->m_dwSamples) : pWave->m_dwSamples;

      if (dwPhonemeEnd <= dwPhonemeStart)
         continue;   // not a valid length

      // loop over all the samples and get the pitch
      pss.fPitchHighest = 0.0;
      pss.fPitchLowest = 1000000.0;
      fp fPitch;
      DWORD dwMid = (dwPhonemeStart + dwPhonemeEnd) / 2;
      DWORD dwThird;
      for (dwThird = 0; dwThird < 3; dwThird++) {
         DWORD dwRangeStart = dwPhonemeStart + dwThird * (dwPhonemeEnd - dwPhonemeStart)/3;
         DWORD dwRangeEnd = dwPhonemeStart + (dwThird+1) * (dwPhonemeEnd - dwPhonemeStart)/3;
         fPitch = pWave->PitchOverRange (PITCH_F0,
            dwRangeStart,
            dwRangeEnd,
            0, NULL, NULL, NULL, FALSE);

         j = (dwRangeStart + dwRangeEnd) / 2;

         if (fPitch < pss.fPitchLowest) {
            dwMid = j;
            pss.fPitchLowest = fPitch;
         }
         pss.fPitchHighest = max(pss.fPitchHighest, fPitch);
      } // dwThird

      pss.fPitchLowest = log(pss.fPitchLowest) / log(2.0) - log((fp)SRBASEPITCH) / log(2.0);
      pss.fPitchHighest = log(pss.fPitchHighest) / log(2.0) - log((fp)SRBASEPITCH) / log(2.0);

      // figure out if vowel or silence
      PLEXENGLISHPHONE ple = MLexiconEnglishPhoneGet (pwp->dwEnglishPhone);
      if (ple && ((ple->dwCategory & PIC_MAJORTYPE) != PIC_MISC) ) {
         pss.fVowel = ((ple->dwCategory & PIC_MAJORTYPE) == PIC_VOWEL);
         pss.fVoiced = (ple->dwCategory & PIC_VOICED) ? TRUE : FALSE;
         pss.fSilence = FALSE;
      }
      else {
         pss.fVowel = FALSE;
         pss.fVoiced = FALSE;
         pss.fSilence = TRUE;
      }

      // see if this phoneme is split by a word
      for (j = 0; j < pWave->m_lWVWORD.Num(); j++) {
         pww = (PWVWORD)pWave->m_lWVWORD.Get(j);
         pww2 = (PWVWORD)pWave->m_lWVWORD.Get(j+1);
         DWORD dwWordStart = min(pww->dwSample, pWave->m_dwSamples);
         DWORD dwWordEnd = pww2 ? min(pww2->dwSample,pWave->m_dwSamples) : pWave->m_dwSamples;
         if ((dwPhonemeStart >= dwWordEnd) || (dwPhonemeStart < dwWordStart))
            continue;   // out of range

         // add this one
         pss.dwStart = dwPhonemeStart;
         pss.dwEnd = min(dwPhonemeEnd, dwWordEnd);
         pss.dwMid = min(dwMid, pss.dwEnd);
         pss.dwWordNum = j;
         lPITCHSUBSYLLABLE.Add (&pss);

         // if the phoneme end is beyond the word end then split
         if (dwPhonemeEnd > dwWordEnd) {
            // split
            pss.dwStart = dwWordEnd;
            pss.dwEnd = dwPhonemeEnd;
            pss.dwMid = max(dwMid, pss.dwStart);
            pss.dwWordNum = j+1;
            lPITCHSUBSYLLABLE.Add (&pss);
         }

         break;
      } // j
   } // i

   // call into syllable-based pitch detect
   PPITCHSUBSYLLABLE ppss = (PPITCHSUBSYLLABLE)lPITCHSUBSYLLABLE.Get(0);
   if (!PitchSubCalc (ppss, lPITCHSUBSYLLABLE.Num(), pWave->m_dwSamplesPerSec))
      return FALSE;

   // fill in and filter
   CMem mem;
   DWORD dwNum = pWave->m_adwPitchSamples[PITCH_F0];
   if (!dwNum)
      return FALSE;  // shouldnt happen
   if (!mem.Required (dwNum * sizeof(fp)))
      return FALSE;
   fp *paf = (fp*)mem.p;
   for (i = 0; i < dwNum; i++)
      paf[i] = -1;

   // from phonemes
   ppss = (PPITCHSUBSYLLABLE)lPITCHSUBSYLLABLE.Get(0);
   for (i = 0; i < lPITCHSUBSYLLABLE.Num(); i++, ppss++) {
      if (ppss->fPitchSub < -900.0)
         continue;   // not valid

      paf[min( ppss->dwMid / pWave->m_adwPitchSkip[PITCH_F0], dwNum-1)] =
         pow (2.0, ppss->fPitchSub) * SRBASEPITCH;
   } // i

   // filter
   fp fAvgPitch = pWave->PitchOverRange (PITCH_F0, 0, pWave->m_dwSamples, 0, NULL, NULL, NULL, FALSE);
   PitchLowPass (paf, dwNum,
      pWave->m_dwSamplesPerSec / pWave->m_adwPitchSkip[PITCH_F0] / 16,
      fAvgPitch);

   // write this
   if (!pWave->m_amemPitch[PITCH_SUB].Required (dwNum * pWave->m_dwChannels * sizeof(WVPITCH)))
      return FALSE;
   pWave->m_adwPitchSkip[PITCH_SUB] = pWave->m_adwPitchSkip[PITCH_F0];
   pWave->m_afPitchMaxStrength[PITCH_SUB] = 1.0;
   pWave->m_adwPitchSamples[PITCH_SUB] = dwNum;
   pWave->m_apPitch[PITCH_SUB] = (PWVPITCH) pWave->m_amemPitch[PITCH_SUB].p;
   memset (pWave->m_apPitch[PITCH_SUB], 0, dwNum * pWave->m_dwChannels * sizeof(WVPITCH));
   for (i = 0; i < dwNum * pWave->m_dwChannels; i++) {
      pWave->m_apPitch[PITCH_SUB][i].fFreq = paf[i / pWave->m_dwChannels];
      pWave->m_apPitch[PITCH_SUB][i].fStrength = 1.0;
   } // i

   return TRUE;
}


/***************************************************************************
PitchSubCalcSyl - Calculates pitchsub, from a list of PITCHSUBSYLLABLE, one per syllable

inputs
   PPITCHSUBSYLLABLE       ppss - List of sub-syllables, one per syllable.
   DWORD                   dwNum - Number of syb-syllables.
   DWORD                   dwSamplesPerSec - Sampling rate
   PCListFixed             plPITCHSUBSLOPE - Initialized and filled in with all the slopes
returns  
   BOOL - TRUE if success
*/
BOOL PitchSubCalcSyl (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec,
                      PCListFixed plPITCHSUBSLOPE)
{
   BOOL fRet = TRUE;
   plPITCHSUBSLOPE->Init (sizeof(PITCHSUBSLOPE));

   // an initial hypothesis
   PCPitchSubHyp pNew = new CPitchSubHyp;
   CListFixed alPingPong[2];  // pingpong
   alPingPong[0].Init (sizeof(PCPitchSubHyp));
   alPingPong[1].Init (sizeof(PCPitchSubHyp));
   alPingPong[0].Add (&pNew);

   // loop
   DWORD dwCur, dwHyp;
   DWORD dwFrom, dwTo;
   PCPitchSubHyp *ppPSH;
   dwTo = 0;   // just in case 0 length
   for (dwCur = 0; dwCur < dwNum; dwCur++) {
      dwFrom = (dwCur % 2);
      dwTo = 1 - dwFrom;

      // clear dwTo
      ppPSH = (PCPitchSubHyp*) alPingPong[dwTo].Get(0);
      for (dwHyp = 0; dwHyp < alPingPong[dwTo].Num(); dwHyp++)
         delete ppPSH[dwHyp];
      alPingPong[dwTo].Clear();

      // expand hypothesis
      if (!PitchSubHypothesizeExpand (ppss, dwNum, dwSamplesPerSec, dwCur, &alPingPong[dwFrom], &alPingPong[dwTo])) {
         fRet = FALSE;
         break;
      }

      // contract hypothesis
      if (!PitchSubHypothesizePrune (&alPingPong[dwTo])) {
         fRet = FALSE;
         break;
      }
   } // dwCur

   // fill in list from best hypothesis in dwTo
   if (alPingPong[dwTo].Num()) {
      ppPSH = (PCPitchSubHyp*) alPingPong[dwTo].Get(0);
      plPITCHSUBSLOPE->Init (sizeof(PITCHSUBSLOPE), ppPSH[0]->m_lPITCHSUBSLOPE.Get(0), ppPSH[0]->m_lPITCHSUBSLOPE.Num());
   }
   else
      fRet = FALSE;  // shouldnt happen

   // delete all
   for (dwTo = 0; dwTo < 2; dwTo++) {
      ppPSH = (PCPitchSubHyp*) alPingPong[dwTo].Get(0);
      for (dwHyp = 0; dwHyp < alPingPong[dwTo].Num(); dwHyp++)
         delete ppPSH[dwHyp];
      alPingPong[dwTo].Clear();
   } // dwTo

   return fRet;
}

/***************************************************************************
PitchSubCalc - Calculates pitchsub, from a list of PITCHSUBSYLLABLE, one per phoneme

inputs
   PPITCHSUBSYLLABLE       ppss - List of sub-syllables, one per phoneme.
                           Their fPitchSub will be filled in
   DWORD                   dwNum - Number of syb-syllables.
   DWORD                   dwSamplesPerSec - Sampling rate
returns  
   BOOL - TRUE if success
*/
BOOL PitchSubCalc (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec)
{
   // wipe out fPitchSub
   DWORD i, j;
   for (i = 0; i < dwNum; i++)
      ppss[i].fPitchSub = -1000.0;

   // make into syllables
   CListFixed lPITCHSUBSYLLABLE;
   lPITCHSUBSYLLABLE.Init (sizeof(PITCHSUBSYLLABLE));
   PITCHSUBSYLLABLE pss;
   for (i = 0; i < dwNum; i++) {
      // if it's silence then add
      if (ppss[i].fSilence) {
         lPITCHSUBSYLLABLE.Add (ppss + i);
         continue;
      }

      // if it's a consonant then do nothing
      if (!ppss[i].fVowel)
         continue;

      pss = ppss[i];

      // else, it's a vowel, so potentially take consonants to left and right
      int iLook;
      int iLeft = (int)i, iRight = (int)i;
      BOOL fLeftVowel = FALSE, fRightVowel = FALSE;
      for (iLook = (int)i-1; iLook >= 0; iLook--) {
         if (ppss[iLook].fSilence || (ppss[iLook].dwWordNum != ppss[i].dwWordNum))
            break;   // found end of word

         // else, found
         iLeft = iLook;
         fLeftVowel = ppss[iLook].fVowel;

         if (ppss[iLook].fVowel)
            break;
      }
      for (iLook = (int)i+1; iLook < (int)dwNum; iLook++) {
         if (ppss[iLook].fSilence || (ppss[iLook].dwWordNum != ppss[i].dwWordNum))
            break;   // found end of word

         // else, found
         iRight = iLook;
         fRightVowel = ppss[iLook].fVowel;

         if (ppss[iLook].fVowel)
            break;
      }

      // extend left
      if (iLeft < (int)i) {
         if (fLeftVowel)
            pss.dwStart -= (ppss[i].dwStart - ppss[iLeft].dwEnd) / 2; // take half of it
         else
            pss.dwStart = ppss[iLeft].dwStart; // all the way to the end
      }

      // extend right
      if (iRight > (int)i) {
         if (fRightVowel)
            pss.dwEnd += (ppss[iRight].dwStart - ppss[i].dwEnd + 1) / 2; // take half of it
         else
            pss.dwEnd = ppss[iRight].dwEnd; // all the way to the end
      }

      // re-do min max. Must be voiced
      for (j = 0; j < dwNum; j++)
         if (ppss[j].fVoiced && (ppss[j].dwMid >= pss.dwStart) && (ppss[j].dwMid < pss.dwEnd)) {
            pss.fPitchHighest = max(pss.fPitchHighest, ppss[j].fPitchHighest);

            if (ppss[j].fPitchLowest < pss.fPitchLowest) {
               pss.fPitchLowest = ppss[j].fPitchLowest;
               pss.dwMid = ppss[j].dwMid;
            }
         }

      // add this
      lPITCHSUBSYLLABLE.Add (&pss);
   } // dwNum



   // calculate
   CListFixed lPITCHSUBSLOPE;
   PPITCHSUBSYLLABLE ppss2 = (PPITCHSUBSYLLABLE)lPITCHSUBSYLLABLE.Get(0);
   if (!PitchSubCalcSyl (ppss2, lPITCHSUBSYLLABLE.Num(), dwSamplesPerSec, &lPITCHSUBSLOPE))
      return FALSE;

   // loop over and fill in where have pitch
   PPITCHSUBSLOPE pPSS = (PPITCHSUBSLOPE) lPITCHSUBSLOPE.Get(0);
   for (i = 0; i < dwNum; i++, ppss++) {
      if (ppss->fSilence) {
         ppss->fPitchSub = -1000;
         continue;
      }

      // find where this is
      fp fTime = (fp)ppss->dwMid / (fp)dwSamplesPerSec;
      fp fAlpha;
      for (j = 0; j < lPITCHSUBSLOPE.Num(); j++) {
         if ((fTime < pPSS[j].afTime[0]) || (fTime > pPSS[j].afTime[1]))
            continue;
         if (pPSS[j].afTime[0] >= pPSS[j].afTime[1])
            continue;   // shouldnt happen

         // else found
         fAlpha = (fTime - pPSS[j].afTime[0]) / (pPSS[j].afTime[1] - pPSS[j].afTime[0]);
         ppss->fPitchSub = fAlpha * (pPSS[j].afOctave[1] - pPSS[j].afOctave[0]) + pPSS[j].afOctave[0];
         break;
      } // j
   } // i

   return TRUE;
}
