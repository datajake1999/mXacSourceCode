/*************************************************************************************
CRenderScene.cpp - Code for UI to modify a scene to be rendered, and for rendering it.

begun 13/3/04 by Mike Rozak.
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





#define SCALEPREVIEW          0.5      // resolution of preview

// RSATTRIB - Structure for modifying attribute
typedef struct {
   GUID              gObject;    // object it's affecting, used for m_fLoadFromFile
   DWORD             dwObject;   // object index, used for !m_fLoadFromFile
   WCHAR             szAttrib[64];  // attribute name
   fp                fValue;     // value it's modified to
} RSATTRIB, *PRSATTRIB;

// RSOBJECT - Structure for object
typedef struct {
   GUID              gMajor;     // major ID
   GUID              gMinor;     // minor ID
   CPoint            pLoc;       // location
   CPoint            pRot;       // rotation
} RSOBJECT, *PRSOBJECT;

// RSCOLOR - Stores surface information
typedef struct {
   GUID              gObject;    // object it's affecting, used for m_fLoadFromFile
   DWORD             dwObject;   // object index, used for !m_fLoadFromFile
   PCObjectSurface   pSurf;      // surface
} RSCOLOR, *PRSCOLOR;

static PWSTR gpszEnabledFalse = L" enabled=false ";

/*************************************************************************************
HotSpotColor - Given an index return a color.

inputs
   DWORD       dwIndex
returns
   COLORREF - color
*/
DLLEXPORT COLORREF HotSpotColor (DWORD dwIndex)
{
   COLORREF ac[] = {
      RGB(0xff,0,0), RGB(0,0xff,0), RGB(0,0,0xff),
      RGB(0xff,0,0xff), RGB(0xff, 0xff,0), RGB(0, 0xff, 0xff),
      RGB(0x80,0,0), RGB(0,0x80,0), RGB(0,0,0x80),
      RGB(0x80,0,0x80), RGB(0x80, 0x80,0), RGB(0, 0x80, 0x80)};

   dwIndex = dwIndex % (sizeof(ac) / sizeof(COLORREF));
   return ac[dwIndex];
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
PWSTR RenderSceneTabs (DWORD dwTab, DWORD dwNum, PWSTR *ppsz, PWSTR *ppszHelp, DWORD *padwID,
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


/*************************************************************************************
RenderSceneAspectToPixels - Determines with width and height that an image will
be given it's aspect ration setting, m_dwAspect.

inputs
   DWORD             dwAspect - Aspect setting.0=2:1, 1=16:9, 2=3:2, 3=1:1,
                                4=2:3, 5=9:16, 6=1:2, 10=360 degreee
   fp                fScale - 1.0 = normal scale, 2.0 = 2x res (4x pixels), etc.
   DWORD             *pdwWidth - Filled with the width, in pixels
   DWORD             *pdwHeight - Filled with the height, in pixels
returns
   none
*/
void RenderSceneAspectToPixels (DWORD dwAspect, fp fScale, DWORD *pdwWidth, DWORD *pdwHeight)
{
   fp fWidth = 1, fHeight = 1;
   fScale *= 1024;   // a default sized image will be 1024 in the longest dimension

   switch (dwAspect) {
   case 0:  // 2:1
      fHeight /= 2.0;
      break;
   case 1:  // 16:9
      fHeight /= (16.0 / 9.0);
      break;
   default:
   case 2:  // 3:2
      fHeight /= (3.0 / 2.0);
      break;
   case 3:  // 1:1
      break;
   case 4:  // 2:3
      fWidth /= (3.0 / 2.0);
      break;
   case 5:  // 9:16
      fWidth /= (16.0 / 9.0);
      break;
   case 6:  // 1:2
      fWidth /= 2.0;
      break;
   case 10: // 360 degree
      fWidth *= 4;
      fHeight *= 2;
      break;
   }

   *pdwWidth = (DWORD)(fScale * fWidth);
   *pdwHeight = (DWORD)(fScale * fHeight);
}


/*****************************************************************************
RenderSceneHotSpotToImage - This takes a hot-spot location for a scene, which
is normalized to a 1000 x 1000 grid, and converts it to the pixels in the image.
The conversion is in place.

inputs
   RECT        *pr - Initially contains 1000x1000 coords, but converted to image
   DWORD       dwWidth - Width of image
   DWORD       dwHeight - Height of image
returns
   none
*/
DLLEXPORT void RenderSceneHotSpotToImage (RECT *pr, DWORD dwWidth, DWORD dwHeight)
{
   pr->left = (pr->left * (int)dwWidth / 1000);
   pr->right = (pr->right * (int)dwWidth / 1000);
   pr->top = (pr->top * (int)dwHeight / 1000);
   pr->bottom = (pr->bottom * (int)dwHeight / 1000);
}


/*****************************************************************************
RenderSceneHotSpotFromImage - This takes an image location and converts
it to a normalize hot-spot location, which
is normalized to a 1000 x 1000 grid.
The conversion is in place.

inputs
   RECT        *pr - Initially contains image coords, converted to hotspot 1000x1000 coords
   DWORD       dwWidth - Width of image
   DWORD       dwHeight - Height of image
returns
   none
*/
DLLEXPORT void RenderSceneHotSpotFromImage (RECT *pr, DWORD dwWidth, DWORD dwHeight)
{
   pr->left = pr->left * 1000 / (int)dwWidth;
   pr->right = pr->right * 1000 / (int)dwWidth;
   pr->top = pr->top * 1000 / (int)dwHeight;
   pr->bottom = pr->bottom * 1000 / (int)dwHeight;
}

/*************************************************************************************
CRenderScene::Constructor and destructor
*/
CRenderScene::CRenderScene (void)
{
   m_szFile[0] = 0;
   m_dwAspect = 2;
   m_dwQuality = 5;
   m_dwAnti = 1;
   m_pRender = NULL;
   m_dwTab = 0;
   m_lIHOTSPOT.Init (sizeof(IHOTSPOT));
   m_lRSATTRIB.Init (sizeof(RSATTRIB));
   m_lRSOBJECT.Init (sizeof(RSOBJECT));
   m_lRSCOLOR.Init (sizeof(RSCOLOR));
   m_iVScroll = 0;
   m_gScene = GUID_NULL;
   m_szSceneBookmark[0] = 0;
   m_fSceneTime = 0;

   m_gCamera = GUID_NULL;
   m_fCameraOverride = FALSE;
   m_pCameraXYZ.Zero();
   m_pCameraXYZ.p[1] = -10;   // start out 10m to south, for !m_fLoadFromFile
   m_pCameraRot.Zero();
   m_fCameraFOV = PI/2;
   m_fCameraExposure = 1;
   m_fMoveDist = 1;  // 1 meter move default

   m_fLoadFromFile = TRUE; // may be modified by caller
      // Usually set based on whether matches MIF3DScene or MIF3DObjects
}

CRenderScene::~CRenderScene (void)
{
   if (m_pRender)
      delete m_pRender;

   // free up hotspots
   DWORD i;
   PIHOTSPOT phs = (PIHOTSPOT)m_lIHOTSPOT.Get(0);
   for (i = 0; i < m_lIHOTSPOT.Num(); i++, phs++)
      if (phs->ps)
         phs->ps->Release();
   m_lIHOTSPOT.Clear();

   // free up the colors
   PRSCOLOR prc = (PRSCOLOR)m_lRSCOLOR.Get(0);
   for (i = 0; i < m_lRSCOLOR.Num(); i++, prc++)
      delete prc->pSurf;
   m_lRSCOLOR.Clear();
}


/*************************************************************************************
CRenderScene::MMLTo - Standard API
*/
static PWSTR gpszFile = L"File";
static PWSTR gpszAspect = L"Aspect";
static PWSTR gpszQuality = L"Quality";
static PWSTR gpszAnti = L"Anti";
static PWSTR gpszHotSpot = L"HotSpot";
static PWSTR gpszLeft = L"Left";
static PWSTR gpszRight = L"Right";
static PWSTR gpszTop = L"Top";
static PWSTR gpszBottom = L"Bottom";
static PWSTR gpszCursor = L"Cursor";
static PWSTR gpszMessage = L"Message";
static PWSTR gpszLangID = L"LangID";
static PWSTR gpszScene = L"Scene";
static PWSTR gpszSceneTime = L"SceneTime";
static PWSTR gpszSceneBookmark = L"SceneBookmark";
static PWSTR gpszCameraOverride = L"CameraOverride";
static PWSTR gpszCamera = L"Camera";
static PWSTR gpszCameraXYZ = L"CameraXYZ";
static PWSTR gpszCameraRot = L"CameraRot";
static PWSTR gpszCameraFOV = L"CameraFOV";
static PWSTR gpszCameraExposure = L"CameraExposure";
static PWSTR gpszObject = L"Object";
static PWSTR gpszAttrib = L"Attrib";
static PWSTR gpszValue = L"Value";
// static PWSTR gpszObject = L"Object";
static PWSTR gpszMajor = L"Major";
static PWSTR gpszMinor = L"Minor";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszRot = L"Rot";
static PWSTR gpszColor = L"Color";

PCMMLNode2 CRenderScene::MMLTo (void)
{
   DWORD i;
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (m_fLoadFromFile ? MIF3DScene() : MIF3DObjects());

   // NOTE: Not saving m_fLoadFromFile

   if (m_fLoadFromFile && !IsEqualGUID (m_gCamera, GUID_NULL))
      MMLValueSet (pNode, gpszCamera, (PBYTE)&m_gCamera, sizeof(m_gCamera));
   if (!m_fLoadFromFile || m_fCameraOverride) {
      MMLValueSet (pNode, gpszCameraOverride, (int)m_fCameraOverride);
      MMLValueSet (pNode, gpszCameraXYZ, &m_pCameraXYZ);
      MMLValueSet (pNode, gpszCameraRot, &m_pCameraRot);
      MMLValueSet (pNode, gpszCameraFOV, m_fCameraFOV);
      MMLValueSet (pNode, gpszCameraExposure, m_fCameraExposure);
   }

   if (m_fLoadFromFile && m_szFile[0])
      MMLValueSet (pNode, gpszFile, m_szFile);
   if (m_dwAspect != 2)
      MMLValueSet (pNode ,gpszAspect, (int)m_dwAspect);
   if (m_dwQuality != 5)
      MMLValueSet (pNode, gpszQuality, (int)m_dwQuality);
   if (m_dwAnti != 1)
      MMLValueSet (pNode, gpszAnti, (int)m_dwAnti);

   PWSTR psz;
   if (!IsEqualGUID (m_gScene, GUID_NULL))
      MMLValueSet (pNode, gpszScene, (PBYTE)&m_gScene, sizeof(m_gScene));
   MMLValueSet (pNode, gpszSceneTime, m_fSceneTime);
   if (m_szSceneBookmark[0])
      MMLValueSet (pNode, gpszSceneBookmark, m_szSceneBookmark);

   // write out the hot spots
   PIHOTSPOT ph;
   ph = (PIHOTSPOT)m_lIHOTSPOT.Get(0);
   for (i = 0; i < m_lIHOTSPOT.Num(); i++,ph++) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszHotSpot);

      MMLValueSet (pSub, gpszLeft, ph->rPosn.left);
      MMLValueSet (pSub, gpszRight, ph->rPosn.right);
      MMLValueSet (pSub, gpszTop, ph->rPosn.top);
      MMLValueSet (pSub, gpszBottom, ph->rPosn.bottom);
      MMLValueSet (pSub, gpszCursor, (int) ph->dwCursor);
      MMLValueSet (pSub, gpszLangID, (int) m_lid);
         // NOTE: Setting the default language ID for all of them

      psz = ph->ps->Get();
      if (psz[0])
         MMLValueSet (pSub, gpszMessage, psz);
   } // i

   // write out the attributes
   PRSATTRIB pr = (PRSATTRIB)m_lRSATTRIB.Get(0);
   for (i = 0; i < m_lRSATTRIB.Num(); i++, pr++) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszAttrib);

      if (m_fLoadFromFile)
         MMLValueSet (pSub, gpszObject, (LPBYTE)&pr->gObject, sizeof(pr->gObject));
      else
         MMLValueSet (pSub, gpszObject, (int)pr->dwObject);

      MMLValueSet (pSub, gpszValue, pr->fValue);
      if (pr->szAttrib[0])
         MMLValueSet (pSub, gpszAttrib, pr->szAttrib);
   } // i

   // write out the objects
   PRSOBJECT po = (PRSOBJECT)m_lRSOBJECT.Get(0);
   for (i = 0; i < m_lRSOBJECT.Num(); i++, po++) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszObject);

      MMLValueSet (pSub, gpszMajor, (LPBYTE)&po->gMajor, sizeof(po->gMajor));
      MMLValueSet (pSub, gpszMinor, (LPBYTE)&po->gMinor, sizeof(po->gMinor));
      MMLValueSet (pSub, gpszLoc, &po->pLoc);
      MMLValueSet (pSub, gpszRot, &po->pRot);
   } // i

   // write out the objects
   PRSCOLOR prc = (PRSCOLOR)m_lRSCOLOR.Get(0);
   for (i = 0; i < m_lRSCOLOR.Num(); i++, prc++) {
      PCMMLNode2 pSub = prc->pSurf->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszColor);

      if (m_fLoadFromFile)
         MMLValueSet (pSub, gpszObject, (LPBYTE)&prc->gObject, sizeof(prc->gObject));
      else
         MMLValueSet (pSub, gpszObject, (int)prc->dwObject);

      pNode->ContentAdd (pSub);
   } // i

   return pNode;
}



