/***********************************************************************
CEscWindow - Code for CEscWindow

begun 3/29/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

// #define FINDWHYNOTOOLTIP

#include "mymalloc.h"
#include <windows.h>
#include <zmouse.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "escarpment.h"
#include "tools.h"
#include "fontcache.h"
#include "paint.h"
#include "textwrap.h"
#include "mmlparse.h"
#include "mmlinterpret.h"
#include "resource.h"
#include "jpeg.h"
#include <crtdbg.h>
#include "control.h"
#include "resleak.h"

extern HINSTANCE ghInstance;
extern DWORD     gdwInitialized; // init count
static BOOL       gfUseTraiditionalCursor = FALSE; // flag so can use traditional cursor in case video driver busted

#define  MOUSETIMER     1
#define  TIMERINTERVAL  250
static char gszEscWindowClass[] = "EscWindowClass%d";
static HMIDIOUT   ghMidi = NULL;
static HWND       ghWndMidiMap = NULL;
static DWORD      gdwMidiMapMessage = 0;
static DWORD      gdwMidiCount = 0;
static DWORD      gdwInstance = 1;  // instance to use
static BOOL       gdwSounds = ESCS_ALL;   // use beeps
typedef struct {
   DWORD       dwMIDI;     // MIDI to send
   DWORD       dwCount;    // decrememnt count every timer. send when reaches 0
} NOTEOFF, *PNOTEOFF;


typedef struct {
   PWSTR    pszTitle;      // page title
   PWSTR    pszSection;    // section title
   PWSTR    pszPage;       // page number
   PWSTR    pszTime;       // time
   PWSTR    pszDate;       // date
} PRINTHF, *PPRINTHF;

/***********************************************************************
EscTraditionalCursorSet - Sets whether or not to use the traiditonal
cursor (arrow and circle-with-slash). Can turn on if the video driver
is busted.

inputs
   BOOL     fUse - If TRUE use traiditoanl
*/
void EscTraditionalCursorSet (BOOL fUse)
{
   gfUseTraiditionalCursor = fUse;
}

/***********************************************************************
EscTraditionalCursorGet - Returns TRUE if the traditional cursor is
being used. FALSE if not
*/
BOOL EscTraditionalCursorGet (void)
{
   return gfUseTraiditionalCursor;
}


/***********************************************************************
Page callback
*/
BOOL PrintPageCallback (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   if (!pPage)
      return FALSE;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PWSTR pszTest = L"TestString";
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         PPRINTHF hf = (PPRINTHF) pPage->m_pUserData;
         p->fMustFree = FALSE;
         if (!_wcsicmp(p->pszSubName, L"PageTitle"))
            p->pszSubString = hf ? hf->pszTitle : pszTest;
         else if (!_wcsicmp(p->pszSubName, L"Section"))
            p->pszSubString = hf ? hf->pszSection : pszTest;
         else if (!_wcsicmp(p->pszSubName, L"Page"))
            p->pszSubString = hf ? hf->pszPage : L"1";
         else if (!_wcsicmp(p->pszSubName, L"Time"))
            p->pszSubString = hf ? hf->pszTime : pszTest;
         else if (!_wcsicmp(p->pszSubName, L"Date"))
            p->pszSubString = hf ? hf->pszDate : pszTest;
      }
      return TRUE;
   }

   return FALSE;
}


/**********************************************************************
EscSoundsSet - Set what kind of sound levels are used in escarpment

inputs
   DWORD - value. Combination of ESCS_XXX
returns
   none
*/
void EscSoundsSet (DWORD dwFlags)
{
   gdwSounds = dwFlags;
}


/**********************************************************************
EscSoundsGet - Returns the types of sounds currently on.

inputs
   none
returns
   DWORD - value. Combinatin of ESCS_XXX
*/
DWORD EscSoundsGet (void)
{
   return gdwSounds;
}


/**********************************************************************
MIDIRemapSet - Sets the ghWndMidiMap. If this is set then all
MIDI calls will be a post message go ghWndMidiMap.

NOTE: When a message is sent, wParam will be 0 for a MIDI open, 1 for a MIDI
   close, or 2 for a MIDI message (in which case lParam is the mssage)

inputs
   HWND        hWnd - Window to post to. Or NULL if turn off.
                     If this is -1 then message will not cause midi to open
                     and will not send to any window
   DWORD       dwMessage - Windows message (WM_USER+XXX) to send to hWnd
returns
   none
*/
void MIDIRemapSet (HWND hWnd, DWORD dwMessage)
{
   ghWndMidiMap = hWnd;
   gdwMidiMapMessage = dwMessage;
}


/**********************************************************************
MIDIClaim & MIDIRelease - So that can have several windows using the midi
device for beeps
*/
HMIDIOUT MIDIClaim (void)
{
   if (!ghMidi) {
      if (ghWndMidiMap) {
         ghMidi = (HMIDIOUT) 1; // so know
         if (ghWndMidiMap != (HWND)-1)
            SendMessage (ghWndMidiMap, gdwMidiMapMessage, 0, 0);
      }
      else
         midiOutOpen (&ghMidi, MIDI_MAPPER, NULL, 0, CALLBACK_NULL);
   }
   if (ghMidi)
      gdwMidiCount++;
   return ghMidi;
}

void MIDIRelease (void)
{
   if (gdwMidiCount)
      gdwMidiCount--;
   if (!gdwMidiCount && ghMidi) {
      if (ghWndMidiMap) {
         if (ghWndMidiMap != (HWND)-1)
            SendMessage (ghWndMidiMap, gdwMidiMapMessage, 1, 0);
      }
      else
         midiOutClose (ghMidi);
      ghMidi = NULL;
   }
}

void MIDIShutdown (void)
{
   if (ghMidi) {
      if (ghWndMidiMap) {
         if (ghWndMidiMap != (HWND)-1)
            SendMessage (ghWndMidiMap, gdwMidiMapMessage, 1, 0);
      }
      else
         midiOutClose (ghMidi);
   }
   ghMidi = NULL;
   gdwMidiCount = 0;
}

/***********************************************************************
MIDIShortMsg - Send a short message using the midi handle or the remap
*/
MMRESULT MIDIShortMsg (HMIDIOUT hMidi, DWORD dw)
{
   if (ghWndMidiMap && (hMidi == (HMIDIOUT)1)) {
      if (ghWndMidiMap != (HWND)-1)
         SendMessage (ghWndMidiMap, gdwMidiMapMessage, 2, (LPARAM) dw);
      return 0;
   }
   else
      return midiOutShortMsg (hMidi, dw);
}


/***********************************************************************
FillRectBackground - Fill a rectangle with the background image.
   Uses m_rLastPage and m_hbmpBackground

inputs
   HDC            hDC - To draw into.
returns
   BOOL - TRUE if success, FALSE if no image, etc.
*/
BOOL CEscWindow::FillRectBackground (HDC hDC)
{
   if (!m_hbmpBackground)
      return FALSE;

   if (!m_hWnd)
      return FALSE;

   // get the window rect and client rect
   RECT rWindow, rClient;
   GetWindowRect (m_hWnd, &rWindow);
   GetClientRect (m_hWnd, &rClient);
   ScreenToClient (m_hWnd, ((LPPOINT)&rWindow) + 0);
   ScreenToClient (m_hWnd, ((LPPOINT)&rWindow) + 1);

   // determine the scaling factor
   double fScaleX = (double)max(m_rBackgroundTo.right - m_rBackgroundTo.left, 1) / (double)max(m_rBackgroundFrom.right - m_rBackgroundFrom.left,1);
   double fScaleY = (double)max(m_rBackgroundTo.bottom - m_rBackgroundTo.top, 1) / (double)max(m_rBackgroundFrom.bottom - m_rBackgroundFrom.top,1);

   // where actually go to
   RECT rTo;
   rTo = m_rBackgroundTo;
   POINT pDelta;
   pDelta.x = (int) floor((double)(rClient.left - rWindow.left) * fScaleX + 0.5);
   pDelta.y = (int) floor((double)(rClient.top - rWindow.top) * fScaleY + 0.5);
   OffsetRect (&rTo, -pDelta.x, -pDelta.y);

   // stretch
   HDC hDCBmp = CreateCompatibleDC (hDC);
   SelectObject (hDCBmp, m_hbmpBackground);
   EscStretchBlt (
      hDC,
      rTo.left, rTo.top, rTo.right - rTo.left, rTo.bottom - rTo.top,
      hDCBmp, m_rBackgroundFrom.left, m_rBackgroundFrom.top,
      m_rBackgroundFrom.right - m_rBackgroundFrom.left, m_rBackgroundFrom.bottom - m_rBackgroundFrom.top,
      SRCCOPY,
      m_hbmpBackground);
   DeleteDC (hDCBmp);

   return TRUE;
}


/***********************************************************************
EscWndProc - Window procedure for the esc window
*/
LRESULT CEscWindow::EscWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC;
         hDC = BeginPaint (hWnd, &ps);
         RECT  r;
         GetClientRect (hWnd, &r);

         // remeber this for DC get
         m_hDC = hDC;
         m_dwDCCount = 1000;  // large #

         // if not bitmap create one
         BOOL fJustMade;
         fJustMade = FALSE;
         if (!m_hbitLastPage) {
            if (m_pPage)
               m_rLastPage = m_pPage->m_rVisible;
            else
               m_rLastPage = m_rClient;

            m_hbitLastPage = CreateCompatibleBitmap (hDC, m_rLastPage.right - m_rLastPage.left,
               m_rLastPage.bottom - m_rLastPage.top);
            fJustMade = FALSE;

            // add to the invalid list
            m_listInvalid.Clear();
            m_listInvalid.Add (&m_rLastPage);
         }
         if (!m_hbitLastPage) {
            // clear DC
            m_hDC = NULL;
            m_dwDCCount = 0;

            EndPaint (m_hWnd, &ps);
            return 0;
         }

         // create and load into memory DC
         HDC   hDCMem;
         RECT  rBit;
         hDCMem = CreateCompatibleDC (hDC);
#ifdef _DEBUG
         int   ix, iy;
         BMPSize (m_hbitLastPage, &ix, &iy);
