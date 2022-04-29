/*************************************************************************************
CRender360.cpp - Code for doing a 360 degree render

begun 19/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "resource.h"


// PINGPONGTHREADINFO
typedef struct {
   PCRender360       pThis;      // this
   DWORD             dwThread;   // thread number
   DWORD             dwWorldChanged;   // value passed in
} PINGPONGTHREADINFO, *PPINGPONGTHREADINFO;

/***********************************************************************************
CRender360::Constructor and destructor
*/
CRender360::CRender360 (void)
{
   memset (m_apRender, 0, sizeof(m_apRender));
   memset (m_apRenderTrad, 0, sizeof(m_apRenderTrad));
   m_pRTShadow = NULL;
   m_pProgress = NULL;
   m_pCamera.Zero();
   m_pImage = NULL;
   m_pFImage = NULL;
   m_pIS = NULL;
   m_f360Long = m_f360Lat = 0;
   m_dwShadowsFlags = 0;
   m_fShadowsLimit = 0;
   m_fHalfDetail = FALSE;
   m_pMem360Calc = &m_mem360Calc;


   m_fWantToCancel = FALSE;
   m_f360ThreadsShutDown = FALSE;

   DWORD i;
   for (i = 0; i < MAX360PINGPONG; i++) {
      m_ahEventToThread[i] = NULL;
      m_ahEventFromThread[i] = NULL;
      m_ahEventPingPongSafe[i] = NULL;
      m_af360PingPongRet[i] = FALSE;
      m_adwPingPongProgress[i] = 0;
   }
}

CRender360::~CRender360 (void)
{
   // do nothing for now
}



/***********************************************************************************
CRender360::Init - Initializes the 360 renderer to use another renderer (such as
a traditional or ray tracer.

inputs
   PCRenderSuper        *papRender - Renderers to use for individual frames. This must
                        remain valid while the CREnder360 object is around.
                        Need MAX360PINGPONG in this list
                        This rendeer should have been initialized
   PCRenderTraditional  *papRenderTrad - Optional. If this is for traditional render
                        then use this so that CloneLightTo() can be called to
                        save processing power.
   PCRenderTraditional  pRTShadow - If this is NOT NULL, and using papRenderTrad,
                        then their pRTShadow shadow's will be clonedto papRenderTrad.
                        When done rendering, the shadows in papRenderTrad will
                        be cloned BACK to pRTShadow
returns
   BOOL - TRUE if successs
*/
BOOL CRender360::Init (PCRenderSuper *papRender, PCRenderTraditional *papRenderTrad,
                       PCRenderTraditional pRTShadow)
{
   DWORD i;
   for (i = 0; i < MAX360PINGPONG; i++)
      if (m_apRender[i] || m_apRenderTrad[i])
         return FALSE;

   for (i = 0; i < MAX360PINGPONG; i++)
      m_apRender[i] = papRender[i];

   for (i = 0; i < MAX360PINGPONG; i++)
      m_apRenderTrad[i] = papRenderTrad ? papRenderTrad[i] : NULL;

   m_pRTShadow = pRTShadow;

   return TRUE;
}



/***********************************************************************************
CRender360::ImageQueryFloat - Standard API from CRenderSuper
*/
DWORD CRender360::ImageQueryFloat (void)
{
   // just default to first renderer
   return m_apRender[0]->ImageQueryFloat();
}


/***********************************************************************************
CRender360::CImageSet - Standard API from CRenderSuper. NOTE: The image must be
2:1 aspect ratio, and the width a multiple of 4, height a multiple of 2.
*/
BOOL CRender360::CImageSet (PCImage pImage)
{
   if (m_apRender[0]->ImageQueryFloat() >= 2)
      return FALSE;

   // BUGFIX - Width must be multiple of 8, height of 4
   if ((pImage->Width() != pImage->Height()*2) || (pImage->Width() & 0x07) ||
      (pImage->Height() & 0x03))
      return FALSE;  // wrong size

   m_pImage = pImage;
   return TRUE;
}

/***********************************************************************************
CRender360::CFImageSet - Standard API from CRenderSuper. NOTE: The image must be
2:1 aspect ratio, and the width a multiple of 4, height a multiple of 2.
*/
BOOL CRender360::CFImageSet (PCFImage pImage)
{
   if (m_apRender[0]->ImageQueryFloat() == 0)
      return FALSE;

   if ((pImage->Width() != pImage->Height()*2) || (pImage->Width() & 0x07) ||
      (pImage->Height() & 0x03))
      return FALSE;  // wrong size

   m_pFImage = pImage;
   return TRUE;
}

/***********************************************************************************
CRender360::CImageStoreSet - Standard API from CRenderSuper. NOTE: The image must be
2:1 aspect ratio, and the width a multiple of 4, height a multiple of 2.
*/
BOOL CRender360::CImageStoreSet (PCImageStore pImage)
{
   if ((pImage->Width() != pImage->Height()*2) || (pImage->Width() & 0x07) ||
      (pImage->Height() & 0x03))
      return FALSE;  // wrong size

   m_pIS = pImage;
   return TRUE;
}


/***********************************************************************************
CRender360::CWorldSet - Standard CRenderSuper call
*/
void CRender360::CWorldSet (PCWorldSocket pWorld)
{
   DWORD i;
   for (i = 0; i < MAX360PINGPONG; i++)
      m_apRender[i]->CWorldSet (pWorld);
}

/***********************************************************************************
CRender360::CameraFlat - Standard CRenderSuperCall.

This doesn't really work with 360 images. Treats it as CameraPerspWallkthrough
*/
void CRender360::CameraFlat (PCPoint pCenter, fp fLongitude, fp fTiltX, fp fTiltY, fp fScale,
                                    fp fTransX, fp fTransY)
{
   CameraPerspWalkthrough (pCenter);
}

/***********************************************************************************
CRender360::CameraFlatGet - Standard CRenderSuperCall.

This doesn't really work with 360 images.
*/
void CRender360::CameraFlatGet (PCPoint pCenter, fp *pfLongitude, fp *pfTiltX, fp *pfTiltY, fp *pfScale, fp *pfTransX, fp *pfTransY)
{
   *pfLongitude = *pfTiltX = *pfTiltY = 0;
   *pfTransX = *pfTransY = 0;
   pCenter->Zero();
}

/***********************************************************************************
CRender360::CameraPerspWalkthrough - Standard CRenderSuperCall.
*/
void CRender360::CameraPerspWalkthrough (PCPoint pLookFrom, fp fLongitude, fp fLatitude, fp fTilt, fp fFOV)
{
   m_pCamera.Copy (pLookFrom);
}

/***********************************************************************************
CRender360::CameraPerspWalkthroughGet - Standard CRenderSuperCall.
*/
void CRender360::CameraPerspWalkthroughGet (PCPoint pLookFrom, fp *pfLongitude, fp *pfLatitude, fp *pfTilt, fp *pfFOV)
{
   pLookFrom->Copy (&m_pCamera);

   *pfLongitude = *pfLatitude = *pfTilt = *pfFOV = 0;
}

/***********************************************************************************
CRender360::CameraPerspObject - Standard CRenderSuperCall.

This gets translated to CameraPerspWalktrhough.
*/
void CRender360::CameraPerspObject (PCPoint pTranslate, PCPoint pCenter, fp fRotateZ, fp fRotateX, fp fRotateY, fp fFOV)
{
   CMatrix m, mInv, mTrans;
   mTrans.Translation (pCenter->p[0], pCenter->p[1], pCenter->p[2]);
   // p2.Add (&pCenter);
   m.FromXYZLLT (pTranslate, fRotateZ, fRotateX, fRotateY);
   m.Invert4 (&mInv);
   mInv.MultiplyRight (&mTrans);

   m_pCamera.Zero();
   m_pCamera.MultiplyLeft (&mInv);
}


