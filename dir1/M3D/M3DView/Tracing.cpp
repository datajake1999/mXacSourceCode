/***************************************************************************
Tracing.cpp - code to handle tracing paper

begun 21/11/2002
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

typedef struct {
   PCHouseView    pv;             // if this is for a house view then will be non-NULL
   PCGroundView   pg;             // if this is for a ground view then will be non-NULL
   char           szAddFile[256];      // if using "add" this will be added
   PCImage        pImage;  // will be filled when "add" called
   DWORD          dwTraceIndex;  // index into the tracing list that working on
   PCRenderTraditional pRender;  // if have a house view, set to pv->m_aRender[0]
   PCListFixed    plTraceInfo;   // list initialized to sizeof(TRACEINFO), from PCHouseView or PCGroundView
   PCWorldSocket  pWorld;        // if house view, this is the world. Else, NULL
} TRACEPAGE, *PTRACEPAGE;

static PWSTR gpszTraceInfo = L"TraceInfo";
static PWSTR gpszCenter = L"Center";
static PWSTR gpszFile = L"File";
static PWSTR gpszAngle = L"Angle";
static PWSTR gpszWidth = L"Width";
static PWSTR gpszTransparency = L"Transparency";
static PWSTR gpszShow = L"Show";
static PWSTR gpszTransModel = L"TransModel";
static PWSTR gpszTraceImage = L"TraceImage";

typedef struct {
   WCHAR       szFile[256];      // file name
   DWORD       dwWidth;          // width in pixels
   DWORD       dwHeight;         // height in pixels
   COLORREF    *pcr;             // memory with RGB values
} TRACECACHE, *PTRACECACHE;

static CListFixed glTraceCache;      // trace cache, initiaized to TRACECACHE
static BOOL gfTraceCacheInit = FALSE;   // set to true if trace cache intiialized

/****************************************************************************
TraceFromCache - This looks in the trace cache for the given file. If it's ont
there it's cached anyhow. It returns a pointer to memory, width and height
of the data.

inputs
   PWSTR       pszFile - file to look for
   DWORD       *pdwWidth - Filled in with the width
   DWORD       *pdwHeight - Filled in with the height
returns
   COLORREF * - Pointer to an array of color values (dont delete or touch).
               Alligned [dwWidth][dwHeight]
*/
COLORREF *TraceFromCache (PWSTR pszFile, DWORD *pdwWidth, DWORD *pdwHeight)
{
   if (!gfTraceCacheInit) {
      gfTraceCacheInit = TRUE;
      glTraceCache.Init (sizeof(TRACECACHE));
   }

   // exists
   DWORD i;
   PTRACECACHE ptc;
   DWORD dwNum;
   ptc = (PTRACECACHE) glTraceCache.Get(0);
   dwNum = glTraceCache.Num();
   for (i = 0; i < dwNum; i++, ptc++)  // BUGFIX - forgot to incrase ptc, so infinite number o fimages
      if (!_wcsicmp(ptc->szFile, pszFile)) {
         *pdwWidth = ptc->dwWidth;
         *pdwHeight = ptc->dwHeight;
         return ptc->pcr;
      }

   // else load and add
   CImage Image;
   TRACECACHE tc;
   memset (&tc,0 ,sizeof(tc));
   wcscpy (tc.szFile, pszFile);
   if (!Image.Init (pszFile)) {
      // add a blank one just so know it's not there and don't keep trying to reload
      glTraceCache.Add (&tc);
      return NULL;    // error. Not there
   }

   // allocate enough memory for it
   tc.dwWidth = Image.Width();
   tc.dwHeight = Image.Height();
   tc.pcr = (COLORREF*) ESCMALLOC (sizeof(COLORREF) * tc.dwWidth * tc.dwHeight);
   if (!tc.pcr)
      return NULL;   // error

   // fill in memory
   PIMAGEPIXEL pip;
   pip = Image.Pixel(0,0);
   for (i = 0; i < tc.dwWidth * tc.dwHeight; i++, pip++)
      tc.pcr[i] = Image.UnGamma (&pip->wRed);

   // add it
   glTraceCache.Add (&tc);

   *pdwWidth = tc.dwWidth;
   *pdwHeight = tc.dwHeight;
   return tc.pcr;
}

