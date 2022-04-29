/***********************************************************************
ControlListBox.cpp - Code for a control

begun 4/16/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "escarpment.h"
#include "resleak.h"

#define ESCM_LINEUP           (ESCM_USER+5)
#define ESCM_LINEDOWN         (ESCM_USER+6)
#define ESCM_HOME             (ESCM_USER+7)
#define ESCM_END              (ESCM_USER+8)
#define ESCM_PAGEUP           (ESCM_USER+11)
#define ESCM_PAGEDOWN         (ESCM_USER+12)

typedef struct {
   PCEscTextBlock    pTB;     // text block. Does NOT delete pNode
   PCMMLNode         pNode;   // node <elem> that created this
   BOOL              fDeleteNode;   // if TRUE, delete pNode when this is deleted
   int               iHeight; // height of the node (including margins)
   int               iPosY;   // Y position. Sum of all Y positions above
   PWSTR             pszName; // pointer to name. may be NULL
   PWSTR             pszData; // pointer to data. may be NULL
} LBELEM, *PLBELEM;

typedef struct {
   int            iLRMargin;  // left right margin
   int            iTBMargin;  // top bottom margin
   int            iBorderSize;    // border width
   COLORREF       cBackground;   // background color
   COLORREF       cBorder;    // border color, default blue
   PWSTR          pszVScroll; // vertical scroll bar name
   COLORREF       cSelection; // color of the selection
   COLORREF       cSelectionBorder; // color of the selection border
   int            iScrollY;   // scrollY in pixels, 0+
   int            iCurSel;    // current selection. Indexed into plistLBELEM. -1 => no selection
   BOOL           fSort;      // if TRUE, sort

   BOOL           fReCalc;    // if TRUE, then recalculate sizes the next chance get, by ReInterpret
   BOOL           fSorted;    // set to TRUE if the list is already sorted. Else, sort at next recalc
   CListFixed     *plistLBELEM;   // list of LBELEM. call LBELEMDelete to delete
   int            iTotalY;    // total Y height. Sum of LBELEM.iPosY
   POINT          pLastMouse; // last mouse location (in page coords)

   CEscControl    *pVScroll;      // vertical scroll control
} LISTBOX, *PLISTBOX;

static void MakeSelVisible (PCEscControl pControl, PLISTBOX pc);

/***********************************************************************
LBELEMDelete - Deletes a list box element

inputs
   PLISTBOX    pc
   DWORD dwNum - number
returns
   none
*/
static void LBELEMDelete (PLISTBOX pc, DWORD dwNum)
{
   LBELEM   *pe;
   pe = (LBELEM*) pc->plistLBELEM->Get(dwNum);
   if (!pe)
      return;

   if (pe->pTB)
      delete pe->pTB;
   if (pe->fDeleteNode && pe->pNode)
      delete pe->pNode;

   pc->plistLBELEM->Remove (dwNum);
}

/***********************************************************************
LBELEMAdd - Adds a LBElem to the list. This also sets the pc->m_fReCalc to
   true. If you're going to be doing stuff with the element then might want
   to call RecalcIfnecessary.

inputs
   PLISTBOX       pc
   PCMMLNode      pNode - node to use
   BOOL           fDelete - If TRUE, delete the node when done. Else, leave it
   DWORD          dwInsert - Element to insert before. If >= num elemens then add
returns
   BOOL - TRIE if sccess
*/
BOOL LBELEMAdd (PLISTBOX pc, PCMMLNode pNode, BOOL fDelete, DWORD dwInsert = (DWORD)-1L)
{
   LBELEM   e;

   memset (&e, 0, sizeof(e));
   e.fDeleteNode = fDelete;
   e.pNode = pNode;
   e.pszData = pNode->AttribGet(L"data");
   e.pszName = pNode->AttribGet(L"name");

   pc->fReCalc = TRUE;

   if (dwInsert < pc->plistLBELEM->Num())
      return pc->plistLBELEM->Insert (dwInsert, &e);
   else
      return pc->plistLBELEM->Add (&e);
}


