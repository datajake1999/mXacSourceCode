/***************************************************************************
CIconButton - C++ object for viewing the IconButton.

begun 31/8/2001
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#define     VISBUF      3
#define     MYICONBUTTONCLASS    "CIconButton"

typedef struct {
   DWORD       dwRes;      // resource ID for bitmap
   COLORREF    cr;         // color to blend with
   DWORD       dwID;       // sort sort of identifier
   HBITMAP     hBit;       // bitmap cache. Must be freed.
   DWORD       dwCount;    // reference count. If 0 then can free.
} BTNCACHE, *PBTNCACHE;

static CListFixed  lButtonCache;    // for storing BTNCACHE
static BOOL        fButtonCacheInit = FALSE; // set to true if list iniitlaized

/****************************************************************************
ButtonBitmapCacheFind - Given a bitmap, color for blending, and ID, this
sees if already exists. If does then returns exsiting bitmap, else returns NULL.

IMPORTANT: Must call ButtonBitmapCacheRelease() to release the refrence count if it
is found.

BUGFIX - Put in to reduce amount of resources used, which was causing a hard hang
at times.

inputs
   DWORD       dwRes - Bitmap resource ID
   COLORREF    cr - Color
   DWORD       dwID - Some sort of identifier for type of operation with color
returns
   HBITMAP - Bitmap to use, or NULL if cant find
*/
HBITMAP ButtonBitmapCacheFind (DWORD dwRes, COLORREF cr, DWORD dwID)
{
   DWORD i;
   PBTNCACHE pc = (PBTNCACHE) lButtonCache.Get(0);
   for (i = 0; i < lButtonCache.Num(); i++) {
      if ((pc[i].cr == cr) && (pc[i].dwID == dwID) && (pc[i].dwRes == dwRes)) {
         pc[i].dwCount++;
         return pc[i].hBit;
      }
   }

   return NULL;
}

/****************************************************************************
ButtonBitmapCacheAdd - Adds a new bitmap to the cache.

IMPORTANT: Release must be called when done.

NOTE: ONLY call this is ButtonBitmapCacheFind() returns NULL.

inputs
   DWORD       dwRes - Bitmap resource ID
   COLORREF    cr - Color
   DWORD       dwID - Some sort of identifier for type of operation with color
   HBITMAP     hBit - Bitmap
returns
   BOOL - TRUE if success
*/
BOOL ButtonBitmapCacheAdd (DWORD dwRes, COLORREF cr, DWORD dwID, HBITMAP hBit)
{
   if (!fButtonCacheInit) {
      fButtonCacheInit = TRUE;
      lButtonCache.Init (sizeof(BTNCACHE));
   }

   // set up info
   BTNCACHE bc;
   memset (&bc, 0, sizeof(bc));
   bc.cr = cr;
   bc.dwCount = 1;
   bc.dwID = dwID;
   bc.dwRes = dwRes;
   bc.hBit = hBit;

   lButtonCache.Add (&bc);

   return TRUE;
}

/****************************************************************************
ButtonBitmapCacheRelease - Release bitmap allocated by ButtonBitmapCacheFind or
ButtonBitmapCacheRelease

inputs
   HBITMAP        hBit - Bitmap to release
returns
   BOOL - TRUE if found
*/
BOOL ButtonBitmapCacheRelease (HBITMAP hBit)
{
   DWORD i;
   PBTNCACHE pc = (PBTNCACHE) lButtonCache.Get(0);
   for (i = 0; i < lButtonCache.Num(); i++) {
      if (pc[i].hBit == hBit) {
         if (pc[i].dwCount)
            pc[i].dwCount--;
         // NOTE: Dont release, since keep around for faster performance
         //if (!pc[i].dwCount) {
         //   DeleteObject (pc[i].hBit);
         //   lButtonCache.Remove (i);
         //}
         return TRUE;
      }
   }

   return FALSE;
}

/****************************************************************************
ButtonBitmapReleaseAll - Call on shutdown to free all
*/
void ButtonBitmapReleaseAll (void)
{
   DWORD i;
   PBTNCACHE pc = (PBTNCACHE) lButtonCache.Get(0);
   for (i = 0; i < lButtonCache.Num(); i++) {
      DeleteObject (pc[i].hBit);
   }
   lButtonCache.Clear();
}

/****************************************************************************
Constructor and destructor
*/
CIconButton::CIconButton (void)
{
   m_hWnd = NULL;
   m_hbmpMask = m_hbmpColorLight = m_hbmpColorMed = m_hbmpColorDark = m_hbmpBW = m_hbmpRed = NULL;
   m_hbmpLowContrast = NULL;
   m_dwID = 0;
   m_dwDir = 0;
   m_pTip = NULL;
   m_iHeight = m_iWidth = 0;
   m_fTimer = FALSE;
   m_dwTimerOn = 0;
   m_dwFlags = 0;
   m_szAccel[0] = 0;
   m_vkAccel = 0;
   m_dwAccelMsg = 0;
   m_fAccelControl = 0;
   m_fAccelAlt = 0;
   m_cBackDark = RGB (0x20,0x20,0x20);
   m_cBackMed = RGB (0x40, 0x40, 0x40);
   m_cBackLight = RGB (0x80, 0x80, 0x80);
   m_cBackOutline = RGB (0, 0, 0);
}


