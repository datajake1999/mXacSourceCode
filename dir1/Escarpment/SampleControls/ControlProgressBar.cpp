/***********************************************************************
ControlProgressBar.cpp - Code for a control

begun 4/8/2000 by Mike Rozak
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
   COLORREF       cTrack;
   COLORREF       cStart, cMiddle, cEnd;  // color at start middle end
   int            iArrowLen;              // arrow length in pixels
   int            iTrackMin, iTrackMax;   // min/max values (actually max = iTrackMax - iTrackPage - 1
   int            iPos;
   int            iMargin;                // margin (in pixels) around the control
   int            iRotations;             // number of rotations througout the length
   
   // calculated by Call to CalcInfo
   DWORD          dwOrient;   // 0 for up down, 1 for right left
   int            iPixelMin;     // pixel where min is (in page posn)
   int            iPixelMax;     // pixel where max is (in page posn)
   int            iPixelLength;  // along the scroll direction, like 200
   int            iPixelWidth;   // # pixels across, like 24

   // render
   CRender        *pRender;         // rendering objet
   BOOL           fRecalcText;      // set to TRUE if need to recalc text

   // state into
   RECT           rLastScreen;         // last HDC rectangle (used for rendering)
} PROGRESSBAR, *PPROGRESSBAR;


/***********************************************************************
CalcInfo - calculate some information for the PROGRESSBAR structure

inputs
   PCEscControl      pControl - control
   PPROGRESSBAR        pc - progressbar
*/
static void CalcInfo (PCEscControl pControl, PPROGRESSBAR pc)
{
   if (pc->pszOrient && !wcsicmp(pc->pszOrient, L"vert"))
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
   pc->iPixelLength = max(0, pc->iPixelLength - 2*pc->iMargin - pc->iArrowLen);
   pc->iPixelWidth = max(0, pc->iPixelWidth - 2*pc->iMargin);
   if (pc->iPixelLength <= 1)
      pc->iPixelLength = 1;

   // real maximum
   if (pc->iTrackMax <= pc->iTrackMin)
      pc->iTrackMax = pc->iTrackMin;
   if (pc->iPos >= pc->iTrackMax)
      pc->iPos = pc->iTrackMax - 1;
   if (pc->iPos < pc->iTrackMin)
      pc->iPos = pc->iTrackMin;

   switch (pc->dwOrient) {
   case 0:  // vert
      pc->iPixelMin += pControl->m_rPosn.top;
      pc->iPixelMax += pControl->m_rPosn.top;
      break;
   case 1: // horz
      pc->iPixelMin += pControl->m_rPosn.left;
      pc->iPixelMax += pControl->m_rPosn.left;
      break;
   }

   // done
}


/***********************************************************************
Control callback
*/
BOOL ControlProgressBar (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   PROGRESSBAR *pc = (PROGRESSBAR*) pControl->m_mem.p;

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(PROGRESSBAR));
         pc = (PROGRESSBAR*) pControl->m_mem.p;
         memset (pc, 0, sizeof(PROGRESSBAR));
         pc->iWidth = pc->iHeight = 0;   // so know if it's set
         pc->cTrack = RGB (0x80, 0x080, 0x80);
         pc->cStart = RGB (0xff, 0x00, 0x00);
         pc->cMiddle = RGB(0x00, 0xff, 0x00);
         pc->cEnd = RGB (0x00, 0x00, 0xff);
         pc->iArrowLen = 24;
         pc->iTrackMin = 0;
         pc->iTrackMax = 100;
         pc->iPos = 1;
         pc->iMargin = 6;
         pc->iRotations = 5;

         pControl->AttribListAddDecimalOrPercent (L"width", &pc->iWidth, &pc->fWidthPercent);
         pControl->AttribListAddDecimalOrPercent (L"height", &pc->iHeight, &pc->fHeightPercent);
         pControl->AttribListAddString (L"orient", &pc->pszOrient, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"rotations", &pc->iRotations, FALSE, TRUE);

         pControl->AttribListAddColor (L"trackcolor", &pc->cTrack, FALSE, TRUE);
         pControl->AttribListAddColor (L"startcolor", &pc->cStart, FALSE, TRUE);
         pControl->AttribListAddColor (L"middlecolor", &pc->cMiddle, FALSE, TRUE);
         pControl->AttribListAddColor (L"endcolor", &pc->cEnd, FALSE, TRUE);

         pControl->AttribListAddDecimal (L"min", &pc->iTrackMin, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"max", &pc->iTrackMax, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"pos", &pc->iPos, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"margin", &pc->iMargin, &pc->fRecalcText, TRUE);

         pc->pRender = new CRender;
      }
      return TRUE;

   case ESCM_INITCONTROL:
      {
         CalcInfo (pControl, pc);

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
            pGiven.y = pc->dwOrient ? (32+2*pc->iMargin - 12) : p->iDisplayWidth;

         p->iWidth = pGiven.x;
         p->iHeight = pGiven.y;
         pc->fRecalcText = FALSE;

      }
      return TRUE;

