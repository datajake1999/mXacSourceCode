/*************************************************************************************
CSlidingWindow.cpp - Window on bottom and top that slide in/out

begun 18/6/06 by Mike Rozak.
Copyright 2006 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <zmouse.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"




// defines
#define TABWIDTH                    128
#define TABSPACING                  25       // spacing between top and bottom of tab, excluding line
#define TOOLBARBUTTONHEIGHT         (24) // BUGFIX - Try smaller *3/2)
#define TABHEIGHT                   (TABSPACING+1)
#define TOPHEIGHT                   (TABHEIGHT + ToolbarHeight())
#define BOTTOMHEIGHT                TABHEIGHT
#define PIXELSSHOWING               (TABHEIGHT/6)
#define COMMANDSPACE                 2  // extra spacing around edit window


#define TASKBARHEIGHT               TABHEIGHT

#define IDC_EDITCOMMAND             1020     // edit control
#define IDC_MYCLOSE                 861     // my close command

#define SLIDETIMERID                42
#define SLIDETIMERMILLISEC          100      // slide every 100th of a sec
#define SLIDETIMERTOTAL             500     // takes 1/2 second to slide in out

#define TIMER_TASKBARSYNC           2045     // syncronize task bar every second
#define TIMER_TASKBARSYNCTIMER      2046     // syncronize task bar in 100 ms
#define TIMER_TURNOFFHIGHLIGHT      2047     // turn off the highlights over the tabs and taskbar


#define USEONLYONETOOLBAR           // if set then use only one toolbar, the ontop on



static PWSTR gapszTabName[NUMTABS] = {L"Explore (F1)", L"Chat (F2)", L"Combat (F3)" , L"Zoom (F4)"};
static WNDPROC gpEditWndProc = NULL;

/*************************************************************************************
CSlidingWindow::Constructor and destructor
*/
CSlidingWindow::CSlidingWindow (void)
{
   m_fOnTop = FALSE;
   m_fLocked = FALSE;
   m_hWnd = NULL;
   m_pMain = NULL;
   m_hWndParent = NULL;
   m_fSlideTimer = FALSE;
   m_iStayOpenTime = 0;

   m_iTabHighlight = -1;
   m_dwTab = 0;
   m_lPCPieChart.Init (sizeof(PCPieChart));

   m_lTASKBARWINDOW.Init (sizeof(TASKBARWINDOW));
   m_fTaskBarTimer = FALSE;
   m_iTaskBarHighlight = -1;


   LOGFONT lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = FontScaleByScreenSize(-20); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfWeight = FW_BOLD;
   lf.lfItalic = TRUE;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   m_hFontBig = CreateFontIndirect (&lf);

   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = FontScaleByScreenSize(-18);
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfWeight = FW_BOLD;
   lf.lfItalic = TRUE;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Symbol");
   m_hFontSymbol = CreateFontIndirect (&lf);

   m_hWndCommand = NULL;

   m_fToolbarVisible = FALSE;

}

CSlidingWindow::~CSlidingWindow (void)
{
   // kill all the pie charts
   DWORD i;
   PCPieChart *ppc = (PCPieChart*)m_lPCPieChart.Get(0);
   for (i = 0; i < m_lPCPieChart.Num(); i++)
      if (ppc[i])
         delete ppc[i];
   m_lPCPieChart.Clear();

   if (IsWindow (m_hWnd))
      DestroyWindow (m_hWnd);

   if (m_hFontBig)
      DeleteObject (m_hFontBig);
   if (m_hFontSymbol)
      DeleteObject (m_hFontSymbol);
}



/************************************************************************************
SlidingWindowWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK SlidingWindowWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCSlidingWindow p = (PCSlidingWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCSlidingWindow p = (PCSlidingWindow) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCSlidingWindow) lpcs->lpCreateParams;

         p->m_hWnd = hWnd; // to remember
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*************************************************************************************
CSlidingWindow::Init - Initializes the sliding window code.

inputs
   HWND           hWndParent - Parent window to create in
   BOOL           fOnTop - If TRUE, this window is created at the top and of hWndParent.
                     If FALSE, the bottom end
   BOOL           fLocked - If TRUE then the window is locked in place. Otherwise, it
                     moves up/down
   PCMainWindow   pMain - Main window

returns
   BOOL - TRUE if success. FALSE if error
*/
BOOL CSlidingWindow::Init (HWND hWndParent, BOOL fOnTop, BOOL fLocked, PCMainWindow pMain)
{
   if (m_hWnd)
      return FALSE;

   m_fOnTop = fOnTop;
   m_fLocked = fLocked;
   m_pMain = pMain;
   m_hWndParent = hWndParent;

   // register the class
   // create window
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = SlidingWindowWndProc;
   wc.lpszClassName = "CircumrealityClientSlidingWindow";
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = NULL;
   wc.hCursor = NULL;
   wc.hbrBackground = NULL;
   RegisterClass (&wc);

   // get the default coords
   RECT r;
   DefaultCoords (m_fLocked ? 2 : 0, &r);

   // create the window
   m_hWnd = CreateWindowEx (
      WS_EX_TOPMOST,
      wc.lpszClassName, "Sliding window",
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
      r.left , r.top , r.right - r.left , r.bottom - r.top ,
      m_hWndParent, NULL, ghInstance, (PVOID) this);

   if (!m_hWnd)
      return FALSE;

   return TRUE;
}



/*************************************************************************************
CSlidingWindow::DefaultCoords - Fills in a rectangle with the default coordingates
for where the client should go.

inputs
   DWORD          dwLocked - 0 for hidden away, 1 for half locked, 2 for fully locked (and open)
   PRECT          pr - Filled in
returns
   none
*/
void CSlidingWindow::DefaultCoords (DWORD dwLocked, PRECT pr)
{
   GetClientRect (m_hWndParent, pr);

   if (m_fOnTop)
      pr->bottom = pr->top + TOPHEIGHT;
   else
      pr->top = pr->bottom - BOTTOMHEIGHT;

   // if not locked then should be scrolled so only a few pixels show
   if (dwLocked < 2) {  // was !fLocked
      int iDelta = ((pr->bottom - pr->top) - PIXELSSHOWING) * (2 - dwLocked) / 2;
      if (m_fOnTop)
         iDelta *= -1;
      pr->top += iDelta;
      pr->bottom += iDelta;
   }
}


