/************************************************************************
CRenderRay.cpp - Ray tracing renderer.

begun 17/4/03 by Mike Rozak
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"

#define BUCKETSIZE         32

// CRTCLONECACHE - Remember which objects are cached
typedef struct {
   GUID              gCode;      // code
   GUID              gSub;       // sub-code
   PCRayObject       pObject;    // all the info for the clone cache
   BOOL              fRecurse;   // if TRUE already processing. This prevents clones from containing themselves
   DWORD             dwCount;    // reference count, when this goes to 0 free it up
} CRTCLONECACHE, *PCRTCLONECACHE;

// CRTCAMERARAY - Which rays leave the camera
typedef struct {
   WORD           wX;      // x pixel
   WORD           wY;      // y pixel
   CPoint         pStart;  // starting pixel
   CPoint         pDir;    // direction
   fp             fContrib; // contribution of the ray to the pixel color
} CRTCAMERARAY, *PCRTCAMERARAY;

/*******************************************************************************
CRenderRay::Constructor and destructor
*/
CRenderRay::CRenderRay (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_pRayObjectWorld = new CRayObject (m_dwRenderShard, this, NULL, FALSE);
   m_pWorld = NULL;
   m_pImage = NULL;
   m_dwCameraModel = CAMERAMODEL_PERSPOBJECT;
   m_pProgress = NULL;
   m_fExposure = CM_BESTEXPOSURE;
   m_cBackground = RGB(210,210,0xff);
   m_dwAmbientExtra = 0;
   m_afCenterPixel[0] = m_afCenterPixel[1] = 0;
   m_afScalePixel[0] = m_afScalePixel[1] = 1;
   m_fCameraFlatLongitude = 0;
   m_fCameraFlatTiltX = PI/2;
   m_fCameraFlatTiltY = 0;
   m_fCameraFlatScale = 20;
   m_fCameraFlatTransX = 0;
   m_fCameraFlatTransY = 0;
   m_CameraFlatCenter.Zero();
   m_CameraMatrixRotAfterTrans.Identity();
   m_CameraMatrixRotOnlyAfterTrans.Identity();
   m_fCameraFOV = PI/4;
   m_CameraLookFrom.Zero();
   m_CameraLookFrom.p[1] = -20;
   m_CameraLookFrom.p[2] = CM_EYEHEIGHT;
   m_fCameraLongitude = m_fCameraLatitude = m_fCameraTilt = 0;
   m_CameraObjectLoc.Zero();
   m_CameraObjectLoc.p[1] = 20;
   m_fCameraObjectRotX = m_fCameraObjectRotY = m_fCameraObjectRotZ = 0;
   m_CameraObjectCenter.Zero();
   m_lCRTOBJECT.Init (sizeof(CRTOBJECT));
   m_mRenderSocket.Identity();
   m_pRayCur = NULL;
   m_dwRenderShow = RENDERSHOW_ALL;
   m_fCloneRender = FALSE;
   m_fRandom = TRUE;
   m_dwSuperSample = 1;
   m_dwSpecial = ORSPECIAL_NONE;
   m_afAmbient[0] = m_afAmbient[1] = m_afAmbient[2] = 0;
   m_dwMaxReflect = 5;
   m_fAntiEdge = TRUE;
   m_fScaleBeam = 1.0;
   m_fMinLight = 1.0 / (fp)0xffff;
   m_fMinLightAnt = m_fMinLightAntLarge = 0;
   m_fMinLightLumens = 0;
   m_iPriorityIncrease = 0;

   // allow one thread per processor
   m_dwMaxThreads = HowManyProcessors();

   DWORD i;
   for (i = 0; i < MAXRAYTHREAD; i++) {
      m_amemRayToLight[i].Required (500000);  // somewhat arbitrary number of 500k rays to light
      m_alCRTRAYREFLECT[i][0].Init (sizeof(CRTRAYREFLECT));
      m_alCRTRAYREFLECT[i][1].Init (sizeof(CRTRAYREFLECT));
   }

   // default to one setting
   CPoint p, pCenter;
   p.Zero ();
   p.p[1] = 20;
   pCenter.Zero();
   CameraPerspObject (&p, &pCenter, 0, 0, 0, PI/4);

   m_lCRTCLONECACHE.Init (sizeof(CRTCLONECACHE));
   m_dwCloneID = 0;

   // clear all the buckets
   memset (m_aBucket, 0, sizeof(m_aBucket));
}

CRenderRay::~CRenderRay (void)
{
   // clear out world
   CWorldSet (NULL);

   if (m_pRayObjectWorld) {
      m_pRayObjectWorld->Clear();
      delete m_pRayObjectWorld;
   }

   // clean out the objects
   PCRTOBJECT po;
   DWORD i;
   po = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
   for (i = 0; i < m_lCRTOBJECT.Num(); i++, po++)
      if (po->pObject)
         delete po->pObject;

   // free all the threads
   for (i = 0; i < MAXRAYTHREAD; i++) {
      if (!m_aBucket[i].hThread)
         continue;

      m_aBucket[i].fWantQuit = TRUE;
      SetEvent (m_aBucket[i].hSignal);
      WaitForSingleObject (m_aBucket[i].hThread, INFINITE);
      CloseHandle (m_aBucket[i].hThread);
      CloseHandle (m_aBucket[i].hSignal);
      CloseHandle (m_aBucket[i].hDone);
   }

   // Clear out clone cache? May not need to do this here
   while (m_lCRTCLONECACHE.Num()) {
      PCRTCLONECACHE pcc;
      pcc = (PCRTCLONECACHE) m_lCRTCLONECACHE.Get(0);
      CloneRelease (&pcc->gCode, &pcc->gSub);
   }
}

/*******************************************************************************
CRenderRay::ImageQueryFloat - From CRenderSuper
*/
DWORD CRenderRay::ImageQueryFloat (void)
{
   return 2;
}

/*******************************************************************************
CRenderRay::CImageSet - From CRenderSuper
*/
BOOL CRenderRay::CImageSet (PCImage pImage)
{
   return FALSE;
}

/*******************************************************************************
CRenderRay::CFImageSet - From CRenderSuper
*/
BOOL CRenderRay::CFImageSet (PCFImage pImage)
{
   if (!pImage || !pImage->Width())
      return FALSE;

   m_pImage = pImage;
   DWORD dwWidth, dwHeight;
   dwWidth = m_pImage->Width();
   dwHeight = m_pImage->Height();


   // set perspective info
   m_RenderMatrix.ScreenInfo (dwWidth, dwHeight);

   // and remember for scaling after perpective
   // replaced by fn call
   RenderCalcCenterScale (dwWidth, dwHeight, &m_afCenterPixel[0], &m_afScalePixel[0]);
   //m_afCenterPixel[0] = (fp) dwWidth / 2.0 + 0.5;   // need +0.5 so it rounds off properly
   //m_afCenterPixel[1] = (fp) dwHeight / 2.0 + 0.5;  // need +0.5 so it rounds off properly
   //m_afScalePixel[0] = (fp) (dwWidth+2) / 2.0; // BUGFIX - Did +1 to width and height do ends up drawing UL corner
   //m_afScalePixel[1] = -(fp) (dwHeight+2) / 2.0;

   return TRUE;
}

/*******************************************************************************
CRenderRay::CWorldSet - From CRenderSuper
*/
void CRenderRay::CWorldSet (PCWorldSocket pWorld)
{
   // if m_pWorld, Unregister notification sockets so know what has changed
   if (m_pWorld)
      m_pWorld->NotifySocketRemove (this);

   m_pWorld = pWorld;

   // Register notification sockets so know what has changed
   if (m_pWorld)
      m_pWorld->NotifySocketAdd (this);
}

/*******************************************************************************
CRenderRay::CameraFlat - From CRenderSuper
*/
void CRenderRay::CameraFlat (PCPoint pCenter, fp fLongitude, fp fTiltX, fp fTiltY, fp fScale,
                                    fp fTransX, fp fTransY)
{
   // store values away
   m_dwCameraModel = CAMERAMODEL_FLAT;
   m_fCameraFlatLongitude = fLongitude;
   m_fCameraFlatTiltX = fTiltX;
   m_fCameraFlatTiltY = fTiltY;
   m_fCameraFlatScale = fScale;
   m_fCameraFlatTransX = fTransX;
   m_fCameraFlatTransY = fTransY;
   m_CameraFlatCenter.Copy (pCenter);

   // clear main matrix
   m_RenderMatrix.PerspectiveScale (2.0 / fScale, 2.0 / fScale, 1);
      // Need the 1.0 so z values stay the same
   m_RenderMatrix.Clear();
   m_RenderMatrix.Rotate (-PI/2, 1);   // rotate -90 degrees around X

   // apply the transforms
   m_RenderMatrix.Translate (fTransX, 0, fTransY);
   // post-translation rotation matrix
   m_CameraMatrixRotAfterTrans.Identity();
   CMatrix t;
   CPoint z;
   z.Zero();
   m_CameraMatrixRotAfterTrans.FromXYZLLT (&z, fLongitude, fTiltX, fTiltY);

   m_CameraMatrixRotOnlyAfterTrans.Copy (&m_CameraMatrixRotAfterTrans);

   // do the translation for center of rotation
   t.Translation (-m_CameraFlatCenter.p[0],-m_CameraFlatCenter.p[1],-m_CameraFlatCenter.p[2]);
   m_CameraMatrixRotAfterTrans.MultiplyLeft (&t);

   m_RenderMatrix.Multiply (&m_CameraMatrixRotAfterTrans);
   //m_RenderMatrix.Rotate (fTiltX, 1);
   //m_RenderMatrix.Rotate (fTiltY, 2);
   //m_RenderMatrix.Rotate (-fLongitude, 3);
}

/*******************************************************************************
CRenderRay::CameraFlatGet - From CRenderSuper
*/
void CRenderRay::CameraFlatGet (PCPoint pCenter, fp *pfLongitude, fp *pfTiltX, fp *pfTiltY, fp *pfScale, fp *pfTransX, fp *pfTransY)
{
   *pfLongitude = m_fCameraFlatLongitude;
   *pfTiltX = m_fCameraFlatTiltX;
   *pfTiltY = m_fCameraFlatTiltY;
   *pfScale = m_fCameraFlatScale;
   *pfTransX = m_fCameraFlatTransX;
   *pfTransY = m_fCameraFlatTransY;
   pCenter->Copy (&m_CameraFlatCenter);
}

/*******************************************************************************
CRenderRay::CameraPerspWalkthrough - From CRenderSuper
*/
void CRenderRay::CameraPerspWalkthrough (PCPoint pLookFrom, fp fLongitude, fp fLatitude, fp fTilt, fp fFOV)
{
   m_dwCameraModel = CAMERAMODEL_PERSPWALKTHROUGH;
   m_fCameraFOV = fFOV;
   m_CameraLookFrom.Copy (pLookFrom);
   m_fCameraLongitude = fLongitude;
   m_fCameraLatitude = fLatitude;
   m_fCameraTilt = fTilt;

   // apply perspective
   m_RenderMatrix.Perspective (fFOV);

   // clear main matrix
   m_RenderMatrix.Clear();
   m_RenderMatrix.Rotate (-PI/2, 1);   // rotate -90 degrees around X

   // apply the transforms
   m_RenderMatrix.Rotate (-fTilt, 2);
   m_RenderMatrix.Rotate (-fLatitude, 1);
   m_RenderMatrix.Rotate (-fLongitude, 3);
      // BUGFIX - Make fLongitude negative so this matrix is inverse of ToXYZLLV,
      // which is used by other two views   

   // translate
   m_RenderMatrix.Translate (-pLookFrom->p[0], -pLookFrom->p[1], -pLookFrom->p[2]);

   // since dont rotate after translating, set matrix to identity
   m_CameraMatrixRotAfterTrans.Identity();
   m_CameraMatrixRotOnlyAfterTrans.Identity();
}

/*******************************************************************************
CRenderRay::CameraPerspWalkthroughGet - From CRenderSuper
*/
void CRenderRay::CameraPerspWalkthroughGet (PCPoint pLookFrom, fp *pfLongitude, fp *pfLatitude, fp *pfTilt, fp *pfFOV)
{
   pLookFrom->Copy (&m_CameraLookFrom);
   *pfLongitude = m_fCameraLongitude;
   *pfLatitude = m_fCameraLatitude;
   *pfTilt = m_fCameraTilt;
   *pfFOV = m_fCameraFOV;
}