/*************************************************************************************
CRenderScene:MMLFrom - Standard API
*/
BOOL CRenderScene::MMLFrom (PCMMLNode2 pNode)
{
   // NOTE: m_fLoadFromFile NOT read in. should be set ahead of time

   // free up hotspots
   DWORD i;
   PIHOTSPOT phs = (PIHOTSPOT)m_lIHOTSPOT.Get(0);
   for (i = 0; i < m_lIHOTSPOT.Num(); i++, phs++)
      if (phs->ps)
         phs->ps->Release();
   m_lIHOTSPOT.Clear();

   // free up the colors
   PRSCOLOR prc = (PRSCOLOR)m_lRSCOLOR.Get(0);
   for (i = 0; i < m_lRSCOLOR.Num(); i++, prc++)
      delete prc->pSurf;
   m_lRSCOLOR.Clear();

   PWSTR psz = MMLValueGet (pNode, gpszFile);
   if (psz && m_fLoadFromFile)
      wcscpy (m_szFile, psz);
   else
      m_szFile[0] = 0;

   m_dwAspect = (DWORD) MMLValueGetInt (pNode ,gpszAspect, 2);
   m_dwQuality = (DWORD) MMLValueGetInt (pNode, gpszQuality, 5);
   m_dwAnti = (DWORD) MMLValueGetInt (pNode, gpszAnti, 1);


   m_gCamera = GUID_NULL;
   if (m_fLoadFromFile)
      MMLValueGetBinary (pNode, gpszCamera, (PBYTE)&m_gCamera, sizeof(m_gCamera));
   m_fCameraOverride = (BOOL) MMLValueGetInt (pNode, gpszCameraOverride, !m_fLoadFromFile);
   if (!m_fLoadFromFile || m_fCameraOverride) {
      MMLValueGetPoint (pNode, gpszCameraXYZ, &m_pCameraXYZ);
      MMLValueGetPoint (pNode, gpszCameraRot, &m_pCameraRot);
      m_fCameraFOV = MMLValueGetDouble (pNode, gpszCameraFOV, PI/2);
      m_fCameraExposure = MMLValueGetDouble (pNode, gpszCameraExposure, 1);
   }

   m_gScene = GUID_NULL;
   MMLValueGetBinary (pNode, gpszScene, (PBYTE) &m_gScene, sizeof(m_gScene));
   m_fSceneTime = MMLValueGetDouble (pNode, gpszSceneTime, 0);
   psz = MMLValueGet (pNode, gpszSceneBookmark);
   if (psz && (wcslen(psz)+1 < sizeof(m_szSceneBookmark)/sizeof(WCHAR)))
      wcscpy (m_szSceneBookmark, psz);
   else
      m_szSceneBookmark[0] = 0;

   // fill in the hot spots
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!wcsicmp(psz, gpszHotSpot)) {
         // new hot spot
         IHOTSPOT hs;
         memset (&hs, 0, sizeof(hs));
         hs.dwCursor = (DWORD)MMLValueGetInt (pSub, gpszCursor, 0);
         hs.rPosn.left = MMLValueGetInt (pSub, gpszLeft, 0);
         hs.rPosn.right = MMLValueGetInt (pSub, gpszRight, 0);
         hs.rPosn.top = MMLValueGetInt (pSub, gpszTop, 0);
         hs.rPosn.bottom = MMLValueGetInt (pSub, gpszBottom, 0);
         hs.lid = (LANGID) MMLValueGetInt (pSub, gpszLangID, m_lid);
         m_lid = hs.lid; // in case different

         hs.ps = new CMIFLVarString;
         if (!hs.ps)
            continue;
         psz = MMLValueGet (pSub, gpszMessage);
         hs.ps->Set (psz ? psz : L"");

         // add it
         m_lIHOTSPOT.Add (&hs);
         continue;
      }
      else if (!wcsicmp(psz, gpszAttrib)) {
         // new hot spot
         RSATTRIB rs;
         memset (&rs, 0, sizeof(rs));
         psz = MMLValueGet (pSub, gpszAttrib);
         if (psz)
            wcscpy (rs.szAttrib, psz);
         rs.fValue = MMLValueGetDouble (pSub, gpszValue, 0);

         if (m_fLoadFromFile)
            MMLValueGetBinary (pSub, gpszObject, (LPBYTE)&rs.gObject, sizeof(rs.gObject));
         else
            rs.dwObject = (DWORD) MMLValueGetInt (pSub, gpszObject, 0);

         // add it
         m_lRSATTRIB.Add (&rs);
         continue;
      }
      else if (!wcsicmp(psz, gpszColor)) {
         // new hot spot
         RSCOLOR rc;
         memset (&rc, 0, sizeof(rc));
         rc.pSurf = new CObjectSurface;
         if (!rc.pSurf)
            continue;
         rc.pSurf->MMLFrom (pSub);

         if (m_fLoadFromFile)
            MMLValueGetBinary (pSub, gpszObject, (LPBYTE)&rc.gObject, sizeof(rc.gObject));
         else
            rc.dwObject = (DWORD) MMLValueGetInt (pSub, gpszObject, 0);

         // add it
         m_lRSCOLOR.Add (&rc);
         continue;
      }
      else if (!wcsicmp(psz, gpszObject)) {
         // new object
         RSOBJECT ro;
         memset (&ro, 0, sizeof(ro));

         MMLValueGetBinary (pSub, gpszMajor, (LPBYTE)&ro.gMajor, sizeof(ro.gMajor));
         MMLValueGetBinary (pSub, gpszMinor, (LPBYTE)&ro.gMinor, sizeof(ro.gMinor));
         MMLValueGetPoint (pSub, gpszLoc, &ro.pLoc);
         MMLValueGetPoint (pSub, gpszRot, &ro.pRot);

         // add it
         m_lRSOBJECT.Add (&ro);
         continue;
      }
   } // i

   return TRUE;
}



