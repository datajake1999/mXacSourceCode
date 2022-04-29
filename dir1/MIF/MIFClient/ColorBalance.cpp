/*************************************************************************************
ColorBalance.cpp - Ensures a specific brightness/dimness for an image

begun 26/4/09 by Mike Rozak.
Copyright 2009 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <zmouse.h>
#include <shlobj.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"

#define DARKINTENSITYMAX         0x50
#define DARKEXTRAINTENSITYMAX    0x20
#define LIGHTINTENSITYMIN        0xb0
#define LIGHTEXTRAINTENSITYMIN   0xe0


/*************************************************************************************
PIMAGEPIXELToIntensity - Convert a PIMAGEPIXEL to an intensity

inputs
   PIMAGEPIXEL       pip - Pixel
returns
   BYTE - Intentisy, from 0..255
*/
__inline BYTE PIMAGEPIXELToIntensity (PIMAGEPIXEL pip)
{
   // NOTE: This isn't exactly correct, but it's good enough
   return (BYTE) (((WORD)UnGamma (pip->wRed) + (WORD)UnGamma(pip->wGreen) + (WORD)UnGamma(pip->wBlue) + 2) / 3);
}

/*************************************************************************************
HistogramFill - Fills in a histogram with the count of each brightness.

inputs
   PCImage        pImage - Image
   DWORD          *padwCount - Pointer to an array of 256 values that indicate the count
                     of the number of pixels with that color
returns
   DWORD dwNum - Total count
*/
DWORD HistogramFill (PCImage pImage, DWORD *padwCount)
{
   memset (padwCount, 0, sizeof(*padwCount) * 256);

   DWORD dwNum = pImage->Width() * pImage->Height();
   DWORD i;
   PIMAGEPIXEL pip = pImage->Pixel (0, 0);
   for (i = 0; i < dwNum; i++, pip++)
      padwCount[PIMAGEPIXELToIntensity(pip)] += 1;

   return dwNum;
}


/*************************************************************************************
HistogramMedian - Find the Nth brightest pixel of the histogram, as an intensity.
If Width()*Height()/2 is passed in, this finds the median.

inputs
   DWORD          *padwCount - Array of 256 histogram points from HistogramFill()
   DWORD          dwPixel - Nth brightest pixel.
returns
   BYTE - Intensity
*/
BYTE HistogramMedian (DWORD *padwCount, DWORD dwPixel)
{
   DWORD dwIntensity;
   for (dwIntensity = 0; dwIntensity < 256; dwIntensity++) {
      if (dwPixel < padwCount[dwIntensity])
         return (BYTE)dwIntensity;
      
      // else, reduce pixel number
      dwPixel -= padwCount[dwIntensity];
   } // dwIntesity

   // if get here, return 255
   return 255;
}

/*************************************************************************************
HistogramErrorForIntensity - Given a specific intensity that gets moved to a
different intensity location, this returns the error.

inputs
   DWORD       *padwCount - Histogram. 256 elements
   DWORD       dwIntensityOrig - Original intensity (from 0..255)
   DWORD       dwIntensityNew - New intensity (from 0..255)
returns
   __int64 - Error
*/
__int64 HistogramErrorForIntensity (DWORD *padwCount, DWORD dwIntensityOrig, DWORD dwIntensityNew)
{
   return (__int64)(padwCount[dwIntensityOrig]) * (__int64)abs((int)dwIntensityOrig - (int)dwIntensityNew);
}

/*************************************************************************************
IntensityRemap - Remaps an intensity.

inputs
   DWORD       dwIntensity - Original intensity, 0..255
   DWORD       dwStart - Start of the squash, 0..255
   DWORD       dwEnd - End of the squash. 0..256 (exclusive)
   DWORD       dwStartTo - Squash the start to
   DWORD       dwEndTo - Squash the end to
   DWORD       dwTruncStart - If less than this value, then automatically truncated (after squashing)
   DWORD       dwTruncEnd - If > this value then autoamtically truncated to this (after squashing)
returns
   DWORD - New intensity index.
*/
DWORD IntensityRemap (DWORD dwIntensity, DWORD dwStart, DWORD dwEnd, DWORD dwStartTo, DWORD dwEndTo,
                      DWORD dwTruncStart, DWORD dwTruncEnd)
{
   fp fAlpha;
   if (dwEnd <= dwStart)
      fAlpha = 0.0;
   else
      fAlpha = (fp)((int)dwIntensity - (int)dwStart) / (fp)(dwEnd - dwStart);
   fp fNew = fAlpha * (fp)(dwEndTo - dwStartTo) + (fp)dwStartTo;
   fNew = floor(fNew + 0.5);  // to round
   int iNew = (int)fNew;
   if (iNew < (int)dwTruncStart)
      iNew = (int)dwTruncStart;
   if (iNew > (int)dwTruncEnd)
      iNew = (int)dwTruncEnd;

   return (DWORD)iNew;
}