/****************************************************************************
TraceCacheClear - Clears the contents of the trace cache. Should be called
before shutdown.
*/
void TraceCacheClear (void)
{
   DWORD i;
   PTRACECACHE ptc;
   DWORD dwNum;
   ptc = (PTRACECACHE) glTraceCache.Get(0);
   dwNum = glTraceCache.Num();
   for (i = 0; i < dwNum; i++) {
      if (ptc[i].pcr)
         ESCFREE (ptc[i].pcr);
   };

   glTraceCache.Clear();
}


/****************************************************************************
TraceInfoToWorld - Given a pointer to a list of TRACEINFO, this writes it
out to the world

inputs
   PCWorldSocket     pWorld - World to read from
   PCListFixed       pList - Initialzief to sizeof(TRACEINFO) and filled with info
returns
   non
*/
void TraceInfoToWorld (PCWorldSocket pWorld, PCListFixed pList)
{
   if (!pWorld)
      return;

   // the the MML from the world
   PCMMLNode2 pAux = pWorld->AuxGet ();
   PCMMLNode2 pFind;
   if (!pAux)
      return;  // nothing to get
   pFind = NULL;
   PWSTR psz;
   pAux->ContentEnum (pAux->ContentFind (gpszTraceInfo), &psz, &pFind);
   if (!pFind) {
      pFind = pAux->ContentAddNewNode ();
      if (!pFind)
         return;
      pFind->NameSet (gpszTraceInfo);
   }

   // clear all the elemnts out of pFind
   while (pFind->ContentNum())
      pFind->ContentRemove (0);

   // add new items in
   DWORD i;
   for (i = 0; i < pList->Num(); i++) {
      PTRACEINFO pti = (PTRACEINFO) pList->Get(i);
      PCMMLNode2 pNode = pFind->ContentAddNewNode ();
      if (!pNode)
         continue;
      pNode->NameSet (gpszTraceImage);

      MMLValueSet (pNode, gpszFile, pti->szFile);
      MMLValueSet (pNode, gpszCenter, &pti->pCenter);
      MMLValueSet (pNode, gpszAngle, &pti->pAngle);
      MMLValueSet (pNode, gpszWidth, pti->fWidth);
      MMLValueSet (pNode, gpszTransparency, pti->fTransparency);
      MMLValueSet (pNode, gpszShow, (int) pti->fShow);
      MMLValueSet (pNode, gpszTransModel, (int) pti->dwTransModel);

   }


   // finally, set dirty flag
   pWorld->DirtySet (TRUE);
}


/****************************************************************************
TraceInfoFromWorld - Given a pointer to a world this extracts the TRACEINFO
structures from it

inputs
   PCWorldSocket     pWorld - World to read from
   PCListFixed       pList - Initialzief to sizeof(TRACEINFO) and filled with info
returns
   non
*/
void TraceInfoFromWorld (PCWorldSocket pWorld, PCListFixed pList)
{
   if (!pWorld)
      return;

   pList->Init (sizeof(TRACEINFO));
   pList->Clear();

   // the the MML from the world
   PCMMLNode2 pAux = pWorld->AuxGet ();
   PCMMLNode2 pFind, pNode;
   if (!pAux)
      return;  // nothing to get
   pFind = NULL;
   PWSTR psz;
   pAux->ContentEnum (pAux->ContentFind (gpszTraceInfo), &psz, &pFind);
   if (!pFind)
      return;  // nothing to get

   // else, deconstruct this
   DWORD i;
   TRACEINFO ti;
   for (i = 0; i < pFind->ContentNum (); i++) {
      pNode = NULL;
      if (!pFind->ContentEnum (i, &psz, &pNode))
         continue;
      if (!pNode)
         continue;
      psz = pNode->NameGet();
      if (!psz || _wcsicmp(psz, gpszTraceImage))
         continue;   // wrong one

      // get it
      memset (&ti, 0, sizeof(ti));
      psz = MMLValueGet (pNode, gpszFile);
      if (psz)
         wcscpy (ti.szFile, psz);
      MMLValueGetPoint (pNode, gpszCenter, &ti.pCenter);
      MMLValueGetPoint (pNode, gpszAngle, &ti.pAngle);
      ti.fWidth = MMLValueGetDouble (pNode, gpszWidth, 1);
      ti.fTransparency = MMLValueGetDouble (pNode, gpszTransparency, .5);
      ti.fShow = (BOOL) MMLValueGetInt (pNode, gpszShow, TRUE);
      ti.dwTransModel = (DWORD) MMLValueGetInt (pNode, gpszTransModel, 0);

      pList->Add (&ti);
   }
}

