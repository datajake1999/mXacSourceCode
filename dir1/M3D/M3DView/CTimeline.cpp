/*****************************************************************************
CTimeline.cpp - Code for drawing the animation timeline
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#define TIMELINECLASS      APPSHORTNAME "TIMELINE"
#define TLEXTRA            5

/*******************************************************************************************
CTimeline::Constructor and destructor */
CTimeline::CTimeline (void)
{
   m_hWnd = NULL;
   m_cBack = m_cLight = m_cShadow = m_cMed = m_cBackShadow = 0;
   m_dwMsg = 0;
   m_dwCapture = 0;
   m_fLimitLeft = 0;
   m_fLimitScene = m_fLimitRight = 1;
   m_fPointer = 0;
   m_fScaleLeft = 0;
   m_fScaleRight = 1;
}

CTimeline::~CTimeline (void)
{
   if (m_dwCapture)
      ReleaseCapture ();

   if (m_hWnd)
      DestroyWindow (m_hWnd);
}


/*******************************************************************************************
CTimeline::Init - Call this after creating the object. THis will cause the window to be
created.

inputs
   HWND        hWndParent - Parent window
   COLORREF    cBack, cLight, cShadow - Colors to use
   DWORD       dwMsg - This message is sent back to the main window as a notification.
                  wParam == 0 if the slider is changed, 1 for the left limit, 2 for the right limit
returns
   BOOL - TRUE if succeded

NOTE: Not visible until move called.
*/
BOOL CTimeline::Init (HWND hWndParent, COLORREF cBack, COLORREF cBackShadow, COLORREF cLight, COLORREF cShadow,
                      COLORREF cOutline, DWORD dwMsg)
{
   if (m_hWnd)
      return FALSE;

   m_cBack = cBack;
   m_cLight = cLight;
   m_cShadow = cShadow;
   m_cBackShadow = cBackShadow;
   m_cOutline = cOutline;
   m_dwMsg = dwMsg;
   m_cMed = RGB(
      GetRValue(m_cLight)/2 + GetRValue(m_cShadow)/2,
      GetGValue(m_cLight)/2 + GetGValue(m_cShadow)/2,
      GetBValue(m_cLight)/2 + GetBValue(m_cShadow)/2);

   static BOOL gfRegistered = FALSE;
   if (!gfRegistered) {
      WNDCLASS wc;

      memset (&wc, 0, sizeof(wc));
      wc.lpfnWndProc = CTimelineWndProc;
      wc.style = 0;
      wc.hInstance = ghInstance;
      wc.hIcon = NULL;
      wc.hCursor = 0;
      wc.hbrBackground = NULL;
      wc.lpszMenuName = NULL;
      wc.lpszClassName = TIMELINECLASS;
      RegisterClass (&wc);

      gfRegistered = TRUE;
   }

   m_hWnd = CreateWindow (TIMELINECLASS, "", WS_CHILD,
      0,0,10,10,
      hWndParent, NULL, ghInstance, (LPVOID)this);
   if (!m_hWnd)
      return FALSE;

   return TRUE;
}

/*******************************************************************************************
CTimeline::Move - Changes the location of the window and causes it to redraw

inputs
   RECT     *pr - New location
returns
   none
*/
void CTimeline::Move (RECT *pr)
{
   MoveWindow (m_hWnd, pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top, TRUE);
   if (!IsWindowVisible (m_hWnd))
      ShowWindow (m_hWnd, SW_SHOW);
}


/*******************************************************************************************
CTimeline::LimitsSet - Sets the left and right playback units.

inputs
   fp       fLeft, fRight - Left an right playback extend units... theoretically when
               playing an animation will only play frm fLeft to fRight. Units are
               really determined by scaleSet
   fp       fScene - Limits on the scene
returns
   BOOL - TRUE if success
*/
BOOL CTimeline::LimitsSet (fp fLeft, fp fRight, fp fScene)
{
   fRight = max(fRight, fLeft);
   m_fLimitLeft = fLeft;
   m_fLimitRight = fRight;
   m_fLimitScene = fScene;
   InvalidateRect (m_hWnd, NULL, FALSE);

   return TRUE;
}

