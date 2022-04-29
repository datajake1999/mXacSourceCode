/************************************************************************
CObjectColumn.cpp - Draws a Column.

begun 14/3/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

/**********************************************************************************
CObjectColumn::Constructor and destructor */
CObjectColumn::CObjectColumn (PVOID pParams, POSINFO pInfo)
{
   m_OSINFO = *pInfo;

   m_dwRenderShow = RENDERSHOW_FRAMING;

   // color for the Column
   ObjectSurfaceAdd (1, RGB(0xff,0xff,0xff), MATERIAL_PAINTSEMIGLOSS, L"Column");
   ObjectSurfaceAdd (2, RGB(0xc0, 0xc0, 0xc0), MATERIAL_PAINTSEMIGLOSS, L"Column base");
   ObjectSurfaceAdd (3, RGB(0xc0, 0xc0, 0xc0), MATERIAL_PAINTSEMIGLOSS, L"Column top");
   ObjectSurfaceAdd (4, RGB(0xc0, 0xc0, 0xc0), MATERIAL_PAINTSEMIGLOSS, L"Column brace");

   // set the infor for the column
   CPoint pStart, pEnd, pFront;
   pStart.Zero();
   pEnd.Zero();
   pEnd.p[2] = 2;
   pFront.Zero();
   pFront.p[1] = -1;
   m_Column.StartEndFrontSet (&pStart, &pEnd, &pFront);

   // scale
   CPoint pScale;
   pScale.Zero();
   pScale.p[0] = .1;
   pScale.p[1] = .1;
   m_Column.SizeSet (&pScale);

   // shape
   m_Column.ShapeSet (NS_RECTANGLE);


   // test the base
   CBASEINFO bi;
   memset (&bi, 0, sizeof(bi));
   bi.dwShape = NS_RECTANGLE;
   bi.fUse = TRUE;
   bi.pSize.p[0] = .3;
   bi.pSize.p[1] = .3;
   bi.pSize.p[2] = .3;
   bi.dwTaper = CT_HALFTAPER;
   //bi.dwBevelMode = 2;
   //bi.pBevelNorm.p[0] = -.2;
   //bi.pBevelNorm.p[2] = -1;
   m_Column.BaseInfoSet (TRUE, &bi);
#if 0
   bi.dwShape = NS_RECTANGLE;
   bi.fUse = TRUE;
   bi.pSize.p[0] = bi.pSize.p[1] = .5;
   bi.pSize.p[2] = .1;
   m_Column.BaseInfoSet (FALSE, &bi);

   // Test different braces
   m_Column.BraceStartSet (.5);
   DWORD i;
   for (i = 0; i < 2; i++) {
      CBRACEINFO bi;
      memset (&bi, 0, sizeof(bi));
      bi.dwBevelMode = 2;
      bi.pBevelNorm.p[2] = -1;
      bi.fUse = TRUE;
      bi.dwShape = NS_IBEAMFB;
      bi.pSize.p[0] = bi.pSize.p[1] = .075;

      bi.pEnd.p[0] = sin((fp)i / 4.0 * 2.0 * PI);
      bi.pEnd.p[1] = cos((fp)i / 4.0 * 2.0 * PI);
      bi.pEnd.p[2] = 2;

      m_Column.BraceInfoSet (i, &bi);
   }
#endif // 0
}


CObjectColumn::~CObjectColumn (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectColumn::Delete - Called to delete this object
*/
void CObjectColumn::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectColumn::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectColumn::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   //CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // object specific
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (1), m_pWorld);
   m_Column.Render (pr, &m_Renderrs, 0x01);
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (2), m_pWorld);
   m_Column.Render (pr, &m_Renderrs, 0x02);
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (3), m_pWorld);
   m_Column.Render (pr, &m_Renderrs, 0x04);
   m_Renderrs.SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (4), m_pWorld);
   m_Column.Render (pr, &m_Renderrs, 0x08);

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectColumn::QueryBoundingBox - Standard API
*/
void CObjectColumn::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   m_Column.QueryBoundingBox (pCorner1, pCorner2);

#ifdef _DEBUG
   // test, make sure bounding box not too small
   CPoint p1,p2;
   DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectColumn::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectColumn::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectColumn::Clone (void)
{
   PCObjectColumn pNew;

   pNew = new CObjectColumn(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   m_Column.CloneTo (&pNew->m_Column);
   return pNew;
}

static PWSTR gpszColumn = L"Column";

PCMMLNode2 CObjectColumn::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   PCMMLNode2 pSub;
   pSub = m_Column.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszColumn);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectColumn::MMLFrom (PCMMLNode2 pNode)
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
      if (!_wcsicmp(psz, gpszColumn))
         m_Column.MMLFrom (pSub);
   }

   return TRUE;
}

