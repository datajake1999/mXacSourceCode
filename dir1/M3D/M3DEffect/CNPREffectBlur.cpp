/*********************************************************************************
CNPREffectBlur.cpp - Code for effect

begun 14/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// PEMTEFFECTBLUR - Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImageSrc;
   PCImage        pImageDest;
   PCFImage       pFImageSrc;
   PCFImage       pFImageDest;
   PCBlurBuf      pbb;
} EMTEFFECTBLUR, *PEMTEFFECTBLUR;


/*********************************************************************************
CBlurBuf::Constructor and destructor
*/
CBlurBuf::CBlurBuf (void)
{
   m_dwWidth = m_dwHeight = 0;
   m_wScaleCenter = m_wScaleNonCenter = m_dwSumScaleNonCenter = 0;
}

CBlurBuf::~CBlurBuf (void)
{
   // nothing for now
}



/*********************************************************************************
CBlurBuf::Init - Initializes the blur buffer to a gaussian.

inputs
   float       fLenStandard - Standard deviation length, in pixels
   DWORD       dwSize - Number of standard deviations to include (min 1)
returns
   BOOL - TRUE if success
*/
BOOL CBlurBuf::Init (float fLenStandard, DWORD dwSize)
{
   DWORD dwCorner = (DWORD)(fLenStandard * (float) dwSize + 2);   // BUGFIX - Was just +1
   m_dwWidth = m_dwHeight = 1 + 2 * dwCorner;

   if (!InitToBlank (m_dwWidth, m_dwHeight))
      return FALSE;

   // fill in the corner..
   DWORD x, y;
   fLenStandard = max (fLenStandard, 0.0001);
   double fScale = -0.5 / (double)fLenStandard / (double)fLenStandard;
   for (y = 0; y <= dwCorner; y++) {
      float *paf = (float*)m_memFloat.p + (y * m_dwWidth);
      fp fYY = dwCorner - y;
      fYY *= fYY;

      for (x = 0; x <= dwCorner; x++, paf++) {
         fp fVal = dwCorner - x;
         paf[0] = exp((fVal * fVal + fYY) * fScale);
      } // x
   } // y


   // finally
   MirrorFromULCorner ();
   ZeroEdges();
   Normalize();
   FloatToWord();

   return TRUE;
}



/*********************************************************************************
CBlurBuf::InitToBlank - This initializes the blur object to blank float and word
values.

inputs
   DWORD       dwWidth, dwHeight - Size
returns
   BOOL - TRUE if successs
*/
BOOL CBlurBuf::InitToBlank (DWORD dwWidth, DWORD dwHeight)
{
   DWORD dwNeed = dwWidth * dwHeight * sizeof(float);
   if (!m_memFloat.Required (dwNeed))
      return FALSE;
   memset (m_memFloat.p, 0, dwNeed);

   dwNeed = dwWidth * dwHeight * sizeof(WORD);
   if (!m_memWORD.Required (dwNeed))
      return FALSE;
   memset (m_memWORD.p, 0, dwNeed);

   return TRUE;
}


/*********************************************************************************
CBlurBuf::MirrorFromULCorner - Mirrors the blur assuming that the UL corner
is valid. NOTE: This only mirrors the floating point one.
*/
void CBlurBuf::MirrorFromULCorner (void)
{
   DWORD x, y;
   DWORD dwWidthHalf = m_dwWidth/2;
   DWORD dwHeightHalf = m_dwHeight/2;
   DWORD dwHeightHalfExtra = (m_dwHeight+1)/2;

   for (y = 0; y < dwHeightHalfExtra; y++) {
      float *paf = (float*)m_memFloat.p + (y * m_dwWidth);
      float *paf2 = paf + (m_dwWidth - 1);

      for (x = 0; x < dwWidthHalf; x++, paf++, paf2--)
         paf2[0] = paf[0];
   } // y

   for (y = 0; y < dwHeightHalf; y++) {
      float *paf = (float*)m_memFloat.p + (y * m_dwWidth);
      float *paf2 = (float*)m_memFloat.p + ((m_dwHeight - y - 1) * m_dwWidth);

      memcpy (paf2, paf, m_dwWidth * sizeof(float));
   }
}