/************************************************************************************
EditSubclassWndProc - to subclass edit control so sends enter up.
*/
static LRESULT CALLBACK EditSubclassWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CHAR:
      if (wParam == L'\r') {
         // allow to slide down right away
         gpMainWindow->m_pSlideTop->SlideDownTimed (0);

         SendMessage (GetParent(hWnd), uMsg, wParam, lParam);
         return 0;
      }
      else
         // make sure it's open for a few seconds
         gpMainWindow->m_pSlideTop->SlideDownTimed (2000);

      break;

   case WM_MOUSEWHEEL:
      // pass up
      return SendMessage (GetParent(hWnd), uMsg, wParam, lParam);

   }

   // else
   if (gpEditWndProc)
      return CallWindowProc (gpEditWndProc, hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CSlidingWindow::ChildWindowContextMenu - Do an effect on the child window

inputs
   PTASKBARWINDOW       ptbw - Taskbar window affecting
   int                  iAction - ID_TASKBAR_SHOW to show/hide taskbar, ID_TASKBAR_DISPLAYONSECONDMONITOR to move to another
                           monitor, ID_TASKBAR_SHOWTITLE to show/hide title
*/
void CSlidingWindow::ChildWindowContextMenu (PTASKBARWINDOW ptbw, int iAction)
{

   switch (ptbw->dwType) {
   case 11: // map
      if (iAction == ID_TASKBAR_SHOW) {
         if (m_pMain->IsWindowObscured(m_pMain->m_pMapWindow->m_hWnd) && IsWindowVisible(m_pMain->m_pMapWindow->m_hWnd) && !m_pMain->m_pMapWindow->m_fHiddenByUser) {
            SetWindowPos (m_pMain->m_pMapWindow->m_hWnd, (HWND)HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            TaskBarSyncTimer ();
         }
         else
            m_pMain->ChildShowToggle (TW_MAP, NULL, 0);
      }
      else if (iAction == ID_TASKBAR_DISPLAYONSECONDMONITOR)
         m_pMain->ChildMoveMonitor (TW_MAP, NULL);
      else
         m_pMain->ChildTitleShowHide (!m_pMain->ChildHasTitle(ptbw->hWnd), ptbw->hWnd, TW_MAP, NULL);
      return;

   case 12: // transcript
      if (iAction == ID_TASKBAR_SHOW) {
         if (m_pMain->IsWindowObscured(m_pMain->m_hWndTranscript) && IsWindowVisible(m_pMain->m_hWndTranscript)) {
            SetWindowPos (m_pMain->m_hWndTranscript, (HWND)HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            TaskBarSyncTimer ();
         }
         else
            m_pMain->ChildShowToggle (TW_TRANSCRIPT, NULL, 0);
      }
      else if (iAction == ID_TASKBAR_DISPLAYONSECONDMONITOR)
         m_pMain->ChildMoveMonitor (TW_TRANSCRIPT, NULL);
      else
         m_pMain->ChildTitleShowHide (!m_pMain->ChildHasTitle(ptbw->hWnd), ptbw->hWnd, TW_TRANSCRIPT, NULL);
      return;

   //case 13: // command line
   //   m_pMain->ChildTitleShowHide (!m_pMain->ChildHasTitle(ptbw->hWnd), ptbw->hWnd, TW_EDIT, NULL);
   //   return;

   case 14: // menu
      if (iAction == ID_TASKBAR_DISPLAYONSECONDMONITOR)
         m_pMain->ChildMoveMonitor (TW_MENU, NULL);
      else
         m_pMain->ChildTitleShowHide (!m_pMain->ChildHasTitle(ptbw->hWnd), ptbw->hWnd, TW_MENU, NULL);
      return;

   case 1:  // icon window
      {
         // find the icon window
         DWORD i;
         PCIconWindow *ppi = (PCIconWindow*)m_pMain->m_lPCIconWindow.Get(0);
         for (i = 0; i < m_pMain->m_lPCIconWindow.Num(); i++)
            if (ppi[i]->m_hWnd == ptbw->hWnd)
               break;
         if (i >= m_pMain->m_lPCIconWindow.Num())
            return;   // not likely to happen, but window just disappoeared

         if (iAction == ID_TASKBAR_SHOW) {
            if (m_pMain->IsWindowObscured(ppi[i]->m_hWnd) && IsWindowVisible(ppi[i]->m_hWnd)) {
               SetWindowPos (ppi[i]->m_hWnd, (HWND)HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
               TaskBarSyncTimer ();
            }
            else
               m_pMain->ChildShowToggle (TW_ICONWINDOW, (PWSTR) ppi[i]->m_memID.p, 0);
         }
         else if (iAction == ID_TASKBAR_DISPLAYONSECONDMONITOR)
            m_pMain->ChildMoveMonitor (TW_ICONWINDOW, (PWSTR) ppi[i]->m_memID.p);
         else
            m_pMain->ChildTitleShowHide (!m_pMain->ChildHasTitle(ptbw->hWnd), ptbw->hWnd, TW_ICONWINDOW, (PWSTR)ppi[i]->m_memID.p);
      }
      return;

   case 0:  // display window
      {
         // find the display window
         DWORD i;
         PCDisplayWindow *ppd = (PCDisplayWindow*)m_pMain->m_lPCDisplayWindow.Get(0);
         for (i = 0; i < m_pMain->m_lPCDisplayWindow.Num(); i++)
            if (ppd[i]->m_hWnd == ptbw->hWnd)
               break;
         if (i >= m_pMain->m_lPCDisplayWindow.Num())
            return;   // not likely to happen, but window just disappoeared

         if (iAction == ID_TASKBAR_SHOW) {
            if (m_pMain->IsWindowObscured(ppd[i]->m_hWnd) && IsWindowVisible(ppd[i]->m_hWnd)) {
               SetWindowPos (ppd[i]->m_hWnd, (HWND)HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
               TaskBarSyncTimer ();
            }
            else
               m_pMain->ChildShowToggle (TW_DISPLAYWINDOW, (PWSTR) ppd[i]->m_memID.p, 0);
         }
         else if (iAction == ID_TASKBAR_DISPLAYONSECONDMONITOR)
            m_pMain->ChildMoveMonitor (TW_DISPLAYWINDOW, (PWSTR) ppd[i]->m_memID.p);
         else
            m_pMain->ChildTitleShowHide (!m_pMain->ChildHasTitle(ptbw->hWnd), ptbw->hWnd, TW_DISPLAYWINDOW, (PWSTR)ppd[i]->m_memID.p);
      }
   } // type
}


/*************************************************************************************
CSlidingWindow::WndProc - Window procedure
*/
LRESULT CSlidingWindow::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      SetTimer (hWnd, TIMER_TURNOFFHIGHLIGHT, 500, NULL);

      if (m_fOnTop) {
         RECT r;

         // create the close button
         CloseButtonLoc (&r);
         m_CloseButton.ColorSet (m_pMain->m_cBackgroundNoTabNoChange, m_pMain->m_cTextDimNoChange, m_pMain->m_cTextDimNoChange, m_pMain->m_cTextNoChange);
            // BUGFIX - Changed m_cVerbDim to m_cBackgroundNoTab
         m_CloseButton.Init (IDB_VERBEXIT, ghInstance, NULL, 
            "Exit", "Press this to stop playing CircumReality.", 3 /* left */, &r, hWnd, IDC_MYCLOSE);

         // create thc command window
         CommandLoc (&r);

         m_hWndCommand = CreateWindowEx (WS_EX_NOPARENTNOTIFY, "Edit", "",
            WS_CHILD | /*WS_VISIBLE |*/ WS_CLIPSIBLINGS | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL,
            r.left, r.top, r.right-r.left, r.bottom-r.top,
            hWnd, (HMENU)IDC_EDITCOMMAND, ghInstance, NULL);


         // NOTE: Because edit window is a child of another child window, selection
         // doesnt work properly. This is a bug/feature in windows
         gpEditWndProc = (WNDPROC)(LONG_PTR)GetWindowLongPtr (m_hWndCommand, GWLP_WNDPROC);
         SetWindowLongPtr (m_hWndCommand, GWLP_WNDPROC, (LONG_PTR) EditSubclassWndProc);
         SendMessage (m_hWndCommand, EM_LIMITTEXT, 256, 0);
         m_pMain->CommandSetFocus(FALSE);

#ifdef USEONLYONETOOLBAR
         SetTimer (hWnd, TIMER_TASKBARSYNC, 2000, NULL);
#endif
      }
      else // !m_fOnTop
         SetTimer (hWnd, TIMER_TASKBARSYNC, 2000, NULL);

      return 0;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC = BeginPaint (hWnd, &ps);

         RECT rClient;
         GetClientRect (hWnd, &rClient);

         HDC hDCBmp;
         HBITMAP hBmp;
         if (m_fOnTop) {
            hDCBmp = CreateCompatibleDC (hDC);
            hBmp = CreateCompatibleBitmap (hDC, rClient.right, rClient.bottom);
            SelectObject (hDCBmp, hBmp);

            TabsDraw (hDCBmp, &rClient, m_iTabHighlight);

            // button draw
            RECT rHDC;
            rHDC.top = rHDC.left = 0;
            rHDC.right = rClient.right - rClient.left;
            rHDC.bottom = rClient.bottom - rClient.top;
            CircumRealityButtonDraw (hDCBmp, &rHDC, &rClient, m_iTabHighlight);

            BitBlt(hDC, rClient.left, rClient.top, rClient.right - rClient.left, rClient.bottom - rClient.top,
               hDCBmp, 0, 0, SRCCOPY);
            DeleteDC (hDCBmp);
            DeleteObject (hBmp);
         }
         else {
            // paint the taskbar
            RECT rHDC;
            hDCBmp = CreateCompatibleDC (hDC);
            hBmp = CreateCompatibleBitmap (hDC, rClient.right - rClient.left, rClient.bottom - rClient.top);
            rHDC.top = rHDC.left = 0;
            rHDC.right = rClient.right - rClient.left;
            rHDC.bottom = rClient.bottom - rClient.top;
            SelectObject (hDCBmp, hBmp);
            TaskBarDraw (hDCBmp, &rHDC, &rClient, m_iTaskBarHighlight);
            BitBlt(hDC, rClient.left, rClient.top, rClient.right - rClient.left, rClient.bottom - rClient.top,
               hDCBmp, 0, 0, SRCCOPY);
            DeleteDC (hDCBmp);
            DeleteObject (hBmp);
         }


         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_DESTROY:
      if (m_hWndCommand) {
         DestroyWindow (m_hWndCommand);
         m_hWndCommand = NULL;
      }

      KillTimer (hWnd, TIMER_TURNOFFHIGHLIGHT);

      if (!m_fOnTop)
         KillTimer (hWnd, TIMER_TASKBARSYNC);
      else {
#ifdef USEONLYONETOOLBAR
         KillTimer (hWnd, TIMER_TASKBARSYNC);
#endif
      }
      if (m_fTaskBarTimer) {
         KillTimer (hWnd, TIMER_TASKBARSYNCTIMER);
         m_fTaskBarTimer = FALSE;
      }
      if (m_fSlideTimer) {
         KillTimer (hWnd, SLIDETIMERID);
         m_fSlideTimer = FALSE;
      }
      break;

   case WM_TIMER:
      if (wParam == SLIDETIMERID) {
         SlideTimer (TRUE);
         return 0;
      }
      else if (wParam == TIMER_TASKBARSYNC) {
         TaskBarSync ();
         return 0;
      }
      else if (wParam == TIMER_TASKBARSYNCTIMER) {
         KillTimer (m_hWnd, TIMER_TASKBARSYNCTIMER);
         m_fTaskBarTimer = FALSE;
         TaskBarSync ();
         return 0;
      }
      else if (wParam == TIMER_TURNOFFHIGHLIGHT) {
         // simulate a WM_MOUSEMOVE so can potentially turn off
         // highlights
         // NOTE: just copying most of the code from mouse move
         POINT p;
         GetCursorPos (&p);
         ScreenToClient (m_hWnd, &p);

         int iOldTab, iOver, iOldTaskBar;
         // RECT rTab;

         if (m_fOnTop) {
            // remember old highlight values
            iOldTab = m_iTabHighlight;
            m_iTabHighlight = -1;
            m_iTabHighlight = TabOverItem (p);

            if (m_iTabHighlight != iOldTab)
               InvalidateRect (m_hWnd, NULL, FALSE);
         }
         else {
            // over the taskbar
            iOver = TaskBarOverItem (p, TRUE);
            iOldTaskBar = m_iTaskBarHighlight;
            m_iTaskBarHighlight = -1;
            if (iOver != -1)
               m_iTaskBarHighlight = iOver;

            // potentially update display
            if (m_iTaskBarHighlight != iOldTaskBar)
               InvalidateRect (m_hWnd, NULL, FALSE);
         }
         return 0;
      }
      break;

   case WM_MOUSEWHEEL:
      // pass up
      return SendMessage (GetParent(hWnd), uMsg, wParam, lParam);

   case WM_COMMAND:
      if (wParam == IDC_MYCLOSE) {
         PostMessage (GetParent(hWnd), WM_CLOSE, 0, 0);
         return 0;
      }
      else if (m_pMain->m_pResVerb && (wParam >= 1000) && (wParam < 1000 + m_pMain->m_pResVerb->m_lPCResVerbIcon.Num())) {
         DWORD i = wParam;
         PCResVerbIcon pvi = *((PCResVerbIcon*)m_pMain->m_pResVerb->m_lPCResVerbIcon.Get(i-1000));

         if (!pvi->m_fHasClick) {
            // deselect the old one
            m_pMain->VerbDeselect ();

            if (m_pMain->m_fMessageDiabled || m_pMain->m_fMenuExclusive) {
               BeepWindowBeep (ESCBEEP_DONTCLICK);
               return 0;
            }


            // no object, so simple click
            m_pMain->SendTextCommand (pvi->m_lid, (PWSTR)pvi->m_memDo.p, NULL, NULL, NULL, TRUE, TRUE, TRUE);
            m_pMain->HotSpotDisable();
            BeepWindowBeep (ESCBEEP_LINKCLICK);
            return 0;
         }

         // if it's already selected then deselect
         if ((m_pMain->m_dwVerbSelected == i-1000) && !m_pMain->m_fVerbSelectedIcon) {
            // deselect this
            m_pMain->VerbDeselect ();
         }
         else {
            // select
            m_pMain->VerbSelect (NULL, NULL, NULL, i-1000, FALSE);

            // select this
            pvi->m_pButton->FlagsSet (pvi->m_pButton->FlagsGet() | IBFLAG_REDARROW);
         }
         BeepWindowBeep (ESCBEEP_MENUOPEN);  // note: have different beep
         m_pMain->VerbTooltipUpdate ();
         m_pMain->HotSpotTooltipUpdate();
         return 0;
      }
      break;

   case WM_CHAR:
      if (m_fOnTop && (wParam == L'\r') && IsWindowVisible (m_hWndCommand) && !m_pMain->m_fMessageDiabled && !m_pMain->m_fMenuExclusive) {
      // NOTE: Disabling enter if it's been too soon since last command
#ifdef _DEBUG
         OutputDebugString ("\r\nEnter pressed");
#endif

         CMem memANSI, memWide;
         if (!memANSI.Required (GetWindowTextLength (m_hWndCommand)+1))
            return 0;
         GetWindowText (m_hWndCommand, (char*)memANSI.p, (DWORD)memANSI.m_dwAllocated);
         DWORD dwLen = (DWORD)strlen((char*)memANSI.p);
         if (!dwLen) {
            EscChime (ESCCHIME_WARNING);
            return 0;
         }
         SetWindowText (m_hWndCommand, "");

         if (!memWide.Required ((dwLen+1)*sizeof(WCHAR)))
            return 0;
         MultiByteToWideChar (CP_ACP, 0, (char*)memANSI.p, -1, (PWSTR) memWide.p, (DWORD)memWide.m_dwAllocated);
         
         m_pMain->SendTextCommand (DEFLANGID, (PWSTR)memWide.p, NULL, NULL, NULL, FALSE, TRUE, TRUE);
            // BUGBUG - right now hard code to english, but language depends on user setting
         m_pMain->HotSpotDisable();
      } // if edit and pres senter
      break;

   case WM_MOUSEACTIVATE:
      m_pMain->CommandSetFocus(FALSE);
      break;

   case WM_SHOWWINDOW:
      // if showing the window set focus to the edit control
      m_pMain->CommandSetFocus(FALSE);
      break;

   case WM_SETFOCUS:
      m_pMain->CommandSetFocus(FALSE);
      break;


   case WM_LBUTTONDOWN:
      {
         POINT p;
         p.x = (short)LOWORD(lParam);
         p.y = (short)HIWORD(lParam);

         if (m_fOnTop) {
            int iOver = TabOverItem (p);
            if ((iOver >= 0) && (iOver < NUMTABS)) {
               m_pMain->TabSwitch ((DWORD)iOver);
               return 0;
            }
#ifdef USEONLYONETOOLBAR
            else if (iOver == -2) {
               goto showmenu;
            }
#endif
         }
         else { // bottom
            // see what clicked on
            int iOver = TaskBarOverItem (p, TRUE);
            if (iOver >= 0) {
               BOOL fMenu = FALSE;
               if (iOver >= 1000) {
                  iOver -= 1000;
                  fMenu = TRUE;
               }

               PTASKBARWINDOW ptbw = (PTASKBARWINDOW) m_lTASKBARWINDOW.Get((DWORD) iOver);
               if (!ptbw) {
                  BeepWindowBeep (ESCBEEP_DONTCLICK);
                  return 0;
               }

               // if it's a menu then ask
               int iAction = ID_TASKBAR_SHOW;
               if (fMenu) {
                  BeepWindowBeep (ESCBEEP_MENUOPEN);

                  HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUTASKBAR));
                  HMENU hSub = GetSubMenu(hMenu,0);

                  // enable/disable show/hide
                  if (ptbw->fCantShowHide)
                     EnableMenuItem (hSub, ID_TASKBAR_SHOW, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED );
                  if (!gfMonitorUseSecond)
                     EnableMenuItem (hSub, ID_TASKBAR_DISPLAYONSECONDMONITOR, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED );

                  POINT p;
                  GetCursorPos (&p);
                  iAction = TrackPopupMenu (hSub, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
                     p.x, p.y, 0, m_pMain->m_hWndPrimary, NULL);

                  DestroyMenu (hMenu);
                  if (!iAction)
                     return 0;
               }

               ChildWindowContextMenu (ptbw, iAction);


               return 0;   // done
            }
            else if (iOver == -2) { // click on main menu
showmenu:
               BeepWindowBeep (ESCBEEP_MENUOPEN);

               // add thei to the main menu
               HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUMAINMENU));
               HMENU hSub = GetSubMenu(hMenu,0);

               // create the popup menus
               HMENU hMenuPopMove = CreatePopupMenu();
               HMENU hMenuPopShow = CreatePopupMenu();
               HMENU hMenuPopSwitch = gfMonitorUseSecond ? CreatePopupMenu() : NULL;
               PTASKBARWINDOW ptbw = (PTASKBARWINDOW) m_lTASKBARWINDOW.Get(0);
               WCHAR szTemp[512];
               DWORD i;
               for (i = 0; i < m_lTASKBARWINDOW.Num(); i++, ptbw++) {
                  // get the text
                  if (!ptbw->pszName) {
                     wcscpy (szTemp,  L"Unknown");
                     if (IsWindow(ptbw->hWnd))
                        GetWindowTextW (ptbw->hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR));
                  }

                  // append menus
                  AppendMenuW (hMenuPopMove, 
                     MF_ENABLED | MF_STRING | (m_pMain->ChildHasTitle(ptbw->hWnd) ? MF_CHECKED : 0),
                     1000 + i,
                     ptbw->pszName ? ptbw->pszName : szTemp);
                  if (!ptbw->fCantShowHide)
                     AppendMenuW (hMenuPopShow, 
                        MF_ENABLED | MF_STRING | (m_pMain->IsWindowObscured(ptbw->hWnd) ? 0 : MF_CHECKED),
                        2000 + i,
                        ptbw->pszName ? ptbw->pszName : szTemp);
                  if (hMenuPopSwitch)
                     AppendMenuW (hMenuPopSwitch, 
                        MF_ENABLED | MF_STRING | (m_pMain->ChildOnMonitor(ptbw->hWnd) ? MF_CHECKED : 0),
                        3000 + i,
                        ptbw->pszName ? ptbw->pszName : szTemp);

               } // i

               // insert menus for popups
               if (hMenuPopSwitch)
                  InsertMenuW (hSub, 2,
                     MF_BYPOSITION | MF_POPUP | MF_ENABLED,
                     (UINT_PTR) hMenuPopSwitch,
                     L"Move a window to your second m&onitor");
               InsertMenuW (hSub, 2,
                  MF_BYPOSITION | MF_POPUP | MF_ENABLED,
                  (UINT_PTR) hMenuPopShow,
                  L"&Show/hide a window");
               InsertMenuW (hSub, 2,
                  MF_BYPOSITION | MF_POPUP | MF_ENABLED,
                  (UINT_PTR) hMenuPopMove,
                  L"Let me &move/size a window");

               // show the menu
               POINT p;
               GetCursorPos (&p);
               int iRet;
               iRet = TrackPopupMenu (hSub, TPM_RIGHTALIGN | (m_fOnTop ? TPM_TOPALIGN : TPM_BOTTOMALIGN) | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
                  p.x, p.y, 0, hWnd, NULL);
               DestroyMenu (hMenu); // BUGFIX - added

               if ((iRet >= 1000) && (iRet <= 4000)) {
                  // user clicked on one of the sub-menus for items
                  ptbw = (PTASKBARWINDOW) m_lTASKBARWINDOW.Get((DWORD)iRet % 1000);
                  if (!ptbw)
                     return 0;   // shouldnt happen, but might

                  // action
                  int iAction;
                  switch (iRet / 1000) {
                  case 1:
                  default:
                     iAction = ID_TASKBAR_SHOWTITLE;
                     break;
                  case 2:
                     iAction = ID_TASKBAR_SHOW;
                     break;
                  case 3:
                     iAction = ID_TASKBAR_DISPLAYONSECONDMONITOR;
                     break;
                  } // swtich

                  ChildWindowContextMenu (ptbw, iAction);
                  return 0;
               }

               switch (iRet) {
               case ID_MAINMENU_EXIT:
                  PostMessage (m_pMain->m_hWndPrimary, WM_CLOSE, 0, 0);
                  return 0;

               case ID_MAINMENU_SETTINGSANDOPTIONS:
                  m_pMain->DialogSettings (0);
                  return 0;

               } // switch


               return 0;   // done
            }
         }

         // if get here then beep
         BeepWindowBeep (ESCBEEP_DONTCLICK);
      }
      break;

   case WM_MOUSEMOVE:
      {
         // make sure the slider is in position
         SlideTimer (FALSE);
   
         HCURSOR hCursor;
         POINT p;
         p.x = (short)LOWORD(lParam);
         p.y = (short)HIWORD(lParam);

         hCursor = m_pMain->m_hCursorNo;
         int iOver, iOldTab, iOldTaskBar;

         if (m_fOnTop) {
            // remember old highlight values
            iOldTab = m_iTabHighlight;
            m_iTabHighlight = -1;

            iOver = TabOverItem (p);
            if ((iOver >= 0) && (iOver < NUMTABS))
               hCursor = m_pMain->m_hCursorHand;
            else if (iOver == -2)
               hCursor = m_pMain->m_hCursorMenu;

            m_iTabHighlight =  iOver;
         }
         else {
            // over the taskbar
            iOldTaskBar = m_iTaskBarHighlight;
            m_iTaskBarHighlight = -1;
            iOver = TaskBarOverItem (p, TRUE);
            if (iOver != -1) {
               if (iOver == -2)
                  hCursor = m_pMain->m_hCursorMenu;
               else if (iOver >= 1000)
                  hCursor = m_pMain->m_hCursorMenu;
               else if (iOver >= 0)
                  hCursor = m_pMain->m_hCursorHand;

               m_iTaskBarHighlight = iOver;
            }
         }
         SetCursor (hCursor);

         // potentially update displays
         if (m_fOnTop) {
            if (m_iTabHighlight != iOldTab)
               InvalidateRect (m_hWnd, NULL, FALSE);
         }
         else {
            if (m_iTaskBarHighlight != iOldTaskBar)
               InvalidateRect (m_hWnd, NULL, FALSE);
         }

      }
      break;

   case WM_SIZE:
      {
         // all the pie charts
         DWORD i;
         PCPieChart *ppc = (PCPieChart*)m_lPCPieChart.Get(0);
         for (i = 0; i < m_lPCPieChart.Num(); i++)
            if (ppc[i]) {
               RECT r;
               PieChartLoc (i, &r);
               ppc[i]->Move (&r);
            }

         // command window
         if (m_hWndCommand) {
            RECT r;
            CommandLoc (&r);
            MoveWindow (m_hWndCommand, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
         }

         // toolbar
         ToolbarArrange ();

      }
      break;

   } // switch uMsg

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*************************************************************************************
CSlidingWindow::Resize - Called when the main window is resized. This causes
the sliding window to resize.

