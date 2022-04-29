/************************************************************************
CObjectSingleSurface.cpp - Draws a box.

begun 29/8/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gszPoints = L"points%d-%d";



typedef struct {
   PCObjectSingleSurface   pThis;
   int                     iSide;   // 1 if clicked on a, -1 if b, 0 if unknown
} DSS, *PDSS;


#define     SURFACEEDGE       10
#define     SIDEA             100
#define     SIDEB             200
#define     MAXCOLORPERSIDE   99          // maximum number of color settings per side



/**********************************************************************************
CObjectSingleSurface::Constructor and destructor

(DWORD)pParams is passed into CSingleSurface::Init(). See definitions of parameters

 */
CObjectSingleSurface::CObjectSingleSurface (PVOID pParams, POSINFO pInfo)
{
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;

   m_ds.Init (m_dwType, (PCObjectTemplate) this);
   m_dwRenderShow = RENDERSHOW_WALLS | RENDERSHOW_FLOORS | RENDERSHOW_ROOFS |
      RENDERSHOW_FRAMING;

   m_dwDisplayControl = 2; // start with scale

   CalcScale ();
}


CObjectSingleSurface::~CObjectSingleSurface (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectSingleSurface::Delete - Called to delete this object
*/
void CObjectSingleSurface::Delete (void)
{
   delete this;
}


/**********************************************************************************
CObjectSingleSurface::CalcScale - Fills m_pScaleXXX with scale information
*/
void CObjectSingleSurface::CalcScale (void)
{
   // NOTE: Not bothering remember undo and redo since this is automatically calculated

   DWORD dwWidth, dwHeight, dwX, dwY;
   dwWidth = m_ds.ControlNumGet(TRUE);
   dwHeight = m_ds.ControlNumGet(FALSE);
   m_pScaleMin.Zero();
   m_pScaleMax.Zero();

   CPoint p;
   for (dwX = 0; dwX < dwWidth; dwX++) for (dwY = 0; dwY < dwHeight; dwY++) {
      m_ds.ControlPointGet (dwX, dwY, &p);

      if ((dwX == 0) && (dwY == 0)) {
         m_pScaleMin.Copy (&p);
         m_pScaleMax.Copy (&p);
      }
      else {
         m_pScaleMin.Min (&p);
         m_pScaleMax.Max (&p);
      }

   }

   // subtract
   m_pScaleSize.Subtract (&m_pScaleMax, &m_pScaleMin);
   for (dwX = 0; dwX < 3; dwX++)
      m_pScaleSize.p[dwX] = max(CLOSE, m_pScaleSize.p[dwX]);
}

/**********************************************************************************
CObjectSingleSurface::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectSingleSurface::Render (POBJECTRENDER pr, DWORD dwSubObject)
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
CObjectSingleSurface::QueryBoundingBox - Standard API
*/
void CObjectSingleSurface::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
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
      OutputDebugString ("\r\nCObjectSingleSurface::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectSingleSurface::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectSingleSurface::Clone (void)
{
   PCObjectSingleSurface pNew;

   pNew = new CObjectSingleSurface(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);
   m_ds.CloneTo (&pNew->m_ds, pNew);

   pNew->m_dwDisplayControl = m_dwDisplayControl;
   pNew->m_pScaleMax.Copy (&m_pScaleMax);
   pNew->m_pScaleSize.Copy (&m_pScaleSize);
   pNew->m_pScaleMin.Copy (&m_pScaleMin);

   return pNew;
}

static PWSTR gpszDisplayControl = L"DisplayControl";



PCMMLNode2 CObjectSingleSurface::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   PCMMLNode2 pSub;
   pSub = m_ds.MMLTo();
   if (pSub) {
      pSub->NameSet (L"SingleSurface");
      pNode->ContentAdd (pSub);
   }


   MMLValueSet (pNode, gpszDisplayControl, (int) m_dwDisplayControl);

   return pNode;
}

BOOL CObjectSingleSurface::MMLFrom (PCMMLNode2 pNode)
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
      if (!_wcsicmp(psz, L"SingleSurface"))
         m_ds.MMLFrom (pSub);
   }

   m_dwDisplayControl = (DWORD) MMLValueGetInt (pNode, gpszDisplayControl, 0);

   // recalculate the scale
   CalcScale();
   return TRUE;
}