/*********************************************************************************
CBlurBuf::ZeroEdges - Looks through all the edges of the gaussian and finds
the maximum value. This is then subtracted from all the other values to ensure
that there are no hard edges.
*/
void CBlurBuf::ZeroEdges (void)
{
   float fMax = 0;
   DWORD x, y;

   // top and bottom
   float *paf;
   for (y = 0; y < m_dwHeight; y += m_dwHeight-1) {
      paf = (float*)m_memFloat.p + (y * m_dwWidth);
      for (x = 0; x < m_dwWidth; x++, paf++)
         fMax = max(fMax, paf[0]);
   } // y

   // left and right
   for (x = 0; x < m_dwWidth; x += m_dwWidth-1) {
      paf = (float*)m_memFloat.p + x;
      for (y = 0; y < m_dwHeight; y++, paf += m_dwWidth)
         fMax = max(fMax, paf[0]);
   } // x

   // subtract all
   for (y = 0; y < m_dwHeight; y++) {
      paf = (float*)m_memFloat.p + (y * m_dwWidth);
      for (x = 0; x < m_dwWidth; x++, paf++) {
         if (paf[0] <= fMax)
            paf[0] = 0;
         else
            paf[0] -= fMax;
      }
   } // y
}



/*********************************************************************************
CBlurBuf::Normalize - Normalizes the floating point values so they some to 1.0.
*/
void CBlurBuf::Normalize (void)
{
   double fSum = 0;
   DWORD x, y;

   // sum up
   float *paf;
   for (y = 0; y < m_dwHeight; y++) {
      paf = (float*)m_memFloat.p + (y * m_dwWidth);
      for (x = 0; x < m_dwWidth; x++, paf++)
         fSum += paf[0];
   } // y

   // scale
   if (!fSum)
      return;
   fSum = 1.0 / fSum;
   for (y = 0; y < m_dwHeight; y++) {
      paf = (float*)m_memFloat.p + (y * m_dwWidth);
      for (x = 0; x < m_dwWidth; x++, paf++)
         paf[0] = (float)(paf[0] * fSum);
   } // y
}


/*********************************************************************************
CBlurBuf::FloatToWord - Converts all the floating point values to a word
representation (so faster to apply to integer). This fills in m_memWORD and
m_wScaleNonCenter and m_wScaleCenter;
*/
void CBlurBuf::FloatToWord (void)
{
   // figure out the sum, excluding the central pixel
   DWORD dwCentX = m_dwWidth/2;
   DWORD dwCentY = m_dwHeight/2;
   float *paf = (float*)m_memFloat.p + (dwCentY * m_dwWidth + dwCentX);
   m_wScaleCenter = (WORD)(paf[0] * (float)0xffff);

   // find max for non-center
   paf = (float*)m_memFloat.p;
   DWORD x,y;
   float fMax = 0;
   for (y = 0; y < m_dwHeight; y++) for (x = 0; x < m_dwWidth; x++, paf++) {
      if ((y == dwCentY) && (x == dwCentY))
         continue;
      fMax = max(fMax, paf[0]);
   } //xy
   m_wScaleNonCenter = (WORD)(fMax * (float)0xffff);

   // figure out scaling so will sum to 0xffff
   double fScale = (double)0xffff / (fMax ? (double)fMax : 1);
   double f;
   WORD *paw;
   m_dwSumScaleNonCenter = 0;
   for (y = 0; y < m_dwHeight; y++) {
      paf = (float*)m_memFloat.p + (y * m_dwWidth);
      paw = (WORD*)m_memWORD.p + (y * m_dwWidth);
      for (x = 0; x < m_dwWidth; x++, paf++, paw++) {
         if (!paf[0]) {
            paw[0] = 0;
            continue;
         }

         f = paf[0] * fScale;
         f = min(f, (double)0xffff);
         paw[0] = (WORD)f;
         paw[0] = max(paw[0], 1);   // so at least have something

         if ((x != dwCentX) || (y != dwCentY))  // BUGFIX - Was &&
            m_dwSumScaleNonCenter += (DWORD) paw[0];
      } // x
   } // y
   m_dwSumScaleNonCenter /= (m_dwWidth * m_dwHeight);
   m_dwSumScaleNonCenter = max(m_dwSumScaleNonCenter, 1);
}


