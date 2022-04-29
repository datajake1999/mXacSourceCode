/***************************************************************************
FFT.cpp - FFT code

begun 3/5/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"


#define DETAILEDPHASE_VOICEDPHASESCALE       1.0      // need to scale voiced phase, so not equal to harmonic phase
   // BUGFIX - Use to be 0.1, but since doing everything using detailed voiced phase, must be 1.0
// #define DETAILEDPHASE_VOICEDPHASESCALE       0.1      // need to scale voiced phase, so not equal to harmonic phase


static CMem gMemSineLUT;         // store sine lookup
static CMem gMemDb;              // store dB lookup

// Psychoacoustic weights
static double gafPsychoDetailed[SRDATAPOINTSDETAILED];
static double gafPsychoDetailedSquared[SRDATAPOINTSDETAILED];
static double gafPsycho[SRDATAPOINTS];
static double gafPsychoSquared[SRDATAPOINTS];
static double gafPsychoSmall[SRDATAPOINTSSMALL];
static double gafPsychoSmallSquared[SRDATAPOINTSSMALL];


/***********************************************************************************
CSinLUT::CSinLUT
*/
CSinLUT::CSinLUT (void)
{
   m_dwNum = 0;
   m_pafLUT = 0;
}

/***********************************************************************************
CSinLUT::Init - Initializes the lookup table

inputs
   DWORD          dwNum - Number of points. This must be a multiple of 4.
returns
   BOOL - TRUE if success
*/
BOOL CSinLUT::Init (DWORD dwNum)
{
   if (!dwNum)
      return FALSE;
   while (dwNum < 4)
      dwNum *= 2;
   if (dwNum % 4)
      return FALSE;

   if (dwNum == m_dwNum)
      return TRUE;   // done

   if (!m_memSin.Required (dwNum * sizeof(double)))
      return FALSE;

   m_pafLUT = (double*)m_memSin.p;
   m_dwNum = dwNum;

   // fill in first part
   DWORD i;
   double fScale = TWOPI / (double)dwNum;
   m_pafLUT[0] = 0;
   m_pafLUT[dwNum/4] = 1;
   for (i = 1; i < dwNum/4; i++)
      m_pafLUT[i] = sin((double)i * fScale);
   for (i = dwNum/4+1; i < dwNum/2; i++)
      m_pafLUT[i] = m_pafLUT[dwNum/2 - i];
   for (i = dwNum/2; i < dwNum; i++)
      m_pafLUT[i] = -m_pafLUT[i - dwNum/2];

   return TRUE;
}

/***********************************************************************************
HanningWindow - Function for calculating a hanning window.

inputs
   fp          fAlpha - From 0.0 to 1.0 to cover an entire wavlength
returns
   fp - Hanning window, from 0.0 to 1.0
*/
__inline fp HanningWindow (fp fAlpha)
{
   return 0.5 - 0.5 * cos(fAlpha * 2.0 * PI);
}

/***********************************************************************************
CreateFFTWindow - Hannding window or whatever.

inputs
   DWORD    dwType - type - 0 for square, 1-triangle, 2-quadratic, 3-hann
   float   *pafWindow - Window function to fill in. dwWindowSize float's
   DWORD    dwWindowSize - Window size
   BOOL     fAreaScale - If TRUE then scale the area under the curve to 1, else curve
                        maxes at 1.0;
*/
void CreateFFTWindow (DWORD dwType, float *pafWindow, DWORD dwWindowSize, BOOL fAreaScale)
{
   // create window
   float   fSum, fTemp;
   DWORD y;

   fSum = 0;
   switch (dwType) {
   case 0:  // square
      for (y = 0; y < dwWindowSize; y++) {
         fTemp = 1;
         fSum += fTemp;
         pafWindow[y] = fTemp;
      }
      break;
   case 1:  // triangle
      for (y = 0; y < dwWindowSize; y++) {
         fTemp = (y < dwWindowSize/2) ? (dwWindowSize/2 - y) : (y - dwWindowSize/2);
         fSum += fTemp;
         pafWindow[y] = fTemp;
      }
      break;
   case 2:  // quadratic
      for (y = 0; y < dwWindowSize; y++) {
         fTemp = ((float)y - dwWindowSize/2) / (float)(dwWindowSize/2) ;
         fTemp *= fTemp;
         fTemp = 1 - fTemp;
         fSum += fTemp;
         pafWindow[y] = fTemp;
      }
      break;
   default:
   case 3:  // cos, hanning window
      for (y = 0; y < dwWindowSize; y++) {
         fTemp = HanningWindow ((fp)y / (fp)dwWindowSize);
         // fTemp = sin((float)y / (float)dwWindowSize * 3.141592653589793);
         // fTemp *= fTemp;
         fSum += fTemp;
         pafWindow[y] = fTemp;
      }
      break;
   }

   // BUGFIX - Add ability to not area scale
   if (fAreaScale) {
      fTemp = 1.0 / fSum * dwWindowSize; // so ends up summing up to dwWindowSize
      for (y = 0; y < dwWindowSize; y++)
         pafWindow[y] *= fTemp;
   }
}

/**********************************************************************************
EToTheTwoPIXOverN - Calculates pow (e, i * 2PI / N)

inputs
   int         i - Numerator, can be negative
   DWORD       dwN - Denominator.
   PCSinLUT    pLUT - Sine lookup
   double       *paf - paf[0] filled with the real, paf[1] with the imaginary
*/
__inline void EToTheTwoPIXOverN (int i, DWORD dwN, PCSinLUT pLUT, double *paf)
{
   paf[0] = pLUT->CosFast (i, dwN);
   paf[1] = pLUT->SinFast (i, dwN);
}

/**********************************************************************************
ImaginaryMult - Multiply two imaginary values.

inputs
   double       *pafA - pafA[0] is real, pafA[1] is imaginary
   double       *pafB - Like pafA
   double       *pafRes - Fill the results in here. Not pafA or pafB
*/
__inline void ImaginaryMult (double *pafA, double *pafB, double *pafRes)
{
   pafRes[0] = pafA[0] * pafB[0] - pafA[1] * pafB[1];
   pafRes[1] = pafA[0] * pafB[1] + pafA[1] * pafB[0];
}


/**********************************************************************************
ImaginaryAdd - Add two imaginary values.

inputs
   float       *pafA - pafA[0] is real, pafA[1] is imaginary
   float       *pafB - Like pafA
   float       *pafRes - Fill the results in here. Not pafA or pafB
*/
__inline void ImaginaryAdd (float *pafA, float *pafB, float *pafRes)
{
   pafRes[0] = pafA[0] + pafB[0];
   pafRes[1] = pafA[1] + pafB[1];
}


/**********************************************************************************
ImaginarySubtract - Subtract two imaginary values.

inputs
   double       *pafA - pafA[0] is real, pafA[1] is imaginary
   double       *pafB - Like pafA
   double       *pafRes - Fill the results in here. Not pafA or pafB
*/
__inline void ImaginarySubtract (double *pafA, double *pafB, double *pafRes)
{
   pafRes[0] = pafA[0] - pafB[0];
   pafRes[1] = pafA[1] - pafB[1];
}


#define COMPLEXCONJ

/**********************************************************************************
FFTRecurseTwo - Recursive FFT. Called if dwNum == 2

inputs
   double       *paf - Array of dwNum x 2 doubles, alternating between real and imaginary.
   // always assumed to be 2 DWORD       dwNum - Number of points
   // int         iSign - If positive does FFT, negative then inverse FFT
   // PCSinLUT    pLUT - Since look-up table. Must be valid!!!
returns
   none
*/
__inline void FFTRecurseTwo (double *paf)
{
   // create the scratch
   double af[4];
   // double *pafEven = &af[0], *pafOdd = &af[2];

   // DWORD dwHalf = 1; // dwNum/2;
   
   // divide and conquer
   memcpy (af, paf, sizeof(double) * 4);
   //memcpy (pafEven, paf, sizeof(double) * 2);
   //memcpy (pafOdd, paf + 2, sizeof(double) * 2);

   // combine
   //double afTemp[2];
   //double afE[2];

#ifdef COMPLEXCONJ
   // trivial case

   // BUGFIX - Tribial EToTheTwoPIXOverN
   //afE[0] = 1;
   //afE[1] = 0;
   //EToTheTwoPIXOverN (0 /* iSign * i */, 2 /*dwNum*/, pLUT, &afE[0]);

   // BUGFIX - Trivial imaginary mult
   //afTemp[0] = /*afE[0]=1 * */ pafOdd[0]; // - afE[1] * pafOdd[1];
   //afTemp[1] = /*afE[0]=1 * */ pafOdd[1]; // + afE[1] * pafOdd[0];
   //ImaginaryMult (&afE[0], pafOdd, &afTemp[0]);

   paf[0] = af[0] + af[2+0]; // pafEven[0] + pafOdd[0]; // afTemp[0];
   paf[1] = af[1] + af[2+1]; // pafEven[1] + pafOdd[1]; // afTemp[1];
   paf[2+0] = af[0] - af[2+0]; // pafEven[0] - pafOdd[0]; // afTemp[0];
   paf[2+1] = af[1] - af[2+1]; // pafEven[1] - pafOdd[1]; // afTemp[1];
//   for (i = 0, pafCur = paf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
//      EToTheTwoPIXOverN (iSign * (int)i, dwNum, pLUT, &afE[0]);
//      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
//      pafCur[0] = pafEvenCur[0] + afTemp[0];
//      pafCur[1] = pafEvenCur[1] + afTemp[1];
//   } // i
//   for (i = 0, pafCur = paf + 2*dwHalf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
//      EToTheTwoPIXOverN (iSign * (int)i, dwNum, pLUT, &afE[0]);
//      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
//      pafCur[0] = pafEvenCur[0] - afTemp[0];
//      pafCur[1] = pafEvenCur[1] - afTemp[1];
//   } // i
#else
   for (i = 0, pafCur = paf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < 1 /*dwHalf*/; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
      EToTheTwoPIXOverN (iSign * -(int)i, dwNum, pLUT, &afE[0]);
      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
      pafCur[0] = pafEvenCur[0] + afTemp[0];
      pafCur[1] = pafEvenCur[1] + afTemp[1];
   } // i
   for (i = 0, pafCur = paf + 2*1 /*dwHalf*/, pafEvenCur = pafEven, pafOddCur = pafOdd; i < 1 /*dwHalf*/; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
      EToTheTwoPIXOverN (iSign * -(int)i, dwNum, pLUT, &afE[0]);
      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
      pafCur[0] = pafEvenCur[0] - afTemp[0];
      pafCur[1] = pafEvenCur[1] - afTemp[1];
   } // i
#endif // COMPLEXCONJ

   // inverse scale by 1/n
   // BUGFIX - Move elsewhere
   //if (iSign < 0) {
   //   double fScale = 1.0 / (double)dwNum;
   //   for (i = 0, pafCur = paf; i < dwNum*2; i++, pafCur++)
   //      pafCur[0] *= fScale;
   //}

   return;
}

/**********************************************************************************
FFTRecurseFourPlus - Recurive FFT. Modified the data in place. For dwNum >= 4.

inputs
   double       *paf - Array of dwNum x 2 doubles, alternating between real and imaginary.
   DWORD       dwNum - Number of points. This MUST be 4+.
   int         iSign - If positive does FFT, negative then inverse FFT
   PCSinLUT    pLUT - Since look-up table. Cannot be NULL.
   PVOID       pScratch - Scratch space. Must be dwNum * 4 * sizeof(double) bytes, PLUS
                  all the scratch space required for recursion...
returns
   none
*/

