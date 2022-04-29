/************************************************************************
CObjectPolyMesh.cpp - Draws a PolyMesh.

begun 3/7/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



/**********************************************************************************
CObjectPolyMesh::Constructor and destructor */
CObjectPolyMesh::CObjectPolyMesh (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;
   m_dwType = (DWORD)(size_t) pParams;


   // color for the PolyMesh
   ObjectSurfaceAdd (0, RGB(0xff,0x40,0x40), MATERIAL_TILEGLAZED);
      //&GTEXTURECODE_Bitmap, &GTEXTURESUB_Printer, &tm);


   m_apParam[0].Zero();
   m_apParam[1].Zero();
   m_apParam[0].p[0] = m_apParam[0].p[1] = m_apParam[0].p[2] = .5;
   //m_PolyMesh.CreateBasedOnType (8, 7, m_apParam);   // plane
   //m_PolyMesh.CreateBasedOnType (0, 20, m_apParam); // sphere
   // set up some parameters based on m_dwType
   m_dwDivisions = 8;
   m_dwSubdivideWork = 1;
   m_dwSubdivideFinal = 2;
   switch (m_dwType) {
   case 3:  // cone, open on tpo
   case 4:  // cone, closed on top
      m_apParam[1].p[0] = 0;
      m_apParam[1].p[1] = -1;
      break;
   case 5:  // disc, also cone
      m_dwDivisions = 16;
      m_apParam[1].p[0] = 0;
      m_apParam[1].p[1] = 0;
      m_dwSubdivideWork = m_dwSubdivideFinal = 0;  // since if subdivide doesn't look like box
      break;
   case 6:  // two cones
      m_apParam[1].p[0] = 1;
      m_apParam[1].p[1] = -1;
      m_dwSubdivideWork = m_dwSubdivideFinal = 0;;  // since if subdivide doesn't look like 
      break;
   case 0:  // sphere
   default:
      // no changes to default settings
      break;
   case 1:  // cylinder without cap
   case 2:  // cylinder with cap
      m_apParam[1].Copy (&m_apParam[0]);
      break;
   case 7:  // box
      m_dwDivisions = 3;
      m_dwSubdivideWork = m_dwSubdivideFinal = -1;  // since if subdivide doesn't look like box
      break;
   case 8:  // plane
      m_dwDivisions = 5;
      m_dwSubdivideWork = m_dwSubdivideFinal = 0;  // since if subdivide doesn't look like box
      break;
   case 9:  // taurus
      m_dwDivisions = 16;
      m_apParam[1].p[0] = m_apParam[1].p[1] = .25;
      break;
   case 10: // cylinder with rounded caps
      m_dwDivisions *= 2;
      m_apParam[0].p[3] = .5;
      m_apParam[1].Copy (&m_apParam[0]);
      break;
   }

   m_PolyMesh.CreateBasedOnType (m_dwType, m_dwDivisions, m_apParam);
   m_PolyMesh.SubdivideSet (m_dwSubdivideWork);
}


CObjectPolyMesh::~CObjectPolyMesh (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectPolyMesh::Delete - Called to delete this object
*/
void CObjectPolyMesh::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectPolyMesh::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectPolyMesh::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // set subdivision base don quality
   if (pr->dwReason == ORREASON_FINAL)
      m_PolyMesh.SubdivideSet (m_dwSubdivideFinal);
   else if (pr->dwReason == ORREASON_WORKING)
      m_PolyMesh.SubdivideSet (m_dwSubdivideWork);
   // NOTE: Dont set for the others sine they don't really indicate what use it's for
   // so might as well leave intact

   // draw
   m_PolyMesh.Render(pr, &m_Renderrs, this);

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectPolyMesh::QueryBoundingBox - Standard API
*/
void CObjectPolyMesh::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   m_PolyMesh.QueryBoundingBox (pCorner1, pCorner2, this);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectPolyMesh::QueryBoundingBox too small.");
#endif
}


