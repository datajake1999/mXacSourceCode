/***********************************************************************
ControlHorizontalLine.cpp - Code for a control

begun 4/2/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include "escarpment.h"
#include "resleak.h"


/***********************************************************************
Bezier4 - Given 4 points in a row, this keeps subdividing until there
shouldn't be a size difference, and then writes the points into the list,
all except the last one.

inputs
   POINT       *paIn - 4 points in a row
   CListFixed  *pList - List of POINT structures to add to. This must be Initialized to sizeof(POINT)
   DWORD       dwMaxDepth - Maximum depth left. If 0, just copies points
returns
   none
*/
void Bezier4 (POINT *paIn, CListFixed *pList, DWORD dwMaxDepth)
{
   // are these points OK in themselves
   DWORD i;
#if 0
   RECT  r;
   int   iDist;
   r.top = r.bottom = paIn[0].y;
   r.left = r.right = paIn[0].x;
   for (i = 1; i < 4; i++) {
      r.top = min(r.top, paIn[i].y);
      r.bottom = max(r.bottom, paIn[i].y);
      r.left = min(r.left, paIn[i].x);
      r.right = max(r.right, paIn[i].x);
   }
   iDist = min (r.bottom - r.top, r.right - r.left);
   if (iDist <= 3) {
#endif // 0
   if (!dwMaxDepth) {
      // it's a small enough distance
      pList->Required (3 + pList->Num());
      for (i = 0; i < 3; i++)
         pList->Add (paIn + i);
      return;
   }

   // else subdivide, find the midpoint
   POINT pMid;
   pMid.x = (paIn[1].x + paIn[2].x) / 2;
   pMid.y = (paIn[1].y + paIn[2].y) / 2;

   // create an array of 4 points and call down
   POINT ap[4];
   ap[0] = paIn[0];
   ap[1].x = (paIn[0].x + paIn[1].x) / 2;
   ap[1].y = (paIn[0].y + paIn[1].y) / 2;
   ap[2].x = (paIn[1].x + pMid.x) / 2;
   ap[2].y = (paIn[1].y + pMid.y) / 2;
   ap[3] = pMid;
   Bezier4 (ap, pList, dwMaxDepth-1);

   // other half
   ap[0] = pMid;
   ap[1].x = (paIn[2].x + pMid.x) / 2;
   ap[1].y = (paIn[2].y + pMid.y) / 2;
   ap[2].x = (paIn[3].x + paIn[2].x) / 2;
   ap[2].y = (paIn[3].y + paIn[2].y) / 2;
   ap[3] = paIn[3];
   Bezier4 (ap, pList, dwMaxDepth-1);

   // done
}


/***********************************************************************
Bezier - Given N points (where N is 3, 6, 9, 12, etc. points), this
fills in a CListFixed object converting from a bezier curve points into
linear points.

inputs
   POINT    *paIn - Array of points
   DWORD    dwNum - 3,6,9, etc.
returns
   PCListFixed - List of points. Must be freed
*/
PCListFixed Bezier (POINT *paIn, DWORD dwNum)
{
//   if ((dwNum % 3) != 0)
//      return NULL;

   CListFixed  *pl;
   pl = new CListFixed;
   if (!pl)
      return NULL;
   pl->Init (sizeof(POINT));

   DWORD i;
   POINT ap[4];
   for (i = 0; (i+2) < dwNum; i += 3) {
      ap[0] = paIn[i];
      ap[1] = paIn[i+1];
      ap[2] = paIn[i+2];
      ap[3] = paIn[(i+3) % dwNum];

      Bezier4 (ap, pl, 3);
   }

   return pl;
}


/***********************************************************************
PointsFill - fill list of points

inputs
   HDC      hDC - DC to draw in
   COLORREF cr - color to draw
   PCListFixed pl - list
returns
   BOOL  - TRUE if OK
*/
void PointsFill (HDC hDC, COLORREF cr, PCListFixed pl)
{
   HBRUSH   hbr, hOld;
   HPEN     hPen, hPenOld;
   hbr = CreateSolidBrush(cr);
   hPen = CreatePen (PS_SOLID, 0, cr);
   hPenOld = (HPEN) SelectObject (hDC, hPen);
   hOld = (HBRUSH) SelectObject (hDC, hbr);
   Polygon (hDC, (POINT*) pl->Get(0), pl->Num());
   SelectObject (hDC, hOld);
   SelectObject (hDC, hPenOld);
   DeleteObject (hbr);
   DeleteObject (hPen);
}

/***********************************************************************
BezierFill - Given N points (3, 7, 11, 15, etc.) that are a bezier curve
outline, this fills in a polygon from the bezier curve outline.

inputs
   HDC      hDC - DC to draw in
   COLORREF cr - color to draw
   POINT    *paIn - input
   DWORD    dwNum - number of points
returns
   BOOL  - TRUE if OK
*/
BOOL BezierFill (HDC hDC, COLORREF cr, POINT *paIn, DWORD dwNum)
{
   PCListFixed pl;
   pl = Bezier (paIn, dwNum);
   if (!pl)
      return FALSE;

   PointsFill (hDC, cr, pl);

   delete pl;
   return TRUE;
}


/***********************************************************************
PolyScale - Given a set of points with values between 0 and 100 in the XY direction,
this scales them to fit in the given rectangle, outputting a PCListFixed object.

inputs
   POINT    *paIn - input
   DWORD    dwNum - number of points in input
   RECT     &pr - To fit in
returns
   PCListFixed - List of POINT strucuted. Must be deleted
*/
PCListFixed PolyScale (POINT *paIn, DWORD dwNum, RECT *pr)
{
   PCListFixed pl;
   pl = new CListFixed;
   if (!pl)
      return NULL;
   pl->Init (sizeof(POINT));

   DWORD i;
   POINT p;
   pl->Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      p.x = paIn[i].x * (pr->right - pr->left) / 100 + pr->left;
      p.y = paIn[i].y * (pr->bottom - pr->top) / 100 + pr->top;

      pl->Add (&p);
   }


   return pl;
}


