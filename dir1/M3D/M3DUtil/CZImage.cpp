/************************************************************************
CZImage - Image that only supports Z-depth, nothing else

begun 5/5/05 by Mike Rozak
Copyright 2005 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// #define  COLORFRAC   14       // # of bits shift color left when doing blend from one side to another


/************************************************************************
Constructor and destructor
*/
CZImage::CZImage (void)
{
   m_dwWidth = m_dwHeight = 0;
   m_f360 = FALSE;

   m_fCritSecInitialized = FALSE;

}

CZImage::~CZImage (void)
{
   // free up critical sections
   DWORD i;
   if (m_fCritSecInitialized)
      for (i = 0; i < IMAGECRITSEC; i++)
         DeleteCriticalSection (&m_aCritSec[i]);
}



/*************************************************************************
CZImage::CritSectionInitialize() - Call this to make sure the scanline
critical sections are initialized. This makes for safer rendering of
lines and filled polys on the image, ensuring that the threads don't
overwrite each others scanlines.

If this has already been called for this object, then it does nothning
*/
void CZImage::CritSectionInitialize (void)
{
   if (m_fCritSecInitialized)
      return;

   DWORD i;
   for (i = 0; i < IMAGECRITSEC; i++)
      InitializeCriticalSection (&m_aCritSec[i]);

   m_fCritSecInitialized = TRUE;
}


/*************************************************************************
CZImage::Init - Initializes the data structures for the image but does NOT
blank out the pixels or Z buffer. You can call this to reinitialze the size.

inputs
   DWORD       dwX, dwY - X Y dimensions
returns
   BOOL - TRUE if successful
*/
BOOL CZImage::Init (DWORD dwX, DWORD dwY)
{
   if (!m_memBuffer.Required(dwX * dwY * sizeof(ZIMAGEPIXEL)))
      return FALSE;
   m_dwWidth = dwX;
   m_dwHeight = dwY;
   return TRUE;
}

/*************************************************************************
CZImage::Init - Initializes the data structures for the image. It also
CLEARS the image color to cColor and sets the ZBuffer to INFINITY.
You can call this to reinitialze the size.

inputs
   DWORD       dwX, dwY - X Y dimensions
   DWORD       dwColor - Color is ignored
   fp      fZ - Z height to use. If deeper than max depth then set to that
returns
   BOOL - TRUE if successful
*/
BOOL CZImage::Init (DWORD dwX, DWORD dwY, DWORD dwColor, fp fZ)
{
   if (!Init(dwX, dwY))
      return FALSE;

   return Clear (fZ);
}

/*************************************************************************
CZImage::Clear - Clears the image color to cColor, and sets the ZBuffer to
infinity.

inputs
   fp      fZ - Z height to use. If deeper than max depth then set to that
returns
   BOOL - TRUE if successful
*/
BOOL CZImage::Clear (fp fZ)
{
   // fill in prototype
   ZIMAGEPIXEL ip;
   memset (&ip, 0, sizeof(ip));
   if (fZ >= ZINFINITE)
      ip.fZ = ZINFINITE;
   else
      ip.fZ = fZ;

   // do the first line
   DWORD    i;
   PZIMAGEPIXEL pip;
   pip = Pixel (0, 0);
   for (i = 0; i < m_dwWidth; i++)
      pip[i] = ip;

   // all the other lines
   for (i = 1; i < m_dwHeight; i++)
      memcpy (Pixel(0,i), Pixel(0,0), sizeof(ZIMAGEPIXEL) * m_dwWidth);

   return TRUE;
}

/*************************************************************************
CZImage::Clone - Creates a new CZImage object which is a clone of this one.
It even includes the same bitmap information and Z Buffer

inputs
   none
returns
   PCZImage  - Image. NULL if cant create
*/
CZImage *CZImage::Clone (void)
{
   PCZImage  p = new CZImage;
   if (!p)
      return FALSE;

   if (!p->Init (m_dwWidth, m_dwHeight)) {
      delete p;
      return NULL;
   }

   // copy over the bitmap
   memcpy (p->Pixel(0,0), Pixel(0,0), m_dwWidth * m_dwHeight * sizeof(ZIMAGEPIXEL));
   p->m_f360 = m_f360;

   return p;
}