/***********************************************************************************
CRender360::CameraPerspObjectGet - Standard CRenderSuperCall.

This gets translated to CameraPerspWalktrhough.
*/
void CRender360::CameraPerspObjectGet (PCPoint pTranslate, PCPoint pCenter, fp *pfRotateZ, fp *pfRotateX, fp *pfRotateY, fp *pfFOV)
{
   pTranslate->Copy (&m_pCamera);
   pTranslate->Scale (-1);

   pCenter->Zero();
   *pfRotateX = *pfRotateY = *pfRotateZ = 0;
}

/***********************************************************************************
CRender360::CameraPerspObjectGet - Standard CRenderSuperCall.
*/
DWORD CRender360::CameraModelGet (void)
{
   return CAMERAMODEL_PERSPWALKTHROUGH;
}


/***********************************************************************************
CRender360::ExposureGet - Standard CRenderSuper call
*/
fp CRender360::ExposureGet (void)
{
   return m_apRender[0]->ExposureGet ();
}

/***********************************************************************************
CRender360::ExposureSet - Standard CRenderSuper call
*/
void CRender360::ExposureSet (fp fExposure)
{
   DWORD i;
   for (i = 0; i < MAX360PINGPONG; i++)
      m_apRender[i]->ExposureSet (fExposure);
}

/***********************************************************************************
CRender360::BackgroundGet - Standard CRenderSuper call
*/
COLORREF CRender360::BackgroundGet (void)
{
   return m_apRender[0]->BackgroundGet();
}


/***********************************************************************************
CRender360::BackgroundSet - Standard CRenderSuper call
*/
void CRender360::BackgroundSet (COLORREF cBack)
{
   DWORD i;
   for (i = 0; i < MAX360PINGPONG; i++)
      m_apRender[i]->BackgroundSet (cBack);
}


/***********************************************************************************
CRender360::AmbientExtraGet - Standard CRenderSuper call
*/
DWORD CRender360::AmbientExtraGet (void)
{
   return m_apRender[0]->AmbientExtraGet ();
}


/***********************************************************************************
CRender360::AmbientExtraSet - Standard CRenderSuper call
*/
void CRender360::AmbientExtraSet (DWORD dwAmbient)
{
   DWORD i;
   for (i = 0; i < MAX360PINGPONG; i++)
      m_apRender[i]->AmbientExtraSet(dwAmbient);
}

/***********************************************************************************
CRender360::RenderShowGet - Standard CRenderSuper call
*/
DWORD CRender360::RenderShowGet (void)
{
   return m_apRender[0]->RenderShowGet();
}


/***********************************************************************************
CRender360::RenderShowGet - Standard CRenderSuper call
*/
void CRender360::RenderShowSet (DWORD dwShow)
{
   DWORD i;
   for (i = 0; i < MAX360PINGPONG; i++)
      m_apRender[i]->RenderShowSet (dwShow);
}




/***********************************************************************************
CRender360::PixelToZ - Standard CRenderSuper call
*/
fp CRender360::PixelToZ (DWORD dwX, DWORD dwY)
{
   // NOTE: Not tested
   if (m_pImage) {
      if ((dwX >= m_pImage->Width()) || (dwY >= m_pImage->Height()))
         return 0;
      return m_pImage->Pixel(dwX, dwY)->fZ;
   }
   else if (m_pFImage) {
      if ((dwX >= m_pFImage->Width()) || (dwY >= m_pFImage->Height()))
         return 0;
      return m_pFImage->Pixel(dwX, dwY)->fZ;
   }
   else
      return 0;
}


/***********************************************************************************
CRender360::PixelToViewerSpace - Standard CRenderSuper call
*/
void CRender360::PixelToViewerSpace (fp dwX, fp dwY, fp fZ, PCPoint p)
{
   // NOTE: Not tested
   DWORD dwWidth = m_pImage ? m_pImage->Width() : (m_pFImage ? m_pFImage->Width() : m_pIS->Width());
   DWORD dwHeight = m_pImage ? m_pImage->Height() : (m_pFImage ? m_pFImage->Height() : m_pIS->Height());

   fp fLat = (fp)dwX / (fp)dwWidth * 2.0 * PI;
   fp fLong = ((fp)dwY / (fp)dwHeight - 0.5) * PI;

   p->p[0] = sin (fLat) * cos(fLong) * fZ;
   p->p[1] = cos (fLat) * cos(fLong) * fZ;
   p->p[2] = sin(fLong) * fZ;
}

/***********************************************************************************
CRender360::PixelToWorldSpace - Standard CRenderSuper call
*/
void CRender360::PixelToWorldSpace (fp dwX, fp dwY, fp fZ, PCPoint p)
{
   PixelToViewerSpace (dwX, dwY, fZ, p);

   p->Add (&m_pCamera);
}


/***********************************************************************************
CRender360::PixelToWorldSpace - Standard CRenderSuper call
*/
BOOL CRender360::WorldSpaceToPixel (PCPoint pWorld, fp *pfX, fp *pfY, fp *pfZ)
{
   // NOTE: Not tested
   CPoint p;
   p.Subtract (pWorld, &m_pCamera);

   // longitude and latitude
   fp fLong = atan2(p.p[1], p.p[0]);
   fp fLen = p.Length();
   fp fLat = fLen ? asin(p.p[2] / fLen) : 0;

   // convert
   DWORD dwWidth = m_pImage ? m_pImage->Width() : (m_pFImage ? m_pFImage->Width() : m_pIS->Width());
   DWORD dwHeight = m_pImage ? m_pImage->Height() : (m_pFImage ? m_pFImage->Height() : m_pIS->Height());
   //DWORD dwWidth = m_pImage ? m_pImage->Width() : m_pFImage->Width();
   //DWORD dwHeight = m_pImage ? m_pImage->Height() : m_pFImage->Height();
   *pfX = fLong / (2.0 * PI) * (fp)dwWidth;
   *pfY = (0.5 - fLat / PI) * (fp)dwHeight;

   if (pfZ)
      *pfZ = PixelToZ (min((DWORD)*pfX, dwWidth-1), min((DWORD)*pfY, dwHeight-1));

   return TRUE;
}




/***********************************************************************************
CRender360::Push - Standard API from CProgressSocket
*/
BOOL CRender360::Push (float fMin, float fMax)
{
   return TRUE;
   // BUGFIX - Do nothing because will do differently for multithreaded
   //if (m_pProgress)
   //   return m_pProgress->Push(fMin, fMax);
   //else
   //   return FALSE;
}

/***********************************************************************************
CRender360::Pop - Standard API from CProgressSocket
*/
BOOL CRender360::Pop (void)
{
   return TRUE;
   // BUGFIX - Do nothing because will do differently for multithreaded
   //if (m_pProgress)
   //   return m_pProgress->Pop();
   //else
   //   return FALSE;
}


/***********************************************************************************
CRender360::Update - Standard API from CProgressSocket
*/
int CRender360::Update (float fProgress)
{
   return 0;
   // BUGFIX - Do nothing because will do differently for multithreaded
   //if (m_pProgress)
   //   return m_pProgress->Update(fProgress);
   //else
   //   return 0;
}


/***********************************************************************************
CRender360::WantToCancel - Standard API from CProgressSocket
*/
BOOL CRender360::WantToCancel (void)
{
   // BUGFIX - Checke m_fWantToCancel so thread safe
   return m_fWantToCancel;

   //if (m_pProgress)
   //   return m_pProgress->WantToCancel();
   //else
   //   return FALSE;
}


