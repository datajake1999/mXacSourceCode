/****************************************************************************
Paint.cpp - Paints in a window using the TWTEXTELEM and TWOBJECTPOSN cals.

begun 3/18/2000 by Mike Rozak
Copyright 2000 all rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include "tools.h"
#include "fontcache.h"
#include "textwrap.h"
#include "paint.h"
#include "mmlinterpret.h"
#include "jpeg.h"
#include "escarpment.h"
#include "resleak.h"



/****************************************************************************
PaintCell - Paints the cell

inputs
   HDC         hDC - to paint to
   TWOBJECTPOSN   *pop - object into
   POINT       *pOffset - offset all painting to
   CELLOBJECT  *pco - cell object
returns
   none
*/
void PaintCell (HDC hDC, TWOBJECTPOSN *pop, POINT *pOffset, CELLOBJECT *pco)
{
   HBRUSH   hbr;
   RECT     rOffset;
   rOffset = pop->r;
   OffsetRect (&rOffset, pOffset->x, pOffset->y);

   if (pco->cBack != (DWORD)-1L) {
      hbr = CreateSolidBrush (pco->cBack);
      FillRect (hDC, &rOffset, hbr);
      DeleteObject (hbr);
   }

   // do the borders
   hbr = CreateSolidBrush (pco->cEdge);
   RECT  r;
   if (pco->iLeft) {
      r = rOffset;
      r.right = r.left + pco->iLeft;
      FillRect (hDC, &r, hbr);
   }
   if (pco->iRight) {
      r = rOffset;
      r.left = r.right - pco->iRight;
      FillRect (hDC, &r, hbr);
   }
   if (pco->iTop) {
      r = rOffset;
      r.bottom = r.top + pco->iTop;
      FillRect (hDC, &r, hbr);
   }
   if (pco->iBottom) {
      r = rOffset;
      r.top = r.bottom - pco->iBottom;
      FillRect (hDC, &r, hbr);
   }
   DeleteObject (hbr);
}



/****************************************************************************
PaintObject - Paints an object.

inputs
   HDC            hDC - to paint
   TWOBJECTPOSN   *pop - object into
   POINT          *pOffset - to add to xy, to go from page to clip
   POINT          *pDCToScreen - offset to add to convert from DC to screen oords
   RECT           *prClip - clipping region - in DC coords
   RECT           *prTotalScreen - Rectangle specifying entire screen
returns
   none
*/
void PaintObject (HDC hDC, TWOBJECTPOSN *pop, POINT *pOffset, POINT *pDCToScreen,
                  RECT *prClip, RECT *prTotalScreen)
{
   RECT  rOffset;

   if (pop->qwID) {
      DWORD *pdw;
      pdw = (DWORD*) pop->qwID;

      switch (*pdw) {
      case MMLOBJECT_CELL:
         PaintCell (hDC, pop, pOffset, (CELLOBJECT*) pdw);
         return;

      case MMLOBJECT_CONTROL:
         {
            CONTROLOBJECT  *po;
            po = (CONTROLOBJECT*) pdw;
            PCEscControl pControl = (PCEscControl) po->pControl;

            rOffset = pop->r;
            OffsetRect (&rOffset, pOffset->x, pOffset->y);

            // intersect this with clipping region
            RECT  rc;
            if (!IntersectRect (&rc, &rOffset, prClip))
               return;
            rOffset = rc;

            RECT  rPage;
            rPage = rc;
            OffsetRect (&rPage, -pOffset->x, -pOffset->y);

            RECT  rScreen;
            rScreen = rOffset;
            OffsetRect (&rScreen, pDCToScreen->x, pDCToScreen->y);
            pControl->Paint (&rPage, &rOffset, &rScreen, prTotalScreen, hDC);
         }
         return;
      }
   }

   // default behaviour
   HBRUSH hbr;
   hbr = CreateSolidBrush (RGB(0x80, 0x80, 0xff));
   rOffset = pop->r;
   OffsetRect (&rOffset, pOffset->x, pOffset->y);
   FillRect (hDC, &rOffset, hbr);
   DeleteObject (hbr);
}