/****************************************************************************
CZImage::DrawPixel - Draws a pixel on the screen.

inputs
   DWORD    dwX, dwY - XY location. ASSUMES that the points are within the drawing area
   fp       fZ - Depth in meters. Positive value
returns
   none
*/
__inline void CZImage::DrawPixel (DWORD dwX, DWORD dwY, fp fZ)
{
   PZIMAGEPIXEL pip = Pixel(dwX, dwY);

   // zbuffer
   if (fZ >= pip->fZ)
      return;

   pip->fZ = fZ;
}


/********************************************************************
DrawLine - Draws a line on the screen.

inputs
   PHILEND     pStart, pEnd - Start and end points. NOTE: While this has
                  safety clipping, it's not terribly fast for long lines off
                  the screen area.
   PIDMISC     pm - Miscellaneous info. Only uses the object ID stuff.
*/
void CZImage::DrawLine (PIHLEND pStart, PIHLEND pEnd, PIDMISC pm)
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

         DrawPixel ((DWORD)x1, (DWORD)y1, pStart->z);
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
      PIHLEND t;
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

   // RGB

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

         DrawPixel ((DWORD) i, (DWORD) fY, (int) fZ);
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

         DrawPixel ((DWORD) fX, (DWORD) i, (int) fZ);
      }
   }


done:
   // free up the critical seciton
   if (dwCritSec != (DWORD)-1)
      LeaveCriticalSection (&m_aCritSec[dwCritSec]);

}

/**************************************************************************
CZImage::DrawHorzLine - Draws a horontal line that has a color blend or solid.

inputs
   int      y - y scan line
   PIHLEND  pLeft, pRight - Left and right edges. Notes:
               pLeft must be left, and pRight must be right
               Ignore pXXXX.y.
               Ignore pXXXX.wColor if fSolid is set.
   PIDMISC  pm - Miscellaneous info. Ignores pm.wColor if !fSolid
   BOOL     fSolid - Set to true if should fill with a solid color, FALSE if color blend
   PCTextureMap pTexture - If not NULL then use the texture map
   DWORD    dwTextInfo - Texture info... one or more of the following flags
               DHLTEXT_TRANSPARENT - Transparent texture
               DHLTEXT_PERPIXTRANS - Transparency might be per pixel
               DHLTEXT_HV - Uses HV
               DHLTEXT_XYZ - Uses XYZ
   DWORD       dwThread - Thread rendering with, from 0 to MAXRAYTHREAD-1
returns
   none
*/
#define  REJECT      (-ZINFINITE)
#define  SHADOWTRANSCUTOFF    0x4000      // if transparency less than this this then entirely
                                          // transparent for calculating shadows
      // BUGFIX - Moved this from 0xf000 to 0x4000 so fewer shadows created

