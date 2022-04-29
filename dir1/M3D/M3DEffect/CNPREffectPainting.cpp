/*********************************************************************************
CNPREffectPainting.cpp - Code for effect

begun 21/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// STROKEINFO - for sorting strokes
typedef struct {
   WORD        wX;         // x coord
   WORD        wY;         // y coord
   float       fZ;         // Z depth
} STROKEINFO, *PSTROKEINFO;

// EMTPAINT - Multhtreaded paint info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PSTROKEINFO    psi;  // stroke info
   PCImage        pImageSrc;
   PCImage        pImageDest;
   PCFImage       pFImageSrc;
   PCFImage       pFImageDest;
   PCPaintingStroke paStroke;
} EMTPAINT, *PEMTPAINT;

/* CPaintingBrush - Shape of a brush for painting */
class CPaintingBrush {
public:
   ESCNEWDELETE;

   CPaintingBrush (void);
   ~CPaintingBrush (void);

   BOOL Init (DWORD dwSize, DWORD dwType, PCNoise2D pNoise, PCPoint pParam);
   void Dab (DWORD dwX, DWORD dwY, DWORD dwStrokeWidth, DWORD dwStrokeHeight, PBYTE pbStroke,
      DWORD *padwStrokeMinMax);

   // can read but don't change
   DWORD             m_dwSize;      // width and height, pixels
   PBYTE             m_pbBrush;     // array of m_dwSize x m_dwSize bytes, 0=none, 255=max dens

private:
   CMem              m_memBrush;    // memory containing the brush
};
typedef CPaintingBrush *PCPaintingBrush;

#define STROKEANTI      2        // how much to antialias strokes.


// PAINTINGPAGE - Page info
typedef struct {
   PCNPREffectPainting pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
   DWORD          dwTab;   // tab to view
} PAINTINGPAGE, *PPAINTINGPAGE;


PWSTR gpszEffectPainting = L"Painting";


// CFieldDetector - Object that detects which way a field is pointing
class CFieldDetector {
public:
   ESCNEWDELETE;

   CFieldDetector (void);
   ~CFieldDetector (void);

   BOOL Init (DWORD dwSize, DWORD dwAngles);

   void FieldCalc (DWORD dwType, DWORD dwX, DWORD dwY, PCImage pImage, PCFImage pFImage,
      BOOL fPerp, PTEXTUREPOINT ptpScore);

private:
   DWORD             m_dwSize;      // width and height, in pixels
   DWORD             m_dwAngles;    // number of angles in field (max 256)
   CMem              m_mem;         // memory for field information
   BYTE              *m_pabField;   // width x height array of bytes with angle number
   DWORD             *m_padwMinMax; // array of heigth x 2 DWORDs of min (inclusive) and max(exclusive)
                                    // valid values in m_pabField
   int               *m_paiScore;   // score for each of the angles, m_dwAngles entries
   PTEXTUREPOINT     m_patpSinCos;   // sine and cos of the angle, m_dwAngles entries
};
typedef CFieldDetector *PCFieldDetector;



/************************************************************************
CFieldDetector::Construcotr and destructor
*/
CFieldDetector::CFieldDetector (void)
{
   m_dwSize = m_dwAngles = 0;
   m_pabField = NULL;
   m_padwMinMax = NULL;
   m_paiScore = NULL;
   m_patpSinCos = NULL;
}

CFieldDetector::~CFieldDetector (void)
{
   // nothing for now
}



/************************************************************************
CFieldDetector::Init - Initializes the field detector to the
given size.

inputs
   DWORD       dwSize - Size in pixels
   DWORD       dwAngles - Number of angles to differentiate between. Max 256.
               Must be even #
returns
   BOOL - TRUE if success
*/
BOOL CFieldDetector::Init (DWORD dwSize, DWORD dwAngles)
{
   dwSize += (dwSize %2);  // so not odd
   dwAngles += (dwAngles % 2);
   dwAngles = max(dwAngles,2);
   DWORD dwNeed =
      dwSize * dwSize * sizeof(BYTE) + // field
      dwSize * 2 * sizeof(DWORD) + // minmax
      dwAngles * sizeof(TEXTUREPOINT) + // sincos
      dwAngles * sizeof(int);  // paiScore
   if (!m_mem.Required(dwNeed))
      return FALSE;

   m_dwSize = dwSize;
   m_dwAngles = dwAngles;
   m_patpSinCos = (PTEXTUREPOINT) m_mem.p;
   m_paiScore = (int*) (m_patpSinCos + dwAngles);
   m_padwMinMax = (DWORD*) (m_paiScore + dwAngles);
   m_pabField = (PBYTE) (m_padwMinMax + 2*dwSize); // need byte last so everything DWORD aligned

   // fill in all the sine and cos
   DWORD i;
   for (i = 0; i < dwAngles; i++) {
      m_patpSinCos[i].h = sin((fp)i / (fp)dwAngles * 2.0 * PI);
      m_patpSinCos[i].v = -cos((fp)i / (fp)dwAngles * 2.0 * PI);  // negative so in screen coords
   }
   
   // x and y
   DWORD x,y;
   DWORD dwHalf = dwSize/2;
   int iX, iY;
   for (y = 0; y < dwSize; y++) {
      iY = (int)y - (int)dwHalf;

      // determine where circle ends
      iX = 4 * (int)dwHalf * (int)dwHalf - (2*iY+1) * (2*iY+1);
      if (iX <= 0) {
         m_padwMinMax[y*2+0] = dwSize;
         m_padwMinMax[y*2+1] = 0;   // so nothing includes
      }
      else {
         iX = (int)sqrt((fp)iX) / 2;
         iX = min(iX, (int)dwHalf);
         m_padwMinMax[y*2+0] = dwHalf - (DWORD) iX;
         m_padwMinMax[y*2+1] = dwHalf + (DWORD) iX;
      }

      for (x = 0; x < dwSize; x++) {
         iX = (int)x - (int)dwHalf;

         fp fAngle = atan2 ((fp)iX + 0.5, -((fp)iY + 0.5));
#if 0 // to test atan assumptions
         fp fX = sin(fAngle)*(fp)dwHalf;
         fp fY = -cos(fAngle)*(fp)dwHalf;
#endif // 0

         fAngle = fAngle /( 2.0 * PI) * (fp)dwAngles;
         fAngle = fmod (fAngle + (fp)dwAngles, (fp)dwAngles);

         m_pabField[y*dwSize + x] = (BYTE)fAngle;
      } // x
   } // y

   return TRUE;
}



/************************************************************************
CFieldDetector::FieldCalc - This calculates the field in the image.

inputs
   DWORD       dwType - Type of filed, 0 for color, 1 for z, 2 for object
   DWORD       dwX - X pixel
   DWORD       dwY - Y pixel
   PCImage     pImage - Image, NULL if using PFImage
   PCFImage    pFImage - Image, NULL if using pImage
   BOOL        fPerp - If TRUE then use the angle perpendicualr to the gradient
   PTEXTUREPOINT ptpScore - Filled with sin and cos for the field, multiplied
               by the field's strength.
returns
   none
*/
void CFieldDetector::FieldCalc (DWORD dwType, DWORD dwX, DWORD dwY, PCImage pImage, PCFImage pFImage,
   BOOL fPerp, PTEXTUREPOINT ptpScore)
{
   // wipe out the memory for the score
   memset (m_paiScore, 0, sizeof(int)*m_dwAngles);

   DWORD dwHalf = m_dwSize / 2;

   // image size
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();
   BOOL f360 = pImage ? pImage->m_f360 : pFImage->m_f360;
   
   // get central object...
   PIMAGEPIXEL pipCenter = pImage ? pImage->Pixel(dwX, dwY) : NULL;
   PFIMAGEPIXEL pfpCenter = pImage ? NULL : pFImage->Pixel(dwX, dwY);
   float fZCenter = pipCenter ? pipCenter->fZ : pfpCenter->fZ;
   fZCenter = max(fZCenter, CLOSE); // so have something to divide by

   DWORD y;
   int iX, iY;
   PBYTE pbLine = m_pabField;
   DWORD dwPixels = 0;
   int iStartMin = f360 ? 0 : ((int)dwHalf - (int)dwX);   // so wont go beyond left edge of image
   int iEndMax = f360 ? (int)m_dwSize : ((int)dwWidth - (int)dwX + (int) dwHalf);   // so wont go beyond right edge of image
   iY = (int)dwY - (int)dwHalf;
   for (y = 0; (y < m_dwSize) && (iY < (int)dwHeight); y++, iY++, pbLine += m_dwSize) {
      if (iY < 0)
         continue;

      // figure out min and max
      int iStart = (int)m_padwMinMax[y*2+0];
      int iEnd = (int)m_padwMinMax[y*2+1];
      int iCur;
      iStart = max(iStart, iStartMin);
      iEnd = min(iEnd, iEndMax);
      PBYTE pbCur = pbLine + iStart;
      for (iCur = iStart; iCur < iEnd; iCur++, pbCur++) {
         iX = iCur + (int)dwX - (int)dwHalf;
         while (iX < 0)
            iX += (int)dwWidth;
         while (iX >= (int)dwWidth)
            iX -= (int)dwWidth;

         // count this pixel
         BYTE bAngle = *pbCur;
         dwPixels++;

         PIMAGEPIXEL pip = pImage ? pImage->Pixel((DWORD)iX, (DWORD)iY) : NULL;
         PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel((DWORD)iX, (DWORD)iY);

         switch (dwType) {
         case 0:  // color
            if (pip)
               m_paiScore[bAngle] += pip->wRed/4 + pip->wGreen/2 + pip->wBlue/4;
            else {
               float f = pfp->fRed/4 + pfp->fGreen/2 + pfp->fBlue/2;
               f = max(f, 0);
               f = min(f, (fp)0xffff);
               m_paiScore[bAngle] += (int)f;
            }
            break;

         case 1:  // Z
            {
               fp fZ = pip ? pip->fZ : pfp->fZ;
               fZ = log(fZ / fZCenter);
               fZ = fZ * 2 * (fp)0xffff + 0.5 * (fp)0xffff;
                  // so that value will be between 0 and 0xfff
                  // the 2.0 is a scale so won't need that big a change in Z to affect
               fZ = max(0, fZ);
               fZ = min((fp)0xffff, fZ);

               m_paiScore[bAngle] += (int) fZ;
            }
            break;

         case 2:  // object
            if (pip) {
               if (HIWORD(pip->dwID) == HIWORD(pipCenter->dwID)) {
                  if (LOWORD(pip->dwID) == LOWORD(pipCenter->dwID))
                     m_paiScore[bAngle] += 0xffff;   // same object as this, and same sub-object
                  else
                     m_paiScore[bAngle] += 0x8000;   // same object, but different subobjects
               }
            }
            else {
               if (HIWORD(pfp->dwID) == HIWORD(pfpCenter->dwID)) {
                  if (LOWORD(pfp->dwID) == LOWORD(pfpCenter->dwID))
                     m_paiScore[bAngle] += (fp)0xffff;   // same object as this, and same sub-object
                  else
                     m_paiScore[bAngle] += (fp)0x8000;   // same object, but different subobjects
               }
            }

            break;

         }

      } // iCur
   } // y
   
   // scale the scores
   if (!dwPixels) {
      // nothing actually included. shouldnt happen
      ptpScore->h = ptpScore->v = 0;
      return;
   }
   DWORD i;

   // figure out the best and worst scores

   // initialize the current score to all positive on right, and negative on left
   int iCurScore = 0;
   for (i = 0; i < m_dwAngles/2; i++)
      iCurScore += m_paiScore[i];
   for (; i < m_dwAngles; i++)
      iCurScore -= m_paiScore[i];

   DWORD dwHighest, dwLowest;
   int iHighest, iLowest, iAbs;
   dwHighest = dwLowest = 0;  // angle #
   iHighest = iLowest = abs(iCurScore);
   for (i = 1; i < m_dwAngles/2; i++) {
      // adjust the score
      iCurScore -= 2 * m_paiScore[i-1];   // since take off positive and add to negative
      iCurScore += 2 * m_paiScore[i+m_dwAngles/2-1];   // since take off negative and add to positive

      // only want to compare absolute values of scores, so no difference
      // between positive and negative side of the spinner
      // thus, iHighest is the angle with the greatest difference, iLowest
      // is the angle with the least difference
      iAbs = abs(iCurScore);
      if (iAbs > iHighest) {
         iHighest = iAbs;
         dwHighest = i;
      }
      else if (iAbs < iLowest) {
         iLowest = iAbs;
         dwLowest = i;
      }
   } // i

   // keep the difference between highest and lowest, for the total score
   iHighest -= iLowest;
   fp fScale = (fp)iHighest / (fp)dwPixels / (fp)0xffff;
   ptpScore->h = m_patpSinCos[fPerp ? dwLowest : dwHighest].h * fScale;
   ptpScore->v = m_patpSinCos[fPerp ? dwLowest : dwHighest].v * fScale;

   return;
}



/************************************************************************
CPaintingBrush::Constructor and destructor
*/
CPaintingBrush::CPaintingBrush (void)
{
   m_dwSize = 0;
   m_pbBrush = 0;
}

CPaintingBrush::~CPaintingBrush (void)
{
   // do nothing for now
}


