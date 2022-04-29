/***********************************************************************
ControlComboBox.cpp - Code for a control

begun 4/18/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "escarpment.h"
#include "control.h"
#include "resleak.h"

typedef struct {
   PCMMLNode         pNode;   // node <elem> that created this
   BOOL              fDeleteNode;   // if TRUE, delete pNode when this is deleted
   PWSTR             pszName; // pointer to name. may be NULL
   PWSTR             pszData; // pointer to data. may be NULL
} LBELEM, *PLBELEM;


/*********************************************************************
DropDownFitToScreen - Make sure the drop-down is on the screen.

BUGFIX - This is put in to fix a bug

inputs
   PCEscPage pPage - init page
returns
   none
*/
void DropDownFitToScreen (PCEscPage pPage)
{
   PCEscWindow pWindow = pPage->m_pWindow;

   RECT rWork;
   SystemParametersInfo (SPI_GETWORKAREA, 0, &rWork, 0);
   // BUGBUG - This won't work properly if the application is on the second window

   RECT r;
   int   iScrollX, iScrollY;
   pWindow->PosnGet(&r);
   iScrollX = iScrollY = 0;
   if (r.left < rWork.left)
      iScrollX = rWork.left-r.left;
   else if (r.right > rWork.right)
      iScrollX = rWork.right - r.right;
   if (r.top < rWork.top)
      iScrollY = rWork.top-r.top;
   else if (r.bottom > rWork.bottom)
      iScrollY = rWork.bottom - r.bottom;

   // if scroll do it
   if (iScrollX || iScrollY)
      pWindow->Move (r.left + iScrollX, r.top + iScrollY);
}

/***********************************************************************
DropDownMakeTextBlock - Makes sure pc->dd.pTextBlock is the currently seleected
element. If it isn't then it generates it. It sends a ESCM_DROPDOWNTEXTBLOCK
to get the contents of the text block.

inputs
   PCEscControl   pControl
   PDROPDOWN      pdd
   int            iWidth - if have to create/reinterpret, the number of pixels avail
   RECT           *pFull - For post processing (if necssary). If NULL then no post interpret
   BOOL           fReinterpret - Even if there, reinterpret
*/
static void DropDownMakeTextBlock (PCEscControl pControl, PDROPDOWN pdd, int iWidth, RECT *pFull, BOOL fReinterpret)
{
   // see what wants for node
   ESCMDROPDOWNTEXTBLOCK p;
   memset (&p, 0, sizeof(p));
   p.fMustSet = pdd->pTextBlock ? FALSE : TRUE;
   pControl->Message (ESCM_DROPDOWNTEXTBLOCK, &p);

   // if it's empty and didn't need to set then return
   if (!p.pNode && !p.pszMML && !p.pszText && pdd->pTextBlock && !fReinterpret)
      return;

   HDC   hDC;
   hDC = pControl->m_pParentPage->m_pWindow->DCGet();

   // see if have to create a new one
   BOOL  fPostInterpret;
   fPostInterpret = FALSE;
   if (!pdd->pTextBlock || p.pNode || p.pszMML || p.pszText) {
      if (pdd->pTextBlock)
         delete pdd->pTextBlock;

      if (p.pszText) {
         p.pNode = new CMMLNode;
         p.pNode->NameSet (L"null");
         p.pNode->ContentAdd (p.pszText);
         p.fDeleteNode = TRUE;
      }
      else if (p.pszMML) {
         CEscError   err;
         p.pNode = ParseMML (p.pszMML, pControl->m_hInstance, NULL, NULL, &err);
         p.fDeleteNode = TRUE;
      }

      // goto done if error
      if (!p.pNode)
         goto done;

      pdd->pTextBlock = pControl->TextBlock (hDC, p.pNode, iWidth, p.fDeleteNode, TRUE);
      if (!pdd->pTextBlock)
         goto done;


      // won't need to reinterpret
      fReinterpret = FALSE;
      fPostInterpret = TRUE;
   }

   if (pdd->pTextBlock && fReinterpret) {
      pdd->pTextBlock->m_fi = pControl->m_fi;
      pdd->pTextBlock->ReInterpret (hDC, iWidth, TRUE);
      fPostInterpret = TRUE;
   }

   if (pFull && fPostInterpret) {
      // postinterpret, moving the text to the right of the butotn.
      int   iVCenter;
      iVCenter = 0;
      if (!wcsicmp (pdd->pszVCenter, L"top"))
         iVCenter = -1;
      else if (!wcsicmp (pdd->pszVCenter, L"bottom"))
         iVCenter = 1;

      int   iDeltaY;
      if (iVCenter < 0)
         iDeltaY = pdd->iMarginTopBottom;
      else if (iVCenter > 0)
         iDeltaY = (pFull->bottom - pFull->top) - pdd->pTextBlock->m_iCalcHeight - pdd->iMarginTopBottom;
      else
         iDeltaY = (pFull->bottom - pFull->top) / 2 - (pdd->pTextBlock->m_iCalcHeight)/2;
      if (pdd->pTextBlock)
         pdd->pTextBlock->PostInterpret (pdd->iMarginLeftRight, iDeltaY, pFull);
   }

done:
   pControl->m_pParentPage->m_pWindow->DCRelease();
}