*/
void CSlidingWindow::Resize (void)
{
   if (!m_hWnd)
      return;

   RECT r;
   DefaultCoords (m_fLocked ? 2 : 0, &r);
   MoveWindow (m_hWnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
}


/*************************************************************************************
CSlidingWindow::Locked - Tells the sliding window that is has been locked or
unlocked. This will cause it to slide (or hide) itself
*/
void CSlidingWindow::Locked (BOOL fLocked)
{
   m_fLocked = fLocked;

   SlideTimer (FALSE);
}


/*************************************************************************************
CSlidingWindow::SlideTimer - Causes the window to slide in/out when the slide timer
is called. If it has reached full position then stops sliding.

inputs
   BOOL        fForTimer - Called because of a timer
*/
void CSlidingWindow::SlideTimer (BOOL fForTimer)
{
   // make sure this window is on top
   if (GetNextWindow (m_hWnd, GW_HWNDPREV))
      SetWindowPos (m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

   // do we want the window in locked posiiton
   BOOL fWantFullLocked = m_fLocked;
   BOOL fWantHalfLocked = FALSE;

   // force open if timer
   if (m_iStayOpenTime > 0)
      fWantHalfLocked = TRUE;

   BOOL fWantLockedOrig = fWantFullLocked | fWantHalfLocked;
   if (!fWantFullLocked) {
      // might still want it locked because cursor might be over it

      // find out where the cursor is
      POINT p;
      RECT r;
      GetCursorPos (&p);
      ScreenToClient (m_hWnd, &p);
      GetClientRect (m_hWnd, &r);

      if (PtInRect (&r, p))
         fWantFullLocked = TRUE;
   }

   // if loading TTS then want it locked
   if (m_pMain->m_fTTSLoadProgress && m_fOnTop)
      fWantHalfLocked = TRUE;

   // get current location, and what want it to be
   RECT rCur, rWant;
   GetWindowRect (m_hWnd, &rCur);
   ScreenToClient (m_hWndParent, (POINT*) &rCur.left);
   ScreenToClient (m_hWndParent, (POINT*) &rCur.right);
   DefaultCoords (fWantFullLocked ? 2 : (fWantHalfLocked ? 1 : 0), &rWant);

   // if it's in the right location then done
   if (rCur.top == rWant.top) {
      // NOTE: only kill the timer if we're in the final state we're supposed to be in
      if ((m_fLocked == (fWantHalfLocked || fWantFullLocked)) && (m_iStayOpenTime <= 0) && m_fSlideTimer) {
         KillTimer (m_hWnd, SLIDETIMERID);
         m_fSlideTimer = FALSE;
      }

      if (fForTimer && (m_iStayOpenTime > 0))
         m_iStayOpenTime -= SLIDETIMERMILLISEC;

      // if don't want it locked originally, and there's no timer, then create
      if ((!fWantLockedOrig || m_iStayOpenTime) && !m_fSlideTimer) { // BUGFIX - If m_iStayOpenTime is set then must have timer
         SetTimer (m_hWnd, SLIDETIMERID, SLIDETIMERMILLISEC, NULL);
         m_fSlideTimer = TRUE;
      }
      return;
   }

   // if there's no timer on then set one
   if (!m_fSlideTimer) {
      SetTimer (m_hWnd, SLIDETIMERID, SLIDETIMERMILLISEC, NULL);
      m_fSlideTimer = TRUE;
   }

   // if not called from a timer then don't bother going further
   if (!fForTimer)
      return;

   // reduce tick count
   if (m_iStayOpenTime > 0)
      m_iStayOpenTime -= SLIDETIMERMILLISEC;

   // else, need to move
   int iMax = (rWant.bottom - rWant.top - PIXELSSHOWING) * SLIDETIMERMILLISEC / SLIDETIMERTOTAL;
   iMax = max(iMax, 1);

   int iDelta = rCur.top - rWant.top;
   iMax = min(iMax, abs(iDelta));   // cant move more than want
   if (iDelta < 0)
      iDelta += iMax;
   else
      iDelta -= iMax;
   rWant.top += iDelta;
   rWant.bottom += iDelta;

   MoveWindow (m_hWnd, rWant.left, rWant.top, rWant.right - rWant.left, rWant.bottom - rWant.top, TRUE);
}



/*************************************************************************************
CSlidingWindow::TabDraw - Draws an individual tab onto the HDC.

inputs
   HDC         hDC - hDC to draw to
   PWSTR       pszText - Text to draw
   RECT        *prTab - Tabs bounding rectangle
   BOOL        fOnTop - Set to TRUE is this tab is on top
   BOOL        fHighlight - If TRUE then draw in the highlight color
returns
   none
*/
void CSlidingWindow::TabDraw (HDC hDC, PWSTR pszText, RECT *prTab, BOOL fOnTop, BOOL fHighlight)
{
   // figure out the points for the rounded tab...
   POINT ap[4];
   DWORD dwNumPoint = sizeof(ap) / sizeof(POINT);
   ap[0].x = prTab->left;
   ap[0].y = prTab->bottom;
   ap[1].x = prTab->left + TABSPACING/4;
   ap[1].y = prTab->top;
   ap[2].x = prTab->right - TABSPACING;
   ap[2].y = prTab->top;
   ap[3].x = prTab->right;
   ap[3].y = prTab->bottom;

   // create the brush for the background and fill
   HBRUSH hbrBack = CreateSolidBrush (fOnTop ? m_pMain->m_crJPEGBackTop /*m_cBackground*/ : m_pMain->m_cVerbDim);
   HBRUSH hbrOld = (HBRUSH)SelectObject (hDC, hbrBack);
   HPEN hPen = CreatePen (PS_SOLID, 0, fOnTop ? m_pMain->m_crJPEGBackTop /*m_cBackground*/ : m_pMain->m_cVerbDim);
   HPEN hPenOld = (HPEN)SelectObject (hDC, hPen);
   Polygon (hDC, ap, dwNumPoint);
   SelectObject (hDC, hbrOld);
   DeleteObject (hbrBack);
   SelectObject (hDC, hPenOld);
   DeleteObject (hPen);
   
   // draw line around polygon
   hPen = CreatePen (PS_SOLID, 0, m_pMain->m_cTextDimNoChange);
   hPenOld = (HPEN)SelectObject (hDC, hPen);
   if (fOnTop) {
      MoveToEx (hDC, 0, prTab->bottom, NULL);
      LineTo (hDC, ap[0].x, ap[0].y);
   }
   else
      MoveToEx (hDC, ap[0].x, ap[0].y, NULL);

   // draw lines around
   DWORD i;
   for (i = 1; i < dwNumPoint; i++)
      LineTo (hDC, ap[i].x, ap[i].y);

   if (fOnTop) // draw line to end of screen
      LineTo (hDC, 10000, prTab->bottom);

   SelectObject (hDC, hPenOld);
   DeleteObject (hPen);

   // draw the text
   HFONT hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_hFont);
   int iOldMode = SetBkMode (hDC, TRANSPARENT);
   SetTextColor (hDC, fHighlight ? m_pMain->m_cTextHighlight : (fOnTop ? m_pMain->m_cText : m_pMain->m_cTextDim));
   RECT rDraw = *prTab;
   rDraw.left += TABSPACING;
   DrawTextW (hDC, pszText, -1, &rDraw, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
   SelectObject (hDC, hFontOld);
   SetBkMode (hDC, iOldMode);
}


/*************************************************************************************
CSlidingWindow::TabLoc - Given a tab index, fills in the rectangle for the tab location.

inputs
   DWORD       dwIndex - Index for the tab, 0-based
   RECT        *prTab - Filled with the tabs location
*/
void CSlidingWindow::TabLoc (DWORD dwIndex, RECT *prTab)
{
   GetClientRect (m_hWnd, prTab);
   prTab->left = TABWIDTH * dwIndex + TABSPACING;
   prTab->right = prTab->left + TABWIDTH + TABSPACING;
   prTab->top = prTab->bottom - TABSPACING - 1;
   prTab->bottom -= 1;
}




/*************************************************************************************
CSlidingWindow::TabPTTLoc - Returns the location of psh to talk

inputs
   RECT        *prMenu - Filled with the menu location
*/
#define PTTWAVEWIDTH    100
void CSlidingWindow::TabPTTLoc (RECT *prMenu)
{
   GetClientRect (m_hWnd, prMenu);
   prMenu->top = prMenu->bottom - TABSPACING - 1;
   prMenu->bottom -= 1;
   prMenu->left = prMenu->right - TABSPACING*1 - PTTWAVEWIDTH;
   prMenu->right = prMenu->left + PTTWAVEWIDTH;
}

/*************************************************************************************
CSlidingWindow::TabProgressLoc - Returns the location of progress

inputs
   RECT        *prMenu - Filled with the menu location
*/
void CSlidingWindow::TabProgressLoc (RECT *prMenu)
{
   TabPTTLoc (prMenu);

   prMenu->right = prMenu->left - TABSPACING;
   prMenu->left = prMenu->right - PTTWAVEWIDTH;
}

/*************************************************************************************
CSlidingWindow::TabsDraw - Draws all the tabs on the HDC. This also draws the menu item.

inputs
   HDC         hDC - hDC to draw to
   RECT        *prHDC - Rectangle for the HDC
   int         iHighlight - Higlight tab, 0+
returns
   none
*/
void CSlidingWindow::TabsDraw (HDC hDC, RECT *prHDC, int iHighlight)
{
   // fill in the background behind the tabs
   RECT rTab;
   HBRUSH hbr;
   rTab = *prHDC;
   //rTab.bottom = TABSPACING;
   hbr = CreateSolidBrush (m_pMain->m_cBackgroundNoTabNoChange);
   FillRect (hDC, &rTab, hbr);
   DeleteObject (hbr);
   //rTab = *prHDC;
   //rTab.top = TABSPACING;
   //if (rTab.top < rTab.bottom)
   //   FillRect (hDC, &rTab, m_pMain->m_hbrBackground);

   // draw the edit text
   if (CommandIsVisible()) {
      CommandLabelLoc (&rTab);
      HFONT hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_hFont);
      int iOldMode = SetBkMode (hDC, TRANSPARENT);
      SetTextColor (hDC, m_pMain->m_cTextDim);
      DrawTextW (hDC, L"Command (F6):", -1, &rTab, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
      SelectObject (hDC, hFontOld);
      SetBkMode (hDC, iOldMode);
   }

   // fill in all the background tabs
   DWORD i;
   for (i = NUMTABS-1; i < NUMTABS; i--) {
      if (i == m_dwTab)
         continue;   // dont do this now

      // draw
      TabLoc (i, &rTab);   // assume 0,0 is UL of display
      TabDraw (hDC, gapszTabName[i], &rTab, FALSE, iHighlight == (int)i);
   } // i

   // draw the tab on top
   TabLoc (m_dwTab, &rTab);   // assume 0,0 is UL of display
   TabDraw (hDC, gapszTabName[m_dwTab], &rTab, TRUE, FALSE);   // never highlight tab on top

   // need to draw PTT
   RECT rDraw;
   if (m_pMain->m_VoiceDisguise.m_fAgreeNoOffensive && m_pMain->m_VoiceChatInfo.m_fAllowVoiceChat && RegisterMode()) {
      TabPTTLoc (&rDraw);

      // if not recording then text
      if (!m_pMain->m_fVCRecording) {
         HFONT hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_hFont);
         int iOldMode = SetBkMode (hDC, TRANSPARENT);
         SetTextColor (hDC, m_pMain->m_cTextDimNoChange);
         DrawTextW (hDC, L"Push \"Ctrl\" to talk", -1, &rDraw, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
         SelectObject (hDC, hFontOld);
         SetBkMode (hDC, iOldMode);
      }
      else {
         // draw the wave
         //HBITMAP hBit = CreateCompatibleBitmap (hDC, rDraw.right-rDraw.left, rDraw.bottom-rDraw.top);
         //HDC hDCBit = CreateCompatibleDC (hDC);
         //SelectObject (hDCBit, hBit);

         EnterCriticalSection (&m_pMain->m_csVCStopped);
         VoiceChatScope (hDC /*hDCBit*/, &rDraw);
         LeaveCriticalSection (&m_pMain->m_csVCStopped);

         //DeleteDC (hDCBit);
         //DeleteObject (hBit);
      }
   } // voice chat

   // draw progress
   TabProgressLoc (&rDraw);
   ProgressDraw (hDC, &rDraw);
}

/*************************************************************************************
CSlidingWindow::ProgressDraw - Draws the progress bar.

inputs
   HDC         hDC - DC
   RECT        *pr - Draw into
returns
   none
*/
void CSlidingWindow::ProgressDraw (HDC hDC, RECT *pr)
{
   WCHAR szTextDownload[128], szTextDraw[128];
   szTextDownload[0] = szTextDraw[0] = 0;

   // if it isn't visible then dont draw
   //if (!m_fProgressVisible)
   //   return;

   // draw the text for progress of download
   if (m_pMain->m_MPI.iReceiveBytesExpect != m_pMain->m_MPI.iReceiveBytes) {
      // figure out progress bar info
      __int64 iCur, iMax;
      double fProgress;
      iCur = m_pMain->m_MPI.iReceiveBytes - m_pMain->m_iBytesShow;
      iMax = m_pMain->m_MPI.iReceiveBytesExpect - m_pMain->m_iBytesShow;
      iMax = max(iMax, 1);
      fProgress = (double)iCur / (double)iMax;
      fProgress = max(fProgress, 0);
      fProgress = min(fProgress, 1);

      HBRUSH hbr = CreateSolidBrush (RGB(0,0x40,0xa0));
      RECT r;
      r = *pr;
      r.top = (r.top + r.bottom)/2+1; // mid point
      r.bottom--;
      r.left++;
      r.right--;
      FrameRect (hDC, &r, hbr);  // so have outline

      // draw progress
      r.right = (int)((double)(r.right - r.left) * fProgress) + r.left;
      FillRect (hDC, &r, hbr);
      DeleteObject (hbr);

      // text to draw...
      PWSTR pszLoading = m_pMain->m_fConnectRemote ? L"Downloading..." : L"Loading...";
      fProgress = (fp)(m_pMain->m_MPI.iReceiveBytesExpect - m_pMain->m_MPI.iReceiveBytes) /
         (fp)max(1, m_pMain->m_MPI.dwBytesPerSec);
      if (fProgress < 60)
         swprintf (szTextDownload, L"%s %d sec", pszLoading, (int)fProgress);
      else if (fProgress < 60 * 60)
         swprintf (szTextDownload, L"%s %.1f min", pszLoading, (double)fProgress / 60.0);
      else
         swprintf (szTextDownload, L"%s %.1f hr", pszLoading, (double)fProgress / 60.0 / 60.0);

   }

   // draw render progress
   if (m_pMain->m_fRenderProgress || m_pMain->m_fTTSLoadProgress) {
      HBRUSH hbr = CreateSolidBrush (RGB(0x80,0,0xc0));
      RECT r;
      r = *pr;
      r.bottom = (r.top + r.bottom)/2-1; // mid point
      r.top++;
      r.left++;
      r.right--;

      FrameRect (hDC, &r, hbr);  // so have outline

      double fProgress = m_pMain->m_fTTSLoadProgress ? m_pMain->m_fTTSLoadProgress : m_pMain->m_fRenderProgress;
      r.right = (int)((double)(r.right - r.left) * fProgress) + r.left;
      FillRect (hDC, &r, hbr);
      DeleteObject (hbr);

      wcscpy (szTextDraw, m_pMain->m_fTTSLoadProgress ? L"Loading..." : L"Drawing...");

   }


   PWSTR pszDraw = NULL;
   if (szTextDraw[0] && szTextDownload[0])
      pszDraw = ((GetTickCount() / 2048) % 2) ? szTextDraw : szTextDownload;
   else if (szTextDraw[0])
      pszDraw = szTextDraw;
   else if (szTextDownload[0])
      pszDraw = szTextDownload;

   if (pszDraw) {
      HFONT hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_hFont);
      int iOldMode = SetBkMode (hDC, TRANSPARENT);
      SetTextColor (hDC, m_pMain->m_cTextDimNoChange);
      RECT rDraw = *pr;
      DrawTextW (hDC, pszDraw, -1, &rDraw, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
      SelectObject (hDC, hFontOld);
      SetBkMode (hDC, iOldMode);
   }
}

/*************************************************************************************
CSlidingWindow::TabOverItem - Test to see if the tab is over an item.

inputs
   POINT       p - Cursor location (in client coords)
returns
   int - Tab number, 0..3 (etc.), -1 if not over anything,
      NUMTABS+1 if over PTT display,
      -2 if over circumreality button
*/
int CSlidingWindow::TabOverItem (POINT p)
{
   //if (p.y >= TABHEIGHT)
   //   return -1;  // definitely not

   // else, might be
   RECT r;
   DWORD i;
   for (i = 0; i < NUMTABS; i++) {
      TabLoc (i, &r);
      if (PtInRect (&r, p))
         return (int)i;
   } // i

   // see if over menu
   TaskBarMenuLoc (&r);
   if (PtInRect (&r, p))
      return -2;

   // see if over menu
   //TabMenuLoc (&r);
   //if (PtInRect (&r, p))
   //   return NUMTABS;

   // see if over the PTT
   //TabPTTLoc (&r);
   //if (PtInRect (&r, p))
   //   return NUMTABS+1;

   // else, none
   return -1;
}




#if 0 // dead code
/*************************************************************************************
CSlidingWindow::TabLocation - Fills in the location of the tab.

inputs
   RECT        *pr - Filled with the location
returns
   non
*/
void CSlidingWindow::TabLocation (RECT *pr)
{
   GetClientRect (m_hWnd, pr);
   pr->top = pr->bottom - TABHEIGHT;
}
#endif // 0



/*************************************************************************************
CSlidingWindow::VoiceChatScope - draws a scope for voice chat.

NOTE: This assumes that in the critical section to protect the wave!

inputs
   HDC         hDC - DC
   RECT        *pr - Location to draw to
returns
   none
*/
void CSlidingWindow::VoiceChatScope (HDC hDC, RECT *pr)
{
#define SAMPLERSPERPIXEL      4     // number of samples drawn per pixel

   int iWidth = pr->right - pr->left;
   int iHeight = pr->bottom - pr->top;

   // try to look 3 window lengths back
   int iLook = (int)m_pMain->m_VCWave.m_dwSamples - iWidth * SAMPLERSPERPIXEL * 3;
   iLook = max(iLook, 0);

   // find the biggest peak after the biggest zero crossing
   DWORD i;
   DWORD dwLastZC = (DWORD)-1;
   DWORD dwPeak = (DWORD)-1, dwPeakZC = (DWORD)-1;
   BOOL fClipped = FALSE;
   int iMax = (int)m_pMain->m_VCWave.m_dwSamples - iWidth * SAMPLERSPERPIXEL;
   for (i = (DWORD)iLook; (int) i < iMax; i++) {
      short sVal = m_pMain->m_VCWave.m_psWave[i*m_pMain->m_VCWave.m_dwChannels];

      // see if zero crossing
      if (i && (m_pMain->m_VCWave.m_psWave[(i-1)*m_pMain->m_VCWave.m_dwChannels] <= 0) && (sVal >= 0))
         dwLastZC = i;

      // see if clipped
      if ((sVal > 32000) || (sVal < -32000))
         fClipped = TRUE;

      // see if have peak
      if ((dwLastZC != (DWORD)-1) && ((dwPeak == (DWORD)-1) || (sVal > m_pMain->m_VCWave.m_psWave[dwPeak*m_pMain->m_VCWave.m_dwChannels])) ) {
         dwPeak = i;
         dwPeakZC = dwLastZC;
      }
   } // i
   
   // if there's no peak then start from iLook, else, from peak
   if (dwPeakZC != (DWORD)-1)
      iLook = (int)dwPeakZC;


   // background
   if (fClipped) {
      HBRUSH hbr = CreateSolidBrush (RGB(0x80,0,0));
      FillRect (hDC, pr, hbr);
      DeleteObject (hbr);
   }
   //else
   //   FillRect (hDC, pr, (HBRUSH) GetStockObject (BLACK_BRUSH));

   // draw the wave
   HPEN hPen, hPenOld;
   short sVal;
   hPen = CreatePen (PS_SOLID, 0, RGB(0xff, 0, 0));
   hPenOld = (HPEN) SelectObject (hDC, hPen);

   for (i = 0; i < (DWORD)iWidth; i++, iLook += SAMPLERSPERPIXEL) {
      if (iLook < (int)m_pMain->m_VCWave.m_dwSamples)
         sVal = m_pMain->m_VCWave.m_psWave[(DWORD)iLook * m_pMain->m_VCWave.m_dwChannels];
      else
         sVal = 0;

      int iY = (((int) sVal + 32768 ) * iHeight + 32768/iHeight) / 0x10000 + pr->top;
      if (i)
         LineTo (hDC, (int)i + pr->left, iY);
      else
         MoveToEx (hDC, (int)i + pr->left, iY, NULL);
   } // i
   SelectObject (hDC, hPenOld);
   DeleteObject (hPen);

   // draw too loud
   if (fClipped) {
      HFONT hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_hFont);
      int iOldMode = SetBkMode (hDC, TRANSPARENT);
      SetTextColor (hDC, RGB(0xff,0xff,0xff));
      DrawTextW (hDC, L"Too loud!", -1, pr, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
      SelectObject (hDC, hFontOld);
      SetBkMode (hDC, iOldMode);
   }
}


/*************************************************************************************
CSlidingWindow::PieChartLoc - Returns the location of the pie chart.

inputs
   DWORD       dwNum - Incremental pie chart number
   RECT        *pr - Filled in
returns
   none
*/
void CSlidingWindow::PieChartLoc (DWORD dwNum, RECT *pr)
{
   TabProgressLoc (pr);
   int iHeight = pr->bottom - pr->top;
   pr->right = pr->left - iHeight * ((int)dwNum+1);
   pr->left = pr->right - iHeight;
}


/*************************************************************************************
CSlidingWindow::InvalidateProgress - Invalidate the progress display
*/
void CSlidingWindow::InvalidateProgress (void)
{
   RECT r;
   TabProgressLoc (&r);
   InvalidateRect (m_hWnd, &r, FALSE);
}


/*************************************************************************************
CSlidingWindow::InvalidatePTT - Invalidate the PTT display
*/
void CSlidingWindow::InvalidatePTT (void)
{
   RECT r;
   TabPTTLoc (&r);
   InvalidateRect (m_hWnd, &r, FALSE);
}


/*************************************************************************************
CSlidingWindow::Invalidate - Invalidate the entire display
*/
void CSlidingWindow::Invalidate (void)
{
   InvalidateRect (m_hWnd, NULL, FALSE);
}



/*************************************************************************************
CSlidingWindow::PieChartAddSet - Adds or sets a pie chart

inputs
   DWORD          dwID - Number
   PWSTR          psz - Name to use. If NULL then delete.
   fp             fValue - Value
   fp             fDelta - Delta change
   COLORREF       cr - Color
returns
   BOOL - TRUE if success
*/
BOOL CSlidingWindow::PieChartAddSet (DWORD dwID, PWSTR psz, fp fValue, fp fDelta, COLORREF cr)
{
   // find this
   PCPieChart pc = NULL;
   while (m_lPCPieChart.Num() <= dwID)
      m_lPCPieChart.Add (&pc);
   PCPieChart *ppc = (PCPieChart*) m_lPCPieChart.Get(0);

   if (!psz) {
      // delete
      if (ppc[dwID])
         delete ppc[dwID];
      ppc[dwID] = NULL;
      RECT r;
      PieChartLoc (dwID, &r);
      InvalidateRect (m_hWnd, &r, FALSE);
   }
   else {
      if (!ppc[dwID]) {
         RECT r;
         PieChartLoc (dwID, &r);
         ppc[dwID] = new CPieChart;
         ppc[dwID]->Init (ghInstance, psz, &r, m_hWnd, dwID);
      }
      ppc[dwID]->ColorSet (m_pMain->m_cBackgroundNoTabNoChange, cr);
      ppc[dwID]->PieSet (fValue, fDelta);
      // NOTE: Not changing the name after the initial go
   }

   return TRUE;
}


/*************************************************************************************
CSlidingWindow::TaskBarLocation - Fills in the location of the taskbar.

inputs
   RECT        *pr - Filled with the location
returns
   non
*/
void CSlidingWindow::TaskBarLocation (RECT *pr)
{
   GetClientRect (m_hWnd, pr);
   pr->bottom = pr->top + TASKBARHEIGHT;
}


/*************************************************************************************
CSlidingWindow::CircumRealityButtonDraw - Draws the circumreality button

inputs
   HDC         hDC - hDC to draw to
   RECT        *prHDC - Rectangle for the HDC
   RECT        *prInWindow - Rectangle as is appears in the main window
   int         iHighlight - Item to highlight. -2 for Circumreality button
returns
   none
*/
void CSlidingWindow::CircumRealityButtonDraw (HDC hDC, RECT *prHDC, RECT *prInWindow, int iHighlight)
{

   // for the mini-menu icon
   HICON hIconMenu = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_MINIMENUICON));

   // draw some graphics to indicate it's the "start" button
   // NOTE: Making it a bit plain
   RECT rTab;
   TaskBarMenuLoc (&rTab);
   int iHeight = rTab.bottom - rTab.top; // prHDC->bottom - prHDC->top - 1;
   DWORD i;
   HPEN hPen = CreatePen (PS_SOLID, 0, m_pMain->m_cBackgroundMenuNoChange);
   HPEN hPenHalf = CreatePen (PS_SOLID, 0,
      RGB(
         GetRValue(m_pMain->m_cBackgroundMenuNoChange)/2 + GetRValue(m_pMain->m_cBackgroundNoTabNoChange)/2,
         GetGValue(m_pMain->m_cBackgroundMenuNoChange)/2 + GetGValue(m_pMain->m_cBackgroundNoTabNoChange)/2,
         GetBValue(m_pMain->m_cBackgroundMenuNoChange)/2 + GetBValue(m_pMain->m_cBackgroundNoTabNoChange)/2
         )
      );
   HPEN hPenOld = (HPEN)SelectObject (hDC, hPen);
   if (!m_fOnTop) for (i = 0; (int)i < iHeight; i++) {
      int iY = (int)i + prHDC->top + 1;
      int iFromCenter = (int)i - iHeight/2;
      int iRight = rTab.right - (iHeight/2 - (int) sqrt((fp)(iHeight * iHeight - iFromCenter * iFromCenter))) - 1;

      SelectObject (hDC, hPen);
      MoveToEx (hDC, 0, iY, NULL);
      LineTo (hDC, iRight, iY);
      SelectObject (hDC, hPenHalf);
      LineTo (hDC, iRight+1, iY);   // antialias
   } // i
   SelectObject (hDC, hPenOld);
   DeleteObject (hPen);
   DeleteObject (hPenHalf);

   // will need to draw menu item icon
   DrawIcon (hDC,
      rTab.left - prInWindow->left, 
      rTab.top - prInWindow->top + (rTab.bottom - rTab.top) / 2 - MINIICONSIZE/2,
      hIconMenu);
   rTab.left += MINIICONSIZE + MINIICONSIZE/4;

   // draw the Circumreality
   HFONT hFontOld = (HFONT) SelectObject (hDC, m_hFontSymbol);
   int iOldMode = SetBkMode (hDC, TRANSPARENT);
   SetTextColor (hDC, (iHighlight == -2) ? m_pMain->m_cTextHighlightNoChange : m_pMain->m_cTextNoChange);
   RECT rDraw = rTab;
   //RECT rDrawn;
   //rDrawn = rDraw;
   //DrawTextW (hDC, L"m", -1, &rDrawn, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_CALCRECT);
   //rDraw.left -= prInWindow->left;
   //rDraw.right -= prInWindow->left;
   //rDraw.top -= prInWindow->top;
   //rDraw.bottom -= prInWindow->top;
   //DrawTextW (hDC, L"m", -1, &rDraw, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
   SelectObject (hDC, m_hFontBig);
   rDraw = rTab;
   //rDraw.left = rDrawn.right + 2;
   rDraw.left -= prInWindow->left;
   rDraw.right -= prInWindow->left;
   rDraw.top -= prInWindow->top;
   rDraw.bottom -= prInWindow->top;
   DrawTextW (hDC, L"CircumReality", -1, &rDraw, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
   SelectObject (hDC, hFontOld);
   SetBkMode (hDC, iOldMode);
}

/*************************************************************************************
CSlidingWindow::TaskBarDraw - Draws all the tabs on the HDC. This also draws the menu item.

inputs
   HDC         hDC - hDC to draw to
   RECT        *prHDC - Rectangle for the HDC
   RECT        *prInWindow - Rectangle as is appears in the main window
   int         iHighlight - Item to highlight. 0+ (or 1000+) for window. -2 for Circumreality button
returns
   none
*/
void CSlidingWindow::TaskBarDraw (HDC hDC, RECT *prHDC, RECT *prInWindow, int iHighlight)
{
   // if more than 1000 then on specific menu, but highlight text all the same
   if (iHighlight >= 1000)
      iHighlight -= 1000;

   // fill in the background behind the tabs
   HBRUSH hbr;
   RECT rTab;
   rTab = *prHDC;
   rTab.top++;
   hbr = CreateSolidBrush (m_pMain->m_cBackgroundNoTabNoChange);
   FillRect (hDC, &rTab, hbr);
   DeleteObject (hbr);

   // line
   HPEN hPen = CreatePen (PS_SOLID, 0, m_pMain->m_cTextDimNoChange);
   HPEN hPenOld = (HPEN)SelectObject (hDC, hPen);
   MoveToEx (hDC, prHDC->left, prHDC->top, NULL);
   LineTo (hDC, prHDC->right, prHDC->top);
   SelectObject (hDC, hPenOld);
   DeleteObject (hPen);

   CircumRealityButtonDraw (hDC, prHDC, prInWindow, iHighlight);



   // draw all the windows
   PTASKBARWINDOW ptbw = (PTASKBARWINDOW) m_lTASKBARWINDOW.Get(0);
   HFONT hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_hFont);
   int iOldMode = SetBkMode (hDC, TRANSPARENT);
   WCHAR szTemp[512];
   DWORD i;
   for (i = 0; i < m_lTASKBARWINDOW.Num(); i++,ptbw++) {
      // draw the mini icon
      //if (ptbw->rLocMiniIcon.right > ptbw->rLocMiniIcon.left)
      //   DrawIcon (hDC, ptbw->rLocMiniIcon.left - prInWindow->left, ptbw->rLocMiniIcon.top - prInWindow->top, hIconMenu);

      // draw the text
      if (ptbw->rLocText.right <= ptbw->rLocText.left)
         continue;   // too small

      // get the text
      if (!ptbw->pszName) {
         wcscpy (szTemp,  L"Unknown");
         if (IsWindow(ptbw->hWnd))
            GetWindowTextW (ptbw->hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR));
      }

      // draw text
      SetTextColor (hDC, (iHighlight == (int)i) ? m_pMain->m_cTextHighlightNoChange : (ptbw->fIsVisible ? m_pMain->m_cTextNoChange : m_pMain->m_cTextDimNoChange));
      RECT rDraw = ptbw->rLocText;
      rDraw.left -= prInWindow->left;
      rDraw.right -= prInWindow->left;
      rDraw.top -= prInWindow->top;
      rDraw.bottom -= prInWindow->top;
      DrawTextW (hDC, ptbw->pszName ? ptbw->pszName : szTemp, -1, &rDraw,
         DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
   } // i
   SelectObject (hDC, hFontOld);
   SetBkMode (hDC, iOldMode);
}