/****************************************************************************
TextPaint - Paints the text.

  The process is:
   1) Paint backgroundobjects first.
   2) Paint text
   3) Paint foreground objects.

inputs
   HDC      hDC - DC to paint
   POITN    *pOffset - Offset all drawing by this amount ont HDC
   CListVariable  *pListTextElem - All of the text elements (TWTEXTELEM)
                  structures/strings describing what is displayed where. Not sorted.
   CListFixed     *pListObjectPosn - All of the object positions, Not sorted.
   RECT     *prClip - Clip region (in HDC coords).
   RECT     *prWindow - Location of clip region (prClip) in Screen coordinates
   RECT     *prTotalScreen - Rectangle covering entire screen
returns
   none
*/
void TextPaint (HDC hDC, POINT *pOffset, CListVariable *pListTextElem, CListFixed *pListObjectPosn,
                RECT *prClip, RECT *prScreen, RECT *prTotalScreen)
{
   DWORD i;
   TWOBJECTPOSN   *pop;
   TWTEXTELEM     *pte;
   HRGN hRgnOld;

   // might actually want to set a hard clip rectangle
   if (prClip) {
      hRgnOld = CreateRectRgn (-10000, -10000, 10000, 10000);
      GetClipRgn(hDC, hRgnOld);
      IntersectClipRect (hDC, prClip->left, prClip->top, prClip->right, prClip->bottom);
   }

   // figure out where objects located in pre-offset space if they're to
   // fit in the clip rectangle. If an object doesn't appear in there then
   // dont draw it
   RECT  rPre;
   RECT  *prPre;
   if (prClip) {
      prPre = &rPre;
      rPre = *prClip;
      OffsetRect (&rPre, -pOffset->x, -pOffset->y);
   }
   else
      prPre = NULL;

   // determine offset from HDC to screen
   POINT pDCToScreen;
   if (prClip && prScreen) {
      pDCToScreen.x = prScreen->left - prClip->left;
      pDCToScreen.y = prScreen->top - prClip->top;
   }
   else
      pDCToScreen.x = pDCToScreen.y = 0;

   // background objects
   HBRUSH hbr;
   RECT  rIntersect;
   for (i = 0; i < pListObjectPosn->Num(); i++) {
      pop = (TWOBJECTPOSN *) pListObjectPosn->Get(i);

      if (!pop->dwBehind)
         continue;

      // if fhtere's no intersect than continue
      if (prPre && !IntersectRect (&rIntersect, prPre, &pop->r))
         continue;

      // paint it
      PaintObject (hDC, pop, pOffset, &pDCToScreen, prClip, prTotalScreen);
   }

   
   // text
   HFONT hOld;
   hOld = NULL;
   SetBkMode (hDC, TRANSPARENT);
   SetTextAlign (hDC, TA_BASELINE | TA_LEFT);
   CMem  mem;
   for (i = 0; i < pListTextElem->Num(); i++) {
      pte = (TWTEXTELEM*) pListTextElem->Get(i);

      // if fhtere's no intersect than continue
      if (prPre && !IntersectRect (&rIntersect, prPre, &pte->rBack))
         continue;

      // all the right font and stuff
      if (!hOld)
         hOld = (HFONT) SelectObject (hDC, pte->pfi->hFont);
      else
         SelectObject (hDC, pte->pfi->hFont);

      // color
      SetTextColor (hDC, pte->pfi->cText);

      // background color
      RECT  rOffset;
      if (pte->pfi->cBack != (DWORD)-1) {
         hbr = CreateSolidBrush (pte->pfi->cBack);
         rOffset = pte->rBack;
         OffsetRect (&rOffset, pOffset->x, pOffset->y);
         FillRect (hDC, &rOffset, hbr);
         DeleteObject (hbr);
      }

      // BUGBUG - 2.0 - not doing dwFlags for embossed, outline, etc.

      // convert the text
      DWORD dwSize;
      dwSize = ((DWORD)wcslen((WCHAR*)(pte + 1)) + 1) * 2;
      if (!mem.Required (dwSize))
         continue;
      WideCharToMultiByte (CP_ACP, 0, (WCHAR*)(pte+1), -1, (char*)mem.p, dwSize,
         0, 0);
      
      // draw it
      // DrawText (hDC, (char*) mem.p, -1, &pte->r, DT_NOPREFIX | DT_NOCLIP | DT_SINGLELINE);
      ExtTextOut (hDC, pte->r.left + pOffset->x, pte->iBaseline + pOffset->y, 0, NULL, (char*)mem.p, (DWORD)strlen((char*)mem.p), NULL);

#if 0
      HPEN  hPen;
      hPen = (HPEN) SelectObject (hDC, GetStockObject (BLACK_PEN));
      MoveToEx (hDC, pte->r.left, pte->r.top, NULL);
      LineTo (hDC, pte->r.right, pte->r.top);
      LineTo (hDC, pte->r.right, pte->r.bottom);
      LineTo (hDC, pte->r.left, pte->r.bottom);
      LineTo (hDC, pte->r.left, pte->r.top);
      MoveToEx (hDC, pte->r.left, pte->iBaseline, NULL);
      LineTo (hDC, pte->r.right, pte->iBaseline);
      SelectObject (hDC, hPen);
#endif
   }
   if (!hOld)
      SelectObject (hDC, hOld);

   // foreground objects
   for (i = 0; i < pListObjectPosn->Num(); i++) {
      pop = (TWOBJECTPOSN *) pListObjectPosn->Get(i);

      if (pop->dwBehind)
         continue;

      // if fhtere's no intersect than continue
      if (prPre && !IntersectRect (&rIntersect, prPre, &pop->r))
         continue;

      // paint it
      PaintObject (hDC, pop, pOffset, &pDCToScreen, prClip, prTotalScreen);
   }

   // undo the clip
   if (prClip) {
      SelectClipRgn (hDC, hRgnOld);
      DeleteObject (hRgnOld);
   }
}