/*************************************************************************************
CRenderScene::Render - Draws the image.

inputs
   fp          fScale - Amount to scale up/down from the standard image size.
                        1.0 = no change, 2.0 = 2x as large (4x the pixels), etc.
   BOOL        fFinalRender - If TRUE draw this as the final render, if FALSE
                        then use using a test render, which means one step lower quality
                        and no final-render flag sent for rendering
   BOOL        fForceReload - If TRUE then reload the scene even if it has already been loaded
   BOOL        fBlankIfFail - If the image fails to load, and this is TRUE, will create
                        a blank image instead. Otherwise, if fails will return null.
   PCProgressSocket pProgres - Progress bar
returns
   PCImage     pImage - Image. NOTE: If the file can't be loaded the image will be a blank one.
                        It should never return NULL (except out of memory)
*/
PCImage CRenderScene::Render (fp fScale, BOOL fFinalRender, BOOL fForceReload,
                              BOOL fBlankIfFail, PCProgressSocket pProgress)
{
   PCImage pImage = NULL;
   DWORD i;

   //determine the real width and height
   DWORD dwWidth, dwHeight;
   RenderSceneAspectToPixels (m_dwAspect, fScale, &dwWidth, &dwHeight);

   // determine the quality
   DWORD dwQuality = m_dwQuality;
   DWORD dwAnti = max(m_dwAnti, 1);
   if (!fFinalRender) {
      if (dwQuality)
         dwQuality = min(dwQuality-1, 4); // dont allow quality to go too high
      dwAnti = 1;
   }

   // see if should reload
   PCSceneSet pSceneSet = NULL;
   PCWorldSocket pWorld = WorldGet (&pSceneSet);
   PWSTR psz = pWorld ? pWorld->NameGet () : NULL;
   if (m_fLoadFromFile && !psz || !psz[0] || wcsicmp(psz, m_szFile))
      fForceReload = TRUE;
   if (!m_pRender)
      fForceReload = TRUE;

   // reload?
   if (!m_fLoadFromFile) {
      // make sure there's a render object, since sometimes use
      if (!m_pRender)
         m_pRender = new CRenderTraditional; // so have something
      if (!m_pRender)
         goto blankimage;

      M3DFileNew (FALSE);

      PRSOBJECT po = (PRSOBJECT)m_lRSOBJECT.Get(0);
      for (i = 0; i < m_lRSOBJECT.Num(); i++, po++) {
         PCObjectSocket pos = ObjectCFCreate (&po->gMajor, &po->gMinor);
         if (!pos)
            continue;

         // create the matrix
         CMatrix m;
         m.FromXYZLLT (&po->pLoc, po->pRot.p[2], po->pRot.p[0], po->pRot.p[1]);
         pos->ObjectMatrixSet (&m);

         // add it
         pWorld->ObjectAdd (pos, TRUE);
      } // i

      // BUGBUG - eventually add lights

      // progress
      if (pProgress)
         pProgress->Push (0, 1);
   }
   else if (fForceReload) {
      // loading from file

      if (m_pRender)
         delete m_pRender;
      m_pRender = NULL;

      if (pProgress)
         pProgress->Push (0, 0.3);

      BOOL fFailedToLoad;
      BOOL fRet;
      fRet = M3DFileOpen (m_szFile, &fFailedToLoad, pProgress, FALSE, &m_pRender);
      if (pProgress)
         pProgress->Pop();
      if (!fRet)
         goto blankimage;

      if (!m_pRender)
         m_pRender = new CRenderTraditional; // so have something
      if (!m_pRender)
         goto blankimage;
      
      if (pProgress)
         pProgress->Push (0.3, 1);
   }

   // set the scene
   if (m_fLoadFromFile && pSceneSet) {
      DWORD dwIndex = pSceneSet->SceneFind (&m_gScene);
      if (dwIndex == -1)
         dwIndex = 0;   // pick one
      PCScene pScene = pSceneSet->SceneGet (dwIndex);
      if (pScene) {
         // find the bookmark
         if (m_szSceneBookmark[0]) {
            CListFixed lBookmark;
            POSMBOOKMARK pb;
            lBookmark.Init (sizeof(OSMBOOKMARK));
            pScene->BookmarkEnum (&lBookmark, TRUE, TRUE);
            pb = (POSMBOOKMARK) lBookmark.Get(0);

            for (i = 0; i < lBookmark.Num(); i++, pb++)
               if (!wcsicmp(pb->szName, m_szSceneBookmark)) {
                  m_fSceneTime = pb->fStart;
                  break;
               }
         }// if bookmark

         pSceneSet->StateSet (pScene, m_fSceneTime);
      }
      else {
         m_gScene = GUID_NULL;   // reset since not there anymore
         m_szSceneBookmark[0] = 0;
      }
   }

   // set all the attributes
   PRSATTRIB pr = (PRSATTRIB) m_lRSATTRIB.Get(0);
   ATTRIBVAL av;
   memset (&av,0 ,sizeof(av));
   for (i = 0; i < m_lRSATTRIB.Num(); i++, pr++) {
      // if !m_fLoadFromFile then do something different
      PCObjectSocket pos;
      if (m_fLoadFromFile)
         pos = pWorld->ObjectGet(pWorld->ObjectFind(&pr->gObject));
      else
         pos = pWorld->ObjectGet(pr->dwObject);

      if (!pos)
         continue;

      av.fValue = pr->fValue;
      wcscpy (av.szName, pr->szAttrib);
      pos->AttribSetGroup (1, &av);
   } // i

   // BUGBUG - set all the colors

   // figure out where the camera is
   BOOL fUseOverride = !m_fLoadFromFile;
   SetRenderCamera (m_pRender);
   if (m_fCameraOverride || !IsEqualGUID(m_gCamera, GUID_NULL))
      fUseOverride = TRUE;

   // render
   if (dwQuality >= 6) {
      // ray tracing
      CRenderRay cRay;
      CFImage cImage;

      if (!cImage.Init (dwWidth, dwHeight)) {
         if (pProgress) pProgress->Pop();
         goto blankimage;
      }
      pImage = new CImage;
      if (!pImage) {
         if (pProgress) pProgress->Pop();
         goto blankimage;
      }
      if (!pImage->Init (dwWidth, dwHeight)) {
         if (pProgress) pProgress->Pop();
         goto blankimage;
      }

      cRay.CWorldSet (pWorld);
      cRay.CFImageSet (&cImage);

      if (fUseOverride) {
         cRay.ExposureSet (CM_LUMENSSUN / exp(m_fCameraExposure));
         cRay.CameraPerspWalkthrough (&m_pCameraXYZ, m_pCameraRot.p[2], m_pCameraRot.p[0], m_pCameraRot.p[1], m_fCameraFOV);
      }
      else {
         DWORD dwCamera;
         CPoint pCenter, pRot, pTrans;
         fp fScale;
         dwCamera = m_pRender->CameraModelGet ();
         switch (dwCamera) {
         case CAMERAMODEL_FLAT:
            m_pRender->CameraFlatGet (&pCenter, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale, &pTrans.p[0], &pTrans.p[1]);
            cRay.CameraFlat (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1], fScale, pTrans.p[0], pTrans.p[1]);
            break;
         case CAMERAMODEL_PERSPWALKTHROUGH:
            m_pRender->CameraPerspWalkthroughGet (&pTrans, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale);
            cRay.CameraPerspWalkthrough (&pTrans, pRot.p[2], pRot.p[0], pRot.p[1], fScale);
            break;
         case CAMERAMODEL_PERSPOBJECT:
            m_pRender->CameraPerspObjectGet (&pTrans, &pCenter,  &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale);
            cRay.CameraPerspObject (&pTrans, &pCenter,  pRot.p[2], pRot.p[0], pRot.p[1], fScale);
            break;
         default:
            return FALSE;  // error
         }
         cRay.ExposureSet (m_pRender->ExposureGet());
      }
      cRay.AmbientExtraSet (m_pRender->AmbientExtraGet());
      cRay.BackgroundSet (m_pRender->BackgroundGet());
      cRay.RenderShowSet ( (DWORD)-1 & ~(RENDERSHOW_VIEWCAMERA | RENDERSHOW_ANIMCAMERA));

      cRay.QuickSetRay (dwQuality - 6, 1, (dwAnti > 2) ? (dwAnti - 1) : 1);
      cRay.m_fAntiEdge = (dwAnti >= 2);

      cRay.Render (0, pProgress);

      // convert to image
      DWORD dwMax = dwWidth * dwHeight;
      PIMAGEPIXEL pip = pImage->Pixel(0,0);
      PFIMAGEPIXEL pfp = cImage.Pixel (0, 0);
      for (i = 0; i < dwMax; i++, pip++, pfp++) {
         pip->wRed = (WORD)max(min(pfp->fRed, (float)0xffff),0);
         pip->wGreen = (WORD)max(min(pfp->fGreen, (float)0xffff),0);
         pip->wBlue = (WORD)max(min(pfp->fBlue, (float)0xffff),0);
         pip->dwID = pfp->dwID;
         pip->dwIDPart = pfp->dwIDPart;
         pip->fZ = pfp->fZ;
      }
   }
   else {
      // traiditional
      CRenderTraditional cRender;
      pImage = new CImage;
      if (!pImage) {
         if (pProgress) pProgress->Pop();
         goto blankimage;
      }
      if (!pImage->Init (dwWidth * dwAnti, dwHeight * dwAnti)) {
         if (pProgress) pProgress->Pop();
         goto blankimage;
      }

      if (m_pRender)
         m_pRender->CloneTo (&cRender);
      cRender.CWorldSet (pWorld);
      cRender.CImageSet (pImage);
      ::QualityApply (dwQuality, &cRender);
      cRender.m_fFinalRender = fFinalRender;
      cRender.RenderShowSet ( cRender.RenderShowGet() & ~(RENDERSHOW_VIEWCAMERA | RENDERSHOW_ANIMCAMERA));
         // dont show the cameras
      if (fUseOverride) {
         cRender.ExposureSet (CM_LUMENSSUN / exp(m_fCameraExposure));
         cRender.CameraPerspWalkthrough (&m_pCameraXYZ, m_pCameraRot.p[2], m_pCameraRot.p[0], m_pCameraRot.p[1], m_fCameraFOV);
      }

      cRender.Render (0, pProgress);

      // downsample
      if (dwAnti >= 2) {
         PCImage pDown = new CImage;
         if (!pDown) {
            if (pProgress) pProgress->Pop();
            goto blankimage;
         }

         pImage->Downsample (pDown, dwAnti);
         delete pImage;
         pImage = pDown;
      }
   }

   if (pProgress)
      pProgress->Pop();

   return pImage;

blankimage:
   if (!fBlankIfFail) {
      // in some cases may want to know that error actually occured
      if (pImage)
         delete pImage;
      return NULL;
   }

   if (!pImage)
      pImage = new CImage;
   if (!pImage)
      return NULL;
   pImage->Init (dwWidth, dwHeight, RGB(0,0,0));
   return pImage;
}


/*************************************************************************************
M3DOpenDialog - Dialog box for opening a .m3d

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL M3DOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
{
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
   memset (&ofn, 0, sizeof(ofn));
   
   // BUGFIX - Set directory
   char szInitial[256];
   strcpy (szInitial, gszAppDir);
   GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = APPLONGNAME " file (*." M3DFILEEXT ")\0*." M3DFILEEXT "\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save " APPLONGNAME :
      "Open " APPLONGNAME " file";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = M3DFILEEXT;
   // nFileExtension 

   if (fSave) {
      if (!GetSaveFileName(&ofn))
         return FALSE;
   }
   else {
      if (!GetOpenFileName(&ofn))
         return FALSE;
   }

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // copy over
   MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, pszFile, dwChars);
   return TRUE;
}



/****************************************************************************
CRenderScene::SetRenderCamera - Sets the render camera based on the camera
specified in m_AI.gCamera. Actually, this just fills in the parameters usually
associated with override.

inputs
   PCRenderTraditional     pRender - If there's no camera, and NOT overridden, then
                           the camera details will be gotten from this.

NOTE: Will need to call ExposureSet() and CameraPerspWalkthrough() after this.
         m_Render.ExposureSet (CM_LUMENSSUN / exp(oac.m_fCamereaExposure));
         m_Render.CameraPerspWalkthrough (&m_pCameraXYZ, m_pCameraRot.p[2], m_pCameraRot.p[0], m_pCameraRot.p[1], m_fCameraFOV);
*/
void CRenderScene::SetRenderCamera (PCRenderTraditional pRender)
{
   if (m_fCameraOverride)
      return;  // no changes
   if (IsEqualGUID(m_gCamera, GUID_NULL))
      goto extract; // no camera

   PCWorldSocket pWorld = WorldGet();
   if (!pWorld)
      return;

   // get object
   PCObjectSocket pos;
   pos = pWorld->ObjectGet(pWorld->ObjectFind(&m_gCamera));
   if (!pos) {
      m_gCamera = GUID_NULL;
      goto extract;
   }

   // get FOV from camera
   OSMANIMCAMERA oac;
   memset (&oac, 0, sizeof(oac));
   pos->Message (OSM_ANIMCAMERA, &oac);
   m_fCameraFOV = PI/4;
   if (oac.poac)
      m_fCameraFOV = oac.poac->m_fFOV;
   m_fCameraFOV = max(m_fCameraFOV, .001);
   m_fCameraFOV = min(m_fCameraFOV, PI * .99);

   // set the exposure
   if (oac.poac)
      m_fCameraExposure = oac.poac->m_fExposure;

   // get the matrix
   CMatrix m;
   pos->ObjectMatrixGet (&m);

   // since we are looking at 0,-1,0 but the normal camera looks at 0,1,0,
   // invert y
   CMatrix mFlip;
   mFlip.Scale (-1, -1, 1);   // BUGFIX - Flip more than just y
   m.MultiplyLeft (&mFlip);

   // convert this
   CPoint p, p2;
   p.Zero();
   p.MultiplyLeft (&m);

   m.ToXYZLLT (&m_pCameraXYZ, &m_pCameraRot.p[2], &m_pCameraRot.p[0], &m_pCameraRot.p[1]);
   return;

extract:
   DWORD dwModel = pRender->CameraModelGet ();
   m_fCameraExposure = log(CM_LUMENSSUN / pRender->ExposureGet());
   if (dwModel == CAMERAMODEL_PERSPWALKTHROUGH) {
      pRender->CameraPerspWalkthroughGet (&m_pCameraXYZ,
         &m_pCameraRot.p[2], &m_pCameraRot.p[0], &m_pCameraRot.p[1], &m_fCameraFOV);
      return;
   }
   else if (dwModel != CAMERAMODEL_PERSPOBJECT)
      return;  // cant handle flat

   // else, get this one...
   //CPoint p2;
   fp fX, fY, fZ;
   CPoint pCenter;
   CMatrix mInv, mTrans;
   pRender->CameraPerspObjectGet (&p2, &pCenter, &fZ, &fX, &fY, &m_fCameraFOV);
   mTrans.Translation (pCenter.p[0], pCenter.p[1], pCenter.p[2]);
   // p2.Add (&pCenter);
   m.FromXYZLLT (&p2, fZ, fX, fY);
   m.Invert4 (&mInv);
   mInv.MultiplyRight (&mTrans);
   mInv.ToXYZLLT (&m_pCameraXYZ, &m_pCameraRot.p[2], &m_pCameraRot.p[0], &m_pCameraRot.p[1]);
}