/*************************************************************************************
CSlidingWindow::TaskBarMenuLoc - Returns the location for the tab's menu icon location.

inputs
   RECT        *prMenu - Filled with the menu location
*/
void CSlidingWindow::TaskBarMenuLoc (RECT *prMenu)
{
   if (!m_fOnTop) {
      TaskBarLocation (prMenu);
      prMenu->left += TABSPACING;
   }
   else {
      prMenu->left = 0;
      prMenu->top = 0;
      prMenu->bottom = TASKBARHEIGHT;
   }
   prMenu->right = prMenu->left + MINIICONSIZE + MINIICONSIZE/4 + 135; // large enough for mini icon and Circumreality
   prMenu->top++; // so no top line
}

/*************************************************************************************
CSlidingWindow::TaskBarOverItem - Test to see if the tab is over an item.

inputs
   POINT       p - Cursor location (in client coords)
   BOOL        fToShowHode - If clicking on the dispaly might cause to show/hide
returns
   int - Index into window number (0+, 1000+ indicates in mini icon), -1 if not over anything,
      -2 if over menu.
*/
int CSlidingWindow::TaskBarOverItem (POINT p, BOOL fToShowHide)
{
   RECT rTaskBar;
   TaskBarLocation (&rTaskBar);
   if (!PtInRect (&rTaskBar, p))
      return -1;  // definitely not

   // see if over menu
   RECT r;
   TaskBarMenuLoc (&r);
   if (PtInRect (&r, p))
      return -2;

   // check for window buttons
   DWORD i;
   PTASKBARWINDOW ptbw = (PTASKBARWINDOW) m_lTASKBARWINDOW.Get(0);
   for (i = 0; i < m_lTASKBARWINDOW.Num(); i++, ptbw++) {
      if (!PtInRect (&ptbw->rLocText, p))
         continue;   // not in rectangle

      if (ptbw->fIsVisible)
         return (int)i + 1000;   // to indicate that pull up menu
      //if (PtInRect (&ptbw->rLocMiniIcon, p))
      //   return (int)i + 1000;   // to indicate mini icon

      // if showing/hiding, but cant, then skip
      if (fToShowHide && ptbw->fCantShowHide)
         continue;

      //if (PtInRect (&ptbw->rLocText, p))
      return (int)i;
   } // i

   // else, none
   return -1;
}