#endif
         SelectObject (hDCMem, m_hbitLastPage);
         rBit.top = rBit.left = 0;
         rBit.right = m_rLastPage.right - m_rLastPage.left;
         rBit.bottom = m_rLastPage.bottom - m_rLastPage.top;

         // if we just made the bitmap and dont have any page then blank it out
         if (fJustMade && !m_pPage) {
            if (!FillRectBackground (hDCMem))
               FillRect (hDCMem, &rBit, (HBRUSH) (COLOR_WINDOW+1));
         }

         // bitmap for fillrectbackground
         HBITMAP hBmpBack = NULL;
         HDC hDCBack = NULL;
         if (m_hbmpBackground && m_listInvalid.Num()) {
            hBmpBack = CreateCompatibleBitmap (hDC, rBit.right, rBit.bottom);
            hDCBack = CreateCompatibleDC (hDC);
            SelectObject (hDCBack, hBmpBack);
            FillRect (hDCBack, &rBit, (HBRUSH) (COLOR_WINDOW+1));

            FillRectBackground (hDCBack);
         }

         // paint any invalid rectangles
         // BUGFIX - this is a bit tricky because the act of repainting might cause
         // other controls to get invalidated.
         while (m_listInvalid.Num()) {
            // remember that painted this
            m_fHasPagePainted = TRUE;

            RECT  *pInvalid;
            pInvalid = (RECT*) m_listInvalid.Get(0);

            // do this crazy dance so that if redraw one control, that causes
            // another to be redrawn, then we won't forget about that redraw
            RECT  rInvalid;
            rInvalid = *pInvalid;
            m_listInvalid.Remove(0);

            // BUGFIX - Move the m_pPage test down so it wouldnt hang
            if (!m_pPage)
               continue;

            // invalid rectangle, paint into it

            // figure out the HDC ands creen coordinates for this
            RECT rPage, rHDC, rScreen;
            IntersectRect (&rPage, &rInvalid, &m_pPage->m_rVisible);
            m_pPage->CoordPageToWindow (&rPage, &rHDC);
            m_pPage->CoordPageToScreen (&rPage, &rScreen);

            if (rPage.right != rPage.left) {
               // blank it out
               if (hBmpBack && hDCBack)
                  BitBlt (hDCMem, rHDC.left, rHDC.top, rHDC.right - rHDC.left, rHDC.bottom - rHDC.top,
                     hDCBack, rHDC.left, rHDC.top, SRCCOPY);
               else
                  FillRect (hDCMem, &rHDC, (HBRUSH) (COLOR_WINDOW+1) );

               // paint it
               m_pPage->Paint (&rPage, &rHDC, &rScreen, hDCMem);
            }

         }


         // bitblt the image
         // rely on invalid rect to do some clipping
         BitBlt (hDC, 0, 0, m_rLastPage.right - m_rLastPage.left, m_rLastPage.bottom - m_rLastPage.top,
            hDCMem, 0, 0, SRCCOPY);

         // remeber the last visible
         if (m_pPage)
            m_rLastPage = m_pPage->m_rVisible;

         // clear DC
         m_hDC = NULL;
         m_dwDCCount = 0;

         if (hDCBack)
            DeleteDC (hDCBack);
         if (hBmpBack)
            DeleteObject (hBmpBack);

         DeleteDC (hDCMem);
         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_SIZE:
      // if iconic then ignore
      if (IsIconic (hWnd))
         return 0;

      // remember the screen coords
      GetWindowRect (hWnd, &m_rScreen);
      GetClientRect (hWnd, &m_rClient);
      ClientToScreen (hWnd, (PPOINT) (&m_rClient) + 0);
      ClientToScreen (hWnd, (PPOINT) (&m_rClient) + 1);

      // free the bitmap
      if (m_hbitLastPage)
         DeleteObject (m_hbitLastPage);
      m_hbitLastPage = NULL;

      // tell the page that have sized
      if (m_pPage) {
         RECT  r;
         GetClientRect (hWnd, &r);
         // assume no horizontal scroll bar
         m_pPage->m_rVisible.left = r.left;
         m_pPage->m_rVisible.right = r.right;
         m_pPage->m_rVisible.bottom = m_pPage->m_rVisible.top + r.bottom;
#if 0 // casuing bugs
         // if top+bottom > total bottom then scroll up
         if (m_pPage->m_rVisible.bottom > m_pPage->m_rTotal.bottom) {
            int   iDelta;
            iDelta = m_pPage->m_rVisible.bottom - m_pPage->m_rTotal.bottom;
            iDelta = min(iDelta, m_pPage->m_rVisible.top);
            m_pPage->m_rVisible.top -= iDelta;
            m_pPage->m_rVisible.bottom -= iDelta;
         }
#endif // 0
         m_pPage->Message (ESCM_SIZE, NULL);

      }

      break;

   case WM_MOVE:
      // if iconic ignore
      if (IsIconic (hWnd))
         return 0;

      // BUGFIX - If this has a background bitmap then invalidate
      if (m_hbmpBackground) {
         if (m_hbitLastPage)
            DeleteObject (m_hbitLastPage);
         m_hbitLastPage = NULL;
         InvalidateRect (hWnd, NULL, FALSE);
      }

      // remember the screen coords
      GetWindowRect (hWnd, &m_rScreen);
      GetClientRect (hWnd, &m_rClient);
      ClientToScreen (hWnd, (PPOINT) (&m_rClient) + 0);
      ClientToScreen (hWnd, (PPOINT) (&m_rClient) + 1);

      // tell the page that have moved
      if (m_pPage)
         m_pPage->Message (ESCM_MOVE, NULL);

      break;


   case WM_CLOSE:
      if (m_pPage)
         m_pPage->Message (ESCM_CLOSE, NULL);
      return 0;


   case WM_VSCROLL:
      {
         // get the current position
         SCROLLINFO  si;
         memset (&si, 0, sizeof(si));
         si.cbSize = sizeof(si);
         si.fMask = SIF_ALL;
         GetScrollInfo (hWnd, SB_VERT, &si);

         // switch nScrollCode
         switch (LOWORD(wParam)) {
         case SB_BOTTOM:
            si.nPos = si.nMax;
            break;
         case SB_ENDSCROLL:
            // ignore end-scroll since really doesn't
            // make a differnet
            return 0;
         case SB_LINEDOWN:
            // BUGFIX - On compaq presario portable scroll buttons move twice as much
            si.nPos += 32; // max(1, si.nPage / 16);
            break;
         case SB_LINEUP:
            // BUGFIX - On compaq presario portable scroll buttons move twice as much
            si.nPos -= 32; // max(1, si.nPage / 16);
            break;
         case SB_PAGEDOWN:
            si.nPos += (si.nPage ? si.nPage : ((si.nMax - si.nMin)/16));
            break;
         case SB_PAGEUP:
            si.nPos -= (si.nPage ? si.nPage : ((si.nMax - si.nMin)/16));
            break;
         case SB_THUMBPOSITION:
         case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;

         case SB_TOP:
         //case SB_LEFT:
         //case SB_RIGHT:
         //case SB_LINELEFT:
         //case SB_LINERIGHT:
         //case SB_PAGELEFT:
         //case SB_PAGERIGHT:
            break;
         }

         si.nPos = max(si.nMin, si.nPos);
         si.nPos = min(si.nPos, si.nMax - max((int)si.nPage-1,0));

         // set it
         // SetScrollPos(hWnd, SB_VERT, si.nPos, TRUE);

#ifdef _DEBUG
   char  szTemp[256];
   wsprintf (szTemp, "si.nPos = %d\r\n", si.nPos);
   OutputDebugString (szTemp);
#endif
         // tell the page
         if (m_pPage)
            m_pPage->VScroll (si.nPos);

      }
      return 0;

   case WM_MOUSEMOVE:
      {
#ifdef FINDWHYNOTOOLTIP
         EscOutputDebugString (L"\r\nCEscWindow::MouseMove");
#endif
         // set the cursor
         ::SetCursor (m_hCursor);

         // show the cursor
         while (ShowCursor (TRUE) < 1);
         while (ShowCursor (FALSE) > 0);  // don't want a really huge cursor count

         // find out where the mouse is
         ESCMMOUSEMOVE  m;
         POINT p;
         m.pPosn.x = (int) (short) LOWORD(lParam);
         m.pPosn.y = (int) (short) HIWORD(lParam);
         m.wKeys = wParam;
         p = m.pPosn;
         ClientToScreen (m_hWnd, &p);
         BOOL fMovedBefore;
         // BUGFIX - Don't have hoverhelp disappear unless have moved before
         fMovedBefore = (m_pLastMouse.x != -1000) || (m_pLastMouse.y != -1000);

#ifdef FINDWHYNOTOOLTIP
         WCHAR szTemp[128];
         swprintf (szTemp, L"\r\n\tCursorPos %d,%d", p.x, p.y);
         EscOutputDebugString (szTemp);
#endif

         // BUGFIX - allow to move a few pixels
         // BUGFIX - Dont update lastmouse unless has actually moved
         if ((abs(m_pLastMouse.x - p.x) > 4) || (abs(m_pLastMouse.y - p.y) > 4)) {
            m_pLastMouse = p;
            m_dwLastMouse = GetTickCount();
#ifdef FINDWHYNOTOOLTIP
            EscOutputDebugString (L"\r\n\tMouse moved");
#endif
         }

         if (m_pPage)
            m_pPage->CoordScreenToPage (&p, &m.pPosn);

         // if it wasn't in the client area before then notify its entered
         if (!m_fMouseOver && PtInRect (&m_rClient, p)) {
            m_fMouseOver = TRUE;
            if (m_pPage)
               m_pPage->Message (ESCM_MOUSEENTER);
         }

         // send this down
         if (m_pPage)
            m_pPage->Message (ESCM_MOUSEMOVE, &m);

         // if flag, close if mouse moves
         // BUGFIX - Don't have hoverhelp disappear unless have moved before
         if (fMovedBefore && (m_dwStyle & EWS_CLOSEMOUSEMOVE)) {
            // close the window
            if (m_pPage)
               m_pPage->Message (ESCM_CLOSE, NULL);
         }
      }
      return 0;

   case WM_LBUTTONDOWN:
   case WM_RBUTTONDOWN:
   case WM_MBUTTONDOWN:
      {
         // find out where the mouse is
         ESCMLBUTTONDOWN  m;
         POINT p;
         m.pPosn.x = (int) (short) LOWORD(lParam);
         m.pPosn.y = (int) (short) HIWORD(lParam);
         m.wKeys = wParam;
         p = m.pPosn;
         ClientToScreen (m_hWnd, &p);
         m_pLastMouse = p;
         m_dwLastMouse = GetTickCount();
         if (m_pPage)
            m_pPage->CoordScreenToPage (&p, &m.pPosn);

         // capture the mouse if it's not already
         if (!m_fCapture) {
            m_fCapture = TRUE;
            SetCapture (m_hWnd);
         }

         // remeber which buton is down & tell page
         switch (uMsg) {
         case WM_LBUTTONDOWN:
            m_fLButtonDown = TRUE;
            if (m_pPage)
               m_pPage->Message (ESCM_LBUTTONDOWN, &m);
            break;
         case WM_RBUTTONDOWN:
            // BUGFIX - Was sending down wrong button
            m_fRButtonDown = TRUE;
            if (m_pPage)
               m_pPage->Message (ESCM_RBUTTONDOWN, &m);
            break;
         case WM_MBUTTONDOWN:
            // BUGFIX - Was sending down wrong button
            m_fMButtonDown = TRUE;
            if (m_pPage)
               m_pPage->Message (ESCM_MBUTTONDOWN, &m);
            break;
         }

      }
      return 0;

   case WM_LBUTTONUP:
   case WM_RBUTTONUP:
   case WM_MBUTTONUP:
      {
         // find out where the mouse is
         ESCMLBUTTONDOWN  m;
         POINT p;
         m.pPosn.x = (int) (short) LOWORD(lParam);
         m.pPosn.y = (int) (short) HIWORD(lParam);
         m.wKeys = wParam;
         p = m.pPosn;
         ClientToScreen (m_hWnd, &p);
         m_pLastMouse = p;
         m_dwLastMouse = GetTickCount();
         if (m_pPage)
            m_pPage->CoordScreenToPage (&p, &m.pPosn);

         // remeber which buton is down & free capture & tell page
         switch (uMsg) {
         case WM_LBUTTONUP:
            m_fLButtonDown = FALSE;
            if (!m_fLButtonDown && !m_fMButtonDown && !m_fRButtonDown)
               MouseCaptureRelease();
            if (m_pPage)
               m_pPage->Message (ESCM_LBUTTONUP, &m);
            break;
         case WM_RBUTTONUP:
            // BUGFIX - Was sending wrong button
            m_fRButtonDown = FALSE;
            if (!m_fLButtonDown && !m_fMButtonDown && !m_fRButtonDown)
               MouseCaptureRelease();
            if (m_pPage)
               m_pPage->Message (ESCM_RBUTTONUP, &m);
            break;
         case WM_MBUTTONUP:
            // BUGFIX - Was sending wrong button
            m_fMButtonDown = FALSE;
            if (!m_fLButtonDown && !m_fMButtonDown && !m_fRButtonDown)
               MouseCaptureRelease();
            if (m_pPage)
               m_pPage->Message (ESCM_MBUTTONUP, &m);
            break;
         }

      }
      return 0;

   case WM_TIMER:
      {
         DWORD dwID = (DWORD) wParam;

         if (dwID == MOUSETIMER) {
#ifdef FINDWHYNOTOOLTIP
         EscOutputDebugString (L"\r\nCEscWindow::MouseTimer");
#endif
            if (m_hDCPrint)
               return 0;   // dont do anything

            // see if there are any midi notes to shut off
            DWORD i,dwNum;
            dwNum = m_listNOTEOFF.Num();
            for (i = dwNum-1; i < dwNum; i--) {
               PNOTEOFF p = (NOTEOFF*) m_listNOTEOFF.Get(i);
               p->dwCount--;

               if (p->dwCount)
                  continue;

               // else delete
               if (m_hMIDI)
                  MIDIShortMsg (m_hMIDI, p->dwMIDI);
               m_listNOTEOFF.Remove (i);

            }

            // turn MIDI off?
            if (m_dwMIDIOff)
               m_dwMIDIOff--;
            if (!m_dwMIDIOff && m_hMIDI) {
               MIDIRelease();
               m_hMIDI = NULL;
            }

            POINT p;
            if (!GetCursorPos (&p)) {
#ifdef FINDWHYNOTOOLTIP
               EscOutputDebugString (L"\r\n\tGetCursorPos failed");
#endif
               p.x = p.y = 0;
            }
#ifdef FINDWHYNOTOOLTIP
            WCHAR szTemp[128];
            swprintf (szTemp, L"\r\n\tCursorPos %d,%d", p.x, p.y);
            EscOutputDebugString (szTemp);
#endif
            BOOL  fOver;
            fOver = (WindowFromPoint(p) == m_hWnd);

#ifdef FINDWHYNOTOOLTIP
            EscOutputDebugString (L"\r\n\tCheckEnabled");
#endif
            // if the window isn't enabled then don't do mouse-move timer messages
            // or hovering
            if (!IsWindowEnabled (m_hWnd)) {
               if (fOver)
                  ::SetCursor (m_hNo);
#ifdef FINDWHYNOTOOLTIP
               EscOutputDebugString (L"\r\n\tNot enabled");
#endif
               return 0;
            }

#ifdef FINDWHYNOTOOLTIP
            EscOutputDebugString (L"\r\n\tCheckSameAsLastMouse");
#endif

            // BUGFIX - Dont have as an exact a mouse move
            if ((abs(m_pLastMouse.x - p.x) <= 4) && (abs(m_pLastMouse.y - p.y) <= 4)) {
            // if ((p.x == m_pLastMouse.x) && (p.y == m_pLastMouse.y)) {
               // it hasn't moved
#ifdef FINDWHYNOTOOLTIP
               EscOutputDebugString (L"\r\n\tSame place");
#endif

               // see if it hasn't moved for awhile
#ifdef FINDWHYNOTOOLTIP
               swprintf (szTemp, L"\r\n\tTick=%d, lastmouse=%d, fOver=%d, ptinrect=%d m_rClient=%d,%d,%d,%d",
                  (int)GetTickCount(), (int)m_dwLastMouse, (int)fOver, (int)PtInRect(&m_rClient,p),
                  (int)m_rClient.left, (int)m_rClient.right, (int)m_rClient.top, (int)m_rClient.bottom);
               EscOutputDebugString (szTemp);
#endif
               if ((GetTickCount() > m_dwLastMouse + 1000) && fOver && PtInRect (&m_rClient, p)) {
                  // it hasn't moved for awhile
                  m_dwLastMouse = (DWORD)-1; // so that won't get reset until move again
#ifdef FINDWHYNOTOOLTIP
                  EscOutputDebugString (L"\r\n\tSuccede");
#endif
                  if (m_pPage) {
#ifdef FINDWHYNOTOOLTIP
                     EscOutputDebugString (L"\r\n\tm_pPage TRUE");
#endif
                     ESCMMOUSEHOVER mh;
                     memset (&mh, 0, sizeof(mh));
                     m_pPage->CoordScreenToPage (&p, &mh.pPosn);
#ifdef FINDWHYNOTOOLTIP
                     swprintf (szTemp, L"\r\n\tmh.pPosn=%d,%d",
                        (int)mh.pPosn.x, (int)mh.pPosn.y);
                     EscOutputDebugString (szTemp);
#endif
                     m_pPage->Message (ESCM_MOUSEHOVER, &mh);
                  }
               }

               return 0;   // hasn't moved
            }
#ifdef FINDWHYNOTOOLTIP
            EscOutputDebugString (L"\r\n\tHas moved");
#endif
            BOOL  fHasReallyMoved;
            fHasReallyMoved = (m_pLastMouse.x >= 0) && (m_pLastMouse.y >= 0);

            // BUGFIX - allow to move a few pixels
            // BUGFIX - Dont update lastmouse unless has actually moved
            if ((abs(m_pLastMouse.x - p.x) > 4) || (abs(m_pLastMouse.y - p.y) > 4)) {
               m_pLastMouse = p;
               m_dwLastMouse = GetTickCount();
#ifdef FINDWHYNOTOOLTIP
               EscOutputDebugString (L"\r\n\tMouse moved");
#endif
            }
            
            // set the cursor
            if (fOver && PtInRect (&m_rClient, p))
               ::SetCursor (m_hCursor);

            // if it wasn't in the client area before then notify its entered
            if (fOver && !m_fMouseOver && PtInRect (&m_rClient, p)) {
               m_fMouseOver = TRUE;
               if (m_pPage)
                  m_pPage->Message (ESCM_MOUSEENTER);
            }

            if (fOver && PtInRect (&m_rClient, p)) {
               // find out where the mouse is
               ESCMMOUSEMOVE  m;
               m.wKeys = 0;
               if (m_pPage)
                  m_pPage->CoordScreenToPage (&p, &m.pPosn);

               // send this down
               if (m_pPage)
                  m_pPage->Message (ESCM_MOUSEMOVE, &m);
            }

            // see if need to set the not in window area bit
            if (fOver && m_fMouseOver && !PtInRect (&m_rClient, p)) {
               m_fMouseOver = FALSE;
               if (m_pPage)
                  m_pPage->Message (ESCM_MOUSELEAVE);
            }

            // if we have the flag set so that if mouse is off window then shut down,
            // then check
            if (m_dwStyle & EWS_CLOSENOMOUSE) {
               RECT rWindow;
               GetWindowRect (m_hWnd, &rWindow);
               if (!PtInRect (&rWindow, p) && !PtInRect (&m_rMouseMove, p)) {
                  // close the window
                  if (m_pPage)
                     m_pPage->Message (ESCM_CLOSE, NULL);
               }
            }
            if (fHasReallyMoved && (m_dwStyle & EWS_CLOSEMOUSEMOVE)) {
               // close the window
               if (m_pPage)
                  m_pPage->Message (ESCM_CLOSE, NULL);
            }

         }
         else {
            DWORD i;
            ESCTIMERID *p;
            for (i = 0; i < m_listESCTIMERID.Num(); i++) {
               p = (ESCTIMERID*) m_listESCTIMERID.Get(i);
               if (p->dwID != dwID)
                  continue;

               // fonud the timer
               ESCMTIMER et;
               et.dwID = dwID;
               if (p->pControl)
                  p->pControl->Message (p->dwMessage, &et);
               else if (p->pPage)
                  p->pPage->Message (p->dwMessage, &et);
            }
         }
      }
      return 0;

   case WM_CHAR:
   case WM_SYSCHAR:
      {
         ESCMCHAR p;
         p.fEaten = FALSE;
         p.lKeyData = lParam;

         // BUGFIX - If use key-sequence for german umlout (see WM_DEADCHAR),
         // then this may be > 127, which means the unicode value may be different
         // than the ANSI value
         char  szOrig[2];
         WCHAR szUnicode[3];
         szOrig[0] = (char) wParam;
         szOrig[1] = 0;
         szUnicode[0] = 0;
         MultiByteToWideChar (CP_ACP, 0, szOrig, -1, szUnicode, sizeof(szUnicode)/2);
         p.wCharCode = szUnicode[0];

         if (m_pPage)
            m_pPage->Message ((uMsg == WM_CHAR) ? ESCM_CHAR : ESCM_SYSCHAR, &p);
      }
      return 0;

   case WM_KEYDOWN:
   case WM_SYSKEYDOWN:
      {
         // BUGFIX - For AltGr in hungarian to work shouldn't do anything with this
         if (wParam == VK_PROCESSKEY)
            break;

         ESCMKEYDOWN p;
         p.fEaten = FALSE;
         p.lKeyData = lParam;
         p.nVirtKey = (int) wParam;

         if (m_pPage)
            m_pPage->Message ((uMsg == WM_KEYDOWN) ? ESCM_KEYDOWN : ESCM_SYSKEYDOWN, &p);
      }
      return 0;

   case WM_KEYUP:
   case WM_SYSKEYUP:
      {
         // BUGFIX - For AltGr in hungarian to work shouldn't do anything with this
         if (wParam == VK_PROCESSKEY)
            break;

         ESCMKEYUP p;
         p.fEaten = FALSE;
         p.lKeyData = lParam;
         p.nVirtKey = (int) wParam;

         if (m_pPage)
            m_pPage->Message ((uMsg == WM_KEYUP) ? ESCM_KEYUP : ESCM_SYSKEYUP, &p);
      }
      return 0;

   case WM_ACTIVATE:
      {
         ESCMACTIVATE m;
         m.fActive = LOWORD(wParam);
         m.fMinimized = HIWORD(wParam);
         m.hWndPrevious = (HWND) lParam;

         if (m_pPage)
            m_pPage->Message (ESCM_ACTIVATE, &m);
      }
      break;

   case WM_ACTIVATEAPP:
      {
         ESCMACTIVATEAPP m;
         m.dwThreadID = (DWORD) lParam;
         m.fActive = (BOOL) wParam;

         if (m_pPage)
            m_pPage->Message (ESCM_ACTIVATEAPP, &m);
      }
      break;

   case WM_ENDSESSION:
      {
         ESCMENDSESSION m;
         m.fEndSession = (BOOL) wParam;
         m.fLogOff = lParam;

         if (m_pPage)
            m_pPage->Message (ESCM_ENDSESSION, &m);
      }
      break;

   case WM_QUERYENDSESSION:
      {
         ESCMQUERYENDSESSION m;
         m.fLogOff = lParam;
         m.fTerminate = TRUE;
         m.nSource = (UINT) wParam;

         if (m_pPage)
            m_pPage->Message (ESCM_QUERYENDSESSION, &m);
         return m.fTerminate;
      }
      break;

   case WM_POWERBROADCAST:
      {
         ESCMPOWERBROADCAST m;
         m.dwData = (DWORD)lParam;
         m.dwPowerEvent = (DWORD) wParam;
         m.iRet = TRUE;

         if (m_pPage)
            m_pPage->Message (ESCM_POWERBROADCAST, &m);
         return m.iRet;
      }
      break;

   case WM_MOUSEWHEEL:
      {
         // BUGFIX - if the cursor isn't over the window then ignore
         POINT p;
         RECT r;
         p.x = (short)LOWORD(lParam);
         p.y = (short)HIWORD(lParam);
         GetWindowRect (hWnd, &r);
         if (!PtInRect (&r, p))
            break; // default behaviour

         ESCMMOUSEWHEEL m;
         m.fKeys = LOWORD(wParam);
         m.pPosn.x = (short) LOWORD(lParam);
         m.pPosn.y = (short) HIWORD(lParam);
         m.zDelta = (short) HIWORD(wParam);

         if (m_pPage)
            m_pPage->Message (ESCM_MOUSEWHEEL, &m);
      }
      break;

   }

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/***********************************************************************
EscWndProc - Window procedure for the esc window
*/
LRESULT CALLBACK EscWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   CEscWindow *p = (CEscWindow*) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   CEscWindow *p = (CEscWindow*) (size_t) GetWindowLongPtr (hWnd, GWL_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE:
      {
         CREATESTRUCT *cs = (CREATESTRUCT*) lParam;
#ifdef _WIN64
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) cs->lpCreateParams);
#else
         SetWindowLongPtr (hWnd, GWL_USERDATA, (LONG) (LONG_PTR) cs->lpCreateParams);
