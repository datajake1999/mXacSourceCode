/************************************************************************
CObjectBooks.cpp - Draws a Books.

begun 14/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

typedef struct {
   fp       fHorzLoc;      // horizontal location on shelf, starting at 0
   fp       fIndent;       // amount that indented from the front of the books
   CPoint   pSize;         // size of book
   COLORREF cColor;        // color of the book
   fp       fBindOut;      // how much the binding is out, in meters
   fp       fPageIndent;   // how much the pages are indented, in meters
} BOOKINFO, *PBOOKINFO;

/**********************************************************************************
CObjectBooks::Constructor and destructor */
CObjectBooks::CObjectBooks (PVOID pParams, POSINFO pInfo)
{
   m_dwRenderShow = RENDERSHOW_MISC;
   m_OSINFO = *pInfo;

   m_lBOOKINFO.Init (sizeof(BOOKINFO));
   m_dwFillInSeed = 0;

   m_pBookSize.Zero();
   m_pBookSize.p[0] = .025;
   m_pBookSize.p[1] = .12;
   m_pBookSize.p[2] = .2;
   m_pBookVar.Zero();
   m_pBookVar.p[0] = .5;
   m_pBookVar.p[1] = .2;
   m_pBookVar.p[2] = .2;
   m_pBookVar.p[3] = 1;  // up to 1 cm indented - units in cm
   m_acColor[0] = RGB(0xff,0,0);
   m_acColor[1] = RGB(0,0xff,0);
   m_acColor[2] = RGB(0,0,0xff);
   m_fBindOut = .2;
   m_fPageIndent = .003;
   m_fRowLength = .5;

   // Recalc books
   // FillBookInfo();


   // color for the Books
   ObjectSurfaceAdd (1, RGB(0x0,0xff,0x0), MATERIAL_PAINTGLOSS, L"Book binding");
   ObjectSurfaceAdd (2, RGB(0xff,0xff,0xff), MATERIAL_FLAT, L"Book pages");
}


CObjectBooks::~CObjectBooks (void)
{
   // do nothing for now
}


/**********************************************************************************
CObjectBooks::Delete - Called to delete this object
*/
void CObjectBooks::Delete (void)
{
   delete this;
}


/**********************************************************************************
CObjectBooks::FillBookInfo - Fills the book information in.

inputs
   none
returns
   none
*/
void CObjectBooks::FillBookInfo (void)
{
   // seed the random
   srand (m_dwFillInSeed);

   // clear the list
   m_lBOOKINFO.Clear();

   // make sure at least have one book
   fp fLenLeft, fCurLoc;
   fLenLeft = max(m_fRowLength, CLOSE);
   fCurLoc = 0;

   // fill in standard information
   BOOKINFO bi;
   memset (&bi, 0, sizeof(bi));
   bi.fPageIndent = m_fPageIndent;

   // convert the random colors to points
   CPoint apColor[3];
   DWORD i;
   for (i = 0; i < 3; i++) {
      apColor[i].p[0] = GetRValue(m_acColor[i]);
      apColor[i].p[1] = GetGValue(m_acColor[i]);
      apColor[i].p[2] = GetBValue(m_acColor[i]);
   }

   // add books
   fp f;
   CPoint pColor;
   for (; fLenLeft > 0; fLenLeft -= (bi.pSize.p[0]+.001), fCurLoc += (bi.pSize.p[0]+.001)) {
      bi.pSize.Copy (&m_pBookSize);

      // random width
      bi.pSize.p[0] *= randf(1.0 - m_pBookVar.p[0], 1.0 + m_pBookVar.p[0]);

      // random scale
      f = randf (1.0 - m_pBookVar.p[1], 1 + m_pBookVar.p[1]);
      bi.pSize.p[1] *= f;
      bi.pSize.p[2] *= f;

      // random aspect
      f = randf (1.0 - m_pBookVar.p[2], 1 + m_pBookVar.p[2]);
      bi.pSize.p[1] *= f;
      bi.pSize.p[2] /= f;

      // random color
      DWORD dw;
      dw = rand() % 3;
      pColor.Average (&apColor[dw], &apColor[(dw+1)%3], randf(0,1));
      pColor.Average (&apColor[(dw+2)%3], randf(0,1));
      bi.cColor = RGB(pColor.p[0], pColor.p[1], pColor.p[2]);
      
      // other values
      bi.fBindOut = bi.pSize.p[0] * m_fBindOut;
      bi.fHorzLoc = fCurLoc;
      bi.fIndent = randf(0, m_pBookVar.p[3]) / 100.0;

      m_lBOOKINFO.Add (&bi);
   }
}