static int _cdecl TASKBARWINDOWSort (const void *elem1, const void *elem2)
{
   TASKBARWINDOW *pdw1, *pdw2;
   pdw1 = (TASKBARWINDOW*) elem1;
   pdw2 = (TASKBARWINDOW*) elem2;

   return _wcsicmp(pdw1->pszName, pdw2->pszName);
}


/*************************************************************************************
CSlidingWindow::TaskBarSync - Fills in the taskbar structures (and redraws) according
to what windows are visible.
*/
void CSlidingWindow::TaskBarSync (void)
{
   RECT rTaskBar;
   TaskBarLocation (&rTaskBar);
   rTaskBar.top++;   // to exclude top line

   // figure out where the menu is, and make sure not to draw on it
   RECT rMenu;
   TaskBarMenuLoc (&rMenu);
   rTaskBar.left = rMenu.right + TABSPACING;
   // done do since spacing later on rTaskBar.right -= TABSPACING;
   rTaskBar.right = max(rTaskBar.right, rTaskBar.left+1);

   // figure out what does on taskbar
   CListFixed lHidden, lOld;
   lHidden.Init (sizeof(TASKBARWINDOW));
   lOld.Init (sizeof(TASKBARWINDOW), m_lTASKBARWINDOW.Get(0), m_lTASKBARWINDOW.Num());
   m_lTASKBARWINDOW.Clear();
   TASKBARWINDOW tbw;

   // add the icon windows
   BOOL fHaveItem = FALSE;
   PCDisplayWindow *ppd = (PCDisplayWindow*)m_pMain->m_lPCDisplayWindow.Get(0);
   DWORD i;
   for (i = 0; i < m_pMain->m_lPCDisplayWindow.Num(); i++) {
      memset (&tbw, 0, sizeof(tbw));
      tbw.hWnd = ppd[i]->m_hWnd;
      tbw.dwType = 0;
      tbw.fIsVisible = !m_pMain->IsWindowObscured(ppd[i]->m_hWnd);
      tbw.pdw = ppd[i];
      tbw.pszName = (PWSTR) ppd[i]->m_memName.p;
      
      if (tbw.fIsVisible)
         m_lTASKBARWINDOW.Add (&tbw);
      else
         lHidden.Add (&tbw);
   }  // over i


   PCIconWindow *ppi = (PCIconWindow*)m_pMain->m_lPCIconWindow.Get(0);
   for (i = 0; i < m_pMain->m_lPCIconWindow.Num(); i++) {
      memset (&tbw, 0, sizeof(tbw));
      tbw.hWnd = ppi[i]->m_hWnd;
      tbw.dwType = 1;
      tbw.fIsVisible = !m_pMain->IsWindowObscured(ppi[i]->m_hWnd);
      tbw.piw = ppi[i];
      tbw.pszName = (PWSTR) ppi[i]->m_memName.p;
      
      if (tbw.fIsVisible)
         m_lTASKBARWINDOW.Add (&tbw);
      else
         lHidden.Add (&tbw);
   }  // over i

   // map
   if (m_pMain->m_pMapWindow && !m_pMain->m_pMapWindow->m_fHiddenByServer) {
      memset (&tbw, 0, sizeof(tbw));
      tbw.hWnd = m_pMain->m_pMapWindow ? m_pMain->m_pMapWindow->m_hWnd : NULL;
      tbw.dwType = 11;
      tbw.fIsVisible = !m_pMain->IsWindowObscured (m_pMain->m_pMapWindow->m_hWnd) && !m_pMain->m_pMapWindow->m_fHiddenByUser;
      tbw.pszName = L"Map";
      if (tbw.fIsVisible)
         m_lTASKBARWINDOW.Add (&tbw);
      else
         lHidden.Add (&tbw);
   }

   // transcript
   memset (&tbw, 0, sizeof(tbw));
   tbw.hWnd = m_pMain->m_hWndTranscript;
   tbw.dwType = 12;
   tbw.fIsVisible = !m_pMain->IsWindowObscured (m_pMain->m_hWndTranscript);
   tbw.pszName = L"Transcript";  // BUGBUG - different name for transcript?
#ifdef UNIFIEDTRANSCRIPT
   tbw.fCantShowHide = TRUE;
#endif
   if (tbw.fIsVisible)
      m_lTASKBARWINDOW.Add (&tbw);
   else
      lHidden.Add (&tbw);

#ifndef UNIFIEDTRANSCRIPT
   if (m_pMain->m_hWndMenu && IsWindowVisible(m_pMain->m_hWndMenu)) {
      // command line
      memset (&tbw, 0, sizeof(tbw));
      tbw.hWnd = m_pMain->m_hWndMenu;
      tbw.dwType = 14;
      tbw.fIsVisible = !m_pMain->IsWindowObscured (m_pMain->m_hWndMenu);
      tbw.fCantShowHide = TRUE;
      tbw.pszName = L"Menu";
      if (tbw.fIsVisible)
         m_lTASKBARWINDOW.Add (&tbw);
      else
         lHidden.Add (&tbw);
   }
#endif // !UNIFIEDTRANSCRIPT

   // sort this list alphabetically
   qsort (m_lTASKBARWINDOW.Get(0), m_lTASKBARWINDOW.Num(), sizeof(TASKBARWINDOW), TASKBARWINDOWSort);
   qsort (lHidden.Get(0), lHidden.Num(), sizeof(TASKBARWINDOW), TASKBARWINDOWSort);

   // combine into one list
   DWORD dwHiddenStart = m_lTASKBARWINDOW.Num();
   PTASKBARWINDOW ptbw = (PTASKBARWINDOW)lHidden.Get(0);
   m_lTASKBARWINDOW.Required (m_lTASKBARWINDOW.Num() + lHidden.Num());
   for (i = 0; i < lHidden.Num(); i++, ptbw++)
      m_lTASKBARWINDOW.Add (ptbw);
   
   // figure out how many spaces
   DWORD dwSpaces = m_lTASKBARWINDOW.Num()+1;   // so that always have blank between start and end
   DWORD dwMaxSpaces = (rTaskBar.right - rTaskBar.left) / 150;
   dwMaxSpaces = max(dwMaxSpaces, 1);
   dwSpaces = max(dwSpaces, dwMaxSpaces);   // so don't make slots too large
   int iSize = (rTaskBar.right - rTaskBar.left) / (int)dwSpaces;
   iSize = max(iSize, 0);

   // fill the locations in
   ptbw = (PTASKBARWINDOW)m_lTASKBARWINDOW.Get(0);
   RECT rLoc;
   int iHeight;
   POINT pMini;
   iHeight = rTaskBar.top - rTaskBar.bottom;
   pMini.x = (rTaskBar.top + rTaskBar.bottom)/2 - MINIICONSIZE/2;
   pMini.y = pMini.x + MINIICONSIZE;

   for (i = 0; i < m_lTASKBARWINDOW.Num(); i++, ptbw++) {
      rLoc = rTaskBar;
      if (ptbw->fIsVisible)
         rLoc.left += (int)i*iSize;
      else
         rLoc.left = rLoc.right - (int)(m_lTASKBARWINDOW.Num()-i) * iSize;
      rLoc.right = rLoc.left + iSize - TABSPACING;

      // BUGFIX - Hide all the visible windows, only show the hidden ones
      // reduce the clutter
      if (ptbw->fIsVisible)
         rLoc.right = rLoc.left = 0;

      //ptbw->rLocMiniIcon = rLoc;
      //ptbw->rLocMiniIcon.right = min(ptbw->rLocMiniIcon.right, ptbw->rLocMiniIcon.left + MINIICONSIZE);
      //ptbw->rLocMiniIcon.top = pMini.x;
      //ptbw->rLocMiniIcon.bottom = pMini.y;

      ptbw->rLocText = rLoc;
      //if (ptbw->fIsVisible)
      //   ptbw->rLocText.left = ptbw->rLocMiniIcon.right+MINIICONSIZE/4;
      //else
      //   ptbw->rLocMiniIcon.right = ptbw->rLocMiniIcon.left - 1;  // no mini icons when not visible
   } // i

   // if nothing has changed then don't bother redrawing
   if ((lOld.Num() == m_lTASKBARWINDOW.Num()) && 
      !memcmp(lOld.Get(0), m_lTASKBARWINDOW.Get(0), lOld.Num()*sizeof(TASKBARWINDOW)))
      return;

   // redraw
   InvalidateRect (m_hWnd, &rTaskBar, FALSE);
}