CIconButton::~CIconButton (void)
{
   if (m_pTip)
      delete m_pTip;

   if (m_hWnd)
      DestroyWindow (m_hWnd);

   if (m_hbmpMask)
      ButtonBitmapCacheRelease (m_hbmpMask);
   if (m_hbmpColorLight)
      ButtonBitmapCacheRelease (m_hbmpColorLight);
   if (m_hbmpColorMed)
      ButtonBitmapCacheRelease (m_hbmpColorMed);
   if (m_hbmpColorDark)
      ButtonBitmapCacheRelease (m_hbmpColorDark);
   if (m_hbmpBW)
      ButtonBitmapCacheRelease (m_hbmpBW);
   if (m_hbmpRed)
      ButtonBitmapCacheRelease (m_hbmpRed);
   if (m_hbmpLowContrast)
      ButtonBitmapCacheRelease (m_hbmpLowContrast);
}

/****************************************************************************
CIconButton::ColorSet - Sets the color of the icon
inputs
   COLORREF    cDark, cMed, cLight, cOut - Colors
returns
   none
*/
void CIconButton::ColorSet (COLORREF cDark, COLORREF cMed, COLORREF cLight, COLORREF cOut)
{
   m_cBackDark = cDark;
   m_cBackMed = cMed;
   m_cBackLight = cLight;
   m_cBackOutline = cOut;
   if (m_hWnd)
      InvalidateRect (m_hWnd, NULL, FALSE);
}

   
/****************************************************************************
CIconButtonWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CIconButton, and calls that.
*/
LRESULT CALLBACK CIconButtonWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCIconButton p = (PCIconButton) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCIconButton p = (PCIconButton) (size_t) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCIconButton) lpcs->lpCreateParams;
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
void InvalidateParent (HWND hWnd)
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
CIconButton::WndProc - Subclass proc for rich-edit
*/
LRESULT CIconButton::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

   switch (uMsg) {

   case WM_CREATE:
      // set the user data to indicate if mouse is over
      if (m_pTip)
         delete m_pTip;
      m_pTip = NULL;
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC   hDCPS;
         hDCPS = BeginPaint (hWnd, &ps);
         BOOL fEnabled = IsWindowEnabled(m_hWnd);

         // blank background
         RECT  rOrig;
         GetClientRect (hWnd, &rOrig);

         // if the rectangle is smaller than the bitmap then shinrk bitmap and antialias
         int iWidth, iHeight;
         HDC hDC, hDCSmall;
         HBITMAP hDraw, hSmall;
         RECT  r;
         int   iScale;
         hDraw = hSmall = NULL;
         hDCSmall = NULL;
         iScale = 1;
         iWidth = rOrig.right - rOrig.left;
         iHeight = rOrig.bottom - rOrig.top;
         // BUGFIX - Take out iWidth+2, iHeight+2
         if ((iWidth < m_iWidth) || (iHeight < m_iHeight)) {
            while ((iScale * iWidth < m_iWidth) || (iScale * iHeight < m_iHeight))
               iScale ++; // BUGFIX - Was iScale*=2, but make antialasing by 1/3 size too

            r.left = r.top = 0;
            r.right = iWidth * iScale;
            r.bottom = iHeight * iScale;

            hDC = CreateCompatibleDC (hDCPS);
            hDCSmall = CreateCompatibleDC (hDCPS);
            hDraw = CreateCompatibleBitmap (hDCPS, r.right, r.bottom);
            hSmall = CreateCompatibleBitmap (hDCPS, iWidth, iHeight);
            SelectObject (hDC, hDraw);
            SelectObject (hDCSmall, hSmall);
         }
         else {
            r = rOrig;
            hDC = hDCPS;
         }

         // blank out the background
         HBRUSH hbr;
         COLORREF cr;
         cr = m_cBackDark;
         hbr = (HBRUSH) CreateSolidBrush (cr);
         FillRect (hDC, &r, hbr);
         DeleteObject (hbr);

#define TVOID     2

         // draw the tab
         if (m_dwFlags & (IBFLAG_BLUELIGHT | IBFLAG_DISABLEDLIGHT | IBFLAG_REDARROW | IBFLAG_DISABLEDARROW)) {
            cr = (fEnabled && (m_dwFlags & (IBFLAG_BLUELIGHT | IBFLAG_REDARROW))) ?
               m_cBackLight : m_cBackMed;
            if (!fEnabled)
               cr = m_cBackDark;

            POINT    ap[6];
            DWORD dwNum = 5;
            POINT pOffset;
            POINT pLastOffset;
            pOffset.x = pOffset.y = 0;
            pLastOffset.x = pLastOffset.y = 0;
            switch (m_dwFlags & IBFLAG_SHAPEMASK) {
            default:
            case IBFLAG_SHAPE_TOP:   // shaped so tab goes on top
               ap[0].x = r.left;
               ap[0].y = r.bottom;
               ap[1] = ap[0];
               ap[1].y = r.top;
               ap[2] = ap[1];
               ap[2].x = (r.right * 3 + r.left+3) / 4;   // BUGFIX - Added +3 for rounding
               ap[3].x = r.right;
               ap[3].y = (r.top * 3 + r.bottom) / 4;
               ap[4] = ap[3];
               ap[4].y = r.bottom;

               pLastOffset.x = -1;
               break;
            case IBFLAG_SHAPE_RIGHT:   // shaped so tab goes on right
               ap[0].x = r.left;
               ap[0].y = r.top;
               ap[1] = ap[0];
               ap[1].x = r.right;
               ap[2] = ap[1];
               ap[2].y = (r.top + r.bottom * 3) / 4;
               ap[3].x = (r.right * 3 + r.left) / 4;
               ap[3].y = r.bottom;
               ap[4] = ap[3];
               ap[4].x = r.left;
               // NOTE: Since dont have buttons on right, not sure how to set offset
               break;
            case IBFLAG_SHAPE_BOTTOM:   // shaped so tab goes on bottom
               ap[0].x = r.right;
               ap[0].y = r.top;
               ap[1] = ap[0];
               ap[1].y = r.bottom;
               ap[2] = ap[1];
               ap[2].x = (r.right + r.left * 3) / 4;
               ap[3].x = r.left;
               ap[3].y = (r.bottom * 3 + r.top+3) / 4;   // BUGFIX - Added +3 for rounding
               ap[4] = ap[3];
               ap[4].y = r.top;

               pOffset.x = -1;
               pOffset.y = -1;
               pLastOffset.x = 1;
               break;
            case IBFLAG_SHAPE_LEFT:   // so tab on left
               ap[0].x = r.right;
               ap[0].y = r.bottom;
               ap[1] = ap[0];
               ap[1].x = r.left;
               ap[2] = ap[1];
               ap[2].y = (r.bottom + r.top*3) / 4;
               ap[3].x = (r.left * 3 + r.right) / 4;
               ap[3].y = r.top;
               ap[4] = ap[3];
               ap[4].x = r.right;
               pOffset.y = -1;
               pLastOffset.y = 1;
               break;

            case IBFLAG_SHAPE_TLOOP:   // top end of a loop
               dwNum = 6;
               ap[0].x = r.right - TVOID;
               ap[0].y = r.bottom;
               ap[1] = ap[0];
               ap[1].y = r.top + TVOID;
               ap[2].x = r.right - 2 * TVOID;
               ap[2].y = r.top;
               ap[3] = ap[2];
               ap[3].x = r.left + 2 * TVOID;
               ap[4].x = r.left + TVOID;
               ap[4].y = ap[1].y;
               ap[5] = ap[4];
               ap[5].y = r.bottom;
               break;
            case IBFLAG_SHAPE_BLOOP:   // bottom end of a loop
               dwNum = 6;
               ap[0].y = r.top;
               ap[0].x = r.right - TVOID;
               ap[1] = ap[0];
               ap[1].y = r.bottom - TVOID;
               ap[2].y = r.bottom;
               ap[2].x = r.right - 2 * TVOID;
               ap[3] = ap[2];
               ap[3].x = r.left + 2 * TVOID;
               ap[4].y = ap[1].y;
               ap[4].x = r.left + TVOID;
               ap[5] = ap[4];
               ap[5].y = r.top;

               pOffset.y = -1;
               break;

            case IBFLAG_SHAPE_VLOOP:   // center end of a vertical loop
               dwNum = 4;
               ap[0].y = r.top;
               ap[0].x = r.left + TVOID;
               ap[1] = ap[0];
               ap[1].y = r.bottom;
               ap[2] = ap[1];
               ap[2].x = r.right - TVOID;
               ap[3] = ap[2];
               ap[3].y = r.top;
               break;

            case IBFLAG_SHAPE_LLOOP:   // left end of a loop
               dwNum = 6;
               ap[0].x = r.right;
               ap[0].y = r.bottom - TVOID;
               ap[1] = ap[0];
               ap[1].x = r.left + TVOID;
               ap[2].x = r.left;
               ap[2].y = r.bottom - 2 * TVOID;
               ap[3] = ap[2];
               ap[3].y = r.top + 2 * TVOID;
               ap[4].x = ap[1].x;
               ap[4].y = r.top + TVOID;
               ap[5] = ap[4];
               ap[5].x = r.right;
               break;
            case IBFLAG_SHAPE_RLOOP:   // right end of a loop
               dwNum = 6;
               ap[0].x = r.left;
               ap[0].y = r.bottom - TVOID;
               ap[1] = ap[0];
               ap[1].x = r.right - TVOID;
               ap[2].x = r.right;
               ap[2].y = r.bottom - 2 * TVOID;
               ap[3] = ap[2];
               ap[3].y = r.top + 2 * TVOID;
               ap[4].x = ap[1].x;
               ap[4].y = r.top + TVOID;
               ap[5] = ap[4];
               ap[5].x = r.left;

               pOffset.x = -1;
               break;
            case IBFLAG_SHAPE_CLOOP:   // center end of a loop
               dwNum = 4;
               ap[0].x = r.left;
               ap[0].y = r.top + TVOID;
               ap[1] = ap[0];
               ap[1].x = r.right;
               ap[2] = ap[1];
               ap[2].y = r.bottom - TVOID;
               ap[3] = ap[2];
               ap[3].x = r.left;
               break;
            }
            DWORD i;
            for (i = 0; i < dwNum; i++) {
               ap[i].x += pOffset.x;
               ap[i].y += pOffset.y;
            }

            if (m_dwFlags & IBFLAG_ENDPIECE) for (i = dwNum-3; i < dwNum; i++) {
               ap[i].x += pLastOffset.x;
               ap[i].y += pLastOffset.y;
            }
            HBRUSH hbrOld;
            HPEN hPenOld, hPen;
            hPen = CreatePen (PS_NULL, 0, cr);
            hbr = (HBRUSH) CreateSolidBrush (cr);
            hbrOld = (HBRUSH) SelectObject (hDC, hbr);
            hPenOld = (HPEN) SelectObject (hDC, hPen);

            if (dwNum)
               Polygon (hDC, ap, (int) dwNum);

            SelectObject (hDC, hbrOld);
            SelectObject (hDC, hPenOld);
            DeleteObject (hbr);
            DeleteObject (hPen);

            // outline
            hPen = CreatePen (PS_SOLID, 0, m_cBackOutline);
            SelectObject (hDC, hPen);  // BUGFIX
            for (i = 0; i < dwNum; i++) {
               POINT p;
               p = ap[i];
               if (i+1 >= dwNum) {
                  // continue just one pixel mode
                  int iDirX, iDirY;
                  iDirX = p.x - ap[i-1].x;
                  iDirY = p.y - ap[i-1].y;
                  if (iDirX)
                     iDirX = (iDirX > 0) ? 1 : -1;
                  if (iDirY)
                     iDirY = (iDirY > 0) ? 1 : -1;
                  p.x += iDirX;
                  p.y += iDirY;
               }

               // offset
               //p.x += pOffset.x;
               //p.y += pOffset.y;

               if (i)
                  LineTo (hDC, p.x, p.y);
               else
                  MoveToEx (hDC, p.x, p.y, NULL);
            }
            SelectObject (hDC, hPenOld);
            DeleteObject (hPen);
            
         }


         // Draw icon
         HDC   hDCImage;
         int   iLeft, iTop;
         iLeft = ((r.right - r.left) - m_iWidth) / 2;
         iTop = ((r.bottom - r.top) - m_iHeight) / 2;

         // draw the ligth or arrow
         int iLeft2, iTop2;
         iLeft2 = ((r.right - r.left) - 32) / 2;
         iTop2 = ((r.bottom - r.top) - 32) / 2;

         // mask
         hDCImage = CreateCompatibleDC (hDC);
         SelectObject (hDCImage, m_hbmpMask);
         BitBlt (hDC, iLeft, iTop, m_iWidth, m_iHeight, hDCImage, 0, 0, SRCAND);

         // image
         HBITMAP hBitColor;
         if (cr == m_cBackLight)
            hBitColor = m_hbmpColorLight;
         else if (cr == m_cBackMed)
            hBitColor = m_hbmpColorMed;
         else
            hBitColor = m_hbmpColorDark;
         // BUGFIX - Get rid of low constrast since trying to reduce bitmap resource usage
         //if ((m_dwFlags & (IBFLAG_DISABLEDARROW | IBFLAG_DISABLEDLIGHT)) &&
         //   !(m_dwFlags & (IBFLAG_BLUELIGHT | IBFLAG_REDARROW)))
         //   hBitColor = m_hbmpLowContrast;
         SelectObject (hDCImage,
            fEnabled ? (m_fTimer ? m_hbmpRed : hBitColor) : m_hbmpBW);
         BitBlt (hDC, iLeft, iTop, m_iWidth, m_iHeight, hDCImage, 0, 0, SRCPAINT);
         DeleteDC (hDCImage);

         // BUGFIX - Draw m or r to indicate the use of middle or right button
         if (m_dwFlags & IBFLAG_MBUTTON)
            DrawIcon (hDC, iLeft + m_iWidth-32, iTop + m_iHeight-32, LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_MBUTTON)));
         if (m_dwFlags & IBFLAG_RBUTTON)
            DrawIcon (hDC, iLeft + m_iWidth-32, iTop + m_iHeight-32, LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_RBUTTON)));

         // may want to bitblip
         if (hDC != hDCPS) {
            // downsample
            int iX, iY, iX2, iY2;
            for (iX = 0; iX < iWidth; iX++) for (iY = 0; iY < iHeight; iY++) {
               WORD wR, wG, wB;
               wR = wG = wB = 0;
               for (iX2 = 0; iX2 < iScale; iX2++) for (iY2 = 0; iY2 < iScale; iY2++) {
                  COLORREF cr;
                  cr = GetPixel (hDC, iX * iScale + iX2, iY * iScale + iY2);
                  wR += GetRValue(cr);
                  wG += GetGValue(cr);
                  wB += GetBValue(cr);
               }
               wR /= (iScale * iScale);
               wG /= (iScale * iScale);
               wB /= (iScale * iScale);
               SetPixel (hDCSmall, iX, iY, RGB(wR, wG, wB));
            }

            BitBlt (hDCPS, rOrig.left, rOrig.top, iWidth, iHeight, hDCSmall, 0, 0, SRCCOPY);
         }

         EndPaint (hWnd, &ps);

         if (hDC != hDCPS)
            DeleteDC (hDC);
         if (hDCSmall)
            DeleteDC (hDCSmall);
         if (hDraw)
            DeleteObject (hDraw);
         if (hSmall)
            DeleteObject (hSmall);
      }
      break;

   case WM_TIMER: // track if cursor over
      {
         m_dwTimerOn += 125;

         POINT p;
         RECT  rw;
         GetCursorPos (&p);
         GetWindowRect (hWnd, &rw);
         if (PtInRect (&rw, p)) {
            if (!m_pTip && (m_dwTimerOn >= 1000)) { // BUGFIX - was 0, but dont bring up so quickly
               CMem  memTemp;

               memTemp.Required (strlen((char*) m_memTip.p) + strlen((char*)m_memName.p) + 512);
               char *psz;
               psz = (char*) memTemp.p;
               strcpy (psz, "<bold>");
               strcat (psz, (char*) m_memName.p);

               if (m_szAccel[0] && m_dwAccelMsg) {
                  strcat (psz, " <italic>(");
                  strcat (psz, m_szAccel);
                  strcat (psz, ")</italic>");
               }

               strcat (psz, "</bold>");

               // BUGFIX - If no tip then dont put in extra paragraph
               if (((char*)m_memTip.p)[0]) {
                  strcat (psz, "<p/>");

                  strcat (psz, (char*) m_memTip.p);
               }

               // show tool tip if hover long enough
               m_pTip = new CToolTip;
               RECT r;
               GetWindowRect (m_hWnd, &r);
               m_pTip->Init (psz, m_dwDir, &r, GetParent(m_hWnd));
            }

            return 0;   // still over
         }

         // else moved off
         // BUGFIX - On WinNT or different version of windows must be HIWORD window handle
         if (m_pTip)
            delete m_pTip;
         m_pTip = NULL;
         InvalidateParent (hWnd);
         KillTimer (hWnd, 42);
         m_fTimer = FALSE;
         m_dwTimerOn = 0;
      }
      return 0;

   case WM_DESTROY:
      if (m_fTimer)
         KillTimer (hWnd, 42);

      break;

   case WM_MOUSEMOVE:
      {
         // set the cursor
         SetCursor (LoadCursor (NULL, IsWindowEnabled(m_hWnd) ? IDC_ARROW : IDC_NO));
         // make sure it's painted
         if (!m_fTimer) {
            InvalidateParent (hWnd);


            SetTimer (hWnd, 42, 125, NULL);
            m_fTimer = TRUE;
            m_dwTimerOn = 0;
         }

      }
      break;

   case WM_LBUTTONUP:
   case WM_MBUTTONUP:   //BUGFIX - handle middle and right buttons too
   case WM_RBUTTONUP:
      if (IsWindowEnabled (m_hWnd)) {
         BeepWindowBeep (ESCBEEP_BUTTONDOWN);
         DWORD dwButton;
         dwButton = 0;
         if (uMsg == WM_MBUTTONUP)
            dwButton = 0x50000;
         else if (uMsg == WM_RBUTTONUP)
            dwButton = 0x60000;
         PostMessage (GetParent(hWnd), WM_COMMAND, dwButton | m_dwID, (LPARAM) hWnd);

         if (m_pTip)
            delete m_pTip;
         m_pTip = NULL;
         if (m_fTimer)
            KillTimer (hWnd, 42);
         m_fTimer = FALSE;
         m_dwTimerOn = 0;

         InvalidateParent (hWnd);   // do dont leave red bits behind
      }
      return 0;
   }
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/****************************************************************************8
CIconButton::TestAccelerator - See if this button wants the accelerator

inputs
   DWORD    dwMsg - Message
   WPARAM   wParam
   LPARAM   lParam
returns
   BOOL - TRUE if the button accepts the acclerator and acts on it, false if
         it doesnt want it
*/
BOOL CIconButton::TestAccelerator (DWORD dwMsg, WPARAM wParam, LPARAM lParam)
{
   if (!m_dwAccelMsg || (m_dwAccelMsg != dwMsg) || !IsWindowEnabled (m_hWnd))
      return FALSE;

   BOOL fControl;

   switch (dwMsg) {
   case WM_CHAR:
   case WM_SYSCHAR:
      if (isalpha (wParam) && islower(wParam))
         wParam = toupper(wParam);
      if ((TCHAR)wParam != m_vkAccel)
         return FALSE;
      if ( ((lParam & (1 << 29)) && !m_fAccelAlt) || (!(lParam & (1 << 29)) && m_fAccelAlt))
         return FALSE;
      fControl = (GetKeyState (VK_CONTROL) < 0);
      if (fControl != m_fAccelControl)
         return FALSE;
      // else ok
      goto good;
      break;

   case WM_KEYDOWN:
       if ((TCHAR)wParam != m_vkAccel)
         return FALSE;
      fControl = (GetKeyState (VK_CONTROL) < 0);
      // NOTE: Not testing for alt
      if (fControl != m_fAccelControl)
         return FALSE;
      // else ok
      goto good;
     break;
   }

   return FALSE;

good:
   PostMessage (GetParent(m_hWnd), WM_COMMAND, m_dwID, (LPARAM) m_hWnd);
   return TRUE;
}