/*************************************************************************
ObjectAttribToCombo - Given an object, this enumerates the attributes and
appends to a mem (often gMemTemp) with the combobox elemns.

inputs
   PCObjectSocket       pos - Object
   PCMem                pMem - To append to. Will append <elem>...</elem>
returns
   none
*/
static void ObjectAttribToCombo (PCObjectSocket pos, PCMem pMem)
{
   CListFixed lav;
   lav.Init (sizeof(ATTRIBVAL));
   pos->AttribGetAll (&lav);

   DWORD i;
   PATTRIBVAL pav = (PATTRIBVAL)lav.Get(0);
   ATTRIBINFO Info;
   for (i = 0; i < lav.Num(); i++, pav++) {
      // get the info
      if (!pos->AttribInfo (pav->szName, &Info))
         Info.pszDescription = NULL;

      MemCat (pMem, L"<elem name=\"");
      MemCatSanitize (pMem, pav->szName);
      MemCat (pMem, L"\"><bold>");
      MemCatSanitize (pMem, pav->szName);
      MemCat (pMem, L"</bold>");
      if (Info.pszDescription) {
         MemCat (pMem, L" - ");
         MemCatSanitize (pMem, (PWSTR) Info.pszDescription);
      }
      MemCat (pMem, L"</elem>");
   } // i
}

/*************************************************************************
ComboBoxSetAttrib - Given an attribute name, sets the attribute. IF the
attribute can't be found, then the attribute will be canged to the first
attribute supported by the object.

inputs
   PCEscPage      pPage - Page
   PWSTR          pszControl - Control name for the combobox
   PWSTR          pszAttrib - Attribute
   DWORD          dwID - If the attribute is automatically changed, then get attribute from this object index
   fp             *pfValue - If the attribute is automatically changed then store value in this
returns
   BOOL - TRUE if success
*/
static BOOL ComboBoxSetAttrib (PCEscPage pPage, PWSTR pszControl, PWSTR pszAttrib,
                               DWORD dwID, fp *pfValue)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   ESCMCOMBOBOXSELECTSTRING fs;
   memset (&fs, 0, sizeof(fs));
   fs.fExact = TRUE;
   fs.iStart = -1;
   fs.psz = pszAttrib;
   if (pControl->Message (ESCM_COMBOBOXSELECTSTRING, &fs) && (fs.dwIndex != -1))
      return TRUE;   // found

   // else, pick first one...
   ESCMCOMBOBOXGETITEM gi;
   memset (&gi, 0, sizeof(gi));
   gi.dwIndex = 0;
   pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
   if (gi.pszName)
      wcscpy (pszAttrib, gi.pszName);
   pControl->AttribSetInt (CurSel(), 0);  // select 0

   // get the value
   PCWorldSocket pWorld = WorldGet();
   PCObjectSocket pos = pWorld ? pWorld->ObjectGet(dwID) : NULL;
   if (pos)
      pos->AttribGet (pszAttrib, pfValue);
   return TRUE;
}


/*************************************************************************
ComboBoxSetAttrib - Given an attribute name, sets the attribute. IF the
attribute can't be found, then the attribute will be canged to the first
attribute supported by the object.

inputs
   PCEscPage      pPage - Page
   PWSTR          pszControl - Control name for the combobox
   PWSTR          pszAttrib - Attribute
   GUID           *pgObject - If the attribute is automatically changed, then get attribute from this
   fp             *pfValue - If the attribute is automatically changed then store value in this
returns
   BOOL - TRUE if success
*/
static BOOL ComboBoxSetAttrib (PCEscPage pPage, PWSTR pszControl, PWSTR pszAttrib,
                               GUID *pgObject, fp *pfValue)
{
   PCWorldSocket pWorld = WorldGet();
   if (!pWorld)
      return FALSE;
   DWORD dwID = pWorld->ObjectFind(pgObject);
   return ComboBoxSetAttrib (pPage, pszControl, pszAttrib, dwID, pfValue);
}

/*************************************************************************
ValueType - Given an attribute, returns the value type, AIT_XXX

inputs
   GUID              *pgObject - Object ID
   PWSTR             pszAttrib - Attrib name
retursn  
   DWORD - Type, AIT_XXX
*/
static DWORD ValueType (GUID *pgObject, PWSTR pszAttrib)
{
   PCWorldSocket pWorld = WorldGet();
   if (!pWorld)
      return AIT_NUMBER;
   PCObjectSocket pos = pWorld->ObjectGet(pWorld->ObjectFind (pgObject));
   if (!pos)
      return AIT_NUMBER;

   ATTRIBINFO Info;
   if (!pos->AttribInfo (pszAttrib, &Info))
      return AIT_NUMBER;
   return Info.dwType;
}


/*************************************************************************
ValueType - Given an attribute, returns the value type, AIT_XXX

inputs
   DWORD             dwID - Index into object list
   PWSTR             pszAttrib - Attrib name
retursn  
   DWORD - Type, AIT_XXX
*/
static DWORD ValueType (DWORD dwID, PWSTR pszAttrib)
{
   PCWorldSocket pWorld = WorldGet();
   if (!pWorld)
      return AIT_NUMBER;
   PCObjectSocket pos = pWorld->ObjectGet(dwID);
   if (!pos)
      return AIT_NUMBER;

   ATTRIBINFO Info;
   if (!pos->AttribInfo (pszAttrib, &Info))
      return AIT_NUMBER;
   return Info.dwType;
}

/*************************************************************************
ValueToControl - Object value to an edit control

inputs
   PCEscPage         pPage - Page
   PWSTR             pszControl - Edit control
   DWORD             dwType - From ValueType()
   fp                fValue - Value to write outf
retursn  
   BOOL - TRUE if success
*/
static BOOL ValueToControl (PCEscPage pPage, PWSTR pszControl, DWORD dwType,
                            fp fValue)
{
   switch (dwType) {
   case AIT_NUMBER:
   case AIT_BOOL:
   case AIT_HZ:
   default:
      DoubleToControl (pPage, pszControl, fValue);
      return TRUE;
   case AIT_DISTANCE:
      MeasureToString (pPage, pszControl, fValue);
      return TRUE;
   case AIT_ANGLE:
      AngleToControl (pPage, pszControl, fValue);
      return TRUE;
   }

   return TRUE;
}


/*************************************************************************
ValueFromControl - Object value to an edit control

inputs
   PCEscPage         pPage - Page
   PWSTR             pszControl - Edit control
   DWORD             dwType - From ValueType()
   fp                *pfValue - Filled with the value from the control
retursn  
   BOOL - TRUE if success
*/
static BOOL ValueFromControl (PCEscPage pPage, PWSTR pszControl, DWORD dwType,
                            fp *pfValue)
{
   switch (dwType) {
   case AIT_NUMBER:
   case AIT_BOOL:
   case AIT_HZ:
   default:
      *pfValue = DoubleFromControl (pPage, pszControl);
      return TRUE;
   case AIT_DISTANCE:
      MeasureParseString (pPage, pszControl, pfValue);
      return TRUE;
   case AIT_ANGLE:
      *pfValue = AngleFromControl (pPage, pszControl);
      return TRUE;
   }

   return TRUE;
}