/*************************************************************************************
CSlidingWindow::TaskBarSyncTimer - Syncronize the taskbar with a delayed timer so
all the synchronization (of several windows) occurs at once.
*/
void CSlidingWindow::TaskBarSyncTimer (void)
{
#ifndef USEONLYONETOOLBAR
   return;
#endif

   if (m_fTaskBarTimer)
      return;  // already set so dont bother

   // set it, so will sync delays
   m_fTaskBarTimer = TRUE;
   SetTimer (m_hWnd, TIMER_TASKBARSYNCTIMER, 100, NULL);
}


/*************************************************************************************
CSlidingWindow::CloseButtonLoc - Location of the close button.
be

inputs
   RECT           *pr - Filled with location
*/
void CSlidingWindow::CloseButtonLoc (RECT *pr)
{
   if (m_hWnd)
      GetClientRect (m_hWnd, pr);
   else {
      pr->left = pr->top = 0;
      pr->right = pr->bottom = 1000;
   }
   pr->left = pr->right - TABHEIGHT;
   pr->bottom = pr->top + TABHEIGHT;
}

/*************************************************************************************
CSlidingWindow::CommandLoc - Returns the location where the command edit window should
be

inputs
   RECT           *pr - Filled with location
*/
void CSlidingWindow::CommandLoc (RECT *pr)
{
   GetClientRect (m_hWnd, pr);
   pr->left = (pr->right * 3 + pr->left) / 4;
   pr->right -= TABHEIGHT; // for closebuttonloc
   pr->bottom = pr->top + TABHEIGHT;

   pr->left += COMMANDSPACE;
   pr->right -= COMMANDSPACE;
   pr->top += COMMANDSPACE;
   pr->bottom -= COMMANDSPACE;
}