void CZImage::DrawHorzLine (int y, const PIHLEND pLeft, const PIHLEND pRight, const PIDMISC pm, BOOL fSolid,
                           const PCTextureMapSocket pTexture, DWORD dwTextInfo, DWORD dwThread)
{
	MALLOCOPT_INIT;
   // check y
   if ((y < 0) || (y >= (int)m_dwHeight))
      return;

   // disable because doing in the polygon method
   // if (!(dwTextInfo & DHLTEXT_PERPIXTRANS) && pm->Material.m_wTransparency >= SHADOWTRANSCUTOFF)
   //   return;  // dont bother

   CMaterial mat;

#ifdef _TIMERS
   gRenderStats.dwNumHLine++;
#endif
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
   DWORD dw, dwPixels;
   PZIMAGEPIXEL pip, pipOrig;
   fp fMinZ;
   pip = pipOrig = Pixel(xStart, y);
   dwPixels = (DWORD) (xEnd - xStart + 1);
   fMinZ = min(pLeft->z, pRight->z);
   for (dw = dwPixels; dw; dw--, pip++)
      if (fMinZ < pip->fZ)
         break;
   if (!dw) {
#ifdef _TIMERS
      gRenderStats.dwPixelTrivial += dwPixels;
#endif
      return;  // trivial reject
   }


   // figure out alpha
   m_amemLineDivide[dwThread].m_dwCurPosn = 0;
   DWORD dwNum;
	MALLOCOPT_OKTOMALLOC;
   dwNum = GenerateAlphaInPerspective (pLeft->x, pRight->x, pLeft->w, pRight->w,
      (int) xStart, (int) xEnd - (int) xStart + 1, &m_amemLineDivide[dwThread]);
	MALLOCOPT_RESTORE;
   if (!dwNum)
      return;
   fp *paf;
   paf = (fp*) m_amemLineDivide[dwThread].p;

   // more complex calculation of Z buffer that takes into account
   // the W-interpolation
   fp zDelta;
   fp fStartZ;
   fp fX;
   pip = pipOrig = Pixel(xStart, y);
   dwPixels = (DWORD) (xEnd - xStart + 1);
   fp *pafCur;
   zDelta = pRight->z - pLeft->z;
   DWORD j;
   TEXTPOINT5 tpCur, tpDelta;
   if (dwTextInfo & DHLTEXT_PERPIXTRANS) {
      if (dwTextInfo & DHLTEXT_HV)
         for (j = 0; j < 2; j++)
            tpDelta.hv[j] = pRight->tp.hv[j] - pLeft->tp.hv[j];

      if (dwTextInfo & DHLTEXT_XYZ)
         for (j = 0; j < 3; j++)
            tpDelta.xyz[j] = pRight->tp.xyz[j] - pLeft->tp.xyz[j];
   }

   for (pafCur = paf,dw = dwPixels, fX = xStart; dw; dw--, pip++, pafCur++) {

      fStartZ = (pLeft->z + zDelta * pafCur[0]);

      // if the pixel is behind then ignore
      if (pip->fZ < fStartZ) {
#ifdef _TIMERS
         gRenderStats.dwPixelBehind++;
#endif
         continue;
      }

      // else, if there's per-pixel transparency then check
      if (dwTextInfo & DHLTEXT_PERPIXTRANS) {
         // BUGBUG - this is the slow way of doing this

         // fill in current texture point
         if (dwTextInfo & DHLTEXT_HV)
            for (j = 0; j < 2; j++)
               tpCur.hv[j] = pLeft->tp.hv[j] + pafCur[0] * tpDelta.hv[j];

         if (dwTextInfo & DHLTEXT_XYZ)
            for (j = 0; j < 3; j++)
               tpCur.xyz[j] = pLeft->tp.xyz[j] + pafCur[0] * tpDelta.xyz[j];

         //memcpy (&mat, &pm->Material, sizeof(mat));
         mat.m_wTransparency = pm->Material.m_wTransparency;
         pTexture->FillPixel (dwThread, TMFP_TRANSPARENCY, NULL, &tpCur, NULL, &mat, NULL, FALSE);
            // NOTE: Using fHighQuality = FALSE for the transparency
         if (mat.m_wTransparency >= SHADOWTRANSCUTOFF)
            continue;
      }

      // the horizontal line is definitely in front
      pip->fZ = fStartZ;
   }
}