/***********************************************************************
UpdateScroll - Update the scroll bars.

inputs
   PCEscControl   pControl
   PLISTBOX       pc
*/
static void UpdateScroll (PCEscControl pControl, PLISTBOX pc)
{
   if (pc->pszVScroll && !pc->pVScroll) {
      pc->pVScroll = pControl->m_pParentPage->ControlFind (pc->pszVScroll);
      if (pc->pVScroll) {
         pc->pVScroll->AttribSet (L"min", L"0");
         pc->pVScroll->m_pParentControl = pControl;
      }
   }

   WCHAR szTemp[64];
   if (pc->pVScroll) {
      swprintf (szTemp, L"%d", pControl->m_rPosn.bottom - pControl->m_rPosn.top - 2*pc->iBorderSize);
      pc->pVScroll->AttribSet (L"page", szTemp);
      swprintf (szTemp, L"%d", pc->iTotalY);
      pc->pVScroll->AttribSet (L"max", szTemp);
      swprintf (szTemp, L"%d", pc->iScrollY);
      pc->pVScroll->AttribSet (L"pos", szTemp);
   }

}

/***********************************************************************
LBSortCallback - Callback to sort the list.
*/
int __cdecl LBSortCallback (const void *elem1, const void *elem2 )
{
   PLBELEM  p1, p2;
   p1 = (PLBELEM) elem1;
   p2 = (PLBELEM) elem2;

   // if no name then cant sort
   if (p1->pszName && !p2->pszName)
      return 1;
   if (!p1->pszName && p2->pszName)
      return -1;
   if (!p1->pszName && !p2->pszName)
      return 0;   // cant tell

   // else compare
   return _wcsicmp (p1->pszName, p2->pszName);
}

/***********************************************************************
RecalcIfNecessary - If the recalc flag is set this:
   1) Resets the flag
   1.5) Sorts if sort flag is set but not sorted
   2) Loops through all the elements
      a) If no CTextBlock then creates one from the node
      b) If CTextBlock then reinterp
      c) Adjust the size
   3) Recalculates the position of all elemens and the size
   4) Make sure not scrolled too far down
   5) Resets the scroll bar

inputs
   PCEscControl      pControl - control
   PLISTBOX          pc - list box info
*/
static void RecalcIfNecessary (PCEscControl pControl, PLISTBOX pc)
{
   if (!pc->fReCalc)
      return;
   pc->fReCalc = FALSE;

   // sort
   if (pc->fSort && !pc->fSorted && pc->plistLBELEM->Num()) {
      // remember the selection
      PLBELEM pe = (LBELEM*) pc->plistLBELEM->Get(pc->iCurSel);
      PCMMLNode   pNode = NULL;
      if (pe)
         pNode = pe->pNode;

      pc->fSorted = TRUE;
      qsort (pc->plistLBELEM->Get(0), pc->plistLBELEM->Num(),
         sizeof(LBELEM), LBSortCallback);

      // loop and find the node
      DWORD i;
      for (i = 0; i < pc->plistLBELEM->Num(); i++) {
         pe = (LBELEM*) pc->plistLBELEM->Get(i);
         if (!pe)
            continue;
         if (pe->pNode == pNode)
            break;
      }
      pc->iCurSel = (int) i;
   }

   // determine the size that should make the elements
   int   iWidth;
   iWidth = pControl->m_rPosn.right - pControl->m_rPosn.left - 2 * pc->iBorderSize - 2*pc->iLRMargin;

   // hDC
   HDC   hDC;
   hDC = pControl->m_pParentPage->m_pWindow->DCGet();

   // look through all the elements
   DWORD i;
   LBELEM   *pe;
   int   iCurY;
   iCurY = 0;
   for (i = 0; i < pc->plistLBELEM->Num(); i++) {
      pe = (LBELEM*) pc->plistLBELEM->Get(i);
      if (!pe)
         continue;

      // create or reinterpret?
      if (pe->pTB) {
         pe->pTB->m_fi = pControl->m_fi;
         pe->pTB->ReInterpret (hDC, iWidth, FALSE);
      }
      else
         pe->pTB = pControl->TextBlock (hDC, pe->pNode, iWidth, FALSE, TRUE);

      // post process
      RECT  r;
      r.left = 0;
      r.top = 0;
      r.right = iWidth + 2*pc->iLRMargin;
      r.bottom = pe->pTB->m_iCalcHeight + 2*pc->iTBMargin;
      pe->pTB->PostInterpret (pc->iLRMargin, pc->iTBMargin, &r);

      // store the new height
      pe->iHeight = r.bottom;
      pe->iPosY = iCurY;
      iCurY += pe->iHeight;
   }

   pControl->m_pParentPage->m_pWindow->DCRelease();

   // store away the new position
   pc->iTotalY = iCurY;

   // make sure scroll not too far
   int   iMax;
   iMax = pc->iTotalY - (pControl->m_rPosn.bottom - pControl->m_rPosn.top - 2*pc->iBorderSize);
   iMax = max(0,iMax);
   pc->iScrollY = min(pc->iScrollY, iMax);


   // make sure selection is within view
   MakeSelVisible (pControl, pc);

   // adjust scroll bar
   UpdateScroll (pControl, pc);
}