/**********************************************************************************
CObjectBooks::RenderBook - Draws a book. The lower left corner of the binding
edge is at 0,0,0. The book's height is positive Z. It extends (opening end)
to positive Y. The thickness extends to positive X.

inputs
   POBJECTRENDER     pr - Render information
   PCRenderSurface   prs - Draw to
   PCPoint           pSize - p[0] = thikcness, p[1] = length, p[2]=height
   fp                fBindOut - Number of meters the binding pops out (or in for paperbacks)
   fp                fPageIndent - Number of meters the edge of the page is in from the cover
   COLORREF          *pcReplace - If not NULL, replace the basic color provided by
                     object surface 1 (for the book binding) with this.
returns
   none
*/
void CObjectBooks::RenderBook (POBJECTRENDER pr, PCRenderSurface prs, PCPoint pSize,
                               fp fBindOut, fp fPageIndent, COLORREF *pcReplace)
{
   // binding color
   prs->SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (1), m_pWorld);
   if (pcReplace) {
      prs->SetDefColor (*pcReplace);
   }

   // left binding
   CPoint   ap[4];
   memset (ap, 0, sizeof(ap));
   ap[0].p[1] = pSize->p[1];
   ap[1].p[1] = pSize->p[1];
   ap[1].p[2] = pSize->p[2];
   ap[2].p[2] = pSize->p[2];
   // ap[3] is 0
   prs->ShapeQuad (&ap[1], &ap[2], &ap[3], &ap[0], FALSE);

   // right binding
   DWORD i;
   for (i = 0; i < 4; i++)
      ap[i].p[0] = pSize->p[0];
   prs->ShapeQuad (&ap[2], &ap[1], &ap[0], &ap[3], FALSE);
   // back of the binding
#define BINDDETAIL      6
   CPoint aBind[2][BINDDETAIL];
   DWORD x,y;
   for (y = 0; y < 2; y++) {
      for (x = 0; x < BINDDETAIL; x++) {
         if (y) {
            aBind[y][x].Copy (&aBind[1-y][x]);
            aBind[y][x].p[2] = 0;
         }
         else if (x < BINDDETAIL/2) {
            aBind[y][x].p[0] = (1-cos ((fp)x / (fp)(BINDDETAIL-1) * PI)) * pSize->p[0] / 2;
            aBind[y][x].p[1] = -sin((fp)x / (fp)(BINDDETAIL-1) * PI) * fBindOut;
            aBind[y][x].p[2] = pSize->p[2];  // since y == 0
            aBind[y][x].p[3] = 1;
         }
         else {
            aBind[y][x].Copy (&aBind[y][BINDDETAIL-x-1]);
            aBind[y][x].p[0] = pSize->p[0] - aBind[y][x].p[0];
         }
      }
   }
   PCPoint pAdd;
   DWORD dwIndex;
   pAdd = prs->NewPoints (&dwIndex, 2 * BINDDETAIL);
   memcpy (pAdd, &aBind[0][0], sizeof(aBind));
   prs->ShapeSurface (0, BINDDETAIL, 2, pAdd, dwIndex, NULL, 0, FALSE);

   // draw the pages
   prs->SetDefMaterial (m_OSINFO.dwRenderShard, ObjectSurfaceFind (2), m_pWorld);
   prs->ShapeBox (CLOSE, max(0,-fBindOut) + CLOSE, fPageIndent,
      pSize->p[0]-CLOSE, pSize->p[1] - fPageIndent, pSize->p[2] - fPageIndent);

   // done
}