#endif
         p = (CEscWindow*) cs->lpCreateParams;
         p->m_hWnd = hWnd;
      }
      break;
   }

   if (p)
      return p->EscWndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/***********************************************************************
RegisterEscWndProc - Registers the EscWndProc class if it isn't already.

inputs
   HINSTANCE   hInstace - hinstance
   DWORD       dwInstance - So that each window can have its own unique icon
*/
void RegisterEscWndProc (HINSTANCE hInstance, DWORD dwInstance)
{
   char  szTemp[256];
   wsprintf (szTemp, gszEscWindowClass, dwInstance);

   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.lpfnWndProc = ::EscWndProc;
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hInstance = hInstance;
   wc.hIcon = LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_DEFAULTICON));
   wc.hCursor = NULL;
   wc.hbrBackground = NULL;
   wc.lpszMenuName = NULL;
   wc.lpszClassName = szTemp;
   RegisterClass (&wc);
}

/***********************************************************************
UnregisterEscWndProc - Registers the EscWndProc class if it isn't already.

inputs
   HINSTANCE   hInstace - hinstance
   DWORD       dwInstance - So that each window can have its own unique icon
*/
void UnregisterEscWndProc (HINSTANCE hInstance, DWORD dwInstance)
{
   char  szTemp[256];
   wsprintf (szTemp, gszEscWindowClass, dwInstance);

   UnregisterClass (szTemp, hInstance);
}

/***********************************************************************
Constructor & destructor
*/
CEscWindow::CEscWindow (void)
{
   m_hWnd = NULL;
   m_hWndParent = NULL;
   m_hInstanceWnd = NULL;
   m_pPage = NULL;
   m_dwTitleStyle = 0;
   m_pszExitCode = NULL;
   m_iExitVScroll = 0;
   m_listESCTIMERID.Init (sizeof(ESCTIMERID));
   m_hbitLastPage = NULL;
   m_pTooltip = NULL;
   m_fIsTimerOn = FALSE;
   m_dwTimerNum = MOUSETIMER + 1;
   m_hMIDI = NULL;
   m_listNOTEOFF.Init (sizeof(NOTEOFF));
   m_dwMIDIOff = 0;
   m_hDC = 0;
   m_dwDCCount = 0;
   m_dwScrollBarRecurse = 0;
   memset (&m_fi, 0, sizeof(m_fi));
   memset (&m_rMouseMove, 0 ,sizeof(m_rMouseMove));

   // clear background
   m_hbmpBackground = 0;
   memset (&m_rBackgroundFrom, 0, sizeof(m_rBackgroundFrom));
   memset (&m_rBackgroundTo, 0, sizeof(m_rBackgroundTo));

   memset (&m_rScreen, 0 ,sizeof(m_rScreen));
   m_rClient = m_rScreen;

   m_fMouseOver = m_fLButtonDown = m_fMButtonDown = m_fRButtonDown = FALSE;
   m_fCapture = FALSE;
   m_pLastMouse.x = m_pLastMouse.y = -1000;
   m_dwLastMouse = GetTickCount();
   m_hHand = NULL;
   //m_hHand = (HCURSOR) LoadImage (ghInstance, MAKEINTRESOURCE(IDC_HANDC), IMAGE_CURSOR,
   //   64, 64, LR_DEFAULTCOLOR);
   m_hHand = (HCURSOR) LoadImage (ghInstance, MAKEINTRESOURCE(IDC_HANDC), IMAGE_CURSOR,
      32, 32, LR_DEFAULTCOLOR);
   m_hNo = (HCURSOR) LoadImage (ghInstance, MAKEINTRESOURCE(IDC_NOC), IMAGE_CURSOR,
      32, 32, LR_DEFAULTCOLOR);
   //m_hHand = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_HANDC));
   m_hCursor = m_hNo;   //LoadCursor (NULL, MAKEINTRESOURCE(IDC_NO));
   memset (&m_rLastPage, 0, sizeof(m_rLastPage));
   m_listInvalid.Init (sizeof(RECT));
   m_dwInstance = 0;

   // printing
   m_hDCPrint = NULL;
   m_fDeleteDCPrint = FALSE;
   memset (&m_PrintInfo, 0, sizeof(m_PrintInfo));
   m_PrintInfo.fColumnLine = m_PrintInfo.fFooterLine = m_PrintInfo.fHeaderLine = m_PrintInfo.fRowLine = TRUE;
   m_PrintInfo.iColumnSepX = m_PrintInfo.iRowSepY = 1440 / 2;
   m_PrintInfo.iFooterSepY = m_PrintInfo.iHeaderSepY = 1440 / 3;
   m_PrintInfo.pszHeader =
      L"<table width=100%% border=0 innerlines=0>"
      L"<tr>"
      L"<td width=50%% align=left><<<PAGETITLE>>></td>"
      L"<td width=50%% align=right>Page <<<PAGE>>></td>"
      L"</tr>"
      L"</table>";
   m_PrintInfo.pszFooter =
      L"<table width=100%% border=0 innerlines=0>"
      L"<tr>"
      L"<td width=50%% align=left><<<TIME>>></td>"
      L"<td width=50%% align=right><<<DATE>>></td>"
      L"</tr>"
      L"</table>";

   m_PrintInfo.rPageMargin.left = m_PrintInfo.rPageMargin.right = 1440 / 2;
   m_PrintInfo.rPageMargin.top = m_PrintInfo.rPageMargin.bottom = 1440 / 2;
      // BUGIX - Smaller margins. top,bottom was 2/3
   m_PrintInfo.wColumns = m_PrintInfo.wRows = 1;
   m_PrintInfo.wFontScale = m_PrintInfo.wOtherScale = 0x100;

   m_fHasPagePainted = FALSE;
}

CEscWindow::~CEscWindow (void)
{
   // if captured release
   MouseCaptureRelease();

   if (m_pTooltip)
      delete m_pTooltip;

   if (m_fIsTimerOn)
      KillTimer (m_hWnd, MOUSETIMER);

   if (m_pPage)
      delete m_pPage;

   if (m_hDC)
      ReleaseDC (m_hWnd, m_hDC);

   if (m_hWnd)
      DestroyWindow (m_hWnd);

   if (m_pszExitCode)
      ESCFREE (m_pszExitCode);

   if (m_hbitLastPage)
      DeleteObject (m_hbitLastPage);

   if (m_hHand)
      DestroyCursor (m_hHand);
   if (m_hNo)
      DestroyCursor (m_hNo);

   if (m_hMIDI)
      MIDIRelease();

   // unregister the clas
   if (m_dwInstance)
      UnregisterEscWndProc (m_hInstanceWnd, m_dwInstance);

   // delete the print DC
   if (m_fDeleteDCPrint && m_hDCPrint)
      DeleteDC (m_hDCPrint);
}


/***********************************************************************
Init - Initializes the window object.

inputs
   HINSTANCE      hInstace - Where to get resource from
   HWND           hParent - Parent window of this. NULL if it's a top-level window.
                           Else it's a pop-up. The default window is a maximized
                           window with a menu, min/max buttons, and resizable
   DWORD          dwStyle - Or the EWS_XXX styles together to get the window
                           style.
   RECT           *pRectSize - Size of the window to create. If NULL use default sizes.
returns
   BOOL - TRUE if succesful
*/
BOOL CEscWindow::Init (HINSTANCE hInstance, HWND hWndParent,
   DWORD dwStyle, RECT *pRectSize)
{
   if (m_hWnd || m_hDCPrint)
      return FALSE;  // already initialized

   if (!gdwInitialized)
      EscInitialize (L"", 0, 0);

   m_hInstanceWnd = hInstance;
   m_hInstancePage = NULL;
   m_hWndParent = hWndParent;
   m_dwStyle = dwStyle;

   // set up defaults for the font
   memset (&m_fi, 0, sizeof(m_fi));
   m_fi.iPointSize = 12;
   wcscpy (m_fi.szFont, L"Arial");
   m_fi.fi.cText = 0; //GetSysColor (COLOR_WINDOWTEXT);
   m_fi.fi.cBack = (DWORD)-1;
   m_fi.fi.iLineSpacePar = -100;
   m_fi.fi.iTab = 96 / 2;

   // window flags
   DWORD dwf;
   // BUGFIX - Option to hide window when start up
   if (dwStyle & EWS_HIDE)
      dwf = 0;
   else
      dwf = WS_VISIBLE;

   if (hWndParent && (dwStyle & EWS_CHILDWINDOW)) {
      dwf |= WS_CHILD;

      if (!(dwStyle & EWS_NOTITLE)) {
         dwf |= WS_CAPTION | WS_SYSMENU;
         if (!(dwStyle & EWS_FIXEDSIZE))
            dwf |= WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
      }

      if (dwStyle & EWS_FIXEDSIZE)
         dwf |= (dwStyle & EWS_CHILDWINDOWHASNOBORDER) ? 0 : WS_BORDER;
      else
         dwf |= WS_SIZEBOX;
   }
   else {
      if (hWndParent)
         dwf |= WS_POPUP;
      else
         dwf |= 0;

      if (dwStyle & EWS_NOTITLE)
         dwf |= WS_POPUP;  // BUGFIX - Was 0, but is have NULL==hWndParent and use 0 then get title
      else
         dwf |= WS_CAPTION | WS_SYSMENU;

      if (dwStyle & EWS_FIXEDSIZE)
         dwf |= WS_BORDER;
      else
         dwf |= WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX;
   }

#if 0
   if (!hWndParent)
      dwf |= WS_MAXIMIZE;
#endif // 0

   if (dwStyle & EWS_NOVSCROLL)
      dwf |= 0;
   else
      dwf |= WS_VSCROLL;

   // don't use CW_USEDEFAULT with a child/popup window
   RECT  rAlt;
   if (!pRectSize && hWndParent) {
      pRectSize = &rAlt;
      rAlt.left = GetSystemMetrics (SM_CXSCREEN) / 4;
      rAlt.right = rAlt.left + GetSystemMetrics (SM_CXSCREEN) / 2;
      rAlt.top = GetSystemMetrics (SM_CYSCREEN) / 4;
      rAlt.bottom = rAlt.top + GetSystemMetrics (SM_CYSCREEN) / 2;

      // if autoheight start out small
      if (dwStyle & EWS_AUTOHEIGHT)
         rAlt.bottom = rAlt.top + 1;

      // if autowidth start out small
      if (dwStyle & EWS_AUTOWIDTH)
         rAlt.right = rAlt.left + 1;
   }

   // create the window
   m_dwInstance = gdwInstance++;
   RegisterEscWndProc (m_hInstanceWnd, m_dwInstance);
   char  szTemp[256];
   wsprintf (szTemp, gszEscWindowClass, m_dwInstance);
   m_hWnd = CreateWindow(  szTemp,  // pointer to registered class name
      "Escarpment", // pointer to window name
      dwf,        // window style
      pRectSize ? pRectSize->left : CW_USEDEFAULT,                // horizontal position of window
      pRectSize ? pRectSize->top : CW_USEDEFAULT,                // vertical position of window
      pRectSize ? (pRectSize->right - pRectSize->left) : CW_USEDEFAULT,           // window width
      pRectSize ? (pRectSize->bottom - pRectSize->top) : CW_USEDEFAULT,          // window height
      hWndParent,      // handle to parent or owner window
      NULL,          // handle to menu or child-window identifier
      m_hInstanceWnd,     // handle to application instance
      (LPVOID) this);        // pointer to window-creation data);
   if (!m_hWnd)
      return FALSE;

   // start the timer
   SetTimer (m_hWnd, MOUSETIMER, TIMERINTERVAL, 0);
   m_fIsTimerOn = TRUE;


  return TRUE;
}

