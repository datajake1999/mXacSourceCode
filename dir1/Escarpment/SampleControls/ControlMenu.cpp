/***********************************************************************
ControlMenu.cpp - Code for a control

begun 4/18/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include "escarpment.h"
#include "control.h"
#include "resleak.h"



typedef struct {
   int            iWidth, iHeight;  // either a number or percent
   BOOL           fWidthPercent, fHeightPercent;   // TRUE if iWidth percent, FALSE if pixels
   PWSTR          pszStyle;   // style string
   PWSTR          pszVCenter; // vertical centering, "top", "bottom", "center"
   PWSTR          pszAppear;  // set to "right" for menu to appear to right, default "below"
   COLORREF       cButton;
   COLORREF       cButtonBase;
   COLORREF       cLight;
   int            iButtonWidth, iButtonHeight;  // actual width and height of drawn thingy
   int            iButtonDepth;  // in Z coordinate
   int            iMarginTopBottom;
   int            iMarginLeftRight;
   int            iMarginMenuText;      // margin between the Menu and the text
   BOOL           fShowButton;         // if TRUE, show the Menu, FALSE just the text

   CEscTextBlock  *pTextBlock;
   CRender        *pRender;         // rendering objet
   BOOL           fRecalcText;      // set to TRUE if need to recalc text
   PCMMLNode      pNodeText;        // text that goes in the menu
   PCMMLNode      pNodeContents;    // menu contents
} MENU, *PMENU;

#define  ESCM_PRESSED      (ESCM_USER+1)
#define  ESCM_KEYOPEN      (ESCM_USER+2)
#define  ESCM_UPARROW      (ESCM_USER+3)
#define  ESCM_DOWNARROW    (ESCM_USER+4)

/* RIGHTSIDE - Returns the number of pixels to the right ot he text. This is generally
   the size of the Menus + margins text */
#define  RIGHTSIDE    (pc->pNodeText && pc->pNodeText->ContentNum() ? ((pc->fShowButton ? (pc->iButtonWidth + pc->iMarginLeftRight) : 0) + pc->iMarginMenuText) : (pc->iButtonWidth + pc->iMarginLeftRight))


#define  USEMENULIGHT pc->cLight
#define  USEMENUCOLOR2 pc->cButton
#define  USEMENUCOLOR3 (fDrawDown ? USEMENULIGHT : USEMENUCOLOR2)
#define  USEMENUCOLOR (pControl->m_fEnabled ? USEMENUCOLOR3 : USEBASECOLOR)
#define  USEBASECOLOR pc->cButtonBase

/***********************************************************************
Page callback
*/
BOOL MenuCallback (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // add the acclerators
      ESCACCELERATOR a;
      memset (&a, 0, sizeof(a));
      a.c = VK_ESCAPE;
      a.dwMessage = ESCM_CLOSE;
      pPage->m_listESCACCELERATOR.Add (&a);
      a.c = VK_UP;
      a.dwMessage = ESCM_UPARROW;
      pPage->m_listESCACCELERATOR.Add (&a);
      a.c = VK_DOWN;
      a.dwMessage = ESCM_DOWNARROW;
      pPage->m_listESCACCELERATOR.Add (&a);

      DropDownFitToScreen(pPage);

      // set the focus to the first control
      if (pPage->m_pUserData)
         pPage->FocusToNextControl ();
      return TRUE;

   case ESCM_UPARROW:
      pPage->FocusToNextControl (FALSE);
      return TRUE;

   case ESCM_DOWNARROW:
      pPage->FocusToNextControl (TRUE);
      return TRUE;

   }

   return FALSE;
}