typedef struct {
   DROPDOWN       dd;
   int            iBorderSize;    // border width
   COLORREF       cBorder;    // border color, default blue
   COLORREF       cSelection; // color of the selection
   COLORREF       cSelectionBorder; // color of the selection border
   int            iLRMargin;  // left right margin
   int            iTBMargin;  // top bottom margin

   int            iCurSel;    // current selection. Indexed into plistLBELEM. -1 => no selection
   BOOL           fSort;      // if TRUE, sort

   BOOL           fReCalc;    // if TRUE, then recalculate sizes the next chance get, by ReInterpret
   BOOL           fSorted;    // set to TRUE if the list is already sorted. Else, sort at next recalc
   CListFixed     *plistLBELEM;   // list of LBELEM. call LBELEMDelete to delete

   PCMMLNode      pNodeTextBlock;   // node used for the text block
} COMBOBOX, *PCOMBOBOX;

#define  ESCM_PRESSED      (ESCM_USER+1)
#define  ESCM_KEYOPEN      (ESCM_USER+2)
#define  ESCM_ENTER        (ESCM_USER+3)

/* RIGHTSIDE - Returns the number of pixels to the right ot he text. This is generally
   the size of the ComboBoxs + margins text */
#define  RIGHTSIDE    ((pdd->fShowButton ? (pdd->iButtonWidth + pdd->iMarginLeftRight) : 0) + pdd->iMarginComboBoxText)


#define  USECOMBOBOXLIGHT pdd->cLight
#define  USECOMBOBOXCOLOR2 pdd->cButton
#define  USECOMBOBOXCOLOR3 (fDrawDown ? USECOMBOBOXLIGHT : USECOMBOBOXCOLOR2)
#define  USECOMBOBOXCOLOR (pControl->m_fEnabled ? USECOMBOBOXCOLOR3 : USEBASECOLOR)
#define  USEBASECOLOR pdd->cButtonBase

/***********************************************************************
LBELEMDelete - Deletes a list box element

inputs
   PCOMBOBOX    pc
   DWORD dwNum - number
returns
   none
*/
static void LBELEMDelete (PCOMBOBOX pc, DWORD dwNum)
{
   LBELEM   *pe;
   pe = (LBELEM*) pc->plistLBELEM->Get(dwNum);
   if (!pe)
      return;

   if (pe->fDeleteNode && pe->pNode)
      delete pe->pNode;

   pc->plistLBELEM->Remove (dwNum);
}

/***********************************************************************
LBELEMAdd - Adds a LBElem to the list. This also sets the pc->m_fReCalc to
   true. If you're going to be doing stuff with the element then might want
   to call RecalcIfnecessary.

inputs
   PCOMBOBOX       pc
   PCMMLNode      pNode - node to use
   BOOL           fDelete - If TRUE, delete the node when done. Else, leave it
   DWORD          dwInsert - Element to insert before. If >= num elemens then add
returns
   BOOL - TRIE if sccess
*/
BOOL LBELEMAdd (PCOMBOBOX pc, PCMMLNode pNode, BOOL fDelete, DWORD dwInsert = (DWORD)-1L)
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
LBSortCallback - Callback to sort the list.
*/
static int __cdecl LBSortCallback (const void *elem1, const void *elem2 )
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
   return wcsicmp (p1->pszName, p2->pszName);
}

/***********************************************************************
RecalcIfNecessary - If the recalc flag is set this:
   1) Resets the flag
   1.5) Sorts if sort flag is set but not sorted

inputs
   PCEscControl      pControl - control
   PCOMBOBOX          pc - list box info
*/
static void RecalcIfNecessary (PCEscControl pControl, PCOMBOBOX pc)
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

}