void FFTRecurseFourPlus (double *paf, DWORD dwNum, int iSign, PCSinLUT pLUT, PVOID pScratch)
{
   // create the scratch
   double *pafEven, *pafOdd;
   DWORD dwNeed = dwNum * 4 * sizeof(double);
   pafEven = (double*)pScratch;
   pafOdd = pafEven + (dwNum*2);

   DWORD dwHalf = dwNum/2;
   
   // divide and conquer
   double *pafEvenCur, *pafOddCur, *pafCur;
   DWORD i;
   for (i = 0, pafEvenCur = pafEven, pafOddCur = pafOdd, pafCur = paf; i < dwHalf; i++) {
      // even
      *(pafEvenCur++) = *(pafCur++);
      *(pafEvenCur++) = *(pafCur++);

      // odd
      *(pafOddCur++) = *(pafCur++);
      *(pafOddCur++) = *(pafCur++);
   } // i

   // call FFTRecurse
   if (dwHalf == 2) {
      FFTRecurseTwo (pafEven);
      FFTRecurseTwo (pafOdd);
   }
   else {
      FFTRecurseFourPlus (pafEven, dwHalf, iSign, pLUT, (PBYTE)pScratch + dwNeed);
      FFTRecurseFourPlus (pafOdd, dwHalf, iSign, pLUT, (PBYTE)pScratch + dwNeed);
   }

   // combine
   double afTemp[2], afE[2];

#ifdef COMPLEXCONJ
   double *pafCur2;
   for (i = 0, pafCur = paf, pafCur2 = paf+2*dwHalf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafCur2 += 2, pafEvenCur += 2, pafOddCur += 2) {
      EToTheTwoPIXOverN (iSign * (int)i, dwNum, pLUT, &afE[0]);
      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
      pafCur[0] = pafEvenCur[0] + afTemp[0];
      pafCur[1] = pafEvenCur[1] + afTemp[1];
      pafCur2[0] = pafEvenCur[0] - afTemp[0];
      pafCur2[1] = pafEvenCur[1] - afTemp[1];
   } // i
//   for (i = 0, pafCur = paf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
//      EToTheTwoPIXOverN (iSign * (int)i, dwNum, pLUT, &afE[0]);
//      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
//      pafCur[0] = pafEvenCur[0] + afTemp[0];
//      pafCur[1] = pafEvenCur[1] + afTemp[1];
//   } // i
//   for (i = 0, pafCur = paf + 2*dwHalf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
//      EToTheTwoPIXOverN (iSign * (int)i, dwNum, pLUT, &afE[0]);
//      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
//      pafCur[0] = pafEvenCur[0] - afTemp[0];
//      pafCur[1] = pafEvenCur[1] - afTemp[1];
//   } // i
#else
   for (i = 0, pafCur = paf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
      EToTheTwoPIXOverN (iSign * -(int)i, dwNum, pLUT, &afE[0]);
      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
      pafCur[0] = pafEvenCur[0] + afTemp[0];
      pafCur[1] = pafEvenCur[1] + afTemp[1];
   } // i
   for (i = 0, pafCur = paf + 2*dwHalf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
      EToTheTwoPIXOverN (iSign * -(int)i, dwNum, pLUT, &afE[0]);
      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
      pafCur[0] = pafEvenCur[0] - afTemp[0];
      pafCur[1] = pafEvenCur[1] - afTemp[1];
   } // i
#endif // COMPLEXCONJ

   // inverse scale by 1/n
   // BUGFIX - Move elsewhere
   //if (iSign < 0) {
   //   double fScale = 1.0 / (double)dwNum;
   //   for (i = 0, pafCur = paf; i < dwNum*2; i++, pafCur++)
   //      pafCur[0] *= fScale;
   //}
}


/**********************************************************************************
FFTRecurse - Recurive FFT. Modified the data in place.

inputs
   double       *paf - Array of dwNum x 2 doubles, alternating between real and imaginary.
   DWORD       dwNum - Number of points
   int         iSign - If positive does FFT, negative then inverse FFT
   PCSinLUT    pLUT - Since look-up table. If not specified, one will be created.
   PVOID       pScratch - Scratch space. Must be dwNum * 4 * sizeof(double) bytes, PLUS
                  all the scratch space required for recursion...
returns
   none
*/

void FFTRecurse (double *paf, DWORD dwNum, int iSign, PCSinLUT pLUT, PVOID pScratch)
{
   // if dwNum == 1, then trivial... do nothing
   if (dwNum <= 1)
      return;

   // create sine LUT
   BOOL fCreatedLUT = FALSE;
   if (!pLUT) {
      pLUT = new CSinLUT;
      if (!pLUT)
         return;  // error
      if (!pLUT->Init (dwNum)) {
         delete pLUT;
         return;
      }
      fCreatedLUT = TRUE;
   }

   if (dwNum == 2)
      FFTRecurseTwo (paf);
   else
      FFTRecurseFourPlus (paf, dwNum, iSign, pLUT, pScratch);


   // free up
   if (fCreatedLUT)
      delete pLUT;
}


/**********************************************************************************
FFTRecurseReal - Recurive FFT. Modified the data in place.

inputs
   float       *paf - Array of dwNum floats, representing real values.
   DWORD       dwNum - Number of points
   int         iSign - If positive does FFT, negative then inverse FFT
   PCSinLUT    pLUT - Since look-up table. If not specified, one will be created.
   PCMem       pMemScratch - Scratch memory for doing FFT
returns
   none
*/
void FFTRecurseReal (float *paf, DWORD dwNum, int iSign, PCSinLUT pLUT, PCMem pMemScratch)
{
   paf += 1;   // since simulating numerical recipies on

   // allocate sine table
   if (!pLUT->Init (dwNum))
      return;

   // allocate all the scratch spaces
   DWORD dwNeed = dwNum*2*sizeof(double);
   DWORD dwNeedTotal = dwNeed;
   DWORD i;
   for (i = dwNum; i; i /= 2)
      dwNeedTotal += i*4*sizeof(double);   // as needed by FFTRecurseReal()
   if (!pMemScratch->Required (dwNeedTotal))
      return;

   double *pafComplex;
   pafComplex = (double*)pMemScratch->p;
   DWORD dwHalf = dwNum/2;

   DWORD dwCopyOver;

#ifdef COMPLEXCONJ   // faster
   double *pafCur;
   double afE[2], afConj[2], afSub[2], afTemp[2], afSum[2];
   if (iSign >= 0) {
      for (dwCopyOver = 0; dwCopyOver < dwNum; dwCopyOver++)
         pafComplex[dwCopyOver] = paf[dwCopyOver];
      // memcpy (pafComplex, paf, sizeof(double)*dwNum);
   }
   else { // inverse
      // stash away original extra
      double fOrigExtra = paf[1];
      paf[1] = 0;

      for (i = 0, pafCur = pafComplex; i <= dwHalf; i++, pafCur += 2) {
         // Fn = (Hn + Hn/2-n*)/2 - i/2(Hn - hN/2-n*)e2pin/N, changed to
         // Fn = (Hn + Hn/2-n*)/2 + i/2(Hn - hN/2-n*)e-2pin/N, changed to

         float *psfHn = &paf[(i == dwHalf) ? 0 : (i*2)];
         double afHn[2];
         afHn[0] = psfHn[0];
         afHn[1] = psfHn[1];
         DWORD dwN2Minus = i ? (dwHalf - i) : 0;
         float *psfHn2n = &paf[dwN2Minus*2];
         double afHn2n[2];
         afHn2n[0] = psfHn2n[0];
         afHn2n[1] = psfHn2n[1];

         // calulcate the complex conjugate
         afConj[0] = afHn2n[0];
         afConj[1] = -afHn2n[1];

         // ecalculate e 2piin/N, add Hn and Hn/2-n*
         ImaginarySubtract (afHn, afConj, afSub);
         EToTheTwoPIXOverN (- (int)i, dwNum, pLUT, afE);
         ImaginaryMult (afSub, afE, afTemp);

         // multiply by i
         afSum[0] = -afTemp[1];
         afSum[1] = afTemp[0];

         // add other bits
         afSum[0] += afHn[0] + afConj[0];
         afSum[1] += afHn[1] + afConj[1];

         // halve
         afSum[0] /= 2;
         afSum[1] /= 2;

         if (i == dwHalf) {
            // store fN/2 into imaginary part of f0
            pafComplex[0*2+0] = (paf[0] + fOrigExtra)/2.0;
            pafComplex[0*2+1] = (paf[0] - fOrigExtra)/2.0;
         }
         else {
            pafCur[0] = afSum[0];
            pafCur[1] = afSum[1];
         }
      } // i
   }

   // FFT
   FFTRecurse (pafComplex, dwNum/2, iSign, pLUT, (PBYTE)pMemScratch->p + dwNeed);

   // put back into place
   if (iSign >= 0) {
      float *pasfCur;

      for (i = 0, pasfCur = paf; i <= dwHalf; i++, pasfCur += 2) {
         // Fn = (Hn + Hn/2-n*)/2 - i/2(Hn - hN/2-n*)e2pin/N

         double *pfHn = &pafComplex[(i == dwHalf) ? 0 : (i*2)];
         DWORD dwN2Minus = i ? (dwHalf - i) : 0;
         double *pfHn2n = &pafComplex[dwN2Minus*2];

         // calulcate the complex conjugate
         afConj[0] = pfHn2n[0];
         afConj[1] = -pfHn2n[1];

         // ecalculate e 2piin/N, add Hn and Hn/2-n*
         ImaginarySubtract (pfHn, afConj, afSub);
         EToTheTwoPIXOverN ((int)i, dwNum, pLUT, afE);
         ImaginaryMult (afSub, afE, afTemp);

         // multiply by negative i
         afSum[0] = afTemp[1];
         afSum[1] = -afTemp[0];

         // add other bits
         afSum[0] += pfHn[0] + afConj[0];
         afSum[1] += pfHn[1] + afConj[1];

         // halve
         afSum[0] /= 2;
         afSum[1] /= 2;

         if (i == dwHalf) {
            // store fN/2 into imaginary part of f0
            paf[0*2+1] = afSum[0];
         }
         else {
            pasfCur[0] = afSum[0];
            pasfCur[1] = afSum[1];
         }
      } // i
   }
   else {
      // inverse FFT
      for (dwCopyOver = 0; dwCopyOver < dwNum; dwCopyOver++)
         paf[dwCopyOver] = pafComplex[dwCopyOver];
      // memcpy (paf, pafComplex, sizeof(double)*dwNum);
   }

#else
   double *pafCur, *pafFrom;
   if (iSign >= 0) {
      for (i = 0, pafCur = pafComplex, pafFrom = paf doesnt work anymore; i < dwNum; i++) {
         *(pafCur++) = *(pafFrom++);
         *(pafCur++) = 0;
      } // i
   }
   else {
      // inverting, so reproduce complex differently
      for (i = 0; i < dwNum; i++) {
         // real
         if (i < dwHalf)
            pafComplex[i*2+0] = paf[i*2+0];
         else if (i == dwHalf)
            pafComplex[i*2+0] = paf[1];
         else // i > dwHalf
            pafComplex[i*2+0] = paf[(dwNum - i)*2+0];

         // imaginary
         if (!i || (i == dwHalf))
            pafComplex[i*2+1] = 0;
         else if (i < dwHalf) 
            pafComplex[i*2+1] = -paf[i*2+1];
         else // i > dwHalf
            pafComplex[i*2+1] = paf[(dwNum-i)*2+1];
      } // i
   }

   // FFT
   FFTRecurse (pafComplex, dwNum, iSign, pLUT, (PBYTE)pMemScratch->p + dwNeed);

#if 0 // to test def _DEBUG
   char szTemp[256];
   OutputDebugString ("\r\nComplex");
   for (i = 0; i < dwNum; i++) {
      sprintf (szTemp, "\r\n\t%g, %g", (double)pafComplex[i*2+0], (double)pafComplex[i*2+1]);
      OutputDebugString (szTemp);
   } // i
#endif

   // convert back, into format produced by Numerical Recipies
   if (iSign >= 0) {
      for (i = 0; i < dwHalf; i++) {
         paf[i*2] = pafComplex[i*2+0];
         paf[i*2+1] = i ? (-pafComplex[i*2+1]) : pafComplex[dwHalf*2+0];
      } // i
   }
   else {   // inverting
      double fScale = 1.0 / 2.0;//BUGGIX - was / (double)dwNum;
      for (i = 0, pafCur = pafComplex, pafFrom = paf; i < dwNum; i++, pafCur+=2)
         *(pafFrom++) = *pafCur * fScale;
   }
#endif // !COMPLEXCONJ


#if 0 // to test inversion
   // try to invert it
   FFTRecurse (pafComplex, dwNum, -iSign, NULL, papScratch+1);
#endif

}




