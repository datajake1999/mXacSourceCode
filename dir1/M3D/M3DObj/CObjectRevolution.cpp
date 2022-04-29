/************************************************************************
CObjectRevolution.cpp - Draws a Revolution.

begun 28/8/02 by Mike Rozak
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
   L"Base", 0, 0
};

/**********************************************************************************
CObjectRevolution::Constructor and destructor */
CObjectRevolution::CObjectRevolution (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;
   m_dwDisplayControl = 0;

   // Initialize revolution object to default
   CPoint pBottom, pAround, pLeft, pScale;
   pBottom.Zero();
   pAround.Zero();
   pAround.p[2] = 1;
   pLeft.Zero();
   pLeft.p[0] = -1;
   pScale.Zero();
   pScale.p[0] = pScale.p[1] = pScale.p[2] = 1;
   m_Rev.BackfaceCullSet (FALSE);
   m_Rev.DirectionSet (&pBottom, &pAround, &pLeft);
   m_Rev.ProfileSet (RPROF_LINECONE);
   m_Rev.RevolutionSet (RREV_CIRCLE);
   m_Rev.ScaleSet (&pScale);


   // color for the Revolution
   TEXTUREMODS tm;
   memset (&tm, 0, sizeof(tm));
   tm.cTint = RGB(0xff,0xff,0xff);
   tm.wBrightness = 0x1000;
   tm.wContrast = 0x1000;
   tm.wHue = 0x0000;
   tm.wSaturation = 0x1000;
   ObjectSurfaceAdd (42, RGB(0xc0,0x80,0xc0), MATERIAL_PAINTSEMIGLOSS);
}


CObjectRevolution::~CObjectRevolution (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectRevolution::Delete - Called to delete this object
*/
void CObjectRevolution::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectRevolution::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectRevolution::Render (POBJECTRENDER pr, DWORD dwSubObject)
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

   // Draw the revolution
   m_Rev.Render (pr, &m_Renderrs);

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectRevolution::QueryBoundingBox - Standard API
*/
void CObjectRevolution::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   m_Rev.QueryBoundingBox (pCorner1, pCorner2);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectRevolution::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectRevolution::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectRevolution::Clone (void)
{
   PCObjectRevolution pNew;

   pNew = new CObjectRevolution(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // clone internal stuff
   m_Rev.CloneTo (&pNew->m_Rev);
   pNew->m_dwDisplayControl = m_dwDisplayControl;

   return pNew;
}

static PWSTR gpszRev = L"Rev";
static PWSTR gpszDisplayControl = L"DisplayControl";

PCMMLNode2 CObjectRevolution::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // variables
   MMLValueSet (pNode, gpszDisplayControl, (int) m_dwDisplayControl);

   PCMMLNode2 pSub;
   pSub = m_Rev.MMLTo ();
   if (pSub) {
      pSub->NameSet (gpszRev);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectRevolution::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   // get variables
   m_dwDisplayControl = (DWORD) MMLValueGetInt (pNode, gpszDisplayControl, 0);

   PWSTR psz;
   PCMMLNode2 pSub;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszRev), &psz, &pSub);
   if (pSub)
      m_Rev.MMLFrom (pSub);

   return TRUE;
}