/***********************************************************************
PointToElem - Given a point (in page coordinates), return a list element
it's over. -1 if not over anythign

inputs
   PCEscControl   pControl
   PLISTBOX       pc
   POINT          *pPoint - pPoint
returns
   DWORD - control #. -1 if none
*/
static DWORD PointToElem (PCEscControl pControl, PLISTBOX pc, POINT *pPoint)
{
   // convert to list space
   int   iY;
   iY = pPoint->y - pControl->m_rPosn.top - pc->iBorderSize + pc->iScrollY;

   // search
   DWORD i;
   LBELEM   *pe;
   for (i = 0; i < pc->plistLBELEM->Num(); i++) {
      pe = (LBELEM*) pc->plistLBELEM->Get(i);
      if (!pe) continue;

      if ((iY >= pe->iPosY) && (iY <= pe->iPosY + pe->iHeight))
         return i;
   }

   // none
   return (DWORD) -1;
}

/***********************************************************************
IsItemVisible - Returns TRUE if the item is visible or not

inputs
   PCEscControl   pControl
   PLISTBOX       pc
   DWORD          dwItem - item
reutnrs
   BOOL - TRUE if it';s visible
*/
static BOOL IsItemVisible (PCEscControl pControl, PLISTBOX pc, DWORD dwItem)
{
   // get the selection
   LBELEM   *pe;
   pe = (LBELEM*) pc->plistLBELEM->Get (dwItem);
   if (!pe)
      return FALSE;

   // location?
   int   iTop, iBottom;
   int   iMax;
   iMax = pControl->m_rPosn.bottom - pControl->m_rPosn.top - 2*pc->iBorderSize;
   iTop = pe->iPosY - pc->iScrollY;
   iBottom = iTop + pe->iHeight;

   // does it fit?
   if ((iTop >= 0) && (iBottom <= iMax))
      return TRUE;

   return FALSE;
}

/***********************************************************************
MakeItemVisible - Make an item visible

inputs
   PCEscControl   pControl
   PLISTBOX       pc
   DWORD          dwItem - item
reutnrs
   none
*/
static void MakeItemVisible (PCEscControl pControl, PLISTBOX pc, DWORD dwItem)
{
   // if it's visible do nothing
   if (IsItemVisible(pControl, pc, dwItem))
      return;

   // get the selection
   LBELEM   *pe;
   pe = (LBELEM*) pc->plistLBELEM->Get (dwItem);
   if (!pe)
      return;

   // location?
   int   iTop, iBottom;
   int   iMax;
   iMax = pControl->m_rPosn.bottom - pControl->m_rPosn.top - 2*pc->iBorderSize;
   iTop = pe->iPosY - pc->iScrollY;
   iBottom = iTop + pe->iHeight;

   // does it fit?
   if ((iTop >= 0) && (iBottom <= iMax))
      return;

   // else scroll
   if (iBottom > iMax)
      pc->iScrollY += iBottom - iMax;
   iTop = pe->iPosY - pc->iScrollY;
   if (iTop < 0)
      pc->iScrollY += iTop;

   // change the scroll bars
   UpdateScroll (pControl, pc);

   // invalidate
   pControl->Invalidate();
}

/***********************************************************************
MakeSelVisible - Make sure the selection is visible.

inputs
   PCEscControl   pControl
   PLISTBOX       pc
reutnrs
   none
*/
static void MakeSelVisible (PCEscControl pControl, PLISTBOX pc)
{
   MakeItemVisible (pControl, pc, (DWORD)pc->iCurSel);
}

