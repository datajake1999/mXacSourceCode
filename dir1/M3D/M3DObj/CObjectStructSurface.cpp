/************************************************************************
CObjectStructSurface.cpp - Draws a box.

begun 12/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gszPoints = L"points%d-%d";


typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;


typedef struct {
   PCObjectStructSurface   pThis;
   int                     iSide;   // 1 if clicked on a, -1 if b, 0 if unknown
} DSS, *PDSS;


static SPLINEMOVEP   gaSplineMovePWall[] = {
   L"Center bottom", 0, 1,
   L"Lower left corner", -1, 1,
   L"Lower right corner", 1, 1,
   L"Upper left corner", -1, -1,
   L"Upper right corner", 1, -1
};

static SPLINEMOVEP   gaSplineMovePRoof[] = {
   L"Center", 0, 0,
   L"Lower left corner", -1, 1,
   L"Lower right corner", 1, 1,
   L"Upper left corner", -1, -1,
   L"Upper right corner", 1, -1
};
#define     SURFACEEDGE       10
#define     SIDEA             100
#define     SIDEB             200
#define     MAXCOLORPERSIDE   99          // maximum number of color settings per side



/**********************************************************************************
CObjectStructSurface::Constructor and destructor

(DWORD)pParams is passed into CDoubleSurface::Init(). See definitions of parameters

 */
CObjectStructSurface::CObjectStructSurface (PVOID pParams, POSINFO pInfo)
{
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;

   m_ds.Init (m_OSINFO.dwRenderShard, m_dwType, (PCObjectTemplate) this);
   m_dwRenderShow = RENDERSHOW_WALLS | RENDERSHOW_FLOORS | RENDERSHOW_ROOFS |
      RENDERSHOW_FRAMING;

   m_dwDisplayControl = 0; // start with drag

   BOOL fInternal, fCurved, fCementBlock, fDropCeiling;
   fInternal = fCurved = fCementBlock = fDropCeiling = FALSE;
   switch (HIWORD(m_dwType)) {
   case 1:  // wall
      m_dwDisplayControl = 2; // start with rotation

      switch (LOWORD(m_dwType)) {
      case 1:
      case 3:
      case 5:
      case 7:
         fInternal = TRUE;
         break;
      default:
         fInternal = FALSE;
         break;
      }
      switch (LOWORD(m_dwType)) {
      case 3:
      case 4:
      case 7:
      case 8:
         fCurved = TRUE;
         break;
      default:
         fCurved = FALSE;
         break;
      }
      switch (LOWORD(m_dwType)) {
      case 5:
      case 6:
      case 7:
      case 8:
         fCementBlock = TRUE;
         break;
      default:
         fCementBlock = FALSE;
      }
      break;
   case 2:  // roof/ceiling
      switch (LOWORD(m_dwType)) {
      case 2:
      case 3:
         fCurved = TRUE;
         break;
      default:
         fCurved = FALSE;
      }
      break;
   case 3:  // floor/ceiling
      fInternal = TRUE;
      switch (LOWORD(m_dwType)) {
      case 1:  // slab
         fCementBlock = TRUE;
         break;
      case 2:  // floor
         break;
      case 3:  // drop ceiling
         fDropCeiling = TRUE;
         break;
      }
      break;
   }

   switch (HIWORD(m_dwType)) {
   case 2:   // roof
      // rotate so it's at a 30 degree angle
      fp fAngle;
      fAngle = -PI/2 * 2.0 / 3.0;
      if (fCurved)
         fAngle = (LOWORD(m_dwType) == 3) ? (-PI/4) : (-PI/2);
      m_MatrixObject.RotationX (fAngle);
      break;

   case 3:
      // rotate so it's at a 90 degree angle
      fAngle = -PI/2;
      m_MatrixObject.RotationX (fAngle);
      break;
   }



}


CObjectStructSurface::~CObjectStructSurface (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectStructSurface::Delete - Called to delete this object
*/
void CObjectStructSurface::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectStructSurface::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectStructSurface::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   m_ds.Render (m_OSINFO.dwRenderShard, pr, &m_Renderrs);

   m_Renderrs.Commit();

}



/**********************************************************************************
CObjectStructSurface::QueryBoundingBox - Standard API
*/
void CObjectStructSurface::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   m_ds.QueryBoundingBox (pCorner1, pCorner2);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectStructSurface::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectStructSurface::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectStructSurface::Clone (void)
{
   PCObjectStructSurface pNew;

   pNew = new CObjectStructSurface(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);
   m_ds.CloneTo (&pNew->m_ds, pNew);

   pNew->m_dwDisplayControl = m_dwDisplayControl;

   return pNew;
}

static PWSTR gpszDisplayControl = L"DisplayControl";



PCMMLNode2 CObjectStructSurface::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   PCMMLNode2 pSub;
   pSub = m_ds.MMLTo();
   if (pSub) {
      pSub->NameSet (L"DoubleSurface");
      pNode->ContentAdd (pSub);
   }


   MMLValueSet (pNode, gpszDisplayControl, (int) m_dwDisplayControl);

   return pNode;
}

BOOL CObjectStructSurface::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // member variables go here
   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, L"DoubleSurface"))
         m_ds.MMLFrom (pSub);
   }

   m_dwDisplayControl = (DWORD) MMLValueGetInt (pNode, gpszDisplayControl, 0);
   return TRUE;
}