/****************************************************************************8
CIconButton::Init - Creates a IconButton window so that it doesn't overlap the given
rectangle.

inputs
   DWORD    dwBmpRes - Bitmap resource ID for the button
   HINSTANCE hInstance - Where to get the icon rom
   char     *pszAccel - Accelerator name. Is of the frm ["ctrl-"] ["alt-"] <char>
                  NOTE: MUST be sanitizied since will be converted directly to MML
   char     *pszName - name string
                  NOTE: MUST be sanitizied since will be converted directly to MML
   char     *pszTip - tooltip string
                  NOTE: MUST be sanitizied since will be converted directly to MML
   DWORD    dwDir - Tooltip direction. 0 is above, 1 is to the right, 2 is below, 3 is to the left
   RECT     *pr - rectangle for the button
   HWND     hWndParent - parent of this window
   DWORD    dwID - ID sent when the button is clicked
   DWORD    dwPixels - Number of pixels across, to control downsampling. Defaults to M3DBUTTONSIZE
returns
   HWND - IconButton
*/
BOOL CIconButton::Init (DWORD dwBmpRes, HINSTANCE hInstance, char *pszAccel, char *pszName, char *pszTip, DWORD dwDir,
                        RECT *pr, HWND hWndParent, DWORD dwID, DWORD dwPixels)
{
   if (m_hWnd)
      return FALSE;

   static BOOL gfRegistered = FALSE;
   if (!gfRegistered ) {
      WNDCLASS wc;

      memset (&wc, 0, sizeof(wc));
      wc.lpfnWndProc = CIconButtonWndProc;
      wc.style = 0;
      wc.hInstance = ghInstance;
      wc.hIcon = NULL;
      wc.hCursor = 0;
      wc.hbrBackground = NULL;
      wc.lpszMenuName = NULL;
      wc.lpszClassName = MYICONBUTTONCLASS;
      RegisterClass (&wc);

      gfRegistered = TRUE;
   }

   // parse the accelerator
   if (pszAccel) {
      strcpy (m_szAccel, pszAccel);
      char *szAlt = "alt-";
      char *szCtrl = "ctrl-";

      if (!_strnicmp(pszAccel, szCtrl, strlen(szCtrl))) {
         m_fAccelControl = TRUE;
         pszAccel += strlen(szCtrl);
      }
      else
         m_fAccelControl = FALSE;

      if (!_strnicmp(pszAccel, szAlt, strlen(szAlt))) {
         m_fAccelAlt = TRUE;
         pszAccel += strlen(szAlt);
      }
      else
         m_fAccelAlt = FALSE;

      // whatever is left is the key
      m_vkAccel = toupper ((int) pszAccel[0]);
      m_dwAccelMsg = WM_CHAR;

      if (!_stricmp(pszAccel, "del")) {
         m_vkAccel = VK_DELETE;
         m_dwAccelMsg = WM_KEYDOWN;
      }
      else if ((toupper(pszAccel[0]) == 'F') && atoi(pszAccel+1)) {
         m_vkAccel = VK_F1 + atoi(pszAccel+1) - 1;
         m_dwAccelMsg = WM_KEYDOWN;
      }

      if (m_fAccelControl && (m_vkAccel >= 'A') && (m_vkAccel <= 'Z'))
         m_vkAccel = m_vkAccel - 'A' + 1;

      // different message if alt
      if (m_fAccelAlt && (m_dwAccelMsg == WM_CHAR))
         m_dwAccelMsg = WM_SYSCHAR;
   }

   m_memTip.Required(strlen(pszTip)+1);
   strcpy ((char*) m_memTip.p, pszTip);
   m_memName.Required(strlen(pszName)+1);
   strcpy ((char*) m_memName.p, pszName);

   m_dwDir = dwDir;
   m_dwID = dwID;

   m_hWnd = CreateWindowEx (0 /*WS_EX_TRANSPARENT*/, MYICONBUTTONCLASS, "",
      WS_VISIBLE | WS_CHILD,
      pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top,
      hWndParent, NULL, ghInstance, (LPVOID)this);
   if (!m_hWnd)
      return FALSE;

   // create the three different bitmaps
   HDC hDCDesk, hDCMask, hDCColorLight, hDCColorMed, hDCColorDark, hDCBW, hDCRed, hDCOrig, hDCLowContrast;
   HBITMAP hbmpOrig;
   //hbmpOrig = LoadBitmap (ghInstance, MAKEINTRESOURCE(dwBmpRes)); // JPegToBitmap (dwJPGRes, ghInstance);
   hbmpOrig = (HBITMAP) LoadImage (hInstance, MAKEINTRESOURCE(dwBmpRes), IMAGE_BITMAP,
      0, 0, LR_DEFAULTCOLOR); // JPegToBitmap (dwJPGRes, ghInstance);
   hDCDesk = GetDC (m_hWnd);
   hDCOrig = CreateCompatibleDC (hDCDesk);

   m_hbmpMask = ButtonBitmapCacheFind (dwBmpRes, 0, 1);
   hDCMask = m_hbmpMask ? NULL : CreateCompatibleDC (hDCDesk);

   m_hbmpColorLight = ButtonBitmapCacheFind (dwBmpRes, m_cBackLight, 2);
   hDCColorLight = m_hbmpColorLight ? NULL : CreateCompatibleDC (hDCDesk);

   m_hbmpColorMed = ButtonBitmapCacheFind (dwBmpRes, m_cBackMed, 2);
   hDCColorMed = m_hbmpColorMed ? NULL : CreateCompatibleDC (hDCDesk);

   m_hbmpColorDark = ButtonBitmapCacheFind (dwBmpRes, m_cBackDark, 2);
   hDCColorDark = m_hbmpColorDark ? NULL : CreateCompatibleDC (hDCDesk);

   m_hbmpBW = ButtonBitmapCacheFind (dwBmpRes, m_cBackDark, 3);
   hDCBW = m_hbmpBW ? NULL : CreateCompatibleDC (hDCDesk);

   m_hbmpRed = ButtonBitmapCacheFind (dwBmpRes, 0 /* so common with object editor*/, 4);
   hDCRed = m_hbmpRed ? NULL : CreateCompatibleDC (hDCDesk);

   // BUGFIX - Get rid of low contrast since trying to reduce bitmap usage
   hDCLowContrast = NULL;
   //m_hbmpLowContrast = ButtonBitmapCacheFind (dwBmpRes, m_cBackMed, 5);
   //hDCLowContrast = m_hbmpLowContrast ? NULL : CreateCompatibleDC (hDCDesk);


   // size of the bitmap
   BITMAP   bm;
   GetObject (hbmpOrig, sizeof(bm), &bm);

   // BUGFIX - Downsample
   DWORD dwDown;
   CImage iGamma; // for gamma correction of downsampling
   for (dwDown = 1; ((dwDown * (dwPixels-2)) < (DWORD) bm.bmHeight) && ((dwDown * (dwPixels-2)) < (DWORD) bm.bmWidth); dwDown++);
      // BUGFIX - Make downsample increase by one instead of double
      // BUGFIX - Make downsample less if larger buttons

   m_iHeight = bm.bmHeight / dwDown;
   m_iWidth = bm.bmWidth / dwDown;

   // create the bitmaps
   // BUGFIX - If no DC then null
   SelectObject (hDCOrig, hbmpOrig);
   if (hDCMask) {
      m_hbmpMask = CreateCompatibleBitmap (hDCDesk, m_iWidth, m_iHeight);
      ButtonBitmapCacheAdd (dwBmpRes, 0, 1, m_hbmpMask);
      SelectObject (hDCMask, m_hbmpMask);
   }
   if (hDCColorLight) {
      m_hbmpColorLight = CreateCompatibleBitmap (hDCDesk, m_iWidth, m_iHeight);
      ButtonBitmapCacheAdd (dwBmpRes, m_cBackLight, 2, m_hbmpColorLight);
      SelectObject (hDCColorLight, m_hbmpColorLight);
   }
   if (hDCColorMed) {
      m_hbmpColorMed = CreateCompatibleBitmap (hDCDesk, m_iWidth, m_iHeight);
      ButtonBitmapCacheAdd (dwBmpRes, m_cBackMed, 2, m_hbmpColorMed);
      SelectObject (hDCColorMed, m_hbmpColorMed);
   }
   if (hDCColorDark) {
      m_hbmpColorDark = CreateCompatibleBitmap (hDCDesk, m_iWidth, m_iHeight);
      ButtonBitmapCacheAdd (dwBmpRes, m_cBackDark, 2, m_hbmpColorDark);
      SelectObject (hDCColorDark, m_hbmpColorDark);
   }
   if (hDCBW) {
      m_hbmpBW = CreateCompatibleBitmap (hDCDesk, m_iWidth, m_iHeight);
      ButtonBitmapCacheAdd (dwBmpRes, m_cBackDark, 3, m_hbmpBW);
      SelectObject (hDCBW, m_hbmpBW);
   }
   if (hDCRed) {
      m_hbmpRed = CreateCompatibleBitmap (hDCDesk, m_iWidth, m_iHeight);
      ButtonBitmapCacheAdd (dwBmpRes, 0 /* so common with object editor*/, 4, m_hbmpRed);
      SelectObject (hDCRed, m_hbmpRed);
   }
   // BUGFIX - Get rid of low constrast to save on bitmaps
   //if (hDCLowContrast) {
   //   m_hbmpLowContrast = CreateCompatibleBitmap (hDCDesk, m_iWidth, m_iHeight);
   //   ButtonBitmapCacheAdd (dwBmpRes, m_cBackMed, 5, m_hbmpLowContrast);
   //   SelectObject (hDCLowContrast, m_hbmpLowContrast);
   //}
   ReleaseDC (m_hWnd, hDCDesk);  // BUGFIX - Was releaseing NULL, probably causing leak

   // if all DC's NULL then skip antialiasing
   if (!hDCMask && !hDCColorLight && !hDCColorMed && !hDCColorDark && !hDCBW &&
      !hDCRed && !hDCLowContrast)
      goto skipanti;

   // get first pixel
   COLORREF cr;
   cr = GetPixel (hDCOrig, 0, 0);

#define  COLORDIST(x,y)    (max(x,y)-min(x,y))

   int   x, y;
   for (y = 0; y < m_iHeight; y++) for (x = 0; x < m_iWidth; x++) {
      COLORREF cNewLight, cNewMed, cNewDark;

      if (dwDown > 1) {
         WORD aw[3];
         DWORD adw[3][3];
         DWORD dwCount, dwWeight, dwCountBack;
         int xx, yy, x2, y2;
         memset (adw, 0, sizeof(adw));
         dwCount = dwCountBack = 0;
         // BUGFIX - Put in slight burring
         //for (xx = 0; xx < (int) dwDown; xx++) for (yy = 0; yy < (int) dwDown; yy++) {
         //      dwWeight = 1;

         // BUGFIX - The following cuases too much blurring
         //for (xx = -1; xx <= (int) dwDown; xx++) for (yy = -1; yy <= (int) dwDown; yy++) {
         //   if ((xx >= 0) && (xx < (int) dwDown) && (yy >= 0) && (yy < (int)dwDown))
         //      dwWeight = 2;
         //   else
         //      dwWeight = 1;

         // BUGFIX - Try medium blurring
         for (xx = 0; xx < (int) dwDown; xx++) for (yy = 0; yy < (int) dwDown; yy++) {
            if ((xx > 0) && (xx < (int) dwDown-1) && (yy > 0) && (yy < (int)dwDown-1))
               dwWeight = 2;
            else
               dwWeight = 1;

            x2 = x*dwDown+xx;
            y2 = y*dwDown+yy;
            if ((x2 >= 0) && (x2 < bm.bmWidth) && (y2 >= 0) && (y2 < bm.bmHeight))
               cNewLight = GetPixel(hDCOrig, x2, y2);
            else
               cNewLight = cr;
            if (cNewLight == cr) {
               cNewLight = m_cBackLight;
               cNewMed = m_cBackMed;
               cNewDark = m_cBackDark;
               dwCountBack += dwWeight;
            }
            else {
               cNewMed = cNewDark = cNewLight;
            }

            // light
            iGamma.Gamma (cNewLight, aw);
            adw[0][0] += (DWORD) aw[0] * dwWeight;
            adw[0][1] += (DWORD) aw[1] * dwWeight;
            adw[0][2] += (DWORD) aw[2] * dwWeight;

            // medium
            if (cNewMed != cNewLight)
               iGamma.Gamma (cNewMed, aw);
            adw[1][0] += (DWORD) aw[0] * dwWeight;
            adw[1][1] += (DWORD) aw[1] * dwWeight;
            adw[1][2] += (DWORD) aw[2] * dwWeight;

            // dark
            if (cNewDark != cNewMed)
               iGamma.Gamma (cNewDark, aw);
            adw[2][0] += (DWORD) aw[0] * dwWeight;
            adw[2][1] += (DWORD) aw[1] * dwWeight;
            adw[2][2] += (DWORD) aw[2] * dwWeight;

            dwCount += dwWeight;
         }

         if (dwCountBack != dwCount) {
            // light
            aw[0] = (WORD) (adw[0][0] / dwCount);
            aw[1] = (WORD) (adw[0][1] / dwCount);
            aw[2] = (WORD) (adw[0][2] / dwCount);
            cNewLight = iGamma.UnGamma (aw);

            // medium
            aw[0] = (WORD) (adw[1][0] / dwCount);
            aw[1] = (WORD) (adw[1][1] / dwCount);
            aw[2] = (WORD) (adw[1][2] / dwCount);
            cNewMed = iGamma.UnGamma (aw);

            // dark
            aw[0] = (WORD) (adw[2][0] / dwCount);
            aw[1] = (WORD) (adw[2][1] / dwCount);
            aw[2] = (WORD) (adw[2][2] / dwCount);
            cNewDark = iGamma.UnGamma (aw);
         }
         else
            cNewLight = cNewMed = cNewDark = cr;
      }
      else
         cNewLight = cNewMed = cNewDark = GetPixel (hDCOrig, x, y);

#define BACKDIST     1
      // color approximately the same
      if ( (COLORDIST(GetRValue(cNewMed), GetRValue(cr)) <= BACKDIST) &&
         (COLORDIST(GetGValue(cNewMed), GetGValue(cr)) <= BACKDIST) &&
         (COLORDIST(GetBValue(cNewMed), GetBValue(cr)) <= BACKDIST)) {

         // it's too different from the backgroun so masdk
         if (hDCMask)
            SetPixel (hDCMask, x, y, RGB(0xff,0xff,0xff));
         if (hDCColorLight)
            SetPixel (hDCColorLight, x, y, 0);
         if (hDCColorMed)
            SetPixel (hDCColorMed, x, y, 0);
         if (hDCColorDark)
            SetPixel (hDCColorDark, x, y, 0);
         if (hDCBW)
            SetPixel (hDCBW, x, y, 0);
         if (hDCRed)
            SetPixel (hDCRed, x, y, 0);
         // BUGFIX - Get rid of low constrast to save on bitmaps
         //if (hDCLowContrast)
         //   SetPixel (hDCLowContrast, x, y, 0);
         continue;
      }

      // else, it's a valid pixel
      if (hDCMask)
         SetPixel (hDCMask, x, y, RGB(0, 0, 0));
      if (hDCColorLight)
         SetPixel (hDCColorLight, x, y, cNewLight);
      if (hDCColorMed)
         SetPixel (hDCColorMed, x, y, cNewMed);
      if (hDCColorDark)
         SetPixel (hDCColorDark, x, y, cNewDark);

      // black and white, and faded
      WORD wFade, wDark;
      wFade = ((WORD)GetRValue(cNewDark) + (WORD)GetGValue(cNewDark) + (WORD)GetBValue(cNewDark)) / 3;
      wDark = ((WORD)GetRValue(m_cBackDark) + (WORD)GetGValue(m_cBackDark) + (WORD)GetBValue(m_cBackDark)) / 3;
      wFade = (wFade + wDark) / 2;
      if (hDCBW)
         SetPixel (hDCBW, x, y, RGB(wFade, wFade, wFade));

      // red color
      if (hDCRed)
         SetPixel (hDCRed, x, y,
            RGB(255, GetGValue(cNewMed), GetBValue(cNewMed)));

      // low contrast
      // BUGFIX - Low contrast arent as dark
      // BUGFIX - Get rid of low contrast to save on bitmaps
      //if (hDCLowContrast)
      //   SetPixel (hDCLowContrast, x, y,
      //      RGB(GetRValue(cNewMed)/2 + GetRValue(m_cBackMed)/2,
      //         GetGValue(cNewMed)/2 + GetGValue(m_cBackMed)/2,
      //         GetBValue(cNewMed)/2 + GetBValue(m_cBackMed)/2));
   }

skipanti:
   if (hDCMask)
      DeleteDC (hDCMask);
   if (hDCColorLight)
      DeleteDC (hDCColorLight);
   if (hDCColorMed)
      DeleteDC (hDCColorMed);
   if (hDCColorDark)
      DeleteDC (hDCColorDark);
   if (hDCBW)
      DeleteDC (hDCBW);
   if (hDCRed)
      DeleteDC (hDCRed);
   if (hDCLowContrast)
      DeleteDC (hDCLowContrast);
   if (hDCOrig)
      DeleteDC (hDCOrig);
   if (hbmpOrig)
      DeleteObject (hbmpOrig);


   return TRUE;
}


