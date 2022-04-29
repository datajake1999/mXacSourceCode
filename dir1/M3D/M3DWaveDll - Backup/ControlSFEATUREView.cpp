/***********************************************************************
ControlSRFEATUREView.cpp - Code for a control

begun 5/8/2005 by Mike Rozak
Copyright 2005 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "m3dwave.h"


typedef struct {
   SRFEATURE      srfBlack;   // black feature
   SRFEATURE      srfWhite;   // white feature
   fp             afOctaveBands[2][SROCTAVE];   // scaling of octave bands. Each is relative weight for size
                        // [0][x] = voiced, [1][x] = unvoiced
   BOOL           fShowBlack; // show black
   BOOL           fShowWhite; // show shite

   int            iBorderSize;    // border width
   COLORREF       cBackground;   // background color
   COLORREF       cBorder;    // border color, default blue
} SRFEATUREVIEW, *PSRFEATUREVIEW;

/***********************************************************************
Control callback
*/
BOOL ControlSRFEATUREView (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   SRFEATUREVIEW  *pc = (SRFEATUREVIEW*) pControl->m_mem.p;
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(SRFEATUREVIEW));
         pc = (SRFEATUREVIEW*) pControl->m_mem.p;
         memset (pc, 0, sizeof(SRFEATUREVIEW));
         pc->iBorderSize = 2;
         pc->cBackground = RGB(0xe0, 0xe0, 0xff); // (DWORD)-1;  // transparent
         pc->cBorder = RGB(0, 0, 0xff);

         DWORD i;
         for (i = 0; i < SROCTAVE; i++)
            pc->afOctaveBands[0][i] = pc->afOctaveBands[1][i] = 1.0;

         // all the attributes
         pControl->AttribListAddDecimal (L"border", &pc->iBorderSize, NULL, TRUE);
         pControl->AttribListAddColor (L"bordercolor", &pc->cBorder, NULL, TRUE);
         pControl->AttribListAddColor (L"color", &pc->cBackground, NULL, TRUE);

      }
      return TRUE;

   case ESCM_DESTRUCTOR:
      // do nothing
      return TRUE;

   case ESCM_INITCONTROL:
      {
         // note that want keyboard
         pControl->m_fWantMouse = FALSE;
         pControl->m_dwWantFocus = 0;
      }
      return TRUE;

   case ESCM_QUERYSIZE:
      {
         PESCMQUERYSIZE p = (PESCMQUERYSIZE) pParam;

         // if had ruthers...
         p->iWidth = p->iDisplayWidth; // full of screen
         p->iHeight = p->iWidth / 2;
      }
      return FALSE;  // pass onto default handler

   case ESCM_SIZE:
      return FALSE;  // default handler

   case ESCM_FOCUS:
      break;   // fall on through to default handler

   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;

         // figure out invalid
         RECT  rInvalid;
         if (!IntersectRect (&rInvalid, &p->rControlHDC, &p->rInvalidHDC))
            return TRUE;  // nothing invalid

         // paint the background
         if (pc->cBackground != (DWORD)-1) {
            HBRUSH hbr;
            hbr = CreateSolidBrush (pc->cBackground);
            FillRect (p->hDC, &p->rControlHDC, hbr);
            DeleteObject (hbr);
         }

         // rectangle
         RECT  rWave;
         rWave = p->rControlHDC;
         rWave.left += pc->iBorderSize;
         rWave.top += pc->iBorderSize;
         rWave.right -= pc->iBorderSize;
         rWave.bottom -= pc->iBorderSize;

#define SKIPLOWEROCTAVE    0 // SRPOINTSPEROCTAVE // dont draw first octave
#define DBTOP              10.0             // top-most db to display
#define DBBOTTOM           80.0             // bottom-most db to display
#define FREQPOWER          2.0         // to bunch lower octaves

         // draw the octave bands
         DWORD dwColor, i;
         fp fSum = 0, fCur;
         for (i = 0; i < SROCTAVE; i++)
            fSum += pc->afOctaveBands[0][i];
                  // NOTE: Only doing voiced bands. leaving unvoiced alone
         if (fSum) {
            HPEN hPenOld, hPen;
            hPen = CreatePen (PS_SOLID, 0, 
               RGB(GetRValue(pc->cBackground)/2, GetGValue(pc->cBackground)/2, GetBValue(pc->cBackground)/2));
            hPenOld = (HPEN) SelectObject (p->hDC, hPen);

            for (i = 0, fCur = pc->afOctaveBands[0][i]; i < SROCTAVE-1; fCur += pc->afOctaveBands[0][++i]) {
               fp fX = pow (fCur / fSum, (fp)FREQPOWER);  // to reduce emphasis on lower sounds
               fX = fX * (fp)(rWave.right - rWave.left) + (fp)rWave.left;

               MoveToEx (p->hDC, (int)fX, rWave.top, NULL);
               LineTo (p->hDC, (int)fX, rWave.bottom);
            } // i

            SelectObject (p->hDC, hPenOld);
            DeleteObject (hPen);
         } // if fSum

         // draw
         for (dwColor = 1; dwColor < 2; dwColor--) {
            BOOL fDraw = dwColor ? pc->fShowWhite : pc->fShowBlack;
            PSRFEATURE psrf = dwColor ? &pc->srfWhite : &pc->srfBlack;
            if (!fDraw)
               continue;

            HPEN hPenOld, hPen;
            hPen = CreatePen (PS_SOLID, 3, dwColor ? RGB(0xff,0xff,0xff) : RGB(0,0,0));
            hPenOld = (HPEN) SelectObject (p->hDC, hPen);

            // loop over all the points
            for (i = SKIPLOWEROCTAVE; i < SRDATAPOINTS; i++) {
               fp fX = (fp)(i-SKIPLOWEROCTAVE) / (fp)(SRDATAPOINTS-SKIPLOWEROCTAVE);
               fX = pow (fX, (fp)FREQPOWER);  // to reduce emphasis on lower sounds
               fX = fX * (fp)(rWave.right - rWave.left) + (fp)rWave.left;
               fp fY = (-(fp)max(psrf->acNoiseEnergy[i], psrf->acVoiceEnergy[i]) - DBTOP) / (DBBOTTOM - DBTOP) /*db*/;
               //if (fY >= 0)
               //   fY = pow(fY, 0.5);  // so emphasize
               fY = fY * (fp)(rWave.bottom - rWave.top) + (fp)rWave.top;
               fY = min(fY, (fp)rWave.bottom);
               fY = max(fY, (fp)rWave.top);

               if (i > SKIPLOWEROCTAVE)
                  LineTo (p->hDC, (int)fX, (int)fY);
               else
                  MoveToEx (p->hDC, (int)fX, (int)fY, NULL);
            } // i


            SelectObject (p->hDC, hPenOld);
            DeleteObject (hPen);
         } // dwColor

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

   case ESCM_SRFEATUREVIEW:
      {
         PESCMSRFEATUREVIEW p = (PESCMSRFEATUREVIEW) pParam;

         pc->cBackground = p->cColor;
         pc->srfBlack = p->srfBlack;
         pc->srfWhite = p->srfWhite;
         pc->fShowBlack = p->fShowBlack;
         pc->fShowWhite = p->fShowWhite;

         memcpy (pc->afOctaveBands, p->afOctaveBands, sizeof(pc->afOctaveBands));

         pControl->Invalidate ();
      }
      return TRUE;

   } // switch
   return FALSE;
}