/****************************************************************************
TraceDialogPage
*/
BOOL TraceDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTRACEPAGE ptp = (PTRACEPAGE)pPage->m_pUserData;
   PCHouseView pv = ptp->pv;
   PCGroundView pg = ptp->pg;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // disable buttons
         PCEscControl pControl;
         BOOL fIso = ptp->pRender ? (ptp->pRender->CameraModelGet() == CAMERAMODEL_FLAT) : TRUE;
         if (!fIso && (pControl = pPage->ControlFind (L"add")))
            pControl->Enable (FALSE);

         // Fill in the list box
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCM_USER+82:   // fill in the list box
      {
         // clear the existing list
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"loaded");
         if (!pControl)
            return FALSE;
         pControl->Message (ESCM_LISTBOXRESETCONTENT);

         MemZero (&gMemTemp);

         DWORD i;
         for (i = 0; i < ptp->plTraceInfo->Num(); i++) {
            PTRACEINFO pti = (PTRACEINFO) ptp->plTraceInfo->Get(i);
            
            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L"><bold>");
            MemCatSanitize (&gMemTemp, pti->szFile);
            MemCat (&gMemTemp, L"</bold>");

            // show hidden and stuff?
            if (!pti->fShow)
               MemCat (&gMemTemp, L" (Hidden)");

            // show the image
            MemCat (&gMemTemp, L"<br/><align align=right><image width=50%% file=\"");
            MemCatSanitize (&gMemTemp, pti->szFile);
            MemCat (&gMemTemp, L"\"/></align>");

            MemCat (&gMemTemp, L"</elem>");
         }

         ESCMLISTBOXADD lba;
         memset (&lba, 0,sizeof(lba));
         lba.pszMML = (PWSTR)gMemTemp.p;

         pControl->Message (ESCM_LISTBOXADD, &lba);

         pControl->AttribSetInt (CurSel(), 0);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PCEscControl pControl;
         DWORD dwSel;
         pControl = pPage->ControlFind (L"loaded");
         dwSel = pControl ? (DWORD) pControl->AttribGetInt (CurSel()) : 0;

         if (!_wcsicmp(psz, L"showhide")) {
            if (dwSel >= ptp->plTraceInfo->Num()) {
               pPage->MBWarning (L"You must select an image first.");
               return TRUE;
            }

            PTRACEINFO pti;
            pti = (PTRACEINFO) ptp->plTraceInfo->Get(dwSel);
            pti->fShow = !pti->fShow;
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();

            // Fill in the list box
            pPage->Message (ESCM_USER+82);
            pControl->AttribSetInt (CurSel(), (int) dwSel);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"remove")) {
            if (dwSel >= ptp->plTraceInfo->Num()) {
               pPage->MBWarning (L"You must select an image first.");
               return TRUE;
            }

            ptp->plTraceInfo->Remove (dwSel);
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();

            // Fill in the list box
            pPage->Message (ESCM_USER+82);
            pControl->AttribSetInt (CurSel(), (int) dwSel);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"showall") || !_wcsicmp(psz, L"hideall")) {
            PTRACEINFO pti;
            DWORD i;
            BOOL fShow = !_wcsicmp(psz, L"showall");
            pti = (PTRACEINFO) ptp->plTraceInfo->Get(0);
            for (i = 0; i < ptp->plTraceInfo->Num(); i++)
               pti[i].fShow = fShow;
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();

            // Fill in the list box
            pPage->Message (ESCM_USER+82);
            pControl->AttribSetInt (CurSel(), (int) dwSel);

            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;   // no name

         if (!_wcsicmp(p->psz, L"edit")) {
            PCEscControl pControl;
            DWORD dwSel;
            pControl = pPage->ControlFind (L"loaded");
            dwSel = pControl ? (DWORD) pControl->AttribGetInt (CurSel()) : 0;
            if (dwSel >= ptp->plTraceInfo->Num()) {
               pPage->MBWarning (L"You must select an image first.");
               return TRUE;
            }

            // make sure looking at it
            if (ptp->pRender && (ptp->pRender->CameraModelGet() != CAMERAMODEL_FLAT)) {
               pPage->MBWarning (L"You must be viewing in flat (isometric) view to modify a tracing paper image.");
               return TRUE;
            }
            PTRACEINFO pti;
            pti = (PTRACEINFO) ptp->plTraceInfo->Get(dwSel);
            if (ptp->pRender) {
               CPoint pCenter, pAngle;
               fp fScale, fTransX, fTransY;
               ptp->pRender->CameraFlatGet (&pCenter, &pAngle.p[2], &pAngle.p[0], &pAngle.p[1],
                  &fScale, &fTransX, &fTransY);
               if (!pAngle.AreClose (&pti->pAngle)) {
                  pPage->MBWarning (L"You must be looking from the same angle as the tracing paper image.");
                  return TRUE;
               }
            }

            // ok, use this
            ptp->dwTraceIndex = dwSel;

            break; // default bhaviour
         }
         else if (!_wcsicmp(p->psz, L"add")) {
            // get the file name
            OPENFILENAME   ofn;
            char  szTemp[256];
            szTemp[0] = 0;
            memset (&ofn, 0, sizeof(ofn));
            char szInitial[256];
            GetLastDirectory(szInitial, sizeof(szInitial));
            ofn.lpstrInitialDir = szInitial;

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;
            ofn.lpstrFilter = "Image files (*.jpg;*.bmp)\0*.jpg;*.bmp\0\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Open image file";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = ".jpg";
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               return TRUE;   // failed to specify file so go back

            strcpy (ptp->szAddFile, szTemp);
            WCHAR szw[256];
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/2);

            if (!ptp->pImage->Init (szw)) {
               pPage->MBWarning (L"The image file couldn't be opened.",
                  L"It may not be a proper bitmap (.bmp) or JPEG (.jpg) file.");
               return TRUE;   // error
            }
            break;   // normal behaviour
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Tracing paper";
            return TRUE;
         }
      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