/*******************************************************************************************
CTimeline::LimitsGet - Fills in the current values for the limits.

inputs
   fp       *pfLeft, *pfRight - Fill in the left and right limits
   fp       *pfScene - Limits on the scene
returns
   none
*/
void CTimeline::LimitsGet (fp *pfLeft, fp *pfRight, fp *pfScene)
{
   if (pfLeft)
      *pfLeft = m_fLimitLeft;
   if (pfRight)
      *pfRight = m_fLimitRight;
   if (pfScene)
      *pfScene = m_fLimitScene;
}

/*******************************************************************************************
CTimeline::PointerSet - Sets the current playback pointer location.

inputs
   fp       fVal - New value. Meaning determined by ScaleSet()
returns
   BOOL - TRUE if success
*/
BOOL CTimeline::PointerSet (fp fVal)
{
   m_fPointer = fVal;
   InvalidateRect (m_hWnd, NULL, FALSE);

   return TRUE;
}

/*******************************************************************************************
CTimeline::PointerGet - Returns the current pointer location
*/
fp CTimeline::PointerGet (void)
{
   return m_fPointer;
}

/*******************************************************************************************
CTimeline::ScaleSet - Sets the meaning of the left and right edges.

inputs
   fp          fLeft, fRight - Values of the left and right edges (as defined by
                  the Move() call with pr->left and pr->right).
                  fRight > fLeft
returns
   BOOL - TRUE if success
*/
BOOL CTimeline::ScaleSet (fp fLeft, fp fRight)
{
   fRight = max(fLeft + CLOSE, fRight);
   m_fScaleLeft = fLeft;
   m_fScaleRight = fRight;
   InvalidateRect (m_hWnd, NULL, FALSE);
   return TRUE;
}