/***********************************************************************
SetCurSel - Sets the current selection.

inputs
   PCEscControl      pControl
   PCOMBOBOX          pc
   int               iSel - new selection
returns
   none
*/
static void SetCurSel (PCEscControl pControl, PCOMBOBOX pc, int iSel)
{
   if ((iSel < 0) || (iSel > (int)pc->plistLBELEM->Num()))
      return;

   pc->iCurSel = iSel;
   pControl->Invalidate();

   // notify application
   LBELEM   *pe;
   pe = (LBELEM*) pc->plistLBELEM->Get ((DWORD) pc->iCurSel);
   ESCNCOMBOBOXSELCHANGE sc;
   sc.dwCurSel = (DWORD) pc->iCurSel;
   sc.pControl = pControl;
   sc.pszData = pe ? pe->pszData : NULL;
   sc.pszName = pe ? pe->pszName : NULL;
   pControl->MessageToParent (ESCN_COMBOBOXSELCHANGE, &sc);
}



/***********************************************************************
Page callback
*/
BOOL ComboBoxCallback (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      ESCACCELERATOR a;
      memset (&a, 0, sizeof(a));
      a.c = VK_ESCAPE;
      a.dwMessage = ESCM_CLOSE;
      pPage->m_listESCACCELERATOR.Add (&a);
      a.c = L' ';
      a.dwMessage = ESCM_ENTER;
      pPage->m_listESCACCELERATOR.Add (&a);
      a.c = VK_RETURN;
      pPage->m_listESCACCELERATOR.Add (&a);

      DropDownFitToScreen(pPage);

      // set the focus to the first control
      if (pPage->m_pUserData)
         pPage->FocusToNextControl ();
      return TRUE;

   case ESCN_LISTBOXSELCHANGE:
      {
         ESCNLISTBOXSELCHANGE *p = (ESCNLISTBOXSELCHANGE*) pParam;

         // if its a mouse click then exit. Elase ignore
         if (p->dwReason == 1) {
            WCHAR sz[16];
            swprintf (sz, L"@%d", p->dwCurSel);
            pPage->Link(sz);
         }
      }
      return TRUE;

   case ESCM_ENTER:
      {
         WCHAR sz[16];
         PCEscControl   pControl;
         pControl = pPage->ControlFind (L"listbox");
         if (!pControl)
            return FALSE;
         DWORD dwNeeded;
         sz[0] = L'@';
         sz[1] = 0;
         pControl->AttribGet (L"cursel", sz + 1, sizeof(sz)-2, &dwNeeded);
         pPage->Link(sz);
      }
      return TRUE;
   }

   return FALSE;
}