/***********************************************************************
MirrorX - Given a PCListFixed of POINTS, this mirrors them around the
given X coordinate of the last point. It does this by walking backwards in the list from
the second-to-last one to the first. (It doesn't mirror the last point
because that's used for the X.)

inputs
   PCListFixed    pl - list
reutrns
   none
*/
void MirrorX (PCListFixed pl)
{
   DWORD dwNum, i;
   dwNum = pl->Num();
   int   iX;
   POINT *p, p2;
   
   // miror around
   p = (POINT*) pl->Get(dwNum-1);
   iX = p->x;

   // loop
   pl->Required (3 + pl->Num());
   for (i = dwNum - 2; i < dwNum; i--) {
      p = (POINT*) pl->Get(i);
      p2 = *p;
      p2.x = iX + (iX - p->x);
      pl->Add (&p2);
   }
}


/***********************************************************************
MirrorY - Like mirror X except around the Y.
*/
void MirrorY (PCListFixed pl)
{
   DWORD dwNum, i;
   dwNum = pl->Num();
   int   iY;
   POINT *p, p2;
   
   // miror around
   p = (POINT*) pl->Get(dwNum-1);
   iY = p->y;

   // loop
   pl->Required (3 + pl->Num());
   for (i = dwNum - 2; i < dwNum; i--) {
      p = (POINT*) pl->Get(i);
      p2 = *p;
      p2.y = iY + (iY - p->y);
      pl->Add (&p2);
   }
}

/***********************************************************************
PaintXXX - Paint one of the separator styles.

inputs
   HDC      hDC - DC to draw into
   RECT     r - bounding rectangle
   COLORREF cr - color
returns
   none
*/
static void PaintLine (HDC hDC, RECT *pr, COLORREF cr)
{
   HBRUSH   hbr;
   hbr = CreateSolidBrush (cr);
   FillRect (hDC, pr, hbr);
   DeleteObject (hbr);
}