#ifdef _DEBUG  // only use old case for debug

/**********************************************************************************
FFTRecurse - Recurive FFT. Modified the data in place.

inputs
   double       *paf - Array of dwNum x 2 doubles, alternating between real and imaginary.
   DWORD       dwNum - Number of points
   int         iSign - If positive does FFT, negative then inverse FFT
   PCSinLUT    pLUT - Since look-up table. If not specified, one will be created.
   PVOID       pScratch - Scratch space. Must be dwNum * 4 * sizeof(double) bytes, PLUS
                  all the scratch space required for recursion...
returns
   none
*/
void FFTRecurseOld (double *paf, DWORD dwNum, int iSign, PCSinLUT pLUT, PVOID pScratch)
{
   // if dwNum == 1, then trivial... do nothing
   if (dwNum <= 1)
      return;

   // create sine LUT
   BOOL fCreatedLUT = FALSE;
   if (!pLUT) {
      pLUT = new CSinLUT;
      if (!pLUT)
         return;  // error
      if (!pLUT->Init (dwNum)) {
         delete pLUT;
         return;
      }
      fCreatedLUT = TRUE;
   }

   // create the scratch
   double *pafEven, *pafOdd;
   DWORD dwNeed = dwNum * 4 * sizeof(double);
   pafEven = (double*)pScratch;
   pafOdd = pafEven + (dwNum*2);

   DWORD dwHalf = dwNum/2;
   
   // divide and conquer
   double *pafEvenCur = pafEven, *pafOddCur = pafOdd, *pafCur = paf;
   DWORD i;
   for (i = 0; i < dwHalf; i++) {
      // even
      *(pafEvenCur++) = *(pafCur++);
      *(pafEvenCur++) = *(pafCur++);

      // odd
      *(pafOddCur++) = *(pafCur++);
      *(pafOddCur++) = *(pafCur++);
   } // i
   FFTRecurseOld (pafEven, dwHalf, iSign, pLUT, (PBYTE)pScratch + dwNeed);
   FFTRecurseOld (pafOdd, dwHalf, iSign, pLUT, (PBYTE)pScratch + dwNeed);

   // combine
   double afTemp[2], afE[2];

#ifdef COMPLEXCONJ
   for (i = 0, pafCur = paf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
      EToTheTwoPIXOverN (iSign * (int)i, dwNum, pLUT, &afE[0]);
      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
      pafCur[0] = pafEvenCur[0] + afTemp[0];
      pafCur[1] = pafEvenCur[1] + afTemp[1];
   } // i
   for (i = 0, pafCur = paf + 2*dwHalf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
      EToTheTwoPIXOverN (iSign * (int)i, dwNum, pLUT, &afE[0]);
      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
      pafCur[0] = pafEvenCur[0] - afTemp[0];
      pafCur[1] = pafEvenCur[1] - afTemp[1];
   } // i
#else
   for (i = 0, pafCur = paf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
      EToTheTwoPIXOverN (iSign * -(int)i, dwNum, pLUT, &afE[0]);
      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
      pafCur[0] = pafEvenCur[0] + afTemp[0];
      pafCur[1] = pafEvenCur[1] + afTemp[1];
   } // i
   for (i = 0, pafCur = paf + 2*dwHalf, pafEvenCur = pafEven, pafOddCur = pafOdd; i < dwHalf; i++, pafCur += 2, pafEvenCur += 2, pafOddCur += 2) {
      EToTheTwoPIXOverN (iSign * -(int)i, dwNum, pLUT, &afE[0]);
      ImaginaryMult (&afE[0], pafOddCur, &afTemp[0]);
      pafCur[0] = pafEvenCur[0] - afTemp[0];
      pafCur[1] = pafEvenCur[1] - afTemp[1];
   } // i
#endif // COMPLEXCONJ

   // inverse scale by 1/n
   // BUGFIX - Move elsewhere
   //if (iSign < 0) {
   //   double fScale = 1.0 / (double)dwNum;
   //   for (i = 0, pafCur = paf; i < dwNum*2; i++, pafCur++)
   //      pafCur[0] *= fScale;
   //}

   // free up
   if (fCreatedLUT)
      delete pLUT;
}


/**********************************************************************************
FFTRecurseRealOld - Recurive FFT. Modified the data in place.

inputs
   double       *paf - Array of dwNum doubles, representing real values.
   DWORD       dwNum - Number of points
   int         iSign - If positive does FFT, negative then inverse FFT
   PCSinLUT    pLUT - Since look-up table. If not specified, one will be created.
   PCMem       pMemScratch - Scratch memory for doing FFT
returns
   none
*/
void FFTRecurseRealOld (double *paf, DWORD dwNum, int iSign, PCSinLUT pLUT, PCMem pMemScratch)
{
   paf += 1;   // since simulating numerical recipies on

   // allocate sine table
   if (!pLUT->Init (dwNum))
      return;

   // allocate all the scratch spaces
   DWORD dwNeed = dwNum*2*sizeof(double);
   DWORD dwNeedTotal = dwNeed;
   DWORD i;
   for (i = dwNum; i; i /= 2)
      dwNeedTotal += i*4*sizeof(double);   // as needed by FFTRecurseReal()
   if (!pMemScratch->Required (dwNeedTotal))
      return;

   double *pafComplex;
   pafComplex = (double*)pMemScratch->p;
   DWORD dwHalf = dwNum/2;


#ifdef COMPLEXCONJ   // faster
   double *pafCur;
   double afE[2], afConj[2], afSub[2], afTemp[2], afSum[2];
   if (iSign >= 0)
      memcpy (pafComplex, paf, sizeof(double)*dwNum);
   else { // inverse
      // stash away original extra
      double fOrigExtra = paf[1];
      paf[1] = 0;

      for (i = 0, pafCur = pafComplex; i <= dwHalf; i++, pafCur += 2) {
         // Fn = (Hn + Hn/2-n*)/2 - i/2(Hn - hN/2-n*)e2pin/N, changed to
         // Fn = (Hn + Hn/2-n*)/2 + i/2(Hn - hN/2-n*)e-2pin/N, changed to

         double *pfHn = &paf[(i == dwHalf) ? 0 : (i*2)];
         DWORD dwN2Minus = i ? (dwHalf - i) : 0;
         double *pfHn2n = &paf[dwN2Minus*2];

         // calulcate the complex conjugate
         afConj[0] = pfHn2n[0];
         afConj[1] = -pfHn2n[1];

         // ecalculate e 2piin/N, add Hn and Hn/2-n*
         ImaginarySubtract (pfHn, afConj, afSub);
         EToTheTwoPIXOverN (- (int)i, dwNum, pLUT, afE);
         ImaginaryMult (afSub, afE, afTemp);

         // multiply by i
         afSum[0] = -afTemp[1];
         afSum[1] = afTemp[0];

         // add other bits
         afSum[0] += pfHn[0] + afConj[0];
         afSum[1] += pfHn[1] + afConj[1];

         // halve
         afSum[0] /= 2;
         afSum[1] /= 2;

         if (i == dwHalf) {
            // store fN/2 into imaginary part of f0
            pafComplex[0*2+0] = (paf[0] + fOrigExtra)/2.0;
            pafComplex[0*2+1] = (paf[0] - fOrigExtra)/2.0;
         }
         else {
            pafCur[0] = afSum[0];
            pafCur[1] = afSum[1];
         }
      } // i
   }

   // FFT
   FFTRecurseOld (pafComplex, dwNum/2, iSign, pLUT, (PBYTE)pMemScratch->p + dwNeed);

   // put back into place
   if (iSign >= 0) {
      for (i = 0, pafCur = paf; i <= dwHalf; i++, pafCur += 2) {
         // Fn = (Hn + Hn/2-n*)/2 - i/2(Hn - hN/2-n*)e2pin/N

         double *pfHn = &pafComplex[(i == dwHalf) ? 0 : (i*2)];
         DWORD dwN2Minus = i ? (dwHalf - i) : 0;
         double *pfHn2n = &pafComplex[dwN2Minus*2];

         // calulcate the complex conjugate
         afConj[0] = pfHn2n[0];
         afConj[1] = -pfHn2n[1];

         // ecalculate e 2piin/N, add Hn and Hn/2-n*
         ImaginarySubtract (pfHn, afConj, afSub);
         EToTheTwoPIXOverN ((int)i, dwNum, pLUT, afE);
         ImaginaryMult (afSub, afE, afTemp);

         // multiply by negative i
         afSum[0] = afTemp[1];
         afSum[1] = -afTemp[0];

         // add other bits
         afSum[0] += pfHn[0] + afConj[0];
         afSum[1] += pfHn[1] + afConj[1];

         // halve
         afSum[0] /= 2;
         afSum[1] /= 2;

         if (i == dwHalf) {
            // store fN/2 into imaginary part of f0
            paf[0*2+1] = afSum[0];
         }
         else {
            pafCur[0] = afSum[0];
            pafCur[1] = afSum[1];
         }
      } // i
   }
   else // inverse FFT
      memcpy (paf, pafComplex, sizeof(double)*dwNum);

#else
   double *pafCur, *pafFrom;
   if (iSign >= 0) {
      for (i = 0, pafCur = pafComplex, pafFrom = paf; i < dwNum; i++) {
         *(pafCur++) = *(pafFrom++);
         *(pafCur++) = 0;
      } // i
   }
   else {
      // inverting, so reproduce complex differently
      for (i = 0; i < dwNum; i++) {
         // real
         if (i < dwHalf)
            pafComplex[i*2+0] = paf[i*2+0];
         else if (i == dwHalf)
            pafComplex[i*2+0] = paf[1];
         else // i > dwHalf
            pafComplex[i*2+0] = paf[(dwNum - i)*2+0];

         // imaginary
         if (!i || (i == dwHalf))
            pafComplex[i*2+1] = 0;
         else if (i < dwHalf) 
            pafComplex[i*2+1] = -paf[i*2+1];
         else // i > dwHalf
            pafComplex[i*2+1] = paf[(dwNum-i)*2+1];
      } // i
   }

   // FFT
   FFTRecurseOld (pafComplex, dwNum, iSign, pLUT, (PBYTE)pMemScratch->p + dwNeed);

#if 0 // to test def _DEBUG
   char szTemp[256];
   OutputDebugString ("\r\nComplex");
   for (i = 0; i < dwNum; i++) {
      sprintf (szTemp, "\r\n\t%g, %g", (double)pafComplex[i*2+0], (double)pafComplex[i*2+1]);
      OutputDebugString (szTemp);
   } // i
#endif

   // convert back, into format produced by Numerical Recipies
   if (iSign >= 0) {
      for (i = 0; i < dwHalf; i++) {
         paf[i*2] = pafComplex[i*2+0];
         paf[i*2+1] = i ? (-pafComplex[i*2+1]) : pafComplex[dwHalf*2+0];
      } // i
   }
   else {   // inverting
      double fScale = 1.0 / 2.0;//BUGGIX - was / (double)dwNum;
      for (i = 0, pafCur = pafComplex, pafFrom = paf; i < dwNum; i++, pafCur+=2)
         *(pafFrom++) = *pafCur * fScale;
   }
#endif // !COMPLEXCONJ


#if 0 // to test inversion
   // try to invert it
   FFTRecurseOld (pafComplex, dwNum, -iSign, NULL, papScratch+1);
#endif

}
#endif // _DEBUG