/*************************************************************************************
HistogramErrorSquash - What's the error if squash a section of the histogram.

inputs
   DWORD       *padwCount - Histogram. 256 elements
   DWORD       dwStart - Start of the squash, 0..255
   DWORD       dwEnd - End of the squash. 0..256 (exclusive)
   DWORD       dwStartTo - Squash the start to
   DWORD       dwEndTo - Squash the end to
   DWORD       dwTruncStart - If less than this value, then automatically truncated (after squashing)
   DWORD       dwTruncEnd - If > this value then autoamtically truncated to this (after squashing)
returns
   __int64 - Error
*/
__int64 HistogramErrorSquash (DWORD *padwCount, DWORD dwStart, DWORD dwEnd, DWORD dwStartTo,
                              DWORD dwEndTo, DWORD dwTruncStart, DWORD dwTruncEnd)
{
   DWORD i;
   DWORD dwMapTo;
   __int64 iError = 0;
   for (i = dwStart; i < dwEnd; i++) {
      dwMapTo = IntensityRemap (i, dwStart, dwEnd, dwStartTo, dwEndTo, dwTruncStart, dwTruncEnd);
      iError += HistogramErrorForIntensity (padwCount, i, dwMapTo);
   }

   return iError;
}


/*************************************************************************************
HistogramIdealRemap - Determine an ideal remap of intensities.

inputs
   DWORD          *padwCount - Histogram. 256
   DWORD          dwNum - Number of pixels in the histogram
   DWORD          dwIntensityMin - Minimum intensity allowed, 0..255
   DWORD          dwIntensityMax - Maximum intensity allowd, 0..255. (inclusive)
                     Assuming that wither dwIntensityMin == 0, or dwIntensityMax == 255
   DWORD          *padwRemap - For each element in the original histogram, this is the new
                     intensity to use. FIlled in. 256 elements
returns
   none
*/
void HistorgramIdealRemap (DWORD *padwCount, DWORD dwNum, DWORD dwIntensityMin, DWORD dwIntensityMax,
                           DWORD *padwRemap)
{
   // mid-point of where want to go
   DWORD dwIntensityMid = (dwIntensityMin + dwIntensityMax)/2;

   // darker or ligher
   BOOL fDarker = (dwIntensityMid < 0x80);

   // what's the median
   DWORD dwMedianOrig = HistogramMedian (padwCount, dwNum/2);

   // does the median need to be remapped?
   DWORD dwMedianNew = dwMedianOrig;
   if (fDarker) {
      if (dwMedianOrig > dwIntensityMid)
         dwMedianNew = dwIntensityMid;  // yes, need to make darker
   }
   else {
      if (dwMedianOrig < dwIntensityMid)
         dwMedianNew = dwIntensityMid; // yes, need to make lighter
   }

   // what are the cutoff points in the original scale
   int iOrigToNew = (int)dwMedianNew - (int)dwMedianOrig;
   int iIntensityMinOrig = (int)dwIntensityMin - iOrigToNew;
   int iIntensityMaxOrig = (int)dwIntensityMax - iOrigToNew;
   iIntensityMinOrig = max(iIntensityMinOrig, 0);  // since cant get from negaitve
   iIntensityMaxOrig = min(iIntensityMaxOrig, 255);   // sine cant get from beyond edge

   // half way between original median and limits. Don't squash in this area
   DWORD dwHalfMinOrig = ((DWORD)iIntensityMinOrig + dwMedianOrig) / 2;
   DWORD dwHalfMaxOrig = ((DWORD)iIntensityMaxOrig + dwMedianOrig) / 2;

   // BUGFIX - interpolate right around median; don't worry about preserving contrast around median
   dwHalfMinOrig = dwHalfMaxOrig = dwMedianOrig;

   // figure out the ideal squash above
   DWORD i;
   __int64 iError;
   DWORD dwBestSquashHigh = (DWORD)-1;
   __int64 iErrorBest;
   for (i = dwHalfMaxOrig; i < 256; i++) {
      iError = HistogramErrorSquash (padwCount, dwHalfMaxOrig, 256, dwHalfMaxOrig, i, 0, (DWORD)iIntensityMaxOrig); 

      if ((dwBestSquashHigh == (DWORD)-1) || (iError < iErrorBest)) {
         dwBestSquashHigh = i;
         iErrorBest = iError;
      }
   } // i
   if (dwBestSquashHigh == (DWORD)-1)
      dwBestSquashHigh = 255; // shouldnt happen

   // squash below
   DWORD dwBestSquashLow = (DWORD)-1;
   for (i = 0; i <= dwHalfMinOrig; i++) {
      iError = HistogramErrorSquash (padwCount, 0, dwHalfMinOrig, i,  dwHalfMinOrig, (DWORD)iIntensityMinOrig, 255); 

      if ((dwBestSquashLow == (DWORD)-1) || (iError < iErrorBest)) {
         dwBestSquashLow = i;
         iErrorBest = iError;
      }
   } // i
   if (dwBestSquashLow == (DWORD)-1)
      dwBestSquashLow = 0; // shouldnt happen

   // remap
   for (i = 0; i < dwHalfMinOrig; i++)
      padwRemap[i] = (DWORD)((int) IntensityRemap (i, 0, dwHalfMinOrig, dwBestSquashLow, dwHalfMinOrig, (DWORD)iIntensityMinOrig, 255) +
         iOrigToNew);
   for (i = dwHalfMinOrig; i < dwHalfMaxOrig; i++)
      padwRemap[i] = (DWORD)((int)i + iOrigToNew);
   for (i = dwHalfMaxOrig; i < 256; i++)
      padwRemap[i] = (DWORD)((int) IntensityRemap (i, dwHalfMaxOrig, 256, dwHalfMaxOrig, dwBestSquashHigh, 0, (DWORD)iIntensityMaxOrig) +
         iOrigToNew);

   // done
}