/*********************************************************************************
CBlurBuf::Blur - Applies the blur filter to the given image.

inputs
   PCFImage       pImage - Source image
   int            iX, iY - X and Y pixels to center on
   BOOL           fIgnoreBackground - Don't blur background pixels
   PTEXTUREPOINT  ptMinMax - If not null then don't blur points <= ptMinMax->h,
                  and don't blur points > ptMinMax->v
   float          *pafColor - Filled with RGB blur of color
returns
   none
*/
void CBlurBuf::Blur (PCFImage pImage, int iX, int iY,
                     BOOL fIgnoreBackground, PTEXTUREPOINT ptMinMax,
                     float *pafColor)
{
   int iXOrig = iX;
   int iYOrig = iY;

   // zero out
   double afColor[3];
   memset (afColor, 0, sizeof(afColor));

   DWORD dwImageWidth = pImage->Width();
   DWORD dwImageHeight = pImage->Height();
   BOOL f360 = pImage->m_f360;
   
   int x, y, xMax;
   iX -= (int)m_dwWidth/2;
   iY -= (int)m_dwHeight/2;
   PFIMAGEPIXEL pfpCenter = pImage->Pixel (iXOrig, iYOrig);
   BOOL fIsBackground = !HIWORD(pfpCenter->dwID);
   BOOL fIsBackPixel = ptMinMax ? (pfpCenter->fZ >= ptMinMax->v) : FALSE;
   BOOL fIsMidPixel = ptMinMax ? ((pfpCenter->fZ >= ptMinMax->h) && !fIsBackPixel) : FALSE;
   double fSumScale = 0;
   for (y = 0; y < (int)m_dwHeight; y++, iY++) {
      if (iY < 0)
         continue;
      if (iY >= (int)dwImageHeight)
         break;   // no more

      if (!f360 && (iX < 0))
         x = -iX;
      else
         x = 0;

      if (f360)
         xMax = (int)m_dwWidth;
      else
         xMax = min((int)m_dwWidth, (int)dwImageWidth - (int)iX);
      float *paf = (float*)m_memFloat.p + (y * m_dwWidth + (DWORD)x);
      PFIMAGEPIXEL pfp = pImage->Pixel ((DWORD)max(iX, 0), (DWORD)iY);
      PFIMAGEPIXEL pfpUse;

      for (; x < xMax; x++, paf++, pfp++) {
         if (!paf[0])
            continue;

         pfpUse = pfp;

         if (f360)
            pfpUse = pImage->Pixel((DWORD)((iX + x + (int)dwImageWidth) % (int)dwImageWidth), (DWORD)iY);
         
         // see if skip because of background
         if (fIsBackground && fIgnoreBackground) {
            if (!HIWORD(pfpUse->dwID))
               pfpUse = pfpCenter;  // then just use the center pixel color for this so no blurring of background
         }
         else if (fIsBackPixel && ((pfpUse->fZ >= ptMinMax->v) || (pfpUse->fZ < ptMinMax->h)))
            pfpUse = pfpCenter;  // beyond blur range
         else if (fIsMidPixel && (pfpUse->fZ < ptMinMax->h))
            pfpUse = pfpCenter;  // foreground doesn't affect

         fSumScale += (double)paf[0];
         afColor[0] += (double) paf[0] * (double) pfpUse->fRed;
         afColor[1] += (double) paf[0] * (double) pfpUse->fGreen;
         afColor[2] += (double) paf[0] * (double) pfpUse->fBlue;
      } // x
   } // y

   DWORD i;
   if (!fSumScale) {
      pafColor[0] = pfpCenter->fRed;
      pafColor[1] = pfpCenter->fGreen;
      pafColor[2] = pfpCenter->fBlue;
   }

   // done
   fSumScale = 1.0 / fSumScale;
   for (i = 0; i < 3; i++)
      pafColor[i] = (float) (afColor[i] * fSumScale);
}