/***********************************************************************************
DbToAmplitude - Convers Db (where 0 dB is an amplitude of 32768) to amplitude.

inputs
   char           dB - Decibels
returns
   int - amplitude
*/
int DbToAmplitude (char dB)
{
   int *paiLUT;

   if (!gMemDb.m_dwAllocated) {
      DWORD i;

      if (!gMemDb.Required (256 * sizeof(int)))
         return 0;
      paiLUT = (int*) gMemDb.p;

      for (i = 0; i < 256; i++)
         paiLUT[i] = (int)(pow(10.0, (double)(char)(BYTE)i / 20.0) * 32768.0);
   }

   paiLUT = (int*) gMemDb.p;
   return paiLUT[(BYTE)dB];
}


/***********************************************************************************
SineLUT - Sine function based on look-up table.

inputs
   DWORD          dwAngle - 0 to 0xffffffff representin 0 to 2PI
returns
   int - Range from -0xffff to 0xffff for sine value
*/
#define SLUTSHIFT       16
#define SLUTSIZE        (1 << (32 - SLUTSHIFT - 2))  //0x4000

int SineLUT (DWORD dwAngle)
{
   WORD *pawLUT;
   DWORD dwSign;

   if (!gMemSineLUT.m_dwAllocated) {
      DWORD i;

      if (!gMemSineLUT.Required (SLUTSIZE * sizeof(WORD)))
         return 0;
      pawLUT = (WORD*) gMemSineLUT.p;

      for (i = 0; i < SLUTSIZE; i++)
         pawLUT[i] = (WORD)(sin((double)i / (double)SLUTSIZE / 4.0 * 2.0 * PI) * (double)0xffff);
   }

   pawLUT = (WORD*) gMemSineLUT.p;
   dwAngle = dwAngle >> SLUTSHIFT;
   dwSign = dwAngle / SLUTSIZE;
   dwAngle -= dwSign * SLUTSIZE;

   switch (dwSign) {
   case 0: // first quadrant
      return (int)(DWORD)pawLUT[dwAngle];
   case 1:  // second
      return (int)(DWORD)pawLUT[SLUTSIZE - dwAngle - 1];
   case 2:  // third
      return -(int)(DWORD)pawLUT[dwAngle];
   case 3:  // fourth
      return -(int)(DWORD)pawLUT[SLUTSIZE - dwAngle - 1];
   default:
      return 0;   // error
   }
}


/******************************************************************************
SRFEATUREInterpolatePhase - Interpolate phase of two or more SR features.

inputs
   PSRFEATURE        paSRFeature - Pointer to an array of SR features
   DWORD             dwNum - Number of features
   fp                *pafWeight - Weighting for each of dwNum. Sum = 1.0. If
                        this is NULL then assume even weight
   PSRFEATURE        pFinal - Filled in with the final feature
returns
   none
*/
void SRFEATUREInterpolatePhase (PSRFEATURE paSRFeature, DWORD dwNum, fp *pafWeight, PSRFEATURE pFinal)
{
   DWORD i, j;
   // average the phase
   for (i = 0; i < SRPHASENUM; i++) {
      fp fSum = 0;
      for (j = 0; j < dwNum; j++) {
         int iPhase1, iPhase2;
         iPhase1 = (int)paSRFeature[j].abPhase[i];
         iPhase2 = (int)paSRFeature[0].abPhase[i];
         // keep the phases all closest to the first one
         if (abs(iPhase1 - iPhase2) > 0x80) {
            if (iPhase1 < iPhase2)
               iPhase1 += 0x100;
            else
               iPhase1 -= 0x100;
         }
         fSum += (fp)iPhase1 * (pafWeight ? pafWeight[j] : (1.0/(fp)dwNum));
      } // j
      
      pFinal->abPhase[i] = (BYTE)(DWORD)(int)fSum;
   } // i
}


/******************************************************************************
SRFEATUREInterpolate - Interpolate two or more SR features.

inputs
   PSRFEATURE        paSRFeature - Pointer to an array of SR features
   DWORD             dwNum - Number of features
   fp                *pafWeight - Weighting for each of dwNum. Sum = 1.0. If
                        this is NULL then assume even weight
   PSRFEATURE        pFinal - Filled in with the final feature
returns
   none
*/
void SRFEATUREInterpolate (PSRFEATURE paSRFeature, DWORD dwNum, fp *pafWeight, PSRFEATURE pFinal)
{
   DWORD i, dwNoise, j;
   for (dwNoise = 0; dwNoise < 2; dwNoise++) {
      for (i = 0; i < SRDATAPOINTS; i++) {
         fp fSum = 0;
         for (j = 0; j < dwNum; j++) {
            fSum += (fp)DbToAmplitude(dwNoise ? paSRFeature[j].acNoiseEnergy[i] : paSRFeature[j].acVoiceEnergy[i]) *
               (pafWeight ? pafWeight[j] : (1.0/(fp)dwNum));
         } // j

         if (dwNoise)
            pFinal->acNoiseEnergy[i] = AmplitudeToDb(fSum);
         else
            pFinal->acVoiceEnergy[i] = AmplitudeToDb(fSum);
      } // i
   } // dwNoise

   // average the phase
   SRFEATUREInterpolatePhase (paSRFeature, dwNum, pafWeight, pFinal);

#ifdef SRFEATUREINCLUDEPCM
   // just copy over the PCM. Interpolating will only muddy the waters too much
   dwNoise = 0;   // to remember the largest contributor
   for (j = 1; j < dwNum; j++)
      if (pafWeight[j] > pafWeight[dwNoise])
         dwNoise = j;

#ifdef SRFEATUREINCLUDEPCM_SHORT
   memcpy (pFinal->asPCM, paSRFeature[dwNoise].asPCM, sizeof(pFinal->asPCM));
#else
   memcpy (pFinal->acPCM, paSRFeature[dwNoise].acPCM, sizeof(pFinal->acPCM));
#endif

   pFinal->bPCMFill = paSRFeature[dwNoise].bPCMFill;
   pFinal->bPCMHarmFadeFull = paSRFeature[dwNoise].bPCMHarmFadeFull;
   pFinal->bPCMHarmFadeStart = paSRFeature[dwNoise].bPCMHarmFadeStart;
   pFinal->bPCMHarmNyquist = paSRFeature[dwNoise].bPCMHarmNyquist;
   pFinal->fPCMScale = paSRFeature[dwNoise].fPCMScale;
#endif
}


/******************************************************************************
SRFEATUREInterpolate - Interpolate two SR features.

inputs
   PSRFEATURE        pSRFeatureA - Pointer SR feature
   PSRFEATURE        pSRFeatureB - Pointer SR feature
   fp                fWeightA - Amount of weight for A, from 0..1
   PSRFEATURE        pFinal - Filled in with the final feature
returns
   none
*/
void SRFEATUREInterpolate (PSRFEATURE pSRFeatureA, PSRFEATURE pSRFeatureB,
                           fp fWeightA, PSRFEATURE pFinal)
{
   SRFEATURE asr[2];
   fp afWeight[2];
   asr[0] = *pSRFeatureA;
   asr[1] = *pSRFeatureB;
   afWeight[0] = fWeightA;
   afWeight[1] = 1.0 - fWeightA;

   SRFEATUREInterpolate (asr, 2, afWeight, pFinal);
}


/*************************************************************************************
SRFEATUREEnergy - Returns the energy of a SR feature. This is the total energy for
the slice.

inputs
   BOOL           fPsycho - If TRUE then use psychoacoustic energy
   PSRFEATURE     pSRFeature - feature
   BOOL           fVoicedOnly - If TRUE return only the voiced energy
returns
   fp - Energy. Linear scale.
*/
fp SRFEATUREEnergy (BOOL fPsycho, PSRFEATURE pSRFeature, BOOL fVoicedOnly)
{
   fp fEnergy = 0;
   DWORD i;

   for (i = 0; i < SRDATAPOINTS; i++) {
      fp fVoice;
      fVoice = (fp)DbToAmplitude(pSRFeature->acVoiceEnergy[i]);
      fEnergy += fVoice * fVoice * (fPsycho ? gafPsychoSquared[i] : 1.0);
   }

   if (!fVoicedOnly) for (i = 0; i < SRDATAPOINTS; i++) {
      fp fNoise;
      fNoise = (fp)DbToAmplitude(pSRFeature->acNoiseEnergy[i]);
      fEnergy += fNoise * fNoise * (fPsycho ? gafPsychoSquared[i] : 1.0);
   }

   return sqrt(fEnergy);
}


/*************************************************************************************
SRFEATUREEnergySmall - Returns the energy of a SR feature. This is the total energy for
the slice.

inputs
   BOOL           fPsycho - If TRUE then use psychoacoustic energy
   PSRFEATURESMALL     pSRFeature - feature
   BOOL           fVoicedOnly - If TRUE return only the voiced energy
   BOOL           fIncludeDelta - If TRUE then include delta
returns
   fp - Energy. Linear scale.
*/
fp SRFEATUREEnergySmall (BOOL fPsycho, PSRFEATURESMALL pSRFeature, BOOL fVoicedOnly, BOOL fIncludeDelta)
{
   fp fEnergy = 0;
   fp fScalePsycho;
   DWORD i;

   for (i = 0; i < SRDATAPOINTSSMALL; i++) {
      fp fVoice;
      fScalePsycho = (fPsycho ? gafPsychoSmallSquared[i] : 1.0);
      fVoice = (fp)DbToAmplitude(pSRFeature->acVoiceEnergyMain[i]);
      fEnergy += fVoice * fVoice * fScalePsycho;

      if (fIncludeDelta) {
         fVoice = (fp)DbToAmplitude(pSRFeature->acVoiceEnergyDelta[i]);
         fEnergy += fVoice * fVoice * fScalePsycho;
      }
   }

   if (!fVoicedOnly) for (i = 0; i < SRDATAPOINTSSMALL; i++) {
      fp fNoise;
      fScalePsycho = (fPsycho ? gafPsychoSmallSquared[i] : 1.0);
      fNoise = (fp)DbToAmplitude(pSRFeature->acNoiseEnergyMain[i]);
      fEnergy += fNoise * fNoise * fScalePsycho;

      if (fIncludeDelta) {
         fNoise = (fp)DbToAmplitude(pSRFeature->acNoiseEnergyDelta[i]);
         fEnergy += fNoise * fNoise * fScalePsycho;
      }
   }

   return sqrt(fEnergy * SRSMALL);  // BUGFIX - Put in SRSMALL so would be on smae units as
         // SRFEATUREEnergy(large).
}