/*******************************************************************************************
CTimeline::WndProc - Handle the window functions
*/
LRESULT CTimeline::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
      case WM_PAINT:
         {
            PAINTSTRUCT ps;
            HDC hDC = BeginPaint (hWnd, &ps);
            RECT r;
            GetClientRect (hWnd, &r);

            HDC hDCNew = CreateCompatibleDC (hDC);
            HBITMAP hBmp = CreateCompatibleBitmap (hDC, r.right, r.bottom);
            SelectObject (hDCNew, hBmp);

            // fill
            HBRUSH hBrush, hbrOld;
            hBrush = CreateSolidBrush (m_cBack);
            FillRect (hDCNew, &r, hBrush);
            DeleteObject (hBrush);

            // pens
            HPEN  hPenLight, hPenShadow, hPenOutline, hPenOld;
            hPenLight = CreatePen (PS_SOLID, 0, m_cLight);
            hPenShadow = CreatePen (PS_SOLID, 0, m_cShadow);
            hPenOutline = CreatePen (PS_SOLID, 0, m_cOutline);
            hPenOld = (HPEN) SelectObject (hDCNew, hPenLight);

            // figure out where left and right limits are
            fp fLeft, fRight, fScene;
            int iLeft, iRight;
            iLeft = r.left + TLEXTRA;
            iRight = max (r.right - TLEXTRA, iLeft+1);
            fLeft = (m_fLimitLeft - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            fRight = (m_fLimitRight - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            fScene = (m_fLimitScene - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;

            // draw lines for the well
            if ((fLeft <= iRight) || (fRight >= iLeft)) {
               int   iWellLeft, iWellRight;
               iWellLeft = (int) max((fp)iLeft, fLeft);
               iWellRight = (int) min((fp)iRight, fRight);

               SelectObject (hDCNew, hPenOutline);
               MoveToEx (hDCNew, iWellLeft, r.top + 1, NULL);
               LineTo (hDCNew, iWellRight, r.top + 1);
               MoveToEx (hDCNew, iWellLeft, r.top + 5, NULL);
               LineTo (hDCNew, iWellRight, r.top + 5);
               MoveToEx (hDCNew, iWellLeft-1, r.top+2, NULL);
               LineTo (hDCNew, iWellLeft-1, r.top+5);
               MoveToEx (hDCNew, iWellRight, r.top+2, NULL);
               LineTo (hDCNew, iWellRight, r.top+5);

               // fill it in
               RECT rFill;
               rFill.left = iWellLeft;
               rFill.right = iWellRight;
               rFill.top = r.top+2;
               rFill.bottom = r.top + 5;
               hBrush = CreateSolidBrush (m_cBackShadow);
               FillRect (hDCNew, &rFill, hBrush);
               DeleteObject (hBrush);
            }

            // the limits
            hBrush = CreateSolidBrush (m_cLight);
            hbrOld = (HBRUSH) SelectObject (hDCNew, hBrush);
            SelectObject (hDCNew, hPenOutline);
            // BUGFIX - If limitscene < 0 then dont draw scene
            if ((m_fLimitScene >= 0) && ((fScene-CLOSE <= iRight) || (fScene+CLOSE >= iLeft)) ) {
               int iPointer = (int) fScene;
               POINT ap[3];
               ap[0].x = iPointer;
               ap[0].y = r.top;
               ap[1].x = iPointer + 3;
               ap[1].y = r.top;
               ap[2].x = iPointer;
               ap[2].y = r.bottom;

               Polygon (hDCNew, ap, 3);
            }
            if ((fLeft-CLOSE <= iRight) || (fLeft+CLOSE >= iLeft)) {
               int iPointer = (int) fLeft;
               POINT ap[4];
               ap[0].x = iPointer-3;
               ap[0].y = r.top;
               ap[1].x = iPointer + 3;
               ap[1].y = r.top;
               ap[2].x = iPointer + 3;
               ap[2].y = r.top + 6;
               ap[3].x = iPointer - 3;
               ap[3].y = r.top + 6;

               Polygon (hDCNew, ap, 4);
            }
            if ((fRight-CLOSE <= iRight) || (fRight+CLOSE >= iLeft)) {
               int iPointer = (int) fRight;
               POINT ap[4];
               ap[0].x = iPointer-3;
               ap[0].y = r.top;
               ap[1].x = iPointer + 3;
               ap[1].y = r.top;
               ap[2].x = iPointer + 3;
               ap[2].y = r.top + 6;
               ap[3].x = iPointer - 3;
               ap[3].y = r.top + 6;

               Polygon (hDCNew, ap, 4);
            }
            SelectObject (hDCNew, hbrOld);
            DeleteObject (hBrush);



            // pointer
            fp fPointer;
            fPointer = (m_fPointer - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            if ((fPointer-CLOSE <= iRight) && (fPointer+CLOSE >= iLeft)) {
               int iPointer = (int) fPointer;

               POINT ap[5];
               ap[0].x = iPointer - 2;
               ap[0].y = r.top;
               ap[1].x = iPointer + 2;
               ap[1].y = r.top;
               ap[2].x = ap[1].x;
               ap[2].y = r.bottom - 3;
               ap[3].x = iPointer;
               ap[3].y = r.bottom-1;
               ap[4].x = ap[0].x;
               ap[4].y = ap[2].y;

               hBrush = CreateSolidBrush (m_cMed);
               hbrOld = (HBRUSH) SelectObject (hDCNew, hBrush);
               SelectObject (hDCNew, hPenOutline);
               Polygon (hDCNew, ap, 5);
               SelectObject (hDCNew, hbrOld);
               DeleteObject (hBrush);

               // shadow and light
               SelectObject (hDCNew, hPenLight);
               MoveToEx (hDCNew, iPointer-1, r.top+1, NULL);
               LineTo (hDCNew, iPointer-1, r.bottom-2);
               SelectObject (hDCNew, hPenShadow);
               MoveToEx (hDCNew, iPointer+1, r.top+1, NULL);
               LineTo (hDCNew, iPointer+1, r.bottom-2);
            }

            // delete pens
            SelectObject (hDCNew, hPenOld);
            DeleteObject (hPenLight);
            DeleteObject (hPenShadow);
            DeleteObject (hPenOutline);

            // transfer bitmap over
            BitBlt (hDC, 0, 0, r.right, r.bottom, hDCNew, 0, 0, SRCCOPY);
            DeleteDC (hDCNew);
            DeleteObject (hBmp);
            EndPaint (hWnd, &ps);
         }
         return 0;

      case WM_LBUTTONDOWN:
         {
            if (m_dwCapture)
               return 0;   // cant deal with this

            // where clicked?
            // info
            int iX, iY;
            iX = (short) LOWORD(lParam);
            iY = (short) HIWORD(lParam);
            RECT r;
            int iLeft, iRight;
            GetClientRect (hWnd, &r);
            iLeft = r.left + TLEXTRA;
            iRight = max (r.right - TLEXTRA, iLeft+1);

            fp fPointer, fLeft, fRight, fScene;
            fPointer = (m_fPointer - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            fLeft = (m_fLimitLeft - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            fRight = (m_fLimitRight - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            fScene = (m_fLimitScene - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            if (fabs(fPointer - iX) < 4)
               m_dwCapture = 1;
            else if (fabs(fLeft - iX) < 4)
               m_dwCapture = 2;
            else if (fabs(fRight - iX) < 4)
               m_dwCapture = 3;
            else if ((m_fLimitScene >= 0) && (fabs(fScene - iX) < 4))
               m_dwCapture = 4;
            else
               m_dwCapture = 1;  // just jump to that point

            SetCapture (hWnd);

            // update
            WndProc (hWnd, WM_MOUSEMOVE, wParam, lParam);
         }
         return 0;

      case WM_LBUTTONUP:
         if (m_dwCapture)
            ReleaseCapture ();
         m_dwCapture = 0;
         return 0;

      case WM_MOUSEMOVE:
         {
            // info
            int iX, iY;
            iX = (short) LOWORD(lParam);
            iY = (short) HIWORD(lParam);
            RECT r;
            int iLeft, iRight;
            GetClientRect (hWnd, &r);
            iLeft = r.left + TLEXTRA;
            iRight = max (r.right - TLEXTRA, iLeft+1);
            
            // set the cursor and move
            // move the pointer
            fp f;
            f = (fp) (iX - iLeft) / (fp) (iRight - iLeft) * (m_fScaleRight - m_fScaleLeft) +
               m_fScaleLeft;
            f = max(0, f);
            if (m_dwCapture == 1) {
               SetCursor(LoadCursor(NULL, IDC_ARROW));

               if (fabs(f - m_fPointer) > CLOSE) {
                  m_fPointer = f;
                  InvalidateRect (hWnd, NULL, FALSE);
                  SendMessage (GetParent(m_hWnd), m_dwMsg, 0, 0);
               }
               return 0;
            }
            else if (m_dwCapture == 2) {
               SetCursor(LoadCursor(NULL, IDC_SIZEWE));

               if ((f <= m_fLimitRight) && (fabs(f - m_fPointer) > CLOSE)) {
                  m_fLimitLeft = f;
                  InvalidateRect (hWnd, NULL, FALSE);
                  SendMessage (GetParent(m_hWnd), m_dwMsg, 1, 0);
               }
               return 0;
            }
            else if (m_dwCapture == 3) {
               SetCursor(LoadCursor(NULL, IDC_SIZEWE));

               if ((f >= m_fLimitLeft) && (fabs(f - m_fPointer) > CLOSE)) {
                  m_fLimitRight = f;
                  InvalidateRect (hWnd, NULL, FALSE);
                  SendMessage (GetParent(m_hWnd), m_dwMsg, 1, 0);
               }
               return 0;
            }
            else if (m_dwCapture == 4) {
               SetCursor(LoadCursor(NULL, IDC_SIZEWE));

               m_fLimitScene = f;
               InvalidateRect (hWnd, NULL, FALSE);
               SendMessage (GetParent(m_hWnd), m_dwMsg, 2, 0);
               return 0;
            }

            // else, not selected, so just find nearest
            fp fPointer, fLeft, fRight, fScene;
            fPointer = (m_fPointer - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            fLeft = (m_fLimitLeft - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            fRight = (m_fLimitRight - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            fScene = (m_fLimitScene - m_fScaleLeft) / (m_fScaleRight - m_fScaleLeft) *
               (fp) (iRight - iLeft) + iLeft;
            if (fabs(fPointer - iX) < 4)
               SetCursor(LoadCursor(NULL, IDC_ARROW));
            else if (fabs(fLeft - iX) < 4)
               SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            else if (fabs(fRight - iX) < 4)
               SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            else if ((m_fLimitScene >= 0) && (fabs(fScene - iX) < 4))
               // BUGFIX - If limitscene < 0 then dont allow to move
               SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            else
               SetCursor(LoadCursor(NULL, IDC_ARROW));  // not over anything so assume move pointer
         }
         return 0;

      case WM_DESTROY:
         if (m_dwCapture)
            ReleaseCapture ();
         m_dwCapture = NULL;
         return 0;
   };

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/*******************************************************************************************
CTimelineWndProc - Window proc. Just calls into object
*/
LRESULT CALLBACK CTimelineWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCTimeline p = (PCTimeline) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCTimeline p = (PCTimeline) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCTimeline) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*****************************************************************************
TimelineTicks - Draws the timeline ticks someplace.

inputs
   HDC            hDC - To draw onto
   RECT           *pr - Rectangle on the HDC
   PCMatrix       pmPixelToTime - Matrix that converts from pixel to time when pixel in .p[0] or .p[1]
   PCMatrix       pmTimeToPixel - Matrix that converts from time to pixel when time in .p[0] or .p[1]
   COLORREF       crMinor - Color of the minor ticks
   COLORREF       crMajor - Color of the major ticks
   DWORD          dwFramesPerSec - If 0, show as time. Else, show as frames. This is the number
                  of frames per second
returns
   none
*/
#define BETWEENMINORTICKS     15    // number of pixels between each minor tick
#define TICKFONT              10

void TimelineTicks (HDC hDC, RECT *pr, PCMatrix pmPixelToTime, PCMatrix pmTimeToPixel,
                    COLORREF crMinor, COLORREF crMajor, DWORD dwFramesPerSec)
{
   // find start and stop time, and how much space betweeen times
   CPoint   pTime;
   fp fTime, fTickTime;
   pTime.Zero();
   pTime.p[0] = pr->left;
   pTime.p[1] = pr->right;
   pTime.p[3] = 1;
   pTime.MultiplyLeft (pmPixelToTime);
   fTime = pTime.p[1] - pTime.p[0];
   if ((fTime <= CLOSE) || (pr->right <= pr->left))
      return;  // no time spanned;
   fTickTime = (fp) (pr->right - pr->left) / (fp) BETWEENMINORTICKS; // number of ticks can fit it
   fTickTime = fTime / fTickTime; // amount of time allowed per minor tick

   // what units?
   double fMajor, fMinor;  // using double precision just to make sure lines up properly
   DWORD dwPrecision;
   if (fTickTime >= 60 * 10) {
      fMinor = 60 * 60; // 1 hr for minor
      fMajor = fMinor * 10;   // 10 hrs for major
      dwPrecision = 0;  // hours
   }
   else if (fTickTime >= 60) {
      fMinor = 60 * 10; // 10 min for minor
      fMajor = fMinor * 6; // 1 hr for major
      dwPrecision = 1;  // minutes
   }
   else if (fTickTime >= 10) {
      fMinor = 60;   // 1 min for minor
      fMajor = fMinor * 10;   // 10 min fo rmajpor
      dwPrecision = 1;  // minutes
   }
   else if (fTickTime >= 1) {
      fMinor = 10;   // 10 sec for minor
      fMajor = fMinor * 6; // 60 sec for major
      dwPrecision = 2;  // seconds
   }
   else if (fTickTime >= 0.1) {
      fMinor = 1.0;  // 1 sec for minor
      fMajor = fMinor * 10;   // 10 sec for major
      dwPrecision = 2;  // seconds
   }
   else if (dwFramesPerSec) {
      DWORD dwNextLevel;
      // so that if have 25 fps will show every 5th one
      for (dwNextLevel = 2; dwNextLevel < dwFramesPerSec; dwNextLevel++)
         if (!(dwFramesPerSec % dwNextLevel))
            break;

      if (fTickTime >= 1.0 / (fp)dwFramesPerSec) {
         fMinor = 1.0 / (fp)dwFramesPerSec * (fp)dwNextLevel;
         fMajor = 1.0;  // 1 sec
      }
      else {
         fMinor = 1.0 / (fp)dwFramesPerSec;
         fMajor = 1.0;  // 1 sec
      }
      dwPrecision = 2;  // seconds
   }
   else if (fTickTime >= .01) {   // show time
      fMinor = 0.1;  // .1 sec
      fMajor = 1.0;
      dwPrecision = 3;  // .1 seconds
   }
   else if (fTickTime >= .001) {
      fMinor = .01;
      fMajor = 0.1;
      dwPrecision = 4;  // .01 seconds
   }
   else {
      fMinor = .001;
      fMajor = .01;;
      dwPrecision = 5;  // .01 seconds
   }

   // find out where we start, in minor units
   double fStart, fEnd;
   fStart = ceil(pTime.p[0] / fMinor) * fMinor;
   fEnd = pTime.p[1];

   // create the pends
   HPEN hPenMajor, hPenMinor, hPenOld;
   hPenMajor = CreatePen (PS_SOLID, 3, crMajor);
   hPenMinor = CreatePen (PS_SOLID, 0, crMinor);
   hPenOld = (HPEN) SelectObject (hDC, hPenMajor);

   // create the two fonts
   HFONT hFontNorm, hFont90, hFontOld;
   LOGFONT lf;
   DWORD i;
   for (i = 0; i < 2; i++) {
      memset (&lf, 0, sizeof(lf));
      lf.lfHeight = -TICKFONT;   // 10 pixels high MulDiv(iPointSize, EscGetDeviceCaps(hDC, LOGPIXELSY), 72); 
      lf.lfCharSet = DEFAULT_CHARSET;
      lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
      lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
      strcpy (lf.lfFaceName, "Arial");
      if (i) {
         lf.lfEscapement = 2700; // 900;
         //lf.lfOrientation = 900;
         hFont90 = CreateFontIndirect (&lf);
      }
      else
      hFontNorm = CreateFontIndirect (&lf);
   }
   hFontOld = (HFONT) SelectObject (hDC, hFontNorm);
   int iOldMode;
   iOldMode = SetBkMode (hDC, TRANSPARENT);

   // draw
   double f;
   for (; fStart < fEnd; fStart += fMinor) {
      // is it a major tick?
      BOOL fIsMajor = (fmod (fStart + fMinor/10, fMajor) < (fMinor/5));

      // convert back
      int iX;
      pTime.Zero();
      pTime.p[0] = fStart;
      pTime.p[3] = 1;
      pTime.MultiplyLeft (pmTimeToPixel);
      iX = (int) pTime.p[0];

      // draw line
      SelectObject (hDC, fIsMajor ? hPenMajor : hPenMinor);
      MoveToEx (hDC, iX, pr->top, NULL);
      LineTo (hDC, iX, pr->bottom);

      // what's the text
      char szTemp[32];
      szTemp[0] = 0;

      // calc text
      if ((fStart >= 60 * 60 - EPSILON) && (fIsMajor || (fMinor >= 60 * 60) )) {
         if (szTemp[0])
            strcat (szTemp, ", ");
         sprintf (szTemp + strlen(szTemp), "%d h", (int) (fStart / 60 / 60));
      }
      if ((fStart >= 60 - EPSILON) && (dwPrecision > 0) && (fIsMajor || (fMinor >= 60))) {
         f = floor(fmod (fStart / 60 + .5, 60));
         if (szTemp[0])
            strcat (szTemp, ", ");
         sprintf (szTemp + strlen(szTemp), "%d m", (int) f);
      }
      // seconds or frames
      if (dwFramesPerSec) {

         if (szTemp[0])
            strcat (szTemp, ", ");
         if (fIsMajor || (fMinor >= 1.0)) {
            f = fmod(fStart, 60);
            sprintf (szTemp + strlen(szTemp), "%d s", (int) floor(f + .5));
         }

         f = floor(fmod (fStart * (fp)dwFramesPerSec + .5, dwFramesPerSec));
         if ((int)f != 0) {
            if (szTemp[0])
               strcat (szTemp, ", ");
            sprintf (szTemp + strlen(szTemp), "f %d", (int) f);
         }
      }
      else if (dwPrecision > 1) {
         f = fmod(fStart, 60);
         if (szTemp[0])
            strcat (szTemp, ", ");
         if (dwPrecision <= 2)
            sprintf (szTemp + strlen(szTemp), "%d s", (int) floor(f + .5));
         else if (dwPrecision == 3)
            sprintf (szTemp + strlen(szTemp), "%.1f s", f);
         else if (dwPrecision == 4)
            sprintf (szTemp + strlen(szTemp), "%.2f s", f);
         else if (dwPrecision == 5)
            sprintf (szTemp + strlen(szTemp), "%.3f s", f);
         else if (dwPrecision == 6)
            sprintf (szTemp + strlen(szTemp), "%.4f s", f);

      }

      // calculate the text size
      int iLen;
      SIZE size;
      iLen = (DWORD)strlen(szTemp);
      SelectObject (hDC, hFontNorm);
      GetTextExtentPoint32 (hDC, szTemp, iLen, &size);

      // draw text
      COLORREF crOld;
      crOld = SetTextColor (hDC, fIsMajor ? crMajor : crMinor);
      SelectObject (hDC, hFont90);
      ExtTextOut (hDC,  iX + size.cy, pr->bottom - size.cx - TICKFONT/2, 0, NULL, szTemp, iLen, NULL);
      SetTextColor (hDC, crOld);
   }

   // free font
   SelectObject (hDC, hFontOld);
   DeleteObject (hFontNorm);
   DeleteObject (hFont90);
   SetBkMode (hDC, iOldMode);

   // free pens
   SelectObject (hDC, hPenOld);
   DeleteObject (hPenMinor);
   DeleteObject (hPenMajor);
}

/*****************************************************************************
TimelineHorzTicks - Draws the horizontal lines in the timeline - basically the value

inputs
   HDC            hDC - To draw onto
   RECT           *pr - Rectangle on the HDC
   PCMatrix       pmPixelToTime - Matrix that converts from pixel to time when pixel in .p[0] or .p[1]
   PCMatrix       pmTimeToPixel - Matrix that converts from time to pixel when time in .p[0] or .p[1]
   COLORREF       crMinor - Color of the minor ticks
   COLORREF       crMajor - Color of the major ticks
   DWORD          dwType - AIT_XXX
returns
   none
*/

void TimelineHorzTicks (HDC hDC, RECT *pr, PCMatrix pmPixelToTime, PCMatrix pmTimeToPixel,
                    COLORREF crMinor, COLORREF crMajor, DWORD dwType)
{
   // find start and stop time, and how much space betweeen times
   CPoint   pRange, pRange2;
   fp fRange, fTickRange;
   pRange.Zero();
   pRange.p[2] = pr->top;
   pRange.p[3] = 1;
   pRange2.Copy (&pRange);
   pRange2.p[2] = pr->bottom;
   pRange.MultiplyLeft (pmPixelToTime);
   pRange2.MultiplyLeft (pmPixelToTime);
   if (dwType == AIT_ANGLE) {
      // show in degrees
      pRange.p[2] *= 360.0 / (2 * PI);
      pRange2.p[2] *= 360.0 / (2 * PI);
   }
   fRange = pRange.p[2] - pRange2.p[2];
   if ((fRange <= CLOSE) || (pr->bottom <= pr->top))
      return;  // no time spanned;
   fTickRange = (fp) (pr->bottom - pr->top) / (fp) BETWEENMINORTICKS; // number of ticks can fit it
   fTickRange = fRange / fTickRange; // amount of time allowed per minor tick

   // what units?
   double fMajor, fMinor;  // using double precision just to make sure lines up properly
   int iPrecision;
   if ((dwType == AIT_DISTANCE) && (MeasureDefaultUnits() & MUNIT_ENGLISH)) {
      if (fTickRange >= METERSPERFOOT * 1000) {
         fMinor = METERSPERFOOT * 10000;
         fMajor = fMinor * 10;
      }
      else if (fTickRange >= METERSPERFOOT * 100) {
         fMinor = METERSPERFOOT * 1000;
         fMajor = fMinor * 10;
      }
      else if (fTickRange >= METERSPERFOOT * 10) {
         fMinor = METERSPERFOOT * 100;
         fMajor = fMinor * 10;
      }
      else if (fTickRange >= METERSPERFOOT) {
         fMinor = METERSPERFOOT * 10;
         fMajor = fMinor * 10;
      }
      else if (fTickRange >= METERSPERINCH) {
         fMinor = METERSPERFOOT;
         fMajor = fMinor * 10;
      }
      else {
         fMinor = METERSPERINCH;
         fMajor = METERSPERFOOT;
      }
   }
   else {
      // straight #'s
      double fTry;
      int ip;
      
      // defaults
      fMinor = .001;
      fMajor = .01;
      iPrecision = 3;
      for (fTry = 1000, ip = -3; fTry >= .0001; fTry /= 10, ip++) {
         if (fTickRange >= fTry) {
            fMinor = fTry * 10;
            fMajor = fTry * 100;
            iPrecision = ip;
            break;
         }
      }

      // if angles an major tick = 100 then set to 90 degrees
      if (dwType == AIT_ANGLE) {
         if (fMinor == 10)
            fMajor = 90;
         else if (fMinor == 100) {
            fMinor = 45;
            fMajor = 360;
         }
      }
   }


   // find out where we start, in minor units
   double fStart, fEnd;
   fStart = ceil(pRange2.p[2] / fMinor) * fMinor;
   fEnd = pRange.p[2];

   // create the pends
   HPEN hPenMajor, hPenMinor, hPenOld;
   hPenMajor = CreatePen (PS_SOLID, 3, crMajor);
   hPenMinor = CreatePen (PS_SOLID, 0, crMinor);
   hPenOld = (HPEN) SelectObject (hDC, hPenMajor);

   // create the two fonts
   HFONT hFontNorm, hFontOld;
   LOGFONT lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -TICKFONT;   // 10 pixels high MulDiv(iPointSize, EscGetDeviceCaps(hDC, LOGPIXELSY), 72); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   hFontNorm = CreateFontIndirect (&lf);
   hFontOld = (HFONT) SelectObject (hDC, hFontNorm);
   int iOldMode;
   iOldMode = SetBkMode (hDC, TRANSPARENT);

   // draw
   for (; fStart < fEnd; fStart += fMinor) {
      // is it a major tick?
      BOOL fIsMajor = (myfmod (fStart + fMinor/50, fMajor) < (fMinor/25));

      // convert back
      int iY;
      pRange.Zero();
      pRange.p[2] = fStart;
      if (dwType == AIT_ANGLE)
         pRange.p[2] *= 2 * PI / 360.0;
      pRange.p[3] = 1;
      pRange.MultiplyLeft (pmTimeToPixel);
      iY = (int) pRange.p[2];

      // draw line
      SelectObject (hDC, fIsMajor ? hPenMajor : hPenMinor);
      MoveToEx (hDC, pr->left, iY, NULL);
      LineTo (hDC, pr->right+1, iY);

      // what's the text
      char szTemp[32];
      szTemp[0] = 0;

      if (dwType == AIT_DISTANCE) {
         MeasureToString (fStart, szTemp, TRUE);
      }
      else {
         char szString[6];
         strcpy (szString, "%.0f");
         szString[2] = '0' + max(0,iPrecision-1);
         sprintf (szTemp + strlen(szTemp), szString, fStart);

         if (dwType == AIT_HZ)
            strcat (szTemp, " Hz");
      }

      // calculate the text size
      int iLen;
      SIZE size;
      iLen = (DWORD)strlen(szTemp);
      SelectObject (hDC, hFontNorm);
      GetTextExtentPoint32 (hDC, szTemp, iLen, &size);

      // draw text
      COLORREF crOld;
      crOld = SetTextColor (hDC, fIsMajor ? crMajor : crMinor);
      ExtTextOut (hDC,  pr->left + TICKFONT/2, iY - size.cy, 0, NULL, szTemp, iLen, NULL);
      ExtTextOut (hDC,  pr->right - TICKFONT/2 - size.cx, iY - size.cy, 0, NULL, szTemp, iLen, NULL);
      SetTextColor (hDC, crOld);
   }

   // free font
   SelectObject (hDC, hFontOld);
   DeleteObject (hFontNorm);
   SetBkMode (hDC, iOldMode);

   // free pens
   SelectObject (hDC, hPenOld);
   DeleteObject (hPenMinor);
   DeleteObject (hPenMajor);
}
