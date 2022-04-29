/*****************************************************************************
CShadowBuf.cpp - For one shadow buffer

begun 27/5/02 by Mike Rozak.
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



/***************************************************************************************
CShadowBuf::Constructor and destructor
*/
CShadowBuf::CShadowBuf (void)
{
   m_pDirection.Zero();
   m_pEye.Zero();
   m_dwXY = 0;
   m_fMin = m_fMax = m_fScale = m_fPush = 0;
   m_apBoundary[0].Zero();
   m_apBoundary[1].Zero();
   m_fIsotropic = FALSE;
}

CShadowBuf::~CShadowBuf (void)
{
   // do nothing
}


/************************************************************************************
CShadowBuf::HasShadowBuf - Tests to see if the shadow buffer actually has a stencil.

returns
   BOOL - TRUE is is does, FALSE if not
*/
DWORD CShadowBuf::HasShadowBuf (void)
{
   return m_memPixels.m_dwAllocated && m_dwXY;
}

/***************************************************************************************
CShadowBuf::Clone - Clones the shadow buffer.

returns
   PCShadowBuf - Clone, or NULL if error
*/
PCShadowBuf CShadowBuf::Clone (void)
{
   PCShadowBuf pClone = new CShadowBuf;
   if (!pClone)
      return NULL;

   pClone->m_pDirection.Copy (&m_pDirection);
   pClone->m_pEye.Copy (&m_pEye);
   m_RM.CloneTo (&pClone->m_RM);
   pClone->m_dwXY = m_dwXY;
#ifdef FLOATSHADOWPIXELS
   pClone->m_fPush = m_fPush;
#endif
   pClone->m_fMin = m_fMin;
   pClone->m_fMax = m_fMax;
   pClone->m_fScale = m_fScale;
   memcpy (pClone->m_apBoundary, m_apBoundary, sizeof(m_apBoundary));
   pClone->m_fIsotropic = m_fIsotropic;
   memcpy (pClone->m_afCenterPixel, m_afCenterPixel, sizeof(m_afCenterPixel));
   memcpy (pClone->m_afScalePixel, m_afScalePixel, sizeof(m_afScalePixel));

   DWORD dwUnitSize;
#ifdef FLOATSHADOWPIXELS
   dwUnitSize = sizeof(float);
#else
   dwUnitSize = sizeof(WORD);
#endif
   DWORD dwNeed = m_dwXY * m_dwXY * dwUnitSize;
   if (m_memPixels.m_dwAllocated && pClone->m_memPixels.Required(dwNeed))
      memcpy (pClone->m_memPixels.p, m_memPixels.p, min(dwNeed, m_memPixels.m_dwAllocated));

   return pClone;
}