/*************************************************************************************
CObjectSingleSurface::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectSingleSurface::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
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
      if (dwSurf >= 1)
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Curvature");

      // start location in mid point
      m_ds.ControlPointGet (dwX, dwY, &pInfo->pLocation);

      return TRUE;
   }
   else if (m_dwDisplayControl == 1) { // edges
      // control points for spline
      DWORD dwNum;
      DWORD dwX, dwSurf;
      dwNum = m_ds.EdgeOrigNumPointsGet();
      dwX = dwID % dwNum;
      dwSurf = dwID / dwNum;
      if (dwSurf >= 1)
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
      m_ds.HVToInfo (pHV.p[0], pHV.p[1], &pInfo->pLocation);

      return TRUE;
   }
   else if (m_dwDisplayControl == 2) { // scale
      if (dwID >= 3)
         return FALSE;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
      //pInfo->dwFreedom = 0;   // any direction
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = (max(m_pScaleSize.p[0], m_pScaleSize.p[1]), m_pScaleSize.p[2]) / 10.0;
      pInfo->cColor = RGB(0,0xff,0xff);
      switch (dwID) {
      case 0:
      default:
         wcscpy (pInfo->szName, L"Width");
         break;
      case 1:
         wcscpy (pInfo->szName, L"Depth");
         break;
      case 2:
         wcscpy (pInfo->szName, L"Height");
         break;
      }
      MeasureToString (m_pScaleSize.p[dwID], pInfo->szMeasurement);

      pInfo->pLocation.Add (&m_pScaleMin, &m_pScaleMax);
      pInfo->pLocation.Scale (.5);
      pInfo->pLocation.p[dwID] = m_pScaleMax.p[dwID];

      return TRUE;
   }
   else {   // see if it's one of the overlays
      return m_ds.ControlPointQuery (m_dwDisplayControl, dwID, pInfo);
   }

   return FALSE;
}

/*************************************************************************************
CObjectSingleSurface::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectSingleSurface::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (m_dwDisplayControl == 0) {
      DWORD dwWidth, dwHeight;
      DWORD dwX, dwY, dwSurf;
      dwWidth = m_ds.ControlNumGet(TRUE);
      dwHeight = m_ds.ControlNumGet(FALSE);
      dwX = dwID % dwWidth;
      dwY = (dwID / dwWidth) % dwHeight;
      dwSurf = dwID / (dwWidth * dwHeight);
      if (dwSurf >= 1)
         return FALSE;

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      // from the ID calculate the dimension and corner
      m_ds.ControlPointSet (dwX, dwY, pVal);
      CalcScale();

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
      if (dwSurf >= 1)
         return FALSE;

      // get the old spline point
      CPoint pOldHV;
      m_ds.EdgeOrigPointGet (dwX, &pOldHV);
      TEXTUREPOINT tpOld, tpNew;
      tpOld.h = pOldHV.p[0];
      tpOld.v = pOldHV.p[1];

      // from the ID calculate the dimension and corner

      // find out where it intersects
      if (!m_ds.HVDrag (&tpOld, pVal, pViewer, &tpNew))
         return FALSE;  // doesnt intersect

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
   else if (m_dwDisplayControl == 2) { // scale
      if (dwID >= 3)
         return FALSE;

      // figure out the new scale
      CPoint pNew, pMid;
      pMid.Add (&m_pScaleMax, &m_pScaleMin);
      pMid.Scale (.5);
      pNew.Copy (&m_pScaleSize);
      pNew.p[dwID] = (pVal->p[dwID] - pMid.p[dwID]) * 2.0;
      pNew.p[dwID] = max(CLOSE, pNew.p[dwID]);

      if (pNew.AreClose (&m_pScaleSize))
         return TRUE;   // no change

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      // allocate enough memory so can do the calculations
      DWORD dwWidth, dwHeight;
      DWORD dwTotal;
      dwWidth = m_ds.ControlNumGet(TRUE);
      dwHeight = m_ds.ControlNumGet(FALSE);
      dwTotal = dwWidth * dwHeight;

      if (!dwTotal)
         return FALSE;
      CMem memPoints, memSegCurveH, memSegCurveV;
      if (!memPoints.Required (dwTotal * sizeof(CPoint)))
         return FALSE;
      if (!memSegCurveH.Required (dwWidth * sizeof(DWORD)))
         return FALSE;
      if (!memSegCurveV.Required (dwHeight * sizeof(DWORD)))
         return FALSE;
      PCPoint pPoints;
      DWORD *padwSegCurveH, *padwSegCurveV;
      pPoints = (PCPoint) memPoints.p;
      padwSegCurveH = (DWORD*) memSegCurveH.p;
      padwSegCurveV = (DWORD*) memSegCurveV.p;

      // fill in the new values
      DWORD i, j, k;
      CPoint p;
      for (i = 0; i < dwWidth; i++) for (j = 0; j < dwHeight; j++) {
         m_ds.ControlPointGet (i, j, &p);

         p.Subtract (&pMid);
         for (k = 0; k < 3; k++)
            p.p[k] = p.p[k] / m_pScaleSize.p[k] * pNew.p[k];
         p.Add (&pMid);

         pPoints[j * dwWidth + i].Copy (&p);
      }

      // do the curvature
      for (i = 0; i < dwWidth; i++)
         padwSegCurveH[i] = m_ds.SegCurveGet (TRUE, i);
      for (i = 0; i < dwHeight; i++)
         padwSegCurveV[i] = m_ds.SegCurveGet (FALSE, i);

      // set new value
      BOOL fLoopH, fLoopV;
      fLoopH = m_ds.m_Spline.LoopGet(TRUE);
      fLoopV = m_ds.m_Spline.LoopGet(FALSE);
      m_ds.ControlPointsSet (fLoopH, fLoopV, dwWidth, dwHeight,
         pPoints, padwSegCurveH, padwSegCurveV);

      CalcScale();

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }
   else {   // see if it's one of the overlays
      return m_ds.ControlPointSet (m_dwDisplayControl, dwID, pVal, pViewer);
   }

   return FALSE;
}

/*************************************************************************************
CObjectSingleSurface::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectSingleSurface::ControlPointEnum (PCListFixed plDWORD)
{
   // 6 control points starting at 10
   DWORD i;

   if (m_dwDisplayControl == 0) {
      DWORD dwNum = m_ds.ControlNumGet(TRUE) * m_ds.ControlNumGet(FALSE);
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl == 1) { // edges
      DWORD dwNum = m_ds.EdgeOrigNumPointsGet();
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl == 2) { // scale
      DWORD dwNum = 3;
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else {   // see if it's one of the overlays
      m_ds.ControlPointEnum (m_dwDisplayControl, plDWORD);
   }

}


/**********************************************************************************
CObjectSingleSurface::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectSingleSurface::DialogQuery (void)
{
   return TRUE;
}



/**********************************************************************************
CObjectSingleSurface::DialogCPQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectSingleSurface::DialogCPQuery (void)
{
   return TRUE;
}

/* SingleSurfaceDialogPage
*/
BOOL SingleSurfaceDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSS pdss = (PDSS)pPage->m_pUserData;
   PCObjectSingleSurface pv = pdss->pThis;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Surface settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* SingleSurfaceDisplayPage
