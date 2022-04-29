/************************************************************************
CProgress.cpp - Creates a progress bar and shows percent complete.
Time delayed so only shows after 1 second of nothing..

begun 19/5/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#define MYPROGRESSCLASS "ASPProgress"

/******************************************************************************
CProgress::Constructor/destructor
*/
CProgress::CProgress (void)
{
   m_dwStartTime = 0;
   m_dwLastUpdateTime = 0;
   m_hWndParent = NULL;
   m_szTitle[0] = 0;
   m_hWnd = NULL;
   m_fProgress = 0;
   m_lStack.Init (sizeof(TEXTUREPOINT));
}

CProgress::~CProgress (void)
{
   if (m_hWnd)
      DestroyWindow (m_hWnd);
   m_dwStartTime = 0;
}

/******************************************************************************
CProgress::Start - Starts up the progress window. Note: The window is not actually
created until about 1 second of progress reports.

inputs
   HWND        hWnd - To use for the parent window, so progress is centered on this.
                        If NULL, will center on the screen
   char        *pszTitle - Title displayed
   BOOL        fShowRightAway - If set then show the progress right away
returns
   BOOL - TRUE if success
*/
BOOL CProgress::Start (HWND hWnd, char *pszTitle, BOOL fShowRightAway)
{
   if (m_hWnd)
      return FALSE;
   strcpy (m_szTitle, pszTitle);
   m_dwLastUpdateTime = m_dwStartTime = GetTickCount();
   m_hWndParent = hWnd;
   m_hWnd = NULL;
   m_fProgress = 0;
   m_lStack.Clear();

   // if show right away set then do some hacks
   if (fShowRightAway) {
      m_dwStartTime -= 600;   // so thinks has been runnning for lon enough
      Update (0);
   }
   return TRUE;
}

/******************************************************************************
CProgress::Stop - Stops the progress window, destorying it if it exsits.
*/
BOOL CProgress::Stop (void)
{
   if (!m_hWnd)
      return FALSE;
   DestroyWindow (m_hWnd);
   m_hWnd = NULL;
   return TRUE;
}

/******************************************************************************
CProgress::Push - Pushes a new minimum and maximum for the progress meter
so that any code following will think min and max are going from 0..1, but it
might really be going from .2 to .6 (if that's what fMin and fMax) are.

inputs
   fp      fMin,fMax - New min and max, from 0..1. (Afected by previous min and
                  max already on stack.
reutrns
   BOOL - TRUE if success
*/
BOOL CProgress::Push (float fMin, float fMax)
{
   PTEXTUREPOINT ptp = m_lStack.Num() ? (PTEXTUREPOINT) (m_lStack.Get(m_lStack.Num()-1)) : NULL;

   if (ptp) {
      fMin = fMin * (ptp->v - ptp->h) + ptp->h;
      fMax = fMax * (ptp->v - ptp->h) + ptp->h;
   }

   TEXTUREPOINT tp;
   tp.h = fMin;
   tp.v = fMax;
   m_lStack.Add (&tp);

   return TRUE;
}

/******************************************************************************
CProgress::Pop - Removes the last changes for percent complete off the stack -
as added by Push()
*/
BOOL CProgress::Pop (void)
{
   if (!m_lStack.Num())
      return FALSE;
   m_lStack.Remove (m_lStack.Num()-1);
   return TRUE;
}