/***********************************************************************************
CRender360::CanRedraw - Standard API from CProgressSocket
*/
void CRender360::CanRedraw (void)
{
   // do nothing
}


/*******************************************************************************
CRender360::ShadowsFlagsSet - Standard API
*/
void CRender360::ShadowsFlagsSet (DWORD dwFlags)
{
   m_dwShadowsFlags = dwFlags;
}



/*******************************************************************************
CRender360::ShadowsFlagsGet - Standard API
*/
DWORD CRender360::ShadowsFlagsGet (void)
{
   return m_dwShadowsFlags;
}



/*******************************************************************************
Render360PingPongThreadProc - Called for each thread.
*/
static DWORD WINAPI Render360PingPongThreadProc(LPVOID lpParameter)
{
   PPINGPONGTHREADINFO pInfo = (PPINGPONGTHREADINFO) lpParameter;

   return pInfo->pThis->PingPongThreadProc (pInfo->dwThread, pInfo->dwWorldChanged);
}


/***********************************************************************************
CRender360::PingPongThreadProc - Thread proc for ping-pong rendering

inputs
   DWORD          dwPing - Thread ping-poing number
   DWORD          dwWorldChanged - World changed intiial param
returns
   DWORD - Standard thred
*/
DWORD CRender360::PingPongThreadProc (DWORD dwPing, DWORD dwWorldChanged)
{
   while (TRUE) {
      // wait for signal
      WaitForSingleObject (m_ahEventToThread[dwPing], INFINITE);

      // if want to shut down then exit
      if (m_f360ThreadsShutDown)
         return 0;

      // else, would have been asked to render
      m_af360PingPongRet[dwPing] = m_apRender[dwPing]->Render (dwWorldChanged, m_ahEventPingPongSafe[dwPing], this);

      // set worldchanged to 0
      dwWorldChanged = 0;

      // note that done rendering
      SetEvent (m_ahEventFromThread[dwPing]);
   } // repeat

   return 0;   // won't get here
}


// MEM360HEADER - Structure for the header
typedef struct {
   DWORD          dwWidth;       // width that's there
   DWORD          dwHeight;      // height that's present
} MEM360HEADER, *PMEM360HEADER;

/***********************************************************************************
CRender360::Render - Standard API from CRenderSuper

This renders in 360 degrees
*/
BOOL CRender360::Render (DWORD dwWorldChanged, HANDLE hEventSafe, PCProgressSocket pProgress)
{
   BOOL fInterpPixels = !(m_dwShadowsFlags & SF_NOSUPERSAMPLE);
   BOOL fRet = FALSE;
   BOOL fHaveCopiedFromRTShadows = FALSE;
   PCImage apImageTemp[MAX360PINGPONG];
   memset (apImageTemp, 0, sizeof(apImageTemp));
   PCFImage apFImageTemp[MAX360PINGPONG];
   memset (apFImageTemp, 0, sizeof(apFImageTemp));
   m_pProgress = pProgress;
   CMem memDrawnOn;
   PBYTE pbDrawnOn = NULL;
   PBYTE pbDrawingOn = NULL;
   DWORD i;
   for (i = 0; i < MAX360PINGPONG; i++)
      if (!m_apRender[i])
         goto done;
   if (!m_pImage && !m_pFImage && !m_pIS)
      goto done;  // error

   m_fWantToCancel = FALSE;

   // if this has a traditional renderer then make sure worldchanged is 0
   // so that can CloneLightsTo() and not lose them
   if (m_apRenderTrad[0])
      dwWorldChanged = 0;

   // find out how many ping pong buffers want
   DWORD dwProcessors = HowManyProcessors();
   DWORD dwNumPing;
   if (dwProcessors >= 4) {
      fInterpPixels = TRUE;   // BUGFIX - Always interp pixels if have plenty of cores
      dwNumPing = 3;
   }
   else if (dwProcessors >= 2)
      dwNumPing = 2;
   else
      dwNumPing = 1;
   dwNumPing = min(dwNumPing, MAX360PINGPONG);

   // create the temporary memory to render to
   //DWORD dwWidth = m_pImage ? m_pImage->Width() : m_pFImage->Width();
   //DWORD dwHeight = m_pImage ? m_pImage->Height() : m_pFImage->Height();
   DWORD dwWidth = m_pImage ? m_pImage->Width() : (m_pFImage ? m_pFImage->Width() : m_pIS->Width());
   DWORD dwHeight = m_pImage ? m_pImage->Height() : (m_pFImage ? m_pFImage->Height() : m_pIS->Height());
   DWORD dwSquareFinal = dwWidth / 4;
   BOOL fEvenFinal = !(dwSquareFinal % 2);
   if (m_pImage || (m_pIS && (m_apRender[0]->ImageQueryFloat() < 2) )) {
      for (i = 0; i < MAX360PINGPONG; i++) {
         apImageTemp[i] = new CImage;
         if (!apImageTemp[i])
            goto done;
      } // i

      if (m_pImage) {
         // wipe the background to black
         m_pImage->Clear (RGB(0,0,0));

         // set flag to indicate it's 360
         m_pImage->m_f360 = TRUE;
      }
   }
   else {   // using floating point
      for (i = 0; i < MAX360PINGPONG; i++) {
         apFImageTemp[i] = new CFImage;
         if (!apFImageTemp[i])
            goto done;
      } // i

      if (m_pFImage) {
         // wipe the background to black
         m_pFImage->Clear (RGB(0,0,0));

         // set flag to indicate it's 360
         m_pFImage->m_f360 = TRUE;
      }
   }

   if (m_pIS) {
      m_pIS->m_dwStretch = 4; // 360 view
      memset (m_pIS->Pixel(0,0), 0, m_pIS->Width()* m_pIS->Height()*3);
   }

   // keep track of which pixels have been drawn on to
   if (m_dwShadowsFlags & SF_TWOPASS360) {
      if (memDrawnOn.Required (dwWidth * dwHeight))
         pbDrawnOn = (PBYTE)memDrawnOn.p;
      if (pbDrawnOn)
         memset (pbDrawnOn, 1, dwWidth * dwHeight);
   }

   // allocate memory for the interpolation information...
   DWORD dwInterpWidth = dwWidth/8 + 1;
   DWORD dwInterpWidthTopBottom = (dwWidth+7)/8;   // so don't cross over
   DWORD dwInterpHeight = dwHeight / 2;
   DWORD dwNeed = sizeof (MEM360HEADER) + dwInterpWidth * dwInterpHeight * sizeof(CPoint)*2;
   DWORD dwX, dwY;
   fp fLong, fLat;
   PCPoint pCur;

   // may already be calculated
   PMEM360HEADER pHeader = (PMEM360HEADER)m_pMem360Calc->p;
   PCPoint paInterp, paInterpTop;
   if ( (m_pMem360Calc->m_dwCurPosn >= dwNeed) && (pHeader->dwWidth == dwInterpWidth) &&
      (pHeader->dwHeight == dwInterpHeight) ) {

         // already exists, so use that
         paInterp = (PCPoint)(pHeader+1);
         paInterpTop = paInterp + (dwInterpWidth * dwInterpHeight);
   }
   else {
      // nothing there, so calc
      if (!m_pMem360Calc->Required (dwNeed))
         goto done;

      pHeader = (PMEM360HEADER)m_pMem360Calc->p;
      pHeader->dwWidth = dwInterpWidth;
      pHeader->dwHeight = dwInterpHeight;
      m_pMem360Calc->m_dwCurPosn = dwNeed;

      paInterp = (PCPoint)(pHeader+1);
      paInterpTop = paInterp + (dwInterpWidth * dwInterpHeight);

      PCPoint pCurTop;
      fp fSinLat, fCosLat;
      fp fOffsetFinal = (fEvenFinal ? 0.5 : 0);

      // before start, calculate remap
      for (pCur = paInterp, pCurTop = paInterpTop, dwY = 0; dwY < dwInterpHeight; dwY++) {
         fLat = ((fp)dwY + fOffsetFinal) / (fp)dwInterpHeight * PI / 2.0;
         fSinLat = sin(fLat);
         fCosLat = cos(fLat);

         for (dwX = 0; dwX < dwInterpWidth; dwX++, pCur++, pCurTop++) {
            fLong = ((fp)dwX + fOffsetFinal) / (fp)dwWidth * 2.0 * PI;

            pCur->p[0] = sin(fLong) * fCosLat;
            pCur->p[1] = cos(fLong) * fCosLat;
            pCur->p[2] = fSinLat;
            pCurTop->Copy (pCur);

            // BUGFIX - Don't take length bevause always 1
            // figure out the length
            // fp fLen = pCur->Length();
            // fLen = 1.0 / fLen;   // so becomes a scale for Z

            fp fInv, fLen;

            // do front
            // intersect with plane at y=1
            fLen = pCur->p[1] ? (1.0 / pCur->p[1]) : 1000000000;
            fInv = fLen * (fp)dwSquareFinal / 2.0;
            pCur->p[0] *= fInv;
            pCur->p[1] = pCur->p[2] * fInv;
            pCur->p[2] = fLen;
            // determine if this is in-bounds
            pCur->p[3] = (pCur->p[0] < (fp)(dwSquareFinal+1)/2.0) && (pCur->p[1] < (fp)(dwSquareFinal+1)/2.0);

            // do top
            // intersect with plane at z=1
            fLen = pCurTop->p[2] ? (1.0 / pCurTop->p[2]) : 1000000000;
            fInv = fLen * (fp)dwSquareFinal / 2.0;
            pCurTop->p[0] *= fInv;
            pCurTop->p[1] *= fInv;
            pCurTop->p[2] = fLen;   // NOTE: This number seems to be too high, but doesn't affect anything
            // determine if this is in-bounds
            pCurTop->p[3] = (pCurTop->p[0] < (fp)(dwSquareFinal+1)/2.0) && (pCurTop->p[1] < (fp)(dwSquareFinal+1)/2.0);
         } // dwX
      } // dwY

   } // precalc

   // create the threads
   HANDLE ahThread[MAX360PINGPONG];
   PINGPONGTHREADINFO afInfo[MAX360PINGPONG];
   DWORD adwAngle[MAX360PINGPONG];
   BOOL afOnTopBottom[MAX360PINGPONG];
   BOOL afAngleEqualsFour[MAX360PINGPONG];
   BOOL afAngleEqualsFive[MAX360PINGPONG];
   DWORD dwID;
   m_f360ThreadsShutDown = FALSE;
   for (i = 0; i < dwNumPing; i++) {
      m_ahEventToThread[i] = CreateEvent (NULL, FALSE, FALSE, NULL);
      m_ahEventFromThread[i] = CreateEvent (NULL, FALSE, FALSE, NULL);
      m_ahEventPingPongSafe[i] = CreateEvent (NULL, FALSE, FALSE, NULL);
      m_af360PingPongRet[i] = FALSE;
      m_adwPingPongProgress[i] = 0;
      adwAngle[i] = (DWORD)-1;
      afOnTopBottom[i] = FALSE;
      afAngleEqualsFour[i] = FALSE;
      afAngleEqualsFive[i] = FALSE;

      afInfo[i].pThis = this;
      afInfo[i].dwThread = i;
      afInfo[i].dwWorldChanged = dwWorldChanged;
      ahThread[i] = CreateThread (NULL, ESCTHREADCOMMITSIZE, Render360PingPongThreadProc, &afInfo[i], 0, &dwID);
      SetThreadPriority (ahThread[i], VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));
   } // i


   // do 6 camera angles
   fp fTemp;
   IMAGEPIXEL ip1;
   FIMAGEPIXEL fip1;
   memset (&ip1, 0, sizeof(ip1));
   memset (&fip1, 0, sizeof(fip1));
   DWORD dwPass, dwPassRendered;

   DWORD dwDetailsPass;
   DWORD dwDetailsStart = (m_dwShadowsFlags & SF_TWOPASS360) ? 0 : 1;
   DWORD dwProgressMax = 6 * 2 * (2 - dwDetailsStart);
   DWORD dwProgress = 0;

