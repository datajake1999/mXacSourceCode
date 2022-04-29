/************************************************************************
CRenderTraditional.cpp - Does a traditional 3D renderer (gauraud shading).
No ray tracing, etc.

begun 6/9/01 by Mike Rozak
Copyright 2001 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\M3D.h"


// #define USEZIMAGESMALL                 // to use small ZImage
   // BUGFIX - Take out because makes a little bit slower

#define ZIMAGESMALLSCALE               8     // every pixel in m_pZImageSmall represens 8x8 pixels in original image

#define  CLIPPLANE_FLOORWIREFRAME      0x37592343

#ifdef _TIMERS
RENDERSTATS gRenderStats;
#endif


#define RTCLONE_MINTHREAD        (4 * RTTHREADSCALE)           // minimum number of threads before use clones, quadcore
#define RTCLONE_LOD              3           // levels of detail for clones

#define RTCLONE_LODSCALE         4.0         // lowest LOD is full with of object * RTCLONE_LODLOWEST, then 1/4*RTCLONE_LODLOWEST witdth, then 1/16th*RTCLONE_LODLOWEST, then 1/64th*RTCLONE_LODLOWEST, etc.
                                             // Best LOD is always 0
#define RTCLONE_LODLOWEST        0.5         // lowest LOD is half the width of the object

// RTCLONEINFO - For storing information about traditional render clones
typedef struct {
   GUID           gCode;      // object ID
   GUID           gSub;       // object sub ID
   BOOL           fRecurse;   // if TRUE already processing. This prevents clones from containing themselves
   DWORD          dwCount;    // reference count, when this goes to 0 free it up
   PCRayObject    apRayObject[RTCLONE_LOD];  // render objects, [0] = greatest detail, [RTCLONE_LOD-1] is least
} RTCLONEINFO, *PRTCLONEINFO;

DWORD       gdwCPSize = 1;        // control point size

/*******************************************************************************
RenderCalcCenterScale - Calculates the center and scal pixel.

inputs
   DWORD          dwWidth - Width in pixels
   DWORD          dwHeight - Height in pixels
   fp             *pafCenterPixel - Filled with [0] = x, [1] = y center
   fp             *pafScalePixel - Filled with [0] = x, [1] = y scale
returns
   none
*/
void RenderCalcCenterScale (DWORD dwWidth, DWORD dwHeight, fp *pafCenterPixel, fp *pafScalePixel)
{
   // and remember for scaling after perpective
   // BUGFIX - Make new centerpixel and scalepixel
   pafCenterPixel[0] = (fp) (dwWidth-1) / 2.0;
   pafCenterPixel[1] = (fp) (dwHeight-1) / 2.0;
   pafScalePixel[0] = (fp) dwWidth / 2.0;
   pafScalePixel[1] = -(fp) dwHeight / 2.0;

   //m_afCenterPixel[0] = (fp) dwWidth / 2.0 + 0.5;   // need +0.5 so it rounds off properly
   //m_afCenterPixel[1] = (fp) dwHeight / 2.0 + 0.5;  // need +0.5 so it rounds off properly
   //m_afScalePixel[0] = (fp) (dwWidth+2) / 2.0; // BUGFIX - Did +1 to width and height do ends up drawing UL corner
   //m_afScalePixel[1] = -(fp) (dwHeight+2) / 2.0;
}


/*************************************************************************************
SCANLINEINTERPCalc - Calculates the scanline interp from for points

inputs
   BOOL           fHomogeneous - Set to TRUE if need homogeneous interpolation, FALSE for 3D space
   DWORD          dwPixels - Number of horizontal pixels in the scanline
   PCPoint        pBaseLeft - Left at the base location
   PCPoint        pBaseRight - Right value at the base location
   PCPoint        pBasePlusMeterLeft - Base plus a meter depth, at the left
   PCPoint        pBasePlusMeterRight - Right base plus a meter depth
   PSCANLINEINTERP   pSLI - Filled in
returns
   none
*/
void SCANLINEINTERPCalc (BOOL fHomogeneous, DWORD dwPixels, PCPoint pBaseLeft, PCPoint pBaseRight,
                         PCPoint pBasePlusMeterLeft, PCPoint pBasePlusMeterRight,
                         PSCANLINEINTERP pSLI)
{
   pSLI->fUse = TRUE;
   pSLI->fHomogeneous = fHomogeneous;
   pSLI->fBaseDeltaIsZero = pBaseLeft->AreClose(pBaseRight) &&
      (fHomogeneous ? (fabs(pBaseLeft->p[3] - pBaseRight->p[3]) < CLOSE) : TRUE);

   DWORD dwNum = fHomogeneous ? 4 : 3;

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      pSLI->afBaseCur[i] = pSLI->afBaseLeft[i] = pBaseLeft->p[i];
      pSLI->afBaseDelta[i] = (double)(pBaseRight->p[i] - pBaseLeft->p[i]) / (double)dwPixels;
      pSLI->afPerMeterCur[i] = pSLI->afPerMeterLeft[i] = (double)(pBasePlusMeterLeft->p[i] - pBaseLeft->p[i]);
      pSLI->afPerMeterDelta[i] = (double)(pBasePlusMeterRight->p[i] - pBaseRight->p[i] - pBasePlusMeterLeft->p[i] + pBaseLeft->p[i]) / (double)dwPixels;
   } // i
}


/*************************************************************************************
SCANLINEINTERPFromXAndZ - Given an X and a Z, this fills in a point.

inputs
   PSCANLINEINTERP   pSLI - Info calcualted by SCANLINEINTERPCalc
   double            fX - X value (in pixels)
   double            fZ - Z value (in meters)
   PCPoint           p - Filled in
returns
   none
*/
__inline void SCANLINEINTERPFromXAndZ (PSCANLINEINTERP pSLI, double fX, double fZ, PCPoint p)
{
   if (pSLI->fBaseDeltaIsZero) {
      p->p[0] = pSLI->afBaseLeft[0] + (pSLI->afPerMeterLeft[0] + fX * pSLI->afPerMeterDelta[0])*fZ;
      p->p[1] = pSLI->afBaseLeft[1] + (pSLI->afPerMeterLeft[1] + fX * pSLI->afPerMeterDelta[1])*fZ;
      p->p[2] = pSLI->afBaseLeft[2] + (pSLI->afPerMeterLeft[2] + fX * pSLI->afPerMeterDelta[2])*fZ;
   }
   else {
      p->p[0] = pSLI->afBaseLeft[0] + fX * pSLI->afBaseDelta[0] + (pSLI->afPerMeterLeft[0] + fX * pSLI->afPerMeterDelta[0])*fZ;
      p->p[1] = pSLI->afBaseLeft[1] + fX * pSLI->afBaseDelta[1] + (pSLI->afPerMeterLeft[1] + fX * pSLI->afPerMeterDelta[1])*fZ;
      p->p[2] = pSLI->afBaseLeft[2] + fX * pSLI->afBaseDelta[2] + (pSLI->afPerMeterLeft[2] + fX * pSLI->afPerMeterDelta[2])*fZ;
   }

   if (!pSLI->fHomogeneous)
      p->p[3] = 1.0; // just to make sure
   else {
      // homogeneous
      if (pSLI->fBaseDeltaIsZero)
         p->p[3] = pSLI->afBaseLeft[3] + (pSLI->afPerMeterLeft[3] + fX * pSLI->afPerMeterDelta[3])*fZ;
      else
         p->p[3] = pSLI->afBaseLeft[3] + fX * pSLI->afBaseDelta[3] + (pSLI->afPerMeterLeft[3] + fX * pSLI->afPerMeterDelta[3])*fZ;
   }
}



/*************************************************************************************
SCANLINEINTERPAdvanceX - Increase the XXXCur values by one pixel worth

inputs
   PSCANLINEINTERP   pSLI - Info calcualted by SCANLINEINTERPCalc
returns
   none
*/
__inline void SCANLINEINTERPAdvanceX (PSCANLINEINTERP pSLI)
{
   // make sure want to use
   if (!pSLI->fUse)
      return;

   if (!pSLI->fBaseDeltaIsZero) {
      pSLI->afBaseCur[0] += pSLI->afBaseDelta[0];
      pSLI->afBaseCur[1] += pSLI->afBaseDelta[1];
      pSLI->afBaseCur[2] += pSLI->afBaseDelta[2];
      if (pSLI->fHomogeneous)
         pSLI->afBaseCur[3] += pSLI->afBaseDelta[3];
   }

   pSLI->afPerMeterCur[0] += pSLI->afPerMeterDelta[0];
   pSLI->afPerMeterCur[1] += pSLI->afPerMeterDelta[1];
   pSLI->afPerMeterCur[2] += pSLI->afPerMeterDelta[2];
   if (pSLI->fHomogeneous)
      pSLI->afPerMeterCur[3] += pSLI->afPerMeterDelta[3];
}


/*************************************************************************************
RendCPSizeGet - Returns the size of the control point

returns
   DWORD - size
*/
DWORD RendCPSizeGet (void)
{
   return gdwCPSize;
}

/*************************************************************************************
RendCPSizeSet - Sets the size of the control point

inputs
   DWORD          dwSize - size
*/
void RendCPSizeSet (DWORD dwSize)
{
   gdwCPSize = dwSize;
}

/*************************************************************************************
CRenderTraditional:: Constructor and destructor
*/
CRenderTraditional::CRenderTraditional (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;

   // make sure gamma initialize
   GammaInit ();
   m_pEffectOutline = new CNPREffectOutline(m_dwRenderShard);
   m_pEffectFog = new CNPREffectFog(m_dwRenderShard);
   m_pEffectFog->m_fFog = FALSE; // default to false

   m_dwThreadsMax = m_dwThreadsPoly = m_dwThreadsShadows = (DWORD)-1;   // so know not initialized
   m_pImage = NULL;  // BUGFIX - Added
   m_pZImage = NULL;
   m_pFImage = NULL;
   m_dwClouds = 0;
   m_dwDrawPlane = (DWORD)-1;
   m_lPCLight.Init (sizeof(PCLight));
   m_lLIGHTINFO.Init (sizeof(LIGHTINFO));
   m_pImageShader = NULL;
   m_pProgress = NULL;
   m_pWorld = NULL;
   m_fBackfaceCull = TRUE;
   m_dwRenderShow = RENDERSHOW_ALL;
   m_fFinalRender = FALSE;
   m_fForShadows = FALSE;
   m_dwCacheImageValid = 0;   // none of the image caches are valid
   m_fSelectedBound = TRUE;
   m_dwRenderModel = RENDERMODEL_SURFACETEXTURE;
   m_cMono = RGB(0xc0, 0xc0, 0xc0);
   m_fExposure = CM_BESTEXPOSURE;
   m_dwIntensityAmbient1 = 0x10000;
   m_dwIntensityAmbient2 = 0x10000/3;  // will be overriden if call LightVectorSet (date,time)
   m_dwIntensityAmbientExtra = 0;
   m_dwIntensityLight = 0x10000 / 3 * 2;
   m_pLightAmbientColor.p[0] = m_pLightAmbientColor.p[1] = m_pLightAmbientColor.p[2] = 1;
   //m_pLightSunColor.p[0] = m_pLightSunColor.p[1] = m_pLightSunColor.p[2] = 1.0;
   //m_pLightAmbientColor.Copy (&m_pLightSunColor);
   //m_pLightMoonColor.Copy (&m_pLightSunColor);
   m_cBackground = RGB(210,210,0xff);
   m_afCenterPixel[0] = m_afCenterPixel[1] = 0;
   m_afScalePixel[0] = m_afScalePixel[1] = 1;
   m_fWantNormalsForThis = TRUE;
   m_fWantTexturesForThis = TRUE;
   m_fPassCameraValid = FALSE;
   m_adwWantClipThis[0] = (DWORD)-1; // ignore others
   m_fDetailForThis = 0.1;
   m_adwObjectID[0] = 1 << 16;   // ignore others
   m_lightWorld.p[0] = -1;
   m_lightWorld.p[1] = -1;
   m_lightWorld.p[2] = 1;
   m_lightWorld.Normalize();
   m_lightView.Copy(&m_lightWorld);
   m_dwCameraModel = CAMERAMODEL_PERSPOBJECT;
   FOVSet (PI/4);
   m_CameraLookFrom.Zero();
   m_CameraLookFrom.p[1] = -20;
   m_CameraLookFrom.p[2] = CM_EYEHEIGHT;
   m_fCameraLongitude = m_fCameraLatitude = m_fCameraTilt = 0;
   m_CameraObjectLoc.Zero();
   m_CameraObjectLoc.p[1] = 20;
   m_fCameraObjectRotX = m_fCameraObjectRotY = m_fCameraObjectRotZ = 0;
   m_fClipNear = EPSILON;
   m_fClipFar = -1;
   m_fClipNearFlat = -10000;
   m_fClipFarFlat = 10000;
   m_fCameraFlatLongitude = 0;
   m_fCameraFlatTiltX = PI/2;
   m_fCameraFlatTiltY = 0;
   m_fCameraFlatScale = 20;
   m_fCameraFlatTransX = 0;
   m_fCameraFlatTransY = 0;
   m_fDetailPixels = 5;  // 5 pixels
   m_CameraMatrixRotAfterTrans.Identity();
   m_CameraMatrixRotOnlyAfterTrans.Identity();
   m_WorldToView.Identity();
   m_CameraObjectCenter.Zero();
   m_CameraFlatCenter.Zero();
   m_listRTCLIPPLANE.Init (sizeof(RTCLIPPLANE));
   m_fDrawControlPointsOnSel = FALSE;
   m_pRPPIXEL = NULL;    // scratch used so know if drawing to RPPIXEL
   m_plRPTEXTINFO = NULL;   // scratch used so know if drawing to RPPIXEL
   m_pRPView = NULL;
   m_pRPMatrix = NULL;
   m_pZImageSmall = NULL;

   // m_fRBGlasses = FALSE;
   // m_fRBRightEyeDominant = FALSE;
   // m_fRBEyeDistance = 0.1;
   m_dwSpecial = ORSPECIAL_NONE;
   m_dwShadowsFlags = 0;   // no flags turned on
   m_fShadowsLimit = 0.0;
   m_fShadowsLimitCenteredOnCamera = FALSE;
   m_iPriorityIncrease = 0;

   m_dwShowOnly = -1;
   m_dwSelectOnly = -1;
   m_fIgnoreSelection = FALSE;
   m_fBlendSelAbove = FALSE;

   m_fGridDraw = FALSE;
   m_mGridFromWorld.Identity();
   m_fGridDrawUD = FALSE;
   m_fGridDrawDots = FALSE;
   m_fGridMajorSize = 1.0;
   m_fGridMinorSize = .2;
   m_crGridMajor = RGB(0xff,0,0);
   m_crGridMinor = RGB(0,0xff,0);

   InitializeCriticalSection (&m_RTCLONECritSec);
   m_lRTCLONEINFO.Init (sizeof(RTCLONEINFO));
   m_mRenderSocket.Identity();
   m_pRayCur = NULL;
   m_fCloneRender = FALSE;
   m_dwCloneID = 0;

   m_dwForceCache = 0;
   m_pRenderForLight = NULL;

   // start the sun in evening in darwin
   // BUGFIX - Had TODFDATE(22,3,2001) instead of gdwToday, but messes up trees
   // BUGFIX - Call default building settgings
   DWORD dwDate, dwTime;
   fp fLat;
   DefaultBuildingSettings (&dwDate, &dwTime, &fLat);
   //LightVectorSet (dwTime, dwDate, fLat, 0);

   // default to one setting
   CPoint p, pCenter;
   p.Zero ();
   p.p[1] = 20;
   pCenter.Zero();
   CameraPerspObject (&p, &pCenter, 0, 0, 0, PI/4);
#if 0
   CPoint pFrom;
   pFrom.Zero();
   pFrom.p[1] = -15;
   pFrom.p[2] = 1;
   CameraPerspWalkthrough (&pFrom); // , PI/12, PI/12, PI/12);
#endif
}

CRenderTraditional::~CRenderTraditional (void)
{
   // end the theads on the destructor
   if (m_dwThreadsMax != (DWORD)-1)
      MultiThreadedEnd ();

   if (m_pImageShader)
      delete m_pImageShader;
   m_pImageShader = NULL;

   if (m_pZImageSmall)
      delete m_pZImageSmall;
   m_pZImageSmall = NULL;

   DWORD i;
   for (i = 0; i < m_lPCLight.Num(); i++) {
      PCLight pl = *((PCLight*)m_lPCLight.Get(i));
      delete pl;
   }
   m_lPCLight.Clear();
   // intentionally left blank

   if (m_pEffectOutline)
      delete m_pEffectOutline;
   if (m_pEffectFog)
      delete m_pEffectFog;

   if (m_pRenderForLight)
      delete m_pRenderForLight;

   DeleteCriticalSection (&m_RTCLONECritSec);
}

/*******************************************************************************
CRenderTraditional::Width - Returns the width of the image in pixels
*/
__inline DWORD CRenderTraditional::Width (void)
{
   if (m_pImage)
      return m_pImage->Width();
   else if (m_pFImage)
      return m_pFImage->Width();
   else // if (m_pZImage)
      return m_pZImage->Width();
  // else
  //    return 0;
}


/*******************************************************************************
CRenderTraditional::Height - Returns the Height of the image in pixels
*/
__inline DWORD CRenderTraditional::Height (void)
{
   if (m_pImage)
      return m_pImage->Height();
   else if (m_pFImage)
      return m_pFImage->Height();
   else // if (m_pZImage)
      return m_pZImage->Height();
  // else
  //    return 0;
}



/*******************************************************************************
CRenderTraditional::CloneLightsTo - Clone the light information to another renderer.
When actually call render with the cloned-to renderer, make sure NOT
to set dwWorldChanged flags, or all will be lost.

inputs
   PCRenderTraditional        pClone - Clone to
*/
void CRenderTraditional::CloneLightsTo (PCRenderTraditional pClone)
{
   // clear what have
   DWORD i;
   PCLight pl;
   for (i = 0; i < pClone->m_lPCLight.Num(); i++) {
      pl = *((PCLight*)pClone->m_lPCLight.Get(i));
      delete pl;
   }
   pClone->m_lPCLight.Clear();

   // init from this one
   pClone->m_lPCLight.Init (sizeof(PCLight), m_lPCLight.Get(0), m_lPCLight.Num());
   PCLight *ppl = (PCLight*)pClone->m_lPCLight.Get(0);
   for (i = 0; i < pClone->m_lPCLight.Num(); i++) {

      _ASSERTE (ppl[i]->HasShadowBuf());

      ppl[i] = ppl[i]->Clone();
   }
}


/*******************************************************************************
CRenderTraditional::CloneTo - Copies settings to another render traiditonal
to clone everything
*/
void CRenderTraditional::CloneTo (PCRenderTraditional pClone)
{
   pClone->m_fBackfaceCull = m_fBackfaceCull;
   pClone->m_dwRenderShow = m_dwRenderShow;
   pClone->m_fFinalRender = m_fFinalRender;
   pClone->m_fForShadows = m_fForShadows;

   pClone->m_fSelectedBound = m_fSelectedBound;
   pClone->m_dwRenderModel = m_dwRenderModel;
   pClone->m_cMono = m_cMono;
   pClone->m_fExposure = m_fExposure;
   pClone->m_dwIntensityAmbient1 = m_dwIntensityAmbient1;
   pClone->m_dwIntensityAmbient2 = m_dwIntensityAmbient2;
   pClone->m_dwIntensityLight = m_dwIntensityLight;
   pClone->m_pLightAmbientColor.Copy (&m_pLightAmbientColor);
   pClone->m_dwIntensityAmbientExtra = m_dwIntensityAmbientExtra;
   pClone->m_cBackground = m_cBackground;
   //pClone->m_cBackgroundMono = m_cBackgroundMono;
   pClone->m_fDetailPixels = m_fDetailPixels;
   // pClone->m_fRBGlasses = m_fRBGlasses;
   pClone->m_dwSpecial = m_dwSpecial;
   pClone->m_dwShadowsFlags = m_dwShadowsFlags;
   pClone->m_iPriorityIncrease = m_iPriorityIncrease;
   pClone->m_fShadowsLimit = m_fShadowsLimit;
   pClone->m_fShadowsLimitCenteredOnCamera = m_fShadowsLimitCenteredOnCamera;
   // pClone->m_fRBRightEyeDominant = m_fRBRightEyeDominant;
   // pClone->m_fRBEyeDistance = m_fRBEyeDistance;
   pClone->m_dwClouds = m_dwClouds;
   pClone->m_dwDrawPlane = m_dwDrawPlane;
   pClone->m_CameraMatrixRotAfterTrans.Copy (&m_CameraMatrixRotAfterTrans);
   pClone->m_WorldToView.Copy (&m_WorldToView);
   pClone->m_CameraMatrixRotOnlyAfterTrans.Copy (&m_CameraMatrixRotOnlyAfterTrans);
   pClone->m_fGridDraw = m_fGridDraw;
   pClone->m_fIgnoreSelection = m_fIgnoreSelection;
   pClone->m_fBlendSelAbove = m_fBlendSelAbove;
   pClone->m_dwShowOnly = m_dwShowOnly;
   pClone->m_dwSelectOnly = m_dwSelectOnly;
   pClone->m_mGridFromWorld.Copy (&m_mGridFromWorld);
   pClone->m_fGridDrawUD = m_fGridDrawUD;
   pClone->m_fGridDrawDots = m_fGridDrawDots;
   pClone->m_fGridMajorSize = m_fGridMajorSize;
   pClone->m_fGridMinorSize = m_fGridMinorSize;
   pClone->m_crGridMajor = m_crGridMajor;
   pClone->m_crGridMinor = m_crGridMinor;
   pClone->m_fDrawControlPointsOnSel = m_fDrawControlPointsOnSel;
   // dont do m_pImage;               // image to use
   // dont do m_pFImage
   // dont do m_afCenterPixel[2];     // center of image. 1/2 m_pImage->m_dwWidth and m_pImage->m_dwHeight
   // dont do m_afScalePixel[2];      // scaling of pixels so -1 to 1 goes to 0 to m_dwImageWidth,height

   pClone->m_pWorld = m_pWorld;

   // dont do m_fWantNormalsForThis;  // TRUE if want normals from this object, FALSE if not
   // dont do m_fWantTexturesForThis; // TRUE if the renderer wants textures
   // dont do m_dwWantClipThis;       // bit-flag for clipping planes that the object crosses and need to clip for
   // dont do m_fDetailForThis;       // amount of detail resolution (in meters) that want for this object
   // dont do m_dwObjectID;           // object ID to use for this. HIWORD is valid, LOWORD is 0's.

   m_aRenderMatrix[0].CloneTo (&pClone->m_aRenderMatrix[0]);
   // dont bother cloning others

   pClone->m_lightWorld.Copy (&m_lightWorld);
   pClone->m_lightView.Copy (&m_lightView);

   if (pClone->m_pEffectOutline)
      delete pClone->m_pEffectOutline;
   pClone->m_pEffectOutline = m_pEffectOutline ? m_pEffectOutline->CloneEffect() : NULL;
   if (pClone->m_pEffectFog)
      delete pClone->m_pEffectFog;
   pClone->m_pEffectFog = m_pEffectFog ? m_pEffectFog->CloneEffect() : NULL;

   // camera models
   pClone->m_dwCameraModel = m_dwCameraModel;
   pClone->m_fCameraFOV = m_fCameraFOV;
   pClone->m_fTanHalfCameraFOV = m_fTanHalfCameraFOV;
   pClone->m_CameraLookFrom.Copy (&m_CameraLookFrom);
   pClone->m_fCameraLongitude = m_fCameraLongitude;
   pClone->m_fCameraLatitude = m_fCameraLatitude;
   pClone->m_fCameraTilt = m_fCameraTilt;
   pClone->m_CameraObjectLoc.Copy (&m_CameraObjectLoc);
   pClone->m_fCameraObjectRotX = m_fCameraObjectRotX;
   pClone->m_fCameraObjectRotY = m_fCameraObjectRotY;
   pClone->m_fCameraObjectRotZ = m_fCameraObjectRotZ;
   pClone->m_fCameraFlatLongitude = m_fCameraFlatLongitude;
   pClone->m_fCameraFlatTiltX = m_fCameraFlatTiltX;
   pClone->m_fCameraFlatTiltY = m_fCameraFlatTiltY;
   pClone->m_fCameraFlatScale = m_fCameraFlatScale;
   pClone->m_fCameraFlatTransX = m_fCameraFlatTransX;
   pClone->m_fCameraFlatTransY = m_fCameraFlatTransY;
   pClone->m_fClipNear = m_fClipNear;
   pClone->m_fClipFar = m_fClipFar;
   pClone->m_fClipNearFlat = m_fClipNearFlat;
   pClone->m_fClipFarFlat = m_fClipFarFlat;
   pClone->m_CameraObjectCenter.Copy (&m_CameraObjectCenter);
   pClone->m_CameraFlatCenter.Copy (&m_CameraFlatCenter);
   pClone->m_listRTCLIPPLANE.Init (sizeof(RTCLIPPLANE), m_listRTCLIPPLANE.Get(0), m_listRTCLIPPLANE.Num());      // list of clipping planes

   pClone->m_dwCacheImageValid = FALSE;
   pClone->ClipRebuild ();

   // clone the shader
   if (pClone->m_pImageShader)
      delete pClone->m_pImageShader;
   pClone->m_pImageShader = NULL;
   if (m_pImageShader)
      pClone->m_pImageShader = m_pImageShader->Clone();

   // BUGFIX - Put this in so cloneto would copy over all the matrices properly
   // refresh the matrices
   CPoint p, pCenter;
   fp fX, fY, fZ, fFOV;
   fp fLong, fLat;
   fp fLongitude, fTilt, fTiltY, fScale, fTransX, fTransY;
   switch (pClone->CameraModelGet()) {
   case CAMERAMODEL_FLAT:
      CameraFlatGet (&pCenter, &fLongitude, &fTilt, &fTiltY, &fScale, &fTransX, &fTransY);
      pClone->CameraFlat (&pCenter, fLongitude, fTilt, fTiltY, fScale, fTransX, fTransY);
      break;

   case CAMERAMODEL_PERSPOBJECT:
      CameraPerspObjectGet (&p, &pCenter, &fZ, &fX, &fY, &fFOV);
      pClone->CameraPerspObject (&p, &pCenter, fZ, fX, fY, fFOV);
      break;

   case CAMERAMODEL_PERSPWALKTHROUGH:
      CameraPerspWalkthroughGet (&p, &fLong, &fLat, &fTilt, &fFOV);
      pClone->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, fFOV);
      break;
   }

   // BUGFIX - Also call into the image set to reset the image width and height
   // paramters
   if (pClone->m_pImage)
      pClone->CImageSet (pClone->m_pImage);
   if (pClone->m_pZImage)
      pClone->CZImageSet (pClone->m_pZImage);
   if (pClone->m_pFImage)
      pClone->CFImageSet (pClone->m_pFImage);
}


static PWSTR gszBackfaceCull = L"BackfaceCull";
static PWSTR gszOutline = L"Outline";
static PWSTR gszRenderShow = L"RenderShow";
static PWSTR gszOutlineColor = L"OutlineColor";
static PWSTR gszOutlineSelected = L"OutlineSelected";
static PWSTR gszSelectedBound = L"SelectedBound";
static PWSTR gszRenderModel = L"RenderModel";
static PWSTR gszMono = L"Mono";
static PWSTR gszIntensityAmbient1 = L"IntensityAmbient1";
static PWSTR gszIntensityAmbient2 = L"IntensityAmbient2";
static PWSTR gszIntensityLight = L"IntensityLight";
static PWSTR gszBackground = L"Background";
static PWSTR gszFog = L"Fog";
static PWSTR gszFogStart = L"FogStart";
static PWSTR gszFogStartFlat = L"FogStartFlat";
static PWSTR gszFogThickness = L"FogThickness";
static PWSTR gszDetailMax = L"DetailMax";
static PWSTR gszDetailPixels = L"DetailPixels";
static PWSTR gszRBGlasses = L"RBGlasses";
static PWSTR gszRBRightEyeDominant = L"RBRightEyeDominant";
static PWSTR gszRBEyeDistance = L"RBEyeDistance";
static PWSTR gszSunDate = L"SunDate";
static PWSTR gszSunTime = L"SunTime";
static PWSTR gszSunLatitude = L"SunLatitude";
static PWSTR gszCameraMatrixRotAfterTrans = L"CameraMatrixRotAfterTrans";
static PWSTR gszCameraMatrixRotOnlyAfterTrans = L"CameraMatrixRotOnlyAfterTrans";
static PWSTR gszFloorWireframe = L"FloorWireframe";
static PWSTR gszFloorWireframeZ = L"FloorWireframeZ";
static PWSTR gszGridDraw = L"GridDraw";
static PWSTR gszGridDrawFromWorld = L"GridDrawFromWorld";
static PWSTR gszGridDrawUD = L"GridDrawUD";
static PWSTR gszGridFromWorld = L"GridFromWorld";
static PWSTR gszGridDrawDots = L"GridDrawDots";
static PWSTR gszGridMajorSize = L"GridMajorSize";
static PWSTR gszGridMinorSize = L"GridMinorSize";
static PWSTR gszGridMajorColor = L"GridMajorColor";
static PWSTR gszGridMinorColor = L"GridMinorColor";
static PWSTR gszDrawControlPointsOnSel = L"DrawControlPointsOnSel";
static PWSTR gszRenderMatrix = L"RenderMatrix";
static PWSTR gszLightWorld = L"LightWorld";
static PWSTR gszLightView = L"LightView";
static PWSTR gszCameraModel = L"CameraModel";
static PWSTR gszCameraFOV = L"CameraFOV";
static PWSTR gszCameraLookFrom = L"CameraLookFrom";
static PWSTR gszCameraLongitude = L"CameraLongitude";
static PWSTR gszCameraLatitude = L"CameraLatitude";
static PWSTR gszCameraTilt = L"CameraTilt";
static PWSTR gszCameraObjectLoc = L"CameraObjectLoc";
static PWSTR gszCameraObjectRotX = L"CameraObjectRotX";
static PWSTR gszCameraObjectRotY = L"CameraObjectRotY";
static PWSTR gszCameraObjectRotZ = L"CameraObjectRotZ";
static PWSTR gszCameraFlatLongitude = L"CameraFlatLongitude";
static PWSTR gszCameraFlatTiltX = L"CameraFlatTiltX";
static PWSTR gszCameraFlatTiltY = L"CameraFlatTiltY";
static PWSTR gszCameraFlatScale = L"CameraFlatScale";
static PWSTR gszCameraFlatTransX = L"CameraFlatTransX";
static PWSTR gszCameraFlatTransY = L"CameraFlatTransY";
static PWSTR gszClipNear = L"ClipNear";
static PWSTR gszClipFar = L"ClipFar";
static PWSTR gszClipNearFlat = L"ClipNearFlat";
static PWSTR gszClipFarFlat = L"ClipFarFlat";
static PWSTR gszCameraObjectCenter = L"CameraObjectCenter";
static PWSTR gszCameraFlatCenter = L"CameraFlatCenter";
static PWSTR gszClipPlane = L"ClipPlane";
static PWSTR gszClipPlaneP0 = L"ClipPlaneP0";
static PWSTR gszClipPlaneP1 = L"ClipPlaneP1";
static PWSTR gszClipPlaneP2 = L"ClipPlaneP2";
static PWSTR gszClipPlaneID = L"ClipPlaneID";
static PWSTR gszDontDrawTransparent = L"DontDrawTransparent";
static PWSTR gszExposure = L"Exposure";
static PWSTR gszClouds = L"Clouds";
static PWSTR gszIntensityAmbientExtra = L"IntensityAmbientExtra";
static PWSTR gszLightSunColor = L"LightSunColor";
static PWSTR gszLightMoonColor = L"LightMoonColor";
static PWSTR gszLightAmbientColor = L"LightAmbientColor";
static PWSTR gszBackgroundMono = L"BackgroundMono";
static PWSTR gszSunWas = L"SunWas";
static PWSTR gszMoonWas = L"MoonWas";
static PWSTR gszMoonVector = L"MoonVector";
static PWSTR gszMoonPhase = L"MoonPhase";
static PWSTR gszDrawPlane = L"DrawPlane";
static PWSTR gpszIgnoreSelection = L"IgnoreSelection";
static PWSTR gpszShowOnly = L"ShowOnly";
static PWSTR gpszSpecial = L"Special";
static PWSTR gpszSelectOnly = L"SelectOnly";
static PWSTR gpszBlendSelAbove = L"BlendSelAbove";
static PWSTR gpszEffectOutline = L"EffectOutline";
static PWSTR gpszEffectFog = L"EffectFog";
static PWSTR gpszShadowsFlags = L"ShadowsFlags";
static PWSTR gpszShadowsLimit = L"ShadowsLimit";
static PWSTR gpszShadowsLimitCenteredOnCamera = L"ShadowsLimitCenteredOnCamera";

/*******************************************************************************
CRenderTraditional::MMLTo - Writes information to the MML node
*/
PCMMLNode2 CRenderTraditional::MMLTo (void)
{
   PCMMLNode2 pNode;
   pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   MMLValueSet (pNode, gszBackfaceCull, (int) m_fBackfaceCull);
   MMLValueSet (pNode, gszRenderShow, (int) m_dwRenderShow);
   // NOTE: not saving m_fFinalRender
   // NOTE: Not saving m_fForShadows
   MMLValueSet (pNode, gszSelectedBound, (int) m_fSelectedBound);
   MMLValueSet (pNode, gszRenderModel, (int) m_dwRenderModel);
   MMLValueSet (pNode, gszMono, (int) m_cMono);
   MMLValueSet (pNode, gszExposure, m_fExposure);
   MMLValueSet (pNode, gszIntensityAmbient1, (int) m_dwIntensityAmbient1);
   MMLValueSet (pNode, gszIntensityAmbient2, (int) m_dwIntensityAmbient2);
   MMLValueSet (pNode, gszIntensityAmbientExtra, (int) m_dwIntensityAmbientExtra);
   MMLValueSet (pNode, gszIntensityLight, (int) m_dwIntensityLight);
   MMLValueSet (pNode, gszLightAmbientColor, &m_pLightAmbientColor);
   MMLValueSet (pNode, gszBackground, (int) m_cBackground);
   //MMLValueSet (pNode, Back()groundMono, (int)m_cBackgroundMono);
   MMLValueSet (pNode, gszDetailPixels, m_fDetailPixels);
   // MMLValueSet (pNode, gszRBGlasses, (int) m_fRBGlasses);
   MMLValueSet (pNode, gpszSpecial, (int)m_dwSpecial);
   MMLValueSet (pNode, gpszShadowsFlags, (int)m_dwShadowsFlags);
   // nOTE: Not setting/getting m_iPriorityIncrease
   MMLValueSet (pNode, gpszShadowsLimit, m_fShadowsLimit);
   MMLValueSet (pNode, gpszShadowsLimitCenteredOnCamera, (int)m_fShadowsLimitCenteredOnCamera);
   // MMLValueSet (pNode, gszRBRightEyeDominant, (int) m_fRBRightEyeDominant);
   // MMLValueSet (pNode, gszRBEyeDistance, m_fRBEyeDistance);
   MMLValueSet (pNode, gszClouds, (int) m_dwClouds);
   MMLValueSet (pNode, gszDrawPlane, (int) m_dwDrawPlane);
   MMLValueSet (pNode, gszCameraMatrixRotAfterTrans, &m_CameraMatrixRotAfterTrans);
   MMLValueSet (pNode, gszCameraMatrixRotOnlyAfterTrans, &m_CameraMatrixRotOnlyAfterTrans);
   MMLValueSet (pNode, gszGridDraw, (int) m_fGridDraw);
   MMLValueSet (pNode, gszGridFromWorld, &m_mGridFromWorld);
   MMLValueSet (pNode, gszGridDrawUD, (int) m_fGridDrawUD);
   MMLValueSet (pNode, gszGridDrawDots, (int) m_fGridDrawDots);
   MMLValueSet (pNode, gszGridMajorSize, m_fGridMajorSize);
   MMLValueSet (pNode, gszGridMinorSize, m_fGridMinorSize);
   MMLValueSet (pNode, gszGridMajorColor, (int) m_crGridMajor);
   MMLValueSet (pNode, gszGridMinorColor, (int) m_crGridMinor);
   MMLValueSet (pNode, gszDrawControlPointsOnSel, (int) m_fDrawControlPointsOnSel);
   MMLValueSet (pNode, gpszIgnoreSelection, (int) m_fIgnoreSelection);
   MMLValueSet (pNode, gpszBlendSelAbove, (int) m_fBlendSelAbove);
   MMLValueSet (pNode, gpszShowOnly, (int) m_dwShowOnly);
   MMLValueSet (pNode, gpszSelectOnly, (int) m_dwSelectOnly);

   PCMMLNode2 pSub;
   pSub = m_aRenderMatrix[0].MMLTo();  // dont bother with others
   if (pSub) {
      pSub->NameSet (gszRenderMatrix);
      pNode->ContentAdd (pSub);
   }

   MMLValueSet (pNode, gszLightWorld, &m_lightWorld);
   MMLValueSet (pNode, gszLightView, &m_lightView);
   MMLValueSet (pNode, gszCameraModel, (int) m_dwCameraModel);
   MMLValueSet (pNode, gszCameraFOV, m_fCameraFOV);
   MMLValueSet (pNode, gszCameraLookFrom, &m_CameraLookFrom);
   MMLValueSet (pNode, gszCameraLongitude, m_fCameraLongitude);
   MMLValueSet (pNode, gszCameraLatitude, m_fCameraLatitude);
   MMLValueSet (pNode, gszCameraTilt, m_fCameraTilt);
   MMLValueSet (pNode, gszCameraObjectLoc, &m_CameraObjectLoc);
   MMLValueSet (pNode, gszCameraObjectRotX, m_fCameraObjectRotX);
   MMLValueSet (pNode, gszCameraObjectRotY, m_fCameraObjectRotY);
   MMLValueSet (pNode, gszCameraObjectRotZ, m_fCameraObjectRotZ);
   MMLValueSet (pNode, gszCameraFlatLongitude, m_fCameraFlatLongitude);
   MMLValueSet (pNode, gszCameraFlatTiltX, m_fCameraFlatTiltX);
   MMLValueSet (pNode, gszCameraFlatTiltY, m_fCameraFlatTiltY);
   MMLValueSet (pNode, gszCameraFlatScale, m_fCameraFlatScale);
   MMLValueSet (pNode, gszCameraFlatTransX, m_fCameraFlatTransX);
   MMLValueSet (pNode, gszCameraFlatTransY, m_fCameraFlatTransY);
   MMLValueSet (pNode, gszClipNear, m_fClipNear);
   MMLValueSet (pNode, gszClipFar, m_fClipFar);
   MMLValueSet (pNode, gszClipNearFlat, m_fClipNearFlat);
   MMLValueSet (pNode, gszClipFarFlat, m_fClipFarFlat);
   MMLValueSet (pNode, gszCameraObjectCenter, &m_CameraObjectCenter);
   MMLValueSet (pNode, gszCameraFlatCenter, &m_CameraFlatCenter);

   if (m_pEffectOutline) {
      pSub = m_pEffectOutline->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszEffectOutline);
         pNode->ContentAdd (pSub);
      }
   }
   if (m_pEffectFog) {
      pSub = m_pEffectFog->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszEffectFog);
         pNode->ContentAdd (pSub);
      }
   }

   DWORD i;
   // NOTE: When do MMLForm will need to clear
   for (i = 0; i < m_listRTCLIPPLANE.Num(); i++) {
      // BUGFIX - Was writing directly into pNode
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gszClipPlane);

      PRTCLIPPLANE prt = (PRTCLIPPLANE) m_listRTCLIPPLANE.Get(i);
      MMLValueSet (pSub, gszClipPlaneP0, &prt->ap[0]);
      MMLValueSet (pSub, gszClipPlaneP1, &prt->ap[1]);
      MMLValueSet (pSub, gszClipPlaneP2, &prt->ap[2]);
      MMLValueSet (pSub, gszClipPlaneID, (int) prt->dwID);
   }


   //pClone->m_dwCacheImageValid = FALSE;

   return pNode;
}


/*******************************************************************************
CRenderTraditional::MMLFrom - Gets MML from a node
*/
BOOL CRenderTraditional::MMLFrom (PCMMLNode2 pNode)
{
   CMatrix Ident;
   Ident.Identity();
   CPoint Zero;
   Zero.Zero();

   if (!m_pEffectOutline)
      m_pEffectOutline = new CNPREffectOutline(m_dwRenderShard); // so always have something
   if (!m_pEffectFog)
      m_pEffectFog = new CNPREffectFog(m_dwRenderShard); // so always have something
   PWSTR psz;
   PCMMLNode2 pSub;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszEffectOutline), &psz, &pSub);
   if (pSub && m_pEffectOutline)
      m_pEffectOutline->MMLFrom (pSub);
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszEffectFog), &psz, &pSub);
   if (pSub && m_pEffectFog)
      m_pEffectFog->MMLFrom (pSub);

   m_fBackfaceCull = (BOOL) MMLValueGetInt (pNode, gszBackfaceCull, (int) m_fBackfaceCull);
   m_dwRenderShow = (DWORD) MMLValueGetInt (pNode, gszRenderShow, (int) m_dwRenderShow);
   // NOTE: Not loading m_fFinalRender
   // NOTE: Not loading m_fForShadows
   m_fSelectedBound = (BOOL) MMLValueGetInt (pNode, gszSelectedBound, (int) m_fSelectedBound);
   m_dwRenderModel = (DWORD) MMLValueGetInt (pNode, gszRenderModel, (int) m_dwRenderModel);
   m_cMono = (COLORREF) MMLValueGetInt (pNode, gszMono, (int) m_cMono);
   m_fExposure = MMLValueGetDouble (pNode, gszExposure, CM_BESTEXPOSURE);
   m_dwIntensityAmbient1 = (DWORD) MMLValueGetInt (pNode, gszIntensityAmbient1, (int) m_dwIntensityAmbient1);
   m_dwIntensityAmbient2 = (DWORD) MMLValueGetInt (pNode, gszIntensityAmbient2, (int) m_dwIntensityAmbient2);
   m_dwIntensityAmbientExtra = (DWORD) MMLValueGetInt (pNode, gszIntensityAmbientExtra, (int) m_dwIntensityAmbientExtra);
   m_dwIntensityLight = (DWORD) MMLValueGetInt (pNode, gszIntensityLight, (int) m_dwIntensityLight);
   MMLValueGetPoint (pNode, gszLightAmbientColor, &m_pLightAmbientColor);
   m_cBackground = (COLORREF) MMLValueGetInt (pNode, gszBackground, (int) m_cBackground);
   //m_cBackgroundMono = (COLORREF) MMLValueGetInt (pNode, Back()groundMono, (int) m_cBackgroundMono);
   m_fDetailPixels = MMLValueGetDouble (pNode, gszDetailPixels, m_fDetailPixels);
   m_dwSpecial = (DWORD) MMLValueGetInt (pNode, gpszSpecial, 0);
   m_dwShadowsFlags = (DWORD) MMLValueGetInt (pNode, gpszShadowsFlags, 0);
   // nOTE: Not setting/getting m_iPriorityIncrease
   m_fShadowsLimit = MMLValueGetDouble (pNode, gpszShadowsLimit, 0);
   m_fShadowsLimitCenteredOnCamera = (BOOL) MMLValueGetInt (pNode, gpszShadowsLimitCenteredOnCamera, FALSE);
   // m_fRBGlasses = (BOOL) MMLValueGetInt (pNode, gszRBGlasses, (int) m_fRBGlasses);
   // m_fRBRightEyeDominant = (BOOL) MMLValueGetInt (pNode, gszRBRightEyeDominant, (int) m_fRBRightEyeDominant);
   // m_fRBEyeDistance = MMLValueGetDouble (pNode, gszRBEyeDistance, m_fRBEyeDistance);
   m_dwClouds = (DWORD) MMLValueGetInt (pNode, gszClouds, (int)m_dwClouds);
   m_dwDrawPlane = (DWORD) MMLValueGetInt (pNode, gszDrawPlane, (int) -1);
   MMLValueGetMatrix (pNode, gszCameraMatrixRotAfterTrans, &m_CameraMatrixRotAfterTrans, &Ident);
   MMLValueGetMatrix (pNode, gszCameraMatrixRotOnlyAfterTrans, &m_CameraMatrixRotOnlyAfterTrans, &Ident);
   m_fGridDraw = (BOOL) MMLValueGetInt (pNode, gszGridDraw, (int) m_fGridDraw);
   MMLValueGetMatrix (pNode, gszGridFromWorld, &m_mGridFromWorld, &Ident);
   m_fGridDrawUD = (BOOL) MMLValueGetInt (pNode, gszGridDrawUD, (int) m_fGridDrawUD);
   m_fGridDrawDots = (BOOL) MMLValueGetInt (pNode, gszGridDrawDots, (int) m_fGridDrawDots);
   m_fGridMajorSize = MMLValueGetDouble (pNode, gszGridMajorSize, m_fGridMajorSize);
   m_fGridMinorSize = MMLValueGetDouble (pNode, gszGridMinorSize, m_fGridMinorSize);
   m_crGridMajor = (COLORREF) MMLValueGetInt (pNode, gszGridMajorColor, (int) m_crGridMajor);
   m_crGridMinor = (COLORREF) MMLValueGetInt (pNode, gszGridMinorColor, (int) m_crGridMinor);
   m_fDrawControlPointsOnSel = (BOOL) MMLValueGetInt (pNode, gszDrawControlPointsOnSel, (int) m_fDrawControlPointsOnSel);
   m_fIgnoreSelection = (BOOL) MMLValueGetInt (pNode, gpszIgnoreSelection, FALSE);
   m_fBlendSelAbove = (BOOL) MMLValueGetInt (pNode, gpszBlendSelAbove, FALSE);
   m_dwShowOnly = (DWORD) MMLValueGetInt (pNode, gpszShowOnly, -1);
   m_dwSelectOnly = (DWORD) MMLValueGetInt (pNode, gpszSelectOnly, -1);

   DWORD i;
   m_listRTCLIPPLANE.Clear();
   for (i = 0; pNode->ContentNum(); i++) {
      PWSTR psz;
      PCMMLNode2 pSub;
      pSub = NULL;
      if (!pNode->ContentEnum (i, &psz, &pSub))
         break;
      if (!pSub)
         continue;
      
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gszRenderMatrix)) {
         m_aRenderMatrix[0].MMLFrom (pSub);  // dont bother with others
      }
      else if (!_wcsicmp (psz, gszClipPlane)) {
         RTCLIPPLANE rt;
         memset (&rt, 0, sizeof(rt));
         // BUGFIX - Was referring to pNode
         MMLValueGetPoint (pSub, gszClipPlaneP0, &rt.ap[0], &Zero);
         MMLValueGetPoint (pSub, gszClipPlaneP1, &rt.ap[1], &Zero);
         MMLValueGetPoint (pSub, gszClipPlaneP2, &rt.ap[2], &Zero);
         rt.dwID = (DWORD) MMLValueGetInt (pSub, gszClipPlaneID, (int) 0);
         m_listRTCLIPPLANE.Add (&rt);
      }
   }


   MMLValueGetPoint (pNode, gszLightWorld, &m_lightWorld, &Zero);
   MMLValueGetPoint (pNode, gszLightView, &m_lightView, &Zero);
   m_dwCameraModel = (DWORD) MMLValueGetInt (pNode, gszCameraModel, (int) m_dwCameraModel);
   m_fCameraFOV = MMLValueGetDouble (pNode, gszCameraFOV, m_fCameraFOV);
   FOVSet (m_fCameraFOV);
   MMLValueGetPoint (pNode, gszCameraLookFrom, &m_CameraLookFrom, &Zero);
   m_fCameraLongitude = MMLValueGetDouble (pNode, gszCameraLongitude, m_fCameraLongitude);
   m_fCameraLatitude = MMLValueGetDouble (pNode, gszCameraLatitude, m_fCameraLatitude);
   m_fCameraTilt = MMLValueGetDouble (pNode, gszCameraTilt, m_fCameraTilt);
   MMLValueGetPoint (pNode, gszCameraObjectLoc, &m_CameraObjectLoc, &Zero);
   m_fCameraObjectRotX = MMLValueGetDouble (pNode, gszCameraObjectRotX, m_fCameraObjectRotX);
   m_fCameraObjectRotY = MMLValueGetDouble (pNode, gszCameraObjectRotY, m_fCameraObjectRotY);
   m_fCameraObjectRotZ = MMLValueGetDouble (pNode, gszCameraObjectRotZ, m_fCameraObjectRotZ);
   m_fCameraFlatLongitude = MMLValueGetDouble (pNode, gszCameraFlatLongitude, m_fCameraFlatLongitude);
   m_fCameraFlatTiltX = MMLValueGetDouble (pNode, gszCameraFlatTiltX, m_fCameraFlatTiltX);
   m_fCameraFlatTiltY = MMLValueGetDouble (pNode, gszCameraFlatTiltY, m_fCameraFlatTiltY);
   m_fCameraFlatScale = MMLValueGetDouble (pNode, gszCameraFlatScale, m_fCameraFlatScale);
   m_fCameraFlatTransX = MMLValueGetDouble (pNode, gszCameraFlatTransX, m_fCameraFlatTransX);
   m_fCameraFlatTransY = MMLValueGetDouble (pNode, gszCameraFlatTransY, m_fCameraFlatTransY);
   m_fClipNear = MMLValueGetDouble (pNode, gszClipNear, m_fClipNear);
   m_fClipFar = MMLValueGetDouble (pNode, gszClipFar, m_fClipFar);
   m_fClipNearFlat = MMLValueGetDouble (pNode, gszClipNearFlat, m_fClipNearFlat);
   m_fClipFarFlat = MMLValueGetDouble (pNode, gszClipFarFlat, m_fClipFarFlat);
   MMLValueGetPoint (pNode, gszCameraObjectCenter, &m_CameraObjectCenter, &Zero);
   MMLValueGetPoint (pNode, gszCameraFlatCenter, &m_CameraFlatCenter, &Zero);


   m_dwCacheImageValid = FALSE;
   ClipRebuild ();

   return TRUE;
}

/*******************************************************************************
CRenderTraditional::CWorldSet - An application calls this to specify what world
object is to be used for drawing

inputs
   PCWorldSocket     pWorld - world
returns
   none
*/
void CRenderTraditional::CWorldSet (PCWorldSocket pWorld)
{
   _ASSERTE (pWorld->RenderShardGet() == m_dwRenderShard);

   m_pWorld = pWorld;
   m_dwCacheImageValid = 0;   // none of the image caches are valid
}


/*******************************************************************************
CRenderTraditional::CImageSet - Tells the rendering object what image to use.
The application should call this before rendering, or if the image object size changes.
This remembers pImage, and sets some scaling matrices accordinly, along with
converversion from W-space to screen coordinates.

inputs
   PCImage     pImage - image to use.
returns
   none
*/
BOOL CRenderTraditional::CImageSet (PCImage pImage)
{
   if (!pImage || !pImage->Width())
      return FALSE;

   m_dwCacheImageValid = 0;   // none of the image caches are valid
   m_pImage = pImage;
   m_pZImage = NULL;
   m_pFImage = NULL;
   DWORD dwWidth, dwHeight;
   dwWidth = m_pImage->Width();
   dwHeight = m_pImage->Height();


   // set perspective info
   m_aRenderMatrix[0].ScreenInfo (dwWidth, dwHeight); // dont bother with others

   // and remember for scaling after perpective
   RenderCalcCenterScale (dwWidth, dwHeight, &m_afCenterPixel[0], &m_afScalePixel[0]);

   // replaced by fn call
   //m_afCenterPixel[0] = (fp) dwWidth / 2.0 + 0.5;   // need +0.5 so it rounds off properly
   //m_afCenterPixel[1] = (fp) dwHeight / 2.0 + 0.5;  // need +0.5 so it rounds off properly
   //m_afScalePixel[0] = (fp) (dwWidth+2) / 2.0; // BUGFIX - Did +1 to width and height do ends up drawing UL corner
   //m_afScalePixel[1] = -(fp) (dwHeight+2) / 2.0;

   return TRUE;
}

/*******************************************************************************
CRenderTraditional::LightVectorSet - Specify the direction of the light vector
in world space, where X = EW, Y = NS, Z = up/down.

inputs
   PCPoint     pLight - vector. Doesn't need to be normalized
returns
   none
*/
void CRenderTraditional::LightVectorSet (PCPoint pLight)
{
   // BUGFIX - Was calling lightvectorset all the time, and hence cache image invalid
   CPoint pTemp;
   pTemp.Copy (pLight);
   pTemp.Normalize();
   if (pTemp.AreClose (&m_lightWorld))
      return;

   m_dwCacheImageValid = 0;   // none of the image caches are valid
   m_lightWorld.Copy(&pTemp);
}

#if 0 // DEAD code - no longer used
/*******************************************************************************
CRenderTraditional::LightVectorSet - Specify the direction of the light vector by
assuming it's the sun.

inputs
   DFTIME      dwTime - Time of day
   DFDATE      dwDate - Date
   fp      fLatitude - Latitude in radians where the house is build. Negative values are south
   fp      fTrueNorth - Number of radians that true north is clockwise of this north.
returns
   none
*/
void CRenderTraditional::LightVectorSet (DFTIME dwTime, DFDATE dwDate, fp fLatitude,
                                         fp fTrueNorth)
{
   // BUGFIX - Remove bause handled in light vector set m_dwCacheImageValid = 0;   // none of the image caches are valid
   m_dwSunTime = dwTime;
   m_dwSunDate = dwDate;
   m_fSunLatitude = fLatitude;


   // calculate where the sun is
   CPoint pSun;
   fp fLat;
   fLat = fLatitude / 2.0 / PI * 360.0;
   SunVector (dwTime, dwDate, fLat, &pSun);
   // rotate this
   CMatrix m;
   m.RotationZ (-fTrueNorth);
   pSun.MultiplyLeft (&m); // normally would invert and transpose, but just put in the opposite angle
   LightVectorSet (&pSun);

   // find where the sun was - so know direction when drawing the sun
   DWORD dwWas;
   dwWas = TODFTIME ((HOURFROMDFTIME(dwTime)+23)%24, MINUTEFROMDFTIME(dwTime));
   SunVector (dwWas, dwDate, fLat, &m_pSunWas);
   m_pSunWas.MultiplyLeft (&m); // normally would invert and transpose, but just put in the opposite angle
   m_pSunWas.Normalize();

   // and moon location
   m_fMoonPhase = MoonVector (dwTime, dwDate, fLat, &m_pMoonVector);
   m_pMoonVector.MultiplyLeft (&m);
   MoonVector (dwWas, dwDate, fLat, &m_pMoonWas);
   m_pMoonWas.MultiplyLeft (&m);

   // Set the light intensity based on the sun's height and cloud cover
   fp f;
   f = (fp) 0x10000 * sqrt(max(pSun.p[2], 0));
   switch (m_dwClouds) {
   case 2:  // light clouds
      f /= 8.0;
      break;
   case 3:  // heavy clouds
      f = 0.0;
      break;
   }
   m_dwIntensityLight = (DWORD) f;

   // Set the light's color based on the sun's height and cloud cover
   m_pLightSunColor.p[0] = m_pLightSunColor.p[1] = m_pLightSunColor.p[2] = 1.0;
   switch (m_dwClouds) {
   case 0:  // full sun
   case 1:  // partly cloudy
      {
         // average between blue and slightly red
         f = (pSun.p[2] + .1) * 10;
         f = max(0, f);
         f = min(1, f);

         // very yellow color for sun light right at extremities
         m_pLightSunColor.p[2] = f;
      }
      break;
   }
   m_pLightMoonColor.p[0] = m_pLightMoonColor.p[1] = .5;
   m_pLightMoonColor.p[2] = 1; //  moon in bluish light


   // Set the ambient intensity based on the sun's height (or moon's)
   fp fMoon;
   f = (fp) 0x10000 * pow(max(pSun.p[2] + .1, 0), 1.0 / 3.0) / 8; // ambient light
   fMoon = (fp) 0x10000 * pow(max(m_pMoonVector.p[2], 0), 1.0 / 3.0) / 8 /
      (CM_LUMENSSUN / CM_LUMENSMOON) * sin(m_fMoonPhase * PI);
   switch (m_dwClouds) {
   case 2:  // light clouds
      f *= 1.5;   // more ambient light
      fMoon *= 1.5;
      break;
   case 3:  // heavy clouds
      f *= .5;
      fMoon *= .5;
      break;
   }
   m_dwIntensityAmbient2 = (DWORD) max(f, fMoon);
   if (fMoon < f)
      // Set the ambient color based on the sun's height and cloud cover
      m_pLightAmbientColor.Copy (&m_pLightSunColor);  // copy the ambient color
   else
      m_pLightAmbientColor.Copy (&m_pLightMoonColor);


   // set the background based on the cloud cover and the location of the sun
   COLORREF c;
   WORD wc[3];
   fp afc[3];
   fp fScale2;
   fScale2 = 1;
   switch (m_dwClouds) {
   case 0:  // full sun
   case 1:  // partly cloudy
      {
         // average between blue and slightly red
         f = (pSun.p[2] + .1) * 10;
         f = max(0, f);
         f = min(1, f);

         c = RGB((BYTE) (f * 210 + (1-f) * 255),
            (BYTE) (f * 210 + (1-f) * 128),
            (BYTE) (f * 255 + (1-f) * 255) );

         note - not tested
         if (m_fFog)
            c = RGB(0xff,0xff,0xff);   // in fog goes to just white
      }
      break;
   case 2:  // light clouds
   case 3:  // heavy clouds
      c = RGB(255,255,255);
      fScale2 = .25;  // needs to be slightly grey
      break;
   }
   // NOTE: Background color NOT affected by moon

   Gamma (c, wc);
   DWORD i;
   fp fSum;
   fSum = 1;   // so dont get zero
   for (i = 0; i < 3; i++) {
      afc[i] = wc[i];
      fSum += afc[i];
   }
   fSum = (fp)(0xffff) * 3.0 / fSum * 7;
   fSum *= (fp) m_dwIntensityAmbient2 / (fp)0x10000 * fScale2;
   for (i = 0; i < 3; i++) {
      afc[i] *= fSum;   // to make sure the light level is always right
      afc[i] = min(0xffff, afc[i]);
      wc[i] = (WORD)afc[i];
   }
   m_cBackground = UnGamma (wc);
}
#endif // 0

/*******************************************************************************
CRenderTraditional::CameraClipNearFar - Sets the clipping planes for near and far.
That is, objects closers than fNear (meters) are clipped, and objects further than
fFar (meters) are clipped.

NOTE: When calling with perspective, this is the distance from the camera.

inputs
   fp      fNear - Near value
   fp      fFar - Far value. if fFar < fNear then fFar is assumed to be infinity
*/
void CRenderTraditional::CameraClipNearFar (fp fNear, fp fFar)
{
   m_dwCacheImageValid = 0;   // none of the image caches are valid
   m_fClipNear = fNear;
   m_fClipFar = fFar;

   ClipRebuild ();
}

/*******************************************************************************
CRenderTraditional::CameraClipNearFarFlat - Sets the clipping planes for near and far.
This if for the flat rendering model.

inputs
   fp      fNear - Near value. Negative values are towards the viewer, away
                  from the center of the house. If -10000 or more then no clipping.
   fp      fFar - Far value. Positive values are away from the cetner of the house.
                  If +10000 of more then no clipping
*/
void CRenderTraditional::CameraClipNearFarFlat (fp fNear, fp fFar)
{
   m_dwCacheImageValid = 0;   // none of the image caches are valid
   m_fClipNearFlat = fNear;
   m_fClipFarFlat = fFar;

   ClipRebuild ();
}

/*******************************************************************************
CRenderTraditional::ClipRebuild - Rebuilds the clipping planes. Used if a camera
is changed, or camera clipping has changed.
*/
void CRenderTraditional::ClipRebuild (void)
{
   CPoint pNormal, pPoint;
   m_aClipAll[0].Clear();  // only wory about first
   m_aClipOnlyScreen[0].Clear();

   // Do something different if flat model instead of perspective
   if (m_dwCameraModel == CAMERAMODEL_FLAT) {
      // clip against zNear
      if (m_fClipNearFlat > -10000) {
         pNormal.Zero4();
         pNormal.p[2] = 1;
         pPoint.Zero4();
         pPoint.p[2] = -m_fClipNearFlat;
         m_aClipAll[0].AddPlane (&pNormal, &pPoint);
         m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);
      }

      // zfar
      if (m_fClipFarFlat < 10000) {
         pNormal.Zero4();
         pNormal.p[2] = -1;
         pPoint.Zero4();
         pPoint.p[2] = -m_fClipFarFlat;
         m_aClipAll[0].AddPlane (&pNormal, &pPoint);
         m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);
      }

      // left side of screen
      pNormal.Zero4();
      pNormal.p[0] = -1;
      pPoint.Zero4();
      pPoint.p[0] = -1;
      m_aClipAll[0].AddPlane (&pNormal, &pPoint);
      m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);

      // right side of screen
      pNormal.Zero4();
      pNormal.p[0] = 1;
      pPoint.Zero4();
      pPoint.p[0] = 1;
      m_aClipAll[0].AddPlane (&pNormal, &pPoint);
      m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);

      // bottom side of screen
      pNormal.Zero4();
      pNormal.p[1] = -1;
      pPoint.Zero4();
      pPoint.p[1] = -1;
      m_aClipAll[0].AddPlane (&pNormal, &pPoint);
      m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);

      // top side of screen
      pNormal.Zero4();
      pNormal.p[1] = 1;
      pPoint.Zero4();
      pPoint.p[1] = 1;
      m_aClipAll[0].AddPlane (&pNormal, &pPoint);
      m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);

      goto otherplanes;
   }

   // clip against zNear
   pNormal.Zero4();
   pNormal.p[2] = 1;
   pPoint.Zero4();
   pPoint.p[2] = -max(m_fClipNear, EPSILON);
   m_aClipAll[0].AddPlane (&pNormal, &pPoint);
   m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);

   // zfar
   if (m_fClipFar > m_fClipNear) {
      pNormal.Zero4();
      pNormal.p[2] = -1;
      pPoint.Zero4();
      pPoint.p[2] = -m_fClipFar;
      m_aClipAll[0].AddPlane (&pNormal, &pPoint);
      m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);
   }

   // left side of screen
   pNormal.Zero4();
   pNormal.p[0] = -1;
   pNormal.p[3] = -1;
   pPoint.Zero4();
   m_aClipAll[0].AddPlane (&pNormal, &pPoint);
   m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);

   // right side of screen
   pNormal.Zero4();
   pNormal.p[0] = 1;
   pNormal.p[3] = -1;
   pPoint.Zero4();
   m_aClipAll[0].AddPlane (&pNormal, &pPoint);
   m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);

   // bottom side of screen
   pNormal.Zero4();
   pNormal.p[1] = -1;
   pNormal.p[3] = -1;
   pPoint.Zero4();
   m_aClipAll[0].AddPlane (&pNormal, &pPoint);
   m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);

   // top side of screen
   pNormal.Zero4();
   pNormal.p[1] = 1;
   pNormal.p[3] = -1;
   pPoint.Zero4();
   m_aClipAll[0].AddPlane (&pNormal, &pPoint);
   m_aClipOnlyScreen[0].AddPlane (&pNormal, &pPoint);

otherplanes:
   // all the user clip planes
   // loop
   PRTCLIPPLANE p;
   DWORD i;
   for (i = 0; i < m_listRTCLIPPLANE.Num(); i++) {
      p = (PRTCLIPPLANE) m_listRTCLIPPLANE.Get(i);
      CPoint ap[4], pCenter;

      DWORD j;
      // convert the three points to perspective space
      for (j = 0; j < 3; j++)
         m_aRenderMatrix[0].Transform (&p->ap[j], &ap[j]);  // don't bother iwth others

      pCenter.Copy (&ap[0]);
      // should I divide out 1? - Probably not since seems to work without that

      // calculate the normal
      CPoint v1, v2, n;
      v1.Subtract (&ap[0], &ap[1]);
      v2.Subtract (&ap[2], &ap[1]);
      v1.Normalize();
      v2.Normalize();
      n.CrossProd (&v1, &v2);
      n.Normalize();
      n.p[3] = 0;  // set to 0 so

#if TRY4VECTOR
      // make a fourth point as an interpolation of the 3rd point and 2nd point
      // so can figure out the plane in 4D
      CPoint pFour;
      pFour.Add (&p->ap[0], &p->ap[2]);
      pFour.Scale(.5);
      m_RenderMatrix.Transform (&pFour, &ap[3]);

      pCenter.Copy (&ap[0]);
      // should I divide out 1 - doesnt matter since im not doing this

      // calculate the normal
      CPoint v1, v2, v3, n;
      v1.Subtract4 (&ap[0], &ap[1]);
      v2.Subtract4 (&ap[2], &ap[1]);
      v3.Subtract4 (&ap[3], &ap[1]);
      v1.Normalize4();
      v2.Normalize4();
      v3.Normalize4();
      n.CrossProd4 (&v1, &v2, &v3);
      n.Normalize4();

#ifdef _DEBUG
      char szTemp[128];
      sprintf (szTemp, "C4 = %g,%g,%g,%g\r\n", (double)n.p[0],(double)n.p[1],(double)n.p[2],(double)n.p[3]);
      OutputDebugString (szTemp);
      sprintf (szTemp, "P4 = %g,%g,%g,%g\r\n", (double)pCenter.p[0],(double)pCenter.p[1],(double)pCenter.p[2],(double)pCenter.p[3]);
      OutputDebugString (szTemp);
#endif

      // if it's 0 length then cross product two and set w = 0
      if ((n.p[0] == 0) && (n.p[1] == 0) && (n.p[2] == 0) && (n.p[3] == 0)) {
         //n.CrossProd (&v1, &v2);

         // Kind of fixes flat view but not quite
      }
#endif // TRY4VECTOR

      m_aClipAll[0].AddPlane (&n, &pCenter);
   }

}


/*******************************************************************************
CRenderTraditional::CameraFlat - Specified a flatted view,

inputs
   fp         fLongitude - Rotate the house around this many radians (clockwise as
                     looking down on the house) around up/down axis
   fp         fTiltX - Tile the house. PI/2 then tilt so looking from the top
   fp         fTiltY - Around Y
   fp         fScale - Number of meters across the width screen
   fp         fTransX - Translate flatted image to the right fTransX meters
   fp         fTransY - Translate flattened image up fTransY meters
returns
   none
*/
void CRenderTraditional::CameraFlat (PCPoint pCenter, fp fLongitude, fp fTiltX, fp fTiltY, fp fScale,
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
   m_dwCacheImageValid = 0;   // none of the image caches are valid

   // clear main matrix
   m_aRenderMatrix[0].PerspectiveScale (2.0 / fScale, 2.0 / fScale, 1); // dont bother with other
      // Need the 1.0 so z values stay the same
   m_aRenderMatrix[0].Clear();
   m_aRenderMatrix[0].Rotate (-PI/2, 1);   // rotate -90 degrees around X

   // apply the transforms
   m_aRenderMatrix[0].Translate (fTransX, 0, fTransY);
   // post-translation rotation matrix
   m_CameraMatrixRotAfterTrans.Identity();
   CMatrix t;
   CPoint z;
   z.Zero();
   m_CameraMatrixRotAfterTrans.FromXYZLLT (&z, fLongitude, fTiltX, fTiltY);

#if 0 // replace by FromXYZLLT
   if (fTiltX) {
      t.RotationX (fTiltX);
      m_CameraMatrixRotAfterTrans.MultiplyLeft (&t);
   }
   if (fTiltY) {
      t.RotationY (fTiltY);
      m_CameraMatrixRotAfterTrans.MultiplyLeft (&t);
   }
   if (fLongitude) {
      t.RotationZ (-fLongitude);
      m_CameraMatrixRotAfterTrans.MultiplyLeft (&t);
   }
#endif // 0
   m_CameraMatrixRotOnlyAfterTrans.Copy (&m_CameraMatrixRotAfterTrans);

   // do the translation for center of rotation
   t.Translation (-m_CameraFlatCenter.p[0],-m_CameraFlatCenter.p[1],-m_CameraFlatCenter.p[2]);
   m_CameraMatrixRotAfterTrans.MultiplyLeft (&t);

   m_aRenderMatrix[0].Multiply (&m_CameraMatrixRotAfterTrans);
   //m_RenderMatrix.Rotate (fTiltX, 1);
   //m_RenderMatrix.Rotate (fTiltY, 2);
   //m_RenderMatrix.Rotate (-fLongitude, 3);

   // rebuild clipping
   ClipRebuild ();
}


/*******************************************************************************
CRenderTraditional::FOVSet - Call this when m_fCameraFOV is to be set
*/
void CRenderTraditional::FOVSet (fp fFOV)
{
   m_fCameraFOV = fFOV;
   m_fTanHalfCameraFOV = tan(m_fCameraFOV / 2.0);
}

/*******************************************************************************
CRenderTraditional::CameraPerspWalkthrough - Position the camera as a virtual reality
walkthrough. Use perpective.

inputs
   PCPoint        pLookFrom - Point to look from (in world space)
   fp         fLongitude - Longitude to look at in radians. 0 = north. + = clockwise
   fp         fLatitude - Latitude to look at in radian. 0 = straigh ahead, + is look up, - = look down
   fp         fTitle - Radians to tilt head, clockwise. 0 is head straight up.
   fp         fFOV - field of view (in world space).
returns
   none
*/
void CRenderTraditional::CameraPerspWalkthrough (PCPoint pLookFrom, fp fLongitude, fp fLatitude, fp fTilt, fp fFOV)
{
   m_dwCameraModel = CAMERAMODEL_PERSPWALKTHROUGH;
   FOVSet (fFOV);
   m_CameraLookFrom.Copy (pLookFrom);
   m_fCameraLongitude = fLongitude;
   m_fCameraLatitude = fLatitude;
   m_fCameraTilt = fTilt;
   m_dwCacheImageValid = 0;   // none of the image caches are valid

   // apply perspective
   m_aRenderMatrix[0].Perspective (fFOV); // don't bother with others

   // clear main matrix
   m_aRenderMatrix[0].Clear();
   m_aRenderMatrix[0].Rotate (-PI/2, 1);   // rotate -90 degrees around X

   // apply the transforms
   m_aRenderMatrix[0].Rotate (-fTilt, 2);
   m_aRenderMatrix[0].Rotate (-fLatitude, 1);
   m_aRenderMatrix[0].Rotate (-fLongitude, 3);
      // BUGFIX - Make fLongitude negative so this matrix is inverse of ToXYZLLV,
      // which is used by other two views   

   // translate
   m_aRenderMatrix[0].Translate (-pLookFrom->p[0], -pLookFrom->p[1], -pLookFrom->p[2]);

   // since dont rotate after translating, set matrix to identity
   m_CameraMatrixRotAfterTrans.Identity();
   m_CameraMatrixRotOnlyAfterTrans.Identity();

   // rebuilt clipping
   ClipRebuild ();
 }

/*******************************************************************************
CRenderTraditional::CameraPerpObject - Use perspective for the viewing, but treat
the house as an object held at arms length and rotated around.

inputs
   PCPoint        pTranslate - Location to hold the center of the house, in world space.
                     Assume looking north.
   fp         fRotateZ - Amount to rotate around Z (up/down) axis
   fp         fRotateX - Amount to rotate around X (east/west) axis
   fp         fRotateY - Amount to rotate around Y (north/south) axis
   fp         fFOV - field of view (in world space).
returns
   none
*/
void CRenderTraditional::CameraPerspObject (PCPoint pTranslate, PCPoint pCenter, fp fRotateZ, fp fRotateX, fp fRotateY, fp fFOV)
{
   m_dwCameraModel = CAMERAMODEL_PERSPOBJECT;
   FOVSet (fFOV);
   m_CameraObjectLoc.Copy (pTranslate);
   m_fCameraObjectRotX = fRotateX;
   m_fCameraObjectRotY = fRotateY;
   m_fCameraObjectRotZ = fRotateZ;
   m_CameraObjectCenter.Copy (pCenter);
   m_dwCacheImageValid = 0;   // none of the image caches are valid

   // apply perspective
   m_aRenderMatrix[0].Perspective (fFOV); // don't bother with others

   // clear main matrix
   m_aRenderMatrix[0].Clear();

   // apply the transforms
   m_aRenderMatrix[0].Rotate (-PI/2, 1);   // rotate -90 degrees around X
   m_aRenderMatrix[0].Translate (pTranslate->p[0], pTranslate->p[1], pTranslate->p[2]);

   // post-translation rotation matrix
   CPoint z;
   CMatrix t;
   z.Zero();
   m_CameraMatrixRotAfterTrans.FromXYZLLT (&z, fRotateZ, fRotateX, fRotateY);
#if 0 // Old code replaced by FromXYZLLT
   m_CameraMatrixRotAfterTrans.Identity();
   if (fRotateZ) {
      t.RotationZ (fRotateZ);
      m_CameraMatrixRotAfterTrans.MultiplyLeft (&t);
      //m_RenderMatrix.Rotate (fRotateZ, 3);
   }
   if (fRotateX) {
      t.RotationX (fRotateX);
      m_CameraMatrixRotAfterTrans.MultiplyLeft (&t);
      //m_RenderMatrix.Rotate (fRotateX, 1);
   }
   if (fRotateY) {
      t.RotationY (fRotateY);
      m_CameraMatrixRotAfterTrans.MultiplyLeft (&t);
      //m_RenderMatrix.Rotate (fRotateY, 2);
   }
#endif
   m_CameraMatrixRotOnlyAfterTrans.Copy (&m_CameraMatrixRotAfterTrans);

   // do the translation for center of rotation
   t.Translation (-m_CameraObjectCenter.p[0],-m_CameraObjectCenter.p[1],-m_CameraObjectCenter.p[2]);
   m_CameraMatrixRotAfterTrans.MultiplyLeft (&t);

   m_aRenderMatrix[0].Multiply (&m_CameraMatrixRotAfterTrans);

   // Set the clipping planes
   ClipRebuild();
}

/*******************************************************************************
CRenderTraditional::CameraModelGets - Retrurns a DWORD for the camera model,
of the form CAMERAMODEL_XXX
*/
DWORD CRenderTraditional::CameraModelGet (void)
{
   return m_dwCameraModel;
}

/*******************************************************************************
CRenderTraditional::CameraPerspWalkthroughGet - Fills in pointers with the
settings for the perspective walkthrough
*/
void CRenderTraditional::CameraPerspWalkthroughGet (PCPoint pLookFrom, fp *pfLongitude, fp *pfLatitude, fp *pfTilt, fp *pfFOV)
{
   pLookFrom->Copy (&m_CameraLookFrom);
   *pfLongitude = m_fCameraLongitude;
   *pfLatitude = m_fCameraLatitude;
   *pfTilt = m_fCameraTilt;
   *pfFOV = m_fCameraFOV;
}

/*******************************************************************************
CRenderTraditional::CameraPerspObjectGet - Fills in the pointers with the settings
for the perspective object */
void CRenderTraditional::CameraPerspObjectGet (PCPoint pTranslate, PCPoint pCenter, fp *pfRotateZ, fp *pfRotateX, fp *pfRotateY, fp *pfFOV)
{
   pTranslate->Copy (&m_CameraObjectLoc);
   pCenter->Copy (&m_CameraObjectCenter);
   *pfRotateX = m_fCameraObjectRotX;
   *pfRotateY = m_fCameraObjectRotY;
   *pfRotateZ = m_fCameraObjectRotZ;
   *pfFOV = m_fCameraFOV;
}

/*******************************************************************************
CRenderTraditional::CameraFlatGet - Fills in the pointers with the flat camera settings
*/
void CRenderTraditional::CameraFlatGet (PCPoint pCenter, fp *pfLongitude, fp *pfTiltX, fp *pfTiltY, fp *pfScale, fp *pfTransX, fp *pfTransY)
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
RenderSort */
int __cdecl RenderSort (const void *elem1, const void *elem2 )
{
   PROINFO p1, p2;
   p1 = (PROINFO) elem1;
   p2 = (PROINFO) elem2;

   if (p1->fZNear > p2->fZNear)
      return 1;
   else if (p1->fZNear < p2->fZNear)
      return -1;
   else
      return 0;
}


/*******************************************************************************
CRenderTraditional::RenderForPainting - Has the 3D engine render, but not for
drawing. This just ends up calculating the location of the pixels, and
what textures is on them, so 3DOB is able to paint.

inputs
   PCMem       pMemRPPIXEL - This will be alloced so the size is dwWidth x dwHeight x sizeof(RPPIXEL).
                  It will then be filled in.
   PCListFixed plRPTEXTINFO - Will be initialized to sizeof (RPTEXTINFO) and filled in with the
                  RPTEXTINFO referred to by pMemRPPIXEL. Only textures which can be
                  modified by painting will be added
   PCPoint     pViewer - Filled in with the location of the viewer
   PCMatrix    pMatrix - This matrix will convert from view space vectors to world space vectors.
                  Useful for transforming light vector
   PCProgressSocket pProgress - For showing progress

returns
   BOOL - TRUE if success
*/
BOOL CRenderTraditional::RenderForPainting (PCMem pMemRPPIXEL, PCListFixed plRPTEXTINFO,
   PCPoint pViewer, PCMatrix pMatrix, PCProgressSocket pProgress)
{
   BOOL fRet = TRUE;
   // must have an image
   if (!(m_pImage || m_pFImage || m_pZImage) || !m_pWorld)
      return FALSE;

   m_pProgress = pProgress;
   if (!pMemRPPIXEL->Required (
      Width() * Height() * sizeof(RPPIXEL)))
      return FALSE;
   plRPTEXTINFO->Init (sizeof(RPTEXTINFO));
   plRPTEXTINFO->Clear();

   DWORD dwOldModel;
   dwOldModel = m_dwRenderModel;
   m_dwRenderModel = RENDERMODEL_SURFACESHADOWS;
   m_pRPPIXEL = (PRPPIXEL) pMemRPPIXEL->p;
   m_plRPTEXTINFO = plRPTEXTINFO;
   m_pRPView = pViewer;
   m_pRPMatrix = pMatrix;

   if (!m_pImageShader)
      m_pImageShader = new CImageShader;
   if (!m_pImageShader) {
      fRet = FALSE;
      goto done;
   }
   if (!m_pImageShader->InitNoClear (Width(), Height())) {
      fRet = FALSE;
      goto done;
   }

   // fill in multithreaded information
   if ((m_dwThreadsMax == (DWORD)-1) && !MultiThreadedInit ()) {
      fRet = FALSE;
      goto done;
   }
   MultiThreadedPolyCalc ();

   // initialize clipplanes
   DWORD i;
   for (i = 0; i < m_dwThreadsMax; i++) {
         // BUGFIX - Was using m_dwThreadsPoly, but want to be extra safe
      m_aClipAll[0].CloneTo (&m_aClipAll[i+1]);
      m_aClipOnlyScreen[0].CloneTo (&m_aClipOnlyScreen[i+1]);
   }

   // initalize the image for critical sections if multhreaded
   if (m_dwThreadsPoly > 1) {
      if (m_pImage)
         m_pImage->CritSectionInitialize();
      if (m_pImageShader)
         m_pImageShader->CritSectionInitialize();
      if (m_pZImage)
         m_pZImage->CritSectionInitialize();
   }

   // if some of the bits have changed then everything is invalid
   m_dwCacheImageValid = 0;

   // draw
   fRet = RenderInternal (FALSE, NULL);

   // NOTE: leaving threads around in case wish to use later
   // howeever, make sure none left
   MultiThreadedWaitForComplete (TRUE);

done:
   m_pProgress = NULL;
   m_pRPPIXEL = NULL;
   m_plRPTEXTINFO = NULL;
   m_dwRenderModel = dwOldModel;
   m_pRPView = NULL;
   m_pRPMatrix = NULL;
   return fRet;
}


/*******************************************************************************
CRenderTraditional::Render - Renders the image.

This:
   1) If it's just a normal image calls RenderInternal.
   2) If it's for 3D glasses calles REnderInternal for both and combines the images

inputs
   DWORD    dwWorldChanged - Bits from CWoldSocket, WORLDC_XXX. These say what's
               been changed since the last render. It's used for optimization so
               that if only a selected object has changed then only the selected
               objects are redrawn.
   HANDLE   hEventSafe - If not NULL, SetEvent() is called on this event when
               it's safe to start up another renderer in a different thread (since
               this one is no longer looking at the world data). NOTE: Don't
               change the world data until AFTER the function returns.
   PCProgressSocket  pProgress - Use this as the progress bar. Cna be NLL
*/
BOOL CRenderTraditional::Render (DWORD dwWorldChanged, HANDLE hEventSafe, PCProgressSocket pProgress)
{
   // m_fShadowsLimit = 50;   // BUGBUG - to test with minimal shadows
   // m_fShadowsLimitCenteredOnCamera = TRUE;   // BUGBUG - to test with minimal shadows

   MALLOCOPT_INIT;
   BOOL fRet = TRUE;
   m_pProgress = pProgress;

   // must have an image
   if (!(m_pImage || m_pFImage || m_pZImage) || !m_pWorld) {
      m_pProgress = NULL;
      if (hEventSafe)
         SetEvent (hEventSafe);
      return FALSE;
   }

   // BUGFIX - If have m_pFImage and rendermode is NOT shadows then error
   if (m_pFImage && ((m_dwRenderModel & 0x0f) != RM_SHADOWS)) {
      if (hEventSafe)
         SetEvent (hEventSafe);
      return FALSE;
   }

   // if using the special feature to show always selected then need to
   // use slightly different render flags
   if (m_dwSelectOnly != -1) {
      DWORD dwBits = dwWorldChanged;

      if (dwBits & WORLDC_OBJECTCHANGESEL)
         dwWorldChanged |= WORLDC_OBJECTCHANGENON;
      if (dwBits & WORLDC_OBJECTCHANGENON)
         dwWorldChanged |= WORLDC_OBJECTCHANGESEL;
   }

#ifdef _TIMERS
   memset (&gRenderStats, 0, sizeof(gRenderStats));
   gRenderStats.dwNumObjects = m_pWorld->ObjectNum();
#endif

   // potentially dirty the lights
   if (dwWorldChanged & (WORLDC_OBJECTADD | WORLDC_OBJECTREMOVE | WORLDC_OBJECTCHANGESEL | WORLDC_OBJECTCHANGENON))
      LightsDirty();

   // create the solid object if it's not already
   if ((m_dwRenderModel & 0x0f) == RM_SHADOWS) {
      if (!m_pImageShader) {
	      MALLOCOPT_OKTOMALLOC;
         m_pImageShader = new CImageShader;
	      MALLOCOPT_RESTORE;
      }
      if (!m_pImageShader) {
         if (hEventSafe)
            SetEvent (hEventSafe);
         return FALSE;   // error
      }
      if (!m_pImageShader->InitNoClear (Width(), Height())) {
         if (hEventSafe)
            SetEvent (hEventSafe);
         return FALSE;   // error
      }

      m_pImageShader->m_dwMaxTransparent = (m_dwShadowsFlags & SF_LOWTRANSPARENCY) ? 2 : 4;
         // BUGFIX - With low transparency, go to depth of 2 instead of 1, since trees to bare with 1
   }
   else {
      // normal renderer - so free up shader's memory
      if (m_pImageShader)
         delete m_pImageShader;
      m_pImageShader = NULL;
   }

   // if anything has changed then the cache is invalid
   if (dwWorldChanged) {
      // if some of the bits have changed then everything is invalid
      if (dwWorldChanged & (WORLDC_CAMERAMOVED | WORLDC_SELADD | WORLDC_SELREMOVE | WORLDC_OBJECTADD | WORLDC_OBJECTREMOVE | WORLDC_SURFACECHANGED | WORLDC_LIGHTCHANGED))
         m_dwCacheImageValid = 0;

      // if a selected object has changed then taht bit is invalid
      if (dwWorldChanged & (WORLDC_OBJECTCHANGESEL | WORLDC_CAMERACHANGESEL))
         m_dwCacheImageValid &= ~(0x02);

      // if a non-selected object has changed then that bit is invalid
      if (dwWorldChanged & (WORLDC_OBJECTCHANGENON | WORLDC_CAMERACHANGENON))
         m_dwCacheImageValid &= ~(0x01);
   }

   // fill in multithreaded information
   if ((m_dwThreadsMax == (DWORD)-1) && !MultiThreadedInit ()) {
      if (hEventSafe)
         SetEvent (hEventSafe);
      return FALSE;
   }
   MultiThreadedPolyCalc ();

   // initialize clipplanes
   DWORD i;
   for (i = 0; i < m_dwThreadsMax; i++) {
         // BUGFIX - Was using m_dwThreadsPoly, but want to be extra safe
      m_aClipAll[0].CloneTo (&m_aClipAll[i+1]);
      m_aClipOnlyScreen[0].CloneTo (&m_aClipOnlyScreen[i+1]);
   }

   // initalize the image for critical sections if multhreaded
   if (m_dwThreadsPoly > 1) {
      if (m_pImage)
         m_pImage->CritSectionInitialize();
      if (m_pImageShader)
         m_pImageShader->CritSectionInitialize();
      if (m_pZImage)
         m_pZImage->CritSectionInitialize();
   }

   // calulate lights
   LightsAddRemove ();
   LightsCalc (&m_pRenderForLight, pProgress);

   // if have low detail option then adjust
   fp fDetailOrig = m_fDetailPixels;
   if (m_dwShadowsFlags & SF_LOWDETAIL)
      m_fDetailPixels *= 2.0; // BUGFIX - Was 4x. Changed to 2x so not as extreme

   // if no 3d glasses then easy
   //if (!m_fRBGlasses || (m_dwCameraModel == CAMERAMODEL_FLAT)) {
      fRet = RenderInternal (TRUE, hEventSafe);
      hEventSafe = NULL;
      m_pProgress = NULL;
      goto stats;
   //}

#if 0
   // else, deal with 3D glasses

   // do the non-dominant eye
   {
      CImage   imageOther;
      CFImage FImageOther;
      if (!imageOther.Init (Width(), Height())) {
         m_pProgress = NULL;
         goto stats;
      }
      PCImage  pImageOrig;
      PCFImage pFImageOrig;
      pImageOrig = m_pImage;
      pFImageOrig = m_pFImage;
      m_pImage = &imageOther;
      m_pFImage = &FImageOther;
      // keep the main camera at the same place as if don't have glasses and move
      // the non-dominant eye. This isn't technically correct but it makes clicking
      // on the screen and finding the right location a lot easier.
      m_RenderMatrix.ScreenInfo (Width(), Height(),
         m_fRBRightEyeDominant ? m_fRBEyeDistance : (-m_fRBEyeDistance));
      if (m_pProgress)
         m_pProgress->Push(0, .4);  // drawing one eye
      fRet = RenderInternal(FALSE);

      // do the dominant eye
      m_pImage = pImageOrig;
      m_pFImage = pFImageOrig;
      m_RenderMatrix.ScreenInfo (Width(), Height());
      if (m_pProgress) {
         m_pProgress->Pop();
         m_pProgress->Push(.4, .8);  // drawing one eye
      }
      if (fRet)
         fRet = RenderInternal(FALSE);

      // now merge them
      if (m_pProgress) {
         m_pProgress->Pop();
         m_pProgress->Push(.8, 1);  // drawing one eye
      }
      if (m_pImage)
         m_pImage->RBGlassesMerge (&imageOther, m_fRBRightEyeDominant);
      if (m_pFImage)
         m_pFImage->RBGlassesMerge (&FImageOther, m_fRBRightEyeDominant);
      m_pProgress = NULL;
   }
#endif // 0

stats:
   // undo Push() added light LightsCalc()
   if (pProgress)
      pProgress->Pop();

   // Test to display shadow buffer
   //if (m_lPCLight.Num()) {
   //   PCLight pl = *((PCLight*)m_lPCLight.Get(0));
   //   pl->TestImage (m_pImage, 1);
   //}
   // Tesr to display shadow buffer
#ifdef _TIMERS
   char  szTemp[64];
   sprintf (szTemp, "dwNumObjects=%d ", gRenderStats.dwNumObjects);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwRenderObject=%d ", gRenderStats.dwRenderObject);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwObjTrivial=%d ", gRenderStats.dwObjTrivial);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwObjCantUseZTest=%d ", gRenderStats.dwObjCantUseZTest);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwObjCovered=%d ", gRenderStats.dwObjCovered);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwObjNotCovered=%d ", gRenderStats.dwObjNotCovered);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwBoundingBoxGet=%d ", gRenderStats.dwBoundingBoxGet);
   OutputDebugString (szTemp);
   OutputDebugString ("\r\n");

   sprintf (szTemp, "dwPolyBackface=%d ", gRenderStats.dwPolyBackface);
   OutputDebugString (szTemp);
   sprintf (szTemp, "fRedrawSel=%d ", (int) gRenderStats.fRedrawSel);
   OutputDebugString (szTemp);
   sprintf (szTemp, "fRedrawNonSel=%d ", (int)gRenderStats.fRedrawNonSel);
   OutputDebugString (szTemp);
   sprintf (szTemp, "fRedrawAll=%d ", (int)gRenderStats.fRedrawAll);
   OutputDebugString (szTemp);
   OutputDebugString ("\r\n");

   sprintf (szTemp, "dwNumPoly=%d ", gRenderStats.dwNumPoly);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwNumTri=%d ", gRenderStats.dwNumTri);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwTriCovered=%d ", gRenderStats.dwTriCovered);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwNumHLine=%d ", gRenderStats.dwNumHLine);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwNumDiagLine=%d ", gRenderStats.dwNumDiagLine);
   OutputDebugString (szTemp);
   OutputDebugString ("\r\n");

   sprintf (szTemp, "dwPixelRequest=%d ", gRenderStats.dwPixelRequest);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwPixelTrivial=%d ", gRenderStats.dwPixelTrivial);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwPixelBehind=%d ", gRenderStats.dwPixelBehind);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwPixelTransparent=%d ", gRenderStats.dwPixelTransparent);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwPixelWritten=%d ", gRenderStats.dwPixelWritten);
   OutputDebugString (szTemp);
   sprintf (szTemp, "dwPixelOverwritten=%d ", gRenderStats.dwPixelOverwritten);
   OutputDebugString (szTemp);
   OutputDebugString ("\r\n");
#endif // _TIMERS

   // NOTE: leaving threads around in case wish to use later
   // howeever, make sure none left
   MultiThreadedWaitForComplete (TRUE);

   // delete the render engine for lights
   if (m_pRenderForLight) {
      delete m_pRenderForLight;
      m_pRenderForLight = NULL;
   }

   // restore detail
   m_fDetailPixels = fDetailOrig;

   if (hEventSafe)
      SetEvent (hEventSafe);
   return fRet;
}

/*******************************************************************************
CRenderTraditional::RenderInternal - Renders using this current settings.

  This:
  1) Wipes out the current screen.
  2) Calls all the objects and gets their bounding boxes
  2a) While it's at it determines amount of details needed
  3) Does trivial clipping, and or remembers if don't need to clip stuff inside
  4) Sorts objects by Z so nearer ones are drawn first
  5) For each object:
   a) Determines if the object is hidden by all objects in front
   b) Calls all the objects and tells them to draw
  6) Calls the image's outline and fog function

inputs
   BOOL     fCanUseCache - If this is set to TRUE, the renderer should try using
            the cache (and generating one necessary) for both the selected and non-selected
            objects. If FALSE, just draw them all.
   HANDLE   hEventSafe - from render. NOTE: WILL always set this if not NULL.

returns
   BOOL - TRUE if success. FALSE if cancelled
*/
BOOL CRenderTraditional::RenderInternal (BOOL fCanUseCache, HANDLE hEventSafe)
{
   // clear the image
   if (!(m_pImage || m_pFImage || m_pZImage) || !m_pWorld) {
      if (hEventSafe) {
         SetEvent (hEventSafe);
         hEventSafe = NULL;
      }
      return FALSE;
   }
   PCImageShader pTempShader;
   pTempShader = NULL;
   
   m_dwCloneID = 1;

   // figure out m_dwForceCache
   if (m_pImageShader)
      m_dwForceCache = FORCECACHE_TRANS | FORCECACHE_SPEC | FORCECACHE_GLOW;
         // BUGFIX - Added FORCECACHE_GLOW because I think was causing a GPFault
         // BUGFIX - Added FORCECACE_SPEC because looks at specularity for
         // determining if should draw line, even if it's transparent
   else  // no-shadows or Z-image
      m_dwForceCache =
         FORCECACHE_TRANS | FORCECACHE_GLOW |
            // BUGFIX - Added FORCECACHE_GLOW so doesn't crash. Was crashing when rendered shadows
         (m_pImage ?  (FORCECACHE_COMBINED | FORCECACHE_GLOW) : 0 /* m_pZImage */);

   // BUGBUG - to see if fixes crash
   // BUGBUG - SHOULD be commented out, recommenting out to make sure it's all working
   // m_dwForceCache |= FORCECACHE_ALL;
   // BUGBUG

   // drawing only takes some of the time
   if (m_pProgress)
      m_pProgress->Push (0, .8);

   // if there's no selection then there isn't any point caching
   BOOL fHaveSelection;
   DWORD *pdwSel, dwSel;
   pdwSel = m_pWorld->SelectionEnum(&dwSel);
   if (m_fIgnoreSelection)
      dwSel = 0;
   else if (m_dwSelectOnly != -1) {
      pdwSel = &m_dwSelectOnly;
      dwSel = 1;
   }
   fHaveSelection = (dwSel > 0);
   if (!fHaveSelection)
      fCanUseCache = FALSE;

   // if we're using the shader render dont bother with the cache
   if ((m_dwRenderModel & 0x0f) == RM_SHADOWS)
      fCanUseCache = FALSE;

   // color for the background
   COLORREF cBack;
   cBack = m_cBackground;
   //((m_dwRenderModel & 0x0f) == RM_MONO) ? m_cBackgroundMono : m_cBackground;

   // BUGFIX - If we're not monoe, and not rendering with shadows, then background
   // also adjusted by exposure
   fp    afBack[3];
   DWORD dwC;
   switch (m_dwRenderModel & 0x0f) {
   case RM_MONO:
   case RM_SHADOWS:
      break;   // do nothing
   default:
      afBack[0] = Gamma (GetRValue(cBack));
      afBack[1] = Gamma (GetGValue(cBack));
      afBack[2] = Gamma (GetBValue(cBack));
      for (dwC = 0; dwC < 3; dwC++) {
         afBack[dwC] *= CM_BESTEXPOSURE / max(m_fExposure, CLOSE);
         if (afBack[dwC] > 0xffff)
            afBack[dwC] = 0xffff;
      }
      cBack = RGB(UnGamma ((WORD)afBack[0]),
         UnGamma ((WORD)afBack[1]),
         UnGamma ((WORD)afBack[2]));
      break;
   }

   // if we want to use the cache then wipe out the cache. Otherwise, just clear
   // the main image
   if (fCanUseCache) {
      // if the cache images are invalid AND we need them, then clear them out
      if (!(m_dwCacheImageValid & 0x01)) {
         if (m_pImage) {
            m_CacheImage[0].Init (Width(), Height(), cBack);
            if (m_dwThreadsPoly > 1)
               m_CacheImage[0].CritSectionInitialize (); // BUGFIX - problems with multithreaded
         }
         else if (m_pFImage) {
            m_FCacheImage[0].Init (Width(), Height(), cBack);
            // NOTE - FCache not tested
            //if (m_dwThreadsPoly > 1)
            //   m_FCacheImage[0].CritSectionInitialize (); // BUGFIX - problems with multithreaded
         }
         else { // if (m_pZImage)
            m_ZCacheImage[0].Init (Width(), Height(), cBack);
            // NOTE - ZCache not tested
            if (m_dwThreadsPoly > 1)
               m_ZCacheImage[0].CritSectionInitialize (); // BUGFIX - problems with multithreaded
         }
      }
      if (!(m_dwCacheImageValid & 0x02)) {
         if (m_pImage) {
            m_CacheImage[1].Init (Width(), Height(), cBack);
            if (m_dwThreadsPoly > 1)
               m_CacheImage[1].CritSectionInitialize (); // BUGFIX - problems with multithreaded
         }
         else if (m_pFImage) {
            m_FCacheImage[1].Init (Width(), Height(), cBack);
            // NOTE - FCache not tested
            //if (m_dwThreadsPoly > 1)
            //   m_FCacheImage[1].CritSectionInitialize (); // BUGFIX - problems with multithreaded
         }
         else if (m_pZImage) {
            m_ZCacheImage[1].Init (Width(), Height(), cBack);
            // NOTE - ZCache not tested
            if (m_dwThreadsPoly > 1)
               m_ZCacheImage[1].CritSectionInitialize (); // BUGFIX - problems with multithreaded
         }
      }
   }
   else {
      if (m_pImage)
         m_pImage->Clear (cBack);
      if (m_pZImage)
         m_pZImage->Clear (cBack);
      if (m_pFImage)
         m_pFImage->Clear (cBack);
      m_dwCacheImageValid = 0;   // probably invalid
   }
   if (m_pImageShader)
      m_pImageShader->Clear ();

   BOOL fCancelled = FALSE;

   // if we can use cache then draw the background then the foreground, then merge
   if (fCanUseCache) {
      // remember the image
      PCImage pMain = m_pImage;
      PCFImage pFMain = m_pFImage;
      PCZImage pZMain = m_pZImage;

      if (m_pProgress)
         m_pProgress->Push (0, .5);

      // draw the non-selected objects
      if (!(m_dwCacheImageValid & 0x01)) {
         if (m_pImage)
            m_pImage = &m_CacheImage[0];
         if (m_pZImage)
            m_pZImage = &m_ZCacheImage[0];
         if (m_pFImage)
            m_pFImage = &m_FCacheImage[0];
               // NOTE - FCache not tested
         if (!fCancelled && RenderPass (1))
            m_dwCacheImageValid |= 0x01;
         else
            fCancelled = TRUE;
#ifdef _TIMERS
         gRenderStats.fRedrawNonSel = TRUE;
#endif
      }

      if (m_pProgress) {
         m_pProgress->Pop();
         m_pProgress->Push (.5, .8);
      }
      // draw the selected objects
      if (!(m_dwCacheImageValid & 0x02)) {
         if (m_pImage)
            m_pImage = &m_CacheImage[1];
         if (m_pZImage)
            m_pZImage = &m_ZCacheImage[1];
         if (m_pFImage)
            m_pFImage = &m_FCacheImage[1];
               // NOTE - FCache not tested
         if (!fCancelled && RenderPass (2))
            m_dwCacheImageValid |= 0x02;
         else
            fCancelled = TRUE;
#ifdef _TIMERS
         gRenderStats.fRedrawSel = TRUE;
#endif
      }

      if (m_pProgress) {
         m_pProgress->Pop();
         m_pProgress->Push (.8, 1);
      }
      // copy and merge
      m_pImage = pMain;
      m_pFImage = pFMain;
      m_pZImage = pZMain;
      if (m_pImage) {
         memcpy (m_pImage->Pixel(0,0), m_CacheImage[0].Pixel(0,0),
            Width() * Height() * sizeof(IMAGEPIXEL));
            // I can do this because I know the images are the same size
         m_pImage->PaintCacheClear();
         m_CacheImage[1].MergeSelected (m_pImage, m_fBlendSelAbove);
      }
      else if (m_pFImage) {
         memcpy (m_pFImage->Pixel(0,0), m_FCacheImage[0].Pixel(0,0),
            Width() * Height() * sizeof(FIMAGEPIXEL));
               // NOTE - FCache not tested
         // Disable because no paint cache m_pFImage->PaintCacheClear();
         m_FCacheImage[1].MergeSelected (m_pFImage, m_fBlendSelAbove);
      }
      else { // if (m_pZImage) {
         memcpy (m_pZImage->Pixel(0,0), m_ZCacheImage[0].Pixel(0,0),
            Width() * Height() * sizeof(ZIMAGEPIXEL));
               // NOTE - ZCache not tested
         // Disable because no paint cache m_pZImage->PaintCacheClear();
         m_ZCacheImage[1].MergeSelected (m_pZImage, m_fBlendSelAbove);
      }
      if (m_pProgress) {
         m_pProgress->Update(1); // got through this
         m_pProgress->Pop();
      }
   }
   else {
      if (m_pImageShader && m_pProgress)
         m_pProgress->Push (0, .2);

      if (!fCancelled && !RenderPass (0))
         fCancelled = TRUE;
#ifdef _TIMERS
      gRenderStats.fRedrawAll = TRUE;
#endif

      if (m_pImageShader && m_pProgress) {
         m_pProgress->Pop ();
         m_pProgress->Push (.2, 1);
      }

      // have the shader convert to the image
      if (!fCancelled && m_pImageShader) {
         if (m_pRPPIXEL) {
            // drawing for the purposes of generating painting info, so dont
            // bother with spotlights, just run the shader
            ShaderForPainting ();
         }
         else {
            // make a spotlight to produce better shadows
            if (m_pProgress)
               m_pProgress->Push (0, .2);

            LightsSpotlight(&m_pRenderForLight, m_pProgress);

            if (m_pProgress) {
               m_pProgress->Pop ();
               m_pProgress->Push (.2, 1);
            }

            fCancelled = !ShaderApply (hEventSafe);
            hEventSafe = NULL;   // since ShaderApply will clear

            if (m_pProgress)
               m_pProgress->Pop ();
         }

         if (m_pImageShader && m_pProgress)
            m_pProgress->Pop();

         // temporarily claim that dont have shader so can draw the other bits
         pTempShader = m_pImageShader;
         m_pImageShader = NULL;
      }

   }

   // drawing only takes some of the time
   if (m_pProgress) {
      m_pProgress->Pop ();
      m_pProgress->Push (.8, 1);
   }

   if (m_pProgress)
      m_pProgress->Update (0);

   // draw the grid
   if (m_fGridDraw)
      DrawGrid (m_crGridMajor, m_fGridMajorSize, m_crGridMinor, m_fGridMinorSize,
         &m_mGridFromWorld, m_fGridDrawUD, m_fGridDrawDots);

   if (m_pProgress)
      m_pProgress->Update (.2);

   // apply outlines, but only if rendering solids, or if selected
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum (&dwNum);
   if (m_fIgnoreSelection)
      dwNum = 0;
   else if (m_dwSelectOnly != -1) {
      pdw = 0; // NOTE: In this case, since it's always selected, dont outline
               // all the time because would be annoying
   }

   if ( ((m_dwRenderModel & 0xf0) || dwNum) && m_pEffectOutline) {
      // dont outline if we're it wireframe mode UNLESS we're showing selected
      DWORD dwOut, dwOrig;
      dwOut = dwOrig = m_pEffectOutline->m_dwLevel;
      if (!(m_dwRenderModel & 0xf0))
         dwOut = 0;

      // outline selected objects only
      m_pEffectOutline->m_dwLevel = dwOut;
      if (m_pImage)
         m_pEffectOutline->Render (m_pImage, m_pImage, this, m_pWorld, m_fFinalRender, NULL);
         // NOTE: Dont bother with progress for outline
      else if (m_pFImage)
         m_pEffectOutline->Render (m_pFImage, m_pFImage, this, m_pWorld, m_fFinalRender, NULL);
            // NOTE - FImage outline not tested
      // NOTE: Dont do outline for ZImage

      m_pEffectOutline->m_dwLevel = dwOrig;
   }

   if (m_pProgress)
      m_pProgress->Update (.4);

   if (m_pProgress)
      m_pProgress->Update (.6);

   // if we have a selection then draw those
   // render the bounding box
   DWORD *pdwElem;
   pdwElem = m_pWorld->SelectionEnum (&dwNum);
   if (m_fIgnoreSelection)
      dwNum = 0;
   else if (m_dwSelectOnly != -1) {
      pdwElem = &m_dwSelectOnly;
      dwNum = 1;
   }

   if (m_fSelectedBound && dwNum) {
      // save the old model
      DWORD i, dwON;
      DWORD dwOldModel;
      BOOL fOldCull;
      dwOldModel = m_dwRenderModel;
      m_dwRenderModel = RENDERMODEL_LINEAMBIENT;
      fOldCull = m_fBackfaceCull;
      m_fBackfaceCull = FALSE;

      for (i = 0; i < dwNum; i++) {
         dwON = pdwElem[i];

         // if only want to show one object and this isn't it then skip
         if ((m_dwShowOnly != -1) && (m_dwShowOnly != dwON))
            continue;

         m_adwObjectID[0] = (dwON + 1) << 16;   // dont care about others
         m_fDetailForThis = .1;
         m_adwWantClipThis[0] = m_aClipOnlyScreen[0].ClipMask(); // ignore others
            // NOTE: Taking advantace of the fact taht m_ClipOnlyScreen is a subset
            // of m_ClipAll. This only clips against only screen, ignoring the other bits.

         PCObjectSocket pos;
         pos = m_pWorld->ObjectGet (dwON);

         CMatrix mBound;
         CPoint pc[2];
         if (!m_pWorld->BoundingBoxGet (dwON, &mBound, &pc[0], &pc[1]))
            continue;
         m_aRenderMatrix[0].Push(); // don't bother with others

         // BUGFIX - If select only is on then dont bother with the box
         if (m_dwSelectOnly == -1) {
            // draw the box
            CRenderSurface cs;
            cs.Init (this);
            cs.Multiply (&mBound);
            cs.SetDefColor (m_pEffectOutline ? m_pEffectOutline->m_cOutlineSelected : RGB(0xff,0,0));
            RENDERSURFACE mat;
            memset (&mat, 0, sizeof(mat));
            mat.wMinorID = 0xe000;  // so doesn't conflict with control drag points
            cs.SetDefMaterial (&mat);
            cs.ShapeBox (pc[0].p[0], pc[0].p[1], pc[0].p[2],pc[1].p[0], pc[1].p[1], pc[1].p[2]);
         }
         m_aRenderMatrix[0].Pop();  // dont bother with others

         // if we have control points AND the we're supposed to draw them on
         // a selection then do so
         if (m_fDrawControlPointsOnSel) {
            CListFixed l;
            l.Init (sizeof(DWORD));
            pos->ControlPointEnum(&l);

            // temporarily switch to solid rendering
            DWORD dwCurModel;
            dwCurModel = m_dwRenderModel;
            m_dwRenderModel = RENDERMODEL_SURFACESHADED;

            DWORD dwControl;
            OSCONTROL osc;
            for (dwControl = 0; dwControl < l.Num(); dwControl++) {
               DWORD *pdw = (DWORD*) l.Get(dwControl);
               if (pos->ControlPointQuery (*pdw, &osc))
                  RenderControlPoint (&osc, &mBound);
            }

            // switch back
            m_dwRenderModel = dwCurModel;

         }

      }

      // restore the old model

      m_dwRenderModel = dwOldModel;
      m_fBackfaceCull = fOldCull;
   }


   if (m_pProgress)
      m_pProgress->Update (.8);

   // apply fog
   if (m_pEffectFog) {
      // NOTE: Dont bother with prgress bar
      if (m_pImage)
         m_pEffectFog->Render (m_pImage, m_pImage, this, m_pWorld, m_fFinalRender, NULL);
      else if (m_pFImage)
         m_pEffectFog->Render (m_pFImage, m_pFImage, this, m_pWorld, m_fFinalRender, NULL);
            // NOTE - FImage fog not tested
      // NOTE: Dont do fog for Z image
   }

   if (m_pProgress)
      m_pProgress->Update (1);

   if (m_pProgress)
      m_pProgress->Pop ();

   // restore shader
   if (pTempShader)
      m_pImageShader = pTempShader;

   if (hEventSafe) {
      SetEvent (hEventSafe);
      hEventSafe = NULL;
   }

   // make sure all the clones are freed
   CloneFreeAll();

   return !fCancelled;
}

/*******************************************************************************
CRenderTraditional::WorldSpaceToPixel - Given a point in world space, this
converts it to a pixel on the screen.

inputs
   PCPoint        pWorld - Point in world space
   fp             *pfX, *pfY - Filled with X and Y
   fp             *pfZ - Filled in with Z Can be null
returns
   BOOL - True if transformed - and is in front of user. FALSE if behind
*/
BOOL CRenderTraditional::WorldSpaceToPixel (PCPoint pWorld, fp *pfX, fp *pfY, fp *pfZ)
{
   // apply rotation to the point
   CPoint pLoc;
   pLoc.Copy (pWorld);
   pLoc.p[3] = 1;

   m_aRenderMatrix[0].Transform (1, &pLoc); // don't bother with others
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
CRenderTraditional::ShadowsFlagsSet - Standard API
*/
void CRenderTraditional::ShadowsFlagsSet (DWORD dwFlags)
{
   m_dwShadowsFlags = dwFlags;
}



/*******************************************************************************
CRenderTraditional::ShadowsFlagsGet - Standard API
*/
DWORD CRenderTraditional::ShadowsFlagsGet (void)
{
   return m_dwShadowsFlags;
}



/*******************************************************************************
CRenderTraditional::RenderControlPoint - Draws a control point on the selected
object.

inputs
   POSCONTROL     posc - Control information
   PCMatrix       pmObject - Rotation/translation matrix for the object
returns
   none
*/
#define  MAXCPSIZE      64
void CRenderTraditional::RenderControlPoint (POSCONTROL posc, PCMatrix pmObject)
{
   // If ZImage then ignore
   if (m_pZImage)
      return;

   m_aRenderMatrix[0].Push(); // dont bother with others
   m_aRenderMatrix[0].Multiply (pmObject);

   // apply rotation to the point
   CPoint pLoc;
   pLoc.Copy (&posc->pLocation);
   pLoc.p[3] = 1;

   m_aRenderMatrix[0].Transform (1, &pLoc);

   // BUGFIX - Nicer looking drag points

   // is this off the screen?
   DWORD dwCPSize, dwWidth, dwHeight;
   dwWidth = Width();
   dwHeight = Height();
   dwCPSize = max(dwWidth / 48, 4);
   dwCPSize = min(dwCPSize, MAXCPSIZE);

   // use clobal CP size scale
   switch (gdwCPSize) {
      case 0:
         dwCPSize /= 2;
         break;
      case 2:
         dwCPSize = (dwCPSize * 3) / 2;
         break;
      default:
         // do nothing
         break;
   }
   dwCPSize = max(4,dwCPSize);

   dwCPSize = (dwCPSize / 2) * 2;      // make even
   if ((pLoc.p[2] >= -CLOSE) && (m_dwCameraModel != CAMERAMODEL_FLAT))
      goto done;  // behind us
   TransformToScreen (1, &pLoc);

   if ((pLoc.p[0] < -(fp)dwCPSize) || (pLoc.p[0] >= (fp)(dwWidth + dwCPSize)) ||
      (pLoc.p[1] <= -(fp)dwCPSize) || (pLoc.p[1] >= (fp)(dwHeight + dwCPSize)) )
      goto done;  // off the screen

   // find Z value - giving the size of the ball the benefiti of the doubt in location
   pLoc.p[2] -= posc->fSize / 2.0;
   if ((pLoc.p[2] <= -CLOSE) && (m_dwCameraModel != CAMERAMODEL_FLAT))
      goto done;

   // figure out the bounds for the objects
   int      aiBound[MAXCPSIZE];
   fp f;
   DWORD i;
   switch (posc->dwStyle) {
      case CPSTYLE_POINTER:
         // make a triangle out of this
         for (i = 0; i < dwCPSize; i++) {
            aiBound[i] = (int) floor(((fp) i / (fp) dwCPSize / .86 - .08) * (fp) (dwCPSize/2));
            if (aiBound[i] > (int)dwCPSize / 2)
               aiBound[i] = -1;
         }
         break;
      case CPSTYLE_SPHERE:
         for (i = 0; i < dwCPSize; i++) {
            f = (((fp) i+.5) / (fp) (dwCPSize)) * 2.0 - 1.0;
            aiBound[i] = (int)(sqrt(1.0 - f * f) * (fp) dwCPSize / 2.0);
         }
         break;
      case CPSTYLE_CUBE:
      default:
         for (i = 0; i < dwCPSize; i++)
            aiBound[i] = (int)dwCPSize / 2;
         break;
   }

   // pregenerate
   IMAGEPIXEL ip;
   FIMAGEPIXEL fip;
   WORD awColor[3];
   int iStartX, iStartY;
   Gamma (posc->cColor, awColor);
   memset (&ip, 0, sizeof(ip));
   memset (&fip, 0, sizeof(fip));
   fip.dwID = ip.dwID = m_adwObjectID[0] | (0xf000 + (WORD) posc->dwID);   // dont care about others
   fip.fZ = ip.fZ = pLoc.p[2];
   fip.dwIDPart = ip.dwIDPart = 1;
   iStartX = (int)pLoc.p[0] - (int)dwCPSize / 2;
   iStartY = (int)pLoc.p[1] - (int)dwCPSize / 2;

   DWORD dwX, dwY;
   int iX, iY;
   PIMAGEPIXEL pip;
   PFIMAGEPIXEL pfp;
   for (dwY = 0; dwY < dwCPSize; dwY++) {
      iY = (int)dwY + iStartY;
      if ((iY < 0) || (iY >= (int)dwHeight))
         continue;   // out of bounds

      for (dwX = 0; dwX < dwCPSize; dwX++) {
         iX = (int)dwX + iStartX;
         if ((iX < 0) || (iX >= (int)dwWidth))
            continue;   // out of bounds

         if (m_pImage) {
            pip = m_pImage->Pixel((DWORD)iX, (DWORD) iY);
            if (pip->fZ < ip.fZ)
               continue;   // it's in front
         }
         else {
            pfp = m_pFImage->Pixel ((DWORD)iX, (DWORD) iY);
               // NOTE - FImage pixel not tested
            if (pfp->fZ < ip.fZ)
               continue;   // it's in front
         }
         // Dont worry about ZImaage because wont get here

         // else, can draw, maybe...
         if ( ((int)dwX < ((int) dwCPSize/2 - aiBound[dwY]-1)) ||
            ((int)dwX > ((int) dwCPSize/2 + aiBound[dwY])) )
            continue;   // beyond the bounds

         // set the color
         BOOL fBlack;
         fBlack = FALSE;
         if (!dwY || (dwY+1 >= dwCPSize))
            fBlack = TRUE; // top and bottom
         else if (!dwX || (dwX+1 >= dwCPSize))
            fBlack = TRUE;
         else if ( ((int)dwX < ((int) dwCPSize/2 - aiBound[dwY-1]-1)) ||
            ((int)dwX > ((int) dwCPSize/2 + aiBound[dwY-1])) )
            fBlack = TRUE; // go up one an it's out of bounds
         else if ( ((int)dwX < ((int) dwCPSize/2 - aiBound[dwY+1]-1)) ||
            ((int)dwX > ((int) dwCPSize/2 + aiBound[dwY+1])) )
            fBlack = TRUE; // go up one an it's out of bounds
         else if ( ((int)dwX-1 < ((int) dwCPSize/2 - aiBound[dwY] - 1)) ||
            ((int)dwX+1 > ((int) dwCPSize/2 + aiBound[dwY])) )
            fBlack = TRUE;

         if (m_pImage)
            *pip = ip;
         else
            *pfp = fip;
               // NOTE - FImage pixel not tested
         // dont worry about zimage because wont get here
   
         if (fBlack) {
            if (m_pImage)
               pip->wRed = pip->wGreen = pip->wBlue = 0;
            else
               pfp->fRed = pfp->fGreen = pfp->fBlue = 0;
                  // NOTE - FImage pixel not tested
            // NOTE: Intentionally ignore m_pZImage
         }
         else {
            if (m_pImage)
               memcpy (&pip->wRed, awColor, sizeof(awColor));
            else {
               pfp->fRed = awColor[0];
               pfp->fGreen = awColor[1];
               pfp->fBlue = awColor[2];
               // NOTE - FImage pixel not tested
            }
            // NOTE: Intentionally ignore m_pZImage
         }
      }  // over x
   } // over y

   

done:
   m_aRenderMatrix[0].Pop();

#if 0 // DEAD code that drew as 3D
   {
      // draw the box
      CRenderSurface cs;
      cs.Init (this);
      cs.Multiply (pmObject);
      cs.SetDefColor (posc->cColor);

      RENDERSURFACE mat;
      memset (&mat, 0, sizeof(mat));
      mat.wMinorID = 0xf000 + (WORD) posc->dwID;
      cs.SetDefMaterial (&mat);

      // draw it
      fp fHalf;
      fHalf = posc->fSize / 2;
      switch (posc->dwStyle) {
      case CPSTYLE_POINTER:
         {
            cs.Translate (posc->pLocation.p[0], posc->pLocation.p[1], posc->pLocation.p[2]);

            // make the matrix that moves the cone (with Y being up) so that
            // it's noew pointin in posc->pDirection
            CMatrix m;
            CPoint pY;
            pY.Zero();
            pY.p[1] = 1;
            m.RotationBasedOnVector (&pY, &posc->pDirection);
            cs.Multiply (&m);

            cs.ShapeFunnel (posc->fSize, fHalf, fHalf/100.0);
         }
         break;

      case CPSTYLE_SPHERE:
         cs.Translate (posc->pLocation.p[0], posc->pLocation.p[1], posc->pLocation.p[2]);
         cs.ShapeEllipsoid (fHalf, fHalf, fHalf);
         break;

      case CPSTYLE_CUBE:
      default:
         cs.ShapeBox (posc->pLocation.p[0] - fHalf, posc->pLocation.p[1] - fHalf, posc->pLocation.p[2] - fHalf,
            posc->pLocation.p[0] + fHalf, posc->pLocation.p[1] + fHalf, posc->pLocation.p[2] + fHalf);
         break;
      }
   }
#endif // 0 - DEAD code
}


/*******************************************************************************
CRenderTraditional::CalcDetail - Given an object's bounding box, this determines
what it needs to be clipped against, and the amount of detail to display it with.

inputs
   DWORD             dwThread - Thread this is being run on, 0..RTINTERNALTHREADS-1
   PCMatrix          pmObjectToWorld - Converts from the object's coods to world space
   PCPoint           pBound1, pBound2 - Two opposite corners of bounding box, in object's coords
   PROINFO           pInfo - Information describing object. pObject must be filled in or NULL.
                              Should initially be cleared to all 0's.
returns
   BOOL - true if success

the following members of pInfo are filled in:
   BOOL              fCanUseZTest;  // set to TRUE if aPoint[2] is valid
   BOOL              fVerySmall; // set to TRUE if the object is so small may not need to be drawn
   CPoint            aPoint[2];  // min and max bounding box, in screen coords
   DWORD             dwClipPlanes;  // clipping planes that need to check against
   fp                fDetail;    // detail that need for the object
   fp                fZNear;     // closest Z. Set to 0 if !fCanUseZTest
   BOOL              fTrivialClip;   // if TRUE then can trivial clip-out the object by bounding box
*/
BOOL CRenderTraditional::CalcDetail (DWORD dwThread, const PCMatrix pmObjectToWorld, const PCPoint pBound1,
                                     const PCPoint pBound2, PROINFO pInfo)
{
   CPoint aInitial[2];
   CPoint apBox[2][2][2];
   aInitial[0].Copy (pBound1);
   aInitial[1].Copy (pBound2);
   DWORD x,y,z, d;

   // figure out the matrix to go from this to perspective and scale
   CMatrix mConvert;
   m_aRenderMatrix[dwThread].GetPerspScale (&mConvert);
   mConvert.MultiplyLeft (&m_WorldToView);
   mConvert.MultiplyLeft (pmObjectToWorld);

   // expand the box's two corners into a full eight points and then rotate
   for (x = 0; x < 2; x++) for (y=0;y<2;y++) for (z=0; z<2;z++) {
      apBox[x][y][z].p[0] = aInitial[x].p[0];
      apBox[x][y][z].p[1] = aInitial[y].p[1];
      apBox[x][y][z].p[2] = aInitial[z].p[2];
      apBox[x][y][z].p[3] = 1;

      // transform to screen space
      apBox[x][y][z].MultiplyLeft (&mConvert);
   }

   // transform to screen space - replaced by code above
   //m_RenderMatrix.Push();
   //m_RenderMatrix.Multiply (&m); // translate into bounding box coords
   //m_RenderMatrix.Transform (8, &apBox[0][0][0]);
   //m_RenderMatrix.Pop();

   // figure out clipping
   BOOL fClipAll;
   fClipAll = TRUE;
      // BUGFIX - Was not clipping selected objects, but
      // cause problems where can't get to floor below(dwSelection != 2);   // clip it all if dwSelection != 2
   // if the object doesn't want to be clipped then dont do it.
   if (pInfo->pObject && fClipAll && pInfo->pObject->Message (OSM_DENYCLIP, NULL))
      fClipAll = FALSE;

   if (fClipAll)
      pInfo->fTrivialClip = m_aClipAll[dwThread].TrivialClip (8, &apBox[0][0][0], &pInfo->dwClipPlanes);
   else
      pInfo->fTrivialClip = m_aClipOnlyScreen[dwThread].TrivialClip (8, &apBox[0][0][0], &pInfo->dwClipPlanes);

   // if it's trivial clip then don't do any other work
   if (pInfo->fTrivialClip) {
#ifdef _TIMERS
      gRenderStats.dwObjTrivial++;
#endif
      return TRUE;
   }

   // convert to screen coordinates. If any w is <= 0 then cant do this
   pInfo->fCanUseZTest = TRUE;
   for (x = 0; x < 2; x++) for (y=0;y<2;y++) for (z=0; z<2;z++) {
      // if a point is behind the viewer then cant use Z test
      if (apBox[x][y][z].p[3] <= EPSILON) {
         pInfo->fCanUseZTest = FALSE;
         break;
      }
   }

   // transform to screen
   if (pInfo->fCanUseZTest) {
      TransformToScreen (8, &apBox[0][0][0]);

      // find min/max
      for (x = 0; x < 2; x++) for (y=0;y<2;y++) for (z=0; z<2;z++) {
         if ((x == 0) && (y == 0) && (z == 0)) {
            pInfo->aPoint[0].Copy (&apBox[x][y][z]);
            pInfo->aPoint[1].Copy (&pInfo->aPoint[0]);
            continue;
         }
         

         // else, min/max
         for (d = 0; d < 4; d++) {
            pInfo->aPoint[0].p[d] = min(pInfo->aPoint[0].p[d], apBox[x][y][z].p[d]);
            pInfo->aPoint[1].p[d] = max(pInfo->aPoint[1].p[d], apBox[x][y][z].p[d]);
         }
      }

      // nearest z
      pInfo->fZNear = pInfo->aPoint[0].p[2];


      // find the detail
      fp fMaxLength, fMaxScreen;  // maximum length across in meters. Only valid if pInfo->fCanUseZTest
      fMaxLength = max (fabs(aInitial[1].p[0] - aInitial[0].p[0]), fabs(aInitial[1].p[1] - aInitial[0].p[1]));
      fMaxLength = max (fMaxLength, fabs(aInitial[1].p[2] - aInitial[0].p[2]));
      fMaxScreen = max(pInfo->aPoint[1].p[0] - pInfo->aPoint[0].p[0],pInfo->aPoint[1].p[1] - pInfo->aPoint[0].p[1]);

      if (fMaxScreen) {
         pInfo->fDetail = fMaxLength / fMaxScreen * m_fDetailPixels;
         pInfo->fDetail = max(pInfo->fDetail, 0);
         if (fMaxScreen < m_fDetailPixels/2)
            pInfo->fVerySmall = TRUE;
      }
      else {
         pInfo->fDetail = 0;  // to start out with
         pInfo->fVerySmall = TRUE;
      }
   }
   else {
      pInfo->fDetail = 0; // BUGFIX
#ifdef _TIMERS
      gRenderStats.dwObjCantUseZTest++;
#endif
   }

   return TRUE;
}

/*******************************************************************************
CRenderTraditional::RenderPass - Does one pass of rendering. Have multiple passes
because want to draw some objects partially solid and partially wireframe. Also
want some to be wireframe,

  This:
  2) Calls all the objects and gets their bounding boxes
  2a) While it's at it determines amount of details needed
  3) Does trivial clipping, and or remembers if don't need to clip stuff inside
  4) Sorts objects by Z so nearer ones are drawn first
  5) For each object:
   a) Determines if the object is hidden by all objects in front
   b) Calls all the objects and tells them to draw

inputs
   DWORD       dwSelection - If 0 then render everything, 1 render non-selected only, 2 render selected only
returns
   BOOL - TRUE if success. FALSE if cancelled
*/
BOOL CRenderTraditional::RenderPass (DWORD dwSelection)
{
	MALLOCOPT_INIT;
   BOOL fCancelled = FALSE;

   // convert the light vector to viewer space, since all others will be that too
   // since the light vector is already normalized don't need to normalize the converted one
   m_fWantNormalsForThis = ((m_dwRenderModel & 0x0f) >= RM_SHADED);
      // BUGFIX - Was ((m_dwRenderModel & 0x0f) == 2) || ((m_dwRenderModel & 0x0f) == 3);

   // BUGFIX - If this is using a CZImage then don't want the normals because they dont matter
   if (m_pZImage)
      m_fWantNormalsForThis = FALSE;

   m_fWantTexturesForThis = ((m_dwRenderModel & 0x0f) >= RM_TEXTURE);   // BUGFIX - Was == 3
   if (m_fWantNormalsForThis) {
      m_lightView.Copy(&m_lightWorld);
      m_aRenderMatrix[0].TransformNormal (1, &m_lightView, FALSE);   // dont bother with others
   }

   if (m_dwCameraModel == CAMERAMODEL_FLAT) {
      m_fPassCameraValid = FALSE;
   }
   else {
      m_fPassCameraValid = TRUE;

      CPoint pV;
      pV.Zero();
      pV.p[3] = 1;
      m_aRenderMatrix[0].TransformViewSpaceToWorldSpace (&pV, &m_pPassCamera); // dont bother with others
   }

   // store matrix away so can do cal detail
   m_aRenderMatrix[0].CTMGet (&m_WorldToView); // don't bother with others

   if (m_pProgress)
      m_pProgress->Push (0, .3);

   // BUGFIX - Was drawin sun and moon under selection
   if ((dwSelection == 0) || (dwSelection == 1)) {
      // draw the sun and the moon
      //DrawSunMoon();
      //if (m_dwClouds == 1)
      //   DrawClouds();

      if ((m_dwDrawPlane >= 0) && (m_dwDrawPlane <= 2))
         DrawPlane (m_dwDrawPlane);
   }


   // come up with a list of all the objects that want to draw
   DWORD i;
   // CMem mem;
   // replaced by m_memRenderPass
   PROINFO pi;
   ROINFO ri;
   DWORD dwNumUsed;
   dwNumUsed = 0;
   DWORD dwObjectNum = m_pWorld->ObjectNum();
	MALLOCOPT_OKTOMALLOC;
   m_memRenderPass.Required (dwObjectNum* 2 * sizeof(ROINFO));
	MALLOCOPT_RESTORE;
   pi = (PROINFO) m_memRenderPass.p;
   if (!pi) {
      if (m_pProgress)
         m_pProgress->Pop();

      // just to make sure all done
      MultiThreadedWaitForComplete(FALSE);
      return FALSE;
   }
   DWORD dwNextProgress, dwProgressSkip;
   dwProgressSkip = max(dwObjectNum / 16,1); 
   dwNextProgress = dwProgressSkip;
   DWORD dwSubObjects, dwSub;
   PCObjectSocket pos;
   for (i = 0; i < dwObjectNum; i++) {
      // if only want to show one object and this isn't it then skip
      if ((m_dwShowOnly != -1) && (m_dwShowOnly != i))
         continue;

      // BUGFIX - also loop through sub-objects
      pos = m_pWorld->ObjectGet(i);
      dwSubObjects = pos->QuerySubObjects();
      dwSubObjects = max(dwSubObjects, 1);

      // if the show categories don't overlap then ignore
      if (!(pos->CategoryQuery() & m_dwRenderShow))
         continue;

      // BUGFIX - If show bit is not on then dont bother
      if (!(pos->ShowGet()))
         continue;

      // update progress bar
      dwNextProgress--;
      if (!dwNextProgress && m_pProgress) {
         switch (m_pProgress->Update ( (fp)i / (fp) dwObjectNum)) {
         case 1:
            dwProgressSkip /= 2;
            dwProgressSkip = max(1,dwProgressSkip);
            break;
         case -1:
            dwProgressSkip *= 2;
            dwProgressSkip = min(dwProgressSkip, dwObjectNum / 8);
            break;
         }
         dwNextProgress = dwProgressSkip;

         if (m_pProgress->WantToCancel()) {
            fCancelled = TRUE;
            break;
         }
      }

      for (dwSub = 0; dwSub < dwSubObjects; dwSub++) {
         memset (&ri, 0, sizeof(ri));
         ri.dwNumber = i+1;
         ri.pObject = pos;
         ri.dwSubObject = (dwSubObjects > 1) ? dwSub : (DWORD)-1;
         if (!ri.pObject)
            continue;

         // BUGFIX - Keep track of selected
         ri.fSelected = m_pWorld->SelectionExists (i);
         if (m_fIgnoreSelection)
            ri.fSelected = FALSE;
         else if (m_dwSelectOnly != -1)
            ri.fSelected = (i == m_dwSelectOnly);

         // if we're looking for only selected or non-selected then see what this is
         if (dwSelection) {
            BOOL fSelected;
            fSelected = ri.fSelected;
            if (dwSelection == 2)   // looking for selected objects
               fSelected = !fSelected;
            if (fSelected)
               continue;
         }
         
         // get the bounding box
         CMatrix  m;
         CPoint   aInitial[2];
         m_pWorld->BoundingBoxGet (i, &m, &aInitial[0], &aInitial[1], ri.dwSubObject);

         // BUGFIX - If the bounding box is 0... exaclty the same, then skip
         // Since subobjects might be 0 sized, especially for caves.
         if ((aInitial[0].p[0] == aInitial[1].p[0]) &&
            (aInitial[0].p[1] == aInitial[1].p[1]) &&
            (aInitial[0].p[2] == aInitial[1].p[2]))
            continue;

         if (!CalcDetail (0, &m, &aInitial[0], &aInitial[1], &ri))
            continue;
         if (ri.fTrivialClip)
            continue;
         if (ri.fVerySmall)
            continue;   // BUGFIX - to small to draw

         // make sure won't overrun memory
         if ((dwNumUsed+1)*sizeof(ROINFO) > m_memRenderPass.m_dwAllocated) {
            //assume alloc succedes
	         MALLOCOPT_OKTOMALLOC;
            m_memRenderPass.Required ((dwNumUsed*3/2+1)*sizeof(ROINFO));
	         MALLOCOPT_RESTORE;
            pi = (PROINFO) m_memRenderPass.p;
         }

         // only use this if it's not trivially clipped. And since it got here
         // we know that it isn't trivially clipped
         pi[dwNumUsed++] = ri;
      } // dwObj

   }

   if (m_pProgress) {
      m_pProgress->Pop();
      m_pProgress->Push(.3, 1);
   }

   // Sort by Z order
   qsort(pi, dwNumUsed, sizeof (ROINFO), RenderSort);

   // loop through all the objects and draw them
   dwProgressSkip = max(dwNumUsed / 16,1); 
   dwNextProgress = dwProgressSkip;

   // create a ZImageSmall if multithreaded
#ifdef USEZIMAGESMALL
   if (m_dwThreadsMax && !m_pZImageSmall)
      m_pZImageSmall = new CZImage;
#endif // 0
   if (m_pZImageSmall) {
      DWORD dwSmallWidth = (Width() + ZIMAGESMALLSCALE - 1) / ZIMAGESMALLSCALE;
      DWORD dwSmallHeight = (Height() + ZIMAGESMALLSCALE - 1) / ZIMAGESMALLSCALE;

      m_pZImageSmall->Init (dwSmallWidth, dwSmallHeight, 0, ZINFINITE);
   }

   if (!fCancelled) for (i = 0; i < dwNumUsed; i++) {
   	MALLOCOPT_ASSERT;
      RenderObject (pi + i);
   	MALLOCOPT_ASSERT;

      // update progress bar
      dwNextProgress--;
      if (!dwNextProgress && m_pProgress) {
         switch (m_pProgress->Update ((fp)i / (fp) dwNumUsed)) {
         case 1:
            dwProgressSkip /= 2;
            dwProgressSkip = max(1,dwProgressSkip);
            break;
         case -1:
            dwProgressSkip *= 2;
            dwProgressSkip = min(dwProgressSkip, dwNumUsed / 8);
            break;
         }
         dwNextProgress = dwProgressSkip;

         if (m_pProgress->WantToCancel()) {
            fCancelled = TRUE;
            break;
         }
      }

      // if have an object then push the polygons to render the entire object
      if (m_dwThreadBufCur != (DWORD)-1) {
         MultiThreadedUnClaimBuf (m_dwThreadBufCur);
         m_dwThreadBufCur = (DWORD)-1;
      }
   }

   if (m_pProgress)
      m_pProgress->Pop();

   // make sure everythingis drawn
   MultiThreadedWaitForComplete (FALSE);

   // delete small Z image
   if (m_pZImageSmall) {
      delete m_pZImageSmall;
      m_pZImageSmall = NULL;
   }

   return !fCancelled;
}


/*******************************************************************************
CRenderTraditional::Clone - Creates a new renderer and duplicates all the
important renderer's parameters

inputs
   none
retursn
   CRenderTraitional* - New render. Has to be deleted by calling application
*/
CRenderTraditional *CRenderTraditional::Clone (void)
{
   PCRenderTraditional pNew;
   pNew = new CRenderTraditional(m_dwRenderShard);
   if (!pNew)
      return NULL;

   // copy over parameters
   CloneTo (pNew);

   return pNew;
}

/*******************************************************************************
CRenderTraditional::QueryWantNormals - Called by the object to determine if
it should bother generating normals for the surface.

inputs
   none
returns
   BOOL - TRUE if is should generate normals, FALSE if they're not needed because
      of the rendering model
*/
BOOL CRenderTraditional::QueryWantNormals (void)
{
   // if rendering clone then want normals
   if (m_pRayCur)
      return TRUE;

   return m_fWantNormalsForThis;
}

/******************************************************************************
CRenderTraditional::QueryCloneRender - From CRenderSocket
*/
BOOL CRenderTraditional::QueryCloneRender (void)
{
   // if rendering clone then want queryclonerender
   if (m_pRayCur)
      return TRUE;

   return (m_dwThreadsPoly >= RTCLONE_MINTHREAD);
}


/******************************************************************************
CRenderTraditional::CloneRender - From CRenderSocket
*/
BOOL CRenderTraditional::CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix)
{
   // if rendering into clone, then want clonerender
   if (m_pRayCur) {
      // make a new one
	   MALLOCOPT_INIT;
	   MALLOCOPT_OKTOMALLOC;
      PCRayObject pNew;
      pNew = new CRayObject (m_dwRenderShard, NULL, this, m_fCloneRender);
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

   // if not enough threads then don't do clonerender()
   if (m_dwThreadsPoly < RTCLONE_MINTHREAD)
      return FALSE;

   // else, get the object
   PCRayObject apRayObject[RTCLONE_LOD];
   CPoint apBound[2];
   PCRayObject pRO = CloneGet (pgCode, pgSub, TRUE, apBound, &apRayObject[0]);
   if (!pRO)
      return TRUE;   // error

   // should render this
   MultiThreadedDrawClone (pgCode, pgSub, dwNum, pamMatrix);

   // NOTE: Don't bother releaseing since that will free up, and
   // dont want that

   return TRUE;
}


/******************************************************************************
CRenderTraditional::QuerySubDetail - From CRenderSocket. Basically end up ignoring
*/
BOOL CRenderTraditional::QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail)
{
   // if rendering to clone then
   if (m_pRayCur) {
      *pfDetail = m_fDetailForThis;
      return TRUE;  // always fail this one
   }

   // see if it's visible
   ROINFO ri;
   memset (&ri, 0, sizeof(ri));
   
   if (!CalcDetail (0, pMatrix, pBound1, pBound2, &ri))
      return FALSE;

   if (ri.fTrivialClip)
      return FALSE;

   // see if behind something

   // do trivial removal - if this object has no chance of being drawn then don't bother
   if (ri.fCanUseZTest) {
      RECT r;
      r.left = (int) floor (ri.aPoint[0].p[0]);
      r.right = (int) ceil (ri.aPoint[1].p[0]);
      r.top = (int) floor (ri.aPoint[0].p[1]);
      r.bottom = (int) ceil (ri.aPoint[1].p[1]);
      if (IsCompletelyCovered (&r, ri.fZNear))
         return FALSE;
   }

   *pfDetail = ri.fDetail;
   return TRUE;
}

/*******************************************************************************
CRenderTraditional::QueryWantTextures - Called by the object to determine if
it should bother generating textures for the surface.

inputs
   none
returns
   BOOL - TRUE if is should generate normals, FALSE if they're not needed because
      of the rendering model
*/
BOOL CRenderTraditional::QueryWantTextures (void)
{
   // if rendering clone then want textures
   if (m_pRayCur)
      return TRUE;

   return m_fWantTexturesForThis;
}

/*******************************************************************************
CRenderTraditional::QueryDetail - Called by the object to determine how
much detail it should produce in the model.

inputs
   none
returns
   fp - Detail level in meters. Ex: If returns 0.05 should returns polygons
      about 5cm large
*/
fp CRenderTraditional::QueryDetail (void)
{
   // if rendering clone then want differnet detail
   if (m_pRayCur)
      return m_fDetailForThis;

   return m_fDetailForThis;
}

/*******************************************************************************
CRenderTraditional::MatrixSet - Sets the rotation and translation matrix to
use for the given points.

inputs
   PCMatrix    pm - matrix to use
returns
   none
*/
void CRenderTraditional::MatrixSet (PCMatrix pm)
{
   // if rendering clone then want differnet matrix
   if (m_pRayCur) {
      m_mRenderSocket.Copy (pm);
      return;
   }

   // would already have had something pushed on the stack, so pop and re-push
   m_aRenderMatrix[0].Pop(); // dont bother with others
   m_aRenderMatrix[0].Push();
   m_aRenderMatrix[0].Multiply (pm);
}

/*******************************************************************************
CRenderTraditional::TransformToScreen - Transforms all the points (in place)
to screen coordinates. It does this by dividing out W (and assumes that no w <= 0)
because clipping should have taken place. It then does scaling and offsets so
the image goes from 0 to width/height.

inputs
   DWORD       dwNum - Number of points
   PCPoint     paPoints - points
returns
   none
*/
void CRenderTraditional::TransformToScreen (DWORD dwNum, PCPoint paPoints)
{
   for (; dwNum; dwNum--, paPoints++) {
      paPoints->p[0] = m_afCenterPixel[0] + ((paPoints->p[0] / paPoints->p[3]) * m_afScalePixel[0]);
      paPoints->p[1] = m_afCenterPixel[1] + ((paPoints->p[1] / paPoints->p[3]) * m_afScalePixel[1]);
      paPoints->p[2] = -paPoints->p[2];   // since zbuffer is as a positive value
   }
}

/*******************************************************************************
CRenderTraditional::ModelInfiniteLight - Given a normal already transformed int
viewer space (not world space, and not screen space), this figures out how much
light hits it, taking into account m_dwIntensityLight.

inputs
   PCPoint     pNormal - Normal translated to viewer space. Assumed to be normalized.
returns
   DWORD - 0x10000 = normal color intensity. 0 = none. Higher is brighter
*/
DWORD CRenderTraditional::ModelInfiniteLight (PCPoint pNormal)
{
   fp f;
   f = pNormal->DotProd (&m_lightView);
   if (f < 0)
      return 0;

   // acount for exposure
   f *= CM_LUMENSSUN / max(m_fExposure, CLOSE);  // counteract by lumens in the sun 
   // BUGFIX - Changed code so would draw better under moonlight
   f *= (fp) m_dwIntensityLight;
   f = min(f, (fp)0xffffffff);
   return (DWORD) f;
   //f = min(f,16); // dont allow to go too high
   //return ((DWORD) (f * 1024.0) * m_dwIntensityLight) / 1024;
}

/*******************************************************************************
CRenderTraditional::ColorModelApply - Looks at the current color model, and
the data in pInfo, to do the least calculations to determine what colors are at
the vertices.

inputs
   DWORD                dwThread - 0 .. RTINTERNALTHREADS-1
   PPOLYRENDERINFO      pInfo - Polygons that are rendered
   DWORD                *pdwUsage - Fulled with
                         fills in pdwUsage for how to interpret PGCOLOR.
                         0 => all points use PGCOLOR[0],
                         1 => use based on color index of vertex
                         2 => use based on vertex index
                         3 => use based on point
   BOOL                 fBack - If TRUE, calculate the light for the back side (if can
                           see the back side of the polygons - no backface culling)
   BOOL                 fGlow - If TRUE, override fBack, and calculate as if glowing
returns
   PGCOLOR - Pointer to a an array of colors. See *pdwUsage for the number.
            This is really a pointer to m_memColorModel (+Back)
*/
PGCOLOR CRenderTraditional::ColorModelApply (DWORD dwThread, PPOLYRENDERINFO pInfo, DWORD *pdwUsage,
                                             BOOL fBack, BOOL fGlow)
{
   MALLOCOPT_INIT;

   // which memories are used
   PCMem pMemLight, pMemColor;
   if (fGlow) {
      pMemLight = &m_amemColorModelLightGlow[dwThread];
      pMemColor = &m_amemColorModelGlow[dwThread];
   }
   else if (fBack) {
      pMemLight = &m_amemColorModelLightBack[dwThread];
      pMemColor = &m_amemColorModelBack[dwThread];
   }
   else {
      pMemLight = &m_amemColorModelLight[dwThread];
      pMemColor = &m_amemColorModel[dwThread];
   }


   // expsure
   DWORD dwAmbient1, dwAmbient2, dwGlow;
   fp fExposure = CM_LUMENSSUN / max(m_fExposure, CLOSE);  // counteract by lumens in the sun 
   fp f;
   f = (fp)m_dwIntensityAmbient1 * fExposure;
   f = min(f,(fp)0xffffffff);
   dwAmbient1 = (DWORD)f;
   f = (fp)(m_dwIntensityAmbient2 + m_dwIntensityAmbientExtra) * fExposure;
   f = min(f,(fp)0xffffffff);
   dwAmbient2 = (DWORD)f;
   f = (fp)0x10000 * fExposure;
   f = min(f,(fp)0xffffffff);
   dwGlow = (DWORD)f;

   // do I need to calculated the intensities for the normals?
   BOOL fCalcForNormals;
   DWORD *pdwNormalIntensity, *pdw;
   PCPoint pp;
   DWORD i;
   pdwNormalIntensity = NULL;
   fCalcForNormals = ((m_dwRenderModel & 0x0f) >= RM_SHADED);
      //(m_dwRenderModel == RENDERMODEL_LINESHADED) || (m_dwRenderModel == RENDERMODEL_SURFACESHADED) ||
      //(m_dwRenderModel == RENDERMODEL_LINETEXTURE) || (m_dwRenderModel == RENDERMODEL_SURFACETEXTURE) ||
      //(m_dwRenderModel == RENDERMODEL_LINESHADOWS) || (m_dwRenderModel == RENDERMODEL_SURFACESHADOWS));
   if (fCalcForNormals) {
      // yes, i do
	   MALLOCOPT_OKTOMALLOC;
      if (!pMemLight->Required (pInfo->dwNumNormals * sizeof(DWORD))) {
   	   MALLOCOPT_RESTORE;
         return NULL;   // error
      }
	   MALLOCOPT_RESTORE;
      pdwNormalIntensity = (DWORD*) pMemLight->p;
      pp = pInfo->paNormals;

      // assume that already normalized

      // calculate the intensity
      if (fBack) {
         CPoint pBack;
         for (i = 0; i < pInfo->dwNumNormals; i++, pp++) {
            pBack.Copy (pp);
            pBack.Scale (-1);
            *(pdwNormalIntensity++) = ModelInfiniteLight (&pBack);
         }
      }
      else {
         for (i = 0; i < pInfo->dwNumNormals; i++, pp++)
            *(pdwNormalIntensity++) = ModelInfiniteLight (pp);
      }

      pdwNormalIntensity = (DWORD*) pMemLight->p;
   }

   PGCOLOR pc, pc2;
   DWORD dwScale, dwC;
   BOOL  fScale;
   DWORD dwModel;
   dwModel = m_dwRenderModel & 0x0f;
   switch (dwModel) {
   case RM_MONO:     // mono color
      {
         if (!pMemColor->Required (sizeof(GCOLOR)))
            return NULL;
         pc = (PGCOLOR) pMemColor->p;
         Gamma (m_cMono, &pc->wRed);
      }
      *pdwUsage = 0; // only 1 color
      return pc;

   case RM_AMBIENT:     // objects colors
      {
         if (!pMemColor->Required (pInfo->dwNumColors * sizeof(GCOLOR)))
            return NULL;
         pc = (PGCOLOR) pMemColor->p;
         dwScale = dwAmbient1 / 16;
         fScale = (dwAmbient1 != 0x10000);

         for (i = pInfo->dwNumColors, pdw = pInfo->paColors, pc2 = pc; i; i--, pdw++, pc2++) {
            Gamma((COLORREF)*pdw, &pc2->wRed);
            if (fScale) {
               dwC = ((dwScale * (DWORD) pc2->wRed) / (0x10000 / 16));
               pc2->wRed = (WORD) (min(0xffff,dwC));
               dwC = ((dwScale * (DWORD) pc2->wGreen) / (0x10000 / 16));
               pc2->wGreen = (WORD) (min(0xffff,dwC));
               dwC = ((dwScale * (DWORD) pc2->wBlue) / (0x10000 / 16));
               pc2->wBlue = (WORD) (min(0xffff,dwC));
            }
         }
      }
      *pdwUsage = 1; // based on color index
      return pc;

   case RM_SHADED:     // shading model
   case RM_TEXTURE:     // texture model.
      {
   	   MALLOCOPT_OKTOMALLOC;
         if (!pMemColor->Required (pInfo->dwNumVertices * sizeof(GCOLOR))) {
      	   MALLOCOPT_RESTORE;
            return NULL;
         }
   	   MALLOCOPT_RESTORE;
         pc = (PGCOLOR) pMemColor->p;

         VERTEX *pv;
         for (i = pInfo->dwNumVertices, pv = pInfo->paVertices, pc2 = pc; i; i--, pv++, pc2++) {
            if (fGlow)
               dwScale = dwGlow;
            else
               dwScale = pdwNormalIntensity[pv->dwNormal] + dwAmbient2;
            dwScale /= 16;

            DWORD dwSpec;
            dwSpec = 0;
            if ((dwModel == RM_TEXTURE) && !fGlow) {
               // BUGFIX - Calculate specularity
               CPoint pNorm, pView;
               pNorm.Copy (&pInfo->paNormals[pv->dwNormal]);
               if (fBack)
                  pNorm.Scale (-1);
               if (m_dwCameraModel == CAMERAMODEL_FLAT) {
                  pView.Zero();
                  pView.p[2] = 1;
               }
               else {
                  pView.Copy (&pInfo->paPoints[pv->dwPoint]);
                  pView.Normalize();
                  pView.Scale (-1); // since going towards viewer
               }
               CPoint pH;
               pH.Add (&pView, &m_lightView);
               pH.Normalize();
               fp fNDotH, fVDotH, fLight;
               fNDotH = pNorm.DotProd (&pH);
               // NOTE: Because it would be too slow to get specularity based on surface
               // (since dont have that info here), just assume a specific type of surface
               WORD wSpecReflect, wSpecExponent;
               //wSpecReflect = 0xffff / 4;  // BUGFIX - Was / 2, from paint gloss
               //wSpecExponent = 2000; // from paint gloss
               wSpecReflect = 0xffff;  // plastic
               wSpecExponent = 2700; // plastic
               if ((fNDotH > 0) && wSpecReflect) {
                  fVDotH = pView.DotProd (&pH);
                  fVDotH = max(0,fVDotH);
                  fNDotH = pow (fNDotH, (fp)((fp)wSpecExponent / 100.0));
                  fLight = fNDotH * (fp) wSpecReflect;

                  // acount for exposure
                  fLight *= CM_LUMENSSUN / max(m_fExposure, CLOSE);  // counteract by lumens in the sun 
                  fLight *= (fp)m_dwIntensityLight / (fp)0x10000;
                  fLight = min(fLight,0x10000); // dont allow to go too high

                  dwSpec = (DWORD) fLight;
               }
            }

            Gamma(pInfo->paColors[pv->dwColor], &pc2->wRed);
            dwC = ((dwScale * (DWORD) pc2->wRed) / (0x10000 / 16)) + dwSpec;
            pc2->wRed = (WORD) (min(0xffff,dwC));
            dwC = ((dwScale * (DWORD) pc2->wGreen) / (0x10000 / 16)) + dwSpec;
            pc2->wGreen = (WORD) (min(0xffff,dwC));
            dwC = ((dwScale * (DWORD) pc2->wBlue) / (0x10000 / 16)) + dwSpec;
            pc2->wBlue = (WORD) (min(0xffff,dwC));

            if ((dwModel == 3) || (dwModel = 4)){
               // store light intensity away so can use on texture maps
               dwC = ((dwScale * (DWORD) 0xffff) / (0x10000 / 16) + dwSpec) / 4;
                  // BUGFIX - Divide by 4 so that can saturate textures
               pc2->wIntensity = (WORD) (min(0xffff,dwC));
            }
         }

      }
      *pdwUsage = 2; // based on vertex - since combine color and normal
      return pc;

   case RM_SHADOWS:  // no color model apply
   default:
      return NULL;
   }

}

/*******************************************************************************
CRenderTraditional::ShouldBackfaceCull - Given a polygon,whose points have all
be transformed to XY coordinates (divide by W), this returns TRUE if it should
be back-faced culled because it's pointing the wrong direction. FALSE if should
draw it.

inputs
   PPOLYDESCRIPT     pPoly - Polygon
   PPOLYRENDERINFO   pInfo - Get the points from here
   BOOL              *pfPerpToViewer - If non-NULL, this is filled with a flag inticating
                     the the polygon is perpendicular to the viewer (viwer is looking face-on).
                     If TRUE viewer is looing face on, FALSE, not.
returns
   BOOL - TRUE if should cull
*/
BOOL CRenderTraditional::ShouldBackfaceCull (const PPOLYDESCRIPT pPoly, const PPOLYRENDERINFO pInfo,
                                             BOOL *pfPerpToViewer)
{
   if (pfPerpToViewer)
      *pfPerpToViewer = FALSE;   // BUGFIX - Assume false

   // else, look at the polygon
   if (pPoly->wNumVertices < 3)
      return FALSE;

   BOOL fFoundVisible;
   fFoundVisible = FALSE;

   // NOTE: Use same polygonization as in CImage to make sure cull the right stuff

   DWORD i;
   DWORD dwNum;
   dwNum = (DWORD) pPoly->wNumVertices;
   for (i = 0; i < dwNum - 2; i++) {
      PDWORD   pdw;
      pdw = (DWORD*)(pPoly + 1);

      // two points
      PCPoint pc, pa, pb;
      pa = pInfo->paPoints + pInfo->paVertices[pdw[(dwNum - i / 2) % dwNum]].dwPoint;
      pc = pInfo->paPoints + pInfo->paVertices[pdw[(((i+1)/2) + 1) % dwNum]].dwPoint;
      if (i % 2)
         pb = pInfo->paPoints + pInfo->paVertices[pdw[dwNum - (i+1) / 2]].dwPoint;
      else
         pb = pInfo->paPoints + pInfo->paVertices[pdw[(i / 2 + 2) % dwNum]].dwPoint;

      // subtract the vectors to make points
      fp   a[3], b[3], n, an[2];
      a[0] = pa->p[0] - pc->p[0];
      a[1] = pa->p[1] - pc->p[1];
      a[2] = pa->p[2] - pc->p[2];
      b[0] = pb->p[0] - pc->p[0];
      b[1] = pb->p[1] - pc->p[1];
      b[2] = pb->p[2] - pc->p[2];

      // BUGFIX - Normalize a and b so that dont have roundoff error
#if 0 // think this code is busted
      if (pfPerpToViewer && !(*pfPerpToViewer)) {
         // BUGFIX - Forgot to include a[2] and b[2]
         n = sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
         if (fabs(n) > CLOSE*10) {   // BUGFIX - Change to close so draw walls straight on
            a[0] /= n;
            a[1] /= n;
         }
         n = sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
         if (fabs(n) > CLOSE*10) {   // BUGFIX - Change to close so draw walls straight on
            b[0] /= n;
            b[1] /= n;
         }
      }
#endif // 0

      // take the cross product. But we only care about z
      if (pfPerpToViewer) {
         an[0] = a[1] * b[2] - a[2] * b[1];
         an[1] = a[2] * b[0] - a[0] * b[2];
      }
      n /*[2]*/ = a[0] * b[1] - a[1] * b[0];

      // BUGFIX - Flipped from <= 0 to >= 0 because this seems to work
      fFoundVisible |= !(n >= 0); // (n <= EPSILON);
      if (pfPerpToViewer && !(*pfPerpToViewer)) {
         n /= sqrt(an[0]*an[0]+an[1]*an[1] + n*n);
         *pfPerpToViewer = (fabs(n) <  .1); // BUGFIX - Changed to CLOSE from EPSILON*10 so draw walls straight on
         // BUGFIX - This .1 is because of roundoff error issues. I think due to using float
      }

      if (fFoundVisible)
         return FALSE;
   }

   return !fFoundVisible;
}

/*******************************************************************************
CRenderTraditional::PolyRenderNoShadows - Called to render polygons if there aren't any shadows

inputs
   DWORD                dwThread - 0 ...RTINTERNALTHREADS-1
   PPOLYRENDERINFO      pInfo - Information that specified what polygons to render.
                           NOTE: The contents of pInfo, along with what it points to,
                           might be changed by this function.
returns
   none
*/
void CRenderTraditional::PolyRenderNoShadows (DWORD dwThread, const PPOLYRENDERINFO pInfo)
{
	MALLOCOPT_INIT;
   // convert to viewer space
   m_aRenderMatrix[dwThread].Transform (pInfo->dwNumPoints, pInfo->paPoints);

   // if want to clip then do so. Note that PPOLYRENDERINFO may be completely changed
   if (m_adwWantClipThis[dwThread]) {
      // NOTE: Taking advantage of the fact that added clipping planes simultaneously
      // to m_ClipAll and m_ClipOnlyScreen at the same time, until added some extra
      // to m_ClipAll. Means that this is OK since m_ClipAll is a superset.
	   MALLOCOPT_OKTOMALLOC;
      m_aClipAll[dwThread].ClipPolygons (m_adwWantClipThis[dwThread], pInfo);
	   MALLOCOPT_RESTORE;
   }

   if (m_fWantNormalsForThis) {
      // if this is set we need normals, so do them
      m_aRenderMatrix[dwThread].TransformNormal (pInfo->dwNumNormals, pInfo->paNormals,
         !pInfo->fAlreadyNormalized);
      pInfo->fAlreadyNormalized = TRUE;
   }

   // BUGFIX - Old location for convert to screen space

   // see if any of the surfaces have a glow
   BOOL fGlow;
   DWORD i;
   PPOLYDESCRIPT pd;
   switch (m_dwRenderModel & 0x0f) {
   case RM_MONO:     // mono color
      fGlow = FALSE; // not on mono
      break;
   case RM_AMBIENT:     // objects colors
   case RM_SHADED:     // shading model
      // keep fglow as it is
      fGlow = TRUE;
      break;
   default:
   case RM_TEXTURE:     // texture model.
      // can set fGlow to be false because know fGlow will only happen if have glowing
      // textures, in which case if drawing as a texture then will use the CTextueMap->LineGet()
      // for the glowing bit, and won't need the pcGlow anymore
      fGlow = FALSE;
      break;
   }
   for (i = pInfo->dwNumPolygons, pd = pInfo->paPolygons; fGlow && i; i--, pd = (PPOLYDESCRIPT)((PBYTE)pd + (sizeof(POLYDESCRIPT) + sizeof(DWORD)*pd->wNumVertices))) {
      if (!pInfo->paSurfaces[pd->dwSurface].Material.m_fGlow)
         fGlow = FALSE;
   }  // i

   // how much to scale glow
   fp fGlowScale;
   if (m_fExposure)
      fGlowScale = CM_LUMENSSUN / m_fExposure;
   else
      fGlowScale = 1;

   // determine colors
   BOOL  fBackfaceDifferent;
   PGCOLOR  pc, pcFront, pcBack, pcGlow;
   DWORD dwUsage = 0;   // init to 0 in case dont call colormodelapply
   pcFront = m_pZImage ? NULL : ColorModelApply (dwThread, pInfo, &dwUsage, FALSE, FALSE);
   switch (m_dwRenderModel & 0x0f) {
   case 0:  // monochomatic
   case 1:  // solid
      // since back side always same color as front side, dont just use the same
      pcBack = pcFront;
      fBackfaceDifferent = FALSE;
      break;
   default:
      // back side might be different color than front
      pcBack = m_pZImage ? NULL : ColorModelApply (dwThread, pInfo, &dwUsage, TRUE, FALSE);
      fBackfaceDifferent = TRUE;
      break;
   }
   if (fGlow)
      pcGlow = ColorModelApply (dwThread, pInfo, &dwUsage, FALSE, TRUE);
   else
      pcGlow = FALSE;

   // BUGFIX - Move convert to screen space after applying color model so can get
   // normals appropriate
   // convert to screen space
   TransformToScreen (pInfo->dwNumPoints, pInfo->paPoints);

   // loop through all the polygons
   DWORD j;
   PIHLEND pEnd;
   PVERTEX pv;
   PCPoint pp;
   DWORD *pdw, dwt;
   IDMISC m;
   PRENDERSURFACE pSurf;
   BOOL  fSolid;
   DWORD dwLastColor;
   DWORD dwNumVertex;
   BOOL  fLineDrawing, fCheckForEdge;
   PIHLEND apEnd[100];
   for (i = pInfo->dwNumPolygons, pd = pInfo->paPolygons; i; i--, pd = (PPOLYDESCRIPT)((PBYTE)pd + (sizeof(POLYDESCRIPT) + sizeof(DWORD)*pd->wNumVertices))) {
      fLineDrawing = !(m_dwRenderModel & 0xf0);

      fCheckForEdge = FALSE;

      // potentially skip all of this if dont want to draw transparent
      // BUGFIX - only skip if if no shadows... ignore the m_wTransparency setting here
      // since test elsewhere in scanline render
      if (m_fForShadows && (// pInfo->paSurfaces[pd->dwSurface].Material.m_wTransparency ||
         pInfo->paSurfaces[pd->dwSurface].Material.m_fNoShadows))
         continue;

      // backface cull this?
      BOOL fCheckForBackface, fBackRet;
      fCheckForBackface = fBackfaceDifferent || (m_fBackfaceCull && pd->fCanBackfaceCull);
      
      if ((m_dwCameraModel == CAMERAMODEL_FLAT) && !fLineDrawing) {
         // if it's the flat camera model then check for backface, because can find of it edge on
         fCheckForEdge = TRUE;
         fCheckForBackface = TRUE;
      }

      if (fCheckForBackface) {
         fBackRet = ShouldBackfaceCull(pd, pInfo, fCheckForEdge ? &fLineDrawing : NULL);

         // if we're checking for an edge and find it is an edge, then don't backface full
         if (fCheckForEdge && fLineDrawing)
            fBackRet = FALSE;
      }
      else
         fBackRet = FALSE;

      // if backface culling returned TRUE and we're allowed to get rid of this then do so
      if (pd->fCanBackfaceCull && fBackRet && m_fBackfaceCull) {
#ifdef _TIMERS
        gRenderStats.dwPolyBackface++;
#endif
         continue;
      }

      // which side for colors
      pc = fBackRet ? pcBack : pcFront;

      dwNumVertex = min(sizeof(apEnd)/sizeof(PIHLEND),(DWORD)pd->wNumVertices);

      // generate the polygon info stuff for the image
	   MALLOCOPT_OKTOMALLOC;
      if (!m_amemPolyVertex[dwThread].Required (dwNumVertex * sizeof(IHLEND))) {
      	MALLOCOPT_RESTORE;
         continue;
      }
   	MALLOCOPT_RESTORE;

      // get the texture map
      BOOL  fTextureMap;
      PCTextureMapSocket pMap;
      fTextureMap = FALSE;
      pMap = NULL;
      if (((m_dwRenderModel == RENDERMODEL_SURFACETEXTURE) || (m_dwRenderModel == RENDERMODEL_SURFACESHADOWS))
         && (pd->dwSurface < pInfo->dwNumSurfaces)) {
         fTextureMap = pInfo->paSurfaces[pd->dwSurface].fUseTextureMap;

         // Actually load a C++ object for the texture map and
         // see that that's valid before saying yea. to texture maps
         if (fTextureMap)
            pMap = TextureCacheGet (m_dwRenderShard, &pInfo->paSurfaces[pd->dwSurface], NULL, NULL);
         if (!pMap)
            fTextureMap = FALSE;
      }

      pEnd = (PIHLEND) m_amemPolyVertex[dwThread].p;
      pdw = (DWORD*) (pd+1);
      fSolid = TRUE;
      dwLastColor = (DWORD)-1;
      for (j = 0; j < dwNumVertex; j++, pEnd++,pdw++) {
         apEnd[j] = pEnd;
         pv = pInfo->paVertices + *pdw;
         pp = pInfo->paPoints + pv->dwPoint;

         pEnd->x = pp->p[0];
         pEnd->y = pp->p[1];
         pEnd->z = pp->p[2];
         pEnd->w = pp->p[3];

         switch (dwUsage) {
         case 1:  // color index
            dwt = pv->dwColor;
            break;
         case 2:  // vertex
            dwt = *pdw;
            break;
         case 3:  // point
            dwt = pv->dwPoint;
            break;
         default:
         case 0:  // always the same
            dwt = 0;
            break;
         }

         if (fTextureMap) {
            // stored the light intensity away with the color
            pEnd->aColor[0] = pEnd->aColor[1] = pEnd->aColor[2] = pc ? pc[dwt].wIntensity : 0;

            // get the index
            PTEXTPOINT5 pt;
            pt = (pv->dwTexture < pInfo->dwNumTextures) ? (pInfo->paTextures + pv->dwTexture) : 0;

            if (pt) {
               pEnd->tp = *pt;
            }
            else
               memset (&pEnd->tp, 0, sizeof(pEnd->tp));
         }
         else {
            if (pcGlow && pInfo->paSurfaces[pd->dwSurface].Material.m_fGlow) {
               pEnd->aColor[0] = pcGlow[dwt].wRed;
               pEnd->aColor[1] = pcGlow[dwt].wGreen;
               pEnd->aColor[2] = pcGlow[dwt].wBlue;
            }
            else if (pc) {   // take color calculated
               pEnd->aColor[0] = pc[dwt].wRed;
               pEnd->aColor[1] = pc[dwt].wGreen;
               pEnd->aColor[2] = pc[dwt].wBlue;
            }
            // else, if no PC then ignore color
         }

         // check for solid
         if (dwLastColor != (DWORD)-1) {
            // we have a previous color
            if (dwLastColor != dwt)
               fSolid = FALSE;
         }
         else
            dwLastColor = dwt;   // remember this
      }

      // shape info
      pSurf = pInfo->paSurfaces + pd->dwSurface;
      m.dwID = m_adwObjectID[dwThread] | pSurf->wMinorID;
      memcpy (&m.Material, &pSurf->Material, sizeof(pSurf->Material));
      //m.wTransparency = pSurf->wTransparency;
      m.dwIDPart = pd->dwIDPart;
      m.fTextureError = 4;
      m.fTextureError = max(2,m.fTextureError);
      if (fSolid) {
         if (pcGlow && pInfo->paSurfaces[pd->dwSurface].Material.m_fGlow) {
            m.wRed = pcGlow[dwLastColor].wRed;
            m.wGreen = pcGlow[dwLastColor].wGreen;
            m.wBlue = pcGlow[dwLastColor].wBlue;
         }
         else if (pc) {
            m.wRed = pc[dwLastColor].wRed;
            m.wGreen = pc[dwLastColor].wGreen;
            m.wBlue = pc[dwLastColor].wBlue;
         }
         // else if no pc then ignore
      }

      // if there's a texture map, and multithreaded, alert it about the multithreaded
      // nature
      // only do if in main thread
      if (!dwThread && m_dwThreadsMax && pMap && (pMap != m_pTextureLast)) {
         m_pTextureLast = pMap;
         pMap->ForceCache (m_dwForceCache);
         //pMap->ForceCache (
         //   FORCECACHE_TRANS |
         //   (m_pImage ?  (FORCECACHE_COMBINED | FORCECACHE_GLOW) : 0 /* m_pZImage */)
         //   );
      }

      if (!fLineDrawing) {
         // have all the points, draw it
         // NOTE: Not checking m_pImage here since ImageShader should be used in that case
         if (!dwThread && m_dwThreadsPoly) // only do if in main thread
            MultiThreadedDrawPolygon (TRUE, dwNumVertex, apEnd, &m, fSolid, pMap, fGlowScale, TRUE);
         else if (m_pImage)
            m_pImage->DrawPolygon (dwNumVertex, apEnd, &m, fSolid, pMap, fGlowScale, TRUE, dwThread ? (dwThread-1) : 0, m_fFinalRender);
         else // m_pZImage
            m_pZImage->DrawPolygon (dwNumVertex, apEnd, &m, fSolid, pMap, TRUE, dwThread ? (dwThread-1) : 0);
#ifdef _TIMERS
         gRenderStats.dwNumPoly++;
#endif
      }
      else {
         // do line rendering
         if (!dwThread && m_dwThreadsPoly) // only do if in main thread
            MultiThreadedDrawPolygon (FALSE, dwNumVertex, apEnd, &m, FALSE, NULL, 0, FALSE);
         else for (j = 0; j < dwNumVertex; j++) {
            if (m_pImage)
               m_pImage->DrawLine (apEnd[j], apEnd[(j+1)%dwNumVertex], &m);
            else // if (m_pZImage
               m_pZImage->DrawLine (apEnd[j], apEnd[(j+1)%dwNumVertex], &m);
            // NOTE: Not checking m_pFImage
         } // j
      }

      // Free up texture map object
      if (!dwThread) // only do if in main thread
         TextureCacheRelease (m_dwRenderShard, pMap);
   }
}

/*******************************************************************************
CRenderTraditional::PolyRenderShadows - Called to render if there are shadows

inputs
   DWORD                dwThread - 0 ...RTINTERNALTHREADS-1
   PPOLYRENDERINFO      pInfo - Information that specified what polygons to render.
                           NOTE: The contents of pInfo, along with what it points to,
                           might be changed by this function.
returns
   none
*/
void CRenderTraditional::PolyRenderShadows (DWORD dwThread, const PPOLYRENDERINFO pInfo)
{
   MALLOCOPT_INIT;
   // convert to viewer space
   m_aRenderMatrix[dwThread].Transform (pInfo->dwNumPoints, pInfo->paPoints);

   // if want to clip then do so. Note that PPOLYRENDERINFO may be completely changed
   if (m_adwWantClipThis[dwThread]) {
      // NOTE: Taking advantage of the fact that added clipping planes simultaneously
      // to m_ClipAll and m_ClipOnlyScreen at the same time, until added some extra
      // to m_ClipAll. Means that this is OK since m_ClipAll is a superset.
	   MALLOCOPT_OKTOMALLOC;
      m_aClipAll[dwThread].ClipPolygons (m_adwWantClipThis[dwThread], pInfo);
	   MALLOCOPT_RESTORE;
   }

   // should always ant normals for this
   if (!m_fWantNormalsForThis)
      return;

   if (m_fWantNormalsForThis) {
      // if this is set we need normals, so do them
      m_aRenderMatrix[dwThread].TransformNormal (pInfo->dwNumNormals, pInfo->paNormals,
         !pInfo->fAlreadyNormalized);
      pInfo->fAlreadyNormalized = TRUE;
   }

   // convert to screen space
   TransformToScreen (pInfo->dwNumPoints, pInfo->paPoints);

   // loop through all the polygons
   DWORD i, j;
   PPOLYDESCRIPT pd;
   PISHLEND pEnd;
   PVERTEX pv;
   PCPoint pp, pn;
   DWORD *pdw;
   ISDMISC m;
   PRENDERSURFACE pSurf;
   BOOL  fSolid;
   DWORD dwLastColor;
   DWORD dwNumVertex;
   BOOL  fLineDrawing, fCheckForEdge;
   PISHLEND apEnd[100];
   for (i = pInfo->dwNumPolygons, pd = pInfo->paPolygons; i; i--, pd = (PPOLYDESCRIPT)((PBYTE)pd + (sizeof(POLYDESCRIPT) + sizeof(DWORD)*pd->wNumVertices))) {
      fLineDrawing = !(m_dwRenderModel & 0xf0);

      fCheckForEdge = FALSE;

      // potentially skip all of this if dont want to draw transparent
      if (m_fForShadows && (pInfo->paSurfaces[pd->dwSurface].Material.m_wTransparency ||
         pInfo->paSurfaces[pd->dwSurface].Material.m_fNoShadows))
         continue;

      // backface cull this?
      BOOL fCheckForBackface, fBackRet;
      fCheckForBackface = (m_fBackfaceCull && pd->fCanBackfaceCull);
      
      if ((m_dwCameraModel == CAMERAMODEL_FLAT) && !fLineDrawing) {
         // if it's the flat camera model then check for backface, because can find of it edge on
         fCheckForEdge = TRUE;
         fCheckForBackface = TRUE;
      }

      if (fCheckForBackface) {
         fBackRet = ShouldBackfaceCull(pd, pInfo, fCheckForEdge ? &fLineDrawing : NULL);

         // if we're checking for an edge and find it is an edge, then don't backface full
         if (fCheckForEdge && fLineDrawing)
            fBackRet = FALSE;
      }
      else
         fBackRet = FALSE;

      // if backface culling returned TRUE and we're allowed to get rid of this then do so
      if (pd->fCanBackfaceCull && fBackRet && m_fBackfaceCull) {
#ifdef _TIMERS
        gRenderStats.dwPolyBackface++;
#endif
         continue;
      }

      dwNumVertex = min(sizeof(apEnd)/sizeof(PISHLEND),(DWORD)pd->wNumVertices);

      // generate the polygon info stuff for the image
	   MALLOCOPT_OKTOMALLOC;
      if (!m_amemPolyVertex[dwThread].Required (dwNumVertex * sizeof(ISHLEND))) {
   	   MALLOCOPT_RESTORE;
         continue;
      }
	   MALLOCOPT_RESTORE;

      // get the texture map
      BOOL  fTextureMap;
      PCTextureMapSocket pMap;
      fTextureMap = FALSE;
      pMap = NULL;
      if (((m_dwRenderModel == RENDERMODEL_SURFACETEXTURE) || (m_dwRenderModel == RENDERMODEL_SURFACESHADOWS))
         && (pd->dwSurface < pInfo->dwNumSurfaces)) {
         fTextureMap = pInfo->paSurfaces[pd->dwSurface].fUseTextureMap;

         // Actually load a C++ object for the texture map and
         // see that that's valid before saying yea. to texture maps
         if (fTextureMap)
            pMap = TextureCacheGet (m_dwRenderShard, &pInfo->paSurfaces[pd->dwSurface], NULL, NULL);
         if (!pMap)
            fTextureMap = FALSE;
      }

      pEnd = (PISHLEND) m_amemPolyVertex[dwThread].p;
      pdw = (DWORD*) (pd+1);
      fSolid = TRUE;
      dwLastColor = (DWORD)-1;
      for (j = 0; j < dwNumVertex; j++, pEnd++,pdw++) {
         apEnd[j] = pEnd;
         pv = pInfo->paVertices + *pdw;
         pp = pInfo->paPoints + pv->dwPoint;
         pn = pInfo->paNormals + pv->dwNormal;

         pEnd->x = pp->p[0];
         pEnd->y = pp->p[1];
         pEnd->z = pp->p[2];
         pEnd->w = pp->p[3];

         // normals
         pEnd->pNorm.Copy (pn); // know it has already been normalized

         if (fTextureMap) {
            // get the index
            PTEXTPOINT5 pt;
            pt = (pv->dwTexture < pInfo->dwNumTextures) ? (pInfo->paTextures + pv->dwTexture) : 0;

            if (pt) {
               pEnd->tp = *pt;
            }
            else
               memset (&pEnd->tp, 0, sizeof(pEnd->tp));
         }
         else {
            memset (&pEnd->tp, 0, sizeof(pEnd->tp));
         }

         // check for solid
         if (dwLastColor == (DWORD)-1)
            dwLastColor = pv->dwColor;   // remember this
      }

      // shape info
      pSurf = pInfo->paSurfaces + pd->dwSurface;
      if ((dwLastColor == -1) || (dwLastColor >= pInfo->dwNumColors)) {
         // shouldnt happen
         m.wRed = m.wGreen = m.wBlue = 0;
      }
      else {
         Gamma(pInfo->paColors[dwLastColor], &m.wRed);
      }
      m.dwID = m_adwObjectID[dwThread] | pSurf->wMinorID;
      memcpy (&m.Material, &pSurf->Material, sizeof(pSurf->Material));
      //m.wTransparency = pSurf->wTransparency;
      m.dwIDPart = pd->dwIDPart;
      m.pTexture = fTextureMap ? pMap : NULL;

      // if there's a texture map, and multithreaded, alert it about the multithreaded
      // nature
      // BUGFIX - Only forcecache if main thread
      if (!dwThread && m_dwThreadsMax && pMap && (pMap != m_pTextureLast)) {
         m_pTextureLast = pMap;
         pMap->ForceCache (m_dwForceCache);
         // pMap->ForceCache (FORCECACHE_TRANS);
      }

      if (!fLineDrawing) {
         // have all the points, draw it
         if (!dwThread && m_dwThreadsPoly) // only from main thread
            MultiThreadedDrawPolygon (TRUE, dwNumVertex, apEnd, &m, TRUE);
         else
            m_pImageShader->DrawPolygon (dwNumVertex, apEnd, &m, TRUE, dwThread ? (dwThread-1) : 0);
#ifdef _TIMERS
         gRenderStats.dwNumPoly++;
#endif
      }
      else {
         // do line rendering
         if (!dwThread && m_dwThreadsPoly) // only from main thread
            MultiThreadedDrawPolygon (FALSE, dwNumVertex, apEnd, &m, TRUE);
         else for (j = 0; j < dwNumVertex; j++)
            m_pImageShader->DrawLine (apEnd[j], apEnd[(j+1)%dwNumVertex], &m, dwThread ? (dwThread-1) : 0);
      }

      // Free up texture map object
      if (!dwThread)
         TextureCacheRelease (m_dwRenderShard, pMap); // only from main thread
   }
}


/*******************************************************************************
CRenderTraditional::PolyRender - Called by the objects when it's rendering.

inputs
   PPOLYRENDERINFO      pInfo - Information that specified what polygons to render.
                           NOTE: The contents of pInfo, along with what it points to,
                           might be changed by this function.
returns
   none
*/
void CRenderTraditional::PolyRender (PPOLYRENDERINFO pInfo)
{
   // if rendering to clone then differnet polyrender
   if (m_pRayCur) {
      // make a new one
	   MALLOCOPT_INIT;
	   MALLOCOPT_OKTOMALLOC;
      PCRayObject pNew;
      pNew = new CRayObject (m_dwRenderShard, NULL, this, m_fCloneRender);
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
      return;
   }

   if (m_pImageShader)
      PolyRenderShadows (0, pInfo);
   else
      PolyRenderNoShadows (0, pInfo);
}


/*******************************************************************************
CRenderTraditional::IsCompletelyCovered - Checks to see if the object would
be completely covered by what's already there.

Internal method used to make it faster to check for covered if there's
a small buffer.

inputs
   RECT     *pRect - rectangle. This can be beyond the end of the CZImage screen.
   fp   fZ - Depth in meters
returns
   BOOL - TRUE if it's completely coverd
*/
BOOL CRenderTraditional::IsCompletelyCovered (const RECT *pRect, fp fZ)
{
   // special case for a small image
   if (m_pZImageSmall) {
      RECT rSmall;
      rSmall.left = pRect->left / ZIMAGESMALLSCALE;
      rSmall.top = pRect->top / ZIMAGESMALLSCALE;
      rSmall.right = (pRect->right + ZIMAGESMALLSCALE - 1) / ZIMAGESMALLSCALE;
      rSmall.bottom = (pRect->bottom + ZIMAGESMALLSCALE - 1) / ZIMAGESMALLSCALE;

      return m_pZImageSmall->IsCompletelyCovered (&rSmall, fZ);
   }

   if (m_pImageShader)
      return m_pImageShader->IsCompletelyCovered (pRect, fZ);
   else if (m_pImage)
      return m_pImage->IsCompletelyCovered (pRect, fZ);
   else if (m_pZImage)
      return m_pZImage->IsCompletelyCovered (pRect, fZ);
   else
      return FALSE;  // error
}

/*******************************************************************************
CRenderTraditional::RenderObject - Tells the object to draw iteself.

inputs
  PROINFO         pObject - object
returns
   none
*/
void CRenderTraditional::RenderObject (PROINFO pObject)
{
   // get the object
   if (!pObject->pObject)
      return;

#ifdef _TIMERS
   gRenderStats.dwRenderObject++;
#endif
   // set up some variables
   m_adwObjectID[0] = (pObject->dwNumber) << 16;   // don't care about others
   m_fDetailForThis = pObject->fDetail;
   m_adwWantClipThis[0] = pObject->dwClipPlanes; // ignore others

   // do trivial removal - if this object has no chance of being drawn then don't bother
   if (pObject->fCanUseZTest) {
      RECT r;
      r.left = (int) floor (pObject->aPoint[0].p[0]);
      r.right = (int) ceil (pObject->aPoint[1].p[0]);
      r.top = (int) floor (pObject->aPoint[0].p[1]);
      r.bottom = (int) ceil (pObject->aPoint[1].p[1]);
      if (IsCompletelyCovered (&r, pObject->fZNear)) {
   #ifdef _TIMERS
         gRenderStats.dwObjCovered++;
   #endif
         return;
      }
   }

#ifdef _TIMERS
   gRenderStats.dwObjNotCovered++;
#endif
   // push the current matrix so objects don't muck up each other
   m_aRenderMatrix[0].Push(); // don't bother with others

   OBJECTRENDER or;
   memset (&or, 0, sizeof(or));
   or.pRS = this;
   or.dwReason = m_fForShadows ? ORREASON_SHADOWS :
      (m_fFinalRender ? ORREASON_FINAL : ORREASON_WORKING);
   or.dwSpecial = m_dwSpecial;
   or.dwShow = m_dwRenderShow;
   or.fSelected = pObject->fSelected;
   if (m_fPassCameraValid) {
      or.fCameraValid = m_fPassCameraValid;
      or.pCamera.Copy (&m_pPassCamera);
   }
   pObject->pObject->Render (&or, pObject->dwSubObject);

   // pop the matrix
   m_aRenderMatrix[0].Pop();  // don't bother with others

}

/*******************************************************************************
CRenderTraditional::PixelToZ - Given a pixel, this returns the Z value.

inputs
   DWORD    dwX, dwY - pixels
returns
   fp - Z value. Positive values are away from the user. Returns a large
         number if clicking on infinity.
*/
fp CRenderTraditional::PixelToZ (DWORD dwX, DWORD dwY)
{
   if (m_pImage) {
      PIMAGEPIXEL pip = m_pImage->Pixel (dwX, dwY);
      return pip->fZ;
   }
   else if (m_pFImage) {
      PFIMAGEPIXEL pfp = m_pFImage->Pixel (dwX, dwY);
            // NOTE - FImage pixel not tested
      return pfp->fZ;
   }
   else { // if (m_pZImage) {
      PZIMAGEPIXEL pzp = m_pZImage->Pixel (dwX, dwY);
            // NOTE - ZImage pixel not tested
      return pzp->fZ;
   }
}

/*******************************************************************************
CRenderTraditional::PixelToViewerSpace - Given a pixel X,Y, (and Z) this converts
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
__inline void CRenderTraditional::PixelToViewerSpace (fp dwX, fp dwY, fp fZ, PCPoint p)
{
   fp fX, fY, fW;

   // start working backwards
   fW = (m_dwCameraModel == CAMERAMODEL_FLAT) ? 1 : (fZ * m_fTanHalfCameraFOV);
   fZ *= -1;
   fp fScale = fW / m_afScalePixel[0];
   fX = ((fp) dwX - m_afCenterPixel[0]) * fScale;
   fY = -((fp) dwY - m_afCenterPixel[1]) * fScale;  // NOTE: This didn't seem to come out right
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
CRenderTraditional::PixelToWorldSpace - Given a pixel XY (and Z), this converts
from the screen coordinates to a position in world space.

inputs
   fp    dwX, dwY - pixels
   fp   fZ - Comes from PixelToZ(), or just make it up. Positive #'s further away
   PCPoint  p - Filled in with X,Y,Z,W in camera space
returns
   none
*/
__inline void CRenderTraditional::PixelToWorldSpace (fp dwX, fp dwY, fp fZ, PCPoint p)
{
   // convert to a point in viewer space
   CPoint v;
   PixelToViewerSpace (dwX, dwY, fZ, &v);

   // then to world space
   m_aRenderMatrix[0].TransformViewSpaceToWorldSpace (&v, p);  // don't bother with others
}


/*******************************************************************************
CRenderTraditional::DrawGrid - Draws a grid on the current image using the z-buffer
depth and ::PixelToWorldSpace().

inputs
   COLORREF    crMajor - major color
   fp      fMajorSize - Size of the major grid in meters
   COLORREF    crMinor - Minor color
   fp      fMinorSize - Size of the minor grid in meters
   PCMatrix    mWorldToGrid - Converts from world coordinates to grid coordinates
   BOOL        fUD - If TRUE, draw the up/down grid. Else, only NS and EW
   BOOL        fDots - If TRUE, draw dots at corners instead of lines.
returns
   none
*/

#define GRIDSKIP        2     // do every other pixel

typedef struct {
   DWORD dwMajor; // 3 bytes represent 3 dimensions mod 256
   DWORD dwMinor; // 3 bytes represent 3 dimensions mod 256
} GINFO, *PGINFO;
void CRenderTraditional::DrawGrid (COLORREF crMajor, fp fMajorSize, COLORREF crMinor, fp fMinorSize,
                                   PCMatrix pmWorldToGrid, BOOL fUD, BOOL fDots)
{
   // cant draw grid on Z
   if (m_pZImage)
      return;

   // BUGFIX - Modify draw grid so it does every other one, and is faster to draw

   // first, allocate enough space for GINFO's and calculate them
   CMem mem;
   DWORD dwX, dwY, dwWidth, dwHeight;
   dwWidth = Width() / GRIDSKIP;
   dwHeight = Height() / GRIDSKIP;
   DWORD dwNum = dwWidth * dwHeight;
   if (!mem.Required (sizeof(GINFO) * dwNum))
      return;

   // if the world to grid is identity then dont use it
   CMatrix mIdent;
   mIdent.Identity();
   if (pmWorldToGrid && mIdent.AreClose(pmWorldToGrid))
      pmWorldToGrid = NULL;

   PIMAGEPIXEL pip;
   PFIMAGEPIXEL pfp;
   PGINFO pgi;
   pgi = (PGINFO) mem.p;
   fp fZ;
   CPoint p, p2;
   for (dwY = 0; dwY < dwHeight; dwY++) {
      if (m_pImage)
         pip = m_pImage->Pixel(0, dwY*GRIDSKIP);
      else
         pfp = m_pFImage->Pixel (0, dwY*GRIDSKIP);
            // NOTE - FImage grid not tested

      for (dwX = 0; dwX < dwWidth; dwX++,  m_pImage ? (pip += GRIDSKIP) : (PIMAGEPIXEL)(pfp += GRIDSKIP), pgi++) {
         fZ = m_pImage ? pip->fZ : pfp->fZ;

         // if nothing drawn there then don't bother to grid
         if (fZ > 1000.0) {
            pgi->dwMajor = pgi->dwMinor = (DWORD)-1;
            continue;
         }

         // else, calculate world space
         PixelToWorldSpace (dwX * GRIDSKIP, dwY * GRIDSKIP, fZ, &p);
         p.p[3] = 1;
         if (pmWorldToGrid)
            p.MultiplyLeft (pmWorldToGrid);
         if (!fUD)
            p.p[2] = 0; // if no UD then just set all to 0

         // scale by the major
         if (fMajorSize) {
            p2.Copy(&p);
            p2.Scale(1.0 / fMajorSize);
            p2.p[0] = myfmod(floor(p2.p[0]),256);
            p2.p[1] = myfmod(floor(p2.p[1]),256);
            p2.p[2] = myfmod(floor(p2.p[2]),256);
            pgi->dwMajor = RGB((BYTE)p2.p[0], (BYTE)p2.p[1], (BYTE)p2.p[2]);
         }
         else
            pgi->dwMajor = (DWORD)-1;

         // scale by the minor
         if (fMinorSize) {
            p2.Copy(&p);
            p2.Scale(1.0 / fMinorSize);
            p2.p[0] = myfmod(floor(p2.p[0]),256);
            p2.p[1] = myfmod(floor(p2.p[1]),256);
            p2.p[2] = myfmod(floor(p2.p[2]),256);
            pgi->dwMinor = RGB((BYTE)p2.p[0], (BYTE)p2.p[1], (BYTE)p2.p[2]);
         }
         else
            pgi->dwMinor = (DWORD)-1;

      }
   }  // dwY


   // loop over all the pixels in the image and apply the color if there's a transition
   // between major or minor
   PGINFO pgiRight, pgiDown;
   pgi = (PGINFO) mem.p;
   WORD  awcMajor[3], awcMinor[3];
   Gamma(crMajor, awcMajor);
   Gamma(crMinor, awcMinor);
   for (dwY = 0; dwY < dwHeight; dwY++) {
      if (m_pImage)
         pip = m_pImage->Pixel (0, dwY * GRIDSKIP);
      else
         pfp = m_pFImage->Pixel (0, dwY * GRIDSKIP);
            // NOTE - FImage grid not tested

      for (dwX = 0; dwX < dwWidth; dwX++,  m_pImage ? (pip += GRIDSKIP) : (PIMAGEPIXEL)(pfp += GRIDSKIP), pgi++) {
         // find the pixel to the right and below
         if (dwX) {
            pgiRight = pgi-1;
         }
         else {
            pgiRight = NULL;
         }

         if (dwY) {
            pgiDown = pgi-dwWidth;
         }
         else {
            pgiDown = NULL;
         }

         // see if there's a major transition
         int iTrans;
         DWORD dwTrans;
         if (pgi->dwMajor != (DWORD)-1) {
            // count the number of transitions
            dwTrans = 0;
            iTrans = fDots ? -1 : 0;
            if (pgiRight && (pgiRight->dwMajor != (DWORD) -1) && (pgiRight->dwMajor != pgi->dwMajor)) {
               if (GetRValue(pgiRight->dwMajor) != GetRValue(pgi->dwMajor))
                  dwTrans |= 0x01;
               if (GetGValue(pgiRight->dwMajor) != GetGValue(pgi->dwMajor))
                  dwTrans |= 0x02;
               if (GetBValue(pgiRight->dwMajor) != GetBValue(pgi->dwMajor))
                  dwTrans |= 0x04;
            }
            if (pgiDown && (pgiDown->dwMajor != (DWORD) -1) && (pgiDown->dwMajor != pgi->dwMajor)) {
               if (GetRValue(pgiDown->dwMajor) != GetRValue(pgi->dwMajor))
                  dwTrans |= 0x01;
               if (GetGValue(pgiDown->dwMajor) != GetGValue(pgi->dwMajor))
                  dwTrans |= 0x02;
               if (GetBValue(pgiDown->dwMajor) != GetBValue(pgi->dwMajor))
                  dwTrans |= 0x04;
            }
            for (; dwTrans; dwTrans >>= 1)
               if (dwTrans & 0x01)
                  iTrans++;

            if (iTrans > 0) {
               // have a transition
               if (m_pImage) {
                  pip->wRed = awcMajor[0];
                  pip->wGreen = awcMajor[1];
                  pip->wBlue = awcMajor[2];
               }
               else {
                  pfp->fRed = awcMajor[0];
                  pfp->fGreen = awcMajor[1];
                  pfp->fBlue = awcMajor[2];
                  // NOTE - FImage grid not tested
               }

               continue;   // done
            }
         }

         // minor transition
         if (pgi->dwMinor != (DWORD)-1) {
            dwTrans = 0;
            iTrans = fDots ? -1 : 0;
            if (pgiRight && (pgiRight->dwMinor != (DWORD) -1) && (pgiRight->dwMinor != pgi->dwMinor)) {
               if (GetRValue(pgiRight->dwMinor) != GetRValue(pgi->dwMinor))
                  dwTrans |= 0x01;
               if (GetGValue(pgiRight->dwMinor) != GetGValue(pgi->dwMinor))
                  dwTrans |= 0x02;
               if (GetBValue(pgiRight->dwMinor) != GetBValue(pgi->dwMinor))
                  dwTrans |= 0x04;
            }
            if (pgiDown && (pgiDown->dwMinor != (DWORD) -1) && (pgiDown->dwMinor != pgi->dwMinor)) {
               if (GetRValue(pgiDown->dwMinor) != GetRValue(pgi->dwMinor))
                  dwTrans |= 0x01;
               if (GetGValue(pgiDown->dwMinor) != GetGValue(pgi->dwMinor))
                  dwTrans |= 0x02;
               if (GetBValue(pgiDown->dwMinor) != GetBValue(pgi->dwMinor))
                  dwTrans |= 0x01;
            }
            for (; dwTrans; dwTrans >>= 1)
               if (dwTrans & 0x01)
                  iTrans++;

            if (iTrans > 0) {
               // have a transition
               if (m_pImage) {
                  pip->wRed = pip->wRed / 2 + awcMinor[0] / 2;
                  pip->wGreen = pip->wGreen / 2 + awcMinor[1] / 2;
                  pip->wBlue = pip->wBlue / 2 + awcMinor[2] / 2;
               }
               else {
                  pfp->fRed = pfp->fRed / 2 + (fp) awcMinor[0] / 2;
                  pfp->fGreen = pfp->fGreen / 2 + (fp) awcMinor[1] / 2;
                  pfp->fBlue = pfp->fBlue / 2 + (fp) awcMinor[2] / 2;
                  // NOTE - FImage grid not tested
               }

               continue;   // done
            }
         }

      }
   } // dwY
}



/**********************************************************************************
CRenderTraditional::ClipPlaneGet - Gets a clip plane (identified by a unique
identifier dwID) and copies the information into pPlane.

inputs
   DWORD          dwID - Unique identifier. NOT an index.
   PRTCLIPPLANE   pPlane - Filled in the plane information, if successful
returns
   BOOL - TRUE if successful
*/
BOOL CRenderTraditional::ClipPlaneGet (DWORD dwID, PRTCLIPPLANE pPlane)
{
   // loop
   PRTCLIPPLANE p;
   DWORD i;
   for (i = 0; i < m_listRTCLIPPLANE.Num(); i++) {
      p = (PRTCLIPPLANE) m_listRTCLIPPLANE.Get(i);

      if (p->dwID == dwID) {
         // found it
         *pPlane = *p;
         return TRUE;
      }
   }

   return FALSE;
}

/**********************************************************************************
CRenderTraditional::ClipPlaneSet - Sets a clip plane (identified by a unique
identifier dwID). If the plane already exists it's replaced by the new one.
If it doesnt exist it's added.

inputs
   DWORD          dwID - Unique identifier. NOT an index.
   PRTCLIPPLANE   pPlane - Plane information.
returns
   BOOL - TRUE if successful
*/
BOOL CRenderTraditional::ClipPlaneSet (DWORD dwID, PRTCLIPPLANE pPlane)
{
   pPlane->dwID = dwID; // just to be sure

   // loop
   PRTCLIPPLANE p;
   DWORD i;
   for (i = 0; i < m_listRTCLIPPLANE.Num(); i++) {
      p = (PRTCLIPPLANE) m_listRTCLIPPLANE.Get(i);

      if (p->dwID == dwID) {
         // found it
         *p = *pPlane;

         // rebuilding clipping
         ClipRebuild ();
         return TRUE;
      }
   }

   // add it
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_listRTCLIPPLANE.Add (pPlane);
	MALLOCOPT_RESTORE;

   // rebuild the planes
   ClipRebuild ();

   return TRUE;
}
/**********************************************************************************
CRenderTraditional::ClipPlaneRemove - Removed a clip plane (identified by a unique
identifier dwID).

inputs
   DWORD          dwID - Unique identifier. NOT an index.
returns
   BOOL - TRUE if successful
*/
BOOL CRenderTraditional::ClipPlaneRemove (DWORD dwID)
{
   // loop
   PRTCLIPPLANE p;
   DWORD i;
   for (i = 0; i < m_listRTCLIPPLANE.Num(); i++) {
      p = (PRTCLIPPLANE) m_listRTCLIPPLANE.Get(i);

      if (p->dwID == dwID) {
         // found it
         m_listRTCLIPPLANE.Remove (i);

         ClipRebuild();
         return TRUE;
      }
   }

   return FALSE;
}


/**********************************************************************************
CRenderTraditional::ClipPlaneClear - Clears out the clip plane
*/
void CRenderTraditional::ClipPlaneClear (void)
{
   if (m_listRTCLIPPLANE.Num()) {
      m_listRTCLIPPLANE.Clear();
      ClipRebuild();
   }
}

/*******************************************************************************
CRenderTraditiaonl::CacheClear - Call this if you change any of the public
member variables. This will ensure that the cached background and cached selection
images will be reset. This is called implicitly when any functions in CRenderTraditional
are called that could change the image.

*/
void CRenderTraditional::CacheClear (void)
{
   m_dwCacheImageValid = 0;
}


/*******************************************************************************
CRenderTraditiaonl::ShaderApply - Called if there's a m_pImageShader. This
then loops through this shader and calculates shadows, applying them to the
main image, m_pImage.

inputs
   HANDLE      hEventSafe - As per other hEventSafe. This will ALWAYS be set
               if it's not NULL.

returns
   BOOL - TRUE if success. FALSE if pressed cancel
*/
BOOL CRenderTraditional::ShaderApply (HANDLE hEventSafe)
{
   DWORD y;
   CPoint pEye, pV;

   // figure out where the Eye is in world space
   if (m_dwCameraModel == CAMERAMODEL_FLAT) {
      DWORD dwWidth = Width();
      DWORD dwHeight = Height();

      PixelToWorldSpace ((fp)dwWidth/2 + .5, (fp)dwHeight/2 + .5,
         -1000, &pEye);
   }
   else {
      pV.Zero();
      pV.p[3] = 1;
      m_aRenderMatrix[0].TransformViewSpaceToWorldSpace (&pV, &pEye);   // don't bother with others
   }

   // will need a matrix to transform normals from image space to world space
   CMatrix mInv;
   m_aRenderMatrix[0].CTMGet (&mInv);  // don't bother with others
   // remove translations and offsets
   mInv.p[0][3] = mInv.p[1][3] = mInv.p[2][3] = 0;
   mInv.p[3][0] = mInv.p[3][1] = mInv.p[3][2] = 0;
   mInv.p[3][3] = 1;
   mInv.Transpose ();

   // store some variables that will be useful
   m_mShaderApplyInv.Copy (&mInv);
   m_pShaderApplyEye.Copy (&pEye);

#if 0
   // Find min and max so can show zbuf
   int   iZMin, iZMax;
   BOOL fFound;
   fFound = FALSE;
   for (y = 0; y < m_pFImage too... Height(); y++) for (x = 0; x < Width(); x++) {
      pis = m_pImageShader->Pixel(x,y);
      if (!pis->pMisc)
         continue;
      if (fFound) {
         iZMin = min(iZMin, pis->iZ);
         iZMax = max(iZMax, pis->iZ);
      }
      else {
         iZMin = iZMax = pis->iZ;
         fFound = TRUE;
      }
   }
#endif // 0

   // BUGFIX - Loop through the scanlines and make sure the textures are cached
   DWORD dwHeight = Height();
   DWORD dwWidth = Width();
   DWORD x, dwText;
   PISPIXEL pis;
#define LASTTEXTNUM     10       // to try and speed this up
   PCTextureMapSocket apTextLast[LASTTEXTNUM];
   memset (apTextLast, 0, sizeof(apTextLast));
   DWORD dwForceCache = 
      FORCECACHE_COLORONLY | FORCECACHE_GLOW | FORCECACHE_TRANS |
      ((m_dwShadowsFlags & SF_NOBUMP) ? 0 : FORCECACHE_BUMP) |
      ((m_dwShadowsFlags & SF_NOSPECULARITY) ? 0 : FORCECACHE_SPEC);
         // BUGFIX - Was usinge FORCECACHE_BUMP for a specularity! replaced with FORCECACHE_SPEC

   // BUGBUG - to see if fixes crash
   // BUGBUG - SHOULD be commented out. recommenting out to make sure all working
   // dwForceCache |= FORCECACHE_ALL;
   // BUGBUG


   for (y = 0; y < dwHeight; y++) {
      for (x = 0; x < dwWidth; x++) {
         for (pis = m_pImageShader->Pixel (x,y); pis; pis = (PISPIXEL)pis->pTrans) {
            // cache
            if (!pis->pMisc)
               continue;

            for (dwText = 0; dwText < LASTTEXTNUM; dwText++)
               if (pis->pMisc->pTexture == apTextLast[dwText])
                  break;
            if (dwText < LASTTEXTNUM)
               continue;   // found match

            // else, cache
            memmove (apTextLast + 1, apTextLast, sizeof(*apTextLast) * (LASTTEXTNUM-1));
            apTextLast[0] = pis->pMisc->pTexture;
            if (apTextLast[0])
               apTextLast[0]->ForceCache (dwForceCache);
         } // pis
      } // x
   } // y

   // safe to render on another thread now
   BOOL fLowerThreads = FALSE;
   if (hEventSafe) {
      SetEvent (hEventSafe);
      hEventSafe = NULL;

      // lower the thread priority
      fLowerThreads = TRUE;
   }

   DWORD i;
   // lower the thread priority so ping-poing approach to rendering 360 works better
   if (fLowerThreads && m_dwThreadsMax)
      for (i = 0; i < m_dwThreadsMax; i++)
         SetThreadPriority (m_aRENDTHREADINFO[i].hThread, VistaThreadPriorityHack(THREAD_PRIORITY_LOWEST, m_iPriorityIncrease));

   DWORD dwProgressSkip;
   // CMem memScratch;
   dwProgressSkip = 4;  // BUGFIX - So so many threads, reduced this to 4 from 8

   BOOL fCancelled = FALSE;

   for (y = 0; y < dwHeight; y += dwProgressSkip) {
      // update progress bar, every other time
      if (m_pProgress) {
         if ((y % (dwProgressSkip*2)) == 0)
            m_pProgress->Update ((fp)y / (fp) dwHeight);

         if (m_pProgress->WantToCancel()) {
            fCancelled = TRUE;
            break;
         }
      }

      // just do one bit at a time
      if (m_dwThreadsShadows)
         MultiThreadedShadowsScanline (y, min(y + dwProgressSkip, dwHeight));
      else
         ShaderApplyScanLines (y, min(y + dwProgressSkip, dwHeight), 0, &m_memShaderApplyScratch[0/*dwThread*/]);
   }  // over Y

   // wait for completion of thread rendering
   MultiThreadedWaitForComplete (FALSE);

   if (fLowerThreads && m_dwThreadsMax)
      for (i = 0; i < m_dwThreadsMax; i++)
         SetThreadPriority (m_aRENDTHREADINFO[i].hThread, VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL, m_iPriorityIncrease));  // to restore it

   // just in case
   if (hEventSafe) {
      SetEvent (hEventSafe);
      hEventSafe = NULL;
   }

   return !fCancelled;
}



/*******************************************************************************
CRenderTraditiaonl::ShaderApplyScanLines - Apply for a few scanlines

inputs
   DWORD       dwStart - Start y
   DWORD       dwEnd - End y
   DWORD       dwThread - Thread number
   PCMem       pMemScratch - Scratch memory
returns
   none
*/
void CRenderTraditional::ShaderApplyScanLines (DWORD dwStart, DWORD dwEnd, DWORD dwThread,
                                               PCMem pMemScratch)
{
	MALLOCOPT_INIT;
   DWORD x, y, i;
   PISPIXEL pCur;
   PIMAGEPIXEL pip;
   PFIMAGEPIXEL pfp;
   PISPIXEL pis;
   PISDMISC pm;
   PISPIXEL pLastDrawn, pLastVisible;
   DWORD dwWidth = Width();
   fp afColor[3];

   // take exposure into account
   fp fExposure;
   fExposure = 1.0 / max(m_fExposure, CLOSE);  // counteract by lumens in the sun 


   // fill in the SCANLINEOPT structure
   SCANLINEOPT SLO;
   // CListFixed lLightIndex, lLightSCANLINEINTERP;
   m_alShaderApplyScanLinesLightIndex[dwThread].Init (sizeof(DWORD));
   m_alShaderApplyScanLinesLightSCANLINEINTERP[dwThread].Init (sizeof(SCANLINEINTERP));
   SLO.plLightIndex = &m_alShaderApplyScanLinesLightIndex[dwThread];
   SLO.plLightSCANLINEINTERP = &m_alShaderApplyScanLinesLightSCANLINEINTERP[dwThread];

   //CMem aMemScanLineLoc[3];
   // CPoint pBaseLeft, pBaseRight, pBasePlusMeterLeft, pBasePlusMeterRight;
   // make sure there's enough memory for the locations
   //DWORD dwNeed = dwWidth * (m_pImageShader->m_dwMaxTransparent+1) * sizeof(CPoint);
   //for (i = 0; i < 3; i++) {
   //   if (!aMemScanLineLoc[i].Required (dwNeed))
   //      return;  // error

   //   SLO.apaScanLineLoc[i] = (PCPoint) aMemScanLineLoc[i].p;
   //}
   int iLine;
   for (i = 1; i < 3; i++) {
      iLine = (int)dwStart + (int) i - 2;

      PixelToWorldSpace (0, (fp)iLine, 0, &SLO.apBaseLeft[i]);
      PixelToWorldSpace (0, (fp)iLine, 1.0, &SLO.apBasePlusMeterLeft[i]);
      PixelToWorldSpace ((fp)dwWidth, (fp)iLine, 0, &SLO.apBaseRight[i]);
      PixelToWorldSpace ((fp)dwWidth, (fp)iLine, 1.0, &SLO.apBasePlusMeterRight[i]);
      SCANLINEINTERPCalc (FALSE, dwWidth, &SLO.apBaseLeft[i], &SLO.apBaseRight[i], &SLO.apBasePlusMeterLeft[i], &SLO.apBasePlusMeterRight[i], &SLO.aSLI[i]);

#if 0
      // precalc the depth info
      if ((iLine < 0) || (iLine >= (int)m_pImageShader->Height()))
         continue;   // since out of bounds
      pis = m_pImageShader->Pixel (0, (DWORD)iLine);
      for (x = 0; x < dwWidth; x++, pis++) {
         // calculate the location
         PCPoint paScanLineLocCur = SLO.apaScanLineLoc[i] + x;
         SCANLINEINTERPFromZ (&SLO.aSLI[i], x, pis->fZ, paScanLineLocCur);


         // do all the transparencies, know sorted from back to front, so 
         // just follow the order
         for (pCur = (PISPIXEL)pis->pTrans; pCur; pCur = (PISPIXEL)pCur->pTrans, paScanLineLocCur += dwWidth)
            SCANLINEINTERPFromZ (&SLO.aSLI[i], x, pCur->fZ, paScanLineLocCur);

         // advance current position in optiimzations
         SCANLINEINTERPAdvanceX (&SLO.aSLI[i]);
      } // x
#endif // 0
   } // i

   for (y = dwStart; y < dwEnd; y++) {
      // update optimizations

      // move up
      //PCPoint paTemp = SLO.apaScanLineLoc[0];
      for (i = 1; i < 3; i++) {
         SLO.aSLI[i-1] = SLO.aSLI[i];
         //SLO.apaScanLineLoc[i-1] = SLO.apaScanLineLoc[i];
         SLO.apBaseLeft[i-1].Copy (&SLO.apBaseLeft[i]);
         SLO.apBaseRight[i-1].Copy (&SLO.apBaseRight[i]);
         SLO.apBasePlusMeterLeft[i-1].Copy (&SLO.apBasePlusMeterLeft[i]);
         SLO.apBasePlusMeterRight[i-1].Copy (&SLO.apBasePlusMeterRight[i]);
      }
      //SLO.apaScanLineLoc[2] = paTemp;

      // calculate anew
      iLine = (int)y + 1;
      PixelToWorldSpace (0, (fp)iLine, 0, &SLO.apBaseLeft[2]);
      PixelToWorldSpace (0, (fp)iLine, 1.0, &SLO.apBasePlusMeterLeft[2]);
      PixelToWorldSpace ((fp)dwWidth, (fp)iLine, 0, &SLO.apBaseRight[2]);
      PixelToWorldSpace ((fp)dwWidth, (fp)iLine, 1.0, &SLO.apBasePlusMeterRight[2]);
      SCANLINEINTERPCalc (FALSE, dwWidth, &SLO.apBaseLeft[2], &SLO.apBaseRight[2], &SLO.apBasePlusMeterLeft[2], &SLO.apBasePlusMeterRight[2], &SLO.aSLI[2]);

      // calculate the lighting
      SLO.plLightIndex->Clear();
      SLO.plLightSCANLINEINTERP->Clear();
   	MALLOCOPT_OKTOMALLOC;
      SLO.plLightIndex->Required (m_lPCLight.Num());
	   MALLOCOPT_RESTORE;
      for (i = 0; i < m_lPCLight.Num(); i++) {
         PCLight pl = *((PCLight*) m_lPCLight.Get(i));
         DWORD dwNumLights = SLO.plLightSCANLINEINTERP->Num();
         pl->CalcSCANLINEINTERP (NULL, dwWidth, &SLO.apBaseLeft[1], &SLO.apBaseRight[1],
            &SLO.apBasePlusMeterLeft[1], &SLO.apBasePlusMeterRight[1], SLO.plLightSCANLINEINTERP);
         SLO.plLightIndex->Add (&dwNumLights);
      } // i

#if 0
      if (iLine < (int)m_pImageShader->Height()) {
         pis = m_pImageShader->Pixel (0, (DWORD)iLine);
         for (x = 0; x < dwWidth; x++, pis++) {
            // calculate the location
            PCPoint paScanLineLocCur = SLO.apaScanLineLoc[2] + x;
            SCANLINEINTERPFromZ (&SLO.aSLI[2], x, pis->fZ, paScanLineLocCur);


            // do all the transparencies, know sorted from back to front, so 
            // just follow the order
            for (pCur = (PISPIXEL)pis->pTrans; pCur; pCur = (PISPIXEL)pCur->pTrans, paScanLineLocCur += dwWidth)
               SCANLINEINTERPFromZ (&SLO.aSLI[2], x, pCur->fZ, paScanLineLocCur);

            // advance current position in optiimzations
            SCANLINEINTERPAdvanceX (&SLO.aSLI[2]);
         } // x
      } // if line
#endif // 0

      for (x = 0; x < dwWidth; x++) {
         if (m_pImage)
            pip = m_pImage->Pixel(x,y);
         else
            pfp = m_pFImage->Pixel (x,y);
         // NOTE: Intentionally ignoring m_pZImage

         pis = m_pImageShader->Pixel (x,y);

         if (m_pImage)
            for (i = 0; i < 3; i++)
               afColor[i] = (fp)((&pip->wRed)[i]) * CM_BESTEXPOSURE;
         else
            for (i = 0; i < 3; i++)
               afColor[i] = ((&pfp->fRed)[i]) * CM_BESTEXPOSURE;
         // NOTE: Intentionally ignoring m_pZImage

         if (pis->pMisc) {
            ShaderPixel (dwThread, pis, /* 0,*/ x, y, afColor, &m_mShaderApplyInv, &m_pShaderApplyEye, &SLO, pMemScratch, FALSE);
            pLastDrawn = pLastVisible = pis;
         }
         else
            pLastDrawn = pLastVisible = NULL;

         // do all the transparencies, know sorted from back to front, so 
         // just follow the order
         // DWORD dwTransDepth;
         for (pCur = (PISPIXEL)pis->pTrans /*, dwTransDepth = 1*/; pCur; pCur = (PISPIXEL)pLastDrawn->pTrans /*, dwTransDepth++*/) {
            pLastDrawn = pCur;
            if (ShaderPixel (dwThread, pLastDrawn, /* dwTransDepth, */ x, y, afColor, &m_mShaderApplyInv, &m_pShaderApplyEye, &SLO, pMemScratch, TRUE))
               pLastVisible = pLastDrawn; // BUGFIX - If totally transparent then dont outline
         }

         // write out
         for (i = 0; i < 3; i++) {
            afColor[i] *= fExposure;
            afColor[i] = max(0,afColor[i]);

            if (m_pImage) {
               afColor[i] = min((fp)0xffff,afColor[i]);
               (&pip->wRed)[i] = (WORD) afColor[i];
            }
            else
               (&pfp->fRed)[i] = afColor[i];
            // NOTE: Intentionally ignoring m_pZImage
         }

         // Need to get the miscindex for the last object shown
         if (pLastVisible && pLastVisible->pMisc) {
            pm = pLastVisible->pMisc;

            if (m_pImage) {
               pip->dwID = pm->dwID;
               pip->fZ = pLastVisible->fZ;
               pip->dwIDPart = pm->dwIDPart;
            }
            else {
               pfp->dwID = pm->dwID;
               pfp->fZ = pLastVisible->fZ;
               pfp->dwIDPart = pm->dwIDPart;
            }
            // NOTE: Intentionally ignoring m_pZImage
         }

         // update the current line
         SCANLINEINTERPAdvanceX (&SLO.aSLI[1]);
         PSCANLINEINTERP pSLI = (PSCANLINEINTERP) SLO.plLightSCANLINEINTERP->Get(0);
         for (i = 0; i < SLO.plLightSCANLINEINTERP->Num(); i++, pSLI++)
            SCANLINEINTERPAdvanceX (pSLI);
      }  // over X
   }  // over Y
}


/*******************************************************************************
CRenderTraditiaonl::ShaderForPainting - Goes through the internally calculated info
and figures fills in m_pRPPIXEL and m_plRPTEXTINFO so can paint onto the image.
*/
void CRenderTraditional::ShaderForPainting (void)
{
   DWORD x, y;
   CPoint pEye, pV;

   // figure out where the Eye is in world space
   if (m_dwCameraModel == CAMERAMODEL_FLAT) {
      DWORD dwHeight = Height();
      DWORD dwWidth = Width();

      PixelToWorldSpace ((fp)dwWidth/2 + .5, (fp)dwHeight/2 + .5,
         -1000, &pEye);
   }
   else {
      pV.Zero();
      pV.p[3] = 1;
      m_aRenderMatrix[0].TransformViewSpaceToWorldSpace (&pV, &pEye);   // don't bother with others
   }
   m_pRPView->Copy (&pEye);

   PRPPIXEL pip;
   pip = m_pRPPIXEL;

   // will need a matrix to transform normals from image space to world space
   CMatrix mInv;
   m_aRenderMatrix[0].CTMGet (&mInv); // don't bother with others
   // remove translations and offsets
   mInv.p[0][3] = mInv.p[1][3] = mInv.p[2][3] = 0;
   mInv.p[3][0] = mInv.p[3][1] = mInv.p[3][2] = 0;
   mInv.p[3][3] = 1;
   mInv.Transpose ();
   m_pRPMatrix->Copy (&mInv);

   PISPIXEL pis;
   PISPIXEL pMax, pTemp;
   PISPIXEL pCur;
   DWORD dwNextProgress, dwProgressSkip;
   DWORD dwHeight = Height();
   DWORD dwWidth = Width();
   dwProgressSkip = max(dwHeight / 32,1); 
   dwNextProgress = dwProgressSkip;
   for (y = 0; y < dwHeight; y++) {
      // update progress bar
      dwNextProgress--;
      if (!dwNextProgress && m_pProgress) {
         switch (m_pProgress->Update ((fp)y / (fp) dwHeight)) {
         case 1:
            dwProgressSkip /= 2;
            dwProgressSkip = max(1,dwProgressSkip);
            break;
         case -1:
            if (dwProgressSkip < 10000)
               dwProgressSkip *= 2;
            dwProgressSkip = min(dwProgressSkip, max(dwHeight / 32,1)); 
            //dwProgressSkip = min(dwProgressSkip, m_pWorld->ObjectNum() / 8);
            break;
         }
         dwNextProgress = dwProgressSkip;
      }

      for (x = 0; x < dwWidth; x++, pip++) {
         pis = m_pImageShader->Pixel (x,y);

         // find the furthest pixel that's less than already drawn
         pMax = NULL;
         for (pCur = (PISPIXEL)pis->pTrans; pCur; pCur = (PISPIXEL)pTemp->pTrans) {
            pTemp = pCur;
            if (!pMax || (pTemp->fZ < pMax->fZ))   // BUGFIX - Was >, but want to find closest
               pMax = pTemp;
         }
         if (!pMax)  // if no transparencies then just set to current
            pMax = pis;


         if (!pMax->pMisc) {
            // nothing at the pixel
            memset (pip, 0, sizeof(*pip));
            continue;
         }

         // location
         CPoint pWorld;
         PixelToWorldSpace (x, y, pMax->fZ, &pWorld);
         pip->afLoc[0] = pWorld.p[0];
         pip->afLoc[1] = pWorld.p[1];
         pip->afLoc[2] = pWorld.p[2];

         // texture
         pip->afHV[0] = pMax->tpText.hv[0];
         pip->afHV[1] = pMax->tpText.hv[1];
         // NOTE: Ignoreing XYZ, but that's OK since will only be painting on 2d surfaces

         // normal
         CPoint pN;
         pN.p[0] = pMax->aiNorm[0] / (fp)0x4000;
         pN.p[1] = pMax->aiNorm[1] / (fp)0x4000;
         pN.p[2] = pMax->aiNorm[2] / (fp)0x4000;
         pN.p[3] = 1;
         pN.Normalize();
         pN.MultiplyLeft (&mInv);
         pip->afNorm[0] = pN.p[0];
         pip->afNorm[1] = pN.p[1];
         pip->afNorm[2] = pN.p[2];

         // texture
         PISDMISC pm;
         RPTEXTINFO ti;
         memset (&ti, 0, sizeof(ti));
         pip->dwTextIndex = 0;
         pm = pMax->pMisc;
         if (!pm || !pm->pTexture)
            continue;
         pm->pTexture->GUIDsGet (&ti.gCode, &ti.gSub);
         if (!IsEqualGUID (ti.gCode, GTEXTURECODE_ImageFile))
            continue;   // can only edit image files

         // make sure there isn't a match already
         DWORD j;
         PRPTEXTINFO pti;
         pti = (PRPTEXTINFO) m_plRPTEXTINFO->Get(0);
         for (j = 0; j < m_plRPTEXTINFO->Num(); j++, pti++) {
            if (IsEqualGUID (ti.gSub, pti->gSub) && IsEqualGUID (ti.gCode, pti->gCode))
               break;
         }
         if (j < m_plRPTEXTINFO->Num())
            pip->dwTextIndex = j+1;
         else {
            m_plRPTEXTINFO->Add (&ti);
            pip->dwTextIndex = m_plRPTEXTINFO->Num(); // specifically doing this after add
         }

      }  // over X
   }  // over Y

}

/*******************************************************************************
ClosestTexture - Given a PISPIXEL for a pixel, this will look at a neighboring
pixel and return TRUE if tthe neighboring pixel is in the same object, filling
in a TEXTPOINT5 with the neighbor's texture. If there is transparency it also
handles this by finding the closest transparent.

Use this to calculate the texture delta.

inpugs
   PISPIXEL       pis - One in the center
   PCImageShader  pShader - Shader to use
   DWORD          dwX, dwY - X and Y to get from the shader
   PTEXTPOINT5    pTexture - Filled in with the texture (if success)
   DWORD          dwTextInfo - Flags from DHLTEXT_XXX
retursn
   BOOL - TRUE if success, FALSE if failure
*/
BOOL ClosestTexture (const PISPIXEL pis, const PCImageShader pShader, DWORD dwX, DWORD dwY,
                     PTEXTPOINT5 pTexture, DWORD dwTextInfo)
{
   PISPIXEL pTest = pShader->Pixel (dwX, dwY);

   PISDMISC pThisMisc, pTestMisc;
   PTEXTPOINT5 ptpBest = NULL;
   fp fBestDist, fTestDist;
   while (pTest) {
      if (pTest->pMisc != pis->pMisc) {
         if (!pTest->pMisc)
            goto skip;
         pThisMisc = pis->pMisc;
         pTestMisc = pTest->pMisc;
         if ((pThisMisc->dwID != pTestMisc->dwID) || (pThisMisc->dwIDPart != pTestMisc->dwIDPart) ||
            (pThisMisc->pTexture != pTestMisc->pTexture)) // BUGFIX - Add test for matching texture
            goto skip;
      }

      // BUGFIX - Best measure of which one to choose is based on z
      fTestDist = fabs(pis->fZ - pTest->fZ);
#if 0
      fTestDist = 0;

      // if HV distance then use that
      if (dwTextInfo & DHLTEXT_HV)
         fTestDist += fabs(pTest->tpText.hv[0] - pis->tpText.hv[0]) +
            fabs(pTest->tpText.hv[1] - pis->tpText.hv[1]);
      // if XYZ distance then use that
      if (dwTextInfo & DHLTEXT_XYZ)
         fTestDist += fabs(pTest->tpText.xyz[0] - pis->tpText.xyz[0]) +
            fabs(pTest->tpText.xyz[1] - pis->tpText.xyz[1]) +
            fabs(pTest->tpText.xyz[2] - pis->tpText.xyz[2]);
#endif

      if (!ptpBest || (fTestDist < fBestDist)) {
         fBestDist = fTestDist;
         ptpBest = &pTest->tpText;
      }


skip:
      if (!pTest->pTrans)
         break;   // no more to test
      pTest = (PISPIXEL) pTest->pTrans;
   }

   if (ptpBest) {
      *pTexture = *ptpBest;
      return TRUE;
   }
   else
      return FALSE;
}


/*******************************************************************************
CRenderTraditional::ShaderPixel - Given a pixel, this calculates the color of it.
NOTE: It DOES NOT look at the ISPIXEL.pTrans setting. That's meant for the caller to do.

inputs
   DWORD    dwThread - Thread that in
   PISPIXEL pis - Pixel to draw (might be a transparent one too)
   // DWORD    dwTransDepth - Transparency depth. 0 = main opaque. 1+ = transparencies
   DWORD    dwX, dwY - XY pixel location
   fp       *pafColor - Should initially be filled with the under-color (because
               may be drawing a transparent pixel). Will be replaced with the new
               color, which may be a combination of under-color and new color.
   PCMatrix pmInv - Matrix to convert normals to world space
   PCPoint  pEye - Location of the eye in world space
   PSCANLINEOPT pSLO - Optimizations for scanline 
   PCMem    pMemTemp - Scratch memory
   BOOL     fCanBeTransparent - Set to TRUE if this is allowed to be transparent. FALSE
            if it isn't, so transparency is squashed if it arises. Put this in
            since occasionally edges of leaves are thought to be opaque, and end up
            overwriting the skydome, which causes a transparency over plain background
returns
   BOOl - TRUE if success. FALSE if pixel is entirely transparent.
*/
BOOL CRenderTraditional::ShaderPixel (DWORD dwThread, const PISPIXEL pis, /* DWORD dwTransDepth,*/ DWORD x, DWORD y, fp *pafColor, const PCMatrix pmInv, const PCPoint pEye,
                                      PSCANLINEOPT pSLO, PCMem pMemScratch, BOOL fCanBeTransparent)
{
   PISDMISC pm;
   CPoint pWorld;
   DWORD i, j;
   CMaterial   Mat;
   BOOL fRet = TRUE;
   float afGlow[3];
   BOOL fIsGlow = FALSE;
   memset (afGlow, 0, sizeof(afGlow));

   // if there isnt any object here then ignore
   if (!pis->pMisc)
      return fRet;


   // get the information
   pm = pis->pMisc;
   DWORD dw = pm->pTexture ? pm->pTexture->DimensionalityQuery(dwThread) : 0;
   DWORD dwTextInfo = 0;
   if (dw & 0x01)
      dwTextInfo |= DHLTEXT_HV;
   if (dw & 0x02)
      dwTextInfo |= DHLTEXT_XYZ;

   // convert this to world space
   // BUGFIX - Optimization
   SCANLINEINTERPFromZ (&pSLO->aSLI[1], pis->fZ, &pWorld);
   // pWorld.Copy (pSLO->apaScanLineLoc[1] + (x + dwTransDepth * m_pImageShader->Height()) );
   // SCANLINEINTERPFromXAndZ (&pSLO->aSLI[1], x, pis->fZ, &pWorld);
   // PixelToWorldSpace (x, y, pis->fZ, &pWorld);

   BOOL fUseBump = pm->pTexture && !(m_dwShadowsFlags & SF_NOBUMP) && pm->pTexture->QueryBump(dwThread);

   // BUGFIX - Dont bother calculating if tpRight and tpBelow ignored
   TEXTPOINT5 tpRight, tpBelow;
   if (dw && pm->pTexture && (pm->pTexture->QueryTextureBlurring(dwThread) || fUseBump) ) {

      // find the texture deltas
      DWORD dwLeft, dwRight, dwAbove, dwBelow;
      TEXTPOINT5 tpLeft, tpAbove;
      if (x && ClosestTexture (pis, m_pImageShader, x-1, y, &tpLeft, dwTextInfo))
         dwLeft = x-1;
      else {
         dwLeft = x;
         tpLeft = pis->tpText;
      }
      if ((x+1 < m_pImageShader->Width()) && ClosestTexture (pis, m_pImageShader, x+1, y, &tpRight, dwTextInfo))
         dwRight = x+1;
      else {
         dwRight = x;
         tpRight = pis->tpText;
      }
      if (y && ClosestTexture (pis, m_pImageShader, x, y-1, &tpAbove, dwTextInfo))
         dwAbove = y-1;
      else {
         dwAbove = y;
         tpAbove = pis->tpText;
      }
      if ((y+1 < m_pImageShader->Height()) && ClosestTexture (pis, m_pImageShader, x, y+1, &tpBelow, dwTextInfo))
         dwBelow = y+1;
      else {
         dwBelow = y;
         tpBelow = pis->tpText;
      }
      if (dwTextInfo & DHLTEXT_HV)
         for (j = 0; j < 2; j++) {
            tpRight.hv[j] -= tpLeft.hv[j];
            tpBelow.hv[j] -= tpAbove.hv[j];
         }
      if (dwTextInfo & DHLTEXT_XYZ)
         for (j = 0; j < 3; j++) {
            tpRight.xyz[j] -= tpLeft.xyz[j];
            tpBelow.xyz[j] -= tpAbove.xyz[j];
         }

      if (dwRight == dwLeft+2) { // managed to get pixels to left and right
         if (dwTextInfo & DHLTEXT_HV)
            for (j = 0; j < 2; j++)
               tpRight.hv[j] /= 2.0;
         if (dwTextInfo & DHLTEXT_XYZ)
            for (j = 0; j < 3; j++)
               tpRight.xyz[j] /= 2.0;
      }
      else if (dwRight == dwLeft) {
         if (dwTextInfo & DHLTEXT_HV)
            tpRight.hv[0] = tpRight.hv[1] = 0; // 0.1;  // so completely antialias
            // BUGFIX - Was defaulting to 1, but that's too much, so not as much
            // Using 1.0 causes spots to appear at the end of leaves

         if (dwTextInfo & DHLTEXT_XYZ)
            memset (&tpRight.xyz, 0, sizeof(tpRight.xyz));
      }

      if (dwBelow == dwAbove+2) { // managed to get pixels to top and bottom
         if (dwTextInfo & DHLTEXT_HV)
            for (j = 0; j < 2; j++)
               tpBelow.hv[j] /= 2.0;
         if (dwTextInfo & DHLTEXT_XYZ)
            for (j = 0; j < 3; j++)
               tpBelow.xyz[j] /= 2.0;
      }
      else if (dwBelow == dwAbove) {
         if (dwTextInfo & DHLTEXT_HV)
            tpBelow.hv[0] = tpBelow.hv[1] = 0; // 0.1; // so completely antialises
               // BUGFIX - As above
         if (dwTextInfo & DHLTEXT_XYZ)
            memset (&tpBelow.xyz, 0, sizeof(tpBelow.xyz));
      }
   } // if need to calcualte tpRight and tpBelow
   else {
      memset (&tpRight, 0, sizeof(tpRight));
      memset (&tpBelow, 0, sizeof(tpBelow));
   }



   // color using?
   WORD awBase[3];
   memcpy (&Mat, &pm->Material, sizeof(Mat));
   if (pm->pTexture) {
      TEXTPOINT5 tp;
      // BUGFIX - Had added 1/2 for the texture size to get sharper trasparencies,
      // but also caused aliasing in textures. Therefore, removed the 1/2
      // BUGFIX - Put 1/2 back in because leaved looked bad without it
      if (dwTextInfo & DHLTEXT_HV)
         for (j = 0; j < 2; j++)
            tp.hv[j] = max(fabs(tpRight.hv[j]), fabs(tpBelow.hv[j])) / 2.0;
      if (dwTextInfo & DHLTEXT_XYZ)
         for (j = 0; j < 3; j++)
            tp.xyz[j] = max(fabs(tpRight.xyz[j]), fabs(tpBelow.xyz[j])) / 2.0;

      // BUGFIX - stop crash
      DWORD dwFillAsk = TMFP_ALL;
      if (m_dwShadowsFlags & SF_NOSPECULARITY)
         dwFillAsk &= ~TMFP_SPECULARITY;
      pm->pTexture->FillPixel (dwThread, dwFillAsk, awBase, &pis->tpText, &tp, &Mat, afGlow, FALSE);
         // NOTE: Passing in fHighQuality = FALSE into renderer
      // NOTE that material may be changed based on texture map

      // exposure
      // how much to scale glow
      //if (m_fExposure)
      //   fGlowScale = /*CM_LUMENSSUN / m_fExposure * */ CM_BESTEXPOSURE;
      //else
      //   fGlowScale = 1;
#define GLOWSCALE (CM_BESTEXPOSURE * (CM_LUMENSSUN / CM_BESTEXPOSURE))
      if (afGlow[0] || afGlow[1] || afGlow[2]) {
         fIsGlow = TRUE;
         afGlow[0] *= GLOWSCALE;
         afGlow[1] *= GLOWSCALE;
         afGlow[2] *= GLOWSCALE;
      }
   }
   else
      // default color
      memcpy (awBase, &pm->wRed, sizeof(awBase));

   // some tests
   BOOL fIsBlack = !awBase[0] && !awBase[1] && !awBase[2];
   BOOL fIsSpecular = Mat.m_wSpecReflect ? TRUE : FALSE;
   BOOL fIsMatteBlack = fIsBlack && !fIsSpecular;

   // can skip ahead if it's matte black and not transpanrent
   if (fIsMatteBlack && !(Mat.m_wTransparency && fCanBeTransparent))
      goto skipbump; // since dont need any of the following

   // calulcate the normal
   // BUGFIX - Not saved normalized. so normalize here
   CPoint pN, pNTemp;
   pNTemp.p[0] = pis->aiNorm[0];  // dont need to do because normalize: / (fp)0x4000
   pNTemp.p[1] = pis->aiNorm[1];  // dont need to do because normalize: / (fp)0x4000
   pNTemp.p[2] = pis->aiNorm[2];  // dont need to do because normalize: / (fp)0x4000
   pNTemp.Normalize();
   // BUGFIX - Because only need 3x3 part, optimize
   // pN.MultiplyLeft (pmInv);
   #define  MMP(i)   pN.p[i] = pmInv->p[0][i] * pNTemp.p[0] + pmInv->p[1][i] * pNTemp.p[1] + pmInv->p[2][i] * pNTemp.p[2] // + p[3][i] * pNTemp->p[3]

      MMP(0);
      MMP(1);
      MMP(2);
      //MMP(3);
   #undef MMP
   pN.p[3] = 1;

   // normal modified by texture
   CPoint pNText;
   pNText.Copy (&pN);

   // modify the normal based on the slop
   TEXTUREPOINT tSlope;
   if (fUseBump && pm->pTexture->PixelBump (dwThread, &pis->tpText, &tpRight, &tpBelow, &tSlope)) {
         // NOTE: Passing fHighQuality = FALSE into PixelBump
      // find the size of the pixel...
      fp fSize;
      CPoint pRight, pUp;
      // BUGFIX - Optimization
      SCANLINEINTERPFromXAndZ (&pSLO->aSLI[1], x+1, pis->fZ, &pRight);
      //PixelToWorldSpace (x+1, y, pis->fZ, &pRight);
      pRight.Subtract (&pWorld);
      fSize = pRight.Length();
      // assuming same size in both directions

      // BUGFIX - if change is much smaller than a pixel then ignore
      if (max(fabs(tSlope.h), fabs(tSlope.v)) < fSize / 100.0) // 1/100th of a pixel
         goto skipbump;

      // find points to the right and left
      // BUGFIX - Dont normalize pRight here ... pRight.Scale (1.0 / fSize);   // BUGFIX - Was normalize
      pUp.CrossProd (&pN, &pRight);
      // BUGFIX - if pN more or less same as pRight, was a bug where would get pUp wrong
      if (pUp.Length() < .1 * fSize) {
         // the normal faces to the right/left
         // BUGFIX - Optimization
         SCANLINEINTERPFromXAndZ (&pSLO->aSLI[0], x, pis->fZ, &pUp);
         //PixelToWorldSpace (x, (fp)y-1, pis->fZ, &pUp);
         pUp.Subtract (&pWorld);
         // pUp.Normalize();
         pRight.CrossProd (&pUp, &pN);
         //pRight.Normalize();
         pRight.Scale (fSize / pRight.Length());   // so same as original length
         pUp.CrossProd (&pN, &pRight);
         // pUp.Normalize();
         pUp.Scale (fSize / pUp.Length());
      }
      else {
         // pUp.Normalize();
         pUp.Scale (fSize / pUp.Length());
         pRight.CrossProd (&pUp, &pN);
         //pRight.Normalize();
         pRight.Scale (fSize / pRight.Length());   // so same as original length
      }

      // not calculalte  hold the LL corner of the pixel at 0,0,0 ans find LR and UL.
      // BUGFIX - Net result it pLR = pRight, pUL = pUp, so just use those
      //CPoint pLR, pUL;
      //pLR.Copy (&pRight);
      // BUGFIX - Dont need to scale because pRight was kept to scale ... pLR.Scale (fSize);
      //pUL.Copy (&pUp);
      // BUGFIX - Dont need to scale because pUp kept to scale pUL.Scale (fSize);

      // add the delta in elevations
      CPoint pDelta;
      pDelta.Copy (&pN);
      pDelta.Scale (tSlope.h);
      pRight.Add (&pDelta);
      pDelta.Copy (&pN);
      pDelta.Scale (-tSlope.v);
      pUp.Add (&pDelta);

      // the normal is the corss product of these two
      pNText.CrossProd (&pRight, &pUp);
      pNText.Normalize();
   }
skipbump:

   if (Mat.m_wTransparency && fCanBeTransparent) {
      // BUGFIX - If totally transparent and no diffuse then it's invisible, so don't
      // outline
      if ((Mat.m_wTransparency >= 0xfffe) && (Mat.m_wSpecReflect ==0))
         fRet = FALSE;

      // if it's transparent first get the color of the object excluding specularity
      fp    afDiffuse[3], afSpecular[3];
      ShaderLighting (&Mat, &pWorld, &pN, &pNText, pEye, awBase,
         pSLO, pis->fZ, pMemScratch, afDiffuse, afSpecular);

      // set the color
      fp fTrans;
      fp fDivideByFFFF = 1.0 / (fp)0xffff;
      fTrans = (fp)Mat.m_wTransparency * fDivideByFFFF;

      // BUGFIX - Transparency if N x V - although some modiciation so it doesn't really
      // happen for glass much
      CPoint pVNormalized;
      fp fNDotEye, fTransAngle;
      pVNormalized.Subtract (pEye, &pWorld);
      pVNormalized.Normalize();
      fNDotEye = fabs(pN.DotProd (&pVNormalized));
      fTransAngle = (fp) Mat.m_wTransAngle * fDivideByFFFF;
      fTrans *= (fNDotEye + (1.0 - fNDotEye) * (1.0 - fTransAngle));
      fp fOneMinusTrans = 1.0 - fTrans;


      for (i = 0; i < 3; i++)
         pafColor[i] = pafColor[i] * fTrans + fOneMinusTrans * afDiffuse[i] + afSpecular[i];
   }
   else if (fIsMatteBlack)
      // opaque, black, and no specularity... so total black. optimization for skydome
      pafColor[0] = pafColor[1] = pafColor[2] = 0;
   else { // opaque
      fp afSpecular[3];

      ShaderLighting (&Mat, &pWorld, &pN, &pNText, pEye, awBase,
         pSLO, pis->fZ, pMemScratch, pafColor, afSpecular);
      for (i = 0; i < 3; i++)
         pafColor[i] += afSpecular[i];
   }

   // add in glow
   if (fIsGlow) {
      pafColor[0] += afGlow[0];
      pafColor[1] += afGlow[1];
      pafColor[2] += afGlow[2];
   }

   return fRet;
}

// BUGFIX - Disabled since must be fast #pragma optimize ("", off)
// BUGFIX - Turn optimization off dont red channel doesnt get nixed when optimize release
// Takeing this out slows a image from 22.5 to 22.9 second

/*******************************************************************************
CRenderTraditional::ShaderLighting - Calculate the lighting for a pixel.

inputs
   PCMaterial   pm - Surface material qualities
   PCPoint     pWorld - Location in world space
   PCPoint     pN - Normal - before texture bump map applied
   PCPoint     pNText - Normal, after texture bump map applied
   PCPoint     pEye - location of the eye in world space
   WORD        *pawColor - Surface color
   PSCANLINEOPT pSLO - Optimizations for scanline 
   fp          fZScanLine - Z value for the scanline, used for optimizations
   PCMem       pMemScratch - Scratch memory for storing stuff
   fp          *pafDiffuse - Filled in with the DIFFUSE value to use. This can be NULL.
   fp          *pafSpecular - Filled with the SPECULAR value to use. This can be NULL
returns
   none
*/
void CRenderTraditional::ShaderLighting (const PCMaterial pm, const PCPoint pWorld, const PCPoint pN, const PCPoint pNText, const PCPoint pEye,
                                         const WORD *pawColor, PSCANLINEOPT pSLO, fp fZScanLine, PCMem pMemScratch,
                                         fp *pafDiffuse, fp *pafSpecular)
{
   // convert the original color to floats
   DWORD i;
   fp afOrigColor[3];
   for (i = 0; i < 3; i++)
      afOrigColor[i] = pawColor[i];

   BOOL fIsBlack = !pawColor[0] && !pawColor[1] && !pawColor[2];
   BOOL fIsSpecular = pm->m_wSpecReflect ? TRUE : FALSE;

   // abmient light and clear to 0
   if (pafDiffuse) {
      if (fIsBlack)
         pafDiffuse[0] = pafDiffuse[1] = pafDiffuse[2] = 0;
      else {
         fp fIntensityAmbient = (fp)m_dwIntensityAmbient2 / (fp) 0x10000 * CM_LUMENSSUN;
         fp fIntensityAmbientExtra = (fp)m_dwIntensityAmbientExtra / (fp) 0x10000 * CM_LUMENSSUN;
         for (i = 0; i < 3; i++)
            pafDiffuse[i] = afOrigColor[i] *
               (fIntensityAmbient * m_pLightAmbientColor.p[i] + fIntensityAmbientExtra);
            //if (pm->m_fSelfIllum && (dwModels & 0x02))
            //   fLight = CM_LUMENSSUN;
      }
   } // if diffuse
   if (pafSpecular)
      pafSpecular[0] = pafSpecular[1] = pafSpecular[2] = 0;

   // if it's matt black then exit here
   BOOL fIsMatteBlack = fIsBlack && !fIsSpecular;
   if (fIsMatteBlack)
      return;

   // vector from pWorld to eye
   CPoint pEyeVect;
   if (m_dwCameraModel == CAMERAMODEL_FLAT)
      pEyeVect.Copy (pEye);
   else
      pEyeVect.Subtract (pEye, pWorld);
   pEyeVect.Normalize();

   // if looking not looking at the side the normal is on, then swap over the normals
   fp fFlip;
   fFlip = pEyeVect.DotProd (pN);
   CPoint pNFlipText;
   pNFlipText.Copy (pNText);
   if (fFlip < 0)
      pNFlipText.Scale (-1);  // yes, looking from back side

   // loop over all the lights and get their intesities
   DWORD dwNumLight = m_lPCLight.Num();
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   if (!pMemScratch->Required (dwNumLight * sizeof(fp) * 5)) {
   	MALLOCOPT_RESTORE;
      return;  // error
   }
	MALLOCOPT_RESTORE;
   fp *pafScratch = (fp*)pMemScratch->p;
   fp *pafIntensity;
   DWORD dwLightsFound = 0;
   fp fMinStrength = 0;
   fp fMaxStrength = 0;
   DWORD dwLight;
   for (dwLight = 0, pafIntensity = pafScratch; dwLight < dwNumLight; dwLight++) {
      PCLight pl = *((PCLight*) m_lPCLight.Get(dwLight));

      // find out the light intensity
      // BUGFIX - Pass fMinStrength in and quickly eliminate lights that too dim
      DWORD dwLightIndex = *((DWORD*)pSLO->plLightIndex->Get(dwLight));
      pl->CalcIntensity (NULL, pWorld, (PSCANLINEINTERP) pSLO->plLightSCANLINEINTERP->Get(dwLightIndex), fZScanLine, pafIntensity, fMinStrength);

      pafIntensity[3] = max(max(pafIntensity[0], pafIntensity[1]), pafIntensity[2]);

      if (pafIntensity[3] <= fMinStrength) {
#if 0
         // HACK - To emphasize where shadows are
         pafPixel[0] = 0xffff;
         pafPixel[1] = pafPixel[2] = 0;
#endif
         continue;   // blackness, or too dim
      }

      // if stronger than strongest then new minimum intensity
      if (pafIntensity[3] > fMaxStrength) {
         fMaxStrength = pafIntensity[3];
         fMinStrength = fMaxStrength / 1024.0;  // if light is 1/1000th of strongest ignore
      }

      // keep this
      pafIntensity[4] = dwLight; // so remember light index
      pafIntensity += 5;
      dwLightsFound++;
   } // dwLight

   // precalc some stuff
   fp fLight, fDivideFFFF, fSpecReflectScale, fMaxBright, fMaxBrightInv;
   double fSpecExponent, fSpecPlasticPower;
   BOOL fPreCalcDone = FALSE;


   // loop over all the lights
   for (dwLight = 0, pafIntensity = pafScratch; dwLight < dwLightsFound; dwLight++, pafIntensity += 5) {
      // double check... if this is too dim then ignore
      if (pafIntensity[3] < fMinStrength)
         continue;

      PCLight pl = *((PCLight*) m_lPCLight.Get(pafIntensity[4]));

      LIGHTINFO li;
      PCPoint pLightVect;  // normalized vector from pointing from pWorld to the light
      pl->LightInfoGet (&li, NULL);
      if (li.dwForm == LIFORM_INFINITE)
         li.pDir.Scale(-1);
      else if (li.dwForm == LIFORM_POINT)
         li.pDir.Subtract (&li.pLoc, pWorld);
      else
         continue;   // not known
      li.pDir.Normalize();
      pLightVect = &li.pDir;

      // precalculuate some values
      if (!fPreCalcDone) {
         fPreCalcDone = TRUE;

         fDivideFFFF = (fp)1.0 / (fp)0xffff;
         fSpecExponent = (double)pm->m_wSpecExponent / 100.0;
         fSpecReflectScale = (fp) pm->m_wSpecReflect * fDivideFFFF;
         if (pm->m_wSpecReflect) {
            if (pm->m_wSpecPlastic > 0x8000)
               fSpecPlasticPower = 1.0 + (fp)(pm->m_wSpecPlastic - 0x8000) / (fp)0x1000;
            else
               fSpecPlasticPower = 1.0 / (1.0 + (fp)(0x8000 - pm->m_wSpecPlastic) / (fp)0x1000);
         }
         else
            fSpecPlasticPower = 1;  // leave default
         fMaxBright = (fp)max(max(afOrigColor[0], afOrigColor[1]),afOrigColor[2]) * fDivideFFFF;
         fMaxBrightInv = (fMaxBright > EPSILON) ? (1.0 / fMaxBright) : 1;
      }

      // if light is behind user then get rid of
      fp fNDotL;
      fNDotL = pLightVect->DotProd (&pNFlipText);
      if (pm->m_wTranslucent && (fNDotL < 0)) {
         fNDotL *= -1;  // so positive

         // make darker
         fNDotL *= (fp)pm->m_wTranslucent * fDivideFFFF;
      }
      if ((fNDotL > 0) /*&& !pm->m_fSelfIllum*/ && pafDiffuse) {
         // diffuse component
         // no need for this: fLight = fNDotL;

         // wrap intensity into
         for (i = 0; i < 3; i++)
            pafDiffuse[i] += pafIntensity[i] * afOrigColor[i] * fNDotL;
      }

      // specular component
      if (!pm->m_wSpecReflect || !pafSpecular || (m_dwShadowsFlags & SF_NOSPECULARITY))
         continue;   // none

      CPoint pH;
      pH.Add (&pEyeVect, pLightVect);
      pH.Normalize();
      fp fNDotH, fVDotH;
      fNDotH = pNFlipText.DotProd (&pH);
      //if (pm->m_fTranslucent) - BUGFIX - take out so no specular on tranclucent side
      //   fNDotH = fabs(fNDotH);
      if ((fNDotH > 0) /* no need to call && pm->m_wSpecReflect && pafSpecular*/) {
         fVDotH = pEyeVect.DotProd (&pH);
         fVDotH = max(0,fVDotH);
         fNDotH = pow ((fp)fNDotH, (fp)fSpecExponent);
         fLight = fNDotH * fSpecReflectScale;


         fp fPureLight, fMixed;
         //if (pm->m_wSpecPlastic > 0x8000)
         //   fVDotH = pow(fVDotH, 1.0 + (fp)(pm->m_wSpecPlastic - 0x8000) / (fp)0x1000);
         //else if (pm->m_wSpecPlastic < 0x8000)
         //   fVDotH = pow(fVDotH, 1.0 / (1.0 + (fp)(0x8000 - pm->m_wSpecPlastic) / (fp)0x1000));
         fVDotH = pow((fp)fVDotH, (fp)fSpecPlasticPower);
         fPureLight = fLight * (1.0 - fVDotH);
         fMixed = fLight * fVDotH * fMaxBrightInv;

         // BUGFIX - Including of fMaxBrightInv above replaces the following code
         // BUGFIX - For the mixing component, offset by the maximum brightness component of
         // the object so the specularily is relatively as bright.
         // DEAD CODE fMax = (pawColor[0] + pawColor[1] + pawColor[2]) / (fp) 0xffff;   // NOTE: Secifically not using /3.0
         // fMax = (fp)max(max(afOrigColor[0], afOrigColor[1]),afOrigColor[2]) / (fp) 0xffff;
         //if (fMax > EPSILON)
         //   fMixed /= fMax;

         // wrap intensity and add into specular component
         fPureLight *= (fp)0xffff;
         for (i = 0; i < 3; i++)
            pafSpecular[i] += pafIntensity[i] * (fPureLight + fMixed * afOrigColor[i]);
      } // if specular light

   } // dwLight

}
// BUGFIX - disabled since must be fast #pragma optimize ("", on)


/*******************************************************************************
CRenderTraditional::LightsDirty - Set all the lights dirty.
*/
void CRenderTraditional::LightsDirty (void)
{
   DWORD i;
   for (i = 0; i < m_lPCLight.Num(); i++) {
      PCLight pl = *((PCLight*)m_lPCLight.Get(i));
      pl->DirtySet ();
   }
}


/*******************************************************************************
CRenderTraditional::LightVectorSetFromSun - Used for the quick rendering (non-shadows). This
looks through all the objects and finds out which one is producing the strongest
infinite light. It also finds the ambient lights. The ambient lights are summed.
*/
void CRenderTraditional::LightVectorSetFromSun (void)
{
   LIGHTINFO   lBest;
   BOOL fFound = FALSE;
   memset (&lBest, 0, sizeof(lBest));
   fp afAmbient[3];
   afAmbient[0] = afAmbient[1] = afAmbient[2] = 0;

   // loop through all the objects in the world asking them for lights
   DWORD dwNum, i, j;
   PLIGHTINFO pli;
   CListFixed lLight;
   lLight.Init (sizeof(LIGHTINFO));
   dwNum = m_pWorld->ObjectNum();
   for (i = 0; i < dwNum; i++) {
      lLight.Clear();
      PCObjectSocket pos = m_pWorld->ObjectGet (i);
      if (!pos)
         continue;
      pos->LightQuery (&lLight, m_dwRenderShow);
      if (!lLight.Num())
         continue;   // wasn't added


      // else was added, see if want
      pli = (PLIGHTINFO)lLight.Get(0);
      for (j = 0; j < lLight.Num(); j++, pli++) {
         if (pli->dwForm == LIFORM_AMBIENT) {
            // remember the ambient light contribution
            afAmbient[0] += (fp) pli->awColor[2][0] * pli->afLumens[2];
            afAmbient[1] += (fp) pli->awColor[2][1] * pli->afLumens[2];
            afAmbient[2] += (fp) pli->awColor[2][2] * pli->afLumens[2];
            continue;
         }

         // else, only want infinite
         if (pli->dwForm != LIFORM_INFINITE)
            continue;

         // if not as strong as existing then continue
         if (fFound && (pli->afLumens[2] <= lBest.afLumens[2]))
            continue;

         // else, keep this


         // get the matrix
         CMatrix m, mInvTrans;
         pos->ObjectMatrixGet (&m);
         m.Invert (&mInvTrans);  // only need 3x3 invert since for normals
         mInvTrans.Transpose();
         pli->pDir.p[3] = 1;
         pli->pDir.MultiplyLeft (&mInvTrans);
         //pli->pLoc.p[3] = 1;
         //pli->pLoc.MultiplyLeft (&m);

         lBest = *pli;
         fFound = TRUE;
      } // j

   } // i


   // if didn't find any light then make one up...
   if (!fFound) {
      lBest.pDir.p[0] = 1;
      lBest.pDir.p[1] = 1;
      lBest.pDir.p[2] = -1;
      lBest.pDir.Normalize();
      lBest.afLumens[2] = CM_LUMENSSUN * 2.0 / 3.0;

      afAmbient[0] = afAmbient[1] = afAmbient[2] = CM_LUMENSSUN / 3 * 0x10000;
   }

   // ambient
   m_dwIntensityAmbient2 = (DWORD) ((afAmbient[0] + afAmbient[1] + afAmbient[2]) / 3 / CM_LUMENSSUN);

   // light intensity
   m_dwIntensityLight = (DWORD) (lBest.afLumens[2] / CM_LUMENSSUN * 0x10000);

   // light
   lBest.pDir.Scale (-1);
   LightVectorSet (&lBest.pDir);
}

/*******************************************************************************
CRenderTraditional::LightsAddRemove - Add/remove lights as necessary according to
what's in the world.
*/
void CRenderTraditional::LightsAddRemove (void)
{
	MALLOCOPT_INIT;
   // if not in shadow mode then delete existing lights and be done with it
   DWORD i, j;
   PCLight pl;
   if ((m_dwRenderModel & 0x0f) != RM_SHADOWS) {
      for (i = 0; i < m_lPCLight.Num(); i++) {
         pl = *((PCLight*)m_lPCLight.Get(i));
         delete pl;
      }
      m_lPCLight.Clear();
      return;
   }

   // clear the existing list
   m_lLIGHTINFO.Clear();

   // NOTE: No longer creating sun and moon in here
   LIGHTINFO li;
   PLIGHTINFO pli;
#if 0
   // create the sun
   if (m_dwIntensityLight) {
      memset (&li, 0, sizeof(li));
      for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
         li.awColor[i][j] = (WORD)(0xffff * m_pLightSunColor.p[j]);
      li.afLumens[2] = (fp)m_dwIntensityLight / (fp) 0x10000 * CM_LUMENSSUN;
      li.dwForm = LIFORM_INFINITE;
      li.pDir.Copy (&m_lightWorld);
      li.pDir.Scale(-1);

      // if no shadows mode then turn of shadows
      if (m_dwShadowsFlags & SF_NOSHADOWS)
         li.fNoShadows = TRUE;

      m_lLIGHTINFO.Add (&li);
   }

   // create a moon light?
   if ((m_pMoonVector.p[2] > 0) && (m_lightWorld.p[2] < .1) && (m_dwClouds < 2)) {
      memset (&li, 0, sizeof(li));
      for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
         li.awColor[i][j] = (WORD)(0xffff * m_pLightMoonColor.p[j]);
      li.afLumens[2] = CM_LUMENSMOON * sin(m_fMoonPhase * PI);
      li.dwForm = LIFORM_INFINITE;
      li.pDir.Copy (&m_pMoonVector);
      li.pDir.Scale(-1);

      // if no shadows mode then turn of shadows
      if (m_dwShadowsFlags & SF_NOSHADOWS)
         li.fNoShadows = TRUE;

      m_lLIGHTINFO.Add (&li);
   }
#endif // 0

   // loop through all the objects in the world asking them for lights
   DWORD dwNum, dwCur;
   dwNum = m_pWorld->ObjectNum();
   dwCur = m_lLIGHTINFO.Num();
   for (i = 0; i < dwNum; i++) {
      PCObjectSocket pos = m_pWorld->ObjectGet (i);
      if (!pos)
         continue;
   	MALLOCOPT_OKTOMALLOC;
      pos->LightQuery (&m_lLIGHTINFO, m_dwRenderShow);
	   MALLOCOPT_RESTORE;
      if (m_lLIGHTINFO.Num() <= dwCur)
         continue;   // wasn't added

      // else was added, so get the matrix
      CMatrix m, mInvTrans;
      pos->ObjectMatrixGet (&m);
      m.Invert (&mInvTrans);  // only need 3x3 invert since for normals
      mInvTrans.Transpose();
      for (j = dwCur; j < m_lLIGHTINFO.Num(); j++) {
         PLIGHTINFO pli = (PLIGHTINFO) m_lLIGHTINFO.Get(j);
         pli->pDir.p[3] = 1;
         pli->pDir.MultiplyLeft (&mInvTrans);
         pli->pLoc.p[3] = 1;
         pli->pLoc.MultiplyLeft (&m);

         // if no shadows mode then turn of shadows
         if (m_dwShadowsFlags & SF_NOSHADOWS)
            pli->fNoShadows = TRUE;

      }
      dwCur = m_lLIGHTINFO.Num();
   }

   // BUGFIX - If no lights then use a default one from sun
   // BUGFIX - But not in final render
   if (!m_lLIGHTINFO.Num() && !m_fFinalRender) {
      memset (&li, 0, sizeof(li));
      for (i = 0; i < 3; i++) for (j = 0; j < 3; j++)
         li.awColor[i][j] = 0xffff;
      li.afLumens[2] = CM_LUMENSSUN * 2.0 / 3.0;
      li.dwForm = LIFORM_INFINITE;
      li.pDir.Zero();
      li.pDir.p[0] = li.pDir.p[1] = 1;
      li.pDir.p[2] = -1;

      // if no shadows mode then turn of shadows
      if (m_dwShadowsFlags & SF_NOSHADOWS)
         li.fNoShadows = TRUE;

      m_lLIGHTINFO.Add (&li);

      li.dwForm = LIFORM_AMBIENT;
      li.afLumens[2] = CM_LUMENSSUN / 3.0;
      // shadows flag already set
      m_lLIGHTINFO.Add (&li);
   }


   // extract out ambient lights
   fp afAmbient[3];
   afAmbient[0] = afAmbient[1] = afAmbient[2] = 0;
   for (i = m_lLIGHTINFO.Num()-1; i < m_lLIGHTINFO.Num(); i--) {
      pli = (PLIGHTINFO) m_lLIGHTINFO.Get(i);
      if (pli->dwForm != LIFORM_AMBIENT)
         continue;

      // add ambient amount
      afAmbient[0] += (fp) pli->awColor[2][0] * pli->afLumens[2];
      afAmbient[1] += (fp) pli->awColor[2][1] * pli->afLumens[2];
      afAmbient[2] += (fp) pli->awColor[2][2] * pli->afLumens[2];

      // delete
      m_lLIGHTINFO.Remove (i);
   }

   fp fMax;
   fMax = max(max(afAmbient[0], afAmbient[1]), afAmbient[2]);
   if (fMax < EPSILON)
      fMax = 1;
   m_pLightAmbientColor.p[0] = afAmbient[0] / fMax;
   m_pLightAmbientColor.p[1] = afAmbient[1] / fMax;
   m_pLightAmbientColor.p[2] = afAmbient[2] / fMax;
   m_dwIntensityAmbient2 = (DWORD) (fMax / CM_LUMENSSUN);

   // go through the list of existing lights... if they don't exist anymore then remove them
   // if find a match, then update them
   for (i = m_lPCLight.Num()-1; i < m_lPCLight.Num(); i--) {
      pl = *((PCLight*)m_lPCLight.Get(i));
      pl->LightInfoGet (&li, NULL);

      for (j = 0; j < m_lLIGHTINFO.Num(); j++) {
         pli = (PLIGHTINFO) m_lLIGHTINFO.Get(j);
         if (!memcmp(&li, pli, sizeof(LIGHTINFO)))
            break;
         // BUGFIX - Was If (li.pDir.AreClose(&pli->pDir) && li.pLoc.AreClose(&pli->pLoc) &&
         //   (li.fInfinite == pli->fInfinite))
         //   break;
         // this didn't work because if change the the directionality or lumens of liht
         // sometimes changes what look at
      }
      if (j < m_lLIGHTINFO.Num()) {
         pl->LightInfoSet (pli, m_fShadowsLimit);
         m_lLIGHTINFO.Remove (j);   // since accounted for it
         continue;   // exists
      }

      // if get here, light no longer wanted
      delete pl;
      m_lPCLight.Remove (i);
   }

   // go through list of lights that want that dont exist and create them
   for (i = 0; i < m_lLIGHTINFO.Num(); i++) {
      pli = (PLIGHTINFO) m_lLIGHTINFO.Get(i);
      // BUGFIX - Only add point and infinite lights in
      if ((pli->dwForm != LIFORM_POINT) && (pli->dwForm != LIFORM_INFINITE))
         continue;

      // create a new light
	   MALLOCOPT_OKTOMALLOC;
      pl = new CLight(m_dwRenderShard, m_iPriorityIncrease);
	   MALLOCOPT_RESTORE;
      if (!pl)
         continue;
      pl->LightInfoSet (pli, m_fShadowsLimit);
	   MALLOCOPT_OKTOMALLOC;
      m_lPCLight.Add (&pl);
	   MALLOCOPT_RESTORE;
   }

   // tell all the lights about the new world and exclusion info
   for (i = 0; i < m_lPCLight.Num(); i++) {
      pl = *((PCLight*)m_lPCLight.Get(i));
      pl->WorldSet (m_pWorld, m_dwRenderShow & ~(RENDERSHOW_VIEWCAMERA | RENDERSHOW_WEATHER),
         m_fFinalRender, m_dwShowOnly);   // BUGFIX - Pass showonly down
   }

}

/*******************************************************************************
CRenderTraditional::LightsCalc - If any lights are dirty, this calculates them.

  NOTE: Will do a pProgress->Push(). when the function returns, and after rendering
  is all done, the caller needs to call pProgres->Pop() to undo this.

  Does the Push() so progress bar moves smoothly along drawing the lights
inputs
   PCRenderTraditional     *ppRender - Will initially be passed a pointer to NULL.
                           If this needs to recalculate the light, this will allocate
                           a CRenderTraditional and then fill it into *ppRender.
                           The caller will need to delete this.
                           This minimizes the creation of render objects
                           Can be NULL, in which case won't create
   PCProgressSocket     - Progress bar
returns
   none
*/
void CRenderTraditional::LightsCalc (PCRenderTraditional *ppRender, PCProgressSocket pProgress)
{
   // how may are dirty
   DWORD dwNum, i;
   PCLight pl;
   dwNum = 0;
   for (i = 0; i < m_lPCLight.Num(); i++) {
      pl = *((PCLight*)m_lPCLight.Get(i));
      if (pl->DirtyGet())
         dwNum++;
   }
   if (!dwNum) {
      if (pProgress)
         pProgress->Push(0,1);
      return;
   }

#ifdef _DEBUG
   OutputDebugString ("RECALCULATING LIGHTS!\r\n");
#endif

   // lights
   DWORD dw;
   dw = 0;
   for (i = 0; i < m_lPCLight.Num(); i++) {
      pl = *((PCLight*)m_lPCLight.Get(i));
      if (!pl->DirtyGet())
         continue;

      if (pProgress)
         pProgress->Push ((fp)dw / (fp)dwNum / 4.0, (fp)(dw+1) / (fp)dwNum / 4.0);

      pl->RecalcIfDirty (ppRender, pProgress);

      if (pProgress)
         pProgress->Pop();

      dw++;
   }

   // final push
   if (pProgress)
      pProgress->Push (.25, 1);
}



/*******************************************************************************
CRenderTraditional::LightsSpotlight - Called in the shader renderer after all the
pixels that looking at have been caluclated - although not colored. This then takes
a sampling and figures out what the user is looking at (about what volume of space
is most prominent.) This volume is then fed back to all the infinite light sources
so they can send a second spot down on the location. This makes for more accurate-
appearing shadows.

inputs
   PCRenderTraditional     *ppRender - Will initially be passed a pointer to NULL.
                           If this needs to recalculate the light, this will allocate
                           a CRenderTraditional and then fill it into *ppRender.
                           The caller will need to delete this.
                           This minimizes the creation of render objects
                           Can be NULL, in which case won't create

*/
#define MAXDISTANCE        100      // 100 meters

void CRenderTraditional::LightsSpotlight (PCRenderTraditional *ppRender, PCProgressSocket pProgress)
{
   if (!m_pImageShader)
      return;

   // make sure have infinite lights
   DWORD dwNum, i;
   PCLight pl;
   LIGHTINFO li;
   dwNum = 0;
   for (i = 0; i < m_lPCLight.Num(); i++) {
      pl = *((PCLight*)m_lPCLight.Get(i));
      pl->LightInfoGet (&li, NULL);
      if ((li.dwForm == LIFORM_INFINITE) && !li.fNoShadows)
         dwNum++;
            // BUGFIX - checking for shadows too
   }
   if (!dwNum)
      return;

   // find the skydome
   dwNum = m_pWorld->ObjectNum();
   DWORD dwSkydome = (DWORD)-1;
   for (i = 0; i < dwNum; i++) {
      PCObjectSocket pos = m_pWorld->ObjectGet(i);
      if (pos->Message (OSM_SKYDOME, NULL)) {
         dwSkydome = i + 1;   // one offset
         break;
      }
   } // i

   // determine the camera's location
   BOOL fCameraValid;
   CPoint pCamera, pClosestToCamera, pCameraTemp;
   fp fCameraTemp;
   fp fClosestToCamera = INFINITE;
   if (CameraModelGet() != CAMERAMODEL_FLAT) {
      fCameraValid = TRUE;
      PixelToWorldSpace (0, 0, 0, &pCamera);
   }
   else
      fCameraValid = FALSE;

   // have infinite

   DWORD dwPass;
   CPoint p;
   CPoint pMin, pMax;   // minimum and maximum
   BOOL fFoundMinMax = FALSE;
   // BUGFIX - Use double precision so no roundoff errir
   double apAverage[4], apDist[4];
   memset (&apAverage, 0, sizeof(apAverage));
   memset (&apDist, 0, sizeof(apDist));
   DWORD k, j;
   for (dwPass = 0; dwPass < 2; dwPass++) {
      // loop through pixels
      DWORD dwX, dwY;
      for (dwY = 0; dwY < m_pImageShader->Height(); dwY += 4)
         for (dwX = 0; dwX < m_pImageShader->Width(); dwX += 4) {
            PISPIXEL pis = m_pImageShader->Pixel(dwX, dwY);
            if (!pis->pMisc)
               continue;

            PISPIXEL pit;
            fp afZMinMax[2];
            BOOL fFoundZ = FALSE;

            // loop through all transparencies too
            for (pit = pis; pit; ) {
               // if it's the skydome then ignore
               PISDMISC pmisc = pit->pMisc;
               if ( (HIWORD(pmisc->dwID) == dwSkydome) || !HIWORD(pmisc->dwID) )
                  goto next;   // it's the skydome, so ignore

               // if transparency behind opaque one then get rid of
               if (pit->fZ > pis->fZ)
                  goto next;

               // else, valid, so find min and max
               if (fFoundZ) {
                  afZMinMax[0] = min(afZMinMax[0], pit->fZ);
                  afZMinMax[1] = max(afZMinMax[1], pit->fZ);
               }
               else {
                  afZMinMax[0] = afZMinMax[1] = pit->fZ;
                  fFoundZ = TRUE;
               }

next:
               // next
               if (pit->pTrans)
                  pit = (PISPIXEL) pit->pTrans;
               else
                  pit = NULL;
            }  // over pit

            // include the min and max points
            if (!fFoundZ)
               continue;

            // potentially limit
            if (m_fShadowsLimit && (m_dwCameraModel != CAMERAMODEL_FLAT) && (afZMinMax[1] > m_fShadowsLimit)) {
               afZMinMax[0] = min(afZMinMax[0], m_fShadowsLimit);
               afZMinMax[1] = min(afZMinMax[1], m_fShadowsLimit);
            }

            for (j = 0; j < ((afZMinMax[0] == afZMinMax[1]) ? (DWORD)1 : (DWORD)2); j++) {
               // NOTE: Was limiting to 100m, but I think this may work better

               // found a point
               PixelToWorldSpace (dwX, dwY, afZMinMax[j], &p);

               if (dwPass == 0) {
                  // BUGFIX - Find the distance to the camrea
                  if (fCameraValid) {
                     pCameraTemp.Subtract (&p, &pCamera);
                     fCameraTemp = pCameraTemp.Length();
                     if (fCameraTemp < fClosestToCamera) {
                        fClosestToCamera = fCameraTemp;
                        pClosestToCamera.Copy (&p);
                     }

                  }

                  if (afZMinMax[j] < MAXDISTANCE) {
                     // find the average location
                     for (k = 0; k < 3; k++)
                        apAverage[k] += p.p[k];
                     apAverage[3]++;
                  }

                  // keep min/max of this point
                  if (fFoundMinMax) {
                     pMin.Min (&p);
                     pMax.Max (&p);
                  }
                  else {
                     pMin.Copy (&p);
                     pMax.Copy (&p);
                     fFoundMinMax = TRUE;
                  }
               }
               else if (afZMinMax[j] < MAXDISTANCE) { // and dwPass==1
                  // find the distance from the average location
                  for (k = 0; k < 3; k++)
                     p.p[k] -= apAverage[k];
                  p.p[0] = fabs(p.p[0]);
                  p.p[1] = fabs(p.p[1]);
                  p.p[2] = fabs(p.p[2]);
                  for (k = 0; k < 3; k++)
                     apDist[k] += p.p[k];
                  apDist[3]++;
               }
            } // j
         } // over x and y

      // average
      if (dwPass == 0) {
         if (apAverage[3]) for (k = 0; k < 3; k++)
            apAverage[k] /= apAverage[3];
      }
      else {
         if (apDist[3]) for (k = 0; k < 3; k++)
            apDist[k] /= apDist[3];
      }
     
   }  // dwPass

   // now know a center point and box surrounding it that covers approx 1/2 distance
   // either way

   // figure out how much area want to cover
   fp fDiameter;
   CPoint pDist;
   for (k = 0; k < 3; k++)
      pDist.p[k] = apDist[k];
   fDiameter = pDist.Length() * 2;  // BUGFIX - Was *4, but produced too much area

   // BUGFIX - If know where the camera is, the diameter is from the average
   // to the nearest point
   if (fCameraValid && (fClosestToCamera < INFINITE)) {
      for (k = 0; k < 3; k++)
         pDist.p[k] = apAverage[k];
      pDist.Subtract (&pClosestToCamera);
      fDiameter = (fDiameter + pDist.Length() * 2.0) / 2.0;   // since actually had radius
         // BUGFIX - Average with the old value so don't get anything too small
      fDiameter = max (fDiameter, 0.01);   // so not 0
   }

   // figure out the diameter and center for the global spot
   if (!fFoundMinMax) {
      pMin.Zero();
      pMax.Zero();
   }
   pDist.Subtract (&pMin, &pMax);
   fp fDiameterGlobal = pDist.Length();

   // BUGFIX - If centered on camera then use that
   BOOL fWantClipPlanes = TRUE;
   if (m_fShadowsLimitCenteredOnCamera && m_fShadowsLimit && fCameraValid) {
      fDiameterGlobal = m_fShadowsLimit * 2.0;  // since is diameter around camera
      pMin.Copy (&pCamera);
      pMax.Copy (&pCamera);
      for (k = 0; k < 3; k++) {
         pMin.p[k] -= m_fShadowsLimit;
         pMax.p[k] += m_fShadowsLimit;
      }
      fWantClipPlanes = FALSE;
   }

   // if the diameter that want to use is within a factor of 2 of the global
   // one, then optimize and only use one
   // BUGFIX - Was / 2.0, but want to encourage only one shadow to be faster
   BOOL fJustGlobal = (fDiameter >= fDiameterGlobal / 4.0);

   // if have a max light length then just global
   if (m_fShadowsLimit)
      fJustGlobal = TRUE;

   fDiameterGlobal = max(fDiameterGlobal, .1);   // at least some size
      // BUGFIX - Was 1.0, changed to 0.1

   // determine the clip planes for this camera
   CPoint apClip[5][3];
   memset (apClip, 0, sizeof(apClip));
   DWORD dwWidth = Width();
   DWORD dwHeight = Height();
   if (m_dwCameraModel == CAMERAMODEL_FLAT) {
      // left side
      PixelToWorldSpace (-1, -1, 0, &apClip[0][0]);
      PixelToWorldSpace (-1, -1, 10, &apClip[0][1]);
      PixelToWorldSpace (-1, dwHeight+1, 10, &apClip[0][2]);

      // top
      PixelToWorldSpace (dwWidth+1, -1, 0, &apClip[1][0]);
      PixelToWorldSpace (dwWidth+1, -1, 10, &apClip[1][1]);
      PixelToWorldSpace (-1, -1, 10, &apClip[1][2]);

      // right side
      PixelToWorldSpace (dwWidth+1, dwHeight+1, 10, &apClip[2][0]);
      PixelToWorldSpace (dwWidth+1, -1, 10, &apClip[2][1]);
      PixelToWorldSpace (dwWidth+1, -1, 0, &apClip[2][2]);

      // bottom
      PixelToWorldSpace (-1, dwHeight+1, 10, &apClip[3][0]);
      PixelToWorldSpace (dwWidth+1, dwHeight+1, 10, &apClip[3][1]);
      PixelToWorldSpace (dwWidth+1, dwHeight+1, 0, &apClip[3][2]);

      // leave last one all 0's and will be ignored
   }
   else {
      // left side
      PixelToWorldSpace ((fp)dwWidth/2, (fp)dwHeight/2, 0, &apClip[0][0]);
      PixelToWorldSpace (-1, -1, 10, &apClip[0][1]);
      PixelToWorldSpace (-1, dwHeight+1, 10, &apClip[0][2]);

      // top
      apClip[1][0].Copy (&apClip[0][0]);
      PixelToWorldSpace (dwWidth+1, -1, 10, &apClip[1][1]);
      PixelToWorldSpace (-1, -1, 10, &apClip[1][2]);

      // right side
      apClip[2][0].Copy (&apClip[0][0]);
      PixelToWorldSpace (dwWidth+1, dwHeight+1, 10, &apClip[2][1]);
      PixelToWorldSpace (dwWidth+1, -1, 10, &apClip[2][2]);

      // bottom
      apClip[3][0].Copy (&apClip[0][0]);
      PixelToWorldSpace (-1, dwHeight+1, 10, &apClip[3][1]);
      PixelToWorldSpace (dwWidth+1, dwHeight+1, 10, &apClip[3][2]);

      // back
      apClip[4][0].Copy (&apClip[0][0]);
      apClip[4][1].Subtract (&apClip[1][2], &apClip[1][1]);
      apClip[4][1].Add (&apClip[4][0]);
      apClip[4][2].Subtract (&apClip[0][2], &apClip[0][1]);
      apClip[4][2].Add (&apClip[4][0]);
   }


   // send it down
   for (i = 0; i < m_lPCLight.Num(); i++) {
      pl = *((PCLight*)m_lPCLight.Get(i));
      pl->LightInfoGet (&li, NULL);
      if (li.dwForm != LIFORM_INFINITE)
         continue;

      if (pProgress) {
         pProgress->Push ((fp)i / (fp)m_lPCLight.Num(), (fp)(i+1) / (fp)m_lPCLight.Num());
         
         if (!fJustGlobal)
            pProgress->Push (0, 0.5);
      }

      // create a spotlight on the center of everything visible
      CPoint pAverage;
      pAverage.Zero();
      pAverage.Average (&pMin, &pMax);
      // only if not ignoring lights

      pl->Spotlight (ppRender, TRUE, &pAverage, fDiameterGlobal, fWantClipPlanes ? &apClip[0][0] : NULL, pProgress);

      if (pProgress && !fJustGlobal) {
         pProgress->Pop();
         pProgress->Push (0.5, 1);
      }

      if (!fJustGlobal) {
         // secondary spot
         pAverage.Zero();
         for (k = 0; k < 3; k++)
            pAverage.p[k] = apAverage[k];
         pl->Spotlight (ppRender, FALSE, &pAverage, fDiameter, fWantClipPlanes ? &apClip[0][0] : NULL, pProgress);
      }
      else
         pl->Spotlight (ppRender, FALSE, NULL, 0, NULL, NULL);   // to clear

      if (pProgress) {
         if (!fJustGlobal)
            pProgress->Pop(); // extra pop
         pProgress->Pop();
      }
   }
}


#if 0 // DEADCODE
/*******************************************************************************
CRenderTraditional::DrawSunMoon - Draw the sun and the moon - if they're high enough
in the sky.

inputs
   none
returns
   none
*/
void CRenderTraditional::DrawSunMoon (void)
{
   DWORD i;

   // link this to drawing the cameras - so that when draw for outlines, dont
   // get the sun/moon in the way
   if (!(m_dwRenderShow & RENDERSHOW_WEATHER) || (m_dwCameraModel == CAMERAMODEL_FLAT))
      return;

   // save the old model
   //DWORD dwOldModel;
   //dwOldModel = m_dwRenderModel;
   //m_dwRenderModel = RENDERMODEL_SURFACEAMBIENT;

   for (i = 0; i < 2; i++) {
      PCPoint  pVector;
      if (i == 0)
         pVector = &m_lightWorld;
      else
         pVector = &m_pMoonVector;

      // if too low in sky continue
      if (pVector->p[2] < -.1)
         continue;

      m_dwObjectID = (DWORD)((WORD) -1) << 16;
      m_fDetailForThis = .1;
      m_dwWantClipThis = m_ClipOnlyScreen.ClipMask();
         // NOTE: Taking advantace of the fact taht m_ClipOnlyScreen is a subset
         // of m_ClipAll. This only clips against only screen, ignoring the other bits.

      m_RenderMatrix.Push();
      {
         // draw the box
         CRenderSurface cs;
         cs.Init (this);
         double f;
         f = pVector->p[2] * 20;
         f = max(f,0);
         f = min(f,1);
         if (i) {
            // moon color is based on its own color, and averaging with background
            // if the sun is up
            WORD awMoon[3], awBack[3];
            Gamma (m_cBackground, awBack);
            Gamma (RGB(0x40, 0x40, 0x40), awMoon);
            DWORD j;
            for (j = 0; j < 3; j++)
               awMoon[j] = (WORD)min(0xffff, (DWORD)awBack[j] + (DWORD)awMoon[j]);

            COLORREF cMoon;
            cMoon = UnGamma (awMoon);
            cs.SetDefColor (cMoon);
         }
         else
            cs.SetDefColor (RGB(255,
               (BYTE) (f * 255 + (1-f) * 32),
               (BYTE) (f * 255 + (1-f) * 32) ));
         RENDERSURFACE Mat;
         memset (&Mat, 0 ,sizeof(Mat));
         Mat.Material.InitFromID (MATERIAL_FLAT);
         //Mat.Material.m_fSelfIllum = TRUE;
         cs.SetDefMaterial (&Mat);

#define  SUNDIST     10000
         // move the sun away
         CPoint pMove;
         pMove.Copy (pVector);
         pMove.Scale (SUNDIST);
         cs.Translate (pMove.p[0], pMove.p[1], pMove.p[2]);

         // move the sun so it's away and looking in right direction
         CPoint pX, pY, pZ;
         pY.Copy (pVector);
         // pY already normalized
         pX.Copy (i ? &m_pMoonWas : &m_pSunWas);
         pX.Subtract (pVector);
         pZ.CrossProd (&pX, &pY);
         pZ.Normalize();
         if (pZ.Length() < .5)
            goto skip;
         pX.CrossProd (&pY, &pZ);
         pX.Normalize();
         CMatrix m;
         m.RotationFromVectors (&pX, &pY, &pZ);
         cs.Multiply (&m);
         
         double fRadius;
         fRadius = 2.0 * PI * SUNDIST * 5.0 / 360.0 / 2.0;
            // sun and moon are about 5 degrees of arc...

         // draw a bunch of polygons
         DWORD dwColor, dwNormal;
         dwColor = cs.DefColor();
         dwNormal = cs.NewNormal(-pY.p[0], -pY.p[1], -pY.p[2], TRUE);
#define  NUMDIV   16
         DWORD y;
         for (y = 0; y < 16; y++) {
            DWORD    dwPoints;
            PCPoint  pap;
            pap = cs.NewPoints (&dwPoints, 4);
            if (!pap)
               continue;
            memset (pap, 0, sizeof(CPoint)*4);
            pap[0].p[2] = (.5 - (fp)y / NUMDIV) * 1.99;
            pap[0].p[0] = -sqrt(1.0 - pap[0].p[2] * pap[0].p[2]);
            pap[0].Scale (fRadius);
            pap[1].Copy (&pap[0]);
            pap[1].p[0] *= -1;
            pap[2].p[2] = (.5 - (fp)(y+1) / NUMDIV) * 1.99;
            pap[2].p[0] = sqrt(1.0 - pap[2].p[2] * pap[2].p[2]);
            pap[2].Scale (fRadius);
            pap[3].Copy (&pap[2]);
            pap[3].p[0] *= -1;

            if (i) {
               fp fScale1, fScale2;
               fScale1 = 0;
               fScale2 = 1;
               if (m_fMoonPhase < .5)
                  fScale2 = m_fMoonPhase * 2;
               if (m_fMoonPhase > .5)
                  fScale1 = (m_fMoonPhase - .5) * 2;
               CPoint p1, p2;
               p1.Copy (&pap[0]);
               p2.Copy (&pap[1]);
               pap[0].Average (&p2, fScale1);
               pap[1].Average (&p1, 1-fScale2);
               p1.Copy (&pap[3]);
               p2.Copy (&pap[2]);
               pap[3].Average (&p2, fScale1);
               pap[2].Average (&p1, 1-fScale2);
            }

            cs.NewQuad (cs.NewVertex(dwPoints, dwNormal),
               cs.NewVertex(dwPoints+1, dwNormal),
               cs.NewVertex(dwPoints+2, dwNormal),
               cs.NewVertex(dwPoints+3, dwNormal),
               FALSE);
         }
      }
skip: // called in case there's an error
      m_RenderMatrix.Pop();

   }  // over i - sun and moon

   // restore
   //m_dwRenderModel = dwOldModel;
}






/*******************************************************************************
CRenderTraditional::DrawClouds - Draw the clouds in the sky
in the sky.

inputs
   none
returns
   none
*/
void CRenderTraditional::DrawClouds (void)
{
   DWORD i;

   // link this to drawing the cameras - so that when draw for outlines, dont
   // get the sun/moon in the way
   if (!(m_dwRenderShow & RENDERSHOW_WEATHER) || (m_dwCameraModel == CAMERAMODEL_FLAT))
      return;

   // save the old model
   //DWORD dwOldModel;
   //dwOldModel = m_dwRenderModel;
   //m_dwRenderModel = RENDERMODEL_SURFACEAMBIENT;

   // how many - use the phase of the moon, which is date specific
   DWORD dwNum;
   srand ((int) (m_fMoonPhase * 0xffff));
   dwNum = (DWORD) MyRand (30, 50);

   // default object info
   m_dwObjectID = (DWORD)((WORD) -1) << 16;
   m_fDetailForThis = .1;
   m_dwWantClipThis = m_ClipOnlyScreen.ClipMask();
      // NOTE: Taking advantace of the fact taht m_ClipOnlyScreen is a subset
      // of m_ClipAll. This only clips against only screen, ignoring the other bits.
   m_RenderMatrix.Push();

   for (i = 0; i < dwNum; i++) {
      // coloration
      CRenderSurface cs;
      cs.Init (this);
      cs.SetDefColor (RGB(0xff,0xff,0xff));
      RENDERSURFACE Mat;
      memset (&Mat, 0 ,sizeof(Mat));
      Mat.Material.InitFromID (MATERIAL_FLAT);
      cs.SetDefMaterial (&Mat);

      // find the center
      CPoint pCenter;
      pCenter.Zero();
      pCenter.p[0] = MyRand (-2000, 2000);
      pCenter.p[1] = MyRand (-2000, 2000);
      pCenter.p[2] = 500; // all clouds at 1 km high

      // size of the cloud
      CPoint pSize;
      pSize.Zero();
      pSize.p[0] = MyRand (300, 600);
      pSize.p[1] = MyRand (300, 600);
      pSize.p[2] = MyRand (200, 400);

      // fluffly cloud
#define  CLOUDX      10
#define  CLOUDY      8
      DWORD stx, sty;
      CPoint ap[CLOUDX * CLOUDY];
      PCPoint pp;
      stx = CLOUDX;
      sty = CLOUDY;
      DWORD x,y;
      for (x = 0; x < stx; x++) for (y = 0; y < sty; y++) {
         pp = &ap[y*stx+x];
         fp fRadK, fRadI;
         fRadK = PI/2 * (1.0-CLOSE) - y / (fp) (sty-1) * PI * (1.0-CLOSE);
         fRadI = (fp)x / (fp) stx * 2.0 * PI;
         pp->p[1] = cos(fRadI) * cos(fRadK) * pSize.p[1]/2;
         pp->p[2] = sin(fRadK) * pSize.p[2]/2;
         pp->p[0] = -sin(fRadI) * cos(fRadK) * pSize.p[0]/2;

         if ((y != 0) && (y != sty-1)) {
            pp->p[0] *= MyRand (.8, 1.2);
            pp->p[1] *= MyRand (.8, 1.2);
            pp->p[2] *= MyRand (.8, 1.2);
         }

         // offset
         pp->Add (&pCenter);
      }
      DWORD dwIndex;
      pp = cs.NewPoints (&dwIndex, stx * sty);
      if (pp) {
         memcpy (pp, ap, stx * sty * sizeof(CPoint));
         cs.ShapeSurface (1, stx, sty, pp, dwIndex);
      }

   }  // over i -clouds
   m_RenderMatrix.Pop();

   // restore
   //m_dwRenderModel = dwOldModel;
}
#endif // 0 - DEAD code


/*******************************************************************************
CRenderTraditional::DrawPlane - Draw the plane
in the sky.

inputs
   DWORD       dwPlane - 0 => x=0 plane, 1 => y=0 plane, 2 => z=0 plane
returns
   none
*/
void CRenderTraditional::DrawPlane (DWORD dwPlane)
{
   // default object info
   m_adwObjectID[0] = (DWORD)((WORD) -1) << 16;
   m_fDetailForThis = .1;
   m_adwWantClipThis[0] = m_aClipOnlyScreen[0].ClipMask();
      // NOTE: Taking advantace of the fact taht m_ClipOnlyScreen is a subset
      // of m_ClipAll. This only clips against only screen, ignoring the other bits.
   m_aRenderMatrix[0].Push();

   // coloration
   {
      CRenderSurface cs;
      cs.Init (this);
      cs.SetDefColor (RGB(0xc0,0xc0,0xc0));
      RENDERSURFACE Mat;
      memset (&Mat, 0 ,sizeof(Mat));
      Mat.Material.InitFromID (MATERIAL_FLAT);
      cs.SetDefMaterial (&Mat);

      // dimension
      DWORD dwDim, dwDim2;
      dwDim = (dwPlane+1) % 3;
      dwDim2 = (dwDim+1) % 3;

   #define PLANEPOINTS     2
   #define PLANESIZE       100.0
      DWORD stx, sty;
      CPoint ap[PLANEPOINTS * PLANEPOINTS];
      PCPoint pp;
      stx = PLANEPOINTS;
      sty = PLANEPOINTS;
      DWORD x,y;
      for (x = 0; x < stx; x++) for (y = 0; y < sty; y++) {
         pp = &ap[y*stx+x];

         pp->Zero();
         pp->p[dwDim] = PLANESIZE * (x ? 1 : -1);
         pp->p[dwDim2] = PLANESIZE * (y ? -1 : 1);
      }
      DWORD dwIndex;
      pp = cs.NewPoints (&dwIndex, stx * sty);
      if (pp) {
         memcpy (pp, ap, stx * sty * sizeof(CPoint));
         cs.ShapeSurface (1, stx, sty, pp, dwIndex, NULL, 0, FALSE);
      }
   }

   m_aRenderMatrix[0].Pop();

   // restore
   //m_dwRenderModel = dwOldModel;
}


/*******************************************************************************
CRenderTraditional::ImageQueryFloat - From CRenderSuper
*/
DWORD CRenderTraditional::ImageQueryFloat (void)
{
   return 1;   // use both
}

/*******************************************************************************
CRenderTraditional::CFImageSet - FromCRenderSuper
*/
BOOL CRenderTraditional::CFImageSet (PCFImage pImage)
{
   if (!pImage || !pImage->Width())
      return FALSE;

   m_dwCacheImageValid = 0;   // none of the image caches are valid
   m_pFImage = pImage;
   m_pImage = NULL;
   m_pZImage = NULL;
   DWORD dwWidth, dwHeight;
   dwWidth = m_pFImage->Width();
   dwHeight = m_pFImage->Height();


   // set perspective info
   m_aRenderMatrix[0].ScreenInfo (dwWidth, dwHeight); // don't bother with others

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
CRenderTraditional::CZImageSet - FromCRenderSuper
*/
BOOL CRenderTraditional::CZImageSet (PCZImage pImage)
{
   if (!pImage || !pImage->Width())
      return FALSE;

   m_dwCacheImageValid = 0;   // none of the image caches are valid
   m_pZImage = pImage;
   m_pImage = NULL;
   m_pFImage = NULL;
   DWORD dwWidth, dwHeight;
   dwWidth = m_pZImage->Width();
   dwHeight = m_pZImage->Height();


   // set perspective info
   m_aRenderMatrix[0].ScreenInfo (dwWidth, dwHeight); // don't bother with others

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
CRenderTraditional::ExposureGet - From CRenderSuper
*/
fp CRenderTraditional::ExposureGet (void)
{
   return m_fExposure;
}

/*******************************************************************************
CRenderTraditional::ExposureSet - From CRenderSuper
*/
void CRenderTraditional::ExposureSet (fp fExposure)
{
   m_fExposure = fExposure;
}

/*******************************************************************************
CRenderTraditional::BackgroundGet - From CRenderSuper
*/
COLORREF CRenderTraditional::BackgroundGet (void)
{
   return m_cBackground;
}

/*******************************************************************************
CRenderTraditional::BackgroundSet - From CRenderSuper
*/
void CRenderTraditional::BackgroundSet (COLORREF cBack)
{
   m_cBackground = cBack;
}

/*******************************************************************************
CRenderTraditional::AmbientExtraGet - From CRenderSuper
*/
DWORD CRenderTraditional::AmbientExtraGet (void)
{
   return m_dwIntensityAmbientExtra;
}

/*******************************************************************************
CRenderTraditional::AmbientExtraSet - From CRenderSuper
*/
void CRenderTraditional::AmbientExtraSet (DWORD dwAmbient)
{
   m_dwIntensityAmbientExtra = dwAmbient;
}


/*******************************************************************************
CRenderTraditional::RenderShowGet - From CRenderSuper
*/
DWORD CRenderTraditional::RenderShowGet (void)
{
   return m_dwRenderShow;
}

/*******************************************************************************
CRenderTraditional::RenderShowSet - From CRenderSuper
*/
void CRenderTraditional::RenderShowSet (DWORD dwShow)
{
   m_dwRenderShow = dwShow;
}


/*******************************************************************************
RendTraditionalThreadProc - Called for each thread. See CreateBucketThread()
*/
static DWORD WINAPI RendTraditionalThreadProc(LPVOID lpParameter)
{
   PRENDTHREADINFO pInfo = (PRENDTHREADINFO) lpParameter;

   pInfo->pRend->MultiThreadedProc (pInfo);
   return 0;
}


/*******************************************************************************
CRenderTraditional::MultiThreadedPolyCalc - Call this after any place that MIGHT
call MultiThreadedInit() to make sure the number of threads for polygons is correctly
allocated.
*/
void CRenderTraditional::MultiThreadedPolyCalc (void)
{
   if (!m_dwThreadsMax) {
      m_dwThreadsPoly = 0;
      return;
   }

   // BUGBUG - in the future may want a flag to minimize the number of
   // threads for some renders but not others, but probably not worth it

   m_dwThreadsPoly = m_dwThreadsMax;

   // if we aren't doubling the number of threads, then keep the same
   // number of poly threads as max threads
   if (RTTHREADSCALE >= 2) {
      // if this is 32-bit windows then just use all possible threads because
      // won't be doing multiple render shards in circumreality
      if (sizeof(PVOID) <= sizeof(DWORD))
         return;

      // have fewer poly threads
      m_dwThreadsPoly = m_dwThreadsMax / 2;
   }

   // if one thread, then zero
   if (m_dwThreadsPoly <= 1)
      m_dwThreadsPoly = 0;

}

/*******************************************************************************
CRenderTraditional::MultiThreadedInit - Initialize the system for multithreaded
rendering.

returns
   BOOL - TRUE if success, FALSE if error that causes rendering to stop
*/
BOOL CRenderTraditional::MultiThreadedInit (void)
{
   m_pTextureLast = NULL;
   m_dwThreadBufCur = (DWORD)-1;

   m_dwThreadsMax = HowManyProcessors();
   if (m_dwThreadsMax <= 1)
      m_dwThreadsMax = 0;  // if only one processor then dont create extra threads

   // initialize with something
   m_dwThreadsPoly = (DWORD)-1;  // so will crash if havent called MultiThreadedPolyCalc
   m_dwThreadsShadows = m_dwThreadsMax;

   if (!m_dwThreadsMax)
      return TRUE;

   // BUGFIX - Since when rendering scanline, pays to have more threads since
   // some might get temporarly stuck waiting for a scanline, double this
   // NOTE: Looked at taking double threads out bu ends up being > 10% slower
   m_dwThreadsMax *= RTTHREADSCALE;
   m_dwThreadsMax = min(m_dwThreadsMax, MAXRAYTHREAD);

   // initialize with something
   m_dwThreadsShadows = m_dwThreadsMax;

   InitializeCriticalSection (&m_RenderCritSec);

   memset (m_aRENDTHREADINFO, 0, sizeof(m_aRENDTHREADINFO));
   memset (m_aRENDTHREADBUF, 0, sizeof(m_aRENDTHREADBUF));
   m_fExitThreads = FALSE;
   m_fMainSuspended = FALSE;

   m_hEventMainSuspended = CreateEvent (NULL, FALSE, FALSE, NULL);

   // memory for the threads
   DWORD dwPerBuf = sizeof(IHLEND) * 4 * 1000; // Slightly faster w/500 polygons... 200;   // about 20 polygons
      // BUGFIX - Was 500. Making this 1000 makes it slightly faster
      // BUGFIX - Was about 100 polygons, but probably too large a group
      // BUGFIX - Was around 50 polygons. change to 200 since a bit faster
   DWORD dwNum = m_dwThreadsMax * BUFPERTHREAD;
   DWORD dwNeed = dwPerBuf * dwNum;
   if (!m_memThreadBuf.Required (dwNeed))
      return FALSE;
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      m_aRENDTHREADBUF[i].pMem = (PBYTE)m_memThreadBuf.p + dwPerBuf * i;
      m_aRENDTHREADBUF[i].dwMax = dwPerBuf;
   } // i

   // create all the threads
   DWORD dwID;
   for (i = 0; i < m_dwThreadsMax; i++) {
      m_aRENDTHREADINFO[i].pRend = this;
      m_aRENDTHREADINFO[i].dwThread = i;
      m_aRENDTHREADINFO[i].fSuspended = TRUE;
      m_aRENDTHREADINFO[i].hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
      m_aRENDTHREADINFO[i].hThread = CreateThread (NULL, ESCTHREADCOMMITSIZE, RendTraditionalThreadProc, &m_aRENDTHREADINFO[i], 0, &dwID);

#if 0 // def _DEBUG
      char szTemp[64];
      sprintf (szTemp, "\r\nThread ID = %x", (int)dwID);
      OutputDebugString (szTemp);
#endif

      SetThreadPriority (m_aRENDTHREADINFO[i].hThread, VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL, m_iPriorityIncrease));  // so doesnt suck up all CPU
   } // i

   return TRUE;
}



/*******************************************************************************
CRenderTraditional::MultiThreadedEnd - Stop multithreaded use.
*/
void CRenderTraditional::MultiThreadedEnd (void)
{
   if (!m_dwThreadsMax)
      return;  // nothing to do

   // shut down all the threads
   EnterCriticalSection (&m_RenderCritSec);
   m_fExitThreads = TRUE;
   DWORD i;
   // alert each thread that something has changed
   for (i = 0; i < m_dwThreadsMax; i++)
      if (m_aRENDTHREADINFO[i].hEvent)
         SetEvent (m_aRENDTHREADINFO[i].hEvent);
   LeaveCriticalSection (&m_RenderCritSec);

   // wait for them all to shut down
   for (i = 0; i < m_dwThreadsMax; i++) {
      if (!m_aRENDTHREADINFO[i].hThread)
         continue;

      WaitForSingleObject (m_aRENDTHREADINFO[i].hThread, INFINITE);
      CloseHandle (m_aRENDTHREADINFO[i].hThread);
      CloseHandle (m_aRENDTHREADINFO[i].hEvent);
   }

   CloseHandle (m_hEventMainSuspended);
   DeleteCriticalSection (&m_RenderCritSec);
}


/*******************************************************************************
CRenderTraditional::MultiThreadedProc - Thread procedure for rendering.

inputs
   RENDTHREADINFO       pInfo - Thread info
returns
   none... returns when m_fExitThreads is set to TRUE
*/
void CRenderTraditional::MultiThreadedProc (PRENDTHREADINFO pInfo)
{
   DWORD i, j;
   DWORD adwCandidate[MAXRAYTHREAD*BUFPERTHREAD];
   DWORD dwNumCand;
   DWORD dwYoungest, dwOldest;
   //RECT rInter;
   // _ASSERTE (m_dwThreadsPoly != (DWORD)-1);
   DWORD dwNumShadows = m_dwThreadsShadows * BUFPERTHREAD;
   DWORD dwNumMax = m_dwThreadsMax * BUFPERTHREAD;
   DWORD dwUse;
   PRENDTHREADBUF ptb;
   // CMem memScratch;
   // replaced by m_memShaderApplyScratch
#define MAXPOLYPOINTS         100
   m_memShaderApplyScratch[pInfo->dwThread].Required (sizeof(PIHLEND) * MAXPOLYPOINTS);
   PIHLEND *paEnd = (PIHLEND*)m_memShaderApplyScratch[pInfo->dwThread].p;

#ifdef _DEBUG
   static DWORD sdwMaxThreadsAtOnce = 0;
#endif

   while (TRUE) {
      WaitForSingleObject (pInfo->hEvent, INFINITE);

      EnterCriticalSection (&m_RenderCritSec);
lookagain:
      if (m_fExitThreads) {
         // had a request to exit, so do so
         LeaveCriticalSection (&m_RenderCritSec);
         return;
      }

      // if have request to temporarily suspend then do so
      if (pInfo->fTempSuspendWant) {
         pInfo->fTempSuspendAcknowledge = TRUE;
         pInfo->fSuspended = TRUE;

         if (m_fMainSuspended) {
            m_fMainSuspended = FALSE;
            SetEvent (m_hEventMainSuspended);
         }

         LeaveCriticalSection (&m_RenderCritSec);
         continue;
      }

      pInfo->fTempSuspendAcknowledge = FALSE;   // since going ahead and working

      // try to find a buffer to render
      dwNumCand = 0;
      dwYoungest = 0;
      dwOldest = (DWORD)-1;
#ifdef _DEBUG
      DWORD dwActive = 1;  // since this thread is automatically active, although nothing claimed
#endif

      // BUGFIX - Move dwNumPoly calc to here because may be set later
      _ASSERTE (m_dwThreadsPoly != (DWORD)-1);
      DWORD dwNumPoly = m_dwThreadsPoly * BUFPERTHREAD;
      dwNumPoly = min(dwNumPoly, dwNumMax);

      for (i = 0; i < dwNumMax; i++) {
         if (m_aRENDTHREADBUF[i].fPoly) {
            if (i >= dwNumPoly)
               continue;
         }
         else {
            if (i >= dwNumShadows)
               continue;
         }

#ifdef _DEBUG
         if (m_aRENDTHREADBUF[i].fClaimed)
            dwActive++;
#endif

         if (m_aRENDTHREADBUF[i].fClaimed || !m_aRENDTHREADBUF[i].fValid)
            continue;

         adwCandidate[dwNumCand++] = i;
         dwYoungest = max(dwYoungest, m_aRENDTHREADBUF[i].dwAge);
         dwOldest = min(dwOldest, m_aRENDTHREADBUF[i].dwAge);
      } // i

#ifdef _DEBUG
      sdwMaxThreadsAtOnce = max(sdwMaxThreadsAtOnce, dwActive);
#endif

      // if there aren't any candidates then done
      if (!dwNumCand) {
         pInfo->fSuspended = TRUE;
         LeaveCriticalSection (&m_RenderCritSec);
         continue;
      }

      // find a candidate that can use, which means it must be within a certain
      // age of the oldest, ase well as not intersecting other candidates
      dwUse = (DWORD)-1;
      for (i = 0; i < dwNumCand; i++) {
         ptb = &m_aRENDTHREADBUF[adwCandidate[i]];

         // if it's too much younger than the oldest then cant look at
         DWORD dwNumToUse = ptb->fPoly ? dwNumPoly : dwNumShadows;
         if (ptb->dwAge > dwOldest + dwNumToUse)
            continue;

         // if it interects a currently claimed block then cant look at
         for (j = 0; j < dwNumMax; j++) {
            if (!m_aRENDTHREADBUF[j].fClaimed)
               continue;   // not claimed

            // test intersect
            // BUGFIX - disable the intersection test because using the
            // critical sections in CImage, CZImage, and CFImage to protect
            // the rendering
            //if (IntersectRect (&rInter, &ptb->rPixels, &m_aRENDTHREADBUF[j].rPixels))
            //   break;
         } // j
         if (j < dwNumMax)
            continue;   // cant use this because the pixels are taken

         // if already have one selected, try to take the oldest
         if ((dwUse != (DWORD)-1) && (m_aRENDTHREADBUF[dwUse].dwAge < ptb->dwAge))
            continue;

         // else, can use this
         dwUse = adwCandidate[i];
      } // i

      // if cant find one to use then suspend
      if (dwUse == (DWORD)-1) {
         pInfo->fSuspended = TRUE;
         LeaveCriticalSection (&m_RenderCritSec);
         continue;
      }

      // else found
      ptb = &m_aRENDTHREADBUF[dwUse];
      pInfo->fSuspended = FALSE;
      ptb->fClaimed = TRUE;
      LeaveCriticalSection (&m_RenderCritSec);


      // render it
      DWORD dwCur = 0;
#ifdef USEZIMAGESMALL
      BOOL fUpdateZImageSmall = FALSE;
#endif
      while (dwCur < ptb->dwUsed) {
         PRENDTHREADPOLY ptp = (PRENDTHREADPOLY)(ptb->pMem + dwCur);
         PRENDTHREADSPOLY ptsp = (PRENDTHREADSPOLY)(ptb->pMem + dwCur);
         PRENDERTHREADCLONE ptc = (PRENDERTHREADCLONE)(ptb->pMem + dwCur);
         DWORD *padw = (DWORD*) (ptb->pMem + dwCur);
         PIHLEND pih = (PIHLEND)(ptp+1);
         PISHLEND pish = (PISHLEND)(ptsp+1);
         PCMatrix pmc = (PCMatrix) (ptc+1);
         DWORD dwNumPtp, dwNumPtsp;

         switch (ptp->dwAction) {
         case 0:  // solid polygon for CImage or CZImage
            _ASSERTE (ptb->fPoly);
            dwNumPtp = min(ptp->dwNum, MAXPOLYPOINTS);
            for (i = 0; i < dwNumPtp; i++)
               paEnd[i] = pih + i;

            if (m_pImage)
               m_pImage->DrawPolygon (dwNumPtp, paEnd, &ptp->Misc, ptp->fSolid,
                  ptp->pTexture, ptp->fGlowScale, ptp->fTriangle, pInfo->dwThread, m_fFinalRender);
            else // m_pZImage
               m_pZImage->DrawPolygon (dwNumPtp, paEnd, &ptp->Misc, ptp->fSolid,
                  ptp->pTexture, ptp->fTriangle, pInfo->dwThread);

            dwCur += sizeof(RENDTHREADPOLY) + ptp->dwNum * sizeof(IHLEND);
#ifdef USEZIMAGESMALL
            fUpdateZImageSmall = TRUE;
#endif
            break;

         case 1:  // lines for CImage or CZImage
            _ASSERTE (ptb->fPoly);
            for (j = 0; j < ptp->dwNum; j++) {
               if (m_pImage)
                  m_pImage->DrawLine (&pih[j], &pih[(j+1)%ptp->dwNum], &ptp->Misc);
               else // if (m_pZImage
                  m_pZImage->DrawLine (&pih[j], &pih[(j+1)%ptp->dwNum], &ptp->Misc);
               // NOTE: Not checking m_pFImage
            } // j

            dwCur += sizeof(RENDTHREADPOLY) + ptp->dwNum * sizeof(IHLEND);
#ifdef USEZIMAGESMALL
            fUpdateZImageSmall = TRUE;
#endif
            break;

         case 10:  // solid polygon for CFImage
            _ASSERTE (ptb->fPoly);
            dwNumPtsp = min(ptsp->dwNum, MAXPOLYPOINTS);
            for (i = 0; i < dwNumPtsp; i++)
               paEnd[i] = (PIHLEND) (pish + i);

            m_pImageShader->DrawPolygon (dwNumPtsp, (PISHLEND*)paEnd, &ptsp->Misc,
               ptsp->fTriangle, pInfo->dwThread);

            dwCur += sizeof(RENDTHREADSPOLY) + ptsp->dwNum * sizeof(ISHLEND);
#ifdef USEZIMAGESMALL
            fUpdateZImageSmall = TRUE;
#endif
            break;

         case 11:  // lines for CFImage
            _ASSERTE (ptb->fPoly);
            for (j = 0; j < ptsp->dwNum; j++)
               m_pImageShader->DrawLine (&pish[j], &pish[(j+1)%ptsp->dwNum], &ptsp->Misc, pInfo->dwThread);

            dwCur += sizeof(RENDTHREADSPOLY) + ptsp->dwNum * sizeof(ISHLEND);
#ifdef USEZIMAGESMALL
            fUpdateZImageSmall = TRUE;
#endif
            break;

         case 13: // renderthreadclone
            {
               _ASSERTE (ptb->fPoly);
               CPoint apBound[2];
               PCRayObject    apRayObject[RTCLONE_LOD];

               m_adwObjectID[pInfo->dwThread+1] = ptc->dwObjectID;
               m_adwWantClipThis[pInfo->dwThread+1] = ptc->dwWantClipThis;
               m_aRendThreadRenderMatrix[dwUse].CloneTo (&m_aRenderMatrix[pInfo->dwThread+1]);
               m_aRenderMatrix[pInfo->dwThread+1].Pop(); // BUGFIX - So undo the object's matrix, and just cave worldspace to camera

               if (CloneGet (&ptc->gCode, &ptc->gSub, FALSE, &apBound[0], &apRayObject[0]))
                  for (j = 0; j < ptc->dwNum; j++) {
                     RECT rChanged;
                     CloneRenderObject (pInfo->dwThread+1, &apBound[0], &apRayObject[0], pmc + j, &rChanged);

                     if ((rChanged.right != rChanged.left) && (rChanged.bottom != rChanged.top)) {
#ifdef USEZIMAGESMALL
                        fUpdateZImageSmall = TRUE;
#endif

                        // Z image has changed
                        if ((ptb->rPixels.right == ptb->rPixels.left) && (ptb->rPixels.bottom == ptb->rPixels.top))
                           ptb->rPixels = rChanged;
                        else {
                           // already have some changes, so union
                           RECT rTemp = ptb->rPixels;
                           UnionRect (&ptb->rPixels, &rTemp, &rChanged);
                        }
                     }
                  }

               dwCur += sizeof(RENDERTHREADCLONE) + ptc->dwNum * sizeof(CMatrix);
            }
            break;

         case 12: // scanlines
            _ASSERTE (!ptb->fPoly);
            ShaderApplyScanLines (padw[1], padw[2], pInfo->dwThread, &m_memShaderApplyScratch[pInfo->dwThread]);
            dwCur = ptb->dwUsed;  // done

            // BUGFIX - readjust paEnd since my have been moved by m_memShaderApplyScratch[pInfo->dwThread]
            paEnd = (PIHLEND*)m_memShaderApplyScratch[pInfo->dwThread].p;

            break;

         default:
            dwCur = ptb->dwUsed;  // error
            break;
         } // switch
      } // renderwing

      // update small ZImage Z
#ifdef USEZIMAGESMALL
      if (fUpdateZImageSmall && m_pZImageSmall) {
         RECT rSmall;
         rSmall.left = ptb->rPixels.left / ZIMAGESMALLSCALE;
         rSmall.top = ptb->rPixels.top / ZIMAGESMALLSCALE;
         rSmall.right = (ptb->rPixels.right + ZIMAGESMALLSCALE - 1) / ZIMAGESMALLSCALE;
         rSmall.bottom = (ptb->rPixels.bottom + ZIMAGESMALLSCALE - 1) / ZIMAGESMALLSCALE;

         rSmall.left = max(rSmall.left, 0);
         rSmall.top = max(rSmall.top, 0);
         rSmall.right = min(rSmall.right, (int)m_pZImageSmall->Width());
         rSmall.bottom = min(rSmall.bottom, (int)m_pZImageSmall->Height());

         // loop over rSmall pixels
         DWORD dwSmallX, dwSmallY;
         DWORD dwWidth = Width();
         DWORD dwHeight = Height();
         DWORD xx, yy;
         PZIMAGEPIXEL pipZ, pipZBig;
         PISPIXEL pipS;
         PIMAGEPIXEL pip;
         float fZ;
         for (dwSmallY = (DWORD)rSmall.top; dwSmallY < (DWORD)rSmall.bottom; dwSmallY++) {
            pipZ = m_pZImageSmall->Pixel ((DWORD)rSmall.left, dwSmallY);
            for (dwSmallX = (DWORD)rSmall.left; dwSmallX < (DWORD)rSmall.right; dwSmallX++, pipZ++) {

               // calculate bounds for this
               DWORD dwLeft = dwSmallX * ZIMAGESMALLSCALE;
               DWORD dwTop = dwSmallY * ZIMAGESMALLSCALE;
               DWORD dwRight = min(dwWidth, (dwSmallX + 1) * ZIMAGESMALLSCALE);
               DWORD dwBottom = min(dwHeight, (dwSmallY + 1) * ZIMAGESMALLSCALE);

               // starting Z and that max
               fZ = -ZINFINITE;
               for (yy = dwTop; yy < dwBottom; yy++) {
                  if (m_pImageShader) 
                     for (xx = dwLeft, pipS = m_pImageShader->Pixel(dwLeft, yy); xx < dwRight; xx++, pipS++)
                        fZ = max(fZ, pipS->fZ);
                  else if (m_pImage)
                     for (xx = dwLeft, pip = m_pImage->Pixel(dwLeft, yy); xx < dwRight; xx++, pip++)
                        fZ = max(fZ, pip->fZ);
                  else if (m_pZImage)
                     for (xx = dwLeft, pipZBig = m_pZImage->Pixel(dwLeft, yy); xx < dwRight; xx++, pipZBig++)
                        fZ = max(fZ, pipZBig->fZ);
               } // yy

               // store this
               pipZ->fZ = fZ;
            } // dwSmallX
         } // dwSmallY
      }
#endif // USEZIMAGESMALL

      // mark as completed, and potentially wake up main thread
      EnterCriticalSection (&m_RenderCritSec);
      ptb->fClaimed = ptb->fValid = FALSE;
      if (m_fMainSuspended) {
         m_fMainSuspended = FALSE;
         SetEvent (m_hEventMainSuspended);
      }

      // go back for another look at the buffers as long as we're in a
      // critical section, and see if can pull any others out
      goto lookagain;
   } // while TRUE

   // else exited
   return;
}


/*******************************************************************************
CRenderTraditional::MultiThreadedWaitForComplete - This waits for
all the threads to complete their operations, and then returns.

inputs
   BOOL        fCancelled - If TRUE then buffers that are valid, but unclaimed,
               are wiped out.
*/
void CRenderTraditional::MultiThreadedWaitForComplete (BOOL fCancelled)
{
   if (!m_dwThreadsMax)
      return; // nothing to wait for

   // make sure to send whatever is left to be rendered
   if ((m_dwThreadBufCur != (DWORD)-1) && !fCancelled) {
      MultiThreadedUnClaimBuf (m_dwThreadBufCur);
      m_dwThreadBufCur = (DWORD)-1;
   }

   DWORD i, dwClaimed;
   DWORD dwNum = m_dwThreadsMax * BUFPERTHREAD;
   while (TRUE) {
      EnterCriticalSection (&m_RenderCritSec);

      // loop through all the buffers and see how many checked out
      dwClaimed = 0;
      for (i = 0; i < dwNum; i++) {
         if (m_aRENDTHREADBUF[i].fClaimed) {
            dwClaimed++;
            continue;
         }

         // if looking to mark all as invalid, then might as well
         // set this to false
         if (fCancelled)
            m_aRENDTHREADBUF[i].fValid = FALSE;

         // if this is valid then might as well be claimed
         if (m_aRENDTHREADBUF[i].fValid)
            dwClaimed++;
      } // i

      // if any are claimed then note that we're waiting
      if (dwClaimed)
         m_fMainSuspended = TRUE;
      else
         m_fMainSuspended = FALSE;
      LeaveCriticalSection (&m_RenderCritSec);

      // if not claimed then done
      if (!dwClaimed)
         break;

      // else, suspend
      WaitForSingleObject (m_hEventMainSuspended, INFINITE);
   }

   // tell all that want them to suspend
   EnterCriticalSection (&m_RenderCritSec);
   for (i = 0; i < m_dwThreadsMax; i++) {
      m_aRENDTHREADINFO[i].fTempSuspendWant = TRUE;
      SetEvent (m_aRENDTHREADINFO[i].hEvent);
   }
   LeaveCriticalSection (&m_RenderCritSec);

   // now, wait for everything to finish
   while (TRUE) {
      EnterCriticalSection (&m_RenderCritSec);

      // see if any haven't yet acknowledged suspension
      DWORD dwAck = 0;
      for (i = 0; i < m_dwThreadsMax; i++)
         if (m_aRENDTHREADINFO[i].fTempSuspendAcknowledge)
            dwAck++;

      m_fMainSuspended = (dwAck < m_dwThreadsMax);

      LeaveCriticalSection (&m_RenderCritSec);

      if (dwAck >= m_dwThreadsMax) {
         // turn off request for suspension
         EnterCriticalSection (&m_RenderCritSec);
         for (i = 0; i < m_dwThreadsMax; i++)
            m_aRENDTHREADINFO[i].fTempSuspendWant = m_aRENDTHREADINFO[i].fTempSuspendAcknowledge = FALSE;
         LeaveCriticalSection (&m_RenderCritSec);

         return;
      }

      // else, suspend
      WaitForSingleObject (m_hEventMainSuspended, INFINITE);
   } // while TRUE
}


/*******************************************************************************
CRenderTraditional::MultiThreadedClaimBuf - Causes the main thread to claim
a buffer for itself... which basically means looking through the list for
an unclaimed and invalid buffer. If there is no free buffer then it waits.

inputs
   BOOL     fPoly - Set to TRUE if claiming for polygon, FALSE if for scanline
returns
   DWORD - Buffer index
*/
DWORD CRenderTraditional::MultiThreadedClaimBuf (BOOL fPoly)
{
   DWORD i, dwFound;
   DWORD dwNum;
   _ASSERTE (m_dwThreadsPoly != (DWORD)-1);
   if (fPoly)
      dwNum = m_dwThreadsPoly * BUFPERTHREAD;
   else
      dwNum = m_dwThreadsShadows * BUFPERTHREAD;
   DWORD dwMaxAge;
   while (TRUE) {
      EnterCriticalSection (&m_RenderCritSec);

      // loop through all the buffers and see how many checked out
      dwMaxAge = 0;
      dwFound = (DWORD)-1;
      for (i = 0; i < dwNum; i++) {
         if (m_aRENDTHREADBUF[i].fClaimed || m_aRENDTHREADBUF[i].fValid) {
            dwMaxAge = max(m_aRENDTHREADBUF[i].dwAge, dwMaxAge);
            continue;
         }

         // else, found
         if (dwFound == (DWORD)-1)
            dwFound = i;
      } // i

      // if any are claimed then note that we're waiting
      if (dwFound == (DWORD)-1)
         m_fMainSuspended = TRUE;
      else {
         m_fMainSuspended = FALSE;
         m_aRENDTHREADBUF[dwFound].dwAge = dwMaxAge+1;   // so keep updating age
         m_aRENDTHREADBUF[dwFound].dwUsed = 0;  // might as well clear
         memset (&m_aRENDTHREADBUF[dwFound].rPixels, 0, sizeof(m_aRENDTHREADBUF[dwFound].rPixels));
         m_aRENDTHREADBUF[dwFound].dwPolyArea = 0;
         m_aRENDTHREADBUF[dwFound].fPoly = fPoly;
      }

      LeaveCriticalSection (&m_RenderCritSec);

      // if not claimed then done
      if (dwFound != (DWORD)-1)
         return dwFound;

      // else, suspend
      WaitForSingleObject (m_hEventMainSuspended, INFINITE);
   } // while TRUE

   // wont ever get here
   return (DWORD)-1;
}




/*******************************************************************************
CRenderTraditional::MultiThreadedUnClaimBuf - This sets the buffer fValid flag
to TRUE, and enables any threads that are suspended and waiting to act.

inputs
   DWORD       dwBuf - Buffer index. From MultiThreadedClaimBuf()
*/
void CRenderTraditional::MultiThreadedUnClaimBuf (DWORD dwBuf)
{
   DWORD i;

   EnterCriticalSection (&m_RenderCritSec);

   // mark as valid and unclaimed
   m_aRENDTHREADBUF[dwBuf].fValid = TRUE;
   m_aRENDTHREADBUF[dwBuf].fClaimed = FALSE;

   // see if can find a suspended render thread and activate it
   for (i = 0; i < m_dwThreadsMax; i++) {
      if (m_aRENDTHREADINFO[i].fSuspended) {
         SetEvent (m_aRENDTHREADINFO[i].hEvent);
         m_aRENDTHREADINFO[i].fSuspended = FALSE;
         break;   // only bother reactivating one
      }
   } // i

   LeaveCriticalSection (&m_RenderCritSec);
}


/*******************************************************************************
CRenderTraditional::MultiThreadedDrawClone - Caches a draw-clone.
Must be called from the primary thread.

CloneGet() must have already been successfully called for this object.

inputs
   GUID           *pgCode - Object code
   GUID           *pgSub - Object subcode
   DWORD          dwNum - Number
   PCMatrix       pcm - Array of dwNum matricies
returns
   BOOL - TRUE if success
*/
BOOL CRenderTraditional::MultiThreadedDrawClone (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pcm)
{
   if (!dwNum)
      return TRUE;

   // repeat
   while (dwNum) {
      // if don't have a buffer then create
      if (m_dwThreadBufCur == (DWORD)-1)
         m_dwThreadBufCur = MultiThreadedClaimBuf (TRUE);

      // if there's stuff here already then unclaim the buffer and start a new one
      if (m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed) {
         MultiThreadedUnClaimBuf (m_dwThreadBufCur);
         m_dwThreadBufCur = (DWORD)-1;
         continue;
      }

      // copy the current render matrix
      m_aRenderMatrix[0].CloneTo (&m_aRendThreadRenderMatrix[m_dwThreadBufCur]);

      // zeor out
      m_aRENDTHREADBUF[m_dwThreadBufCur].dwPolyArea = 0;
      memset (&m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels, 0, sizeof(m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels));

      DWORD dwMax = m_aRENDTHREADBUF[m_dwThreadBufCur].dwMax - m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed;
      dwMax = dwMax - sizeof(RENDERTHREADCLONE);
      dwMax /= sizeof(CMatrix);

      // dont do any more than dwNum or dwMax
      dwMax = min(dwMax, dwNum);
      DWORD dwNeed = sizeof(RENDERTHREADCLONE) + sizeof(CMatrix) * dwNum;
      PRENDERTHREADCLONE ptc = (PRENDERTHREADCLONE) (m_aRENDTHREADBUF[m_dwThreadBufCur].pMem + m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed);
      PCMatrix pmTC = (PCMatrix) (ptc + 1);

      ptc->dwAction = 13;
      ptc->dwNum = dwMax;
      ptc->gCode = *pgCode;
      ptc->gSub = *pgSub;
      ptc->dwWantClipThis = m_adwWantClipThis[0];
      ptc->dwObjectID = m_adwObjectID[0];

      // copy over matricies
      memcpy (pmTC, pcm, sizeof(CMatrix) * dwMax);

      m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed = dwNeed;

      // move on
      pcm += dwMax;
      dwNum -= dwMax;

      // finally unclaim this
      MultiThreadedUnClaimBuf (m_dwThreadBufCur);
      m_dwThreadBufCur = (DWORD)-1;
   }

   return TRUE;

}

/*******************************************************************************
CRenderTraditional::MultiThreadedDrawPolygon - Caches a draw-polygon.

inputs
   BOOL           fFill - If TRUE then fill the polygon, else just draw lines
   DWORD          dwNum - Number of points
   PIHLEND        ppEnd[] - List of points
   PIDMISC        pm - Misc info
   BOOL           fSolid - TRUE if it's a solid color
   PCTextureMapSocket pTexture - Texture to draw4
   fp             fGlowScale - Amount of glow
   BOOL           fTriangle - If TRUE then convert to triangles first
returns
   BOOL - TRUE if succeess
*/

#define MAXBLOCKAREA       256       // if larger than this then move into new thread
   // BUGFIX - Was 256, upped to 512 and got slightly faster
   // BUGFIX - Went from 512 to 256 because about the same speed, and 256 seems more right
#define POLYAREASCALE      2        // scan be slightly larger than MAXBLOCKAREA indicates

BOOL CRenderTraditional::MultiThreadedDrawPolygon (BOOL fFill, DWORD dwNum, const PIHLEND ppEnd[],
      const PIDMISC pm, BOOL fSolid, const PCTextureMapSocket pTexture, fp fGlowScale,
      BOOL fTriangle)
{
   // if don't have a buffer then create
   if (m_dwThreadBufCur == (DWORD)-1)
      m_dwThreadBufCur = MultiThreadedClaimBuf (TRUE);

   // determine the new area
   DWORD i;
   TEXTUREPOINT tpMin, tpMax;
   for (i = 0; i < dwNum; i++) {
      if (i) {
         tpMin.h = min(tpMin.h, ppEnd[i]->x);
         tpMin.v = min(tpMin.v, ppEnd[i]->y);
         tpMax.h = max(tpMax.h, ppEnd[i]->x);
         tpMax.v = max(tpMax.v, ppEnd[i]->y);
      }
      else {
         tpMin.h = tpMax.h = ppEnd[i]->x;
         tpMin.v = tpMax.v = ppEnd[i]->y;
      }
   }

   // rectangle in ints
   RECT r;
   r.left = floor(tpMin.h);
   r.right = ceil(tpMax.h) + 1;
   r.top = floor(tpMin.v);
   r.bottom = ceil(tpMax.v) + 1;

   RECT rNew;
   // rectangle in ints
   if (m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed)
      UnionRect (&rNew, &m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels, &r);
   else {
      m_aRENDTHREADBUF[m_dwThreadBufCur].dwPolyArea = 0;
      rNew = r;
   }

   // increase the polygon area
   //DWORD dwImageWidth = Width();
   //DWORD dwImageHeight = Height();
   if (m_aRENDTHREADBUF[m_dwThreadBufCur].dwPolyArea) {
      DWORD dwMaxPolyArea = MAXBLOCKAREA * MAXBLOCKAREA * POLYAREASCALE;
      dwMaxPolyArea = max(dwMaxPolyArea, 1);

      if (m_aRENDTHREADBUF[m_dwThreadBufCur].dwPolyArea > dwMaxPolyArea) {
         MultiThreadedUnClaimBuf (m_dwThreadBufCur);
         m_dwThreadBufCur = MultiThreadedClaimBuf (TRUE);
         m_aRENDTHREADBUF[m_dwThreadBufCur].dwPolyArea = 0;
         rNew = r;   // since starting from scratch
      }
   }


   // BUGFIX - If the drawing area is already too large then create a new buffer
   if (m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed) {
      int iWidth = rNew.right - rNew.left;
      int iHeight = rNew.bottom - rNew.top;
      int iSize = MAXBLOCKAREA; // (int) max(dwImageWidth, dwImageHeight) / MAXBLOCKAREA;
      iSize = max(iSize, 1);

      if ((iWidth > iSize) && (iHeight > iSize)) {
         MultiThreadedUnClaimBuf (m_dwThreadBufCur);
         m_dwThreadBufCur = MultiThreadedClaimBuf (TRUE);
         m_aRENDTHREADBUF[m_dwThreadBufCur].dwPolyArea = 0;
         rNew = r;   // since starting from scratch
      }
   }

   // remember the area
   m_aRENDTHREADBUF[m_dwThreadBufCur].dwPolyArea += (DWORD)(r.right - r.left) * (DWORD)(r.bottom - r.top);

   // figure out how much memory this will require
   DWORD dwNeed = sizeof(RENDTHREADPOLY) + dwNum * sizeof(IHLEND);

   // if what we need will overrun the current buffer then check it in and
   // pull out another one
   if (dwNeed + m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed > m_aRENDTHREADBUF[m_dwThreadBufCur].dwMax) {
      // if way too large then just done... shouldnt happen
      if (dwNeed >= m_aRENDTHREADBUF[m_dwThreadBufCur].dwMax)
         return FALSE;

      MultiThreadedUnClaimBuf (m_dwThreadBufCur);
      m_dwThreadBufCur = MultiThreadedClaimBuf (TRUE);
      rNew = r;   // since starting from scratch
   } // if need more space


   // prepare to copy over
   PRENDTHREADPOLY prp = (PRENDTHREADPOLY) (m_aRENDTHREADBUF[m_dwThreadBufCur].pMem + m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed);
   PIHLEND pih = (PIHLEND)(prp + 1);

   // copy over
   prp->dwAction = fFill ? 0 : 1;
   prp->dwNum = dwNum;
   prp->fGlowScale = fGlowScale;
   prp->fSolid = fSolid;
   prp->fTriangle = fTriangle;
   if (pm)
      memcpy (&prp->Misc, pm, sizeof(prp->Misc));
   prp->pTexture = pTexture;
   for (i = 0; i < dwNum; i++, pih++)
      memcpy (pih, ppEnd[i], sizeof(*pih));

   m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels = rNew;

   // increase
   m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed += dwNeed;


   return TRUE;
}

/*******************************************************************************
CRenderTraditional::MultiThreadedDrawPolygon - (Shader version) Caches a draw-polygon.

inputs
   BOOL           fFill - If TRUE then fill the polygon, else just draw lines
   DWORD          dwNum - Number of points
   PISHLEND        ppEnd[] - List of points
   PISDMISC        pm - Misc info
   BOOL           fTriangle - If TRUE then convert to triangles first
returns
   BOOL - TRUE if succeess
*/
BOOL CRenderTraditional::MultiThreadedDrawPolygon (BOOL fFill, DWORD dwNum, const PISHLEND ppEnd[],
      const PISDMISC pm, BOOL fTriangle)
{
   // if don't have a buffer then create
   if (m_dwThreadBufCur == (DWORD)-1)
      m_dwThreadBufCur = MultiThreadedClaimBuf (TRUE);

   // figure out how much memory this will require
   DWORD dwNeed = sizeof(RENDTHREADSPOLY) + dwNum * sizeof(ISHLEND);

   // if what we need will overrun the current buffer then check it in and
   // pull out another one
   if (dwNeed + m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed > m_aRENDTHREADBUF[m_dwThreadBufCur].dwMax) {
      // if way too large then just done... shouldnt happen
      if (dwNeed >= m_aRENDTHREADBUF[m_dwThreadBufCur].dwMax)
         return FALSE;

      MultiThreadedUnClaimBuf (m_dwThreadBufCur);
      m_dwThreadBufCur = MultiThreadedClaimBuf (TRUE);
   } // if need more space


   // prepare to copy over
   PRENDTHREADSPOLY prp = (PRENDTHREADSPOLY) (m_aRENDTHREADBUF[m_dwThreadBufCur].pMem + m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed);
   PISHLEND pih = (PISHLEND)(prp + 1);

   // copy over
   prp->dwAction = fFill ? 10 : 11;
   prp->dwNum = dwNum;
   prp->fTriangle = fTriangle;
   if (pm)
      memcpy (&prp->Misc, pm, sizeof(prp->Misc));

   DWORD i;
   TEXTUREPOINT tpMin, tpMax;
   for (i = 0; i < dwNum; i++, pih++) {
      memcpy (pih, ppEnd[i], sizeof(*pih));
      if (i) {
         tpMin.h = min(tpMin.h, pih->x);
         tpMin.v = min(tpMin.v, pih->y);
         tpMax.h = max(tpMax.h, pih->x);
         tpMax.v = max(tpMax.v, pih->y);
      }
      else {
         tpMin.h = tpMax.h = pih->x;
         tpMin.v = tpMax.v = pih->y;
      }
   }

   // rectangle in ints
   RECT r;
   r.left = floor(tpMin.h);
   r.right = ceil(tpMax.h) + 1;
   r.top = floor(tpMin.v);
   r.bottom = ceil(tpMax.v) + 1;
   if (m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed) {
      RECT rCur = m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels;
      UnionRect (&m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels, &rCur, &r);
   }
   else
      m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels = r;

   // increase
   m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed += dwNeed;


   return TRUE;
}



/*******************************************************************************
CRenderTraditional::MultiThreadedShadowsScanline - (Shader version) Adds
some scanlines to the queue

inputs
   DWORD          dwStart - Start y
   DWORD          dwEnd - End y
returns
   BOOL - TRUE if succeess
*/
BOOL CRenderTraditional::MultiThreadedShadowsScanline (DWORD dwStart, DWORD dwEnd)
{
   // if happen to have current buffer, which shouldn't, then check it in
   if (m_dwThreadBufCur != (DWORD)-1)
      MultiThreadedUnClaimBuf (m_dwThreadBufCur);
   
   // claim one
   m_dwThreadBufCur = MultiThreadedClaimBuf (FALSE);

   // we will always have enough since only need 3 DWORDs
   DWORD *pdw = (DWORD*)m_aRENDTHREADBUF[m_dwThreadBufCur].pMem;
   pdw[0] = 12;   // number indicating what to do
   pdw[1] = dwStart;
   pdw[2] = dwEnd;
   m_aRENDTHREADBUF[m_dwThreadBufCur].dwUsed = sizeof(DWORD)*3;
   m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels.top = (int)dwStart;
   m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels.bottom = (int)dwEnd;
   m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels.left = 0;
   m_aRENDTHREADBUF[m_dwThreadBufCur].rPixels.right = 1;

   // check it back in
   MultiThreadedUnClaimBuf (m_dwThreadBufCur);
   m_dwThreadBufCur = (DWORD)-1;

   return TRUE;
}



/*******************************************************************************
CRenderTraditional::CloneFreeAll - Call this at the end of rendering
to free all the clones
*/
void CRenderTraditional::CloneFreeAll (void)
{
   // loop through all the clones
   DWORD i, j;
   PRTCLONEINFO pci = (PRTCLONEINFO) m_lRTCLONEINFO.Get(0);
   for (i = 0; i < m_lRTCLONEINFO.Num(); i++, pci++)
      for (j = 0; j < RTCLONE_LOD; j++)
         if (pci->apRayObject[j])
            delete pci->apRayObject[j];
               // NOTE: This will also release the textures
   m_lRTCLONEINFO.Clear();
}


/*******************************************************************************
CRenderTraditional::CloneGet - Gets a clone object (CRayObject) and potentially
creates one if it doesn't exist.

For every call to CloneGet()
there must be a call to CloneRelease() to free up the object.

inputs
   GUID        *pgCode - code for the clone
   GUID        *pgSub - Subcode
   BOOL        fCreateIfNotExist - If a clone isn't found AND this is TRUE,
                  then the clone will be created. ONLY set this to TRUE if IN
                  the main thread, NOT in one of the render subthreads.
   PCPoint        papBound - Pointer to an array of 2 points that are filled with min
                  and max bounding box coords (in clone's).
   PCRayObject *ppRayObject - Array of RTCLONE_LOD - If not NULL, this is filled
                  with all the clone objects for different levels of detail
returns
   PCRayObject - For the most detailed clone, or NULL if can't find
*/
PCRayObject CRenderTraditional::CloneGet (GUID *pgCode, GUID *pgSub, BOOL fCreateIfNotExist,
                                          PCPoint papBound, PCRayObject *ppRayObject)
{
   PCRayObject pRet;

   EnterCriticalSection (&m_RTCLONECritSec);
   DWORD i;
   PRTCLONEINFO pci = (PRTCLONEINFO) m_lRTCLONEINFO.Get(0);
   for (i = 0; i < m_lRTCLONEINFO.Num(); i++, pci++)
      if (IsEqualGUID(pci->gCode, *pgCode) && IsEqualGUID(pci->gSub, *pgSub)) {
         // found a match
         if (pci->fRecurse) {
            LeaveCriticalSection (&m_RTCLONECritSec);
            return NULL;   // error, object trying to contain itself
         }

         pci->dwCount++;
         if (ppRayObject)
            memcpy (ppRayObject, pci->apRayObject, sizeof(pci->apRayObject));
         pRet = pci->apRayObject[0];
         if (papBound) {
            papBound[0].Copy (&pRet->m_apBound[0]);
            papBound[1].Copy (&pRet->m_apBound[1]);
         }
         LeaveCriticalSection (&m_RTCLONECritSec);
         return pRet;
      }

   // if get here then didn't find
   // if not create, then exit now
   if (!fCreateIfNotExist) {
      LeaveCriticalSection (&m_RTCLONECritSec);
      return NULL;
   }

   // else, fill in a temporary structure
   RTCLONEINFO ci;
   memset (&ci, 0, sizeof(ci));
   ci.gCode = *pgCode;
   ci.gSub = *pgSub;
   ci.fRecurse = TRUE;
   ci.dwCount = 1;
   for (i = 0; i < RTCLONE_LOD; i++)
      ci.apRayObject[i] = new CRayObject (m_dwRenderShard, NULL, this, TRUE);
      // assuming will work
   
   m_lRTCLONEINFO.Add (&ci);
   LeaveCriticalSection (&m_RTCLONECritSec);


   // else, need to create it
   PCObjectClone pClone;
   pClone = ObjectCloneGet (m_dwRenderShard, pgCode, pgSub);

   // calculate different LOD
   if (pClone) for (i = 0; i < RTCLONE_LOD; i++) {

      // store away the old object and socket
      PCRayObject pOldRay;
      CMatrix mOldMatrix;
      BOOL fOldCloneRender;
      fp fOldDetail;
      pOldRay = m_pRayCur;
      fOldCloneRender = m_fCloneRender;
      fOldDetail = m_fDetailForThis;
      mOldMatrix.Copy (&m_mRenderSocket);
      m_pRayCur = ci.apRayObject[i];
      m_mRenderSocket.Identity();
      m_fCloneRender = TRUE;
      m_fDetailForThis = 0;

      if (i) {
         CPoint pSize;
         pSize.Subtract (&ci.apRayObject[0]->m_apBound[0], &ci.apRayObject[0]->m_apBound[1]);
         fp fDist = pSize.Length() * RTCLONE_LODLOWEST;
         fDist = max(fDist, CLOSE);

         fp fScale = (fp)(RTCLONE_LOD - i - 1);
         fScale = pow ((fp)(1.0 / RTCLONE_LODSCALE), fScale);
         fDist *= fScale;

         m_fDetailForThis = fDist;
      }

      // render
      OBJECTRENDER or;
      memset (&or, 0, sizeof(or));
      or.dwReason = m_fForShadows ? ORREASON_SHADOWS :
         (m_fFinalRender ? ORREASON_FINAL : ORREASON_WORKING);
      or.dwSpecial = m_dwSpecial;
      or.dwShow = m_dwRenderShow;
      or.pRS = this;
      pClone->Render (&or, m_dwCloneID++); 
         // NOTE: May end up with different object number for clones than traditioanl render...

      // make sure to recalc the bounding box
      ci.apRayObject[i]->CalcBoundBox();


      // restore the old object and matrix
      m_pRayCur = pOldRay;
      m_mRenderSocket.Copy (&mOldMatrix);
      m_fCloneRender = fOldCloneRender;
      m_fDetailForThis = fOldDetail;
   } // i

   // release clone
   pClone->Release();

   // fill in info again
   EnterCriticalSection (&m_RTCLONECritSec);
   pci = (PRTCLONEINFO) m_lRTCLONEINFO.Get(0);
   for (i = 0; i < m_lRTCLONEINFO.Num(); i++, pci++)
      if (IsEqualGUID(pci->gCode, *pgCode) && IsEqualGUID(pci->gSub, *pgSub)) {
         pci->fRecurse = FALSE;
         break;
      }
   LeaveCriticalSection (&m_RTCLONECritSec);

   // done
   if (ppRayObject)
      memcpy (ppRayObject, ci.apRayObject, sizeof(ci.apRayObject));
   pRet = pci->apRayObject[0];
   if (papBound) {
      papBound[0].Copy (&pRet->m_apBound[0]);
      papBound[1].Copy (&pRet->m_apBound[1]);
   }
   return pRet;
}


/***************************************************************************************
CRenderTraditional::CloneRelease - Releases a clone. Must be called for every call to CloneGet()
after the object is finished being used.


inputs
   GUID           *pgCode - Code ID
   GUID           *pcSub - Sub ID
returns
   BOOL - TRUE if success
*/
BOOL CRenderTraditional::CloneRelease (GUID *pgCode, GUID *pgSub)
{
   // BUGFIX - Dont call this since release all the clones
   // at the end of rendering
   return TRUE;

#if 0
   not tested
   EnterCriticalSection (&m_RTCLONECritSec);
   // find the object
   PRTCLONEINFO pcc = (PRTCLONEINFO) m_lRTCLONEINFO.Get(0);
   DWORD i, j;
   for (i = 0; i < m_lRTCLONEINFO.Num(); i++, pcc++) {
      if (IsEqualGUID (pcc->gSub, *pgSub) && IsEqualGUID (pcc->gCode, *pgCode))
         break;
   }
   if (i >= m_lRTCLONEINFO.Num()) {
      LeaveCriticalSection (&m_RTCLONECritSec);
      return FALSE;
   }

   if (pcc->dwCount >= 1)
      pcc->dwCount--;

   if (pcc->dwCount >= 1) {
      LeaveCriticalSection (&m_RTCLONECritSec);
      return TRUE;   // nothing to do since still there
   }

   // else, delete the object
   RTCLONEINFO ci = *pcc;
   memset (pcc->apRayObject, 0, sizeof(pcc->apRayObject));
   LeaveCriticalSection (&m_RTCLONECritSec);
   for (i = 0; i < RTCLONE_LOD; i++)
      delete ci.apRayObject[i];
   EnterCriticalSection (&m_RTCLONECritSec);

   // now, because deleting the object may have deleted other clones, go through the
   // entire clone list and remove all the empty entries
   pcc = (PRTCLONEINFO) m_lRTCLONEINFO.Get(0);
   for (i = m_lRTCLONEINFO.Num()-1; i < m_lRTCLONEINFO.Num(); i--) {
      if (pcc[i].dwCount)
         continue;

      for (j = 0; j < RTCLONE_LOD; j++)
         if (pcc[i].apRayObject[j])
            break;
      if (j < RTCLONE_LOD)
         continue;

      // else, not used, so delete
      m_lRTCLONEINFO.Remove (i);
      pcc = (PRTCLONEINFO) m_lRTCLONEINFO.Get(0); // since removal may have realloced
   } // i

   LeaveCriticalSection (&m_RTCLONECritSec);

   return TRUE;
#endif // 0
}


/***************************************************************************************
CRenderTraditional::CloneRenderObject - Renders a specific clone object
in one of the rendersthreads.

NOTE: Modified m_adwWantClipThis[dwThread]

inputs
   DWORD          dwThread - 1..MAXRAYTHREAD-1
   PCPoint        papBound - Array of two points used for bounding box
   PCRayObject    *papRayObject - Array of RTCLONE_LOD objects for different levels of details
   PCMatrix       pm - Matrix used to affect the clone. Convert to world space
   RECT           *prChanged - If not NULL, this will be filled in with a rectangle (in pixels) for
                  what has changed. If nothing is known to have changed, this fills in with 0,0,0,0
returns
   BOOL - TRUE if success
*/
BOOL CRenderTraditional::CloneRenderObject (DWORD dwThread, PCPoint papBound, PCRayObject *papRayObject, PCMatrix pm,
                                            RECT *prChanged)
{
   if (prChanged)
      memset (prChanged, 0, sizeof(*prChanged));

   // see if object exists, etc

   // see if would be clipped
   // CMatrix mCTM, mViewToWorld;
   ROINFO ri;
   memset (&ri, 0, sizeof(ri));
   //m_aRenderMatrix[dwThread].CTMGet(&mCTM);
   //m_WorldToView.Invert4 (&mViewToWorld);
   //mCTM.MultiplyRight (&mViewToWorld);
   if (!CalcDetail (dwThread, pm, papBound+0, papBound+1, &ri)) {
      // m_aRenderMatrix[dwThread].Pop();
      return TRUE;
   }

   if (ri.fTrivialClip) {
      // m_aRenderMatrix[dwThread].Pop();
      return TRUE;
   }

   if (ri.fVerySmall) {
      // m_aRenderMatrix[dwThread].Pop();
      return TRUE;
   }


   // do trivial removal - if this object has no chance of being drawn then don't bother
   if (ri.fCanUseZTest) {
      RECT r;
      r.left = (int) floor (ri.aPoint[0].p[0]);
      r.right = (int) ceil (ri.aPoint[1].p[0]);
      r.top = (int) floor (ri.aPoint[0].p[1]);
      r.bottom = (int) ceil (ri.aPoint[1].p[1]);
      if (IsCompletelyCovered (&r, ri.fZNear)) {
         // m_aRenderMatrix[dwThread].Pop();
         return TRUE;
      }

      if (prChanged)
         *prChanged = r;
   }


   // select level-of-detail
   DWORD dwLOD = 0;
   if (ri.fDetail > CLOSE) {
      CPoint pSize;
      pSize.Subtract (&papRayObject[0]->m_apBound[0], &papRayObject[0]->m_apBound[1]);
      fp fDist = pSize.Length() * RTCLONE_LODLOWEST;
      fDist = max(fDist, CLOSE);

      // fDetail  = fDist * pow (1.0 / RTCLONE_LODSCALE, (fp)(RTCLONE_LOD - i - 1));
      // fDetail / fDist = pow (1.0 / RTCLONE_LODSCALE, (fp)(RTCLONE_LOD - i - 1));
      // fDetail / fDist = exp(log(pow (1.0 / RTCLONE_LODSCALE, (fp)(RTCLONE_LOD - i - 1))));
      // fDetail / fDist = exp(log(1.0 / RTCLONE_LODSCALE) * (fp)(RTCLONE_LOD - i - 1))));
      // log(fDetail / fDist) = log(1.0 / RTCLONE_LODSCALE) * (fp)(RTCLONE_LOD - i - 1)
      // log(fDetail / fDist) / log(1.0 / RTCLONE_LODSCALE) = RTCLONELOD - i - 1
      // i = RTCLONE_LOD - log(fDetail / fDist) / log(1.0 / RTCLONE_LODSCALE) - 1
      fp fi = (fp)RTCLONE_LOD - log(ri.fDetail / fDist) / log(1.0 / RTCLONE_LODSCALE) - 1.0;
      fi = floor(fi + 0.5);
      fi = max(fi, 0);
      fi = min(fi, (fp)RTCLONE_LOD - 1);
      dwLOD = (DWORD)fi;

   }

   // remember the clip planes
   m_adwWantClipThis[dwThread] = ri.dwClipPlanes;

   // render this object
   BOOL fRet = CloneRenderRayObject (dwThread, papRayObject[dwLOD], pm);

   // restore
   // m_aRenderMatrix[dwThread].Pop();
   return fRet;
}


/***************************************************************************************
CRenderTraditional::CloneRenderRayObject - Renders a CRayObject. Called by CloneRenderObject

inputs
   DWORD          dwThread - 1..MAXRAYTHREAD*RTTHREADSHACLE-1
   PCRayObject    pObject - Object to render
   PCMatrix       pm - Converts from the object space to world space
returns
   BOOL - TRUE if success
*/
BOOL CRenderTraditional::CloneRenderRayObject (DWORD dwThread, PCRayObject pObject, PCMatrix pm)
{
   BOOL fRet = TRUE;
   DWORD i;

   if (pObject->m_dwNumTri || pObject->m_dwNumQuad) {
      // figure out how much will need
      POLYRENDERINFO pi;
      memset (&pi, 0, sizeof(pi));
      pi.fBonesAlreadyApplied = TRUE; // has to be this case, I think
      pi.fAlreadyNormalized = TRUE;

      DWORD dwNeed =
         pObject->m_dwNumSurfaces * sizeof(RENDERSURFACE) +
         pObject->m_dwNumPoints * sizeof(CPoint) +
         pObject->m_dwNumNormals * sizeof(CPoint) +
         pObject->m_dwNumTextures * sizeof(TEXTPOINT5) +
         pObject->m_dwNumColors * sizeof(COLORREF) +
         pObject->m_dwNumVertices * sizeof(VERTEX) +
         pObject->m_dwNumTri * (sizeof(POLYDESCRIPT) + 3 * sizeof(DWORD)) +
         pObject->m_dwNumQuad * (sizeof(POLYDESCRIPT) + 4 * sizeof(DWORD));
      if (!m_amemCloneScratch[dwThread].Required (dwNeed))
         return FALSE;

      pi.dwNumSurfaces = pObject->m_dwNumSurfaces;
      pi.dwNumColors = pObject->m_dwNumColors;
      pi.dwNumPoints = pObject->m_dwNumPoints;
      pi.dwNumNormals = pObject->m_dwNumNormals;
      pi.dwNumPolygons = pObject->m_dwNumTri + pObject->m_dwNumQuad;
      pi.dwNumTextures = pObject->m_dwNumTextures;
      pi.dwNumVertices = pObject->m_dwNumVertices;

      pi.paSurfaces = (PRENDERSURFACE) m_amemCloneScratch[dwThread].p;
      pi.paPoints = (PCPoint) (pi.paSurfaces + pi.dwNumSurfaces);
      pi.paNormals = pi.paPoints + pi.dwNumPoints;
      pi.paTextures = (PTEXTPOINT5) (pi.paNormals + pi.dwNumNormals);
      pi.paColors = (COLORREF*) (pi.paTextures + pi.dwNumTextures);
      pi.paVertices = (PVERTEX) (pi.paColors + pi.dwNumColors);
      pi.paPolygons = (PPOLYDESCRIPT) (pi.paVertices + pi.dwNumVertices);

      memcpy (pi.paSurfaces, pObject->m_pRENDERSURFACE, pObject->m_dwNumSurfaces * sizeof(RENDERSURFACE));
      for (i = 0; i < pi.dwNumPoints; i++) {
         pi.paPoints[i].p[0] = pObject->m_pPoints[i].p[0];
         pi.paPoints[i].p[1] = pObject->m_pPoints[i].p[1];
         pi.paPoints[i].p[2] = pObject->m_pPoints[i].p[2];
         pi.paPoints[i].p[3] = 1.0;
      }
      for (i = 0; i < pi.dwNumNormals; i++) {
         DWORD dw = pObject->m_pNormals[i].dw;

         pi.paNormals[i].p[0] = (fp)(short) (WORD) ((dw & 0x3ff) << 6) / 32767.0;
         pi.paNormals[i].p[1] = (fp)(short) (WORD) (((dw >> 10) & 0x3ff) << 6) / 32767.0;
         pi.paNormals[i].p[2] = (fp)(short) (WORD) (((dw >> 20) & 0x3ff) << 6) / 32767.0;
         pi.paNormals[i].p[3] = 1.0;
      }
      memcpy (pi.paTextures, pObject->m_pTextures, pObject->m_dwNumTextures * sizeof(TEXTPOINT5));
      memcpy (pi.paColors, pObject->m_pColors, pObject->m_dwNumColors * sizeof(COLORREF));
      PPOLYDESCRIPT pCur = pi.paPolygons;
      if (pObject->m_dwPolyDWORD) {
         PCRTTRIDWORD pt = (PCRTTRIDWORD) pObject->m_pTri;
         PCRTQUADDWORD pq = (PCRTQUADDWORD) pObject->m_pQuad;
         PCRTVERTEXDWORD pv = (PCRTVERTEXDWORD) pObject->m_pVertices;
         
         for (i = 0; i < pi.dwNumVertices; i++) {
            pi.paVertices[i].dwColor = pv[i].dwColor;
            pi.paVertices[i].dwNormal = pv[i].dwNormal;
            pi.paVertices[i].dwPoint = pv[i].dwPoint;
            pi.paVertices[i].dwTexture = pv[i].dwTexture;
         } // i

         for (i = 0; i < pObject->m_dwNumTri; i++, pt++, pCur = (PPOLYDESCRIPT)((PBYTE)pCur+(sizeof(POLYDESCRIPT)+3*sizeof(DWORD))) ) {
            pCur->dwIDPart = pt->dwIDPart;
            pCur->dwSurface = pt->dwSurface;
            pCur->fCanBackfaceCull = pt->fCanBackfaceCull;
            pCur->wNumVertices = 3;

            ((DWORD*)(pCur + 1))[0] = pt->adwVertex[0];
            ((DWORD*)(pCur + 1))[1] = pt->adwVertex[1];
            ((DWORD*)(pCur + 1))[2] = pt->adwVertex[2];
         } // i

         for (i = 0; i < pObject->m_dwNumQuad; i++, pq++, pCur = (PPOLYDESCRIPT)((PBYTE)pCur+(sizeof(POLYDESCRIPT)+4*sizeof(DWORD))) ) {
            pCur->dwIDPart = pq->dwIDPart;
            pCur->dwSurface = pq->dwSurface;
            pCur->fCanBackfaceCull = pq->fCanBackfaceCull;
            pCur->wNumVertices = 4;

            ((DWORD*)(pCur + 1))[0] = pq->adwVertex[0];
            ((DWORD*)(pCur + 1))[1] = pq->adwVertex[1];
            ((DWORD*)(pCur + 1))[2] = pq->adwVertex[2];
            ((DWORD*)(pCur + 1))[3] = pq->adwVertex[3];
         } // i
      }
      else {
         PCRTTRIWORD pt = (PCRTTRIWORD) pObject->m_pTri;
         PCRTQUADWORD pq = (PCRTQUADWORD) pObject->m_pQuad;
         PCRTVERTEXWORD pv = (PCRTVERTEXWORD) pObject->m_pVertices;

         for (i = 0; i < pi.dwNumVertices; i++) {
            pi.paVertices[i].dwColor = pv[i].dwColor;
            pi.paVertices[i].dwNormal = pv[i].dwNormal;
            pi.paVertices[i].dwPoint = pv[i].dwPoint;
            pi.paVertices[i].dwTexture = pv[i].dwTexture;
         } // i

         for (i = 0; i < pObject->m_dwNumTri; i++, pt++, pCur = (PPOLYDESCRIPT)((PBYTE)pCur+(sizeof(POLYDESCRIPT)+3*sizeof(DWORD))) ) {
            pCur->dwIDPart = pt->dwIDPart;
            pCur->dwSurface = pt->dwSurface;
            pCur->fCanBackfaceCull = pt->fCanBackfaceCull;
            pCur->wNumVertices = 3;

            ((DWORD*)(pCur + 1))[0] = pt->adwVertex[0];
            ((DWORD*)(pCur + 1))[1] = pt->adwVertex[1];
            ((DWORD*)(pCur + 1))[2] = pt->adwVertex[2];
         } // i

         for (i = 0; i < pObject->m_dwNumQuad; i++, pq++, pCur = (PPOLYDESCRIPT)((PBYTE)pCur+(sizeof(POLYDESCRIPT)+4*sizeof(DWORD))) ) {
            pCur->dwIDPart = pq->dwIDPart;
            pCur->dwSurface = pq->dwSurface;
            pCur->fCanBackfaceCull = pq->fCanBackfaceCull;
            pCur->wNumVertices = 4;

            ((DWORD*)(pCur + 1))[0] = pq->adwVertex[0];
            ((DWORD*)(pCur + 1))[1] = pq->adwVertex[1];
            ((DWORD*)(pCur + 1))[2] = pq->adwVertex[2];
            ((DWORD*)(pCur + 1))[3] = pq->adwVertex[3];
         } // i

      }

      // multiply by the matrix
      m_aRenderMatrix[dwThread].Push();
      m_aRenderMatrix[dwThread].Multiply (pm);

      // render all the polygons
      if (m_pImageShader)
         PolyRenderShadows (dwThread, &pi);
      else
         PolyRenderNoShadows (dwThread, &pi);
   
      m_aRenderMatrix[dwThread].Pop();

   } // if have triangles or quads

   // need to do sub-objects
   PCRTSUBOBJECT pso = (PCRTSUBOBJECT)pObject->m_lCRTSUBOBJECT.Get(0);
   CMatrix mMod;
   for (i = 0; (i < pObject->m_lCRTSUBOBJECT.Num()) && fRet; i++, pso++) {
      mMod.Copy (pm);
      if (!pso->fMatrixIdent)
         mMod.MultiplyLeft (&pso->mMatrix);

      if (!CloneRenderRayObject (dwThread, pso->pObject, &mMod))
         fRet = FALSE;
   } // i

   // render other clones
   if (pObject->m_pCloneObject) {
      CPoint apBound[2];
      PCRayObject    apRayObject[RTCLONE_LOD];

      if (!CloneGet (&pObject->m_gCloneCode, &pObject->m_gCloneSub,
         FALSE, &apBound[0], &apRayObject[0]))
         return FALSE;

      PCRTCLONE pcl = (PCRTCLONE) pObject->m_lCRTCLONE.Get(0);
      for (i = 0; fRet && (i < pObject->m_lCRTCLONE.Num()); i++, pcl++) {
         mMod.Copy (pm);
         mMod.MultiplyLeft (&pcl->mMatrix);
         fRet &= CloneRenderObject (dwThread, &apBound[0], &apRayObject[0], &mMod, NULL); 
      }
   }

   return fRet;
}



// FUTURERELEASE - If have surface all one color, but bumps, it comes out completely plain
// when in shadow - just ambient light. Might need to a) fix the specularity light
// to handle ambient, and b) hack so even get some directionlaity under ambient

// FUTURERELEASE - In shadow mode, when look at "camera shot" of eage eye, the
// bump maps cause light to appear on the edge of one wall, were there
// probably shouldn't be light.

// BUGBUG - If create a log-wall texture and place on a flat surface (such as ground,
// or top of object) then render in shadows mode with infinite light at 7AM or 7PM,
// don't seem to get shadowing correct on texture

// BUGBUG - At some point might want to rebase the dll's so they load faster

// BUGBUG - Make field of view vertical so that when create as widescreen get what
// expect

// BUGBUG - May want to make an optimization that remebers what objects are visible
// (by looking through all the pixels). Then, the next time that render, draw those
// objects first (or at least encourage them to be drawn first). This should speed
// up because the image doesn't change much from one go to another and later
// BB calls will quickly eliminate. Don't do for selected-only render

// BUGBUG - the bounding box seems to be drawn in the wrong location, particularly
// when I resize a polymesh object in phase I. I suspect this appeared after I
// added multithreading