/***********************************************************************
PageDisplay (File) - There's a whole bunch of overloaded page displays with
   different first paraemeters. PageDisplay() creates a CEscPage object
   and initializes it with the given text. Once initialized, it's displayed
   in the MainWindow. PageDisplay assumes that there's a message proc someplace
   to handle the messages. (You might want to use PageDialog() to include
   a message proc.)

inputs
   HINSTANCE   hInstance - Used for getting resources from

   PWSTR    pszPageText - NULL terminated text string
      or
   PSTR     pszPageText - NULL terminated ANSI text string
      or
   DWORD    dwResource - Resource ID for "TEXT" resource containing either unicode or ANSI>
      or
   PWSTR    pszFile - File string
      or
   PCMMLNode   pNode - Already compiled node. This node will be NOT freed automatically
                  by the CEscWindow, so don't count on it being around later
   PESCPAGECALLBACK  pCallback - Callback for page notifications. Basically the same
            as a dialog procedure.
   PVOID       pUserData - appears in pview
returns
   BOOL - TRUE if succesful. If there's an error it may be reported in pCallback.
*/
BOOL CEscWindow::PageDisplay (HINSTANCE hInstance, PWSTR pszPageText, PESCPAGECALLBACK pCallback, PVOID pUserData)
{
   // make sure one's not already opened
   if (!m_hWnd || m_pPage)
      return FALSE;

   m_hInstancePage = hInstance;

   // clear the old returns
   if (m_pszExitCode)
      ESCFREE (m_pszExitCode);
   m_pszExitCode = NULL;

   m_pPage = new CEscPage;
   if (!m_pPage)
      return FALSE;

   m_fHasPagePainted = FALSE;
   if (!m_pPage->Init (pszPageText, pCallback, this, pUserData, &m_fi)) {
      delete m_pPage;
      m_pPage = NULL;
      return FALSE;
   }

   // reset some variables
   m_fMouseOver = m_fLButtonDown = m_fMButtonDown = m_fRButtonDown = FALSE;
   m_pLastMouse.x = m_pLastMouse.y = -1000;
   m_dwLastMouse = GetTickCount();

   return TRUE;
}

BOOL CEscWindow::PageDisplay (HINSTANCE hInstance, PSTR pszPageText, PESCPAGECALLBACK pCallback, PVOID pUserData)
{
   // make sure one's not already opened
   if (!m_hWnd || m_pPage)
      return FALSE;

   m_hInstancePage = hInstance;

   // clear the old returns
   if (m_pszExitCode)
      ESCFREE (m_pszExitCode);
   m_pszExitCode = NULL;

   m_pPage = new CEscPage;
   if (!m_pPage)
      return FALSE;

   m_fHasPagePainted = FALSE;
   if (!m_pPage->Init (pszPageText, pCallback, this, pUserData, &m_fi)) {
      delete m_pPage;
      m_pPage = NULL;
      return FALSE;
   }

   // reset some variables
   m_fMouseOver = m_fLButtonDown = m_fMButtonDown = m_fRButtonDown = FALSE;
   m_pLastMouse.x = m_pLastMouse.y = -1000;
   m_dwLastMouse = GetTickCount();

   return TRUE;
}

BOOL CEscWindow::PageDisplay (HINSTANCE hInstance, DWORD dwResource, PESCPAGECALLBACK pCallback, PVOID pUserData)
{
   // make sure one's not already opened
   if (!m_hWnd || m_pPage)
      return FALSE;

   m_hInstancePage = hInstance;

   // clear the old returns
   if (m_pszExitCode)
      ESCFREE (m_pszExitCode);
   m_pszExitCode = NULL;

   m_pPage = new CEscPage;
   if (!m_pPage)
      return FALSE;

   m_fHasPagePainted = FALSE;
   if (!m_pPage->Init (dwResource, pCallback, this, pUserData, &m_fi)) {
      delete m_pPage;
      m_pPage = NULL;
      return FALSE;
   }

   // reset some variables
   m_fMouseOver = m_fLButtonDown = m_fMButtonDown = m_fRButtonDown = FALSE;
   m_pLastMouse.x = m_pLastMouse.y = -1000;
   m_dwLastMouse = GetTickCount();

   return TRUE;
}

BOOL CEscWindow::PageDisplayFile (HINSTANCE hInstance, PWSTR pszFile, PESCPAGECALLBACK pCallback, PVOID pUserData)
{
   // make sure one's not already opened
   if (!m_hWnd || m_pPage)
      return FALSE;

   m_hInstancePage = hInstance;

   // clear the old returns
   if (m_pszExitCode)
      ESCFREE (m_pszExitCode);
   m_pszExitCode = NULL;

   m_pPage = new CEscPage;
   if (!m_pPage)
      return FALSE;

   m_fHasPagePainted = FALSE;
   if (!m_pPage->Init (pszFile, pCallback, this, pUserData, &m_fi)) {
      delete m_pPage;
      m_pPage = NULL;
      return FALSE;
   }

   // reset some variables
   m_fMouseOver = m_fLButtonDown = m_fMButtonDown = m_fRButtonDown = FALSE;
   m_pLastMouse.x = m_pLastMouse.y = -1000;
   m_dwLastMouse = GetTickCount();

   return TRUE;
}

BOOL CEscWindow::PageDisplay (HINSTANCE hInstance, PCMMLNode pNode, PESCPAGECALLBACK pCallback, PVOID pUserData)
{
   // make sure one's not already opened
   if (!m_hWnd || m_pPage)
      return FALSE;

   m_hInstancePage = hInstance;

   // clear the old returns
   if (m_pszExitCode)
      ESCFREE (m_pszExitCode);
   m_pszExitCode = NULL;

   m_pPage = new CEscPage;
   if (!m_pPage)
      return FALSE;

   m_fHasPagePainted = FALSE;
   if (!m_pPage->Init (pNode, pCallback, this, pUserData, &m_fi)) {
      delete m_pPage;
      m_pPage = NULL;
      return FALSE;
   }

   // reset some variables
   m_fMouseOver = m_fLButtonDown = m_fMButtonDown = m_fRButtonDown = FALSE;
   m_pLastMouse.x = m_pLastMouse.y = -1000;
   m_dwLastMouse = GetTickCount();

   return TRUE;
}

/***********************************************************************
PageClose - Forcefully closes a page (if there is one).

inputs
   noen
returns
   BOOL - TRUE if OK
*/
BOOL CEscWindow::PageClose (void)
{
   if (m_pPage) {
      delete m_pPage;
   }
   m_pPage = NULL;
   return TRUE;
}


/***********************************************************************
DialogLoop - Internal function that does the dialog loop. If there's a parent
window the parent is disabled

returns
   PWSTR - results
*/
PWSTR CEscWindow::DialogLoop (void)
{
   // this will create modal dialog-box functionality
   // disable parent if there is one
   if (m_hWndParent)
      EnableWindow (m_hWndParent, FALSE);

   // messge loop - wait until there's an exit code
   MSG   msg;
   while (!m_pszExitCode && GetMessage (&msg, NULL, 0, 0)) {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
#ifdef _DEBUG
      //_CrtCheckMemory ();
#endif // DEBUG
   }

   // if there are any messages on the queue get rid of them
   while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage (&msg);

      // BUGFIX - Ignore timer messages so that shuts down right away if 3d render control going
      if (msg.message == WM_TIMER)
         continue;

      DispatchMessage (&msg);
   }

   // get rid of the page if its still there
   PageClose ();

   // enable parent if there is one
   if (m_hWndParent)
      EnableWindow (m_hWndParent, TRUE);

   // set the cursor to no-op so people know to wait
   SetCursor (IDC_NOCURSOR);

   // BUGFIX - Show the cursor. Sometimes cursor dissappears if using dialog and
   // press escape. This just makes sure it's visible when exit out of escarpment's
   // message loop
   while (ShowCursor (TRUE) < 1);
   while (ShowCursor (FALSE) > 0);  // don't want a really huge cursor count

   return m_pszExitCode;
}

/***********************************************************************
PageDialog (File) - There's a whole bunch of overloaded page dialogs with
   different first paraemeters. PageDialog() basically calls PageDisplay()
   and then waits in a message loop until the page exits. When it exits
   it writes its return string under m_pszExitCode.

inputs
   PWSTR    pszPageText - NULL terminated text string
      or
   PSTR     pszPageText - NULL terminated ANSI text string
      or
   DWORD    dwResource - Resource ID for "TEXT" resource containing either unicode or ANSI>
      or
   PWSTR    pszFile - File string
      or
   PCMMLNode   pNode - Already compiled node. This node will be freed automatically
                  by the CEscWindow, so don't count on it being around later
   PESCPAGECALLBACK  pCallback - Callback for page notifications. Basically the same
            as a dialog procedure.
returns
   PWSTR - Exit string. NULL if failed. If there's an error it may be reported in pCallback.
            This is valid until the CEscWindow is destroyed or the next pagedialog
*/
PWSTR CEscWindow::PageDialog (HINSTANCE hInstance, PWSTR pszPageText, PESCPAGECALLBACK pCallback, PVOID pUserData)
{
   if (!PageDisplay (hInstance, pszPageText, pCallback, pUserData))
      return NULL;

   return DialogLoop ();
}

PWSTR CEscWindow::PageDialog (HINSTANCE hInstance, PSTR pszPageText, PESCPAGECALLBACK pCallback, PVOID pUserData)
{
   if (!PageDisplay (hInstance, pszPageText, pCallback, pUserData))
      return NULL;

   return DialogLoop ();
}

PWSTR CEscWindow::PageDialog (HINSTANCE hInstance, DWORD dwResource, PESCPAGECALLBACK pCallback, PVOID pUserData)
{
   if (!PageDisplay (hInstance, dwResource, pCallback, pUserData))
      return NULL;

   return DialogLoop ();
}

PWSTR CEscWindow::PageDialogFile (HINSTANCE hInstance, PWSTR pszFile, PESCPAGECALLBACK pCallback, PVOID pUserData)
{
   if (!PageDisplay (hInstance, pszFile, pCallback, pUserData))
      return NULL;

   return DialogLoop ();
}

PWSTR CEscWindow::PageDialog (HINSTANCE hInstance, PCMMLNode pNode, PESCPAGECALLBACK pCallback, PVOID pUserData)
{
   if (!PageDisplay (hInstance, pNode, pCallback, pUserData))
      return NULL;

   return DialogLoop ();
}

/***********************************************************************
Page callback
*/
BOOL HoverHelpCallback (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // add the acclerators
      ESCACCELERATOR a;
      memset (&a, 0, sizeof(a));
      a.c = VK_ESCAPE;
      a.dwMessage = ESCM_CLOSE;
      pPage->m_listESCACCELERATOR.Add (&a);

      DropDownFitToScreen(pPage);
      return TRUE;
   }

   return FALSE;
}

/***********************************************************************
HoverHelp - Shows the help tooltip at/under the prControlr ect.

THis:
   1) Create a CEscWindow that is a child window of the current, located near
         the control, and which disappears if the mouse moves. Unless
         fKeyActivated, then disappears on escape
   2) Use pNode to create a new PageDialog.(or PageDisplay?)

NOTE: If the pNode->AttribGet("hresize") is found as TRUE then the horizontal
   size will be shrunk too.

inputs
   PCMMLNode   pNode - node. This will be copied by the HoverHelp call.
   BOOL        fKeyActivated - If TRUE, window only closes with escape. If FALSE,
                  closes when mouse moves.
   POINT       *pUL - Upper-left corner of hover help
returns
   BOOL - TRUE if OK
*/
BOOL CEscWindow::HoverHelp (HINSTANCE hInstance, PCMMLNode pNode, BOOL fKeyActivated, POINT *pUL)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   CEscWindow  cWindow;

   // beep when enter
   Beep (ESCBEEP_MENUOPEN);

   // if move over then create a window underneath using the menucontents
   // subnode

   // size?
   RECT  rSize;
   rSize.left = pUL->x;
   rSize.top = pUL->y;
   rSize.right = rSize.left + GetSystemMetrics(SM_CXSCREEN) / 3;
   rSize.bottom = rSize.top + 1; // so grows in size

   DWORD dwStyle;
   dwStyle = EWS_NOTITLE | EWS_FIXEDSIZE | EWS_NOVSCROLL | EWS_AUTOHEIGHT;

   // if open by the mouse, close by the mouse
   if (!fKeyActivated)
      dwStyle |= EWS_CLOSEMOUSEMOVE;

   // if hresize then autowidth
   BOOL  fVal = FALSE;
   if (AttribToYesNo (pNode->AttribGet(L"HResize"), &fVal)) {
      if (fVal) {
         dwStyle |= EWS_AUTOWIDTH;
         rSize.right = rSize.left + 1;
      }
   }

   // chime
   EscChime (ESCCHIME_HOVERFHELP);

   // BUGFIX - Don't speak for hover help because it's too annoying
#if 0
   // speak the help text
   if (EscSpeakTTS()) {
      CMem  mem;
      EscTextFromNode (pNode, &mem);
      mem.CharCat (0);

      EscSpeak ((PWSTR) mem.p);
   }
