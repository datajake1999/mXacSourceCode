/*************************************************************************************
CTickerTape.cpp - Window on bottom and top that slide in/out

begun 19/4/09 by Mike Rozak.
Copyright 2009 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <zmouse.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"




// defines
#define SLIDETIMERID                42
#define SLIDETIMERMILLISEC          100      // slide every 100th of a sec
#define SLIDETIMERTOTAL             500     // takes 1/2 second to slide in out

#define TICKERTIMERID               43
#define TICKERTIMERMILLISEC         (1000 / ((giCPUSpeed >= 2) ? 60 : 30))      // slide every 100th of a sec


// TICKERINFO - Information about a particular tick string
typedef struct {
   BOOL              fSymbol;    // if TRUE then use the symbol font
   BOOL              fRectValid; // set to TRUE if the rectangle is valid
   COLORREF          cColor;     // color
   RECT              rRect;      // location rectangle
   HBITMAP           hBmp;       // bitmap of the text
} TICKERINFO, *PTICKERINFO;


/*************************************************************************************
CTickerTape::Constructor and destructor
*/
CTickerTape::CTickerTape (void)
{
   m_fOnTop = FALSE;
   m_hWnd = NULL;
   m_pMain = NULL;
   m_hWndParent = NULL;
   m_fSlideTimer = FALSE;
   m_iStayOpenTime = 0;
   m_hFontBig = NULL;
   m_hFontSymbol = NULL;
   m_iFontSize = 0;
   m_iFontPixels = 0;
   m_lTICKERINFO.Init (sizeof(TICKERINFO));
   m_dwLastTick = 0;
}

CTickerTape::~CTickerTape (void)
{
   DWORD i;
   PTICKERINFO pti = (PTICKERINFO) m_lTICKERINFO.Get(0);
   for (i = 0; i < m_lTICKERINFO.Num(); i++, pti++)
      if (pti->hBmp)
         DeleteObject (pti->hBmp);
   m_lTICKERINFO.Clear();

   if (IsWindow (m_hWnd))
      DestroyWindow (m_hWnd);

   if (m_hFontBig)
      DeleteObject (m_hFontBig);
   if (m_hFontSymbol)
      DeleteObject (m_hFontSymbol);
}

/************************************************************************************
CTickerTape::CreateFontIfNecessary - This creates the font if it's not already created.

inputs
   BOOL        fForce - If TRUE then forces the creation (even if was created)
returns
   BOOL - TRUE if succcess
*/
BOOL CTickerTape::CreateFontIfNecessary (BOOL fForce)
{
   if (fForce) {
      if (m_hFontBig)
         DeleteObject (m_hFontBig);
      if (m_hFontSymbol)
         DeleteObject (m_hFontSymbol);
      m_hFontBig = m_hFontSymbol = NULL;
   }

   if (m_hFontBig && m_hFontSymbol)
      return TRUE;

   int iFontSize = FontScaleByScreenSize(- (int)(pow(sqrt(2.0), (fp)m_iFontSize) * 40.0));
   LOGFONT lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = iFontSize; 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfWeight = FW_NORMAL;
   lf.lfItalic = FALSE;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   m_hFontBig = CreateFontIndirect (&lf);

   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = iFontSize;
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfWeight = FW_NORMAL;
   lf.lfItalic = FALSE;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Symbol");
   m_hFontSymbol = CreateFontIndirect (&lf);

   if (!m_hFontBig && !m_hFontSymbol)
      return FALSE;

   // see how big the font is
   _ASSERTE (m_hWndParent);
   HDC hDC = GetDC (m_hWndParent);
   HFONT hFontOld = (HFONT) SelectObject (hDC, m_hFontBig);
   RECT rDraw;
   memset (&rDraw, 0, sizeof(rDraw));
   rDraw.right = rDraw.bottom = 10000;
   DrawTextW (hDC, L"CircumReality" /* doesn't really matter*/, -1, &rDraw, DT_SINGLELINE | DT_LEFT | DT_TOP | DT_CALCRECT);
   SelectObject (hDC, hFontOld);
   ReleaseDC (m_hWndParent, hDC);
   m_iFontPixels = rDraw.bottom;

   return TRUE;
}