/**********************************************************************************
CObjectBooks::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectBooks::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   // fill in the info
   // BUGFIX - Do this later so have a GUID set by the time get here
   DWORD dwSeed;
   DWORD *padw;
   DWORD i;
   dwSeed = 0;
   padw = (DWORD*) &m_gGUID;
   for (i = 0; i < sizeof(m_gGUID)/sizeof(DWORD); i++)
      dwSeed += padw[i];
   if (dwSeed != m_dwFillInSeed) {
      m_dwFillInSeed = dwSeed;
      FillBookInfo ();
   }

   // draw all the books
   DWORD dwNum;
   dwNum = m_lBOOKINFO.Num();
   for (i = 0; i < dwNum; i++) {
      PBOOKINFO pbi = (PBOOKINFO) m_lBOOKINFO.Get(i);

      // translate
      m_Renderrs.Push();
      m_Renderrs.Translate (pbi->fHorzLoc, (dwNum>1) ? pbi->fIndent : 0, 0);

      // if only one book then draw without changes
      if (dwNum == 1) {
         RenderBook (pr, &m_Renderrs, &m_pBookSize, m_fBindOut * m_pBookSize.p[0],
            pbi->fPageIndent, NULL);
      }
      else {
         RenderBook (pr, &m_Renderrs, &pbi->pSize, pbi->fBindOut, pbi->fPageIndent, &pbi->cColor);
      }

      // pop
      m_Renderrs.Pop();
   }

   m_Renderrs.Commit();
}



/**********************************************************************************
CObjectBooks::QueryBoundingBox - Standard API
*/
void CObjectBooks::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   // fill in the info
   // BUGFIX - Do this later so have a GUID set by the time get here
   DWORD dwSeed;
   DWORD *padw;
   DWORD i;
   dwSeed = 0;
   padw = (DWORD*) &m_gGUID;
   for (i = 0; i < sizeof(m_gGUID)/sizeof(DWORD); i++)
      dwSeed += padw[i];
   if (dwSeed != m_dwFillInSeed) {
      m_dwFillInSeed = dwSeed;
      FillBookInfo ();
   }

   // draw all the books
   DWORD dwNum;
   CPoint pSize;
   CPoint p1, p2;
   fp fBindOut;
   CMatrix m;
   dwNum = m_lBOOKINFO.Num();
   for (i = 0; i < dwNum; i++) {
      PBOOKINFO pbi = (PBOOKINFO) m_lBOOKINFO.Get(i);

      // translate
      m.Translation (pbi->fHorzLoc, (dwNum>1) ? pbi->fIndent : 0, 0);

      // if only one book then draw without changes
      if (dwNum == 1) {
         pSize.Copy (&m_pBookSize);
         fBindOut = m_fBindOut * m_pBookSize.p[0];
      }
      else {
         pSize.Copy (&pbi->pSize);
         fBindOut = pbi->fBindOut;
      }

      p1.Zero();
      p2.Copy (&pSize);
      if (fBindOut > 0)
         p1.p[1] = -fBindOut;
      
      BoundingBoxApplyMatrix (&p1, &p2, &m);

      if (i) {
         pCorner1->Min(&p1);
         pCorner2->Max(&p2);
      }
      else {
         pCorner1->Copy(&p1);
         pCorner2->Copy(&p2);
      }
   }

#ifdef _DEBUG
   // test, make sure bounding box not too small
   //CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectBooks::QueryBoundingBox too small.");
#endif
}