static void PaintDoubleLine (HDC hDC, RECT *pr, COLORREF cr)
{
   RECT  r;

   HBRUSH   hbr;
   hbr = CreateSolidBrush (cr);

   r = *pr;
   r.bottom = r.top + (r.bottom - r.top) / 3;
   FillRect (hDC, &r, hbr);

   r = *pr;
   r.top = r.bottom - (r.bottom - r.top) / 3;
   FillRect (hDC, &r, hbr);

   DeleteObject (hbr);
}

static void Paint3D (HDC hDC, RECT *pr, COLORREF cr)
{
   HBRUSH   hbr;
   RECT  r;

   // top hilight
   r = *pr;
   r.bottom = r.top + (pr->bottom - pr->top) / 3;
   FillRect (hDC, &r, (HBRUSH) (COLOR_BTNHILIGHT+1));

   // central
   hbr = CreateSolidBrush (cr);
   r.top = r.bottom;
   r.bottom = pr->bottom - (pr->bottom - pr->top) / 3;
   FillRect (hDC, &r, hbr);
   DeleteObject (hbr);

   // buttom
   r.top = r.bottom;
   r.bottom = pr->bottom;
   FillRect (hDC, &r, (HBRUSH) (COLOR_BTNSHADOW+1));
}

static void PaintS (HDC hDC, RECT *pr, COLORREF cr)
{
#define  SCURVEWIDTH    8
#define  SWIDTH         48
#define  LINEWIDTH      2
   POINT ap[] = {
      {SCURVEWIDTH, 0},    // start of curve
      {0,0},
      {0,100},
      {SCURVEWIDTH,100}, // start of second curve
      {SCURVEWIDTH*2,75},
      {SWIDTH-SCURVEWIDTH*2,25},
      {SWIDTH-SCURVEWIDTH,0},   // start of third curve
      {SWIDTH,0},
      {SWIDTH,100},
      {SWIDTH-SCURVEWIDTH,100}, // start of heading back
      {SWIDTH-LINEWIDTH,100},
      {SWIDTH-LINEWIDTH,0},
      {SWIDTH-SCURVEWIDTH,LINEWIDTH},   // start of third curve
      {SWIDTH/2,50},
      {SWIDTH/2,50},
      {SCURVEWIDTH,100-LINEWIDTH}, // start of second curve
      {LINEWIDTH,100},
      {LINEWIDTH,0}
   };

   // scale to fit
   PCListFixed pl;
   pl = PolyScale (ap, sizeof(ap) / sizeof(POINT), pr);
   if (!pl)
      return;

   BezierFill (hDC, cr, (POINT*) pl->Get(0), pl->Num());

   // flip on the X axis and draw again
   DWORD i;
   int   iX;
   iX = (pr->right + pr->left) / 2;
   POINT *p;
   for (i = 0; i < pl->Num(); i++) {
      p = (POINT*) pl->Get(i);
      p->x = iX + (iX - p->x);
   }
   BezierFill (hDC, cr, (POINT*) pl->Get(0), pl->Num());
   
   delete pl;
}