/*************************************************************************************
CSlidingWindow::CommandLabelLoc - Returns the location where the command edit window should
be

inputs
   RECT           *pr - Filled with location
*/
void CSlidingWindow::CommandLabelLoc (RECT *pr)
{
   CommandLoc (pr);
   int iWidth = pr->right - pr->left;
   pr->right = pr->left - 2 * COMMANDSPACE;
   pr->left = pr->right - iWidth/2;
}


/*************************************************************************************
CSlidingWindow::SlideDownTimed - Causes the sliding winddow to slide down and
stay open the given number of milliseconds.

inputs
   DWORD             dwTime - Number of milliseconds
returns
   none
*/
void CSlidingWindow::SlideDownTimed (DWORD dwTime)
{
   m_iStayOpenTime = (int)dwTime;

   // start timer if not already
   SlideTimer (FALSE);
}


/*************************************************************************************
CSlidingWindow::CommandSetFocus - Sets the focus to the command line
*/
void CSlidingWindow::CommandSetFocus (void)
{
   if (m_hWndCommand && IsWindowVisible (m_hWndCommand) && (GetFocus() != m_hWndCommand))
      SetFocus (m_hWndCommand);
}



/*************************************************************************************
CSlidingWindow::CommandIsVisible - Returns TRUE if the command is visible.
*/
BOOL CSlidingWindow::CommandIsVisible (void)
{
   return (m_hWndCommand && IsWindowVisible (m_hWndCommand));
}