/*******************************************************************************
CRenderRay::CameraPerspObject - From CRenderSuper
*/
void CRenderRay::CameraPerspObject (PCPoint pTranslate, PCPoint pCenter, fp fRotateZ, fp fRotateX, fp fRotateY, fp fFOV)
{
   m_dwCameraModel = CAMERAMODEL_PERSPOBJECT;
   m_fCameraFOV = fFOV;
   m_CameraObjectLoc.Copy (pTranslate);
   m_fCameraObjectRotX = fRotateX;
   m_fCameraObjectRotY = fRotateY;
   m_fCameraObjectRotZ = fRotateZ;
   m_CameraObjectCenter.Copy (pCenter);

   // apply perspective
   m_RenderMatrix.Perspective (fFOV);

   // clear main matrix
   m_RenderMatrix.Clear();

   // apply the transforms
   m_RenderMatrix.Rotate (-PI/2, 1);   // rotate -90 degrees around X
   m_RenderMatrix.Translate (pTranslate->p[0], pTranslate->p[1], pTranslate->p[2]);

   // post-translation rotation matrix
   CPoint z;
   CMatrix t;
   z.Zero();
   m_CameraMatrixRotAfterTrans.FromXYZLLT (&z, fRotateZ, fRotateX, fRotateY);
   m_CameraMatrixRotOnlyAfterTrans.Copy (&m_CameraMatrixRotAfterTrans);

   // do the translation for center of rotation
   t.Translation (-m_CameraObjectCenter.p[0],-m_CameraObjectCenter.p[1],-m_CameraObjectCenter.p[2]);
   m_CameraMatrixRotAfterTrans.MultiplyLeft (&t);

   m_RenderMatrix.Multiply (&m_CameraMatrixRotAfterTrans);

}


/*******************************************************************************
CRenderRay::CameraPerspObjectGet - From CRenderSuper
*/
void CRenderRay::CameraPerspObjectGet (PCPoint pTranslate, PCPoint pCenter, fp *pfRotateZ, fp *pfRotateX, fp *pfRotateY, fp *pfFOV)
{
   pTranslate->Copy (&m_CameraObjectLoc);
   pCenter->Copy (&m_CameraObjectCenter);
   *pfRotateX = m_fCameraObjectRotX;
   *pfRotateY = m_fCameraObjectRotY;
   *pfRotateZ = m_fCameraObjectRotZ;
   *pfFOV = m_fCameraFOV;
}

/*******************************************************************************
CRenderRay::CameraModelGet - From CRenderSuper
*/
DWORD CRenderRay::CameraModelGet (void)
{
   return m_dwCameraModel;
}

/*******************************************************************************
CRenderRay::Render - From CRenderSuper
*/
BOOL CRenderRay::Render (DWORD dwWorldChanged, HANDLE hEventSafe, PCProgressSocket pProgress)
{
   if (!m_pImage || !m_pWorld) {
      if (hEventSafe)
         SetEvent (hEventSafe);
      return FALSE;
   }

   // create the threads if they're not already created
   DWORD i;
   if (!m_aBucket[0].pRay) {
      // create all the threads
      for (i = 0; i < MAXRAYTHREAD; i++)
         CreateBucketThread (i);
   }

   // max threads
   m_dwMaxThreads = max(1,m_dwMaxThreads);
   m_dwMaxThreads = min(MAXRAYTHREAD, m_dwMaxThreads);

   // sync up the objects
   if (!SyncUpObjects()) {
      if (hEventSafe)
         SetEvent (hEventSafe);
      return FALSE;
   }

   // get al the dirty objects renderef
   if (!RenderDirtyObjects()) {
      if (hEventSafe)
         SetEvent (hEventSafe);
      return FALSE;
   }

   CalcAmbientLight ();

   // transfer all objects into m_pRayObjectWorld just to make life easier
   PCRTOBJECT pc = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
   m_pRayObjectWorld->Clear();
   m_pRayObjectWorld->m_fDontDelete = TRUE;
   for (i = 0; i < m_lCRTOBJECT.Num(); i++, pc++) {
      if (!pc->fVisible)
         continue;

      // else, add
      m_pRayObjectWorld->SubObjectAdd (pc->pObject);
   }
   m_pRayObjectWorld->CalcBoundBox();

   // safe to render in another thread here
   BOOL fLowerThreads = FALSE;
   if (hEventSafe) {
      SetEvent (hEventSafe);
      hEventSafe = NULL;

      // lower the thread priority
      fLowerThreads = TRUE;
   }

   // lower the thread priority so ping-poing approach to rendering 360 works better
   if (fLowerThreads)
      for (i = 0; i < MAXRAYTHREAD; i++)
         SetThreadPriority (m_aBucket[i].hThread, VistaThreadPriorityHack(THREAD_PRIORITY_LOWEST, m_iPriorityIncrease));

   // clear out buffers
   for (i = 0; i < MAXRAYTHREAD; i++)
      m_amemRayToLight[i].m_dwCurPosn = 0;

   // set random?
   if (m_fRandom)
      srand (GetTickCount());

   // which threads are available
   HANDLE      ahWait[MAXRAYTHREAD];
   BOOL        afThreadInUse[MAXRAYTHREAD];
   memset (&afThreadInUse, 0, sizeof(afThreadInUse));

   m_pProgress = pProgress;
   m_pImage->Clear (m_cBackground);

   DWORD xMax, yMax;
   xMax = (m_pImage->Width() + BUCKETSIZE-1) / BUCKETSIZE;
   yMax = (m_pImage->Height() + BUCKETSIZE-1) / BUCKETSIZE;

   // two passes, the first to draw at the prescribed resoltuion, the second to
   // anti-alias the edges
   DWORD dwPass;
   BOOL fWantToCancel = FALSE;
   for (dwPass = 0; dwPass < (DWORD) (m_fAntiEdge ? 2 : 1); dwPass++) {
      if (fWantToCancel)
         break;   // exit out here

      if (m_fAntiEdge && pProgress) {
         if (dwPass == 0)
            pProgress->Push (0, .5);
         else
            pProgress->Push (.5, 1);
      }

      CMem memImage;
      WORD *pawScore = NULL;
      BYTE *pabRGB;
      WORD wScoreThresh = 0;
      fp fPassScale = 1.0;
      if (dwPass == 1) {
         m_dwSuperSample++;   // so higher sampling rate

         // allocate some scratch memory
         DWORD dwNeed,dw;
         dw = m_pImage->Width() * m_pImage->Height();
         dwNeed = dw * (3 + sizeof(WORD));
         if (!memImage.Required (dwNeed))
            break;
         pawScore = (WORD*) memImage.p;
         pabRGB = (BYTE*) (pawScore + dw);

         // get the RGB of all the pixels
         PFIMAGEPIXEL pip;
         float f;
         DWORD j;
         pip = m_pImage->Pixel(0,0);
         for (i = 0; i < dw; i++, pip++) {
            for (j = 0; j < 3; j++) {
               f = (&pip->fRed)[j];
               f = max(f,0);
               f = min(f, (fp)0xffff);
               pabRGB[i*3+j] = UnGamma ((WORD)f);
            } // j
         } // i

         // find the score
         int x,y,xx,yy;
         int iHeight, iWidth;
         WORD wScore;
         DWORD dw1,dw2;
         PFIMAGEPIXEL pip2;
         iHeight = (int)m_pImage->Height();
         iWidth = (int)m_pImage->Width();
         BOOL fNewMajor, fNewMinor, fNewSub;
         WORD wMax, wTemp;
         unsigned __int64 qwTotal;
         qwTotal = wMax = 0;
         for (y=0; y < iHeight; y++) for (x = 0; x < iWidth; x++) {
            pip = m_pImage->Pixel((DWORD)x, (DWORD) y);
            dw1 = (DWORD) (x + y * iWidth);

            // start with score of 0
            wScore = 0;
            fNewMajor = fNewMinor = fNewSub = 0;

            for (yy = max(y-1,0); yy < min(y+2, iHeight); yy++)
            for (xx = max(x-1,0); xx < min(x+2, iWidth); xx++) {
               if ((x == xx) && (y == yy))
                  continue;   // dont check against self

               pip2 = m_pImage->Pixel ((DWORD)xx, (DWORD) yy);
               dw2 = (DWORD) (xx + yy * iWidth);

               // calculate the score, first doing the difference in pixels
               wTemp = 0;
               for (i = 0; i < 3; i++)
                  if (pabRGB[dw1*3+i] > pabRGB[dw2*3+i])
                     wTemp += (pabRGB[dw1*3+i] - pabRGB[dw2*3+i]);
                  else
                     wTemp += (pabRGB[dw2*3+i] - pabRGB[dw1*3+i]);
               wScore = max(wScore, wTemp);

               // note if different objects
               if (HIWORD(pip->dwID) != HIWORD(pip2->dwID))
                  fNewMajor = TRUE;
               else if (LOWORD(pip->dwID) != LOWORD(pip2->dwID))
                  fNewMinor = TRUE;
               else if (pip->dwIDPart != pip2->dwIDPart)
                  fNewSub = TRUE;
            } // xx and yy

            // if the object IDs are off increase the score
            if (fNewMajor)
               wScore *= 2;
            else if (fNewMinor)
               wScore += wScore / 2;
            else if (fNewSub)
               wScore += wScore / 4;

            // remember average and max
            qwTotal += wScore;
            wMax = max(wMax, wScore);
            pawScore[dw1] = wScore;
         } // x and y


         // average
         WORD wAverage, wHigh, wMed, wMedAvg;
         wAverage = (WORD) (qwTotal / dw);
         wHigh = (wAverage + wMax) / 2;
         wMed = (wAverage + wHigh) / 2;
         wMedAvg = (wAverage + wMed) / 2;

         // find out how many points are more than wAverage
         DWORD dw3;
         dw1 = dw2 = dw3 = 0;
         for (i = 0; i < dw; i++)
            if (pawScore[i] >= wMed)
               dw3++;
            else if (pawScore[i] >= wMedAvg)
               dw2++;
            else if (pawScore[i] >= wAverage)
               dw1++;
         dw2 += dw3;
         dw1 += dw2; // since if above med, also above average

         // determine the score threshhold... if there arent many >= average then
         // use average, else use wHigh
         if (dw1 < dw/3)
            wScoreThresh = wAverage;
         else if (dw2 < dw/3)
            wScoreThresh = wMedAvg;
         else if (dw3 < dw/3)
            wScoreThresh = wMed;
         else
            wScoreThresh = wHigh;

         // go through all the pixels that meet the threshhold and reduce their intensity
         // since will be adding more
         fp fScale;
         pip = m_pImage->Pixel (0,0);
         fScale = (fp)(m_dwSuperSample-1) * (fp)(m_dwSuperSample-1);
         fScale = fScale / (fScale + (fp)m_dwSuperSample * (fp)m_dwSuperSample);
         fPassScale = 1.0 - fScale;
         for (i = 0; i < dw; i++, pip++) {
            if (pawScore[i] < wScoreThresh)
               continue;
            pip->fRed *= fScale;
            pip->fGreen *= fScale;
            pip->fBlue *= fScale;
         }
      } // dwPass == 1

      // calculate the beam's spread and width
      m_fBeamSpread = m_fBeamBase = 0;
      m_dwSuperSample = max (m_dwSuperSample,1); // just to make sure
      i = m_pImage->Width() * m_dwSuperSample;
      if (m_dwCameraModel == CAMERAMODEL_FLAT)
         m_fBeamBase = m_fCameraFlatScale / (fp) i * m_fScaleBeam;
      else
         m_fBeamSpread = tan(m_fCameraFOV / (fp)i / 2.0) * m_fScaleBeam;

      // minimum amount of light
      m_fMinLightAnt = m_fMinLight / (fp) (m_dwSuperSample * m_dwSuperSample);
      m_fMinLightAntLarge = m_fMinLightAnt * (fp)0xffff;
      m_fMinLightLumens = m_fMinLight * (fp)0xffff / (fp)m_dwSuperSample / (fp) m_dwSuperSample / m_fExposure;

      // loop until all buckets are done
      DWORD dwNextEmpty, dwBucketsDone, j, dwWait;
      dwNextEmpty = dwBucketsDone = 0;
      while (TRUE) {
         // if there are any that aren't done and can find empty thread slot then create
         while (!fWantToCancel && (dwNextEmpty < xMax * yMax)) {
            for (i = 0; i < m_dwMaxThreads; i++)
               if (!afThreadInUse[i])
                  break;
            if (i >= m_dwMaxThreads)
               break;   // no empty slots

            // figure out how large the ray area is
            RECT rect;
            rect.left = (int)(dwNextEmpty % xMax) * BUCKETSIZE;
            rect.top = (int)(dwNextEmpty / xMax) * BUCKETSIZE;
            rect.right = rect.left + BUCKETSIZE - 1;
            rect.bottom = rect.top + BUCKETSIZE - 1;
            rect.right = min(rect.right, (int)m_pImage->Width()-1);
            rect.bottom = min(rect.bottom, (int)m_pImage->Height()-1);
            if ((rect.right < rect.left) || (rect.bottom < rect.top)) {
               // shouldnt happen
               dwNextEmpty++;
               continue;
            }

            // create rays for the bucket
            if (!CalcPixelsForBucket (&rect, pawScore, wScoreThresh, fPassScale, &m_amemCRTCAMERARAY[i], &m_adwCameraRayNum[i])) {
               // some sort of error
               dwNextEmpty++;
               continue;
            }

            // start the calculations
            SetEvent (m_aBucket[i].hSignal);
            afThreadInUse[i] = TRUE;
            dwNextEmpty++;
         } // while buckets empty and can fill thread

         // wait for one of the threads to finish
         for (i = j = 0; i < MAXRAYTHREAD; i++)
            if (afThreadInUse[i]) {
               ahWait[j] = m_aBucket[i].hDone;
               j++;
            }
         
         // if there arent any threads out then exit
         if (!j)
            break;
         dwWait = WaitForMultipleObjects (j, ahWait, FALSE, 500);
         if (dwWait != WAIT_TIMEOUT) {
            dwWait -= WAIT_OBJECT_0;
            if ((dwWait >= 0) && (dwWait < j)) {
               for (i = 0; i < MAXRAYTHREAD; i++)
                  if (m_aBucket[i].hDone == ahWait[dwWait])
                     afThreadInUse[i] = FALSE;
               dwBucketsDone++;
            }
            else {
               // error
               break;
            }
         }

         // percent complete
         if (m_pProgress) {
            // test to see if want to quit
            fWantToCancel  = m_pProgress->WantToCancel();

            m_pProgress->Update ((fp)dwBucketsDone / (fp)(xMax * yMax));

            // send notification so that can update graphics
            m_pProgress->CanRedraw ();
         }
      } // while stuff to process


      if (m_fAntiEdge && pProgress)
         pProgress->Pop();
      if (dwPass == 1)
         m_dwSuperSample--;   // restore sampking rate
   } // dwpass

   m_pProgress = NULL;

   // restore thread priorities that lowerd
   if (fLowerThreads)
      for (i = 0; i < MAXRAYTHREAD; i++)
         SetThreadPriority (m_aBucket[i].hThread, VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL, m_iPriorityIncrease));


   if (hEventSafe)
      SetEvent (hEventSafe);
   return TRUE;
}


