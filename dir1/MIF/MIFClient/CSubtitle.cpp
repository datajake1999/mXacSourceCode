/*************************************************************************************
CSubtitle.cpp - Subtitles for drawing text

begun 20/6/06 by Mike Rozak.
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




#define BUFFERSIZE(x)            ((x)*4)        // number of pixels between lines and the edge
#define BUFFERBETWEENLINESSIZE(x)   (BUFFERSIZE(x)*2) // space between lines

#define TIMERFADEID              1234           // ID number to check whether items have gone out of date
#define TIMERFADETIME            250            // check 1/4th of a second

static char *gpszSubtitleWindow = "CircumrealityClientSubtitleWindow";



/************************************************************************************
SubtitleWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK SubtitleWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCSubtitle p = (PCSubtitle) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCSubtitle p = (PCSubtitle)(LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCSubtitle) lpcs->lpCreateParams;

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
CSubtitle::Constructor and destructor
*/
CSubtitle::CSubtitle (void)
{
   m_hWndParent = NULL;
   m_dwFontSize = 2;
   m_hWnd = NULL;
   m_lSUBTITLEPHRASE.Init (sizeof(SUBTITLEPHRASE));
   m_dwCorner = 2;
   m_dwLastAutoMove = 0;

   // create the fonts
   DWORD i;
   LOGFONT  lf;
   for (i = 0; i < SUBTITLEFONTS; i++) {
      if (!i) {
         m_ahFontBold[i] = m_ahFontItalic[i] = NULL;
         continue;
      }

      memset (&lf, 0, sizeof(lf));
      lf.lfHeight = FontScaleByScreenSize(-(int)BUFFERSIZE(i)*2); 
      lf.lfCharSet = DEFAULT_CHARSET;
      lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
      lf.lfWeight = FW_BOLD;
      strcpy (lf.lfFaceName, "Arial");
      m_ahFontBold[i] = CreateFontIndirect (&lf);

      lf.lfWeight = 0;
      lf.lfItalic = TRUE;
      m_ahFontItalic[i] = CreateFontIndirect (&lf);

   } // i

   // register the window class
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = SubtitleWndProc;
   wc.lpszClassName = gpszSubtitleWindow;
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = NULL;
   wc.hCursor = NULL;
   wc.hbrBackground = NULL;
   RegisterClass (&wc);

   m_hbrBackground = CreateSolidBrush (RGB(0xff, 0xff, 0xc0));

}

CSubtitle::~CSubtitle (void)
{
   // close the window
   if (m_hWnd) {
      DestroyWindow (m_hWnd);
      m_hWnd = NULL;
   }

   // free up the fonts
   DWORD i;
   for (i = 0; i < SUBTITLEFONTS; i++) {
      if (m_ahFontBold[i]) {
         DeleteObject (m_ahFontBold[i]);
         m_ahFontBold[i] = 0;
      }
      if (m_ahFontItalic[i]) {
         DeleteObject (m_ahFontItalic[i]);
         m_ahFontItalic[i] = 0;
      }
   } // i

   // free up brushes
   DeleteObject (m_hbrBackground);
}


/*************************************************************************************
CSubtitle::Init - Call this once to initialize the parameters.

inputs
   HWND           hWndParent - Parent window to create tooltip off of
returns
   BOOL - TRUE if success
*/
BOOL CSubtitle::Init (HWND hWndParent)
{
   if (m_hWndParent)
      return FALSE;

   m_hWndParent = hWndParent;
   return TRUE;
}




/*************************************************************************************
CSubtitle::End - Nice shutodwn
*/
void CSubtitle::End (void)
{
   if (m_hWnd)
      DestroyWindow (m_hWnd);
   m_hWnd = NULL;

   m_hWndParent = NULL;
}

/*************************************************************************************
CSubtitle::FontSet - Changes the font size

inputs
   DWORD          dwFontSize - From 0 .. SUBTITLEFONTS-1. 0 => hide
returns
   BOOL - TRUE if success
*/
BOOL CSubtitle::FontSet (DWORD dwFontSize)
{
   if (dwFontSize == m_dwFontSize)
      return TRUE;
   if (dwFontSize >= SUBTITLEFONTS)
      return FALSE;

   m_dwFontSize = dwFontSize;
   RECT r;
   CalcSizeAndLoc (&r);
   Refresh (&r);

   return TRUE;
}


