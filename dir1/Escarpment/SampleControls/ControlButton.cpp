/***********************************************************************
ControlButton.cpp - Code for a control

begun 4/4/2000 by Mike Rozak
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
   COLORREF       cButton;
   COLORREF       cButtonBase;
   COLORREF       cHighlight; // if selected
   COLORREF       cLight;
   int            iButtonWidth, iButtonHeight;  // actual width and height of drawn thingy
   int            iButtonDepth;  // in Z coordinate
   int            iMarginTopBottom;
   int            iMarginLeftRight;
   int            iMarginButtonText;      // margin between the button and the text
   PWSTR          pszHRef;
   BOOL           fShowButton;         // if TRUE, show the button, FALSE just the text
   BOOL           fChecked;            // if TRUE button checked
   PWSTR          pszGroup;            // group box
   BOOL           fRadioButton;        // if TRUE its a radio button - set one and others in group clear
   BOOL           fCheckBox;           // if true its a checkbox
   CEscTextBlock  *pTextBlock;
   CRender        *pRender;         // rendering objet
   BOOL           fRecalcText;      // set to TRUE if need to recalc text
} BUTTON, *PBUTTON;

#define  ESCM_PRESSED      (ESCM_USER+1)

/* LEFTSIDE - Returns the number of pixels to the left ot he text. This is generally
   the size of the buttons + margins text */
#define  LEFTSIDE    (pControl->m_pNode->ContentNum() ? ((pc->fShowButton ? (pc->iButtonWidth + pc->iMarginLeftRight) : 0) + pc->iMarginButtonText) : (pc->iButtonWidth + pc->iMarginLeftRight))


#define  USEBUTTONLIGHT    pc->cLight
#define  USEBUTTONCOLOR2   pc->cButton
#define  USEBUTTONCOLOR3   (fDrawDown ? USEBUTTONLIGHT : USEBUTTONCOLOR2)
#define  BUTTONCOLOR    (pControl->m_fEnabled ? USEBUTTONCOLOR3 : BASECOLOR)
#define  BASECOLOR      pc->cButtonBase

#define  USEBASECOLOR pRender->m_DefColor[0]=GetRValue(crBase);pRender->m_DefColor[1]=GetGValue(crBase);pRender->m_DefColor[2]=GetBValue(crBase)
#define  USEBUTTONCOLOR pRender->m_DefColor[0]=GetRValue(crButton);pRender->m_DefColor[1]=GetGValue(crButton);pRender->m_DefColor[2]=GetBValue(crButton)

