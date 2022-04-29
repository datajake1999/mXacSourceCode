/*****************************************************************************
CImageShader.cpp - Stores the image to be used for the advanced shader.

begun 26/5/02 by Mike Rozak.
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#define MISCBLOCKSIZE            256      // number of ISDMISC in a list entry
#define TRANSBLOCKSIZE           256      // number of ISPIXEL in a list entry

/************************************************************************
Constructor and destructor
*/
CImageShader::CImageShader (void)
{
   m_dwMaxTransparent = 4;

   m_dwWidth = m_dwHeight = 0;
   DWORD i;
   for (i = 0; i < MAXRAYTHREAD; i++) {
      m_adwMisc[i] = 1;  // always start at 1 since 0 is background
      m_adwTrans[i] = 1; // awlays start with 1 since 0 is none
   }

   m_fCritSecInitialized = FALSE;


}

CImageShader::~CImageShader (void)
{
   // free up critical sections
   DWORD i;
   if (m_fCritSecInitialized)
      for (i = 0; i < IMAGECRITSEC; i++)
         DeleteCriticalSection (&m_aCritSec[i]);
}



/*************************************************************************
CImageShader::CritSectionInitialize() - Call this to make sure the scanline
critical sections are initialized. This makes for safer rendering of
lines and filled polys on the image, ensuring that the threads don't
overwrite each others scanlines.

If this has already been called for this object, then it does nothning
*/
void CImageShader::CritSectionInitialize (void)
{
   if (m_fCritSecInitialized)
      return;

   DWORD i;
   for (i = 0; i < IMAGECRITSEC; i++)
      InitializeCriticalSection (&m_aCritSec[i]);

   m_fCritSecInitialized = TRUE;
}


/*************************************************************************
CImageShader::InitNoClear - Initializes the data structures for the image but does NOT
blank out the pixels or Z buffer. You can call this to reinitialze the size.

inputs
   DWORD       dwX, dwY - X Y dimensions
returns
   BOOL - TRUE if successful
*/
BOOL CImageShader::InitNoClear (DWORD dwX, DWORD dwY)
{
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   if (!m_memBuffer.Required(dwX * dwY * sizeof(ISPIXEL))) {
   	MALLOCOPT_RESTORE;
      return FALSE;
   }
	MALLOCOPT_RESTORE;
   DWORD i;
   for (i = 0; i < MAXRAYTHREAD; i++) {
   	MALLOCOPT_OKTOMALLOC;
      if (!m_amemZTemp[i].Required (dwX * sizeof(fp))) {
      	MALLOCOPT_RESTORE;
         return FALSE;
      }
   	MALLOCOPT_RESTORE;
   } // i
   m_dwWidth = dwX;
   m_dwHeight = dwY;
   for (i = 0; i < MAXRAYTHREAD; i++) {
      m_adwMisc[i] = 1;
      m_adwTrans[i] = 1;

      m_alMisc[i].Clear();
      m_alTrans[i].Clear();
   }
   return TRUE;
}

/*************************************************************************
CImageShader::Init - Initializes the data structures for the image. It also
CLEARS the image color to cColor and sets the ZBuffer to INFINITY.
You can call this to reinitialze the size.

inputs
   DWORD       dwX, dwY - X Y dimensions
   fp      fZ - Z height to use. If deeper than max depth then set to that
returns
   BOOL - TRUE if successful
*/
BOOL CImageShader::Init (DWORD dwX, DWORD dwY, fp fZ)
{
   if (!InitNoClear(dwX, dwY))
      return FALSE;

   return Clear (fZ);
}

/*************************************************************************
CImageShader::Clear - Clears the image color to cColor, and sets the ZBuffer to
infinity.

inputs
   fp      fZ - Z height to use. If deeper than max depth then set to that
returns
   BOOL - TRUE if successful
*/
BOOL CImageShader::Clear (fp fZ)
{
   // fill in prototype
   ISPIXEL ip;
   memset (&ip, 0, sizeof(ip));
   //fZ *= (fp)0x10000;
   if (fZ >= ZINFINITE)
      ip.fZ = ZINFINITE;
   else
      ip.fZ = (int) fZ;

   // do the first line
   DWORD    i;
   PISPIXEL pip;
   pip = Pixel (0, 0);
   for (i = 0; i < m_dwWidth; i++)
      pip[i] = ip;

   // all the other lines
   for (i = 1; i < m_dwHeight; i++)
      memcpy (Pixel(0,i), Pixel(0,0), sizeof(ISPIXEL) * m_dwWidth);

   // BUGFIX - might as well clear indecies
   for (i = 0; i < MAXRAYTHREAD; i++) {
      m_adwMisc[i] = 1;
      m_adwTrans[i] = 1;

      m_alMisc[i].Clear();
      m_alTrans[i].Clear();
   }

   return TRUE;
}

/*************************************************************************
CImageShader::Clone - Creates a new CImageShader object which is a clone of this one.
It even includes the same bitmap information and Z Buffer

inputs
   none
returns
   PCImageShader  - Image. NULL if cant create
*/
CImageShader *CImageShader::Clone (void)
{
   PCImageShader  p = new CImageShader;
   if (!p)
      return FALSE;

   if (!p->InitNoClear (m_dwWidth, m_dwHeight)) {
      delete p;
      return NULL;
   }

   p->m_dwMaxTransparent = m_dwMaxTransparent;

   // copy over the bitmap
   memcpy (p->Pixel(0,0), Pixel(0,0), m_dwWidth * m_dwHeight * sizeof(ISPIXEL));
   memcpy (p->m_adwMisc, m_adwMisc, sizeof(m_adwMisc));
   memcpy (p->m_adwTrans, m_adwTrans, sizeof(m_adwTrans));

   DWORD i, j, k;
   size_t dwSize;
   for (i = 0; i < MAXRAYTHREAD; i++) {
      // NOTE: No tested
      p->m_alMisc[i].Clear();
      p->m_alTrans[i].Clear();

      // copy over misc
      for (j = 0; j < m_alMisc[i].Num(); j++) {
         dwSize = m_alMisc[i].Size(j);
         p->m_alMisc[i].Add (m_alMisc[i].Get(j), dwSize);
      } // j

      // copy over trans
      for (j = 0; j < m_alTrans[i].Num(); j++) {
         dwSize = m_alTrans[i].Size(j);
         p->m_alTrans[i].Add (m_alTrans[i].Get(j), dwSize);
      } // j
   } // i

   // remap pixels
   PISPIXEL pip = p->Pixel(0,0);
   PISPIXEL pCur;
   PBYTE pb;
   for (i = 0; i < m_dwWidth * m_dwHeight; i++, pip++) {
      for (pCur = pip; pCur; pCur = (PISPIXEL)pCur->pTrans) {  // BUGFIX - Was pip->pTrans
         // remap misc
         if (pCur->pMisc) {
            for (j = 0; j < MAXRAYTHREAD; j++) {
               for (k = 0; k < m_alMisc[j].Num(); k++) {
                  dwSize = m_alMisc[j].Size(k);
                  pb = (PBYTE) m_alMisc[j].Get(k);

                  if (((PBYTE)pCur->pMisc >= pb) && ((PBYTE)pCur->pMisc < pb + dwSize))
                     break;
               } // k
               if (k < m_alMisc[j].Num())
                  break;
            } // j
            if (j < MAXRAYTHREAD) // found conversion
               pCur->pMisc = (PISDMISC) (((PBYTE)pCur->pMisc - pb) + (PBYTE)p->m_alMisc[j].Get(k));
            else  // no conversion
               pCur->pMisc = NULL;

         } // if misc

         // same for transparency
         if (pCur->pTrans) {
            for (j = 0; j < MAXRAYTHREAD; j++) {
               for (k = 0; k < m_alTrans[j].Num(); k++) {
                  dwSize = m_alTrans[j].Size(k);
                  pb = (PBYTE) m_alTrans[j].Get(k);

                  if (((PBYTE)pCur->pTrans >= pb) && ((PBYTE)pCur->pTrans < pb + dwSize))
                     break;
               } // k
               if (k < m_alTrans[j].Num())
                  break;
            } // j
            if (j < MAXRAYTHREAD) // found conversion
               pCur->pTrans = (PISPIXEL) (((PBYTE)pCur->pTrans - pb) + (PBYTE)p->m_alTrans[j].Get(k));
            else  // no conversion
               pCur->pTrans = NULL;

         } // if Trans
      } // pCur
   } // i

   return p;
}