/*********************************************************************************
DrawPolygonInt - Draws a polygon, clipping against the screen border.

inputs
   DWORD       dwNum - Number of vertices. Must be 3 or more.
   PIHLEND     ppEnd[] - Array of pointers to endpoints of the polygon. dwNum elements.
                  if fSolid then ppEnd[x]->wColor is ignored
   PIDMISC     pm - Miscellaneous info
                  if !fSolid the pm->wColor is ignored
   BOOL        fSolid - Set to TRUE if polygon is one solid color, FALSE if it's blended.
   PCTextureMap pTexture - If not NULL then use this texture map for color
   DWORD       dwThread - Thread rendering with, from 0 to MAXRAYTHREAD-1
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
   // fp   afColor[3];       // Current RGB color, from 0..65535. Initialized to color at iTop
   // fp   afColorDelta[3];  // RGB color delta per scanline
   TEXTPOINT5 tpStart;      // starting point for the texture
   TEXTPOINT5 tpDelta;      // end - start

   int      iState;           // current state. -1 = already used, 0 = active, 1 is waiting to be used
   int      iTop, iBottom;    // top and bottom Y, inclusive
   DWORD    dwMemAlpha;       // index in m_memPolyDivide where alpha is stored
   DWORD    dwNum;            // number of points in dwMemAlpha
   fp   *pafAlpha;        // point to precalculated alpha values
} DPS, *PDPS;

void CZImage::DrawPolygonInt (DWORD dwNum, const PIHLEND ppEnd[], const PIDMISC pm, BOOL fSolid,
                          const PCTextureMapSocket pTexture,
                          DWORD dwThread, DWORD dwTextInfo)
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
   plistPoly->Init (sizeof(DPS));
   plistPoly->Clear();
   PDPS pd;
   int y;

   // first pass, create structures for all the sides of the triangle
   // and then subdivide enough for perspective
   PIHLEND p1, p2, t;
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      DPS dps;
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
         dps.iTop, dps.iBottom - dps.iTop + 1, pmemPolyDivide);
   	MALLOCOPT_RESTORE;
      dps.dwNum /= sizeof(fp);
      if (!dps.dwNum)
         continue;

      // and the deltas
      dps.fX = p1->x;
      dps.fXDelta = (p2->x - dps.fX) * yDelta;
      dps.fX += (fNewY - p1->y) * dps.fXDelta;

      dps.fZ = p1->z;
      dps.fZDelta = p2->z - dps.fZ;   // since will multiply this by alpha
      dps.fW = p1->w;
      dps.fWDelta = p2->w - dps.fW;   // since will multiply this by alpha

      if (dwTextInfo & DHLTEXT_HV)
         for (j = 0; j < 2; j++) {
            dps.tpStart.hv[j] = p1->tp.hv[j];
            dps.tpDelta.hv[j] = p2->tp.hv[j] - p1->tp.hv[j];
         }
      if (dwTextInfo & DHLTEXT_XYZ)
         for (j = 0; j < 3; j++) {
            dps.tpStart.xyz[j] = p1->tp.xyz[j];
            dps.tpDelta.xyz[j] = p2->tp.xyz[j] - p1->tp.xyz[j];
         }

      // else, add
	   MALLOCOPT_OKTOMALLOC;
      plistPoly->Add (&dps);
	   MALLOCOPT_RESTORE;

   } // i

   // go back over the points
   DWORD dwpn;
   dwpn = plistPoly->Num();
   for (i = 0; i < dwpn; i++) {
      pd = (PDPS) plistPoly->Get(i);
      
      // it's OK to use the memory now
      pd->pafAlpha = (fp*) (((PBYTE) pmemPolyDivide->p) + pd->dwMemAlpha);
   }

   // find first scanline that need to worry about
   DWORD dwNumLine;
   DWORD dwNumActive;
   dwNumActive = 0;
   pd = (PDPS) plistPoly->Get(0);
   dwNumLine = plistPoly->Num();

   // mark those on the next-look scanline as active and then find the one below
   y = iNextLook;
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


      PDPS pLast, pCur;
      IHLEND left, right;
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
               DPS   temp;
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
         for (i = 0; i < dwNumLine; i++) if (!pd[i].iState) {
            if (!pLast) {
               pLast = &pd[i];
               continue;
            }
            pCur = &pd[i];

            // if get here have a pLast, and a new point, draw a line between them
            // while we're at it increment

            // left values
            fp fAlpha;
            fAlpha = pLast->pafAlpha[0];
            left.x = pLast->fX;
            pLast->fX += pLast->fXDelta;
            left.z = pLast->fZ + pLast->fZDelta * fAlpha;
            left.w = pLast->fW + pLast->fWDelta * fAlpha;
            if (dwTextInfo & DHLTEXT_HV)
               for (j = 0; j < 2; j++)
                  left.tp.hv[j] = pLast->tpStart.hv[j] + pLast->tpDelta.hv[j] * fAlpha;
            if (dwTextInfo & DHLTEXT_XYZ)
               for (j = 0; j < 3; j++)
                  left.tp.xyz[j] = pLast->tpStart.xyz[j] + pLast->tpDelta.xyz[j] * fAlpha;

            fAlpha = pCur->pafAlpha[0];
            right.x = pCur->fX;
            pCur->fX += pCur->fXDelta;
            right.z = pCur->fZ + pCur->fZDelta * fAlpha;
            right.w = pCur->fW + pCur->fWDelta * fAlpha;
            if (dwTextInfo & DHLTEXT_HV)
               for (j = 0; j < 2; j++)
                  right.tp.hv[j] = pCur->tpStart.hv[j] + pCur->tpDelta.hv[j] * fAlpha;
            if (dwTextInfo & DHLTEXT_XYZ)
               for (j = 0; j < 3; j++)
                  right.tp.xyz[j] = pCur->tpStart.xyz[j] + pCur->tpDelta.xyz[j] * fAlpha;

            // increaste index into alphas
            if (pLast->dwNum) {
               pLast->dwNum--;
               pLast->pafAlpha++;
            }
            if (pCur->dwNum) {
               pCur->dwNum--;
               pCur->pafAlpha++;
            }

            pLast = NULL;  // wipe it out for next time

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
            DrawHorzLine (y, &left, &right, pm, fSolid, pTexture, dwTextInfo, dwThread);

         } // over lines
      } // over y

   } // while iNextLook
   

done:
   // free up the critical seciton
   if (dwCritSec != (DWORD)-1)
      LeaveCriticalSection (&m_aCritSec[dwCritSec]);
}


/*********************************************************************************
DrawPolygon - Draws a polygon, clipping against the screen border.

inputs
   DWORD       dwNum - Number of vertices. Must be 3 or more.
   PIHLEND     ppEnd[] - Array of pointers to endpoints of the polygon. dwNum elements.
                  if fSolid then ppEnd[x]->wColor is ignored
   PIDMISC     pm - Miscellaneous info
                  if !fSolid the pm->wColor is ignored
   BOOL        fSolid - Set to TRUE if polygon is one solid color, FALSE if it's blended.
   PCTextureMap pTexture - If not NULL then use this texture map for color
   BOOL        fTriangle - If TRUE, and the polygon isnt a triangle, then convert into one
   DWORD       dwThread - Thread rendering with, from 0 to MAXRAYTHREAD-1
returns
   none
*/

