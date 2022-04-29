/************************************************************************
CObjectNoodle.cpp - Draws a Noodle.

begun 1/3/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaSplineMoveP[] = {
   L"Start", -1, 0,
   L"Center", 0, 0,
   L"End", 1, 0
};

/**********************************************************************************
CObjectNoodle::Constructor and destructor */
CObjectNoodle::CObjectNoodle (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;
   m_dwDisplayControl = 0;
   m_dwPath = 1;
   m_pPathParam.Zero();
   m_pPathParam.p[0] = 1.0;
   PathSplineFromParam ();

   // color for the Noodle
   TEXTUREMODS tm;
   memset (&tm, 0, sizeof(tm));
   tm.cTint = RGB(0xff,0xff,0xff);
   tm.wBrightness = 0x1000;
   tm.wContrast = 0x1000;
   tm.wHue = 0x0000;
   tm.wSaturation = 0x1000;
   ObjectSurfaceAdd (42, RGB(0x80,0xc0,0xc0), MATERIAL_PAINTSEMIGLOSS);

   // create the object
   m_Noodle.BackfaceCullSet (TRUE);
   m_Noodle.DrawEndsSet (TRUE);
   CPoint pScale;
   pScale.Zero();
   pScale.p[0] = pScale.p[1] = .1;
   m_Noodle.ScaleVector (&pScale);
   m_Noodle.ShapeDefault (NS_RECTANGLE);
}


CObjectNoodle::~CObjectNoodle (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectNoodle::PathSplineFromParam - Looks at m_pPath and m_pPathParam and fills
in the path spline.

inputs
   none
returns
   none
*/
void CObjectNoodle::PathSplineFromParam (void)
{
   CListFixed lPath;
   CPoint pPath;
   DWORD dwCurve = SEGCURVE_LINEAR;
   BOOL fLoop = FALSE;
   pPath.Zero();
   lPath.Init (sizeof(CPoint));

   switch (m_dwPath) {
   case 1:  // line

      lPath.Required (2);

      // first point at 0
      lPath.Add (&pPath);

      // second up
      pPath.p[2] += m_pPathParam.p[0];
      lPath.Add (&pPath);
      break;

   case 2:  // circle
      fLoop = TRUE;
      dwCurve = SEGCURVE_ELLIPSENEXT;

      lPath.Required (8);

      // left
      pPath.p[0] = -m_pPathParam.p[0];
      pPath.p[2] = 0;
      lPath.Add (&pPath);

      // UL
      pPath.p[2] = m_pPathParam.p[1];
      lPath.Add (&pPath);

      // U
      pPath.p[0] = 0;
      lPath.Add (&pPath);

      // UR
      pPath.p[0] = m_pPathParam.p[0];
      lPath.Add (&pPath);

      // R
      pPath.p[2] = 0;
      lPath.Add (&pPath);

      // BR
      pPath.p[2] = -m_pPathParam.p[1];
      lPath.Add (&pPath);

      // B
      pPath.p[0] = 0;
      lPath.Add (&pPath);

      // BL
      pPath.p[0] = -m_pPathParam.p[0];
      lPath.Add (&pPath);
      break;

   case 3:  // rectangle
      fLoop = TRUE;

      lPath.Required (4);

      // UL
      pPath.p[0] = -m_pPathParam.p[0];
      pPath.p[2] = m_pPathParam.p[1];
      lPath.Add (&pPath);

      // UR
      pPath.p[0] = m_pPathParam.p[0];
      lPath.Add (&pPath);

      // BR
      pPath.p[2] = -m_pPathParam.p[1];
      lPath.Add (&pPath);

      // BL
      pPath.p[0] = -m_pPathParam.p[0];
      lPath.Add (&pPath);
      break;

   case 4:  // coil
   case 5:  // spring
      {
         if (m_dwPath == 4) { // coil
            m_pPathParam.p[0] = max(m_pPathParam.p[0], .001);
            m_pPathParam.p[1] = max(m_pPathParam.p[1], m_pPathParam.p[0] / 20);
         }
         else {   // spring
            m_pPathParam.p[0] = max(m_pPathParam.p[0], .001);
            m_pPathParam.p[1] = max (m_pPathParam.p[1], .001);
            m_pPathParam.p[2] = max (m_pPathParam.p[2], m_pPathParam.p[1] / 20);
         }

         fp fDeltaR = (m_dwPath == 4) ? m_pPathParam.p[1] / 4.0 : 0;
         fp fDeltaV = (m_dwPath == 5) ? m_pPathParam.p[2] / 4.0 : 0;
         fp fRadius = (m_dwPath == 4) ? 0 : m_pPathParam.p[0];
         fp fVert = 0;
         fp fAngle = 0;

         dwCurve = SEGCURVE_ELLIPSENEXT;

         while (TRUE) {
            // always add at least one point
            pPath.p[0] = cos(fAngle) * fRadius;
            pPath.p[2] = -sin(fAngle) * fRadius;
            pPath.p[1] = -fVert;
            lPath.Add (&pPath);

            // might want to exit here
            if (m_dwPath == 4) {   // coil
               if (fRadius > m_pPathParam.p[0])
                  break;
            }
            else { // spring
               if (fVert > m_pPathParam.p[1])
                  break;
            }
            if (lPath.Num() > 100)
               break;   // just an arbitrary limit

            pPath.p[0] = cos(fAngle + PI/4) * (fRadius + fDeltaR/2) * sqrt((fp)2);
            pPath.p[2] = -sin(fAngle + PI/4) * (fRadius + fDeltaR/2) * sqrt((fp)2);
            pPath.p[1] = -(fVert + fDeltaV/2);
            lPath.Add (&pPath);

            // increment
            fAngle += PI/2;
            fRadius += fDeltaR;
            fVert += fDeltaV;
         }
         
      }
      break;
   default: // custom
      return;
   }

   m_Noodle.PathSpline (fLoop, lPath.Num(), (PCPoint) lPath.Get(0), (DWORD*)(size_t)dwCurve, 2);
}

/**********************************************************************************
CObjectNoodle::Delete - Called to delete this object
*/
void CObjectNoodle::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectNoodle::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectNoodle::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // object specific
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (42), m_pWorld);

   m_Noodle.Render (pr, &m_Renderrs);

   m_Renderrs.Commit();
}





/**********************************************************************************
CObjectNoodle::QueryBoundingBox - Standard API
*/
void CObjectNoodle::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   m_Noodle.QueryBoundingBox (pCorner1, pCorner2);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectNoodle::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectNoodle::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectNoodle::Clone (void)
{
   PCObjectNoodle pNew;

   pNew = new CObjectNoodle(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   m_Noodle.CloneTo (&pNew->m_Noodle);
   pNew->m_dwDisplayControl = m_dwDisplayControl;
   pNew->m_dwPath = m_dwPath;
   pNew->m_pPathParam.Copy (&m_pPathParam);

   return pNew;
}

static PWSTR gpszNoodle = L"Noodle";
static PWSTR gpszPath = L"Path";
static PWSTR gpszPathParam = L"PathParam";

PCMMLNode2 CObjectNoodle::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   PCMMLNode2 pSub;
   pSub = m_Noodle.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszNoodle);
      pNode->ContentAdd (pSub);
   }

   MMLValueSet (pNode, gpszDisplayControl, (int) m_dwDisplayControl);
   MMLValueSet (pNode, gpszPath, (int) m_dwPath);
   MMLValueSet (pNode, gpszPathParam, &m_pPathParam);

   return pNode;
}