/************************************************************************
CPaintingBrush::Init - Initializes the brush to a given size and shape.

inputs
   DWORD             dwSize - Width and height in pixels
   DWORD             dwType - 0 for circular, 1 for horizontal, 2 for verical,
                     3 for diagonal 1, 4 for diagonal 2
   PCNoise2D         pNoise - Noise functions, so all brushes use the same bristles
   PCPoint           pParam - p[0] = amount of noise (0..1), p[1] = amount of 2nd harmonic noise (0..1),
                     p[3] = contrast (0..1), p[4] = scale (0..1)
returns
   BOOL - TRUE if success
*/
BOOL CPaintingBrush::Init (DWORD dwSize, DWORD dwType, PCNoise2D pNoise,
                           PCPoint pParam)
{
   if (!m_memBrush.Required (dwSize * dwSize))
      return FALSE;
   m_dwSize = dwSize;
   m_pbBrush = (PBYTE)m_memBrush.p;
   memset (m_pbBrush, 0, dwSize * dwSize);

   // fill in shape
   DWORD x,y;
   PBYTE pb = m_pbBrush;
   fp fSizeHalf = (fp)dwSize/2;
   for (y = 0; y < dwSize; y++) {
      for (x = 0; x < dwSize; x++, pb++) {
         fp fx = 1.0 - (fp)x / fSizeHalf;
         fp fy = 1.0 - (fp)y / fSizeHalf;
         fp fScore;

         switch (dwType) {
         case 0:  // circular
         default:
            fScore = 1.0 - fx * fx - fy * fy;
            fScore = sqrt(max(fScore,0));
            break;

         case 1:  // horizontal
            fScore = 1.0 - fabs(fy) * 2;
            break;

         case 2:  // vertical
            fScore = 1.0 - fabs(fx) * 2;
            break;

         case 4:  // diagonal 2
            fx = - fx;
            // fall through
         case 3:  // diagonal
            fScore = 1.0 - fabs(fx - fy) * 2;

            // slice off end
            if ((fx + fy >= 1.75) || (fx + fy < -1.75))
               fScore = 0;
            break;
         }

         if (fScore <= 0)
            continue;   // too low

         // brush parameters
         if (pParam->p[0])
            fScore += pNoise->Value ((fp)x / (fp)dwSize, (fp)y / (fp)dwSize) * pParam->p[0];
         if (pParam->p[1])
            fScore += pNoise->Value (2.0 * (fp)x / (fp)dwSize, 2.0 * (fp)y / (fp)dwSize) * pParam->p[1];
         if (fScore <= 0)
            continue;   // too low
         if (pParam->p[2] != 0.5)
            fScore = pow (fScore, pParam->p[2] * 2);
         if (pParam->p[3] != 0.5)
            fScore *= (pParam->p[3] * 2);

         fScore = min(fScore, 1);
         pb[0] = (BYTE)(fScore*255.0);
      } // xx
   } // y

   return TRUE;
}


/************************************************************************
CPaintingBrush::Dab - Dabs the brush on the image. It leaves the maximum
value on the stroke's image.

inputs
   DWORD          dwX - Center X of the dab. Must be >= 1/2 of m_dwSize.
                        Must be < dwStrokeWidth-(m_dwSize+1)/2
   DWORD          dwY - Center Y of the dab. Must be >= 1/2 of m_dwSize.
                        Must be < dwStrokeHeight-(m_dwSize+1)/2
   DWORD          dwStrokeWidth - Number ofpixels in the stroke memory
   DWORD          dwStrokeHeight - Number of pixels in stroke memory
   PBYTE          pbStroke - Array of dwStrokeWidth x dwStrokeHeight
   DWORD          *padwStrokeMinMax - Pointer to an array of dwStrokeHeight x 2
                  DWORDs with min and max valid X values for the stroke.
                  If the brush paints on the Y line then it will adapt the values.
                  This also will clear the memory if the stroke min/max are changed
                  at all. Used as an optimizations so don't have to clear memory to
                  0, or to compare it all when apply to CImage.
returns
   none
*/
__inline void CPaintingBrush::Dab (DWORD dwX, DWORD dwY, DWORD dwStrokeWidth, DWORD dwStrokeHeight, PBYTE pbStroke,
   DWORD *padwStrokeMinMax)
{
   // go to UL corner
   dwX -= (m_dwSize/2);
   dwY -= (m_dwSize/2);
   DWORD dwXMax = dwX + m_dwSize;

#ifdef _DEBUG
   if ((dwX >= dwStrokeWidth) || (dwX+m_dwSize >= dwStrokeWidth) || (dwY >= dwStrokeHeight) || (dwY+m_dwSize >= dwStrokeHeight))
      return;
#endif

   // loop
   DWORD x,y;
   padwStrokeMinMax += (2 * dwY);
   PBYTE pbTo = pbStroke + (dwY * dwStrokeWidth);
   PBYTE pbFrom = m_pbBrush;
   for (y = 0; y < m_dwSize; y++, dwY++, padwStrokeMinMax += 2, pbTo += dwStrokeWidth, pbFrom += m_dwSize) {
      // if this hasn't been initialized then just to a straight copy
      if (!padwStrokeMinMax[0] && !padwStrokeMinMax[1]) {
         padwStrokeMinMax[0] = dwX;
         padwStrokeMinMax[1] = dwXMax;
         memcpy (pbTo + dwX, pbFrom, m_dwSize);
         continue;
      }

      // else see if have to expand, so zero out
      if (padwStrokeMinMax[0] > dwX) {
         memset (pbTo + dwX, 0, padwStrokeMinMax[0] - dwX);
         padwStrokeMinMax[0] = dwX;
      }
      if (padwStrokeMinMax[1] < dwXMax) {
         memset (pbTo + padwStrokeMinMax[1], 0, dwXMax - padwStrokeMinMax[1]);
         padwStrokeMinMax[1] = dwXMax;
      }

      // do minimums
      PBYTE pbXFrom = pbFrom;
      PBYTE pbXTo = pbTo + dwX;
      for (x = 0; x < m_dwSize; x++, pbXFrom++, pbXTo++)
         pbXTo[0] = max(pbXTo[0], pbXFrom[0]);
   } // y
}



/************************************************************************
CPaintingStroke::Constructor and destructor
*/
CPaintingStroke::CPaintingStroke (void)
{
   m_dwType = 0;
   m_lPCPaintingBrush.Init (sizeof(PCPaintingBrush));
   m_dwWidth = m_dwHeight = 0;
   m_padwStrokeMinMax = NULL;
   m_pbStroke = NULL;
   m_pParam.Zero();
}

CPaintingStroke::~CPaintingStroke (void)
{
   DWORD i;
   PCPaintingBrush *ppb = (PCPaintingBrush*)m_lPCPaintingBrush.Get(0);
   for (i = 0; i < m_lPCPaintingBrush.Num(); i++)
      if (ppb[i])
         delete ppb[i];
}

/************************************************************************
CPaintingStroke::Init - Initializes the stroke to use the given brush.

inputs
   DWORD          dwType - Type of brush. Same as CPaintingBrush::Init
   DWORD          dwNoiseRes - Noise resolution, passed into noise initializer
   PCPoint        pParam - Paramters passed to CPaintingBrush::Init
returns
   BOOL - TRUE if success
*/
BOOL CPaintingStroke::Init (DWORD dwType, DWORD dwNoiseRes, PCPoint pParam)
{
   if (m_lPCPaintingBrush.Num())
      return FALSE;

   m_dwType = dwType;
   m_pParam.Copy (pParam);
   m_Noise.Init (dwNoiseRes, dwNoiseRes);

   return TRUE;
}


/************************************************************************
CPaintingStroke::InitStrokeMem - Initializes the stroke memory so it's
large enough for dwWidth x dwHeight

inputs
   DWORD          dwWidth - Width in pixels
   DWORD          dwHeight - Height in pixels
returns
   none
*/
BOOL CPaintingStroke::InitStrokeMem (DWORD dwWidth, DWORD dwHeight)
{
   DWORD dwMinMaxNeed = dwHeight * 2 * sizeof(DWORD);
   DWORD dwNeed = dwWidth * dwHeight + dwMinMaxNeed;
   if (!m_memStroke.Required (dwNeed))
      return FALSE;
   m_dwWidth = dwWidth;
   m_dwHeight = dwHeight;
   m_padwStrokeMinMax = (DWORD*)m_memStroke.p;
   m_pbStroke = (PBYTE) (m_padwStrokeMinMax + (dwHeight * 2));
   memset (m_padwStrokeMinMax, 0, dwMinMaxNeed);

   return TRUE;
}


/************************************************************************
CPaintingStroke::GetBrush - Gets a brush of the given size. If the
brush doesn't exist it's created.

inputs
   DWORD       dwSize - Size
returns
   PCPaintingBrush - Brush
*/
PCPaintingBrush CPaintingStroke::GetBrush (DWORD dwSize)
{
   // expand the list as necessary
   while (dwSize >= m_lPCPaintingBrush.Num()) {
      PCPaintingBrush pRet = NULL;
      m_lPCPaintingBrush.Add (&pRet);
   }

   // get the brush
   PCPaintingBrush *ppb = (PCPaintingBrush*)m_lPCPaintingBrush.Get(dwSize);

   // create brush
   if (!ppb[0]) {
      ppb[0] = new CPaintingBrush;
      ppb[0]->Init (dwSize, m_dwType, &m_Noise, &m_pParam);
   }

   return ppb[0];

}

/************************************************************************
CPaintingStroke::PaintSegment - This paints a straight segment of the
brush from pStart, to pEnd. It assumes that InitStrokeMem has been called
and that the brush (including it's size) will never go off the stroke
memory.

inputs
   POINT          *pStart - Starting point
   POINT          *pEnd - Ending point
   DWORD          dwSizeStart - Size at the start
   DWORD          dwSizeEnd - Size at the end
   BOOL           fIncludeLast - If TRUE include pEnd in the segment, else exclude
*/
void CPaintingStroke::PaintSegment (POINT *pStart, POINT *pEnd, DWORD dwSizeStart, DWORD dwSizeEnd, BOOL fIncludeLast)
{
   int iDeltaX = pEnd->x - pStart->x;
   int iDeltaY = pEnd->y - pStart->y;
   int iDeltaXAbs = abs(iDeltaX);
   int iDeltaYAbs = abs(iDeltaY);
   int iDeltaSize = (int)dwSizeEnd - (int)dwSizeStart;
   PCPaintingBrush pb;

   // if both 0 then just point
   if (!iDeltaXAbs && !iDeltaYAbs) {
      if (!fIncludeLast)
         return;

      pb = GetBrush (max(dwSizeStart, dwSizeEnd));
      pb->Dab ((DWORD)pStart->x, (DWORD)pStart->y,
         m_dwWidth, m_dwHeight, m_pbStroke, m_padwStrokeMinMax);
      return;
   }

   int i, iMax, x, y, iSize;
   if (iDeltaXAbs >= iDeltaYAbs) {  // loop in x
      iMax = iDeltaXAbs + (fIncludeLast ? 1 : 0);
      for (i = 0; i < iMax; i += STROKEANTI) {
         x = (iDeltaX >= 0) ? (pStart->x + i) : (pStart->x - i);
         y = i * iDeltaY / iDeltaXAbs + pStart->y;
         iSize = i * iDeltaSize / iDeltaXAbs + (int)dwSizeStart;

         pb = GetBrush ((DWORD)iSize);
         pb->Dab ((DWORD)x, (DWORD)y,
            m_dwWidth, m_dwHeight, m_pbStroke, m_padwStrokeMinMax);
      } // i
   }
   else {   // loop in y
      iMax = iDeltaYAbs + (fIncludeLast ? 1 : 0);
      for (i = 0; i < iMax; i += STROKEANTI) {
         y = (iDeltaY >= 0) ? (pStart->y + i) : (pStart->y - i);
         x = i * iDeltaX / iDeltaYAbs + pStart->x;
         iSize = i * iDeltaSize / iDeltaYAbs + (int)dwSizeStart;

         pb = GetBrush ((DWORD)iSize);
         pb->Dab ((DWORD)x, (DWORD)y,
            m_dwWidth, m_dwHeight, m_pbStroke, m_padwStrokeMinMax);
      } // i
   } // if loop in y

   // done
   return;
}



/************************************************************************
CPaintingStroke::PaintSegments - This paints a series of line segments.
It allocates all the memory needed for the buffer first.

inputs
   POINT          *paSeg - Array of POINTS for the line segments, in CFImage space
                  NOTE: These segments are modified in place into offset and antialiased coords
   DWORD          *padwSize - Array of sizes for the line segments, in CFImage space
                  NOTE: These sizes are modified in place into offset and antialiased coords
   DWORD          dwNum - Number of segments
   POINT          *pOffset - This is filled with the offset that is added onto
                     the stroke's painting space to get to the image space.
                     NOTE: Also need to include STROKEANTI for antialiasing
returns
   BOOL - TRUE if success
*/
BOOL CPaintingStroke::PaintSegments (POINT *paSeg, DWORD *padwSize, DWORD dwNum, POINT *pOffset)
{
   // if no segments done
   if (!dwNum)
      return TRUE;

   // find maximum size for line
   DWORD dwMaxSize = 0;
   DWORD dwMaxHalf;
   DWORD i;
   for (i = 0; i < dwNum; i++)
      dwMaxSize = max(padwSize[i], dwMaxSize);
   dwMaxHalf = (dwMaxSize * STROKEANTI + 1) / 2;
   dwMaxSize = dwMaxHalf * 2;

   // scale by antialiasing and determine bounding box
   RECT rBound, rThis;
   for (i = 0; i < dwNum; i++) {
      padwSize[i] *= STROKEANTI;
      paSeg[i].x *= STROKEANTI;
      paSeg[i].y *= STROKEANTI;

      // bounding box for this
      rThis.left = paSeg[i].x - (int)dwMaxHalf;
      rThis.top = paSeg[i].y - (int)dwMaxHalf;
      rThis.right = rThis.left + (int)dwMaxSize;
      rThis.bottom = rThis.top + (int)dwMaxSize;

      if (i) {
         // expand bounding rectangle to account
         rBound.left = min(rBound.left, rThis.left);
         rBound.right = max(rBound.right, rThis.right);
         rBound.top = min(rBound.top, rThis.top);
         rBound.bottom = max(rBound.bottom, rThis.bottom);
      }
      else
         rBound = rThis;   // first one
   } // i

   // store the offset
#define EXTRAPAD        1     // in case roundoff err
   pOffset->x = rBound.left - EXTRAPAD;
   pOffset->y = rBound.top - EXTRAPAD;

   // adjust by the offset
   for (i = 0; i < dwNum; i++) {
      paSeg[i].x -= pOffset->x;
      paSeg[i].y -= pOffset->y;
   }

   // initialize
   if (!InitStrokeMem ((DWORD)(rBound.right - rBound.left + 2*EXTRAPAD),
      (DWORD)(rBound.bottom - rBound.top + 2*EXTRAPAD)))
      return FALSE;

   // if only one point then just dot
   if (dwNum == 1) {
      PaintSegment (paSeg, paSeg, padwSize[0], padwSize[0], TRUE);
      return TRUE;
   }

   // draw the line segments
   for (i = 0; i+1 < dwNum; i++)
      PaintSegment (paSeg + i, paSeg + (i+1), padwSize[i], padwSize[i+1], (i+2 == dwNum));

   return TRUE;
}