/*******************************************************************************
CRenderRay::ExposureGet - From CRenderSuper
*/
fp CRenderRay::ExposureGet (void)
{
   return m_fExposure;
}

/*******************************************************************************
CRenderRay::ExposureSet - From CRenderSuper
*/
void CRenderRay::ExposureSet (fp fExposure)
{
   m_fExposure = fExposure;
}

/*******************************************************************************
CRenderRay::BackgroundGet - From CRenderSuper
*/
COLORREF CRenderRay::BackgroundGet (void)
{
   return m_cBackground;
}

/*******************************************************************************
CRenderRay::BackgroundSet - From CRenderSuper
*/
void CRenderRay::BackgroundSet (COLORREF cBack)
{
   m_cBackground = cBack;
}

/*******************************************************************************
CRenderRay::AmbientExtraGet - From CRenderSuper
*/
DWORD CRenderRay::AmbientExtraGet (void)
{
   return m_dwAmbientExtra;
}

/*******************************************************************************
CRenderRay::AmbientExtraSet - From CRenderSuper
*/
void CRenderRay::AmbientExtraSet (DWORD dwAmbient)
{
   m_dwAmbientExtra = dwAmbient;
}




/*******************************************************************************
CRTOBJECTSort - Sorts by CRTOBJECT structures.
*/
int __cdecl CRTOBJECTSort (const void *elem1, const void *elem2 )
{
   PCRTOBJECT p1, p2;
   p1 = (PCRTOBJECT) elem1;
   p2 = (PCRTOBJECT) elem2;

   return memcmp(&p1->gID, &p2->gID, sizeof(p1->gID));
}


typedef struct {
   GUID        gID;     // object ID
   DWORD       dwIndex; // index
   BOOL        fVisible; // true if visible
   PCObjectSocket pos;  // object this is from
} SUO, *PSUO;


/*******************************************************************************
SUOSort - Sorts by SUO structures.
*/
int __cdecl SUOSort (const void *elem1, const void *elem2 )
{
   PSUO p1, p2;
   p1 = (PSUO) elem1;
   p2 = (PSUO) elem2;

   return memcmp(&p1->gID, &p2->gID, sizeof(p1->gID));
}


/*******************************************************************************
CRenderRay::SyncUpObjects - Syncs up the ray-tracer's copy of objects with those
objects actually in the world.

inputs
   none
retursn
   BOOL - TRUE if success
*/
BOOL CRenderRay::SyncUpObjects (void)
{
	MALLOCOPT_INIT;
   // sort the list of existing objects
   qsort(m_lCRTOBJECT.Get(0), m_lCRTOBJECT.Num(), sizeof (CRTOBJECT), CRTOBJECTSort);

   // enumerate all the objects that exist and sort those
   CListFixed lSUO;
   SUO suo;
   PCObjectSocket pos;
   DWORD i;
   lSUO.Init (sizeof(SUO));
   memset (&suo, 0, sizeof(suo));
   for (i = 0; i < m_pWorld->ObjectNum(); i++) {
      pos = m_pWorld->ObjectGet(i);
      if (!pos)
         continue;
      suo.dwIndex = i;
      pos->GUIDGet (&suo.gID);
      suo.fVisible = pos->ShowGet();

      // if showing an object that not supposed to (like camera) then make invisible
      if (!(pos->CategoryQuery() & m_dwRenderShow))
         suo.fVisible = FALSE;

      suo.pos = pos;
	   MALLOCOPT_OKTOMALLOC;
      lSUO.Add (&suo);
	   MALLOCOPT_RESTORE;
   }

   // sort that list by guid
   qsort(lSUO.Get(0), lSUO.Num(), sizeof (SUO), SUOSort);

   // loop through each of them, adding, removing, or changing
   PCRTOBJECT pc;
   PSUO ps;
   CRTOBJECT crt;
   memset (&crt, 0, sizeof(crt));
   DWORD dwCRT, dwSUO;
   int iRet;
   BOOL fMoreCRT, fMoreSUO;
   pc = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
   ps = (PSUO) lSUO.Get(0);
   dwCRT = dwSUO = 0;
   while (TRUE) {
      fMoreCRT = (dwCRT < m_lCRTOBJECT.Num());
      fMoreSUO = (dwSUO < lSUO.Num());
      if (fMoreCRT && !fMoreSUO) {
         // delete this object then
         delete pc[dwCRT].pObject;
         m_lCRTOBJECT.Remove (dwCRT);
         pc = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
         continue;
      }
      else if (!fMoreCRT && fMoreSUO) {
         // no more objects in list but have some that want to add
         crt.fDirty = TRUE;
         crt.fVisible = ps[dwSUO].fVisible;
         crt.gID = ps[dwSUO].gID;
	      MALLOCOPT_OKTOMALLOC;
         crt.pObject = new CRayObject (m_dwRenderShard, this, NULL, FALSE);
	      MALLOCOPT_RESTORE;
         crt.pos = ps[dwSUO].pos;
         if (!crt.pObject)
             return FALSE; // error
         crt.pObject->m_dwID = ps[dwSUO].dwIndex;

         // add it
	      MALLOCOPT_OKTOMALLOC;
         m_lCRTOBJECT.Add (&crt);
	      MALLOCOPT_RESTORE;
         pc = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
         dwSUO++;
         dwCRT++;
         continue;
      }
      else if (!fMoreCRT && !fMoreSUO) {
         // end of both lists
         break;
      }

      // else, compare
      iRet = memcmp(&pc[dwCRT].gID, &ps[dwSUO].gID, sizeof(pc[dwCRT].gID));

      // if they're same then just copy some stuff over
      if (iRet == 0) {
         pc[dwCRT].fVisible = ps[dwSUO].fVisible;
         pc[dwCRT].pObject->m_dwID = ps[dwSUO].dwIndex;
         pc[dwCRT].pos = ps[dwSUO].pos;
         dwSUO++;
         dwCRT++;
         continue;
      }

      // else
      if (iRet < 0) {
         // the object appears in ray tracer but no matching object in world, so delete
         delete pc[dwCRT].pObject;
         m_lCRTOBJECT.Remove (dwCRT);
         pc = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
         continue;
      }
      else {
         // the object appears in world, but no matching object in ray tracer, so add
         crt.fDirty = TRUE;
         crt.fVisible = ps[dwSUO].fVisible;
         crt.gID = ps[dwSUO].gID;
         crt.pObject = new CRayObject (m_dwRenderShard, this, NULL, FALSE);
         crt.pos = ps[dwSUO].pos;
         if (!crt.pObject)
             return FALSE; // error
         crt.pObject->m_dwID = ps[dwSUO].dwIndex;

         // add it
         m_lCRTOBJECT.Insert (dwCRT, &crt);
         pc = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
         dwSUO++;
         dwCRT++;
         continue;
      }

   } // over all obhects


   // done
   return TRUE;
}


/*******************************************************************************
CRenderRay::RenderDirtyObjects - If there are any dirty objects then they're re-rendered.

returns
   BOOL - TRUE if success
*/
BOOL CRenderRay::RenderDirtyObjects (void)
{
   MALLOCOPT_INIT;
   DWORD i;
   PCRTOBJECT pc;
   OBJECTRENDER or;
   CListFixed lLIGHTINFO;
   lLIGHTINFO.Init (sizeof(LIGHTINFO));
   pc = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
   for (i = 0; i < m_lCRTOBJECT.Num(); i++, pc++) {
      // if they're not dirty or not visible then ignore
      if (!pc->fDirty)
         continue;

      // NOTE: Getting object even if not visible so can also get the lights

      // clear out
      pc->pObject->Clear();

      // save away
      m_pRayCur = pc->pObject;
      m_mRenderSocket.Identity();

      // render
      memset (&or, 0, sizeof(or));
      or.dwReason = ORREASON_FINAL;
      or.dwSpecial = m_dwSpecial;
      or.dwShow = m_dwRenderShow;
      or.pRS = this;
      pc->pos->Render (&or, (DWORD)-1);

      // make sure to recalc the bounding box
      pc->pObject->CalcBoundBox();


      // get all the lights
      lLIGHTINFO.Clear();
      MALLOCOPT_OKTOMALLOC;
      pc->pos->LightQuery (&lLIGHTINFO, m_dwRenderShow);
      MALLOCOPT_RESTORE;
      if (lLIGHTINFO.Num()) {
         PLIGHTINFO pli = (PLIGHTINFO) lLIGHTINFO.Get(0);
         CMatrix m, mInv;
         pc->pos->ObjectMatrixGet (&m);
         m.Invert (&mInv);
         mInv.Transpose ();
         DWORD j;
         for (j = 0; j < lLIGHTINFO.Num(); j++) {
            pli[j].pLoc.p[3] = 1;
            pli[j].pLoc.MultiplyLeft (&m);
            pli[j].pDir.p[3] = 1;
            pli[j].pDir.MultiplyLeft (&mInv);
         }

         pc->pObject->LightsAdd (pli, lLIGHTINFO.Num());
      }

      pc->fDirty = FALSE;
   } // over all objects


   return TRUE;
}

/*******************************************************************************
CRenderRay::QueryWantNormals - Called by the object to determine if
it should bother generating normals for the surface.

inputs
   none
returns
   BOOL - TRUE if is should generate normals, FALSE if they're not needed because
      of the rendering model
*/
BOOL CRenderRay::QueryWantNormals (void)
{
   return TRUE;
}


/******************************************************************************
CRenderRay::QuerySubDetail - From CRenderSocket. Basically end up ignoring
*/
BOOL CRenderRay::QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail)
{
   *pfDetail = 0;
   return TRUE;  // always fail this one
}

/******************************************************************************
CRenderRay::QueryCloneRender - From CRenderSocket
*/
BOOL CRenderRay::QueryCloneRender (void)
{
   return TRUE;
}


/******************************************************************************
CRenderRay::CloneRender - From CRenderSocket
*/
BOOL CRenderRay::CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix)
{
   if (!m_pRayCur)
      return FALSE;  // shouldnt happen

   // make a new one
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   PCRayObject pNew;
   pNew = new CRayObject (m_dwRenderShard, this, NULL, m_fCloneRender);
	MALLOCOPT_RESTORE;
   if (!pNew)
      return FALSE;
   pNew->m_dwID = m_pRayCur->m_dwID;   // so keeps same ID

   if (!pNew->ClonesAdd (pgCode, pgSub, pamMatrix, dwNum)) {
      // error of some sort
      delete pNew;
      return FALSE;
   }
   pNew->CalcBoundBox();   // recalc bounding box

   // else, add in to raycur
   m_pRayCur->SubObjectAdd (pNew, NULL);

   return TRUE;
}


/*******************************************************************************
CRenderRay::QueryWantTextures - Called by the object to determine if
it should bother generating textures for the surface.

inputs
   none
returns
   BOOL - TRUE if is should generate normals, FALSE if they're not needed because
      of the rendering model
*/
BOOL CRenderRay::QueryWantTextures (void)
{
   return TRUE;
}

/*******************************************************************************
CRenderRay::QueryDetail - Called by the object to determine how
much detail it should produce in the model.

inputs
   none
returns
   fp - Detail level in meters. Ex: If returns 0.05 should returns polygons
      about 5cm large
*/
fp CRenderRay::QueryDetail (void)
{
   return 0;
}

/*******************************************************************************
CRenderRay::MatrixSet - Sets the rotation and translation matrix to
use for the given points.

inputs
   PCMatrix    pm - matrix to use
returns
   none
*/
void CRenderRay::MatrixSet (PCMatrix pm)
{
   m_mRenderSocket.Copy (pm);
}