/*************************************************************************************
CSubtitle::FontGet - Returns the current font size. See FontSet()
*/
DWORD CSubtitle::FontGet (void)
{
   return m_dwFontSize;
}


/*************************************************************************************
CSubtitle::Phrase - Call this when a phrase starts speaking, or finishes speaking.

inputs
   GUID           *pgID - Guid of the speaker
   PWSTR          pszSpeaker - String for the speaker
   PWSTR          pszSpoken - String for what was spoken
   BOOL           fFinished - If TRUE, the phrase has just finished speaking. If FALSE
                  it has just started
returns
   none
*/
void CSubtitle::Phrase (GUID *pgID, PWSTR pszSpeaker, PWSTR pszSpoken, BOOL fFinished)
{
   GUID gNull = GUID_NULL;
   if (!pgID)
      pgID = &gNull;
   if (!pszSpeaker)
      pszSpeaker = L"";
   // if no spoken, or is just a space or just a period, then ignore
   if (!pszSpoken || (((iswspace (pszSpoken[0]) || (pszSpoken[0] == L'.')) && !pszSpoken[1])))
      return;  // ignore

   // if speaking then just add
   RECT r;
   if (!fFinished) {
      // if the phrase window is off then don't bother adding
      if (!m_dwFontSize)
         return;

      SUBTITLEPHRASE stp;
      memset (&stp, 0, sizeof(stp));
      stp.gID = *pgID;
      stp.iTimeLeft = 3000;   // phrase must be up for at least 3 seconds

      m_lSUBTITLEPHRASE.Add (&stp);
      m_lSpeaker.Add (pszSpeaker, (wcslen(pszSpeaker)+1)*sizeof(WCHAR));
      m_lSpoken.Add (pszSpoken, (wcslen(pszSpoken)+1)*sizeof(WCHAR));

      CalcSizeAndLoc (&r);
      Refresh (&r);
      return;
   }

   // see if can find
   PSUBTITLEPHRASE pstp = (PSUBTITLEPHRASE)m_lSUBTITLEPHRASE.Get(0);
   DWORD i;
   for (i = 0; i < m_lSUBTITLEPHRASE.Num(); i++, pstp++) {
      if (!IsEqualGUID(pstp->gID, *pgID))
         continue;   // different guid

      if (_wcsicmp((PWSTR)m_lSpeaker.Get(i), pszSpeaker))
         continue;   // different name

      if (_wcsicmp((PWSTR)m_lSpoken.Get(i), pszSpoken))
         continue;   // different string

      // else, remove this
      pstp->fFinished = TRUE;
      pstp->iTimeLeft = 3000; // 3 second after is done talking

      DeleteOldPhraseIfNecessary ();
      return;
   } // i

   return;
}



/*************************************************************************************
CSubtitle::This sees if any phrases need deleting. If they do, they're remved
and the display is refreshed
*/
void CSubtitle::DeleteOldPhraseIfNecessary (void)
{
   BOOL fChanged = FALSE;
   PSUBTITLEPHRASE pstp = (PSUBTITLEPHRASE)m_lSUBTITLEPHRASE.Get(0);
   DWORD i;
   for (i = m_lSUBTITLEPHRASE.Num()-1; i < m_lSUBTITLEPHRASE.Num(); i--) {
      if ((pstp[i].iTimeLeft > 0) || !pstp->fFinished)
         continue;   // keep this

      // delete
      m_lSUBTITLEPHRASE.Remove (i);
      m_lSpeaker.Remove (i);
      m_lSpoken.Remove (i);
      pstp = (PSUBTITLEPHRASE)m_lSUBTITLEPHRASE.Get(0);
      fChanged = TRUE;
   } // i

   if (fChanged) {
      RECT r;
      CalcSizeAndLoc (&r);
      Refresh (&r);
   }
}