/************************************************************************
CPaintingStroke::PaintCurvedSegments - This paints a series of line segments,
but adds a curve to them so they're drawn smmothly

inputs
   POINT          *paSeg - Array of POINTS for the line segments, in CFImage space
   DWORD          *padwSize - Array of sizes for the line segments, in CFImage space
   DWORD          dwNum - Number of segments
   DWORD          dwCurve - Use 1 for none, 2 for 2x as many points as original, etc.
   POINT          *pOffset - This is filled with the offset that is added onto
                     the stroke's painting space to get to the image space.
                     NOTE: Also need to include STROKEANTI for antialiasing
returns
   BOOL - TRUE if success
*/
BOOL CPaintingStroke::PaintCurvedSegments (POINT *paSeg, DWORD *padwSize, DWORD dwNum,
                                           DWORD dwCurve, POINT *pOffset)
{
   // allocate enough memory for curve
   DWORD dwCount = ((dwNum - 1) * dwCurve + 1);
   DWORD dwNeed = dwCount * (sizeof(POINT) + sizeof(DWORD));
   if (!m_memCurve.Required (dwNeed))
      return FALSE;
   POINT *paCurve = (POINT*)m_memCurve.p;
   DWORD *padwCurveSize = (DWORD*) (paCurve + dwCount);

   // fill in the points
   DWORD i;
   for (i = 0; i < dwCount; i++) {
      DWORD dwOrig = i / dwCurve;
      DWORD dwMod = i - (dwOrig * dwCurve);
      if (!dwMod) {
         // just copy the original point
         paCurve[i] = paSeg[dwOrig];
         padwCurveSize[i] = padwSize[dwOrig];
         continue;
      }

      // else, interp
      fp t = (fp)dwMod / (fp)dwCurve;
      POINT *pPrev, *pThis, *pNext, *pNext2;
      pPrev = paSeg + (dwOrig ? (dwOrig-1) : 0);
      pThis = paSeg + dwOrig;
      pNext = paSeg + min(dwOrig+1, dwNum-1);
      pNext2 = paSeg + min(dwOrig+2, dwNum-1);
      paCurve[i].x = (int)HermiteCubic (t, pPrev->x, pThis->x, pNext->x, pNext2->x);
      paCurve[i].y = (int)HermiteCubic (t, pPrev->y, pThis->y, pNext->y, pNext2->y);
      padwCurveSize[i] = (padwSize[dwOrig] * (dwCurve - dwMod) +
         padwSize[min(dwOrig+1,dwNum-1)] * dwMod) / dwCurve;
   } // i

   // draw it
   return PaintSegments (paCurve, padwCurveSize, dwCount, pOffset);
}




/************************************************************************
CPaintingStroke::Paint - Paints the line to the given CImage of CFImage.

NOTE: With multithreaded might end up painting strokes over one another.
I suspect this wont be a problem, but might be worth fixing at some point.

inputs
   POINT          *paSeg - Array of POINTS for the line segments, in CFImage space
   DWORD          *padwSize - Array of sizes for the line segments, in CFImage space
   DWORD          dwNum - Number of segments
   DWORD          dwCurve - Use 1 for none, 2 for 2x as many points as original, etc.
   PCImage        pImage - If not NULL use this as the image
   PCFImage       pFImage - If not NULL use this as the image
   WORD           *pawColor - Color to use
returns
   BOOL - TRUE if success
*/
BOOL CPaintingStroke::Paint (POINT *paSeg, DWORD *padwSize, DWORD dwNum,
                              DWORD dwCurve, PCImage pImage, PCFImage pFImage,
                              WORD *pawColor)
{
   // figure out stroke
   POINT pOffset;
   if (!PaintCurvedSegments (paSeg, padwSize, dwNum, dwCurve, &pOffset))
      return FALSE;

   // size of the image
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();
   BOOL f360 = pImage ? pImage->m_f360 : pFImage->m_f360;

   // how does this translate into antaliased y
   int iYImage, iYStroke, iXImage, iXStroke;
   iYImage = (pOffset.y >= 0) ? (pOffset.y / STROKEANTI) :
      -((-pOffset.y+STROKEANTI-1) / STROKEANTI);
   iYStroke = iYImage * STROKEANTI - pOffset.y;
   iXImage = (pOffset.x >= 0) ? (pOffset.x / STROKEANTI) :
      -((-pOffset.x+STROKEANTI-1) / STROKEANTI);
   iXStroke = iXImage * STROKEANTI - pOffset.x;

   // loop
   for (; iYStroke < (int)m_dwHeight; iYImage++, iYStroke += STROKEANTI) {
      // if beyond top of screen then continue
      if (iYImage < 0)
         continue;

      // if beyond bottom of screen then break
      if (iYImage >= (int)dwHeight)
         break;

      // loop over X values
      int iXCurImage = iXImage;
      int iXCurStroke = iXStroke;
      for (; iXCurStroke < (int)m_dwWidth; iXCurImage++, iXCurStroke += STROKEANTI) {
         // if 360 degree image then do modulo
         if ((iXCurImage < 0) || (iXCurImage >= (int)dwWidth)) {
            if (!f360)
               continue;

            iXCurImage = (iXCurImage + (int)dwWidth*16) % (int)dwWidth;
         }

         // loop over the antialias lines
         int iYAnti, iXAnti;
         DWORD dwSum = 0;
         int iYWithAnti = iYStroke;
         for (iYAnti = 0; iYAnti < STROKEANTI; iYAnti++, iYWithAnti++) {
            if ((iYWithAnti < 0) || (iYWithAnti >= (int)m_dwHeight))
               continue;   // out of bounds

            // get the min and max
            DWORD *pdwMinMax = m_padwStrokeMinMax + (iYWithAnti * 2);
            if (!pdwMinMax[0] && !pdwMinMax[1])
               continue;   // no points there

            // if completely out of min/max range then skip
            if ((iXCurStroke >= (int)pdwMinMax[1]) || (iXCurStroke + STROKEANTI <= (int)pdwMinMax[0]))
               continue;

            PBYTE pbLine = m_pbStroke + (iYWithAnti * (int)m_dwWidth);
            int iXWithAnti = iXCurStroke;
            for (iXAnti = 0; iXAnti < STROKEANTI; iXAnti++, iXWithAnti++) {
               if ((iXWithAnti < (int)pdwMinMax[0]) || (iXWithAnti >= (int)pdwMinMax[1]))
                  continue;   // not valid

               // get the value
               dwSum += (DWORD)pbLine[iXWithAnti];
            } // iXAnti
         } // iYAnti
         
         // if no values summed up then no point interpolating
         if (!dwSum)
            continue;

         dwSum /= (STROKEANTI * STROKEANTI); // so with be 255 at max
         if (dwSum)
            dwSum++;    // so will be 256 at max, nicer number

         if (pImage) {
            PIMAGEPIXEL pip = pImage->Pixel ((DWORD)iXCurImage, (DWORD)iYImage);
            DWORD dwInv = 256 - dwSum;
            
            pip->wRed = (WORD)((DWORD)pip->wRed * dwInv / 256 + pawColor[0] * dwSum / 256);
            pip->wGreen = (WORD)((DWORD)pip->wGreen * dwInv / 256 + pawColor[1] * dwSum / 256);
            pip->wBlue = (WORD)((DWORD)pip->wBlue * dwInv / 256 + pawColor[2] * dwSum / 256);
         }
         else {
            PFIMAGEPIXEL pip = pFImage->Pixel ((DWORD)iXCurImage, (DWORD)iYImage);
            fp fScale = (fp)dwSum / (fp)256.0;
            fp fInv = 1.0 - fScale;
            
            pip->fRed = pip->fRed * fInv + (fp)pawColor[0] * fScale;
            pip->fGreen = pip->fGreen * fInv + (fp)pawColor[1] * fScale;
            pip->fBlue = pip->fBlue * fInv + (fp)pawColor[2] * fScale;
         }
      } // iXCurStroke, iCCurImage

   } // iYStroke and IYImage

   return TRUE;
}




/*********************************************************************************
CNPREffectPainting::Constructor and destructor
*/
CNPREffectPainting::CNPREffectPainting (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fIgnoreBackground = TRUE;

   m_dwTextBack = 1;
   m_acTextColor[0] = RGB(0xff,0xff,0xff);
   m_acTextColor[1] = RGB(0x80,0x80,0x80);
   m_fTextWidth = 1;
   m_pTextSurf = new CObjectSurface;
   DefaultSurfFromNewTexture (m_dwRenderShard, (GUID*)&GTEXTURECODE_SidingStucco, (GUID*)&GTEXTURESUB_SidingStucco, m_pTextSurf);

   m_dwStrokesNum = 1000;

   m_fStrokeMomentum = m_fStrokeRandom = m_fStrokeZWeight = m_fStrokeColorWeight =
      m_fStrokeObjectWeight = m_fStrokePrefWeight = 0.5;
   m_fStrokePrefAngle = PI/4;
   m_fStrokeZPerp = m_fStrokeColorPerp = m_fStrokeObjectPerp = FALSE;
   m_fStrokeCrosshatch = FALSE;
   m_fStrokePrefEither = TRUE;

   m_fStrokeZBlur = m_fStrokeColorBlur = m_fStrokeObjectBlur = 5;

   m_fStrokeLen = 5;
   m_fStrokeStep = 2;
   m_fStrokeAnchor = 0.2;
   m_fStrokeLenVar = 0.5;
   m_fStrokePenColor = 0.5;
   m_fStrokePenObject = 0.5;

   m_pStrokeWidth.p[0] = .5;
   m_pStrokeWidth.p[1] = 1;
   m_pStrokeWidth.p[2] = .5;
   m_pStrokeWidth.p[3] = .2;

   m_pColorVar.Zero();
   m_pColorVar.p[0] = m_pColorVar.p[1] = m_pColorVar.p[2] = .1;
   m_pColorPalette.Zero();
   m_fColorHueShift = 0;
   m_fColorUseFixed = 0;
   m_acColorFixed[0] = RGB(0,0,0);
   m_acColorFixed[1] = RGB(0xff,0xff,0xff);
   m_acColorFixed[2] = RGB(0xff,0,0);
   m_acColorFixed[3] = RGB(0,0xff,0);
   m_acColorFixed[4] = RGB(0,0,0xff);

   m_fDeltaFilterWidth = 0;
   m_fBackMatch = 0;
   m_cBackColor = RGB(0xff,0xff,0xff);

   m_dwBrushShape = 0;
   m_pBrushParam.Zero();
   m_pBrushParam.p[2] = m_pBrushParam.p[3] = 0.5;  // scaling and contrast

   m_fSpyglassScale = 1;
   m_fSpyglassBlur = .3;
   m_fSpyglassStickOut = 0;
   m_dwSpyglassShape = 0;

   DWORD dwThread;
   for (dwThread = 0; dwThread < MAXRAYTHREAD; dwThread++) {
      m_alPathPOINT[dwThread].Init (sizeof(POINT));
      m_alPathDWORD[dwThread].Init (sizeof(DWORD));
   }
}

CNPREffectPainting::~CNPREffectPainting (void)
{
   if (m_pTextSurf)
      delete m_pTextSurf;
}


/*********************************************************************************
CNPREffectPainting::Delete - From CNPREffect
*/
void CNPREffectPainting::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectPainting::QueryInfo - From CNPREffect
*/
void CNPREffectPainting::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = FALSE;
   pqi->pszDesc = L"Makes the image look like it was painted.";
   pqi->pszName = L"Painting";
   pqi->pszID = gpszEffectPainting;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszTextBack = L"TextBack";
static PWSTR gpszTextColor0 = L"TextColor0";
static PWSTR gpszTextColor1 = L"TextColor1";
static PWSTR gpszTextWidth = L"TextWidth";
static PWSTR gpszTextSurf = L"TextSurf";
static PWSTR gpszStrokesNum = L"StrokesNum";
static PWSTR gpszStrokeMomentum = L"StrokeMomentum";
static PWSTR gpszStrokeRandom = L"StrokeRandom";
static PWSTR gpszStrokeZWeight = L"StrokeZWeight";
static PWSTR gpszStrokeColorWeight = L"StrokeColorWeight";
static PWSTR gpszStrokeObjectWeight = L"StrokeObjectWeight";
static PWSTR gpszStrokePrefWeight = L"StrokePrefWeight";
static PWSTR gpszStrokePrefAngle = L"StrokePrefAngle";
static PWSTR gpszStrokeZPerp = L"StrokeZPerp";
static PWSTR gpszStrokeColorPerp = L"StrokeColorPerp";
static PWSTR gpszStrokeObjectPerp = L"StrokeObjectPerp";
static PWSTR gpszStrokeCrosshatch = L"StrokeCrosshatch";
static PWSTR gpszStrokePrefEither = L"StrokePrefEither";
static PWSTR gpszStrokeZBlur = L"StrokeZBlur";
static PWSTR gpszStrokeColorBlur = L"StrokeColorBlur";
static PWSTR gpszStrokeObjectBlur = L"StrokeObjectBlur";
static PWSTR gpszStrokeLen = L"StrokeLen";
static PWSTR gpszStrokeStep = L"StrokeStep";
static PWSTR gpszStrokeAnchor = L"StrokeAnchor";
static PWSTR gpszStrokeLenVar = L"StrokeLenVar";
static PWSTR gpszStrokePenColor = L"StrokePenColor";
static PWSTR gpszStrokePenObject = L"StrokePenObject";
static PWSTR gpszStrokeWidth = L"StrokeWidth";
static PWSTR gpszColorVar = L"ColorVar";
static PWSTR gpszColorPalette = L"ColorPalette";
static PWSTR gpszColorHueShift = L"ColorHueShift";
static PWSTR gpszColorUseFixed = L"ColorUseFixed";
static PWSTR gpszBrushShape = L"BrushShape";
static PWSTR gpszBrushParam = L"BrushParam";
static PWSTR gpszDeltaFilterWidth = L"DeltaFilterWidth";
static PWSTR gpszBackMatch = L"BackMatch";
static PWSTR gpszBackColor = L"BackColor";
static PWSTR gpszSpyglassScale = L"SpyglassScale";
static PWSTR gpszSpyglassStickOut = L"SpyglassStickOut";
static PWSTR gpszSpyglassBlur = L"SpyglassBlur";
static PWSTR gpszSpyglassShape = L"SpyglassShape";