/*************************************************************************************
SRFEATURECompareSmall - Compares two SR features (small).

NOTE: This uses the energy to make the comparison energy/amplitude independent.

NOTE: This assumes that both speech and model DON'T have 0 energy level

inputs
   BOOL              fPsycho - If TRUE then psychoacoustic compare
   PSRFEATURESMALL   pSRFSpeech - SR feature from the speech
   fp                fSRFSpeech - Energy of pSRFSpeech, from SRFEATUREEnergy(TRUE = psycho, ///)
   PSRFEATURESMALL   pSRFModel - SR feature #1 from the model
   fp                fSRFModel - Energy of this feature
returns
   fp - 0 if entirely the same, 1 if entirely different
*/
fp SRFEATURECompareSmall (BOOL fPsycho, PSRFEATURESMALL pSRFSpeech, fp fSRFSpeech,
                     PSRFEATURESMALL pSRFModel, fp fSRFModel)
{
   // what is the energy of the inerpolated model?
   //fSRFSpeech = max(fSRFSpeech, MINENERGYCOMPARESMALL);   // just so dont get divide by 0
   //fEnergyInterp = max(fEnergyInterp, MINENERGYCOMPARESMALL);

   // calculate the interpolated value
   double fEnergy = 0;
   // int iVoiced, iNoise, iVoicedDelta, iNoiseDelta, iEnergy = 0;
   DWORD i;

   for (i = 0; i < SRDATAPOINTSSMALL; i++) {
      double fEnergyThis =
         ((double)DbToAmplitude(pSRFModel->acVoiceEnergyMain[i]) * (double)DbToAmplitude(pSRFSpeech->acVoiceEnergyMain[i])) +
         ((double)DbToAmplitude(pSRFModel->acNoiseEnergyMain[i]) * (double)DbToAmplitude(pSRFSpeech->acNoiseEnergyMain[i])) +
         ((double)DbToAmplitude(pSRFModel->acVoiceEnergyDelta[i]) * (double)DbToAmplitude(pSRFSpeech->acVoiceEnergyDelta[i])) +
         ((double)DbToAmplitude(pSRFModel->acNoiseEnergyDelta[i]) * (double)DbToAmplitude(pSRFSpeech->acNoiseEnergyDelta[i]));
      double fScale =
         (fPsycho ? gafPsychoSmallSquared[i] : 1.0);

      fEnergy += fEnergyThis * fScale;

#if 0 // not doing any more because not really faster
      note - not handling fPsycho properly
      iVoiced = (int)pSRFModel->acVoiceEnergyMain[i] + (int)pSRFSpeech->acVoiceEnergyMain[i];
      iNoise = (int)pSRFModel->acNoiseEnergyMain[i] + (int)pSRFSpeech->acNoiseEnergyMain[i];
      iVoicedDelta = (int)pSRFModel->acVoiceEnergyDelta[i] + (int)pSRFSpeech->acVoiceEnergyDelta[i];
      iNoiseDelta = (int)pSRFModel->acNoiseEnergyDelta[i] + (int)pSRFSpeech->acNoiseEnergyDelta[i];
      iVoiced = max(iVoiced, SRABSOLUTESILENCE);
      iVoiced = min(iVoiced, SRMAXLOUDNESS);
      iNoise = max(iNoise, SRABSOLUTESILENCE);
      iNoise = min(iNoise, SRMAXLOUDNESS);
      iVoicedDelta = max(iVoicedDelta, SRABSOLUTESILENCE);
      iVoicedDelta = min(iVoicedDelta, SRMAXLOUDNESS);
      iNoiseDelta = max(iNoiseDelta, SRABSOLUTESILENCE);
      iNoiseDelta = min(iNoiseDelta, SRMAXLOUDNESS);

      iEnergy += DbToAmplitude(iVoiced) + DbToAmplitude (iNoise) + DbToAmplitude(iVoicedDelta) + DbToAmplitude (iNoiseDelta);
#endif // 0
   } // i

#if 0 // def _DEBUG  // BUGFIX - Had this accidentally set when doing test, but
      // should be exactly same.
   fSRFSpeech = SRFEATUREEnergy (pSRFSpeech);
   fSRFModel = SRFEATUREEnergy (pSRFModel);
#endif

   // fEnergy = (fp)iEnergy * (fp)0x8000; // multiply by 0x8000 to counteract value in dbtoamplitude
   fEnergy *= SRSMALL; // BUGFIX - multiply by SRSMALL since other energy calcs done that way
   fEnergy /= (fSRFSpeech * fSRFModel);
   fEnergy = 1.0 - fEnergy;

   return fEnergy;
}



/*************************************************************************************
SRFEATURECompare - Compares a SR feature from the record speech with a mix of
two SRFEATUREs from the phoneme template. This returns a linear value from 0 to
1 indicating how much error there is.

NOTE: This uses the energy to make the comparison energy/amplitude independent.

inputs
   BOOL              fPsycho - If TRUE then psychoacoustic compare
   PSRFEATURE        pSRFSpeech - SR feature from the speech
   fp                fSRFSpeech - Energy of pSRFSpeech, from SRFEATUREEnergy()
   PSRFEATURE        pSRFModel1 - SR feature #1 from the model
   fp                fSRFModel1 - Energy of this feature
   PSRFEATURE        pSRFModel2 - SR feature #2 from the model. This can be NULL if fWeightModel1=1
   fp                fSRFModel2 - Energy of this feature
   fp                fWeightModel1 - Amount to weight model 1, from 0 to 1. (1 being 100% 1)
                        The weight for the 2nd model is 1-fWeightModel1
returns
   fp - 0 if entirely the same, 1 if entirely different
*/
fp SRFEATURECompare (BOOL fPsycho, PSRFEATURE pSRFSpeech, fp fSRFSpeech,
                     PSRFEATURE pSRFModel1, fp fSRFModel1,
                     PSRFEATURE pSRFModel2, fp fSRFModel2,
                     fp fWeightModel1)
{
   // what is the energy of the inerpolated model?
   BOOL fAllFirst = (fWeightModel1 == 1);
   fp fEnergyInterp = fWeightModel1 * fSRFModel1 + (1.0 - fWeightModel1) * fSRFModel2;
   fSRFSpeech = max(fSRFSpeech, (fp)MINENERGYCOMPARE);   // just so dont get divide by 0
   fEnergyInterp = max(fEnergyInterp, (fp)MINENERGYCOMPARE);

   // calculate the interpolated value
   fp fVoiced, fNoise, fVoicedSpeech, fNoiseSpeech;
   fp fWeightModel2 = 1.0 - fWeightModel1;
   fp fEnergy = 0;
   DWORD i;

   for (i = 0; i < SRDATAPOINTS; i++) {
      if (fAllFirst)
         fVoiced = (fp)DbToAmplitude(pSRFModel1->acVoiceEnergy[i]);
      else
         fVoiced = fWeightModel1 * (fp)DbToAmplitude(pSRFModel1->acVoiceEnergy[i]) +
            fWeightModel2 * (fp)DbToAmplitude(pSRFModel2->acVoiceEnergy[i]);
      if (fAllFirst)
         fNoise = (fp)DbToAmplitude(pSRFModel1->acNoiseEnergy[i]);
      else
         fNoise = fWeightModel1 * (fp)DbToAmplitude(pSRFModel1->acNoiseEnergy[i]) +
            fWeightModel2 * (fp)DbToAmplitude(pSRFModel2->acNoiseEnergy[i]);

      fVoicedSpeech = (fp)DbToAmplitude(pSRFSpeech->acVoiceEnergy[i]);
      fNoiseSpeech = (fp)DbToAmplitude(pSRFSpeech->acNoiseEnergy[i]);

      // dot product
      fEnergy += (max(fVoiced,MINENERGYLEVEL) * max(fVoicedSpeech, MINENERGYLEVEL) +
         max(fNoise,MINENERGYLEVEL) * max(fNoiseSpeech, MINENERGYLEVEL)) *
         (fPsycho ? gafPsychoSquared[i] : 1.0);
   } // i
   fEnergy /= (fSRFSpeech * fEnergyInterp);
   fEnergy = 1.0 - fEnergy;

#if 0 // old code
   fEnergyInterp = fSRFSpeech / fEnergyInterp;  // now use this as a scale
   for (i = 0; i < SRDATAPOINTS; i++) {
      if (fAllFirst)
         fVoiced = (fp)DbToAmplitude(pSRFModel1->acVoiceEnergy[i]);
      else
         fVoiced = fWeightModel1 * (fp)DbToAmplitude(pSRFModel1->acVoiceEnergy[i]) +
            fWeightModel2 * (fp)DbToAmplitude(pSRFModel2->acVoiceEnergy[i]);
      if (fAllFirst)
         fNoise = (fp)DbToAmplitude(pSRFModel1->acNoiseEnergy[i]);
      else
         fNoise = fWeightModel1 * (fp)DbToAmplitude(pSRFModel1->acNoiseEnergy[i]) +
            fWeightModel2 * (fp)DbToAmplitude(pSRFModel2->acNoiseEnergy[i]);

      // scale so in the same units as the speech SRFeatyre
      fVoiced *= fEnergyInterp;
      fNoise *= fEnergyInterp;

      // difference compared to spoken. Note: Dont need to scale the voice energy
      // (assuming it's energy is correct) since have already scaled template
      // interpolation so at same level.
      fVoiced -= DbToAmplitude(pSRFSpeech->acVoiceEnergy[i]);
      fNoise -= DbToAmplitude(pSRFSpeech->acNoiseEnergy[i]);

      // energy
      fEnergy += fVoiced * fVoiced + fNoise * fNoise;
   } // i

   // results
   fEnergy = sqrt(fEnergy);
   fEnergy /= fSRFSpeech;
#endif // 0 - old code

   return fEnergy;
}



/*************************************************************************************
SRFEATURECompareAbsolute - Compare SR features with absolute energy comparisons.

inputs
   BOOL              fPsycho - If TRUE then psychoacoustic compare
   PSRFEATURE        pSRFSpeech - SR feature from the speech
   PSRFEATURE        pSRFModel - SR feature #2
returns
   fp - Absolute energy difference
*/
fp SRFEATURECompareAbsolute (BOOL fPsycho, PSRFEATURE pSRFSpeech, PSRFEATURE pSRFModel)
{
   double fError = 0;
   DWORD i;

   for (i = 0; i < SRDATAPOINTS; i++) {
      fp fVoiced = DbToAmplitude(pSRFSpeech->acVoiceEnergy[i]) - DbToAmplitude(pSRFModel->acVoiceEnergy[i]);
      fp fNoise = DbToAmplitude(pSRFSpeech->acNoiseEnergy[i]) - DbToAmplitude(pSRFModel->acNoiseEnergy[i]);
      fError += (fVoiced * fVoiced + fNoise * fNoise) *
         (fPsycho ? gafPsychoSquared[i] : 1.0);
   } // i
   return sqrt(fError);
}