/****************************************************************************
CImageShader::MiscAdd - Add a new entry to misc. If the value immediately
before it is the same then just uses that.

inputs
   PISDMISC    pMisc - Info
returns
   PISDMISC - New location. 0 if error
*/
__inline PISDMISC CImageShader::MiscAdd (const PISDMISC pMisc, DWORD dwThread)
{
   DWORD dwCur = m_adwMisc[dwThread];
   DWORD dwIndex = dwCur / MISCBLOCKSIZE;
   DWORD dwOffset = dwCur % MISCBLOCKSIZE;
   DWORD dwIndexPrev = (dwCur-1) / MISCBLOCKSIZE;
   DWORD dwOffsetPrev = (dwCur-1) % MISCBLOCKSIZE;

   // add blank list entries until make it
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_alMisc[dwThread].Required (dwIndex+1);
   while (m_alMisc[dwThread].Num() <= dwIndex)
      m_alMisc[dwThread].Add (NULL, MISCBLOCKSIZE * sizeof(ISDMISC));
	MALLOCOPT_RESTORE;

   // get it
   PISDMISC pmNew = (PISDMISC) m_alMisc[dwThread].Get(dwIndex);
   if (!pmNew)
      return NULL;
   pmNew += dwOffset;

   // see if it's the same as the previous
   if (dwCur >= 2) {
      PISDMISC pmPrev = (PISDMISC) m_alMisc[dwThread].Get(dwIndexPrev) + dwOffsetPrev;
      if (!memcmp (pmPrev, pMisc, sizeof(*pMisc)))
         return pmPrev;
   }


   // else add it
   m_adwMisc[dwThread]++;
   memcpy (pmNew, pMisc, sizeof(*pMisc));

   return pmNew;
}


/****************************************************************************
CImageShader::DrawPixel - Draws a pixel on the screen, witout texture.

inputs
   DWORD    dwX, dwY - XY location. ASSUMES that the points are within the drawing area
   fp       fZ - Depth in meters. Positive value
   PISDMISC  pm - Use the object ID information from here, not transparency or color
returns
   none
*/
__inline void CImageShader::DrawPixel (DWORD dwX, DWORD dwY, fp fZ, const PISDMISC pm, DWORD dwThread)
{
   PISPIXEL pip = Pixel(dwX, dwY);

   // zbuffer
   if (fZ >= pip->fZ)
      return;

   PVOID pTrans;
   pTrans = pip->pTrans;
   memset (pip, 0, sizeof(*pip));
   pip->fZ = fZ;
   pip->pMisc = MiscAdd (pm, dwThread);
   pip->pTrans = pTrans;
}


/****************************************************************************
CImageShader::DrawOpaquePixel - Fills in the opaque pixel, but also clears
transparencies that shouldnt be there.

inputs
   PISPIXEL    pip - Pixel that drawing onto.
   PISPIXEL    pTrans - Transparent pixel to add
returns
   BOOL - TRUE if added
*/
__inline void CImageShader::DrawOpaquePixel (PISPIXEL pip, const PISPIXEL pTrans, DWORD dwThread)
{
   PISPIXEL pTransCur = (PISPIXEL) pip->pTrans;
   *pip = *pTrans;

   // keep deleting opaque
   PISPIXEL pipt;
   while (pTransCur) {
      pipt = pTransCur;
      if (pipt->fZ < pip->fZ)
         break;   // transparent pixel is infront

      // else, remove this because new opaque pixel is in front
      pTransCur = (PISPIXEL) pipt->pTrans;
   }

   // write out
   pip->pTrans = pTransCur;
}

/****************************************************************************
CImageShader::DrawTransparentPixel - Adds a transparent pixel to the given XY location.
This does tests to make sure it will be visible, and eliminates another transparent
pixel if there are already too many.

inputs
   PISPIXEL    pip - Pixel that drawing onto.
   PISPIXEL    pTrans - Transparent pixel to add
   PCTextureMapSocket pTexture - Texture to use.
   PCMaterial  pMaterial - Base material
   PTEXTPOINT5 pPixelSize - How much the texture covers in a pixel
returns
   BOOL - TRUE if added
*/
__inline void CImageShader::DrawTransparentPixel (PISPIXEL pip, const PISPIXEL pTrans,
                                         const PCTextureMapSocket pTexture, const PCMaterial pMaterial,
                                         const PTEXTPOINT5 pPixelSize, DWORD dwThread)
{
	MALLOCOPT_INIT;
   if (pTrans->fZ >= pip->fZ)
      return;

   // BUGFIX - if can see complete through then dont add transparency.
   // this way, when drawing grass/leaves, will be better
   // WORD awBase[3];
   CMaterial Mat;
   //TEXTPOINT5 tp;
   float afGlow[3];
   BOOL fGlow;
   // memcpy (&Mat, pMaterial, sizeof(Mat));
   Mat.m_wTransparency = pMaterial->m_wTransparency;
   Mat.m_wSpecReflect = pMaterial->m_wSpecReflect;
   //memset (&tp, 0, sizeof(tp));  // NOTE: No doing antialiasing, see how well works
   // BUGFIX - In case no texture
   if (pTexture) {
      // NOTE: When added pPixelSize in, get problem that edges of leaves are very very blurred,
      // so have some thickness, and get drawn
      pTexture->FillPixel (dwThread, TMFP_TRANSPARENCY | TMFP_SPECULARITY, NULL, &pTrans->tpText, pPixelSize, &Mat, afGlow, FALSE);
         // NOTE: Using fHighQuality = FALSE for transparency test
      fGlow = afGlow[0] || afGlow[1] || afGlow[2];
   }
   else
      fGlow = FALSE;

   // see if can skip
   // BUGIFX - Was just testing !Mat.m_wSpectReflect
   if ((Mat.m_wTransparency >= 0xff00) && (Mat.m_wSpecReflect < 0x100) && !fGlow)
      return;

   // if this isn't transparent at all then just draw as opaque
   if (Mat.m_wTransparency <= 0x00ff) {
      DrawOpaquePixel (pip, pTrans, dwThread);
      return;
   }

   // see how many transparent pixels already exist and the max depth
   // NOTE: Changed code so the deepest (highest Z) is first in list, followed by
   // lesser Z, in sorted order
   DWORD dwNum;
   PISPIXEL pCur, pInsertBeforeIndex;
   DWORD dwInsertBefore;
   PISPIXEL pipt, piptLast, piptInsertBefore, piptInsertAfter;
   dwNum = 0;
   piptInsertBefore = NULL;
   for (piptLast = NULL, pCur = (PISPIXEL)pip->pTrans; pCur; pCur = (PISPIXEL)pipt->pTrans) {
      pipt = pCur;
      if (!piptInsertBefore && (pTrans->fZ > pipt->fZ)) {
         piptInsertBefore = pipt;
         piptInsertAfter = piptLast;
         dwInsertBefore = dwNum;
         pInsertBeforeIndex = pCur;
      }
      dwNum++;
      piptLast = pipt;
   }
   if (!piptInsertBefore) {
      dwInsertBefore = dwNum;
      pInsertBeforeIndex = 0;
      piptInsertAfter = piptLast;
   }

   // if there are too many then eliminate, starting at the deepest
   PISPIXEL pInsertedTrans;
   if (dwNum >= m_dwMaxTransparent) {
      // this would have been the deepest
      if (!dwInsertBefore)
         return;

      // else, remove the deepest, and use its pointer for our current buffer
      pipt = (PISPIXEL) pip->pTrans;
      pInsertedTrans = (PISPIXEL) pip->pTrans;
      pip->pTrans = pipt->pTrans;
      dwNum--;
      dwInsertBefore--;
      if (!dwInsertBefore)
         piptInsertAfter = NULL; // since just eliminated first one
   }
   else {
      // allocate new slot
      DWORD dwCur = m_adwTrans[dwThread];
      DWORD dwIndex = dwCur / MISCBLOCKSIZE;
      DWORD dwOffset = dwCur % MISCBLOCKSIZE;

      // add blank list entries until make it
	   MALLOCOPT_OKTOMALLOC;
      while (m_alTrans[dwThread].Num() <= dwIndex)
         m_alTrans[dwThread].Add (NULL, MISCBLOCKSIZE * sizeof(ISPIXEL));
	   MALLOCOPT_RESTORE;

      // get it
      pipt = (PISPIXEL) m_alTrans[dwThread].Get(dwIndex);
      if (!pipt)
         return;
      pipt += dwOffset;

      pInsertedTrans = pipt;
      m_adwTrans[dwThread]++;
   }

   // have pipt and dwInsertedTrans, as well as piptInsertAfter, and dwInsertBefore
   *pipt = *pTrans;
   pipt->pTrans = piptInsertBefore ? pInsertBeforeIndex : 0;
   if (dwInsertBefore)  // then piptInsertAfter is valid
      piptInsertAfter->pTrans = pInsertedTrans;
   else
      pip->pTrans = pInsertedTrans;   // just after current pixel
}