/*************************************************************************************
CSubtitle::CalcSizeAndLoc - Calculates the window's size and location. If the window
should be destroyed, all pr->XXX values are set to 0

inputs
   RECT              *pr - Filled with the rectangle
returns
   none
*/
void CSubtitle::CalcSizeAndLoc (RECT *pr)
{
   // if nothing then 0-sized
   if (!m_lSUBTITLEPHRASE.Num() || !m_dwFontSize) {
      pr->top = pr->left = pr->bottom = pr->right = 0;
      return;
   }

   // get main's client area
   RECT rParent;
   GetClientRect (m_hWndParent, &rParent);
   ClientToScreen (m_hWndParent, (LPPOINT)&rParent.left);
   ClientToScreen (m_hWndParent, (LPPOINT)&rParent.right);

   // determine the width
   pr->left = 0;
   //pr->right = rParent.right - rParent.left;
   //pr->right = pr->right / 4 * (int)m_dwFontSize;
   pr->right = 640 / BUFFERSIZE(2) * BUFFERSIZE(m_dwFontSize);

   // determine the height
   pr->top = 0;
   pr->bottom = 0;
   int iWidthTotal = pr->right - pr->left - 3 * BUFFERSIZE(m_dwFontSize);
   int iWidthSpeaker = iWidthTotal / 5;
   int iWidthSpoken = iWidthTotal - iWidthSpeaker;
   if ((iWidthSpeaker <= 0) || (iWidthSpoken <= 0)) {
      pr->top = pr->left = pr->bottom = pr->right = 0;
      return;
   }
   DWORD i;
   HWND hWnd = m_hWnd ? m_hWnd : m_hWndParent;
   HDC hDC = GetDC (hWnd);
   HFONT hFontOld;
   int iOldMode = SetBkMode (hDC, TRANSPARENT);
   RECT r;
   for (i = 0; i < m_lSUBTITLEPHRASE.Num(); i++) {
      int iMax;

      // increase bottom by one space
      pr->bottom += (int) (i ? BUFFERBETWEENLINESSIZE(m_dwFontSize) : BUFFERSIZE(m_dwFontSize));

      // maximum height for speaker
      r.top = 0;
      r.bottom = 1000;
      r.left = 0;
      r.right = iWidthSpeaker;
      hFontOld = (HFONT) SelectObject (hDC, m_ahFontItalic[m_dwFontSize]);
      DrawTextW (hDC, (PWSTR)m_lSpeaker.Get(i), -1, &r, DT_CALCRECT | DT_LEFT | DT_WORDBREAK);
      iMax = r.bottom - r.top;

      // maximum height for text
      r.top = 0;
      r.bottom = 1000;
      r.left = 0;
      r.right = iWidthSpoken;
      SelectObject (hDC, m_ahFontBold[m_dwFontSize]);
      DrawTextW (hDC, (PWSTR)m_lSpoken.Get(i), -1, &r, DT_CALCRECT | DT_LEFT | DT_WORDBREAK);
      SelectObject (hDC, hFontOld);
      iMax = max(iMax, r.bottom - r.top);

      // increase height
      pr->bottom += iMax;
   } // i
   SetBkMode (hDC, iOldMode);
   ReleaseDC (hWnd, hDC);
   pr->bottom += BUFFERSIZE(m_dwFontSize);   // one last bit of buffer

   // determmine the acceptable client area
   int iBuffer = min(rParent.right - rParent.left, rParent.bottom - rParent.top) / 20;
   rParent.left += iBuffer;
   rParent.right -= iBuffer;
   rParent.top += iBuffer;
   rParent.bottom -= iBuffer;

   // figure out the offset based on
   POINT pOffset;
   switch (m_dwCorner) {
   case 0:  // UL
   default:
      pOffset.x = rParent.left - pr->left;
      pOffset.y = rParent.top - pr->top;
      break;

   case 1:  // UR
      pOffset.x = rParent.right - pr->right;
      pOffset.y = rParent.top - pr->top;
      break;

   case 2:  // LR
      pOffset.x = rParent.right - pr->right;
      pOffset.y = rParent.bottom - pr->bottom;
      break;

   case 3:  // LL
      pOffset.x = rParent.left - pr->left;
      pOffset.y = rParent.bottom - pr->bottom;
      break;
   } // m_dwCorner
   OffsetRect (pr, pOffset.x, pOffset.y);
   // done
}