void CZImage::DrawPolygon (DWORD dwNum, const PIHLEND ppEnd[], const PIDMISC pm, BOOL fSolid,
                          const PCTextureMapSocket pTexture, BOOL fTriangle,
                          DWORD dwThread)
{
   // will need to check for transparency per pixel
   DWORD dwTextInfo;
   if (pTexture) {
      dwTextInfo = (pTexture->MightBeTransparent(dwThread) & 0x02) ? (DHLTEXT_TRANSPARENT | DHLTEXT_PERPIXTRANS) : 0;
      DWORD dw = pTexture->DimensionalityQuery (dwThread);
      if (dw & 0x01)
         dwTextInfo |= DHLTEXT_HV;
      if (dw & 0x02)
         dwTextInfo |= DHLTEXT_XYZ;
   }
   else
      dwTextInfo = 0;

   if (!(dwTextInfo & DHLTEXT_PERPIXTRANS) && pm->Material.m_wTransparency >= SHADOWTRANSCUTOFF)
      return;  // dont bother

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

   if (fTriangle && (dwNum > 3)) {
      // if want it converted into triangles and it isn't, then convert it
      PIHLEND ap[3];

      DWORD i;
#if 0 // slower, but better - cant do this because messes up textures, and doesnt seem to work better
      IHLEND   iCenter;
      memset (&iCenter, 0, sizeof(iCenter));
      for (i = 0; i < dwNum; i++) {
         iCenter.tp.h += ppEnd[i]->tp.h;
         iCenter.tp.v += ppEnd[i]->tp.v;

         iCenter.x += ppEnd[i]->x;
         iCenter.y += ppEnd[i]->y;
         iCenter.z += ppEnd[i]->z;
         iCenter.w += ppEnd[i]->w;
      }
      iCenter.tp.h /= (fp)dwNum;
      iCenter.tp.v /= (fp)dwNum;
      iCenter.x /= (fp)dwNum;
      iCenter.y /= (fp)dwNum;
      iCenter.z /= (fp)dwNum;
      iCenter.w /= (fp)dwNum;

      for (i = 0; i < dwNum; i++) {
         ap[0] = ppEnd[i];
         ap[1] = ppEnd[(i+1)%dwNum];
         ap[2] = &iCenter;

         DrawPolygonInt (3, ap, pm, fSolid, pTexture, FALSE);
      }
#else // doesnt work well because end up with crackes when intersect
      for (i = 0; i < (dwNum - 2); i++) {
         ap[0] = ppEnd[(dwNum - i / 2) % dwNum];
         ap[1] = ppEnd[(((i+1)/2) + 1) % dwNum];
         if (i % 2)
            ap[2] = ppEnd[dwNum - (i+1) / 2];
         else
            ap[2] = ppEnd[(i / 2 + 2) % dwNum];

         DrawPolygonInt (3, ap, pm, fSolid, pTexture, dwThread, dwTextInfo);
      }
#endif // 0
   }
   else
      DrawPolygonInt (dwNum, ppEnd, pm, fSolid, pTexture, dwThread, dwTextInfo);

}