*/
BOOL SingleSurfaceDisplayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectSingleSurface pv = (PCObjectSingleSurface)pPage->m_pUserData;

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
         case 2:  // scale
            pControl = pPage->ControlFind (L"scale");
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
         else if ((pControl = pPage->ControlFind (L"scale")) && pControl->AttribGetBOOL(Checked()))
            dwNew = 2;  // scale
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
            MemCat (&memGroup, L"scale,curve,edge");
            DWORD i, j;
            PWSTR psz;
            PTEXTUREPOINT ptp;
            DWORD dwNum;
            DWORD dwColor;
            BOOL fClockwise;
            PCSplineSurface pss;
            j = 0;
            pss = &pv->m_ds.m_Spline;
            for (i = 0; i < pss->OverlayNum(); i++) {
               if (!pss->OverlayEnum (i, &psz, &ptp, &dwNum, &fClockwise))
                  continue;
               dwColor = OverlayShapeNameToColor(psz);
               if (dwColor == (DWORD)-1)
                  continue;
               MemCat (&memGroup, L",xc");
               MemCat (&memGroup, (int) dwColor);
            }

            MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
            MemCat (&gMemTemp, (PWSTR) memGroup.p);
            MemCat (&gMemTemp, L" name=scale><bold>Size</bold><br/>");
            MemCat (&gMemTemp, L"Shows three control points that allow you to resize the curve.</xChoiceButton>");

            MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
            MemCat (&gMemTemp, (PWSTR) memGroup.p);
            MemCat (&gMemTemp, L" name=curve><bold>Curvature</bold><br/>");
            MemCat (&gMemTemp, L"These control points let you curve the surface.</xChoiceButton>");

            MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
            MemCat (&gMemTemp, (PWSTR) memGroup.p);
            MemCat (&gMemTemp, L" name=edge><bold>Edge</bold><br/>"
               L"Show these control points to cut away the edge of the surface so it isn't "
               L"rectangluar.</xChoiceButton>");

            j = 0;
            pss = &pv->m_ds.m_Spline;
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
               MemCat (&gMemTemp, L"</bold><br/>");
               MemCat (&gMemTemp, L"Control points for a color/texture overlay.</xChoiceButton>");
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
CObjectSingleSurface::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectSingleSurface::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
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
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSINGLEDIALOG, SingleSurfaceDialogPage, &dss);
firstpage2:
   if (!pszRet) {
      CalcScale();
      return FALSE;
   }
   if (!_wcsicmp(pszRet, L"detail")) {
      pszRet = m_ds.SurfaceDetailPage (pWindow);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"curvesegment")) {
      pszRet = m_ds.SurfaceCurvaturePage (pWindow);
      CalcScale();
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"controlpoints")) {
      pszRet = m_ds.SurfaceControlPointsPage (pWindow);
      CalcScale();
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"displaycontrol")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEDISPLAY, SingleSurfaceDisplayPage, this);
         // NOTE: Using same dialog for singlesurface as for structsurface
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
   else if (!_wcsicmp(pszRet, L"oversidea")) {
      pszRet = m_ds.SurfaceOverMainPage (pWindow, &m_dwDisplayControl);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }

   CalcScale();

   return !_wcsicmp(pszRet, Back());
}