BOOL CObjectNoodle::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszNoodle))
         m_Noodle.MMLFrom (pSub);
   }

   m_dwDisplayControl = (DWORD) MMLValueGetInt (pNode, gpszDisplayControl, 0);
   m_dwPath = (DWORD) MMLValueGetInt (pNode, gpszPath, 0);
   MMLValueGetPoint (pNode, gpszPathParam, &m_pPathParam);

   return TRUE;
}


/**********************************************************************************
CObjectNoodle::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectNoodle::DialogQuery (void)
{
   return TRUE;
}


/**********************************************************************************
CObjectNoodle::DialogCPQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectNoodle::DialogCPQuery (void)
{
   return TRUE;
}


/* NoodleDialogPage
*/
BOOL NoodleDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectNoodle pv = (PCObjectNoodle) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // path
         ComboBoxSet (pPage, L"path", pv->m_dwPath);
         pControl = pPage->ControlFind (L"custompath");
         if (pControl)
            pControl->Enable (pv->m_dwPath == 0);

         // shape
         DWORD dwShape;
         if (!pv->m_Noodle.ShapeDefaultGet(&dwShape))
            dwShape = 0;
         ComboBoxSet (pPage, L"shape", dwShape);
         pControl = pPage->ControlFind (L"customshape");
         if (pControl)
            pControl->Enable (dwShape == 0);

         // front
         PCSpline ps;
         ps = pv->m_Noodle.FrontSplineGet();
         pControl = pPage->ControlFind (L"front");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), ps ? TRUE : FALSE);
         pControl = pPage->ControlFind (L"customfront");
         if (pControl)
            pControl->Enable (ps ? TRUE : FALSE);

         // front
         ps = pv->m_Noodle.ScaleSplineGet ();
         pControl = pPage->ControlFind (L"scale");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), ps ? TRUE : FALSE);
         pControl = pPage->ControlFind (L"customscale");
         if (pControl)
            pControl->Enable (ps ? TRUE : FALSE);

         // bevel
         WCHAR szTemp[32];
         DWORD i;
         DWORD dwMode;
         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"bevel%d", (int) i);
            pv->m_Noodle.BevelGet (!i, &dwMode, NULL);
            ComboBoxSet (pPage, szTemp, dwMode);
         }

         // end caps
         pControl = pPage->ControlFind (L"drawends");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_Noodle.DrawEndsGet());

         // sizes
         CPoint pScale;
         pScale.Zero();
         pv->m_Noodle.ScaleVectorGet (&pScale);
         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"size%d", (int) i);
            MeasureToString (pPage, szTemp, pScale.p[i]);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (pv->m_Noodle.ScaleSplineGet() ? FALSE : TRUE);
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // BUGFIX - Allow edit controls for these
         // since only edit controls are ones for object
         pv->m_pWorld->ObjectAboutToChange (pv);
         if (!pv->m_Noodle.ScaleSplineGet()) {
            CPoint pScale;
            pv->m_Noodle.ScaleVectorGet(&pScale);
            MeasureParseString (pPage, L"size0", &pScale.p[0]);
            MeasureParseString (pPage, L"size1", &pScale.p[1]);
            pScale.p[0] = max(CLOSE,pScale.p[0]);
            pScale.p[1] = max(CLOSE,pScale.p[1]);
            pv->m_Noodle.ScaleVector (&pScale);
            
         }
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         DWORD dwVal;
         PCEscControl pControl;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszBevel = L"bevel";
         DWORD dwBevelLen = (DWORD)wcslen(pszBevel);

         if (!wcsncmp(psz, pszBevel, dwBevelLen)) {
            DWORD dwBevel = _wtoi(psz + dwBevelLen);
            if (dwBevel > 1)
               return TRUE;
            DWORD dwMode;
            pv->m_Noodle.BevelGet (!dwBevel, &dwMode, NULL);
            if (dwMode == dwVal)
               return TRUE;

            // else, switching modes
            CPoint pVal;
            dwMode = dwVal;
            pVal.Zero();
            pVal.p[2] = (dwBevel ? 1 : -1);
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwDisplayControl = 4;   // bevel
            pv->m_Noodle.BevelSet (!dwBevel, dwMode, &pVal);
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }

         if (!_wcsicmp(psz, L"path")) {
            if (dwVal == pv->m_dwPath)
               return TRUE;   // no real change

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwPath = dwVal;
            pv->m_pPathParam.Zero();
            switch (pv->m_dwPath) {
            case 1:  // line
               pv->m_pPathParam.p[0] = 1;
               break;
            case 2:  // circle
            case 3:  // rectangle
               pv->m_pPathParam.p[0] = pv->m_pPathParam.p[1] = .5;
               break;
            case 4:  // coil
               pv->m_pPathParam.p[0] = .5; // outer radius
               pv->m_pPathParam.p[1] = .25;   // distance between coil
               break;
            case 5:  // spring
               pv->m_pPathParam.p[0] = .5; // radius
               pv->m_pPathParam.p[1] = 1; // height
               pv->m_pPathParam.p[2] = .25; // distance between coil - in height
               break;
            }
            pv->PathSplineFromParam ();
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable button
            pControl = pPage->ControlFind (L"custompath");
            if (pControl)
               pControl->Enable (pv->m_dwPath == 0);
         }
         else if (!_wcsicmp(psz, L"shape")) {
            DWORD dwShape;
            if (!pv->m_Noodle.ShapeDefaultGet(&dwShape))
               dwShape = 0;

            if (dwVal == dwShape)
               return TRUE;   // no real change
            dwShape = dwVal;

            pv->m_pWorld->ObjectAboutToChange (pv);
            if (dwShape)
               pv->m_Noodle.ShapeDefault (dwShape);
            else {
               CPoint      paPoints[4];
               memset (paPoints, 0, sizeof(paPoints));
               paPoints[0].p[0] = -0.5;
               paPoints[0].p[1] = .5;
               paPoints[1].p[0] = .5;
               paPoints[1].p[1] = .5;
               paPoints[2].p[0] = .5;
               paPoints[2].p[1] = -.5;
               paPoints[3].p[0] = -.5;
               paPoints[3].p[1] = -.5;
               pv->m_Noodle.ShapeSpline (TRUE, 4, paPoints, (DWORD*)SEGCURVE_LINEAR, 2);
            }
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable button
            pControl = pPage->ControlFind (L"customshape");
            if (pControl)
               pControl->Enable (dwShape == 0);
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

         if (!_wcsicmp(psz,  L"front")) {
            BOOL fWant, fHave;
            fWant = p->pControl->AttribGetBOOL (Checked());

            PCSpline ps;
            ps = pv->m_Noodle.FrontSplineGet();
            fHave = (ps ? TRUE : FALSE);

            if (!fWant == !fHave)
               return TRUE;   // nothing to do

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwDisplayControl = 3;   // front
            CPoint ap[2];
            memset (ap, 0, sizeof(ap));
            ap[0].p[1] = -1;
            ap[1].Copy (&ap[0]);
            if (ps)
               pv->m_Noodle.FrontVector (&ap[0]);
            else
               pv->m_Noodle.FrontSpline (2, &ap[0], (DWORD*) SEGCURVE_LINEAR);
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable button
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"customfront");
            if (pControl)
               pControl->Enable (pv->m_Noodle.FrontSplineGet() ? TRUE : FALSE);
         }
         else if (!_wcsicmp(psz,  L"scale")) {
            BOOL fWant, fHave;
            fWant = p->pControl->AttribGetBOOL (Checked());

            PCSpline ps;
            ps = pv->m_Noodle.ScaleSplineGet();
            fHave = (ps ? TRUE : FALSE);

            if (!fWant == !fHave)
               return TRUE;   // nothing to do

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwDisplayControl = 2;   // scale
            CPoint ap[2];
            memset (ap, 0, sizeof(ap));
            if (ps) {
               ps->OrigPointGet (0, &ap[0]);
               pv->m_Noodle.ScaleVector (&ap[0]);
            }
            else {
               pv->m_Noodle.ScaleVectorGet (&ap[0]);
               ap[1].Copy (&ap[0]);
               pv->m_Noodle.ScaleSpline (2, &ap[0], (DWORD*) SEGCURVE_LINEAR);
            }
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable button
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"customscale");
            if (pControl)
               pControl->Enable (pv->m_Noodle.ScaleSplineGet() ? TRUE : FALSE);
            DWORD i;
            WCHAR szTemp[64];
            for (i = 0; i < 2; i++) {
               swprintf (szTemp, L"size%d", (int) i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->Enable (pv->m_Noodle.ScaleSplineGet() ? FALSE : TRUE);
            }
         }
         else if (!_wcsicmp(psz,  L"drawends")) {
            BOOL fWant;
            fWant = p->pControl->AttribGetBOOL (Checked());

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_Noodle.DrawEndsSet (fWant);
            pv->m_Noodle.BackfaceCullSet (fWant);
            pv->m_pWorld->ObjectChanged (pv);
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Noodle settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* NoodleDisplayPage
*/
BOOL NoodleDisplayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectNoodle pv = (PCObjectNoodle) pPage->m_pUserData;
   PWSTR apszName[5] = {L"path", L"shape", L"scale", L"front", L"bevel"};

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         PWSTR psz;

         psz = NULL;
         if (pv->m_dwDisplayControl < 5)
            psz = apszName[pv->m_dwDisplayControl];
         else
            break;
         pControl = pPage->ControlFind (psz);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         // keep track of current display control
         DWORD i;
         for (i = 0; i < 5; i++)
            if (!_wcsicmp(psz, apszName[i]) && p->pControl->AttribGetBOOL(Checked())) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_dwDisplayControl = i;
               pv->m_pWorld->ObjectChanged (pv);

               // make sure they're visible
               pv->m_pWorld->SelectionClear();
               GUID gObject;
               pv->GUIDGet (&gObject);
               pv->m_pWorld->SelectionAdd (pv->m_pWorld->ObjectFind(&gObject));
               break;
            }
         // fall through
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Which control points are displayed";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/********************************************************************************
NoodGenerateThreeDFromSpline - Given a spline on a surface, this sets a threeD
control with the spline.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSpline    pSpline - Spline to draw
   PCSpline    pShapeSpline - Spline to use for the shape. If NULL use pSpline for the shape
   DWORD       dwUse - If 0 it's for adding/remove splines, else if 1 it's for cycling curves
   BOOL        fRot - Rotate by 90 degrees
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   LOBYTE = x
   3rd lowest byte = 1 for edge, 2 for point
