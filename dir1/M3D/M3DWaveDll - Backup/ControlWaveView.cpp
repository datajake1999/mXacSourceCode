/***********************************************************************
ControlWaveView.cpp - Code for a control

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

#define ESCM_NEWSCROLL        (ESCM_USER+2)

typedef struct {
   PCM3DWave      pWave;      // wave file
   PCListFixed    plMarkers;  // marker locations
   
   int            iDisplay;   // what to display, 0 = wave, 1 = fft, 2 = features
   int            iZoom;      // amount to zoom, 0 = full out, 10 = zoomed far in
   int            iScrollX;   // amount of scrolling horizontal. 0 = no. positive => number of samples
   int            iBorderSize;    // border width
   COLORREF       cBackground;   // background color
   COLORREF       cBorder;    // border color, default blue
   PWSTR          pszHScroll;  // scroll bar name
   PWSTR          pszVScroll; // vertical scroll bar name
   int            iDragging;  // which marker is being dragged, -1 if not

   CEscControl    *pHScroll;      // horizontal scroll control
   CEscControl    *pVScroll;      // vertical scroll control
   POINT          pLastMouse;    // last recorded mouse position
} WAVEVIEW, *PWAVEVIEW;

/***********************************************************************
ZoomToSamples - Zoom to number of samples
*/
static int ZoomToSamples (PWAVEVIEW pc)
{
   if (!pc->pWave)
      return 1;

   double fSamples = (double)pc->pWave->m_dwSamples * pow (0.5, (double)pc->iZoom/20.0);
   fSamples = max(fSamples, 1);
   return (int)fSamples;
}


/***********************************************************************
MiscUpdate - Does miscellaenous update
   - Make sure selection is within range

inputs
   PCEscControl      pControl
   PWAVEVIEW             pc - info
*/
static void MiscUpdate (PCEscControl pControl, PWAVEVIEW pc)
{
   // do checks to make sure selection is OK & stuff like that
   pc->iZoom = min(pc->iZoom, 100);
   pc->iZoom = max(0, pc->iZoom);
   int iSamples = (int)(pc->pWave ? pc->pWave->m_dwSamples : 0);
   int iWindow = ZoomToSamples (pc);
   iSamples -= iWindow;
   iSamples = max(iSamples, 0);
   pc->iScrollX = min(pc->iScrollX, iSamples);
   pc->iScrollX = max(0, pc->iScrollX);
}

/***********************************************************************
UpdateScroll - Updates teh scroll bars to the new info

inputs
   PCEscControl      pControl
   PWAVEVIEW             pc - info
   BOOL              fAll - If TRUE then update both scrollbars, else only horizontal
*/
static void UpdateScroll (PCEscControl pControl, PWAVEVIEW pc, BOOL fAll = TRUE)
{
   // find hscroll & vscroll
   if (pc->pszHScroll && !pc->pHScroll) {
      pc->pHScroll = pControl->m_pParentPage->ControlFind (pc->pszHScroll);
      if (pc->pHScroll) {
         pc->pHScroll->AttribSet (L"min", L"0");
         pc->pHScroll->m_pParentControl = pControl;
      }
   }
   if (pc->pszVScroll && !pc->pVScroll) {
      pc->pVScroll = pControl->m_pParentPage->ControlFind (pc->pszVScroll);
      if (pc->pVScroll) {
         pc->pVScroll->AttribSet (L"min", L"0");
         pc->pVScroll->AttribSet (L"max", L"100");
         pc->pVScroll->AttribSet (L"page", L"1");
         pc->pVScroll->m_pParentControl = pControl;
      }
   }

   // set their new values
   WCHAR szTemp[64];
   if (pc->pHScroll) {
      swprintf (szTemp, L"%d", (int)(pc->pWave ? pc->pWave->m_dwSamples : 0));
      pc->pHScroll->AttribSet (L"max", szTemp);
      swprintf (szTemp, L"%d", ZoomToSamples (pc));
      pc->pHScroll->AttribSet (L"page", szTemp);
      swprintf (szTemp, L"%d", pc->iScrollX);
      pc->pHScroll->AttribSet (L"pos", szTemp);
   }
   if (fAll && pc->pVScroll) {
      //swprintf (szTemp, L"%d", LinesVisible (pControl, pc, TRUE));
      //pc->pVScroll->AttribSet (L"page", szTemp);
      //swprintf (szTemp, L"%d", pc->iNumLines);
      //pc->pVScroll->AttribSet (L"max", szTemp);
      swprintf (szTemp, L"%d", pc->iZoom);
      pc->pVScroll->AttribSet (L"pos", szTemp);
   }

}