/*********************************************************************************
CNPREffectPainting::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectPainting::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectPainting);

   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);


   MMLValueSet (pNode, gpszTextBack, (int)m_dwTextBack);
   MMLValueSet (pNode, gpszTextColor0, (int)m_acTextColor[0]);
   MMLValueSet (pNode, gpszTextColor1, (int)m_acTextColor[1]);
   MMLValueSet (pNode, gpszTextWidth, m_fTextWidth);
   MMLValueSet (pNode, gpszStrokesNum, (int) m_dwStrokesNum);
   PCMMLNode2 pSub = m_pTextSurf->MMLTo();
   if (pSub) {
      pSub->NameSet (gpszTextSurf);
      pNode->ContentAdd (pSub);
   }

   MMLValueSet (pNode, gpszSpyglassScale, m_fSpyglassScale);
   MMLValueSet (pNode, gpszSpyglassBlur, m_fSpyglassBlur);
   MMLValueSet (pNode, gpszSpyglassStickOut, m_fSpyglassStickOut);
   MMLValueSet (pNode, gpszSpyglassShape, (int)m_dwSpyglassShape);

   MMLValueSet (pNode, gpszStrokeMomentum, m_fStrokeMomentum);
   MMLValueSet (pNode, gpszStrokeRandom, m_fStrokeRandom);
   MMLValueSet (pNode, gpszStrokeZWeight, m_fStrokeZWeight);
   MMLValueSet (pNode, gpszStrokeColorWeight, m_fStrokeColorWeight);
   MMLValueSet (pNode, gpszStrokeObjectWeight, m_fStrokeObjectWeight);
   MMLValueSet (pNode, gpszStrokePrefWeight, m_fStrokePrefWeight);
   MMLValueSet (pNode, gpszStrokePrefAngle, m_fStrokePrefAngle);
   MMLValueSet (pNode, gpszStrokeZPerp, (int) m_fStrokeZPerp);
   MMLValueSet (pNode, gpszStrokeColorPerp, (int) m_fStrokeColorPerp);
   MMLValueSet (pNode, gpszStrokeObjectPerp, (int) m_fStrokeObjectPerp);
   MMLValueSet (pNode, gpszStrokePrefEither, (int) m_fStrokePrefEither);
   MMLValueSet (pNode, gpszStrokeCrosshatch, (int) m_fStrokeCrosshatch);

   MMLValueSet (pNode, gpszStrokeZBlur, m_fStrokeZBlur);
   MMLValueSet (pNode, gpszStrokeColorBlur, m_fStrokeColorBlur);
   MMLValueSet (pNode, gpszStrokeObjectBlur, m_fStrokeObjectBlur);


   MMLValueSet (pNode, gpszStrokeLen, m_fStrokeLen);
   MMLValueSet (pNode, gpszStrokeStep, m_fStrokeStep);
   MMLValueSet (pNode, gpszStrokeAnchor, m_fStrokeAnchor);
   MMLValueSet (pNode, gpszStrokeLenVar, m_fStrokeLenVar);
   MMLValueSet (pNode, gpszStrokePenColor, m_fStrokePenColor);
   MMLValueSet (pNode, gpszStrokePenObject, m_fStrokePenObject);

   MMLValueSet (pNode, gpszStrokeWidth, &m_pStrokeWidth);

   MMLValueSet (pNode, gpszDeltaFilterWidth, m_fDeltaFilterWidth);
   MMLValueSet (pNode, gpszBackMatch, m_fBackMatch);
   MMLValueSet (pNode, gpszBackColor, (int)m_cBackColor);

   MMLValueSet (pNode, gpszColorVar, &m_pColorVar);
   MMLValueSet (pNode, gpszColorPalette, &m_pColorPalette);
   MMLValueSet (pNode, gpszColorHueShift, m_fColorHueShift);
   MMLValueSet (pNode, gpszColorUseFixed, (int)m_fColorUseFixed);
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"ColorFixed%d", (int) i);
      MMLValueSet (pNode, szTemp, (int)m_acColorFixed[i]);
   }

   MMLValueSet (pNode, gpszBrushShape, (int)m_dwBrushShape);
   MMLValueSet (pNode, gpszBrushParam, &m_pBrushParam);

   return pNode;
}


/*********************************************************************************
CNPREffectPainting::MMLFrom - From CNPREffect
*/
BOOL CNPREffectPainting::MMLFrom (PCMMLNode2 pNode)
{
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);


   m_dwTextBack = (DWORD) MMLValueGetInt (pNode, gpszTextBack, (int)1);
   m_acTextColor[0] = (COLORREF) MMLValueGetInt (pNode, gpszTextColor0, (int)0);
   m_acTextColor[1] = (COLORREF) MMLValueGetInt (pNode, gpszTextColor1, (int)0);
   m_fTextWidth = MMLValueGetDouble (pNode, gpszTextWidth, 1);
   m_dwStrokesNum = (DWORD)MMLValueGetInt (pNode, gpszStrokesNum, 10000);

   m_fSpyglassScale = MMLValueGetDouble (pNode, gpszSpyglassScale, 1);
   m_fSpyglassBlur = MMLValueGetDouble (pNode, gpszSpyglassBlur, 0);
   m_fSpyglassStickOut = MMLValueGetDouble (pNode, gpszSpyglassStickOut, 0);
   m_dwSpyglassShape = (DWORD) MMLValueGetInt (pNode, gpszSpyglassShape, (int)0);

   PCMMLNode2 pSub = NULL;
   PWSTR psz;
   pNode->ContentEnum (pNode->ContentFind (gpszTextSurf), &psz, &pSub);
   if (pSub) {
      if (m_pTextSurf)
         delete m_pTextSurf;
      m_pTextSurf = new CObjectSurface;
      m_pTextSurf->MMLFrom (pSub);
   }

   m_fStrokeMomentum = MMLValueGetDouble (pNode, gpszStrokeMomentum, 0.5);
   m_fStrokeRandom = MMLValueGetDouble (pNode, gpszStrokeRandom, 0.5);
   m_fStrokeZWeight = MMLValueGetDouble (pNode, gpszStrokeZWeight, 0.5);
   m_fStrokeColorWeight = MMLValueGetDouble (pNode, gpszStrokeColorWeight, 0.5);
   m_fStrokeObjectWeight = MMLValueGetDouble (pNode, gpszStrokeObjectWeight, 0.5);
   m_fStrokePrefWeight = MMLValueGetDouble (pNode, gpszStrokePrefWeight, 0.5);
   m_fStrokePrefAngle = MMLValueGetDouble (pNode, gpszStrokePrefAngle, PI/4);
   m_fStrokeZPerp = (BOOL) MMLValueGetInt (pNode, gpszStrokeZPerp, (int) FALSE);
   m_fStrokeColorPerp = (BOOL) MMLValueGetInt (pNode, gpszStrokeColorPerp, (int) FALSE);
   m_fStrokeObjectPerp = (BOOL) MMLValueGetInt (pNode, gpszStrokeObjectPerp, (int) FALSE);
   m_fStrokePrefEither = (BOOL) MMLValueGetInt (pNode, gpszStrokePrefEither, (int) TRUE);
   m_fStrokeCrosshatch = (BOOL) MMLValueGetInt (pNode, gpszStrokeCrosshatch, (int) FALSE);

   m_fStrokeZBlur = MMLValueGetDouble (pNode, gpszStrokeZBlur, 5);
   m_fStrokeColorBlur = MMLValueGetDouble (pNode, gpszStrokeColorBlur, 5);
   m_fStrokeObjectBlur = MMLValueGetDouble (pNode, gpszStrokeObjectBlur, 5);

   m_fStrokeLen = MMLValueGetDouble (pNode, gpszStrokeLen, 5);
   m_fStrokeStep = MMLValueGetDouble (pNode, gpszStrokeStep, 2);
   m_fStrokeAnchor = MMLValueGetDouble (pNode, gpszStrokeAnchor, 0);
   m_fStrokeLenVar = MMLValueGetDouble (pNode, gpszStrokeLenVar, .5);
   m_fStrokePenColor = MMLValueGetDouble (pNode, gpszStrokePenColor, .5);
   m_fStrokePenObject = MMLValueGetDouble (pNode, gpszStrokePenObject, .5);

   m_fDeltaFilterWidth = MMLValueGetDouble (pNode, gpszDeltaFilterWidth, 0);
   m_fBackMatch = MMLValueGetDouble (pNode, gpszBackMatch, 0);
   m_cBackColor = MMLValueGetInt (pNode, gpszBackColor, (int)0);

   MMLValueGetPoint (pNode, gpszStrokeWidth, &m_pStrokeWidth);

   MMLValueGetPoint (pNode, gpszColorVar, &m_pColorVar);
   MMLValueGetPoint (pNode, gpszColorPalette, &m_pColorPalette);
   m_fColorHueShift = MMLValueGetDouble (pNode, gpszColorHueShift, 0);
   m_fColorUseFixed = (BOOL) MMLValueGetInt (pNode, gpszColorUseFixed, (int)0);
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"ColorFixed%d", (int) i);
      m_acColorFixed[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, (int)m_acColorFixed[i]);
   }

   m_dwBrushShape = (DWORD)MMLValueGetInt (pNode, gpszBrushShape, 0);
   MMLValueGetPoint (pNode, gpszBrushParam, &m_pBrushParam);

   return TRUE;
}