/***********************************************************************
SetCurSel - Sets the current selection.

inputs
   PCEscControl      pControl
   PLISTBOX          pc
   int               iSel - new selection'
   DWORD             dwReason - 0 = scroll with keyboard, 1 = click with mouse
returns
   none
*/
static void SetCurSel (PCEscControl pControl, PLISTBOX pc, int iSel, DWORD dwReason)
{
   if ((iSel < 0) || (iSel > (int)pc->plistLBELEM->Num()))
      return;

   pc->iCurSel = iSel;
   pControl->Invalidate();
   MakeSelVisible (pControl, pc);

   // change the scroll bars
   UpdateScroll (pControl, pc);

   // notify application
   LBELEM   *pe;
   pe = (LBELEM*) pc->plistLBELEM->Get ((DWORD) pc->iCurSel);
   ESCNLISTBOXSELCHANGE sc;
   sc.dwCurSel = (DWORD) pc->iCurSel;
   sc.pControl = pControl;
   sc.pszData = pe ? pe->pszData : NULL;
   sc.pszName = pe ? pe->pszName : NULL;
   sc.dwReason = dwReason;
   pControl->MessageToParent (ESCN_LISTBOXSELCHANGE, &sc);
}

/***********************************************************************
Control callback
*/
BOOL ControlListBox (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   LISTBOX  *pc = (LISTBOX*) pControl->m_mem.p;
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(LISTBOX));
         pc = (LISTBOX*) pControl->m_mem.p;
         memset (pc, 0, sizeof(LISTBOX));

         // init variables
         pc->iLRMargin = 4;
         pc->iTBMargin = 4;
         pc->iBorderSize = 2;
         pc->cBackground = RGB(0xe0, 0xe0, 0xff);
         pc->cBorder = RGB (0,0,0xff);
         pc->cSelection = RGB(0xff, 0xc0, 0xc0);
         pc->cSelectionBorder = RGB(0,0,0);
         pc->iScrollY = 0;
         pc->iCurSel = 0;
         pc->fSort = FALSE;

         pc->fReCalc = TRUE;
         pc->plistLBELEM = new CListFixed;
         if (pc->plistLBELEM)
            pc->plistLBELEM->Init (sizeof(LBELEM));

         // all the attributes
         pControl->AttribListAddDecimal (L"LRMargin", &pc->iLRMargin, &pc->fReCalc, TRUE);
         pControl->AttribListAddDecimal (L"TBMargin", &pc->iTBMargin, &pc->fReCalc, TRUE);
         pControl->AttribListAddDecimal (L"border", &pc->iBorderSize, &pc->fReCalc, TRUE);
         pControl->AttribListAddDecimal (L"scrolly", &pc->iScrollY, FALSE, TRUE);
         pControl->AttribListAddDecimal (L"cursel", &pc->iCurSel, FALSE, TRUE);
         pControl->AttribListAddColor (L"bordercolor", &pc->cBorder, NULL, TRUE);
         pControl->AttribListAddColor (L"color", &pc->cBackground, NULL, TRUE);
         pControl->AttribListAddString (L"vscroll", &pc->pszVScroll, NULL, TRUE);
         pControl->AttribListAddColor (L"selcolor", &pc->cSelection, NULL, TRUE);
         pControl->AttribListAddColor (L"selbordercolor", &pc->cSelectionBorder, NULL, TRUE);
         pControl->AttribListAddBOOL (L"sort", &pc->fSort, &pc->fReCalc, TRUE);

      }
      return TRUE;

   case ESCM_DESTRUCTOR:
      if (pc->plistLBELEM) {
         DWORD i, dwNum;
         dwNum = pc->plistLBELEM->Num();
         for (i = dwNum-1; i < dwNum; i--)
            LBELEMDelete (pc, i);
         delete pc->plistLBELEM;
      }

      return TRUE;

   case ESCM_INITCONTROL:
      {
         // parse own MMLNode for <elem>
         DWORD i;
         PWSTR pszContent;
         PCMMLNode   pSubNode;
         for (i = 0; i < pControl->m_pNode->ContentNum(); i++) {
            pSubNode = NULL;
            pControl->m_pNode->ContentEnum (i, &pszContent, &pSubNode);
            if (!pSubNode)
               continue;

            // see if it's named <elem>
            WCHAR *psz;
            psz = pSubNode->NameGet();
            if (!psz || _wcsicmp(psz, L"elem"))
               continue;

            // else it's a valid element
            LBELEMAdd (pc, pSubNode, FALSE);
         }

         // note that want keyboard
         pControl->m_fWantMouse = TRUE;
         pControl->m_dwWantFocus = 2;

         // add acceleartors
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = VK_UP;
         a.dwMessage = ESCM_LINEUP;
         pControl->m_listAccelFocus.Add (&a);

         a.c = VK_DOWN;
         a.dwMessage = ESCM_LINEDOWN;
         pControl->m_listAccelFocus.Add (&a);

         a.c = VK_PRIOR;
         a.dwMessage = ESCM_PAGEUP;
         pControl->m_listAccelFocus.Add (&a);

         a.c = VK_NEXT;
         a.dwMessage = ESCM_PAGEDOWN;
         pControl->m_listAccelFocus.Add (&a);

         a.c = VK_HOME;
         a.dwMessage = ESCM_HOME;
         pControl->m_listAccelFocus.Add (&a);

         a.c = VK_END;
         a.dwMessage = ESCM_END;
         pControl->m_listAccelFocus.Add (&a);

      }
      return TRUE;

   case ESCM_QUERYSIZE:
      {
         PESCMQUERYSIZE p = (PESCMQUERYSIZE) pParam;

         // if had ruthers...
         p->iWidth = p->iDisplayWidth / 3;
         p->iHeight = p->iWidth / 2;
      }
      return FALSE;  // pass onto default handler

   case ESCM_SIZE:
      // need to resize all elements
      pc->fReCalc = TRUE;
      return FALSE;  // default handler

   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;

         // recalc everything if necessary
         RecalcIfNecessary (pControl, pc);

         // paint the background
         if (pc->cBackground != (DWORD)-1) {
            HBRUSH hbr;
            hbr = CreateSolidBrush (pc->cBackground);
            FillRect (p->hDC, &p->rControlHDC, hbr);
            DeleteObject (hbr);
         }

         // paint selection
         LBELEM   *pe;
         pe = (LBELEM*) pc->plistLBELEM->Get ((DWORD) pc->iCurSel);
         if (pe) {
            RECT  r;
            HDC   hDC = p->hDC;
            r.left = p->rControlHDC.left + pc->iBorderSize + 1;
            r.right = p->rControlHDC.right - pc->iBorderSize - 2;
            r.top = p->rControlHDC.top +pc->iBorderSize + pe->iPosY - pc->iScrollY + 1;
            r.bottom = r.top + pe->iHeight - 3;

            HBRUSH   hbr;
            HPEN     hOld, hPen;
            hbr = CreateSolidBrush (pc->cSelection);
            hPen = CreatePen (PS_SOLID, 0, pc->cSelectionBorder);
            hOld = (HPEN) SelectObject (hDC, hPen);

            FillRect (hDC, &r, hbr);

            // outline
            MoveToEx (hDC, r.left, r.top, NULL);
            LineTo (hDC, r.right, r.top);
            LineTo (hDC, r.right, r.bottom);
            LineTo (hDC, r.left, r.bottom);
            LineTo (hDC, r.left, r.top);

            DeleteObject (hbr);
            SelectObject (hDC, hOld);
            DeleteObject (hPen);
         }

         // paint contents
         DWORD i;
         for (i = 0; i < pc->plistLBELEM->Num(); i++) {
            pe = (LBELEM*) pc->plistLBELEM->Get(i);
            if (!pe) continue;
            if (!pe->pTB) continue;

            // if its off the display dont draw
            if ((pe->iPosY - pc->iScrollY) > (p->rControlHDC.bottom - p->rControlHDC.top))
               continue;
            if ((pe->iPosY - pc->iScrollY + pe->iHeight) < 0)
               continue;

            // else draw
            POINT pOffset;
            pOffset.x = p->rControlHDC.left + pc->iBorderSize;
            pOffset.y = p->rControlHDC.top + pc->iBorderSize + pe->iPosY - pc->iScrollY;
            pe->pTB->Paint (p->hDC, &pOffset, &p->rControlHDC, &p->rControlScreen,  &p->rTotalScreen);
         }

         // draw border
         if (pc->iBorderSize) {
            HBRUSH hbr;
            int   iBorder = (int) pc->iBorderSize;
            hbr = CreateSolidBrush (pc->cBorder);

            // left
            RECT r;
            r = p->rControlHDC;
            r.right = r.left + iBorder;
            FillRect (p->hDC, &r, hbr);

            // right
            r = p->rControlHDC;
            r.left = r.right - iBorder;
            FillRect (p->hDC, &r, hbr);

            // top
            r = p->rControlHDC;
            r.bottom = r.top + iBorder;
            FillRect (p->hDC, &r, hbr);

            // bottom
            r = p->rControlHDC;
            r.top = r.bottom - iBorder;
            FillRect (p->hDC, &r, hbr);

            DeleteObject (hbr);
         }
      }
      return TRUE;

   case ESCM_PAINTMOUSEOVER:
      {
         ESCMPAINT *p = (ESCMPAINT*) pParam;

         DWORD dwElem;
         PLBELEM  pe;
         dwElem = PointToElem (pControl, pc, &pc->pLastMouse);
         pe = (LBELEM*) pc->plistLBELEM->Get(dwElem);
#define MOUSEOVERSIZE   2  
         if (pe) {
            HPEN hPen, hPenOld;
            HBRUSH hBrush;
            hPen = CreatePen (PS_SOLID, MOUSEOVERSIZE, RGB(255, 0, 0));
            hPenOld = (HPEN) SelectObject (p->hDC, hPen);
            hBrush = (HBRUSH) SelectObject (p->hDC, GetStockObject(NULL_BRUSH));

            RECT  r;
            r.left = p->rControlHDC.left + pc->iBorderSize + 1;
            r.right = p->rControlHDC.right - pc->iBorderSize - 2;
            r.top = p->rControlHDC.top +pc->iBorderSize + pe->iPosY - pc->iScrollY + 1;
            r.bottom = r.top + pe->iHeight - 2;

            RoundRect (p->hDC,
               r.left+1, r.top+1 ,
               r.right, r.bottom,
               MOUSEOVERSIZE * 4, MOUSEOVERSIZE * 4);

            SelectObject (p->hDC, hPenOld);
            SelectObject (p->hDC, hBrush);
            DeleteObject (hPen);
         }
         
      }
      return TRUE;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         if (p->pControl == pc->pVScroll) {
            pc->iScrollY = p->iPos;
            pControl->Invalidate();
         }
      }
      return TRUE;

   case ESCM_LBUTTONDOWN:
      {
         PESCMLBUTTONDOWN p = (PESCMLBUTTONDOWN) pParam;

         // find out what clicked on
         DWORD dwElem;
         dwElem = PointToElem (pControl, pc, &p->pPosn);
         
         // beep depending upon if clicked on element or not
         pControl->m_pParentPage->m_pWindow->Beep (
            (dwElem != (DWORD)-1) ? ESCBEEP_RADIOCLICK : ESCBEEP_DONTCLICK);

         if (dwElem != (DWORD)-1) {
            SetCurSel (pControl, pc, (int) dwElem, 1);
         }
      }
      return TRUE;

   case ESCM_MOUSEMOVE:
      {
         PESCMMOUSEMOVE p = (PESCMMOUSEMOVE) pParam;

         // remember the last mouse position and invalidate so can redraw
         // red-outline over text
         pc->pLastMouse = p->pPosn;
         pControl->Invalidate ();

      }
      return TRUE;

   case ESCM_MOUSEWHEEL:
      {
         PESCMMOUSEWHEEL p = (PESCMMOUSEWHEEL) pParam;
         RecalcIfNecessary (pControl, pc);

         // BUGFIX - If list box has focus then scroll

         if (p->zDelta < 0) {
            DWORD i;
            BOOL fFoundVisible, f;
            fFoundVisible = FALSE;
            for (i = 0; i < pc->plistLBELEM->Num(); i++) {
               f = IsItemVisible (pControl, pc, i);
               if (f)
                  fFoundVisible = TRUE;
               else if (fFoundVisible)
                  break; // found the first non-visible one after the visible ones
            }
            if (i >= pc->plistLBELEM->Num())
               break;   // shouldn't happen

            // scroll to that one
            MakeItemVisible (pControl, pc, i);
         }
         else {
            // find the first visible item
            DWORD i;
            for (i = 0; i < pc->plistLBELEM->Num(); i++)
               if (IsItemVisible (pControl, pc, i))
                  break;
            if (i >= pc->plistLBELEM->Num())
               break;   // shouldn't happen

            // scroll up one
            if (i)
               MakeItemVisible (pControl, pc, i-1);
         }

      }
      return TRUE;

   case ESCM_LINEUP:
      RecalcIfNecessary (pControl, pc);

      // dont go beyond top
      if (pc->iCurSel <= 0) {
         pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
         return TRUE;
      }

      SetCurSel (pControl, pc, pc->iCurSel - 1, 0);

      return TRUE;

   case ESCM_LINEDOWN:
      RecalcIfNecessary (pControl, pc);

      // dont go beyond top
      if (pc->iCurSel+1 >= (int) pc->plistLBELEM->Num()) {
         pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
         return TRUE;
      }

      SetCurSel (pControl, pc, pc->iCurSel + 1, 0);
      return TRUE;

   case ESCM_PAGEUP:
   case ESCM_PAGEDOWN:
      {
         RecalcIfNecessary (pControl, pc);

         // if page up/down and already at the beginning/end then beep
         if ((dwMessage == ESCM_PAGEUP) && (pc->iCurSel == 0)) {
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return TRUE;
         }
         if ((dwMessage == ESCM_PAGEDOWN) && (pc->iCurSel == (int)pc->plistLBELEM->Num()-1)) {
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return TRUE;
         }

         // look about 1 page's worth up and see what control is there
         LBELEM   *pe;
         int   iY;
         pe = (LBELEM*) pc->plistLBELEM->Get(pc->iCurSel);
         iY = pe ? pe->iPosY : 0;   // nothing

         // approximate a page up/down
         int   iHeight;
         iHeight = pControl->m_rPosn.bottom - pControl->m_rPosn.top - 2*pc->iBorderSize - 2*pc->iTBMargin;
         iHeight = max(1, iHeight);

         if (dwMessage == ESCM_PAGEUP)
            iY -= iHeight;
         else
            iY += iHeight;

         // find what's under
         POINT point;
         DWORD dwElem;
         point.x = 0;
         point.y = iY + pControl->m_rPosn.top + pc->iBorderSize - pc->iScrollY;
         dwElem = PointToElem (pControl, pc, &point);
         if (dwElem == (DWORD)-1) {
            // max at top/bottom
            dwElem = (dwMessage == ESCM_PAGEUP) ? 0 : (pc->plistLBELEM->Num()-1);
         }

         // set the selection
         SetCurSel (pControl, pc, (int) dwElem, 0);
      }
      return TRUE;


   case ESCM_HOME:
      RecalcIfNecessary (pControl, pc);
      SetCurSel (pControl, pc, 0, 0);
      return TRUE;

   case ESCM_END:
      RecalcIfNecessary (pControl, pc);
      SetCurSel (pControl, pc, pc->plistLBELEM->Num()-1, 0);
      return TRUE;

   // BUGBUG - 2.0 - At some point ESCM_CHAR - type in text


   case ESCM_LISTBOXADD:
      {
         ESCMLISTBOXADD *p = (ESCMLISTBOXADD*) pParam;
         // BUGFIX - Take out RecalcIfNecessary so it's not so slow adding
         // groups of elements at once
         // RecalcIfNecessary (pControl, pc);

         // no longer sorted
         pc->fSorted = FALSE;
         pc->fReCalc = TRUE;

         PCMMLNode   pNode;
         if (p->pNode) {
            pNode = p->pNode->Clone();
            if (!pNode)
               return FALSE;
         }
         else if (p->pszMML) {
            CEscError   err;
            pNode = ParseMML (p->pszMML, pControl->m_hInstance, NULL, NULL, &err);
            if (!pNode)
               return FALSE;

            // loop through all the sub-nodes
            DWORD    dwNum, i;
            dwNum = pNode->ContentNum();
            // BUGFIX - If dwInsertBefore was -1, was added them in the reverse direction
            for (i = (p->dwInsertBefore == (DWORD)-1) ? 0 : dwNum-1; i < dwNum; i = i + ((p->dwInsertBefore == (DWORD)-1) ? 1 : -1)) {
               PWSTR psz;
               PCMMLNode   pSubNode;
               pSubNode = NULL;
               pNode->ContentEnum (i, &psz, &pSubNode);
               if (!pSubNode) continue;

               // have sub node. Delete it from the main node so it doesn't get deleted
               // and call back into seld
               // BUGFIX - Do a clone instead of this remove because problems
               // with new/delete across EXE/DLL
               // pNode->ContentRemove (i, FALSE);

               // call back
               ESCMLISTBOXADD add;
               add = *p;
               add.pNode = pSubNode;
               add.pszMML = NULL;
               pControl->Message (dwMessage, &add);
            }

            delete pNode;

            // invalid
            pControl->Invalidate();
            return TRUE;
         }
         else {
            // just passed in text so make up own node
            pNode = new CMMLNode;
            pNode->NameSet (L"elem");
            pNode->ContentAdd (p->pszText);
            pNode->AttribSet (L"name", p->pszText);
            pNode->AttribSet (L"data", p->pszText);
         }

         // if no node done
         if (!pNode)
            return FALSE;

         // else, add it
         LBELEMAdd (pc, pNode, TRUE, p->dwInsertBefore);

         // invalidate
         pControl->Invalidate();
      }
      return TRUE;



   case ESCM_LISTBOXDELETE:
      {
         ESCMLISTBOXDELETE *p = (ESCMLISTBOXDELETE*) pParam;
         RecalcIfNecessary (pControl, pc);

         // will need to recalc height & stuff
         pc->fReCalc = TRUE;

         // delete it
         LBELEMDelete (pc, p->dwIndex);

         // invalidate
         pControl->Invalidate();
      }
      return TRUE;




   case ESCM_LISTBOXFINDSTRING:
      {
         ESCMLISTBOXFINDSTRING *p = (ESCMLISTBOXFINDSTRING*) pParam;
         RecalcIfNecessary (pControl, pc);

         // loop
         DWORD i, dwNum;
         dwNum = pc->plistLBELEM->Num();
         if (!dwNum) {
            p->dwIndex = (DWORD)-1;
            return TRUE;
         }

         DWORD dwCur;
         PLBELEM  pe;
         for (i = 0; i < dwNum; i++) {
            // really look at
            dwCur = (DWORD) (p->iStart + 1 + (int) i) % dwNum;

            // get it
            pe = (PLBELEM) pc->plistLBELEM->Get(dwCur);
            if (!pe) continue;

            // if no string continue
            if (!pe->pszName) continue;

            // does it match
            if (p->fExact && !_wcsicmp(pe->pszName, p->psz)) {
               p->dwIndex = dwCur;
               return TRUE;
            }
            else if (!p->fExact && wcsstr(pe->pszName, p->psz)) {
               p->dwIndex = dwCur;
               return TRUE;
            }
         }

         // else no match
         p->dwIndex = (DWORD)-1;
      }
      return TRUE;



   case ESCM_LISTBOXGETCOUNT:
      {
         ESCMLISTBOXGETCOUNT *p = (ESCMLISTBOXGETCOUNT*) pParam;
         RecalcIfNecessary (pControl, pc);

         p->dwNum = pc->plistLBELEM->Num();
      }
      return TRUE;





   case ESCM_LISTBOXGETITEM:
      {
         ESCMLISTBOXGETITEM *p = (ESCMLISTBOXGETITEM*) pParam;
         RecalcIfNecessary (pControl, pc);

         PLBELEM  pe;
         pe = (PLBELEM) pc->plistLBELEM->Get(p->dwIndex);
         if (!pe) {
            p->pszData = p->pszName = NULL;
            memset (&p->rPage, 0, sizeof(p->rPage));
            return TRUE;
         }

         // else
         p->pszData = pe->pszData;
         p->pszName = pe->pszName;
         p->rPage.left = pControl->m_rPosn.left + pc->iBorderSize;
         p->rPage.right = pControl->m_rPosn.right - pc->iBorderSize;
         p->rPage.top = pControl->m_rPosn.top + pc->iBorderSize - pc->iScrollY + pe->iPosY;
         p->rPage.bottom = p->rPage.top + pe->iHeight;
      }
      return TRUE;







   case ESCM_LISTBOXITEMFROMPOINT:
      {
         ESCMLISTBOXITEMFROMPOINT *p = (ESCMLISTBOXITEMFROMPOINT*) pParam;
         RecalcIfNecessary (pControl, pc);

         p->dwIndex = PointToElem (pControl, pc, &p->pPoint);
      }
      return TRUE;


   case ESCM_LISTBOXRESETCONTENT:
      {
         // delete all
         DWORD i, dwNum;
         dwNum = pc->plistLBELEM->Num();
         for (i = dwNum-1; i < dwNum; i--)
            LBELEMDelete (pc, i);
         // BUGFIX - take out
         //delete pc->plistLBELEM;

         // invalidate
         pc->fReCalc = TRUE;
         pControl->Invalidate();
      }
      return TRUE;


   case ESCM_LISTBOXSELECTSTRING:
      {
         ESCMLISTBOXSELECTSTRING *p = (ESCMLISTBOXSELECTSTRING*) pParam;
         RecalcIfNecessary (pControl, pc);

         pControl->Message (ESCM_LISTBOXFINDSTRING, pParam);

         if (p->dwIndex != (DWORD)-1) {
            SetCurSel (pControl, pc, (int) p->dwIndex, 0);
         }
      }
      return TRUE;


   }

   return FALSE;
}


// BUGBUG - 2.0 - when do hover help, consider bringingup help when hover over a speicifc item