/***********************************************************************
WhatMarkerOver - Returns the marker number its over, or -1 if none
*/
static int WhatMarkerOver (PCEscControl pControl, PWAVEVIEW pc, PVOID pParam)
{
   ESCMLBUTTONDOWN *p = (ESCMLBUTTONDOWN*)pParam;
   if (pc->iDragging != -1)
      return pc->iDragging;

   // figure out what sample this is over
   RECT rWave;
   rWave = pControl->m_rPosn;
   rWave.left += pc->iBorderSize;
   rWave.right -= pc->iBorderSize;
   rWave.right = max(rWave.right, rWave.left+1);   // so no divide by 0
   int iZoom = ZoomToSamples (pc);
   iZoom = max(iZoom, 1);

   // determine location
   double fMaxDelta = 3 /*pixels*/ / (double)(rWave.right - rWave.left) * (double)iZoom;
   double fSample = (double)(p->pPosn.x - rWave.left) / (double)(rWave.right - rWave.left);
   fSample = fSample * (double)iZoom + (double)pc->iScrollX;

   // loop
   DWORD i;
   PWAVEVIEWMARKER pMark = (PWAVEVIEWMARKER) pc->plMarkers->Get(0);
   fp fBestDist = 0;
   int iOver =-1;
   for (i = 0; i < pc->plMarkers->Num(); i++) {
      fp fThis = pMark[i].dwSample;
      if ((fThis < fSample - fMaxDelta) || (fThis > fSample + fMaxDelta))
         continue;   // too far away

      // look at distance
      fThis = fabs(fThis - fSample);
      // else, if its the closest take it
      if ((iOver == -1) || (fThis < fBestDist)) {
         iOver = (int)i;
         fBestDist = fThis;
      }
   } // i

   return iOver;
}