/***********************************************************************
DropDownMessageHandler - Have you default message proc call this after
   you've handled messages yourself, or have ignored them. Escpecially
   for the messages handled by this callback
*/
BOOL DropDownMessageHandler (PCEscControl pControl, DWORD dwMessage, PVOID pParam, PDROPDOWN pdd)
{
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         memset (pdd, 0, sizeof(DROPDOWN));
         pdd->iWidth = pdd->iHeight = 0;   // so know if it's set
         pdd->cButton = RGB(0xa0,0x70,0xff); // RGB(0x70,0x70,0xff);
         pdd->cButtonBase = RGB(0x70, 0x70, 0x70);
         pdd->cLight = RGB(0xff, 0x40, 0x40);
         pdd->iButtonWidth = pdd->iButtonHeight = 16; // 24;
         pdd->iButtonDepth = pdd->iButtonWidth * 2 / 3;
         pdd->iMarginTopBottom = pdd->iMarginLeftRight= pdd->iMarginComboBoxText = 4;
         pdd->fShowButton = TRUE;
         pdd->pszVCenter = L"center";
         pdd->pRender = new CRender;
         pdd->iCBHeight = -1;
         pdd->iCBWidth = -1;
         pdd->cTBackground = RGB(0xf0, 0xff, 0xf0);
         pdd->cBBackground = RGB(0xc0, 0xcf, 0xc0);
         pControl->AttribListAddDecimalOrPercent (L"width", &pdd->iWidth, &pdd->fWidthPercent);
         pControl->AttribListAddDecimalOrPercent (L"height", &pdd->iHeight, &pdd->fHeightPercent);
         pControl->AttribListAddString (L"style", &pdd->pszStyle, &pdd->fRecalcText, TRUE);
         pControl->AttribListAddString (L"appear", &pdd->pszAppear, &pdd->fRecalcText, TRUE);
         pControl->AttribListAddColor (L"color", &pdd->cButton, FALSE, TRUE);
         pControl->AttribListAddColor (L"basecolor", &pdd->cButtonBase, FALSE, TRUE);
         pControl->AttribListAddColor (L"lightcolor", &pdd->cLight, FALSE, TRUE);
         pControl->AttribListAddBOOL (L"showbutton", &pdd->fShowButton, &pdd->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"buttonwidth", &pdd->iButtonWidth, &pdd->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"buttonheight", &pdd->iButtonHeight, &pdd->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"buttondepth", &pdd->iButtonDepth, &pdd->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"margintopbottom", &pdd->iMarginTopBottom, &pdd->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"marginleftright", &pdd->iMarginLeftRight, &pdd->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"marginbuttonText", &pdd->iMarginComboBoxText, &pdd->fRecalcText, TRUE);
         pControl->AttribListAddString (L"valign", &pdd->pszVCenter, &pdd->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"cbheight", &pdd->iCBHeight, NULL, FALSE);
         pControl->AttribListAddDecimal (L"cbwidth", &pdd->iCBWidth, NULL, FALSE);
         pControl->AttribListAddColor (L"tcolor", &pdd->cTBackground, NULL, FALSE);
         pControl->AttribListAddColor (L"bcolor", &pdd->cBBackground, NULL, FALSE);
      }
      return TRUE;


   case ESCM_INITCONTROL:
      {
         // add the acclerators
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = L' ';
         a.dwMessage = ESCM_KEYOPEN;
         pControl->m_listAccelFocus.Add (&a);
         a.c = L'\n';
         pControl->m_listAccelFocus.Add (&a);
         a.c = (pdd->pszAppear && !wcsicmp(pdd->pszAppear, L"right")) ? VK_RIGHT : VK_DOWN;
         pControl->m_listAccelFocus.Add (&a);

         pControl->m_fWantMouse = TRUE;
         pControl->m_dwWantFocus = 1;

         // indicate that we want to be redrawn on a mov
         pControl->m_fRedrawOnMove = TRUE;
      }
      return TRUE;

   case ESCM_SWITCHACCEL:
      // bring down menu
      pControl->Message (ESCM_KEYOPEN);
      return FALSE;


      // BUGFIX - The drop-down on mouse enter is WAY too annoying so disable
#if 0
   case ESCM_MOUSEENTER:
      // invalidate
      pControl->Invalidate();

      // call "pressed" to bring down ComboBox when enters
      pControl->Message (ESCM_PRESSED);
      return TRUE;