/**********************************************************************************
CObjectPolyMesh::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectPolyMesh::Clone (void)
{
   PCObjectPolyMesh pNew;

   pNew = new CObjectPolyMesh(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // other info
   m_PolyMesh.CloneTo (&pNew->m_PolyMesh);
   pNew->m_apParam[0].Copy (&m_apParam[0]);
   pNew->m_apParam[1].Copy (&m_apParam[1]);
   pNew->m_dwDivisions = m_dwDivisions;
   pNew->m_dwSubdivideWork = m_dwSubdivideWork;
   pNew->m_dwSubdivideFinal = m_dwSubdivideFinal;

   return pNew;
}


static PWSTR gpszDivisions = L"Divisions";
static PWSTR gpszPolyMesh = L"PolyMesh";
static PWSTR gpszSubdivideWork = L"SubdivideWork";
static PWSTR gpszSubdivideFinal = L"SubdivideFinal";

PCMMLNode2 CObjectPolyMesh::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszDivisions, (int)m_dwDivisions);
   MMLValueSet (pNode, gpszSubdivideWork, (int)m_dwSubdivideWork);
   MMLValueSet (pNode, gpszSubdivideFinal, (int)m_dwSubdivideFinal);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Param%d", (int) i);
      MMLValueSet (pNode, szTemp, &m_apParam[i]);
   }

   PCMMLNode2 pSub;
   pSub = m_PolyMesh.MMLTo ();
   if (pSub) {
      pSub->NameSet (gpszPolyMesh);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectPolyMesh::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   m_dwDivisions = (DWORD) MMLValueGetInt (pNode, gpszDivisions, 16);
   m_dwSubdivideWork = (DWORD) MMLValueGetInt (pNode, gpszSubdivideWork, 1);
   m_dwSubdivideFinal = (DWORD) MMLValueGetInt (pNode, gpszSubdivideFinal, (int)2);


   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Param%d", (int) i);
      MMLValueGetPoint (pNode, szTemp, &m_apParam[i]);
   }

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszPolyMesh), &psz, &pSub);
   if (pSub)
      m_PolyMesh.MMLFrom (pSub);

   return TRUE;
}





/*************************************************************************************
CObjectPolyMesh::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPolyMesh::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   if (!m_dwDivisions)
      return FALSE;  // no contrl points

   CPoint pScaleSize;
   m_PolyMesh.VertToBoundBox (NULL, NULL, &pScaleSize);
   fp fKnobSize = max(max (pScaleSize.p[0], pScaleSize.p[1]), pScaleSize.p[2]) / 100.0;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
   //pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = fKnobSize;
   pInfo->cColor = RGB(0xff,0,0xff);
   wcscpy (pInfo->szName, L"Shape");

   switch (m_dwType) {
      default:
      case 0:  // sphere
      case 7:  // box
         switch (dwID) {
         case 0:
         case 1:
         case 2:
            pInfo->pLocation.p[dwID] = m_apParam[0].p[dwID];
            break;
         case 3:
            pInfo->cColor = RGB(0xff,0xff,0xff);
            pInfo->pLocation.Copy (&m_apParam[0]);
            break;
         default:
            return FALSE;  // error
         }
         break;
      case 3:  // cone, open on tpo
      case 4:  // cone, closed on top
         switch (dwID) {
         case 0:
         case 1:
            pInfo->pLocation.p[dwID] = m_apParam[0].p[dwID];
            break;
         case 2:
            pInfo->pLocation.p[dwID] = m_apParam[1].p[1];
            break;
         case 3:
            pInfo->cColor = RGB(0xff,0xff,0xff);
            pInfo->pLocation.Copy (&m_apParam[0]);
            pInfo->pLocation.p[2] = m_apParam[1].p[1];
            break;
         default:
            return FALSE;  // error
         }
         break;
      case 5:  // disc, also cone
      case 8:  // plane
         switch (dwID) {
         case 0:
         case 1:
            pInfo->pLocation.p[dwID] = m_apParam[0].p[dwID];
            break;
         case 2:
            pInfo->cColor = RGB(0xff,0xff,0xff);
            pInfo->pLocation.Copy (&m_apParam[0]);
            pInfo->pLocation.p[2] = 0;
            break;
         default:
            return FALSE;  // error
         }
         break;
      case 6:  // two cones
         switch (dwID) {
         case 0:
         case 1:
            pInfo->pLocation.p[dwID] = m_apParam[0].p[dwID];
            break;
         case 2:
            pInfo->pLocation.p[2] = m_apParam[1].p[0];
            break;
         case 3:
            pInfo->pLocation.p[2] = m_apParam[1].p[1];
            break;
         case 4:
            pInfo->cColor = RGB(0xff,0xff,0xff);
            pInfo->pLocation.Copy (&m_apParam[0]);
            pInfo->pLocation.p[2] = m_apParam[1].p[0];
            break;
         default:
            return FALSE;  // error
         }
         break;
      case 1:  // cylinder without cap
      case 2:  // cylinder with cap
         switch (dwID) {
         case 0:
         case 1:
         case 2:
         case 3:
            pInfo->pLocation.p[dwID%2] = m_apParam[dwID/2].p[dwID%2];
            pInfo->pLocation.p[2] = ((dwID/2) ? -1 : 1) * m_apParam[0].p[2];
            break;
         case 4:
            pInfo->pLocation.p[2] = m_apParam[0].p[2];
            break;
         case 5:
            pInfo->cColor = RGB(0xff,0xff,0xff);
            pInfo->pLocation.Copy (&m_apParam[0]);
            break;
         default:
            return FALSE;  // error
         }
         break;
      case 10:  // cylinder rounded
         switch (dwID) {
         case 0:
         case 1:
         case 2:
         case 3:
         case 4:
         case 5:
            pInfo->pLocation.p[dwID%3] = m_apParam[dwID/3].p[dwID%3];
            if (dwID == 5)
               pInfo->pLocation.p[dwID%3] *= -1;
            pInfo->pLocation.p[2] += ((dwID/3) ? -1 : 1) * m_apParam[0].p[3];
            break;
         case 6:
            pInfo->cColor = RGB(0xff,0xff,0xff);
            pInfo->pLocation.Copy (&m_apParam[0]);
            pInfo->pLocation.p[2] += m_apParam[0].p[3];
            break;
         default:
            return FALSE;  // error
         }
         break;
      case 9:  // taurus
         switch (dwID) {
         case 0:
         case 1:
            pInfo->pLocation.p[dwID] = m_apParam[0].p[dwID] + m_apParam[1].p[0];  // extend by x
            break;
         case 2:
            pInfo->cColor = RGB(0xff,0xff,0);
            pInfo->pLocation.p[0] = -m_apParam[0].p[0] - m_apParam[1].p[0];
            break;
         case 3:
            pInfo->cColor = RGB(0xff,0xff,0);
            pInfo->pLocation.p[0]= -m_apParam[0].p[0];
            pInfo->pLocation.p[2] = m_apParam[1].p[1];
            break;
         case 4:
            pInfo->cColor = RGB(0xff,0xff,0xff);
            pInfo->pLocation.Copy (&m_apParam[0]);
            pInfo->pLocation.p[0] += m_apParam[1].p[0];
            pInfo->pLocation.p[1] += m_apParam[1].p[0];
            pInfo->pLocation.p[2] = 0;
            break;
         default:
            return FALSE;  // error
         }
         break;
   }
   return TRUE;
}

/*************************************************************************************
CObjectPolyMesh::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectPolyMesh::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (!m_dwDivisions)
      return FALSE;  // no contrl points

   CPoint ap[2];
   fp fLen;
   ap[0].Copy (&m_apParam[0]);
   ap[1].Copy (&m_apParam[1]);

   CPoint pMin;
   CPoint pt;
   pt.Copy (pVal);
   pMin.p[0] = pMin.p[1] = pMin.p[2] = CLOSE;

   switch (m_dwType) {
      default:
      case 0:  // sphere
      case 7:  // box
         switch (dwID) {
         case 0:
         case 1:
         case 2:
            ap[0].p[dwID] = pVal->p[dwID];
            break;
         case 3:
            fLen = pVal->Length();
            fLen /= ap[0].Length();
            ap[0].Scale (fLen);
            break;
         default:
            return FALSE;  // error
         }
         ap[0].Max (&pMin);
         ap[1].Max (&pMin);
         break;
      case 3:  // cone, open on tpo
      case 4:  // cone, closed on top
         switch (dwID) {
         case 0:
         case 1:
            ap[0].p[dwID] = pVal->p[dwID];
            break;
         case 2:
            ap[1].p[1] = pVal->p[dwID];
            break;
         case 3:
            fLen = pVal->Length();
            fLen /= sqrt(ap[0].p[0] * ap[0].p[0] + ap[0].p[1] * ap[0].p[1] + ap[1].p[1] * ap[1].p[1]);
            ap[0].Scale (fLen);
            ap[1].Scale (fLen);
            break;
         default:
            return FALSE;  // error
         }
         ap[0].Max (&pMin);
         break;
      case 5:  // disc, also cone
      case 8:  // plane
         switch (dwID) {
         case 0:
         case 1:
            ap[0].p[dwID] = pVal->p[dwID];
            break;
         case 2:
            fLen = pVal->Length();
            ap[0].p[2] = 0;   // since always 0
            fLen /= ap[0].Length();
            ap[0].Scale (fLen);
            break;
         default:
            return FALSE;  // error
         }
         ap[0].Max (&pMin);
         break;
      case 6:  // two cones
         switch (dwID) {
         case 0:
         case 1:
            ap[0].p[dwID] = pVal->p[dwID];
            break;
         case 2:
            ap[1].p[0] = pVal->p[2];
            break;
         case 3:
            ap[1].p[1] = pVal->p[2];
            break;
         case 4:
            fLen = pVal->Length();
            fLen /= sqrt(ap[0].p[0] * ap[0].p[0] + ap[0].p[1] * ap[0].p[1] + ap[1].p[0] * ap[1].p[0]);
            ap[0].Scale (fLen);
            ap[1].Scale (fLen);
            break;
         default:
            return FALSE;  // error
         }
         ap[0].Max (&pMin);
         break;
      case 1:  // cylinder without cap
      case 2:  // cylinder with cap
         switch (dwID) {
         case 0:
         case 1:
         case 2:
         case 3:
            ap[dwID/2].p[dwID%2] = pVal->p[dwID%2];
            break;
         case 4:
            ap[0].p[2] = pVal->p[2];
            break;
         case 5:
            fLen = pVal->Length();
            fLen /= ap[0].Length();
            ap[0].Scale (fLen);
            ap[1].Scale (fLen);
            break;
         default:
            return FALSE;  // error
         }
         ap[0].Max (&pMin);
         ap[1].Max (&pMin);
         break;
      case 10:  // cylinder, rounded cap
         switch (dwID) {
         case 0:
         case 1:
         case 2:
         case 3:
         case 4:
         case 5:
            if ((dwID%3) == 2)
               pt.p[2] -= ((dwID/3) ? -1 : 1) * ap[0].p[3];
            if (dwID == 5)
               pt.p[2] *= -1;
            ap[dwID/3].p[dwID%3] = pt.p[dwID%3];

            if ((dwID % 3) < 2) {
               ap[0].p[3] = pVal->p[2];
               if (dwID / 3)
                  ap[0].p[3] *= -1;
               ap[0].p[3] = max(ap[0].p[3], CLOSE);
            }
            break;
         case 6:
            fLen = pt.Length();
            pt.Copy (&ap[0]);
            pt.p[2] += ap[0].p[3];
            fLen /= pt.Length();
            ap[0].Scale (fLen);
            ap[0].p[3] *= fLen;
            ap[1].Scale (fLen);
            break;
         default:
            return FALSE;  // error
         }
         ap[0].Max (&pMin);
         ap[1].Max (&pMin);
         break;
      case 9:  // taurus
         switch (dwID) {
         case 0:
         case 1:
            ap[0].p[dwID] = pVal->p[dwID] - ap[1].p[dwID];
            break;
         case 2:
            ap[1].p[0] = -(pVal->p[0] + ap[0].p[0]);
            break;
         case 3:
            ap[1].p[1] = pVal->p[2];
            break;
         case 4:
            pt.p[0] -= ap[1].p[0];
            pt.p[1] -= ap[1].p[0];
            pt.p[2] = 0;
            fLen = pt.Length();
            ap[0].p[2] = 0;   // should be zero anyway
            fLen /= ap[0].Length();
            ap[0].Scale (fLen);
            ap[1].Scale (fLen);
            break;
         default:
            return FALSE;  // error
         }
         ap[0].Max (&pMin);
         ap[1].Max (&pMin);
         break;
   }

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);
   m_apParam[0].Copy (&ap[0]);
   m_apParam[1].Copy (&ap[1]);
   m_PolyMesh.CreateBasedOnType (m_dwType, m_dwDivisions, m_apParam);
   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
   return TRUE;
}

/*************************************************************************************
CObjectPolyMesh::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectPolyMesh::ControlPointEnum (PCListFixed plDWORD)
{
   if (!m_dwDivisions)
      return;  // no contrl points

   DWORD i;
   DWORD dwNum;

   switch (m_dwType){
   case 0:  // sphere
   case 7:  // box
   case 3:  // cone, open on tpo
   case 4:  // cone, closed on top
   default:
      dwNum = 4;  // 0 = x, 1=y, 2=z, 3=combo
      break;
   case 5:  // disc, also cone
   case 8:  // plane
      dwNum = 3;  // 0 = x, 1=y, 2=combo
      break;
   case 6:  // two cones
      dwNum = 5;  // 0 = x, 1=y, 2=top, 3=bottom, 4=combo
      break;
   case 1:  // cylinder without cap
   case 2:  // cylinder with cap
      dwNum = 6;  // 0 = topx, 1=topy, 2=topz, 3=bottomx, 4=bottomy, 5=combo
      break;
   case 9:  // taurus
      dwNum = 5;  // 0 = rad x, 1=rady, 2=extrude x, 3=extrude y, 4=combo
      break;
   case 10: // cylinder, rounded edges
      dwNum = 7;  // 012 = xyz top, 345=xyz bottom, 6=combo
      break;
   }

   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
}


/* PolyMeshDialog1Page - phase I
*/
static BOOL PolyMeshDialog1Page (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPolyMesh pv = (PCObjectPolyMesh) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"points");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_dwDivisions);

         ComboBoxSet (pPage, L"SubWork", pv->m_dwSubdivideWork+1); 
         ComboBoxSet (pPage, L"SubFinal", pv->m_dwSubdivideFinal+1); 
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!psz || _wcsicmp(psz, L"points"))
            break;

         // get value
         DWORD dw;
         dw = (DWORD) p->pControl->AttribGetInt (Pos());
         dw = max(1,dw);   // always at least 1
         if (dw == pv->m_dwDivisions)
            break;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         pv->m_dwDivisions = dw;
         pv->m_PolyMesh.CreateBasedOnType(pv->m_dwType, pv->m_dwDivisions, pv->m_apParam);
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // if the major ID changed then pick the first minor and texture that
         // comes to mind
         if (!_wcsicmp(p->pControl->m_pszName, L"SubWork")) {
            DWORD dwVal = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;

            // if it hasn't reall change then ignore
            if (dwVal == pv->m_dwSubdivideWork+1)
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwSubdivideWork = dwVal - 1;
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"SubFinal")) {
            DWORD dwVal = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;

            // if it hasn't reall change then ignore
            if (dwVal == pv->m_dwSubdivideFinal+1)
               return TRUE;

            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwSubdivideFinal = dwVal - 1;
            pv->m_pWorld->ObjectChanged (pv);

            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (p->psz && !_wcsicmp(p->psz, L"convert")) {
            pv->m_pWorld->ObjectAboutToChange (pv);
            pv->m_dwDivisions = 0;  // so convert over
            pv->m_pWorld->ObjectChanged (pv);
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Polygon mesh (phase I) settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/* PolyMeshDialog1Page - phase II
*/
static BOOL PolyMeshDialog2Page (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectPolyMesh pv = (PCObjectPolyMesh) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Polygon mesh (phase II) settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/**********************************************************************************
CObjectPolyMesh::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectPolyMesh::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
redo:
   if (m_dwDivisions)
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOLYMESHDIALOG1, PolyMeshDialog1Page, this);
   else {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOLYMESHDIALOG2, PolyMeshDialog2Page, this);
   }
   if (!pszRet)
      return FALSE;
   else if (!_wcsicmp(pszRet, L"convert"))
      goto redo;

   return !_wcsicmp(pszRet, Back());
   return FALSE;
}


/**********************************************************************************
CObjectPolyMesh::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectPolyMesh::DialogQuery (void)
{
   return TRUE;
}


/*****************************************************************************************
CObjectPolyMesh::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectPolyMesh::EditorCreate (BOOL fAct)
{
   // can only edit when in phase II
   if (m_dwDivisions)
      return FALSE;

   if (!fAct)
      return TRUE;

   return ObjectViewNew (m_pWorld, &m_gGUID, VIEWWHAT_POLYMESH);
}


/*****************************************************************************************
CObjectPolyMesh::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectPolyMesh::EditorDestroy (void)
{
   // can only edit when in phase II
   if (m_dwDivisions)
      return TRUE;

   return ObjectViewDestroy (m_pWorld, &m_gGUID);
}

/*****************************************************************************************
CObjectPolyMesh::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectPolyMesh::EditorShowWindow (BOOL fShow)
{
   // can only edit when in phase II
   if (m_dwDivisions)
      return FALSE;

   return ObjectViewShowHide (m_pWorld, &m_gGUID, fShow);
}


/**********************************************************************************
CObjectPolyMesh::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectPolyMesh::Message (DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case OSM_POLYMESH:
      {
         POSMPOLYMESH p = (POSMPOLYMESH) pParam;
         p->ppm = this;
      }
      return TRUE;

   }

   return FALSE;
}


/*****************************************************************************************
CObjectPolyMesh::Merge -
asks the object to merge with the list of objects (identified by GUID) in pagWith.
dwNum is the number of objects in the list. The object should see if it can
merge with any of the ones in the list (some of which may no longer exist and
one of which may be itself). If it does merge with any then it return TRUE.
if no merges take place it returns false.
*/
BOOL CObjectPolyMesh::Merge (GUID *pagWith, DWORD dwNum)
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
      OSMPOLYMESH os;
      memset (&os, 0, sizeof(os));
      if (!pos->Message (OSM_POLYMESH, &os))
         continue;
      if (!os.ppm)
         continue;

      // dont merge with self
      if (os.ppm == this)
         continue;

      // inform that about to change
      m_pWorld->ObjectAboutToChange (this);

      // figure out transformation
      CMatrix mTrans, mInv;
      pos->ObjectMatrixGet (&mTrans);
      m_MatrixObject.Invert4 (&mInv);
      mTrans.MultiplyRight (&mInv);

      // merge it
      fRet = TRUE;
      if (m_PolyMesh.Merge (&os.ppm->m_PolyMesh, &mTrans, this, os.ppm)) {
         // delete the other object
         m_pWorld->ObjectRemove (dwFind);
      }

      // inform that changed
      m_pWorld->ObjectChanged (this);
   }

   return fRet;
}


/*****************************************************************************************
CObjectPolyMesh::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
BOOL CObjectPolyMesh::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   return m_PolyMesh.AttribGetIntern (pszName, pfValue);
}


/*****************************************************************************************
CObjectPolyMesh::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectPolyMesh::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   m_PolyMesh.AttribGetAllIntern (plATTRIBVAL);
}


/*****************************************************************************************
CObjectPolyMesh::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectPolyMesh::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   m_PolyMesh.AttribSetGroupIntern(dwNum, paAttrib, m_pWorld, this);
}


/*****************************************************************************************
CObjectPolyMesh::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectPolyMesh::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   return m_PolyMesh.AttribInfoIntern (pszName, pInfo);
}

/*****************************************************************************************
CObjectPolyMesh::Deconstruct - Standard call
*/
BOOL CObjectPolyMesh::Deconstruct (BOOL fAct)
{
   return FALSE;
}


// BUGBUG - with symmetry on, 3x3 cube, subdivide center front of cube by slicing
// horizontally (or was it diagonally) ended up subdividing improperly and creating
// too many copies. I think I had x, y, and z summetry all on, which could explain the
// problem.