*/
BOOL NoodGenerateThreeDFromSpline (PWSTR pszControl, PCEscPage pPage, PCSpline pSpline,
                                      PCSpline pSplineShape, DWORD dwUse, BOOL fRot)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;
   if (!pSplineShape)
      pSpline = pSplineShape;

   // figure out center
   CPoint pMin, pMax, pt;
   DWORD i,j;
   for (i = 0; i < pSplineShape->OrigNumPointsGet(); i++) {
      pSplineShape->OrigPointGet(i, &pt);
      pt.p[3] = 1;

      if (!i) {
         pMin.Copy (&pt);
         pMax.Copy (&pt);
         continue;
      }

      for (j = 0; j < 3; j++) {
         pMin.p[j] = min(pMin.p[j], pt.p[j]);
         pMax.p[j] = max(pMax.p[j], pt.p[j]);
      }
   }

   // and center
   CPoint pCenter;
   pCenter.Add (&pMin, &pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fMax;
   fMax = max(pMax.p[0] - pMin.p[0], pMax.p[2] - pMin.p[2]);
   // NOTE: assume p[1] == 0
   fMax = max(0.001, fMax);
   fMax /= 5;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   if (fRot)
      MemCat (&gMemTemp, L"<rotatex val=-90/>");
   MemCat (&gMemTemp, L"<backculloff/>");   //  so that Z is up (on the screen)


   // scale to convert from points in pSpline to points in pSplineShape
   fp fScaleIndex;
   fScaleIndex = (pSpline->LoopedGet() ? 0 : -1) + pSpline->OrigNumPointsGet();
      // BUGFIX - Was pSplineShape->LoopedGet() but dont need to do since noodle object makes sure synchronized
   fScaleIndex = 1.0 / fScaleIndex;

   // draw the outline
   DWORD dwNum;
   dwNum = pSpline->OrigNumPointsGet();
   DWORD x;
   for (x = 0; x < dwNum; x++) {
      CPoint p1, p2;
      pSplineShape->LocationGet ((fp) x * fScaleIndex, &p1);
      //pSpline->OrigPointGet (x, &p1);
      p1.p[3] = 1;
      pSplineShape->LocationGet ((fp) ((x+1) % dwNum) * fScaleIndex, &p2);
      //pSpline->OrigPointGet ((x+1) % dwNum, &p2);
      p2.p[3] = 1;

      if (!pSplineShape->LoopedGet() && (x+1 >= dwNum))
         break;   // not looped, so don't complete

      // center and scale
      p1.Subtract (&pCenter);
      p2.Subtract (&pCenter);
      p1.Scale (1.0 / fMax);
      p2.Scale (1.0 / fMax);


      // draw a line
      if (dwUse == 0)
         MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
      else {
         DWORD dwSeg;
         pSpline->OrigSegCurveGet (x, &dwSeg);
         switch (dwSeg) {
         case SEGCURVE_CUBIC:
            MemCat (&gMemTemp, L"<colordefault color=#8080ff/>");
            break;
         case SEGCURVE_CIRCLENEXT:
            MemCat (&gMemTemp, L"<colordefault color=#ffc0c0/>");
            break;
         case SEGCURVE_CIRCLEPREV:
            MemCat (&gMemTemp, L"<colordefault color=#c04040/>");
            break;
         case SEGCURVE_ELLIPSENEXT:
            MemCat (&gMemTemp, L"<colordefault color=#40c040/>");
            break;
         case SEGCURVE_ELLIPSEPREV:
            MemCat (&gMemTemp, L"<colordefault color=#004000/>");
            break;
         default:
         case SEGCURVE_LINEAR:
            MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
            break;
         }
      }

      // set the ID
      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)((1 << 16) | x));
      MemCat (&gMemTemp, L"/>");

      MemCat (&gMemTemp, L"<shapearrow tip=false width=.2");

      WCHAR szTemp[128];
      swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
         (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
      MemCat (&gMemTemp, szTemp);

      // do push point if more than 3 points
      if ((dwUse == 0) && (dwNum > (DWORD)(3 - (pSpline->LoopedGet() ? 0 : 1) ))) {
         MemCat (&gMemTemp, L"<matrixpush>");
         swprintf (szTemp, L"<translate point=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2]);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
         // set the ID
         MemCat (&gMemTemp, L"<id val=");
         MemCat (&gMemTemp, (int)((2 << 16) | x));
         MemCat (&gMemTemp, L"/>");

         MemCat (&gMemTemp, L"<MeshSphere radius=.4/><shapemeshsurface/>");

         MemCat (&gMemTemp, L"</matrixpush>");
      }
   }

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}