/***********************************************************************
Control callback
*/
BOOL ControlMenu (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   MENU *pc = (MENU*) pControl->m_mem.p;

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(MENU));
         pc = (MENU*) pControl->m_mem.p;
         memset (pc, 0, sizeof(MENU));
         pc->iWidth = pc->iHeight = 0;   // so know if it's set
         pc->cButton = RGB(0xa0,0x70,0xff); // RGB(0x70,0x70,0xff);
         pc->cButtonBase = RGB(0x70, 0x70, 0x70);
         pc->cLight = RGB(0xff, 0x40, 0x40);
         pc->iButtonWidth = pc->iButtonHeight = 16; // 24;
         pc->iButtonDepth = pc->iButtonWidth * 2 / 3;
         pc->iMarginTopBottom = pc->iMarginLeftRight= pc->iMarginMenuText = 4;
         pc->fShowButton = TRUE;
         pc->pszVCenter = L"center";
         pc->pRender = new CRender;

         pControl->AttribListAddDecimalOrPercent (L"width", &pc->iWidth, &pc->fWidthPercent);
         pControl->AttribListAddDecimalOrPercent (L"height", &pc->iHeight, &pc->fHeightPercent);
         pControl->AttribListAddString (L"style", &pc->pszStyle, &pc->fRecalcText, TRUE);
         pControl->AttribListAddString (L"appear", &pc->pszAppear, &pc->fRecalcText, TRUE);
         pControl->AttribListAddColor (L"color", &pc->cButton, FALSE, TRUE);
         pControl->AttribListAddColor (L"basecolor", &pc->cButtonBase, FALSE, TRUE);
         pControl->AttribListAddColor (L"lightcolor", &pc->cLight, FALSE, TRUE);
         pControl->AttribListAddBOOL (L"showbutton", &pc->fShowButton, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"buttonwidth", &pc->iButtonWidth, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"buttonheight", &pc->iButtonHeight, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"buttondepth", &pc->iButtonDepth, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"margintopbottom", &pc->iMarginTopBottom, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"marginleftright", &pc->iMarginLeftRight, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"marginbuttonText", &pc->iMarginMenuText, &pc->fRecalcText, TRUE);
         pControl->AttribListAddString (L"valign", &pc->pszVCenter, &pc->fRecalcText, TRUE);

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
         a.c = (pc->pszAppear && !wcsicmp(pc->pszAppear, L"right")) ? VK_RIGHT : VK_DOWN;
         pControl->m_listAccelFocus.Add (&a);

         pControl->m_fWantMouse = TRUE;
         pControl->m_dwWantFocus = 1;

         // indicate that we want to be redrawn on a mov
         pControl->m_fRedrawOnMove = TRUE;

         // find the text and contents
         DWORD i;
         for (i = 0; i < pControl->m_pNode->ContentNum(); i++) {
            PWSTR psz;
            PCMMLNode   pSub;
            pSub = NULL;
            pControl->m_pNode->ContentEnum(i, &psz, &pSub);
            if (!pSub) continue;
            if (!wcsicmp(pSub->NameGet(), L"MenuText"))
               pc->pNodeText = pSub;
            else if (!wcsicmp(pSub->NameGet(), L"MenuContents"))
               pc->pNodeContents = pSub;
            // else ignore
         }
      }
      return TRUE;

   case ESCM_KEYOPEN:
   case ESCM_PRESSED:
      {
         CEscWindow  cWindow;
         if (!pc->pNodeContents)
            return TRUE;  // must have contents

         // beep when enter
         pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_MENUOPEN);

         // if move over then create a window underneath using the menucontents
         // subnode

         // size?
         RECT  rSize;
         pControl->m_pParentPage->CoordPageToScreen (&pControl->m_rPosn, &rSize);

         DWORD dwStyle;
         dwStyle = EWS_NOTITLE | EWS_FIXEDSIZE | EWS_NOVSCROLL | EWS_AUTOHEIGHT;

         // if open by the mouse, close by the mouse
         if (dwMessage == ESCM_PRESSED)
            dwStyle |= EWS_CLOSENOMOUSE;

         // can set menu to appear to the right also
         BOOL  fRight;
         fRight = FALSE;
         if (pc->pszAppear && !wcsicmp(pc->pszAppear, L"right")) {
            int   iX;
            iX = rSize.right - rSize.left;
            rSize.right += iX;
            rSize.left += iX;
            fRight = TRUE;
         }

         // make width at least 1/6 of screen
         int   iWidth, iVal;
         BOOL  fPercent;
         iWidth = rSize.right - rSize.left;
         iWidth = max(iWidth, GetSystemMetrics (SM_CXSCREEN) / 6);
         // make wider at request of pNodeContents
         if (AttribToDecimalOrPercent (pc->pNodeContents->AttribGet(L"width"), &fPercent, &iVal)) {
            iWidth = fPercent ? (GetSystemMetrics(SM_CXSCREEN) * iVal / 100) : iVal;
            rSize.right = rSize.left + iWidth;
         }
         else {
            rSize.right = rSize.left + 2; // it'll resize
            dwStyle |= EWS_AUTOWIDTH;
         }


         if (!fRight)
            rSize.top = rSize.bottom;  // move immediately below menu
         rSize.bottom = rSize.top + 1; // since will automatically be resized

         if (!cWindow.Init(pControl->m_hInstance, pControl->m_pParentPage->m_pWindow->m_hWnd,
            dwStyle, &rSize))
            return TRUE;

         // specify that is mose moves off window (except for where control
         // is located) that should shut down
         pControl->m_pParentPage->CoordPageToScreen (&pControl->m_rPosn, &cWindow.m_rMouseMove);

         // bring up the window
         WCHAR    *psz;
         PCMMLNode   pMenu;
         pMenu = pc->pNodeContents->Clone();
         if (!pMenu)
            return 0;
         // set the menu to type <main>
         pMenu->NameSet (L"<Main>");
         psz = cWindow.PageDialog (pMenu, MenuCallback, (dwMessage == ESCM_KEYOPEN) ? (PVOID) 1 : 0);
         // BUGFIX - Delete pMenu because PageDialog no longer deletingit
         delete pMenu;

         // hide the window so it's not visible is a call is made from this
         cWindow.ShowWindow (SW_HIDE);

         if (!wcsicmp(psz, L"[close]")) {
            // just closed because didn't select
            // close sound
            pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_MENUCLOSE);

            //do nothing
         }
         else {
            // link, so tell page
            pControl->m_pParentPage->Link(psz);
         };
      }
      return TRUE;

   case ESCM_MOUSEENTER:
      // invalidate
      pControl->Invalidate();

      // call "pressed" to bring down menu when enters
      pControl->Message (ESCM_PRESSED);
      return TRUE;

   case ESCM_DESTRUCTOR:
      if (pc->pTextBlock)
         delete pc->pTextBlock;
      if (pc->pRender)
         delete pc->pRender;
      return TRUE;

   case ESCM_QUERYSIZE:
      {
         ESCMQUERYSIZE  *p = (ESCMQUERYSIZE*) pParam;

         // delete the current  text block if have one
         if (pc->pTextBlock)
            delete pc->pTextBlock;
         pc->pTextBlock = NULL;

         int   iVCenter = 0;
         if (!wcsicmp (pc->pszVCenter, L"top"))
            iVCenter = -1;
         else if (!wcsicmp (pc->pszVCenter, L"bottom"))
            iVCenter = 1;

         // see how large is given
         POINT pGiven;
         pGiven.x = pc->fWidthPercent ? (pc->iWidth * p->iDisplayWidth / 100) : pc->iWidth;
         pGiven.y = pc->fHeightPercent ? (pc->iHeight * p->iDisplayWidth / 100) : pc->iHeight;

         // what size do we try to calculate for
         POINT pCalc;
         pCalc = pGiven;
         if (!pCalc.x)
            pCalc.x = p->iDisplayWidth;
         if (!pCalc.y)
            pCalc.y = p->iDisplayWidth / 2;   // dont make Menu too tall
         pCalc.x -= (RIGHTSIDE + pc->iMarginLeftRight);
         pCalc.y -= 2 * pc->iMarginTopBottom;

         // see what it comes out at
         if (pc->pNodeText && pc->pNodeText->ContentNum()) {
            pc->pTextBlock = pControl->TextBlock (p->hDC, pc->pNodeText, pCalc.x, FALSE, TRUE);
            if (!pc->pTextBlock)
               return FALSE;
            // set the size we want
            p->iWidth = pGiven.x ? pGiven.x : (pc->pTextBlock->m_iCalcWidth + RIGHTSIDE + pc->iMarginLeftRight);
            p->iHeight = pGiven.y ? pGiven.y : (max(pc->fShowButton ? pc->iButtonHeight : 0, pc->pTextBlock->m_iCalcHeight) + 2*pc->iMarginTopBottom );
         }
         else {
            // set the size we want
            p->iWidth = pGiven.x ? pGiven.x : (RIGHTSIDE + pc->iMarginLeftRight);
            p->iHeight = pGiven.y ? pGiven.y : ((pc->fShowButton ? pc->iButtonHeight : 0) + 2*pc->iMarginTopBottom );
         }
         pc->fRecalcText = FALSE;


         // postinterpret, moving the text to the right of the butotn.
         RECT rFull;
         rFull.left = rFull.top = 0;
         rFull.right = p->iWidth;
         rFull.bottom = p->iHeight;
         int   iDeltaY;
         // BUGFIX - crashing on menu if no text block
         if (!pc->pTextBlock)
            return TRUE;
         if (iVCenter < 0)
            iDeltaY = pc->iMarginTopBottom;
         else if (iVCenter > 0)
            iDeltaY = p->iHeight - pc->pTextBlock->m_iCalcHeight - pc->iMarginTopBottom;
         else
            iDeltaY = p->iHeight / 2 - (pc->pTextBlock->m_iCalcHeight)/2;
         if (pc->pTextBlock)
            pc->pTextBlock->PostInterpret (pc->iMarginLeftRight, iDeltaY, &rFull);

      }
      return TRUE;

   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;

         BOOL  fDrawDown;
         fDrawDown = pControl->m_fMouseOver;

         int   iVCenter = 0;
         if (!wcsicmp (pc->pszVCenter, L"top"))
            iVCenter = -1;
         else if (!wcsicmp (pc->pszVCenter, L"bottom"))
            iVCenter = 1;

         // draw the text
         if (pc->pTextBlock) {
            POINT pOffset;
            pOffset.x = p->rControlHDC.left;
            pOffset.y = p->rControlHDC.top;

            pc->pTextBlock->Paint (p->hDC, &pOffset, &p->rControlHDC, &p->rControlScreen,
               &p->rTotalScreen);
         }

         // paint the button
         if (pc->fShowButton) {
            RECT  r;
            if (iVCenter < 0)
               r.top = p->rControlHDC.top;
            else if (iVCenter > 0)
               r.top = p->rControlHDC.bottom - 2*pc->iMarginTopBottom - pc->iButtonHeight;
            else
               r.top = (p->rControlHDC.bottom + p->rControlHDC.top) / 2 - pc->iButtonHeight / 2 - pc->iMarginTopBottom;
            r.right = p->rControlHDC.right;
            r.left = r.right - (pc->iButtonWidth + 2*pc->iMarginLeftRight);
            r.bottom = r.top + pc->iButtonHeight + 2*pc->iMarginTopBottom;

            // figure out where this will be on the screen
            RECT  rScreen;
            rScreen = r;
            OffsetRect (&rScreen, p->rControlScreen.left - p->rControlHDC.left,
               p->rControlScreen.top - p->rControlHDC.top);
            pc->pRender->InitForControl (p->hDC, &r, &rScreen, &p->rTotalScreen);
            // note negative translation
            pc->pRender->Translate ((r.right - r.left) / 2, -(r.bottom - r.top)/2);

            // scale so that a -1,-1,0 to 1,1,1 object is the right size
            pc->pRender->Scale (pc->iButtonWidth / 2, pc->iButtonHeight / 2, pc->iButtonDepth);

            Draw3DButton (pc->pRender, USEBASECOLOR, USEMENUCOLOR, pc->pszStyle,
               (pc->pszAppear && !wcsicmp(pc->pszAppear, L"right")) ? L"rightarrow" : L"downarrow", fDrawDown);

            pc->pRender->Commit (p->hDC, &r);
         }

      }
      return TRUE;

   case ESCM_SWITCHACCEL:
      // bring down menu
      pControl->Message (ESCM_KEYOPEN);
      return FALSE;


   case ESCM_LBUTTONDOWN:
      // beep
      pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
      return TRUE;
   }

   return FALSE;
}

// BUGBUG - 2.0 - way to have bitmap graphics for menus/buttons/scroll bars?


// BUGBUG - 2.0 - sometimes a sub-menu within a menu doesn't seem to get the link message
// sent all the way up to the menu's parent
