/***************************************************************************
CPieChart - C++ object for viewing the IconButton.

begun 31/8/2001
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"




#define     MYPIECHARTCLASS    "CPieChart"
#define     TIMERSPEED        100         // timer 10x a second



/****************************************************************************
Constructor and destructor
*/
CPieChart::CPieChart (void)
{
   m_hWnd = NULL;
   m_hBmp = NULL;
   m_cBackground = RGB(0,0,0);
   m_cPie = RGB(0xff,0,0);
   m_fValue = .25;
   m_fValueDrawn = 0;
   m_fValueDelta = .01;
   m_dwID = 0;
   m_pTip = NULL;
   m_fTimer = FALSE;
   m_dwTimerOn = 0;
   m_dwLastTick = (DWORD)-1;
}


CPieChart::~CPieChart (void)
{
   if (m_pTip)
      delete m_pTip;

   if (m_hWnd)
      DestroyWindow (m_hWnd);

   if (m_hBmp)
      DeleteObject (m_hBmp);
}

/****************************************************************************
CPieChart::ColorSet - Sets the color of the icon
inputs
   COLORREF    cBackground, cPie - Colors
returns
   none
*/
void CPieChart::ColorSet (COLORREF cBackground, COLORREF cPie)
{
   if ((m_cBackground == cBackground) && (m_cPie == cPie))
      return;  // nothing changed

   m_cBackground = cBackground;
   m_cPie = cPie;
   if (m_hBmp) {
      // invalidate bitmap
      DeleteObject (m_hBmp);
      m_hBmp = NULL;
   }
   if (m_hWnd)
      InvalidateRect (m_hWnd, NULL, FALSE);
}

   
/****************************************************************************
CPieChartWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CPieChart, and calls that.
*/
LRESULT CALLBACK CPieChartWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCPieChart p = (PCPieChart) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCPieChart p = (PCPieChart) (LONG_PTR)GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCPieChart) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/******************************************************************************
InvalidateParent - Invalidates the rectangle in the parent.

inputs
   HWND     hWnd - child window to invalidate
*/
static void InvalidateParent (HWND hWnd)
{
   // was doing this because buttons were transparent, but now that they're not
   // change code to just invalidate button
   InvalidateRect (hWnd, NULL, FALSE);

#if 0 // dead code
   HWND hWndParent = GetParent(hWnd);
   RECT r, rp;

   GetWindowRect (hWnd, &r);

   POINT px;
   px.x = r.left;
   px.y = r.top;
   ScreenToClient (hWndParent, &px);

   rp.left = px.x;
   rp.top = px.y;
   rp.right = rp.left + r.right - r.left;
   rp.bottom = rp.top + r.bottom - r.top;

   InvalidateRect (hWndParent, &rp, TRUE);
#endif
}