/********************************************************************
DrawLine - Draws a line on the screen.

inputs
   PHILEND     pStart, pEnd - Start and end points. NOTE: While this has
                  safety clipping, it's not terribly fast for long lines off
                  the screen area.
   PISDMISC     pm - Miscellaneous info. Only uses the object ID stuff.
*/
void CImageShader::DrawLine (PISHLEND pStart, PISHLEND pEnd,
                             PISDMISC pm, DWORD dwThread)
{
   // for citical seciton
   DWORD dwCritSec = (DWORD)-1;  // not using any right now

   // NOTE - Ignoring the /w effect on drawing.
   fp   fDeltaX, fDeltaY;
   fp   x1, x2, y1, y2;
doagain:
   x1 = pStart->x;
   x2 = pEnd->x;
   y1 = pStart->y;
   y2 = pEnd->y;

   fDeltaX = x2 - x1;
   fDeltaY = y2 - y1;

   if (!fDeltaX && !fDeltaY) {
      // just a point
      if ((x1 >= 0) && ((DWORD) x1 < m_dwWidth) && (y1 >= 0) && ((DWORD)y1 < m_dwHeight)) {
         // potentially enter a critical seciton
         if (m_fCritSecInitialized && (y1 >= 0)) {
            DWORD dwCritSecWant = ((DWORD)y1 / IMAGESCANLINESPERCRITSEC) % IMAGECRITSEC;
            if (dwCritSecWant != dwCritSec) {
               if (dwCritSec != (DWORD)-1)
                  LeaveCriticalSection (&m_aCritSec[dwCritSec]);
               dwCritSec = dwCritSecWant;
               EnterCriticalSection (&m_aCritSec[dwCritSec]);
            }
         }

         DrawPixel ((DWORD)x1, (DWORD)y1, pStart->z, pm, dwThread);
      }
      goto done;
   }

   fp   fAbsX, fAbsY;
   fAbsX = max(fDeltaX,-fDeltaX);
   fAbsY = max(fDeltaY, -fDeltaY);

   // determine if it's mostly up/down
   BOOL  fRightLeft;
   int   i;
   fp   fY, fX, fDelta, fZ, fDeltaZ;
   fRightLeft = (fAbsX > fAbsY);

   // rearrange this data so if we're mostly up/down draw from top to bottom
   // and if we're mostly right/left draw from left to right
   if ( (fRightLeft && (fDeltaX < 0)) || (!fRightLeft && (fDeltaY < 0)) ) {
      PISHLEND t;
      t = pStart;
      pStart = pEnd;
      pEnd = t;
      goto doagain;
   }

#ifdef _TIMERS
   gRenderStats.dwNumDiagLine++;
#endif

   fDeltaZ = (pEnd->z - pStart->z) / (fRightLeft ? fAbsX : fAbsY);
   fZ = pStart->z;

   // if it's mostly up/down do one algorith, else another
   if (fRightLeft) {
      // either moving right/left
      fY = y1;
      fDelta = fDeltaY / fDeltaX;

      // NOTE: fDeltaX is always more than 0, so can move in right direction

      int   ix1, ix2;
      ix1 = (int) x1;
      ix2 = (int) x2;
      for (i = ix1; i < ix2; i++, fY += fDelta, fZ += fDeltaZ) {

         if ( (i < 0) || (i >= (int) m_dwWidth) || (fY < 0) || (fY >= m_dwHeight))
            continue;

         // potentially enter a critical seciton
         if (m_fCritSecInitialized && (fY >= 0)) {
            DWORD dwCritSecWant = ((DWORD)fY / IMAGESCANLINESPERCRITSEC) % IMAGECRITSEC;
            if (dwCritSecWant != dwCritSec) {
               if (dwCritSec != (DWORD)-1)
                  LeaveCriticalSection (&m_aCritSec[dwCritSec]);
               dwCritSec = dwCritSecWant;
               EnterCriticalSection (&m_aCritSec[dwCritSec]);
            }
         }

         DrawPixel ((DWORD) i, (DWORD) fY, (int) fZ, pm, dwThread);
      }

   }
   else {
      // moving up/down
      fX = x1;
      fDelta = fDeltaX / fDeltaY;

      // NOTE: fDeltaY is always more than 0, so can move in down direction

      int   iy1, iy2;
      iy1 = (int) y1;
      iy2 = (int) y2;

      // move to the right
      for (i = iy1; i < iy2; i++, fX += fDelta, fZ += fDeltaZ) {

         if ( (i < 0) || (i >= (int) m_dwHeight) || (fX < 0) || (fX >= m_dwWidth))
            continue;

         // potentially enter a critical seciton
         if (m_fCritSecInitialized && (i >= 0)) {
            DWORD dwCritSecWant = ((DWORD)i / IMAGESCANLINESPERCRITSEC) % IMAGECRITSEC;
            if (dwCritSecWant != dwCritSec) {
               if (dwCritSec != (DWORD)-1)
                  LeaveCriticalSection (&m_aCritSec[dwCritSec]);
               dwCritSec = dwCritSecWant;
               EnterCriticalSection (&m_aCritSec[dwCritSec]);
            }
         }
         DrawPixel ((DWORD) fX, (DWORD) i, (int) fZ, pm, dwThread);
      }
   }

done:
   // free up the critical seciton
   if (dwCritSec != (DWORD)-1)
      LeaveCriticalSection (&m_aCritSec[dwCritSec]);


}