/***************************************************************************************
CShadowBuf::Init - Sets up a shadow buffer.

inputs
   PCRenderTraditional pRender - Renderer to use, so don't keep recreating threads and whatnot
   PCPoint     pEye - Where the eye is located
   PCPoint     pDirection - Direction looking
   PCPoint     pUp - Up vector. Must be perpendicular to pDirection. Can be NULL.
   PCWorldSocket     pWorld - World to drae
   fp          fIsotripicScale - If > 0, then shadow buffer will be isotropic.
               In that case, this is the number of meters across (in either direction)
               for the buffer. pEye is the location of the center of the isotropic space.

               if 0, then use a 90 degree field of view and perspective

               if < 0 use that as FOV

   DWORD       dwShowFlags - Which objects are to be drawn for this shadow buffer.
               If objects are hidden, dont want their shadows appearing either
   DWORD       dwPixels - Number of pixels across and up/down
   BOOL        fFinalRender - If TRUE then this is for the final render (full detail),
               else for workspace
   DWORD       dwShowOnly - If -1 then shadows generated from all objects. Else,
                  only generated from the given object number. (Used by polymesh
                  view to have object cast shadows on self.)
   PCPoint     papClipPlane - Array of 15 points, [5][3], for four clipping planes,
               and three points (in world space) defining each plane. The points go clockwise to create
               the normal, which points OUTSIDE the plane. The clipping planes
               should encompass what's visible by the camera. This can be NULL.
   fp          fLightMaximum - Maximum distance the light will travel in meters. If 0, the
               light is unrestricted. Only affects non-isotropic
   PCProgressSocket  pProgress - So can show progress
returns
   BOOL - TRUE if success
*/
BOOL CShadowBuf::Init (PCRenderTraditional pRender, PCPoint pEye, PCPoint pDirection, PCPoint pUp, PCWorldSocket pWorld,
   fp fIsotripicScale, DWORD dwShowFlags, DWORD dwPixels, BOOL fFinalRender, DWORD dwShowOnly,
   PCPoint papClipPlane, fp fLightMaximum, PCProgressSocket pProgress)
{
   m_pDirection.Copy (pDirection);
   m_pDirection.Normalize();
   m_pEye.Copy (pEye);
   m_dwXY = dwPixels;
   DWORD dwUnitSize;
#ifdef FLOATSHADOWPIXELS
   dwUnitSize = sizeof(float);
#else
   dwUnitSize = sizeof(WORD);
#endif
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   if (!m_memPixels.Required(m_dwXY * m_dwXY * dwUnitSize)) {
   	MALLOCOPT_RESTORE;
      return FALSE;
   }
	MALLOCOPT_RESTORE;
   m_fIsotropic = (fIsotripicScale > 0);
   if (m_fIsotropic)
      fLightMaximum = 0;

   CZImage   Image;
	MALLOCOPT_OKTOMALLOC;
   // CRenderTraditional Render;
   if (!Image.Init (m_dwXY, m_dwXY, RGB(0,0,0))) {
   	MALLOCOPT_RESTORE;
      return FALSE;
   }
	MALLOCOPT_RESTORE;
   pRender->CZImageSet (&Image);
   pRender->CWorldSet (pWorld);
   pRender->m_dwRenderShow = dwShowFlags;
   pRender->m_dwRenderModel = RENDERMODEL_SURFACETEXTURE; // BUGFIX - was RENDERMODEL_SURFACEMONO;
   if (pRender->m_pEffectOutline)
      pRender->m_pEffectOutline->m_dwLevel = 0;   // no outline
   pRender->m_fFinalRender = fFinalRender; // do this so trees aren't drawn as boxes 
      // BUGFIX - Final render option passed through init
   pRender->m_fForShadows = TRUE;  // so know that drawing for shadows
   // pRender->m_fDontDrawTransparent = TRUE; // dont draw transparent surfaces - let light pass through
      // BUGFIX - Dont need anymore since pass in Z info info
   pRender->m_dwShowOnly = dwShowOnly;   // BUGFIX - so shadows only calculated for the visible object
   DWORD i;

   // BUGFIX - Set low-detail flag so faster
   DWORD dwShadowsFlags = pRender->ShadowsFlagsGet();
   pRender->ShadowsFlagsSet (dwShadowsFlags | SF_LOWDETAIL);

   pRender->ClipPlaneClear();

   // fill in the clip planes
   RTCLIPPLANE rt;
   if (papClipPlane) {
      // loop
      CPoint pA, pB, pN;
      for (i = 0; i < 5; i++, papClipPlane += 3) {
         // find normal
         pA.Subtract (&papClipPlane[0], &papClipPlane[1]);
         pB.Subtract (&papClipPlane[2], &papClipPlane[1]);
         pN.CrossProd (&pB, &pA);

         if (pN.DotProd (pDirection) <= 0)
            continue;   // light coming thrigh this side, so eliminate clip plane

         // add the clip plane
         rt.dwID = 100 + i;
         rt.ap[0].Copy (&papClipPlane[2]);
         rt.ap[1].Copy (&papClipPlane[1]);
         rt.ap[2].Copy (&papClipPlane[0]);
         pRender->ClipPlaneSet (rt.dwID, &rt);
      } // i
   } // if clip plane

   // NOTE - If selection will draw the selection boxes in the shadow. May want to fix this at some point

   // figure out matrix that will look this way
   CMatrix  m, mInv;
   CPoint pX, pY, pZ;
   CPoint pCent, pRot;
   pY.Copy (&m_pDirection);
   if (pUp)
      pZ.Copy (pUp);
   else {
      pZ.Zero();
      pZ.p[2] = 1;
   }
   pX.CrossProd (&pY, &pZ);
   if (pX.Length() < CLOSE) {
      pZ.Zero();
      pZ.p[0] = 1;
      pX.CrossProd (&pY, &pZ);
      if (pX.Length() < CLOSE)
         return FALSE;
   }
   pX.Normalize();
   pZ.CrossProd (&pX, &pY);
   pZ.Normalize();
   m.RotationFromVectors (&pX, &pY, &pZ);
   m.Invert (&mInv);

   if (m_fIsotropic) {
      mInv.ToXYZLLT (&pCent, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
      pCent.Zero();  // in case it isn't already

      // find out translation
      pX.Copy (pEye);
      pX.p[3] = 1;
      pX.MultiplyLeft (&mInv);


      pRender->CameraFlat (&pCent, pRot.p[2], pRot.p[0], pRot.p[1], fIsotripicScale,
         -pX.p[0], -pX.p[2]);
   }
   else {
      fp fAngle;
      fAngle = PI / 2;
      if (fIsotripicScale < 0)
         fAngle = -fIsotripicScale;
      fAngle *= 1.05;//.15; // BUGFIX - was 1.01, but not enough to cover all cracks
         // BUGFIX - Was 1.1 but didnt cover all pixels because of a bad assumption bug
      fAngle = min(PI*.90, fAngle);

      m.ToXYZLLT (&pCent, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
      pRender->CameraPerspWalkthrough (pEye, pRot.p[2], pRot.p[0], pRot.p[1], fAngle);
         // NOTE: Doing a bit more than 90 degrees just to take are of border considerations
   }

   // add clip-plane if fLightMaximum and non-isotropic
   if (fLightMaximum) {
      pRender->CameraClipNearFar (CLOSE, fLightMaximum);
      // create a plane at fLightMaximum for clipping
      //memset (&rt, 0, sizeof(rt));
      //rt.dwID = 156; // random number
      //pRender->PixelToWorldSpace (0, 0, fLightMaximum, &rt.ap[0]);
      //pRender->PixelToWorldSpace (10, 0, fLightMaximum, &rt.ap[1]);
      //pRender->PixelToWorldSpace (10, 10, fLightMaximum, &rt.ap[2]);

      //pRender->ClipPlaneSet (rt.dwID, &rt);
   }
   else
      pRender->CameraClipNearFar ();

   // have it draw
   // BUGFIX - If render failed because cancelled then exit
   if (!pRender->Render (-1, NULL, pProgress)) {
      m_dwXY = FALSE;
      return FALSE;
   }

   // copy over the render matrix
   pRender->m_aRenderMatrix[0].CloneTo (&m_RM);

   // find min and max distances
   BOOL fFound;
   PZIMAGEPIXEL pip;
   DWORD x,y;
   fp   fMin, fMax;
   fFound = FALSE;
   pip = Image.Pixel(0,0);
   for (x = 0; x < m_dwXY * m_dwXY; x++, pip++) {
      if (pip->fZ == ZINFINITE)
         continue;
      if (fFound) {
         fMin = min(fMin, pip->fZ);
         fMax = max(fMax, pip->fZ);
      }
      else {
         fMin = fMax = pip->fZ;
         fFound = TRUE;
      }
   }
   if (fFound) {
      // if light maximum then limit distance
      if (fLightMaximum) {
         fMin = min(fMin, fLightMaximum);
         fMax = min(fMax, fLightMaximum);
      }
      m_fMin = (fp)fMin;
      m_fMax = (fp)fMax;
   }
   else {
      m_fMin = m_fMax = 0;
      m_apBoundary[0].Zero();
      m_apBoundary[1].Zero();
   }

#if 0 // def _DEBUG  // save image
   static int siImageNum = 0;
   char szTemp[64];
   sprintf (szTemp, "c:\\temp\\z%d.bmp", siImageNum++);
   CImage cSave;
   cSave.Init (m_dwXY, m_dwXY);
   pip = Image.Pixel(0,0);
   PIMAGEPIXEL pip2 = cSave.Pixel (0,0);
   for (x = 0; x < m_dwXY * m_dwXY; x++, pip++, pip2++) {
      fp f = (pip->fZ - m_fMin) / max(m_fMax - m_fMin, CLOSE) * (fp)0xffff;
      pip2->wRed = pip2->wGreen = pip2->wBlue = (WORD)f;
   } // x
   HWND hWnd = GetDesktopWindow ();
   HDC hDC = GetDC (hWnd);
   HBITMAP hBit = cSave.ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);
   BitmapSave (hBit, szTemp);
   DeleteObject (hBit);
#endif

   // fill in pixels
   CPoint pt;
#ifndef FLOATSHADOWPIXELS
   fp fScale, ft;
   fScale = m_fMax - m_fMin;
   if (fScale < CLOSE)
      fScale = CLOSE;
   fScale = (fp) 0xfffe / fScale;
#endif
   fFound = FALSE;

   // BUGFIX - if it's not isotropic include the current point in the bounding
   // volume
   if (!m_fIsotropic) {
      fFound = TRUE;
      pRender->PixelToWorldSpace (m_dwXY/2, m_dwXY/2, 0, &pt);
      m_apBoundary[0].Copy (&pt);
      m_apBoundary[1].Copy (&pt);
   }
   for (y = 0; y < m_dwXY; y++) for (x = 0; x < m_dwXY; x++) {
      pip = Image.Pixel(x,y);
#ifdef FLOATSHADOWPIXELS
      float *pfShad = ((float*)m_memPixels.p) + (y * m_dwXY + x);
#else
      WORD *pw = ((WORD*)m_memPixels.p) + (y * m_dwXY + x);
#endif

      if (pip->fZ == ZINFINITE) {
         // nothing there
#ifdef FLOATSHADOWPIXELS
         if (fLightMaximum)
            *pfShad = fLightMaximum;   // limit
         else
            *pfShad = ZINFINITE;
#else
         *pw = 0xffff;
#endif
         continue;
      }

#ifdef FLOATSHADOWPIXELS
      *pfShad = pip->fZ;
      if (fLightMaximum && (*pfShad > fLightMaximum))
         *pfShad = fLightMaximum;
#else
      ft = (pip->fZ - m_fMin) * fScale;   // BUGFIX - Changed from iZ to fZ
      ft = min(0xfffe, ft);
      ft = max(0, ft);
      *pw = (WORD)ft;
#endif

      // convert this to world space
      pRender->PixelToWorldSpace (x, y, *pfShad, &pt);   // BUGFIX - Was pip->fZ, but use *pfShad so can take fLightMaximum into account
      if (fFound) {
         m_apBoundary[0].Min (&pt);
         m_apBoundary[1].Max (&pt);
      }
      else {
         m_apBoundary[0].Copy (&pt);
         m_apBoundary[1].Copy (&pt);
         fFound = TRUE;
      }
   }

   for (i = 0; i < 2; i++) {
      m_afScalePixel[i] = pRender->m_afScalePixel[i];
      m_afCenterPixel[i] = pRender->m_afCenterPixel[i];
   }

   // enlarge the boundary just a bit
   CPoint pDif;
   fp fBuffer;
   if (fIsotripicScale > 0)
      fBuffer = fIsotripicScale;
   else {
      pDif.Subtract (&m_apBoundary[0], pEye);
      fBuffer = pDif.Length();
      pDif.Subtract (&m_apBoundary[1], pEye);
      fBuffer += pDif.Length();
   }
   fBuffer /= 100.0;
   pDif.Subtract (&m_apBoundary[1], &m_apBoundary[0]);
   pDif.Scale (.01);
   pDif.p[0] += fBuffer;
   pDif.p[1] += fBuffer;
   pDif.p[2] += fBuffer;
   m_apBoundary[1].Add (&pDif);
   m_apBoundary[0].Subtract (&pDif);

   // BUGFIX - If it's isotropic, push the min and max back a bit to account
   // for potential error
   // NOTE: I seem to have to multiply x 5 to get ok shadows on land. I wouldnt
   // expect that high of a number
   m_fPush = 0;
   if (m_fIsotropic) {
      m_fPush = fIsotripicScale / (fp)dwPixels * 2.0;
         // BUGFIX - Lowered from *5 to *2... * 5.0;// / 10.0; // allow 10th of a pixel error
      m_fMin += m_fPush;
      m_fMax += m_fPush;
   }

   m_fScale = (m_fMax - m_fMin) / (fp)0xfffe;

#if 0 // def _DEBUG  // to test save image
   CImage ImageTest;
   ImageTest.Init (1024, 1024);
   TestToImage (&ImageTest);
   HDC hDC = GetDC (NULL);
   HBITMAP hBmp = ImageTest.ToBitmap (hDC);
   ReleaseDC (NULL, hDC);
   BitmapSave (hBmp, "c:\\temp\\Shadows.bmp");
   DeleteObject (hBmp);
#endif

   // done
   return TRUE;
}


/***************************************************************************************
CShadowBuf::ShadowTranform - Takes a point and transforms it according
the the shadow buffer conversions. Use as an optimization for rendering

inputs
   PCPoint        pWorld - Location in world space
   PCPoint        pShadow - Transformed to location in shadow space, to be
                  sent to ::ShadowTest()
returns
   none
*/
void CShadowBuf::ShadowTransform (const PCPoint pWorld, PCPoint pShadow)
{
   m_RM.Transform (pWorld, pShadow);
}


/***************************************************************************************
CShadowBuf::ShadowTest - Given a point in world space, this sees if it shows up in this shadow
buffer. If it does then it fills in pfDist, pdwX, and pdwY

inputs
   CPoint         pTest - Point in world space (or might already be transformed)
   BOOL           fIsTransformed - If TRUE, then pTest has already been transformed by
                  m_RM from CShadowBuf::ShadowTransform()
   fp             *pfDist - If in this buffer, filled with the distance (from shadow buffer).
                  Used to compare against other points in the shadow buffer.
   fp             *pfX, *pfY - If in this buffer, filled with the X and Y coordinates
returns
   BOOL - True if found the point
*/
BOOL CShadowBuf::ShadowTest (const PCPoint pTest, BOOL fIsTransformed, fp *pfDist, fp *pfX, fp *pfY)
{
   // apply the matrix
   CPoint pScreen;
   if (fIsTransformed)
      pScreen.Copy (pTest);
   else
      m_RM.Transform (pTest, &pScreen);

#if 0 // def _DEBUG
   static DWORD dwCount = 0;
   WCHAR szTemp[128];
   if (dwCount < 100) {
      swprintf (szTemp, L"\r\nShadowTest = %g, %g, %g, %g", (double)pScreen.p[0], (double)pScreen.p[1], (double)pScreen.p[2], (double)pScreen.p[3]);
      OutputDebugStringW (szTemp);
      dwCount++;
   }
#endif

   // if behind camera then return false
   if (pScreen.p[3] < CLOSE)
      return FALSE;
   fp fInv = 1.0 / pScreen.p[3];
   pScreen.p[0] = m_afCenterPixel[0] + ((pScreen.p[0] * fInv) * m_afScalePixel[0]);
   pScreen.p[1] = m_afCenterPixel[1] + ((pScreen.p[1] * fInv) * m_afScalePixel[1]);
   //pScreen.p[2] = -pScreen.p[2];   // since zbuffer is as a positive value

   if ((pScreen.p[0] < 0) || (pScreen.p[0] >= (fp) m_dwXY) || (pScreen.p[1] < 0) || (pScreen.p[1] >= (fp) m_dwXY))
      return FALSE;

   *pfDist = -pScreen.p[2];
   *pfX = pScreen.p[0];
   *pfY = pScreen.p[1];

   return TRUE;
}

/***************************************************************************************
CShadowBuf::TestToImage - Test function that transfers the depth information onto an image
so can see how well it worked.

inputs
   PCZImage     pFill - To fill
returns
   none
*/
void CShadowBuf::TestToImage (PCImage pFill)
{
   DWORD x, y, dwMaxX, dwMaxY;
   dwMaxX = min(m_dwXY, pFill->Width());
   dwMaxY = min(m_dwXY, pFill->Height());

   for (y = 0; y < dwMaxY; y++) for (x = 0; x < dwMaxX; x++) {
      PIMAGEPIXEL pip = pFill->Pixel(x,y);
#ifdef FLOATSHADOWPIXELS
      float fShad = *(((float*) m_memPixels.p) + (y * m_dwXY + x));
      if (fShad == ZINFINITE)
         fShad = 0xffff;
      else {
         fp fScale, ft;
         fScale = m_fMax - m_fMin;
         if (fScale < CLOSE)
            fScale = CLOSE;
         fScale = (fp) 0xfffe / fScale;
         ft = (fShad - m_fMin) * fScale;   // BUGFIX - Changed from iZ to fZ, // BUGFIX - Need to use fShad
         ft = min(0xfffe, ft);
         ft = max(0, ft);
         fShad = ft;
      }
      pip->wRed = pip->wGreen = pip->wBlue = (0xffff - (WORD)fShad);
#else
      WORD *pw = ((WORD*) m_memPixels.p) + (y * m_dwXY + x);
      pip->wRed = pip->wGreen = pip->wBlue = (0xffff - *pw);
#endif
   }
}

/***************************************************************************************
CShadowBuf::PixelInterp - Gets the value of a pixel, but interpolates the four surrounding
points. Returns fp.

inputs
   fp          fX,fY - X and Y - assume they'r within range
   BOOL        fFast - If TRUE then rounds the point, gets the surrounding points, and
               returns the LOWEST value.
returns
   fp - Depth
*/
#define FASTBORDER      1
fp CShadowBuf::PixelInterp (fp fX, fp fY, BOOL fFast)
{
   if (fFast) {
      int iX = (int) floor(fX + 0.5);
      if ((iX < -FASTBORDER) || (iX >= (int)m_dwXY + FASTBORDER))
         return 1000000;  // out of range
      int iY = (int) floor(fY + 0.5);
      if ((iY < -FASTBORDER) || (iY >= (int)m_dwXY + FASTBORDER))
         return 1000000;  // out of range

      int iXMin = max(iX - FASTBORDER, 0);
      int iYMin = max(iY - FASTBORDER, 0);
      int iXMax = min(iX + FASTBORDER, (int)m_dwXY-1);
      int iYMax = min(iY + FASTBORDER, (int)m_dwXY-1);
      int iXCur, iYCur;
#ifdef FLOATSHADOWPIXELS
      float *pfShad;
      float fMin = m_fMin;
#else
      WORD *paw;
      WORD wMin = 0; // BUGFIX - Start with 0, and find the furthest point
#endif
      for (iYCur = iYMin; iYCur <= iYMax; iYCur++) {
#ifdef FLOATSHADOWPIXELS
         pfShad = ((float*)m_memPixels.p) + ((DWORD)iXMin + (DWORD)iYCur * m_dwXY);
         for (iXCur = iXMin; iXCur <= iXMax; iXCur++, pfShad++)
            fMin = max(fMin, *pfShad);
#else
         paw = ((WORD*)m_memPixels.p) + ((DWORD)iXMin + (DWORD)iYCur * m_dwXY);
         for (iXCur = iXMin; iXCur <= iXMax; iXCur++, paw++)
            wMin = max(wMin, *paw);
#endif
      } // iYCur

#ifdef FLOATSHADOWPIXELS
      return fMin + m_fPush;
#else
      if (wMin == 0xffff)
         return 1000000;
      return (fp) (fp)wMin * m_fScale + m_fMin;
#endif
   }

   // else
   if ((fX < 0) || (fY < 0) || (fX >= m_dwXY) || (fY >= m_dwXY))
      return 1000000;   // out of range

   // pixel to left, and above
   DWORD dwX, dwY;
   dwX = (DWORD) fX;
   dwY = (DWORD) fY;
   if ((dwX+1 >= m_dwXY) || (dwY+1 >= m_dwXY)) {
      // cant interpolate
      return Pixel (dwX, dwY);
   }

   fp af[2][2];
   DWORD x,y;
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++)
      af[x][y] = Pixel(dwX+x, dwY+y);

   // so fX from 0..1
   fX -= dwX;
   fY -= dwY;

   fp fOneMinusX = 1.0 - fX;
   return (1.0 - fY) * (fX * af[1][0] + fOneMinusX * af[0][0]) +
      fY * (fX * af[1][1] + fOneMinusX * af[0][1]);
}