/*************************************************************************************
ImageSquashIntensity - Squash the intensities of an image.

inputs
   PCImage        pImage - Image. Modified in place.
   DWORD          dwIntensityMin - Minimum intensity allowed, 0..255
   DWORD          dwIntensityMax - Maximum intensity allowd, 0..255. (inclusive)
                     Assuming that wither dwIntensityMin == 0, or dwIntensityMax == 255
returns
   none
*/
void ImageSquashIntensity (PCImage pImage, DWORD dwIntensityMin, DWORD dwIntensityMax)
{
   // fill in the histogram
   DWORD adwCount[256];
   DWORD dwNum = HistogramFill (pImage, adwCount);

   // find the remap
   DWORD adwRemap[256];
   HistorgramIdealRemap (adwCount, dwNum, dwIntensityMin, dwIntensityMax, adwRemap);

   // remap all the colors
   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   DWORD i, j;
   DWORD dwIntensityOrig, dwIntensityWant;
   for (i = 0; i < dwNum; i++, pip++) {
      dwIntensityOrig = PIMAGEPIXELToIntensity (pip);
      dwIntensityWant = adwRemap[dwIntensityOrig];
      _ASSERTE (dwIntensityWant < 256);
      if (dwIntensityWant == dwIntensityOrig)
         continue;   // no change

      // ungamma
      int aiColor[3];
      aiColor[0] = (int)(DWORD)UnGamma (pip->wRed);
      aiColor[1] = (int)(DWORD)UnGamma (pip->wGreen);
      aiColor[2] = (int)(DWORD)UnGamma (pip->wBlue);

      // how much to change intensity
      int iChange = (int)dwIntensityWant - (int)dwIntensityOrig;
      int iExcess = 0;
      for (j = 0; j < 3; j++)
         aiColor[j] += iChange;


      while (TRUE) {
         BOOL afMaxedOut[2][3];  // [0 = low, 1=high][0..2]
         DWORD adwMaxedOut[2];
         memset (adwMaxedOut, 0, sizeof(adwMaxedOut));
         iExcess = 0;
         for (j = 0; j < 3; j++) {
            afMaxedOut[0][j] = FALSE;
            afMaxedOut[1][j] = FALSE;

            // see if out of range
            if (aiColor[j] <= 0) {
               iExcess += aiColor[j];
               aiColor[j] = 0;
               afMaxedOut[0][j] = TRUE;
               adwMaxedOut[0]++;
            }
            else if (aiColor[j] >= 255) {
               iExcess += aiColor[j] - 255;
               aiColor[j] = 255;
               afMaxedOut[1][j] = TRUE;
               adwMaxedOut[1]++;
            }
         } // j

         if (!iExcess)
            break;   // nothing left

         // if they're all maxed out then stop now because can't do anything
         DWORD dwHigh = (iExcess > 0) ? 1 : 0;
         if (adwMaxedOut[dwHigh] >= 3)
            break;

         // add the excess to the ones not maxed out
         iChange = iExcess / (int) (3 - adwMaxedOut[dwHigh]);
         for (j = 0; j < 3; j++)
            if (!afMaxedOut[dwHigh][j])
               aiColor[j] += iChange;

         // go back and repeat, and see what the new excess is
      } // while TRUE

      // when get here, have non maxed out RGB
      pip->wRed = Gamma((BYTE)(DWORD)aiColor[0]);
      pip->wGreen = Gamma((BYTE)(DWORD)aiColor[1]);
      pip->wBlue = Gamma((BYTE)(DWORD)aiColor[2]);
   } // i, every pixel
}