static void PaintWave (HDC hDC, RECT *pr, COLORREF cr)
{
   // determine the number of revolutions (about)
   int   iNum;
   iNum = pr->bottom - pr->top;
   if (iNum <= 0)
      iNum = 1;
   iNum = (pr->right - pr->left) / (iNum * 4);
   if (iNum <= 0)
      iNum = 1;

   // actually want 1/2 extra
   POINT p;
   iNum = iNum * 2 + 1;
   CListFixed  l;
   l.Init (sizeof(POINT));
   int   i;
   int   iTop, iBottom, iMid, iFix, iCur;
#define  WAVEOFFSET     ((pr->bottom - pr->top) / 5)
   iTop = pr->top;
   iBottom = pr->bottom - WAVEOFFSET;
   iMid = (pr->top + pr->bottom) / 2;
   iFix = (pr->right - pr->left - WAVEOFFSET) / iNum / 4;
   for (i = 0; i <= iNum; i++) {
      iCur = i * (pr->right - pr->left - WAVEOFFSET) / iNum + pr->left + WAVEOFFSET;

      if (i % 2) { // odd
         p.x = iCur - iFix;
         p.y = iTop;
         l.Add (&p);
         p.x = iCur;
         p.y = iMid;
         l.Add (&p);
         if (i != iNum) {
            p.x = iCur + iFix;
            p.y = iBottom;
            l.Add (&p);
         }
      }
      else {   // !(i%2)
         if (i != 0) {
            p.x = iCur - iFix;
            p.y = iBottom;
            l.Add (&p);
         }
         p.x = iCur;
         p.y = iMid;
         l.Add (&p);
         p.x = iCur + iFix;
         p.y = iTop;
         l.Add (&p);
      }
   }

   // add 2 ppoints for line
   p.x = pr->right - WAVEOFFSET/2;
   p.y = iMid + WAVEOFFSET/2;
   l.Add (&p);
   l.Add (&p);

   // go back
   iTop += WAVEOFFSET;
   iMid += WAVEOFFSET;
   iBottom += WAVEOFFSET;
   for (i = iNum; i >= 0; i--) {
      iCur = i * (pr->right - pr->left - WAVEOFFSET) / iNum + pr->left;

      if (i % 2) { // odd
         if (i != iNum) {
            p.x = iCur + iFix;
            p.y = iBottom;
            l.Add (&p);
         }
         p.x = iCur;
         p.y = iMid;
         l.Add (&p);
         p.x = iCur - iFix;
         p.y = iTop;
         l.Add (&p);
      }
      else {   // !(i%2)
         p.x = iCur + iFix;
         p.y = iTop;
         l.Add (&p);
         p.x = iCur;
         p.y = iMid;
         l.Add (&p);
         if (i != 0) {
            p.x = iCur - iFix;
            p.y = iBottom;
            l.Add (&p);
         }
      }
   }


   // fill
   BezierFill (hDC, cr, (POINT*) l.Get(0), l.Num());
}

/***********************************************************************
PaintBezierQuadrant - Given the UL quadrant of a bezier curve, in 0..100 by
   0..100 dimensions, this mirrors it and paints the whole thing.

inputs
   HDC      hDC - DC to draw into
   RECT     r - bounding rectangle
   COLORREF cr - color
   POINT    *paIn - Input
   DWORD    dwNum - Number of points
returns
   none
*/
static void PaintBezierQuadrant (HDC hDC, RECT *pr, COLORREF cr, POINT *paIn, DWORD dwNum)
{
   // scale to fit
   PCListFixed pl;
   pl = PolyScale (paIn, dwNum, pr);
   if (!pl)
      return;

   PCListFixed pl2;
   pl2 = Bezier ((POINT*) pl->Get(0), pl->Num());
   if (!pl2)
      return;
   pl2->Add (pl->Get(pl->Num()-1)); // add the last point on
   delete pl;
   pl = pl2;

   // mirror on X
   MirrorX (pl);

   // mirror on Y
   MirrorY (pl);

   // delete the last point because it's redundant (a copy of the first)
   pl->Remove(pl->Num()-1);

   // fill this as a bezier
   PointsFill (hDC, cr, pl);

   delete pl;
}

typedef struct {
   PWSTR          pszStyle;
   COLORREF       cr;
} HORIZONTALLINE, *PHORIZONTALLINE;