/*************************************************************************************
CObjectStructSurface::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectStructSurface::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = .2;

   if (m_dwDisplayControl == 0) {
      // control points for spline
      DWORD dwWidth, dwHeight;
      DWORD dwX, dwY, dwSurf;
      dwWidth = m_ds.ControlNumGet(TRUE);
      dwHeight = m_ds.ControlNumGet(FALSE);
      dwX = dwID % dwWidth;
      dwY = (dwID / dwWidth) % dwHeight;
      dwSurf = dwID / (dwWidth * dwHeight);
      if (dwSurf > 1)
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
//      pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Curvature");

      // start location in mid point
      int iSurf;
      iSurf = dwSurf ? -1 : 1;
      m_ds.ControlPointGet (iSurf, dwX, dwY, &pInfo->pLocation);

      // determine if its constrained in any way
      BOOL fConst[3];
      fConst[0] = fConst[1] = fConst[2] = FALSE;
      if (m_ds.m_fConstBottom && (dwY == dwHeight-1)) {
         fConst[2] = TRUE;
      }
#if 0
      if (m_ds.m_fConstPlane && ((dwY == dwHeight-1) || (dwY == 0)) && ((dwX == dwWidth-1) || (dwX==0) )) {
         fConst[1] = TRUE;
      }
#endif // 0
      if (fConst[2] && fConst[1]) {
         // can only move in X
         // BUGFIX - Since freedom != 0 causes problems, use freedom=0: pInfo->dwFreedom = 2;
         //pInfo->pV1.Zero();
         //pInfo->pV1.p[0] = 1;
      }
      else if (fConst[2]) {
         // can move in X and Y
         // BUGFIX - Since freedom != 0 causes problems, use freedom=0: pInfo->dwFreedom = 1;
         //pInfo->pV1.Zero();
         //pInfo->pV1.p[0] = 1;
         //pInfo->pV2.Zero();
         //pInfo->pV2.p[1] = 1;
      }
      else if (fConst[1]) {
         // can move in X and Z
         // BUGFIX - Since freedom != 0 causes problems, use freedom=0: pInfo->dwFreedom = 1;
         //pInfo->pV1.Zero();
         //pInfo->pV1.p[0] = 1;
         //pInfo->pV2.Zero();
         //pInfo->pV2.p[2] = 1;
      }

      return TRUE;
   }
   else if (m_dwDisplayControl == 1) { // edges
      // control points for spline
      DWORD dwNum;
      DWORD dwX, dwSurf;
      dwNum = m_ds.EdgeOrigNumPointsGet();
      dwX = dwID % dwNum;
      dwSurf = dwID / dwNum;
      if (dwSurf > 1)
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0xff,0x00);
      wcscpy (pInfo->szName, L"Edge");

      // get the point
      CPoint pHV;
      m_ds.EdgeOrigPointGet (dwX, &pHV);
      if (dwSurf)
         m_ds.HVToInfo (-1, 1.0 - pHV.p[0], pHV.p[1], &pInfo->pLocation);
      else
         m_ds.HVToInfo (1, pHV.p[0], pHV.p[1], &pInfo->pLocation);

      return TRUE;
   }
   else if (m_dwDisplayControl == 2) { // rotation
      // control points for spline
      DWORD dwWidth, dwHeight;
      DWORD dwX, dwY, dwSurf;
      dwWidth = m_ds.ControlNumGet(TRUE);
      dwHeight = m_ds.ControlNumGet(FALSE);
      dwX = (dwID % 2) ? (dwWidth-1) : 0;
      dwY = ((dwID / 2) % 2) ? (dwHeight-1) : 0;
      dwSurf = dwID / 4;
      if (dwSurf > 0)
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0, 0,0xff);
      wcscpy (pInfo->szName, L"Rotation/stretch point");

      // start location in mid point
      m_ds.ControlPointGet (0, dwX, dwY, &pInfo->pLocation);

      return TRUE;
   }
   else {   // see if it's one of the overlays
      return m_ds.ControlPointQuery (m_dwDisplayControl, dwID, pInfo);
   }

   return FALSE;
}

/*************************************************************************************
CObjectStructSurface::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectStructSurface::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (m_dwDisplayControl == 0) {
      DWORD dwWidth, dwHeight;
      DWORD dwX, dwY, dwSurf;
      dwWidth = m_ds.ControlNumGet(TRUE);
      dwHeight = m_ds.ControlNumGet(FALSE);
      dwX = dwID % dwWidth;
      dwY = (dwID / dwWidth) % dwHeight;
      dwSurf = dwID / (dwWidth * dwHeight);
      if (dwSurf > 1)
         return FALSE;

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      // from the ID calculate the dimension and corner
      int iSide;
      iSide = dwSurf ? -1 : 1;
      m_ds.ControlPointSet (iSide, dwX, dwY, pVal);

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }
   else if (m_dwDisplayControl == 1) { // edge control points
      DWORD dwNum;
      DWORD dwX, dwSurf;
      dwNum = m_ds.EdgeOrigNumPointsGet();
      dwX = dwID % dwNum;
      dwSurf = dwID / dwNum;
      if (dwSurf > 1)
         return FALSE;

      // get the old spline point
      CPoint pOldHV;
      m_ds.EdgeOrigPointGet (dwX, &pOldHV);
      TEXTUREPOINT tpOld, tpNew;
      tpOld.h = pOldHV.p[0];
      if (dwSurf)
         tpOld.h = 1.0 - tpOld.h;
      tpOld.v = pOldHV.p[1];

      // from the ID calculate the dimension and corner
      int iSide = dwSurf ? -1 : 1;

      // find out where it intersects
      if (!m_ds.HVDrag (iSide, &tpOld, pVal, pViewer, &tpNew))
         return FALSE;  // doesnt intersect
      if (dwSurf)
         tpNew.h = 1.0 - tpNew.h;

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      // set the new point
      CMem  memPoints;
      CMem  memSegCurve;
      DWORD dwOrig;
      dwOrig = m_ds.EdgeOrigNumPointsGet();
      if (!memPoints.Required (dwOrig * sizeof(CPoint)))
         return FALSE;
      if (!memSegCurve.Required (dwOrig * sizeof(DWORD)))
         return FALSE;
      PCPoint paPoints;
      DWORD *padw;
      paPoints = (PCPoint) memPoints.p;
      padw = (DWORD*) memSegCurve.p;
      DWORD i;
      for (i = 0; i < dwOrig; i++)
         m_ds.EdgeOrigPointGet (i, paPoints+i);
      paPoints[dwX].p[0] = tpNew.h;
      paPoints[dwX].p[1] = tpNew.v;
      for (i = 0; i < dwOrig; i++)
         m_ds.EdgeOrigSegCurveGet (i, padw + i);
      DWORD dwMinDivide, dwMaxDivide;
      fp fDetail;
      m_ds.EdgeDivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
      m_ds.EdgeInit (TRUE, dwOrig, paPoints, NULL, padw, dwMinDivide, dwMaxDivide, fDetail);

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }
   else if (m_dwDisplayControl == 2) { // rotation
      DWORD dwX, dwY, dwSurf;
      dwX = dwID % 2;
      dwY = (dwID / 2) % 2;
      dwSurf = dwID / 4;
      if (dwSurf > 0)
         return FALSE;

      if (!m_ds.ControlPointSetRotation (dwX, dwY, pVal, TRUE, TRUE))
         return FALSE;

      return TRUE;
   }
   else {   // see if it's one of the overlays
      return m_ds.ControlPointSet (m_dwDisplayControl, dwID, pVal, pViewer);
   }

   return FALSE;
}

/*************************************************************************************
CObjectStructSurface::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectStructSurface::ControlPointEnum (PCListFixed plDWORD)
{
   // 6 control points starting at 10
   DWORD i;

   if (m_dwDisplayControl == 0) {
      DWORD dwNum = m_ds.ControlNumGet(TRUE) * m_ds.ControlNumGet(FALSE) * 2;
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl == 1) { // edges
      DWORD dwNum = m_ds.EdgeOrigNumPointsGet() * 2;
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl == 2) { // rotation
      DWORD dwNum = 4;
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else {   // see if it's one of the overlays
      m_ds.ControlPointEnum (m_dwDisplayControl, plDWORD);
   }

}


/**********************************************************************************
CObjectStructSurface::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectStructSurface::DialogQuery (void)
{
   return TRUE;
}



/**********************************************************************************
CObjectStructSurface::DialogCPQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectStructSurface::DialogCPQuery (void)
{
   return TRUE;
}

/* SurfaceDialogPage
*/
BOOL SurfaceDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSS pdss = (PDSS)pPage->m_pUserData;
   PCObjectStructSurface pv = pdss->pThis;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // potentially disable the button for reducing size
         if (!pv->m_ds.ShrinkToCutouts (TRUE)) {
            PCEscControl pControl = pPage->ControlFind (L"reducetocutout");
            if (pControl)
               pControl->Enable(FALSE);
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp (p->pControl->m_pszName, L"reducetocutout")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_ds.ShrinkToCutouts (FALSE);
            pv->m_pWorld->ObjectChanged(pv);
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Surface settings";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WHICHSIDEA")) {
            p->pszSubString = L"";
            if (pdss->iSide == 1)
               p->pszSubString = L"<bold>You clicked on side A.</bold>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WHICHSIDEB")) {
            p->pszSubString = L"";
            if (pdss->iSide == -1)
               p->pszSubString = L"<bold>You clicked on side B.</bold>";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* SurfaceDisplayPage
*/
BOOL SurfaceDisplayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectStructSurface pv = (PCObjectStructSurface)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // set thecheckboxes
         pControl = NULL;
         switch (pv->m_dwDisplayControl) {
         case 0:  // curvature
            pControl = pPage->ControlFind (L"curve");
            break;
         case 1:  // edge
            pControl = pPage->ControlFind (L"edge");
            break;
         case 2:  // rotation
            pControl = pPage->ControlFind (L"rotate");
            break;
         default:
            {
               WCHAR szTemp[32];
               swprintf (szTemp, L"xc%d", (int) pv->m_dwDisplayControl);
               pControl = pPage->ControlFind (szTemp);
            }

         }
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // find otu which is checked
         PCEscControl pControl;
         DWORD dwNew;
         if ((pControl = pPage->ControlFind (L"curve")) && pControl->AttribGetBOOL(Checked()))
            dwNew = 0;  // curve;
         else if ((pControl = pPage->ControlFind (L"edge")) && pControl->AttribGetBOOL(Checked()))
            dwNew = 1;  // edge
         else if ((pControl = pPage->ControlFind (L"rotate")) && pControl->AttribGetBOOL(Checked()))
            dwNew = 2;  // rotation
         else if (p->pControl->AttribGetBOOL(Checked()) && (p->pControl->m_pszName[0] == L'x') && (p->pControl->m_pszName[1] == L'c')) {
            dwNew = _wtoi(p->pControl->m_pszName + 2);
         }
         else
            break;   // none of the above
         if (dwNew == pv->m_dwDisplayControl)
            return TRUE;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDisplayControl = dwNew;
         pv->m_pWorld->ObjectChanged (pv);

         // make sure they're visible
         pv->m_pWorld->SelectionClear();
         GUID gObject;
         pv->GUIDGet (&gObject);
         pv->m_pWorld->SelectionAdd (pv->m_pWorld->ObjectFind(&gObject));
         return TRUE;

      }
      break;   // default


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Which control points are displayed";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DISPLAY")) {
            MemZero (&gMemTemp);


            // first, calculate all the choices for overlays
            CMem  memGroup;
            MemZero (&memGroup);
            if (HIWORD(pv->m_dwType) != 1)
               MemCat (&memGroup, L"curve,edge");
            else
               MemCat (&memGroup, L"rotate,curve,edge");
            DWORD i, j;
            PWSTR psz;
            PTEXTUREPOINT ptp;
            DWORD dwNum;
            DWORD dwColor;
            BOOL fClockwise;
            PCSplineSurface pss;
            for (j = 0; j < 2; j++) {
               pss = j ? &pv->m_ds.m_SplineB : &pv->m_ds.m_SplineA;
               for (i = 0; i < pss->OverlayNum(); i++) {
                  if (!pss->OverlayEnum (i, &psz, &ptp, &dwNum, &fClockwise))
                     continue;
                  dwColor = OverlayShapeNameToColor(psz);
                  if (dwColor == (DWORD)-1)
                     continue;
                  MemCat (&memGroup, L",xc");
                  MemCat (&memGroup, (int) dwColor);
               }
            }

            // then put in the buttons
            if (HIWORD(pv->m_dwType) == 1) {
               MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
               MemCat (&gMemTemp, (PWSTR) memGroup.p);
               MemCat (&gMemTemp, L" name=rotate><bold>Rotation</bold><br/>");
               MemCat (&gMemTemp, L"Adjusting the control points will stretch the surface "
                  L"and/or rotate it. <italic>(Only recommended for walls.)</italic></xChoiceButton>");
            }

            MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
            MemCat (&gMemTemp, (PWSTR) memGroup.p);
            MemCat (&gMemTemp, L" name=curve><bold>Curvature</bold><br/>");
            MemCat (&gMemTemp, L"These control points let you curve the surface.</xChoiceButton>");

            MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
            MemCat (&gMemTemp, (PWSTR) memGroup.p);
            MemCat (&gMemTemp, L" name=edge><bold>Edge</bold><br/>"
               L"Show these control points to cut away the edge of the surface so it isn't "
               L"rectangluar.</xChoiceButton>");

            for (j = 0; j < 2; j++) {
               pss = j ? &pv->m_ds.m_SplineB : &pv->m_ds.m_SplineA;
               for (i = 0; i < pss->OverlayNum(); i++) {
                  if (!pss->OverlayEnum (i, &psz, &ptp, &dwNum, &fClockwise))
                     continue;
                  dwColor = OverlayShapeNameToColor(psz);
                  if (dwColor == (DWORD)-1)
                     continue;
                  WCHAR szTemp[128];
                  wcscpy (szTemp, psz);
                  if (wcschr(szTemp, L':'))
                     (wcschr(szTemp, L':'))[0] = 0;

                  MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
                  MemCat (&gMemTemp, (PWSTR) memGroup.p);
                  MemCat (&gMemTemp, L" name=xc");
                  MemCat (&gMemTemp, (int)dwColor);
                  MemCat (&gMemTemp, L"><bold>");
                  MemCatSanitize (&gMemTemp, szTemp);
                  MemCat (&gMemTemp, j ? L" (on side B)" : L" (on side A)");
                  MemCat (&gMemTemp, L"</bold><br/>");
                  MemCat (&gMemTemp, L"Control points for a color/texture overlay.</xChoiceButton>");
               }
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/**********************************************************************************
CObjectStructSurface::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectStructSurface::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   DSS dss;
   dss.pThis = this;
   dss.iSide = 0;
   PCLAIMSURFACE pcs;
   pcs = m_ds.ClaimFindByID (dwSurface);
   if (pcs) switch (pcs->dwReason) {
      case 0:
         dss.iSide = 1;
         break;
      case 1:
         dss.iSide = -1;
         break;
   }

firstpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEDIALOG, SurfaceDialogPage, &dss);
firstpage2:
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"thickness")) {
      pszRet = m_ds.SurfaceThicknessPage(pWindow);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"framing")) {
      pszRet = m_ds.SurfaceFramingPage(pWindow);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"detail")) {
      pszRet = m_ds.SurfaceDetailPage (pWindow);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"bevelling")) {
      pszRet = m_ds.SurfaceBevellingPage (pWindow);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"curvesegment")) {
      pszRet = m_ds.SurfaceCurvaturePage (pWindow);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"controlpoints")) {
      pszRet = m_ds.SurfaceControlPointsPage (pWindow);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"hideclad")) {
      pszRet = m_ds.SurfaceShowCladPage (pWindow);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"displaycontrol")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEDISPLAY, SurfaceDisplayPage, this);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"edgemodify")) {
      pszRet = m_ds.SurfaceEdgePage (pWindow);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"edgeinter")) {
      pszRet = m_ds.SurfaceEdgeInterPage (pWindow);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"floors")) {
      pszRet = m_ds.SurfaceFloorsPage (pWindow);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"oversidea") || !_wcsicmp(pszRet, L"oversideb")) {
      BOOL fSideA = !_wcsicmp(pszRet, L"oversidea");
      pszRet = m_ds.SurfaceOverMainPage (pWindow, fSideA, &m_dwDisplayControl);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"cutouta") || !_wcsicmp(pszRet, L"cutoutb")) {
      BOOL fSideA = !_wcsicmp(pszRet, L"cutouta");
      pszRet = m_ds.SurfaceCutoutMainPage (pWindow, fSideA);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }

   return !_wcsicmp(pszRet, Back());
}



/**********************************************************************************
CObjectStructSurface::DialogCPShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectStructSurface::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   DSS dss;
   dss.pThis = this;
   dss.iSide = 0;
   PCLAIMSURFACE pcs;
   pcs = m_ds.ClaimFindByID (dwSurface);
   if (pcs) switch (pcs->dwReason) {
      case 0:
         dss.iSide = 1;
         break;
      case 1:
         dss.iSide = -1;
         break;
   }

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEDISPLAY, SurfaceDisplayPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}


/**************************************************************************************
CObjectTemplate::MoveReferencePointQuery - 
given a move reference index, this fill in pp with the position of
the move reference RELATIVE to ObjectMatrixGet. References are numbers
from 0+. If the index is more than the number of points then the
function returns FALSE

inputs
   DWORD       dwIndex - index.0 .. # ref
   PCPoint     pp - Filled with point relative to ObjectMatrixGet() IF its valid
returns
   BOOL - TRUE if valid index.
*/
BOOL CObjectStructSurface::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = (HIWORD(m_dwType)==1) ? gaSplineMovePWall : gaSplineMovePRoof;
   dwDataSize = (HIWORD(m_dwType)==1) ? sizeof(gaSplineMovePWall) : sizeof(gaSplineMovePRoof);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   DWORD dwWidth, dwHeight;
   DWORD dwX, dwY;
   dwWidth = m_ds.ControlNumGet(TRUE);
   dwHeight = m_ds.ControlNumGet(FALSE);
   dwX = (ps[dwIndex].iX < 0) ? 0 : (dwWidth-1);
   dwY = (ps[dwIndex].iY < 0) ? 0 : (dwHeight-1);
   
   // special case for center bottom
   if ((ps[dwIndex].iX == 0) && (ps[dwIndex].iY != 0)) {
      CPoint p2;
      dwX = 0;
      m_ds.ControlPointGet (0, dwX, dwY, &p2);
      dwX = dwWidth-1;
      m_ds.ControlPointGet (0, dwX, dwY, pp);
      pp->Add (&p2);
      pp->Scale (.5);
      return TRUE;
   }

   // special case for center center
   if ((ps[dwIndex].iX == 0) && (ps[dwIndex].iY == 0)) {
      CPoint p2, p3, p4;
      m_ds.ControlPointGet (0, 0, 0, &p2);
      m_ds.ControlPointGet (0, dwWidth-1, 0, &p3);
      m_ds.ControlPointGet (0, dwWidth-1, dwHeight-1, &p4);
      m_ds.ControlPointGet (0, 0, dwHeight-1, pp);
      pp->Add (&p2);
      pp->Add (&p3);
      pp->Add (&p4);
      pp->Scale (1.0 / 4.0);
      return TRUE;
   }

   return m_ds.ControlPointGet (0, dwX, dwY, pp);
}

/**************************************************************************************
CObjectStructSurface::MoveReferenceStringQuery -
given a move reference index (numbered from 0 to the number of references)
this fills in a string at psz and dwSize BYTES that names the move reference
to the end user. *pdwNeeded is filled with the number of bytes needed for
the string. Returns FALSE if dwIndex is too high, or dwSize is too small (although
pdwNeeded will be filled in)

inputs
   DWORD       dwIndex - index. 0.. # ref
   PWSTR       psz - To be filled in witht he string
   DWORD       dwSize - # of bytes available in psz
   DWORD       *pdwNeeded - If not NULL, filled with the size needed
returns
   BOOL - TRUE if psz copied.
*/
BOOL CObjectStructSurface::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = (HIWORD(m_dwType)==1) ? gaSplineMovePWall : gaSplineMovePRoof;
   dwDataSize = (HIWORD(m_dwType)==1) ? sizeof(gaSplineMovePWall) : sizeof(gaSplineMovePRoof);
   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   DWORD dwNeeded;
   dwNeeded = ((DWORD)wcslen (ps[dwIndex].pszName) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, ps[dwIndex].pszName);
      return TRUE;
   }
   else
      return FALSE;
}