/*********************************************************************************
CNPREffectPainting::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectPainting::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectPainting::CloneEffect - From CNPREffect
*/
CNPREffectPainting * CNPREffectPainting::CloneEffect (void)
{
   PCNPREffectPainting pNew = new CNPREffectPainting(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fIgnoreBackground = m_fIgnoreBackground;

   pNew->m_dwTextBack = m_dwTextBack;
   memcpy (&pNew->m_acTextColor, &m_acTextColor, sizeof(m_acTextColor));
   pNew->m_fTextWidth = m_fTextWidth;
   if (pNew->m_pTextSurf)
      delete pNew->m_pTextSurf;
   pNew->m_pTextSurf = m_pTextSurf->Clone();

   pNew->m_dwStrokesNum = m_dwStrokesNum;
   pNew->m_fStrokeMomentum = m_fStrokeMomentum;
   pNew->m_fStrokeRandom = m_fStrokeRandom;
   pNew->m_fStrokeZWeight = m_fStrokeZWeight;
   pNew->m_fStrokeColorWeight = m_fStrokeColorWeight;
   pNew->m_fStrokeObjectWeight = m_fStrokeObjectWeight;
   pNew->m_fStrokePrefWeight = m_fStrokePrefWeight;
   pNew->m_fStrokePrefAngle = m_fStrokePrefAngle;
   pNew->m_fStrokeColorPerp = m_fStrokeColorPerp;
   pNew->m_fStrokeObjectPerp = m_fStrokeObjectPerp;
   pNew->m_fStrokeCrosshatch = m_fStrokeCrosshatch;
   pNew->m_fStrokePrefEither = m_fStrokePrefEither;
   pNew->m_fStrokeZBlur = m_fStrokeZBlur;
   pNew->m_fStrokeColorBlur = m_fStrokeColorBlur;
   pNew->m_fStrokeObjectBlur = m_fStrokeObjectBlur;

   pNew->m_fStrokeLen = m_fStrokeLen;
   pNew->m_fStrokeStep = m_fStrokeStep;
   pNew->m_fStrokeAnchor = m_fStrokeAnchor;
   pNew->m_fStrokeLenVar = m_fStrokeLenVar;
   pNew->m_fStrokePenColor = m_fStrokePenColor;
   pNew->m_fStrokePenObject = m_fStrokePenObject;

   pNew->m_fSpyglassScale = m_fSpyglassScale;
   pNew->m_fSpyglassBlur = m_fSpyglassBlur;
   pNew->m_fSpyglassStickOut = m_fSpyglassStickOut;
   pNew->m_dwSpyglassShape = m_dwSpyglassShape;

   pNew->m_pStrokeWidth.Copy (&m_pStrokeWidth);

   pNew->m_pColorVar.Copy (&m_pColorVar);
   pNew->m_pColorPalette.Copy (&m_pColorPalette);
   pNew->m_fColorHueShift = m_fColorHueShift;
   pNew->m_fColorUseFixed = m_fColorUseFixed;
   memcpy (&pNew->m_acColorFixed, &m_acColorFixed, sizeof(m_acColorFixed));

   pNew->m_fDeltaFilterWidth = m_fDeltaFilterWidth;
   pNew->m_fBackMatch = m_fBackMatch;
   pNew->m_cBackColor = m_cBackColor;

   pNew->m_dwBrushShape = m_dwBrushShape;
   pNew->m_pBrushParam.Copy (&m_pBrushParam);

   return pNew;
}





/*********************************************************************************
CNPREffectPainting::ClearBackground - Wipes out the background image as
specified by the settings.

inputs
   PCImage        pImageDest - Dest image. If NULL then use pFImage
   PCFImage       pFImageDest - Dest floating point image. If NULL then use pImage
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectPainting::ClearBackground (PCImage pImageDest, PCFImage pFImageDest)
{
   PIMAGEPIXEL pip = pImageDest ? pImageDest->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageDest ? NULL : pFImageDest->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();
   BOOL f360 = pImageDest ? pImageDest->m_f360 : pFImageDest->m_f360;

   WORD aw[2][3];
   DWORD x,y,i;
   Gamma (m_acTextColor[0], &aw[0][0]);
   Gamma (m_acTextColor[1], &aw[1][0]);

   switch (m_dwTextBack) {
   case 0:  // none
      return TRUE;

   case 1:  // solid
      {
         for (y = 0; y < dwHeight; y++) {
            for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
               // ignore background?
               if (m_fIgnoreBackground) {
                  WORD w = pip ? HIWORD(pip->dwID) : HIWORD(pfp->dwID);
                  if (!w)
                     continue;
               }

               if (pip)
                  memcpy (&pip->wRed, &aw[0][0], 3 * sizeof(WORD));
               else
                  for (i = 0; i < 3; i++)
                     (&pfp->fRed)[i] = aw[0][i];
            } // x
         } // y
      }
      return TRUE;

   case 2:  // top to bttom
      {
         for (y = 0; y < dwHeight; y++) {
            WORD wScale, wInv;
            WORD awCur[3];
            wScale = y * 0xffff / dwHeight;
            wInv = 0xffff - wScale;
            for (i = 0; i < 3; i++)
               awCur[i] = (WORD)(((DWORD)aw[0][i] * (DWORD)wInv +
                  (DWORD)aw[1][i] * (DWORD)wScale) >> 16);

            for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
               // ignore background?
               if (m_fIgnoreBackground) {
                  WORD w = pip ? HIWORD(pip->dwID) : HIWORD(pfp->dwID);
                  if (!w)
                     continue;
               }

               if (pip)
                  memcpy (&pip->wRed, &awCur[0], 3 * sizeof(WORD));
               else
                  for (i = 0; i < 3; i++)
                     (&pfp->fRed)[i] = awCur[i];
            } // x
         } // y
      }
      return TRUE;

   case 3:  // left to right
      {
         for (x = 0; x < dwWidth; x++) {
            WORD wScale, wInv;
            WORD awCur[3];
            wScale = x * 0xffff / dwWidth;
            wInv = 0xffff - wScale;
            for (i = 0; i < 3; i++)
               awCur[i] = (WORD)(((DWORD)aw[0][i] * (DWORD)wInv +
               (DWORD)aw[1][i] * (DWORD)wScale) >> 16);

            pip = pImageDest ? pImageDest->Pixel(x,0) : NULL;
            pfp = pImageDest ? NULL : pFImageDest->Pixel(x,0);

            for (y = 0; y < dwHeight; y++, (pip ? (pip+=dwWidth) : (PIMAGEPIXEL)(pfp+=dwWidth))) {
               // ignore background?
               if (m_fIgnoreBackground) {
                  WORD w = pip ? HIWORD(pip->dwID) : HIWORD(pfp->dwID);
                  if (!w)
                     continue;
               }

               if (pip)
                  memcpy (&pip->wRed, &awCur[0], 3 * sizeof(WORD));
               else
                  for (i = 0; i < 3; i++)
                     (&pfp->fRed)[i] = awCur[i];
            } // y
         } // x
      }
      return TRUE;
   } // switch

   // else, use texture
   m_fTextWidth = max(m_fTextWidth, 0.0001);
   fp fWidthPerPixel = m_fTextWidth / (fp)dwWidth;

   // try creating the texture
   PCTextureMapSocket pTexture;
   RENDERSURFACE rs;
   memset (&rs, 0, sizeof(rs));
   memcpy (&rs.abTextureMatrix, &m_pTextSurf->m_mTextureMatrix, sizeof(m_pTextSurf->m_mTextureMatrix));
   memcpy (&rs.afTextureMatrix, &m_pTextSurf->m_afTextureMatrix, sizeof(m_pTextSurf->m_afTextureMatrix));
   rs.fUseTextureMap = m_pTextSurf->m_fUseTextureMap;
   rs.gTextureCode = m_pTextSurf->m_gTextureCode;
   rs.gTextureSub = m_pTextSurf->m_gTextureSub;
   memcpy (&rs.Material, &m_pTextSurf->m_Material, sizeof(m_pTextSurf->m_Material));
   wcscpy (rs.szScheme, m_pTextSurf->m_szScheme);
   rs.TextureMods = m_pTextSurf->m_TextureMods;
   pTexture = TextureCacheGet (m_dwRenderShard, &rs, NULL, NULL);
   if (!pTexture)
      return TRUE;   // couldnt create, so no changes

   // see delta for right...
   TEXTPOINT5 tpRightDelta, tpDownDelta;
   TEXTUREPOINT tp;
   CPoint pZero, p;
   tp.h = fWidthPerPixel;
   tp.v = 0;
   TextureMatrixMultiply2D (m_pTextSurf->m_afTextureMatrix, &tp);
   tpRightDelta.hv[0] = tp.h;
   tpRightDelta.hv[1] = tp.v;
   tp.h = 0;
   tp.v = fWidthPerPixel;
   TextureMatrixMultiply2D (m_pTextSurf->m_afTextureMatrix, &tp);
   tpDownDelta.hv[0] = tp.h;
   tpDownDelta.hv[1] = tp.v;
   pZero.Zero();
   p.Zero();
   p.p[0] = fWidthPerPixel;
   p.MultiplyLeft (&m_pTextSurf->m_mTextureMatrix);
   pZero.MultiplyLeft  (&m_pTextSurf->m_mTextureMatrix);
   p.Subtract (&pZero);
   for (i = 0; i < 3; i++)
      tpRightDelta.xyz[i] = p.p[i];
   p.Zero();
   p.p[1] = -fWidthPerPixel;
   p.MultiplyLeft (&m_pTextSurf->m_mTextureMatrix);
   p.Subtract (&pZero);
   for (i = 0; i < 3; i++)
      tpDownDelta.xyz[i] = p.p[i];

   // find max
   TEXTPOINT5 tpMax;
   for (i = 0; i < 2; i++)
      tpMax.hv[i] = max(fabs(tpRightDelta.hv[i]), fabs(tpDownDelta.hv[i]));
   for (i = 0; i < 3; i++)
      tpMax.xyz[i] = max(fabs(tpRightDelta.xyz[i]), fabs(tpDownDelta.xyz[i]));

   float f;
   DWORD j;
   TEXTPOINT5 tpCurY, tpCurX;
   CMaterial Material;
   memset (&tpCurY, 0, sizeof(tpCurY));
   pTexture->MaterialGet (0, &Material);
   for (y = 0; y < dwHeight; y++) {
      PIMAGEPIXEL pipStart = pip;
      PFIMAGEPIXEL pfpStart = pfp;

      // increase by delta
      for (i = 0; i < 2; i++)
         tpCurY.hv[i] += tpDownDelta.hv[i];
      for (i = 0; i < 3; i++)
         tpCurY.xyz[i] += tpDownDelta.xyz[i];

      tpCurX = tpCurY;

      for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
         // increase by delta so don't forget
         for (i = 0; i < 2; i++)
            tpCurX.hv[i] += tpRightDelta.hv[i];
         for (i = 0; i < 3; i++)
            tpCurX.xyz[i] += tpRightDelta.xyz[i];

         // ignore background?
         if (m_fIgnoreBackground) {
            WORD w = pip ? HIWORD(pip->dwID) : HIWORD(pfp->dwID);
            if (!w)
               continue;
         }

         float afGlow[3];
         WORD awColor[3];
         memset (afGlow, 0 ,sizeof(afGlow));
         pTexture->FillPixel (0, TMFP_ALL, &awColor[0], &tpCurX, &tpMax, &Material, &afGlow[0], FALSE);
            // NOTE: Passing in fHighQuality = FALSE into fillpixel
         for (i = 0; i < 3; i++)
            afGlow[i] += awColor[i];


         // write it back
         if (pip) {
            for (j = 0; j < 3; j++) {
               f = afGlow[j];
               f = max(f, 0);
               f = min(f, (float)0xffff);
               (&pip->wRed)[j] = (WORD)f;
            } // j
         }
         else
            for (j = 0; j < 3; j++)
               (&pfp->fRed)[j] = afGlow[j];

      } // x

   } // y


   // finally, release the texture
   TextureCacheRelease (m_dwRenderShard, pTexture);

   return TRUE;
}

static int __cdecl STROKEINFOCompare (const void *p1, const void *p2)
{
   PSTROKEINFO ph1 = (PSTROKEINFO)p1;
   PSTROKEINFO ph2 = (PSTROKEINFO)p2;

   if (ph1->fZ > ph2->fZ)
      return -1;
   else if (ph1->fZ < ph2->fZ)
      return 1;
   else
      return 0;
}

/*********************************************************************************
CNPREffectPainting::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectPainting::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTPAINT pep = (PEMTPAINT)pParams;

   PSTROKEINFO psi = pep->psi;
   PCImage pImageSrc = pep->pImageSrc;
   PCImage pImageDest = pep->pImageDest;
   PCFImage pFImageSrc = pep->pFImageSrc;
   PCFImage pFImageDest = pep->pFImageDest;
   PCPaintingStroke pStroke = pep->paStroke + dwThread;

   srand(GetTickCount() + dwThread * 1000);  // set random seed

   DWORD i;
   psi += pep->dwStart;
   for (i = pep->dwStart; i < pep->dwEnd; i++, psi++)
      Stroke (dwThread, psi->wX, psi->wY, pImageSrc, pFImageSrc, pImageDest, pFImageDest, pStroke);
}

/*********************************************************************************
CNPREffectPainting::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImageSrc - Image. If NULL then use pFImage
   PCFImage       pFImageSrc - Floating point image. If NULL then use pImage
   PCImage        pImageDest - Dest image. If NULL then use pFImage
   PCFImage       pFImageDest - Dest floating point image. If NULL then use pImage
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectPainting::RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                                  PCImage pImageDest, PCFImage pFImageDest,
                                  PCProgressSocket pProgress)
{
   CBlurBuf blur;

   // clear the background
   if (!ClearBackground (pImageDest, pFImageDest))
      return FALSE;

   PIMAGEPIXEL pip = pImageSrc ? pImageSrc->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageSrc ? NULL : pFImageSrc->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();
   BOOL f360 = pImageDest ? pImageDest->m_f360 : pFImageDest->m_f360;


   CPaintingStroke avStroke[MAXRAYTHREAD];
   srand(GetTickCount());  // set random seed
   DWORD i;
   for (i = 0; i < MAXRAYTHREAD; i++)
      if (!avStroke[i].Init (m_dwBrushShape, 8, &m_pBrushParam))
         return FALSE;

   // set up filter for stroke probability
   if (m_fDeltaFilterWidth > 0)
      blur.Init (m_fDeltaFilterWidth * (fp)dwWidth / 100.0, 2);

   // set up some global values
#define FIELDANGLES        16    // number of angles of resultion for fields
   CFieldDetector FieldColor, FieldZ, FieldObject;
   FieldColor.Init (max((fp)dwWidth*m_fStrokeColorBlur/100.0,2), FIELDANGLES);
   FieldZ.Init (max((fp)dwWidth*m_fStrokeZBlur/100.0,2), FIELDANGLES);
   FieldObject.Init (max((fp)dwWidth*m_fStrokeObjectBlur/100.0,2), FIELDANGLES);
   m_pFieldColor = &FieldColor;
   m_pFieldZ = &FieldZ;
   m_pFieldObject = &FieldObject;
   m_tpPrefAngle.h = sin (m_fStrokePrefAngle);
   m_tpPrefAngle.v = -cos (m_fStrokePrefAngle); // do negative because in screen space

   if (!m_dwStrokesNum)
      goto donestrokes;

   // how many strokes?
   if (!m_memStrokes.Required ((m_dwStrokesNum + m_dwStrokesNum/10) * sizeof(STROKEINFO)))
      return FALSE;
   DWORD dwAllocated = (DWORD)m_memStrokes.m_dwAllocated / sizeof(STROKEINFO);
   PSTROKEINFO psi = (PSTROKEINFO)m_memStrokes.p;
   DWORD dwNumStrokes = 0;

   // background color
   WORD awBackColor[3];
   Gamma (m_cBackColor, &awBackColor[0]);

   // spyglass
   // determine circle height/width
   DWORD dwMin = min(dwWidth, dwHeight);
   DWORD dwCircleWidth = dwMin;
   DWORD dwCircleHeight = dwMin;
   if (m_dwSpyglassShape == 2) {
      // elliptical
      dwCircleWidth = dwWidth;
      dwCircleHeight = dwHeight;
   }
   dwCircleWidth = (DWORD)((fp)dwCircleWidth * m_fSpyglassScale / 2.0);   // radius
   dwCircleHeight = (DWORD)((fp)dwCircleHeight * m_fSpyglassScale / 2.0);
   dwCircleWidth = max(dwCircleWidth, 1);
   dwCircleHeight = max(dwCircleHeight, 1);

   // center...
   TEXTUREPOINT afCenter[2];
   afCenter[0].h = (int)dwWidth/2;
   afCenter[0].v = (int)dwHeight/2;
   if (m_dwSpyglassShape == 3) {
      // binoculars
      afCenter[1] = afCenter[0];
      afCenter[0].h = (fp)dwMin/2;
      afCenter[1].h = (fp)dwWidth - (fp)dwMin/2;
   }

   fp fScaleX = 1.0 / (fp)dwCircleWidth;
   fp fScaleY = 1.0 / (fp)dwCircleHeight;

   // blur square
   m_fSpyglassBlur = max(m_fSpyglassBlur, 0.00001);
   fp fBlurSquare = (1.0 - m_fSpyglassBlur) * (1.0 - m_fSpyglassBlur);


   // figure out area per stroke
   double fArea = (double)dwWidth * (double)dwHeight / (double)m_dwStrokesNum;
   fp fSize = (fp)sqrt(fArea);
   fp fX, fY;
   for (fY = 0; fY < (fp)dwHeight; fY += fSize)
      for (fX = 0; fX < (fp)dwWidth; fX += fSize) {
         // calulate the pixel
         fp fXCur, fYCur;
         fYCur = randf(fY, fY+fSize);
         if (fYCur >= (fp)dwHeight)
            continue; // off screen
         fXCur = randf(fX, fX+fSize);
         if (fXCur >= (fp)dwWidth) {
            if (!f360)
               continue;   // off screen
            fXCur = myfmod(fXCur, (fp)dwWidth); // so doesn't loop
         }

         psi[dwNumStrokes].wX = (WORD)fXCur;
         psi[dwNumStrokes].wY = (WORD)fYCur;

         // get the pixel
         pip = pImageSrc ? pImageSrc->Pixel(psi[dwNumStrokes].wX, psi[dwNumStrokes].wY) : NULL;
         pfp = pImageSrc ? NULL : pFImageSrc->Pixel(psi[dwNumStrokes].wX, psi[dwNumStrokes].wY);

         // ignore background?
         if (m_fIgnoreBackground) {
            WORD w = pip ? HIWORD(pip->dwID) : HIWORD(pfp->dwID);
            if (!w)
               continue;
         }

         // ignore if too far from filter width
         if (m_fDeltaFilterWidth > 0) {
            WORD awColor[3];
            float afColor[3];
            float fDist = 0;

            if (pip) {
               blur.Blur (pImageSrc, (int)psi[dwNumStrokes].wX, (int)psi[dwNumStrokes].wY,
                  FALSE, NULL, 0, &awColor[0]);
               for (i = 0; i < 3; i++)
                  fDist += abs((int)(DWORD)awColor[i] - (int)(DWORD)(&pip->wRed)[i]);
            }
            else {
               blur.Blur (pFImageSrc, (int)psi[dwNumStrokes].wX, (int)psi[dwNumStrokes].wY,
                  FALSE, NULL, &afColor[0]);
               for (i = 0; i < 3; i++)
                  fDist += fabs(afColor[i] - (&pfp->fRed)[i]);
            }
            fDist /= (256*3/10);
            if ((rand() % 256) >= fDist)
               continue;   // not enough difference
         }

         // ignore if similar to base color
         if (m_fBackMatch) {
            float fDist = 0;

            if (pip) {
               for (i = 0; i < 3; i++)
                  fDist += abs((int)(DWORD)awBackColor[i] - (int)(DWORD)(&pip->wRed)[i]);
            }
            else {
               for (i = 0; i < 3; i++)
                  fDist += fabs((float)awBackColor[i] - (&pfp->fRed)[i]);
            }
            fDist /= (float)0xffff; // NOTE: Also had /3, but took out to scale
            if (fDist < m_fBackMatch)
               continue;
         }

         // spyglass
         if (m_dwSpyglassShape) {
            // if out too close then dont affect
            if (pip) {
               if (pip->fZ < m_fSpyglassStickOut)
                  goto donespyglass;
            }
            else {
               if (pfp->fZ < m_fSpyglassStickOut)
                  goto donespyglass;
            }

            // distance from the center...
            fp fX = ((fp)fXCur - afCenter[0].h) * fScaleX;
            fp fY = ((fp)fYCur - afCenter[0].v) * fScaleY;
            fp fDist = fX * fX + fY * fY;
            if (m_dwSpyglassShape == 3) {
               // binoculars
               fX = ((fp)fXCur - afCenter[1].h) * fScaleX;
               fY = ((fp)fYCur - afCenter[1].v) * fScaleY;
               fp fDist2 = fX * fX + fY * fY;
               fDist = min(fDist, fDist2);
            }

            // if closer than blue then all visible
            if (fDist <= fBlurSquare)
               goto donespyglass;

            // if further away then all black
            if (fDist >= 1)
               continue;

            // else, blend
            // note: Dealing with square space so get rid of mach band
            fDist -= fBlurSquare;
            fDist /= (1.0 - fBlurSquare);
            //fDist = sqrt(fDist);
            //fDist -= (1.0 - m_fSpyglassBlur);
            //fDist /= m_fSpyglassBlur;
            // fDist = min(fDist, 1);
            fDist *= 256;
            if ((rand()%256) < fDist)
               continue;
         } // spyglassshape
donespyglass:

         // add it
         psi[dwNumStrokes].fZ = pip ? pip->fZ : pfp->fZ;
         dwNumStrokes++;
         if (dwNumStrokes >= dwAllocated)
            break;   // used all allocated
      } // fX, fY

   // sort the strokes
   qsort (psi, dwNumStrokes, sizeof(STROKEINFO), STROKEINFOCompare);

   // BUGFIX - multithreaded
   EMTPAINT ep;
   memset (&ep, 0, sizeof(ep));
   ep.pImageSrc = pImageSrc;
   ep.pImageDest = pImageDest;
   ep.pFImageSrc = pFImageSrc;
   ep.pFImageDest = pFImageDest;
   ep.psi = psi;
   ep.paStroke = avStroke;
   ThreadLoop (0, dwNumStrokes, 8, &ep, sizeof(ep), pProgress);
      // NOTE: Use a lot of passes so that get layers in drawing

   // loop through all the strokes drawing lines
   // old code
   //for (i = 0; i < dwNumStrokes; i++, psi++) {
   //   if (!(i % 256) && pProgress)
   //      pProgress->Update ((fp)i / (fp)dwNumStrokes);
   //
   //   Stroke (psi->wX, psi->wY, pImageSrc, pFImageSrc, pImageDest, pFImageDest, &vStroke);
   //} // i


donestrokes:

   return TRUE;
}


/*********************************************************************************
CNPREffectPainting::Render - From CNPREffect
*/
BOOL CNPREffectPainting::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pOrig, NULL, pDest, NULL, pProgress);
}