/***********************************************************************
Draw3DButton - Draws the 3D button for the button control, combox box,
   menu, time control, date control, and name control.

Scaling is assumed to already have been done. All objects are drawn from
-1 to 1 in X, Y, and Z.

inputs
   PCRender    pRender - Rendering object.
   COLORREF    crBase - Color of the base
   COLORREF    crButton - Color of the button
   PWSTR       pszStyle - Style speicified. May be nULL
   PWSTR       pszDefault - Default style if pszStyle==NULL or it's not a match
   BOOL        fDrawDown - if TRUE, draw in the down position
returns
   none
*/
void Draw3DButton (PCRender pRender, COLORREF crBase, COLORREF crButton,
                   PWSTR pszStyle, PWSTR pszDefault, BOOL fDrawDown)
{
   WCHAR *psz;
   psz = pszStyle ? pszStyle : pszDefault;
   if (!wcsicmp(psz, L"sphere")) {
      USEBASECOLOR;
      pRender->MeshEllipsoid(1.0, 1.0, .2);
      pRender->ShapeMeshSurface ();
      USEBUTTONCOLOR;
      //if (fDrawDown)
      //   pRender->Translate (0, 0, -.3);
      //pRender->MeshSphere(.75);
      pRender->MeshEllipsoid(.75, .75, fDrawDown ? .4 : .75);
      pRender->ShapeMeshSurface ();
   }
   else if (!wcsicmp(psz, L"box")) {
      USEBASECOLOR;
      pRender->ShapeBox (2, 2, .4);
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->ShapeBox (1.5, 1.5, 2);
   }
   else if (!wcsicmp(psz, L"beveled")) {
      USEBASECOLOR;
      pRender->ShapeFlatPyramid (2, 2, 1.7, 1.7, .2);
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->ShapeFlatPyramid (1.6, 1.6, 1, 1, 1);
   }
   else if (!wcsicmp(psz, L"righttriangle")) {
      USEBASECOLOR;
      pRender->ShapeDeepTriangle (2, 2, 1.7, 1.7, .2);
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->ShapeDeepTriangle (1.6, 1.6, 1.5, 1.5, 1);
   }
   else if (!wcsicmp(psz, L"lefttriangle")) {
      pRender->Rotate (PI,3);
      USEBASECOLOR;
      pRender->ShapeDeepTriangle (2, 2, 1.7, 1.7, .2);
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->ShapeDeepTriangle (1.6, 1.6, 1.5, 1.5, 1);
   }
   else if (!wcsicmp(psz, L"uptriangle")) {
      pRender->Rotate (PI/2,3);
      USEBASECOLOR;
      pRender->ShapeDeepTriangle (2, 2, 1.7, 1.7, .2);
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->ShapeDeepTriangle (1.6, 1.6, 1.5, 1.5, 1);
#ifdef _DEBUG
      OutputDebugString ("Paint UpTriangle\r\n");
#endif
   }
   else if (!wcsicmp(psz, L"downtriangle")) {
      pRender->Rotate (-PI/2,3);
      USEBASECOLOR;
      pRender->ShapeDeepTriangle (2, 2, 1.7, 1.7, .2);
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->ShapeDeepTriangle (1.6, 1.6, 1.5, 1.5, 1);
   }
   else if (!wcsicmp(psz, L"rightarrow")) {
      USEBASECOLOR;
      pRender->ShapeDeepArrow (2, 2, .2);
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->ShapeDeepArrow (1.6, 1.6, 1);
   }
   else if (!wcsicmp(psz, L"leftarrow")) {
      pRender->Rotate (PI,3);
      USEBASECOLOR;
      pRender->ShapeDeepArrow (2, 2, .2);
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->ShapeDeepArrow (1.6, 1.6, 1);
   }
   else if (!wcsicmp(psz, L"uparrow")) {
      pRender->Rotate (PI/2,3);
      USEBASECOLOR;
      pRender->ShapeDeepArrow (2, 2, .2);
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->ShapeDeepArrow (1.6, 1.6, 1);
   }
   else if (!wcsicmp(psz, L"downarrow")) {
      pRender->Rotate (-PI/2,3);
      USEBASECOLOR;
      pRender->ShapeDeepArrow (2, 2, .2);
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->ShapeDeepArrow (1.6, 1.6, 1);
   }
   else if (!wcsicmp(psz, L"teapot")) {
      USEBUTTONCOLOR;
      pRender->Scale (.35);
      pRender->Translate (0, -.8, 0);
      if (fDrawDown)
         pRender->Rotate (-PI/8, 3);
      pRender->ShapeTeapot ();
   }
   else if (!wcsicmp(psz, L"check")) {
      USEBASECOLOR;
      pRender->ShapeDeepFrame (2, 2, .5, .3, .1);

      if (fDrawDown) {
         USEBUTTONCOLOR;
         pnt   p[3];
         pnt   c[3];
         CopyPnt (pRender->m_DefColor, c[0]);
         CopyPnt (pRender->m_DefColor, c[1]);
         CopyPnt (pRender->m_DefColor, c[2]);
         // BUGFIX - Had a Z value of 0 but was clipping on on Win98
         p[0][0] = -.6;
         p[0][1] = 0;
         p[0][2] = .1;
         p[1][0] = -.4;
         p[1][1] = .2;
         p[1][2] = .1;
         p[2][0] = -.1;
         p[2][1] = -.7;
         p[2][2] = .1;
         pRender->ShapePolygon (3, p, c);

         CopyPnt (p[2], p[0]);
         p[1][0] = .3;
         p[1][1] = .7;
         p[1][2] = .1;
         p[2][0] = .6;
         p[2][1] = .5;
         p[2][2] = .1;
         pRender->ShapePolygon (3, p, c);
      }

   }
   else if (!wcsicmp(psz, L"X")) {
      USEBASECOLOR;
      pRender->ShapeDeepFrame (2, 2, .5, .3, .1);

      if (fDrawDown) {
         USEBUTTONCOLOR;
         // pRender->Translate (0, 0, .75);
         pRender->Rotate (PI/4, 3);
         pRender->ShapeBox (.2, 1.6, .5);
         pRender->Rotate (PI/2, 3);
         pRender->ShapeBox (.2, 1.6, .5);
      }

   }
   else if (!wcsicmp(psz, L"toggle")) {
      USEBASECOLOR;
      pRender->ShapeFlatPyramid (1, .6, .8, .4, .2);

      USEBUTTONCOLOR;
      pRender->Rotate (fDrawDown ? (PI/4) : (-PI/4), 1);
      pRender->ShapeFlatPyramid (.4, .2, 1, .3, 1);
      //pRender->ShapeBox (1, .2, 2);

   }
   else if (!wcsicmp(psz, L"light")) {
      USEBASECOLOR;
      pRender->ShapeFlatPyramid (2, 2, 1.6, 1.6, .1);

      USEBUTTONCOLOR;
      pRender->MatrixPush();
      pRender->Translate (-.6, 0, -.65);
      pRender->Rotate (-PI/2, 3);
      pRender->MeshFunnel (1.2, 1, 1);
      pRender->ShapeMeshSurface ();
      pRender->MatrixPop();
      pRender->Translate (0, fDrawDown ? -.3 : .3, 0);
      pRender->Rotate (fDrawDown ? (PI/4) : (-PI/4), 1);
      pRender->ShapeFlatPyramid (1, 1, .9, .7, 1.0);
      //pRender->ShapeBox (1, .2, 2);

   }
   else if (!wcsicmp(psz, L"cylinder")) {
      USEBASECOLOR;
      pRender->MeshEllipsoid(1.0, 1.0, .2);
      pRender->ShapeMeshSurface ();
      USEBUTTONCOLOR;
      if (fDrawDown)
         pRender->Translate (0, 0, -.7);
      pRender->Rotate (PI / 2, 1);
      pRender->MeshFunnel (.7, .75, .75);
      pRender->ShapeMeshSurface ();
      pRender->Translate (0, .7, 0);
      pRender->MeshEllipsoid (.75, .5, .75);
      pRender->ShapeMeshSurface ();
   }
   else {
      // no matching string so use default
      Draw3DButton (pRender, crBase, crButton, pszDefault, pszDefault, fDrawDown);
   }
   // BUGBUG - 2.0 - circular rubber-like button that pushes in
}