/***********************************************************************
Control callback
*/
BOOL ControlHorizontalLine (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   HORIZONTALLINE *pc = (HORIZONTALLINE*) pControl->m_mem.p;

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(HORIZONTALLINE));
         pc = (HORIZONTALLINE*) pControl->m_mem.p;
         memset (pc, 0, sizeof(HORIZONTALLINE));
         pControl->AttribListAddString (L"style", &pc->pszStyle, FALSE, TRUE);
         pControl->AttribListAddColor (L"color", &pc->cr, FALSE, TRUE);
      }
      return TRUE;


   case ESCM_QUERYSIZE:
      {
         ESCMQUERYSIZE  *p = (ESCMQUERYSIZE*) pParam;
         int iFW, iFH;
         int   iFinalWidth = 0, iFinalHeight = 0;
         BOOL  fPercent;

         // find the type since that may affect the height - actually most are fairly
         // high, except for the straight line
         BOOL  fLine;
         WCHAR *pszStyle;
         pszStyle = pc->pszStyle;
         fLine = !pszStyle || !_wcsicmp(pszStyle, L"line");
         
         if (AttribToDecimalOrPercent (pControl->m_pNode->AttribGet(L"width"), &fPercent, &iFW)) {
            if (fPercent)
               iFinalWidth = iFW * p->iDisplayWidth / 100;
            else
               iFinalWidth = iFW;
         }
         else  // default to 80%
            iFinalWidth = p->iDisplayWidth;

         if (AttribToDecimalOrPercent (pControl->m_pNode->AttribGet(L"height"), &fPercent, &iFH)) {
            if (fPercent)
               iFinalHeight = iFH * p->iDisplayWidth / 100;
            else
               iFinalHeight = iFH;
         }
         else
            iFinalHeight = fLine ? 2 : 16;

         // store away
         p->iWidth = iFinalWidth;
         p->iHeight = iFinalHeight;
      }
      return TRUE;


   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;

         // get the color
         COLORREF cr;
         cr = pc->cr;

         WCHAR *pszStyle;
         pszStyle = pc->pszStyle;
         if (!pszStyle || !_wcsicmp(pszStyle, L"line"))
            PaintLine (p->hDC, &p->rControlHDC, cr);
         else if (!_wcsicmp(pszStyle, L"doubleline"))
            PaintDoubleLine (p->hDC, &p->rControlHDC, cr);
         else if (!_wcsicmp(pszStyle, L"3d")) {
            if (pc->cr == 0)
               cr = GetSysColor (COLOR_BTNFACE);
            Paint3D (p->hDC, &p->rControlHDC, cr);
         }
         else if (!_wcsicmp(pszStyle, L"star1")) {
#if 0
            POINT ap[] = {
               {0, 50},
               {30,45},
               {50,45},
               {50,0}
            };
#endif
            POINT ap[] = {
               {0, 50},
               {5,45},
               {10,45},
               {45,45}, // start of second curve
               {48,45},
               {50,20},
               {50,0}
            };
            PaintBezierQuadrant (p->hDC, &p->rControlHDC, cr, ap, sizeof(ap)/sizeof(POINT));
         }
         else if (!_wcsicmp(pszStyle, L"star2")) {
            POINT ap[] = {
               {0, 50},
               {5,45},
               {10,45},
               {35,45}, // start of second curve
               {38,45},
               {40,20},
               {40,0},   // start of third curve
               {40,20},
               {42,50},
               {50,50}
            };
            PaintBezierQuadrant (p->hDC, &p->rControlHDC, cr, ap, sizeof(ap)/sizeof(POINT));
         }
         else if (!_wcsicmp(pszStyle, L"star3")) {
            POINT ap[] = {
               {0, 50},
               {2,50},
               {5,20},
               {5,0}, // start of second curve
               {5,20},
               {7,50},
               {10,45},   // start of third curve
               {20,46},
               {40,47},
               {50,59}
            };
            PaintBezierQuadrant (p->hDC, &p->rControlHDC, cr, ap, sizeof(ap)/sizeof(POINT));
         }
         else if (!_wcsicmp(pszStyle, L"star4")) {
            POINT ap[] = {
               {0, 50},
               {0,40},
               {0,30},
               {0,0}, // start of second curve
               {0,35},
               {2,45},
               {5,45},   // start of third curve
               {20,46},
               {40,47},
               {50,59}
            };
            PaintBezierQuadrant (p->hDC, &p->rControlHDC, cr, ap, sizeof(ap)/sizeof(POINT));
         }
         else if (!_wcsicmp(pszStyle, L"S"))
            PaintS (p->hDC, &p->rControlHDC, cr);
         else if (!_wcsicmp(pszStyle, L"Wave"))
            PaintWave (p->hDC, &p->rControlHDC, cr);
         else // default
            PaintLine (p->hDC, &p->rControlHDC, cr);

      }
      return TRUE;


   }

   return FALSE;
}