/*******************************************************************************
CRenderRay::PolyRender - Called by the objects when it's rendering.

inputs
   PPOLYRENDERINFO      pInfo - Information that specified what polygons to render.
                           NOTE: The contents of pInfo, along with what it points to,
                           might be changed by this function.
returns
   none
*/
void CRenderRay::PolyRender (PPOLYRENDERINFO pInfo)
{
   if (!m_pRayCur)
      return;  // shouldnt happen

   // make a new one
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   PCRayObject pNew;
   pNew = new CRayObject (m_dwRenderShard, this, NULL, m_fCloneRender);
	MALLOCOPT_RESTORE;
   if (!pNew)
      return;
   pNew->m_dwID = m_pRayCur->m_dwID;   // so keeps same ID

   if (!pNew->PolygonsSet (pInfo, &m_mRenderSocket, m_pWorld)) {
      // error of some sort
      delete pNew;
      return;
   }
   pNew->CalcBoundBox();   // recalc bounding box

   // else, add in to raycur
   m_pRayCur->SubObjectAdd (pNew, NULL);
}



/*******************************************************************************
CRenderRay::RenderShowGet - From CRenderSuper
*/
DWORD CRenderRay::RenderShowGet (void)
{
   return m_dwRenderShow;
}

/*******************************************************************************
CRenderRay::RenderShowSet - From CRenderSuper
*/
void CRenderRay::RenderShowSet (DWORD dwShow)
{
   m_dwRenderShow = dwShow;
   DirtyAllObjects();
}


/*******************************************************************************
CRenderRay::DirtyAllObjects - Dirty all the objects contained within

*/
void CRenderRay::DirtyAllObjects (void)
{
   PCRTOBJECT pc = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
   DWORD i;
   for (i = 0; i < m_lCRTOBJECT.Num(); i++, pc++)
      pc->fDirty = TRUE;
}


/*******************************************************************************
CRenderRay::WorldChanged - From CViewSocket
*/
void CRenderRay::WorldChanged (DWORD dwChanged, GUID *pgObject)
{
   // only care if changed an object
   DWORD dwCare = WORLDC_OBJECTCHANGESEL | WORLDC_OBJECTCHANGENON;
   if (!(dwCare & dwChanged))
      return;

   if (pgObject) {
      // if only one object then dirty just that
      CRTOBJECT co;
      PCRTOBJECT pFind;
      co.gID = *pgObject;
      pFind = (PCRTOBJECT) bsearch(&co, m_lCRTOBJECT.Get(0), m_lCRTOBJECT.Num(), sizeof (CRTOBJECT), CRTOBJECTSort);

      if (pFind)
         pFind->fDirty = TRUE;
   }
   else
      DirtyAllObjects ();
}

/*******************************************************************************
CRenderRay::WorldUndoChanged - From CViewSocket
*/
void CRenderRay::WorldUndoChanged (BOOL fUndo, BOOL fRedo)
{
   // do nothing
}

/*******************************************************************************
CRenderRay::WorldAboutToChange - From CViewSocket
*/
void CRenderRay::WorldAboutToChange (GUID *pgObject)
{
   // do nothing
}

/*******************************************************************************
CRenderRay::CalcPixelsForBucket - Calculate what pixels belong to a bucket.

NOTE: Uses the m_fRandom to determine if jitter should be done. If not m_fRandom
then no randomization of pixels is done. Also, assumes that if m_fRandom is set
then srand() has already been used.

Uses m_dwSuperSample to determine super sampling amount. ANd m_fExposure to determine
how bright the pixel will be.

inputs
   RECT           *pr - Pixels to draw. (NOTE: These are inclusive of points,
                     so if draw only one pixel at 10, 15, will be 10,15,10,15
   WORD           *pawScore - If not NULL then this is an array of scores for the pixels
                     (size m_pImage->Width() x m_pImage->Height()). Only those pixels higher
                     than wScoreThresh will be kept. If NULL then all pixels used
   WORD           wScoreThresh - Score threshhold. Used if pawScore is not NULL
   fp             fColorScale - Scale all the color intensities by this amount.
   PCMem          pMem - Memory that will be modified to contain the array
                  of CRTCAMERARAY
   DWORD          *pdwRays - Filled in with the number of rays in pMem
returns
   BOOL - TRUE if success
*/
BOOL CRenderRay::CalcPixelsForBucket (RECT *pr, WORD *pawScore, WORD wScoreThresh,
                                      fp fColorScale, PCMem pMem, DWORD *pdwRays)
{
   // make sure enough memory
   DWORD dwNeed;
   dwNeed = (DWORD) (pr->right - pr->left + 1) * (DWORD) (pr->bottom - pr->top + 1) *
      m_dwSuperSample * m_dwSuperSample;
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   if (!pMem->Required (dwNeed * sizeof(CRTCAMERARAY))) {
   	MALLOCOPT_RESTORE;
      return FALSE;
   }
	MALLOCOPT_RESTORE;
   PCRTCAMERARAY pCur;
   pCur = (PCRTCAMERARAY) pMem->p;

   // FUTURERELEASE - When do depth of field will need slightly different viewer location

   // figure out where the Eye is in world space
   CPoint pEye;
   if (m_dwCameraModel == CAMERAMODEL_FLAT) {
      CPoint pEye2;
      PixelToWorldSpace ((fp)m_pImage->Width()/2 + .5, (fp)m_pImage->Height()/2 + .5,
         -2000, &pEye);
      PixelToWorldSpace ((fp)m_pImage->Width()/2 + .5, (fp)m_pImage->Height()/2 + .5,
         0, &pEye2);
      pEye.Subtract (&pEye2);
   }
   else {
      CPoint pV;
      pV.Zero();
      pV.p[3] = 1;
      m_RenderMatrix.TransformViewSpaceToWorldSpace (&pV, &pEye);
   }

   // what are the limits
   CPoint ap[4];  // [0]=ul, [1]=ur, [2]=ll, [3]=lr
   CPoint pTop, pBottom;
   PixelToWorldSpace ((fp)pr->left - .5, (fp)pr->top - .5, 1000, &ap[0]);
   PixelToWorldSpace ((fp)pr->right + .5, (fp)pr->top - .5, 1000, &ap[1]);
   PixelToWorldSpace ((fp)pr->left - .5, (fp)pr->bottom + .5, 1000, &ap[2]);
   PixelToWorldSpace ((fp)pr->right + .5, (fp)pr->bottom + .5, 1000, &ap[3]);
   DWORD i;
   if (m_dwCameraModel != CAMERAMODEL_FLAT)
      for (i = 0; i < 4; i++)
         ap[i].Subtract (&pEye);

   DWORD dwHalf;
   dwHalf = m_dwSuperSample/2;  // half way point

   // initialize ray
   CRTCAMERARAY ray;
   ray.pStart.Copy (&pEye);
   ray.fContrib = (fp)0xffff / (fp)m_dwSuperSample / (fp) m_dwSuperSample / m_fExposure * fColorScale;

   // loop
   DWORD x, y, xx, yy;
   fp fx, fy;
   fp fRangeX, fRangeY, fSuper;
   fRangeX = (pr->right - pr->left)+1;
   fRangeY = (pr->bottom - pr->top)+1;
   fSuper = (fp) (m_dwSuperSample-1) / 2.0;
   *pdwRays = 0;
   for (y = 0; y <= (DWORD)(pr->bottom - pr->top); y++)
   for (x = 0; x <= (DWORD)(pr->right - pr->left); x++) {
      // if we are only drawing certain pixels then text
      if (pawScore && (pawScore[x + (DWORD) pr->left + (y +(DWORD)pr->top)* m_pImage->Width()] < wScoreThresh))
         continue;

      // oversample
      for (yy = 0; yy < m_dwSuperSample; yy++)
      for (xx = 0; xx < m_dwSuperSample; xx++) {
         if (m_dwSuperSample) {
            // make sure that the first ray to appear for the super-sample appears
            // as the centeral one so the object outline is more accurate
            if (xx == 0)
               fx = (fp) dwHalf;
            else if (xx == dwHalf)
               fx = 0;
            else
               fx = (fp) xx;
            if (yy == 0)
               fy = (fp) dwHalf;
            else if (yy == dwHalf)
               fy = 0;
            else
               fy = (fp) yy;
         }
         else
            fx = fy = 0;

         // if have jitter then add randomness
         if (m_fRandom) {
            fx += randf(-.5, .5);
            fy += randf(-.5, .5);
         }

         fx = (fp) x + (fx - fSuper) / (fp)m_dwSuperSample;
         fy = (fp) y + (fy - fSuper) / (fp)m_dwSuperSample;
         fx = (fx + .5) / fRangeX;
         fy = (fy + .5) / fRangeY;

         // average
         pTop.Average (&ap[1], &ap[0], fx);
         pBottom.Average (&ap[3], &ap[2], fx);
         ray.pDir.Average (&pBottom, &pTop, fy);

         // if flag then eye location different
         if (m_dwCameraModel == CAMERAMODEL_FLAT) {
            ray.pStart.Add (&ray.pDir, &pEye);
            ray.pDir.Subtract (&ray.pStart);
         }

         // add
         ray.wX = (WORD) pr->left + (WORD)x;
         ray.wY = (WORD) pr->top + (WORD)y;

         *pCur = ray;
         pCur++;

         (*pdwRays)++;
      }
   } // x, y
   return TRUE;
}


/*******************************************************************************
CRenderRay::PixelToZ - Given a pixel, this returns the Z value.

inputs
   DWORD    dwX, dwY - pixels
returns
   fp - Z value. Positive values are away from the user. Returns a large
         number if clicking on infinity.
*/
fp CRenderRay::PixelToZ (DWORD dwX, DWORD dwY)
{
   PFIMAGEPIXEL pip = m_pImage->Pixel (dwX, dwY);
   return pip->fZ;
}

/*******************************************************************************
CRenderRay::PixelToViewerSpace - Given a pixel X,Y, (and Z) this converts
to the camera space, where Z is negative. It takes into account the current
rendering model and FOV. If uses the Z value from PixelToZ(). If no Z value
exists then the Z will be around -32000 meters.

inputs
   fp    dwX, dwY - pixels
   fp   fZ - Comes from PixelToZ(), or just make it up. Positive #'s further away
   PCPoint  p - Filled in with X,Y,Z,W in camera space
returns
   none
*/
void CRenderRay::PixelToViewerSpace (fp dwX, fp dwY, fp fZ, PCPoint p)
{
   fp fX, fY, fW;

   // start working backwards
   fW = (m_dwCameraModel == CAMERAMODEL_FLAT) ? 1 : (fZ * tan(m_fCameraFOV / 2.0));
   fZ *= -1;
   fX = ((fp) dwX - m_afCenterPixel[0]) / m_afScalePixel[0] * fW;
   fY = -((fp) dwY - m_afCenterPixel[1]) / m_afScalePixel[0] * fW;  // NOTE: This didn't seem to come out right
      // NOTE: Use scalepixel[0] since want to skip the Y stretched used for clipping
   if (m_dwCameraModel == CAMERAMODEL_FLAT) {
      fX *= m_fCameraFlatScale/2;
      fY *= m_fCameraFlatScale/2;
   }

   p->p[0] = fX;
   p->p[1] = fY;
   p->p[2] = fZ;
   p->p[3] = 1;   // was fW, but this seems to cause problems with matrix inversion;
}

/*******************************************************************************
CRenderRay::PixelToWorldSpace - Given a pixel XY (and Z), this converts
from the screen coordinates to a position in world space.

inputs
   fp    dwX, dwY - pixels
   fp   fZ - Comes from PixelToZ(), or just make it up. Positive #'s further away
   PCPoint  p - Filled in with X,Y,Z,W in camera space
returns
   none
*/
void CRenderRay::PixelToWorldSpace (fp dwX, fp dwY, fp fZ, PCPoint p)
{
   // convert to a point in viewer space
   CPoint v;
   PixelToViewerSpace (dwX, dwY, fZ, &v);

   // then to world space
   m_RenderMatrix.TransformViewSpaceToWorldSpace (&v, p);
}


/*******************************************************************************
CRenderRay::WorldSpaceToPixel - Given a point in world space, this
converts it to a pixel on the screen.

inputs
   PCPoint        pWorld - Point in world space
   fp             *pfX, *pfY - Filled with X and Y
returns
   BOOL - True if transformed - and is in front of user. FALSE if behind
*/
BOOL CRenderRay::WorldSpaceToPixel (PCPoint pWorld, fp *pfX, fp *pfY, fp *pfZ)
{
   // apply rotation to the point
   CPoint pLoc;
   pLoc.Copy (pWorld);
   pLoc.p[3] = 1;

   m_RenderMatrix.Transform (1, &pLoc);
   if ((pLoc.p[2] >= -CLOSE) && (m_dwCameraModel != CAMERAMODEL_FLAT))
      return FALSE;  // behind
   TransformToScreen (1, &pLoc);

   *pfX = pLoc.p[0];
   *pfY = pLoc.p[1];
   if (pfZ)
      *pfZ = pLoc.p[2];
   return TRUE;
}


/*******************************************************************************
CRenderRay::ShadowsFlagsSet - Standard API
*/
void CRenderRay::ShadowsFlagsSet (DWORD dwFlags)
{
   // NOTE: m_dwShadowsFlags not used by ray tracer
   m_dwShadowsFlags = dwFlags;
}



/*******************************************************************************
CRenderRay::ShadowsFlagsGet - Standard API
*/
DWORD CRenderRay::ShadowsFlagsGet (void)
{
   return m_dwShadowsFlags;
}