#ifdef _DEBUG
   DWORD dwStartTime = GetTickCount ();
#endif

   BOOL fHaveSentLightInfoOver = FALSE;

   // determine if looking at an odd angle
   fp fpOddAngle = myfmod(m_f360Long, PI/2);
   int iOddAngle;
   if ((fpOddAngle >= PI/8.0) && (fpOddAngle < PI*3.0/8.0)) {
      fpOddAngle = PI/4.0;
      iOddAngle = (int)dwWidth / 8;
   }
   else {
      fpOddAngle = 0.0;
      iOddAngle = 0;
   }
   BOOL fOddAngle = fpOddAngle ? TRUE : FALSE;

   for (dwDetailsPass = dwDetailsStart; dwDetailsPass < 2; dwDetailsPass++) {
      DWORD dwPingInUse;
      // set the image size
      DWORD dwSquareCur = dwSquareFinal / (dwDetailsPass ? 1 : 2);

      // may have extra-low quality because of IsPainterly() being tru
      if (m_fHalfDetail)
         dwSquareCur /= 2;

      dwSquareCur = max(dwSquareCur, 2);  // always at least something
      DWORD dwSquareCurMinusOne = dwSquareCur - 1;
      fp fScaleDetailThis = (fp)dwSquareCur / (fp)dwSquareFinal;
      fp fFullWayCur = (fp)dwSquareCurMinusOne;
      fp fMidWayCur = fFullWayCur/2.0;

      for (dwPingInUse = 0; dwPingInUse < dwNumPing; dwPingInUse++) {
         if (apImageTemp[dwPingInUse]) {
            if (!apImageTemp[dwPingInUse]->Init (dwSquareCur, dwSquareCur))
               goto done;

            if (!m_apRender[dwPingInUse]->CImageSet (apImageTemp[dwPingInUse]))
               goto done;
         }
         else {   // using floating point
            if (!apFImageTemp[dwPingInUse]->Init (dwSquareCur, dwSquareCur))
               goto done;

            if (!m_apRender[dwPingInUse]->CFImageSet (apFImageTemp[dwPingInUse]))
               goto done;
         }

         // set the shadows flag for the render. If first detail pass then minimal
         m_apRender[dwPingInUse]->ShadowsFlagsSet (dwDetailsPass ? m_dwShadowsFlags : (DWORD)-1);
         
         // set the shadows distance
         if (m_apRenderTrad[dwPingInUse]) {
            m_apRenderTrad[dwPingInUse]->m_fShadowsLimit = m_fShadowsLimit;
            m_apRenderTrad[dwPingInUse]->m_fShadowsLimitCenteredOnCamera = TRUE; // minimize renders
         }

      } // dwPingInUse

      DWORD dwSidesRendered = 0;

      dwPass = dwPassRendered = 0;

      while (TRUE) {
         // periodically cann m_pProgress and update m_fWantToCancel, so
         // waitformultipleobjects needs timeout
         if (m_pProgress && m_pProgress->WantToCancel())
            m_fWantToCancel = TRUE; // other threads will check periodically and cancel out automagically

         // update the progress
         if (m_pProgress)
            m_pProgress->Update ((fp)dwProgress / (fp)dwProgressMax);

         // see if we can do anything
         DWORD dwCanStartNewRender = (DWORD)-1;
         DWORD dwCanProcessRender = (DWORD)-1;
         BOOL  fThreadProcessingWorld = FALSE;
         DWORD dwProcessing = 0;
         DWORD dwPing;
         for (dwPing = 0; dwPing < dwNumPing; dwPing++)
            switch (m_adwPingPongProgress[dwPing]) {
               case 0:  // not doing anything
                  dwCanStartNewRender = dwPing;
                  break;

               case 1:  // processing, not safe to hand out to another thread
               case 3: // renderded, but NOT safe to procede
                  fThreadProcessingWorld = TRUE;
                  dwProcessing++;
                  break;

               case 2: // rendering, BUT safe to proceded
                  dwProcessing++;
                  break;

               case 4:  // rendered, ready for processing
                  dwCanProcessRender = dwPing;
                  // BUGFIX - Dont bother dwProcessing++;
                  break;
            } // switch

         // if want to quit and nothing processing then exit
         if (m_fWantToCancel && !dwProcessing)
            break;

         // if safe to start new render then do so now
         if (!fThreadProcessingWorld && (dwCanStartNewRender != (DWORD) -1) && (dwPass < 6)) {
            DWORD dwAngle;
            BOOL fOnTopBottom;
            dwPingInUse = dwCanStartNewRender;

            // determine current point direction
            CPoint pLook;
            pLook.p[0] = cos(m_f360Lat) * sin(m_f360Long);
            pLook.p[1] = cos(m_f360Lat) * cos(m_f360Long);
            pLook.p[2] = sin(m_f360Lat);
            
            // compare this to the possible directions
            DWORD dwClosest = (DWORD)-1;
            fp fClosest = 0;
            CPoint pSideDir;

            // BUGFIX - Render 4 directions first, then top/bottom
            DWORD dwStartDir = 0, dwEndDir = 6;
            if ((dwSidesRendered & 0x0f) == 0x0f)
               dwStartDir = 4;
            else
               dwEndDir = 4;
            for (i = dwStartDir; i < dwEndDir; i++) {
               if (dwSidesRendered & (1 << i))
                  continue;   // already drawn

               // determine direction
               pSideDir.Zero();
               switch (i) {
               case 0: // look north
               case 1: // look east
               case 2: // look south
               case 3: // look west
                  pSideDir.p[0] = sin((fp)i*PI/2.0+fpOddAngle);
                  pSideDir.p[1] = cos((fp)i*PI/2.0+fpOddAngle);
                  break;
                  //pSideDir.p[0] = 1;
                  //break;
                  //pSideDir.p[1] = -1;
                  //break;
                  //pSideDir.p[0] = -1;
                  //break;
               case 4:  // look up
                  pSideDir.p[2] = 1;
                  break;
               case 5: // look down
                  pSideDir.p[2] = -1;
                  break;
               }

               fp fCompare = pSideDir.DotProd (&pLook);

               // since up/down are lower priority, reduce their dot product
               if (i >= 4)
                  fCompare -= 0.5;

               if ((dwClosest == -1) || (fCompare > fClosest)) {
                  dwClosest = i;
                  fClosest = fCompare;
               }
            } // i
            if (dwClosest == -1)
               break;   // shouldnt happen

            dwAngle = dwClosest;
            BOOL fAngleEqualsFour = (dwAngle == 4);   // up
            BOOL fAngleEqualsFive = (dwAngle == 5);   // down
            dwSidesRendered |= (1 << dwAngle);

            fOnTopBottom = (dwAngle >= 4);

            // make sure longitude in clockwise direction...
            fLong = -(fp)dwAngle / 4.0 * 2.0 * PI - fpOddAngle;
            fLat = 0;
            if (fAngleEqualsFour) {
               // look up
               fLong = -fpOddAngle;
               fLat = PI/2;
            }
            else if (fAngleEqualsFive) {
               // look down
               fLong = -fpOddAngle;
               fLat = -PI/2;
            }

            // move the camera
            m_apRender[dwPingInUse]->CameraPerspWalkthrough (&m_pCamera, fLong, fLat, 0, PI/2);

            // remember
            adwAngle[dwPingInUse] = dwAngle;
            afOnTopBottom[dwPingInUse] = fOnTopBottom;
            afAngleEqualsFour[dwPingInUse] = fAngleEqualsFour;
            afAngleEqualsFive[dwPingInUse] = fAngleEqualsFive;

            // clone lights from the original
            if (m_pRTShadow && m_apRenderTrad[dwPingInUse] && !fHaveCopiedFromRTShadows && !m_fWantToCancel) {
                     // BUGFIX - Only clonelights if !m_fWantToCancel, so dont get half way through and quitting
               fHaveCopiedFromRTShadows = TRUE;
               m_pRTShadow->CloneLightsTo (m_apRenderTrad[dwPingInUse]);
            }

            // start the renderer
            SetEvent (m_ahEventToThread[dwPingInUse]);
            m_adwPingPongProgress[dwPingInUse] = 1;   // since waiting

            dwPass++;   // since onto the next pass
            continue;   // repeat
         } // started new render

         // if can process the render then do that now
         if (dwCanProcessRender != (DWORD)-1) {
            dwPingInUse = dwCanProcessRender;
            m_adwPingPongProgress[dwPingInUse] = 0;  // since just processed the render
            dwProgress++;  // since just finished processing

            // if error then want to quit
            if (!m_af360PingPongRet[dwPingInUse]) {
               m_fWantToCancel = TRUE;
               break;
            }

            DWORD dwAngle = adwAngle[dwPingInUse];
            BOOL fOnTopBottom = afOnTopBottom[dwPingInUse];
            BOOL fAngleEqualsFour = afAngleEqualsFour[dwPingInUse];
            BOOL fAngleEqualsFive = afAngleEqualsFive[dwPingInUse];

            // stretch out the image...
            DWORD dwQuad;
            fp fX, fY, fXOrig, fYOrig;
            int iXUL, iYUL;
            int iImageX, iImageY, iImageXBase, iImageYBase;
            DWORD dwInterp, dwTopAngle;
            DWORD dwMaxTopAngle = fOnTopBottom ? 4 : 1;
            DWORD dwQuadMax = fOnTopBottom ? 2 : 4;
            DWORD dwXMax = fOnTopBottom ? dwInterpWidthTopBottom : dwInterpWidth;
            for (pCur = fOnTopBottom ? paInterpTop : paInterp, dwY = 0; dwY < dwInterpHeight; dwY++) {
               for (dwX = 0; dwX < dwXMax; dwX++, pCur++) {
                  // if out of bounds then ignore
                  if (!pCur->p[3])
                     continue;

                  // loop through the 4 quadrants, lowest bit = 0 for left,1 for right,
                  // 2nd bit = 0 for top, 1 for bottom
                  for (dwQuad = 0; dwQuad < dwQuadMax; dwQuad++) {
                     BOOL fRight = (dwQuad%2) ? TRUE : FALSE;
                     BOOL fBottom = (dwQuad/2) ? TRUE : FALSE;
                     int fRightInt = fRight ? -1 : 1;
                     int fBottomInt = fBottom ? 1 : -1;
                     fp fScaleX = (fp)fRightInt * fScaleDetailThis;
                     fp fScaleY = (fp)fBottomInt * fScaleDetailThis;

                     // if we're dawing the top/bottom pantes then ignore
                     // the mirror for fBottom

                     // if we have an odd number of pixels then make sure not to add the seem twice
                     if (!fEvenFinal) {
                        if ((dwX == 0) && fRight)
                           continue;
                        if ((dwY == 0) && fBottom)
                           continue;
                     }

                     // BUGFIX - moved out of dwTopAngleLoop
                     // figure out the pixel in the original image
                     fXOrig = fMidWayCur + pCur->p[0] * fScaleX; // (fRight ? -1 : 1);
                     fYOrig = fMidWayCur + pCur->p[1] * fScaleY; // (fBottom ? 1 : -1);

                     // BUGFIX - moved out of dwTopAngleLoop
                     if (fOnTopBottom) {
                        iImageXBase = (int)dwWidth; // + (int)(dwTopAngle*dwWidth/4);

                        // looking up or down
                        iImageXBase += (int)dwX * fRightInt;
                        iImageYBase = (int)(dwHeight+1)/2 + (int)dwY * (fAngleEqualsFour ? -1 : 1);

                        if (fEvenFinal) {
                           if (fRight)   // if left looking
                              iImageXBase--;

                           if (fAngleEqualsFour)
                              iImageYBase--;
                        } // fEvenFinal
                     } // if angle >= 4 (rendering top or bottom)
                     else {
                        // else, it's one of the 4 vertial panes
                        // what pixel is this in the actual image...
                        iImageXBase = (int)dwWidth + (int)(dwAngle*dwWidth/4)
                           + (int)dwX * fRightInt;
                        iImageYBase = (int)(dwHeight+1)/2 + (int)dwY * fBottomInt;

                        if (fEvenFinal) {
                           if (fRight)   // if left looking
                              iImageXBase--;
                           if (!fBottom)
                              iImageYBase--;
                        } // fEvenFinal
                     }
                     iImageXBase = (iImageXBase + (int)dwWidth) % (int)dwWidth;
                     // bounds checking
                     if ((iImageYBase < 0) || (iImageYBase >= (int)dwHeight))
                        continue;   // exceeds bounaries

                     // create several instances of this pixels...
                     // for the 4 vertical panes, only one instance, but for
                     // top and bottom pane need 4 instances
                     for (dwTopAngle = 0; dwTopAngle < dwMaxTopAngle; dwTopAngle++) {

                        // figure out the pixel in the original image
                        fX = fXOrig;
                        fY = fYOrig;

                        if (fOnTopBottom) switch (dwTopAngle) {
                        case 0: // look north
                           if (fAngleEqualsFour)
                              fY = fFullWayCur - fY;
                           break;
                        case 1: // look east
                           fTemp = fX;
                           fX = fFullWayCur - fY;
                           if (fAngleEqualsFour)
                              fY = fFullWayCur - fTemp;
                           else
                              fY = fTemp;
                           break;
                        case 2: // look south
                           fX = fFullWayCur - fX;
                           if (fAngleEqualsFive)
                              fY = fFullWayCur - fY;
                           break;
                        case 3: // look west
                           fTemp = fX;
                           fX = fY;
                           if (fAngleEqualsFour)
                              fY = fTemp;
                           else
                              fY = fFullWayCur - fTemp;
                           break;
                        }

                        // for interpolation...
                        iXUL = floor(fX);
                        iYUL = floor(fY);
                        fX -= iXUL;
                        fY -= iYUL;

                        iImageX = iImageXBase + iOddAngle;
                        if (fOnTopBottom)
                           iImageX += (int)(dwTopAngle*dwWidth/4);
                        // bound checking.
                        // know that iImageX >= 0, and < dwWidth
                        if (iImageX >= (int)dwWidth)
                           iImageX = iImageX % (int)dwWidth;
                        iImageY = iImageYBase;

                        // interpolation pixels and weighting
                        if (apImageTemp[dwPingInUse]) {
                           PIMAGEPIXEL pip, pip2;
                           if (m_pIS) {
                              pip = &ip1;
                              PBYTE pb = m_pIS->Pixel ((DWORD)iImageX, (DWORD)iImageY);
                              pip->wRed = Gamma(pb[0]);
                              pip->wGreen = Gamma(pb[1]);
                              pip->wBlue = Gamma(pb[2]);
                           }
                           else
                              pip = m_pImage->Pixel ((DWORD)iImageX, (DWORD)iImageY);

                           // determine if should wipe
                           if (pbDrawnOn) {
                              pbDrawingOn = pbDrawnOn + ((DWORD)iImageX + (DWORD)iImageY * dwWidth);
                              if (!*pbDrawingOn) {
                                 pip->wRed = pip->wGreen = pip->wBlue = 0; // wipe out
                                 pip->fZ = ZINFINITE;
                                 pip->dwID = 0;
                                 pip->dwIDPart = 0;

                                 *pbDrawingOn = TRUE; // so wont blank the next time
                              }
                           }

                           // BUGFIX - If jaggies OK then faster
                           if (!fInterpPixels) {
                              int iX = iXUL, iY = iYUL;
                              if (fX >= 0.5)
                                 iX++;
                              if (fY >= 0.5)
                                 iY++;

                              // get the pixel
                              if (!dwDetailsPass && !fOnTopBottom) {
                                 // BUGFIX - Min/max iX,iY so that rapid render for 1st pass works
                                 // NOTE: Doesn't quite work all the way... still see line at top
                                 // But, ignoring since it's only a temporary image
                                 iX = max(iX, 0);
                                 iY = max(iY, 0);
                                 iX = min(iX, (int)dwSquareCurMinusOne);
                                 iY = min(iY, (int)dwSquareCurMinusOne);
                              }
                              else {
                                 if ((iX < 0) || (iX >= (int)dwSquareCur) || (iY < 0) || (iY >= (int)dwSquareCur))
                                    continue;   // off this image
                              }

                              pip2 = apImageTemp[dwPingInUse]->Pixel ((DWORD)iX, (DWORD)iY);
                              pip->wRed = pip2->wRed;
                              pip->wGreen = pip2->wGreen;
                              pip->wBlue = pip2->wBlue;

                              if (!pip->dwID) {
                                 pip->dwID = pip2->dwID;
                                 pip->dwIDPart = pip2->dwIDPart;
                                 pip->fZ = pip2->fZ * pCur->p[2];  // NOTE: NOT interpolating Z. may need to do this
                                    // BUGFIX - Should be pCur->p[2]. Was bug
                              }

                              // store the value away if m_pIS
                              if (m_pIS) {
                                 PBYTE pb = m_pIS->Pixel ((DWORD)iImageX, (DWORD)iImageY);
                                 pb[0] = UnGamma (pip->wRed);
                                 pb[1] = UnGamma (pip->wGreen);
                                 pb[2] = UnGamma (pip->wBlue);
                              }
                           }
                           // else, allow supersample, so inter
                           else {
                              DWORD dwAlphaLR = (DWORD) (fX * (fp)0xffff);
                              DWORD dwAlphaUD = (DWORD) (fY * (fp)0xffff);

                              for (dwInterp = 0; dwInterp < 4; dwInterp++) {
                                 int iX = iXUL, iY = iYUL;
                                 DWORD dwScore;

                                 if (dwInterp%2) {   // right
                                    iX++;
                                    dwScore = dwAlphaLR;
                                 }
                                 else  // left
                                    dwScore = 0x10000-dwAlphaLR;
                                 dwScore = min(dwScore, 0xffff);  // so dont exceed bounds of dword

                                 if (dwInterp/2) {  // bottom
                                    iY++;
                                    dwScore = (dwScore * dwAlphaUD) / 0x10000;
                                 }
                                 else
                                    dwScore = (dwScore * (0x10000 - dwAlphaUD)) / 0x10000;

                                 // get the pixel
                                 if (!dwDetailsPass && !fOnTopBottom) {
                                    // BUGFIX - Min/max iX,iY so that rapid render for 1st pass works
                                    // NOTE: Doesn't quite work all the way... still see line at top
                                    // But, ignoring since it's only a temporary image
                                    iX = max(iX, 0);
                                    iY = max(iY, 0);
                                    iX = min(iX, (int)dwSquareCurMinusOne);
                                    iY = min(iY, (int)dwSquareCurMinusOne);
                                 }
                                 else {
                                    if ((iX < 0) || (iX >= (int)dwSquareCur) || (iY < 0) || (iY >= (int)dwSquareCur))
                                       continue;   // off this image
                                 }

                                 pip2 = apImageTemp[dwPingInUse]->Pixel ((DWORD)iX, (DWORD)iY);
                                 pip->wRed += (WORD)(dwScore * (DWORD) pip2->wRed / 0x10000);
                                 pip->wGreen += (WORD)(dwScore * (DWORD) pip2->wGreen / 0x10000);
                                 pip->wBlue += (WORD)(dwScore * (DWORD) pip2->wBlue / 0x10000);

                                 if (!pip->dwID) {
                                    pip->dwID = pip2->dwID;
                                    pip->dwIDPart = pip2->dwIDPart;
                                    pip->fZ = pip2->fZ * pCur->p[2];  // NOTE: NOT interpolating Z. may need to do this
                                       // BUGFIX - Should be pCur->p[2]. Was bug
                                 }

                                 // store the value away if m_pIS
                                 if (m_pIS) {
                                    PBYTE pb = m_pIS->Pixel ((DWORD)iImageX, (DWORD)iImageY);
                                    pb[0] = UnGamma (pip->wRed);
                                    pb[1] = UnGamma (pip->wGreen);
                                    pb[2] = UnGamma (pip->wBlue);
                                 }
                              } //dwInterp
                           } // if interp
                        }  // if integer image


                        else if (apFImageTemp[dwPingInUse]) {
                           PFIMAGEPIXEL pip, pip2;
                           if (m_pIS) {
                              pip = &fip1;
                              PBYTE pb = m_pIS->Pixel ((DWORD)iImageX, (DWORD)iImageY);
                              pip->fRed = Gamma(pb[0]);
                              pip->fGreen = Gamma(pb[1]);
                              pip->fBlue = Gamma(pb[2]);
                           }
                           else
                              pip = m_pFImage->Pixel ((DWORD)iImageX, (DWORD)iImageY);

                           // determine if should wipe
                           if (pbDrawnOn) {
                              pbDrawingOn = pbDrawnOn + ((DWORD)iImageX + (DWORD)iImageY * dwWidth);
                              if (!*pbDrawingOn) {
                                 pip->fRed = pip->fGreen = pip->fBlue = 0; // wipe out
                                 pip->fZ = ZINFINITE;
                                 pip->dwID = 0;
                                 pip->dwIDPart = 0;

                                 *pbDrawingOn = TRUE; // so wont blank the next time
                              }
                           }

                           // BUGFIX - If jaggies OK then faster
                           if (!fInterpPixels) {
                              int iX = iXUL, iY = iYUL;
                              if (fX >= 0.5)
                                 iX++;
                              if (fY >= 0.5)
                                 iY++;

                              // get the pixel
                              if (!dwDetailsPass && !fOnTopBottom) {
                                 // BUGFIX - Min/max iX,iY so that rapid render for 1st pass works
                                 // NOTE: Doesn't quite work all the way... still see line at top
                                 // But, ignoring since it's only a temporary image
                                 iX = max(iX, 0);
                                 iY = max(iY, 0);
                                 iX = min(iX, (int)dwSquareCurMinusOne);
                                 iY = min(iY, (int)dwSquareCurMinusOne);
                              }
                              else {
                                 if ((iX < 0) || (iX >= (int)dwSquareCur) || (iY < 0) || (iY >= (int)dwSquareCur))
                                    continue;   // off this image
                              }

                              pip2 = apFImageTemp[dwPingInUse]->Pixel ((DWORD)iX, (DWORD)iY);
                              pip->fRed = pip2->fRed;
                              pip->fGreen = pip2->fGreen;
                              pip->fBlue = pip2->fBlue;

                              if (!pip->dwID) {
                                 pip->dwID = pip2->dwID;
                                 pip->dwIDPart = pip2->dwIDPart;
                                 pip->fZ = pip2->fZ * pCur->p[2];  // NOTE: NOT interpolating Z. may need to do this
                              }

                              // store the value away if m_pIS
                              if (m_pIS) {
                                 PBYTE pb = m_pIS->Pixel ((DWORD)iImageX, (DWORD)iImageY);
                                 pip->fRed = max(min(pip->fRed, (fp)0xffff), 0);
                                 pip->fGreen = max(min(pip->fGreen, (fp)0xffff), 0);
                                 pip->fBlue = max(min(pip->fBlue, (fp)0xffff), 0);
                                 pb[0] = UnGamma ((WORD)pip->fRed);
                                 pb[1] = UnGamma ((WORD)pip->fGreen);  // BUGFIX - Was fRed, changed to fGreen
                                 pb[2] = UnGamma ((WORD)pip->fBlue);  // BUGFIX - Was fRed, changed to fBlue
                              }              
                           }
                           else {
                              // no jaggies, so interp

                              // if it's the 2nd pass and not drawn on then clear

                              for (dwInterp = 0; dwInterp < 4; dwInterp++) {
                                 int iX = iXUL, iY = iYUL;
                                 fp fScore;

                                 if (dwInterp%2) {   // right
                                    iX++;
                                    fScore = fX;
                                 }
                                 else  // left
                                    fScore = 1.0 - fX;

                                 if (dwInterp/2) {  // bottom
                                    iY++;
                                    fScore = fScore * fY;
                                 }
                                 else
                                    fScore = fScore * (1.0 - fY);

                                 // get the pixel
                                 if (!dwDetailsPass && !fOnTopBottom) {
                                    // BUGFIX - Min/max iX,iY so that rapid render for 1st pass works
                                    // NOTE: Doesn't quite work all the way... still see line at top
                                    // But, ignoring since it's only a temporary image
                                    iX = max(iX, 0);
                                    iY = max(iY, 0);
                                    iX = min(iX, (int)dwSquareCurMinusOne);
                                    iY = min(iY, (int)dwSquareCurMinusOne);
                                 }
                                 else {
                                    if ((iX < 0) || (iX >= (int)dwSquareCur) || (iY < 0) || (iY >= (int)dwSquareCur))
                                       continue;   // off this image
                                 }

                                 pip2 = apFImageTemp[dwPingInUse]->Pixel ((DWORD)iX, (DWORD)iY);
                                 pip->fRed += fScore * pip2->fRed;
                                 pip->fGreen += fScore * pip2->fGreen;
                                 pip->fBlue += fScore * pip2->fBlue;

                                 if (!pip->dwID) {
                                    pip->dwID = pip2->dwID;
                                    pip->dwIDPart = pip2->dwIDPart;
                                    pip->fZ = pip2->fZ * pCur->p[2];  // NOTE: NOT interpolating Z. may need to do this
                                 }

                                 // store the value away if m_pIS
                                 if (m_pIS) {
                                    PBYTE pb = m_pIS->Pixel ((DWORD)iImageX, (DWORD)iImageY);
                                    pip->fRed = max(min(pip->fRed, (fp)0xffff), 0);
                                    pip->fGreen = max(min(pip->fGreen, (fp)0xffff), 0);
                                    pip->fBlue = max(min(pip->fBlue, (fp)0xffff), 0);
                                    pb[0] = UnGamma ((WORD)pip->fRed);
                                    pb[1] = UnGamma ((WORD)pip->fGreen);  // BUGFIX - Was fRed, changed to fGreen
                                    pb[2] = UnGamma ((WORD)pip->fBlue);  // BUGFIX - Was fRed, changed to fBlue
                                 }
                              } //dwInterp
                           } // no jaggies
                        } // if float image
                     } // dwTopAngle
                  } // dwQuad
               } // dwX

               // finish up if any extra because of interpwidthbottom
               for (; dwX < dwInterpWidth; dwX++, pCur++);
            } // dwY

            // after incoporate can redraw... if last pass then dont call
            if (m_pProgress) {
               if (dwDetailsPass) { // second pass
                  if (dwPassRendered < 5)
                     m_pProgress->CanRedraw ();
               }
               else { // first pass
                  // only draw every few passes, namely first ones and following ones
                  if ((dwPassRendered == 0) || (dwPassRendered == 2) || (dwPassRendered == 4) || (dwPassRendered == 5))
                     m_pProgress->CanRedraw ();
               }
            }         // done
            dwPassRendered++; // since just rendered
            continue;
         } // can process render


         // else, wait for events
         HANDLE ahWait[MAX360PINGPONG*2];
         DWORD adwWaitFor[MAX360PINGPONG*2];
         BOOL afWaitForSafe[MAX360PINGPONG*2];
         DWORD dwNumWait = 0;
         for (dwPing = 0; dwPing < dwNumPing; dwPing++)
            switch (m_adwPingPongProgress[dwPing]) {
               case 0:  // not doing anything
                  break;

               case 1:  // processing, not safe to hand out to another thread
                  // waiting for safe
                  ahWait[dwNumWait] = m_ahEventPingPongSafe[dwPing];
                  adwWaitFor[dwNumWait] = dwPing;
                  afWaitForSafe[dwNumWait] = TRUE;
                  dwNumWait++;

                  // waiting for done rendering
                  ahWait[dwNumWait] = m_ahEventFromThread[dwPing];
                  adwWaitFor[dwNumWait] = dwPing;
                  afWaitForSafe[dwNumWait] = FALSE;
                  dwNumWait++;
                  break;

               case 2:  // rendering, but safe to hand out
                  // waiting for done rendering
                  ahWait[dwNumWait] = m_ahEventFromThread[dwPing];
                  adwWaitFor[dwNumWait] = dwPing;
                  afWaitForSafe[dwNumWait] = FALSE;
                  dwNumWait++;
                  break;

               case 3: // rendered, but not safe to proceded
                  // waiting for safe
                  ahWait[dwNumWait] = m_ahEventPingPongSafe[dwPing];
                  adwWaitFor[dwNumWait] = dwPing;
                  afWaitForSafe[dwNumWait] = TRUE;
                  dwNumWait++;
                  break;

               case 4:  // rendered, ready for processing
                  // NOTE: Shouldnt ever get hit
                  break;
            } // switch

         // if nothing then break (because nothing left to do)
         if (!dwNumWait)
            break;

         // wait
         DWORD dwRet = WaitForMultipleObjects (dwNumWait, ahWait, FALSE, 250);   // test every 100 MS for a cancel
         if ((dwRet == WAIT_TIMEOUT) || (dwRet < WAIT_OBJECT_0))
            continue;   // timed out
         dwRet -= WAIT_OBJECT_0;
         if (dwRet >= dwNumWait)
            break; // shouldnt happen

         dwPing = adwWaitFor[dwRet];
         DWORD dwDeclaredSafeBy = (DWORD)-1;
         if (afWaitForSafe[dwRet])
            // just got signal that what was waiting for was safe
            switch (m_adwPingPongProgress[dwPing]) {
               case 0:  // not doing anything
               case 2:  // rendering, but safe to hand out
               case 4:  // rendered, ready for processing
                  // shouldnt happen
                  break;
               case 1:  // processing, not safe to hand out to another thread
                  m_adwPingPongProgress[dwPing] = 2;
                  dwDeclaredSafeBy = dwPing;
                  dwProgress++;  // since got half way
                  break;
               case 3: // rendered, but not safe to proceded
                  m_adwPingPongProgress[dwPing] = 4;
                  dwDeclaredSafeBy = dwPing;
                  dwProgress++;  // since got half way
                  break;
            }
         else
            // just got sigal that completed rendering
            switch (m_adwPingPongProgress[dwPing]) {
               case 0:  // not doing anything
               case 3: // rendered, but not safe to proceded
                  // shoudlnt happen
                  break;
               case 1:  // processing, not safe to hand out to another thread
                  m_adwPingPongProgress[dwPing] = 3;
                  break;
               case 2:  // rendering, but safe to hand out
                  m_adwPingPongProgress[dwPing] = 4;
                  break;
            }

         // before send over, send lights
         // this will speed up rendering for night scenes
         if ((dwDeclaredSafeBy != (DWORD)-1) && !fHaveSentLightInfoOver && m_apRenderTrad[0]) {
            fHaveSentLightInfoOver = TRUE;

            // BUGFIX - Only do this if have m_pRTShadow since only matters then,
            // and is messing up assert
            if (!(m_dwShadowsFlags & SF_NOSHADOWS) && !m_fWantToCancel)
               for (i = 0; i < MAX360PINGPONG; i++)
                  if (i != dwDeclaredSafeBy)
                     m_apRenderTrad[dwDeclaredSafeBy]->CloneLightsTo (m_apRenderTrad[i]);

            // clone back to shadow
            if (m_pRTShadow && !(m_dwShadowsFlags & SF_NOSHADOWS) && !m_fWantToCancel)
               m_apRenderTrad[dwDeclaredSafeBy]->CloneLightsTo (m_pRTShadow);

         }

         // loop around
      } // while dwPass < 6


      // reset drawn on
      if (!dwDetailsPass && pbDrawnOn)
         memset (pbDrawnOn, 0, dwWidth * dwHeight);
   }  // dwDetailsPass

   // tell to shut down
   m_f360ThreadsShutDown = TRUE;
   for (i = 0; i < dwNumPing; i++) {
      // signal to wake up thread
      while (TRUE) {
         // BUGFIX - doing just in case misses message, which might be able to
         SetEvent (m_ahEventToThread[i]);

         if (WaitForSingleObject (ahThread[i], 100) != WAIT_TIMEOUT)
            break;
      }

      CloseHandle (ahThread[i]);
      CloseHandle (m_ahEventToThread[i]);
      CloseHandle (m_ahEventPingPongSafe[i]);
   } // i

#ifdef _DEBUG
   WCHAR szTemp[256];
   swprintf (szTemp, L"\r\nTime for 360 = %d\r\n", (int)(GetTickCount() - dwStartTime));
   OutputDebugStringW (szTemp);
#endif

   // success
   fRet = TRUE;
   if (m_fWantToCancel)
      fRet = FALSE;

done:
   for (i = 0; i < MAX360PINGPONG; i++) {
      if (apImageTemp[i])
         delete apImageTemp[i];
      if (apFImageTemp[i])
         delete apFImageTemp[i];
   } // i
   m_pProgress = NULL;
   if (hEventSafe)
      SetEvent (hEventSafe);
   return fRet;
}