/*************************************************************************************
CSlidingWindow::CommandLineShow - Show/hide the command line

inputs
   BOOL           fShow - If TRUE then show the command line
*/
void CSlidingWindow::CommandLineShow (BOOL fShow)
{
   if (!m_hWndCommand)
      return;  // cant do anything

   BOOL fIsVisible = IsWindowVisible (m_hWndCommand);
   if (!fIsVisible == !fShow)
      return;  // no change

   // else
   if (fIsVisible)
      // hide
      ShowWindow (m_hWndCommand, SW_HIDE);
   else
      ShowWindow (m_hWndCommand, SW_SHOW);

   // redraw
   Invalidate ();
}


/*************************************************************************************
CSlidingWindow::CommandTextSet - Sets the text of the command

inputs
   PWSTR          psz - String to set. If NULL, the command is cleared
returns
   none
*/
void CSlidingWindow::CommandTextSet (PWSTR psz)
{
   if (!m_hWndCommand)
      return;

   if (!psz) {
      SendMessage (m_hWndCommand, EM_REPLACESEL, TRUE, (LPARAM)L"");  // select all
      return;
   }

   CMem memAnsi;
   if (!memAnsi.Required ((wcslen(psz)+1)*sizeof(WCHAR)))
      return;
   WideCharToMultiByte (CP_ACP, 0, psz, -1,
      (char*)memAnsi.p, (DWORD)memAnsi.m_dwAllocated, 0, 0);
   SetWindowText (m_hWndCommand, (char*)memAnsi.p);
   SendMessage (m_hWndCommand, EM_SETSEL, 0, -1);  // select all
}




/*************************************************************************************
CSlidingWindow::ToolbarHeight - Returns the height of the toolbar

returns
   DWORD - Height of toolbar
*/
DWORD CSlidingWindow::ToolbarHeight (void)
{
   RECT r;
   ToolbarLoc (&r);

   return (DWORD)r.bottom;
}


/*************************************************************************************
CSlidingWindow::ToolbarLoc - Location of the verb toolbar.
be

inputs
   RECT           *pr - Filled with location
*/
void CSlidingWindow::ToolbarLoc (RECT *pr)
{
   if (!CommandIsVisible())
      // BUGFIX - Let this go all the way across the top
      CloseButtonLoc (pr);
   else
      CommandLabelLoc (pr);

   RECT r;
   if (m_hWnd)
      GetClientRect (m_hWnd, &r);
   else {
      r.left = r.top = 0;
      r.right = r.bottom = 1000;
   }

   // get "Circumreality" menu location
   RECT rCircum;
   TaskBarMenuLoc (&rCircum);
   r.left = rCircum.right; // toobar starts to right of text

   pr->top = r.top;
   pr->right = pr->left - COMMANDSPACE;
   pr->left = r.left + TABSPACING/2;  // so align with tabs

   DWORD dwRows, dwColumns;
   dwColumns = (DWORD)max(pr->right - pr->left, 1) / TOOLBARBUTTONHEIGHT;
   dwColumns = max(dwColumns, 1);
   if (m_pMain && m_pMain->m_pResVerb)
      dwRows = (m_pMain->m_pResVerb->m_lPCResVerbIcon.Num() + dwColumns-1) / dwColumns;
   else
      dwRows = 0;

   pr->bottom = max(pr->bottom, pr->top + (int)dwRows * TOOLBARBUTTONHEIGHT);

      // BUGFIX - Was entire TABSPACING, but made half so not as much
}



/*************************************************************************************
CSlidingWindow::ToolbarVisible - Returns TRUE if the toolbar is visible
*/
BOOL CSlidingWindow::ToolbarIsVisible (void)
{
   return m_fToolbarVisible;
}


/*************************************************************************************
CSlidingWindow::ToolbarShow - Shows or hides the toolbar.

inputs
   BOOL           fShow - TRUE if showing
*/
void CSlidingWindow::ToolbarShow (BOOL fShow)
{
   if (!fShow == !m_fToolbarVisible)
      return;  // no change

   m_fToolbarVisible = TRUE;

   // if not visible hide
   if (!m_fToolbarVisible) {
      if (!m_pMain->m_pResVerb)
         return;

      PCResVerbIcon *ppr = (PCResVerbIcon*)m_pMain->m_pResVerb->m_lPCResVerbIcon.Get(0);
      DWORD dwNum = m_pMain->m_pResVerb->m_lPCResVerbIcon.Num();
      DWORD i;
      // NOTE: not tested
      for (i = 0; i < dwNum; i++) {
         PCResVerbIcon pr = ppr[i];
         if (pr->m_pButton) {
            delete pr->m_pButton;
            pr->m_pButton = NULL;
         }
      } // i

      return;
   }

   // else, just call the arrange
   ToolbarArrange ();

}




/*************************************************************************************
CSlidingWindow::VerbButtonsArrange - Arrange and show verb buttons.
*/
void CSlidingWindow::ToolbarArrange (void)
{
   if (!m_fToolbarVisible || !m_pMain->m_pResVerb || !m_fOnTop)
      return;  // nothing to do

   // icon size
   DWORD dwSize = TOOLBARBUTTONHEIGHT;

   // rows and columns
   RECT r;
   DWORD dwRows, dwColumns;
   ToolbarLoc (&r);
   r.right = max(r.right, r.left);
   r.bottom = max(r.bottom, r.top);
   dwColumns = (DWORD)(r.right - r.left) / dwSize;
   dwColumns = max(dwColumns, 1);
   dwRows = (DWORD)(r.bottom - r.top) / dwSize;
   dwRows = max(dwRows, 1);

   // figure out the offset
   POINT pOffset;
   pOffset.x = ((r.right - r.left) - (int)dwColumns * (int)dwSize) / 2 + r.left;
   pOffset.y = ((r.bottom - r.top) - (int)dwRows * (int)dwSize) / 2 + r.top;

   // loop throw all the rows and columns
   DWORD x, y;
   PCResVerbIcon *ppr = (PCResVerbIcon*)m_pMain->m_pResVerb->m_lPCResVerbIcon.Get(0);
   CMem memName, memSan;
   DWORD dwMaxRows = m_pMain->m_pResVerb->m_lPCResVerbIcon.Num() / dwColumns + 1;
   for (y = 0; y < dwMaxRows; y++) {
      for (x = 0; x < dwColumns; x++) {
         DWORD dwVerb = x + y * dwColumns;
         if (dwVerb >= m_pMain->m_pResVerb->m_lPCResVerbIcon.Num())
            continue;
         PCResVerbIcon pr = ppr[dwVerb];

         // figure out rect where it goes
         r.left = pOffset.x + (int)x * (int)dwSize;
         r.right = r.left + (int)dwSize;
         r.top = pOffset.y + (int)y * (int)dwSize;
         r.bottom = r.top + (int)dwSize;

         // move or create it
         if (pr->m_pButton) {
            pr->m_pButton->Move (&r);
            continue;
         }
         
         // convert to ANSI
         PWSTR psz = (PWSTR)pr->m_memShow.p;
         if (!psz || !psz[0])
            psz = (PWSTR)pr->m_memDo.p;
         MemZero (&memSan);   // sanitize
         MemCatSanitize (&memSan, psz);
         psz = (PWSTR)memSan.p;
         if (!memName.Required ((wcslen(psz)+1)*sizeof(WCHAR)))
            continue;
         WideCharToMultiByte (CP_ACP, 0, psz, -1, (char*)memName.p, (DWORD)memName.m_dwAllocated, 0,0);

         // else, need to create
         pr->m_pButton = new CIconButton;
         if (!pr->m_pButton)
            continue;

         // set the color and info
         if (pr->m_fHasClick)
            pr->m_pButton->ColorSet (m_pMain->m_cBackgroundNoChange, m_pMain->m_cVerbDimNoChange, m_pMain->m_cTextDimNoChange, m_pMain->m_cTextNoChange);
         else
            pr->m_pButton->ColorSet (m_pMain->m_cBackgroundNoTabNoChange, m_pMain->m_cTextDimNoChange, m_pMain->m_cTextDimNoChange, m_pMain->m_cTextNoChange);
            // BUGFIX - Changed m_cVerbDim to m_cBackgroundNoTab

         if (!pr->m_pButton->Init (pr->IconResourceID(), pr->IconResourceInstance(), NULL,
            (char*)memName.p, "", 1, &r, m_hWnd, 1000 + dwVerb, dwSize)) {
               delete pr->m_pButton;
               continue;
            }
      } // x
   } // y
}


/*************************************************************************************
CSlidingWindow::TutorialCursor - Ask for the location of a specific item
so can slide to it on the tutorial.

inputs
   DWORD          dwItem - 0 for menu button, 1 for list of windows,
                           2 for tab, 3 for toolbar, 4 for edit
   POINT          *pp - Filled with the location in SCREEN coordinates
   RECT           *pr - Filled with rectangle
returns
   none
*/
void CSlidingWindow::TutorialCursor (DWORD dwItem, POINT *pp, RECT *pr)
{
   RECT rLoc;
   switch (dwItem) {
   case 0:  // menu
   default:
      TaskBarMenuLoc (&rLoc);
      *pr = rLoc;
      break;

   case 1:  // list of windows
      {
         RECT r;
         GetClientRect (m_hWnd, &r);
         TaskBarMenuLoc (&rLoc);
         *pr = rLoc;
         rLoc.left = rLoc.right;
         rLoc.right = r.right;
      }
      break;

   case 2:  // tab
      TabLoc (0, &rLoc);
      *pr = rLoc;
      break;

   case 3:  // toolbar
      ToolbarLoc (&rLoc);
      *pr = rLoc;
      break;

   case 4:  // edit
      CommandLoc (&rLoc);
      *pr = rLoc;
      break;
   } // switch

   // find average
   pp->x = (rLoc.right + rLoc.left) / 2;
   pp->y = (rLoc.top + rLoc.bottom) / 2;

   // convert to screen
   ClientToScreen (m_hWnd, pp);

   // also do the taskbar
   RECT rThis;
   GetClientRect (m_hWnd, &rThis);
   pr->top = rThis.top; // so more easily visible
   pr->bottom = rThis.bottom;
   POINT *paRect = (POINT*)pr;
   ClientToScreen (m_hWnd, paRect + 0);
   ClientToScreen (m_hWnd, paRect + 1);

   // make sure not beyond the edge of the main windows client
   // this ensures that popup bits will actually popup
   RECT rMain;
   GetClientRect (m_pMain->m_hWndPrimary, &rMain);
   ClientToScreen (m_pMain->m_hWndPrimary, (POINT*) &rMain.left);
   ClientToScreen (m_pMain->m_hWndPrimary, (POINT*) &rMain.right);
   rMain.top++;
   rMain.bottom--;
   rMain.right--;
   rMain.left++;

   pp->x = max(pp->x, rMain.left);
   pp->x = min(pp->x, rMain.right);
   pp->y = max(pp->y, rMain.top);
   pp->y = min(pp->y, rMain.bottom);
}