#endif // 0

   if (!cWindow.Init(m_hInstanceWnd, m_hWnd,
      dwStyle, &rSize))
      return TRUE;

   // bring up the window
   PCMMLNode   pMenu;
   pMenu = pNode->Clone();
   if (!pMenu)
      return 0;

   // set the menu to type <main>
   //pMenu->NameSet (L"<Main>");
   pMenu->NameSet (L"NULL");
   cWindow.PageDialog (hInstance, pMenu, HoverHelpCallback, fKeyActivated ? (PVOID) 1 : 0);
   // BUGFIX - PageDialog no longer deleting pNode
   delete pMenu;  // since page dialog didn't delete it

   // hide the window so it's not visible is a call is made from this
   cWindow.ShowWindow (SW_HIDE);

   // close sound
   Beep (ESCBEEP_MENUCLOSE);

   return TRUE;
}

/***********************************************************************
Move - Moves the window so it's upper left corner is at the given XY position.
   In the process this invalidates the CEscPage so it's redrawn.

inputs
   int   iX, iY - XY
return
   BOOL - TRUE if OK
*/
BOOL CEscWindow::Move (int iX, int iY)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   RECT  r;
   PosnGet (&r);
   r.right = r.right - r.left + iX;
   r.bottom = r.bottom - r.top + iY;
   r.left = iX;
   r.top = iY;
   return PosnSet (&r);
}

/***********************************************************************
Size - Changes the size of the window. This in turn tells CEscPage it's size
   has changed, so it will recalc word-wrap.

niputs
   int   iX, iY - width & height
returns
   BOOL - TRUE if OK
*/
BOOL CEscWindow::Size (int iX, int iY)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   RECT  r;
   PosnGet (&r);
   r.right = r.left + iX;
   r.bottom = r.top + iY;
   return PosnSet (&r);
}

/***********************************************************************
Center - Moves the window so its centered in the screen.

inputis
   none
returns
   BOOL - TRUE if OK
*/
BOOL CEscWindow::Center (void)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   RECT r, rWork;

   GetWindowRect (m_hWnd, &r);
   SystemParametersInfo (SPI_GETWORKAREA, 0, &rWork, 0);

   int   cx, cy;
   cx = (rWork.right - rWork.left) / 2 + rWork.left;
   cy = (rWork.bottom - rWork.top) / 2 + rWork.top;
   MoveWindow (m_hWnd, cx - (r.right-r.left)/2, cy - (r.bottom-r.top)/2, r.right-r.left, r.bottom-r.top, TRUE);

   return TRUE;
}

/***********************************************************************
ShowWindow - Shows/hides the window.

inputs
   int   nCmdShow - See Windows API ShowWindow
returns
   BOOL - TRUE if succes
*/
BOOL CEscWindow::ShowWindow (int nCmdShow)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   ::ShowWindow (m_hWnd, nCmdShow);
   return TRUE;
}

/***********************************************************************
PosnGet - Returns the current bounding rectangle for the window (in screen
   coordinates.)

inputs
   RECT  *pr - filled in
returns
   BOOL - TRUE if OK
*/
BOOL CEscWindow::PosnGet (RECT *pr)
{
   *pr = m_rScreen;
   return TRUE;
}

/***********************************************************************
PosnSet - Basically does a size and then move.

inputs
   RECT  *pr - rectangle
returns
   BOOL - TRUE if OK
*/
BOOL CEscWindow::PosnSet (RECT *pr)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   if (!m_hWnd)
      return FALSE;

   // BUGFIX - If it's the same as the old window location then movewindow won't actually
   // do anything, but this may be a problem with refresh
   RECT rClient, rScreen;
   HWND hParent = GetParent (m_hWnd);
   GetWindowRect (m_hWnd, &rScreen);
   rClient = rScreen;
   if (hParent) {
      ScreenToClient (hParent, (LPPOINT)&rClient.left);
      ScreenToClient (hParent, (LPPOINT)&rClient.right);
   }

   if ((rClient.left == pr->left) && (rClient.right == pr->right) && (rClient.top == pr->top) && (rClient.bottom == pr->bottom)) {
      // theoretically hasn't moved, but may wish to recalc some stuff


      // if the screen location is also the same then no change
      if ((rScreen.left == m_rScreen.left) && (rScreen.right == m_rScreen.right) && (rScreen.top == m_rScreen.top) && (rScreen.bottom == m_rScreen.bottom))
         return TRUE;

      // else, send a message so reacts to this
      SendMessage (m_hWnd, WM_MOVE, 0, MAKELPARAM(pr->left, pr->top));

      return TRUE;
   }

   return MoveWindow (m_hWnd, pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top, TRUE);
}


/***********************************************************************
TitleGet - Gets the window title.
BUGBUG - 2.0 - right now just using standard WIndows API titles, but eventually
need spiffy ones.

inputs
   WCHAR    *psz - filled in with title
   DWORD    dwSize - number of bytes available
returns
   BOOL - TRUE if success, FALSE if error (not large enough)
*/
BOOL CEscWindow::TitleGet (WCHAR *psz, DWORD dwSize)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   if (!m_hWnd)
      return FALSE;

   char  szTemp[512];
   GetWindowText (m_hWnd, szTemp, sizeof(szTemp));
   return MultiByteToWideChar (CP_ACP, 0, szTemp, -1, psz, dwSize / 2) ? TRUE : FALSE;
}


/***********************************************************************
TitleSet - Sets the window title.

inputs
   WCHAR    *psz - title
returns
   BOOL - TRUE if OK
*/
BOOL CEscWindow::TitleSet (WCHAR *psz)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   if (!m_hWnd)
      return FALSE;

   char  szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);
   SetWindowText (m_hWnd, szTemp);
   return TRUE;
}

/***********************************************************************
TimerSet - Creates a new timer (using windows API TimerSet and a unique ID).

inputs
   DWORD dwTime - milliseconds
   PCEscControl pControl - Control to send ESCM_TIMER message to
      or
   PCEscPage pPage - Page to send ESCM_TIMER message to
   DWORD dwMessage - Message to send
returns
   DWORD - new timer ID (for freeing). 0 if error
*/
DWORD CEscWindow::TimerSet (DWORD dwTime, PCEscControl pControl, DWORD dwMessage)
{
   DWORD dwID;
   dwID = m_dwTimerNum++;
   if (!SetTimer (m_hWnd, dwID, dwTime, NULL))
      return 0;

   ESCTIMERID t;
   memset (&t, 0, sizeof(t));
   t.dwID = dwID;
   t.dwMessage = dwMessage;
   t.pControl = pControl;
   m_listESCTIMERID.Add (&t);

   return dwID;
}

DWORD CEscWindow::TimerSet (DWORD dwTime, PCEscPage pPage, DWORD dwMessage)
{
   DWORD dwID;
   dwID = m_dwTimerNum++;
   if (!SetTimer (m_hWnd, dwID, dwTime, NULL))
      return 0;

   ESCTIMERID t;
   memset (&t, 0, sizeof(t));
   t.dwID = dwID;
   t.dwMessage = dwMessage;
   t.pPage = pPage;
   m_listESCTIMERID.Add (&t);

   return dwID;
}

/***********************************************************************
TimerKill - Kills a timer.

inputs
   DWORD dwID - returned from TimerSet
returns
   BOOL - TRUE if OK
*/
BOOL CEscWindow::TimerKill (DWORD dwID)
{
   // find the timer
   DWORD i;
   for (i = 0; i < m_listESCTIMERID.Num(); i++) {
      PESCTIMERID p = (PESCTIMERID) m_listESCTIMERID.Get(i);
      if (p->dwID != dwID)
         continue;

      // found so delete
      KillTimer (m_hWnd, dwID);
      m_listESCTIMERID.Remove (i);
      return TRUE;
   }

   return FALSE;  // couldn't find
}


/*************************************************************************
SetCursor - Sets the currently used cursor

inputs
   HCURSOR - hCURSOR
      or
   DWORD dwID - Cursor ID from LoadCursor (NULL, XXX)
reutrns
   BOOL - TRUE if OK
*/
BOOL CEscWindow::SetCursor (HCURSOR hCursor)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   if (m_hCursor == hCursor)
      return TRUE;

   ::SetCursor (hCursor);
   m_hCursor = hCursor;

   return TRUE;
}

BOOL CEscWindow::SetCursor (DWORD dwID)
{
   switch (dwID) {
   case IDC_HANDCURSOR:
      if (gfUseTraiditionalCursor)
         return SetCursor((HCURSOR) LoadCursor(NULL, IDC_ARROW));
      else
         return SetCursor (m_hHand);
   case IDC_NOCURSOR:
      if (gfUseTraiditionalCursor)
         return SetCursor((HCURSOR) LoadCursor(NULL, IDC_NO));
      else
         return SetCursor (m_hNo);
   default:
      return SetCursor((HCURSOR) LoadCursor(NULL, MAKEINTRESOURCE(dwID)));
   }

}


/*************************************************************************
IconSet - Sets the icon for the window

inputs
   HICON hIcon
returns
   BOOL - TRUE if success
*/
BOOL CEscWindow::IconSet (HICON hIcon)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   if (!m_hWnd)
      return FALSE;

#ifdef _WIN64
   SetClassLongPtr (m_hWnd, GCLP_HICON, (LONG_PTR) hIcon);
#else
   SetClassLong (m_hWnd, GCL_HICON, (LONG) (LONG_PTR) hIcon);
#endif

   return TRUE;
}

/**************************************************************************
ScrollBar - Called by the view window telling the main window it should update
its scroll bar.

BUGBUG - 2.0 - in the future will use own scroll bar, so this changes

inputs
   int      iPos - Vertical scroll position (in pixels)
   int      iHeight - Height of display area (in pixels)
   int      iMax - Maximum height of page data (in pixels)
returns
   none
*/
void CEscWindow::ScrollBar (int iPos, int iHeight, int iMax)
{
   // cant do this if printing
   if (m_hDCPrint)
      return;

#if 0
   char  szTemp[256];
   wsprintf (szTemp, "ScrollBar = %d, %d, %d\r\n", iPos, iHeight, iMax);
   OutputDebugString (szTemp);
#endif
   // if style is not to have scroll bar then ignore
   if (m_dwStyle & EWS_NOVSCROLL)
      return;

   // figure out what think the scroll bar should be
   SCROLLINFO  si;
   memset (&si, 0, sizeof(si));
   si.cbSize = sizeof(si);
   si.fMask = SIF_ALL;
   si.nMin = 0;
   si.nMax = iMax-1;
   si.nPage = iHeight;
   si.nPos = iPos;

   // now, see what it thinks it is
   SCROLLINFO  sic;
   memset (&sic, 0, sizeof(sic));
   sic.cbSize = sizeof(sic);
   sic.fMask = SIF_ALL;
   GetScrollInfo (m_hWnd, SB_VERT, &sic);

   // if they're the same then do nothing
   if ( (si.nMin == sic.nMin) && (si.nMax == sic.nMax) &&
      (si.nPage == sic.nPage) && (si.nPos == sic.nPos))
      return;

   // BUGFIX - If we're in a big loop of resizing the window because having a scroll
   // bar on the sied makes it too long, or not having it makes it too short, then
   // just get out while can
#ifdef _DEBUG
   if (m_dwScrollBarRecurse) {
      char  szTemp[128];
      sprintf (szTemp, "m_dwScrollBarRecurse=%d\r\n", (int)m_dwScrollBarRecurse);
      OutputDebugString (szTemp);
   }
#endif
   if (m_dwScrollBarRecurse == 4)
      si.fMask |= SIF_DISABLENOSCROLL;
   else if (m_dwScrollBarRecurse > 4)
      return;
   else
      EnableScrollBar (m_hWnd, SB_VERT, ESB_ENABLE_BOTH);

   // else change it
   m_dwScrollBarRecurse++;
   SetScrollInfo (m_hWnd, SB_VERT, &si, TRUE);
   m_dwScrollBarRecurse--;

}


/***************************************************************************
MouseCaptureRelease - A page calls this to release its capture.

inputs
   none
returns
   BOOL - TRUE if successful
*/
BOOL CEscWindow::MouseCaptureRelease (void)
{
   if (!m_fCapture)
      return FALSE;

   m_fCapture = FALSE;
   m_fLButtonDown = m_fMButtonDown = m_fRButtonDown = FALSE;   // because don't know anymore

   // let the timer determine if the mouse is no longer over

   return ReleaseCapture();
}

/***************************************************************************
Beep - Plays a beep. See ESCBEEP_XXX for some beeps.

inputs
   DWORD    dwSound - ESCBEEP_XXX
   DWORD    dwDuration - duration in ms
returns
   BOOL - TRUE if OK
*/
BOOL CEscWindow::Beep (DWORD dwSound, DWORD dwDuration)
{
   // cant do this if printing
   if (m_hDCPrint)
      return FALSE;

   if (!(gdwSounds & ESCS_CLICKS))
      return TRUE;

   // turn midi on if not
   if (!m_hMIDI) {
      m_hMIDI = MIDIClaim();
      if (!m_hMIDI)
         return FALSE;
      m_dwMIDIOff = 0;
   }

   // how long till shut midi off
   m_dwMIDIOff = max(m_dwMIDIOff, (dwDuration + 30000) / TIMERINTERVAL);

   BYTE  bChannel = 0x09;
   BYTE  bNote = (BYTE) dwSound;
   BYTE  bVolume = 0x30;
   DWORD dw;

   // message to send for note on
   // 0x9f - for channel down, bNote - for note, 0x7f for volume
   dw = (0x90 | bChannel) | (((DWORD) bNote) << 8) | (((DWORD) bVolume) << 16);
   if (MIDIShortMsg (m_hMIDI, dw))
      return FALSE;

   // to turn off same, except 8f
   dw = (0x80 | bChannel) | (((DWORD) bNote) << 8) | (((DWORD) bVolume) << 16);
   NOTEOFF  no;
   no.dwCount = max(dwDuration / TIMERINTERVAL,1);
   no.dwMIDI = dw;
   m_listNOTEOFF.Add (&no);

   return TRUE;

}


/***************************************************************************
DCGet - Returns the HDC of the display area. Controls/views should call this
rather than using m_hWnd to get the DC becuase if the window is used for
printing then this will be the DC to the printer. Remember, call DCRelease()
once per DCGet(), and call DCRelease() before returning from the function.

  NOTE: Do not paint to this DC. Only use it for getting information, such as
  font sizes, etc. Only paint when have a ESCM_PAINT.

inputs
   none
returns
   HDC - hDC
*/
HDC CEscWindow::DCGet (void)
{
   // if printing use that
   if (m_hDCPrint)
      return m_hDCPrint;

   if (!m_hDC) {
      m_hDC = GetDC (m_hWnd);
      m_dwDCCount = 0;
   }

   m_dwDCCount++;
   return m_hDC;
}


/***************************************************************************
DCRelease - Frees the DC. Actually reduces a counter, and when it goes to 0 then
frees.
*/
void CEscWindow::DCRelease()
{
   // if printint this is a NO-OP
   if (m_hDCPrint)
      return;

   m_dwDCCount--;
   if (!m_dwDCCount) {
      ReleaseDC (m_hWnd, m_hDC);
      m_hDC =  NULL;
   }
}