/**************************************************************************
CImageShader::DrawHorzLine - Draws a horontal line that has a color blend or solid.

inputs
   int      y - y scan line
   PISHLEND  pLeft, pRight - Left and right edges. Notes:
               pLeft must be left, and pRight must be right
               NOTE: the normals DONT have to be normalized
   PTEXTPOINT5 ptDeltaLeft, ptDeltaRight - Change in the texture over the pixel as
                  it goes from y to y+1.
   PISDMISC  pm - Miscellaneous info. Ignores pm.wColor if !fSolid
   BOOL     fSolid - Set to true if should fill with a solid color, FALSE if color blend
   PCTextureMap pTexture - If not NULL then use the texture map
   PTEXTPOINT5 ptPixelSize - Size of pixel (vertically) in absolute hv, xyz
   DWORD    dwTextInfo - Texture info... one or more of the DHLTEXT_XXX flags
returns
   none
*/
#define  REJECT      (-ZINFINITE)
void CImageShader::DrawHorzLine (int y, const PISHLEND pLeft, const PISHLEND pRight,
                                 // no longer using PTEXTUREPOINT ptDeltaStart, PTEXTUREPOINT ptDeltaEnd,
                                 const PISDMISC pm, const PTEXTPOINT5 ptPixelSize,
                                 DWORD dwTextInfo, DWORD dwThread)
{
	MALLOCOPT_INIT;
   // check y
   if ((y < 0) || (y >= (int)m_dwHeight))
      return;

#ifdef _TIMERS
   gRenderStats.dwNumHLine++;
#endif
   DWORD dwNewID;
   dwNewID = pm->dwIDPart;

   // find out where it starts on the screen
   int   xStart, xEnd;
   fp fFloor;
   xStart = (int) ceil(pLeft->x);
   fFloor = floor(pRight->x);
   // if the right ends exactly on an integer, don't count it because count the
   // left if it's exactly on an integer. Counting both causes problems
   xEnd = (int) ((fFloor == pRight->x) ? (fFloor-1) : fFloor);
   //xEnd = (int) floor(pRight->x);
   xStart = max(0,xStart);
   xEnd = min((int)m_dwWidth-1,xEnd);

   // figure out z delta
   fp   xDelta = pRight->x - pLeft->x;
   if ((xEnd < xStart) || (xDelta <= 0))
      return;  // cant do this
   fp fSkip;
   fSkip = xStart - pLeft->x;


#ifdef _TIMERS
   gRenderStats.dwPixelRequest += (xEnd - xStart + 1);
#endif
   // simple trivial reject of the entire line by seeing if none
   // of the line couldnt possibly be in front
   DWORD dw, dwPixels, dwCount;
   PISPIXEL pip, pipOrig;
   fp fMinZ;
   pip = pipOrig = Pixel(xStart, y);
   dwPixels = (DWORD) (xEnd - xStart + 1);
   dwCount = 0;
   fMinZ = min(pLeft->z, pRight->z);
   for (dw = dwPixels; dw; dw--, pip++)
      if (pip->fZ <= fMinZ)
         dwCount++;
   if (dwCount == dwPixels) {
#ifdef _TIMERS
      gRenderStats.dwPixelTrivial += dwCount;
#endif
      return;  // trivial reject
   }


   // figure out alpha
   m_amemLineDivide[dwThread].m_dwCurPosn = 0;
   DWORD dwNum;
	MALLOCOPT_OKTOMALLOC;
   dwNum = GenerateAlphaInPerspective (pLeft->x, pRight->x, pLeft->w, pRight->w,
      (int) xStart, (int) xEnd - (int) xStart + 1, &m_amemLineDivide[dwThread], TRUE);
	MALLOCOPT_RESTORE;
   if (!dwNum)
      return;
   fp *paf;
   paf = (fp*) m_amemLineDivide[dwThread].p;

   // more complex calculation of Z buffer that takes into account
   // the W-interpolation
   fp zDelta;
   fp   *pf, *pfOrig;
   fp fStartZ;
   fp fX;
   pf = pfOrig = (fp*) m_amemZTemp[dwThread].p;
   pip = pipOrig = Pixel(xStart, y);
   dwPixels = (DWORD) (xEnd - xStart + 1);
   dwCount = 0;
   fp *pafCur;
   zDelta = pRight->z - pLeft->z;
   for (pafCur = paf,dw = dwPixels, fX = xStart; dw; dw--, pip++, pf++, pafCur++) {

      fStartZ = (pLeft->z + zDelta * pafCur[0]);

      // compare the z buffer
      if (pip->fZ >= fStartZ) {
         // the horizontal line is definitely in front
         *pf = fStartZ;
         dwCount++;
      }
      else {
         // the horizontal line is behind
         *pf = REJECT;
#ifdef _TIMERS
         gRenderStats.dwPixelBehind++;
#endif
         // dont change dwcount
      }
   }

   // if !dwCount then trivial reject
   if (!dwCount) {
      return;
   }

   // if dwCount == dwPixels and not transparent then trivial accept
   //BOOL fTrivialAccept;
   //fTrivialAccept = ((dwCount == dwPixels) && !pm->Material.m_wTransparency);

   // color blend
   ISPIXEL ip;
   memset (&ip, 0, sizeof(ip));
   ip.pMisc = MiscAdd (pm, dwThread);
   
   // what's the total span
   fp fTotal;
   fTotal = pRight->x - pLeft->x;

   // keep a bool to remember if there's a texture since if not don't
   // need to interpolate some stuff
   BOOL fTexture, fInterpNorm;
   fTexture = (pm->pTexture ? TRUE : FALSE);
   // TEXTUREPOINT tpLeftDelta, tpRightDelta;
   TEXTPOINT5 tpLeftX, tpDeltaX;
   BOOL fTransparent;
   fTransparent = (pm->Material.m_wTransparency ? TRUE : FALSE);
   DWORD j;
   if (fTexture) {
      // BUGFIX - not sure why was doing the later code, but simplify
      tpLeftX = pLeft->tp;

      if (dwTextInfo & DHLTEXT_HV)
         for (j = 0; j < 2; j++)
            tpDeltaX.hv[j] = pRight->tp.hv[j] - tpLeftX.hv[j];
      if (dwTextInfo & DHLTEXT_XYZ)
         for (j = 0; j < 3; j++)
            tpDeltaX.xyz[j] = pRight->tp.xyz[j] - tpLeftX.xyz[j];

#if 0 // old code
      fp fAlpha;
      fAlpha = 0; // BUGFIX : Handled by GenerateAlphaInPerspective - fSkip / fTotal;
      // tpLeftDelta.h = (1.0 - fAlpha) * ptDeltaStart->h + fAlpha * ptDeltaEnd->h;
      // tpLeftDelta.v = (1.0 - fAlpha) * ptDeltaStart->v + fAlpha * ptDeltaEnd->v;
      for (j = 0; j < 2; j++)
         tpLeftX.hv[j] = (1.0 - fAlpha) * pLeft->tp.hv[j] + fAlpha * pRight->tp.hv[j];
      for (j = 0; j < 3; j++)
         tpLeftX.xyz[j] = (1.0 - fAlpha) * pLeft->tp.xyz[j] + fAlpha * pRight->tp.xyz[j];

      fAlpha = 1; // BUGFIX: handled by GenerateAlphaInPerspective - (fSkip + (fp)dwPixels-1) / fTotal;
      // tpRightDelta.h = (1.0 - fAlpha) * ptDeltaStart->h + fAlpha * ptDeltaEnd->h;
      // tpRightDelta.v = (1.0 - fAlpha) * ptDeltaStart->v + fAlpha * ptDeltaEnd->v;
      for (j = 0; j < 2; j++)
         tpRightX.hv[j] = (1.0 - fAlpha) * pLeft->tp.hv[j] + fAlpha * pRight->tp.hv[j];
      for (j = 0; j < 3; j++)
         tpRightX.xyz[j] = (1.0 - fAlpha) * pLeft->tp.xyz[j] + fAlpha * pRight->tp.xyz[j];
#endif // 0

      // BUGFIX - Was testing for any MightBeTransparentFlat(), but dont need to
      if ((dwTextInfo & DHLTEXT_PERPIXTRANS))
         fTransparent = TRUE;
   }

   // are normals interpolated
   CPoint pNLeft, pNRight;
   pNLeft.Copy (&pLeft->pNorm);
   // BUGFIX - Dont normalize here pNLeft.Normalize();
   pNRight.Copy (&pRight->pNorm);
   // BUGFIX - Dont normalize here pNRight.Normalize();
   fInterpNorm = !(pNLeft.AreClose (&pNRight));
   // BUGFIX - Remove this since GenerateAlphaInPerspective automatically does
   //if (fInterpNorm) {
   //   CPoint pNewLeft, pNewRight;
   //   pNewLeft.Average (&pNLeft, &pNRight, 1.0 - fSkip / fTotal);
   //   pNewRight.Average (&pNLeft, &pNRight, 1.0 - (fSkip + (fp) dwPixels-1) / fTotal);
   //   pNLeft.Copy (&pNewLeft);
   //   pNRight.Copy (&pNewRight);
   //   pNLeft.Normalize();
   //   pNRight.Normalize();
   //}

   // if it's trivial accept it's even easier
   pip = pipOrig;
   pf = pfOrig;

   // precalc a flat norm
   short aiFlatNorm[3];
   short aiNormStart[3];
   CPoint pNormDelta;
   if (fInterpNorm) {
      aiNormStart[0] = (short) (pNLeft.p[0] * (fp)0x4000);
      aiNormStart[1] = (short) (pNLeft.p[1] * (fp)0x4000);
      aiNormStart[2] = (short) (pNLeft.p[2] * (fp)0x4000);
      pNormDelta.Subtract (&pNRight, &pNLeft);
      pNormDelta.Scale ((fp)0x4000);
   }
   else {
      aiFlatNorm[0] = (short) (pNLeft.p[0] * (fp)0x4000);
      aiFlatNorm[1] = (short) (pNLeft.p[1] * (fp)0x4000);
      aiFlatNorm[2] = (short) (pNLeft.p[2] * (fp)0x4000);
   }

   //fp fPrev, fNext;
   pafCur = paf;
   TEXTPOINT5 tpLast, tpDelta;
   for (dw = dwPixels; dw; dw--, pip++, pf++, pafCur++) {
      // deal with z
      if (*pf == (fp)REJECT) {
         continue;
      }
      ip.fZ = *pf;

      // interpolate normals
      if (fInterpNorm) {
         // fp fLen;
         // BUGFIX - Optimizations
         // BUGFIX - Dont normalize here
         // pNorm.Average (&pNRight, &pNLeft, *pafCur);
         //fLen = pNorm.Length();
         //pNorm.Scale (32767.0 / (fLen ? fLen : CLOSE));
         //ip.aiNorm[0] = (short) pNorm.p[0];
         //ip.aiNorm[1] = (short) pNorm.p[1];
         //ip.aiNorm[2] = (short) pNorm.p[2];

         ip.aiNorm[0] = aiNormStart[0] + (short)(pNormDelta.p[0] * (*pafCur));
         ip.aiNorm[1] = aiNormStart[1] + (short)(pNormDelta.p[1] * (*pafCur));
         ip.aiNorm[2] = aiNormStart[2] + (short)(pNormDelta.p[2] * (*pafCur));
      }
      else {
         memcpy (ip.aiNorm, aiFlatNorm, sizeof(aiFlatNorm));
         //ip.aiNorm[0] = (short) (pNLeft.p[0] * 32767.0);
         //ip.aiNorm[1] = (short) (pNLeft.p[1] * 32767.0);
         //ip.aiNorm[2] = (short) (pNLeft.p[2] * 32767.0);
      }

      if (fTexture) {
         fp fAlpha = *pafCur;

         // interpolate the texture
         if (dwTextInfo & DHLTEXT_HV)
            for (j = 0; j < 2; j++)
               ip.tpText.hv[j] = tpLeftX.hv[j] + fAlpha * tpDeltaX.hv[j];
         if (dwTextInfo & DHLTEXT_XYZ)
            for (j = 0; j < 3; j++)
               ip.tpText.xyz[j] = tpLeftX.xyz[j] + fAlpha * tpDeltaX.xyz[j];
         //for (j = 0; j < 2; j++)
         //   ip.tpText.hv[j] = fAlphaOneMinus * tpLeftX.hv[j] + fAlpha * tpRightX.hv[j];
         //for (j = 0; j < 3; j++)
         //   ip.tpText.xyz[j] = fAlphaOneMinus * tpLeftX.xyz[j] + fAlpha * tpRightX.xyz[j];

         // if this is the first pixel, then try to calculate the next location, and
         // pretend it's the last. If no adjacent pixels then assume 0
         if ((dwTextInfo & DHLTEXT_PERPIXTRANS) && (dw == dwPixels)) {
            if (dw >= 2) {
               fAlpha = pafCur[1];
               if (dwTextInfo & DHLTEXT_HV)
                  for (j = 0; j < 2; j++)
                     tpLast.hv[j] = tpLeftX.hv[j] + fAlpha * tpDeltaX.hv[j];
               if (dwTextInfo & DHLTEXT_XYZ)
                  for (j = 0; j < 3; j++)
                     tpLast.xyz[j] = tpLeftX.xyz[j] + fAlpha * tpDeltaX.xyz[j];
            }
            else
               tpLast = ip.tpText;
         }
#if 0 // no longer using atpDelta
         // vertical change in texture
         ip.atpDelta[1].h = (1.0 - fAlpha) * tpLeftDelta.h + fAlpha * tpRightDelta.h;
         ip.atpDelta[1].v = (1.0 - fAlpha) * tpLeftDelta.v + fAlpha * tpRightDelta.v;

         // horizontal change
         if ((dw != dwPixels) && dwNum) {
            // see difference between this point and next
            fPrev = pafCur[-1];
            fNext = pafCur[1];
            ip.atpDelta[0].h = (((1.0 - fNext) * tpLeftX.h + fNext * tpRightX.h) -
               ((1.0 - fPrev) * tpLeftX.h + fPrev * tpRightX.h)) / 2;
            ip.atpDelta[0].v = ((1.0 - fNext) * tpLeftX.v + fNext * tpRightX.v -
               ((1.0 - fPrev) * tpLeftX.v + fPrev * tpRightX.v)) / 2;
         }
         else if (dwNum) {
            // still have another point to come, since take distance between that one
            fAlpha = pafCur[1];
            ip.atpDelta[0].h = (1.0 - fAlpha) * tpLeftX.h + fAlpha * tpRightX.h - ip.tpText.h;
            ip.atpDelta[0].v = (1.0 - fAlpha) * tpLeftX.v + fAlpha * tpRightX.v - ip.tpText.v;
         }
         //else if (pafCur != paf) {
         //   // nothing to the right, so look left
         //   fAlpha = pafCur[-1];
         //   ip.atpDelta[0].h = ip.tpText.h - ((1.0 - fAlpha) * tpLeftX.h + fAlpha * tpRightX.h);
         //   ip.atpDelta[0].v = ip.tpText.v - ((1.0 - fAlpha) * tpLeftX.v + fAlpha * tpRightX.v);
         //}
         else {
            // must be a single point. Just use delta from entire line and hope correct
            fp fx = pRight->x - pLeft->x;
            if (fx < CLOSE)
               fx = CLOSE;
            ip.atpDelta[0].h = (pRight->tp.h - pLeft->tp.h) / fx;
            ip.atpDelta[0].v = (pRight->tp.v - pLeft->tp.v) / fx;
         }
#endif // 0

         // BUGFIX - If set the following (for test purposes) then don't get
         // any random horizontal lines poping up. If not set then end up
         // with occasional horizontal lines out of place - 
         //ip.atpDelta[1].h = ip.atpDelta[1].v = 0;
      }

#ifdef _TIMERS
      if (pip[0].dwMiscIndex)
         gRenderStats.dwPixelOverwritten++;
      gRenderStats.dwPixelWritten++;
#endif
      // copy over
      if (fTransparent) {
         if ((dwTextInfo & DHLTEXT_PERPIXTRANS)) {
            // find the difference in pixels, so get blurring right
            if (dwTextInfo & DHLTEXT_HV)
               for (j = 0; j < 2; j++)
                  tpDelta.hv[j] = fabs(ip.tpText.hv[j] - tpLast.hv[j]) + ptPixelSize->hv[j];
            if (dwTextInfo & DHLTEXT_XYZ)
               for (j = 0; j < 3; j++)
                  tpDelta.xyz[j] = fabs(ip.tpText.xyz[j] - tpLast.xyz[j]) + ptPixelSize->xyz[j];

            // remember the last max
            tpLast = ip.tpText;  // BUGFIX - was, which caused problems with transparency optimization pip->tpText;
         }

         DrawTransparentPixel (pip, &ip, pm->pTexture, &pm->Material, &tpDelta, dwThread);
      }
      else
         DrawOpaquePixel (pip, &ip, dwThread);
   } // over pixels

}