/******************************************************************************
CPieChart::WndProc - Subclass proc for rich-edit
*/
LRESULT CPieChart::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

   switch (uMsg) {

   case WM_CREATE:
      // set the user data to indicate if mouse is over
      if (m_pTip)
         delete m_pTip;
      m_pTip = NULL;

      SetTimer (hWnd, 42, TIMERSPEED, NULL);
      m_fTimer = TRUE;
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC   hDCPS;
         hDCPS = BeginPaint (hWnd, &ps);
         RECT r;
         GetClientRect (hWnd, &r);

         // draw to bitmap
#define SUPERSAMPLE     4
         if (!m_hBmp) {
            CImage Image;

            POINT pSize;
            pSize.x = (r.right - r.left) * SUPERSAMPLE;
            pSize.y = (r.right - r.top) * SUPERSAMPLE;
            HDC hDC = CreateCompatibleDC (hDCPS);
            m_hBmp = CreateCompatibleBitmap (hDCPS, pSize.x, pSize.y);
            SelectObject (hDC, m_hBmp);

            // fill with the background
            HBRUSH hbrBack = CreateSolidBrush (m_cBackground);
            RECT rFill;
            rFill.top = rFill.left = 0;
            rFill.right = pSize.x;
            rFill.bottom = pSize.y;
            FillRect (hDC, &rFill, hbrBack);
            DeleteObject (hbrBack);

            // pie
            m_fValueDrawn = m_fValue;
            if (m_fValue > 0.01) {
               HBRUSH hbrPie = CreateSolidBrush (m_cPie);
               HBRUSH hbrOld = (HBRUSH)SelectObject (hDC, hbrPie);
               HPEN hPen = CreatePen (PS_SOLID, 0,
                  RGB(
                     GetRValue(m_cPie)/2 + GetRValue(m_cBackground)/2,
                     GetGValue(m_cPie)/2 + GetGValue(m_cBackground)/2,
                     GetBValue(m_cPie)/2 + GetBValue(m_cBackground)/2
                  ));
               HPEN hPenOld = (HPEN)SelectObject (hDC, hPen);

               POINT pCenter;
               pCenter.x = pSize.x / 2;
               pCenter.y = pSize.y / 2;
               int iRadius = pCenter.x;
               fp fRadius = iRadius;
               POINT pStart;
               pStart.x = (int)(sin(m_fValue * 2.0 * PI) * fRadius) + pCenter.x;
               pStart.y = (int)(-cos(m_fValue * 2.0 * PI) * fRadius) + pCenter.y;

               if (m_fValue < 0.99)
                  Pie (hDC, 0, 0, 2*iRadius, 2*iRadius,
                     pStart.x, pStart.y,
                     pCenter.x, r.top);
               else
                  Ellipse (hDC, 0, 0, 2*iRadius, 2*iRadius);

               SelectObject (hDC, hPenOld);
               SelectObject (hDC, hbrOld);
               DeleteObject (hbrPie);
               DeleteObject (hPen);
            }

            DeleteDC (hDC);

            Image.Init (m_hBmp);
            DeleteObject (m_hBmp);
            m_hBmp = Image.ToBitmapAntiAlias (hDCPS, SUPERSAMPLE);
         } // if !m_hBmp

         // bit blit
         if (m_hBmp) {
            HDC hDC = CreateCompatibleDC (hDCPS);
            SelectObject (hDC, m_hBmp);
            BitBlt (hDCPS, r.left, r.top, r.right-r.left, r.bottom-r.top, hDC, 0,0, SRCCOPY);
            DeleteDC (hDC);
         }

         EndPaint (hWnd, &ps);
      }
      break;

   case WM_TIMER: // track if cursor over
      {
         // store the time for the tick so that know accurate how much
         // time has passed. This is especially important for short
         // pie charts, like 2 second ones
         DWORD dwTime = GetTickCount();
         DWORD dwDelta = TIMERSPEED;
         if (dwTime >= m_dwLastTick)
            dwDelta = dwTime - m_dwLastTick;
         m_dwLastTick = dwTime;

         // update pie
         if (m_fValueDelta) {
            fp fNew = m_fValue + m_fValueDelta * (fp)dwDelta / 1000.0;
            fNew = max(fNew, 0);
            fNew = min(fNew, 1);
            m_fValue = fNew;
            if (fabs(fNew - m_fValueDrawn) > 0.01) { // only if changes enough
               if (m_hBmp) {
                  DeleteObject (m_hBmp);
                  m_hBmp = NULL;
               }
               InvalidateParent (hWnd);
            }
         }

         m_dwTimerOn += TIMERSPEED;

         POINT p;
         RECT  rw;
         GetCursorPos (&p);
         GetWindowRect (hWnd, &rw);
         if (PtInRect (&rw, p)) {
            if (!m_pTip && (m_dwTimerOn >= 1000)) { // BUGFIX - was 0, but dont bring up so quickly
               CMem  memTemp;

               memTemp.Required (wcslen((PWSTR) m_memTip.p)*2 + 512);
               char *psz;
               psz = (char*) memTemp.p;
               strcpy (psz, "<bold>");
               WideCharToMultiByte (CP_ACP, 0, (PWSTR)m_memTip.p, -1, psz + strlen(psz), (DWORD)memTemp.m_dwAllocated, 0, 0);
               strcat (psz, "</bold>");

               // show tool tip if hover long enough
               m_pTip = new CToolTip;
               RECT r;
               GetWindowRect (m_hWnd, &r);
               m_pTip->Init (psz, 2, &r, GetParent(m_hWnd));
            }

            return 0;   // still over
         }

         // else moved off
         // BUGFIX - On WinNT or different version of windows must be HIWORD window handle
         if (m_pTip)
            delete m_pTip;
         m_pTip = NULL;
      }
      return 0;

   case WM_DESTROY:
      if (m_fTimer)
         KillTimer (hWnd, 42);

      break;

   case WM_LBUTTONDOWN:
   case WM_MBUTTONDOWN:
   case WM_RBUTTONDOWN:
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return 0;

   case WM_MOUSEMOVE:
      m_dwTimerOn = 0;
      break;

   case WM_SIZE:
      if (m_hBmp) {
         DeleteObject (m_hBmp);
         m_hBmp = NULL;
      }
      break;
   }
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/****************************************************************************8
CPieChart::Init - Creates a IconButton window so that it doesn't overlap the given
rectangle.

inputs
   HINSTANCE hInstance - Where to get the icon rom
   PWSTR     pszTip - tooltip string. NOTE: This is sanitized when set
   RECT     *pr - rectangle for the button
   HWND     hWndParent - parent of this window
   DWORD    dwID - ID sent when the button is clicked
returns
   HWND - IconButton
*/
BOOL CPieChart::Init (HINSTANCE hInstance, PWSTR pszTip, RECT *pr, HWND hWndParent, DWORD dwID)
{
   if (m_hWnd)
      return FALSE;

   static BOOL gfRegistered = FALSE;
   if (!gfRegistered ) {
      WNDCLASS wc;

      memset (&wc, 0, sizeof(wc));
      wc.lpfnWndProc = CPieChartWndProc;
      wc.style = 0;
      wc.hInstance = ghInstance;
      wc.hIcon = NULL;
      wc.hCursor = (HCURSOR) LoadImage (ghInstance, MAKEINTRESOURCE(IDC_CURSORNO), IMAGE_CURSOR,
         32, 32, LR_DEFAULTCOLOR);
      wc.hbrBackground = NULL;
      wc.lpszMenuName = NULL;
      wc.lpszClassName = MYPIECHARTCLASS;
      RegisterClass (&wc);

      gfRegistered = TRUE;
   }

   MemZero (&m_memTip);
   MemCatSanitize (&m_memTip, pszTip);

   m_dwID = dwID;

   m_hWnd = CreateWindowEx (0 /*WS_EX_TRANSPARENT*/, MYPIECHARTCLASS, "",
      WS_VISIBLE | WS_CHILD,
      pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top,
      hWndParent, NULL, ghInstance, (LPVOID)this);
   if (!m_hWnd)
      return FALSE;


   return TRUE;
}