/***************************************************************************
ScrollMe - Called by the page to tell the main window that the user has moved
the scroll bar, or whatever (VScroll called) and that if there's a bitmap
stored of the page it should be scrolled.

inputs
   none
returns
   none
*/
void CEscWindow::ScrollMe (void)
{
   // cant do this if printing
   if (m_hDCPrint)
      return;

   RECT  rOld, rNew;
   rOld = m_rLastPage;
   rNew = m_pPage->m_rVisible;

   // if the rectangles are the same then do nothing
   if (!memcmp(&rNew, &rOld, sizeof(RECT)))
      return;

   // BUGFIX - If this has a background bitmap then invalidate it all
   if (m_hbmpBackground) {
      if (m_hbitLastPage)
         DeleteObject (m_hbitLastPage);
      m_hbitLastPage = NULL;
   }

   // cause us to panit
   InvalidateRect (m_hWnd, NULL, FALSE);

   // if there's no bitmap cached then wipe that out too
   if (m_hbitLastPage && m_fHasPagePainted) {
      // move. First generate new bitmap
      HDC hDC, hDCSrc, hDCDest;
      HBITMAP  hNew;
      hDC = DCGet ();
      if (!hDC)
         return;
      hNew = CreateCompatibleBitmap (hDC, rNew.right - rNew.left,
         rNew.bottom - rNew.top);
      if (!hNew) {
         DCRelease ();
         return;
      }
      hDCSrc = CreateCompatibleDC (hDC);
      if (!hDCSrc) {
         DeleteObject (hNew);
         DCRelease ();
         return;
      }
      hDCDest = CreateCompatibleDC (hDC);
      if (!hDCDest) {
         DeleteDC (hDCSrc);
         DeleteObject (hNew);
         DCRelease ();
         return;
      }
      DCRelease ();

      // select the bitmpas in
      SelectObject (hDCSrc, m_hbitLastPage);
      SelectObject (hDCDest, hNew);

      // bitblt
      BitBlt (hDCDest,
         rOld.left - rNew.left,
         rOld.top - rNew.top,
         rOld.right - rOld.left, rOld.bottom - rOld.top,
         hDCSrc,
         0, 0, SRCCOPY);

      // done
      DeleteDC (hDCSrc);
      DeleteDC (hDCDest);
      DeleteObject (m_hbitLastPage);
      m_hbitLastPage = hNew;
      m_rLastPage = rNew;
   }


   // BUGFIX - in overview.mml, when press the "macros" link, it seems like the
   // screen is only half refreshed. All other jumps to the middle of the document
   // seem to be OK
   // if we havne't painted then delete the old bitmap
   if (!m_fHasPagePainted) {
      if (m_hbitLastPage)
         DeleteObject (m_hbitLastPage);
      m_hbitLastPage = NULL;
   }

   // figure out what part of the page is now visible and tell it that
   // it has something invalid
   RECT  rInt;
   if (IntersectRect (&rInt, &rOld, &rNew)) {
      RECT  r;

      if (rInt.left > rNew.left) {
         // image to the left needs represhing
         r = rNew;
         r.right = rInt.left;
         m_pPage->Invalidate (&r);
      }
      if (rInt.right < rNew.right) {
         // image to the right needs represhing
         r = rNew;
         r.left = rInt.right;
         m_pPage->Invalidate (&r);
      }
      if (rInt.top > rNew.top) {
         // image to the top needs represhing
         r = rNew;
         r.bottom = rInt.top;
         m_pPage->Invalidate (&r);
      }
      if (rInt.bottom < rNew.bottom) {
         // image to the bottom needs represhing
         r = rNew;
         r.top = rInt.bottom;
         m_pPage->Invalidate (&r);
      }
   }
   else {
      // nothing intersected so invalidate the whole page
      m_pPage->Invalidate();
   }
}



/***************************************************************************
InitForPrint - Initialize the CEscWindow object so that it can be used
for printing. If it's initialized for printing it cannot be used for
displaying.

inputs
   HDC            hDCPrint - Print HDC. If NULL, this uses the default printer
   HINSTANCE      hInstace - Where to get resource from
   HWND           hParent - Used to bring dialogs on top of
returns
   BOOL - TRUE if succesful

*/
BOOL CEscWindow::InitForPrint (HDC hDCPrint, HINSTANCE hInstance, HWND hWndParent)
{
   if (m_hWnd || m_hDCPrint)
      return FALSE;  // already initialized

   if (!gdwInitialized)
      return FALSE;

   m_hInstanceWnd = m_hInstancePage = hInstance;
   m_hWndParent = hWndParent;
   m_dwStyle = 0;
   m_hDCPrint = hDCPrint;
   m_fDeleteDCPrint = hDCPrint ? FALSE : TRUE;

   // if no print DC then make one up and set flag that need to delete
   if (!hDCPrint) {
#if 0
      // print dialog box
      PRINTDLG pd;
      memset (&pd, 0, sizeof(pd));
      pd.lStructSize = sizeof(pd);
      pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
      pd.hwndOwner = hWndParent;
      pd.nFromPage = 0;
      pd.nToPage = 0;
      pd.nCopies = 1;

      if (!PrintDlg (&pd))
         return FALSE;  // cancelled or error
      m_hDCPrint = pd.hDC;
#endif // 0

      // use the default printer
      PRINTER_INFO_1 *ppi;
      BYTE  abHuge[10000];
      DWORD dwNeeded, dwCount;
      ppi = (PRINTER_INFO_1*) abHuge;
      if (!EnumPrinters (PRINTER_ENUM_DEFAULT, NULL, 1, abHuge, sizeof(abHuge), &dwNeeded, &dwCount))
         return FALSE;
      if (!dwCount)
         return FALSE;
 
      m_hDCPrint = CreateDC (NULL, ppi->pName, NULL, NULL);
      if (!m_hDCPrint)
         return FALSE;
   }

   // set up defaults for the font
   memset (&m_fi, 0, sizeof(m_fi));
   m_fi.iPointSize = 12;
   wcscpy (m_fi.szFont, L"Arial");
   m_fi.fi.cText = 0; // GetSysColor (COLOR_WINDOWTEXT);
   m_fi.fi.cBack = (DWORD)-1;
   m_fi.fi.iLineSpacePar = -100;
   m_fi.fi.iTab = 96 / 2;

   return TRUE;
}


/***************************************************************************
PrintPageLoad - Loads in a page for the purpose of printing. You must have
called InitForPrint() before calling this.

inputs
   PWSTR    pszPageText - NULL terminated text string
      or
   PSTR     pszPageText - NULL terminated ANSI text string
      or
   DWORD    dwResource - Resource ID for "TEXT" resource containing either unicode or ANSI>
      or
   PWSTR    pszFile - File string
      or
   PCMMLNode   pNode - Already compiled node. This node will be freed automatically
                  by the CEscWindow, so don't count on it being around later
   PESCPAGECALLBACK  pCallback - Callback for page notifications. Basically the same
            as a dialog procedure.
   PVOID       pUserData - appears in pview
   int      iWidth - Width of column in hDCPrint coordinates. If 0 (default) then it's
                  calculated from the page info. Used by MML pages for word wrap.
                  This remains constant for the duration of the page being open.
   int      iHeight - Height of row in hDCPrint coordinates. If 0 (default) then it's
                  calculated from the page ingo. Used by MML pages that stretch vertically
                  so they're as tall as the "page"

returns
   BOOL - TRUE if succesful. If there's an error it may be reported in pCallback.

*/
BOOL CEscWindow::PrintPageLoad (HINSTANCE hInstance, DWORD dwResource,
                                PESCPAGECALLBACK pCallback, PVOID pUserData,
                                int iWidth, int iHeight)
{
   // main func
   if (!PrintPageLoad2 (hInstance, pCallback, pUserData, iWidth, iHeight))
      return FALSE;

   // temporarily change font scale
   WORD  wOldScale;
   wOldScale = EscFontScaleGet ();
   EscFontScaleSet (m_PrintInfo.wFontScale);

   if (!m_pPage->Init (dwResource, pCallback, this, pUserData, &m_fi)) {
      EscFontScaleSet (wOldScale);
      delete m_pPage;
      m_pPage = NULL;
      return FALSE;
   }
   EscFontScaleSet (wOldScale);

   return TRUE;
}

BOOL CEscWindow::PrintPageLoad (HINSTANCE hInstance, PWSTR psz,
                                PESCPAGECALLBACK pCallback, PVOID pUserData,
                                int iWidth, int iHeight)
{
   // main func
   if (!PrintPageLoad2 (hInstance, pCallback, pUserData, iWidth, iHeight))
      return FALSE;

   // temporarily change font scale
   WORD  wOldScale;
   wOldScale = EscFontScaleGet ();
   EscFontScaleSet (m_PrintInfo.wFontScale);

   if (!m_pPage->Init (psz, pCallback, this, pUserData, &m_fi)) {
      EscFontScaleSet (wOldScale);
      delete m_pPage;
      m_pPage = NULL;
      return FALSE;
   }
   EscFontScaleSet (wOldScale);

   return TRUE;
}

BOOL CEscWindow::PrintPageLoad (HINSTANCE hInstance, char *psz,
                                PESCPAGECALLBACK pCallback, PVOID pUserData,
                                int iWidth, int iHeight)
{
   // main func
   if (!PrintPageLoad2 (hInstance, pCallback, pUserData, iWidth, iHeight))
      return FALSE;

   // temporarily change font scale
   WORD  wOldScale;
   wOldScale = EscFontScaleGet ();
   EscFontScaleSet (m_PrintInfo.wFontScale);

   if (!m_pPage->Init (psz, pCallback, this, pUserData, &m_fi)) {
      EscFontScaleSet (wOldScale);
      delete m_pPage;
      m_pPage = NULL;
      return FALSE;
   }
   EscFontScaleSet (wOldScale);

   return TRUE;
}

BOOL CEscWindow::PrintPageLoadFile (HINSTANCE hInstance, PWSTR pszFile,
                                PESCPAGECALLBACK pCallback, PVOID pUserData,
                                int iWidth, int iHeight)
{
   // main func
   if (!PrintPageLoad2 (hInstance, pCallback, pUserData, iWidth, iHeight))
      return FALSE;

   // temporarily change font scale
   WORD  wOldScale;
   wOldScale = EscFontScaleGet ();
   EscFontScaleSet (m_PrintInfo.wFontScale);

   if (!m_pPage->InitFile (pszFile, pCallback, this, pUserData, &m_fi)) {
      EscFontScaleSet (wOldScale);
      delete m_pPage;
      m_pPage = NULL;
      return FALSE;
   }
   EscFontScaleSet (wOldScale);

   return TRUE;
}

BOOL CEscWindow::PrintPageLoad2 (HINSTANCE hInstance, PESCPAGECALLBACK pCallback, PVOID pUserData,
                                int iWidth, int iHeight)
{
   // make sure one's not already opened
   if (!m_hDCPrint)
      return FALSE;

   if (m_pPage) {
      delete m_pPage;
      m_pPage = NULL;
   }

   m_hInstancePage = hInstance;

   // clear the old returns
   if (m_pszExitCode)
      ESCFREE (m_pszExitCode);
   m_pszExitCode = NULL;

   m_pPage = new CEscPage;
   if (!m_pPage)
      return FALSE;

   // set m_rClient based on iWidth & iHeight. Might have to make something up
   if (!iWidth || !iHeight) {
      // get the info
      ESCPRINTINFO   pi;
      ESCPRINTINFO2  pi2;
      if (!PrintSetMapMode())
         return FALSE;
      PrintRealInfo (&pi, &pi2);

      if (!iWidth)
         iWidth = pi2.iColumnWidth;
      if (!iHeight)
         iHeight = pi2.iRowHeight;
   }
   m_rClient.left = m_rClient.top = 0;
   m_rClient.right = iWidth;
   m_rClient.bottom = iHeight;

   return TRUE;
}
/***************************************************************************
PrintPaint - This causes the page to paint to a printer. Before calling
this the application must have:
   - Called InitForPrint
   - Call SetICMMode (m_hDCPrint, ICM_ON); a appropriate
   - Call SetMapMode() to adjust scaling of fonts and pixels
   - Called PrintPageLoad - Make sure to specify width & height
   - Called StartDoc
   - Called StartPage
   - Call SetICMMode and SetMapMode after every StartPage.

This is a low level API. Most applications will not use it since the functionality
is encompassed in CEscWindow::Print().

NOTE: You can figure out how much should be displayed by:
   a) Determine the # of vertical hDCPrint units to optimally display
         Use m_pPage->m_rTotal to determine the total area needed for printing
   b) Determine a minimum range that's acceptable in order not to break text,
      such as (a) * 0.95
   c) Call m_pPage->m_TextBlock.FindBreak(), and use that.


inputs
   RECT     *pPageCoord - Rectangle on the page to print.
   RECT     *pHDCCoord - Same size rectangle, in HDC coordinates
   RECT     *pScreenCoord - Used for 3D controls so they have the proper
            perspective. Use the rectangle for the entire page
returns
   BOOL - TRUE if successful
*/
BOOL CEscWindow::PrintPaint (RECT *pPageCoord, RECT *pHDCCoord, RECT *pScreenCoord)
{
   if (!m_hDCPrint || !m_pPage)
      return FALSE;

   // paint
   return m_pPage->Paint (pPageCoord, pHDCCoord, pScreenCoord, m_hDCPrint);
}


/***************************************************************************
PrintCalcPages - Calculate the number of pages needed to print this document.

NOTE: THis is optional. You don't have to call it.

Before calling PrintCalcPages you must:
   - Call InitForPrint
   - Set up the m_PrintInfo as desired
   - Call PrintPageLoad

returns
   DWORD - number of pages
*/
DWORD CEscWindow::PrintCalcPages (void)
{
   DWORD dwPages;

   // print nothing
   Print (NULL, 1, 1000000, ESCPRINT_EVENONLY | ESCPRINT_ODDONLY, &dwPages);

   return dwPages;
}