#endif // 0

   case ESCM_DESTRUCTOR:
      if (pdd->pRender)
         delete pdd->pRender;
      if (pdd->pTextBlock)
         delete pdd->pTextBlock;
      return TRUE;

   case ESCM_QUERYSIZE:
      {
         ESCMQUERYSIZE  *p = (ESCMQUERYSIZE*) pParam;

         int   iVCenter = 0;
         if (!wcsicmp (pdd->pszVCenter, L"top"))
            iVCenter = -1;
         else if (!wcsicmp (pdd->pszVCenter, L"bottom"))
            iVCenter = 1;

         // see how large is given
         POINT pGiven;
         pGiven.x = pdd->fWidthPercent ? (pdd->iWidth * p->iDisplayWidth / 100) : pdd->iWidth;
         pGiven.y = pdd->fHeightPercent ? (pdd->iHeight * p->iDisplayWidth / 100) : pdd->iHeight;

         // what size do we try to calculate for
         POINT pCalc;
         pCalc = pGiven;
         if (!pCalc.x)
            pCalc.x = p->iDisplayWidth;
         if (!pCalc.y)
            pCalc.y = p->iDisplayWidth / 2;   // dont make ComboBox too tall
         pCalc.x -= (RIGHTSIDE + pdd->iMarginLeftRight);
         pCalc.y -= 2 * pdd->iMarginTopBottom;

         DropDownMakeTextBlock (pControl, pdd, pCalc.x, NULL, TRUE);
         if (pdd->pTextBlock) {
            // set the size we want
            p->iWidth = pGiven.x ? pGiven.x : (pdd->pTextBlock->m_iCalcWidth + RIGHTSIDE + pdd->iMarginLeftRight);
            p->iHeight = pGiven.y ? pGiven.y : (max(pdd->fShowButton ? pdd->iButtonHeight : 0, pdd->pTextBlock->m_iCalcHeight) + 2*pdd->iMarginTopBottom );
         }
         else {
            // set the size we want
            p->iWidth = pGiven.x ? pGiven.x : (RIGHTSIDE + pdd->iMarginLeftRight);
            p->iHeight = pGiven.y ? pGiven.y : ((pdd->fShowButton ? pdd->iButtonHeight : 0) + 2*pdd->iMarginTopBottom );
         }
         pdd->fRecalcText = FALSE;

      }
      return TRUE;

   case ESCM_SIZE:
      {
         RECT  rFull;
         rFull.left = rFull.top = 0;
         rFull.right = pControl->m_rPosn.right - pControl->m_rPosn.left;
         rFull.bottom = pControl->m_rPosn.bottom - pControl->m_rPosn.top;

         // reinterpret
         DropDownMakeTextBlock (pControl, pdd, rFull.right - (RIGHTSIDE + pdd->iMarginLeftRight), &rFull, TRUE);
      }
      return FALSE;  // default handler too

   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;

         BOOL  fDrawDown;
         fDrawDown = pControl->m_fMouseOver;

         int   iVCenter = 0;
         if (!wcsicmp (pdd->pszVCenter, L"top"))
            iVCenter = -1;
         else if (!wcsicmp (pdd->pszVCenter, L"bottom"))
            iVCenter = 1;

         // recacalculate the text block in case the selection has changed
         RECT  rFull;
         rFull.left = rFull.top = 0;
         rFull.right = pControl->m_rPosn.right - pControl->m_rPosn.left;
         rFull.bottom = pControl->m_rPosn.bottom - pControl->m_rPosn.top;

         // reinterpret
         DropDownMakeTextBlock (pControl, pdd, rFull.right - (RIGHTSIDE + pdd->iMarginLeftRight), &rFull, FALSE);

         // draw the text
         if (pdd->pTextBlock) {
            POINT pOffset;
            pOffset.x = p->rControlHDC.left;
            pOffset.y = p->rControlHDC.top;

            pdd->pTextBlock->Paint (p->hDC, &pOffset, &p->rControlHDC, &p->rControlScreen,
               &p->rTotalScreen);
         }

         // paint the button
         if (pdd->fShowButton) {
            RECT  r;
            if (iVCenter < 0)
               r.top = p->rControlHDC.top;
            else if (iVCenter > 0)
               r.top = p->rControlHDC.bottom - 2*pdd->iMarginTopBottom - pdd->iButtonHeight;
            else
               r.top = (p->rControlHDC.bottom + p->rControlHDC.top) / 2 - pdd->iButtonHeight / 2 - pdd->iMarginTopBottom;
            r.right = p->rControlHDC.right;
            r.left = r.right - (pdd->iButtonWidth + 2*pdd->iMarginLeftRight);
            r.bottom = r.top + pdd->iButtonHeight + 2*pdd->iMarginTopBottom;

            // figure out where this will be on the screen
            RECT  rScreen;
            rScreen = r;
            OffsetRect (&rScreen, p->rControlScreen.left - p->rControlHDC.left,
               p->rControlScreen.top - p->rControlHDC.top);
            pdd->pRender->InitForControl (p->hDC, &r, &rScreen, &p->rTotalScreen);
            // note negative translation
            pdd->pRender->Translate ((r.right - r.left) / 2, -(r.bottom - r.top)/2);

            // scale so that a -1,-1,0 to 1,1,1 object is the right size
            pdd->pRender->Scale (pdd->iButtonWidth / 2, pdd->iButtonHeight / 2, pdd->iButtonDepth);

            Draw3DButton (pdd->pRender, USEBASECOLOR, USECOMBOBOXCOLOR, pdd->pszStyle,
               (pdd->pszAppear && !wcsicmp(pdd->pszAppear, L"right")) ? L"rightarrow" : L"downarrow", fDrawDown);

            pdd->pRender->Commit (p->hDC, &r);
         }

      }
      return TRUE;



   case ESCM_LBUTTONDOWN:
      // BUGFIX - Allow to click on combobox so can press instead of hover

      // must release capture or bad things happen
      pControl->m_pParentPage->MouseCaptureRelease(pControl);

      // do pressed so it clses when move off
      pControl->Message (ESCM_PRESSED);

#if 0
      // beep
      pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