/**********************************************************************************
CObjectRevolution::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectRevolution::DialogQuery (void)
{
   return TRUE;
}


/**********************************************************************************
CObjectRevolution::DialogCPQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectRevolution::DialogCPQuery (void)
{
   return TRUE;
}

/* RevolutionDialogPage
*/
BOOL RevolutionDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectRevolution pv = (PCObjectRevolution) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // path
         ComboBoxSet (pPage, L"path", pv->m_Rev.RevolutionGet());
         pControl = pPage->ControlFind (L"custompath");
         if (pControl)
            pControl->Enable (pv->m_Rev.RevolutionGet() == 0);

         // shape
         ComboBoxSet (pPage, L"shape", pv->m_Rev.ProfileGet());
         pControl = pPage->ControlFind (L"customshape");
         if (pControl)
            pControl->Enable (pv->m_Rev.ProfileGet() == 0);

         // NOTE: Not allowing to set backface cull
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

         if (!_wcsicmp(psz, L"path")) {
            if (dwVal == pv->m_Rev.RevolutionGet())
               return TRUE;   // no real change

            pv->m_pWorld->ObjectAboutToChange (pv);
            if (dwVal)
               pv->m_Rev.RevolutionSet (dwVal);
            else {
               PCSpline ps;
               CSpline s;
               ps = pv->m_Rev.RevolutionGetSpline();
               ps->CloneTo (&s);
               pv->m_Rev.RevolutionSet (&s);
            }
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable button
            pControl = pPage->ControlFind (L"custompath");
            if (pControl)
               pControl->Enable (pv->m_Rev.RevolutionGet() == 0);
         }
         else if (!_wcsicmp(psz, L"shape")) {
            if (dwVal == pv->m_Rev.ProfileGet())
               return TRUE;   // no real change

            pv->m_pWorld->ObjectAboutToChange (pv);
            if (dwVal)
               pv->m_Rev.ProfileSet (dwVal);
            else {
               PCSpline ps;
               CSpline s;
               ps = pv->m_Rev.ProfileGetSpline();
               ps->CloneTo (&s);
               pv->m_Rev.ProfileSet (&s);
            }
            pv->m_pWorld->ObjectChanged (pv);

            // enable/disable button
            pControl = pPage->ControlFind (L"customshape");
            if (pControl)
               pControl->Enable (pv->m_Rev.ProfileGet() == 0);
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Lathe settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* RevolutionCurvePathPage
*/
BOOL RevolutionCurvePathPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectRevolution pv = (PCObjectRevolution)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         PCSpline ps;
         ps = pv->m_Rev.RevolutionGetSpline();
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
            ps = pv->m_Rev.RevolutionGetSpline();
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
            CSpline s;
            s.Init (fLooped, dwOrig, (PCPoint) memPoints.p, NULL, (DWORD*)memSegCurve.p, dwVal, dwVal, fDetail);

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_Rev.RevolutionSet (&s);
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
         ps = pv->m_Rev.RevolutionGetSpline();
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

         CSpline s;
         s.Init (fLooped, dwOrig, paPoints, NULL, padw, dwMaxDivide, dwMinDivide, fDetail);

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDisplayControl = 2;   // so display these points
         pv->m_Rev.RevolutionSet (&s);
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         ps = pv->m_Rev.RevolutionGetSpline();
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
            ps = pv->m_Rev.RevolutionGetSpline();
            if (!ps)
               return FALSE;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
               return FALSE;
            CSpline s;
            s.Init (p->pControl->AttribGetBOOL(Checked()), dwOrig, (PCPoint)memPoints.p, NULL, (DWORD*) memSegCurve.p,
               dwMinDivide, dwMaxDivide, fDetail);

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwDisplayControl = 2;   // so display these points
            pv->m_Rev.RevolutionSet (&s);
            pv->m_pWorld->ObjectChanged (pv);

            // redraw the shapes
            ps = pv->m_Rev.RevolutionGetSpline();
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
            p->pszSubString = L"Lathe revolution shape";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* RevolutionCurveProfilePage
*/
BOOL RevolutionCurveProfilePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectRevolution pv = (PCObjectRevolution)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // draw images
         PCSpline ps;
         ps = pv->m_Rev.ProfileGetSpline();
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
            ps = pv->m_Rev.ProfileGetSpline();
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
            CSpline s;
            s.Init (fLooped, dwOrig, (PCPoint) memPoints.p, NULL, (DWORD*)memSegCurve.p, dwVal, dwVal, fDetail);

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_Rev.ProfileSet (&s);
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
         ps = pv->m_Rev.ProfileGetSpline();
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

         CSpline s;
         s.Init (fLooped, dwOrig, paPoints, NULL, padw, dwMaxDivide, dwMinDivide, fDetail);

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDisplayControl = 1;   // so display these points
         pv->m_Rev.ProfileSet (&s);
         pv->m_pWorld->ObjectChanged (pv);

         // redraw the shapes
         ps = pv->m_Rev.ProfileGetSpline();
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
            ps = pv->m_Rev.ProfileGetSpline();
            if (!ps)
               return FALSE;
            if (!ps->ToMem (&fLooped, &dwOrig, &memPoints, NULL, &memSegCurve, &dwMinDivide, &dwMaxDivide, &fDetail))
               return FALSE;
            CSpline s;
            s.Init (p->pControl->AttribGetBOOL(Checked()), dwOrig, (PCPoint)memPoints.p, NULL, (DWORD*) memSegCurve.p,
               dwMinDivide, dwMaxDivide, fDetail);

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwDisplayControl = 1;   // so display these points
            pv->m_Rev.ProfileSet (&s);
            pv->m_pWorld->ObjectChanged (pv);

            // redraw the shapes
            ps = pv->m_Rev.ProfileGetSpline();
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
            p->pszSubString = L"Lathe profile shape";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* RevolutionDisplayPage
*/
BOOL RevolutionDisplayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectRevolution pv = (PCObjectRevolution) pPage->m_pUserData;
   PWSTR apszName[5] = {L"scale", L"shape", L"path"};

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         PWSTR psz;

         psz = NULL;
         if (pv->m_dwDisplayControl < 3)
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
         for (i = 0; i < 3; i++)
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


/**********************************************************************************
CObjectRevolution::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectRevolution::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
firstpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLREVDIALOG, RevolutionDialogPage, this);
firstpage2:
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"custompath")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLREVCURVE, RevolutionCurvePathPage, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"customshape")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLREVCURVE, RevolutionCurveProfilePage, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectRevolution::DialogCPShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectRevolution::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLREVDISPLAY, RevolutionDisplayPage, this);
   if (!pszRet)
      return FALSE;
   return !_wcsicmp(pszRet, Back());
}


/**************************************************************************************
CObjectRevolution::MoveReferencePointQuery - 
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
BOOL CObjectRevolution::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaSplineMoveP;
   dwDataSize = sizeof(gaSplineMoveP);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always use 0,0,0
   pp->Zero();
      

   return TRUE;
}

/**************************************************************************************
CObjectRevolution::MoveReferenceStringQuery -
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
BOOL CObjectRevolution::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
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



/*************************************************************************************
CObjectRevolution::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectRevolution::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (dwID >= ControlPointNum ())
      return FALSE;


   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   //pInfo->dwFreedom = 0;   // any
   pInfo->dwStyle = CPSTYLE_SPHERE;
   pInfo->cColor = RGB(0xff,0,0xff);

   // determine the size
   CPoint pSize;
   m_Rev.ScaleGet (&pSize);
   pInfo->fSize = max(pSize.p[2], max(pSize.p[0], pSize.p[1]));
   pInfo->fSize /= 10;
   pInfo->fSize = max(.001, pInfo->fSize);

   // get the profile and rev splines
   PCSpline psProf, psRev;
   psProf = m_Rev.ProfileGetSpline();
   psRev  = m_Rev.RevolutionGetSpline ();
   if (!psProf || !psRev)
      return FALSE;

   PWSTR psz;
   CPoint p;

   switch (m_dwDisplayControl) {
   case 0:  // scale
      pInfo->cColor = RGB(0,0xff,0);
      pInfo->dwStyle = CPSTYLE_CUBE;


      pInfo->pLocation.p[dwID] = pSize.p[dwID] / ((dwID == 2) ? 1.0 : 2.0);

      switch (dwID) {
      case 0:
      default:
         psz = L"Width";
         break;
      case 1:
         psz = L"Depth";
         break;
      case 2:
         psz = L"Height";
         break;
      }

      wcscpy (pInfo->szName, psz);
      MeasureToString (pSize.p[dwID], pInfo->szMeasurement);
      break;

   case 1:  // profile
      pInfo->cColor = RGB(0xff,0,0);
      pInfo->dwStyle = CPSTYLE_SPHERE;
      if (!psProf->OrigPointGet (dwID, &p))
         return FALSE;
      pInfo->pLocation.p[0] = p.p[0] * pSize.p[0] / 2.0;
      pInfo->pLocation.p[1] = 0;
      pInfo->pLocation.p[2] = p.p[1] * pSize.p[2];

      wcscpy (pInfo->szName, L"Profile");
      break;

   case 2:  // revolution
      pInfo->cColor = RGB(0xff,0xff,0);
      
      if (!psRev->OrigPointGet (dwID, &p))
         return FALSE;
      pInfo->pLocation.p[0] = -p.p[0] * pSize.p[0] / 2.0;
      pInfo->pLocation.p[1] = -p.p[1] * pSize.p[1] / 2.0;
      pInfo->pLocation.p[2] = pSize.p[2] / 2.0; // half way up

      wcscpy (pInfo->szName, L"Revolution");
      break;

   }
   
   return TRUE;
}

/*************************************************************************************
CObjectRevolution::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectRevolution::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID >= ControlPointNum ())
      return FALSE;

   // determine the size
   CPoint pSize;
   DWORD i;
   m_Rev.ScaleGet (&pSize);
   for (i = 0; i < 3; i++)
      pSize.p[i] = max(CLOSE, pSize.p[i]);

   CPoint p;

   // get the profile and rev splines
   PCSpline psProf, psRev;
   psProf = m_Rev.ProfileGetSpline();
   psRev  = m_Rev.RevolutionGetSpline ();
   if (!psProf || !psRev)
      return FALSE;

   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   switch (m_dwDisplayControl) {
   case 0:  // scale
      pSize.p[dwID] = pVal->p[dwID];
      if (dwID != 2)
         pSize.p[dwID] *= 2.0;
      pSize.p[dwID] = max(CLOSE, pSize.p[dwID]);   // so not too small
      m_Rev.ScaleSet (&pSize);
      break;

   case 1:  // profile
      {
         CSpline s;
         psProf->CloneTo (&s);

         p.Zero();
         p.p[0] = pVal->p[0] / (pSize.p[0] / 2.0);
         p.p[1] = pVal->p[2] / pSize.p[2];
         p.p[0] = max(0,p.p[0]);
         p.p[0] = min(1,p.p[0]);
         p.p[1] = max(0,p.p[1]);
         p.p[1] = min(1,p.p[1]);
         s.OrigPointReplace (dwID, &p);

         m_Rev.ProfileSet (&s);
      }
      break;

   case 2:  // revolution
      {
         CSpline s;
         psRev->CloneTo (&s);

         p.Zero();
         p.p[0] = -pVal->p[0] / (pSize.p[0] / 2.0);
         p.p[1] = -pVal->p[1] / (pSize.p[1] / 2.0);
         p.p[0] = max(-1,p.p[0]);
         p.p[0] = min(1,p.p[0]);
         p.p[1] = max(-1,p.p[1]);
         p.p[1] = min(1,p.p[1]);
         s.OrigPointReplace (dwID, &p);

         m_Rev.RevolutionSet (&s);
      }
      break;

   }

   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}


/*************************************************************************************
CObjectRevolution::ControlPointNum - Returns th enumber of contorl points
*/
DWORD CObjectRevolution::ControlPointNum (void)
{
   PCSpline ps;
   DWORD dwNum;
   dwNum = 0;

   switch (m_dwDisplayControl) {
   case 0:  // scale
      dwNum = 3;
      break;

   case 1:  // profile
      if (!m_Rev.ProfileGet() && (ps = m_Rev.ProfileGetSpline()))
         dwNum = ps->OrigNumPointsGet();
      break;

   case 2:  // revolution
      if (!m_Rev.RevolutionGet() && (ps = m_Rev.RevolutionGetSpline()))
         dwNum = ps->OrigNumPointsGet();
      break;

   }

   return dwNum;
}

/*************************************************************************************
CObjectRevolution::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectRevolution::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i, dwNum;

   dwNum = ControlPointNum ();

   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
}