/***********************************************************************
Control callback
*/
BOOL ControlWaveView (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   WAVEVIEW  *pc = (WAVEVIEW*) pControl->m_mem.p;
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(WAVEVIEW));
         pc = (WAVEVIEW*) pControl->m_mem.p;
         memset (pc, 0, sizeof(WAVEVIEW));
         pc->iBorderSize = 2;
         pc->cBackground = RGB(0xe0, 0xe0, 0xff); // (DWORD)-1;  // transparent
         pc->cBorder = RGB(0, 0, 0xff);
         pc->iDragging = -1;

         pc->plMarkers = new CListFixed;
         pc->plMarkers->Init (sizeof(WAVEVIEWMARKER));

         // all the attributes
         pControl->AttribListAddDecimal (L"border", &pc->iBorderSize, NULL, TRUE);
         pControl->AttribListAddDecimal (L"display", &pc->iDisplay, NULL, TRUE);
         pControl->AttribListAddColor (L"bordercolor", &pc->cBorder, NULL, TRUE);
         pControl->AttribListAddColor (L"color", &pc->cBackground, NULL, TRUE);
         pControl->AttribListAddDecimal (L"zoom", &pc->iZoom, NULL, TRUE, ESCM_NEWSCROLL);
         pControl->AttribListAddDecimal (L"scrollx", &pc->iScrollX, NULL, TRUE, ESCM_NEWSCROLL);
         pControl->AttribListAddString (L"hscroll", &pc->pszHScroll, NULL, TRUE);
         pControl->AttribListAddString (L"vscroll", &pc->pszVScroll, NULL, TRUE);

      }
      return TRUE;

   case ESCM_DESTRUCTOR:
      if (pc->plMarkers)
         delete pc->plMarkers;
      return TRUE;

   case ESCM_INITCONTROL:
      {
         // note that want keyboard
         pControl->m_fWantMouse = TRUE;
         pControl->m_dwWantFocus = 0;
      }
      return TRUE;

   case ESCM_QUERYSIZE:
      {
         PESCMQUERYSIZE p = (PESCMQUERYSIZE) pParam;

         // if had ruthers...
         p->iWidth = p->iDisplayWidth; // full of screen
         p->iHeight = p->iWidth / 4;
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

         RECT  rWave;
         int iZoom = ZoomToSamples (pc);
         if (pc->pWave) {
            rWave = p->rControlHDC;
            rWave.left += pc->iBorderSize;
            rWave.top += pc->iBorderSize;
            rWave.right -= pc->iBorderSize;
            rWave.bottom -= pc->iBorderSize;

            BOOL fWhiteText = TRUE;

            switch (pc->iDisplay) {
            default:
            case 0:  // paint wave
               fWhiteText = FALSE;
               pc->pWave->PaintWave (p->hDC, &rWave,
                  pc->iScrollX, (fp)pc->iScrollX + iZoom,
                  32767, -32768, 0 /* channel */);
               break;

            case 1:  // FFT
               pc->pWave->PaintFFT (p->hDC, &rWave,
                  pc->iScrollX, (fp)pc->iScrollX + iZoom,
                  0.5, 1, 0 /*channel*/,
                  pControl->m_pParentPage ? pControl->m_pParentPage->m_pWindow->m_hWnd : NULL, 0);
               break;

            case 2:  // SR feature
               pc->pWave->PaintFFT (p->hDC, &rWave,
                  pc->iScrollX, (fp)pc->iScrollX + iZoom,
                  0, 1, 0 /*channel*/,
                  pControl->m_pParentPage ? pControl->m_pParentPage->m_pWindow->m_hWnd : NULL, 4);
               break;
            } // swithc iDisplay

            if (iZoom && pc->pWave) {
               CMatrix mPixelToTime, mTimeToPixel;
               fp fScale;
               fScale = (fp)iZoom / (fp)(rWave.right-rWave.left) / (fp)pc->pWave->m_dwSamplesPerSec;
               mPixelToTime.Scale (fScale, fScale, 1);
               fScale = (fp)pc->iScrollX / (fp)pc->pWave->m_dwSamplesPerSec;
               mTimeToPixel.Translation (fScale, fScale, 0);
               mPixelToTime.MultiplyRight (&mTimeToPixel);
               mPixelToTime.Invert4 (&mTimeToPixel);
               TimelineTicks (p->hDC, &rWave, &mPixelToTime, &mTimeToPixel,
                     fWhiteText ? RGB(0xc0,0xc0,0xc0) : RGB(0xc0,0xc0,0xc0),
                     fWhiteText ? RGB(0xff,0xff,0xff) : RGB(0x80,0x80,0x80),
                     (iZoom < 100) ? pc->pWave->m_dwSamplesPerSec : 0);
            }
         }

         // draw all the markers
         DWORD i;
         PWAVEVIEWMARKER pMark = (PWAVEVIEWMARKER) pc->plMarkers->Get(0);
         if (iZoom && pc->pWave) for (i = 0; i < pc->plMarkers->Num(); i++, pMark++) {
            // figure out its location
            double fLoc = ((double)pMark->dwSample - (double)pc->iScrollX) / (double)iZoom;
            fLoc = fLoc * (double)(rWave.right-rWave.left) + (double)rWave.left;
            if ((fLoc < (double)rWave.left) || (fLoc >= (double)rWave.right))
               continue;   // not visible
            int iLoc = (int)fLoc;

            HPEN hPenOld, hPen;
            hPen = CreatePen (PS_SOLID, 5, pMark->cColor);
            hPenOld = (HPEN) SelectObject (p->hDC, hPen);

            MoveToEx (p->hDC, iLoc, (rWave.top*3 + rWave.bottom)/4, NULL);
            LineTo (p->hDC, iLoc, (rWave.top + rWave.bottom*3)/4);

            SelectObject (p->hDC, hPenOld);
            DeleteObject (hPen);
         } // i


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

   case ESCM_NEWSCROLL:
      // sent when app changes the scroll position
      MiscUpdate (pControl, pc);
      UpdateScroll (pControl, pc);
      pControl->Invalidate ();
      return TRUE;

   case ESCM_WAVEVIEW:
      {
         PESCMWAVEVIEW p = (PESCMWAVEVIEW) pParam;

         pc->pWave = p->pWave;
         pc->iZoom = 0;
         pc->iScrollX = 0;
         MiscUpdate (pControl, pc);
         UpdateScroll (pControl, pc);
         pControl->Invalidate ();
      }
      return TRUE;

   case ESCM_WAVEVIEWMARKERSSET:
      {
         PESCMWAVEVIEWMARKERSSET p = (PESCMWAVEVIEWMARKERSSET) pParam;

         pc->plMarkers->Init (sizeof(WAVEVIEWMARKER), p->paMarker, p->dwNum);
         pControl->Invalidate ();
      }
      return TRUE;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         if (p->pControl == pc->pHScroll) {
            pc->iScrollX = p->iPos;
            MiscUpdate (pControl, pc);
            pControl->Invalidate();
         }
         else if (p->pControl == pc->pVScroll) {
            pc->iZoom = p->iPos;
            MiscUpdate (pControl, pc);
            UpdateScroll (pControl, pc, FALSE);
            pControl->Invalidate();
         }
      }
      return TRUE;

   case ESCM_LBUTTONDOWN:
      {
         ESCMLBUTTONDOWN *p = (ESCMLBUTTONDOWN*)pParam;

         // if not dragging then ignore
         if (pc->iDragging != -1)
            return TRUE;

         // if disabled then beep
         if (!pc->pWave) {
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_DONTCLICK);
            return FALSE;
         }

         pc->iDragging = WhatMarkerOver (pControl, pc, pParam);
         if (pc->iDragging == -1) {
            // nothing near
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_DONTCLICK);
            return TRUE;
         }

         // beep so know dragging
         pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_SCROLLDRAGSTART);

      }
      return TRUE;

   case ESCM_MOUSEMOVE:
   case ESCM_LBUTTONUP:
      // set the cursor
      int iOver = WhatMarkerOver (pControl, pc, pParam);
      pControl->m_pParentPage->SetCursor ((iOver != -1) ? IDC_HANDCURSOR : IDC_NOCURSOR);

      if ((pc->iDragging >= 0) && (pc->iDragging < (int)pc->plMarkers->Num()) && pc->pWave) {
         // we've moved
         ESCMMOUSEMOVE *p = (ESCMMOUSEMOVE*)pParam;

         // figure out what sample this is over
         RECT rWave;
         rWave = pControl->m_rPosn;
         rWave.left += pc->iBorderSize;
         rWave.right -= pc->iBorderSize;
         rWave.right = max(rWave.right, rWave.left+1);   // so no divide by 0
         int iZoom = ZoomToSamples (pc);
         iZoom = max(iZoom, 1);

         // convert this to a sample number
         double fSample = (double)(p->pPosn.x - rWave.left) / (rWave.right - rWave.left);
         fSample = fSample * (double)iZoom + (double)pc->iScrollX;
         fSample = min(fSample, (double)pc->pWave->m_dwSamples-1);
         fSample = max(fSample, 0);

         // write this
         PWAVEVIEWMARKER pMark = (PWAVEVIEWMARKER) pc->plMarkers->Get(pc->iDragging);
         pMark->dwSample = (DWORD)fSample;

         // redraw
         pControl->Invalidate();

         // send notificaiton back
         ESCNWAVEVIEWMARKERCHANGED mc;
         memset (&mc, 0, sizeof(mc));
         mc.pControl = pControl;
         mc.dwNum = (DWORD)pc->iDragging;
         mc.dwSample = pMark->dwSample;
         pControl->MessageToParent (ESCN_WAVEVIEWMARKERCHANGED, &mc);
      }
      if (dwMessage == ESCM_LBUTTONUP) {
         if (pc->iDragging != -1)
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_SCROLLDRAGSTOP);
         pc->iDragging = -1;  // to clear

         return TRUE;
      }
      return FALSE;

   } // switch
   return FALSE;
}