/**********************************************************************************
CObjectStructSurface::ContHVQuery
Called to see the HV location of what clicked on, or in other words, given a line
from pEye to pClick (object sapce), where does it intersect surface dwSurface (LOWORD(PIMAGEPIXEL.dwID))?
Fills pHV with the point. Returns FALSE if doesn't intersect, or invalid surface, or
can't tell with that surface. NOTE: if pOld is not NULL, then assume a drag from
pOld (converted to world object space) to pClick. Finds best intersection of the
surface then. Useful for dragging embedded objects around surface
*/
BOOL CObjectStructSurface::ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV)
{
   return m_ds.ContHVQuery (pEye, pClick, dwSurface, pOld, pHV);
}


/**********************************************************************************
CObjectTemplate::ContCutout
Called by an embeded object to specify an arch cutout within the surface so the
object can go through the surface (like a window). The container should check
that pgEmbed is a valid object. dwNum is the number of points in the arch,
paFront and paBack are the container-object coordinates. (See CSplineSurface::ArchToCutout)
If dwNum is 0 then arch is simply removed. If fBothSides is true then both sides
of the surface are cleared away, else only the side where the object is embedded
is affected.

*/
BOOL CObjectStructSurface::ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides)
{
   return m_ds.ContCutout (pgEmbed, dwNum, paFront, paBack, fBothSides);
}

/**********************************************************************************
CObjectStructSurface::ContCutoutToZipper -
Assumeing that ContCutout() has already been called for the pgEmbed, this askes the surfaces
for the zippering information (CRenderSufrace::ShapeZipper()) that would perfectly seal up
the cutout. plistXXXHVXYZ must be initialized to sizeof(HVXYZ). plisstFrontHVXYZ is filled with
the points on the side where the object was embedded, and plistBackHVXYZ are the opposite side.
NOTE: These are in the container's object space, NOT the embedded object's object space.
*/
BOOL CObjectStructSurface::ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ)
{
   return m_ds.ContCutoutToZipper (pgEmbed, plistFrontHVXYZ, plistBackHVXYZ);
}