/*************************************************************************************
CObjectColumn::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectColumn::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   switch (dwID) {
   case 1:
   case 2:
   case 10:
      break;   // OK
   default:
      if ((dwID >= 11) && (dwID < 11 + COLUMNBRACES))
         break;
      return FALSE;
   }

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
//   pInfo->dwFreedom = 0;   // any
   pInfo->dwStyle = CPSTYLE_CUBE;
   CPoint pSize;
   m_Column.SizeGet(&pSize);
   pInfo->fSize = max(pSize.p[0], pSize.p[1])*1.5;
   pInfo->cColor = RGB(0xff,0,0xff);
   CPoint pStart, pEnd, pDir;
   m_Column.StartEndFrontGet (&pStart, &pEnd);
   pDir.Subtract (&pEnd, &pStart);

   switch (dwID) {
   case 1:  // bottom point
      pInfo->pLocation.Copy (&pStart);
      wcscpy (pInfo->szName, L"Base");
      MeasureToString (pDir.Length(), pInfo->szMeasurement);
      break;
   case 2:  // top point
      pInfo->pLocation.Copy (&pEnd);
      wcscpy (pInfo->szName, L"Top");
      MeasureToString (pDir.Length(), pInfo->szMeasurement);
      break;
   case 10: // where braces intersect
      pDir.Normalize();
      pDir.Scale (-m_Column.BraceStartGet());
      pDir.Add (&pEnd);
      pInfo->pLocation.Copy (&pDir);
      wcscpy (pInfo->szName, L"Brace anchor");
      MeasureToString (m_Column.BraceStartGet(), pInfo->szMeasurement);
      break;
   default: // brace top
      {
         CBRACEINFO bi;
         if (!m_Column.BraceInfoGet (dwID - 11, &bi))
            return FALSE;

         pInfo->pLocation.Copy (&bi.pEnd);
         wcscpy (pInfo->szName, L"Brace");
      }
      break;
   }

   
   return TRUE;
}

/*************************************************************************************
CObjectColumn::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectColumn::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   CPoint pStart, pEnd, pDir;
   m_Column.StartEndFrontGet (&pStart, &pEnd);
   pDir.Subtract (&pEnd, &pStart);

   switch (dwID) {
   case 1:  // bottom point
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);
      pStart.p[2] = pVal->p[2];
      m_Column.StartEndFrontSet (&pStart, NULL);
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
      return TRUE;

   case 2:  // top point
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);
      pEnd.p[2] = pVal->p[2];
      m_Column.StartEndFrontSet (NULL, &pEnd);
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
      return TRUE;

   case 10: // where braces intersect
      fp fVal;
      fVal = pEnd.p[2] - pVal->p[2];
      fVal = max(0.01,fVal);  // not less than 0
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);
      m_Column.BraceStartSet (fVal);
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);
      return TRUE;

   default: // brace top
      if ((dwID < 11) || (dwID >= 11 + COLUMNBRACES))
         return FALSE;  // out of range

      {
         CBRACEINFO bi;
         if (!m_Column.BraceInfoGet (dwID - 11, &bi))
            return FALSE;
         bi.pEnd.Copy (pVal);

         if (m_pWorld)
            m_pWorld->ObjectAboutToChange (this);
         m_Column.BraceInfoSet (dwID - 11, &bi);
         if (m_pWorld)
            m_pWorld->ObjectChanged (this);
      }
      return TRUE;
   }
   return TRUE;
}

/*************************************************************************************
CObjectColumn::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectColumn::ControlPointEnum (PCListFixed plDWORD)
{
   // 2 control points at top and bottom
   DWORD i;
   for (i = 1; i < 3; i++)
      plDWORD->Add (&i);

   // see if has any braces
   CBRACEINFO bi;
   DWORD dwBraces;
   dwBraces = 0;
   for (i = 0; i < COLUMNBRACES; i++) {
      if (!m_Column.BraceInfoGet (i, &bi))
         break;
      if (!bi.fUse)
         continue;   // BUGFIX - Was break, but wouldn't draw brace control points then
      dwBraces++;

      // if this is the first time then add up/down
      DWORD dwAdd;
      if (dwBraces == 1) {
         dwAdd = 10;
         plDWORD->Add (&dwAdd);
      }

      // add this
      dwAdd = 11 + i;
      plDWORD->Add (&dwAdd);
   }
}


/* ColumnPage
*/
BOOL ColumnPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectColumn pv = (PCObjectColumn)pPage->m_pUserData;
   PCColumn pc = &pv->m_Column;
   static BOOL fIgnoreMessage = TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         fIgnoreMessage = TRUE;

         WCHAR szTemp[64];
         ESCMCOMBOBOXSELECTSTRING css;
         PCEscControl pControl;
         CPoint pt;
         memset (&css, 0, sizeof(css));
         css.fExact = TRUE;
         css.iStart = -1;
         css.psz = szTemp;

         // main column
         swprintf (szTemp, L"%d", (int) pc->ShapeGet());
         pControl = pPage->ControlFind (L"mainshape");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);
         pc->SizeGet (&pt);
         MeasureToString (pPage, L"mainwidth", pt.p[0]);
         MeasureToString (pPage, L"maindepth", pt.p[1]);

         // bottom
         CBASEINFO bi;
         pc->BaseInfoGet (TRUE, &bi);
         if (bi.dwShape == NS_CUSTOM)
            bi.dwShape = NS_RECTANGLE;
         if (bi.fUse) {
            pControl = pPage->ControlFind (L"bottomused");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);
         }
         swprintf (szTemp, L"%d", (int) bi.dwShape);
         pControl = pPage->ControlFind (L"bottomshape");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);
         swprintf (szTemp, L"%d", (int) bi.dwTaper);
         pControl = pPage->ControlFind (L"bottomtaper");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);
         MeasureToString (pPage, L"bottomwidth", bi.pSize.p[0]);
         MeasureToString (pPage, L"bottomdepth", bi.pSize.p[1]);
         MeasureToString (pPage, L"bottomheight", bi.pSize.p[2]);

         // top
         pc->BaseInfoGet (FALSE, &bi);
         if (bi.dwShape == NS_CUSTOM)
            bi.dwShape = NS_RECTANGLE;
         if (bi.fUse) {
            pControl = pPage->ControlFind (L"topused");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);
         }
         swprintf (szTemp, L"%d", (int) bi.dwShape);
         pControl = pPage->ControlFind (L"topshape");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);
         swprintf (szTemp, L"%d", (int) bi.dwTaper);
         pControl = pPage->ControlFind (L"toptaper");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);
         MeasureToString (pPage, L"topwidth", bi.pSize.p[0]);
         MeasureToString (pPage, L"topdepth", bi.pSize.p[1]);
         MeasureToString (pPage, L"topheight", bi.pSize.p[2]);

         // braces
         // find the first brace used
         DWORD i;
         CBRACEINFO bri;
         for (i = 0; i < COLUMNBRACES; i++) {
            if (!pc->BraceInfoGet (i, &bri))
               continue;
            if (!bri.fUse)
               continue;
            break;
         }
         if (i >= COLUMNBRACES) {
            memset (&bri, 0, sizeof(bri));
            bri.dwShape = NS_RECTANGLE;
            bri.pSize.p[0] = .075;
            bri.pSize.p[1] = .075;
         }
         swprintf (szTemp, L"%d", (int) bri.dwShape);
         pControl = pPage->ControlFind (L"braceshape");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);
         MeasureToString (pPage, L"bracewidth", bri.pSize.p[0]);
         MeasureToString (pPage, L"bracedepth", bri.pSize.p[1]);
         for (i = 0; i < COLUMNBRACES; i++) {
            if (!pc->BraceInfoGet (i, &bri))
               continue;
            if (!bri.fUse)
               continue;

            // check this
            swprintf (szTemp, L"brace%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);
         }

         fIgnoreMessage = FALSE;

      }
      break;

   case ESCM_USER+86:   // load in and set values
      {
         if (pv->m_pWorld)
            pv->m_pWorld->ObjectAboutToChange (pv);

         WCHAR szTemp[64];
         DWORD i;
         PCEscControl pControl;
         ESCMCOMBOBOXGETITEM gi;
         CPoint pt;
         memset (&gi, 0, sizeof(gi));

         // main column
         pControl = pPage->ControlFind (L"mainshape");
         if (pControl) {
            gi.dwIndex = (DWORD) pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            i = NS_RECTANGLE;
            if (gi.pszName)
               i = _wtoi (gi.pszName);
            pc->ShapeSet (i);
         }
         pt.Zero();
         MeasureParseString (pPage, L"mainwidth", &pt.p[0]);
         MeasureParseString (pPage, L"maindepth", &pt.p[1]);
         for (i = 0; i < 3; i++)
            if (pt.p[i] <= 0.0)
               pt.p[i] = .075;
         pc->SizeSet (&pt);

         // bottom
         CBASEINFO bi;
         pc->BaseInfoGet (TRUE, &bi);
         pControl = pPage->ControlFind (L"bottomused");
         if (pControl)
            bi.fUse = pControl->AttribGetBOOL (Checked());

         pControl = pPage->ControlFind (L"bottomshape");
         if (pControl) {
            gi.dwIndex = (DWORD) pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            i = NS_RECTANGLE;
            if (gi.pszName)
               i = _wtoi (gi.pszName);
            bi.dwShape = i;
         }
         pControl = pPage->ControlFind (L"bottomtaper");
         if (pControl) {
            gi.dwIndex = (DWORD) pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            i = 0;
            if (gi.pszName)
               i = _wtoi (gi.pszName);
            bi.dwTaper = i;
         }
         MeasureParseString (pPage, L"bottomwidth", &pt.p[0]);
         MeasureParseString (pPage, L"bottomdepth", &pt.p[1]);
         MeasureParseString (pPage, L"bottomheight", &pt.p[2]);
         for (i = 0; i < 3; i++)
            if (pt.p[i] <= 0.0)
               pt.p[i] = .075;
         bi.pSize.Copy (&pt);
         bi.dwBevelMode = 0;  // default to this
         bi.fCapped = TRUE;   // default to this
         pc->BaseInfoSet (TRUE, &bi);

         // top
         pc->BaseInfoGet (FALSE, &bi);
         pControl = pPage->ControlFind (L"topused");
         if (pControl)
            bi.fUse = pControl->AttribGetBOOL (Checked());

         pControl = pPage->ControlFind (L"topshape");
         if (pControl) {
            gi.dwIndex = (DWORD) pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            i = NS_RECTANGLE;
            if (gi.pszName)
               i = _wtoi (gi.pszName);
            bi.dwShape = i;
         }
         pControl = pPage->ControlFind (L"toptaper");
         if (pControl) {
            gi.dwIndex = (DWORD) pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            i = 0;
            if (gi.pszName)
               i = _wtoi (gi.pszName);
            bi.dwTaper = i;
         }
         MeasureParseString (pPage, L"topwidth", &pt.p[0]);
         MeasureParseString (pPage, L"topdepth", &pt.p[1]);
         MeasureParseString (pPage, L"topheight", &pt.p[2]);
         for (i = 0; i < 3; i++)
            if (pt.p[i] <= 0.0)
               pt.p[i] = .075;
         bi.pSize.Copy (&pt);
         bi.dwBevelMode = 0;  // default to this
         bi.fCapped = TRUE;   // default to this
         pc->BaseInfoSet (FALSE, &bi);

         // braces
         // find the first brace used
         CBRACEINFO bri;
         for (i = 0; i < COLUMNBRACES; i++) {
            if (!pc->BraceInfoGet (i, &bri))
               continue;

            // see if it's checked
            BOOL fCheck;
            swprintf (szTemp, L"brace%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            fCheck = FALSE;
            if (pControl)
               fCheck = pControl->AttribGetBOOL (Checked());

            if (fCheck && !bri.fUse) {
               // not checked. Set a default
               bri.dwBevelMode = 0;
               CPoint pEnd;
               pc->StartEndFrontGet (NULL, &pEnd);
               pEnd.p[0] += sin((fp)i / 4.0 * 2.0 * PI);
               pEnd.p[1] += cos((fp)i / 4.0 * 2.0 * PI);
               bri.pEnd.Copy (&pEnd);
            }
            bri.fUse = fCheck;

            pControl = pPage->ControlFind (L"braceshape");
            if (pControl) {
               DWORD j;
               gi.dwIndex = (DWORD) pControl->AttribGetInt (CurSel());
               pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
               j = NS_RECTANGLE;
               if (gi.pszName)
                  j = _wtoi (gi.pszName);
               bri.dwShape = j;
            }

            // size
            MeasureParseString (pPage, L"bracewidth", &pt.p[0]);
            MeasureParseString (pPage, L"bracedepth", &pt.p[1]);
            DWORD j;
            for (j = 0; j < 2; j++)
               if (pt.p[j] <= 0.0)
                  pt.p[j] = .075;
            bri.pSize.Copy (&pt);

            pc->BraceInfoSet (i, &bri);
         }

         if (pv->m_pWorld)
            pv->m_pWorld->ObjectChanged (pv);
      }
      return TRUE;

   case ESCN_COMBOBOXSELCHANGE:
   case ESCN_BUTTONPRESS:
   case ESCN_EDITCHANGE:
      if (!fIgnoreMessage)
         pPage->Message (ESCM_USER+86);  // load in values
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Column settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectColumn::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectColumn::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLCOLUMN, ColumnPage, this);

   return pszRet && !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectTemplate::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectColumn::DialogQuery (void)
{
   return TRUE;
}