/**********************************************************************************
CIconButton::Enable - Enable or disable the button.

inptus
   BOOL     fEnable - if TRUE then enable, else disable
*/
void CIconButton::Enable (BOOL fEnable)
{
   // if already enabled do nothing
   if (IsWindowEnabled(m_hWnd) == fEnable)
      return;

   EnableWindow (m_hWnd, fEnable);
   InvalidateParent (m_hWnd);
}



/***********************************************************************************
CIconButton::Move - Moves a button

inputs
   RECT     *pr - Move to
returns
   BOOL - TRUE if success
*/
BOOL CIconButton::Move (RECT *pr)
{
   // clera old
   InvalidateParent (m_hWnd);

   MoveWindow (m_hWnd, pr->left, pr->top, pr->right - pr->left, pr->bottom - pr->top, TRUE);

   // clear new position
   InvalidateParent (m_hWnd);

   return TRUE;
}

/***********************************************************************************
CIconButton::FlagsSet - Set some display flags for the control.

inputs
   DWORD       dwFlags - One or more IBFLAG_XXX
retursn
   none
*/
void CIconButton::FlagsSet (DWORD dwFlags)
{
   // if no change ignore
   if (dwFlags == m_dwFlags)
      return;

   // else rmember
   m_dwFlags = dwFlags;
   InvalidateParent (m_hWnd);
}

/***********************************************************************************
CIconButton::FlagsGet - Returns the display flags from the control.

inputs
   none
returns
   DWORD - One or more IBFLAG_XXX.
*/
DWORD CIconButton::FlagsGet (void)
{
   return m_dwFlags;
}