/*******************************************************************************
CRenderRay::TransformToScreen - Transforms all the points (in place)
to screen coordinates. It does this by dividing out W (and assumes that no w <= 0)
because clipping should have taken place. It then does scaling and offsets so
the image goes from 0 to width/height.

inputs
   DWORD       dwNum - Number of points
   PCPoint     paPoints - points
returns
   none
*/
void CRenderRay::TransformToScreen (DWORD dwNum, PCPoint paPoints)
{
   for (; dwNum; dwNum--, paPoints++) {
      paPoints->p[0] = m_afCenterPixel[0] + ((paPoints->p[0] / paPoints->p[3]) * m_afScalePixel[0]);
      paPoints->p[1] = m_afCenterPixel[1] + ((paPoints->p[1] / paPoints->p[3]) * m_afScalePixel[1]);
      paPoints->p[2] = -paPoints->p[2];   // since zbuffer is as a positive value
   }
}


/*******************************************************************************
RayThreadProc - Called for each thread. See CreateBucketThread()
*/
static DWORD WINAPI RayThreadProc(LPVOID lpParameter)
{
   PCRTBUCKETINFO pInfo = (PCRTBUCKETINFO) lpParameter;

   pInfo->pRay->BucketThread (pInfo->dwThread);
   return 0;
}

/*******************************************************************************
CRenderRay::CreateBucketThread - Call this to create a thread for the ray and then
process it. This assumes that m_amemCRTCAMERARAY[dwThread] and m_adwCameraRayNum[dwThread]
have been initialized

inputs
   DWORD          dwThread - Thread number, from 0..MAXRAYTHREAD-1
returns
   HANDLE - Handle to the thread; this will be destroyed when the thread dies.
            NULL if error. CloseHandle() must be called on this after the thread
            has finished.
*/
HANDLE CRenderRay::CreateBucketThread (DWORD dwThread)
{
   // fill in the bucket info
   m_aBucket[dwThread].dwThread = dwThread;
   m_aBucket[dwThread].pRay = this;
   m_aBucket[dwThread].hSignal = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_aBucket[dwThread].hDone = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_aBucket[dwThread].fWantQuit = FALSE;

   // create the thread
   HANDLE hThread;
   DWORD dwID;
   hThread = CreateThread (NULL, ESCTHREADCOMMITSIZE, RayThreadProc, &m_aBucket[dwThread], 0, &dwID);
   SetThreadPriority (hThread, VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL, m_iPriorityIncrease));  // so doesnt suck up all CPU

   // remember the tread
   m_aBucket[dwThread].hThread = hThread;
   return hThread;
}

/*******************************************************************************
CRenderRay::RayIntersect - Intersects the given ray with any.
If there is an intersection, updates the alpha and intersection information in
pRay.

inputs
   DWORD          dwThread - Thread ID
   DWORD          dwBound - Bounding volume ID, for optimizations
   PCRTRAYPATH    pRay - Ray to intersect against
returns
   BOOL - TRUE if success and interescted something
*/
BOOL CRenderRay::RayIntersect (DWORD dwThread, DWORD dwBound, PCRTRAYPATH pRay)
{
   return m_pRayObjectWorld->RayIntersect (dwThread, dwBound, pRay, TRUE);
      // skip initial boundsphere test with world

#if 0 // old code
   BOOL fFind = FALSE;

   //NOTE - Should eliminate those objects not in the bounding box

   //NOTE - Should sort objects by nearest to source

   // NOTE - Try intersecting againt the last object that successfully intersected agains (for
   // the previous ray) first. Since rays close to one another, pretty good chance will be fast

   // loop through all the objects
   DWORD dwNum = m_lCRTOBJECT.Num();
   PCRTOBJECT pc = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
   DWORD i;
   BOOL fSub;
   for (i = 0; i < dwNum; i++, pc++) {
      // if found an object and we're in "stop at any opaque" and found an
      // opaque obejct then exit
      if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
         break;

      if (!pc->fVisible)
         continue;

      // intersect
      fSub = pc->pObject->RayIntersect (dwThread, dwBound, pRay);
      if (fSub)
         fFind = TRUE;
   }

   return fFind;
#endif // 0
}

/*******************************************************************************
CRenderRay::BucketThread - This internal function is called by the thread proc
for each bucket. It returns when the entire bucket has been processed.

inputs
   DWORD          dwThread - Thread number, from 0..MAXRAYTHREAD-1
returns
   none
*/
void CRenderRay::BucketThread (DWORD dwThread)
{
   // repeat
   while (TRUE) {
      // wait for a signal
      WaitForSingleObject (m_aBucket[dwThread].hSignal, INFINITE);
      if (m_aBucket[dwThread].fWantQuit)
         return;

      // clear out the relfection ping-pong buffers
      m_alCRTRAYREFLECT[dwThread][0].Clear();
      m_alCRTRAYREFLECT[dwThread][1].Clear();
      m_adwRayReflectBuf[dwThread] = 0;

      DWORD i, j;
      PCRTCAMERARAY pc = (PCRTCAMERARAY) m_amemCRTCAMERARAY[dwThread].p;
      // figure out the bounding volume for all the rays
      CPoint apBound[2][2];   // calculated bounding volumes
      CPoint pEnd;
      memset (apBound, 0, sizeof(apBound));
      for (i = 0; i < m_adwCameraRayNum[dwThread]; i++, pc++) {
         pc->pDir.Normalize();   // since need normalized for the ray later, and also better for calc

         for (j = 0; j < 2; j++) {
            if (j) {
               pEnd.Copy (&pc->pDir);
               pEnd.Scale (LONGDIST);
               pEnd.Add (&pc->pStart);
            }
            else
               pEnd.Copy (&pc->pStart);

            if (i) {
               apBound[j][0].Min (&pEnd);
               apBound[j][1].Max (&pEnd);
            }
            else {
               apBound[j][0].Copy (&pEnd);
               apBound[j][1].Copy (&pEnd);
            }
         } // j
      } // i
      CPoint apNewBound[2];
      m_pRayObjectWorld->BoundingVolumeInit (dwThread, 0 /* camera to objects*/, apBound[0], apBound[1], apNewBound);



      // loop through all the rays and see what they hit
      pc = (PCRTCAMERARAY) m_amemCRTCAMERARAY[dwThread].p;
      float afScale[3];
      for (i = 0; i < m_adwCameraRayNum[dwThread]; i++, pc++) {
         BOOL fFind;
         CRTRAYPATH ray;
         memset (&ray, 0, sizeof(ray));
         ray.pStart.Copy (&pc->pStart);
         ray.pDir.Copy (&pc->pDir);
         // done above ray.pDir.Normalize();
         ray.fAlpha = LONGDIST;
         ray.mToWorld.Identity();
         ray.fBeamOrig = m_fBeamBase;
         ray.fBeamSpread = m_fBeamSpread;

   tryagain:
         fFind = RayIntersect (dwThread, 0 /* camera to objects */, &ray);
         if (!fFind)
            continue;

         // found pixel, get the surface info
         CRTSURFACE surf;
         if (!ray.pInterRayObject->SurfaceInfoGet (dwThread, &ray, &surf))
            continue;

         // if this is just going through a leaf then skip
         if (RayPassThroughLeaf (&ray, &surf))
            goto tryagain;

         // add reflection and refraction
         afScale[0] = afScale[1] = afScale[2] = pc->fContrib;
         QueueReflect (dwThread, pc->wX, pc->wY, afScale, &ray, &surf);

         // apply the color
         PixelFromSurface (dwThread, pc->wX, pc->wY, &pc->pStart, afScale, &surf);
      } // i

      // flush all the rays to the light before begin reflection and refraction pass
      CalculateRayToLight (dwThread);

      // do all the reflection and refraction rays
      for (i = 0; i < m_dwMaxReflect; i++) {
         CalculateReflect (dwThread, i+1 < m_dwMaxReflect);
         CalculateRayToLight (dwThread);
      }

      CalculateRayToLight (dwThread);

      // signal that done
      SetEvent (m_aBucket[dwThread].hDone);
   }
}



/***************************************************************************************
CRenderRay::CalcAmbientLight - Sums up all the ambient lights and fills in m_afAmbient.

While at it, this fills in m_lLIGHTINFO
*/
void CRenderRay::CalcAmbientLight (void)
{
	MALLOCOPT_INIT;
   // reset
   m_afAmbient[0] = m_afAmbient[1] = m_afAmbient[2] = m_dwAmbientExtra / (fp)0xffff * CM_LUMENSSUN;
   m_lLIGHTINFO.Init (sizeof(LIGHTINFO));
   m_lLIGHTINFO.Clear();

   // all other lights
   DWORD i, j, k;
   PCRTOBJECT po = (PCRTOBJECT) m_lCRTOBJECT.Get(0);
   DWORD dwNum = m_lCRTOBJECT.Num();
   PLIGHTINFO pLight;
   DWORD dwLightNum;
   for (i = 0; i < dwNum; i++, po++) {
      pLight = po->pObject->LightsGet (&dwLightNum);
      if (!pLight || !dwLightNum)
         continue;

      for (j = 0; j < dwLightNum; j++, pLight++) {
         if (pLight->dwForm == LIFORM_AMBIENT) {
            for (k = 0; k < 3; k++)
               m_afAmbient[k] += pLight->afLumens[2] * (fp)pLight->awColor[2][k] / (fp)0xffff;
         }
         else {
            // it's a light to add
	         MALLOCOPT_OKTOMALLOC;
            m_lLIGHTINFO.Add (pLight);
	         MALLOCOPT_RESTORE;
         }
      } // j
   } // i
}


/***************************************************************************************
CRenderRay::CalculateRayToLight - Sends a ray from a point to one light source. If
there are no obstructions then the light intensity is added to the pixel.

inputs
   DWORD          dwThread - 0..MAXRAYTHREAD-1
   PCRTRAYTOLIGHTHDR ph - Header info (contaiing pixel)
   PCRTRAYTOLIGHT pr - Ray info, containing destination at light
returns
   none
*/
void CRenderRay::CalculateRayToLight (DWORD dwThread, PCRTRAYTOLIGHTHDR ph, PCRTRAYTOLIGHT pr)
{
   // if the final intensity is 0 (or close) then dont bother
   if ((pr->afColor[0] < m_fMinLightAntLarge) && (pr->afColor[1] < m_fMinLightAntLarge) && (pr->afColor[2] < m_fMinLightAntLarge))
      return;

   CRTRAYPATH ray;
   memset (&ray, 0, sizeof(ray));
   DWORD i;
   for (i = 0; i < 3; i++) {
      ray.pStart.p[i] = ph->afSurfLoc[i];
      ray.pDir.p[i] = pr->afLightLoc[i] - ph->afSurfLoc[i];
   }
   ray.fAlpha = ray.pDir.Length();
   if (ray.fAlpha)
      ray.pDir.Scale (1.0 / ray.fAlpha);
   else
      ray.pDir.p[0] = 1;   // just pick a dir since exact match

   ray.mToWorld.Identity();
   ray.dwDontIntersect = ph->dwDontIntersect;
   ray.pDontIntersect = ph->pDontIntersect;
   ray.fStopAtAnyOpaque = TRUE;
   ray.fBeamOrig = ph->fBeamWidth;
   ray.fBeamSpread = m_fBeamSpread;

   BOOL fFind;
anothertry:
   fFind = RayIntersect (dwThread, 1 /* objects to light*/, &ray);
   if (fFind && ray.fInterOpaque)
      return;  // in shadow

   if (fFind) {
      // found a ray, see what the surface of the object is like since may
      // be transparent
      CRTSURFACE surf;
      if (!ray.pInterRayObject->SurfaceInfoGet (dwThread, &ray, &surf, TRUE))
         return;  // in shadow
      if (!surf.fRefract)
         return;  // no refraction so dont bother

      // If too dim dont bother
      if ((surf.afRefractColor[0] <= m_fMinLightAnt) && (surf.afRefractColor[1] <= m_fMinLightAnt)
         && (surf.afRefractColor[2] <= m_fMinLightAnt))
         return;  // too dark

      // else, obscures some light...
      pr->afColor[0] *= surf.afRefractColor[0];
      pr->afColor[1] *= surf.afRefractColor[1];
      pr->afColor[2] *= surf.afRefractColor[2];

      // and go on
      ray.dwDontIntersect = ray.dwInterPolygon;
      ray.pDontIntersect = ray.pInterRayObject;
      ray.pStart.Copy (&surf.pLoc);
      ray.fBeamOrig = surf.fBeamWidth;
      for (i = 0; i < 3; i++)
         ray.pDir.p[i] = pr->afLightLoc[i] - ray.pStart.p[i];
      ray.fAlpha = ray.pDir.Length();
      if (ray.fAlpha)
         ray.pDir.Scale (1.0 / ray.fAlpha);
      else
         ray.pDir.p[0] = 1;   // just pick a dir since exact match

      goto anothertry;
   }


   // else, add to pixel
   PFIMAGEPIXEL pip;
   pip = m_pImage->Pixel (ph->wX, ph->wY);
   pip->fRed += pr->afColor[0];
   pip->fGreen += pr->afColor[1];
   pip->fBlue += pr->afColor[2];
}