/*********************************************************************************
CNPREffectPainting::Render - From CNPREffect
*/
BOOL CNPREffectPainting::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pOrig, NULL, pDest, pProgress);
}


/*****************************************************************************
RenderSceneTabs - This code displays the tabs used for the RSTABS macro.

inputs
   DWORD          dwTab - Tab ID that's currently selected
   DWORD          dwNum - Number of tabs
   PWSTR          *ppsz - Pointer to an array of dwNum tabs
   PWSTR          *ppszHelp - Pointer to an arrya of dwNum help entries
   DWORD          *padwID - Array of dwNum IDs for each tab
   DWORD          dwSkipNum - Number of IDs stored in padwSkip
   DWORD          *padwSkip - If a tab is this number then it's skipped
returns
   PWSTR - gMemTemp.p with text
*/
static PWSTR RenderSceneTabs (DWORD dwTab, DWORD dwNum, PWSTR *ppsz, PWSTR *ppszHelp, DWORD *padwID,
                       DWORD dwSkipNum, DWORD *padwSkip)
{
   MemCat (&gMemTemp, L"<tr>");

   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      for (j = 0; j < dwSkipNum; j++)
         if (padwID[i] == padwSkip[j])
            break;   // skip this
      if (j < dwSkipNum)
         continue;

      if (!ppsz[i]) {
         MemCat (&gMemTemp, L"<td/>");
         continue;
      }

      MemCat (&gMemTemp, L"<td align=center");
      if (padwID[i] != dwTab)
         MemCat (&gMemTemp, L" bgcolor=#8080a0");
      MemCat (&gMemTemp,
         L">"
         L"<a href=tabpress:");
      MemCat (&gMemTemp, (int)padwID[i]);
      MemCat (&gMemTemp, L">"
         L"<bold>");
      MemCatSanitize (&gMemTemp, ppsz[i]);
      MemCat (&gMemTemp, L"</bold>"
         L"<xHoverHelp>");
      MemCatSanitize (&gMemTemp, ppszHelp[i]);
      MemCat (&gMemTemp,
         L"</xHoverHelp>"
         L"</a>"
         L"</td>");
   } // i

   MemCat (&gMemTemp, L"</tr>");
   return (PWSTR)gMemTemp.p;
}