/*************************************************************************************
SRFEATURECompareSequence - Compares a sequence of SRFEATUREs in speech to an
set of 3 phone-model SR features (representing the beginning, middle, and end of
the phone). These 3 SRFEATUREs are interpolated over the range.

The returned result is an error, in dB, that's the average error incurred per
"sample"... that is per SRFEATURE. Thus, the total error incurred is the return
value times the number of SRFEATUREs.

inputs
   BOOL              fPsycho - If TRUE then psychoacoustic compare
   PSRFEATURE        paSRFSpeech - Pointer to an array of speech features
   fp                *pafSRFSpeech - Pointer to an array of speech feature energies
                                    (from SRFEATUREEnergy)
   DWORD             dwNum - Number of elements in paSRSpeech
   fp                fMaxSpeechWindow - The maximum energy of the speech in the
                        period about 5 seconds before and after the speech to be
                        analyzed. This ensures the volume of the phoneme with
                        resepect to the overall volume of the speech is taken
                        into account
   PSRFEATURE        pSRFModel1 - Feature for the start of the model. It is
                        assumed that when the 3 SRFEATUREs were trained, they were
                        normalized where their equivalent fMaxSpeechWindow had
                        a value of 0x10000. This way it's easy to tell how loud
                        the phoneme should be compared to the energy of the
                        surrounding speech.
   fp                fSRFModel1 - Energy of pSRFModel1
   PSRFEATURE        pSRFModel2 - Middle feature
   fp                fSRModel2 - Energy of middle feature
   PSRFEATURE        pSRFModel3 - Right-most feature
   fp                fSRModel3 - Energy of right-most feature
returns
   fp - Energy in dB, where 0 is a perfect match, and higher numbers represent
         poorer matched
*/
fp SRFEATURECompareSequence (BOOL fPsycho, PSRFEATURE paSRFSpeech, fp *pafSRFSpeech, DWORD dwNum, fp fMaxSpeechWindow,
                             PSRFEATURE pSRFModel1, fp fSRFModel1,
                             PSRFEATURE pSRFModel2, fp fSRFModel2,
                             PSRFEATURE pSRFModel3, fp fSRFModel3)
{
   fMaxSpeechWindow = max(fMaxSpeechWindow, CLOSE);

   // figure out the maximum input and output energies
   fp fMaxSpeech = MINENERGYCOMPARE;
   fp fMaxModel;
   DWORD i;
   for (i = 0; i < dwNum; i++)
      fMaxSpeech = max(fMaxSpeech, pafSRFSpeech[i]);
   fMaxModel = max(max(fSRFModel1, fSRFModel2), fSRFModel3);
   fMaxModel = max(fMaxModel, MINENERGYCOMPARE);

   // calculate the energies...
   fp fTotal = 0;
   fp fDiff, fDiff2;
   DWORD dwMid = dwNum / 2;
   for (i = 0; i < dwNum; i++) {
      // figure how much to weight
      PSRFEATURE p1, p2;
      fp f1, f2;
      fp fWeight1;
      if (dwNum == 1) {
         p1 = pSRFModel2;
         f1 = fSRFModel2;
         p2 = NULL;
         f2 = 0;
         fWeight1 = 1;
      }
      else if (i < dwMid) {
         fWeight1 = 1.0 - (fp)i / ((fp)(dwNum-1)/2.0);
         p1 = pSRFModel1;
         p2 = pSRFModel2;
         f1 = fSRFModel1;
         f2 = fSRFModel2;
      }
      else {
         fWeight1 = 2 - (fp)i / ((fp)(dwNum-1)/2.0);
         p1 = pSRFModel2;
         p2 = pSRFModel3;
         f1 = fSRFModel2;
         f2 = fSRFModel3;
      }

      // figure out the difference ignoring energy
      fp fSlice = SRFEATURECompare (fPsycho, paSRFSpeech + i, pafSRFSpeech[i],
         p1, f1, p2, f2, fWeight1);

      // convert this to log... where positive numbers are more energy
      //fSlice = 1.0 - fSlice;
      //fSlice = max(fSlice, CLOSE);  // so not too pad
      //fSlice = -log10(fSlice) * 20.0;
      //fSlice = min(fSlice, 80);
         // BUGFIX - Was close, but allowed for too many dB difference.
         // Changed to larger number, maxes out at 80db error
      fSlice = max(fSlice, 0);
      fSlice *= 40;  // BUGFIX - Provide a 40 "db" range

      // find the error between the slice's energy and the energy at
      // the models's max. This is (assuming that the the phone and the
      // speech segment were normalized) the extra error that needs to be
      // added to the fSlice error. However, since a difference in volume
      // is not as bad as a difference in the basic audio (figured out by fSlice),
      // only use about 1/2 this as error for the score
      fDiff = f1 * fWeight1 + (1.0 - fWeight1) * f2;
      fDiff /= fMaxModel;; // normalized so if at max = 1.0
      fDiff2 = max(pafSRFSpeech[i],MINENERGYCOMPARE) / fMaxSpeech;  // normalized so if at max = 1.0
      fDiff = max(fDiff, EPSILON);
      fDiff2 = max(fDiff2, EPSILON);
      fDiff = fabs(log10(fDiff / fDiff2)) * 20.0;  // so know error between these two volumes
      fSlice += fDiff; // * .75;   // add 3/4 of this

      // add this to the total
      fTotal += fSlice;
   } // i

   // scale the total by the number of frames to get the average
   fTotal /= (fp)dwNum;

   // now, find the error between the max of the dwNum speech and the max
   // of the speech within a 10 second window...
   fp fDiffSpeech = fMaxSpeech / fMaxSpeechWindow;
   fDiffSpeech = max(fDiffSpeech, CLOSE);

   // and the difference between the max phone energy and the equivalent
   // that it should have been normalized to
   fp fDiffModel = fMaxModel / (fp)0x10000;
   fDiffModel = max(fDiffModel, CLOSE);

   // what is the ratio for this...
   fDiff = fDiffSpeech / fDiffModel;

   // convert this to dB - but take absolute difference
   // add only half of this to the overall error
   fDiff = fabs(log10(fDiff)) *20.0;
   fTotal += fDiff / 2.0;

   return fTotal;
}




/*************************************************************************************
SRFEATUREConvert - Converts from small to large srfeature, or vice versa.

inputs
   PSRFEATURESMALL      pFrom - From
   PSRFEATURE           pTo - To
   BOOL                 fIncludeDelta - If TRUE, include delta in high SRFEATURE
returns
   none
*/
void SRFEATUREConvert (PSRFEATURESMALL pFrom, PSRFEATURE pTo, BOOL fIncludeDelta)
{
   DWORD i, dwIndex;
   fp fLeft, fRight;
   for (i = 0; i < SRDATAPOINTS; i++) {
      if (fIncludeDelta)
         dwIndex = i * 2;
      else
         dwIndex = i;
      fp fIndex = (fp)(dwIndex % SRDATAPOINTS) / SRSMALL;

      DWORD dwLeft = (DWORD)fIndex;
      DWORD dwRight = dwLeft+1;
      if (dwRight >= SRDATAPOINTSSMALL)
         dwRight = SRDATAPOINTSSMALL-1;
      fIndex -= dwLeft;
      fp fIndexInv = 1.0 - fIndex;

      // voiced
      if (dwIndex >= SRDATAPOINTS) {
         fLeft = (fp)DbToAmplitude(pFrom->acVoiceEnergyDelta[dwLeft]);
         fRight =(fp)DbToAmplitude(pFrom->acVoiceEnergyDelta[dwRight]);
      }
      else {
         fLeft = (fp)DbToAmplitude(pFrom->acVoiceEnergyMain[dwLeft]);
         fRight =(fp)DbToAmplitude(pFrom->acVoiceEnergyMain[dwRight]);
      }
      pTo->acVoiceEnergy[i] = AmplitudeToDb (fLeft * fIndexInv + fRight * fIndex);

      // unvoiced
      if (dwIndex >= SRDATAPOINTS) {
         fLeft = (fp)DbToAmplitude(pFrom->acNoiseEnergyDelta[dwLeft]);
         fRight =(fp)DbToAmplitude(pFrom->acNoiseEnergyDelta[dwRight]);
      }
      else {
         fLeft = (fp)DbToAmplitude(pFrom->acNoiseEnergyMain[dwLeft]);
         fRight =(fp)DbToAmplitude(pFrom->acNoiseEnergyMain[dwRight]);
      }
      pTo->acNoiseEnergy[i] = AmplitudeToDb (fLeft * fIndexInv + fRight * fIndex);
   } // i

   memset (&pTo->abPhase, 0, sizeof(pTo->abPhase));
}


/*************************************************************************************
SRFEATUREScale - Scales the SRFeature by the given amount.

inputs
   PSRFEATURESMALL   pSR - Features
   fp                fScale - Amount to scale, linear. 1.0 = no change
returns
   none
*/
void SRFEATUREScale (PSRFEATURESMALL pSR, fp fScale)
{
   DWORD i;
   int iScale = AmplitudeToDb (fScale * (fp)0x8000);
   int iTemp;

   for (i = 0; i < SRDATAPOINTSSMALL; i++) {
      // voiced
      iTemp = (int)pSR->acVoiceEnergyMain[i] + iScale;
      iTemp = max(iTemp, SRABSOLUTESILENCE);
      iTemp = min(iTemp, SRMAXLOUDNESS);
      pSR->acVoiceEnergyMain[i] = (char)iTemp;

      // noise
      iTemp = (int)pSR->acNoiseEnergyMain[i] + iScale;
      iTemp = max(iTemp, SRABSOLUTESILENCE);
      iTemp = min(iTemp, SRMAXLOUDNESS);
      pSR->acNoiseEnergyMain[i] = (char)iTemp;

      // voiced
      iTemp = (int)pSR->acVoiceEnergyDelta[i] + iScale;
      iTemp = max(iTemp, SRABSOLUTESILENCE);
      iTemp = min(iTemp, SRMAXLOUDNESS);
      pSR->acVoiceEnergyDelta[i] = (char)iTemp;

      // noise
      iTemp = (int)pSR->acNoiseEnergyDelta[i] + iScale;
      iTemp = max(iTemp, SRABSOLUTESILENCE);
      iTemp = min(iTemp, SRMAXLOUDNESS);
      pSR->acNoiseEnergyDelta[i] = (char)iTemp;
   } // i
}



/*************************************************************************************
SRFEATUREScale - Scales the SRFeature by the given amount.

inputs
   PSRFEATURE        pSR - Features
   fp                fScale - Amount to scale, linear. 1.0 = no change
returns
   none
*/
void SRFEATUREScale (PSRFEATURE pSR, fp fScale)
{
   DWORD i;
   int iScale = AmplitudeToDb (fScale * (fp)0x8000);
   int iTemp;

   for (i = 0; i < SRDATAPOINTS; i++) {
      // voiced
      iTemp = (int)pSR->acVoiceEnergy[i] + iScale;
      iTemp = max(iTemp, SRABSOLUTESILENCE);
      iTemp = min(iTemp, 127);
      pSR->acVoiceEnergy[i] = (char)iTemp;

      // noise
      iTemp = (int)pSR->acNoiseEnergy[i] + iScale;
      iTemp = max(iTemp, SRABSOLUTESILENCE);
      iTemp = min(iTemp, 127);
      pSR->acNoiseEnergy[i] = (char)iTemp;
   } // i
}


/*************************************************************************************
SRFEATUREConvert - Converts from large to small srfeature, or vice versa.

In the process, this makes sure all the levels are above -70 db.

inputs
   PSRFEATURE        pFrom - From
   PSRFEATURE        pPrev - Previous SR feature, for delta. If NULL then assume same as this
   PSRFEATURESMALL   pTo - To
returns
   none
*/
void SRFEATUREConvert (PSRFEATURE pFrom, PSRFEATURE pPrev, PSRFEATURESMALL pTo)
{
   DWORD i;
   fp afVoice[SRDATAPOINTS], afNoise[SRDATAPOINTS], afVoicePrev[SRDATAPOINTS], afNoisePrev[SRDATAPOINTS];
   for (i = 0; i < SRDATAPOINTS; i++) {
      afVoice[i] = DbToAmplitude (max(pFrom->acVoiceEnergy[i], SRNOISEFLOOR));
      afNoise[i] = DbToAmplitude (max(pFrom->acNoiseEnergy[i], SRNOISEFLOOR));
      afVoicePrev[i] = DbToAmplitude (max(pPrev ? pPrev->acVoiceEnergy[i] : pFrom->acVoiceEnergy[i], SRNOISEFLOOR));
      afNoisePrev[i] = DbToAmplitude (max(pPrev ? pPrev->acNoiseEnergy[i] : pFrom->acNoiseEnergy[i], SRNOISEFLOOR));
   } // i

   // windowed
   int j;
   DWORD dwWeightTotal, dwWeight;
   fp fSumVoice, fSumNoise, fSumVoicePrev, fSumNoisePrev;
   for (i = 0; i < SRDATAPOINTSSMALL; i++) {
      int iOffset = (int)i * SRSMALL;
      dwWeightTotal = 0;
      fSumVoice = fSumNoise = fSumVoicePrev = fSumNoisePrev = 0;

      for (j = iOffset-SRSMALL; j <= iOffset+SRSMALL; j++) { // specifically use <=
         if ((j < 0) || (j >= SRDATAPOINTS))
            continue;

         dwWeight = SRSMALL+1 - (DWORD) abs(iOffset - j);
         dwWeightTotal += dwWeight;
         fSumVoice += (fp)dwWeight * afVoice[j];
         fSumNoise += (fp)dwWeight * afNoise[j];
         fSumVoicePrev += (fp)dwWeight * afVoicePrev[j];
         fSumNoisePrev += (fp)dwWeight * afNoisePrev[j];
      } // j

      fSumVoice /= (fp)dwWeightTotal;
      fSumNoise /= (fp)dwWeightTotal;
      fSumVoicePrev /= (fp)dwWeightTotal;
      fSumNoisePrev /= (fp)dwWeightTotal;

      pTo->acVoiceEnergyMain[i] = AmplitudeToDb (fSumVoice);
      pTo->acNoiseEnergyMain[i] = AmplitudeToDb (fSumNoise);
      pTo->acVoiceEnergyDelta[i] = AmplitudeToDb (fabs(fSumVoice - fSumVoicePrev));
      pTo->acNoiseEnergyDelta[i] = AmplitudeToDb (fabs(fSumNoise - fSumNoisePrev));
   } // i
}