/*************************************************************************
RenderScenePage
*/
BOOL RenderScenePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCRenderScene prs = (PCRenderScene)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // scroll to right position
         if (prs->m_iVScroll > 0) {
            pPage->VScroll (prs->m_iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            prs->m_iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         PCEscControl pControl;
         DWORD i, j;
         WCHAR szTemp[64];
         ESCMLISTBOXADD lba;
         PCSceneSet pSceneSet = NULL;
         PCWorldSocket pWorld = WorldGet (&pSceneSet);
         PCScene pSceneCur = NULL;

         // fill in the camera
         pControl = pPage->ControlFind (L"camera");
         if (pWorld && pControl) {
            // camera
            DWORD dwSel;
            CListFixed lCamera;
            lCamera.Init (sizeof(OSMANIMCAMERA));
            CameraEnum(pWorld, &lCamera);

            // add the option for view camera
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = L"(View camera)";
            pControl->Message (ESCM_LISTBOXADD, &lba);
            dwSel = 0;  // default to the selection being this

            // add option for the custom camera
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = L"(Custom camera)";
            pControl->Message (ESCM_LISTBOXADD, &lba);
            if (prs->m_fCameraOverride)
               dwSel = 1;

            // add the items
            POSMANIMCAMERA pc;
            GUID g;
            pc = (POSMANIMCAMERA) lCamera.Get(0);
            for (i = 0; i < lCamera.Num(); i++, pc++) {
               PWSTR psz = pc->poac->StringGet(OSSTRING_NAME);
               if (!psz)
                  psz = L"Unnnamed";
               pc->poac->GUIDGet (&g);
               memset (&lba, 0, sizeof(lba));
               lba.dwInsertBefore = -1;
               lba.pszText = psz;
               pControl->Message (ESCM_LISTBOXADD, &lba);

               // set selection?
               if (IsEqualGUID (g, prs->m_gCamera))
                  dwSel = i + 2;
            }  // i
            if ((dwSel < 2) && !IsEqualGUID (prs->m_gCamera, GUID_NULL))
               prs->m_gCamera = GUID_NULL;   // since couldnt find

            pControl->AttribSetInt (CurSel(), (int)dwSel);
            
            MeasureToString (pPage, L"movedist", prs->m_fMoveDist);
         } // camera

         pControl = pPage->ControlFind (L"scene");
         if (pWorld && pSceneSet && pControl) {
            DWORD dwSel = -1;

            // scene
            pSceneSet->StateGet (&pSceneCur, NULL);
            for (i = 0; i < pSceneSet->SceneNum(); i++) {
               PCScene pScene = pSceneSet->SceneGet(i);
               if (!pScene)
                  continue;

               PWSTR pszName;
               pszName = pScene->NameGet ();
               if (!pszName || !pszName[0])
                  pszName = L"Unnamed";

               if (pSceneCur == pScene) {
                  // since this is the current scene, change the string, just in case
                  // dont need to set changed bit though
                  pSceneCur->GUIDGet(&prs->m_gScene);
                  dwSel = i;
               }

               memset (&lba, 0, sizeof(lba));
               lba.dwInsertBefore = -1;
               lba.pszText = pszName;
               pControl->Message (ESCM_LISTBOXADD, &lba);
            }

            if (dwSel != -1)
               pControl->AttribSetInt (CurSel(), dwSel);
         }  // if scene display
         DoubleToControl (pPage, L"SceneTime", prs->m_fSceneTime);

         // bookmarks
         pControl = pPage->ControlFind (L"bookmarkrange");
         if (pControl && pSceneCur) {
            // bookmarks
            CListFixed lBookmark;
            POSMBOOKMARK pb;
            lBookmark.Init (sizeof(OSMBOOKMARK));
            pSceneCur->BookmarkEnum (&lBookmark, TRUE, TRUE);
            pb = (POSMBOOKMARK) lBookmark.Get(0);
            DWORD dwSel = -1;

            for (i = 0; i < lBookmark.Num(); i++, pb++) {
               memset (&lba, 0, sizeof(lba));
               lba.dwInsertBefore = -1;
               lba.pszText = pb->szName;
               pControl->Message (ESCM_LISTBOXADD, &lba);

               if (!wcsicmp(prs->m_szSceneBookmark, pb->szName))
                  dwSel = i;
            }

            // set selection?
            if (dwSel != -1)
               pControl->AttribSetInt (CurSel(), (int)dwSel);
            else {
               prs->m_szSceneBookmark[0] = 0;
               pControl->AttribSetInt (CurSel(), -1);
            }
         } // bookmarks

         pControl = pPage->ControlFind (L"file");
         if (pControl)
            pControl->AttribSet (Text(), prs->m_szFile);

         ComboBoxSet (pPage, L"aspect", prs->m_dwAspect);
         ComboBoxSet (pPage, L"quality", prs->m_dwQuality);
         ComboBoxSet (pPage, L"anti", prs->m_dwAnti);

         // disable?
         if (prs->m_fReadOnly) {
            // BUGBUG - disable all controls
         }

         // image width and height
         DWORD dwWidth, dwHeight;
         RenderSceneAspectToPixels (prs->m_dwAspect, SCALEPREVIEW, &dwWidth, &dwHeight);

         // fill in the controls
         PIHOTSPOT ph = (PIHOTSPOT)prs->m_lIHOTSPOT.Get(0);
         CListFixed lCD;
         lCD.Init (sizeof(CONTROLIMAGEDRAGRECT));
         CONTROLIMAGEDRAGRECT cd;
         memset (&cd, 0, sizeof(cd));
         for (i = 0; i < prs->m_lIHOTSPOT.Num(); i++, ph++) {
            swprintf (szTemp, L"hotmsg%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSet (Text(), ph->ps->Get());

            swprintf (szTemp, L"hotcursor%d", (int)i);
            ComboBoxSet (pPage, szTemp, ph->dwCursor);

            // add to list that will send to bitmap
            cd.cColor = HotSpotColor(i);
            cd.fModulo = (prs->m_dwAspect == 10);
            cd.rPos = ph->rPosn;
            RenderSceneHotSpotToImage (&cd.rPos, dwWidth, dwHeight);
            lCD.Add (&cd);
         } // i

         // send message to image to show hotspots
         ESCMIMAGERECTSET is;
         memset (&is, 0, sizeof(is));
         is.dwNum = lCD.Num();
         is.pRect = (PCONTROLIMAGEDRAGRECT)lCD.Get(0);
         pControl = pPage->ControlFind (L"image");
         if (pControl)
            pControl->Message (ESCM_IMAGERECTSET, &is);

         // set the language for the hotspots
         MIFLLangComboBoxSet (pPage, L"langid", prs->m_lid, prs->m_pProj);


         // fill in the attributes to be modified
         if (prs->m_dwTab == 4) {
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(0);
            for (i = 0; i < prs->m_lRSATTRIB.Num(); i++, pr++) {
               swprintf (szTemp, L"rsattribnum%d", (int)i);
               if (prs->m_fLoadFromFile)
                  ComboBoxSetAttrib (pPage, szTemp, pr->szAttrib, &pr->gObject, &pr->fValue);
               else
                  ComboBoxSetAttrib (pPage, szTemp, pr->szAttrib, pr->dwObject, &pr->fValue);

               swprintf (szTemp, L"rsattribval%d", (int)i);
               DWORD dwType;
               if (prs->m_fLoadFromFile)
                  dwType = ValueType (&pr->gObject, pr->szAttrib);
               else
                  dwType = ValueType (pr->dwObject, pr->szAttrib);
               ValueToControl (pPage, szTemp, dwType, pr->fValue);
            } // i
         }

         // tab for modifying objects
         if (prs->m_dwTab == 6) {
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(0);
            for (i = 0; i < prs->m_lRSOBJECT.Num(); i++, po++) {
               for (j = 0; j < 3; j++) {
                  swprintf (szTemp, L"objloc%d%d", (int)j, (int)i);
                  MeasureToString (pPage, szTemp, po->pLoc.p[j]);
               }
               for (j = 0; j < 3; j++) {
                  swprintf (szTemp, L"objrot%d%d", (int)j, (int)i);
                  AngleToControl (pPage, szTemp, po->pRot.p[j]);
               }
            } // i
         } // tab==6

         // update the controls for location
         pPage->Message (ESCM_USER+150);
      }
      break;

   case ESCM_USER+150:  // update controls for location
      {
         MeasureToString (pPage, L"pos0", prs->m_pCameraXYZ.p[0]);
         MeasureToString (pPage, L"pos1", prs->m_pCameraXYZ.p[1]);
         MeasureToString (pPage, L"pos2", prs->m_pCameraXYZ.p[2]);
         AngleToControl (pPage, L"fov", prs->m_fCameraFOV);
         DoubleToControl (pPage, L"exposure", prs->m_fCameraExposure);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"rot0");
         if (pControl)
            pControl->AttribSetInt (L"pos", (int)
               (prs->m_pCameraRot.p[0] / 2.0 / PI * 360));
         pControl = pPage->ControlFind (L"rot1");
         if (pControl)
            pControl->AttribSetInt (L"pos", (int)
               (prs->m_pCameraRot.p[1] / 2.0 / PI * 360));
         pControl = pPage->ControlFind (L"rot2");
         if (pControl)
            pControl->AttribSetInt (L"pos", -(int)
               (prs->m_pCameraRot.p[2] / 2.0 / PI * 360));
      }
      return TRUE;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;   // no name

         if (!wcsicmp(p->pControl->m_pszName, L"scene")) {
            PCSceneSet pSceneSet = NULL;
            PCWorldSocket pWorld = WorldGet (&pSceneSet);
            PCScene pScene = pSceneSet ? pSceneSet->SceneGet (p->dwCurSel) : NULL;
            if (!pScene)
               return TRUE;

            pScene->GUIDGet (&prs->m_gScene);
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage()); // need to do with aspect change
            return TRUE;
         }
         else if (!wcsicmp(p->pControl->m_pszName, L"camera")) {
            PCWorldSocket pWorld = WorldGet ();
            if (!pWorld)
               return TRUE;

            // get the list
            CListFixed lCamera;
            lCamera.Init (sizeof(OSMANIMCAMERA));
            CameraEnum (pWorld, &lCamera);
            POSMANIMCAMERA pc;
            pc = (p->dwCurSel >= 2) ? (POSMANIMCAMERA) lCamera.Get(p->dwCurSel - 2) : NULL;

            BOOL fReload = FALSE;
            if (pc) {
               pc->poac->GUIDGet (&prs->m_gCamera);
               prs->m_fCameraOverride = FALSE;
            }
            else {
               prs->m_fCameraOverride = (p->dwCurSel == 1);
               fReload = !prs->m_fCameraOverride;
               prs->m_gCamera = GUID_NULL;
            }
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[1] = fReload;  // since may have switched back to original
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            // will want to update the text/sliders/values for the custom
            // camera to reflect any standard camera that using
            pPage->Message (ESCM_USER+150);
            return TRUE;
         }
         else if (!wcsicmp(p->pControl->m_pszName, L"bookmarkrange")) {
            PCSceneSet pSceneSet = NULL;
            PCWorldSocket pWorld = WorldGet (&pSceneSet);
            PCScene pSceneCur = NULL;
            if (pSceneSet)
               pSceneSet->StateGet(&pSceneCur, NULL);
            if (!pSceneCur)
               return TRUE;

            // bookmarks
            CListFixed lBookmark;
            POSMBOOKMARK pb;
            lBookmark.Init (sizeof(OSMBOOKMARK));
            pSceneCur->BookmarkEnum (&lBookmark, TRUE, TRUE);
            pb = (POSMBOOKMARK) lBookmark.Get(p->dwCurSel);

            if (!pb)
               prs->m_szSceneBookmark[0] = 0;
            else {
               wcscpy (prs->m_szSceneBookmark, pb->szName);
               DoubleToControl (pPage, L"SceneTime", prs->m_fSceneTime = pb->fStart);
            }
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszHotCursor = L"hotcursor", pszRSAttribNum = L"rsattribnum";
         DWORD dwHotCursorLen = wcslen(pszHotCursor), dwRSAttribNumLen = wcslen(pszRSAttribNum);
         if (!wcsncmp(psz, pszRSAttribNum, dwRSAttribNumLen)) {
            DWORD dwIndex = _wtoi(psz + dwRSAttribNumLen);
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(dwIndex);
            if (!pr || !p->pszName)
               break;
            if (!wcsicmp(pr->szAttrib, p->pszName))
               return TRUE;   // no change

            wcscpy (pr->szAttrib, p->pszName);

            // get the value...
            PCWorldSocket pWorld = WorldGet();
            PCObjectSocket pos;
            if (prs->m_fLoadFromFile)
               pos = pWorld ? pWorld->ObjectGet(pWorld->ObjectFind(&pr->gObject)) : NULL;
            else
               pos = pWorld ? pWorld->ObjectGet(pr->dwObject) : NULL;
            if (pos)
               pos->AttribGet (pr->szAttrib, &pr->fValue);

            // new value
            WCHAR szTemp[64];
            DWORD dwType;
            if (prs->m_fLoadFromFile)
               dwType = ValueType (&pr->gObject, pr->szAttrib);
            else
               dwType = ValueType (pr->dwObject, pr->szAttrib);
            swprintf (szTemp, L"rsattribval%d", (int)dwIndex);
            ValueToControl (pPage, szTemp, dwType, pr->fValue);

            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!wcsncmp(psz, pszHotCursor, dwHotCursorLen)) {
            DWORD dwIndex = _wtoi(psz + dwHotCursorLen);
            PIHOTSPOT ph = (PIHOTSPOT)prs->m_lIHOTSPOT.Get(dwIndex);
            if (!ph)
               break;
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == ph->dwCursor)
               return TRUE;

            // else changed
            ph->dwCursor = dwVal;
            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!wcsicmp(psz, L"langid")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            LANGID *padw = (LANGID*)prs->m_pProj->m_lLANGID.Get(dwVal);
            dwVal = padw ? padw[0] : prs->m_lid;
            if (dwVal == prs->m_lid)
               return TRUE;

            // else changed
            prs->m_lid = (LANGID)dwVal;
            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!wcsicmp(psz, L"aspect")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == prs->m_dwAspect)
               return TRUE;

            prs->m_dwAspect = dwVal;
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage()); // need to do with aspect change
            return TRUE;
         }
         else if (!wcsicmp(psz, L"anti")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == prs->m_dwAnti)
               return TRUE;

            prs->m_dwAnti = dwVal;
            prs->m_fChanged = TRUE;
            // dont refresh since wont show anything
            return TRUE;
         }
         else if (!wcsicmp(psz, L"quality")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == prs->m_dwQuality)
               return TRUE;

            prs->m_dwQuality = dwVal;
            prs->m_fChanged = TRUE;
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszHotMsg = L"hotmsg", pszRSAttribVal = L"rsattribval",
            pszObjLoc = L"objloc", pszObjRot = L"objrot";
         DWORD dwHotMsgLen = wcslen(pszHotMsg), dwRSAttribValLen = wcslen(pszRSAttribVal),
            dwObjLocLen = wcslen(pszObjLoc), dwObjRotLen = wcslen(pszObjRot);

         if (!wcsncmp(psz, pszObjLoc, dwObjLocLen)) {
            DWORD dwDim = psz[dwObjLocLen] - L'0';
            DWORD dwIndex = _wtoi(psz + (dwObjLocLen+1));
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(dwIndex);
            if (!po)
               break;

            MeasureParseString (pPage, psz, &po->pLoc.p[dwDim]);
            prs->m_fChanged = TRUE;

            // redraw...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (!wcsncmp(psz, pszObjRot, dwObjRotLen)) {
            DWORD dwDim = psz[dwObjRotLen] - L'0';
            DWORD dwIndex = _wtoi(psz + (dwObjRotLen+1));
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(dwIndex);
            if (!po)
               break;

            po->pRot.p[dwDim] = AngleFromControl (pPage, psz);
            prs->m_fChanged = TRUE;

            // redraw...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (!wcsncmp(psz, pszRSAttribVal, dwRSAttribValLen)) {
            DWORD dwIndex = _wtoi(psz + dwRSAttribValLen);
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(dwIndex);
            if (!pr)
               break;

            DWORD dwType;
            if (prs->m_fLoadFromFile)
               dwType = ValueType (&pr->gObject, pr->szAttrib);
            else
               dwType = ValueType (pr->dwObject, pr->szAttrib);
            ValueFromControl (pPage, psz, dwType, &pr->fValue);
            prs->m_fChanged = TRUE;

            // redraw...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (!wcsncmp(psz, pszHotMsg, dwHotMsgLen)) {
            DWORD dwIndex = _wtoi(psz + dwHotMsgLen);
            PIHOTSPOT ph = (PIHOTSPOT)prs->m_lIHOTSPOT.Get(dwIndex);
            if (!ph)
               break;

            WCHAR szTemp[512];
            DWORD dwNeeded;
            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            ph->ps->Set(szTemp);
            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!wcsicmp(psz, L"SceneTime")) {
            prs->m_fSceneTime = DoubleFromControl (pPage, psz);
            prs->m_fSceneTime = max(0, prs->m_fSceneTime);
            prs->m_fChanged = TRUE;

            // de-select bookmark list
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"bookmarkrange");
            pControl->AttribSetInt (CurSel(), -1);

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!wcsicmp(psz, L"movedist")) {
            MeasureParseString (pPage, L"movedist", &prs->m_fMoveDist);
            return TRUE;
         }
         else if (!wcsicmp(psz, L"pos0") || !wcsicmp(psz, L"pos1") || !wcsicmp(psz, L"pos2") ||
            !wcsicmp(psz, L"fov") || !wcsicmp(psz, L"exposure")) {

            MeasureParseString (pPage, L"pos0", &prs->m_pCameraXYZ.p[0]);
            MeasureParseString (pPage, L"pos1", &prs->m_pCameraXYZ.p[1]);
            MeasureParseString (pPage, L"pos2", &prs->m_pCameraXYZ.p[2]);
            prs->m_fCameraFOV = AngleFromControl (pPage, L"fov");
            prs->m_fCameraExposure = DoubleFromControl (pPage, L"exposure");

            prs->m_fChanged = TRUE;

            // make sure that custom is current selection
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"camera");
            if (pControl && (pControl->AttribGetInt(CurSel()) != 1))
               pControl->AttribSetInt (CurSel(), 1);
            prs->m_fCameraOverride = TRUE;
            prs->m_gCamera = GUID_NULL;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszRot = L"rot";
         DWORD dwRotLen = wcslen(pszRot);
         if (!wcsncmp(psz, pszRot, dwRotLen)) {
            DWORD dwNum = _wtoi(psz + dwRotLen);
            if (dwNum >= 3)
               return TRUE;

            fp fVal = (fp)p->iPos / 360.0 * 2.0 * PI * ((dwNum == 2) ? -1 : 1);
            if (fabs(fVal - prs->m_pCameraRot.p[dwNum]) < 0.001)
               return TRUE;   // no change

            prs->m_pCameraRot.p[dwNum] = fVal;
            prs->m_fChanged = TRUE;

            // make sure that custom is current selection
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"camera");
            if (pControl && (pControl->AttribGetInt(CurSel()) != 1))
               pControl->AttribSetInt (CurSel(), 1);
            prs->m_fCameraOverride = TRUE;
            prs->m_gCamera = GUID_NULL;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszHotDel = L"hotdel", pszRSAttribDel = L"rsattribdel",
            pszObjDel = L"objdel", pszObjTop = L"objtop", pszRSColorDel = L"rscolordel";
         DWORD dwHotDelLen = wcslen(pszHotDel), dwRSAttribDel = wcslen(pszRSAttribDel),
            dwObjDelLen = wcslen(pszObjDel), dwObjTopLen = wcslen(pszObjTop),
            dwRSColorDelLen = wcslen(pszRSColorDel);
         if (!wcsncmp(psz, pszObjDel, dwObjDelLen)) {
            DWORD dwIndex = _wtoi(psz + dwObjDelLen);
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(dwIndex);
            if (!po)
               break;

            // will need to modify all attribs to note deletion
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(0);
            DWORD i;
            for (i = prs->m_lRSATTRIB.Num()-1; i < prs->m_lRSATTRIB.Num(); i--) {
               if (pr[i].dwObject < dwIndex)
                  continue;   // no change
               if (pr[i].dwObject > dwIndex) {
                  pr[i].dwObject--;
                  continue;
               }

               // else, delete
               prs->m_lRSATTRIB.Remove (i);
               pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(0);
            }

            // will need to modify all paints to note deletion
            PRSCOLOR prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(0);
            for (i = prs->m_lRSCOLOR.Num()-1; i < prs->m_lRSCOLOR.Num(); i--) {
               if (prc[i].dwObject < dwIndex)
                  continue;   // no change
               if (prc[i].dwObject > dwIndex) {
                  prc[i].dwObject--;
                  continue;
               }

               // else, delete
               delete prc[i].pSurf;
               prs->m_lRSCOLOR.Remove (i);
               prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(0);
            }

            // remove object
            prs->m_lRSOBJECT.Remove (dwIndex);
            prs->m_fChanged = TRUE;

            // refresh image...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszObjTop, dwObjTopLen)) {
            DWORD dwIndex = _wtoi(psz + dwObjTopLen);
            if (dwIndex >= prs->m_lRSOBJECT.Num())
               return TRUE;
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(0);

            RSOBJECT rTemp;
            rTemp = po[0];
            po[0] = po[dwIndex];
            po[dwIndex] = rTemp;
            prs->m_fChanged = TRUE;

            // will need to modify all attribs to note move
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(0);
            DWORD i;
            for (i = prs->m_lRSATTRIB.Num()-1; i < prs->m_lRSATTRIB.Num(); i--) {
               if (pr[i].dwObject == dwIndex)
                  pr[i].dwObject = 0;
               else if (pr[i].dwObject == 0)
                  pr[i].dwObject = dwIndex;
            }

            // will need to modify all paints to note move
            PRSCOLOR prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(0);
            for (i = prs->m_lRSCOLOR.Num()-1; i < prs->m_lRSCOLOR.Num(); i--) {
               if (prc[i].dwObject == dwIndex)
                  prc[i].dwObject = 0;
               else if (prc[i].dwObject == 0)
                  prc[i].dwObject = dwIndex;
            }


            // refresh image...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszRSAttribDel, dwRSAttribDel)) {
            DWORD dwIndex = _wtoi(psz + dwRSAttribDel);
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(dwIndex);
            if (!pr)
               break;

            // delete
            prs->m_lRSATTRIB.Remove (dwIndex);
            prs->m_fChanged = TRUE;

            // refresh image...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[1] = TRUE;
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszRSColorDel, dwRSColorDelLen)) {
            DWORD dwIndex = _wtoi(psz + dwRSColorDelLen);
            PRSCOLOR pr = (PRSCOLOR)prs->m_lRSCOLOR.Get(dwIndex);
            if (!pr)
               break;

            // delete
            delete pr->pSurf;
            prs->m_lRSCOLOR.Remove (dwIndex);
            prs->m_fChanged = TRUE;

            // refresh image...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszHotDel, dwHotDelLen)) {
            DWORD dwIndex = _wtoi(psz + dwHotDelLen);
            PIHOTSPOT ph = (PIHOTSPOT)prs->m_lIHOTSPOT.Get(dwIndex);
            if (!ph)
               break;

            // delete
            ph->ps->Release();
            prs->m_lIHOTSPOT.Remove (dwIndex);
            prs->m_fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsicmp(psz, L"open")) {
            if (!M3DOpenDialog (pPage->m_pWindow->m_hWnd, prs->m_szFile, sizeof(prs->m_szFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            prs->m_fChanged = TRUE;
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[1] = TRUE;
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            PCEscControl pControl;
            pControl = pPage->ControlFind (L"file");
            if (pControl)
               pControl->AttribSet (Text(), prs->m_szFile);
            return TRUE;
         }
         else if (!wcsicmp(psz, L"refresh")) {
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[1] = TRUE;
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!wcsicmp(psz, L"finalquality")) {
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[0] = af[1] = TRUE;
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!wcsicmp(psz, L"objnew")) {
            RSOBJECT ro;
            memset (&ro, 0, sizeof(ro));

            if (!ObjectCFNewDialog (pPage->m_pWindow->m_hWnd, &ro.gMajor, &ro.gMinor))
               return TRUE;   // cancelled

            // add it
            prs->m_lRSOBJECT.Add (&ro);
            prs->m_fChanged = TRUE;

            BOOL af[2];
            memset (af, 0, sizeof(af));

            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            pPage->Exit (RedoSamePage()); // since need to add entry
            return TRUE;
         }
         else if (!wcsicmp(psz, L"moveforward") || !wcsicmp(psz, L"moveback") ||
            !wcsicmp(psz, L"rotleft") || !wcsicmp(psz, L"rotright")) {

            if (!wcsicmp(psz, L"moveforward")) {
               prs->m_pCameraXYZ.p[0] -= sin(prs->m_pCameraRot.p[2]) * prs->m_fMoveDist;
               prs->m_pCameraXYZ.p[1] += cos(prs->m_pCameraRot.p[2]) * prs->m_fMoveDist;
            }
            else if (!wcsicmp(psz, L"moveback")) {
               prs->m_pCameraXYZ.p[0] += sin(prs->m_pCameraRot.p[2]) * prs->m_fMoveDist;
               prs->m_pCameraXYZ.p[1] -= cos(prs->m_pCameraRot.p[2]) * prs->m_fMoveDist;
            }
            else if (!wcsicmp(psz, L"rotleft"))
               prs->m_pCameraRot.p[2] += PI/8;
            else if (!wcsicmp(psz, L"rotright"))
               prs->m_pCameraRot.p[2] -= PI/8;
            prs->m_pCameraRot.p[2] = myfmod(prs->m_pCameraRot.p[2] + PI,2.0*PI) - PI;

            prs->m_fChanged = TRUE;

            // make sure that custom is current selection
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"camera");
            if (pControl && (pControl->AttribGetInt(CurSel()) != 1))
               pControl->AttribSetInt (CurSel(), 1);
            prs->m_fCameraOverride = TRUE;
            prs->m_gCamera = GUID_NULL;

            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            // update the controls for location
            pPage->Message (ESCM_USER+150);
            return TRUE;
         }
      }
      break;

   case ESCM_USER+102:  // called to indicate that should redraw
      {
         CProgress Progress;
         BOOL *paf = (BOOL*)pParam;

         Progress.Start (pPage->m_pWindow->m_hWnd, "Drawing...", paf[0] || paf[1]);

         if (prs->m_pImage)
            delete prs->m_pImage;
         prs->m_pImage = prs->Render (SCALEPREVIEW, paf[0], paf[1], TRUE, &Progress);
         if (!prs->m_pImage)
            return TRUE;   // unlikely

         if (prs->m_hBmp)
            DeleteObject (prs->m_hBmp);
         HDC hDC = GetDC (pPage->m_pWindow->m_hWnd);
         prs->m_hBmp = prs->m_pImage->ToBitmap (hDC);
         ReleaseDC (pPage->m_pWindow->m_hWnd, hDC);

         PCEscControl pControl = pPage->ControlFind (L"image");
         WCHAR szTemp[32];
         swprintf (szTemp, L"%x", (int)prs->m_hBmp);
         if (pControl)
            pControl->AttribSet (L"hbitmap", szTemp);
      }
      return TRUE;

   case ESCN_IMAGEDRAGGED:
      {
         if (prs->m_fReadOnly)
            return TRUE;   // cant change

         PESCNIMAGEDRAGGED p = (PESCNIMAGEDRAGGED)pParam;

         if (prs->m_dwTab == 5) {   // hot spots
            // image width and height
            DWORD dwWidth, dwHeight;
            RenderSceneAspectToPixels (prs->m_dwAspect, SCALEPREVIEW, &dwWidth, &dwHeight);

            IHOTSPOT hs;
            memset (&hs, 0, sizeof(hs));
            hs.rPosn = p->rPos;
            RenderSceneHotSpotFromImage (&hs.rPosn, dwWidth, dwHeight);
            hs.ps = new CMIFLVarString;
            if (!hs.ps)
               return TRUE;

            prs->m_lIHOTSPOT.Add (&hs);
            prs->m_fChanged = TRUE;

            // refresh
            pPage->Exit (RedoSamePage());
         }
         else if (prs->m_dwTab == 4) { // attributes
            // see if clickd outside the image
            if (!prs->m_pImage || (p->rPos.left < 0) || (p->rPos.left >= (int)prs->m_pImage->Width()) ||
               (p->rPos.top < 0) || (p->rPos.top >= (int)prs->m_pImage->Height())) {

               pPage->MBWarning (L"Please click on the image.");
               return TRUE;
            }

            // get the pixel
            PIMAGEPIXEL pip = prs->m_pImage->Pixel ((DWORD)p->rPos.left, (DWORD)p->rPos.top);
            DWORD dwID = HIWORD(pip->dwID);
            // if not an object, or if beyond the range of acceptable objects
            if (!dwID || (!prs->m_fLoadFromFile && (dwID-1 >= prs->m_lRSOBJECT.Num())) ) {
               pPage->MBWarning (L"You must click on an object.", L"You clicked on a background.");
               return TRUE;
            }
            dwID--;

            // get the object
            PCWorldSocket pWorld = WorldGet();
            PCObjectSocket pos = pWorld->ObjectGet (dwID);
            if (!pos)
               return TRUE;

            // get the guid
            RSATTRIB rs;
            memset (&rs, 0, sizeof(rs));
            pos->GUIDGet (&rs.gObject);
            rs.dwObject = dwID;
            
            // add it
            prs->m_lRSATTRIB.Add (&rs);
            prs->m_fChanged = TRUE;

            // refresh
            pPage->Exit (RedoSamePage());
         }
         else if (prs->m_dwTab == 7) { // colors
            // see if clickd outside the image
            if (!prs->m_pImage || (p->rPos.left < 0) || (p->rPos.left >= (int)prs->m_pImage->Width()) ||
               (p->rPos.top < 0) || (p->rPos.top >= (int)prs->m_pImage->Height())) {

               pPage->MBWarning (L"Please click on the image.");
               return TRUE;
            }

            // get the pixel
            PIMAGEPIXEL pip = prs->m_pImage->Pixel ((DWORD)p->rPos.left, (DWORD)p->rPos.top);
            DWORD dwID = HIWORD(pip->dwID);
            // if not an object, or if beyond the range of acceptable objects
            if (!dwID || (!prs->m_fLoadFromFile && (dwID-1 >= prs->m_lRSOBJECT.Num())) ) {
               pPage->MBWarning (L"You must click on an object.", L"You clicked on a background.");
               return TRUE;
            }
            dwID--;

            // get the object
            PCWorldSocket pWorld = WorldGet();
            PCObjectSocket pos = pWorld->ObjectGet (dwID);
            if (!pos)
               return TRUE;
            DWORD dwSurf = LOWORD(pip->dwID);

            // get the guid
            RSCOLOR rc;
            memset (&rc, 0, sizeof(rc));
            pos->GUIDGet (&rc.gObject);
            rc.dwObject = dwID;

            // see if already have this one..
            PRSCOLOR prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(0);
            DWORD i;
            for (i = 0; i < prs->m_lRSCOLOR.Num(); i++, prc++) {
               if (prc->pSurf->m_dwID != dwSurf)
                  continue;

               if (prs->m_fLoadFromFile && IsEqualGUID(prc->gObject, rc.gObject))
                  break;
               else if (!prs->m_fLoadFromFile && (prc->dwObject == rc.dwObject))
                  break;
            } // i
            if (i >= prs->m_lRSCOLOR.Num()) {
               // need to add
               rc.pSurf = pos->SurfaceGet (dwSurf);
               if (!rc.pSurf) {
                  pPage->MBWarning (L"You can't paint this surface.");
                  return TRUE;
               }
               rc.pSurf->m_dwID = dwSurf;

               prs->m_lRSCOLOR.Add (&rc);
               prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(prs->m_lRSCOLOR.Num()-1);
            }

            if (!TextureSelDialog (pPage->m_pWindow->m_hWnd, prc->pSurf, pWorld))
               return TRUE;   // nothing changed
          

            // note changed
            prc->pSurf->m_dwID = dwSurf;
            prs->m_fChanged = TRUE;

            // redraw
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            // refresh
            pPage->Exit (RedoSamePage());
         }
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         PWSTR pszTab = L"tabpress:";
         DWORD dwLen = wcslen(pszTab);

         if (!wcsncmp(p->psz, pszTab, dwLen)) {
            prs->m_dwTab = (DWORD)_wtoi(p->psz + dwLen);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszIfTab = L"IFTAB", pszEndIfTab = L"ENDIFTAB";
         DWORD dwIfTabLen = wcslen(pszIfTab), dwEndIfTabLen = wcslen(pszEndIfTab);

         if (!wcsncmp (p->pszSubName, pszIfTab, dwIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwIfTabLen);
            if (dwNum == prs->m_dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!wcsncmp (p->pszSubName, pszEndIfTab, dwEndIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwEndIfTabLen);
            if (dwNum == prs->m_dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!wcsicmp (p->pszSubName, L"IFEXISTINGCAMERA")) {
            if (prs->m_fLoadFromFile)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!wcsicmp (p->pszSubName, L"ENDIFEXISTINGCAMERA")) {

            if (prs->m_fLoadFromFile)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"RSTABS")) {
            PWSTR apsz[] = {
               L"File",
               L"Objects",
               L"Quality",
               L"Scene",
               L"Camera",
               L"Paint",
               L"Attributes",
               L"Hot spots",
            };
            PWSTR apszHelp[] = {
               L"Loads in a different " APPSHORTNAMEW L" file.",
               L"Add and move objects within the scene.",
               L"Changes the quality of the image.",
               L"Controls which animation scene and timeslice are drawn.",
               L"Places the camera.",
               L"Lets you change an object's color or texture.",
               L"Changes the objects' attributes.",
               L"Select which areas can be clicked on.",
            };
            DWORD adwID[] = {
               0,  //L"File",
               6, // objects
               1, //L"Quality",
               2, //L"Scene",
               3, // L"Camera",
               7, // L"Paint",
               4, // L"Attributes",
               5, // L"Hot spots",
            };

            CListFixed lSkip;
            lSkip.Init (sizeof(DWORD));
            DWORD dw;

            // skip depending upon if have file or not
            if (prs->m_fLoadFromFile) {
               dw = 6;  // objects
               lSkip.Add (&dw);
            }
            else {
               dw = 0;  // file
               lSkip.Add (&dw);

               dw = 2;  //scene
               lSkip.Add (&dw);
            }


            // if there's no scene then skip the scene selection
            PCSceneSet pScene;
            PCWorldSocket pWorld = WorldGet (&pScene);
            if (!pScene || !pScene->SceneNum()) {
               dw = 2;  // skip scene
               lSkip.Add (&dw);
            }

            p->pszSubString = RenderSceneTabs (prs->m_dwTab, sizeof(apsz)/sizeof(PWSTR), apsz,
               apszHelp, adwID, lSkip.Num(), (DWORD*)lSkip.Get(0));
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"3D scene resource";
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"IMAGEDRAG")) {
            MemZero (&gMemTemp);

            MemCat (&gMemTemp, L"<imagedrag name=image clickmode=");
            switch (prs->m_fReadOnly ? 10000 : prs->m_dwTab) {
            case 4:  // attributes
            case 7:  // paint
               // click only
               MemCat (&gMemTemp, L"1");
               break;
            case 5:  // hot spots
               // click and drag
               MemCat (&gMemTemp, L"2");
               break;
            default: // others (including RO)... cant click
               MemCat (&gMemTemp, L"0");
               break;
            }
            MemCat (&gMemTemp, L" border=2 width=");
            int iWidth = 90;  // percent
            DWORD dwWidth, dwHeight;
            RenderSceneAspectToPixels (prs->m_dwAspect, 1, &dwWidth, &dwHeight);
            if (dwWidth < dwHeight)
               iWidth = iWidth * (int)dwWidth / (int)dwHeight;

            MemCat (&gMemTemp, L"% hbitmap=");
            WCHAR szTemp[32];
            swprintf (szTemp, L"%x", (int)prs->m_hBmp);
            MemCat (&gMemTemp, szTemp);

            MemCat (&gMemTemp, L"/>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"LIBENABLE")) {
            if (prs && prs->m_fReadOnly)
               p->pszSubString = L"enabled=false";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"LIBREADONLY")) {
            if (prs && prs->m_fReadOnly)
               p->pszSubString = L"readonly=true";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"HOTSPOTS")) {
            MemZero (&gMemTemp);

            DWORD i;
            PIHOTSPOT ph = (PIHOTSPOT)prs->m_lIHOTSPOT.Get(0);
            WCHAR szColor[32];
            for (i = 0; i < prs->m_lIHOTSPOT.Num(); i++, ph++) {
               ColorToAttrib (szColor, HotSpotColor(i));

               MemCat (&gMemTemp, L"<tr><td bgcolor=");
               MemCat (&gMemTemp, szColor);
               MemCat (&gMemTemp, L"><bold>Message to send for pixels (");
               MemCat (&gMemTemp, ph->rPosn.left);
               MemCat (&gMemTemp, L", ");
               MemCat (&gMemTemp, ph->rPosn.top);
               MemCat (&gMemTemp, L") to (");
               MemCat (&gMemTemp, ph->rPosn.right);
               MemCat (&gMemTemp, L", ");
               MemCat (&gMemTemp, ph->rPosn.bottom);
               MemCat (&gMemTemp, L")</bold> - If the user "
			               L"clicks on this range then send the following message. The drop-down listbox "
			               L"lets you control the icon over the region."
		                  L"</td>"
		                  L"<td bgcolor=");
               MemCat (&gMemTemp, szColor);
               MemCat (&gMemTemp, L"><bold><edit width=100% maxchars=256 ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=hotmsg");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></bold><br/>"
			               L"<xComboCursor ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=hotcursor");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/><br/>");
			      MemCat (&gMemTemp, L"<button ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=hotdel");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Remove this</bold></button>"
		                  L"</td></tr>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"OBJECTS")) {
            MemZero (&gMemTemp);
            PCWorldSocket pWorld = WorldGet();

            DWORD i;
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(0);
            for (i = 0; i < prs->m_lRSOBJECT.Num(); i++) {
               PCObjectSocket pos = pWorld->ObjectGet(i);
               PWSTR psz = pos ? pos->StringGet(OSSTRING_NAME) : NULL;
               if (!psz)
                  psz = L"Unknown";

               MemCat (&gMemTemp, L"<tr><td><bold>");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold><p/>");

               if (i) {
                  // button to move to top of list
                  MemCat (&gMemTemp,
                     L"<button style=uparrow name=objtop");
                  MemCat (&gMemTemp, (int)i);
                  if (prs->m_fReadOnly)
                     MemCat (&gMemTemp, gpszEnabledFalse);
                  MemCat (&gMemTemp,
                     L"><bold>Top of list</bold>"
                     L"<xHoverHelp>Moves the object to the top of the list so it's easier to modify.</xHoverHelp>"
                     L"</button>"
                     L"<br/>");
               }

               MemCat (&gMemTemp,
                  L"<button name=objdel");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"><bold>Delete this</bold></button>"

                  L"</td>"
                  L"<td>"
                  L"<align align=right><bold>"
                  L"X: <edit width=80% maxchars=64 name=objloc0");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/><br/>"
                  L"Y: <edit width=80% maxchars=64 name=objloc1");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/><br/>"
                  L"Z: <edit width=80% maxchars=64 name=objloc2");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/>"
                  L"<p/>"
                  L"Rot: <edit width=80% maxchars=64 name=objrot2");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/><br/>"
                  L"Pitch: <edit width=80% maxchars=64 name=objrot0");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/><br/>"
                  L"Yaw: <edit width=80% maxchars=64 name=objrot1");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/>"
                  L"</bold></align>"
                  L"</td>"
                  L"</tr>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"RSATTRIB")) {
            MemZero (&gMemTemp);

            PCWorldSocket pWorld = WorldGet();

            DWORD i;
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(0);
            for (i = 0; i < prs->m_lRSATTRIB.Num(); i++, pr++) {
               MemCat (&gMemTemp, L"<tr><td><bold>");

               PCObjectSocket pObject;
               if (prs->m_fLoadFromFile)
                  pObject = pWorld->ObjectGet(pWorld->ObjectFind(&pr->gObject));
               else
                  pObject = pWorld->ObjectGet(pr->dwObject);

               PWSTR psz = pObject ? pObject->StringGet(OSSTRING_NAME) : NULL;
               if (!psz)
                  psz = L"Non-existant";

               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold>"
                  L"</td>"

                  L"<td>"
			         L"<combobox width=100% cbheight=300 ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=rsattribnum");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L">");
               ObjectAttribToCombo (pObject, &gMemTemp);
               MemCat (&gMemTemp, L"</combobox>"
		            L"</td>"

		            L"<td><bold><edit width=100% maxchars=64 ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=rsattribval");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></bold><br/>");
			      MemCat (&gMemTemp, L"<button ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=rsattribdel");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Remove this</bold></button>"
                  L"</td></tr>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!wcsicmp(p->pszSubName, L"RSCOLOR")) {
            MemZero (&gMemTemp);

            PCWorldSocket pWorld = WorldGet();

            DWORD i;
            PRSCOLOR prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(0);
            for (i = 0; i < prs->m_lRSCOLOR.Num(); i++, prc++) {
               MemCat (&gMemTemp, L"<tr><td><bold>");

               PCObjectSocket pObject;
               if (prs->m_fLoadFromFile)
                  pObject = pWorld->ObjectGet(pWorld->ObjectFind(&prc->gObject));
               else
                  pObject = pWorld->ObjectGet(prc->dwObject);

               PWSTR psz = pObject ? pObject->StringGet(OSSTRING_NAME) : NULL;
               if (!psz)
                  psz = L"Non-existant";

               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold>"
                  L"</td>"

                  L"<td>");
               MemCat (&gMemTemp, (int)prc->pSurf->m_dwID);
               MemCat (&gMemTemp,
		            L"</td>"

		            L"<td>");

               WCHAR szName[256];
               if (prc->pSurf->m_fUseTextureMap) {
                  szName[0] = 0;
                  TextureNameFromGUIDs (&prc->pSurf->m_gTextureCode, &prc->pSurf->m_gTextureSub,
                     NULL, NULL, szName);
                  MemCatSanitize (&gMemTemp, szName);
               }
               else
                  MemCat (&gMemTemp, L"Color");

               MemCat (&gMemTemp, L"<br/>");
			      MemCat (&gMemTemp, L"<button ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=rscolordel");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Remove this</bold></button>"
                  L"</td></tr>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CRenderScene::Edit - This brings up a dialog box for editing the object.