/**************************************************************************
CZImage::IsCompletelyCovered - Returns TRUE if all points in the rectangle have
z-buffer values less than iZ. Use this to determine if painting an object would
be useless because it's completely covered by other objects.

inputs
   RECT     *pRect - rectangle. This can be beyond the end of the CZImage screen.
   fp   fZ - Depth in meters
returns
   BOOL - TRUE if it's completely coverd
*/
BOOL CZImage::IsCompletelyCovered (const RECT *pRect, fp fZ)
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
   PZIMAGEPIXEL pip;
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

/********************************************************************************
CZImage:: Overlay - Copy the image from the source (this object) to the destination
   CZImage object. This code will do checking againt boundry conditions so as not
   to overwrite area.

inputs
   RECT        *prSrc - Source rectangle. If NULL use the entire source
   CZImage      *pDestImage - To copy onto
   POINT       pDest - Where to copy the upper left corner onto
   BOOL        fOverlayZObject - If TRUE, copy over the Z depth information from
               the frist. If FALSE, leave the original Z information.
*/
BOOL CZImage::Overlay (RECT *prSrc, CZImage *pDestImage, POINT pDest, BOOL fOverlayZObject)
{
   RECT r;

   // dont go beyond source image
   if (prSrc) {
      r.left = max(0, prSrc->left);
      r.top = max(0, prSrc->top);
      r.right = min((int)m_dwWidth, prSrc->right);
      r.bottom = min((int)m_dwHeight, prSrc->bottom);
   }
   else {
      r.top = r.left = 0;
      r.right = (int) m_dwWidth;
      r.bottom = (int) m_dwHeight;
   }

   // don't go beyond dest image
   int iWidth, iHeight;
   iWidth = r.right - r.left;
   iHeight = r.bottom - r.top;
   if (pDest.x < 0) {
      iWidth += pDest.x;
      r.left -= pDest.x;
      pDest.x = 0;
   }
   if (pDest.y < 0) {
      iHeight += pDest.y;
      r.top -= pDest.y;
      pDest.y = 0;
   }
   if (pDest.x + iWidth > (int) pDestImage->m_dwWidth)
      iWidth = (int)pDestImage->m_dwWidth - pDest.x;
   if (pDest.y + iHeight > (int) pDestImage->m_dwHeight)
      iHeight = (int)pDestImage->m_dwHeight - pDest.y;
   if ((iWidth <= 0) || (iHeight <= 0))
      return TRUE;
   r.right = r.left + iWidth;
   r.bottom = r.top + iHeight;

   // copy
   int x, y;
   PZIMAGEPIXEL ps, pd;
   for (y = 0; y < iHeight; y++) {
      ps = Pixel(r.left, r.top + y);
      pd = pDestImage->Pixel (pDest.x, pDest.y + y);

      for (x = iWidth; x; x--, ps++, pd++) {
         if (fOverlayZObject)
            pd->fZ = ps->fZ;
      }
   }

   return TRUE;
}