/**********************************************************************************
CObjectStructSurface::ContThickness - 
returns the thickness of the surface (dwSurface) at pHV. Used by embedded
objects like windows to know how deep they should be.

NOTE: usually overridden
*/
fp CObjectStructSurface::ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV)
{
   return m_ds.ContThickness (dwSurface, pHV);
}


/**********************************************************************************
CObjectStructSurface::ContSideInfo -
returns a DWORD indicating what class of surface it is... right now
0 = internal, 1 = external. dwSurface is the surface. If fOtherSide
then want to know what's on the opposite side. Returns 0 if bad dwSurface.

NOTE: usually overridden
*/
DWORD CObjectStructSurface::ContSideInfo (DWORD dwSurface, BOOL fOtherSide)
{
   return m_ds.ContSideInfo (dwSurface, fOtherSide);
}


/**********************************************************************************
CObjectStructSurface::ContMatrixFromHV - THE SUPERCLASS SHOULD OVERRIDE THIS. Given
a point on a surface (that supports embedding), this returns a matrix that translates
0,0,0 to the same in world space as where pHV is, and also applies fRotation around Y (clockwise)
so that X and Z are (for the most part) still on the surface.

inputs
   DWORD             dwSurface - Surface ID that can be embedded. Should check to make sure is valid
   PTEXTUREPOINT     pHV - HV, 0..1 x 0..1 locaiton within the surface
   fp            fRotation - Rotation in radians, clockwise.
   PCMatrix          pm - Filled with the new rotation matrix
returns
   BOOL - TRUE if success
*/
BOOL CObjectStructSurface::ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm)
{
   if (!m_ds.ContMatrixFromHV (dwSurface, pHV, fRotation, pm))
      return FALSE;


   // then rotate by the object's rotation matrix to get to world space
   pm->MultiplyRight (&m_MatrixObject);

   return TRUE;
}


/**********************************************************************************
CObjectStructSurface::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectStructSurface::Message (DWORD dwMessage, PVOID pParam)
{
   // NOTE: At some point may want to trap some messages
   switch (dwMessage) {
   case OSM_STRUCTSURFACE:
      {
         POSMSTRUCTSURFACE p = (POSMSTRUCTSURFACE) pParam;
         p->poss = this;
      }
      return TRUE;

   case OSM_WALLMOVED:
      {
         POSMWALLMOVED p = (POSMWALLMOVED) pParam;

         // only supported by walls
         if (HIWORD(m_dwType) != 1)
            return FALSE;

         // convert this point into spline space
         CMatrix m, mInv;
         m_ds.MatrixGet (&m);
         m.MultiplyRight (&m_MatrixObject);
         m.Invert4 (&mInv);

         // convert
         CPoint pOld, pNew;
         pOld.Copy (&p->pOld);
         pNew.Copy (&p->pNew);
         pOld.p[3] = pNew.p[3] = 1;
         pOld.MultiplyLeft (&mInv);
         pNew.MultiplyLeft (&mInv);

         // if is close to either corner
         CPoint pv;
         DWORD dwWidth, dwHeight;
         dwWidth = m_ds.ControlNumGet(TRUE);
         dwHeight = m_ds.ControlNumGet(FALSE);
         m_ds.m_SplineCenter.ControlPointGet (0, dwHeight-1, &pv);
         pv.Subtract (&pOld);
         if (pv.Length() < .01) {   // allow up to 1 cm away
            m_ds.ControlPointSetRotation (0, 1, &pNew, FALSE, TRUE);
            return TRUE;
         }
         m_ds.m_SplineCenter.ControlPointGet (dwWidth-1, dwHeight-1, &pv);
         pv.Subtract (&pOld);
         if (pv.Length() < .01) {   // allow up to 1 cm away
            m_ds.ControlPointSetRotation (1, 1, &pNew, FALSE, TRUE);
            return TRUE;
         }
      }
      return TRUE;
   }

   return m_ds.Message (dwMessage, pParam);
}

/*************************************************************************************
CObjectStructSurface::IntelligentAdjust
Tells the object to intelligently adjust itself based on nearby objects.
For walls, this means triming to the roof line, for floors, different
textures, etc. If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden
*/
BOOL CObjectStructSurface::IntelligentAdjust (BOOL fAct)
{
   if (!((HIWORD(m_dwType) == 1) || (HIWORD(m_dwType) == 2)) )
      return FALSE;
   if (!fAct)
      return TRUE;
   if (!m_pWorld)
      return FALSE;

   // BUGFIX - Just do an integrity check while at it
   m_ds.EmbedIntegrityCheck();

   BOOL fRoof;
   fRoof = (HIWORD(m_dwType) == 2);

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);
   m_ds.IntersectWithOtherSurfaces (0x0001, fRoof ? 1 : 0, NULL, FALSE);

   // if it's a wall then do the bevel
   if (HIWORD(m_dwType) == 1)
      m_ds.WallIntelligentBevel ();

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}

/**********************************************************************************
CObjectStructSurface::
Called to tell the container that one of its contained objects has been renamed.
*/
BOOL CObjectStructSurface::ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew)
{
   return m_ds.ContEmbeddedRenamed (pgOld, pgNew);
}

/*************************************************************************************
CObjectStructSurface::IntelligentPositionDrag - The template object is unable to chose an intelligent
position so it just retunrs FALSE.

inputs
   POSINTELLIPOS     pInfo - Information that might be useful to chose the position
returns
   BOOL - TRUE if the object has moved, rotated, scaled, itself to an intelligent
      location. FALSE if doesn't know and its up to the application.
*/
typedef struct {
   fp      fZ;      // height
   DWORD       dwCount; // number
} IPD, *PIPD;
BOOL CObjectStructSurface::IntelligentPositionDrag (CWorldSocket *pWorld, POSINTELLIPOSDRAG pInfo,  POSINTELLIPOSRESULT pResult)
{
   // only allow dragging if wall
   if (HIWORD(m_dwType) != 1)
      return FALSE;

   // walls can be dragged
   if (!pWorld && !pInfo && !pResult)
      return TRUE;

   // some parameter checks
   if (!pWorld || !pInfo || !pResult || !pInfo->paWorldCoord || !pInfo->dwNumWorldCoord)
      return FALSE;

   // create a list of all the points that dragged over to determine height - which
   // is all care about with walls
   CListFixed lHeight;
   lHeight.Init (sizeof(IPD));
   DWORD i, j;
   PIPD pi;
   for (i = 0; i < pInfo->dwNumWorldCoord; i++) {
      fp fZ = pInfo->paWorldCoord[i].p[2];

      // loop through the current list and see if can find same height
      pi = (PIPD)lHeight.Get(0);
      for (j = 0; j < lHeight.Num(); j++) {
         if (fabs(fZ - pi[j].fZ) < .01)
            break;
      }
      if (j < lHeight.Num()) {
         // average and increment the counter
         pi[j].fZ = ((pi[j].fZ * pi[j].dwCount) + fZ) / (fp)(pi[j].dwCount+1);
         pi[j].dwCount++;
      }
      else {
         // add it
         IPD id;
         id.dwCount = 1;
         id.fZ = fZ;
         lHeight.Add (&id);
      }
   }

   // look at what we have
   if (!lHeight.Num())
      return FALSE;
   pi = (PIPD) lHeight.Get(0);
   DWORD dwMax;
   dwMax = 0;
   for (i = 1; i < lHeight.Num(); i++)
      if (pi[i].dwCount > pi[dwMax].dwCount)
         dwMax = i;

   // have a height
   fp fHeight;
   fHeight = pi[dwMax].fZ;

   // now, look at intersect a point with the plane, running from
   // the eye to the plane
   CPoint apBoundary[2];
   CPoint pPlane, pPlaneN;
   pPlane.Zero();
   pPlane.p[2] = fHeight;
   pPlaneN.Zero();
   pPlaneN.p[2] = 1;
   for (i = 0; i < 2; i++) {
      CPoint pTo;
      pTo.Copy (&pInfo->paWorldCoord[i ? (pInfo->dwNumWorldCoord-1) : 0]);

      // look from
      CPoint pFrom;
      if (pInfo->fViewFlat) {
         pFrom.Subtract (&pInfo->pCamera, &pInfo->pLookAt);
         pFrom.Normalize();
         pFrom.Add (&pTo);
      }
      else {
         pFrom.Copy (&pInfo->pCamera);
      }

      // intersect
      if (!IntersectLinePlane (&pFrom, &pTo, &pPlane, &pPlaneN, &apBoundary[i]))
         return FALSE;  // for some reason not intersecting with plane we chose
   }

   // Deal with snap to other walls?
   DWORD dwEdge;
   OSMWALLQUERYEDGES wqTest, awqClosest[2];
   fp afWQDist[2];
   DWORD adwWQEdge[2];
   adwWQEdge[0] = adwWQEdge[1] = -1;
   for (dwEdge = 0; dwEdge < 2; dwEdge++) {
      // convert to world space
      CPoint pCorner1, pCorner2, pDelta;
      pDelta.Zero();
      pDelta.p[0] = pDelta.p[1] = pDelta.p[2] = .5;   // within .5m
      pCorner1.Subtract (&apBoundary[dwEdge], &pDelta);
      pCorner2.Add (&apBoundary[dwEdge], &pDelta);

      CListFixed list;
      CMatrix mIdent;
      mIdent.Identity();
      list.Init (sizeof(DWORD));
      list.Clear();
      pWorld->IntersectBoundingBox (&mIdent, &pCorner1, &pCorner2, &list);

      // sent messages out
      DWORD i, dwInter;
      for (i = 0; i < list.Num(); i++) {
         DWORD dwObject = *((DWORD*) list.Get(i));
         PCObjectSocket pos = pWorld->ObjectGet (dwObject);
         if (!pos)
            continue;

         if (!pos->Message (OSM_WALLQUERYEDGES, &wqTest))
            continue;

         for (dwInter = 0; dwInter < 2; dwInter++) {
            // if it's not intersecting then don't care
            if ( (fabs(wqTest.apCorner[dwInter].p[0] - apBoundary[dwEdge].p[0]) > .5)  ||
               (fabs(wqTest.apCorner[dwInter].p[1] - apBoundary[dwEdge].p[1]) > .5)  ||
               (fabs(wqTest.apCorner[dwInter].p[2] - apBoundary[dwEdge].p[2]) > .01) )   // allow z to be off by 1m
               continue;
            // BUGFIX - The Z-level for walls was 1m. This was WAY too much since what would
            // happen is would click and drag on the floor and would snap to a wall of
            // a building block. The problem is that the building block walls are slightly
            // lower because they need to be to cover the floor. This would cause the newly
            // created wall to be lower (or higher), which would then screw up intelligent ajust

            // BUGFIX - Snap to the closest one
            CPoint p;
            p.Subtract (&wqTest.apCorner[dwInter], &apBoundary[dwEdge]);
            fp fDist;
            fDist = p.Length();
            if ((adwWQEdge[dwEdge] == -1) || fDist < afWQDist[dwEdge]) {
               afWQDist[dwEdge] = fDist;
               adwWQEdge[dwEdge] = dwInter;
               awqClosest[dwEdge] = wqTest;
            }
         }  // over dwInter
      }  // over i


   }  // over dwEdge

   // BUGFIX - Snap to edge, make sure not snapping both edges of the wall to the
   // same location. If are, chose whichever one is closest to the original boundary
   // location and change only that
   if ((adwWQEdge[0] != adwWQEdge[1]) && awqClosest[0].apCorner[adwWQEdge[0]].AreClose(&awqClosest[1].apCorner[adwWQEdge[1]])) {
      CPoint a, b;
      a.Subtract (&apBoundary[0], &awqClosest[0].apCorner[adwWQEdge[0]]);
      b.Subtract (&apBoundary[1], &awqClosest[0].apCorner[adwWQEdge[0]]);
      if (a.Length() < b.Length())
         adwWQEdge[1] = -1;   // dont snap B
      else
         adwWQEdge[0] = -1;   // dont snap A
   }
   for (dwEdge = 0; dwEdge < 2; dwEdge++)
      // BUGFIX - Snap to the closest one of the items that found
      if (adwWQEdge[dwEdge] != -1) {
         // found a match
         apBoundary[dwEdge].p[0] = awqClosest[dwEdge].apCorner[adwWQEdge[dwEdge]].p[0];
         apBoundary[dwEdge].p[1] = awqClosest[dwEdge].apCorner[adwWQEdge[dwEdge]].p[1];
         // dont do z.
      }

   // if get here, boundary has start and end position
   // convert points to object space and set.
   // NOTE: Getting the matrix again each time because matrix may change each time set.
   CMatrix mInv;
   for (i = 0; i < 2; i++) {
      m_MatrixObject.Invert4 (&mInv);

      apBoundary[i].p[3] = 1;
      apBoundary[i].MultiplyLeft (&mInv);

      m_ds.ControlPointSetRotation (i, 1, &apBoundary[i], FALSE, TRUE);
   }


   // use the calculated matrix
   memset (pResult, 0, sizeof(*pResult));
   pResult->mObject.Copy (&m_MatrixObject);

   return TRUE;
}