/****************************************************************************
EffectPaintingPage
*/
BOOL EffectPaintingPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPAINTINGPAGE pmp = (PPAINTINGPAGE)pPage->m_pUserData;
   PCNPREffectPainting pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         // set the checkbox
         if (pControl = pPage->ControlFind (L"ignorebackground"))
            pControl->AttribSetBOOL (Checked(), pv->m_fIgnoreBackground);
         if (pControl = pPage->ControlFind (L"strokeprefeither"))
            pControl->AttribSetBOOL (Checked(), pv->m_fStrokePrefEither);
         if (pControl = pPage->ControlFind (L"strokezperp"))
            pControl->AttribSetBOOL (Checked(), pv->m_fStrokeZPerp);
         if (pControl = pPage->ControlFind (L"strokecolorperp"))
            pControl->AttribSetBOOL (Checked(), pv->m_fStrokeColorPerp);
         if (pControl = pPage->ControlFind (L"strokeobjectperp"))
            pControl->AttribSetBOOL (Checked(), pv->m_fStrokeObjectPerp);
         if (pControl = pPage->ControlFind (L"colorusefixed"))
            pControl->AttribSetBOOL (Checked(), pv->m_fColorUseFixed);
         if (pControl = pPage->ControlFind (L"strokecrosshatch"))
            pControl->AttribSetBOOL (Checked(), pv->m_fStrokeCrosshatch);

         // edit
         MeasureToString (pPage, L"textwidth", pv->m_fTextWidth);
         DoubleToControl (pPage, L"strokesnum", pv->m_dwStrokesNum);
         AngleToControl (pPage, L"strokeprefangle", pv->m_fStrokePrefAngle);
         DoubleToControl (pPage, L"strokezblur", pv->m_fStrokeZBlur);
         DoubleToControl (pPage, L"strokecolorblur", pv->m_fStrokeColorBlur);
         DoubleToControl (pPage, L"strokeobjectblur", pv->m_fStrokeObjectBlur);
         DoubleToControl (pPage, L"strokelen", pv->m_fStrokeLen);
         DoubleToControl (pPage, L"strokestep", pv->m_fStrokeStep);
         DoubleToControl (pPage, L"deltafilterwidth", pv->m_fDeltaFilterWidth);
         MeasureToString (pPage, L"spyglassstickout", pv->m_fSpyglassStickOut);

         ComboBoxSet (pPage, L"brushshape", pv->m_dwBrushShape);

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"strokewidth%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pStrokeWidth.p[i]);
         } // i
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"colorpalette%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pColorPalette.p[i]);
         } // i

         // texture
         ComboBoxSet (pPage, L"textback", pv->m_dwTextBack);
         ComboBoxSet (pPage, L"spyglassshape", pv->m_dwSpyglassShape);

         // colors
         FillStatusColor (pPage, L"textcolor0", pv->m_acTextColor[0]);
         FillStatusColor (pPage, L"textcolor1", pv->m_acTextColor[1]);
         FillStatusColor (pPage, L"backcolor", pv->m_cBackColor);

         for (i = 0; i < 5; i++) {
            swprintf (szTemp, L"colorfixed%d", (int) i);
            FillStatusColor (pPage, szTemp, pv->m_acColorFixed[i]);
         } // i

         // scrollbars
         if (pControl = pPage->ControlFind (L"strokeprefweight"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokePrefWeight * 100));
         if (pControl = pPage->ControlFind (L"strokemomentum"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokeMomentum * 100));
         if (pControl = pPage->ControlFind (L"strokerandom"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokeRandom * 100));
         if (pControl = pPage->ControlFind (L"strokeprefweight"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokePrefWeight * 100));
         if (pControl = pPage->ControlFind (L"strokezweight"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokeZWeight * 100));
         if (pControl = pPage->ControlFind (L"strokecolorweight"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokeColorWeight * 100));
         if (pControl = pPage->ControlFind (L"strokeobjectweight"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokeObjectWeight * 100));
         if (pControl = pPage->ControlFind (L"strokePenObject"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokePenObject * 100));
         if (pControl = pPage->ControlFind (L"strokeanchor"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokeAnchor * 100));
         if (pControl = pPage->ControlFind (L"strokelenvar"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokeLenVar * 100));
         if (pControl = pPage->ControlFind (L"strokePenColor"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStrokePenColor * 100));
         if (pControl = pPage->ControlFind (L"strokewidth3"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_pStrokeWidth.p[3] * 100));
         if (pControl = pPage->ControlFind (L"colorhueshift"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fColorHueShift * 100));
         if (pControl = pPage->ControlFind (L"backmatch"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBackMatch * 100));
         if (pControl = pPage->ControlFind (L"spyglassscale"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fSpyglassScale * 100));
         if (pControl = pPage->ControlFind (L"spyglassblur"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fSpyglassBlur * 100));
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"colorvar%d", (int)i);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSetInt (Pos(), (int)(pv->m_pColorVar.p[i] * 100));
         }
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"brushparam%d", (int)i);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSetInt (Pos(), (int)(pv->m_pBrushParam.p[i] * 100));
         }


      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE)pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"textback")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwTextBack)
               return TRUE;   // no change

            pv->m_dwTextBack = dwVal;
            pPage->Message (ESCM_USER+189);  // update bitmap

            return TRUE;
         } // pattern
         else if (!_wcsicmp(psz, L"brushshape")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwBrushShape)
               return TRUE;   // no change

            pv->m_dwBrushShape = dwVal;
            pPage->Message (ESCM_USER+189);  // update bitmap

            return TRUE;
         } // pattern
         else if (!_wcsicmp(psz, L"spyglassshape")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwSpyglassShape)
               return TRUE;   // no change

            pv->m_dwSpyglassShape = dwVal;
            pPage->Message (ESCM_USER+189);  // update bitmap

            return TRUE;
         } // pattern
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszColorChangeFixed = L"changecolorfixed";
         DWORD dwColorChangeFixedLen = (DWORD)wcslen(pszColorChangeFixed);

         // see about all effects checked or unchecked
         if (!_wcsicmp(p->pControl->m_pszName, L"alleffects")) {
            pmp->fAllEffects = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }

         if (!_wcsicmp(p->pControl->m_pszName, L"ignorebackground")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_fIgnoreBackground) {
               pv->m_fIgnoreBackground = fNew;
               pPage->Message (ESCM_USER+189);  // update bitmap
            }
            return TRUE;
         };
         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"changetextcolor0")) {
            pv->m_acTextColor[0] = AskColor (pPage->m_pWindow->m_hWnd,
               pv->m_acTextColor[0], pPage, L"textcolor0");
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changetextcolor1")) {
            pv->m_acTextColor[1] = AskColor (pPage->m_pWindow->m_hWnd,
               pv->m_acTextColor[1], pPage, L"textcolor1");
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changebackcolor")) {
            pv->m_cBackColor = AskColor (pPage->m_pWindow->m_hWnd,
               pv->m_cBackColor, pPage, L"backcolor");
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changetext")) {
            CWorld World;
            World.RenderShardSet (pv->m_dwRenderShard);
            if (TextureSelDialog (pv->m_dwRenderShard, pPage->m_pWindow->m_hWnd, pv->m_pTextSurf, pmp->pWorld ? pmp->pWorld : &World))
               pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"strokeprefeither")) {
            pv->m_fStrokePrefEither = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"strokezperp")) {
            pv->m_fStrokeZPerp = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"strokecolorperp")) {
            pv->m_fStrokeColorPerp = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"strokeobjectperp")) {
            pv->m_fStrokeObjectPerp = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"strokecrosshatch")) {
            pv->m_fStrokeCrosshatch = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorusefixed")) {
            pv->m_fColorUseFixed = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszColorChangeFixed, dwColorChangeFixedLen)) {
            DWORD dwNum = _wtoi(p->pControl->m_pszName + dwColorChangeFixedLen);
            WCHAR szTemp[64];
            swprintf (szTemp, L"colorfixed%d", (int) dwNum);
            pv->m_acColorFixed[dwNum] = AskColor (pPage->m_pWindow->m_hWnd,
               pv->m_acColorFixed[dwNum], pPage, szTemp);
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }


      }
      break;   // default

   case ESCN_EDITCHANGE:
      {
         // just get all values
         if (pmp->dwTab == 0) {  // background
            MeasureParseString (pPage, L"textwidth", &pv->m_fTextWidth);
         }
         else if (pmp->dwTab == 1) {   // strokes
            pv->m_dwStrokesNum = max(DoubleFromControl (pPage, L"strokesnum"), 0);
            pv->m_fStrokeLen = DoubleFromControl (pPage, L"strokelen");
            pv->m_fStrokeStep = DoubleFromControl (pPage, L"strokestep");
         }
         else if (pmp->dwTab == 2) {   // misc
            pv->m_fDeltaFilterWidth = DoubleFromControl (pPage, L"deltafilterwidth");
            MeasureParseString (pPage, L"spyglassstickout", &pv->m_fSpyglassStickOut);
         }
         else if (pmp->dwTab == 3) {   // stroke path
            pv->m_fStrokePrefAngle = AngleFromControl (pPage, L"strokeprefangle");
            pv->m_fStrokeZBlur = DoubleFromControl (pPage, L"strokezblur");
            pv->m_fStrokeColorBlur = DoubleFromControl (pPage, L"strokecolorblur");
            pv->m_fStrokeObjectBlur = DoubleFromControl (pPage, L"strokeobjectblur");
         }
         else if (pmp->dwTab == 4) {   // stroke shape
            DWORD i;
            WCHAR szTemp[64];
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"strokewidth%d", (int)i);
               pv->m_pStrokeWidth.p[i] = DoubleFromControl (pPage, szTemp);
            } // i
         }
         else if (pmp->dwTab == 5) {   // stroke shape
            DWORD i;
            WCHAR szTemp[64];
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"colorpalette%d", (int)i);
               pv->m_pColorPalette.p[i] = DoubleFromControl (pPage, szTemp);
            } // i
         }

         pPage->Message (ESCM_USER+189);  // update bitmap
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         PWSTR pszTab = L"tabpress:";
         DWORD dwLen = (DWORD)wcslen(pszTab);

         if (!wcsncmp(p->psz, pszTab, dwLen)) {
            pmp->dwTab = (DWORD)_wtoi(p->psz + dwLen);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }

      }
      break;

   case ESCN_SCROLL:
   //case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         fp *pf = NULL;
         PWSTR pszColorVar = L"colorvar", pszBrushParam = L"brushparam";
         DWORD dwColorVarLen = (DWORD)wcslen(pszColorVar),
            dwBrushParamLen = (DWORD)wcslen(pszBrushParam);

         if (!_wcsicmp (psz, L"strokeprefweight"))
            pf = &pv->m_fStrokePrefWeight;
         else if (!_wcsicmp (psz, L"strokemomentum"))
            pf = &pv->m_fStrokeMomentum;
         else if (!_wcsicmp (psz, L"strokerandom"))
            pf = &pv->m_fStrokeRandom;
         else if (!_wcsicmp (psz, L"strokezweight"))
            pf = &pv->m_fStrokeZWeight;
         else if (!_wcsicmp (psz, L"strokecolorweight"))
            pf = &pv->m_fStrokeColorWeight;
         else if (!_wcsicmp (psz, L"strokeobjectweight"))
            pf = &pv->m_fStrokeObjectWeight;
         else if (!_wcsicmp (psz, L"strokeanchor"))
            pf = &pv->m_fStrokeAnchor;
         else if (!_wcsicmp (psz, L"strokeLenVar"))
            pf = &pv->m_fStrokeLenVar;
         else if (!_wcsicmp (psz, L"strokePenColor"))
            pf = &pv->m_fStrokePenColor;
         else if (!_wcsicmp (psz, L"strokePenObject"))
            pf = &pv->m_fStrokePenObject;
         else if (!_wcsicmp (psz, L"strokewidth3"))
            pf = &pv->m_pStrokeWidth.p[3];
         else if (!_wcsicmp (psz, L"colorhueshift"))
            pf = &pv->m_fColorHueShift;
         else if (!_wcsicmp (psz, L"backmatch"))
            pf = &pv->m_fBackMatch;
         else if (!_wcsicmp (psz, L"spyglassscale"))
            pf = &pv->m_fSpyglassScale;
         else if (!_wcsicmp (psz, L"spyglassblur"))
            pf = &pv->m_fSpyglassBlur;
         else if (!wcsncmp(psz, pszColorVar, dwColorVarLen))
            pf = &pv->m_pColorVar.p[_wtoi(psz + dwColorVarLen)];
         else if (!wcsncmp(psz, pszBrushParam, dwBrushParamLen))
            pf = &pv->m_pBrushParam.p[_wtoi(psz + dwBrushParamLen)];

         if (!pf)
            break;   // not known

         *pf = (fp)p->iPos / 100.0;
         pPage->Message (ESCM_USER+189);  // update bitmap
         return TRUE;
      }
      break;

   case ESCM_USER+189:  // update image
      {
         if (pmp->hBit)
            DeleteObject (pmp->hBit);
         if (pmp->fAllEffects)
            pmp->hBit = EffectImageToBitmap (pmp->pTest, pmp->pAllEffects, NULL, pmp->pRender, pmp->pWorld);
         else
            pmp->hBit = EffectImageToBitmap (pmp->pTest, NULL, pmp->pe, pmp->pRender, pmp->pWorld);

         WCHAR szTemp[32];
         swprintf (szTemp, L"%lx", (__int64)pmp->hBit);
         PCEscControl pControl = pPage->ControlFind (L"image");
         if (pControl)
            pControl->AttribSet (L"hbitmap", szTemp);
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszIfTab = L"IFTAB", pszEndIfTab = L"ENDIFTAB";
         DWORD dwIfTabLen = (DWORD)wcslen(pszIfTab), dwEndIfTabLen = (DWORD)wcslen(pszEndIfTab);

         if (!wcsncmp (p->pszSubName, pszIfTab, dwIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwIfTabLen);
            if (dwNum == pmp->dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!wcsncmp (p->pszSubName, pszEndIfTab, dwEndIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwEndIfTabLen);
            if (dwNum == pmp->dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Painting";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)pmp->hBit);
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RSTABS")) {
            PWSTR apsz[] = {
               L"Bkgnd",
               L"Strokes",
               L"Path",
               L"Brush",
               L"Color",
               L"Misc.",
            };
            PWSTR apszHelp[] = {
               L"Selects the background to paint on.",
               L"Controls the number and length of strokes.",
               L"Controls the path of strokes.",
               L"Lets you change the type and size of the brush.",
               L"Controls the color of the stroke.",
               L"Miscellaneous settings.",
            };
            DWORD adwID[] = {
               0,  //L"Back",
               1, // Strokes
               3, // stroke path
               4, // brush
               5, // color
               2, // misc
            };

            CListFixed lSkip;
            lSkip.Init (sizeof(DWORD));


            p->pszSubString = RenderSceneTabs (pmp->dwTab, sizeof(apsz)/sizeof(PWSTR), apsz,
               apszHelp, adwID, lSkip.Num(), (DWORD*)lSkip.Get(0));
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*********************************************************************************
CNPREffectPainting::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectPainting::IsPainterly (void)
{
   return TRUE;
}

/*********************************************************************************
CNPREffectPainting::Dialog - From CNPREffect
*/
BOOL CNPREffectPainting::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   PAINTINGPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pe = this;
   mp.pTest = pTest;
   mp.pRender = pRender;
   mp.pWorld = pWorld;
   mp.pAllEffects = pAllEffects;

   // delete existing
   if (mp.hBit)
      DeleteObject (mp.hBit);
   if (mp.fAllEffects)
      mp.hBit = EffectImageToBitmap (pTest, pAllEffects, NULL, pRender, pWorld);
   else
      mp.hBit = EffectImageToBitmap (pTest, NULL, this, pRender, pWorld);

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTPAINTING, EffectPaintingPage, &mp);
   mp.iVScroll = pWindow->m_iExitVScroll;
   if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
      goto redo;

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}



/*********************************************************************************
CNPREffectPainting::DirectionCalc - Calculates the direction that will take
along the path. m_pFieldXXX must be valid.

inputs
   DWORD          dwX - Current X pixel, must be in image
   DWORD          dwY - Current Y pixel, must be in image
   PCImage        pImage - Image to use, or NULL if use pFImage
   PCFImage       pFImage - Image to use, or NULL if use pImage
   BOOL           fBackwards - Set to TRUE if going backwards (from center to
                     start of stroke)
   BOOL           fCrosshatch - If TRUE this is a crosshatch line.
   PTEXTUREPOINT  ptpDir - Initially filled with the previous return from
                     DirectionCalc, which is a normalized 2D vector. If there
                     wasn't any previous direction then set to 0. This
                     is then modified using effect settings to determine
                     the new vector, which is then modified.
returns
   none
*/
void CNPREffectPainting::DirectionCalc (DWORD dwX, DWORD dwY, PCImage pImage,
                                        PCFImage pFImage, BOOL fBackwards,
                                        BOOL fCrosshatch, PTEXTUREPOINT ptpDir)
{
   // scale by momentum
   ptpDir->h *= m_fStrokeMomentum;
   ptpDir->v *= m_fStrokeMomentum;

   // add random amount
   if (m_fStrokeRandom) {
      ptpDir->h += randf(-m_fStrokeRandom, m_fStrokeRandom);
      ptpDir->v += randf(-m_fStrokeRandom, m_fStrokeRandom);
   }

   // preferred angle
   if (m_fStrokePrefWeight) {
      TEXTUREPOINT tpAngle;

      // if doing cross-hatch then will be opposite preferred angle
      if (fCrosshatch) {
         tpAngle.h = m_tpPrefAngle.v;
         tpAngle.v = -m_tpPrefAngle.h;
      }
      else
         tpAngle = m_tpPrefAngle;

      // scale
      tpAngle.h *= m_fStrokePrefWeight;
      tpAngle.v *= m_fStrokePrefWeight;

      if (fBackwards) {
         tpAngle.h *= -1;
         tpAngle.v *= -1;
      }

      // may want to flip
      if (m_fStrokePrefEither && (tpAngle.h * ptpDir->h + tpAngle.v * ptpDir->v < 0)) {
         tpAngle.h *= -1;
         tpAngle.v *= -1;
      }

      ptpDir->h += tpAngle.h;
      ptpDir->v += tpAngle.v;
   }

   // z weight, color weight, object weight
   DWORD i;
   for (i = 0; i < 3; i++) {
      fp fWeight;
      PCFieldDetector pField;
      BOOL fPerp;
      switch (i) {
      case 0:
      default:
         fWeight = m_fStrokeColorWeight;
         pField = m_pFieldColor;
         fPerp = m_fStrokeColorPerp;
         break;
      case 1:
         fWeight = m_fStrokeZWeight;
         pField = m_pFieldZ;
         fPerp = m_fStrokeZPerp;
         break;
      case 2:
         fWeight = m_fStrokeObjectWeight;
         pField = m_pFieldObject;
         fPerp = m_fStrokeObjectPerp;
         break;
      }

      if (!fWeight)
         continue;   // nothing

      if (fCrosshatch)
         fPerp = !fPerp;

      TEXTUREPOINT tpField;
      pField->FieldCalc (i, dwX, dwY, pImage, pFImage, fPerp, &tpField);

      // flip if going wrong direction
      if (tpField.h * ptpDir->h + tpField.v * ptpDir->v < 0) {
         tpField.h *= -1;
         tpField.v *= -1;
      }

      ptpDir->h += tpField.h * fWeight;
      ptpDir->v += tpField.v * fWeight;
   } // i

   // normalize
   fp fLen;
   while (TRUE) {
      fLen = sqrt(ptpDir->h * ptpDir->h + ptpDir->v * ptpDir->v);
      if (fLen)
         break;

      // else, pitck random number
      ptpDir->h = randf(-1,1);
      ptpDir->v = randf(-1,1);
   };
   fLen = 1.0 / fLen;
   ptpDir->h *= fLen;
   ptpDir->v *= fLen;
}


/*********************************************************************************
CNPREffectPainting::PathCalc - Calculates the path a line should take, and
how long it should be. m_pFieldXXX must be valid.

inputs
   DWORD          dwX - Starting X
   DWORD          dwY - Starting Y
   PCImage        pImage - Image to use, or NULL if use pFImage
   PCFImage       pFImage - Image to use, or NULL if use pImage
   PCListFixed    plPath - List pre-initialized to sizeof(POINT), that will be filled
                  with points for the path.
   DWORD          *pdwAnchor - Filled with the anchor point (index into list)
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectPainting::PathCalc (DWORD dwX, DWORD dwY, PCImage pImage,
                                   PCFImage pFImage, PCListFixed plPath,
                                   DWORD *pdwAnchor)
{
   *pdwAnchor = 0;

   // width and height
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();
   PIMAGEPIXEL pipLast = pImage ? pImage->Pixel(dwX, dwY) : NULL;
   PFIMAGEPIXEL pfpLast = pImage ? NULL : pFImage->Pixel(dwX, dwY);
   BOOL f360 = pImage ? pImage->m_f360 : pFImage->m_f360;

   // determine the length of the stroke in pixels
   fp fTotal = randf(1.0 - m_fStrokeLenVar, 1.0 + m_fStrokeLenVar);
   fp fStep = m_fStrokeStep * (fp)dwWidth / 100.0;
   fStep = max(fStep, 1);  // at least one pixel
   fTotal *= m_fStrokeLen * (fp)dwWidth / 100.0;
   fTotal = max(fTotal, 0);
   fp afLen[2];
   afLen[1] = fTotal * m_fStrokeAnchor;  // before
   afLen[0] = fTotal - afLen[1]; // after

   // put in first point
   POINT pt;
   TEXTUREPOINT tpLoc;
   pt.x = (int)dwX;
   pt.y = (int)dwY;
   plPath->Clear();
   plPath->Add (&pt);
   tpLoc.h = dwX;
   tpLoc.v = dwY;

   // last angle
   TEXTUREPOINT tpMomentum, tpMomentumForward;
   tpMomentum.h = tpMomentum.v = 0;
   tpMomentumForward = tpMomentum;
   BOOL fMomentumBackSet = FALSE;

   // determine if this is a crosshatch
   BOOL fCrosshatch = FALSE;
   if (m_fStrokeCrosshatch && (rand()%2))
      fCrosshatch = TRUE;

   // loop
   DWORD dwBack;
   for (dwBack = 0; dwBack < 2; dwBack++) {
      // if going back then reverse the momentum, and start at beginngin
      if (dwBack) {
         tpMomentum.h = -tpMomentumForward.h;
         tpMomentum.v = -tpMomentumForward.v;
         pt.x = (int)dwX;
         pt.y = (int)dwY;
         tpLoc.h = dwX;
         tpLoc.v = dwY;
         pipLast = pImage ? pImage->Pixel(dwX, dwY) : NULL;
         pfpLast = pImage ? NULL : pFImage->Pixel(dwX, dwY);
      }

      for (; afLen[dwBack] > 0; afLen[dwBack] -= fStep) {
         // given the current location, determine the direction to the new one
         if ((f360 || ((pt.x >= 0) && (pt.x < (int)dwWidth))) && (pt.y >= 0) && (pt.y < (int)dwHeight))
            DirectionCalc ((DWORD)(pt.x + (int)dwWidth*16) % dwWidth, (DWORD)pt.y, pImage, pFImage,
               (BOOL)dwBack, fCrosshatch, &tpMomentum);

         // set the momenetum if not already set
         if (!fMomentumBackSet) {
            tpMomentumForward = tpMomentum;
            fMomentumBackSet = TRUE;
         }

         // increase the location
         fp fLeft = min(fStep, afLen[dwBack]);
         fLeft = max(fLeft, 0);  // dont go backwards if cut too much off
         tpLoc.h += tpMomentum.h * fLeft;
         tpLoc.v += tpMomentum.v * fLeft;
         pt.x = (int)tpLoc.h;
         pt.y = (int)tpLoc.v;

         // get new pixel for penalty
         if ((f360 || ((pt.x >= 0) && (pt.x < (int)dwWidth))) && (pt.y >= 0) && (pt.y < (int)dwHeight)) {
            int ix = (pt.x + (int)dwWidth*16) % (int)dwWidth; // modulo
            PIMAGEPIXEL pipCur = pImage ? pImage->Pixel((DWORD)ix, (DWORD)pt.y) : NULL;
            PFIMAGEPIXEL pfpCur = pImage ? NULL : pFImage->Pixel((DWORD)ix, (DWORD)pt.y);

            fp fPenalty;

            if (pipLast) {
               fPenalty = abs((int)(DWORD)pipLast->wRed - (int)(DWORD)pipCur->wRed) +
                  abs((int)(DWORD)pipLast->wGreen - (int)(DWORD)pipCur->wGreen) +
                  abs((int)(DWORD)pipLast->wBlue - (int)(DWORD)pipCur->wBlue);
               fPenalty = fPenalty / 3.0 / (fp)0xffff * m_fStrokePenColor;

               if (HIWORD(pipLast->dwID) != HIWORD(pipCur->dwID))
                  fPenalty += m_fStrokePenObject * 2;
               else if (LOWORD(pipLast->dwID) != LOWORD(pipCur->dwID))
                  fPenalty += m_fStrokePenObject;

               afLen[dwBack] -= fPenalty * fTotal;
               // BUGFIX - Dont update so compare to original pipLast = pipCur;
            }
            else {
               fPenalty = fabs(pfpLast->fRed - pfpCur->fRed) +
                  fabs(pfpLast->fGreen - pfpCur->fGreen) +
                  fabs(pfpLast->fBlue - pfpCur->fBlue);
               fPenalty = fPenalty / 3.0 / (fp)0xffff * m_fStrokePenColor;

               if (HIWORD(pfpLast->dwID) != HIWORD(pfpCur->dwID))
                  fPenalty += m_fStrokePenObject * 2;
               else if (LOWORD(pfpLast->dwID) != LOWORD(pfpCur->dwID))
                  fPenalty += m_fStrokePenObject;

               afLen[dwBack] -= fPenalty * fTotal;
               // BUGFIX - Dont update so compare to original pfpLast = pfpCur;
            }
         }  // if can compare last


         // add the point
         if (dwBack) {
            plPath->Insert (0, &pt);
            *pdwAnchor = *pdwAnchor + 1;
         }
         else
            plPath->Add (&pt);
      }
   } // dwBack

   return TRUE;
}


/*********************************************************************************
CNPREffectPainting::Stroke - Draws a stroke. m_pFieldXXX must be valid.

inputs
   DWORD          dwThread - Thread form 0 .. MAXRAYTHREAD-1
   DWORD          dwX - Starting X
   DWORD          dwY - Starting Y
   PCImage        pImageSrc - Image to use, or NULL if use pFImage
   PCFImage       pFImageSrc - Image to use, or NULL if use pImage
   PCImage        pImageDest - Image to use, or NULL if use pFImage
   PCFImage       pFImageDest - Image to use, or NULL if use pImage
   PCPaintingStroke pStroke - What to paint with
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectPainting::Stroke (DWORD dwThread, DWORD dwX, DWORD dwY, PCImage pImageSrc, PCFImage pFImageSrc,
                                 PCImage pImageDest, PCFImage pFImageDest,
                                 PCPaintingStroke pStroke)
{
   DWORD dwWidth = pImageSrc ? pImageSrc->Width() : pFImageSrc->Width();
   DWORD i, j, dwThick, dwAnchor;
   if (!PathCalc (dwX, dwY, pImageSrc, pFImageSrc, &m_alPathPOINT[dwThread], &dwAnchor))
      return FALSE;

   // calculate the color of the pixel
   WORD awColor[3];
   PIMAGEPIXEL pip;
   PFIMAGEPIXEL pfp;
   pip = pImageSrc ? pImageSrc->Pixel(dwX, dwY) : NULL;
   pfp = pImageSrc ? NULL : pFImageSrc->Pixel(dwX, dwY);
   if (pip)
      memcpy (&awColor[0], &pip->wRed, sizeof(awColor));
   else for (j = 0; j < 3; j++) {
      float f = (&pfp->fRed)[j];
      f = max(f,0);
      f = min(f,(fp)0xffff);
      awColor[j] = (WORD)f;
   }
   Color (awColor);

   // make up thickness
   if (!dwAnchor && (dwAnchor+1 < m_alPathPOINT[dwThread].Num()))
      dwAnchor++; // if anchor starts at the very beginning then move up one
   m_alPathDWORD[dwThread].Clear();
   fp f, fThick;
   m_alPathDWORD[dwThread].Required (m_alPathPOINT[dwThread].Num());
   for (i = 0; i < m_alPathPOINT[dwThread].Num(); i++) {
      if (i < dwAnchor) {
         f = (fp)i / (fp)dwAnchor;
         fThick = (1.0 - f) * m_pStrokeWidth.p[0] + f * m_pStrokeWidth.p[1];
      }
      else if (i == dwAnchor)
         fThick = m_pStrokeWidth.p[1];
      else {
         f = (fp)(i - dwAnchor) / (fp)(m_alPathPOINT[dwThread].Num() - dwAnchor);
         fThick = (1.0 - f) * m_pStrokeWidth.p[1] + f * m_pStrokeWidth.p[2];
      }

      fThick *= (fp)dwWidth / 100.0;
      if (m_pStrokeWidth.p[3])
         fThick *= randf (1.0 - m_pStrokeWidth.p[3], 1.0 + m_pStrokeWidth.p[3]);
      fThick = max(fThick, 1);
      fThick = min(fThick, 100); // dont make too large
      dwThick = (DWORD)(fThick + 0.5);
      m_alPathDWORD[dwThread].Add (&dwThick);
   }


   // draw it
   return pStroke->Paint ((POINT*)m_alPathPOINT[dwThread].Get(0), (DWORD*)m_alPathDWORD[dwThread].Get(0),
      m_alPathPOINT[dwThread].Num(), 2,   // NOTE: Hardcode smoothing of 2
      pImageDest, pFImageDest, &awColor[0]);
}


/*********************************************************************************
CNPREffectPainting::Color - Modifies the stroke color based on settings.

inputs
   WORD        *pawColor - RGB color. Should be filled with initial value and
                     modified in place
*/
void CNPREffectPainting::Color (WORD *pawColor)
{
   // if using fixed colors then different code
   DWORD i;
   if (m_fColorUseFixed) {
      BYTE abColor[3];
      for (i = 0; i < 3; i++)
         abColor[i] = UnGamma(pawColor[i]);

      DWORD dwDist, dwBestDist = 0;
      DWORD dwBest = -1;
      for (i = 0; i < 5; i++) {
         BYTE r = GetRValue(m_acColorFixed[i]);
         BYTE g = GetGValue(m_acColorFixed[i]);
         BYTE b = GetBValue(m_acColorFixed[i]);

         dwDist = (r >= abColor[0]) ? (r - abColor[0]) : (abColor[0] - r);
         dwDist += (g >= abColor[1]) ? (g - abColor[1]) : (abColor[1] - g);
         dwDist += (b >= abColor[2]) ? (b - abColor[2]) : (abColor[2] - b);

         if ((dwBest == -1) || (dwDist < dwBestDist)) {
            dwBestDist = dwDist;
            dwBest = i;
         }
      } // i

      // save this out
      Gamma (m_acColorFixed[dwBest], pawColor);
      return;
   }

   DWORD adwSplit[3];
   float afScale[3];
   for (i = 0; i < 3; i++) {
      adwSplit[i] = (DWORD) floor(m_pColorPalette.p[i] + 0.5);
      adwSplit[i] = max(adwSplit[i], 1);

      if (i == 0)
         afScale[i] = (float)adwSplit[i] / 256.0;
      else if (adwSplit[i] >= 2)
         afScale[i] = (float)(adwSplit[i] - 1) / 256.0;
      else
         afScale[i] = 1;
   }

   // convert this to HLS
   float fH, fL, fS;
   ToHLS256 (UnGamma(pawColor[0]), UnGamma(pawColor[1]), UnGamma(pawColor[2]),
      &fH, &fL, &fS);

   // add variations
   if (!fS && !fH)
      fH = randf(0, 256);   // if no saturation, then random hue
   if (m_pColorVar.p[0])
      fH = myfmod(fH + randf(-128, 128)*m_pColorVar.p[0], 256);
   if (m_pColorVar.p[1]) {
      fL += randf(-128, 128) * m_pColorVar.p[1];
      fL = max(fL, 0);
      fL = min(fL, 255);
   }
   if (m_pColorVar.p[2]) {
      fS += randf(-128, 128) * m_pColorVar.p[2];
      fS = max(fS, 0);
      fS = min(fS, 255);
   }

   // apply quantization
   if (m_pColorPalette.p[0]) {
      fH = floor (fH * afScale[0] - m_fColorHueShift + 0.5);
      fH = (fH + m_fColorHueShift) / afScale[0];
      fH = myfmod (fH, 256);
   }

   // lightness
   if (m_pColorPalette.p[1]) {
      if (adwSplit[1] >= 2)
         fL = floor(fL * afScale[1] + 0.5) / afScale[1];
      else
         fL = 128;   // mid point
   }

   // saturations
   if (m_pColorPalette.p[2]) {
      if (adwSplit[2] >= 2)
         fS = floor(fS * afScale[2] + 0.5) / afScale[2];
      else
         fS = 256;   // full saturation
   }


   // convert back to HLS
   float afColor[3];
   FromHLS256 (fH, fL, fS, &afColor[0], &afColor[1], &afColor[2]);
   GammaFloat256 (afColor);
   for (i = 0; i < 3; i++) {
      afColor[i] = max(afColor[i], 0);
      afColor[i] = min(afColor[i], (float)0xffff);
      pawColor[i] = (WORD)afColor[i];
   }
}


// BUGBUG - when transfer effects, note that painting also references a texture