/********************************************************************************
CZImage::MergeSelected - Merges this image (which is selected objects) in with
the main image (pDestImage), of unselected objects. In the merging: If the
selected object is in front, use that color. If it's behind, then blend the colors
with what's behind.

NOTE: This assumes that both images are the same size.

inputs
   CZImage      *pDestImage - To merge into
   BOOL        fBlendSelAbove - If TRUE then blend the selection above the objects
               behind, else blend normal to z
*/
void CZImage::MergeSelected (CZImage *pDestImage, BOOL fBlendSelAbove)
{
   PZIMAGEPIXEL pSrc, pDest;
   pSrc = Pixel(0,0);
   pDest = pDestImage->Pixel(0,0);
   DWORD dwNum, i;
   dwNum = Width() * Height();
   // BUGFIX - Was dwNum+1
   for (i = dwNum; i; i--, pSrc++, pDest++) {
      if (pSrc->fZ < pDest->fZ) {
         *pDest = *pSrc;
         continue;
      }
   }
}



/********************************************************************************
CZImage:: Merge - Merge the image from the source (this object) to the destination
   CZImage object. This code will do checking againt boundry conditions so as not
   to overwrite area. If the source Z height is < the destination height then the
   pixel is transferred (object and all), otherwise the destination pixel is left.

inputs
   RECT        *prSrc - Source rectangle. If NULL use the entire source
   CZImage      *pDestImage - To copy onto
   POINT       pDest - Where to copy the upper left corner onto
*/
BOOL CZImage::Merge (RECT *prSrc, CZImage *pDestImage, POINT pDest)
{
   RECT r;

   // dont go beyond source image
   if (prSrc) {
      r.left = max(0, prSrc->left);
      r.top = max(0, prSrc->top);
      r.right = min((int)m_dwWidth, prSrc->right);
      r.bottom = min((int)m_dwHeight, prSrc->bottom);
   }
   else {
      r.top = r.left = 0;
      r.right = (int) m_dwWidth;
      r.bottom = (int) m_dwHeight;
   }

   // don't go beyond dest image
   int iWidth, iHeight;
   iWidth = r.right - r.left;
   iHeight = r.bottom - r.top;
   if (pDest.x < 0) {
      iWidth += pDest.x;
      r.left -= pDest.x;
      pDest.x = 0;
   }
   if (pDest.y < 0) {
      iHeight += pDest.y;
      r.top -= pDest.y;
      pDest.y = 0;
   }
   if (pDest.x + iWidth > (int) pDestImage->m_dwWidth)
      iWidth = (int)pDestImage->m_dwWidth - pDest.x;
   if (pDest.y + iHeight > (int) pDestImage->m_dwHeight)
      iHeight = (int)pDestImage->m_dwHeight - pDest.y;
   if ((iWidth <= 0) || (iHeight <= 0))
      return TRUE;
   r.right = r.left + iWidth;
   r.bottom = r.top + iHeight;

   // copy
   int x, y;
   PZIMAGEPIXEL ps, pd;

   for (y = 0; y < iHeight; y++) {
      ps = Pixel(r.left, r.top + y);
      pd = pDestImage->Pixel (pDest.x, pDest.y + y);

      for (x = iWidth; x; x--, ps++, pd++) {
         if (ps->fZ < pd->fZ)
            *pd = *ps;
      }
   }

   return TRUE;
}