/*****************************************************************************************
CObjectStructSurface::Merge -
asks the object to merge with the list of objects (identified by GUID) in pagWith.
dwNum is the number of objects in the list. The object should see if it can
merge with any of the ones in the list (some of which may no longer exist and
one of which may be itself). If it does merge with any then it return TRUE.
if no merges take place it returns false.
*/
BOOL CObjectStructSurface::Merge (GUID *pagWith, DWORD dwNum)
{
   BOOL fRet = FALSE;
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      DWORD dwFind;

      // make sure it's not this object
      if (IsEqualIID (pagWith[i], m_gGUID))
         continue;

      dwFind = m_pWorld->ObjectFind (&pagWith[i]);
      if (dwFind == -1)
         continue;
      PCObjectSocket pos;
      pos = m_pWorld->ObjectGet (dwFind);
      if (!pos)
         continue;

      // send a message to see if it is another struct surface
      OSMSTRUCTSURFACE os;
      memset (&os, 0, sizeof(os));
      if (!pos->Message (OSM_STRUCTSURFACE, &os))
         continue;
      if (!os.poss)
         continue;

      fRet |= Merge (os.poss);
   }

   return fRet;
}


/*****************************************************************************************
CObjectStructSurface::Merge - Merge with an individual struct surface object.

inputs
   PCObjectStructSurface      poss - Other object to merge with. This object will be delted
                              from the m_pWorld by this call. (If successful)
returns
   BOOL - TRUE if merged, FALSE if didn't merge (for whatever reason)
*/
BOOL CObjectStructSurface::Merge (CObjectStructSurface *poss)
{
   // dont merge with self
   if (poss == this)
      return FALSE;

   // make sure same thickness
   if ((fabs(m_ds.m_fThickA - poss->m_ds.m_fThickA) > CLOSE) ||
      (fabs(m_ds.m_fThickB - poss->m_ds.m_fThickB) > CLOSE) ||
      (fabs(m_ds.m_fThickStud - poss->m_ds.m_fThickStud) > CLOSE))
      return FALSE;

   // make sure 2x2
   if ((poss->m_ds.ControlNumGet (TRUE) != 2) || (poss->m_ds.ControlNumGet(FALSE) != 2) ||
      (m_ds.ControlNumGet (TRUE) != 2) || (m_ds.ControlNumGet(FALSE) != 2))
      return FALSE;

   // make sure not curved
   if ((poss->m_ds.SegCurveGet (TRUE, 0) != SEGCURVE_LINEAR) || (poss->m_ds.SegCurveGet (FALSE, 0) != SEGCURVE_LINEAR) ||
      (m_ds.SegCurveGet (TRUE, 0) != SEGCURVE_LINEAR) || (m_ds.SegCurveGet (FALSE, 0) != SEGCURVE_LINEAR))
      return FALSE;

   // create the matricies that translate from object space to world space
   CMatrix amObjectToWorld[2];
   DWORD i, x,y;
   for (i = 0; i < 2; i++)
      amObjectToWorld[i].Copy (&(i ? poss : this)->m_MatrixObject);

   // make sure flat and have same normal
   CPoint ap[2][2][2];
   CPoint an[2][2];
   CPoint a, b;
   memset (ap, 0, sizeof(ap));
   for (i = 0; i < 2; i++) for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
      (i ? poss : this)->m_ds.ControlPointGet (0, x, y, &ap[i][y][x]);
      ap[i][y][x].p[3] = 1;
      ap[i][y][x].MultiplyLeft (&amObjectToWorld[i]);
   }

   for (i = 0; i < 2; i++) for (x = 0; x < 2; x++) {
      if (x) {
         a.Subtract (&ap[i][0][1], &ap[i][0][0]);
         b.Subtract (&ap[i][0][1], &ap[i][1][1]);
      }
      else {
         a.Subtract (&ap[i][1][0], &ap[i][1][1]);
         b.Subtract (&ap[i][1][0], &ap[i][0][0]);
      }

      an[i][x].CrossProd (&a, &b);
      an[i][x].Normalize();
      if (an[i][x].Length() < .5)
         return FALSE;  // no normal detected

      // if they're not all the same then error
      if (an[i][x].DotProd(&an[0][0]) < .99)
         return FALSE;
   }

   // remember the UL point
   CPoint pULStart;
   pULStart.Copy (&ap[0][0][0]);

   // create a matrix that translates to a plane
   CPoint apDim[3];
   CMatrix mPlaneToWorld, mWorldToPlane, mTrans;
   apDim[1].Copy (&an[0][0]);
   apDim[1].Scale (-1); // so normal points in y=-1 direction
   apDim[0].Subtract (&ap[0][1][1], &ap[0][1][0]);
   apDim[2].CrossProd (&apDim[0], &apDim[1]);
   apDim[2].Normalize();
   if (apDim[2].Length() < .5)
      return FALSE;
   apDim[0].CrossProd (&apDim[1], &apDim[2]);
   apDim[0].Normalize();
   mPlaneToWorld.RotationFromVectors (&apDim[0], &apDim[1], &apDim[2]);
   mTrans.Translation (pULStart.p[0], pULStart.p[1], pULStart.p[2]);
   mPlaneToWorld.MultiplyRight (&mTrans);
   mPlaneToWorld.Invert4 (&mWorldToPlane);

   // make sure on the same plane
   for (i = 0; i < 2; i++) {
      a.Copy (&ap[i][0][0]);
      a.p[3] = 1;
      a.MultiplyLeft (&mWorldToPlane);
      if (fabs(a.p[1]) > .01) // BUGFIX - Was CLOSE, but thought to be more tolerant
         return FALSE;
   }

   // convert the corners onto the plane, and find min and max for entire thing and parts
   CPoint pMin, pMax;   // NOTE: pMax and pMin are used later on
   CPoint apMin[2], apMax[2];
   pMin.Zero();
   pMax.Zero();
   for (i = 0; i < 2; i++) for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
      ap[i][y][x].p[3] = 1;
      ap[i][y][x].MultiplyLeft (&mWorldToPlane);

      // per shape
      if ((x == 0) && (y == 0)) {
         apMin[i].Copy (&ap[i][y][x]);
         apMax[i].Copy (&apMin[i]);
      }
      else {
         apMin[i].Min (&ap[i][y][x]);
         apMax[i].Max (&ap[i][y][x]);
      }
   }
   pMin.Copy (&apMin[0]);
   pMin.Min (&apMin[1]);
   pMax.Copy (&apMax[0]);
   pMax.Max (&apMax[1]);

   // crude test to make sure they touch
   for (i = 0; i < 3; i++) {
      if (i == 1)
         continue;   // dont bother with y
      if ((apMax[0].p[i] + CLOSE*10 < apMin[1].p[i]) || (apMax[1].p[i] + CLOSE*10 < apMin[0].p[i]))
         return FALSE;  // dont intersect
   }

   // make a matrix that converts from HV space into plane space
   CMatrix mHVToPlane, mPlaneToHV;
   apDim[0].Zero();
   apDim[0].p[0] = pMax.p[0] - pMin.p[0];
   apDim[1].Zero();
   apDim[1].p[2] = -(pMax.p[2] - pMin.p[2]);
   apDim[2].Zero();
   apDim[2].p[1] = 1;   // just so can invert matrix
   mHVToPlane.RotationFromVectors (&apDim[0], &apDim[1], &apDim[2]);
   mTrans.Translation (pMin.p[0], 0, pMax.p[2]);
   mHVToPlane.MultiplyRight (&mTrans);
   mHVToPlane.Invert4 (&mPlaneToHV);

   // make two matrices that convert from object space into HV space
   CMatrix amObjectToHV[2];
   for (i = 0; i < 2; i++) {
      amObjectToHV[i].Multiply (&mWorldToPlane, &amObjectToWorld[i]);   // get object to plane
      amObjectToHV[i].MultiplyRight (&mPlaneToHV); // get object to HV
      }

   // create two lists for the edge points of the two surfaces, translated
   // into the new surface, as TEXTUREPOINT
   CListFixed alEdge[2];
   alEdge[0].Init (sizeof(TEXTUREPOINT));
   alEdge[1].Init (sizeof(TEXTUREPOINT));
   for (i = 0; i < 2; i++) {
      PCDoubleSurface p = &(i ? poss : this)->m_ds;
      for (x = 0; x < p->m_SplineEdge.QueryNodes(); x++) {
         // get the point
         TEXTUREPOINT tp;
         PCPoint pf;
         pf = p->m_SplineEdge.LocationGet (x);
         tp.h = pf->p[0];
         tp.v = pf->p[1];
         tp.h = min(1,tp.h);
         tp.h = max(0,tp.h);
         tp.v = min(1,tp.v);
         tp.v = max(0,tp.v);

         // convert to new coords
         CPoint pw;
         if (!p->HVToInfo (0, tp.h, tp.v, &pw))
            return FALSE;  // error that shouldnt happen
         pw.p[3] = 1;
         pw.MultiplyLeft (&amObjectToHV[i]);


         // add it
         tp.h = pw.p[0];
         tp.v = pw.p[1];
         tp.h = min(1,tp.h);
         tp.h = max(0,tp.h);
         tp.v = min(1,tp.v);
         tp.v = max(0,tp.v);
         alEdge[i].Add (&tp);
      }
   }

   // find the min and max in HV space
   TEXTUREPOINT tpMin, tpMax;
   PTEXTUREPOINT pt;
   for (i = 0; i < 2; i++) for (x = 0; x < alEdge[i].Num(); x++) {
      pt = (PTEXTUREPOINT) alEdge[i].Get(x);
      if ((i == 0) && (x == 0)) {
         tpMin = *pt;
         tpMax = *pt;
      }
      else {
         tpMin.h = min(tpMin.h, pt->h);
         tpMax.h = max(tpMax.h, pt->h);
         tpMin.v = min(tpMin.v, pt->v);
         tpMax.v = max(tpMax.v, pt->v);
      }
   }

   // create a list of lines for them...
   CListFixed lLines, lLinesTemp;
   TEXTUREPOINT atp[2];
   lLines.Init (sizeof(TEXTUREPOINT)*2);
   lLinesTemp.Init (sizeof(TEXTUREPOINT)*2);
   for (i = 0; i < 2; i++) {
      // create lines for this
      lLinesTemp.Clear();
      pt = (PTEXTUREPOINT) alEdge[i].Get(0);
      for (x = 0; x < alEdge[i].Num(); x++) {
         atp[0] = pt[x];
         atp[1] = pt[(x+1) % alEdge[i].Num()];
         lLinesTemp.Add (atp);
      }

      // minimize - remove linear lengths
      LinesMinimize (&lLinesTemp, 0.0);

      // extend them all and add to main one
      TEXTUREPOINT t;
      fp fLen;
      for (x = 0; x < lLinesTemp.Num(); x++) {
         pt = (PTEXTUREPOINT) lLinesTemp.Get(x);
         t.h = pt[1].h - pt[0].h;
         t.v = pt[1].v - pt[0].v;
         fLen = sqrt(t.h * t.h + t.v * t.v);
         if (fLen > CLOSE) {
            fLen = 1.0 / fLen * 0.05;  // so extend just a bit
            t.h *= fLen;
            t.v *= fLen;

            pt[0].h -= t.h;
            pt[0].v -= t.v;
            pt[1].h += t.h;
            pt[1].v += t.v;
         }

         // add this to main list
         lLines.Add (pt);
      }
   }

   // go from the lines to sequences to paths
   PCListFixed plLines2;
   plLines2 = LineSegmentsToSequences (&lLines);
   if (!plLines2)
      return FALSE;

   // get rid of dangling sequences
   LineSequencesRemoveHanging (plLines2, TRUE);
   if (!plLines2->Num()) {
      delete plLines2;
      return FALSE;
   }

   // get the scores
   CListVariable lv;
   DWORD       dwTemp[100];
   PCListFixed plScores;
   BOOL fClockwise;
   fClockwise = TRUE;
   plScores = SequencesSortByScores (plLines2, !fClockwise);
   if (!plScores) {
      for (i = 0; i < plLines2->Num(); i++) {
         PCListFixed pl = *((PCListFixed*) plLines2->Get(i));
         delete pl;
      }
      delete plLines2;
      return FALSE;
   }