/***************************************************************************************
CRenderRay::CalculateRayToLight - Goes through all the rays in the m_amemRayToLight
buffer and sends them to their source light source.

inputs
   DWORD          dwThread - 0..MAXRAYTHREAD-1
returns
   none
*/
void CRenderRay::CalculateRayToLight (DWORD dwThread)
{
   PCMem pMem = &m_amemRayToLight[dwThread];
   if (!pMem->m_dwCurPosn)
      return;  // nothing to send

   // send rays to light
   DWORD dwCur, dwNeed;
   PCRTRAYTOLIGHTHDR ph;
   PCRTRAYTOLIGHT pr;

   // loop over all the lights
   PLIGHTINFO pli;
   DWORD dwLight, i;
   pli = (PLIGHTINFO) m_lLIGHTINFO.Get(0);
   for (dwLight = 0; dwLight < m_lLIGHTINFO.Num(); dwLight++, pli++) {
      // if is a no-shadows then ignore since wont have any shows anyway
      if (pli->fNoShadows)
         continue;

      // Calculate bounding box to the light
      // figure out the bounding volume for all the rays
      CPoint apBound[2][2];   // calculated bounding volumes
      BOOL fFound;
      fFound = FALSE;
      memset (apBound, 0, sizeof(apBound));
      DWORD k;
      for (dwCur = 0; dwCur < pMem->m_dwCurPosn; dwCur += dwNeed) {
         ph = (PCRTRAYTOLIGHTHDR) ((PBYTE)pMem->p + dwCur);
         pr = (PCRTRAYTOLIGHT) (ph + 1);
         dwNeed = sizeof(CRTRAYTOLIGHTHDR) + ph->dwNumLight * sizeof(CRTRAYTOLIGHT);
         if (dwCur + dwNeed > pMem->m_dwCurPosn)
            break;   // shouldnt happen

         // loop over all the rays, only dealing with the one to the current ray
         for (i = 0; i < ph->dwNumLight; i++, pr++) {
            if (pr->dwLight != dwLight)
               continue;

            // BUGFIX - If this is a zero-strength light then ignore
            if (!pr->afColor[0] && !pr->afColor[1] && !pr->afColor[2])
               continue;

            // add this to min and max
            if (fFound) {
               for (k = 0; k < 3; k++) {
                  apBound[1][0].p[k] = min (apBound[1][0].p[k], pr->afLightLoc[k]);
                  apBound[1][1].p[k] = max (apBound[1][1].p[k], pr->afLightLoc[k]);

                  apBound[0][0].p[k] = min (apBound[0][0].p[k], ph->afSurfLoc[k]);
                  apBound[0][1].p[k] = max (apBound[0][1].p[k], ph->afSurfLoc[k]);
               }
            }
            else {
               for (k = 0; k < 3; k++) {
                  apBound[1][0].p[k] = apBound[1][1].p[k] = pr->afLightLoc[k];
                  apBound[0][0].p[k] = apBound[0][1].p[k] = ph->afSurfLoc[k];
                     // BUGGIX - Was apBound[1][1], fixed to apBound[0][1]
               }
               fFound = TRUE;
            }
         } // i
      } // dwCur
      if (!fFound)
         continue;
      CPoint apNewBound[2];
      m_pRayObjectWorld->BoundingVolumeInit (dwThread, 1 /* objects to lights*/, apBound[0], apBound[1], apNewBound);


      for (dwCur = 0; dwCur < pMem->m_dwCurPosn; dwCur += dwNeed) {
         ph = (PCRTRAYTOLIGHTHDR) ((PBYTE)pMem->p + dwCur);
         pr = (PCRTRAYTOLIGHT) (ph + 1);
         dwNeed = sizeof(CRTRAYTOLIGHTHDR) + ph->dwNumLight * sizeof(CRTRAYTOLIGHT);
         if (dwCur + dwNeed > pMem->m_dwCurPosn)
            break;   // shouldnt happen

         // loop over all the rays, only dealing with the one to the current ray
         for (i = 0; i < ph->dwNumLight; i++, pr++) {
            if (pr->dwLight != dwLight)
               continue;

            // BUGFIX - If this is a zero-strength light then ignore
            if (!pr->afColor[0] && !pr->afColor[1] && !pr->afColor[2])
               continue;

            // send this ray
            CalculateRayToLight (dwThread, ph, pr);
         } // i
         
      } // dwCur
   } // dwLight



   // clear out list
   pMem->m_dwCurPosn = 0;
}