/***********************************************************************
Control callback
*/
BOOL ControlButton (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   BUTTON *pc = (BUTTON*) pControl->m_mem.p;

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(BUTTON));
         pc = (BUTTON*) pControl->m_mem.p;
         memset (pc, 0, sizeof(BUTTON));
         pc->iWidth = pc->iHeight = 0;   // so know if it's set
         pc->cButton = RGB(0x70,0x70,0xff);
         pc->cButtonBase = RGB(0x70, 0x70, 0x70);
         pc->cLight = RGB(0xff, 0x40, 0x40);
         pc->cHighlight = (DWORD) -1;
         pc->iButtonWidth = pc->iButtonHeight = 24;
         pc->iButtonDepth = pc->iButtonWidth * 2 / 3;
         pc->iMarginTopBottom = pc->iMarginLeftRight= pc->iMarginButtonText = 6;
         pc->fShowButton = TRUE;
         pc->pszVCenter = L"center";
         pc->pRender = new CRender;

         pControl->AttribListAddDecimalOrPercent (L"width", &pc->iWidth, &pc->fWidthPercent);
         pControl->AttribListAddDecimalOrPercent (L"height", &pc->iHeight, &pc->fHeightPercent);
         pControl->AttribListAddString (L"style", &pc->pszStyle, &pc->fRecalcText, TRUE);
         pControl->AttribListAddColor (L"color", &pc->cButton, FALSE, TRUE);
         pControl->AttribListAddColor (L"basecolor", &pc->cButtonBase, FALSE, TRUE);
         pControl->AttribListAddColor (L"highlightcolor", &pc->cHighlight, FALSE, TRUE);
         pControl->AttribListAddColor (L"lightcolor", &pc->cLight, FALSE, TRUE);
         pControl->AttribListAddBOOL (L"showbutton", &pc->fShowButton, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"buttonwidth", &pc->iButtonWidth, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"buttonheight", &pc->iButtonHeight, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"buttondepth", &pc->iButtonDepth, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"margintopbottom", &pc->iMarginTopBottom, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"marginleftright", &pc->iMarginLeftRight, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"marginButtonText", &pc->iMarginButtonText, &pc->fRecalcText, TRUE);
         pControl->AttribListAddString (L"href", &pc->pszHRef, FALSE, FALSE);
         pControl->AttribListAddBOOL (L"checked", &pc->fChecked, NULL, TRUE);
         pControl->AttribListAddString (L"group", &pc->pszGroup, NULL, FALSE);
         pControl->AttribListAddString (L"valign", &pc->pszVCenter, &pc->fRecalcText, TRUE);
         pControl->AttribListAddBOOL (L"checkbox", &pc->fCheckBox, NULL, TRUE);
         pControl->AttribListAddBOOL (L"radiobutton", &pc->fRadioButton, NULL, TRUE);

      }
      return TRUE;

   case ESCM_INITCONTROL:
      {
         // add the acclerators
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = L' ';
         a.dwMessage = ESCM_PRESSED;
         pControl->m_listAccelFocus.Add (&a);
         a.c = L'\n';
         pControl->m_listAccelFocus.Add (&a);

         pControl->m_fWantMouse = TRUE;
         pControl->m_dwWantFocus = 1;

         // indicate that we want to be redrawn on a mov
         pControl->m_fRedrawOnMove = TRUE;
      }
      return TRUE;

   case ESCM_FOCUS:
      // if focus switches to this window and its a radio button then call pressed
      if (pControl->m_fFocus && pc->fRadioButton)
         pControl->Message (ESCM_PRESSED);
      return FALSE;  // fall ont default handler

   case ESCM_KEYDOWN:
   case ESCM_SYSKEYDOWN:
      {
         // up/down arrows for radio buttons
         ESCMKEYDOWN *p = (ESCMKEYDOWN*) pParam;

         if (!pc->fRadioButton)
            return FALSE;
         if (!pc->pszGroup)
            return FALSE;

         BOOL  fForward;
         switch (p->nVirtKey) {
         case VK_UP:
         case VK_LEFT:
            fForward = FALSE;
            break;
         case VK_DOWN:
         case VK_RIGHT:
            fForward = TRUE;
            break;
         default:
            return FALSE;  // not trapping
         }
         p->fEaten = TRUE;

         WCHAR *pCur, *pComma;
         WCHAR c;
         PCEscControl   pFirst, pLast;
         BOOL  fWantNext;
         fWantNext = FALSE;
         pFirst = pLast = NULL;
         for (pCur = pc->pszGroup; pCur && *pCur; pCur = pComma ? (pComma+1) : NULL) {
            pComma = wcschr(pCur, L',');
            if (pComma) {
               c = *pComma;
               *pComma = NULL;
            }

            PCEscControl   pCont;
            PCEscControl   *ppCont;
            ppCont = (PCEscControl*) pControl->m_pParentPage->m_treeControls.Find(pCur);
            if (pComma)
               *pComma = c;

            if (!ppCont)
               continue;
            pCont = *ppCont;
            if (!pFirst)
               pFirst = pCont;

            if (fWantNext) {
               pControl->m_pParentPage->FocusSet (pCont);
               return TRUE;
            }

            if (pCont == pControl) {
               // if going forward want next
               if (fForward)
                  fWantNext = TRUE;
               else {
                  // going back. Want the last unless not there yet
                  if (pLast) {
                     pControl->m_pParentPage->FocusSet (pLast);
                     return TRUE;
                  }
                  // else go all the way to the end
               }
            }

            // remember the last one
            pLast = pCont;
         }

         // if going back then use the last one
         if (!fForward && pLast) {
            pControl->m_pParentPage->FocusSet (pLast);
            return TRUE;
         }
         if (fForward && pFirst) {
            pControl->m_pParentPage->FocusSet (pFirst);
            return TRUE;
         }

      }
      return TRUE;


   case ESCM_MOUSEMOVE:
      // if the button is down then refresh since may change the state when
      // move mouse off button region
      if (pControl->m_fLButtonDown)
         pControl->Invalidate();
      return FALSE;

   case ESCM_SWITCHACCEL:
   case ESCM_PRESSED:
      {
         // if it's a checkbox flip the check
         if (pc->fCheckBox || pc->fRadioButton) {
            if (pc->fRadioButton)
               pc->fChecked = TRUE;
            else
               pc->fChecked = !pc->fChecked;
            pControl->Invalidate();
         }

         // if its a radio button make sure other radio buttons not checked
         if (pc->pszGroup) {
            WCHAR *pCur, *pComma;
            WCHAR c;
            for (pCur = pc->pszGroup; pCur && *pCur; pCur = pComma ? (pComma+1) : NULL) {
               pComma = wcschr(pCur, L',');
               if (pComma) {
                  c = *pComma;
                  *pComma = NULL;
               }

               PCEscControl   pCont;
               PCEscControl   *ppCont;
               ppCont = (PCEscControl*) pControl->m_pParentPage->m_treeControls.Find(pCur);
               if (pComma)
                  *pComma = c;

               if (!ppCont)
                  continue;
               pCont = *ppCont;
               if (pCont == pControl)
                  continue;
               // else, make sure it's unchecked
               pCont->AttribSet (L"checked", L"no");
            }
         }


         // send a message to parent
         ESCNBUTTONPRESS bp;
         bp.pControl = pControl;
         pControl->MessageToParent (ESCN_BUTTONPRESS,&bp);

         // if hlink do it
         if (pc->pszHRef)
            pControl->m_pParentPage->Link (pc->pszHRef);
      }
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
            pCalc.y = p->iDisplayWidth / 2;   // dont make button too tall
         pCalc.x -= (LEFTSIDE + pc->iMarginLeftRight);
         pCalc.y -= 2 * pc->iMarginTopBottom;

         // see what it comes out at
         if (pControl->m_pNode->ContentNum()) {
            pc->pTextBlock = pControl->TextBlock (p->hDC, pControl->m_pNode, pCalc.x, FALSE, TRUE);
            if (!pc->pTextBlock)
               return FALSE;
            // set the size we want
            p->iWidth = pGiven.x ? pGiven.x : (pc->pTextBlock->m_iCalcWidth + LEFTSIDE + pc->iMarginLeftRight);
            p->iHeight = pGiven.y ? pGiven.y : (max(pc->fShowButton ? pc->iButtonHeight : 0, pc->pTextBlock->m_iCalcHeight) + 2*pc->iMarginTopBottom );
         }
         else {
            // set the size we want
            p->iWidth = pGiven.x ? pGiven.x : (LEFTSIDE + pc->iMarginLeftRight);
            p->iHeight = pGiven.y ? pGiven.y : ((pc->fShowButton ? pc->iButtonHeight : 0) + 2*pc->iMarginTopBottom );
         }
         pc->fRecalcText = FALSE;


         // postinterpret, moving the text to the right of the butotn.
         RECT rFull;
         rFull.left = rFull.top = 0;
         rFull.right = p->iWidth;
         rFull.bottom = p->iHeight;
         int   iDeltaY;
         if (iVCenter < 0)
            iDeltaY = pc->iMarginTopBottom;
         else if (iVCenter > 0)
            iDeltaY = p->iHeight - pc->pTextBlock->m_iCalcHeight - pc->iMarginTopBottom;
         else
            iDeltaY = p->iHeight / 2 - (pc->pTextBlock ? (pc->pTextBlock->m_iCalcHeight)/2 : 0);
         if (pc->pTextBlock)
            pc->pTextBlock->PostInterpret (LEFTSIDE, iDeltaY, &rFull);

      }
      return TRUE;

   case ESCM_RADIOWANTFOCUS:
      {
         PESCMRADIOWANTFOCUS p = (PESCMRADIOWANTFOCUS) pParam;
         // p->fAccept defaults to TRUE

         // sent to see if want focus because we're a group
         if (pc->fChecked)
            return TRUE;

         // else, see if anyone has focus
         if (!pc->pszGroup)
            return TRUE;
         WCHAR *pCur, *pComma;
         WCHAR c;
         for (pCur = pc->pszGroup; pCur && *pCur; pCur = pComma ? (pComma+1) : NULL) {
            pComma = wcschr(pCur, L',');
            if (pComma) {
               c = *pComma;
               *pComma = NULL;
            }

            PCEscControl   pCont;
            PCEscControl   *ppCont;
            ppCont = (PCEscControl*) pControl->m_pParentPage->m_treeControls.Find(pCur);
            if (pComma)
               *pComma = c;

            if (!ppCont)
               continue;
            pCont = *ppCont;
            if (pCont == pControl)
               continue;

            // else, make sure it's unchecked
            DWORD dwNeeded;
            WCHAR szValue[16];
            BOOL  fRet;
            pCont->AttribGet (L"checked", szValue, sizeof(szValue), &dwNeeded);
            AttribToYesNo(szValue, &fRet);
            if (fRet) {
               p->fAcceptFocus = FALSE;
               return TRUE;
            }
         }
      }
      return TRUE;

   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;