#ifdef _DEBUG
   {
   // display all the sequences
   char szTemp[64];
   PCListFixed pt;
   DWORD j;
   for (i = 0; i < plLines2->Num(); i++) {
      sprintf (szTemp, "Sequence %d (score=%d):", i, *((int*)plScores->Get(i)));
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)plLines2->Get(i));

      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }
   }
#endif

   PathFromSequences (plLines2, &lv, dwTemp, 0, sizeof(dwTemp)/sizeof(DWORD), NULL);

#ifdef _DEBUG
   {
      // display all the sequences
      char szTemp[64];
      //DWORD i, j;
      DWORD j;
      DWORD *padw, dw;
      for (i = 0; i < lv.Num(); i++) {
         sprintf (szTemp, "Path %d:", i);
         OutputDebugString (szTemp);

         dw =  (DWORD)lv.Size(i) / sizeof(DWORD);
         padw = (DWORD*)  lv.Get(i);

         for (j = 0; j < dw; j++) {
            sprintf (szTemp, " %d", padw[j]);
            OutputDebugString (szTemp);
         }

         OutputDebugString ("\r\n");
      }
   }
#endif

   DWORD dwBest;
   dwBest = PathWithHighestScore (plLines2, &lv, plScores);
   delete plScores;

#if 0// def _DEBUG
   char szTemp[64];
   sprintf (szTemp, "Highest = %d\r\n", dwBest);
   OutputDebugString (szTemp);