/* NoodleCurvePathPage
*/
BOOL NoodleCurvePathPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectNoodle pv = (PCObjectNoodle)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         PCSpline ps;
         ps = pv->m_Noodle.PathSplineGet();
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1);

            pControl = pPage->ControlFind (L"looped");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), ps->LoopedGet());

            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            ComboBoxSet (pPage, L"divide", dwMax);
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"divide")) {
            PCSpline ps;
            ps = pv->m_Noodle.PathSplineGet();
            if (!ps)
               return TRUE;
            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            if (dwVal == dwMax)
               return TRUE;   // nothing to change

            // get all the points
            CMem  memPoints, memSegCurve;
            DWORD dwOrig;
            BOOL fLooped;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMin, &dwMax, &fDetail))
               return FALSE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_Noodle.PathSpline (fLooped, dwOrig,
               (PCPoint) memPoints.p, (DWORD*) memSegCurve.p, dwVal);
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         BOOL  fCol;
         if (!_wcsicmp(p->pControl->m_pszName, L"edgeaddremove"))
            fCol = TRUE;
         else if (!_wcsicmp(p->pControl->m_pszName, L"edgecurve"))
            fCol = FALSE;
         else
            break;

         // figure out x, y, and what clicked on
         DWORD x, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (BYTE) p->dwMajor;
         if ((dwMode < 1) || (dwMode > 2))
            break;

         // get all the points
         CMem  memPoints, memSegCurve;
         DWORD dwMinDivide, dwMaxDivide, dwOrig;
         fp fDetail;
         BOOL fLooped;
         PCSpline ps;
         ps = pv->m_Noodle.PathSplineGet();
         if (!ps)
            return FALSE;
         if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
            return FALSE;

         // make sure have enough memory for extra
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         PCPoint paPoints;
         DWORD *padw;
         paPoints = (PCPoint) memPoints.p;
         padw = (DWORD*) memSegCurve.p;

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               memmove (paPoints + (x+1), paPoints + x, sizeof(CPoint) * (dwOrig-x));
               paPoints[x+1].Add (paPoints + ((x+2) % (dwOrig+1)));
               paPoints[x+1].Scale (.5);
               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               dwOrig++;
            }
            else if (dwMode == 2) {
               // deleting
               memmove (paPoints + x, paPoints + (x+1), sizeof(CPoint) * (dwOrig-x-1));
               memmove (padw + x, padw + (x+1), sizeof(DWORD) * (dwOrig - x - 1));
               dwOrig--;
            }
         }
         else {
            // setting curvature
            if (dwMode == 1) {
               padw[x] = (padw[x] + 1) % (SEGCURVE_MAX+1);
            }
         }

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDisplayControl = 0;   // so display these points
         pv->m_Noodle.PathSpline (fLooped, dwOrig, paPoints, padw, dwMaxDivide);
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         ps = pv->m_Noodle.PathSplineGet();
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1);
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

         if (!_wcsicmp(psz, L"looped")) {
            // get all the points
            CMem  memPoints, memSegCurve;
            DWORD dwMinDivide, dwMaxDivide, dwOrig;
            fp fDetail;
            BOOL fLooped;
            PCSpline ps;
            ps = pv->m_Noodle.PathSplineGet();
            if (!ps)
               return FALSE;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
               return FALSE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwDisplayControl = 0;   // so display these points
            pv->m_Noodle.PathSpline (p->pControl->AttribGetBOOL(Checked()), dwOrig,
               (PCPoint) memPoints.p, (DWORD*) memSegCurve.p, dwMaxDivide);
            pv->m_pWorld->ObjectChanged (pv);

            // redraw the shapes
            ps = pv->m_Noodle.PathSplineGet();
            if (ps) {
               NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0);
               NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1);
            }
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Extrusion path";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* NoodleCurveShapePage
*/
BOOL NoodleCurveShapePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectNoodle pv = (PCObjectNoodle)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         PCSpline ps;
         ps = pv->m_Noodle.ShapeSplineGet();
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, FALSE);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, FALSE);

            pControl = pPage->ControlFind (L"looped");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), ps->LoopedGet());

            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            ComboBoxSet (pPage, L"divide", dwMax);
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"divide")) {
            PCSpline ps;
            ps = pv->m_Noodle.ShapeSplineGet();
            if (!ps)
               return TRUE;
            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            if (dwVal == dwMax)
               return TRUE;   // nothing to change

            // get all the points
            CMem  memPoints, memSegCurve;
            DWORD dwOrig;
            BOOL fLooped;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMin, &dwMax, &fDetail))
               return FALSE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_Noodle.ShapeSpline (fLooped, dwOrig,
               (PCPoint) memPoints.p, (DWORD*) memSegCurve.p, dwVal);
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         BOOL  fCol;
         if (!_wcsicmp(p->pControl->m_pszName, L"edgeaddremove"))
            fCol = TRUE;
         else if (!_wcsicmp(p->pControl->m_pszName, L"edgecurve"))
            fCol = FALSE;
         else
            break;

         // figure out x, y, and what clicked on
         DWORD x, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (BYTE) p->dwMajor;
         if ((dwMode < 1) || (dwMode > 2))
            break;

         // get all the points
         CMem  memPoints, memSegCurve;
         DWORD dwMinDivide, dwMaxDivide, dwOrig;
         fp fDetail;
         BOOL fLooped;
         PCSpline ps;
         ps = pv->m_Noodle.ShapeSplineGet();
         if (!ps)
            return FALSE;
         if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
            return FALSE;

         // make sure have enough memory for extra
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         PCPoint paPoints;
         DWORD *padw;
         paPoints = (PCPoint) memPoints.p;
         padw = (DWORD*) memSegCurve.p;

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               memmove (paPoints + (x+1), paPoints + x, sizeof(CPoint) * (dwOrig-x));
               paPoints[x+1].Add (paPoints + ((x+2) % (dwOrig+1)));
               paPoints[x+1].Scale (.5);
               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               dwOrig++;
            }
            else if (dwMode == 2) {
               // deleting
               memmove (paPoints + x, paPoints + (x+1), sizeof(CPoint) * (dwOrig-x-1));
               memmove (padw + x, padw + (x+1), sizeof(DWORD) * (dwOrig - x - 1));
               dwOrig--;
            }
         }
         else {
            // setting curvature
            if (dwMode == 1) {
               padw[x] = (padw[x] + 1) % (SEGCURVE_MAX+1);
            }
         }

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDisplayControl = 1;   // shape
         pv->m_Noodle.ShapeSpline (fLooped, dwOrig, paPoints, padw, dwMaxDivide);
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         ps = pv->m_Noodle.ShapeSplineGet();
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, FALSE);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, FALSE);
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

         if (!_wcsicmp(psz, L"looped")) {
            // get all the points
            CMem  memPoints, memSegCurve;
            DWORD dwMinDivide, dwMaxDivide, dwOrig;
            fp fDetail;
            BOOL fLooped;
            PCSpline ps;
            ps = pv->m_Noodle.ShapeSplineGet();
            if (!ps)
               return FALSE;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
               return FALSE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwDisplayControl = 1;   // shape
            pv->m_Noodle.ShapeSpline (p->pControl->AttribGetBOOL(Checked()), dwOrig,
               (PCPoint) memPoints.p, (DWORD*) memSegCurve.p, dwMaxDivide);
            pv->m_pWorld->ObjectChanged (pv);

            // redraw the shapes
            ps = pv->m_Noodle.ShapeSplineGet();
            if (ps) {
               NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, ps, 0, FALSE);
               NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, ps, 1, FALSE);
            }
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Profile shape";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/* NoodleCurveFrontPage
*/
BOOL NoodleCurveFrontPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectNoodle pv = (PCObjectNoodle)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         PCSpline ps;
         ps = pv->m_Noodle.FrontSplineGet();
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, pv->m_Noodle.PathSplineGet(), 0);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, pv->m_Noodle.PathSplineGet(), 1);

            pControl = pPage->ControlFind (L"looped");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), ps->LoopedGet());

            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            ComboBoxSet (pPage, L"divide", dwMax);
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"divide")) {
            PCSpline ps;
            ps = pv->m_Noodle.FrontSplineGet();
            if (!ps)
               return TRUE;
            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            if (dwVal == dwMax)
               return TRUE;   // nothing to change

            // get all the points
            CMem  memPoints, memSegCurve;
            DWORD dwOrig;
            BOOL fLooped;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMin, &dwMax, &fDetail))
               return FALSE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_Noodle.FrontSpline (dwOrig,
               (PCPoint) memPoints.p, (DWORD*) memSegCurve.p, dwVal);
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         BOOL  fCol;
         if (!_wcsicmp(p->pControl->m_pszName, L"edgeaddremove"))
            fCol = TRUE;
         else if (!_wcsicmp(p->pControl->m_pszName, L"edgecurve"))
            fCol = FALSE;
         else
            break;

         // figure out x, y, and what clicked on
         DWORD x, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (BYTE) p->dwMajor;
         if ((dwMode < 1) || (dwMode > 2))
            break;

         // get all the points
         CMem  memPoints, memSegCurve;
         DWORD dwMinDivide, dwMaxDivide, dwOrig;
         fp fDetail;
         BOOL fLooped;
         PCSpline ps;
         ps = pv->m_Noodle.FrontSplineGet();
         if (!ps)
            return FALSE;
         if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
            return FALSE;

         // make sure have enough memory for extra
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         PCPoint paPoints;
         DWORD *padw;
         paPoints = (PCPoint) memPoints.p;
         padw = (DWORD*) memSegCurve.p;

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               memmove (paPoints + (x+1), paPoints + x, sizeof(CPoint) * (dwOrig-x));
               paPoints[x+1].Add (paPoints + ((x+2) % (dwOrig+1)));
               paPoints[x+1].Scale (.5);
               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               dwOrig++;
            }
            else if (dwMode == 2) {
               // deleting
               memmove (paPoints + x, paPoints + (x+1), sizeof(CPoint) * (dwOrig-x-1));
               memmove (padw + x, padw + (x+1), sizeof(DWORD) * (dwOrig - x - 1));
               dwOrig--;
            }
         }
         else {
            // setting curvature
            if (dwMode == 1) {
               padw[x] = (padw[x] + 1) % (SEGCURVE_MAX+1);
            }
         }

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDisplayControl = 3;   // front
         pv->m_Noodle.FrontSpline (dwOrig, paPoints, padw, dwMaxDivide);
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         ps = pv->m_Noodle.FrontSplineGet();
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, pv->m_Noodle.PathSplineGet(), 0);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, pv->m_Noodle.PathSplineGet(), 1);
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Extrusion front";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFUSELOOP")) {
            p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFUSELOOP")) {
            p->pszSubString = L"</comment>";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* NoodleCurveScalePage
*/
BOOL NoodleCurveScalePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectNoodle pv = (PCObjectNoodle)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         PCSpline ps;
         ps = pv->m_Noodle.ScaleSplineGet();
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, pv->m_Noodle.PathSplineGet(), 0);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, pv->m_Noodle.PathSplineGet(), 1);

            pControl = pPage->ControlFind (L"looped");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), ps->LoopedGet());

            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            ComboBoxSet (pPage, L"divide", dwMax);
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         DWORD dwVal;
         dwVal = p->pszName ? (DWORD) _wtoi(p->pszName) : 0;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"divide")) {
            PCSpline ps;
            ps = pv->m_Noodle.ScaleSplineGet();
            if (!ps)
               return TRUE;
            DWORD dwMin, dwMax;
            fp fDetail;
            ps->DivideGet (&dwMin, &dwMax, &fDetail);
            if (dwVal == dwMax)
               return TRUE;   // nothing to change

            // get all the points
            CMem  memPoints, memSegCurve;
            DWORD dwOrig;
            BOOL fLooped;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMin, &dwMax, &fDetail))
               return FALSE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_Noodle.ScaleSpline (dwOrig,
               (PCPoint) memPoints.p, (DWORD*) memSegCurve.p, dwVal);
            pv->m_pWorld->ObjectChanged (pv);
            return TRUE;
         }
      }

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         BOOL  fCol;
         if (!_wcsicmp(p->pControl->m_pszName, L"edgeaddremove"))
            fCol = TRUE;
         else if (!_wcsicmp(p->pControl->m_pszName, L"edgecurve"))
            fCol = FALSE;
         else
            break;

         // figure out x, y, and what clicked on
         DWORD x, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (BYTE) p->dwMajor;
         if ((dwMode < 1) || (dwMode > 2))
            break;

         // get all the points
         CMem  memPoints, memSegCurve;
         DWORD dwMinDivide, dwMaxDivide, dwOrig;
         fp fDetail;
         BOOL fLooped;
         PCSpline ps;
         ps = pv->m_Noodle.ScaleSplineGet();
         if (!ps)
            return FALSE;
         if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
            return FALSE;

         // make sure have enough memory for extra
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         PCPoint paPoints;
         DWORD *padw;
         paPoints = (PCPoint) memPoints.p;
         padw = (DWORD*) memSegCurve.p;

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               memmove (paPoints + (x+1), paPoints + x, sizeof(CPoint) * (dwOrig-x));
               paPoints[x+1].Add (paPoints + ((x+2) % (dwOrig+1)));
               paPoints[x+1].Scale (.5);
               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               dwOrig++;
            }
            else if (dwMode == 2) {
               // deleting
               memmove (paPoints + x, paPoints + (x+1), sizeof(CPoint) * (dwOrig-x-1));
               memmove (padw + x, padw + (x+1), sizeof(DWORD) * (dwOrig - x - 1));
               dwOrig--;
            }
         }
         else {
            // setting curvature
            if (dwMode == 1) {
               padw[x] = (padw[x] + 1) % (SEGCURVE_MAX+1);
            }
         }

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDisplayControl = 2;   // Scale
         pv->m_Noodle.ScaleSpline (dwOrig, paPoints, padw, dwMaxDivide);
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         ps = pv->m_Noodle.ScaleSplineGet();
         if (ps) {
            NoodGenerateThreeDFromSpline (L"edgeaddremove", pPage, ps, pv->m_Noodle.PathSplineGet(), 0);
            NoodGenerateThreeDFromSpline (L"edgecurve", pPage, ps, pv->m_Noodle.PathSplineGet(), 1);
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Extrusion size";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFUSELOOP")) {
            p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFUSELOOP")) {
            p->pszSubString = L"</comment>";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/**********************************************************************************
CObjectNoodle::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectNoodle::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
firstpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLEDIALOG, NoodleDialogPage, this);
firstpage2:
   if (!pszRet)
      return FALSE;

   if (!_wcsicmp(pszRet, L"custompath")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLECURVE, NoodleCurvePathPage, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"customshape")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLECURVE, NoodleCurveShapePage, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"customfront")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLECURVE, NoodleCurveFrontPage, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"customscale")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLECURVE, NoodleCurveScalePage, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectNoodle::DialogCPShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectNoodle::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLNOODLEDISPLAY, NoodleDisplayPage, this);
   if (!pszRet)
      return FALSE;
   return !_wcsicmp(pszRet, Back());
}


/*************************************************************************************
CObjectNoodle::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectNoodle::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   DWORD i;
   CPoint pUp, pRight, pFront, pLoc, pScale;
   fp fLoc;
   if (dwID >= ControlPointNum ())
      return FALSE;


   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   //pInfo->dwFreedom = 0;   // any
   pInfo->dwStyle = CPSTYLE_SPHERE;
   pInfo->cColor = RGB(0xff,0,0xff);

   // determine the size
   CPoint pSize;
   PCSpline ps;
   if (ps = m_Noodle.ScaleSplineGet()) {
      CPoint p;
      pSize.Zero();
      for (i = 0; i < ps->OrigNumPointsGet(); i++) {
         ps->OrigPointGet (0, &p);
         pSize.Max (&p);
      }
   }
   else {
      m_Noodle.ScaleVectorGet (&pSize);
   }
   pInfo->fSize = max(pSize.p[0], pSize.p[1]) * 2;
   pInfo->fSize = max(.001, pInfo->fSize);

   switch (m_dwDisplayControl) {
   case 0:  // path
      pInfo->cColor = RGB(0xff,0xff,0);
      
      switch (m_dwPath) {
      case 1:  // line
         pInfo->pLocation.p[2] = m_pPathParam.p[0];
         wcscpy (pInfo->szName, L"Length");
         MeasureToString (m_pPathParam.p[0], pInfo->szMeasurement);
         break;
      case 2:  // circle
      case 3:  // rectangle
         switch (dwID) {
         case 0:  // right/left
            pInfo->pLocation.p[0] = m_pPathParam.p[0];
            wcscpy (pInfo->szName, L"Width");
            MeasureToString (m_pPathParam.p[0] * 2, pInfo->szMeasurement);
            break;
         case 1:  // up/down
            pInfo->pLocation.p[2] = m_pPathParam.p[1];
            wcscpy (pInfo->szName, L"Height");
            MeasureToString (m_pPathParam.p[1] * 2, pInfo->szMeasurement);
            break;
         }
         break;
      case 4:  // coil
         switch (dwID) {
         case 0:  // right/left
            pInfo->pLocation.p[0] = m_pPathParam.p[0];
            wcscpy (pInfo->szName, L"Radius");
            MeasureToString (m_pPathParam.p[0], pInfo->szMeasurement);
            break;
         case 1:  // radius delta
            pInfo->pLocation.p[0] = m_pPathParam.p[1];
            wcscpy (pInfo->szName, L"Coil separation");
            MeasureToString (m_pPathParam.p[1], pInfo->szMeasurement);
            break;
         }
         break;
      case 5:  // spring
         switch (dwID) {
         case 0:  // radius
            pInfo->pLocation.p[0] = m_pPathParam.p[0];
            wcscpy (pInfo->szName, L"Radius");
            MeasureToString (m_pPathParam.p[0], pInfo->szMeasurement);
            break;
         case 1:  // height
            pInfo->pLocation.p[1] = -m_pPathParam.p[1];
            wcscpy (pInfo->szName, L"Height");
            MeasureToString (m_pPathParam.p[1], pInfo->szMeasurement);
            break;
         case 2:  // height delta
            pInfo->pLocation.p[1] = -m_pPathParam.p[2];
            wcscpy (pInfo->szName, L"Coil separation");
            MeasureToString (m_pPathParam.p[2], pInfo->szMeasurement);
            break;
         }
         break;
      default: // custom
         ps = m_Noodle.PathSplineGet ();
         if (!ps)
            return FALSE;
         if (!ps->OrigPointGet (dwID, &pInfo->pLocation))
            return FALSE;
         wcscpy (pInfo->szName, L"Path");
         break;
      }
      break;
   case 1:  // shape
      pInfo->cColor = RGB(0xff,0,0);
      pInfo->dwStyle = CPSTYLE_SPHERE;
      ps = m_Noodle.ShapeSplineGet();
      if (!ps)
         return FALSE;

      if (!ps->OrigPointGet (dwID, &pSize))
         return FALSE;
      fLoc = 0;   // always do shape from the end
      
      // get the vectors
      if (!m_Noodle.LocationAndDirection (fLoc, &pLoc, &pFront, &pUp, &pRight, &pScale))
         return FALSE;

      pRight.Scale (pScale.p[0] * pSize.p[0]);
      pUp.Scale (-pScale.p[1] * pSize.p[1]);
      pInfo->pLocation.Add (&pRight, &pUp);
      pInfo->pLocation.Add (&pLoc);
      pInfo->fSize /= 10.0;   // since only numbs on end

      wcscpy (pInfo->szName, L"Profile shape");
      break;

   case 2:  // scale
      DWORD dwDim;
      dwDim = dwID / (ControlPointNum() / 2);
      dwID = dwID % (ControlPointNum() / 2);
      pInfo->cColor = RGB(0,0xff,0);
      pInfo->dwStyle = CPSTYLE_CUBE;
      ps = m_Noodle.ScaleSplineGet();
      if (ps) {
         if (!ps->OrigPointGet (dwID, &pSize))
            return FALSE;
         fLoc = ps->OrigNumPointsGet() - (ps->LoopedGet() ? 0 : 1);
         fLoc = (fp) dwID / fLoc;
      }
      else {
         m_Noodle.ScaleVectorGet (&pSize);
         fLoc = .5;
      }
      
      // get the vectors
      if (!m_Noodle.LocationAndDirection (fLoc, &pLoc, &pFront, &pUp, &pRight))
         return FALSE;

      pInfo->pLocation.Copy (dwDim ? &pUp : &pRight);
      pInfo->pLocation.Scale (pSize.p[dwDim] / 2.0);
      pInfo->pLocation.Add (&pLoc);
      pInfo->fSize /= 10.0;   // since only numbs on end

      wcscpy (pInfo->szName, dwDim ? L"Height" : L"Width");
      MeasureToString (pSize.p[dwDim], pInfo->szMeasurement);
      break;

   case 3:  // front
      pInfo->cColor = RGB(0,0,0xff);
      pInfo->dwStyle = CPSTYLE_POINTER;
      ps = m_Noodle.FrontSplineGet();
      if (ps) {
         if (!ps->OrigPointGet (dwID, &pSize))
            return FALSE;
         fLoc = ps->OrigNumPointsGet() - (ps->LoopedGet() ? 0 : 1);
         fLoc = (fp) dwID / fLoc;
      }
      else {
         m_Noodle.FrontVectorGet (&pSize);
         fLoc = .5;
      }
      pSize.Normalize();

      // where does it go
      ps = m_Noodle.PathSplineGet();
      if (!ps)
         return FALSE;
      if (!ps->LocationGet (fLoc, &pInfo->pLocation))
         return FALSE;
      //pInfo->pDirection.Copy (&pSize);
      pSize.Scale (pInfo->fSize);
      pInfo->pLocation.Add (&pSize);
      pInfo->fSize /= 10.0; // since offset from center
      wcscpy (pInfo->szName, L"Front direction");
      break;

   case 4:  // bevel
      pInfo->cColor = RGB(0,0xff,0);
      pInfo->dwStyle = CPSTYLE_POINTER;

      // get the bevel
      DWORD dwMode;
      m_Noodle.BevelGet (!dwID, &dwMode, &pScale);
      if (!dwMode)
         return FALSE;
      pScale.Normalize();
      pScale.Scale (pInfo->fSize);

      // find out the direction
      if (!m_Noodle.LocationAndDirection (dwID ? 1 : 0, &pLoc, &pFront, &pUp, &pRight))
         return FALSE;
      if (dwMode == 2) {
         // relative to object
         pFront.Zero();
         pFront.p[2] = 1;
         pRight.Zero();
         pRight.p[0] = 1;
         pUp.Zero();
         pUp.p[1] = -1;
      }
      CPoint p1, p2;
      p2.Zero();
      p1.Copy (&pFront);
      p1.Scale (pScale.p[2]);
      p2.Add (&p1);
      p1.Copy (&pRight);
      p1.Scale (pScale.p[0]);
      p2.Add (&p1);
      p1.Copy (&pUp);
      p1.Scale (-pScale.p[1]);
      p2.Add (&p1);

      // make sure points in right direction
      if (!dwID)
         pFront.Scale(-1); // since should be going in opposite direction
      if (pFront.DotProd(&p2) < 0)
         p2.Scale (-1); // flip direction

      pInfo->pLocation.Add (&pLoc, &p2);
//      pInfo->pDirection.Copy (&p2);
      pInfo->fSize /= 10.0; // since offset from center
      wcscpy (pInfo->szName, L"Bevel");

      break;
   }
   
   return TRUE;
}

/*************************************************************************************
CObjectNoodle::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectNoodle::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   PCSpline ps;
   CPoint pUp, pRight, pFront, pLoc, pScale;
   CPoint p, p2;
   fp fLoc;
   CPoint pSize;

   if (dwID >= ControlPointNum ())
      return FALSE;
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   switch (m_dwDisplayControl) {
   case 0:  // path
      switch (m_dwPath) {
      case 1:  // line
         m_pPathParam.p[0] = max (pVal->p[2], .001);
         break;
      case 2:  // circle
      case 3:  // rectangle
         switch (dwID) {
         case 0:  // right/left
            m_pPathParam.p[0] = max(pVal->p[0], .001);
            break;
         case 1:  // up/down
            m_pPathParam.p[1] = max(pVal->p[2], .001);
            break;
         }
         break;
      case 4:  // coil
         switch (dwID) {
         case 0:  // right/left
            m_pPathParam.p[0] = max(pVal->p[0], .001);
            break;
         case 1:  // radius delta
            m_pPathParam.p[1] = max(pVal->p[0], .001);
            break;
         }
         break;
      case 5:  // spring
         switch (dwID) {
         case 0:  // radius
            m_pPathParam.p[0] = max(pVal->p[0], .001);
            break;
         case 1:  // height
            m_pPathParam.p[1] = max(-pVal->p[1], .001);
            break;
         case 2:  // height delta
            m_pPathParam.p[2] = max(-pVal->p[1], .001);
            break;
         }
         break;
      default: // custom
         {
            CMem memPoints, memSegCurve;
            BOOL fLooped;
            DWORD dwPoints, dwMinDivide, dwMaxDivide;
            fp fDetail;

            ps = m_Noodle.PathSplineGet ();
            if (!ps)
               return FALSE;
            if (!ps->ToMem (&fLooped, &dwPoints, &memPoints, NULL,&memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
               return FALSE;

            PCPoint pPoints;
            pPoints = (PCPoint) memPoints.p;
            pPoints[dwID].Copy (pVal);

            m_Noodle.PathSpline (fLooped, dwPoints, (PCPoint)memPoints.p,
               (DWORD*) memSegCurve.p, dwMaxDivide);
         }
         break;
      }

      // recalc
      PathSplineFromParam();
      break;

   case 1:  // shape
      fLoc = .5;

      // get the vectors
      if (!m_Noodle.LocationAndDirection (fLoc, &pLoc, &pFront, &pUp, &pRight, &pScale))
         return FALSE;

      p.Subtract (pVal, &pLoc);
      pScale.p[0] = max(pScale.p[0], CLOSE);
      pScale.p[1] = max(pScale.p[1], CLOSE);
      p2.Zero();
      p2.p[0] = pRight.DotProd (&p) / pScale.p[0];
      p2.p[1] = -pUp.DotProd (&p) / pScale.p[1];

      // make sure in range of -.5 to .5 so scale OK
      p2.p[0] = min(.5, p2.p[0]);
      p2.p[1] = min(.5, p2.p[1]);
      p2.p[0] = max(-.5, p2.p[0]);
      p2.p[1] = max(-.5, p2.p[1]);

      // set it
      ps = m_Noodle.ShapeSplineGet();
      if (ps) {
         CMem memPoints, memSegCurve;
         BOOL fLooped;
         DWORD dwPoints, dwMinDivide, dwMaxDivide;
         fp fDetail;

         if (!ps->ToMem (&fLooped, &dwPoints, &memPoints, NULL,&memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
            return FALSE;

         PCPoint pPoints;
         pPoints = (PCPoint) memPoints.p;
         pPoints[dwID].Copy (&p2);

         m_Noodle.ShapeSpline (fLooped, dwPoints, (PCPoint)memPoints.p,
            (DWORD*) memSegCurve.p, dwMaxDivide);
      }
      else
         return FALSE;
      break;

   case 2:  // scale
      DWORD dwDim;
      dwDim = dwID / (ControlPointNum() / 2);
      dwID = dwID % (ControlPointNum() / 2);
      ps = m_Noodle.ScaleSplineGet();
      if (ps) {
         fLoc = ps->OrigNumPointsGet() - (ps->LoopedGet() ? 0 : 1);
         fLoc = (fp) dwID / fLoc;
      }
      else {
         fLoc = .5;
      }
      
      // get the vectors
      if (!m_Noodle.LocationAndDirection (fLoc, &pLoc, &pFront, &pUp, &pRight))
         return FALSE;

      p.Subtract (pVal, &pLoc);
      p2.Copy (dwDim ? &pUp : &pRight);
      p2.Normalize();
      pSize.p[dwDim] = p2.DotProd (&p) * 2.0;
      pSize.p[dwDim] = max(0, pSize.p[dwDim]);

      // set it
      ps = m_Noodle.ScaleSplineGet();
      if (ps) {
         CMem memPoints, memSegCurve;
         BOOL fLooped;
         DWORD dwPoints, dwMinDivide, dwMaxDivide;
         fp fDetail;

         if (!ps->ToMem (&fLooped, &dwPoints, &memPoints, NULL,&memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
            return FALSE;

         PCPoint pPoints;
         pPoints = (PCPoint) memPoints.p;
         pPoints[dwID].p[dwDim] = pSize.p[dwDim];

         m_Noodle.ScaleSpline (dwPoints, (PCPoint)memPoints.p,
            (DWORD*) memSegCurve.p, dwMaxDivide);
      }
      else {
         m_Noodle.ScaleVectorGet (&p);
         p.p[dwDim] = pSize.p[dwDim];
         m_Noodle.ScaleVector (&p);
      }
      break;

   case 3:  // front
      ps = m_Noodle.FrontSplineGet();
      if (ps) {
         fLoc = ps->OrigNumPointsGet() - (ps->LoopedGet() ? 0 : 1);
         fLoc = (fp) dwID / fLoc;
      }
      else {
         fLoc = .5;
      }

      // where does it go?
      ps = m_Noodle.PathSplineGet();
      if (!ps)
         return FALSE;
      if (!ps->LocationGet (fLoc, &pSize))
         return FALSE;

      pSize.Subtract (pVal);
      pSize.Scale(-1);
      pSize.Normalize();

      if (pSize.Length() > .5) {
         ps = m_Noodle.FrontSplineGet();
         if (ps) {
            CMem memPoints, memSegCurve;
            BOOL fLooped;
            DWORD dwPoints, dwMinDivide, dwMaxDivide;
            fp fDetail;

            if (!ps->ToMem (&fLooped, &dwPoints, &memPoints, NULL,&memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
               return FALSE;

            PCPoint pPoints;
            pPoints = (PCPoint) memPoints.p;
            pPoints[dwID].Copy (&pSize);

            m_Noodle.FrontSpline (dwPoints, (PCPoint)memPoints.p,
               (DWORD*) memSegCurve.p, dwMaxDivide);
         }
         else
            m_Noodle.FrontVector (&pSize);
      }
      break;
      
   case 4:  // bevel
      // get the bevel
      DWORD dwMode;
      m_Noodle.BevelGet (!dwID, &dwMode, &pScale);
      if (!dwMode)
         return FALSE;

      // find out the direction
      if (!m_Noodle.LocationAndDirection (dwID ? 1 : 0, &pLoc, &pFront, &pUp, &pRight))
         return FALSE;
      if (dwMode == 2) {
         // relative to object
         pFront.Zero();
         pFront.p[2] = 1;
         pRight.Zero();
         pRight.p[0] = 1;
         pUp.Zero();
         pUp.p[1] = -1;
      }

      // figure out the scale
      p.Subtract (pVal, &pLoc);

      pScale.p[2] = p.DotProd (&pFront);
      pScale.p[1] = -p.DotProd (&pUp);
      pScale.p[0] = p.DotProd (&pRight);

      m_Noodle.BevelSet (!dwID, dwMode, &pScale);

      break;
   }

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;

}


/*************************************************************************************
CObjectNoodle::ControlPointNum - Returns th enumber of contorl points
*/
DWORD CObjectNoodle::ControlPointNum (void)
{
   PCSpline ps;
   DWORD dwNum;
   dwNum = 0;

   switch (m_dwDisplayControl) {
   case 0:  // path
      switch (m_dwPath) {
      case 1:  // line
         dwNum = 1;
         break;
      case 2:  // circle
      case 3:  // rectangle
      case 4:  // coil
         dwNum = 2;
         break;
      case 5:  // spring
         dwNum = 3;
         break;
      default: // custom
         ps = m_Noodle.PathSplineGet ();
         if (ps)
            dwNum = ps->OrigNumPointsGet();
         break;
      }
      break;
   case 1:  // shape
      // only if not a default shape
      DWORD dwShape;
      if (!m_Noodle.ShapeDefaultGet(&dwShape))
         dwShape = 0;
      if (!dwShape && (ps = m_Noodle.ShapeSplineGet()))
         dwNum = ps->OrigNumPointsGet();
      break;

   case 2:  // scale
      ps = m_Noodle.ScaleSplineGet ();
      if (ps)
         dwNum = ps->OrigNumPointsGet() * 2;
      else
         dwNum = 1 * 2;
      break;
   case 3:  // front
      ps = m_Noodle.FrontSplineGet ();
      if (ps)
         dwNum = ps->OrigNumPointsGet();
      else
         dwNum = 1;
      break;
   case 4:  // bevel
      dwNum = 2;  // will error out if bevel is fixed
      break;
   }

   return dwNum;
}

/*************************************************************************************
CObjectNoodle::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectNoodle::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i, dwNum;

   dwNum = ControlPointNum ();

   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
}


/**************************************************************************************
CObjectNoodle::MoveReferencePointQuery - 
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
BOOL CObjectNoodle::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaSplineMoveP;
   dwDataSize = sizeof(gaSplineMoveP);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   int i;
   i = ps[dwIndex].iX;
   m_Noodle.LocationAndDirection ((fp) (i + 1) / 2.0, pp);
      

   return TRUE;
}

/**************************************************************************************
CObjectNoodle::MoveReferenceStringQuery -
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
BOOL CObjectNoodle::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaSplineMoveP;
   dwDataSize = sizeof(gaSplineMoveP);
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