/****************************************************************************
CProgressWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CIconButton, and calls that.
*/
LRESULT CALLBACK CProgressWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCProgress p = (PCProgress) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCProgress p = (PCProgress) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCProgress) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/******************************************************************************
CProgress::Update - Updates the progress bar. Should be called once in awhile
by the code.

inputs
   fp         fProgress - Value from 0..1 for the progress from 0% to 100%
returns
   int - 1 if in the future give progress indications more often (which means they
         were too far apart), 0 for the same amount, -1 for less often
*/
int  CProgress::Update (float fProgress)
{
   // check to make sure we actually started
   if (!m_dwStartTime)
      return 0;

   // get the current time
   DWORD dwTime;
   int   iRet; // what should return
   dwTime = GetTickCount();
   if (dwTime - m_dwLastUpdateTime > 1000)
      iRet = 1;
   else if (dwTime - m_dwLastUpdateTime < 250)
      iRet = -1;
   else
      iRet = 0;
   m_dwLastUpdateTime = dwTime;

   // progress
   if (m_lStack.Num()) {
      PTEXTUREPOINT ptp = (PTEXTUREPOINT) m_lStack.Get(m_lStack.Num()-1);
      fProgress = fProgress * (ptp->v - ptp->h) + ptp->h;
   }

   // if there's a window then update
   if (m_hWnd) {
      if (fabs(fProgress - m_fProgress) > .01) {
         // only bother if there's a certain amunt of change

         // update the change
         // blank background
         RECT  rOrig;
         GetClientRect (m_hWnd, &rOrig);

         // figure out progress
         int   iProg1, iProg2;
         iProg1 = (int)((rOrig.right - rOrig.left) * m_fProgress + rOrig.left);
         iProg2 = (int)((rOrig.right - rOrig.left) * fProgress + rOrig.left);
         m_fProgress = fProgress;

         rOrig.left = min(iProg1, iProg2) - 1;
         rOrig.right = max(iProg1, iProg2) + 1;
         InvalidateRect (m_hWnd, &rOrig, FALSE);
         UpdateWindow (m_hWnd);

         // BUGFIX - Try to keep updating in release mode, since
         // progress bars seem to die in release, but not debug
         MSG msg;
         while( PeekMessage (&msg, m_hWnd, 0, 0, PM_REMOVE )) {
            TranslateMessage(&msg); 
            DispatchMessage(&msg); 
         } // while peekmessage
      }
   }
   else if (dwTime - m_dwStartTime > 500) {  // BUGFIX - Was 1000, but 1/2 before showing is probably better
      // create the window
      m_fProgress = fProgress;

      static BOOL gfRegistered = FALSE;
      if (!gfRegistered ) {
         WNDCLASS wc;

         memset (&wc, 0, sizeof(wc));
         wc.lpfnWndProc = CProgressWndProc;
         wc.style = 0;
         wc.hInstance = ghInstance;
         wc.hIcon = NULL;
         wc.hCursor = 0;
         wc.hbrBackground = NULL;
         wc.lpszMenuName = NULL;
         wc.lpszClassName = MYPROGRESSCLASS;
         RegisterClass (&wc);

         gfRegistered = TRUE;
      }

      // find out where this goes
      RECT  rParent;
      if (m_hWndParent)
         GetWindowRect (m_hWndParent, &rParent);
      else {
         rParent.left = rParent.top = 0;
         rParent.right = GetSystemMetrics (SM_CXSCREEN);
         rParent.bottom = GetSystemMetrics (SM_CYSCREEN);
      }
      int ix, iy, iHeight;
      ix = (rParent.left + rParent.right) / 2;
      iy = (rParent.top + rParent.bottom) / 2;
      iHeight = GetSystemMetrics (SM_CYSMCAPTION) + GetSystemMetrics (SM_CYBORDER);
      m_hWnd = CreateWindowEx (WS_EX_TOPMOST | WS_EX_TOOLWINDOW, MYPROGRESSCLASS, m_szTitle,
         WS_VISIBLE | WS_BORDER | WS_OVERLAPPED,
         ix - 150, iy - iHeight, 300, iHeight * 2,
         NULL, NULL, ghInstance, (LPVOID)this);
      if (!m_hWnd)
         return 0;
      UpdateWindow (m_hWnd);  // redraw immediately

   }
   else {
      // just remember this and do nothing else
      m_fProgress = fProgress;
   }

   return iRet;
}


/******************************************************************************
CProgress::WndProc - Subclass proc for rich-edit
*/
LRESULT CProgress::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

   switch (uMsg) {

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC   hDC;
         hDC = BeginPaint (hWnd, &ps);

         // blank background
         RECT  rOrig;
         GetClientRect (hWnd, &rOrig);

         // figure out progress
         int   iProg;
         iProg = (int)((rOrig.right - rOrig.left) * m_fProgress + rOrig.left);

         HBRUSH hbr;
         RECT rFill;

         // progress
         rFill = rOrig;
         rFill.right = iProg;
         hbr = CreateSolidBrush (RGB(0xff, 0, 0));
         FillRect (hDC, &rFill, hbr);
         DeleteObject (hbr);

         // what's left
         rFill = rOrig;
         rFill.left = iProg;
         hbr = CreateSolidBrush (RGB(0, 0, 0));
         FillRect (hDC, &rFill, hbr);
         DeleteObject (hbr);

         EndPaint (hWnd, &ps);
      }
      break;

   }
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/******************************************************************************
CProgress::WantToCancel - Returns TRUE if the user has pressed cancel and the
operation should exit.
*/
BOOL CProgress::WantToCancel (void)
{
   return FALSE;  // cant handle cancel in this one
}

/******************************************************************************
CProgress::CanRedraw - Called by the renderer (or whatever) to tell the caller
that the display can be updated with a new image. This is used by the ray-tracer
to indicate it has finished drawing another bucket.
*/
void CProgress::CanRedraw (void)
{
   // do nothing
}