/*********************************************************************************
CBlurBuf::Blur - Applies the blur filter to the given image.

inputs
   PCImage        pImage - Source image
   int            iX, iY - X and Y pixels to center on
   BOOL           fIgnoreBackground - Don't blur background pixels
   PTEXTUREPOINT  ptMinMax - If not null then don't blur points <= ptMinMax->h,
                  and don't blur points > ptMinMax->v
   WORD           wScaleSaturated - Glow
   WORD           *pawColor - Filled with RGB blur of color
returns
   none
*/
void CBlurBuf::Blur (PCImage pImage, int iX, int iY,
                     BOOL fIgnoreBackground, PTEXTUREPOINT ptMinMax,
                     WORD wScaleSaturated,
                     WORD *pawColor)
{
   int iXOrig = iX;
   int iYOrig = iY;

   // zero out
   __int64 aiColor[3];
   memset (aiColor, 0, sizeof(aiColor));

   DWORD dwImageWidth = pImage->Width();
   DWORD dwImageHeight = pImage->Height();
   DWORD dwHalfWidth = m_dwWidth/2;
   DWORD dwHalfHeight = m_dwHeight/2;
   BOOL f360 = pImage->m_f360;

   int x, y, xMax;
   DWORD i;
   iX -= (int)m_dwWidth/2;
   iY -= (int)m_dwHeight/2;
   PIMAGEPIXEL pipCenter = pImage->Pixel (iXOrig, iYOrig);
   BOOL fIsBackground = !HIWORD(pipCenter->dwID);
   BOOL fIsBackPixel = ptMinMax ? (pipCenter->fZ >= ptMinMax->v) : FALSE;
   BOOL fIsMidPixel = ptMinMax ? ((pipCenter->fZ >= ptMinMax->h) && !fIsBackPixel) : FALSE;
   DWORD dwSumScale = 0;
   for (y = 0; y < (int)m_dwHeight; y++, iY++) {
      if (iY < 0)
         continue;
      if (iY >= (int)dwImageHeight)
         break;   // no more

      if (!f360 && (iX < 0))
         x = -iX;
      else
         x = 0;

      if (f360)
         xMax = (int)m_dwWidth;
      else
         xMax = min((int)m_dwWidth, (int)dwImageWidth - (int)iX);

      WORD *paw = (WORD*)m_memWORD.p + (y * m_dwWidth + (DWORD)x);
      PIMAGEPIXEL pip = pImage->Pixel ((DWORD)max(iX, 0), (DWORD)iY);
      PIMAGEPIXEL pipUse;

      for (; x < xMax; x++, paw++, pip++) {
         if (!paw[0])
            continue;   // nothing

         if ((y == (int)dwHalfHeight) && (x == (int)dwHalfWidth))
            continue;   // dont do central point

         pipUse = pip;

         if (f360)
            pipUse = pImage->Pixel((DWORD)((iX + x + (int)dwImageWidth) % (int)dwImageWidth), (DWORD)iY);

         // see if skip because of background
         if (fIsBackground && fIgnoreBackground) {
            if (!HIWORD(pipUse->dwID))
               pipUse = pipCenter;  // then just use the center pixel color for this so no blurring of background
         }
         else if (fIsBackPixel && ((pipUse->fZ >= ptMinMax->v) || (pipUse->fZ < ptMinMax->h)))
            pipUse = pipCenter;  // beyond blur range
         else if (fIsMidPixel && (pipUse->fZ < ptMinMax->h))
            pipUse = pipCenter;  // foreground doesn't affect

         WORD *pawCur = &pipUse->wRed;
         for (i = 0; i < 3; i++) {
            if (wScaleSaturated) {
               __int64 iVal = (__int64)((DWORD)paw[0] * (DWORD)pawCur[i]);
               if (pawCur[i] >= 0xff00)
                  iVal = iVal * (__int64)wScaleSaturated / (__int64) 256;
               aiColor[i] += iVal;
            }
            else
               aiColor[i] += (__int64)((DWORD)paw[0] * (DWORD)pawCur[i]);
         }
         dwSumScale += (DWORD)paw[0];
      } // x
   } // y

   // if no sumscale then just include center at full scale
   dwSumScale /= (m_dwWidth * m_dwHeight);
   if (!dwSumScale) {
      pawColor[0] = pipCenter->wRed;
      pawColor[1] = pipCenter->wGreen;
      pawColor[2] = pipCenter->wBlue;
      return;
   }

   // done
   WORD *paw = (WORD*)m_memWORD.p + (dwHalfHeight * m_dwWidth + dwHalfWidth);
   WORD *pawCenter = &pipCenter->wRed;
   for (i = 0; i < 3; i++) {
      aiColor[i] = (aiColor[i] >> 16) * (__int64) m_wScaleNonCenter;
      aiColor[i] = aiColor[i] * (__int64)m_dwSumScaleNonCenter / dwSumScale;
      aiColor[i] += (__int64)((DWORD)pawCenter[i] * (DWORD)m_wScaleCenter);
      aiColor[i] >>= 16;
      if (aiColor[i] <= 0xffff)
         pawColor[i] = (WORD)aiColor[i];
      else
         pawColor[i] = 0xffff;
   } // i
}