/*********************************************************************************
DrawPolygonInt - Internal function. Draws a polygon, clipping against the screen border.
                  Assumes its converted to the right number of sides.

inputs
   DWORD       dwNum - Number of vertices. Must be 3 or more.
   PISHLEND     ppEnd[] - Array of pointers to endpoints of the polygon. dwNum elements.
                  NOTE: The normals HAVE to be normalized
   PISDMISC     pm - Miscellaneous info
                  if !fSolid the pm->wColor is ignored
   DWORD          dwTextInfo - Set of DHLTEXT_XXX flags.
returns
   none
*/
// draw polygon structure, one per vertex
typedef struct {
   fp   fX;               // current X. initialized to the X at iTop
   fp   fXDelta;          // amount X is increased every scan line down
   fp   fZ;               // current Z. initialized to the Z at iTop
   fp   fZDelta;          // amount Z is increased every scan line down
   fp   fW;               // current W value
   fp   fWDelta;          // amount W changes every scna line down
   fp   fYDelta;          // change in y
   CPoint pNormStart;       // normal at the top and bottom
   CPoint pNormDelta;      // normal delta
   TEXTPOINT5 tpTextStart; // texture at the top and bottom
   TEXTPOINT5 tpTextDelta; // delta from apText2 to apText1

   int      iState;           // current state. -1 = already used, 0 = active, 1 is waiting to be used
   int      iTop, iBottom;    // top and bottom Y, inclusive
   DWORD    dwMemAlpha;       // index in m_memPolyDivide where alpha is stored
   DWORD    dwNum;            // number of points in dwMemAlpha
   DWORD    dwOrigNum;        // original value in dwNum
   fp   *pafAlpha;        // point to precalculated alpha values
} SDPS, *PSDPS;