#endif //0
      return TRUE;

   case ESCM_KEYOPEN:
   case ESCM_PRESSED:
      {
         CEscWindow  cWindow;
         // beep when enter
         pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_MENUOPEN);

         // if move over then create a window underneath using the ComboBoxcontents
         // subnode

         // size?
         RECT  rSize;
         pControl->m_pParentPage->CoordPageToScreen (&pControl->m_rPosn, &rSize);

         DWORD dwStyle;
         dwStyle = EWS_NOTITLE | EWS_FIXEDSIZE | EWS_NOVSCROLL | EWS_AUTOHEIGHT;

         // if open by the mouse, close by the mouse
         if (dwMessage == ESCM_PRESSED)
            dwStyle |= EWS_CLOSENOMOUSE;

         // can set ComboBox to appear to the right also
         BOOL  fRight;
         fRight = FALSE;
         if (pdd->pszAppear && !wcsicmp(pdd->pszAppear, L"right")) {
            int   iX;
            iX = rSize.right - rSize.left;
            rSize.right += iX;
            rSize.left += iX;
            fRight = TRUE;
         }

         // make width at least 1/6 of screen
         int   iWidth, iHeight;
         iWidth = rSize.right - rSize.left;
         iWidth = max(iWidth, GetSystemMetrics (SM_CXSCREEN) / 6);
         rSize.right = rSize.left + 2; // it'll resize
         if (pdd->iCBWidth > 0)
            iWidth = pdd->iCBWidth;
         if (pdd->iCBHeight > 0)
            iHeight = pdd->iCBHeight;
         else
            iHeight = iWidth;
         // automatically use autowidth to include scroll bar & stuff
         dwStyle |= EWS_AUTOWIDTH;


         if (!fRight)
            rSize.top = rSize.bottom;  // move immediately below ComboBox
         rSize.bottom = rSize.top + 1; // since will automatically be resized

         if (!cWindow.Init(pControl->m_hInstance, pControl->m_pParentPage->m_pWindow->m_hWnd,
            dwStyle, &rSize))
            return TRUE;

         // specify that is mose moves off window (except for where control
         // is located) that should shut down
         pControl->m_pParentPage->CoordPageToScreen (&pControl->m_rPosn, &cWindow.m_rMouseMove);

         // call into main control
         ESCMDROPDOWNOPENED m;
         memset (&m, 0, sizeof(m));
         m.pWindow = &cWindow;
         m.iHeight = iHeight;
         m.iWidth = iWidth;
         m.fKeyOpen = (dwMessage == ESCM_KEYOPEN);
         pControl->Message (ESCM_DROPDOWNOPENED, &m);
      }
      return TRUE;
   }

   return FALSE;
}