// BLURPAGE - Page info
typedef struct {
   PCNPREffectBlur pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} BLURPAGE, *PBLURPAGE;



PWSTR gpszEffectBlur = L"Blur";



/*********************************************************************************
CNPREffectBlur::Constructor and destructor
*/
CNPREffectBlur::CNPREffectBlur (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fIgnoreBackground = TRUE;

   m_fBlurWidth = 1;
   m_dwBlurSize = 2;
   m_fScaleSaturated = 1;

   m_fBlurRange = FALSE;
   m_tpBlurMinMax.h = 10;
   m_tpBlurMinMax.v = 100 * 1000;
}

CNPREffectBlur::~CNPREffectBlur (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectBlur::Delete - From CNPREffect
*/
void CNPREffectBlur::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectBlur::QueryInfo - From CNPREffect
*/
void CNPREffectBlur::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = FALSE;
   pqi->pszDesc = L"Blurs an image.";
   pqi->pszName = L"Blur";
   pqi->pszID = gpszEffectBlur;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszBlurWidth = L"BlurWidth";
static PWSTR gpszScaleSaturated = L"ScaleSaturated";
static PWSTR gpszBlurSize = L"BlurSize";
static PWSTR gpszBlurRange = L"BlurRange";
static PWSTR gpszBlurMinMax = L"BlurMinMax";

/*********************************************************************************
CNPREffectBlur::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectBlur::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectBlur);

   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   MMLValueSet (pNode, gpszBlurWidth, m_fBlurWidth);
   MMLValueSet (pNode, gpszScaleSaturated, m_fScaleSaturated);
   MMLValueSet (pNode, gpszBlurSize, (int) m_dwBlurSize);

   MMLValueSet (pNode, gpszBlurRange, (int)m_fBlurRange);
   MMLValueSet (pNode, gpszBlurMinMax, &m_tpBlurMinMax);

   return pNode;
}


/*********************************************************************************
CNPREffectBlur::MMLFrom - From CNPREffect
*/
BOOL CNPREffectBlur::MMLFrom (PCMMLNode2 pNode)
{
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   m_fBlurWidth = MMLValueGetDouble (pNode, gpszBlurWidth, 1);
   m_fScaleSaturated = MMLValueGetDouble (pNode, gpszScaleSaturated, 1);
   m_dwBlurSize = (DWORD) MMLValueGetInt (pNode, gpszBlurSize, (int) 2);

   m_fBlurRange = (BOOL) MMLValueGetInt (pNode, gpszBlurRange, (int)FALSE);
   MMLValueGetTEXTUREPOINT (pNode, gpszBlurMinMax, &m_tpBlurMinMax);
   return TRUE;
}




/*********************************************************************************
CNPREffectBlur::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectBlur::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectBlur::CloneEffect - From CNPREffect
*/
CNPREffectBlur * CNPREffectBlur::CloneEffect (void)
{
   PCNPREffectBlur pNew = new CNPREffectBlur(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fIgnoreBackground = m_fIgnoreBackground;
   pNew->m_fBlurWidth = m_fBlurWidth;
   pNew->m_dwBlurSize = m_dwBlurSize;
   pNew->m_fScaleSaturated = m_fScaleSaturated;

   pNew->m_fBlurRange = m_fBlurRange;
   pNew->m_tpBlurMinMax = m_tpBlurMinMax;
   return pNew;
}




/*********************************************************************************
CNPREffectBlur::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectBlur::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTEFFECTBLUR pep = (PEMTEFFECTBLUR)pParams;

   PCImage pImageSrc = pep->pImageSrc;
   PCImage pImageDest = pep->pImageDest;
   PCFImage pFImageSrc = pep->pFImageSrc;
   PCFImage pFImageDest = pep->pFImageDest;
   PCBlurBuf pbb = pep->pbb;

   PIMAGEPIXEL pip = pImageDest ? pImageDest->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageDest ? NULL : pFImageDest->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();

   WORD wScaleSaturated = (pImageSrc && (m_fScaleSaturated > 1)) ?
      (WORD)(256.0 * m_fScaleSaturated) : 0;

   DWORD x, y;
   PTEXTUREPOINT pMinMax = m_fBlurRange ? &m_tpBlurMinMax : NULL;

   if (pip)
      pip += pep->dwStart * dwWidth;
   if (pfp)
      pfp += pep->dwStart * dwWidth;
   for (y = pep->dwStart; y < pep->dwEnd; y++) {
      for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
         if (pMinMax) {
            if (pip) {
               if (pip->fZ < pMinMax->h)
                  continue;   // not modified
            }
            else {
               if (pfp->fZ < pMinMax->h)
                  continue;   // not modified
            }
         }

         if (pip)
            pbb->Blur (pImageSrc, (int)x, (int)y, m_fIgnoreBackground, pMinMax,
               wScaleSaturated, &pip->wRed);
         else
            pbb->Blur (pFImageSrc, (int)x, (int)y, m_fIgnoreBackground, pMinMax,
               &pfp->fRed);
      } // x
   } // y
   
}

/*********************************************************************************
CNPREffectBlur::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImageSrc - Image. If NULL then use pFImage
   PCFImage       pFImageSrc - Floating point image. If NULL then use pImage
   PCImage        pImageDest - Image. If NULL then use pFImage
   PCFImage       pFImageDest - Floating point image. If NULL then use pImage
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectBlur::RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                                PCImage pImageDest, PCFImage pFImageDest,
                                PCProgressSocket pProgress)
{
   CBlurBuf bb;
   PIMAGEPIXEL pip = pImageDest ? pImageDest->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageDest ? NULL : pFImageDest->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();
   if (!bb.Init (m_fBlurWidth / 100.0 * (fp)dwWidth, m_dwBlurSize))
      return FALSE;

   // BUGFIX - Multithreaded
   EMTEFFECTBLUR em;
   memset (&em, 0, sizeof(em));
   em.pbb = &bb;
   em.pFImageDest = pFImageDest;
   em.pFImageSrc = pFImageSrc;
   em.pImageDest = pImageDest;
   em.pImageSrc = pImageSrc;
   ThreadLoop (0, dwHeight, 4, &em, sizeof(em), pProgress);


   return TRUE;
}


/*********************************************************************************
CNPREffectBlur::Render - From CNPREffect
*/
BOOL CNPREffectBlur::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pOrig, NULL, pDest, NULL, pProgress);
}