/*************************************************************************************
CSubtitle::Refresh - Causes the window to be refreshed at the new location.

inputs
   RECT        *pr - New window location. If this is all 0's then should be deleted
returns
   none
*/
void CSubtitle::Refresh (RECT *pr)
{
   // see if should delete
   if ((pr->right == 0) && (pr->left == 0)) {
      if (m_hWnd)
         DestroyWindow (m_hWnd);
      m_hWnd = NULL;
      return;
   }

   // get the current window's location and see if has changed
   if (m_hWnd) {
      RECT r;
      GetWindowRect (m_hWnd, &r);
      if ((r.left != pr->left) || (r.right != pr->right) || (r.bottom != pr->bottom) || (r.top != pr->top))
         MoveWindow (m_hWnd, pr->left, pr->top, pr->right-pr->left, pr->bottom-pr->top, TRUE);

      // invalidate
      InvalidateRect (m_hWnd, NULL, FALSE);
      return;
   }


   // else, need to create
   if (!IsWindow(m_hWndParent))
      return;  // error
   m_hWnd = CreateWindow (gpszSubtitleWindow, "",
      WS_POPUP,
      pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top,
      m_hWndParent, (HMENU) NULL, ghInstance, (PVOID)this);

   ShowWindow (m_hWnd, SW_SHOWNA);
   // done
}


/*************************************************************************************
CSubtitle::WndProc - standard API
*/
LRESULT CSubtitle::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      SetTimer (hWnd, TIMERFADEID, TIMERFADETIME, NULL);
      break;

   case WM_DESTROY:
      KillTimer (hWnd, TIMERFADEID);
      break;

   case WM_TIMER:
      if (wParam == TIMERFADEID) {
         // reduce the times in the phrases
         DWORD i;
         PSUBTITLEPHRASE pstp = (PSUBTITLEPHRASE)m_lSUBTITLEPHRASE.Get(0);
         for (i = 0; i < m_lSUBTITLEPHRASE.Num(); i++, pstp++)
            pstp->iTimeLeft -= TIMERFADETIME;

         // see if any phrases need to be deleted
         DeleteOldPhraseIfNecessary ();
         return 0;
      }
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC = BeginPaint (hWnd, &ps);

         FillRect (hDC, &ps.rcPaint, m_hbrBackground);

         RECT rClient;
         GetClientRect (hWnd, &rClient);
         
         // draw the outline