/***********************************************************************
Control callback
*/
BOOL ControlComboBox (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   COMBOBOX *pc = (COMBOBOX*) pControl->m_mem.p;

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(COMBOBOX));
         pc = (COMBOBOX*) pControl->m_mem.p;
         memset (pc, 0, sizeof(COMBOBOX));

         // from listbox
         pc->iLRMargin = 4;
         pc->iTBMargin = 4;
         pc->iBorderSize = 0;
         pc->cBorder = RGB (0,0,0xff);
         pc->cSelection = RGB(0xff, 0xc0, 0xc0);
         pc->cSelectionBorder = RGB(0,0,0);
         pc->iCurSel = 0;
         pc->fSort = FALSE;

         pc->fReCalc = TRUE;
         pc->plistLBELEM = new CListFixed;
         if (pc->plistLBELEM)
            pc->plistLBELEM->Init (sizeof(LBELEM));


         // from listbox
         pControl->AttribListAddDecimal (L"LRMargin", &pc->iLRMargin, FALSE, FALSE);
         pControl->AttribListAddDecimal (L"TBMargin", &pc->iTBMargin, FALSE, FALSE);
         pControl->AttribListAddDecimal (L"border", &pc->iBorderSize, FALSE, FALSE);
         pControl->AttribListAddDecimal (L"cursel", &pc->iCurSel, FALSE, TRUE);
         pControl->AttribListAddColor (L"bordercolor", &pc->cBorder, NULL, FALSE);
         pControl->AttribListAddColor (L"selcolor", &pc->cSelection, NULL, FALSE);
         pControl->AttribListAddColor (L"selbordercolor", &pc->cSelectionBorder, NULL, FALSE);
         pControl->AttribListAddBOOL (L"sort", &pc->fSort, &pc->fReCalc, TRUE);

         // constructor for dropdown
         DropDownMessageHandler (pControl, dwMessage, pParam, &pc->dd);
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
            if (!psz || wcsicmp(psz, L"elem"))
               continue;

            // else it's a valid element
            LBELEMAdd (pc, pSubNode, FALSE);
         }

         // following is drop-down general
         DropDownMessageHandler (pControl, dwMessage, pParam, &pc->dd);
      }
      return TRUE;

   case ESCM_DROPDOWNOPENED:
      {
         PESCMDROPDOWNOPENED p = (PESCMDROPDOWNOPENED)pParam;

         WCHAR       pszD[] = L"%d";
         WCHAR       szTemp[32];

         if (!pc->plistLBELEM->Num()) {
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return TRUE;
         }

         // sort if not already sorted
         RecalcIfNecessary(pControl, pc);

         // create a main node
         PCMMLNode   pMain;
         pMain = new CMMLNode;
         pMain->NameSet (L"<Main>");

         // specify that should have no margins on top/bottom/left/right. Cant
         // do this until page accepts a margin tag
         PCMMLNode   pPageInfo;
         pPageInfo = pMain->ContentAddNewNode ();;
         pPageInfo->NameSet (L"PageInfo");
         pPageInfo->AttribSet (L"LRMargin", L"0");
         pPageInfo->AttribSet (L"TBMargin", L"0");

         // background color fade or something
         PCMMLNode   pBack;
         pBack = pMain->ContentAddNewNode ();
         pBack->NameSet (L"colorblend");
         pBack->AttribSet (L"posn", L"background");
         //swprintf (szTemp, L"#%x", pc->cTBackground);
         ColorToAttrib (szTemp, pc->dd.cTBackground);
         pBack->AttribSet (L"tcolor", szTemp);
         //swprintf (szTemp, L"#%x", pc->cBBackground);
         ColorToAttrib (szTemp, pc->dd.cBBackground);
         pBack->AttribSet (L"bcolor", szTemp);

         // create the lsit box
         PCMMLNode   pList;
         pList = pMain->ContentAddNewNode();;
         pList->NameSet (L"ListBox");

         // set the listbox attributes as appropriate, including width & height
         pList->AttribSet (L"name", L"listbox");
         pList->AttribSet (L"Vscroll", L"ListScroll");
         swprintf (szTemp, pszD, p->iHeight);
         pList->AttribSet (L"height", szTemp);
         swprintf (szTemp, pszD, p->iWidth);
         pList->AttribSet (L"width", szTemp);
         swprintf (szTemp, pszD, pc->iCurSel);
         pList->AttribSet (L"cursel", szTemp);
         swprintf (szTemp, pszD, pc->iLRMargin);
         pList->AttribSet (L"LRMargin", szTemp);
         swprintf (szTemp, pszD, pc->iTBMargin);
         pList->AttribSet (L"TBMargin", szTemp);
         swprintf (szTemp, pszD, pc->iBorderSize);
         pList->AttribSet (L"border", szTemp);
         //swprintf (szTemp, L"#%x", pc->cBorder);
         ColorToAttrib (szTemp, pc->cBorder);
         pList->AttribSet (L"bordercolor", szTemp);
         //swprintf (szTemp, L"#%x", pc->cBackground);
         //pList->AttribSet (L"color", szTemp);
         pList->AttribSet (L"color", L"transparent");
         //swprintf (szTemp, L"#%x", pc->cSelection);
         ColorToAttrib (szTemp, pc->cSelection);
         pList->AttribSet (L"selcolor", szTemp);
         //swprintf (szTemp, L"#%x", pc->cSelectionBorder);
         ColorToAttrib (szTemp, pc->cSelectionBorder);
         pList->AttribSet (L"selbordercolor", szTemp);

         // fill the list box with elements
         DWORD i;
         for (i = 0; i < pc->plistLBELEM->Num();i++) {
            PLBELEM pe = (PLBELEM) pc->plistLBELEM->Get(i);
            if (!pe)
               continue;

            pList->ContentAddCloneNode (pe->pNode);
         }

         // add scroll bar
         PCMMLNode   pScroll;
         pScroll = pMain->ContentAddNewNode ();
         pScroll->NameSet (L"ScrollBar");
         pScroll->AttribSet (L"name", L"ListScroll");
         pScroll->AttribSet (L"orient", L"vert");
         swprintf (szTemp, pszD, p->iHeight);
         pScroll->AttribSet (L"height", szTemp);

         // bring up the window
         WCHAR    *psz;
         psz = p->pWindow->PageDialog (pMain, ComboBoxCallback, p->fKeyOpen ? (PVOID) 1 : 0);
         // BUGFIX - PageDialog no longer deleting pMain. Cloning instead
         delete pMain;
         
         // hide the window so it's not visible is a call is made from this
         p->pWindow->ShowWindow (SW_HIDE);

         if (!wcsicmp(psz, L"[close]")) {
            // just closed because didn't select
            // close sound
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_MENUCLOSE);

            //do nothing
         }
         else {
            if (psz[0] != L'@')
               return TRUE;

            // number get it
            DWORD dw;
            dw = (DWORD) _wtoi (psz + 1);

            // change sel
            SetCurSel (pControl, pc, (int) dw);
         };
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

      // the following is the dropdown specific
      DropDownMessageHandler (pControl, dwMessage, pParam, &pc->dd);
      return TRUE;

   case ESCM_DROPDOWNTEXTBLOCK:
      {
         PESCMDROPDOWNTEXTBLOCK p = (PESCMDROPDOWNTEXTBLOCK) pParam;

         // figure out the current element
         if ((pc->iCurSel < 0) || (pc->iCurSel >= (int) pc->plistLBELEM->Num()))
            pc->iCurSel = 0;

         // get it
         LBELEM  *pe;
         pe = (LBELEM*) pc->plistLBELEM->Get((DWORD)pc->iCurSel);
         if (!pe)
            return TRUE;   // nothing to get

         // only give new node if forced or if it's really changed
         if (p->fMustSet || (pc->pNodeTextBlock != pe->pNode)) {
            p->pNode = pe->pNode;
            p->fDeleteNode = FALSE;
         }
         pc->pNodeTextBlock = pe->pNode;

      }
      return TRUE;

   case ESCM_COMBOBOXADD:
      {
         ESCMCOMBOBOXADD *p = (ESCMCOMBOBOXADD*) pParam;
         // BUGFIX - don't recalc every time in case adding lots of elements
         // at once
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
            for (i = dwNum-1; i < dwNum; i--) {
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
               ESCMCOMBOBOXADD add;
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



   case ESCM_COMBOBOXDELETE:
      {
         ESCMCOMBOBOXDELETE *p = (ESCMCOMBOBOXDELETE*) pParam;
         RecalcIfNecessary (pControl, pc);

         // will need to recalc height & stuff
         pc->fReCalc = TRUE;

         // delete it
         LBELEMDelete (pc, p->dwIndex);

         // invalidate
         pControl->Invalidate();
      }
      return TRUE;




   case ESCM_COMBOBOXFINDSTRING:
      {
         ESCMCOMBOBOXFINDSTRING *p = (ESCMCOMBOBOXFINDSTRING*) pParam;
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
            if (p->fExact && !wcsicmp(pe->pszName, p->psz)) {
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



   case ESCM_COMBOBOXGETCOUNT:
      {
         ESCMCOMBOBOXGETCOUNT *p = (ESCMCOMBOBOXGETCOUNT*) pParam;
         RecalcIfNecessary (pControl, pc);

         p->dwNum = pc->plistLBELEM->Num();
      }
      return TRUE;





   case ESCM_COMBOBOXGETITEM:
      {
         ESCMCOMBOBOXGETITEM *p = (ESCMCOMBOBOXGETITEM*) pParam;
         RecalcIfNecessary (pControl, pc);

         PLBELEM  pe;
         pe = (PLBELEM) pc->plistLBELEM->Get(p->dwIndex);
         if (!pe) {
            p->pszData = p->pszName = NULL;
            return TRUE;
         }

         // else
         p->pszData = pe->pszData;
         p->pszName = pe->pszName;
      }
      return TRUE;




   case ESCM_COMBOBOXRESETCONTENT:
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


   case ESCM_COMBOBOXSELECTSTRING:
      {
         ESCMCOMBOBOXSELECTSTRING *p = (ESCMCOMBOBOXSELECTSTRING*) pParam;
         RecalcIfNecessary (pControl, pc);

         pControl->Message (ESCM_COMBOBOXFINDSTRING, pParam);

         if (p->dwIndex != (DWORD)-1) {
            SetCurSel (pControl, pc, (int) p->dwIndex);
         }
      }
      return TRUE;
   }

   return DropDownMessageHandler (pControl, dwMessage, pParam, pc ? &pc->dd : NULL);
}

// BUGBUG - 2.0 - way to have bitmap graphics for ComboBoxs/buttons/scroll bars?