/***************************************************************************************
CRenderRay::QueueRayToLight - Adds an entry to sending a ray to a light source.

inputs
   DWORD          dwThread - 0 .. MAXRAYTHREAD-1
   DWORD          dwX, dwY - Pixels x and y
   PCPoint        pSurf - Location of the surface
   DWORD          dwNum - Number of rays
   fp             fBeamWidth - Width of the beam (in meters) at point where reflects to light
   PCRayObject    pDontIntersect - Dont intersect with this triangle (because coming from it)
   DWORD          dwDontIntersect - Dont intersect with this triangle (because coming from it)
returns
   PCRTRAYTOLIGHT - Pointer to a list of dwNum ray-to-light structures that need to be filled in
                  NULL if error
*/
PCRTRAYTOLIGHT CRenderRay::QueueRayToLight (DWORD dwThread, DWORD dwX, DWORD dwY, PCPoint pSurf, DWORD dwNum,
                                            fp fBeamWidth, PCRayObject pDontIntersect, DWORD dwDontIntersect)
{
   // how much memory to we need
   DWORD dwNeed = sizeof (CRTRAYTOLIGHTHDR) + dwNum * sizeof(CRTRAYTOLIGHT);
   PCMem pMem = &m_amemRayToLight[dwThread];
   if (pMem->m_dwCurPosn + dwNeed >= pMem->m_dwAllocated) {
      // draw all the rays so can clear out the buffer
      CalculateRayToLight (dwThread);

      if (pMem->m_dwCurPosn + dwNeed >= pMem->m_dwAllocated)
         return NULL;    // error
   }

   PCRTRAYTOLIGHTHDR ph;
   ph = (PCRTRAYTOLIGHTHDR) ((PBYTE)pMem->p + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwNeed;

   ph->dwNumLight = dwNum;
   ph->dwDontIntersect = dwDontIntersect;
   ph->pDontIntersect = pDontIntersect;
   ph->wX = (WORD) dwX;
   ph->wY = (WORD) dwY;
   ph->fBeamWidth = fBeamWidth;
   DWORD i;
   for (i = 0; i < 3; i++)
      ph->afSurfLoc[i] = pSurf->p[i];

   return (PCRTRAYTOLIGHT) (ph + 1);
}

/***************************************************************************************
CRenderRay::CalculateReflect - Goes through all the reflection and refraction items
in the current ping-pong buffers and renders them.

inputs
   DWORD          dwThread - 0..MAXRAYTHREAD-1
   BOOL           fMoreReflect - If TRUE then allow the addition of sub-reflections
                  and refractions to the queue. Otherwise, this will be the last one
returns
   none
*/
void CRenderRay::CalculateReflect (DWORD dwThread, BOOL fMoreReflect)
{
   PCListFixed pList = &m_alCRTRAYREFLECT[dwThread][m_adwRayReflectBuf[dwThread]];
   PCRTRAYREFLECT prr = (PCRTRAYREFLECT) pList->Get(0);

   // clear the other ping-pong list because one reflection may lead to another
   m_adwRayReflectBuf[dwThread] = (m_adwRayReflectBuf[dwThread]+1)%2;
   m_alCRTRAYREFLECT[dwThread][m_adwRayReflectBuf[dwThread]].Clear();

   if (!pList->Num())
      return;  // nothing to test

   // Calculate the bounding box for reflection
   CPoint apBound[2][2];   // calculated bounding volumes
   CPoint pEnd;
   DWORD i, j, k;
   memset (apBound, 0, sizeof(apBound));
   for (i = 0; i < pList->Num(); i++, prr++) {
      for (j = 0; j < 2; j++) {
         for (k = 0; k < 3; k++) {
            pEnd.p[k] = prr->afStart[k];
            if (j)
               pEnd.p[k] += prr->afDir[k] * LONGDIST;
         }

         if (i) {
            apBound[j][0].Min (&pEnd);
            apBound[j][1].Max (&pEnd);
         }
         else {
            apBound[j][0].Copy (&pEnd);
            apBound[j][1].Copy (&pEnd);
         }
      } // j
   } // i
   CPoint apNewBound[2];
   m_pRayObjectWorld->BoundingVolumeInit (dwThread, 2 /* refelection*/, apBound[0], apBound[1], apNewBound);


   // loop
   BOOL fFind;
   CRTRAYPATH ray;
   prr = (PCRTRAYREFLECT) pList->Get(0);
   for (i = 0; i < pList->Num(); i++, prr++) {
      memset (&ray, 0, sizeof(ray));
      for (j = 0; j < 3; j++) {
         ray.pStart.p[j] = prr->afStart[j];
         ray.pDir.p[j] = prr->afDir[j];
      }

      ray.fAlpha = LONGDIST;
      ray.mToWorld.Identity();
      ray.fBeamOrig = prr->fBeamWidth;
      ray.fBeamSpread = m_fBeamSpread;
      ray.dwDontIntersect = prr->dwDontIntersect;
      ray.pDontIntersect = prr->pDontIntersect;

tryagain:
      fFind = RayIntersect (dwThread, 2 /* reflection */, &ray);
      if (!fFind) {
         // need to simulate the background color
         float afColor[3];
         WORD awColor[3];
         Gamma (m_cBackground, awColor);
         for (j = 0; j < 3; j++) {
            afColor[j] = (fp)awColor[j] / (fp) 0xffff * CM_BESTEXPOSURE;
            afColor[j] *= prr->afColor[j];
         }

         // add to the pixel
         PFIMAGEPIXEL pip;
         pip = m_pImage->Pixel (prr->wX, prr->wY);
         // assume that pixel already has an object filled in
         pip->fRed += afColor[0];
         pip->fGreen += afColor[1];
         pip->fBlue += afColor[2];
         continue;
      }

      // found pixel, get the surface info
      CRTSURFACE surf;
      if (!ray.pInterRayObject->SurfaceInfoGet (dwThread, &ray, &surf))
         continue;

      // if this is just going through a leaf then skip
      if (RayPassThroughLeaf (&ray, &surf))
         goto tryagain;

      // add reflection and refraction
      if (fMoreReflect)
         QueueReflect (dwThread, prr->wX, prr->wY, prr->afColor, &ray, &surf);

      // apply the color
      PixelFromSurface (dwThread, prr->wX, prr->wY, &ray.pStart, prr->afColor, &surf);
   } // i
}

/***************************************************************************************
CRenderRay::QueueReflect - Adds the reflection and refraction rays to the queue.

inputs
   DWORD          dwThread - 0 .. MAXRAYTHREAD-1
   DWORD          dwX, dwY - Pixels x and y
   float          *pafColorScale - Amount to scale the color by, 3 floats
   PCRTRAYPATH    pRay - Ray information that was sent into SurfaceInfoGet(). Some of
                  this info is used for queing.
   PCRTSURFACE    pSurf - Surface that has information already filled in about
                  reflection and refraction.
returns
   BOOL - TRUE if success
*/
BOOL CRenderRay::QueueReflect (DWORD dwThread, DWORD dwX, DWORD dwY, const float *pafColorScale,
                               const PCRTRAYPATH pRay, const PCRTSURFACE pSurf)
{
	MALLOCOPT_INIT;
   PCListFixed pList = &m_alCRTRAYREFLECT[dwThread][m_adwRayReflectBuf[dwThread]];
   CRTRAYREFLECT rr;
   DWORD i;
   
   if (pSurf->fReflect) {
      memset (&rr, 0, sizeof(rr));

      for (i = 0; i < 3; i++) {
         rr.afColor[i] = pSurf->afReflectColor[i] * pafColorScale[i];
         rr.afDir[i] = pSurf->pReflectDir.p[i];
         rr.afStart[i] = pSurf->pLoc.p[i];
      } // i

      // If reflection too dim then prune it out
      if ((rr.afColor[0] <= m_fMinLightLumens) && (rr.afColor[1] <= m_fMinLightLumens) && (rr.afColor[2] <= m_fMinLightLumens))
         goto toodim1;

      rr.dwDontIntersect = pRay->dwInterPolygon;
      rr.pDontIntersect = pRay->pInterRayObject;
      rr.fBeamWidth = pSurf->fBeamWidth;
      rr.wX = (WORD)dwX;
      rr.wY = (WORD)dwY;

	   MALLOCOPT_OKTOMALLOC;
      pList->Add (&rr);
	   MALLOCOPT_RESTORE;
   } // if reflect
toodim1:

   if (pSurf->fRefract) {
      memset (&rr, 0, sizeof(rr));

      for (i = 0; i < 3; i++) {
         rr.afColor[i] = pSurf->afRefractColor[i] * pafColorScale[i];
         rr.afDir[i] = pSurf->pRefractDir.p[i];
         rr.afStart[i] = pSurf->pLoc.p[i];
      } // i

      // If reflection too dim then prune it out
      if ((rr.afColor[0] <= m_fMinLightLumens) && (rr.afColor[1] <= m_fMinLightLumens) && (rr.afColor[2] <= m_fMinLightLumens))
         goto toodim2;


      rr.dwDontIntersect = pRay->dwInterPolygon;
      rr.pDontIntersect = pRay->pInterRayObject;
      rr.fBeamWidth = pSurf->fBeamWidth;
      rr.wX = (WORD)dwX;
      rr.wY = (WORD)dwY;

	   MALLOCOPT_OKTOMALLOC;
      pList->Add (&rr);
	   MALLOCOPT_RESTORE;
   } // if Refract
toodim2:

   return TRUE;
}

/***************************************************************************************
CRenderRay::PixelFromSurface - Fills in a pixel based on the surface.

what this does:
   1) If the pixel doesn't already have an object, the pixel is zeroed out and an object is set
   2) Ambient and glow colors are included in the pixel right away
   3) Database gets added to for including influence from lights
   4) Database gets added to for reflection and refraction

inputs
   DWORD                dwThread - Thread index to use so threads wont clash
   DWORD                dwX, dwY - x and y pixel
   PCPoint              pEye - Location of the eye (in world space)
   float                *pafScale - Array of 3 floats. Scale the color by this amount
   PCRTSURFACE          pSurf - Surface information from call to SurfaceInfoGet
returns
   BOOL - TRUE if success
*/
BOOL CRenderRay::PixelFromSurface (DWORD dwThread, DWORD dwX, DWORD dwY, PCPoint pEye, float *pafScale, PCRTSURFACE pSurf)
{
   PFIMAGEPIXEL pip = m_pImage->Pixel (dwX, dwY);

   // if pixel currently unwritten then set some info
   if (!pip->dwID) {
      pip->dwID = ((pSurf->dwIDMajor + 1) << 16) | pSurf->dwIDMinor;
      pip->dwIDPart = pSurf->dwIDPart;
      pip->fRed = pip->fGreen = pip->fBlue = 0;

      // calculate z value
      CPoint pCamera, p1;
      fp fLen;
      PixelToWorldSpace (dwX, dwY, 0, &pCamera);
      PixelToWorldSpace (dwX, dwY, 1, &p1);
      p1.Subtract (&pCamera);
      fLen = p1.Length();
      p1.Subtract (&pSurf->pLoc, &pCamera);
      pip->fZ = p1.Length()/ fLen;
   }

   // glow
   if (pSurf->afGlow[0] || pSurf->afGlow[1] || pSurf->afGlow[2]) {
      pip->fRed += pafScale[0] * pSurf->afGlow[0] * CM_LUMENSSUN / (fp)0xffff;
      pip->fGreen += pafScale[1] * pSurf->afGlow[1] * CM_LUMENSSUN / (fp)0xffff;
      pip->fBlue += pafScale[2] * pSurf->afGlow[2] * CM_LUMENSSUN / (fp)0xffff;
   }

   // normalize the eye vector
   CPoint pEyeNorm;
   pEyeNorm.Subtract (pEye, &pSurf->pLoc);
   pEyeNorm.Normalize();

   // ambient
   fp fTrans, fTransAngle, fNDotEye;
   fTrans = (fp)pSurf->Material.m_wTransparency / (fp)0xffff;
   if (fTrans) {
      // BUGFIX - Transparency depends on angle of viewer
      fNDotEye = fabs(pEyeNorm.DotProd (&pSurf->pNormText));
      fTransAngle = (fp) pSurf->Material.m_wTransAngle / (fp)0xffff;
      fTrans *= (fNDotEye + (1.0 - fNDotEye) * (1.0 - fTransAngle));
   }
   fTrans = 1.0 - fTrans;

   // BUGFIX - If reflective then decrease the color by that much too
   if (pSurf->Material.m_wReflectAmount)
      fTrans *= (fp)(0xffff - pSurf->Material.m_wReflectAmount) / (fp)0xffff;

   if (fTrans) {
      pip->fRed += pafScale[0] * m_afAmbient[0] * (fp)pSurf->awColor[0] / (fp)0xffff * fTrans;
      pip->fGreen += pafScale[1] * m_afAmbient[1] * (fp)pSurf->awColor[1] / (fp)0xffff * fTrans;
      pip->fBlue += pafScale[2] * m_afAmbient[2] * (fp)pSurf->awColor[2] / (fp)0xffff * fTrans;
   }

   // calculate light amt for reflection for each of the lights
   PLIGHTINFO pli;
   float afIllum[3];
   DWORD i, j;
   if (pSurf->awColor[0] || pSurf->awColor[1] || pSurf->awColor[2] || pSurf->Material.m_wSpecReflect) {
      // calculate the number of rays that will need to send to light sources
      DWORD dwNumRays = 0;
      pli = (PLIGHTINFO) m_lLIGHTINFO.Get(0);
      for (i =0 ; i < m_lLIGHTINFO.Num(); i++, pli++) {
         // only send ray if the light can cast shadows
         if (!pli->fNoShadows)
            dwNumRays++;
         // FUTURERELEASE - At some point may send multiple rays
      }

      PCRTRAYTOLIGHT prl, prlOrig;
      prl = prlOrig = NULL;
      if (dwNumRays)
         prlOrig = prl = QueueRayToLight (dwThread, dwX, dwY, &pSurf->pLoc, dwNumRays,
            pSurf->fBeamWidth, pSurf->pInterRayObject, pSurf->dwInterPolygon);

      // keep track of the maxiumum strength
      float fMax, fTemp;
      fMax = 0;

      pli = (PLIGHTINFO) m_lLIGHTINFO.Get(0);
      for (i =0 ; i < m_lLIGHTINFO.Num(); i++, pli++) {
         // BUGBUG - shadow renderer always letting some light through for infinite
         // light sources, so insides aren't totally dark. May want to do the same for ray tracing

         // FUTURERELEASE - At some point may send multiple rays
         if (!IlluminationFromLight (pSurf, pli, &pEyeNorm, afIllum)) {
            // even if no light, because may add to lights-to-calc list (and have already
            // allocated that) then add
            afIllum[0] = afIllum[1] = afIllum[2] = 0;
         }
         else {
            // else, slcale
            for (j = 0; j < 3; j++)
               afIllum[j] *= pafScale[j];
         }


         // keep track of the max
         fMax = max(max(max(fMax, afIllum[0]), afIllum[1]), afIllum[2]);

         if (pli->fNoShadows || !prl) {
            // add in if no chance of being shadowed
            pip->fRed += afIllum[0];
            pip->fGreen += afIllum[1];
            pip->fBlue += afIllum[2];
         }
         else {
            for (j = 0; j < 3; j++) {
               prl->afColor[j] = afIllum[j];
               if (pli->dwForm == LIFORM_INFINITE)
                  prl->afLightLoc[j] = -pli->pDir.p[j] * LONGDIST;   // FUTURERELEASE - If stochastic this is different
               else
                  prl->afLightLoc[j] = pli->pLoc.p[j];   // FUTURERELEASE - If stochastic this is different
               prl->dwLight = i;
            } // j
            prl++;
         }
      } // i

      // BUGFIX - go through all the rays that added. If any are so dim that less than fMax/1000
      // then eliminate. This way, if moon visible while sun is out dont send rays to the moon
      fMax *= m_fMinLight * 10;
      prl = prlOrig;
      // BUGFIX - Added if prl since sometimes no rays
      if (prl) for (i = 0; i < dwNumRays; i++, prl++) {
         fTemp = max(max(prl->afColor[0], prl->afColor[1]), prl->afColor[2]);
         if (fTemp <= fMax)
            prl->afColor[0] = prl->afColor[1] = prl->afColor[2] = 0;
      }
   } // if rays to light

   return TRUE;
}


/*******************************************************************************
CRenderRay::LightIntensity - Calculates the light intensity based on the angle
of incidence.

FUTURERELEASE - May need to modify this slightly when do volumetric light

inputs
   PLIGHTINFO        pLight - Light
   PCPoint            pLoc - Location of the point in space.
   float             *pafColor - Filled with rgb
   PCPoint           pDir - Filled with a vector from the light to the surface, normalized
returns
   none
*/
void CRenderRay::LightIntensity (PLIGHTINFO pLight, PCPoint pLoc, float *pafColor, PCPoint pDir)
{
   fp fScaleDiffuse, fScaleFront, fScaleBack;
   fScaleDiffuse = 1;
   fScaleFront = fScaleBack = 0;

   // take distance into account
   fp fDist;
   if (pLight->dwForm != LIFORM_INFINITE) {
      pDir->Subtract (pLoc, &pLight->pLoc);
      fDist = pDir->Length();
      if (fDist > EPSILON)
         fDist = 1.0 / fDist;
      pDir->Scale (fDist);
      fDist *= fDist;
      fDist = min(100,fDist);
   }
   else {
      pDir->Copy (&pLight->pDir);
      fDist = 1;
   }

   if (pLight->afLumens[0] || pLight->afLumens[1]) {
      // what's the intensity?
      fScaleFront = pDir->DotProd (&pLight->pDir);
      fScaleBack = -fScaleFront;

      DWORD i,j;
      fp afDirectCos[2][2];
      for (i = 0; i < 2; i++) {
         // calculate the cos, but only bother if have light coming from that direction
         if (pLight->afLumens[i]) {
            for (j = 0; j < 2; j++)
               afDirectCos[i][j] = cos(pLight->afDirectionality[i][j] / 2.0);
         }
         else {
            afDirectCos[i][0] = afDirectCos[i][1] = 1;
         }
      } // i

      if (fScaleFront <= afDirectCos[0][0])
         fScaleFront = 0;
      else if (fScaleFront >= afDirectCos[0][1])
         fScaleFront = 1;
      else
         fScaleFront = (fScaleFront - afDirectCos[0][0]) / (afDirectCos[0][1] - afDirectCos[0][0]);

      if (fScaleBack <= afDirectCos[1][0])
         fScaleBack = 0;
      else if (fScaleBack >= afDirectCos[1][1])
         fScaleBack = 1;
      else
         fScaleBack = (fScaleBack - afDirectCos[1][0]) / (afDirectCos[1][1] - afDirectCos[1][0]);

   }

   DWORD i;
   for (i = 0; i < 3; i++)
      pafColor[i] = ((fp)pLight->awColor[0][i] * pLight->afLumens[0] * fScaleFront +
         (fp)pLight->awColor[1][i] * pLight->afLumens[1] * fScaleBack +
         (fp)pLight->awColor[2][i] * pLight->afLumens[2] * fScaleDiffuse) / (fp) 0xffff * fDist;
}

/*******************************************************************************
CRenderRay::IlluminationFromLight - Calculates the amount of illumination from
the light. This assumes that there is nothing obstructing between the surface
and the light

FUTURERELEASE - At some point modify this so can do volumetric light.

inputs
   PCRTSURFACE          pSurf - Surface from
   PLIGHTINFO           pLight - Light going to
   PCPoint              pEyeNorm - Vector from the surface to the eye, normalized
   float                *pafIllum - FIlled with illumination amount in lumens.
returns
   BOOL - TRUE if success. FALSE if fail, maybe no light coming from it
*/
BOOL CRenderRay::IlluminationFromLight (PCRTSURFACE pSurf, PLIGHTINFO pLight, PCPoint pEyeNorm,
                                        float *pafIllum)
{
   // if looking not looking at the side the normal is on, then swap over the normals
   fp fFlip;
   fFlip = pEyeNorm->DotProd (&pSurf->pNorm);
   CPoint pNFlipText;
   pNFlipText.Copy (&pSurf->pNormText);
   if (fFlip < 0)
      pNFlipText.Scale (-1);  // yes, looking from back side

   // loop over all the lights
   float pafLightColor[3];
   float pafIntensity[3];

   // find out the light intensity
   CPoint pLightVector;
   LightIntensity (pLight, &pSurf->pLoc, pafIntensity, &pLightVector);
   pLightVector.Scale (-1);
   if ((pafIntensity[0] == 0) && (pafIntensity[1] == 0) && (pafIntensity[2] == 0))
      return FALSE;  // blackness

   // start out with no color
   pafLightColor[0] = pafLightColor[1] = pafLightColor[2] = 0;

   // if light is behind user then get rid of
   fp fNDotL, fLight;
   DWORD i;
   BOOL fUseTranslucent;
   fNDotL = pLightVector.DotProd (&pNFlipText);
   if (pSurf->Material.m_wTranslucent && (fNDotL < 0)) {
      fNDotL *= -1;
      fUseTranslucent = TRUE;
   }
   else
      fUseTranslucent = FALSE;

   if ((fNDotL > 0) /*&& !pSurf->Material.m_fSelfIllum*/) {
      // BUGFIX - amount of color depends on transparency too
      fp fTrans = 1;
      if (pSurf->Material.m_wTransparency) {
         fTrans = (fp)pSurf->Material.m_wTransparency / (fp)0xffff;

         // BUGFIX - Transparency depends on angle of viewer
         fp fNDotEye, fTransAngle;
         fNDotEye = fabs(pEyeNorm->DotProd (&pNFlipText));
         fTransAngle = (fp) pSurf->Material.m_wTransAngle / (fp)0xffff;
         fTrans *= (fNDotEye + (1.0 - fNDotEye) * (1.0 - fTransAngle));

         fTrans = 1.0 - fTrans;
      }

      // BUGFIX - If reflective then decrease the color by that much too
      if (pSurf->Material.m_wReflectAmount)
         fTrans *= (fp)(0xffff - pSurf->Material.m_wReflectAmount) / (fp)0xffff;

      // if translucent side then might be darker
      if (fUseTranslucent)
         fTrans *= (fp) pSurf->Material.m_wTranslucent / (fp)0xffff;

      // diffuse component
      fLight = fNDotL * fTrans;
      for (i = 0; i < 3; i++)
         pafLightColor[i] += fLight * (fp)pSurf->awColor[i];
   }

   // specular component
   CPoint pH;
   pH.Add (pEyeNorm, &pLightVector);
   pH.Normalize();
   fp fNDotH, fVDotH;
   fNDotH = pNFlipText.DotProd (&pH);
   //if (pSurf->Material.m_fTranslucent)  BUGFIX - Take out because dont want specularity on non-translucent side
   //   fNDotH = fabs(fNDotH);
   if (!fUseTranslucent && (fNDotH > 0) && pSurf->Material.m_wSpecReflect) {
      fVDotH = pEyeNorm->DotProd (&pH);
      fVDotH = max(0,fVDotH);
      fNDotH = pow (fNDotH, (fp)((fp)pSurf->Material.m_wSpecExponent / 100.0));
      fLight = fNDotH * (fp) pSurf->Material.m_wSpecReflect / (fp)0xffff;


      fp fPureLight, fMixed, fMax;
      if (pSurf->Material.m_wSpecPlastic > 0x8000)
         fVDotH = pow(fVDotH, (fp)(1.0 + (fp)(pSurf->Material.m_wSpecPlastic - 0x8000) / (fp)0x1000));
      else if (pSurf->Material.m_wSpecPlastic < 0x8000)
         fVDotH = pow(fVDotH, (fp)(1.0 / (1.0 + (fp)(0x8000 - pSurf->Material.m_wSpecPlastic) / (fp)0x1000)));
      fPureLight = fLight * (1.0 - fVDotH);
      fMixed = fLight * fVDotH;

      // BUGFIX - For the mixing component, offset by the maximum brightness component of
      // the object so the specularily is relatively as bright.
      //fMax = (pawColor[0] + pawColor[1] + pawColor[2]) / (fp) 0xffff;   // NOTE: Secifically not using /3.0
      fMax = (fp)max(max(pSurf->awColor[0], pSurf->awColor[1]),pSurf->awColor[2]) / (fp) 0xffff;
      if (fMax > EPSILON)
         fMixed /= fMax;

      for (i = 0; i < 3; i++)
         pafLightColor[i] += fPureLight * (fp)0xffff + fMixed * (fp)pSurf->awColor[i];
   }


   // add lights color to main color
   for (i = 0; i < 3; i++)
      pafIllum[i] = pafLightColor[i] * pafIntensity[i] / (fp)0xffff;

   return TRUE;
}


/*******************************************************************************
CRenderRay::RayPassThroughLeaf - Optimization so that if a ray hits an object that
is completely transparent at the point then it just passes through without having to
go to the reflection and refraction pass.

inputs
   PCRTRAYPATH          pRay - Use the "inter" information to determine what polygon intersected and how
   PCRTSURFACE          pSurf - Already filled by a call to SurfaceInfoGet()
returns
   BOOL - TRUE if it does pass through the leaf, in which case pRay will be modified to
         continue from the current starting point. FALSE if it doesn't pass through neatly
*/
BOOL CRenderRay::RayPassThroughLeaf (PCRTRAYPATH pRay, PCRTSURFACE pSurf)
{
   // must have had transparent, without 
   if (!pSurf->fRefract)
      return FALSE;

   // no reflection
   if (pSurf->fReflect)
      return FALSE;

   // cant be any bend, must be full transparency, and no specularity
   if ((pSurf->Material.m_wIndexOfRefract != 100) || (pSurf->Material.m_wTransparency < 0xfff0) ||
      (pSurf->Material.m_wSpecReflect))
      return FALSE;

   // no glow
   if (pSurf->afGlow[0] || pSurf->afGlow[1] || pSurf->afGlow[1])
      return FALSE;

   // ok

   // transfer over points
   pRay->dwDontIntersect = pRay->dwInterPolygon;
   pRay->fBeamOrig = pSurf->fBeamWidth;
   pRay->pDontIntersect = pRay->pInterRayObject;
   pRay->pStart.Copy (&pSurf->pLoc);
   pRay->fAlpha = LONGDIST;   // assume that not limited (aka: not just going to light)
   return TRUE;
}



/*******************************************************************************
CRenderRay::QuickSetRay - Call this to initialize the ray tracer's member variables
to they're optimum for the quality, strobe, and anti-aliasing (occurring AFTER the
ray tracer's own antialiasing). You don't need to call this; it's just easier

inputs
   DWORD             dwQuality - Quality level for ray tracing. 0 = fast ray tracing
                        with hard shadows, 1 = soft shadows, 2 = radiosity-like
   DWORD             dwStrobe - Number of strobes per frame. (Won't need to to as much
                        anti-aliasing if doing motion blur.)
   DWORD             dwAnti - Amount of antialiasing applied against the image that the
                        ray tracer produces.
returns
   none
*/
void CRenderRay::QuickSetRay (DWORD dwQuality, DWORD dwStrobe, DWORD dwAnti)
{
   // set some limits
   dwStrobe = max(dwStrobe, 1);
   dwAnti = max(dwAnti, 1);

   // use random if strobe or anti or dwQuality >= 1
   m_fRandom = (dwQuality || (dwStrobe > 1) || (dwAnti > 1));

   // set to one super sample
   m_dwSuperSample = 1;

   // max reflection depth
   m_dwMaxReflect = 3 + dwQuality * 3;

   // always antialias edge
   m_fAntiEdge = TRUE;

   // if we are doing motion blur then will need to scale the beam down a bit so that
   // textures aren't over antialiased
   m_fScaleBeam = 1.0 / sqrt((double)dwStrobe);

   // more sensative for better quality
   m_fMinLight = 1.0 / (fp)0xffff / (fp)(dwQuality+1);

   // FUTURERELEASE - Will need to tweak when do soft shadows and radiosity
}


/***************************************************************************************
CRenderRay::CloneGet - Returns a PCRayObject for a clone. For every call to CloneGet()
there must be a call to CloneRelease() to free up the object.

inputs
   GUID           *pgCode - Code ID
   GUID           *pcSub - Sub ID
   PCPoint        papBound - Pointer to an array of 2 points that are filled with min
                  and max bounding box coords (in clone's).
returns
   PCRayObject - Clone
*/
PCRayObject CRenderRay::CloneGet (GUID *pgCode, GUID *pgSub, PCPoint papBound)
{
   // find the object
   PCRTCLONECACHE pcc = (PCRTCLONECACHE) m_lCRTCLONECACHE.Get(0);
   DWORD i;
   for (i = 0; i < m_lCRTCLONECACHE.Num(); i++, pcc++) {
      if (IsEqualGUID (pcc->gSub, *pgSub) && IsEqualGUID (pcc->gCode, *pgCode)) {
         if (pcc->fRecurse)
            return NULL;   // error, object trying to contain itself

         pcc->dwCount++;
         papBound[0].Copy (&pcc->pObject->m_apBound[0]);
         papBound[1].Copy (&pcc->pObject->m_apBound[1]);
         return pcc->pObject;
      }
   }

   // else, need to create it
   PCObjectClone pClone;
   pClone = ObjectCloneGet (m_dwRenderShard, pgCode, pgSub);
   if (!pClone)
      return NULL;   // doesnt exist

   // create the entry
   CRTCLONECACHE cc;
   memset (&cc, 0, sizeof(cc));
   cc.dwCount = 1;
   cc.fRecurse = TRUE;
   cc.gCode = *pgCode;
   cc.gSub = *pgSub;
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   cc.pObject = new CRayObject (m_dwRenderShard, this, NULL, m_fCloneRender);
	MALLOCOPT_RESTORE;
   if (!cc.pObject)
      return NULL;   // error
	MALLOCOPT_OKTOMALLOC;
   m_lCRTCLONECACHE.Add (&cc);
	MALLOCOPT_RESTORE;

   // store away the old object and socket
   PCRayObject pOldRay;
   CMatrix mOldMatrix;
   BOOL fOldCloneRender;
   pOldRay = m_pRayCur;
   fOldCloneRender = m_fCloneRender;
   mOldMatrix.Copy (&m_mRenderSocket);
   m_pRayCur = cc.pObject;
   m_mRenderSocket.Identity();
   m_fCloneRender = TRUE;

   // render
   OBJECTRENDER or;
   memset (&or, 0, sizeof(or));
   or.dwReason = ORREASON_FINAL;
   or.dwSpecial = m_dwSpecial;
   or.dwShow = m_dwRenderShow;
   or.pRS = this;
   pClone->Render (&or, m_dwCloneID++); 
      // NOTE: May end up with different object number for clones than traditioanl render...

   // make sure to recalc the bounding box
   cc.pObject->CalcBoundBox();


   // restore the old object and matrix
   m_pRayCur = pOldRay;
   m_mRenderSocket.Copy (&mOldMatrix);
   m_fCloneRender = fOldCloneRender;

   // need to find again because others added
   pcc = (PCRTCLONECACHE) m_lCRTCLONECACHE.Get(0);
   for (i = 0; i < m_lCRTCLONECACHE.Num(); i++, pcc++) {
      if (IsEqualGUID (pcc->gSub, *pgSub) && IsEqualGUID (pcc->gCode, *pgCode))
         break;
   }
   if (i >= m_lCRTCLONECACHE.Num())
      return NULL;   // shouldnt happen


   // update bounding box
   papBound[0].Copy (&pcc->pObject->m_apBound[0]);
   papBound[1].Copy (&pcc->pObject->m_apBound[1]);
   
   
   // turn off recurse
   pcc->fRecurse = FALSE;

   // release clone
   pClone->Release();

   return pcc->pObject;
}


/***************************************************************************************
CRenderRay::CloneRelease - Releases a clone. Must be called for every call to CloneGet()
after the object is finished being used.


inputs
   GUID           *pgCode - Code ID
   GUID           *pcSub - Sub ID
returns
   BOOL - TRUE if success
*/
BOOL CRenderRay::CloneRelease (GUID *pgCode, GUID *pgSub)
{
   // find the object
   PCRTCLONECACHE pcc = (PCRTCLONECACHE) m_lCRTCLONECACHE.Get(0);
   DWORD i;
   for (i = 0; i < m_lCRTCLONECACHE.Num(); i++, pcc++) {
      if (IsEqualGUID (pcc->gSub, *pgSub) && IsEqualGUID (pcc->gCode, *pgCode))
         break;
   }
   if (i >= m_lCRTCLONECACHE.Num())
      return FALSE;

   if (pcc->dwCount >= 1)
      pcc->dwCount--;

   if (pcc->dwCount >= 1)
      return TRUE;   // nothing to do since still there

   // else, delete the object
   PCRayObject pObj;
   pObj = pcc->pObject;
   pcc->pObject = NULL;
   delete pObj;

   // now, because deleting the object may have deleted other clones, go through the
   // entire clone list and remove all the empty entries
   pcc = (PCRTCLONECACHE) m_lCRTCLONECACHE.Get(0);
   for (i = m_lCRTCLONECACHE.Num()-1; i < m_lCRTCLONECACHE.Num(); i--) {
      if (!pcc[i].pObject && !pcc[i].dwCount) {
         m_lCRTCLONECACHE.Remove (i);
         pcc = (PCRTCLONECACHE) m_lCRTCLONECACHE.Get(0); // since removal may have realloced
      }
   } // i


   return TRUE;
}


// BUGBUG - Show flags (such as hiding walls) dont translate as far as ray tracing goes.
// So if hide walls and ray trace, still see walls

// BUGBUG - Created an oval (actually parallel center bit and rounded at end) carpet
// from a stock object. Rendered it at night with a light coming in at an angle. It
// seems as though the texture mapping for the triangles didn't work too weel since I can
// see the ends. This may be caused by using quads instead of triangles? or something
// else?


// BUGBUG - Can this be translated to ray tracing?
// May want to make an optimization that remebers what objects are visible
// (by looking through all the pixels). Then, the next time that render, draw those
// objects first (or at least encourage them to be drawn first). This should speed
// up because the image doesn't change much from one go to another and later
// BB calls will quickly eliminate. Don't do for selected-only render

// BUGBUG - Ray tracing transmission seems slightly broken. Create eyeball and look
// at it sideways, so looking partly through cornea. Will see triangulation that shouldn
// seet because surface is smooth

// BUGBUG - If create a glass sphere (or eye using glass cornea) then the reflections
// and whatnot seem to make the ball way to bright. Need to go through the math
// and figure out what is going on.  May have fixed this.

// BUGBUG - Transparency is somewhat busted. If have grass, which is semi-transparent
// at the base, the transparency becomes really obvious, as though light is being added
// to it. May have fixed this.

// BUGBUG - there's a bug in ray tracing where if have texture overlapped over a lot
// of small polygons (such as a rock with detail 32 with test pattern on top), then
// ray tracing blurs the edges of the triangles an awful lot, which causes everything
// to be blurred. The inner parts of the triangles are ok.

// BUGBUG - bug when ray tracing metaballs. Any quads that are almost triangles, which
// is common in ray tracing, have problems with textures. Maybe be particular to
// bump maps. No such problems if have solid surface

// BUGBUG - ray tracing breaks down with faces because some polygons become so small (millimeters)
// that my 1 mm test (and/or numerical error) become a problem
