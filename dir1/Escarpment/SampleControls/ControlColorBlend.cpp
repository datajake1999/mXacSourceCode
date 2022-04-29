/***********************************************************************
ControlColorBlend.cpp - Code for a control

begun 3/31/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include "escarpment.h"
#include "resleak.h"

typedef struct {
   PWSTR       pszHRef;
} COLORBLEND, *PCOLORBLEND;

/***********************************************************************
Control callback
*/
BOOL ControlColorBlend (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   COLORBLEND  *p = (COLORBLEND*) pControl->m_mem.p;
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(COLORBLEND));
         p = (COLORBLEND*) pControl->m_mem.p;
         memset (p, 0, sizeof(COLORBLEND));
         p->pszHRef = NULL;
         pControl->AttribListAddString (L"href", &p->pszHRef);
      }
      return TRUE;

   case ESCM_INITCONTROL:
      {
         // if this has a href then want mouse
         pControl->m_fWantMouse = p->pszHRef ? TRUE : FALSE;
         if (pControl->m_fWantMouse)
            pControl->m_dwWantFocus = 1;

         // secify that accept space or enter
         if (pControl->m_dwWantFocus) {
            ESCACCELERATOR a;
            memset (&a, 0, sizeof(a));
            a.c = L' ';
            a.dwMessage = ESCM_SWITCHACCEL;
            pControl->m_listAccelFocus.Add (&a);
            a.c = L'\n';
            pControl->m_listAccelFocus.Add (&a);
         }
      }
      return TRUE;


   case ESCM_LBUTTONDOWN:
      {
         if (p->pszHRef) {
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_LINKCLICK);

            // must release capture or bad things happen
            pControl->m_pParentPage->MouseCaptureRelease(pControl);

            pControl->m_pParentPage->Link (p->pszHRef);
         }
      }
      return TRUE;

   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;
         COLORREF ul, ll, ur, lr;
         COLORREF cr;

         // get the color from the attributes
         ul = ll = ur = lr = RGB(0,0,0xff);
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"color"), &cr))
            ul = ll = ur = lr = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"lcolor"), &cr))
            ul = ll = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"rcolor"), &cr))
            ur = lr = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"tcolor"), &cr))
            ul = ur = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"bcolor"), &cr))
            ll = lr = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"ulcolor"), &cr))
            ul = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"llcolor"), &cr))
            ll = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"urcolor"), &cr))
            ur = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"lrcolor"), &cr))
            lr = cr;

         // paint
         HBRUSH   hbr;

      #define  COLORDIST(x,y)    (DWORD)(max(x,y)-min(x,y))

         // figure out how many divions across & down need
         DWORD dwAcross, dwDown;
         dwAcross = COLORDIST(GetRValue(ul), GetRValue(ur));
         dwAcross = max(dwAcross, COLORDIST(GetGValue(ul), GetGValue(ur)));
         dwAcross = max(dwAcross, COLORDIST(GetBValue(ul), GetBValue(ur)));
         dwAcross = max(dwAcross, COLORDIST(GetRValue(ll), GetRValue(lr)));
         dwAcross = max(dwAcross, COLORDIST(GetGValue(ll), GetGValue(lr)));
         dwAcross = max(dwAcross, COLORDIST(GetBValue(ll), GetBValue(lr)));

         dwDown = COLORDIST(GetRValue(ul), GetRValue(ll));
         dwDown = max(dwDown, COLORDIST(GetGValue(ul), GetGValue(ll)));
         dwDown = max(dwDown, COLORDIST(GetBValue(ul), GetBValue(ll)));
         dwDown = max(dwDown, COLORDIST(GetRValue(ur), GetRValue(lr)));
         dwDown = max(dwDown, COLORDIST(GetGValue(ur), GetGValue(lr)));
         dwDown = max(dwDown, COLORDIST(GetBValue(ur), GetBValue(lr)));

         if (((dwAcross+1)*(dwDown+1)) > 100) {
            // divide by 4 - mach banding
            dwAcross /= 4;
            dwDown /= 4;
         }
         else {
            dwAcross /= 2;
            dwDown /= 2;
         }

         // also, no more than a trasition every 4? pixels
         dwAcross = min((DWORD) (p->rControlHDC.right - p->rControlHDC.left) / 4, dwAcross);
         dwDown = min((DWORD) (p->rControlHDC.bottom - p->rControlHDC.top) / 4, dwDown);

         // at least 2
         dwAcross = max(dwAcross,2);
         dwDown = max(dwDown, 2);

         // loop
         DWORD x, y;
         double   iLeft, iTop;
         double   iDeltaX, iDeltaY;
         RECT  r;
         iTop = p->rControlHDC.top;
         iDeltaX = (double) (p->rControlHDC.right - p->rControlHDC.left) / (double) dwAcross;
         iDeltaY = (double) (p->rControlHDC.bottom - p->rControlHDC.top) / (double) dwDown;

      #define ALPHA(x,y,amt)     (BYTE)((1.0-(amt))*(x) + (amt)*(y))
         for (y = 0; y < dwDown; y++, iTop += iDeltaY ) {
            // rectangle
            iLeft = p->rControlHDC.left;
            r.top = (int) iTop;
            r.bottom = (int) (iTop + iDeltaY);
            if (y+1 == dwDown)
               r.bottom = p->rControlHDC.bottom;  // ensure theres no roundoff error

            // if not in refresh area then skip
            if ((r.bottom < p->rInvalidHDC.top) || (r.top > p->rInvalidHDC.bottom))
               continue;

            COLORREF cLeft, cRight;
            double   fAlpha;
            fAlpha = (double) y / (double) (dwDown-1);
            cLeft = RGB(
               ALPHA(GetRValue(ul),GetRValue(ll),fAlpha),
               ALPHA(GetGValue(ul),GetGValue(ll),fAlpha),
               ALPHA(GetBValue(ul),GetBValue(ll),fAlpha)
               );
            cRight = RGB(
               ALPHA(GetRValue(ur),GetRValue(lr),fAlpha),
               ALPHA(GetGValue(ur),GetGValue(lr),fAlpha),
               ALPHA(GetBValue(ur),GetBValue(lr),fAlpha)
               );

            for (x = 0; x < dwAcross; x++, iLeft += iDeltaX) {
               // rectangle
               r.left = (int) iLeft;
               r.right = (int) (iLeft + iDeltaX);
               if (x+1 == dwAcross)
                  r.right = p->rControlHDC.right;  // ensure theres no roundoff error

               // if not in refresh area then skip
               if ((r.right < p->rInvalidHDC.left) || (r.left > p->rInvalidHDC.right))
                  continue;

               COLORREF c;
               double   fAlpha;
               fAlpha = (double) x / (double) (dwAcross-1);
               c = RGB(
                  ALPHA(GetRValue(cLeft),GetRValue(cRight),fAlpha),
                  ALPHA(GetGValue(cLeft),GetGValue(cRight),fAlpha),
                  ALPHA(GetBValue(cLeft),GetBValue(cRight),fAlpha)
               );

               // paint it
               hbr = CreateSolidBrush (c);
               FillRect (p->hDC, &r, hbr);
               DeleteObject (hbr);
            }
         }
      }
      return TRUE;


   }

   return FALSE;
}

// BUGBUG - 2.0 - need a way of making a watermark color blend