/**********************************************************************************
CObjectBooks::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectBooks::Clone (void)
{
   PCObjectBooks pNew;

   pNew = new CObjectBooks(NULL, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   pNew->m_pBookSize.Copy (&m_pBookSize);
   pNew->m_pBookVar.Copy (&m_pBookVar);
   memcpy (pNew->m_acColor, m_acColor, sizeof(m_acColor));
   pNew->m_fBindOut = m_fBindOut;
   pNew->m_fPageIndent = m_fPageIndent;
   pNew->m_fRowLength = m_fRowLength;
   pNew->m_dwFillInSeed = m_dwFillInSeed;

   // clone book info
   pNew->m_lBOOKINFO.Init (sizeof(BOOKINFO), m_lBOOKINFO.Get(0), m_lBOOKINFO.Num());

   return pNew;
}

static PWSTR gpszBookSize = L"BookSize";
static PWSTR gpszBookVar = L"BookVar";
static PWSTR gpszBindOut = L"BindOut";
static PWSTR gpszPageIndent = L"PageIndent";
static PWSTR gpszRowLength = L"RowLength";

PCMMLNode2 CObjectBooks::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszBookSize, &m_pBookSize);
   MMLValueSet (pNode, gpszBookVar, &m_pBookVar);
   MMLValueSet (pNode, gpszBindOut, m_fBindOut);
   MMLValueSet (pNode, gpszPageIndent, m_fPageIndent);
   MMLValueSet (pNode, gpszRowLength, m_fRowLength);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"Color%d", (int) i);
      MMLValueSet (pNode, szTemp, (int)m_acColor[i]);
   }

   return pNode;
}

BOOL CObjectBooks::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;

   MMLValueGetPoint (pNode, gpszBookSize, &m_pBookSize);
   MMLValueGetPoint (pNode, gpszBookVar, &m_pBookVar);
   m_fBindOut = MMLValueGetDouble (pNode, gpszBindOut, 0);
   m_fPageIndent = MMLValueGetDouble (pNode, gpszPageIndent, 0);
   m_fRowLength = MMLValueGetDouble (pNode, gpszRowLength, 0);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"Color%d", (int) i);
      m_acColor[i]= (COLORREF) MMLValueGetInt (pNode, szTemp, (int)0);
   }

   // Recalc books
   FillBookInfo();

   return TRUE;
}


/*************************************************************************************
CObjectBooks::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBooks::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = m_pBookSize.p[0]*4;

   if (dwID >= 1)
      return FALSE;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
//   pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = fKnobSize;
   pInfo->cColor = RGB(0,0xff,0xff);
   wcscpy (pInfo->szName, L"Row length");
   MeasureToString (m_fRowLength, pInfo->szMeasurement);

   pInfo->pLocation.Copy (&m_pBookSize);
   pInfo->pLocation.Scale (.5);
   pInfo->pLocation.p[0] = m_fRowLength;

   return TRUE;
}

/*************************************************************************************
CObjectBooks::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBooks::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   if (dwID >= 1)
      return FALSE;

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   m_fRowLength = max(0, pVal->p[0]);

   FillBookInfo();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}

/*************************************************************************************
CObjectBooks::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectBooks::ControlPointEnum (PCListFixed plDWORD)
{
   DWORD i;

   DWORD dwNum = 1;
   plDWORD->Required (dwNum);
   for (i = 0; i < dwNum; i++)
      plDWORD->Add (&i);
}



/* BooksDialogPage
*/
BOOL BooksDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCObjectBooks pv = (PCObjectBooks) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // enable/disable modify custom shape
         PCEscControl pControl;
         WCHAR szTemp[32];
         DWORD i;

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"color%d", (int)i);
            FillStatusColor (pPage, szTemp, pv->m_acColor[i]);
         }

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"booksize%d", (int)i);
            MeasureToString (pPage, szTemp, pv->m_pBookSize.p[i]);
         }
         MeasureToString (pPage, L"pageindent", pv->m_fPageIndent);

         // scrolling
         pControl = pPage->ControlFind (L"bindout");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBindOut * 100));
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"bookvar%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pBookVar.p[i] * 100));
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

         PWSTR pszChangeColor = L"changecolor";
         DWORD dwChangeColorLen = (DWORD)wcslen(pszChangeColor);

         // if it's for the colors then do those
         if (!wcsncmp(psz, pszChangeColor, dwChangeColorLen)) {
            DWORD i = _wtoi(psz + dwChangeColorLen);
            WCHAR szTemp[32];
            i = min(2, i);
            swprintf (szTemp, L"color%d", (int)i);


            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acColor[i], pPage, szTemp);
            if (cr != pv->m_acColor[i]) {
               pv->m_pWorld->ObjectAboutToChange (pv);
               pv->m_acColor[i] = cr;
               pv->FillBookInfo();
               pv->m_pWorld->ObjectChanged (pv);
            }
         }

      }
      break;   // default


   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!psz)
            break;

         // get value
         fp fVal, *pf;
         fVal = p->pControl->AttribGetInt (Pos()) / 100.0;
         pf = NULL;

         if (!_wcsicmp(psz, L"bindout"))
            pf = &pv->m_fBindOut;
         else if (!_wcsicmp(psz, L"bookvar0"))
            pf = &pv->m_pBookVar.p[0];
         else if (!_wcsicmp(psz, L"bookvar1"))
            pf = &pv->m_pBookVar.p[1];
         else if (!_wcsicmp(psz, L"bookvar2"))
            pf = &pv->m_pBookVar.p[2];
         else if (!_wcsicmp(psz, L"bookvar3"))
            pf = &pv->m_pBookVar.p[3];

         if (!pf || (*pf == fVal))
            break;   // no change

         pv->m_pWorld->ObjectAboutToChange (pv);
         *pf = fVal;
         pv->FillBookInfo();
         pv->m_pWorld->ObjectChanged (pv);
      }
      break;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         // since any edit change will result in redraw, get them all
         pv->m_pWorld->ObjectAboutToChange (pv);

         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"booksize%d", (int)i);
            MeasureParseString (pPage, szTemp, &pv->m_pBookSize.p[i]);
            pv->m_pBookSize.p[i] = max(2*CLOSE, pv->m_pBookSize.p[i]);
         }
         MeasureParseString (pPage, L"pageindent", &pv->m_fPageIndent);

         pv->FillBookInfo ();
         pv->m_pWorld->ObjectChanged (pv);


         break;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Row-of-books settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectBooks::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CObjectBooks::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   PWSTR pszRet;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBOOKSDIALOG, BooksDialogPage, this);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CObjectBooks::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectBooks::DialogQuery (void)
{
   return TRUE;
}