inputs
   HWND           hWnd - Window to bring dialog up from
   LANGID         lid - Language ID to use as default
   BOOL           fReadOnly - If TRUE then data is read only and cant be changed
   PCMIFLProj     pProj - Project it's it
returns
   BOOL - TRUE if changed, FALSE if didnt
*/
BOOL CRenderScene::Edit (HWND hWnd, LANGID lid, BOOL fReadOnly, PCMIFLProj pProj)
{
   m_fChanged = FALSE;
   m_hBmp = NULL;
   m_pImage = NULL;
   m_fReadOnly  = fReadOnly;
   m_lid = lid;
   m_iVScroll = 0;
   m_pProj = pProj;
   m_dwTab = m_fLoadFromFile ? 0 : 6;
   CEscWindow Window;

   // if any hotspots then fix the language id
   PIHOTSPOT phs = (PIHOTSPOT)m_lIHOTSPOT.Get(0);
   if (m_lIHOTSPOT.Num())
      m_lid = phs->lid;


   // render an initial pass
   {
      CProgress Progress;
      Progress.Start (hWnd, "Drawing...", TRUE);
      m_pImage = Render (SCALEPREVIEW, FALSE, TRUE, TRUE, &Progress);
      if (!m_pImage)
         goto done;
   }
   HDC hDC = GetDC (hWnd);
   m_hBmp = m_pImage->ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);
   if (!m_hBmp)
      goto done;

   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRENDERSCENE, RenderScenePage, this);
   m_iVScroll = Window.m_iExitVScroll;
   if (psz && !wcsicmp(psz, RedoSamePage()))
      goto redo;


done:
   if (m_hBmp)
      DeleteObject (m_hBmp);
   if (m_pImage)
      delete m_pImage;
   return m_fChanged;
}

/*************************************************************************************
MIFImage - String for an image name.
*/
PWSTR MIFImage (void)
{
   return L"Image";
}

/*************************************************************************************
MIF3DScene - String for an 3D scene
*/
PWSTR MIF3DScene (void)
{
   return L"ThreeDScene";
}

/*************************************************************************************
MIF3DObjects - String for an 3D scene
*/
PWSTR MIF3DObjects (void)
{
   return L"ThreeDObjects";
}


// BUGBUG - with 360 degree view hotspots are treated as wraparound

// BUGBUG - when have 360 degrees some camera movements (like angle) may be disabled
