/***********************************************************************
ControlButton.cpp - Code for a control

begun 4/5/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include "escarpment.h"
#include "resleak.h"


#define  ESCM_LEFTARROW          (ESCM_USER+1)
#define  ESCM_RIGHTARROW         (ESCM_USER+2)
#define  ESCM_UPARROW            (ESCM_USER+3)
#define  ESCM_DOWNARROW          (ESCM_USER+4)
#define  ESCM_PAGEDOWN           (ESCM_USER+5)
#define  ESCM_PAGEUP             (ESCM_USER+6)
#define  ESCM_HOME               (ESCM_USER+7)
#define  ESCM_END                (ESCM_USER+8)

typedef struct {
   int            iWidth, iHeight;  // either a number or percent
   BOOL           fWidthPercent, fHeightPercent;   // TRUE if iWidth percent, FALSE if pixels
   PWSTR          pszOrient;
   PWSTR          pszKnobStyle;   // style string
   PWSTR          pszTrackStyle;
   PWSTR          pszArrowStyle;
   COLORREF       cKnob;
   COLORREF       cTrack;
   COLORREF       cArrow;
   COLORREF       cDisabled;
   COLORREF       cHeld;                  // if portion of scroll bar is held
   int            iArrowLen;              // arrow length in pixels
   int            iTrackMin, iTrackMax;   // min/max values (actually max = iTrackMax - iTrackPage - 1
   int            iTrackPage;             // size of the page
   int            iPos;
   int            iMargin;                // margin (in pixels) around the control
   int            iDepth;                 // depth
   
   // calculated by Call to CalcInfo
   DWORD          dwOrient;   // 0 for up down, 1 for right left
   POINT          pKnobCenter;   // where the center of the knob is in page coord (in page posn)
   int            iPixelMin;     // pixel where min is (in page posn)
   int            iPixelMax;     // pixel where max is (in page posn)
   int            iPixelKnob;    // size in pixels
   int            iPixelLength;  // along the scroll direction, like 200
   int            iPixelWidth;   // # pixels across, like 24
   int            iRealMax;      // real scroll position max
   BOOL           fDisabled;     // disabled either because tracking is too large or control disabled

   // render
   CRender        *pRender;         // rendering objet
   BOOL           fRecalcText;      // set to TRUE if need to recalc text

   // state into
   RECT           rLastScreen;         // last HDC rectangle (used for rendering)
   DWORD          dwObjectHeld;     // object clicked on & dragging. 0 if none
   int            iKnobOffset;      // if draggin on knob, offset
} SCROLLBAR, *PSCROLLBAR;


/***********************************************************************
CalcInfo - calculate some information for the SCROLLBAR structure

inputs
   PCEscControl      pControl - control
   PSCROLLBAR        pc - scrollbar
*/
static void CalcInfo (PCEscControl pControl, PSCROLLBAR pc)
{
   if (pc->pszOrient && !_wcsicmp(pc->pszOrient, L"vert"))
      pc->dwOrient = 0;
   else
      pc->dwOrient = 1;

   // from orientation figure width & height
   int   iLen2;
   switch (pc->dwOrient) {
   case 0:  // vert
      pc->iPixelLength = pControl->m_rPosn.bottom - pControl->m_rPosn.top;
      pc->iPixelWidth = pControl->m_rPosn.right - pControl->m_rPosn.left;
      break;
   case 1: // horz
      pc->iPixelWidth = pControl->m_rPosn.bottom - pControl->m_rPosn.top;
      pc->iPixelLength = pControl->m_rPosn.right - pControl->m_rPosn.left;
      break;
   }
   iLen2 = pc->iPixelLength;
   // take off for margins
   pc->iPixelLength = max(0, pc->iPixelLength - 2*pc->iMargin - 2*pc->iArrowLen);
   pc->iPixelWidth = max(0, pc->iPixelWidth - 2*pc->iMargin);

   // how large is the knob
   if (pc->iTrackMax <= pc->iTrackMin)
      pc->iPixelKnob = pc->iPixelLength;
   else
      pc->iPixelKnob = max(pc->iPixelWidth / 2,
         (int) ((double) pc->iTrackPage / (pc->iTrackMax - pc->iTrackMin) * (double) pc->iPixelLength));
   if (pc->iPixelKnob > pc->iPixelLength)
      pc->iPixelKnob = pc->iPixelLength;
   pc->iPixelLength -= pc->iPixelKnob;
   if (pc->iPixelLength < 0)
      pc->iPixelLength = 0;

   // real maximum
   pc->iRealMax = pc->iTrackMax - pc->iTrackPage; // from iMin to iRealMax (exclusive iRealMax)
   if (pc->iRealMax <= pc->iTrackMin)
      pc->iRealMax = pc->iTrackMin;
   if (pc->iPos >= pc->iRealMax)
      pc->iPos = pc->iRealMax; // - 1;
   if (pc->iPos < pc->iTrackMin)
      pc->iPos = pc->iTrackMin;

   // are we disabled because of this
   pc->fDisabled = !pControl->m_fEnabled || (pc->iRealMax <= pc->iTrackMin);

   // so where is the center of the knob
   int   iCenter;
   pc->iPixelMin = pc->iMargin + pc->iArrowLen + pc->iPixelKnob / 2;
   pc->iPixelMax = iLen2 - (pc->iMargin + pc->iArrowLen + pc->iPixelKnob / 2);
   if (pc->iPixelMax <= pc->iPixelMin)
      pc->iPixelMax = pc->iPixelMin+1;
   pc->iPixelLength = pc->iPixelMax- pc->iPixelMin;
   iCenter = (int) ((pc->iPos - pc->iTrackMin) / (double)(max(1,pc->iRealMax-pc->iTrackMin)) *
      pc->iPixelLength);
   iCenter = min(iCenter, pc->iPixelLength);
   iCenter += pc->iPixelMin;

   switch (pc->dwOrient) {
   case 0:  // vert
      pc->pKnobCenter.x = (pControl->m_rPosn.right + pControl->m_rPosn.left) / 2;
      pc->pKnobCenter.y = pControl->m_rPosn.top + iCenter;
      pc->iPixelMin += pControl->m_rPosn.top;
      pc->iPixelMax += pControl->m_rPosn.top;
      break;
   case 1: // horz
      pc->pKnobCenter.y = (pControl->m_rPosn.bottom + pControl->m_rPosn.top) / 2;
      pc->pKnobCenter.x = pControl->m_rPosn.left + iCenter;
      pc->iPixelMin += pControl->m_rPosn.left;
      pc->iPixelMax += pControl->m_rPosn.left;
      break;
   }

   // done
}