/*********************************************************************************
CNPREffectBlur::Render - From CNPREffect
*/
BOOL CNPREffectBlur::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pOrig, NULL, pDest, pProgress);
}




/****************************************************************************
EffectBlurPage
*/
BOOL EffectBlurPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PBLURPAGE pmp = (PBLURPAGE)pPage->m_pUserData;
   PCNPREffectBlur pv = pmp->pe;

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
         if (pControl = pPage->ControlFind (L"blurrange"))
            pControl->AttribSetBOOL (Checked(), pv->m_fBlurRange);

         DoubleToControl (pPage, L"blurwidth", pv->m_fBlurWidth);
         DoubleToControl (pPage, L"blursize", pv->m_dwBlurSize);
         DoubleToControl (pPage, L"scalesaturated", pv->m_fScaleSaturated);

         MeasureToString (pPage, L"blurzmin", pv->m_tpBlurMinMax.h);
         MeasureToString (pPage, L"blurzmax", pv->m_tpBlurMinMax.v);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

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
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"blurrange")) {
            pv->m_fBlurRange = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         };
      }
      break;   // default

   case ESCN_EDITCHANGE:
      {
         // just get all values
         pv->m_fBlurWidth = DoubleFromControl (pPage, L"blurwidth");
         pv->m_dwBlurSize = (DWORD) DoubleFromControl (pPage, L"blursize");
         pv->m_fScaleSaturated = DoubleFromControl (pPage, L"scalesaturated");
         pv->m_fScaleSaturated = max(pv->m_fScaleSaturated, 1);

         fp fTemp;
         MeasureParseString (pPage, L"blurzmin", &fTemp);
         pv->m_tpBlurMinMax.h = fTemp;
         MeasureParseString (pPage, L"blurzmax", &fTemp);
         pv->m_tpBlurMinMax.v = fTemp;

         pPage->Message (ESCM_USER+189);  // update bitmap
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

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Blur";
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
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*********************************************************************************
CNPREffectBlur::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectBlur::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectBlur::Dialog - From CNPREffect
*/
BOOL CNPREffectBlur::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   BLURPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTBLUR, EffectBlurPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}