void CImageShader::DrawPolygonInt (DWORD dwNum, const PISHLEND ppEnd[],
                                const PISDMISC pm, DWORD dwThread, DWORD dwTextInfo)
{

	MALLOCOPT_INIT;
   // for citical seciton
   DWORD dwCritSec = (DWORD)-1;  // not using any right now


   PCMem pmemPolyDivide = &m_amemPolyDivide[dwThread];
   PCListFixed plistPoly = &m_alistPoly[dwThread];

   // reset memory for pixels
   pmemPolyDivide->m_dwCurPosn = 0;

   // initialize the polygon stuff, filling in the lines
   int   iNextLook;
   iNextLook = (int) m_dwHeight;
   plistPoly->Init (sizeof(SDPS));
   // redundant plistPoly->Clear();
   PSDPS pd;
   int y;

   // first pass, create structures for all the sides of the triangle
   // and then subdivide enough for perspective
   PISHLEND p1, p2, t;
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      SDPS dps;
      p1 = ppEnd[i];
      p2 = ppEnd[((i+1) < dwNum) ? (i+1) : 0];

      // p1 should be higher (lower y) than p2
      if (p1->y > p2->y) {
         t = p1;
         p1 = p2;
         p2 = t;
      }

      // find the delta
      fp yDelta;
      yDelta = p2->y - p1->y;
      if (yDelta <= 0.0)
         continue;
      yDelta = 1.0 / yDelta;

      // clip in Y direction?
      fp   fNewY;
      fNewY = ceil(p1->y);
      if (fNewY < 0)
         fNewY = 0;

      // convert the y range to integers
      dps.iTop = (int) fNewY;
      fp fAlpha;
      fAlpha = floor(p2->y);
      // if the bottom ends exactly on an integer, don't count it because count the
      // top if it's exactly on an integer. Counting both causes problems
      dps.iBottom = (int) ((fAlpha == p2->y) ? (fAlpha-1) : fAlpha);
      if (dps.iBottom >= (int) m_dwHeight)
         dps.iBottom = (int) m_dwHeight - 1;

      // if bottom < top then nothing here to draw so skip
      if (dps.iBottom < dps.iTop) {
         continue;
      }
      // if this is the first line so far then remember where things start
      if (dps.iTop < iNextLook)
         iNextLook = dps.iTop;

      // how much does x and the color change?
      dps.iState = 1;   // not yet done

      // calculate alpha values
      dps.dwMemAlpha = (DWORD) pmemPolyDivide->m_dwCurPosn;
	   MALLOCOPT_OKTOMALLOC;
      dps.dwNum = GenerateAlphaInPerspective (p1->y, p2->y, p1->w, p2->w,
         dps.iTop, dps.iBottom - dps.iTop + 1, pmemPolyDivide, TRUE);
   	MALLOCOPT_RESTORE;
      dps.dwNum /= sizeof(fp);
      if (!dps.dwNum)
         continue;
      dps.dwOrigNum = dps.dwNum;

      // and the deltas
      dps.fX = p1->x;
      dps.fXDelta = (p2->x - dps.fX) * yDelta;
      dps.fX += (fNewY - p1->y) * dps.fXDelta;

      dps.fYDelta = p2->y - p1->y;

      dps.fZ = p1->z;
      dps.fZDelta = p2->z - dps.fZ;   // since will multiply this by alpha
      dps.fW = p1->w;
      dps.fWDelta = p2->w - dps.fW;   // since will multiply this by alpha


      // delta norm
      dps.pNormStart.Copy (&p1->pNorm);
      dps.pNormDelta.Subtract (&p2->pNorm, &dps.pNormStart);

      // calculate delta of textures
      dps.tpTextStart = p1->tp;
      if (dwTextInfo & DHLTEXT_HV)
         for (j = 0; j < 2; j++)
            dps.tpTextDelta.hv[j] = p2->tp.hv[j] - dps.tpTextStart.hv[j];
      if (dwTextInfo & DHLTEXT_XYZ)
         for (j = 0; j < 3; j++)
            dps.tpTextDelta.xyz[j] = p2->tp.xyz[j] - dps.tpTextStart.xyz[j];

      // cut of the small bits at the top and bottom
      //fp fTotal, fSkip;
      //fTotal = p2->y - p1->y;
      //fSkip = fNewY - p1->y;
      //fAlpha = 0; // BUGFIX - Handled by GenerateAlphaInPerspective - fSkip / fTotal;
      //dps.apNorm[0].Average (&p2->pNorm, &p1->pNorm, fAlpha);
      //dps.apText[0].h = (1.0 - fAlpha) * p1->tp.h - fAlpha * p2->tp.h;
      //dps.apText[0].v = (1.0 - fAlpha) * p1->tp.v - fAlpha * p2->tp.v;
      //fAlpha = 1; // BUGFIX - Handled by GenerateAlphaInPerspective (fSkip + dps.iBottom - dps.iTop) / fTotal;
      //dps.apNorm[1].Average (&p2->pNorm, &p1->pNorm, fAlpha);
      //dps.apText[1].h = (1.0 - fAlpha) * p1->tp.h - fAlpha * p2->tp.h;
      //dps.apText[1].v = (1.0 - fAlpha) * p1->tp.v - fAlpha * p2->tp.v;

      // else, add
	   MALLOCOPT_OKTOMALLOC;
      plistPoly->Add (&dps);
	   MALLOCOPT_RESTORE;

   }

   // go back over the points
   DWORD dwpn;
   dwpn = plistPoly->Num();
   for (i = 0; i < dwpn; i++) {
      pd = (PSDPS) plistPoly->Get(i);
      
      // it's OK to use the memory now
      pd->pafAlpha = (fp*) (((PBYTE) pmemPolyDivide->p) + pd->dwMemAlpha);
   }

   // find first scanline that need to worry about
   DWORD dwNumLine;
   DWORD dwNumActive;
   dwNumActive = 0;
   pd = (PSDPS) plistPoly->Get(0);
   dwNumLine = plistPoly->Num();

   // mark those on the next-look scanline as active and then find the one below
   y = iNextLook;
   TEXTPOINT5 tpPixelSize, tpLast, tpCurAvg;
   BOOL fLastValid = FALSE;
   while (TRUE) {
      // if iNextLook is beyond then end of the screen then skip
      if (iNextLook >= (int) m_dwHeight)
         break;

      iNextLook = (int) m_dwHeight;
      for (i = 0; i < dwNumLine; i++) {
         if (pd[i].iState < 1)
            continue;

         // see if should make this active
         if (pd[i].iTop <= y) {
            pd[i].iState = 0;
            dwNumActive++;
            continue;
         }

         // check
         if (pd[i].iTop < iNextLook)
            iNextLook = pd[i].iTop;
      }

      // if there's nothing active then we're all done
      if (!dwNumActive)
         goto done;


      PSDPS pLast, pCur, ps;
      ISHLEND left, right;
      // loop from current y down to iNextLook
      for (; y < iNextLook; y++) {
         // go through all the lines and see if any should be deactivated
         // also, while at it, do a bubble sort so X appears on the ledt
   bubble:
         pLast = NULL;
         for (i = 0; i < dwNumLine; i++) if (!pd[i].iState) {
            // consider deactivating it
            if (y > pd[i].iBottom) {
               pd[i].iState = -1;
               dwNumActive--;
               continue;
            }

            // if there's a last value, then check to make sure
            if (pLast && (pLast->fX > pd[i].fX)) {
               // bubble sort
               SDPS   temp;
               temp = *pLast;
               *pLast = pd[i];
               pd[i] = temp;
               goto bubble;
            }
            pLast = &pd[i];
         }

         // if there aren't any active AND iNextLook >= end, then done
         if (!dwNumActive && (iNextLook >= (int) m_dwHeight))
            goto done;

#ifdef _DEBUG
         if (dwNumActive % 2)
            OutputDebugString ("Odd number of lines in polygon!\r\n");
#endif
         // when we get here we have an even number of lines, and they're
         // sorted. Therefore, draw them
         pLast = NULL;
         DWORD j;
         for (i = 0; i < dwNumLine; i++) if (!pd[i].iState) {
            if (!pLast) {
               pLast = &pd[i];
               continue;
            }
            pCur = &pd[i];

            // if get here have a pLast, and a new point, draw a line between them
            // while we're at it increment

            // left values
            fp fAlpha; //, fNext, fPrev;
            //TEXTUREPOINT atpDelta[2];
            DWORD k;
            PISHLEND pi;
            for (k = 0; k < 2; k++) {
               ps = (k ? pCur : pLast);
               pi = (k ? &right : &left);

               fAlpha = ps->pafAlpha[0];
               pi->x = ps->fX;
               ps->fX += ps->fXDelta;
               pi->z = ps->fZ + ps->fZDelta * fAlpha;
               pi->w = ps->fW + ps->fWDelta * fAlpha;

               for (j = 0; j < 3; j++)
                  pi->pNorm.p[j] = ps->pNormStart.p[j] + fAlpha * ps->pNormDelta.p[j];

               if (dwTextInfo & DHLTEXT_HV)
                  for (j = 0; j < 2; j++) {
                     pi->tp.hv[j] = ps->tpTextStart.hv[j] + fAlpha * ps->tpTextDelta.hv[j];

                     if (k)
                        tpCurAvg.hv[j] += pi->tp.hv[j] / 2.0;
                     else
                        tpCurAvg.hv[j] =  pi->tp.hv[j] / 2.0;
                  }
               if (dwTextInfo & DHLTEXT_XYZ)
                  for (j = 0; j < 3; j++) {
                     pi->tp.xyz[j] = ps->tpTextStart.xyz[j] + fAlpha * ps->tpTextDelta.xyz[j];

                     if (k)
                        tpCurAvg.xyz[j] += pi->tp.xyz[j] / 2.0;
                     else
                        tpCurAvg.xyz[j] =  pi->tp.xyz[j] / 2.0;
                  }

               // include in average
#if 0 // no longer using atpDelta
               if ((ps->dwNum != ps->dwOrigNum) && ps->dwOrigNum) {
                  // see difference between this point and next
                  fPrev = ps->pafAlpha[-1];
                  fNext = ps->pafAlpha[1];
                  atpDelta[k].h = (((1.0 - fNext) * ps->apText[0].h + fNext * ps->apText[1].h) -
                     ((1.0 - fPrev) * ps->apText[0].h + fPrev * ps->apText[1].h)) / 2;
                  atpDelta[k].v = (((1.0 - fNext) * ps->apText[0].v + fNext * ps->apText[1].v) -
                     ((1.0 - fPrev) * ps->apText[0].v + fPrev * ps->apText[1].v)) / 2;
               }
               else if (ps->dwOrigNum) {
                  // see difference between this point and next
                  fNext = ps->pafAlpha[1];
                  atpDelta[k].h = ((1.0 - fNext) * ps->apText[0].h + fNext * ps->apText[1].h) - pi->tp.h;
                  atpDelta[k].v = ((1.0 - fNext) * ps->apText[0].v + fNext * ps->apText[1].v) - pi->tp.v;
               }
               //else if (ps->dwOrigNum) {
               //   // since at end, difference between this and last
               //   fNext = ps->pafAlpha[-1];
               //   atpDelta[k].h = pi->tp.h - ((1.0 - fNext) * ps->apText[0].h + fNext * ps->apText[1].h);
               //   atpDelta[k].v = pi->tp.v - ((1.0 - fNext) * ps->apText[0].v + fNext * ps->apText[1].v);
               //}
               else {
                  // less than one line, so just take the entire delta
                  fp fy = ps->fYDelta;
                  fy = max(fy, CLOSE);
                  atpDelta[k].h = (ps->apText[1].h - ps->apText[0].h) /fy;
                  atpDelta[k].v = (ps->apText[1].v - ps->apText[0].v) / fy;
               }
#endif // 0

               // increaste index into alphas
               if (ps->dwNum) {
                  ps->dwNum--;
                  ps->pafAlpha++;
               }
            }


#if 0 // no loner using atpDelta
            // adjust atpDelta[0] and [1] because at the momenet they do not perfectly
            // represent the texture tansition on a point going straight down.
            fp fDeltaX;
            TEXTUREPOINT pDelta;
            fDeltaX = right.x - left.x;
            pDelta.h = right.tp.h - left.tp.h;
            pDelta.v = right.tp.v - left.tp.v;
            if (fDeltaX > CLOSE) for (k = 0; k < 2; k++) {
               fp fScale;
               ps = (k ? pCur : pLast);
               fScale = ps->fXDelta / fDeltaX;
               atpDelta[k].h -= fScale * pDelta.h;
               atpDelta[k].v -= fScale * pDelta.v;
            }
#endif // 0

            pLast = NULL;  // wipe it out for next time

            // BUGBUG - dont calculate if no texture, and only care if
            // have transparency that might change
            // calculate delta
            if (fLastValid) {
               if (dwTextInfo & DHLTEXT_HV)
                  for (j = 0; j < 2; j++)
                     tpPixelSize.hv[j] = fabs(tpLast.hv[j] - tpCurAvg.hv[j]);
               if (dwTextInfo & DHLTEXT_XYZ)
                  for (j = 0; j < 3; j++)
                     tpPixelSize.xyz[j] = fabs(tpLast.xyz[j] - tpCurAvg.xyz[j]);
            }
            else
               memset (&tpPixelSize, 0, sizeof(tpPixelSize));  // set to 0 since nothing else
            fLastValid = TRUE;
            if (dwTextInfo)   // only set if have texture
               tpLast = tpCurAvg;

            // potentially enter a critical seciton
            if (m_fCritSecInitialized && (y >= 0)) {
               DWORD dwCritSecWant = ((DWORD)y / IMAGESCANLINESPERCRITSEC) % IMAGECRITSEC;
               if (dwCritSecWant != dwCritSec) {
                  if (dwCritSec != (DWORD)-1)
                     LeaveCriticalSection (&m_aCritSec[dwCritSec]);
                  dwCritSec = dwCritSecWant;
                  EnterCriticalSection (&m_aCritSec[dwCritSec]);
               }
            }

            // draw the line
            DrawHorzLine (y, &left, &right, /*&atpDelta[0], &atpDelta[1],*/ pm,
               &tpPixelSize, dwTextInfo, dwThread);

         } // over lines
      } // y to iNextLook

   } // while TRUE