#define DEFAULTCOLOR(x) pc->pRender->m_DefColor[0]=GetRValue(x);pc->pRender->m_DefColor[1]=GetGValue(x);pc->pRender->m_DefColor[2]=GetBValue(x)
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
            pc->pRender->Translate ((double)iX / 2, -iY, 0);
            pc->pRender->Rotate (PI/2, 3);  // rotate so same orienttion as X
            break;

         case 1:  // right left
            // center vertically
            pc->pRender->Translate (0, -(double)iY / 2, 0);
            break;
         }
         pc->pRender->Translate (pc->iMargin, 0, pc->iPixelWidth / 2.0);

         // draw the track
         pnt   ap[2];
         DEFAULTCOLOR (pc->cTrack);
         ap[0][1] = ap[1][1] = 0;
         ap[0][0] = 0;
         ap[1][0] = pc->iPixelLength + pc->iArrowLen;
         ap[0][2] = ap[1][2] = 0;

         pc->pRender->ShapeArrow (2, (double*) ap,
            pc->iPixelWidth / 8.0, FALSE);


#define  PIXELINC       3
         // figure out how many units in 5 pixels
         double   fUnitPer2Pixel;
         fUnitPer2Pixel = (pc->iTrackMax - pc->iTrackMin) / (double) pc->iPixelLength * PIXELINC;
         double   fUnit;
         DEFAULTCOLOR (pc->cStart);
         // how far to go
         double   fMax;
         fMax = pc->iPos;
         // if all done then draw to the end and no arrow
         if (pc->iPos >= (pc->iTrackMax-1))
            fMax = pc->iPos + fUnitPer2Pixel * pc->iArrowLen / PIXELINC;
         pc->pRender->Rotate (PI / 2.0, 1);
         for (fUnit = pc->iTrackMin; fUnit < fMax; fUnit += fUnitPer2Pixel) {
            // change color
            double   fAlpha;
            fAlpha = (fUnit - pc->iTrackMin) / (double) max(pc->iTrackMax - pc->iTrackMin,1);
            if (fAlpha <= .5) {
               fAlpha *= 2;
               pc->pRender->m_DefColor[0] = GetRValue(pc->cStart) * (1.0 - fAlpha) + GetRValue(pc->cMiddle) * fAlpha;
               pc->pRender->m_DefColor[1] = GetGValue(pc->cStart) * (1.0 - fAlpha) + GetGValue(pc->cMiddle) * fAlpha;
               pc->pRender->m_DefColor[2] = GetBValue(pc->cStart) * (1.0 - fAlpha) + GetBValue(pc->cMiddle) * fAlpha;
            }
            else {
               fAlpha = (fAlpha - 0.5) * 2;
               fAlpha = min(1.0, fAlpha);
               pc->pRender->m_DefColor[0] = GetRValue(pc->cMiddle) * (1.0 - fAlpha) + GetRValue(pc->cEnd) * fAlpha;
               pc->pRender->m_DefColor[1] = GetGValue(pc->cMiddle) * (1.0 - fAlpha) + GetGValue(pc->cEnd) * fAlpha;
               pc->pRender->m_DefColor[2] = GetBValue(pc->cMiddle) * (1.0 - fAlpha) + GetBValue(pc->cEnd) * fAlpha;
            }

            // how large?
            double   fSize;
            fSize = min (PIXELINC, (fMax - fUnit) / fUnitPer2Pixel * PIXELINC);

            // BUGFIX - Make rotation and advancement of arrow betting
            double   fRotAlpha;
            fRotAlpha = fSize / PIXELINC;

            // draw the widget
            pc->pRender->MatrixPush();

            // draw box
            pc->pRender->Translate (fSize / 2, pc->iPixelWidth / 4.0, 0);
            pc->pRender->ShapeBox (fSize, pc->iPixelWidth / 2.0, pc->iPixelWidth / 2.0);
            pc->pRender->MatrixPop();

            // advance
            pc->pRender->Translate (PIXELINC * fRotAlpha, 0, 0);

            // rotate
            pc->pRender->Rotate (fUnitPer2Pixel / (double)(pc->iTrackMax - pc->iTrackMin) * pc->iRotations * 2.0 * PI * fRotAlpha, 1);
         }

         // paint eh arrow head
         if (pc->iPos < pc->iTrackMax-1) {
            pc->pRender->MatrixPush();
            pc->pRender->Translate (pc->iArrowLen / 3.0, 0, 0);
            pc->pRender->Rotate (-PI/2, 1);
            pc->pRender->ShapeDeepArrow (pc->iArrowLen, pc->iPixelWidth, pc->iPixelWidth/2.0);
            pc->pRender->MatrixPop();
         }


         // commitit
         pc->pRender->Commit (p->hDC, &p->rControlHDC);
         pc->rLastScreen = p->rControlScreen;
      }
      return TRUE;

   }

   return FALSE;
}