#define DEFAULTCOLOR(x) pc->pRender->m_DefColor[0]=GetRValue(x);pc->pRender->m_DefColor[1]=GetGValue(x);pc->pRender->m_DefColor[2]=GetBValue(x)

/***********************************************************************
Control callback
*/
BOOL ControlScrollBar (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   SCROLLBAR *pc = (SCROLLBAR*) pControl->m_mem.p;

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(SCROLLBAR));
         pc = (SCROLLBAR*) pControl->m_mem.p;
         memset (pc, 0, sizeof(SCROLLBAR));
         pc->iWidth = pc->iHeight = 0;   // so know if it's set
         pc->cKnob = RGB(0x70,0x70,0xff);
         pc->cArrow = RGB (0x80, 0x080, 0xc0);
         pc->cTrack = RGB (0x80, 0x080, 0x80);
         pc->cDisabled = RGB (0x80, 0x080, 0x80);
         pc->cHeld = RGB(0xff, 0x40, 0x40);
         pc->iArrowLen = 24;
         pc->iTrackMin = 0;
         pc->iTrackMax = 100;
         pc->iTrackPage = 0;
         pc->iPos = 1;
         pc->iMargin = 6;
         pc->iDepth = 48 / 3; // same as button

         pControl->AttribListAddDecimalOrPercent (L"width", &pc->iWidth, &pc->fWidthPercent);
         pControl->AttribListAddDecimalOrPercent (L"height", &pc->iHeight, &pc->fHeightPercent);
         pControl->AttribListAddString (L"knobstyle", &pc->pszKnobStyle, &pc->fRecalcText, TRUE);
         //pControl->AttribListAddString (L"trackstyle", &pc->pszTrackStyle, &pc->fRecalcText, TRUE);
         pControl->AttribListAddString (L"arrowstyle", &pc->pszArrowStyle, &pc->fRecalcText, TRUE);
         pControl->AttribListAddString (L"orient", &pc->pszOrient, &pc->fRecalcText, TRUE);

         pControl->AttribListAddColor (L"knobcolor", &pc->cKnob, FALSE, TRUE);
         pControl->AttribListAddColor (L"trackcolor", &pc->cTrack, FALSE, TRUE);
         pControl->AttribListAddColor (L"arrowcolor", &pc->cArrow, FALSE, TRUE);
         pControl->AttribListAddColor (L"disabledcolor", &pc->cDisabled, FALSE, TRUE);
         pControl->AttribListAddColor (L"heldcolor", &pc->cHeld, FALSE, TRUE);

         pControl->AttribListAddDecimal (L"depth", &pc->iDepth, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"min", &pc->iTrackMin, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"max", &pc->iTrackMax, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"page", &pc->iTrackPage, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"pos", &pc->iPos, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"margin", &pc->iMargin, &pc->fRecalcText, TRUE);

         pc->pRender = new CRender;
      }
      return TRUE;

   case ESCM_INITCONTROL:
      {
         CalcInfo (pControl, pc);

         // add the acclerators
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = (pc->dwOrient == 0) ? VK_UP : VK_LEFT;;
         a.dwMessage = ESCM_LEFTARROW;
         pControl->m_listAccelFocus.Add (&a);

         a.c = (pc->dwOrient == 0) ? VK_DOWN : VK_RIGHT;
         a.dwMessage = ESCM_RIGHTARROW;
         pControl->m_listAccelFocus.Add (&a);

         a.c = VK_NEXT;
         a.dwMessage = ESCM_PAGEDOWN;
         pControl->m_listAccelFocus.Add (&a);

         a.c = VK_PRIOR;
         a.dwMessage = ESCM_PAGEUP;
         pControl->m_listAccelFocus.Add (&a);

         a.c = VK_HOME;
         a.dwMessage = ESCM_HOME;
         pControl->m_listAccelFocus.Add (&a);

         a.c = VK_END;
         a.dwMessage = ESCM_END;
         pControl->m_listAccelFocus.Add (&a);

         pControl->m_fWantMouse = TRUE;
         pControl->m_dwWantFocus = 1;

         // indicate that we want to be redrawn on a mov
         pControl->m_fRedrawOnMove = TRUE;
      }
      return TRUE;

   case ESCM_DESTRUCTOR:
      if (pc->pRender)
         delete pc->pRender;
      return TRUE;

   case ESCM_QUERYSIZE:
      {
         ESCMQUERYSIZE  *p = (ESCMQUERYSIZE*) pParam;

         // see how large is given
         POINT pGiven;
         pGiven.x = pc->fWidthPercent ? (pc->iWidth * p->iDisplayWidth / 100) : pc->iWidth;
         pGiven.y = pc->fHeightPercent ? (pc->iHeight * p->iDisplayWidth / 100) : pc->iHeight;

         CalcInfo (pControl, pc);

         // what size do we try to calculate for
         if (!pGiven.x)
            pGiven.x = pc->dwOrient ? p->iDisplayWidth : (32 + 2*pc->iMargin - 12);
         if (!pGiven.y)
            pGiven.y = pc->dwOrient ? (32 + 2*pc->iMargin - 12) : p->iDisplayWidth;

         p->iWidth = pGiven.x;
         p->iHeight = pGiven.y;
         pc->fRecalcText = FALSE;

      }
      return TRUE;

   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;

         CalcInfo (pControl, pc);
         pc->pRender->InitForControl(p->hDC, &p->rControlHDC, &p->rControlScreen, &p->rTotalScreen);

         int   iX, iY;
         iX = p->rControlScreen.right - p->rControlScreen.left;
         iY = p->rControlScreen.bottom - p->rControlScreen.top;
         switch (pc->dwOrient) {
         case 0:  // up down
            // center horizontally
            pc->pRender->Translate ((double)iX / 2, 0, 0);
            break;

         case 1:  // right left
            // center vertically
            pc->pRender->Translate (0, -(double)iY / 2, 0);
            break;
         }

         // draw the two end arrows
         DWORD dwShape;
         if (pc->iArrowLen) {
            if (!pc->pszArrowStyle) // L"triangle"
               dwShape = 0;
            else if (!_wcsicmp(pc->pszArrowStyle, L"sphere"))
               dwShape = 1;
            else if (!_wcsicmp (pc->pszArrowStyle, L"cone"))
               dwShape = 2;
            else  // L"triangle"
               dwShape = 0;

            // top arrow
            DEFAULTCOLOR (pc->fDisabled ? pc->cDisabled : pc->cArrow);
            pc->pRender->m_dwMajorObjectID = 2;
            if (pc->dwObjectHeld == pc->pRender->m_dwMajorObjectID) {
               DEFAULTCOLOR (pc->cHeld);
            }
            pc->pRender->MatrixPush();
            switch (pc->dwOrient) {
            case 0:  // up down
               // center horizontally
               pc->pRender->Translate (0, -(pc->iArrowLen/2+pc->iMargin), 0);
               pc->pRender->Rotate (PI/2, 3);
               break;

            case 1:  // right left
               // center vertically
               pc->pRender->Translate (pc->iArrowLen/2 + pc->iMargin, 0, 0);
               pc->pRender->Rotate (PI, 3);
               break;
            }

            // different shapes than just triangles
            switch (dwShape) {
            case 1:  // spehere
               pc->pRender->MeshEllipsoid (pc->iArrowLen/2.0, pc->iPixelWidth/2.0, pc->iDepth);
               pc->pRender->ShapeMeshSurface ();
               break;

            case 2:  // cone
               pc->pRender->Translate (-pc->iArrowLen / 2.0, 0, 0);
               pc->pRender->Rotate (-PI/2, 3);
               pc->pRender->MeshFunnel (pc->iArrowLen, pc->iPixelWidth / 2.0, 0);
               pc->pRender->ShapeMeshSurface ();
               break;

            default: // triangle
               pc->pRender->ShapeDeepTriangle (pc->iArrowLen, pc->iPixelWidth,
                  pc->iArrowLen * 2 / 3, pc->iPixelWidth * 2 / 3, pc->iDepth / 2.0);
            }
            pc->pRender->MatrixPop();

            // rrow at other end
            DEFAULTCOLOR (pc->fDisabled ? pc->cDisabled : pc->cArrow);
            pc->pRender->m_dwMajorObjectID = 3;
            if (pc->dwObjectHeld == pc->pRender->m_dwMajorObjectID) {
               DEFAULTCOLOR (pc->cHeld);
            }
            pc->pRender->MatrixPush();
            switch (pc->dwOrient) {
            case 0:  // up down
               // center horizontally
               pc->pRender->Translate (0, -(p->rControlPage.bottom - p->rControlPage.top -
                  pc->iArrowLen/2 - pc->iMargin), 0);
               pc->pRender->Rotate (-PI/2, 3);
               break;

            case 1:  // right left
               // center vertically
               pc->pRender->Translate (p->rControlPage.right - p->rControlPage.left -
                  pc->iArrowLen/2 - pc->iMargin, 0, 0);
               // no rotation
               break;
            }

            // different shapes than just triangles
            switch (dwShape) {
            case 1:  // spehere
               pc->pRender->MeshEllipsoid (pc->iArrowLen/2.0, pc->iPixelWidth/2.0, pc->iDepth);
               pc->pRender->ShapeMeshSurface ();
               break;

            case 2:  // cone
               pc->pRender->Translate (-pc->iArrowLen / 2.0, 0, 0);
               pc->pRender->Rotate (-PI/2, 3);
               pc->pRender->MeshFunnel (pc->iArrowLen, pc->iPixelWidth / 2.0, 0);
               pc->pRender->ShapeMeshSurface ();
               break;

            default: // triangle
               pc->pRender->ShapeDeepTriangle (pc->iArrowLen, pc->iPixelWidth,
                  pc->iArrowLen * 2 / 3, pc->iPixelWidth * 2 / 3, pc->iDepth / 2.0);
            }

            pc->pRender->MatrixPop();
         }

         // track up to knob
         pc->pRender->m_dwMajorObjectID = 4;
         pc->pRender->MatrixPush();
         DEFAULTCOLOR (pc->fDisabled ? pc->cDisabled : pc->cTrack);
         if (pc->dwObjectHeld == pc->pRender->m_dwMajorObjectID) {
            DEFAULTCOLOR (pc->cHeld);
         }
         
         // different shape than just circular??? Probably not
         pnt   ap[2];
         switch (pc->dwOrient) {
         case 0:  // up down
            ap[0][0] = ap[1][0] = 0;
            ap[0][1] = -(pc->iArrowLen*3/4 + pc->iMargin);
            ap[1][1] = -(pc->pKnobCenter.y - p->rControlPage.top);
            ap[0][2] = ap[1][2] = 0; // pc->iDepth / 4.0;
            break;

         case 1:  // right left
            ap[0][1] = ap[1][1] = 0;
            ap[0][0] = pc->iArrowLen*3/4 + pc->iMargin;
            ap[1][0] = pc->pKnobCenter.x - p->rControlPage.left;
            ap[0][2] = ap[1][2] = 0; // pc->iDepth / 4.0;
            break;
         }
         pc->pRender->ShapeArrow (2, (double*) ap,
            min(pc->iPixelWidth / 4.01, pc->iDepth / 4.01), min(pc->iPixelWidth / 4.01, pc->iDepth / 4.01),
            FALSE);
         pc->pRender->MatrixPop();

         // rest of track
         pc->pRender->m_dwMajorObjectID = 5;
         pc->pRender->MatrixPush();
         DEFAULTCOLOR (pc->fDisabled ? pc->cDisabled : pc->cTrack);
         if (pc->dwObjectHeld == pc->pRender->m_dwMajorObjectID) {
            DEFAULTCOLOR (pc->cHeld);
         }

         // different shape than just circular? Probably not
         switch (pc->dwOrient) {
         case 0:  // up down
            ap[0][0] = ap[1][0] = 0;
            ap[0][1] = -(p->rControlPage.bottom - p->rControlPage.top -
                  pc->iArrowLen*3/4 - pc->iMargin);
            ap[1][1] = -(pc->pKnobCenter.y - p->rControlPage.top);
            ap[0][2] = ap[1][2] = 0; // pc->iDepth / 4.0;
            break;

         case 1:  // right left
            ap[0][1] = ap[1][1] = 0;
            ap[0][0] = p->rControlPage.right - p->rControlPage.left -
                  pc->iArrowLen*3/4 - pc->iMargin;
            ap[1][0] = pc->pKnobCenter.x - p->rControlPage.left;
            ap[0][2] = ap[1][2] = 0; // pc->iDepth / 4.0;
            break;
         }
         pc->pRender->ShapeArrow (2, (double*) ap,
            min(pc->iPixelWidth / 4.01, pc->iDepth / 4.01), min(pc->iPixelWidth / 4.01, pc->iDepth / 4.01),
            FALSE);
         pc->pRender->MatrixPop();

         // knob
         // only draw it if it's not the full display
         if (pc->iRealMax > pc->iTrackMin) {
            // have a variety of knobs
            if (!pc->pszKnobStyle) // L"knob"
               dwShape = 0;
            else if (!_wcsicmp(pc->pszKnobStyle, L"sphere"))
               dwShape = 1;
            else if (!_wcsicmp(pc->pszKnobStyle, L"beveled"))
               dwShape = 2;
            else if (!_wcsicmp(pc->pszKnobStyle, L"righttriangle"))
               dwShape = 3;
            else if (!_wcsicmp(pc->pszKnobStyle, L"lefttriangle"))
               dwShape = 4;
            else if (!_wcsicmp(pc->pszKnobStyle, L"box"))
               dwShape = 5;
            else if (!_wcsicmp(pc->pszKnobStyle, L"knob"))
               dwShape = 0;
            else
               dwShape = 0;

            pc->pRender->m_dwMajorObjectID = 6;
            pc->pRender->MatrixPush();
            DEFAULTCOLOR (pc->fDisabled ? pc->cDisabled : pc->cKnob);
            if (pc->dwObjectHeld == pc->pRender->m_dwMajorObjectID) {
               DEFAULTCOLOR (pc->cHeld);
            }

            switch (pc->dwOrient) {
            case 0:  // up down
               pc->pRender->Translate (0, -(pc->pKnobCenter.y - p->rControlPage.top), 0);
               // dont need to rotate
               break;

            case 1:  // right left
               pc->pRender->Translate (pc->pKnobCenter.x - p->rControlPage.left, 0, 0);
               pc->pRender->Rotate (PI/2, 3);
               break;
            }
            switch (dwShape) {
            case 1:  // sphere
               pc->pRender->MeshEllipsoid (pc->iPixelWidth/2.0, pc->iPixelKnob/2.0, pc->iDepth);
               pc->pRender->ShapeMeshSurface ();
               break;

            case 2:  // bevelled
               pc->pRender->ShapeFlatPyramid (pc->iPixelWidth, pc->iPixelKnob,
                  pc->iPixelWidth * 2.0 / 3, pc->iPixelKnob * 2.0 / 3, pc->iDepth);
               break;
            case 3:  // righttriangle
               pc->pRender->ShapeDeepTriangle (pc->iPixelWidth, pc->iPixelKnob,
                  pc->iPixelWidth * 2.0 / 3, pc->iPixelKnob * 2.0 / 3, pc->iDepth);
               break;
            case 4:  // lefttriangle
               pc->pRender->Rotate (PI, 3);
               pc->pRender->ShapeDeepTriangle (pc->iPixelWidth, pc->iPixelKnob,
                  pc->iPixelWidth * 2.0 / 3, pc->iPixelKnob * 2.0 / 3, pc->iDepth);
               break;
            case 5:  // box
               pc->pRender->ShapeFlatPyramid (pc->iPixelWidth, pc->iPixelKnob,
                  pc->iPixelWidth , pc->iPixelKnob, pc->iDepth);
               break;
            default: // knob
               pc->pRender->Scale (pc->iPixelWidth / 2, pc->iPixelKnob / 2, pc->iDepth);
               pc->pRender->Rotate (PI / 2, 1);
               pc->pRender->MeshFunnel (.7, 1, 1);
               pc->pRender->ShapeMeshSurface ();
               pc->pRender->Translate (0, .7, 0);
               pc->pRender->Rotate (PI/2, 3);
               pc->pRender->MeshEllipsoid (.4, 1, 1);
               pc->pRender->ShapeMeshSurface ();
            }
            pc->pRender->MatrixPop();
         }

         // commitit
         pc->pRender->Commit (p->hDC, &p->rControlHDC);
         pc->rLastScreen = p->rControlScreen;
      }
      return TRUE;



   case ESCM_LBUTTONDOWN:
      {
         ESCMLBUTTONDOWN *p = (ESCMLBUTTONDOWN*)pParam;
         CalcInfo (pControl, pc);

         // if disabled then beep
         if (pc->fDisabled) {
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_DONTCLICK);
            return TRUE;
         }

         // see which object we're over
         pc->dwObjectHeld = 0;
         DWORD dwMinor;
         pc->pRender->ObjectGet (p->pPosn.x - pControl->m_rPosn.left + pc->rLastScreen.left,
            p->pPosn.y - pControl->m_rPosn.top + pc->rLastScreen.top,
            &pc->dwObjectHeld, &dwMinor);
         if (!pc->dwObjectHeld) {
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_DONTCLICK);
            return FALSE;
         }

         // if capturing the knob remember offset
         switch (pc->dwObjectHeld) {
         case 2:  // up/left arrow
            // set the timer so scrolling takes place while the button is down
            pControl->TimerSet (250);
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_SCROLLLINEUP);
            return pControl->Message (ESCM_LEFTARROW);

         case 3:  // right/down arrow
            // set the timer so scrolling takes place while the button is down
            pControl->TimerSet (250);
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_SCROLLLINEDOWN);
            return pControl->Message (ESCM_RIGHTARROW);

         case 4:  // up/left track bar
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_SCROLLPAGEUP);
            return pControl->Message (ESCM_PAGEUP);

         case 5:  // down/right track bar
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_SCROLLPAGEDOWN);
            return pControl->Message (ESCM_PAGEDOWN);

         case 6:  // drag bar
            {
               switch (pc->dwOrient) {
               case 0:  // up down
                  pc->iKnobOffset = p->pPosn.y - pc->pKnobCenter.y;
                  break;

               case 1:  // right left
                  pc->iKnobOffset = p->pPosn.x - pc->pKnobCenter.x;
                  break;
               }

               pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_SCROLLDRAGSTART);

               // invalidate because will draw in a different color
               pControl->Invalidate();
            }
         }


      }
      return TRUE;

   case ESCM_CONTROLTIMER:
      // timer to scroll up/down
      if (pControl->m_fLButtonDown) {
         pControl->m_pParentPage->m_pWindow->Beep((pc->dwObjectHeld == 2) ?
            ESCBEEP_SCROLLLINEUP : ESCBEEP_SCROLLLINEDOWN);
         return pControl->Message ((pc->dwObjectHeld == 2) ? ESCM_LEFTARROW : ESCM_RIGHTARROW);
      }
      else
         pControl->TimerKill();  // spurrious. shouldn have this
      return TRUE;

   case ESCM_MOUSEWHEEL:
      {
         PESCMMOUSEWHEEL p = (PESCMMOUSEWHEEL) pParam;

         // BUGFIX - If scrollbar has focus then scroll

         if (p->zDelta < 0)
            pControl->Message (ESCM_DOWNARROW);
         else
            pControl->Message (ESCM_UPARROW);

      }
      return TRUE;


   case ESCM_LEFTARROW:
   case ESCM_RIGHTARROW:
   case ESCM_UPARROW:
   case ESCM_DOWNARROW:
   case ESCM_PAGEDOWN:
   case ESCM_PAGEUP:
   case ESCM_HOME:
   case ESCM_END:
      {
         int   iDelta;
         switch (dwMessage) {
            case ESCM_LEFTARROW:
            case ESCM_UPARROW:
               iDelta = -max(pc->iTrackPage / 32, 1);
               break;
            case ESCM_RIGHTARROW:
            case ESCM_DOWNARROW:
               iDelta = max(pc->iTrackPage / 32, 1);
               break;
            case ESCM_PAGEDOWN:
               iDelta = max(pc->iTrackPage, 4);
               break;
            case ESCM_PAGEUP:
               iDelta = -max(pc->iTrackPage, 4);
               break;
            case ESCM_HOME:
               iDelta = -(pc->iTrackMax - pc->iTrackMin);
               break;
            case ESCM_END:
               iDelta = pc->iTrackMax - pc->iTrackMin;
               break;
         }

         // increase
         pc->iPos += iDelta;

         // call calc info to check values
         CalcInfo (pControl, pc);

         // redraw
         pControl->Invalidate();

         // semd a message
         ESCNSCROLL s;
         s.pControl = pControl;
         s.iMax = pc->iTrackMax;
         s.iMin = pc->iTrackMin;
         s.iPage = pc->iTrackPage;
         s.iPos = pc->iPos;
         pControl->MessageToParent (ESCN_SCROLL, &s);
      }
      return TRUE;

   case ESCM_MOUSEMOVE:
   case ESCM_LBUTTONUP:
      if (pc->dwObjectHeld == 6) {
         // we've moved
         ESCMMOUSEMOVE *p = (ESCMMOUSEMOVE*)pParam;
         CalcInfo (pControl, pc);

         // find out where we are
         int   iPos;
         switch (pc->dwOrient) {
         case 0:  // up down
            iPos = p->pPosn.y - pc->iKnobOffset;
            break;

         case 1:  // right left
            iPos = p->pPosn.x - pc->iKnobOffset;
            break;
         }

         // min/maxit
         iPos = max(iPos, pc->iPixelMin);
         iPos = min(iPos, pc->iPixelMax);

         // convert it
         iPos = (int) ((double)(iPos - pc->iPixelMin) * (pc->iRealMax - pc->iTrackMin) /
            (double)max(1,pc->iPixelLength)) + pc->iTrackMin;
         pc->iPos = iPos;

         // redraw
         pControl->Invalidate();

         // send a message to parent
         CalcInfo (pControl, pc);   // to make sure position isn't too much
         ESCNSCROLL s;
         s.pControl = pControl;
         s.iMax = pc->iTrackMax;
         s.iMin = pc->iTrackMin;
         s.iPage = pc->iTrackPage;
         s.iPos = pc->iPos;
         pControl->MessageToParent ((dwMessage == ESCM_LBUTTONUP) ? ESCN_SCROLL : ESCN_SCROLLING,
            &s);
      }
      if (dwMessage == ESCM_LBUTTONUP) {
         // kill timer
         pControl->TimerKill();

         if (pc->dwObjectHeld == 6)
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_SCROLLDRAGSTOP);

         // redraw since no longer captures
         pc->dwObjectHeld = 0;
         pControl->Invalidate();
         return TRUE;
      }
      return FALSE;

   }

   return FALSE;
}


// BUGBUG - 2.0 - way to bitmap graphics for buttons/scroll bars?

// BUGBUG - 2.0 - global APIs so an app can set the default scroll bar settings, allowing
// customization of scroll bars in combo boxes, the main window, etc. without hassle