/***************************************************************************
PrintRealInfo - Takes the m_PrintInfo strucutre, copies it to pi, and then
scales all the widths & heights so they're in printer coordinates. Make sure
to have SetMapMode() set as appropriate

inputs
   PESCPRINTINFO     pi - filled in
   PESCPRINTINFO2    pi2 - derived info. Filled in
*/
void CEscWindow::PrintRealInfo (PESCPRINTINFO pi, PESCPRINTINFO2 pi2)
{
   // pixels per inch
   int   iXPerInch, iYPerInch;
   iXPerInch = EscGetDeviceCaps (m_hDCPrint, LOGPIXELSX);
   iYPerInch = EscGetDeviceCaps (m_hDCPrint, LOGPIXELSY);

#define  TWIPS    1440.0

   // convert print into to logical units
   *pi = m_PrintInfo;
   pi->iColumnSepX = (int) ((double)pi->iColumnSepX / TWIPS * iXPerInch);
   pi->iRowSepY = (int) ((double)pi->iRowSepY / TWIPS * iYPerInch);
   pi->iFooterSepY = (int) ((double)pi->iFooterSepY / TWIPS * iYPerInch);
   pi->iHeaderSepY = (int) ((double)pi->iHeaderSepY / TWIPS * iYPerInch);
   pi->rPageMargin.left = (int) ((double)pi->rPageMargin.left / TWIPS * iXPerInch);
   pi->rPageMargin.right = (int) ((double)pi->rPageMargin.right / TWIPS * iXPerInch);
   pi->rPageMargin.top = (int) ((double)pi->rPageMargin.top / TWIPS * iYPerInch);
   pi->rPageMargin.bottom = (int) ((double)pi->rPageMargin.bottom / TWIPS * iYPerInch);

   // some other values
   pi2->rPage.left = pi2->rPage.top = 0;
   pi2->rPage.right = EscGetDeviceCaps (m_hDCPrint, PHYSICALWIDTH);
   pi2->rPage.bottom = EscGetDeviceCaps (m_hDCPrint, PHYSICALHEIGHT);
   pi2->iColumnWidth = (pi2->rPage.right - pi->rPageMargin.left - pi->rPageMargin.right -
      ((int) pi->wColumns - 1) * pi->iColumnSepX) / (int) pi->wColumns;

   // esimtate column height so that those pages that stretch themselves will fit
   pi2->iHeaderHeight = pi2->iFooterHeight = 0;
   // actually calculate header/footer
   if (pi->pszHeader) {
      CEscTextBlock  tb;
      PCMMLNode   pNode;
      tb.Init ();
      pNode = ParseMML (pi->pszHeader, m_hInstancePage, PrintPageCallback, m_pPage,
         tb.m_pError, TRUE);
      if (pNode) {
         tb.Interpret (m_pPage, NULL, pNode, m_hDCPrint, m_hInstancePage,
            pi2->rPage.right - pi2->rPage.left - pi->rPageMargin.right - pi->rPageMargin.left);
         pi2->iHeaderHeight = tb.m_iCalcHeight;
         delete pNode;
      }
   }
   if (pi->pszFooter) {
      CEscTextBlock  tb;
      PCMMLNode   pNode;
      tb.Init ();
      pNode = ParseMML (pi->pszFooter, m_hInstancePage, PrintPageCallback, m_pPage,
         tb.m_pError, TRUE);
      if (pNode) {
         tb.Interpret (m_pPage, NULL, pNode, m_hDCPrint, m_hInstancePage,
            pi2->rPage.right - pi2->rPage.left - pi->rPageMargin.right - pi->rPageMargin.left);
         pi2->iFooterHeight = tb.m_iCalcHeight;
         delete pNode;
      }
   }
   pi2->iRowHeight = (pi2->rPage.bottom - pi->rPageMargin.top - pi->rPageMargin.bottom -
      pi2->iHeaderHeight - pi2->iFooterHeight - (pi->pszHeader ? pi->iHeaderSepY : 0) -
      (pi->pszFooter ? pi->iFooterSepY : 0) -
      ((int) pi->wRows - 1) * pi->iRowSepY) / (int) pi->wRows;

}

/***************************************************************************
PrintSetMapMode - Internal function that sets the mapping mode bsaed upon
the m_PrintInfo. This basically affects scaling.
*/
BOOL CEscWindow::PrintSetMapMode (void)
{
#if 1
   if (!SetMapMode (m_hDCPrint, MM_ISOTROPIC))
      return FALSE;
   if (!SetWindowExtEx (m_hDCPrint,
      96 * 0x100 / (int) m_PrintInfo.wOtherScale,
      96 * 0x100 / (int) m_PrintInfo.wOtherScale,
      NULL))
      return FALSE;
   if (!SetViewportExtEx (m_hDCPrint, GetDeviceCaps (m_hDCPrint, LOGPIXELSX),
      GetDeviceCaps (m_hDCPrint, LOGPIXELSY), NULL))
      return FALSE;
#else
   if (!SetMapMode (m_hDCPrint, MM_TEXT))
      return FALSE;
#endif

   return TRUE;
}

/***************************************************************************
Print - Prints the document.

Before calling PrintCalcPages you must:
   - Call InitForPrint
   - Set up the m_PrintInfo as desired
   - Call PrintPageLoad

inputs
   PWSTR    pszDocName - Document name. If NULL uses a default name
   DWORD    dwStart - start page (1-based). 1 default.
   DWORD    dwNum - number of pages. If specify more pages than there are in
            the document then limits itself to the number of pages in the
            document
   DWORD    dwFlags - Zero or more of the ESCPRINT_XXX flags
               ESCPRINT_EVENONLY - Only print even pages
               ESCPRINT_ODDONLY - Only print odd pages
   DWORD    *pdwNumPages - Filled in with the number of pages processed.
               If have EVENONLY or ODDONLY, or dwStart!=0 or dwNum too small,
               not necessarily the number of pages printed.
returns
   BOOL - TRUE if successful
*/
BOOL CEscWindow::Print (PWSTR pszDocName, DWORD dwStart, DWORD dwNum, DWORD dwFlags, DWORD *pdwNumPages)
{
   if (pdwNumPages)
      *pdwNumPages = 0;

   if (!m_hDCPrint || !m_pPage)
      return FALSE;

   // set the mode
   if (!PrintSetMapMode())
      return FALSE;

   // get the info
   ESCPRINTINFO   pi;
   ESCPRINTINFO2  pi2;
   PrintRealInfo (&pi, &pi2);

   // beging document
   DOCINFO  di;
   BOOL  fDocStarted = FALSE;
   memset (&di, 0, sizeof(di));
   char  szTemp[512];
   if (pszDocName)
      WideCharToMultiByte (CP_ACP, 0, pszDocName, -1, szTemp, sizeof(szTemp), 0, 0);
   else
      strcpy (szTemp, "Escarpment document");
   di.cbSize = sizeof(DOCINFO);
   di.lpszDocName = szTemp;

   // start printing
   int iPageY; // vertical index into page, for what printing
   iPageY = 0;

   DWORD dwPage;
   dwPage = 1;

   while (iPageY < m_pPage->m_rTotal.bottom) {

      // figure out if want to print this page
      BOOL  fPrintPage = TRUE;
      if (dwPage < dwStart)
         fPrintPage = FALSE;
      if (dwPage+dwStart > dwNum)
         break;   // done
      if ((dwFlags & ESCPRINT_EVENONLY) && (dwPage % 2))
         fPrintPage = FALSE;
      if ((dwFlags & ESCPRINT_ODDONLY) && !(dwPage % 2))
         fPrintPage = FALSE;

      // begin page
      if (fPrintPage && !fDocStarted) {
         if (!StartDoc (m_hDCPrint, &di))
            return FALSE;

         // BUGFIX - SetICMMode here (and once per doc) instead of
         // every page. If do every page then it's mostly B&W printing
         // on the second page
         SetICMMode (m_hDCPrint, ICM_ON);
         fDocStarted = TRUE;
      }
      if (fPrintPage && !StartPage (m_hDCPrint)) {
         AbortDoc (m_hDCPrint);
         return FALSE;
      }

      // reset mapping mode
      if (!PrintSetMapMode ()) {
         if (fPrintPage)
            EndPage (m_hDCPrint);
         AbortDoc (m_hDCPrint);
         return FALSE;
      }

      // print the header and footer
      PRINTHF hf;
      WCHAR szDate[32], szTime[32], szPage[16];
      hf.pszSection = m_pPage->m_TextBlock.SectionFromY (iPageY);
      hf.pszTitle = m_pPage->m_TextBlock.m_pNodePageInfo ?
         m_pPage->m_TextBlock.m_pNodePageInfo->AttribGet(L"title") : NULL;
      SYSTEMTIME  st;
      GetLocalTime (&st);
      swprintf (szDate, L"%d-%d-%d", (int) st.wDay, (int) st.wMonth, (int) st.wYear);
      swprintf (szTime, L"%d:%d%d", (int) st.wHour, (int) st.wMinute / 10,
         (int) st.wMinute % 10);
      hf.pszDate = szDate;
      hf.pszTime = szTime;
      swprintf (szPage, L"%d", dwPage);
      hf.pszPage = szPage;
      PVOID pUserData;
      pUserData = m_pPage->m_pUserData;
      m_pPage->m_pUserData = &hf;

      if (fPrintPage && pi.pszHeader) {
         CEscTextBlock  tb;
         PCMMLNode   pNode;
         tb.Init ();
         pNode = ParseMML (pi.pszHeader, m_hInstancePage, PrintPageCallback, m_pPage,
            tb.m_pError, TRUE);
         if (pNode) {
            RECT r;
            r.top = pi.rPageMargin.top;
            r.left = pi.rPageMargin.left;
            r.right = pi2.rPage.right - pi.rPageMargin.right;
            r.bottom = pi.rPageMargin.top + pi2.iHeaderHeight;
            tb.Interpret (m_pPage, NULL, pNode, m_hDCPrint, m_hInstancePage,
               r.right - r.left);
            tb.PostInterpret (r.left, r.top, &r);
            POINT pnt;
            pnt.x = pnt.y = 0;
            tb.Paint (m_hDCPrint, &pnt, &r, &r, &pi2.rPage);
            delete pNode;
         }
      }
      if (fPrintPage && pi.pszFooter) {
         CEscTextBlock  tb;
         PCMMLNode   pNode;
         tb.Init ();
         pNode = ParseMML (pi.pszFooter, m_hInstancePage, PrintPageCallback, m_pPage,
            tb.m_pError, TRUE);
         if (pNode) {
            RECT r;
            r.top = pi2.rPage.bottom - pi2.iFooterHeight - pi.rPageMargin.bottom;
            r.left = pi.rPageMargin.left;
            r.right = pi2.rPage.right - pi.rPageMargin.right;
            r.bottom = pi2.rPage.bottom - pi.rPageMargin.bottom;
            tb.Interpret (m_pPage, NULL, pNode, m_hDCPrint, m_hInstancePage,
               r.right - r.left);
            tb.PostInterpret (r.left, r.top, &r);
            POINT pnt;
            pnt.x = pnt.y = 0;
            tb.Paint (m_hDCPrint, &pnt, &r, &r, &pi2.rPage);
            delete pNode;
         }
      }

      // restore the old user data
      m_pPage->m_pUserData = pUserData;


      // lines between header/footer and text?
      if (fPrintPage && pi.fHeaderLine && pi.pszHeader) {
         HPEN  hOld;
         hOld = (HPEN) SelectObject (m_hDCPrint, GetStockObject (BLACK_PEN));
         MoveToEx (m_hDCPrint,
            pi.rPageMargin.left,
            pi.rPageMargin.top + pi2.iHeaderHeight + pi.iHeaderSepY/2, NULL);
         LineTo (m_hDCPrint,
            pi2.rPage.right - pi.rPageMargin.right,
            pi.rPageMargin.top + pi2.iHeaderHeight + pi.iHeaderSepY/2);
         SelectObject (m_hDCPrint, hOld);
      }

      if (fPrintPage && pi.fFooterLine && pi.pszFooter) {
         HPEN  hOld;
         hOld = (HPEN) SelectObject (m_hDCPrint, GetStockObject (BLACK_PEN));
         MoveToEx (m_hDCPrint,
            pi.rPageMargin.left,
            pi2.rPage.bottom - pi.rPageMargin.bottom - pi2.iFooterHeight - pi.iFooterSepY/2, NULL);
         LineTo (m_hDCPrint,
            pi2.rPage.right - pi.rPageMargin.right,
            pi2.rPage.bottom - pi.rPageMargin.bottom - pi2.iFooterHeight - pi.iFooterSepY/2);
         SelectObject (m_hDCPrint, hOld);
      }

      // loop through all the columns and row
      WORD  wRow, wColumn;
      for (wRow = 0; wRow < pi.wRows; wRow++) {
         // lines between rows?
         if (fPrintPage && pi.fRowLine && wRow) {
            HPEN  hOld;
            hOld = (HPEN) SelectObject (m_hDCPrint, GetStockObject (BLACK_PEN));
            int   iY;
            iY = pi.rPageMargin.top + pi2.iHeaderHeight +
               (pi.pszHeader ? pi.iHeaderSepY : 0) +
               ((int)wRow) * (pi2.iRowHeight + pi.iRowSepY) - pi.iRowSepY / 2;

            MoveToEx (m_hDCPrint,
               pi.rPageMargin.left,
               iY, NULL);
            LineTo (m_hDCPrint,
               pi2.rPage.right - pi.rPageMargin.right,
               iY);
            SelectObject (m_hDCPrint, hOld);
         }

         for (wColumn = 0; wColumn < pi.wColumns; wColumn++) {
            // calculate how much text can fit
            int   iFit;
            iFit = m_pPage->m_TextBlock.FindBreak (iPageY + pi2.iRowHeight * 19 / 20,
               iPageY + pi2.iRowHeight);

            // paint
            RECT  rPage, rDC;
            rPage.left = 0;
            rPage.right = pi2.iColumnWidth;
            rPage.top = iPageY;
            rPage.bottom = iFit;
            rDC.left = pi.rPageMargin.left + ((int) wColumn) * (pi2.iColumnWidth + pi.iColumnSepX);
            rDC.top = pi.rPageMargin.top + pi2.iHeaderHeight +
               (pi.pszHeader ? pi.iHeaderSepY : 0) +
               ((int)wRow) * (pi2.iRowHeight + pi.iRowSepY);
            rDC.right = rDC.left + pi2.iColumnWidth;
            rDC.bottom = rDC.top + (iFit - iPageY);

            if (fPrintPage && !PrintPaint (&rPage, &rDC, &pi2.rPage)) {
               // errro
               if (fPrintPage)
                  EndPage (m_hDCPrint);
               AbortDoc (m_hDCPrint);
               return TRUE;
            }

            // lines between columns?
            if (fPrintPage && pi.fColumnLine && wColumn) {
               HPEN  hOld;
               hOld = (HPEN) SelectObject (m_hDCPrint, GetStockObject (BLACK_PEN));

               MoveToEx (m_hDCPrint,
                  rDC.left - pi.iColumnSepX / 2,
                  rDC.top, NULL);
               LineTo (m_hDCPrint,
                  rDC.left - pi.iColumnSepX / 2,
                  rDC.top + pi2.iRowHeight);
               SelectObject (m_hDCPrint, hOld);
            }

            // move on
            iPageY = iFit;
         }
      }

      // end page
      if (fPrintPage)
         EndPage (m_hDCPrint);
      dwPage++;
   }

   // end document
   if (fDocStarted)
      EndDoc (m_hDCPrint);

   if (pdwNumPages)
      *pdwNumPages = dwPage-1;
   return TRUE;
}


/*******************************************************************8
EscGetDeviceCaps - This is a utility function that does GetDeviceCaps()
on the HDC. If it's going to the printer and there's scaling, then
the numbers for LOGPIXELXS, LOGPIXELSY, PHYSICALWIDTH, PHYSICALHEIGHT
are scaled appropriately so that when printing is done it all turns
out OK.

You should use this instead of GetDeviceCaps() in your controls to ensure
they draw properly.

inputs
   HDC   hDC - HDC
   int   nIndex - index
returns
   int
*/
int EscGetDeviceCaps (HDC hDC, int nIndex)
{
   int   iRet;
   iRet = GetDeviceCaps (hDC, nIndex);
   if (GetMapMode(hDC) != MM_ISOTROPIC)
      return iRet;

   // else we're mapping, so scale
   SIZE  w, v;
   switch (nIndex) {
   case LOGPIXELSX:
   case PHYSICALWIDTH:
      GetWindowExtEx (hDC, &w);
      GetViewportExtEx (hDC, &v);
      return (int) ((double)iRet * w.cx / (double) v.cx);

   case LOGPIXELSY:
   case PHYSICALHEIGHT:
      GetWindowExtEx (hDC, &w);
      GetViewportExtEx (hDC, &v);
      return (int) ((double)iRet * w.cy / (double) v.cy);
   }

   return iRet;
}