/**********************************************************************************
CObjectSingleSurface::DialogCPShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectSingleSurface::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSURFACEDISPLAY, SingleSurfaceDisplayPage, this);
   CalcScale ();  // automatically recalc scale
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}

/**********************************************************************************
CObjectSingleSurface::ContHVQuery
Called to see the HV location of what clicked on, or in other words, given a line
from pEye to pClick (object sapce), where does it intersect surface dwSurface (LOWORD(PIMAGEPIXEL.dwID))?
Fills pHV with the point. Returns FALSE if doesn't intersect, or invalid surface, or
can't tell with that surface. NOTE: if pOld is not NULL, then assume a drag from
pOld (converted to world object space) to pClick. Finds best intersection of the
surface then. Useful for dragging embedded objects around surface
*/
BOOL CObjectSingleSurface::ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV)
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
BOOL CObjectSingleSurface::ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides)
{
   return m_ds.ContCutout (pgEmbed, dwNum, paFront, paBack, fBothSides);
}

/**********************************************************************************
CObjectSingleSurface::ContCutoutToZipper -
Assumeing that ContCutout() has already been called for the pgEmbed, this askes the surfaces
for the zippering information (CRenderSufrace::ShapeZipper()) that would perfectly seal up
the cutout. plistXXXHVXYZ must be initialized to sizeof(HVXYZ). plisstFrontHVXYZ is filled with
the points on the side where the object was embedded, and plistBackHVXYZ are the opposite side.
NOTE: These are in the container's object space, NOT the embedded object's object space.
*/
BOOL CObjectSingleSurface::ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ)
{
   return FALSE;
}


/**********************************************************************************
CObjectSingleSurface::ContThickness - 
returns the thickness of the surface (dwSurface) at pHV. Used by embedded
objects like windows to know how deep they should be.

NOTE: usually overridden
*/
fp CObjectSingleSurface::ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV)
{
   return 0;
}


/**********************************************************************************
CObjectSingleSurface::ContSideInfo -
returns a DWORD indicating what class of surface it is... right now
0 = internal, 1 = external. dwSurface is the surface. If fOtherSide
then want to know what's on the opposite side. Returns 0 if bad dwSurface.

NOTE: usually overridden
*/
DWORD CObjectSingleSurface::ContSideInfo (DWORD dwSurface, BOOL fOtherSide)
{
   return m_ds.ContSideInfo (dwSurface, fOtherSide);
}


/**********************************************************************************
CObjectSingleSurface::ContMatrixFromHV - THE SUPERCLASS SHOULD OVERRIDE THIS. Given
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
BOOL CObjectSingleSurface::ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm)
{
   if (!m_ds.ContMatrixFromHV (dwSurface, pHV, fRotation, pm))
      return FALSE;


   // then rotate by the object's rotation matrix to get to world space
   pm->MultiplyRight (&m_MatrixObject);

   return TRUE;
}


/**********************************************************************************
CObjectSingleSurface::
Called to tell the container that one of its contained objects has been renamed.
*/
BOOL CObjectSingleSurface::ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew)
{
   return m_ds.ContEmbeddedRenamed (pgOld, pgNew);
}