#ifdef _DEBUG
         if (pControl->m_pszName && !wcsicmp(pControl->m_pszName, L"down"))
            OutputDebugString ("Down\r\n");
#endif

         BOOL  fCurrentState;
         if (pc->fCheckBox || pc->fRadioButton)
            fCurrentState = pc->fChecked;
         else
            fCurrentState = FALSE;
         BOOL  fDrawDown;
         if (!pc->fRadioButton)
            fDrawDown = (pControl->m_fMouseOver & pControl->m_fLButtonDown) ? !fCurrentState : fCurrentState;
         else
            fDrawDown = (pControl->m_fMouseOver & pControl->m_fLButtonDown) ? TRUE : fCurrentState;

         // BUGBUG - 2.0 - if attributes changed may need to recalculate the
         // button's textblock.

         int   iVCenter = 0;
         if (!wcsicmp (pc->pszVCenter, L"top"))
            iVCenter = -1;
         else if (!wcsicmp (pc->pszVCenter, L"bottom"))
            iVCenter = 1;

         // highlight if checked & highlight color
         if ((pc->cHighlight != (DWORD)-1) && pc->fChecked) {
            HBRUSH hbr;
            hbr = CreateSolidBrush (pc->cHighlight);
            FillRect (p->hDC, &p->rControlHDC, hbr);
            DeleteObject (hbr);
         }
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
            r.left = p->rControlHDC.left;
            if (iVCenter < 0)
               r.top = p->rControlHDC.top;
            else if (iVCenter > 0)
               r.top = p->rControlHDC.bottom - 2*pc->iMarginTopBottom - pc->iButtonHeight;
            else
               r.top = (p->rControlHDC.bottom + p->rControlHDC.top) / 2 - pc->iButtonHeight / 2 - pc->iMarginTopBottom;
            r.right = r.left + pc->iButtonWidth + 2*pc->iMarginLeftRight;
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

            Draw3DButton (pc->pRender, BASECOLOR, BUTTONCOLOR, pc->pszStyle, L"cylinder", fDrawDown);

            pc->pRender->Commit (p->hDC, &r);
         }

      }
      return TRUE;



   case ESCM_LBUTTONDOWN:
      // beep
      pControl->m_pParentPage->m_pWindow->Beep (
         (pc->fRadioButton || pc->fCheckBox) ? ESCBEEP_RADIOCLICK : ESCBEEP_BUTTONDOWN);

      pControl->Invalidate ();
      return TRUE;


   case ESCM_LBUTTONUP:
      // beep
      if (!(pc->fRadioButton || pc->fCheckBox))
         pControl->m_pParentPage->m_pWindow->Beep (ESCBEEP_BUTTONUP);

      // actually do something when click
      if (pControl->m_fMouseOver)
         pControl->Message (ESCM_PRESSED);

      // don't click if lift up outside the button area
      pControl->Invalidate ();
      return TRUE;
   }

   return FALSE;
}

// BUGBUG - 2.0 - way to have bitmap graphics for buttons/scroll bars?

// BUGBUG - 2.0 - if don't have 3D button in it, verical center of text doesn't seem to
// work properly