/*****************************************************************************
BitmapToDIB - Necessary to print in color?
*/
HDIB BitmapToDIB(HBITMAP hBitmap) 
{ 
    BITMAP              bm;         // bitmap structure 
    BITMAPINFOHEADER    bi;         // bitmap header 
    LPBITMAPINFOHEADER  lpbi;       // pointer to BITMAPINFOHEADER 
    DWORD               dwLen;      // size of memory block 
    HANDLE              hDIB, h;    // handle to DIB, temp handle 
    HDC                 hDC;        // handle to DC 
    WORD                biBits;     // bits per pixel 
//    HPALLETE            hPal = NULL;
 
    // check if bitmap handle is valid 
 
    if (!hBitmap) 
        return NULL; 
 
    // fill in BITMAP structure, return NULL if it didn't work 
 
    if (!GetObject(hBitmap, sizeof(bm), (LPSTR)&bm)) 
        return NULL; 
 
    // if no palette is specified, use default palette 
 
//    if (hPal == NULL) 
//        hPal = GetStockObject(DEFAULT_PALETTE); 
 
    // calculate bits per pixel 
 
    biBits = bm.bmPlanes * bm.bmBitsPixel; 
 
    // make sure bits per pixel is valid 
 
    if (biBits <= 1) 
        biBits = 1; 
    else if (biBits <= 4) 
        biBits = 4; 
    else if (biBits <= 8) 
        biBits = 8; 
    else // if greater than 8-bit, force to 24-bit 
        biBits = 24; 
 
    // initialize BITMAPINFOHEADER 
 
    bi.biSize = sizeof(BITMAPINFOHEADER); 
    bi.biWidth = bm.bmWidth; 
    bi.biHeight = bm.bmHeight; 
    bi.biPlanes = 1; 
    bi.biBitCount = biBits; 
    bi.biCompression = BI_RGB; 
    bi.biSizeImage = 0; 
    bi.biXPelsPerMeter = 0; 
    bi.biYPelsPerMeter = 0; 
    bi.biClrUsed = 0; 
    bi.biClrImportant = 0; 
 
    // calculate size of memory block required to store BITMAPINFO 
 
    dwLen = bi.biSize; // + PaletteSize((LPSTR)&bi); 
 
    // get a DC 
 
    hDC = GetDC(NULL); 
 
    // select and realize our palette 
 
//    hPal = SelectPalette(hDC, hPal, FALSE); 
//    RealizePalette(hDC); 
 
    // alloc memory block to store our bitmap 
 
    hDIB = GlobalAlloc(GHND, dwLen); 
 
    // if we couldn't get memory block 
 
    if (!hDIB) 
    { 
      // clean up and return NULL 
 
//      SelectPalette(hDC, hPal, TRUE); 
//      RealizePalette(hDC); 
      ReleaseDC(NULL, hDC); 
      return NULL; 
    } 
 
    // lock memory and get pointer to it 
 
    lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDIB); 
 
    /// use our bitmap info. to fill BITMAPINFOHEADER 
 
    *lpbi = bi; 
 
    // call GetDIBits with a NULL lpBits param, so it will calculate the 
    // biSizeImage field for us     
 
    GetDIBits(hDC, hBitmap, 0, (UINT)bi.biHeight, NULL, (LPBITMAPINFO)lpbi, 
        DIB_RGB_COLORS); 
 
    // get the info. returned by GetDIBits and unlock memory block 
 
    bi = *lpbi; 
    GlobalUnlock(hDIB); 

 #define WIDTHBYTES(bits)    (((bits) + 31) / 32 * 4) 

    // if the driver did not fill in the biSizeImage field, make one up  
    if (bi.biSizeImage == 0) 
        bi.biSizeImage = WIDTHBYTES((DWORD)bm.bmWidth * biBits) * bm.bmHeight; 
 
    // realloc the buffer big enough to hold all the bits 
 
    dwLen = bi.biSize + /* PaletteSize((LPSTR)&bi) +*/ bi.biSizeImage; 
 
    if (h = GlobalReAlloc(hDIB, dwLen, 0)) 
        hDIB = h; 
    else 
    { 
        // clean up and return NULL 
 
        GlobalFree(hDIB); 
        hDIB = NULL; 
//        SelectPalette(hDC, hPal, TRUE); 
//        RealizePalette(hDC); 
        ReleaseDC(NULL, hDC); 
        return NULL; 
    } 
 
    // lock memory block and get pointer to it */ 
 
    lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDIB); 
 
    // call GetDIBits with a NON-NULL lpBits param, and actualy get the 
    // bits this time 
 
    if (GetDIBits(hDC, hBitmap, 0, (UINT)bi.biHeight, (LPSTR)lpbi + 
            (WORD)lpbi->biSize /*+ PaletteSize((LPSTR)lpbi)*/, (LPBITMAPINFO)lpbi, 
            DIB_RGB_COLORS) == 0) 
    { 
        // clean up and return NULL 
 
        GlobalUnlock(hDIB); 
        hDIB = NULL; 
//        SelectPalette(hDC, hPal, TRUE); 
//        RealizePalette(hDC); 
        ReleaseDC(NULL, hDC); 
        return NULL; 
    } 
 
    bi = *lpbi; 
 
    // clean up  
    GlobalUnlock(hDIB); 
//    SelectPalette(hDC, hPal, TRUE); 
//    RealizePalette(hDC); 
    ReleaseDC(NULL, hDC); 
 
    // return handle to the DIB 
    return hDIB; 
} 
/*************************************************************************
*
SaveDIB()
Saves the specified DIB into the specified file name on disk.  No
error checking is done, so if the file already exists, it will be
written over.
Parameters:
HDIB hDib - Handle to the dib to save

LPSTR lpFileName - pointer to full pathname to save DIB under
Return value: 0 if successful, or one of:
******/  
WORD SaveDIB(HDIB hDib, LPSTR lpFileName)
{
   BITMAPFILEHEADER    bmfHdr;     // Header for Bitmap file
   LPBITMAPINFOHEADER  lpBI;       // Pointer to DIB info structure
   HANDLE              fh;         // file handle for opened file
   DWORD               dwDIBSize;
   DWORD               dwWritten;
   if (!hDib) 
      return 1;
      
   fh = CreateFile(lpFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
   if (fh == INVALID_HANDLE_VALUE)
      return 1;
   // Get a pointer to the DIB memory, the first of which contains 
   // a BITMAPINFO structure 
   lpBI = (LPBITMAPINFOHEADER)GlobalLock(hDib);
   if (!lpBI)     {
      CloseHandle(fh);
      return 1; 
   }    
   // Check to see if we're dealing with an OS/2 DIB.  If so, don't    
   // save it because our functions aren't written to deal with these 
   // DIBs. 
   if (lpBI->biSize != sizeof(BITMAPINFOHEADER))     { 
      GlobalUnlock(hDib);
      CloseHandle(fh);
      return 1;
   } 
   // Fill in the fields of the file header 
   // Fill in file type (first 2 bytes must be "BM" for a bitmap)
   bmfHdr.bfType = ((WORD) ('M' << 8) | 'B'); // DIB_HEADER_MARKER;  // "BM"  
   // Calculating the size of the DIB is a bit tricky (if we want to     
   // do it right).  The easiest way to do this is to call GlobalSize()
   // on our global handle, but since the size of our global memory may have 
   // been padded a few bytes, we may end up writing out a few too 
   // many bytes to the file (which may cause problems with some apps, 
   // like HC 3.0). 
   //  
   // So, instead let's calculate the size manually.  
   //    
   // To do this, find size of header plus size of color table.  Since the 
   // first DWORD in both BITMAPINFOHEADER and BITMAPCOREHEADER conains  
   // the size of the structure, let's use this.  
   // Partial Calculation  
   dwDIBSize = *(LPDWORD)lpBI; // assume 0 -  + PaletteSize((LPSTR)lpBI);  
   // Now calculate the size of the image   
   // It's an RLE bitmap, we can't calculate size, so trust the biSizeImage 
   // field      
   // Calculating the size of the DIB is a bit tricky (if we want to 
   // do it right).  The easiest way to do this is to call GlobalSize()  
   // on our global handle, but since the size of our global memory may have 
   // been padded a few bytes, we may end up writing out a few too  
   // many bytes to the file (which may cause problems with some apps, 
   // like HC 3.0). 
   //   
   // So, instead let's calculate the size manually. 
   //   
   // To do this, find size of header plus size of color table.  Since the 
   // first DWORD in both BITMAPINFOHEADER and BITMAPCOREHEADER conains 
   // the size of the structure, let's use this.
   // Partial Calculation   
   dwDIBSize = *(LPDWORD)lpBI; // assume 0 + PaletteSize((LPSTR)lpBI); 
   // Now calculate the size of the image  
   // It's an RLE bitmap, we can't calculate size, so trust the biSizeImage 
   // field      
   if ((lpBI->biCompression == BI_RLE8) || (lpBI->biCompression == BI_RLE4)) 
      dwDIBSize += lpBI->biSizeImage;
   else     {        
      DWORD dwBmBitsSize;  // Size of Bitmap Bits only    
      // It's not RLE, so size is Width (DWORD aligned) * Height  
      dwBmBitsSize = WIDTHBYTES((lpBI->biWidth)*((DWORD)lpBI->biBitCount)) *     
         lpBI->biHeight;      
      dwDIBSize += dwBmBitsSize;      
      // Now, since we have calculated the correct size, why don't we    
      // fill in the biSizeImage field (this will fix any .BMP files which  
      // have this field incorrect).     
      lpBI->biSizeImage = dwBmBitsSize;  
   }    
   // Calculate the file size by adding the DIB size to sizeof(BITMAPFILEHEADER)  
   bmfHdr.bfSize = dwDIBSize + sizeof(BITMAPFILEHEADER); 
   bmfHdr.bfReserved1 = 0; 
   bmfHdr.bfReserved2 = 0;  
   // Now, calculate the offset the actual bitmap bits will be in 
   // the file -- It's the Bitmap file header plus the DIB header,  
   // plus the size of the color table.   
   bmfHdr.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + lpBI->biSize;
      // assume 0 pallette +  PaletteSize((LPSTR)lpBI);      
   // Write the file header  
   WriteFile(fh, (LPSTR)&bmfHdr, sizeof(BITMAPFILEHEADER), &dwWritten, NULL); 
   // Write the DIB header and the bits -- use local version of
   // MyWrite, so we can write more than 32767 bytes of data    
   WriteFile(fh, (LPSTR)lpBI, dwDIBSize, &dwWritten, NULL);  
   GlobalUnlock(hDib); 
   CloseHandle(fh);  
   if (dwWritten == 0)   
      return 1;
   // oops, something happened in the write 
   else     
      return 0; 
   // Success code
} 


WORD DestroyDIB(HDIB hDib)
{
   GlobalFree(hDib);   
   return 0; 
}  

 
/***************************************************************************
EscStretchBlt - Replacement for StretchBlt so that you can actually print
to the printer. It tests to see if the HDC is a printer. If so, it
does special code.


inputs
   Same as EscStretchBlt except that at the end also takes HBITMAP
   for the source bitmap.
*/
typedef HANDLE HDIB;

BOOL EscStretchBlt(  HDC hdcDest,      // handle to destination device context
  int nXOriginDest, // x-coordinate of upper-left corner of dest. rectangle
  int nYOriginDest, // y-coordinate of upper-left corner of dest. rectangle
  int nWidthDest,   // width of destination rectangle
  int nHeightDest,  // height of destination rectangle
  HDC hdcSrc,       // handle to source device context
  int nXOriginSrc,  // x-coordinate of upper-left corner of source rectangle
  int nYOriginSrc,  // y-coordinate of upper-left corner of source rectangle
  int nWidthSrc,    // width of source rectangle
  int nHeightSrc,   // height of source rectangle
  DWORD dwRop,       // raster operation code
  HBITMAP hbmpSource)
{
   if (EscGetDeviceCaps(hdcDest, TECHNOLOGY) != DT_RASPRINTER)
      return StretchBlt (hdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest,
         hdcSrc, nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, dwRop);

   // else complex stuff
   HDIB hd;
   hd = BitmapToDIB(hbmpSource);
   BITMAPINFOHEADER  *pi;
   pi = (BITMAPINFOHEADER*) GlobalLock (hd);
   int   iOldMode;
   iOldMode = SetStretchBltMode (hdcDest, COLORONCOLOR);
   StretchDIBits(  hdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest,
      nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc,
      (LPSTR)pi +(WORD)pi->biSize,
      (BITMAPINFO*) pi,
      DIB_RGB_COLORS, 
      dwRop);
   SetStretchBltMode (hdcDest, iOldMode);

   if (hd) {
      GlobalUnlock (hd);
      GlobalFree (hd);
   }
   return TRUE;
}


/***************************************************************************
EscBitBlt - Replacement for BitBlt so that you can actually print
to the printer. It tests to see if the HDC is a printer. If so, it
does special code.

inputs
   Same as EscBitBlt except that at the end also takes HBITMAP
   for the source bitmap.
*/
BOOL EscBitBlt( HDC hdcDest, // handle to destination device context
  int nXDest,  // x-coordinate of destination rectangle's upper-left 
               // corner
  int nYDest,  // y-coordinate of destination rectangle's upper-left 
               // corner
  int nWidth,  // width of destination rectangle
  int nHeight, // height of destination rectangle
  HDC hdcSrc,  // handle to source device context
  int nXSrc,   // x-coordinate of source rectangle's upper-left 
               // corner
  int nYSrc,   // y-coordinate of source rectangle's upper-left 
               // corner
  DWORD dwRop,  // raster operation code
  HBITMAP hbmpSource)
{
   if (EscGetDeviceCaps(hdcDest, TECHNOLOGY) != DT_RASPRINTER)
      return BitBlt (hdcDest, nXDest, nYDest, nWidth, nHeight,
         hdcSrc, nXSrc, nYSrc, dwRop);

   // else do my special stretch
   return EscStretchBlt (hdcDest, nXDest, nYDest, nWidth, nHeight,
      hdcSrc, nXSrc, nYSrc, nWidth, nHeight, dwRop, hbmpSource);
}


// BUGBUG - 2.0 - allow CWindow to be child window - that way can have it do the title bar and scroll
// bar? Basically...
//    - If window has title bar & scroll bar, create two sub-CWindows.
//    - Provide a CPage callback with user-data pointing to CWindows
//    - When user drags, or scrolls, or whatever, ends up calling function
//       in CWindows to notify of change