/*************************************************************************************
SRDETAILEDPHASEShift - Shifts the voiced portion up/down. Does NOT affect the harmonic
phase.

inputs
   PSRDETAILEDPHASE  pSDPOrig - original
   int               iShiftUp - Number of units to shift up/down. Max SRDATAPOINTSDETAILED
   PSRDETAILEDPHASE  pSDPDest - Destination filled with shifted phases. Can be pSDPOrig.
*/
void SRDETAILEDPHASEShift (PSRDETAILEDPHASE pSDPOrig, int iShiftUp, PSRDETAILEDPHASE pSDPDest)
{
   iShiftUp = max(iShiftUp, -SRDATAPOINTSDETAILED);
   iShiftUp = min(iShiftUp, SRDATAPOINTSDETAILED);

   // copy
   if (pSDPDest != pSDPOrig)
      *pSDPDest = *pSDPOrig;

   DWORD i;

   if (iShiftUp > 0) {
      memmove (&pSDPDest->afVoicedPhase[iShiftUp][0], &pSDPDest->afVoicedPhase[0][0],
         sizeof(pSDPDest->afVoicedPhase[0]) * (SRDATAPOINTSDETAILED - iShiftUp));

      for (i = 1; i < (DWORD)iShiftUp; i++)
         memcpy (&pSDPDest->afVoicedPhase[i][0], &pSDPDest->afVoicedPhase[0][0], sizeof(pSDPDest->afVoicedPhase[0]));
   }
   else if (iShiftUp < 0) {
      memmove (&pSDPDest->afVoicedPhase[0][0], &pSDPDest->afVoicedPhase[-iShiftUp][0],
         sizeof(pSDPDest->afVoicedPhase[0]) * (SRDATAPOINTSDETAILED + iShiftUp));

      for (i = (DWORD)(SRDATAPOINTSDETAILED + iShiftUp); i < SRDATAPOINTSDETAILED-1; i++)
         memcpy (&pSDPDest->afVoicedPhase[i][0], &pSDPDest->afVoicedPhase[SRDATAPOINTSDETAILED-1][0], sizeof(pSDPDest->afVoicedPhase[0]));
   }
}


/*************************************************************************************
SRDETAILEDPHASEDotProd - Do a dot product between two SRDETAILEDPHASE.afVoicedPhase.
Use this to see how much energy is in common.

inputs
   BOOL              fPsycho - If TRUE use psychoacoustic compare
   PSRDETAILEDPHASE  pSDPA - One
   PSRDETAILEDPHASE  pSDPB - Two
returns
   fp - Energy.
*/
__inline fp SRDETAILEDPHASEDotProd (BOOL fPsycho, PSRDETAILEDPHASE pSDPA, PSRDETAILEDPHASE pSDPB)
{
   double fSum = 0.0;
   DWORD i;
   for (i = 0; i < SRDATAPOINTSDETAILED; i++)
      fSum += (
         (double)pSDPA->afVoicedPhase[i][0] * (double)pSDPB->afVoicedPhase[i][0] +
         (double)pSDPA->afVoicedPhase[i][1] * (double)pSDPB->afVoicedPhase[i][1]) *
         (fPsycho ? gafPsychoDetailedSquared[i] : 1.0);

   return fSum;
}


/*************************************************************************************
SRDETAILEDPHASEEnergy - Calculated the energy in the afVoicedPhase section
of SRDETAILEDPHASE only

inputs
   BOOL              fPsycho - If TRUE use psychoacoustic compare
   PSRDETAILEDPHASE  pSDP - Has the phase
returns
   fp - Energy.
*/
fp SRDETAILEDPHASEEnergy (BOOL fPsycho, PSRDETAILEDPHASE pSDP)
{
   return sqrt(SRDETAILEDPHASEDotProd(fPsycho, pSDP, pSDP));
}


/*************************************************************************************
SRDETAILEDPHASEShiftRange - Shifts a SRDETAILEDPHASE over a range of values,
comparing it with another SRDETAILEDPHASE, and returns the shift that matches
the best.

inputs
   PSRDETAILEDPHASE  pSDPShifted - One to be shifted
   PSRDETAILEDPHASE  pSDPFixed- Fixed, NOT shifted
   DWORD             dwMaxShift - Maximum shift up/down. Max SRDATAPOINTSDETAILED
returns
   int - How much pSDPShifted should be shifted UP
*/
int SRDETAILEDPHASEShiftRange (PSRDETAILEDPHASE pSDPShifted, PSRDETAILEDPHASE pSDPFixed,
                               DWORD dwMaxShift)
{
   fp fBest = 0;
   int iBest = -(int)dwMaxShift*2;

   int iShift;
   fp fScore;
   SRDETAILEDPHASE SDPShift;
   for (iShift = -(int)dwMaxShift; iShift <= (int)dwMaxShift; iShift++) {
      SRDETAILEDPHASEShift (pSDPShifted, iShift, &SDPShift);
      fScore = SRDETAILEDPHASEDotProd (TRUE, &SDPShift, pSDPFixed);
         // BUGFIX - Using psychoacoustic

      if ((iShift == -(int)dwMaxShift) || (fScore > fBest)) {
         // found a match
         iBest = iShift;
         fBest = fScore;
      }
   } // iShift

   return iBest;
}



/*************************************************************************************
SRDETAILEDPHASEShiftAndBlend - Use this at a join between two units.
It first shifts the phase up/down to minimize the breaks, and then does
a blend. ONLY does SRDETAILEDPHASE.afVoicedPhase

inputs
   PSRDETAILEDPHASE  paSDP - Array of phases. Modified in place
   DWORD             dwNum - Number of phase
   DWORD             dwIndex - Index into the array with the start of the right phoneme
                                 segment is. The left one to blend ends at dwIndex-1
   DWORD             dwWidth - Width of the window, in frames (to the left and right).
                              When blurring phase, used as the bottom harmonic width.
   DWORD             dwWidthTop - Width of the window for the top harmonic. Only used for blurring phase.
   BOOL              fShift - If TRUE then shift, otherwise don't
   BOOL              fBlend - If TRUE then blend, otherwise don't
returns
   none
*/
#define SRDEFAILTPHASE_MAXSHIFT     (PHASEDETAILED * SRPOINTSPEROCTAVE / 4)      // 1/4 of an octave up/down

void SRDETAILEDPHASEShiftAndBlend (PSRDETAILEDPHASE paSDP, DWORD dwNum, DWORD dwIndex,
                                   DWORD dwWidth, DWORD dwWidthTop, BOOL fShift, BOOL fBlend)
{
   if (!fShift && !fBlend)
      return;  // doing nothing
   if ((dwIndex < 1) || (dwIndex >= dwNum))
      return;  // do nothign

   // determine the weight of left and right
   fp fWeightLeft = CLOSE, fWeightRight = CLOSE;
   int iOffset, iCur;
   fp fScale, fEnergy;
   for (iOffset = -(int)dwWidth; iOffset < (int)dwWidth; iOffset++) {
      iCur = iOffset + (int)dwIndex;
      if ((iCur < 0) || (iCur >= (int)dwNum))
         continue;   // out of range

      fEnergy = SRDETAILEDPHASEEnergy(FALSE, paSDP + iCur);
         // DON'T use psychoacoustic here
      if (iOffset < 0) {
         fScale = (int)dwWidth + 1 + iOffset;
         fWeightLeft += fScale * fEnergy;
      }
      else {
         fScale = (int)dwWidth - iOffset;
         fWeightRight += fScale * fEnergy;
      }
   } // iOffset
   fp fWeightSum = fWeightLeft + fWeightRight;
   fWeightRight = fWeightLeft / fWeightSum;  // intentionally using fWeightLeft so that right won't
         // change much if it has much more energy than the left
   fWeightLeft = 1.0 - fWeightRight;

   // shift
   if (fShift) {
      int iShiftLeftTotal = SRDETAILEDPHASEShiftRange (paSDP + (dwIndex-1), paSDP + dwIndex, SRDEFAILTPHASE_MAXSHIFT);
      int iShift;

      // shift individuals
      for (iOffset = -(int)dwWidth; iOffset < (int)dwWidth; iOffset++) {
         iCur = iOffset + (int)dwIndex;
         if ((iCur < 0) || (iCur >= (int)dwNum))
            continue;   // out of range

         if (iOffset < 0) {
            fScale = (fp)((int)dwWidth + 1 + iOffset) / (fp)dwWidth;
            fScale *= fWeightLeft;
            iShift = (int)(fScale * iShiftLeftTotal);
         }
         else {
            fScale = (fp) ((int)dwWidth - iOffset) / (fp)dwWidth;
            fScale *= fWeightRight;
            iShift = (int)(fScale * (-iShiftLeftTotal)); // shift in opposite direction
         }

         // shift
         SRDETAILEDPHASEShift (paSDP + iCur, iShift, paSDP + iCur);
      } // iOffset
   } // if fShift

   // blur
   DWORD i;
   if (fBlend) {
      // copy over original phases
      SRDETAILEDPHASE aSDP[2];
      memcpy (aSDP, paSDP + (dwIndex-1), sizeof(aSDP));

      // loop over points to blur
      for (i = 0; i < SRDATAPOINTSDETAILED; i++) {
         // how big it the window? from width at base to 1/2 width at top
         DWORD dwWindow = ((SRDATAPOINTSDETAILED - i) * dwWidth + i * dwWidthTop + SRDATAPOINTSDETAILED/2) / SRDATAPOINTSDETAILED;
         dwWindow = max(dwWindow, 1);

         for (iOffset = -(int)dwWindow; iOffset < (int)dwWindow; iOffset++) {
            iCur = iOffset + (int)dwIndex;
            if ((iCur < 0) || (iCur >= (int)dwNum))
               continue;   // out of range

            PSRDETAILEDPHASE pSDPReverse;
            if (iOffset < 0) {
               // fscale is amount of reverse to incluce
               fScale = (fp)((int)dwWindow + 1 + iOffset) / (fp)dwWindow;
               fScale *= fWeightLeft;
               pSDPReverse = &aSDP[1];
            }
            else {
               // fscale is amount of reverse to incluce
               fScale = (fp) ((int)dwWindow - iOffset) / (fp)dwWindow;
               fScale *= fWeightRight;
               pSDPReverse = &aSDP[0];
            }
            paSDP[iCur].afVoicedPhase[i][0] = paSDP[iCur].afVoicedPhase[i][0] * (1.0 - fScale) + pSDPReverse->afVoicedPhase[i][0] * fScale;
            paSDP[iCur].afVoicedPhase[i][1] = paSDP[iCur].afVoicedPhase[i][1] * (1.0 - fScale) + pSDPReverse->afVoicedPhase[i][1] * fScale;
         } // iOffset

      } // i
   } // if fBlur
}