/*************************************************************************************
AverageColorOfImage - Finds the average color of an image.

inputs
   PCImage        pImage - Image
   WORD           *pawColorAll - Array of 3 colors
   WORD           *pawColorTop - Array of 3 colors for color at top
returns
   none
*/
void AverageColorOfImage (PCImage pImage, WORD *pawColorAll, WORD *pawColorTop)
{
   QWORD qwSum[3];
   memset (qwSum, 0, sizeof(qwSum));
   
   DWORD i;
   DWORD dwMax = pImage->Width() * pImage->Height();
   PIMAGEPIXEL pip = pImage->Pixel (0, 0);
   DWORD dwDoneTop = min(10, pImage->Height()) * pImage->Width() - 1;
   for (i = 0; i < dwMax; i++, pip++) {
      qwSum[0] += pip->wRed;
      qwSum[1] += pip->wGreen;
      qwSum[2] += pip->wBlue;

      if (i == dwDoneTop) {
         pawColorTop[0] = (WORD)(qwSum[0] / (QWORD)(i+1));
         pawColorTop[1] = (WORD)(qwSum[1] / (QWORD)(i+1));
         pawColorTop[2] = (WORD)(qwSum[2] / (QWORD)(i+1));
      }
   } // i

   pawColorAll[0] = (WORD)(qwSum[0] / (QWORD)i);
   pawColorAll[1] = (WORD)(qwSum[1] / (QWORD)i);
   pawColorAll[2] = (WORD)(qwSum[2] / (QWORD)i);
}


/*************************************************************************************
ImageSquashIntensityToBitmap - Squash the intensities of an image and convers it to a bitmpa

inputs
   PCImage        pImage - Image
   BOOL           fLightBackground - TRUE if it's a light background
   DWORD          dwExtra - 0 for no change, 1 for dark/light, 2 for extra dark/light
   HWND           hWnd - Window to use. If NULL then use desktop
   COLORREF       *pcrAverageAll - Filled with the average color of the image
   COLORREF       *pcrAverageTop - Filled with the average color of the image
returns
   HBITMAP - Bitmap
*/
HBITMAP ImageSquashIntensityToBitmap (PCImage pImage, BOOL fLightBackground, DWORD dwExtra, HWND hWnd,
                                      COLORREF *pcrAverageAll, COLORREF *pcrAverageTop)
{
   if (pcrAverageAll)
      *pcrAverageAll = RGB(0,0,0);
   if (pcrAverageTop)
      *pcrAverageTop = RGB(0,0,0);

   PCImage pClone = pImage->Clone();
   if (!pClone)
      return NULL;

   if (dwExtra) {
      if (fLightBackground)
         ImageSquashIntensity (pClone, (dwExtra >= 2) ? LIGHTEXTRAINTENSITYMIN : LIGHTINTENSITYMIN, 255);
      else
         ImageSquashIntensity (pClone, 0, (dwExtra >= 2) ? DARKEXTRAINTENSITYMAX : DARKINTENSITYMAX);
   }

   if (pcrAverageAll || pcrAverageTop) {
      WORD awColorAll[3], awColorTop[3];
      AverageColorOfImage (pClone, awColorAll, awColorTop);
      if (pcrAverageAll)
         *pcrAverageAll = UnGamma (awColorAll);
      if (pcrAverageTop)
         *pcrAverageTop = UnGamma (awColorTop);
   }

   if (!hWnd)
      hWnd = GetDesktopWindow();
   HDC hDC = GetDC (hWnd);
   HBITMAP hbmp = pClone->ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);
   
   delete pClone;

   return hbmp;
}