TraceAddPage
*/
BOOL TraceAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTRACEPAGE ptp = (PTRACEPAGE)pPage->m_pUserData;
   PCHouseView pv = ptp->pv;
   PCGroundView pg = ptp->pg;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // determine if it's mostly BonW, WOnB, or color
         DWORD dwAverage, i,dwNum;
         PIMAGEPIXEL pip;
         dwAverage = 0;
         dwNum = ptp->pImage->Width() * ptp->pImage->Height();
         pip = ptp->pImage->Pixel(0,0);
         for (i = 0; i < dwNum; i++, pip++) {
            dwAverage += (pip->wRed / 3 / 256 + pip->wGreen / 3 / 256 + pip->wBlue / 3 / 256);
         }
         dwAverage /= dwNum;

         // check buttons
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"sidontknow");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
         if (dwAverage > 200)
            pControl = pPage->ControlFind (L"tbonw");
         else if (dwAverage < 20)
            pControl = pPage->ControlFind (L"twonb");
         else
            pControl = pPage->ControlFind (L"tcolor");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         DoubleToControl (pPage, L"dpi", 300);
         MeasureToString (pPage, L"onpaper", .01);
         MeasureToString (pPage, L"inmodel", 1);
         MeasureToString (pPage, L"width", 1);
         MeasureToString (pPage, L"height", 1);
      }
      break;
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;   // no name

         if (!_wcsicmp(p->psz, L"next")) {
            TRACEINFO ti;
            memset (&ti, 0, sizeof(ti));

            // get the transparency mode
            PCEscControl pControl;
            if ((pControl = pPage->ControlFind (L"tbonw")) && pControl->AttribGetBOOL(Checked())) {
               ti.dwTransModel = 0;
               ti.fTransparency = 0;
            }
            else if ((pControl = pPage->ControlFind (L"twonb")) && pControl->AttribGetBOOL(Checked())) {
               ti.dwTransModel = 1;
               ti.fTransparency = 0;
            }
            else {
               ti.dwTransModel = 2;
               ti.fTransparency = .75;
            }

            // get angle from flat view
            CPoint pCenter;
            fp fTransX, fTransY;
            if (ptp->pRender) {
               ptp->pRender->CameraFlatGet (&pCenter, &ti.pAngle.p[2], &ti.pAngle.p[0], &ti.pAngle.p[1],
                  &ti.fWidth, &fTransX, &fTransY);
               ptp->pRender->PixelToWorldSpace ((fp)pv->m_aImage[0].Width() / 2.0,
                  (fp)pv->m_aImage[0].Height() / 2.0, 0, &ti.pCenter);
            }
            else {
               // get cetner of ground display
               RECT r;
               GetClientRect (pg->m_hWndMap, &r);
               pg->TracePixelToWorldSpace ((r.right + r.left)/2.0, (r.bottom + r.top) / 2.0, &ti.pCenter);
               ti.fWidth = (fp)(r.right - r.left) / pg->m_fViewScale * pg->m_gState.fScale;
               ti.pAngle.Zero();
            }
            ti.fShow = TRUE;
            MultiByteToWideChar (CP_ACP, 0, ptp->szAddFile, -1, ti.szFile, sizeof(ti.szFile));

            // figure out what scale information is given
            if ((pControl = pPage->ControlFind (L"siscan")) && pControl->AttribGetBOOL(Checked())) {
               fp fDPI, fOnPaper, fInModel;
               fDPI = DoubleFromControl (pPage, L"dpi");
               fDPI = max(1, fDPI);
               fOnPaper = 0;
               MeasureParseString (pPage, L"onpaper", &fOnPaper);
               fOnPaper = max(.001, fOnPaper);
               fInModel = 0;
               MeasureParseString (pPage, L"inmodel", &fInModel);
               fInModel = max(.001, fInModel);

               ti.fWidth = (fp) ptp->pImage->Width() / fDPI / INCHESPERMETER / fOnPaper * fInModel;
            }
            else if ((pControl = pPage->ControlFind (L"siwidth")) && pControl->AttribGetBOOL(Checked())) {
               ti.fWidth = 0;
               MeasureParseString (pPage, L"width", &ti.fWidth);
            }
            else if ((pControl = pPage->ControlFind (L"siheight")) && pControl->AttribGetBOOL(Checked())) {
               ti.fWidth = 0;
               MeasureParseString (pPage, L"height", &ti.fWidth);
               ti.fWidth = ti.fWidth / (fp) ptp->pImage->Height() * (fp) ptp->pImage->Width();
            }
            else {
               // do nothing because already loaded
            }
            ti.fWidth = max(.001, ti.fWidth);

            // add it
            ptp->plTraceInfo->Add (&ti);
            ptp->dwTraceIndex = ptp->plTraceInfo->Num()-1;

            // write it out too
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();

            break;   // normal behaviour
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add new tracing paper";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TRACEFILE")) {
            MemZero (&gMemTemp);
            WCHAR szw[256];
            MultiByteToWideChar (CP_ACP, 0, ptp->szAddFile, -1, szw, sizeof(szw)/2);
            MemCat (&gMemTemp, szw);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WIDTH")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, (int) ptp->pImage->Width());
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"HEIGHT")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, (int) ptp->pImage->Height());
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
TraceEditPage
*/
BOOL TraceEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTRACEPAGE ptp = (PTRACEPAGE)pPage->m_pUserData;
   PCHouseView pv = ptp->pv;
   PCGroundView pg = ptp->pg;
   PTRACEINFO pti = (PTRACEINFO) ptp->plTraceInfo->Get(ptp->dwTraceIndex);

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         if (!pti)
            break;   // dont know what to do

         // set the width
         MeasureToString (pPage, L"width", pti->fWidth);

         // check buttons
         PCEscControl pControl;
         switch (pti->dwTransModel) {
         case 0: // b o nw
         default:
            pControl = pPage->ControlFind (L"tbonw");
            break;
         case 1:  // w on b
            pControl = pPage->ControlFind (L"twonb");
            break;
         case 2:  // color
            pControl = pPage->ControlFind (L"tcolor");
            break;
         }
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // scrollbar
         pControl = pPage->ControlFind (L"transparency");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pti->fTransparency * 100));
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;
         if (!pti)
            break;

         // only do one scroll bar
         if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"transparency"))
            break;

         // set value
         fp fVal;
         fVal = p->pControl->AttribGetInt (Pos()) / 100.0;
         if (fVal != pti->fTransparency) {
            pti->fTransparency = fVal;
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"width")) {
            MeasureParseString (pPage, psz, &pti->fWidth);
            pti->fWidth = max(.001, pti->fWidth);
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();
            return TRUE;
         }
      }

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszMoveUD = L"moveud", pszMoveLR = L"movelr", pszScale = L"scale";
         DWORD dwMoveUD = (DWORD)wcslen(pszMoveUD), dwMoveLR = (DWORD)wcslen(pszMoveLR), dwScale = (DWORD)wcslen(pszScale);

         if (!wcsncmp(psz, pszMoveUD, dwMoveUD)) {
            int iAmt = _wtoi(psz + dwMoveUD);
            if (abs(iAmt) == 2)
               iAmt *= 5;
            else if (abs(iAmt) == 3)
               iAmt *= 25;
            CPoint p1, p2;
            if (ptp->pRender) {
               ptp->pRender->PixelToWorldSpace (0, 0, 0, &p1);
               ptp->pRender->PixelToWorldSpace (0, -iAmt, 0, &p2);
            }
            else {
               pg->TracePixelToWorldSpace (0, 0, &p1);
               pg->TracePixelToWorldSpace (0, -iAmt, &p2);
            }
            p2.Subtract (&p1);
            pti->pCenter.Add (&p2);
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();
            return TRUE;
         }
         else if (!wcsncmp(psz, pszMoveLR, dwMoveLR)) {
            int iAmt = _wtoi(psz + dwMoveLR);
            if (abs(iAmt) == 2)
               iAmt *= 5;
            else if (abs(iAmt) == 3)
               iAmt *= 25;
            CPoint p1, p2;
            if (ptp->pRender) {
               ptp->pRender->PixelToWorldSpace (0, 0, 0, &p1);
               ptp->pRender->PixelToWorldSpace (iAmt, 0, 0, &p2);
            }
            else {
               pg->TracePixelToWorldSpace (0, 0, &p1);
               pg->TracePixelToWorldSpace (iAmt, 0, &p2);
            }
            p2.Subtract (&p1);
            pti->pCenter.Add (&p2);
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();
            return TRUE;
         }
         else if (!wcsncmp(psz, pszScale, dwScale)) {
            int iAmt = _wtoi(psz + dwScale);
            fp fScale = 1;
            switch (iAmt) {
            case -3:
               fScale = 1.0 / 1.1;
               break;
            case -2:
               fScale = 1.0 / 1.01;
               break;
            case -1:
               fScale = 1.0 / 1.001;
               break;
            case 1:
               fScale = 1.001;
               break;
            case 2:
               fScale = 1.01;
               break;
            case 3:
               fScale = 1.1;
               break;
            }
            pti->fWidth *= fScale;
            pti->fWidth = max(.001, pti->fWidth);
            MeasureToString (pPage, L"width", pti->fWidth);
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"tbonw")) {
            if (p->pControl->AttribGetBOOL(Checked()))
               pti->dwTransModel = 0;
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"twonb")) {
            if (p->pControl->AttribGetBOOL(Checked()))
               pti->dwTransModel = 1;
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"tcolor")) {
            if (p->pControl->AttribGetBOOL(Checked()))
               pti->dwTransModel = 2;
            TraceInfoToWorld (ptp->pWorld, ptp->plTraceInfo);
            if (pv)
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            else if (pg)
               pg->InvalidateDisplay();
            return TRUE;
         }
      }

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify tracing paper";
            return TRUE;
         }
      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