/*************************************************************************************
SRDETAILEDPHASEFromSRFEATURE - Converts a SRFEATURE and pitch to a SRDETAILEDPHASE,
which can be used for better pitch bends and blending.

inputs
   PSRFEATURE        pSRF - Feature
   fp                fPitch - Pitch, in Hz
   PSRDETAILEDPHASE  pSDP - Filled in
returns
   none
*/
void SRDETAILEDPHASEFromSRFEATURE (PSRFEATURE pSRF, fp fPitch, PSRDETAILEDPHASE pSDP)
{
   DWORD i, j;
   DWORD dwLast = (DWORD)-1;
   fp fSinLast, fCosLast;
   fp fDeltaSin, fDeltaCos;
   for (i = 0; i < SRPHASENUM; i++) {
      // find the voiced element
      fp fIndex = log(fPitch * (fp)(i+1) / SRBASEPITCH) / log(2.0) * (fp)(SRPOINTSPEROCTAVE * PHASEDETAILED);
      fIndex = max(fIndex, 0);
      fIndex = min(fIndex, SRDATAPOINTSDETAILED-1);
      DWORD dwIndex = (DWORD)fIndex;

      // what's this amplitude?
      fp fAmplitude = DbToAmplitude (pSRF->acVoiceEnergy[dwIndex / PHASEDETAILED]);
      fAmplitude = max(fAmplitude, CLOSE);   // always have something

      // figure out sin/cos
      fp fPhase = (fp)pSRF->abPhase[i] / (fp)256.0 * 2.0 * PI;
      fp fSin = sin(fPhase);
      fp fCos = cos(fPhase);

      // write it out for harmonics
      pSDP->afHarmPhase[i][0] = fSin * fAmplitude;
      pSDP->afHarmPhase[i][1] = fCos * fAmplitude;

      // fill in the phase elements
      if (dwLast == (DWORD)-1) {
         fSinLast = fSin;
         fCosLast = fCos;
         dwLast = 0;
      }
      fDeltaSin = (fSin - fSinLast) / (fp)(dwIndex - dwLast + 1);
      fDeltaCos = (fCos - fCosLast) / (fp)(dwIndex - dwLast + 1);
      fSinLast += fDeltaSin;
      fCosLast += fDeltaCos;
      for (j = dwLast; j <= dwIndex; j++, fSinLast += fDeltaSin, fCosLast += fDeltaCos) {
         fAmplitude = DbToAmplitude (pSRF->acVoiceEnergy[j / PHASEDETAILED]);
         fAmplitude = max(fAmplitude, CLOSE);   // always have something
         pSDP->afVoicedPhase[j][0] = fSinLast * fAmplitude;
         pSDP->afVoicedPhase[j][1] = fCosLast * fAmplitude;
      } // i

      dwLast = dwIndex;
      fSinLast = fSin;
      fCosLast = fCos;

      // potentially finish off
      if (i >= SRPHASENUM-1) for (j = dwIndex+1; j < SRDATAPOINTSDETAILED; j++) {
         fAmplitude = DbToAmplitude (pSRF->acVoiceEnergy[j / PHASEDETAILED]);
         fAmplitude = max(fAmplitude, CLOSE);   // always have something
         pSDP->afVoicedPhase[j][0] = fSinLast * fAmplitude;
         pSDP->afVoicedPhase[j][1] = fCosLast * fAmplitude;
      } // j
   } // i

   // done
}



/*************************************************************************************
SRFEATUREPhaseInterp - Interpolates phases.

inputs
   DWORD             dwHarmonic - Specific harmonic number. If -1 then do all harmonics
   fp                fAlpha - If 0.0 then all A, if 1.0 then all B
   PSRFEATURE        pSRFA - A feature
   PSRFEATURE        pSRFB - B feature
   PSRFEATURE        pSRFDest - Detination feature
returns
   none
*/
void SRFEATUREPhaseInterp (DWORD dwHarmonic, fp fAlpha, PSRFEATURE pSRFA, PSRFEATURE pSRFB, PSRFEATURE pSRFDest)
{
   fp fAlphaInv = 1.0 - fAlpha;

   DWORD i;
   for (i = (dwHarmonic == (DWORD)-1) ? 0 : dwHarmonic; i < ((dwHarmonic == (DWORD)-1) ? SRPHASENUM : (dwHarmonic+1)); i++) {
      // A
      fp fPhase = (fp)pSRFA->abPhase[i] / (fp)256.0 * 2.0 * PI;
      fp fSinA = sin(fPhase);
      fp fCosA = cos(fPhase);

      // B
      fPhase = (fp)pSRFB->abPhase[i] / (fp)256.0 * 2.0 * PI;
      fp fSinB = sin(fPhase);
      fp fCosB = cos(fPhase);

      // Sum
      fp fSinSum = fAlphaInv * fSinA + fAlpha * fSinB;
      fp fCosSum = fAlphaInv * fCosA + fAlpha * fCosB;

      fPhase = atan2(fSinSum, fCosSum);
      pSRFDest->abPhase[i] = (BYTE) myfmod(fPhase / (2.0 * PI) * 256.0 + 0.5, 256.0);
   } // i
}

/*************************************************************************************
SRDETAILEDPHASEToSRFEATURE - Converts a SRDETAILEDPHASE and pitch to a SRFEATURE,
which can be used for better pitch bends and blending.

inputs
   PSRDETAILEDPHASE  pSDP - Already valid, from SRDETAILEDPHASEFromSRFEATURE()
   fp                fPitch - Pitch after bend, in Hz
   PSRFEATURE        pSRF - Feature to be filled in. This ONLY modifies abPhase.
returns
   none
*/
void SRDETAILEDPHASEToSRFEATURE (PSRDETAILEDPHASE pSDP, fp fPitch, PSRFEATURE pSRF)
{
   DWORD i;
   fp fSin, fCos, fPhase;
   for (i = 0; i < SRPHASENUM; i++) {
      // BUGFIX - take tis out so can let fundamental phase through
      //if (!i) {
      //   pSRF->abPhase[i] = 0;
      //   continue;
      //}

      // find the voiced element
      fp fIndex = log(fPitch * (fp)(i+1) / SRBASEPITCH) / log(2.0) * (fp)(SRPOINTSPEROCTAVE * PHASEDETAILED);
      fIndex = max(fIndex, 0);
      fIndex = min(fIndex, SRDATAPOINTSDETAILED-1);
      DWORD dwIndex = (DWORD)fIndex;

      // average the sin/cos from both the phase and voiced
      fSin = (1.0 - DETAILEDPHASE_VOICEDPHASESCALE) * pSDP->afHarmPhase[i][0] + DETAILEDPHASE_VOICEDPHASESCALE * pSDP->afVoicedPhase[dwIndex][0];
      fCos = (1.0 - DETAILEDPHASE_VOICEDPHASESCALE) * pSDP->afHarmPhase[i][1] + DETAILEDPHASE_VOICEDPHASESCALE * pSDP->afVoicedPhase[dwIndex][1];

      fPhase = atan2(fSin, fCos);

      pSRF->abPhase[i] = (BYTE) myfmod(fPhase / (2.0 * PI) * 256.0 + 0.5, 256.0);
   } // i
}


/*************************************************************************************
CalcPscyhoacousticWeights - Calculates the psychoacoustic weights. Call this
at DLL startup for initialization.
*/
#define PSYCH_BEGINPEAK          3     // octave 3
#define PSYCH_ENDPEAK            6     // octabe 6
#define PSYCH_LOWDB              -12   // 12 dB down at 0th octave
#define PSYCH_HIGHDB             -6    // 6 db down at 7th octave

void CalcPscyhoacousticWeights (void)
{
   DWORD i;
   fp fOctave, fScale;
   for (i = 0; i < SRDATAPOINTSDETAILED; i++) {
      fOctave = (fp)i / (PHASEDETAILED * SRPOINTSPEROCTAVE);

      if (fOctave < PSYCH_BEGINPEAK) {
         fOctave /= (fp)PSYCH_BEGINPEAK;
         fScale = (1.0 - fOctave) * (fp)PSYCH_LOWDB;
      }
      else if (fOctave > PSYCH_ENDPEAK) {
         fOctave = (fOctave - PSYCH_ENDPEAK) / (fp) (SROCTAVE - PSYCH_ENDPEAK);
         fScale = fOctave * PSYCH_HIGHDB;
      }
      else
         fScale = 0; // dB

      // BUGFIX - Scale by dB so approximately energy neutral with non-psycho
      fScale += 3.0;

      fScale = pow(10, fScale / 20.0);

      // detailed
      gafPsychoDetailed[i] = fScale;
      gafPsychoDetailedSquared[i] = gafPsychoDetailed[i] * gafPsychoDetailed[i];

      // normal quality
      if (!(i % (SRDATAPOINTSDETAILED / SRDATAPOINTS))) {
         gafPsycho[i / (SRDATAPOINTSDETAILED / SRDATAPOINTS)] = gafPsychoDetailed[i];
         gafPsychoSquared[i / (SRDATAPOINTSDETAILED / SRDATAPOINTS)] = gafPsychoDetailedSquared[i];
      }

      // small
      if (!(i % (SRDATAPOINTSDETAILED / SRDATAPOINTSSMALL))) {
         gafPsychoSmall[i / (SRDATAPOINTSDETAILED / SRDATAPOINTSSMALL)] = gafPsychoDetailed[i];
         gafPsychoSmallSquared[i / (SRDATAPOINTSDETAILED / SRDATAPOINTSSMALL)] = gafPsychoDetailedSquared[i];
      }

   } // i
}

/*************************************************************************************
SRFEATUREToHarmonics - Converts a SRFeature to harmonic energies.

inputs
   PSRFEATURE        pSRF - Feature
   DWORD             dwHarmonics - Number of harmonics to generate (including 0 harmonic).
   float             *pafVoiced - Array of dwHarmonics for voiced
   float             *pafNoise - Array of dwHarmonics for noise
   fp                fPitch - Pitch, in Hz
returns
   none
*/
void SRFEATUREToHarmonics (PSRFEATURE pSRF, DWORD dwHarmonics, float *pafVoiced, float *pafNoise, fp fPitch)
{
   // set 0 harmonics to 0
   pafVoiced[0] = pafNoise[0] = 0;

   DWORD i;
   for (i = 1; i < dwHarmonics; i++) {
      fp fIndex = log(fPitch * (fp)i / SRBASEPITCH) / log(2.0) * (fp)SRPOINTSPEROCTAVE;
      fIndex = max(fIndex, 0);
      fIndex = min(fIndex, SRDATAPOINTS-1);

      // NOTE: Not bothering to interpolate and get accurare

      DWORD dwIndex = (DWORD)(fIndex + 0.5);

      // adjust energy
      int iDBAdjust = ((int)dwIndex - (int)SRDATAPOINTS/2) * 6 / SRPOINTSPEROCTAVE;

      // what's this amplitude?
      int iEnergy;
      iEnergy = (char)pSRF->acVoiceEnergy[dwIndex] - iDBAdjust;
      iEnergy = max(iEnergy, -127);
      iEnergy = min(iEnergy, 127);
      pafVoiced[i] = DbToAmplitude ((char)iEnergy);

#ifdef _DEBUG
      if (iEnergy > -30)
         iEnergy += 1;
#endif

      iEnergy = (char)pSRF->acNoiseEnergy[dwIndex] - iDBAdjust;
      iEnergy = max(iEnergy, -127);
      iEnergy = min(iEnergy, 127);
      pafNoise[i] = DbToAmplitude ((char)iEnergy);
   } // i
}


/*************************************************************************************
SRFEATUREFromHarmonics - Given the harmonics, fill in the SRFEATURE.

inputs
   DWORD             dwHarmonics - Number of harmonics to generate (including 0 harmonic).
   float             *pafVoiced - Array of dwHarmonics for voiced
   float             *pafNoise - Array of dwHarmonics for noise
   fp                fPitch - Pitch, in Hz
   PSRFEATURE        pSRF - Feature, filled in
returns
   none
*/
void SRFEATUREFromHarmonics (DWORD dwHarmonics, float *pafVoiced, float *pafNoise, fp fPitch, PSRFEATURE pSRF)
{
   DWORD i, dwVoiced;
   for (i = 0; i < SRDATAPOINTS; i++) {
      fp fPitchTry = pow((fp)2.0, (fp)i / SRPOINTSPEROCTAVE) * SRBASEPITCH;
      fp fHarmonic = fPitchTry / fPitch;

      DWORD dwIndex = (DWORD)fHarmonic;
      fHarmonic -= (fp)dwIndex;
      DWORD dwIndexPlusOne = dwIndex+1;
      dwIndex = min(dwIndex, dwHarmonics-1);
      dwIndexPlusOne = min(dwIndexPlusOne, dwHarmonics-1);

      int iDBAdjust = ((int)i - (int)SRDATAPOINTS/2) * 6 / SRPOINTSPEROCTAVE;

      for (dwVoiced = 0; dwVoiced < 2; dwVoiced++) {
         float *paf = dwVoiced ? pafVoiced : pafNoise;
         fp fValue = (1.0 - fHarmonic) * paf[dwIndex] + fHarmonic * paf[dwIndexPlusOne];

         int iDb = AmplitudeToDb (fValue) + iDBAdjust;
         iDb = max(iDb, -127);
         iDb = min(iDb, 127);

         if (dwVoiced)
            pSRF->acVoiceEnergy[i] = (char)iDb;
         else
            pSRF->acNoiseEnergy[i] = (char)iDb;
      } // dwVoiced
   } // i
}