/**********************************************************************************
CPieChart::Enable - Enable or disable the button.

inptus
   BOOL     fEnable - if TRUE then enable, else disable
*/
void CPieChart::Enable (BOOL fEnable)
{
   // if already enabled do nothing
   if (IsWindowEnabled(m_hWnd) == fEnable)
      return;

   EnableWindow (m_hWnd, fEnable);
   InvalidateParent (m_hWnd);
}



/***********************************************************************************
CPieChart::Move - Moves a button

inputs
   RECT     *pr - Move to
returns
   BOOL - TRUE if success
*/
BOOL CPieChart::Move (RECT *pr)
{
   // clera old
   InvalidateParent (m_hWnd);

   MoveWindow (m_hWnd, pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top, TRUE);

   // clear new position
   InvalidateParent (m_hWnd);

   return TRUE;
}


/***********************************************************************************
CPieChart::PieSet - Sets the value of the pie chart.

inputs
   fp          fValue - Value from 0 to 1
   fp          fValueDelta - How much the value changes, per second
*/
void CPieChart::PieSet (fp fValue, fp fValueDelta)
{
   m_fValueDelta = fValueDelta;
   fValue = min(fValue, 1);
   fValue = max(fValue, 0);
   if (fValue == m_fValue)
      return;  // no change

   // else, will need to redraw
   m_fValue = fValue;
   if (m_hBmp) {
      DeleteObject (m_hBmp);
      m_hBmp = NULL;
   }
   InvalidateParent (m_hWnd);
}