TraceDialog - Bring up the tracing dialog.

inputs
   PCHouseView    pv - House view. Use NULL if tracing is for ground
   PCGroundView   pg - Ground view. Use NULL if tracing if for house
returns
   BOOL - TRUE if pressed back, FALSE if something else
*/
BOOL TraceDialog (PCHouseView pv, PCGroundView pg)
{
   CEscWindow cWindow;
   RECT r;
   CImage   ImageTemp;
   DialogBoxLocation (pv ? pv->m_hWnd : pg->m_hWnd, &r);

   cWindow.Init (ghInstance, pv ? pv->m_hWnd : pg->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // start with the first page
   TRACEPAGE tp;
   memset (&tp, 0, sizeof(tp));
   tp.pv = pv;
   tp.pg = pg;
   tp.pImage = &ImageTemp;
   if (pv) {
      tp.plTraceInfo = &pv->m_lTraceInfo;
      tp.pRender = pv->m_apRender[0];
      tp.pWorld = pv->m_pWorld;
   }
   else if (pg) {
      tp.plTraceInfo = &pg->m_lTraceInfo;
      // render == NULL
      // world == null
   }
firstpage:
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTRACEDIALOG, TraceDialogPage, &tp);
firstpage2:
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"add")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTRACEADD, TraceAddPage, &tp);

      if (pszRet && !_wcsicmp(pszRet, L"next")) {
         pszRet = L"edit";
         goto firstpage2;
      }
      else if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"edit")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTRACEEDIT, TraceEditPage, &tp);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }

   return (pszRet && !_wcsicmp(pszRet, Back()));
}