#endif
   if (dwBest == -1) {
      for (i = 0; i < plLines2->Num(); i++) {
         PCListFixed pl = *((PCListFixed*) plLines2->Get(i));
         delete pl;
      }
      delete plLines2;
      return FALSE;
   }

   PCListFixed pEdge;
   pEdge = PathToSplineList (plLines2, &lv, dwBest, !fClockwise);

   // Delete plLines2
   for (i = 0; i < plLines2->Num(); i++) {
      PCListFixed pl = *((PCListFixed*) plLines2->Get(i));
      delete pl;
   }
   delete plLines2;

   if (!pEdge)
      return FALSE;
   CListFixed lEdgePoints;
   lEdgePoints.Init (sizeof(CPoint), pEdge->Get(0), pEdge->Num());
   delete pEdge;
   pEdge = 0;

   // find the min and max of the new edge. If it's not the same size as the old min and
   // max then didn't intersect properly, so error
   CPoint pEMin, pEMax;
   for (x = 0; x < lEdgePoints.Num(); x++) {
      PCPoint p = (PCPoint)lEdgePoints.Get(x);
      if (x) {
         pEMin.Min (p);
         pEMax.Max (p);
      }
      else {
         pEMin.Copy (p);
         pEMax.Copy (p);
      }
   }

   // BUGFIX - Changed from CLOSE to .05 because when intersected building block with
   // one at a 45 degree angle had some error or .015 or so.
   if ((fabs(pEMin.p[0] - tpMin.h) > .05) || (fabs(pEMin.p[1] - tpMin.v) > .05) ||
      (fabs(pEMax.p[0] - tpMax.h) > .05) || (fabs(pEMax.p[1] - tpMax.v) > .05)) {
         delete pEdge;
         return FALSE;
      }


   // calculate the location of the corners (as translated into new HV) so can tell
   // which side ends up being closest to the new edge so can pull bevelling from that
   CPoint apBevel[2][2][2];   // [0=this,1=poss][y][x]
   for (i = 0; i < 2; i++) for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) {
      apBevel[i][y][x].Copy (&ap[i][y][x]);
      apBevel[i][y][x].p[3] = 1;
      apBevel[i][y][x].MultiplyLeft (&mPlaneToHV);
   }
   // for the 4 bevel corners, figure out which corner is closest to it...
   DWORD adwBevel[4];      // [0]=top,[1]=right,[2]=bottom,[3]=left
                           // values are 0..3 for this, 4..7 for poss, lower 2 bits are 0=top,1=right,2=bottom,3=left
   for (i = 0; i < 4; i++) {
      // line that looking for
      CPoint apLook[2];
      apLook[0].Zero();
      apLook[1].Zero();
      switch (i) {
         case 0:  // top
            apLook[1].p[0] = 1;
            break;
         case 1:  // right
            apLook[0].p[0] = apLook[1].p[0] = 1;
            apLook[1].p[1] = 1;
            break;
         case 2:  // bottom
            apLook[0].p[1] = apLook[1].p[1] = 1;
            apLook[1].p[0] = 1;
            break;
         case 3:  // left
            apLook[1].p[1] = 1;
            break;
      }

      fp fClosest;
      for (x = 0; x < 8; x++) {
         // what looking at
         PCPoint apAt[2];
         switch (x % 4) {
            case 0:  // top
               apAt[0] = &apBevel[x/4][0][0];
               apAt[1] = &apBevel[x/4][0][1];
               break;
            case 1: // right
               apAt[0] = &apBevel[x/4][0][1];
               apAt[1] = &apBevel[x/4][1][1];
               break;
            case 2:  // bottom
               apAt[0] = &apBevel[x/4][1][0];
               apAt[1] = &apBevel[x/4][1][1];
               break;
            case 3: // left
               apAt[0] = &apBevel[x/4][0][0];
               apAt[1] = &apBevel[x/4][1][0];
               break;
         }

         // dot-product of angle is one score... if == 1 then they're paralleel
         CPoint pA, pB;
         fp fDot;
         pA.Subtract (&apLook[0], &apLook[1]);
         pB.Subtract (apAt[0], apAt[1]);
         pA.Normalize();
         pB.Normalize();
         fDot = fabs(pA.DotProd (&pB));

         // distance from the edge
         fp fDist;
         pA.Average (apAt[0], apAt[1]);
         switch (i) {
            case 0: // top
               fDist = pA.p[1];
               break;
            case 1:  // right
               fDist = 1.0 - pA.p[0];
               break;
            case 2:  // bottom
               fDist = 1.0 - pA.p[1];
               break;
            case 3:  // left
               fDist = pA.p[0];
               break;
         }
         fDist = 1.0 - fDist;

         // total score
         fDist *= fDot;
         if ((x == 0) || fDist > fClosest) {
            fClosest = fDist;
            adwBevel[i] = x;
         }
      }  // over x
   } // over i

   // note what the bevels should be
   fp afBevel[4]; // [0] = top, etc.
   for (i = 0; i < 4; i++) {
      fp f;
      switch (adwBevel[i]) {
         case 0:  // top
            f = m_ds.m_fBevelTop;
            break;
         case 1:  // right
            f = m_ds.m_fBevelRight;
            break;
         case 2:  // bottom
            f = m_ds.m_fBevelBottom;
            break;
         case 3:  // left
            f = m_ds.m_fBevelLeft;
            break;
         case 4:  // top
            f = poss->m_ds.m_fBevelTop;
            break;
         case 5:  // right
            f = poss->m_ds.m_fBevelRight;
            break;
         case 6:  // bottom
            f = poss->m_ds.m_fBevelBottom;
            break;
         case 7:  // left
            f = poss->m_ds.m_fBevelLeft;
            break;
      }
      afBevel[i] = f;
   }

   // tell the world object is about to change
   m_pWorld->ObjectAboutToChange (this);
   m_pWorld->ObjectAboutToChange (poss);

   DWORD dwSurf;
   for (dwSurf = 0; dwSurf < 2; dwSurf++) {  // 1 = from poss, 0 = from this
      PCObjectStructSurface pFrom = (dwSurf ? poss : this);

      // adjust all overlays in current surface
      PWSTR pszName;
      WCHAR szName[256];
      PTEXTUREPOINT pPoints;
      DWORD dwNum;
      CPoint pLoc;
      CListFixed lPoints;
      TEXTUREPOINT tp;
      lPoints.Init (sizeof(TEXTUREPOINT));
      DWORD dwCutout;

      // BUGFIX - If a cutout is in the overlapping area, where the two surfaces
      // merge, AND it's a building block clip, then remove it. Do this because the
      // clip was probably due to the intel-adjust intersection of the two floors,
      // and is moot when the floors are combined
      CPoint apBevMax[2], apBevMin[2];
      for (i = 0; i < 2; i++) for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) {
         if ((y == 0) && (x == 0)) {
            apBevMax[i].Copy (&apBevel[i][y][x]);
            apBevMin[i].Copy (&apBevel[i][y][x]);
         }
         else {
            apBevMax[i].Max (&apBevel[i][y][x]);
            apBevMin[i].Min (&apBevel[i][y][x]);
         }
      }
      for (x = 0; x < 2; x++) {
         PCSplineSurface pss = x ? &pFrom->m_ds.m_SplineA : &pFrom->m_ds.m_SplineB;
         for (i = pss->CutoutNum()-1; i < pss->CutoutNum(); i--) {
            if (!pss->CutoutEnum (i, &pszName, &pPoints, &dwNum, &fClockwise))
               continue;
            
            // BUGFIX - If cutout is NOT clockwise then remove it since will preclude
            // the union of the two objects. Put this in when two EA roofs dont merge
            // properly due to counerclockwise cutout
            if (!fClockwise)
               goto cutoutremove;

            if (wcsncmp(pszName, gpszBuildBlockClip, wcslen(gpszBuildBlockClip)))
               continue;   // dont care about this

            // find its center
            tp.h = tp.v = 0;
            for (y = 0; y < dwNum; y++) {
               pLoc.Zero();
               pFrom->m_ds.HVToInfo (x ? 1 : -1, pPoints[y].h, pPoints[y].v, &pLoc); // note m_ds
               pLoc.p[3] = 1;
               pLoc.MultiplyLeft (&amObjectToHV[dwSurf]);   // NOTE the section
               // NOTE: Specifically not flipping tp.h if it's on side B
               tp.h += pLoc.p[0];
               tp.v += pLoc.p[1];
            }
            tp.h /= (fp)dwNum;
            tp.v /= (fp)dwNum;

            // if it's in both then delete
            BOOL fIn[2];
            fIn[0] = (tp.h >= apBevMin[0].p[0]) && (tp.h <= apBevMax[0].p[0]) &&
               (tp.v >= apBevMin[0].p[1]) && (tp.v <= apBevMax[0].p[1]);
            fIn[1] = (tp.h >= apBevMin[1].p[0]) && (tp.h <= apBevMax[1].p[0]) &&
               (tp.v >= apBevMin[1].p[1]) && (tp.v <= apBevMax[1].p[1]);

            if (!(fIn[0] && fIn[1]))
               continue;   // dont bother remove since not in both spots

cutoutremove:
            // else, remove
            wcscpy (szName, pszName);
            pss->CutoutRemove (szName);
         }
      }


      for (dwCutout = 0; dwCutout < 2; dwCutout++) {
         for (x = 0; x < 2; x++) {
            PCSplineSurface pss = x ? &pFrom->m_ds.m_SplineA : &pFrom->m_ds.m_SplineB;
            PCSplineSurface pssThis = x ? &m_ds.m_SplineA : &m_ds.m_SplineB;

            // FUTURERELEASE - This and other adjustments might have problems with bevelling...
            // because chang definition on side a vs side b.
            // Just documenting for now
            for (i = 0; i < (dwCutout ? pss->CutoutNum() : pss->OverlayNum()); i++) {
               if (dwCutout) {
                  if (!pss->CutoutEnum (i, &pszName, &pPoints, &dwNum, &fClockwise))
                     continue;
               }
               else {
                  if (!pss->OverlayEnum (i, &pszName, &pPoints, &dwNum, &fClockwise))
                     continue;
               }
               wcscpy (szName, pszName);
               lPoints.Clear();
               for (y = 0; y < dwNum; y++) {
                  pLoc.Zero();
                  pFrom->m_ds.HVToInfo (x ? 1 : -1, pPoints[y].h, pPoints[y].v, &pLoc); // note m_ds
                  pLoc.p[3] = 1;
                  pLoc.MultiplyLeft (&amObjectToHV[dwSurf]);   // NOTE the section
                  tp.h = pLoc.p[0];
                  tp.v = pLoc.p[1];
                  if (x == 0)
                     tp.h = 1.0 - tp.h;
                  tp.h = min(1,tp.h);
                  tp.h = max(0,tp.h);
                  tp.v = min(1,tp.v);
                  tp.v = max(0,tp.v);

                  lPoints.Add (&tp);
               }

               // write it into this one
               if (dwCutout) {
                  // BUGFIX: if the cutout exists already then change the name
                  DWORD dwCount = 1;
                  if (dwSurf == 1) while (TRUE) {
                     DWORD dw,  dwn;
                     PWSTR pszT;
                     BOOL fc;
                     PTEXTUREPOINT ppa;
                     for (dw = 0; dw < pssThis->CutoutNum(); dw++) {
                        pssThis->CutoutEnum (dw, &pszT, &ppa, &dwn, &fc);
                        if (!_wcsicmp(szName, pszT))
                           break;
                     }
                     if (dw >= pssThis->CutoutNum())
                        break;   // no match

                     // come up with a new name and try again
                     swprintf (szName, L"%s%d", pszName, (int) dwCount++);
                  }

                  pssThis->CutoutSet (szName, (PTEXTUREPOINT) lPoints.Get(0), lPoints.Num(), fClockwise);
               }
               else {   // it's an overlay
                  if (dwSurf) {
                     // need to transfer the color and everything
                     DWORD dwDisplayControl, dwOrig;
                     PWSTR psz;
                     dwDisplayControl = dwOrig = 0;
                     psz = wcschr (szName, L':');
                     if (!psz)
                        continue;   // shouldnt happen
                     dwOrig = _wtoi(psz+1);
                     psz[0] = 0; // get rid of colon
                     m_ds.AddOverlay (x ? TRUE : FALSE, szName,
                        (PTEXTUREPOINT) lPoints.Get(0), lPoints.Num(), fClockwise,
                        &dwDisplayControl, FALSE);

                     // transfer the color
                     PCObjectSurface pSurf;
                     pSurf = poss->SurfaceGet (dwOrig);
                     if (pSurf) {
                        pSurf->m_dwID = dwDisplayControl;
                        SurfaceSet (pSurf);
                        delete pSurf;
                     }
                  }
                  else
                     pssThis->OverlaySet (szName, (PTEXTUREPOINT) lPoints.Get(0), lPoints.Num(), fClockwise);
               }

            } // i - overlays or cutouts
         }  // x - sides
      }  // dwCutouts

      // adjust all embedded objects in current surface - and tell to re-cutout
      GUID gEmbed;
      PCLAIMSURFACE pcs;
      fp fRotation;
      DWORD dwSurface;
      BOOL fSideA;

      for (i = pFrom->ContEmbeddedNum() - 1; i < pFrom->ContEmbeddedNum(); i--) {
         if (!pFrom->ContEmbeddedEnum (i, &gEmbed))
            continue;

         if (!pFrom->ContEmbeddedLocationGet (&gEmbed, &tp, &fRotation, &dwSurface))
            continue;

         // find out which side it's on from the surface
         pcs = pFrom->m_ds.ClaimFindByID (dwSurface);
         if (!pcs)
            continue;
         switch (pcs->dwReason) {
            case 0:
            case 3:
               fSideA = TRUE;
               break;
            default:
               fSideA = FALSE;
               break;
         }

         // get the coords
         pLoc.Zero();
         pFrom->m_ds.HVToInfo (fSideA ? 1 : -1, tp.h, tp.v, &pLoc); // note m_ds
         pLoc.p[3] = 1;
         pLoc.MultiplyLeft (&amObjectToHV[dwSurf]);   // NOTE the section
         tp.h = pLoc.p[0];
         tp.v = pLoc.p[1];
         if (!fSideA)
            tp.h = 1.0 - tp.h;
         tp.h = min(1,tp.h);
         tp.h = max(0,tp.h);
         tp.v = min(1,tp.v);
         tp.v = max(0,tp.v);
         
         // Deal with rotation
         CMatrix mOrig;
         mOrig.Identity();
         pFrom->ContMatrixFromHV (dwSurface, &tp, fRotation, &mOrig);   // embed to object space
         // dont do mOrig.MultiplyRight (&pFrom->m_MatrixObject); // now converts from embed to world space
         mOrig.MultiplyRight (&mWorldToPlane);  // now converts from embed to plane space
         CPoint pC, pU;
         pC.Zero();
         pC.p[3] = 1;
         pU.Copy (&pC);
         pU.p[2] = 1;
         pC.MultiplyLeft (&mOrig);
         pU.MultiplyLeft (&mOrig);
         pU.Subtract (&pC);
         fRotation = atan2(pU.p[0], pU.p[2]);
         if (!fSideA)
            fRotation *= -1;  // since on other side, in opposite direciton

         if (pFrom) {
            pFrom->ContEmbeddedRemove (&gEmbed);
            pcs = m_ds.ClaimFindByReason (fSideA ? 0 : 1);
            if (pcs)
               ContEmbeddedAdd (&gEmbed, &tp, fRotation, pcs->dwID);
         }
         else
            // just move it, since already in object
            ContEmbeddedLocationSet (&gEmbed, &tp, fRotation, dwSurface);
      }

      // after xfer all of own bits over, change the edge and expand the current surface
      // this will reduce processing for the second half when merge in other object
      if (dwSurf == 0) {

         // expand the current surface
         CPoint apNew[2][2];   //[y][x] - new control points
         CMatrix mIdent, mPlaneToObject;
         mIdent.Identity();
         m_ds.MatrixSet (&mIdent);
         m_MatrixObject.Invert4 (&mPlaneToObject); // now filled with world to object
         mPlaneToObject.MultiplyLeft (&mPlaneToWorld);
         for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) {
            apNew[y][x].p[0] = x ? pMax.p[0] : pMin.p[0];
            apNew[y][x].p[1] = pMin.p[1];
            apNew[y][x].p[2] = y ? pMin.p[2] : pMax.p[2];
            apNew[y][x].p[3] = 1;
            apNew[y][x].MultiplyLeft (&mPlaneToObject);
         }
         fp fDetail;
         DWORD dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV;
         m_ds.DetailGet (&dwMinDivideH, &dwMaxDivideH, &dwMinDivideV, &dwMaxDivideV, &fDetail);
         m_ds.ControlPointsSet (2, 2, &apNew[0][0], (DWORD*)SEGCURVE_LINEAR, (DWORD*)SEGCURVE_LINEAR, 
            max(dwMaxDivideH, dwMaxDivideV));
      }

   }  // dwSurf

   // adjust current edge to new edge
   m_ds.EdgeInit (TRUE, lEdgePoints.Num(), (PCPoint) lEdgePoints.Get(0), NULL,
      (DWORD*) SEGCURVE_LINEAR, 0, 0, .1);


   // deal with bevelled edges
   m_ds.m_fBevelTop = afBevel[0];
   m_ds.m_fBevelRight = afBevel[1];
   m_ds.m_fBevelBottom = afBevel[2];
   m_ds.m_fBevelLeft = afBevel[3];

   // transfer floor height information
   fp *pf, *pfCur;
   DWORD dwNum, dwNumCur;
   for (i = 0; i < 2; i++) {
      pf = (fp *) (i ? m_ds.m_listFloorsA.Get(0) : m_ds.m_listFloorsB.Get(0));
      dwNum = (i ? m_ds.m_listFloorsA.Num() : m_ds.m_listFloorsB.Num());

      for (x = 0; x < dwNum; x++)
         pf[x] = pf[x] * apBevel[0][1][0].p[1] + (1.0 - pf[x]) * apBevel[0][0][0].p[1];
   }
   for (i = 0; i < 2; i++) {
      fp f;
      pf = (fp *) (i ? poss->m_ds.m_listFloorsA.Get(0) : poss->m_ds.m_listFloorsB.Get(0));
      dwNum = (i ? poss->m_ds.m_listFloorsA.Num() : poss->m_ds.m_listFloorsB.Num());

      for (x = 0; x < dwNum; x++) {
         f = pf[x] * apBevel[1][1][0].p[1] + (1.0 - pf[x]) * apBevel[0][0][0].p[1];

         pfCur = (fp *) (i ? m_ds.m_listFloorsA.Get(0) : m_ds.m_listFloorsB.Get(0));
         dwNumCur = (i ? m_ds.m_listFloorsA.Num() : m_ds.m_listFloorsB.Num());
         for (y = 0; y < dwNumCur; y++)
            if (fabs(pfCur[y] - f) < .01)
               break;
         if (y >= dwNumCur) {
            if (i)
               m_ds.m_listFloorsA.Add (&f);
            else
               m_ds.m_listFloorsB.Add (&f);
         }
      }
   }

   // tell the world object changed
   m_pWorld->ObjectChanged (this);
   m_pWorld->ObjectChanged (poss);

   // delete the other object
   GUID gOther;
   DWORD dwFind;
   poss->GUIDGet (&gOther);
   dwFind = m_pWorld->ObjectFind (&gOther);
   if (dwFind != -1)
      m_pWorld->ObjectRemove (dwFind);


   return TRUE;
}


// NOT GOING TO FIX - When merge floor sections, may want to see if fundamental texture ID/color
// differs from the one that merging with. If it does then create an overlay for the merge-with
// so keep the color. Example: One BB has tile floor and the other wood, but
// no overlays. If merge, should create an overlay so they look the same -
// The reason: Don't really know what shape to make the new overlay because there
// are probably cutouts where the two surfaces intersect. Not sure which cutouts to use
// to change the shape of the overlay. The result would be unpredicatable which way
// the textures would go.

// BUGBUG - If create curved roof and move a corner, shouldn't the curve control points
// also moved in order to keep it rectangular?