#define BORDERPENWIDTH     2
#define BORDERPENWIDTHHALF    (BORDERPENWIDTH)
         HPEN hPen = CreatePen (PS_INSIDEFRAME, BORDERPENWIDTH, RGB(0x40, 0, 0));
         HPEN hPenOld = (HPEN) SelectObject (hDC, hPen);
         SelectObject (hDC, GetStockObject (HOLLOW_BRUSH));
         Rectangle (hDC, rClient.left + BORDERPENWIDTHHALF, rClient.top + BORDERPENWIDTHHALF,
            rClient.right - BORDERPENWIDTHHALF, rClient.bottom - BORDERPENWIDTHHALF);
         //MoveToEx (hDC, rClient.left + BORDERPENWIDTHHALF, rClient.top + BORDERPENWIDTHHALF, NULL);
         //LineTo (hDC, rClient.right - BORDERPENWIDTHHALF - 1, rClient.top + BORDERPENWIDTHHALF);
         //LineTo (hDC, rClient.right - BORDERPENWIDTHHALF - 1, rClient.bottom - BORDERPENWIDTHHALF - 1);
         //LineTo (hDC, rClient.left + BORDERPENWIDTHHALF, rClient.bottom - BORDERPENWIDTHHALF - 1);
         //LineTo (hDC, rClient.left + BORDERPENWIDTHHALF, rClient.top + BORDERPENWIDTHHALF);
         SelectObject (hDC, hPenOld);
         DeleteObject (hPen);


         int iWidthTotal = rClient.right - rClient.left - 3 * BUFFERSIZE(m_dwFontSize);
         int iWidthSpeaker = iWidthTotal / 5;
         int iWidthSpoken = iWidthTotal - iWidthSpeaker;
         if ((iWidthSpeaker <= 0) || (iWidthSpoken <= 0))
            goto donepaint;

         DWORD i;
         HFONT hFontOld;
         int iOldMode = SetBkMode (hDC, TRANSPARENT);
         RECT r, rSpeaker, rSpoken;
         PSUBTITLEPHRASE pstp = (PSUBTITLEPHRASE)m_lSUBTITLEPHRASE.Get(0);
         for (i = 0; i < m_lSUBTITLEPHRASE.Num(); i++, pstp++) {
            int iMax;

            // figure out the color
            GUID *pg = &pstp->gID;
            COLORREF cColor = RGB(0,0,0);
            if (IsEqualGUID(*pg, CLSID_PlayerAction))
               cColor = RGB(0x80, 0x80, 0x80);
            else if (IsEqualGUID(*pg, GUID_NULL))
               cColor = RGB(0xc0, 0xc0, 0xc0);
            else {
               COLORREF ac[12] = {
                  RGB(0xff, 0xff, 0), RGB(0, 0xff, 0xff), RGB(0xff, 0, 0xff),
                  RGB(0xff, 0, 0), RGB(0, 0xff, 0), RGB(0, 0, 0xff),
                  RGB(0xc0, 0xc0, 0x40), RGB(0x40, 0xc0, 0xc0), RGB(0xc0, 0x40, 0xc0),
                  RGB(0xc0, 0x40, 0x40), RGB(0x40, 0xc0, 0x40), RGB(0x40, 0x40, 0xc0)
               };
               PBYTE pb = (PBYTE) pg;
               DWORD dwSum = 0;
               DWORD j;
               for (j = 0; j < sizeof(*pg); j++)
                  dwSum += (DWORD)pb[j];

               cColor = ac[dwSum % 12];
            }
            cColor = RGB(GetRValue(cColor)/2, GetGValue(cColor)/2, GetBValue(cColor)/2);
            SetTextColor (hDC, cColor);

            // increase bottom by one space
            rClient.top += (int) (i ? BUFFERBETWEENLINESSIZE(m_dwFontSize) : BUFFERSIZE(m_dwFontSize));

            rSpeaker = rClient;
            rSpeaker.left = BUFFERSIZE(m_dwFontSize);
            rSpeaker.right = rSpeaker.left + iWidthSpeaker;

            rSpoken = rClient;
            rSpoken.right = rClient.right - (int)BUFFERSIZE(m_dwFontSize);
            rSpoken.left = rSpoken.right - iWidthSpoken;

            // maximum height for speaker
            r = rSpeaker;
            hFontOld = (HFONT) SelectObject (hDC, m_ahFontItalic[m_dwFontSize]);
            DrawTextW (hDC, (PWSTR)m_lSpeaker.Get(i), -1, &r, DT_CALCRECT | DT_LEFT | DT_WORDBREAK);
            DrawTextW (hDC, (PWSTR)m_lSpeaker.Get(i), -1, &rSpeaker, DT_LEFT | DT_WORDBREAK);
            iMax = r.bottom - r.top;

            // maximum height for text
            r = rSpoken;
            SelectObject (hDC, m_ahFontBold[m_dwFontSize]);
            DrawTextW (hDC, (PWSTR)m_lSpoken.Get(i), -1, &r, DT_CALCRECT | DT_LEFT | DT_WORDBREAK);
            DrawTextW (hDC, (PWSTR)m_lSpoken.Get(i), -1, &rSpoken, DT_LEFT | DT_WORDBREAK);
            SelectObject (hDC, hFontOld);
            iMax = max(iMax, r.bottom - r.top);
            

            // increase height
            rClient.top += iMax;
         } // i
         SetBkMode (hDC, iOldMode);


donepaint:
         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_MOUSEMOVE:
      {
         // move the window to the next quadrant
         DWORD dwTime = GetTickCount();
         if ((m_dwLastAutoMove > dwTime) || (m_dwLastAutoMove + 1000 < dwTime)) {
            RECT r;
            m_dwCorner = (m_dwCorner+1) % 4;
            CalcSizeAndLoc (&r);
            Refresh (&r);
            m_dwLastAutoMove = dwTime;
         }
      }
      return 0;
   };

   // default
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