/************************************************************************************
TickerTapeWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK TickerTapeWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCTickerTape p = (PCTickerTape) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCTickerTape p = (PCTickerTape) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCTickerTape) lpcs->lpCreateParams;

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
CTickerTape::Init - Initializes the sliding window code.

inputs
   HWND           hWndParent - Parent window to create in
   BOOL           fOnTop - If TRUE, this window is created at the top and of hWndParent.
                     If FALSE, the bottom end
   int            iFontSize - Font size to start out with, 0 for default, 1 for larger, -1 for smalelr
   PCMainWindow   pMain - Main window

returns
   BOOL - TRUE if success. FALSE if error
*/
BOOL CTickerTape::Init (HWND hWndParent, BOOL fOnTop, int iFontSize, PCMainWindow pMain)
{
   if (m_hWnd)
      return FALSE;

   m_fOnTop = fOnTop;
   m_pMain = pMain;
   m_hWndParent = hWndParent;
   m_iFontSize = iFontSize;
   CreateFontIfNecessary (TRUE);

   // register the class
   // create window
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = TickerTapeWndProc;
   wc.lpszClassName = "CircumrealityClientTickerTape";
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = NULL;
   wc.hCursor = pMain->m_hCursorNo;
   wc.hbrBackground = NULL;
   RegisterClass (&wc);

   // get the default coords
   RECT r;
   DefaultCoords (FALSE, &r);

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
CTickerTape::DefaultCoords - Fills in a rectangle with the default coordingates
for where the client should go.

inputs
   DWORD          dwLocked - Set to TRUE if open
   PRECT          pr - Filled in
returns
   none
*/
void CTickerTape::DefaultCoords (DWORD dwLocked, PRECT pr)
{
   GetClientRect (m_hWndParent, pr);

   if (!CreateFontIfNecessary (FALSE))
      return;

   int iHeight = m_iFontPixels * 5 / 4;

   if (m_fOnTop) {
      pr->bottom = pr->top;
      pr->top = pr->bottom - iHeight;
   }
   else {
      pr->top = pr->bottom;
      pr->bottom = pr->top + iHeight;
   }

   if (dwLocked) {
      int iDelta = (pr->bottom - pr->top);
      if (!m_fOnTop)
         iDelta *= -1;
      pr->top += iDelta;
      pr->bottom += iDelta;
   }
}



/*************************************************************************************
CTickerTape::WndProc - Window procedure
*/
LRESULT CTickerTape::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      SetTimer (m_hWnd, TICKERTIMERID, TICKERTIMERMILLISEC, NULL);

      return 0;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC = BeginPaint (hWnd, &ps);

         RECT rClient;
         GetClientRect (hWnd, &rClient);

         HDC hDCBmp;
         HBITMAP hBmp;
         hDCBmp = CreateCompatibleDC (hDC);
         hBmp = CreateCompatibleBitmap (hDC, rClient.right, rClient.bottom);
         SelectObject (hDCBmp, hBmp);

         // clear background
         HBRUSH hbr;
         hbr = CreateSolidBrush (m_pMain->m_cBackgroundNoTabNoChange);
         FillRect (hDCBmp, &rClient, hbr);

         // need to draw text
         DWORD i;
         PTICKERINFO pti = (PTICKERINFO)m_lTICKERINFO.Get(0);
         for (i = 0; i < m_lTICKERINFO.Num(); i++, pti++) {
            if (!pti->fRectValid)
               break;

            HDC hDCText;
            hDCText = CreateCompatibleDC (hDC);

            // if there's not bitmap then create
            if (!pti->hBmp) {
               RECT rDraw;
               rDraw.top = rDraw.left = 0;
               rDraw.right = pti->rRect.right - pti->rRect.left;
               rDraw.bottom = pti->rRect.bottom - pti->rRect.top;
               pti->hBmp = CreateCompatibleBitmap (hDC, rDraw.right, rDraw.bottom);
               if (!pti->hBmp) {
                  DeleteDC (hDC);
                  continue;
               }
               SelectObject (hDCText, pti->hBmp);

               // blank it out
               FillRect (hDCText, &rDraw, hbr);

               HFONT hFontOld = (HFONT) SelectObject (hDCText, pti->fSymbol ? m_hFontSymbol : m_hFontBig);
               int iOldMode = SetBkMode (hDCText, TRANSPARENT);
               SetTextColor (hDCText, pti->cColor);
               DrawTextW (hDCText, (PWSTR)m_lTickerStrings.Get(i), -1, &rDraw, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
               SetBkMode (hDCText, iOldMode);
               SelectObject (hDCText, hFontOld);
            }
            else
               SelectObject (hDCText, pti->hBmp);

            // draw
            BitBlt(hDCBmp, pti->rRect.left, pti->rRect.top, pti->rRect.right - pti->rRect.left, pti->rRect.bottom - pti->rRect.top,
               hDCText, 0, 0, SRCCOPY);
            DeleteDC (hDCText);

         } // i


         BitBlt(hDC, rClient.left, rClient.top, rClient.right - rClient.left, rClient.bottom - rClient.top,
            hDCBmp, 0, 0, SRCCOPY);
         DeleteDC (hDCBmp);
         DeleteObject (hBmp);
         DeleteObject (hbr);


         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_DESTROY:
      if (m_fSlideTimer) {
         KillTimer (hWnd, SLIDETIMERID);
         m_fSlideTimer = FALSE;
      }
      KillTimer (hWnd, TICKERTIMERID);
      break;

   case WM_TIMER:
      if (wParam == SLIDETIMERID) {
         SlideTimer (TRUE);
         return 0;
      }
      else if (wParam == TICKERTIMERID) {
         TickerTimer ();
         return 0;
      }
      break;

   case WM_MOUSEWHEEL:
      // pass up
      return SendMessage (GetParent(hWnd), uMsg, wParam, lParam);



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


   } // switch uMsg

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*************************************************************************************
CTickerTape::Resize - Called when the main window is resized. This causes
the sliding window to resize.

*/
void CTickerTape::Resize (void)
{
   if (!m_hWnd)
      return;

   RECT r;
   DefaultCoords (FALSE, &r);
   MoveWindow (m_hWnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
}

/*************************************************************************************
CTickerTape::TickerTimer - Updates the window for the ticker.
*/
void CTickerTape::TickerTimer (void)
{
   // how much time between now and last tick
   DWORD dwNow = GetTickCount();
   if ((dwNow < m_dwLastTick) || (dwNow > m_dwLastTick + 1000))
      m_dwLastTick = dwNow;   // non-sensical time between so assume 0
   fp fTime = (fp)(dwNow - m_dwLastTick) / 1000.0;
   m_dwLastTick = dwNow;

   // font size
   if (!CreateFontIfNecessary(FALSE))
      return;

   // what's the movement rate
   int iMove = (int((fp)m_iFontPixels * fTime * 4.0));
      // *2.0 = move at 50 character heights per second
   iMove = max(iMove, 1);  // at least one pixel

   // client rectangle
   RECT rClient;
   GetClientRect (m_hWnd, &rClient);

   // move everything that's alive
   PTICKERINFO pti; //  = (PTICKERINFO)m_lTICKERINFO.Get(0);
   DWORD i;
   DWORD dwLastVisible = (DWORD)-1;
   for (i = 0; i < m_lTICKERINFO.Num(); i++) {
      pti = (PTICKERINFO)m_lTICKERINFO.Get(i);  // just in case

      // if invalid rectangle then stop
      if (!pti->fRectValid)
         break;

      // move this
      pti->rRect.left -= iMove;
      pti->rRect.right -= iMove;

      // no longer visible
      if (pti->rRect.right <= rClient.left) {
         if (pti->hBmp)
            DeleteObject (pti->hBmp);

         m_lTICKERINFO.Remove (i);
         m_lTickerStrings.Remove (i);
         i--;

         // make sure invalidated
         Invalidate ();

         continue;
      }

      // remember this as visible
      dwLastVisible = i;
   } // i

   // see if anything should be added
   while (TRUE) {
      if (!m_lTICKERINFO.Num())
         break;   // no more visible

      // if last visible is all the way on the screen then consider adding
      pti = (PTICKERINFO)m_lTICKERINFO.Get(0);

      // how far right is the rightmost
      int iRightMost = (dwLastVisible == (DWORD)-1) ? rClient.right : pti[dwLastVisible].rRect.right;
      if (iRightMost > rClient.right)
         break;   // don't add yet

      // else, can add
      DWORD dwAdd = (dwLastVisible == (DWORD)-1) ? 0 : (dwLastVisible+1);
      if (dwAdd >= m_lTICKERINFO.Num())
         break;   // nothing left to add

      // else, figure out the size
      HDC hDC = GetDC (m_hWnd);
      HFONT hFontOld = (HFONT) SelectObject (hDC, pti[dwAdd].fSymbol ? m_hFontSymbol : m_hFontBig);
      RECT rDraw;
      memset (&rDraw, 0, sizeof(rDraw));
      rDraw.right = rDraw.bottom = 100000;
      DrawTextW (hDC, (PWSTR)m_lTickerStrings.Get(dwAdd), -1, &rDraw, DT_SINGLELINE | DT_LEFT | DT_TOP | DT_CALCRECT);
      SelectObject (hDC, hFontOld);
      ReleaseDC (m_hWnd, hDC);

      pti[dwAdd].fRectValid = TRUE;
      pti[dwAdd].rRect = rClient;
      pti[dwAdd].rRect.left = max(iRightMost, rClient.right);
      pti[dwAdd].rRect.right = pti[dwAdd].rRect.left + rDraw.right - rDraw.left + m_iFontPixels;
         // NOTE: Adding a bit of spacing - iFontPixels

      // continue
      dwLastVisible = dwAdd;
   } // while (TRUE)

   // if something is visible, make sure the window stays open
   pti = (PTICKERINFO)m_lTICKERINFO.Get(0);
   if (m_lTICKERINFO.Num() && pti->fRectValid) {
      SlideDownTimed (500);  // so stays open for a second longer

      // invalidate
      Invalidate ();
   }
}


/*************************************************************************************
CTickerTape::SlideTimer - Causes the window to slide in/out when the slide timer
is called. If it has reached full position then stops sliding.

inputs
   BOOL        fForTimer - Called because of a timer
*/
void CTickerTape::SlideTimer (BOOL fForTimer)
{
   // make sure this window is on top
   if (GetNextWindow (m_hWnd, GW_HWNDPREV))
      SetWindowPos (m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

   // do we want the window in locked posiiton
   BOOL fWantFullLocked = FALSE;
   BOOL fWantHalfLocked = FALSE;

   // force open if timer
   if (m_iStayOpenTime > 0)
      fWantHalfLocked = TRUE;

   BOOL fWantLockedOrig = fWantFullLocked | fWantHalfLocked;

   // get current location, and what want it to be
   RECT rCur, rWant;
   GetWindowRect (m_hWnd, &rCur);
   ScreenToClient (m_hWndParent, (POINT*) &rCur.left);
   ScreenToClient (m_hWndParent, (POINT*) &rCur.right);
   DefaultCoords (fWantFullLocked || fWantHalfLocked, &rWant);

   // if it's in the right location then done
   if (rCur.top == rWant.top) {
      // NOTE: only kill the timer if we're in the final state we're supposed to be in
      if ((FALSE == (fWantHalfLocked || fWantFullLocked)) && (m_iStayOpenTime <= 0) && m_fSlideTimer) {
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
   int iMax = (rWant.bottom - rWant.top) * SLIDETIMERMILLISEC / SLIDETIMERTOTAL;
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
CTickerTape::Invalidate - Invalidate the entire display
*/
void CTickerTape::Invalidate (void)
{
   InvalidateRect (m_hWnd, NULL, FALSE);
}




/*************************************************************************************
CTickerTape::SlideDownTimed - Causes the sliding winddow to slide down and
stay open the given number of milliseconds.

inputs
   DWORD             dwTime - Number of milliseconds
returns
   none
*/
void CTickerTape::SlideDownTimed (DWORD dwTime)
{
   m_iStayOpenTime = (int)dwTime;

   // start timer if not already
   SlideTimer (FALSE);
}


/*************************************************************************************
CTickerTape::StringAdd - Adds a string to the ticker tape.

inputs
   PWSTR          psz - String to add
   COLORREF       cColor - Color
returns
   BOOL - TRUE if success
*/
BOOL CTickerTape::StringAdd (PWSTR psz, COLORREF cColor)
{
   if (!psz[0])
      return TRUE;   // empty string

   // if something is already there then add a divider
   TICKERINFO ti;
   memset (&ti, 0, sizeof(ti));
   ti.fRectValid = FALSE;
   if (m_lTICKERINFO.Num()) {
      WCHAR szSep[2];
      szSep[0] = L'A' + 28 * 4 + 6;
      szSep[1] = 0;

      ti.fSymbol = TRUE;
      ti.cColor = RGB(
         GetRValue(m_pMain->m_cBackgroundNoTabNoChange)/2 + 0x80,
         GetGValue(m_pMain->m_cBackgroundNoTabNoChange)/2 + 0x80,
         GetBValue(m_pMain->m_cBackgroundNoTabNoChange)/2 + 0x80
         );
      m_lTICKERINFO.Add (&ti);

      m_lTickerStrings.Add (szSep, (wcslen(szSep)+1)*sizeof(WCHAR));
   }

   // add this string
   ti.fSymbol = FALSE;
   ti.cColor = cColor;
   m_lTICKERINFO.Add (&ti);
   m_lTickerStrings.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));

   return TRUE;
}



/*************************************************************************************
CTickerTape::FontSet - Sets the font size.

inputs
   DWORD          dwSize - Font size, 2 = normal
returns
   BOOL - TRUE if success
*/
BOOL CTickerTape::FontSet (DWORD dwSize)
{
   int iWant = (int)dwSize - 2;
   if (iWant == m_iFontSize)
      return TRUE;   // no change

   m_iFontSize = iWant;
   if (!CreateFontIfNecessary(TRUE))
      return FALSE;  // error

   // invalidate all the window locations
   DWORD i;
   PTICKERINFO pti = (PTICKERINFO)m_lTICKERINFO.Get(0);
   for (i = 0; i < m_lTICKERINFO.Num(); i++, pti++) {
      pti->fRectValid = FALSE;
      if (pti->hBmp) {
         DeleteObject (pti->hBmp);
         pti->hBmp = NULL;
      }
   }

   // move window
   RECT rWant;
   DefaultCoords (FALSE, &rWant);
   MoveWindow (m_hWnd, rWant.left, rWant.top, rWant.right - rWant.left, rWant.bottom - rWant.top, TRUE);

   // done
   return TRUE;
}


/*************************************************************************************
CTickerTape::Phrase - Call this when a phrase starts speaking, or finishes speaking.

inputs
   GUID           *pgID - Guid of the speaker
   PWSTR          pszSpeaker - String for the speaker
   PWSTR          pszSpoken - String for what was spoken
   BOOL           fFinished - If TRUE, the phrase has just finished speaking. If FALSE
                  it has just started
returns
   none
*/
void CTickerTape::Phrase (GUID *pgID, PWSTR pszSpeaker, PWSTR pszSpoken, BOOL fFinished)
{
   // if finished then ignore
   if (fFinished)
      return;

   GUID gNull = GUID_NULL;
   if (!pgID)
      pgID = &gNull;
   if (IsEqualGUID(*pgID, GUID_NULL))
      pszSpeaker = NULL;   // narrator then don't show
   if (!pszSpeaker)
      pszSpeaker = L"";
   if (!_wcsicmp (pszSpeaker, gpszNarrator))
      pszSpeaker = L"";
   // if no spoken, or is just a space or just a period, then ignore
   if (!pszSpoken || (((iswspace (pszSpoken[0]) || (pszSpoken[0] == L'.')) && !pszSpoken[1])))
      return;  // ignore

   CMem mem;
   MemZero (&mem);
   if (pszSpeaker[0]) {
      MemCat (&mem, pszSpeaker);
      MemCat (&mem, L": ");
   }
   MemCat (&mem, pszSpoken);

   PWSTR psz = (PWSTR)mem.p;
   for (; *psz; psz++)
      if (iswspace(*psz))
         *psz = L' ';   // make sure tabs and whatnot converted to space

   StringAdd ((PWSTR)mem.p, RGB(0xe0,0xe0,0xe0));
}