done:
   // free up the critical seciton
   if (dwCritSec != (DWORD)-1)
      LeaveCriticalSection (&m_aCritSec[dwCritSec]);
   
}




/*********************************************************************************
DrawPolygon - Draws a polygon, clipping against the screen border.

inputs
   DWORD       dwNum - Number of vertices. Must be 3 or more.
   PISHLEND     ppEnd[] - Array of pointers to endpoints of the polygon. dwNum elements.
                  NOTE: The normals HAVE to be normalized
   PISDMISC     pm - Miscellaneous info
                  if !fSolid the pm->wColor is ignored
   BOOL        fTriangle - If TRUE, and the polygon isnt a triangle, then convert into one
returns
   none
*/
void CImageShader::DrawPolygon (DWORD dwNum, const PISHLEND ppEnd[],
                                const PISDMISC pm, BOOL fTriangle, DWORD dwThread)
{
   // BUGFIX - Made a DrawPolygonInt() method that should make the code run slightly faster

   if (dwNum < 3)
      return;

#ifdef _TIMERS
   gRenderStats.dwNumTri++;
#endif
   DWORD i;

   // BUGFIX: see if can trivially reject this because entirely hidden
   CPoint pMin, pMax;
   for (i = 0; i < dwNum; i++) {
      if (i) {
         pMin.p[0] = min(pMin.p[0], ppEnd[i]->x);
         pMax.p[0] = max(pMax.p[0], ppEnd[i]->x);
         pMin.p[1] = min(pMin.p[1], ppEnd[i]->y);
         pMax.p[1] = max(pMax.p[1], ppEnd[i]->y);
         pMin.p[2] = min(pMin.p[2], ppEnd[i]->z);
      }
      else {
         pMax.p[0] = pMin.p[0] = ppEnd[i]->x;
         pMax.p[1] = pMin.p[1] = ppEnd[i]->y;
         pMin.p[2] = ppEnd[i]->z;
      }
   }
   RECT r;
   r.left = (int) floor (pMin.p[0]);
   r.right = (int) ceil (pMax.p[0]);
   r.top = (int) floor (pMin.p[1]);
   r.bottom = (int) ceil (pMax.p[1]);
   if (IsCompletelyCovered (&r, pMin.p[2])) {
#ifdef _TIMERS
      gRenderStats.dwTriCovered++;
#endif
      return;
   }


   // see if it's a subpixel polygon
   // dont worry about checking that max is on the integer because drawing goes
   // from min (inclusive) to max (exclusive)
   if (((r.left+1 == r.right) && (pMin.p[0] != (fp)r.left)) ||
      ((r.top+1 == r.bottom) && (pMin.p[1] != (fp)r.top)) )
      return;

   // will need to check for transparency per pixel
   DWORD dwTextInfo;
   if (pm->pTexture) {
      dwTextInfo = (pm->pTexture->MightBeTransparent(dwThread) & 0x02) ? (DHLTEXT_TRANSPARENT | DHLTEXT_PERPIXTRANS) : 0;
      DWORD dw = pm->pTexture->DimensionalityQuery (dwThread);
      if (dw & 0x01)
         dwTextInfo |= DHLTEXT_HV;
      if (dw & 0x02)
         dwTextInfo |= DHLTEXT_XYZ;
   }
   else
      dwTextInfo = 0;

   if (fTriangle && (dwNum > 3)) {
      // if want it converted into triangles and it isn't, then convert it
      PISHLEND ap[3];

      DWORD i;
      for (i = 0; i < (dwNum - 2); i++) {
         ap[0] = ppEnd[(dwNum - i / 2) % dwNum];
         ap[1] = ppEnd[(((i+1)/2) + 1) % dwNum];
         if (i % 2)
            ap[2] = ppEnd[dwNum - (i+1) / 2];
         else
            ap[2] = ppEnd[(i / 2 + 2) % dwNum];

         DrawPolygonInt (3, ap, pm, dwThread, dwTextInfo);
      }
   }
   else
      DrawPolygonInt (dwNum, ppEnd, pm, dwThread, dwTextInfo);

}

/**************************************************************************
CImageShader::IsCompletelyCovered - Returns TRUE if all points in the rectangle have
z-buffer values less than iZ. Use this to determine if painting an object would
be useless because it's completely covered by other objects.

inputs
   RECT     *pRect - rectangle. This can be beyond the end of the CImageShader screen.
   fp   fZ - Depth in meters
returns
   BOOL - TRUE if it's completely coverd
*/
BOOL CImageShader::IsCompletelyCovered (const RECT *pRect, fp fZ)
{
   RECT  r;
   r.left = max(0,pRect->left);
   r.top = max(0,pRect->top);
   r.right = min((int)m_dwWidth-1, pRect->right);
   r.bottom = min((int)m_dwHeight-1, pRect->bottom);

   // if negative size then return true
   if ((r.right < r.left) || (r.bottom < r.top))
      return TRUE;

   int y;
   PISPIXEL pip;
   DWORD dwCount;
   for (y = r.top; y <= r.bottom; y++) {
      pip = Pixel(r.left, y);
      dwCount = (DWORD) (r.right - r.left + 1);
      for (; dwCount; dwCount--, pip++)
         if (pip->fZ > fZ)
            return FALSE;
   }

   return TRUE;
}