/************************************************************************************
TraceSheetMerge - Merges a tracing paper image with the passed in CImage. This ASSUMES
that the tracing paper is visible, the right angle is shown, and already in isometric view.

inputs
   PTRACEINFO           pti - Trace information
   PCRenderTraditional  pRender - Where rendering to. Used for getting XY zero.
                        Use NULL if drawing onto ground
   PCGroundView         pg - Ground that drawing onto. Use NULL if drawing onto rendered image
   PCImage              pImage - Image rendering to
returns
   BOOL - TRUE if success
*/
BOOL TraceSheetMerge (PTRACEINFO pti, PCRenderTraditional pRender, PCGroundView pg, PCImage pImage)
{
   fp fCamScale;
   if (pRender) {
      CPoint pCamCenter, pCamAngle;
      fp fCamX, fCamY;
      pRender->CameraFlatGet (&pCamCenter, &pCamAngle.p[2], &pCamAngle.p[0], &pCamAngle.p[1],
         &fCamScale, &fCamX, &fCamY);
   }
   else {
      // scale from ground
      fCamScale = pImage->Width() / pg->m_fViewScale * pg->m_gState.fScale;
   }

   // get the bitmap
   COLORREF *pcr;
   DWORD dwTraceWidth, dwTraceHeight;
   pcr = TraceFromCache (pti->szFile, &dwTraceWidth, &dwTraceHeight);
   if (!pcr)
      return FALSE;

   // find out where this is in world space
   fp fCenterX, fCenterY;
   if (pRender) {
      if (!pRender->WorldSpaceToPixel (&pti->pCenter, &fCenterX, &fCenterY))
         return FALSE;
   }
   else {
      pg->TraceWorldSpaceToPixel (&pti->pCenter, &fCenterX, &fCenterY);
   }

   // how many pixels is pImage?
   DWORD dwImageWidth, dwImageHeight;
   dwImageWidth = pImage->Width();
   dwImageHeight = pImage->Height();

   // know then extent of the trace image in pixels
   TEXTUREPOINT tpMin, tpMax;
   tpMin.h = fCenterX - pti->fWidth / fCamScale * (fp)dwImageWidth / 2;
   tpMax.h = fCenterX + pti->fWidth / fCamScale * (fp)dwImageWidth / 2;
   tpMin.v = fCenterY - pti->fWidth / fCamScale * (fp)dwImageWidth / 2 * (fp) dwTraceHeight / (fp) dwTraceWidth;
   tpMax.v = fCenterY + pti->fWidth / fCamScale * (fp)dwImageWidth / 2 * (fp) dwTraceHeight / (fp) dwTraceWidth;

   // is it visible?
   if ((tpMin.h >= (fp)dwImageWidth) || (tpMin.v >= (fp) dwImageHeight) ||
      (tpMax.h <= 0) || (tpMax.v <= 0))
      return FALSE;  // not visible

   // convert min and max to DWORD
   DWORD dwMinX, dwMaxX, dwMinY, dwMaxY;
   dwMinX = (DWORD) max(0, tpMin.h);
   dwMaxX = (DWORD) min((fp)dwImageWidth, tpMax.h);
   dwMinY = (DWORD) max(0, tpMin.v);
   dwMaxY = (DWORD) min((fp)dwImageHeight, tpMax.v);

   // go through the image
   DWORD x,y, xx, yy;
   COLORREF *pix;
   WORD aw[3];
   DWORD dwTrans, dwBaseTrans;   // from 0 to 0xffff
   fp fxx, fyy, fDelta;
   PIMAGEPIXEL pip;
   dwBaseTrans = (DWORD) (pti->fTransparency * (fp)0xffff);
   for (y = dwMinY; y < dwMaxY; y++) {
      pip = pImage->Pixel (dwMinX, y);

      // find the location in the tracing paper
      fxx = ((fp)dwMinX - tpMin.h) / (tpMax.h - tpMin.h) * (fp) dwTraceWidth;
      fDelta = 1.0 / (tpMax.h - tpMin.h) * (fp) dwTraceWidth;
      fyy = ((fp)y - tpMin.v) / (tpMax.v - tpMin.v) * (fp) dwTraceHeight;

      // max sure yy in range
      fyy = max(0, fyy);
      fyy = min(dwTraceHeight-1, fyy);
      yy = (DWORD)fyy;

      for (x = dwMinX; x < dwMaxX; x++, pip++, fxx += fDelta) {
         // make sure xx in range
         if (fxx < 0)
            xx = 0;  // roundoff
         else if (fxx >= (fp)dwTraceWidth-1)
            xx = dwTraceWidth-1;
         else
            xx = (DWORD) fxx;

         // get the colorref
         pix = pcr + (yy * dwTraceWidth + xx);

         // convert these to gamma
         pImage->Gamma (*pix, &aw[0]);

         switch (pti->dwTransModel) {
            case 0:  // B on white
               dwTrans = ((DWORD) aw[0] + (DWORD)aw[1] + (DWORD)aw[2]) / 3;
               break;
            case 1:  // W on B
               dwTrans = 0xffff - ((DWORD) aw[0] + (DWORD)aw[1] + (DWORD)aw[2]) / 3;
               break;
            case 2:  // color
               dwTrans = 0;
         }
         dwTrans += dwBaseTrans;
         dwTrans = min(0xffff,dwTrans);

         pip->wRed = (WORD)(((DWORD)pip->wRed * dwTrans + (DWORD) aw[0] * (0xffff - dwTrans)) >> 16);
         pip->wGreen = (WORD)(((DWORD)pip->wGreen * dwTrans + (DWORD) aw[1] * (0xffff - dwTrans)) >> 16);
         pip->wBlue = (WORD)(((DWORD)pip->wBlue * dwTrans + (DWORD) aw[2] * (0xffff - dwTrans)) >> 16);

      } // over x
   } // over y

   return TRUE;
}



/************************************************************************************
TraceApply - Merges all the tracing paper images with the passed in CImage. This does tests for
that the tracing paper is visible, the right angle is shown, and already in isometric view.

inputs
   PCListFixed          plTRACEINFO - List of traceinfo
   PCRenderTraditional  pRender - Where rendering to. Used for getting XY zero.
   PCImage              pImage - Image rendering to
returns
   BOOL - TRUE if success
*/
BOOL TraceApply (PCListFixed plTRACEINFO, PCRenderTraditional pRender, PCImage pImage)
{
   if (pRender->CameraModelGet() != CAMERAMODEL_FLAT)
      return FALSE;
   CPoint pCamCenter, pCamAngle;
   fp fCamScale, fCamX, fCamY;
   pRender->CameraFlatGet (&pCamCenter, &pCamAngle.p[2], &pCamAngle.p[0], &pCamAngle.p[1],
      &fCamScale, &fCamX, &fCamY);

   DWORD i, dwNum;
   PTRACEINFO pti;
   pti = (PTRACEINFO) plTRACEINFO->Get(0);
   dwNum = plTRACEINFO->Num();
   for (i = 0; i < dwNum; i++) {
      if (!pti[i].fShow)
         continue;  // not showing
      if (!pCamAngle.AreClose (&pti[i].pAngle))
         continue;  // not same angle

      // draw
      TraceSheetMerge (&pti[i], pRender, NULL, pImage);
   }

   // done
   return TRUE;
}


/************************************************************************************
TraceApply - Merges all the tracing paper images with the passed in CImage. This does tests for
that the tracing paper is visible, the right angle is shown, and already in isometric view.

inputs
   PCListFixed          plTRACEINFO - List of traceinfo
   PCGroundView         pg - Ground view
   PCImage              pImage - Image rendering to
returns
   BOOL - TRUE if success
*/
BOOL TraceApply (PCListFixed plTRACEINFO, PCGroundView pg, PCImage pImage)
{
   DWORD i, dwNum;
   PTRACEINFO pti;
   pti = (PTRACEINFO) plTRACEINFO->Get(0);
   dwNum = plTRACEINFO->Num();
   for (i = 0; i < dwNum; i++) {
      if (!pti[i].fShow)
         continue;  // not showing
      // draw
      TraceSheetMerge (&pti[i], NULL, pg, pImage);
   }

   // done
   return TRUE;
}

