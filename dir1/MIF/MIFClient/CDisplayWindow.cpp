/*************************************************************************************
CDisplayWindow.cpp - Code for displaying the icon client window.

begun 29/5/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
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

extern PWSTR gpszMainDisplayWindow;

static PWSTR gpszDisplayWindowPrefix = L"DisplayWindow:";
static PWSTR gpszDisplayWindow = L"DisplayWindow";

#define TIMER_SCROLL360             2042     // if mouse hovering over then scroll
#define TIMER_DELAYCLEAR            2043     // don't clear 360 right away in case drawing

#define SCROLL360TIME               (125 - min(giCPUSpeed + 1,3)*25)      // number of milliseoncsd
#define DELAYCLEARTIME              1000     // how long to delay before refresh.

#define DELAYEDSCROLLTIME           500      // don't scroll until in scrolling position for 500 ms

#define FEATURE_SCROLLANYWHERE         // if defined, then can click anywhere on the 360 and scroll

/*************************************************************************************
CDisplayWindow::Constructor and destructor
*/
CDisplayWindow::CDisplayWindow (void)
{
   m_pMain = NULL;
   m_hWnd = NULL;
   MemZero (&m_memID);
   MemZero (&m_memName);
   m_pvi = NULL;
   m_dwTimeFirstWantToScroll = 0;
   m_fDelayClearTimerOn = FALSE;
   m_fDelayClearTimerWentOff = TRUE;   // start state
#undef GetSystemMetrics

// BUGFIX - Can always scroll at the bottom and top
//#ifdef FEATURE_SCROLLANYWHERE
//   m_iScrollSize = 0;
//#else
   m_iScrollSize = GetSystemMetrics (SM_CYHSCROLL) * 4;  // BUGFIX - Larger scroll area
//#endif
   m_fScrollCapture = FALSE;
   m_fScrollLastLR = m_fScrollLastUD = 0.0;
}

CDisplayWindow::~CDisplayWindow (void)
{
#if 0 // no longer used
   // save location in registry or user file
   PWSTR pszName = (PWSTR)m_memName.p;
   if (m_pMain && m_hWnd && pszName && pszName[0]) {
      CMem mem;
      MemZero (&mem);
      MemCat (&mem, gpszDisplayWindowPrefix);
      MemCat (&mem, pszName);

      CMMLNode2 node;
      node.NameSet (gpszDisplayWindow);
      //m_pMain->ChildLocSave (m_hWnd, &node, !IsWindowVisible(m_hWnd));

      // save it
      m_pMain->UserSave (TRUE, (PWSTR) mem.p, &node);
   }
#endif // 0

   DeletePCVisView();

   // delete the window
   if (m_hWnd)
      DestroyWindow (m_hWnd);
}



/*************************************************************************************
CDisplayWindow::DelayClearTimerKill - Kill the delay clear timer.
*/
void CDisplayWindow::DelayClearTimerKill (void)
{
   if (m_fDelayClearTimerOn && m_hWnd) {
      KillTimer (m_hWnd, TIMER_DELAYCLEAR);
      m_fDelayClearTimerOn = FALSE;
   }
}


/*************************************************************************************
CDisplayWindow::DelayClearTimerStart - Start the delay clear timer
*/
void CDisplayWindow::DelayClearTimerStart (void)
{
   // kill in case running
   DelayClearTimerKill();

   if (m_hWnd) {
      SetTimer (m_hWnd, TIMER_DELAYCLEAR, DELAYCLEARTIME, NULL);
      m_fDelayClearTimerOn = TRUE;
   }
}

/*************************************************************************************
CDisplayWindow::DeletePCVisView - Deletes the viewview
*/
void CDisplayWindow::DeletePCVisView (void)
{
   if (!m_pvi)
      return;

   // NOTE: This lets the main window delete the vis image, since it
   // maintains the master list
   DWORD dwIndex = -1;
   if (m_pMain)
      dwIndex = m_pMain->VisImageFindPtr (m_pvi);
   if (dwIndex != -1)
      m_pMain->VisImageDelete (dwIndex);
   else
      delete m_pvi; // shouldnt happen
   m_pvi = NULL;
}

/************************************************************************************
DisplayWindowWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK DisplayWindowWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCDisplayWindow p = (PCDisplayWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCDisplayWindow p = (PCDisplayWindow) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCDisplayWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CDisplayWindow::Init - Initializes the icon window.

inputs
   PCMainWindow         pMain - Main window
   PCMMLNode2            pNode - Node that's of type "DisplayWindow".
                        NOTE: It's the responsibility of CDisplayWindow to delete this node
   __int64              iTime - Time when this all came in so can keep menus up to date
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CDisplayWindow::Init (PCMainWindow pMain, PCMMLNode2 pNode, __int64 iTime)
{

   // get the ID
   PWSTR pszID = MMLValueGet (pNode, L"ID");
   if (!pszID || !pszID[0]) {
      delete pNode;
      return FALSE;
   }

   PWSTR pszName = MMLValueGet (pNode, L"Name");
   if (!pszName)
      pszName = L"";

   if (!Init (pMain, pszName, pszID, pNode))
      return FALSE;

   // update the contents
   return Update (pNode, iTime);
}


/*************************************************************************************
CDisplayWindow::Init - Initializes the icon window.

inputs
   PCMainWindow         pMain - Main window
   PWSTR                pszID - ID
   PWSTR                pszName - Name
   PCMMLNode2            pNode - Node that's of type "DisplayWindow", used only
                           to determine window loc. Can be NULL
                           NOTE: This is NOT deleted by this fucntion call
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CDisplayWindow::Init (PCMainWindow pMain, PWSTR pszName, PWSTR pszID, PCMMLNode2 pNode)
{
   if (m_hWnd || m_pMain) {
      return FALSE;  // just to check
   }
   m_pMain = pMain;

   // create room
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = DisplayWindowWndProc;
   wc.lpszClassName = "CircumrealityDisplayWindow";
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = NULL;
   wc.hCursor = NULL;
   wc.hbrBackground = NULL;
   RegisterClass (&wc);

   // get the ID
   PWSTR psz;
   CMem mem;
   psz = pszID;
   MemZero (&m_memID);
   MemCat (&m_memID, psz);

   psz = pszName;
   if (!psz)
      psz = L"";
   MemZero (&m_memName);
   MemCat (&m_memName, psz);
   if (!mem.Required ((wcslen(psz)+1)*sizeof(WCHAR))) {
      return FALSE;
   }
   WideCharToMultiByte (CP_ACP, 0, psz, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0,0);

   // default location
   RECT r;
   POINT pCenter;
   BOOL fHidden = FALSE;
   GetClientRect (m_pMain->m_hWndPrimary, &r);  // assume primary for now
   pCenter.x = (r.left + r.right)/2;
   pCenter.y = (r.top + r.bottom)/2;
   r.left = (r.left + pCenter.x) / 2;
   r.right = (r.right + pCenter.x) / 2;
   r.top = (r.top + pCenter.y) / 2;
   r.bottom = (r.bottom + pCenter.y) / 2;

   // try getting the location from the node
   BOOL fTitle = TRUE;
   DWORD dwMonitor;
   m_pMain->ChildLocGet (TW_DISPLAYWINDOW, (PWSTR)m_memID.p, &dwMonitor, &r, &fHidden, &fTitle);
   //if (pNode)
   //   m_pMain->ChildLocGet (pNode, &r, &fHidden);

#if  0 // no longer used
   // try loading the location from the user file
   CMem memFile;
   MemZero (&memFile);
   MemCat (&memFile, gpszDisplayWindowPrefix);
   MemCat (&memFile, (PWSTR)m_memName.p);
   PCMMLNode2 pUser = m_pMain->UserLoad (TRUE, (PWSTR)memFile.p);
   if (pUser) {
      // m_pMain->ChildLocGet (pUser, &r, &fHidden);
      delete pUser;
   }
#endif // 0

   // BUGFIX - temporarily turn off saving so doesnt mess up settings
   gfChildLocIgnoreSave = TRUE;

   m_pMain->ChildShowTitleIfOverlap (NULL, &r, dwMonitor, fHidden, &fTitle);
   m_hWnd = CreateWindowEx (
      (fTitle ? WS_EX_IFTITLE : WS_EX_IFNOTITLE) | WS_EX_ALWAYS,
      wc.lpszClassName, (char*)mem.p,
      WS_ALWAYS | (fTitle ? WS_IFTITLECLOSE : 0) | WS_CLIPSIBLINGS |
      (fHidden ? 0 : WS_VISIBLE),
      r.left , r.top , r.right - r.left , r.bottom - r.top ,
      dwMonitor ? pMain->m_hWndSecond : pMain->m_hWndPrimary,
      NULL, ghInstance, (PVOID) this);

   // BUGFIX - temporarily turn off saving so doesnt mess up settings
   gfChildLocIgnoreSave = FALSE;

   if (!m_hWnd)
      return FALSE;

   // BUGFIX - Because visible flag may have been messed up, re-get
   //m_pMain->ChildLocSave (m_hWnd, TW_DISPLAYWINDOW, (PWSTR)m_memID.p, NULL);

   // bring to foreground
   SetWindowPos (m_hWnd, (HWND)HWND_TOP, 0,0,0,0,
      SWP_NOMOVE | SWP_NOSIZE);
   SetActiveWindow (m_hWnd);


   // if this is the main display window then note the change
   if (!_wcsicmp((PWSTR)m_memID.p, gpszMainDisplayWindow)) {
      RECT r;
      GetClientRect (m_hWnd, &r);
      if (r.bottom - r.top)
         m_pMain->m_fMainAspect = (fp)(r.right - r.left) / (fp)(r.bottom - r.top);
      if (m_pMain->m_fMainAspect != m_pMain->m_fMainAspectSentToServer) {
         m_pMain->m_fMainAspectSentToServer = m_pMain->m_fMainAspect;
         m_pMain->InfoForServer (L"mainaspect", NULL, m_pMain->m_fMainAspect);
      }
   }

   // start the delay clear timer
   // doesn't seem to do anything DelayClearTimerStart();

   // update the contents
   return TRUE;
}

/*************************************************************************************
CDisplayWindow::Update - Updates the icon view based on the given MML node, which
is a CircumrealityDisplayWindow() node.

inputs
   PCMMLNOde            pNode - Node
                        NOTE: It's the responsibility of CDisplayWindow to delete this node
   __int64              iTime - Time stamp for when originally came in so can keep latest menu
returns
   BOOL - TRUE if success
*/
BOOL CDisplayWindow::Update (PCMMLNode2 pNode, __int64 iTime)
{
   BOOL fAutoShow = MMLValueGetInt (pNode, L"AutoShow", 0);

   // get the text for the window and set, just in case hase changed
   PWSTR psz;
   CMem mem;
   psz = MMLValueGet (pNode, L"Name");
   if (!psz)
      psz = L"";
   if (!mem.Required ((wcslen(psz)+1)*sizeof(WCHAR))) {
      delete pNode;
      return FALSE;
   }
   WideCharToMultiByte (CP_ACP, 0, psz, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0,0);
   SetWindowText (m_hWnd, (char*)mem.p);


   PCMMLNode2 pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (CircumrealityObjectDisplay()), &psz, &pSub);
   if (!pSub) {
      delete pNode;
      return FALSE;
   }

   // delete the existing one
   // BUGFIX - Don't delete
   // DeletePCVisView ();


   BOOL fRet = m_pMain->ObjectDisplay (pSub, NULL, this, FALSE, FALSE, (PWSTR)m_memID.p, FALSE, iTime);
      // defaulting fCanChatTo = FALSE
   delete pNode;

   // finally
   if (fAutoShow)
      m_pMain->ChildShowWindow (m_hWnd, TW_DISPLAYWINDOW, (PWSTR)m_memID.p, SW_SHOW);

   // start the delay timer
   // doesn't seem to do anything DelayClearTimerStart ();

   return fRet;
}



/*************************************************************************************
CDisplayWindow::WndProc - Hand the windows messages
*/
LRESULT CDisplayWindow::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      m_hWnd = hWnd;
      SetTimer (hWnd, TIMER_SCROLL360, SCROLL360TIME, NULL);
      break;

   case WM_MOUSEACTIVATE:
      {
         // change the menu to reflect this
         PCVisImage pviMain = gpMainWindow->FindMainVisImage();
         // BUGFIX - Was using m_pvi instead of pviMain, but want menu only associated with main
         if (m_pMain->m_pMenuContext != pviMain) {
            m_pMain->m_pMenuContext = pviMain;
            m_pMain->MenuReDraw ();
         }

         // see if can type here
         if (m_pvi && m_pvi->CanGetFocus()) {
            SetFocus (m_pvi->WindowForTextGet());
            return MA_ACTIVATE;
         }
         else
            m_pMain->CommandSetFocus(FALSE);
               // BUGFIX - So mousewheel always works
      }
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC = BeginPaint (hWnd, &ps);
         RECT rClient;
         GetClientRect (hWnd, &rClient);

         // find the main image and paint that
         if (m_pvi) {
            BOOL fWantedToShowDrawing = FALSE;
            BOOL fPaintedImage = FALSE;
            m_pvi->Paint (hWnd, hDC, &ps.rcPaint, TRUE,
               m_fDelayClearTimerOn || !m_fDelayClearTimerWentOff, TRUE, &fWantedToShowDrawing, &fPaintedImage);

            // if painted an image then make sure the timer is off
            if (fPaintedImage) {
               DelayClearTimerKill ();

               m_fDelayClearTimerWentOff = FALSE;  // so ok not to draw next time
            }
            else if (fWantedToShowDrawing) {
               // nothing painted, and wanted to show drawing
               if (!m_fDelayClearTimerOn && !m_fDelayClearTimerWentOff)
                  // start up timer
                  DelayClearTimerStart ();
            }
         }
         else {
         #ifdef MIFTRANSPARENTWINDOWS // dont do for transparent windows
            BOOL fTransparent = !m_pMain->ChildHasTitle (m_hWnd);
         #else
            BOOL fTransparent = FALSE;
         #endif

            // BUGFIX - only paint if no timer on
            if (!(m_fDelayClearTimerOn || !m_fDelayClearTimerWentOff))
               m_pMain->BackgroundStretch (BACKGROUND_IMAGES, fTransparent, WANTDARK_NORMAL, &rClient, hWnd,
                  GetParent(hWnd), 1, hDC, NULL);
         }

         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_MOUSEWHEEL:
      {
         if (!m_pvi || !m_pvi->IsAnyLayer360())
            return 0;

         short sAmt = (short)HIWORD(wParam);

         // have mouse wheel rotate camera
         m_pMain->m_f360Long = myfmod(m_pMain->m_f360Long + (fp)sAmt / (fp)WHEEL_DELTA * PI / 16.0, 2.0*PI);

#if 0 // BUGFIX - No longer having mouse wheel affect FOV, now rotates
         m_pMain->m_f360FOV /= pow (2, (fp)sAmt / (fp)WHEEL_DELTA / 4.0);
         m_pMain->m_f360FOV = min(m_pMain->m_f360FOV, m_pMain->m_tpFOVMinMax.v);
         m_pMain->m_f360FOV = max(m_pMain->m_f360FOV, m_pMain->m_tpFOVMinMax.h);
#endif

         m_pMain->Vis360Changed ();
      }
      return 0;

   case WM_TIMER:
      if (wParam == TIMER_DELAYCLEAR) {
         // stop this timer
         DelayClearTimerKill ();

         m_fDelayClearTimerWentOff = TRUE;

         // invalidate this window so redraws
         InvalidateRect (m_hWnd, NULL, FALSE);
      }
      else if (wParam == TIMER_SCROLL360) {
         // BUGFIX - If the tutorial mouse-move is on dont scroll
         if (m_pMain->m_fTutorialCursor) {
            m_dwTimeFirstWantToScroll = 0;
            return 0;
         }

         POINT p;
         GetCursorPos (&p);
         if (WindowFromPoint(p) != hWnd) {
            m_dwTimeFirstWantToScroll = 0;
            return 0;
         }

         ScreenToClient (hWnd, &p);

         fp fLR, fUD, fLRScroll, fUDScroll;
         BOOL fInScrollRegion;
         RoomAutoScrollAmount (&p, &fLR, &fUD, &fLRScroll, &fUDScroll, &fInScrollRegion);
         if ((!fLR && !fUD) || fInScrollRegion) {
            m_dwTimeFirstWantToScroll = 0;
            return 0; // no change
         }

         DWORD dwTime = GetTickCount();
         if (m_dwTimeFirstWantToScroll > dwTime)
            m_dwTimeFirstWantToScroll = 0;   // just in case wrap around
         if (!m_dwTimeFirstWantToScroll)
            m_dwTimeFirstWantToScroll = dwTime;
         if (m_dwTimeFirstWantToScroll + DELAYEDSCROLLTIME >= dwTime)
            return 0;   // no change

         m_pMain->m_f360Long = myfmod(m_pMain->m_f360Long + fLR, 2.0*PI);
         m_pMain->m_f360Lat = min(m_pMain->m_f360Lat + fUD, PI/2);
         m_pMain->m_f360Lat = max(m_pMain->m_f360Lat, -PI/2);

         // inform that have scrolled
         m_pMain->Vis360Changed ();
      }
      break;

   case WM_MOVING:
      if (m_pMain->TrapWM_MOVING (hWnd, lParam, FALSE))
         return TRUE;
      break;
   case WM_SIZING:
      if (m_pMain->TrapWM_MOVING (hWnd, lParam, TRUE))
         return TRUE;
      break;

   // BUGFIX - WM_MOVE and WM_SIZE end up doing the same thning
   case WM_MOVE:
   case WM_SIZE:
      {
         m_pMain->ChildLocSave (hWnd, TW_DISPLAYWINDOW, (PWSTR)m_memID.p, NULL);

         if (!m_pvi)
            break;

         // adjust the size
         RECT r;
         GetClientRect (m_hWnd, &r);
         m_pvi->RectSet (&r, uMsg == WM_MOVE);

         // might want to realc image if it's a 360 view
         m_pvi->Vis360Changed ();

         // if this is the main display window then note the change
         if (!_wcsicmp((PWSTR)m_memID.p, gpszMainDisplayWindow)) {
            if (r.bottom - r.top)
               m_pMain->m_fMainAspect = (fp)(r.right - r.left) / (fp)(r.bottom - r.top);
            if (m_pMain->m_fMainAspect != m_pMain->m_fMainAspectSentToServer) {
               m_pMain->m_fMainAspectSentToServer = m_pMain->m_fMainAspect;
               m_pMain->InfoForServer (L"mainaspect", NULL, m_pMain->m_fMainAspect);
            }
         }

      }
      break;

   case WM_DESTROY:
      KillTimer (hWnd, TIMER_SCROLL360);

      m_hWnd = NULL;
      break;

   case WM_CHAR:
      {
         // see if can type here... doesn't semto get called so no worry
         if (m_pvi && m_pvi->CanGetFocus())
            break; // return SendMessage (pvi->WindowForTextGet(), uMsg, wParam, lParam);
         else  // pass it up
            return SendMessage (GetParent(hWnd), uMsg, wParam, lParam);
      }
      break;

   case WM_MOUSEMOVE:
      {
         HCURSOR hc = NULL;

         // BUGFIX - If have verb tooltip, then can click on this too
         POINT p;
         p.x = (short)LOWORD(lParam);
         p.y = (short)HIWORD(lParam);
         ClientToScreen (m_hWnd, &p);
         if (m_pvi && !IsEqualGUID(m_pvi->m_gID,GUID_NULL) && m_pMain->VerbTooltipUpdate (m_pMain->ChildOnMonitor(hWnd), TRUE, p.x, p.y)) {
#ifdef _DEBUG
            OutputDebugStringW (L"\r\nWM_MOUSEMOVE: VerbTooltipUpdate()");
#endif
            SetCursor (m_pMain->m_hCursorHand);
            break;
         }
         else
            m_pMain->VerbTooltipUpdate ();   // to clear


         // see if over autoscroll loc
         fp fX, fY, fLRScroll, fUDScroll;
         BOOL fInScrollRegion;
         p.x = (short)LOWORD(lParam);
         p.y = (short)HIWORD(lParam);
         RoomAutoScrollAmount (&p, &fX, &fY, &fLRScroll, &fUDScroll, &fInScrollRegion);

         // just in case, if the mouse is captured, but not in the scroll region (because not 360)
         // then release
         if (!fInScrollRegion && m_fScrollCapture) {
            m_fScrollCapture = FALSE;
            ReleaseCapture();
         }

         if (fInScrollRegion) {
            SetCursor (m_fScrollCapture ? m_pMain->m_hCursor360ScrollOn : m_pMain->m_hCursor360Scroll);

            // if mouse captured the scroll
            if (m_fScrollCapture && ((fLRScroll != m_fScrollLastLR) || (fUDScroll != m_fScrollLastUD)) ) {
               fp fScrollSpeed = PI * 4.0; // min(m_pMain->m_f360FOV * 2.0, PI * 2.0);
               m_pMain->m_f360Long += (fLRScroll - m_fScrollLastLR) * fScrollSpeed; // 4.0 * PI;
               m_pMain->m_f360Long = myfmod(m_pMain->m_f360Long, 2.0*PI);
                     // BUGFIX - Scrolling is very fast
               m_pMain->m_f360Lat -= (fUDScroll - m_fScrollLastUD) * PI;
               m_pMain->m_f360Lat = min(m_pMain->m_f360Lat, PI/2);
               m_pMain->m_f360Lat = max(m_pMain->m_f360Lat, -PI/2);

               // inform that have scrolled
               m_pMain->Vis360Changed ();

               m_fScrollLastLR = fLRScroll;
               m_fScrollLastUD = fUDScroll;
            }
            break;
         }
         else if (fX || fY) {
            // handle cursor
            if (fabs(fX) >= fabs(fY))
               hc = (fX >= 0) ? m_pMain->m_hCursorRotRight : m_pMain->m_hCursorRotLeft;
            else
               hc = (fY >= 0) ? m_pMain->m_hCursorRotUp : m_pMain->m_hCursorRotDown;
            SetCursor (hc);
            break;
         }


         PCCircumrealityHotSpot ph = m_pvi ? m_pvi->HotSpotMouseIn (p.x, p.y) : NULL;
         if (ph && !m_pMain->m_fMessageDiabled && !m_pMain->m_fMenuExclusive) {
            switch (ph->m_dwCursor) {
            case 0: // arrow
            default:
               hc = LoadCursor(NULL, IDC_ARROW);
               break;
            case 1:  // eye
               hc = m_pMain->m_hCursorEye;   // BUGBUG - eventually better eye cursor
               break;
            case 2:  // hand
               hc = m_pMain->m_hCursorHand;
               break;
            case 3:  // mouth
               hc = m_pMain->m_hCursorMouth;   // BUGBUG - eventually better mouth cursor
               break;
            case 4:  // walk
               hc = m_pMain->m_hCursorWalk;
               break;
            case 5:  // key
               hc = m_pMain->m_hCursorKey;
               break;
            case 6:  // door
               hc = m_pMain->m_hCursorDoor;
               break;
            case 7:  // walkdont
               hc = m_pMain->m_hCursorWalkDont;
               break;
            case 8:  // rot left
               hc = m_pMain->m_hCursorRotLeft;
               break;
            case 9:  // rot right
               hc = m_pMain->m_hCursorRotRight;
               break;

            case 10:  // menu
               hc = m_pMain->m_hCursorMenu;
               break;
            }
            SetCursor (hc);
         }
         else if (m_pvi && m_pvi->IsAnyLayer360()) {
#ifdef FEATURE_SCROLLANYWHERE
            SetCursor (m_fScrollCapture ? m_pMain->m_hCursor360ScrollOn : m_pMain->m_hCursor360Scroll);
#else
            SetCursor (m_pMain->m_hCursorNoMenu);
#endif // FEATURE_SCROLLANYWHERE
         }
         else
            SetCursor (m_pMain->m_hCursorNo);

         m_pMain->HotSpotTooltipUpdate (m_pvi, m_pMain->ChildOnMonitor(m_hWnd), TRUE, p.x, p.y);
      }
      break;

   case WM_LBUTTONUP:
      {
         // clicked, so capture
         if (m_fScrollCapture) {
            ReleaseCapture ();
            m_fScrollCapture = FALSE;
            BeepWindowBeep (ESCBEEP_SCROLLDRAGSTART);

            // set the cursor
            SendMessage (hWnd, WM_MOUSEMOVE, wParam, lParam);
         }

      }
      break;

   case WM_RBUTTONDOWN:
      {
         // set this as the foreground window
         SetWindowPos (hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

         // pull up context menu
         if (!m_pMain->m_fMessageDiabled && m_pvi && m_pvi->IsAnyLayer360()) {
            BeepWindowBeep (ESCBEEP_MENUOPEN);

            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENU360CONTEXT));
            HMENU hSub = GetSubMenu(hMenu,0);

            if (m_pMain->m_f360FOV <= m_pMain->m_tpFOVMinMax.h)
               EnableMenuItem (hSub, ID_CONTEXT360_ZOOMIN, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);
            if (m_pMain->m_f360FOV >= m_pMain->m_tpFOVMinMax.v)
               EnableMenuItem (hSub, ID_CONTEXT360_ZOOMOUT, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            switch (iRet) {
            case ID_CONTEXT360_ZOOMIN:
            case ID_CONTEXT360_ZOOMOUT:
               {
                  m_pMain->m_f360FOV /= pow (2, ((iRet == ID_CONTEXT360_ZOOMIN) ? 1 : -1) / 2.0);
                  m_pMain->m_f360FOV = min(m_pMain->m_f360FOV, m_pMain->m_tpFOVMinMax.v);
                  m_pMain->m_f360FOV = max(m_pMain->m_f360FOV, m_pMain->m_tpFOVMinMax.h);

                  m_pMain->Vis360Changed ();
                  return 0;
               }
               break;
            } // switch

            return 0;
         }
         else
            BeepWindowBeep (ESCBEEP_DONTCLICK);
      }
      break;

   case WM_LBUTTONDOWN:
      {
         // set this as the foreground window
         SetWindowPos (hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

         // see if in <click>
         POINT p;
         p.x = (short)LOWORD(lParam);
         p.y = (short)HIWORD(lParam);
         ClientToScreen (m_hWnd, &p);
         if (m_pvi && !IsEqualGUID(m_pvi->m_gID,GUID_NULL) && m_pMain->VerbTooltipUpdate (m_pMain->ChildOnMonitor(hWnd), TRUE, p.x, p.y)) {
            m_pMain->VerbClickOnObject (&m_pvi->m_gID);
            return 0;
         }


         // see if over autoscroll loc
         fp fX, fY, fLRScroll, fUDScroll;
         BOOL fInScrollRegion;
         p.x = (short)LOWORD(lParam);
         p.y = (short)HIWORD(lParam);
         RoomAutoScrollAmount (&p, &fX, &fY, &fLRScroll, &fUDScroll, &fInScrollRegion);

         if (fInScrollRegion) {
            BeepWindowBeep (ESCBEEP_SCROLLDRAGSTART);

            if (!m_fScrollCapture) {
               m_fScrollLastLR = fLRScroll;
               m_fScrollLastUD = fUDScroll;
               SetCapture (m_hWnd);
               m_fScrollCapture = TRUE;
               SetCursor (m_pMain->m_hCursor360ScrollOn);
            }
            return 0;
         }

         PCCircumrealityHotSpot ph = m_pvi ? m_pvi->HotSpotMouseIn ((short)LOWORD(lParam), (short)HIWORD(lParam)) : NULL;
         if (ph && !m_pMain->m_fMessageDiabled && !m_pMain->m_fMenuExclusive) {
            PWSTR psz = ph->m_ps->Get();

            if (ph->m_dwCursor == 10) {
               psz = m_pMain->ContextMenuDisplay (hWnd, NULL, &ph->m_lMenuShow, &ph->m_lMenuExtraText, &ph->m_lMenuDo);
               if (!psz)
                  psz = L"";
            }

            // get the main window
            m_pMain->SendTextCommand (ph->m_lid, psz, NULL, m_pvi ? &m_pvi->m_gID : NULL, NULL, TRUE, TRUE, TRUE);
            if (!(psz && (psz[0] == L'@')))  // so can quickly click after showing map
               m_pMain->HotSpotDisable();

            // play a click
            BeepWindowBeep (ESCBEEP_LINKCLICK);
            return 0;
         }
         else if (!m_pMain->m_fMessageDiabled && m_pvi && m_pvi->IsAnyLayer360()) {
#ifdef FEATURE_SCROLLANYWHERE
            // scroll anywhere in 360 image
            BeepWindowBeep (ESCBEEP_SCROLLDRAGSTART);

            if (!m_fScrollCapture) {
               m_fScrollLastLR = fLRScroll;
               m_fScrollLastUD = fUDScroll;
               SetCapture (m_hWnd);
               m_fScrollCapture = TRUE;
               SetCursor (m_pMain->m_hCursor360ScrollOn);
            }
#else
            BeepWindowBeep (ESCBEEP_MENUOPEN);

            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENU360CONTEXT));
            HMENU hSub = GetSubMenu(hMenu,0);

            if (m_pMain->m_f360FOV <= m_pMain->m_tpFOVMinMax.h)
               EnableMenuItem (hSub, ID_CONTEXT360_ZOOMIN, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);
            if (m_pMain->m_f360FOV >= m_pMain->m_tpFOVMinMax.v)
               EnableMenuItem (hSub, ID_CONTEXT360_ZOOMOUT, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            switch (iRet) {
            case ID_CONTEXT360_ZOOMIN:
            case ID_CONTEXT360_ZOOMOUT:
               {
                  m_pMain->m_f360FOV /= pow (2, ((iRet == ID_CONTEXT360_ZOOMIN) ? 1 : -1) / 2.0);
                  m_pMain->m_f360FOV = min(m_pMain->m_f360FOV, m_pMain->m_tpFOVMinMax.v);
                  m_pMain->m_f360FOV = max(m_pMain->m_f360FOV, m_pMain->m_tpFOVMinMax.h);

                  m_pMain->Vis360Changed ();
                  return 0;
               }
               break;
            } // switch
#endif // FEATURE_SCROLLANYWHERE

            return 0;
         }
         else
            BeepWindowBeep (ESCBEEP_DONTCLICK);
      }
      break;

   case WM_CLOSE:
      // just hide this
      m_pMain->ChildShowWindow (m_hWnd, TW_DISPLAYWINDOW, (PWSTR)this->m_memID.p, SW_HIDE);

      // show bottom pane so player knows is closed
      if (m_pMain->m_pSlideBottom)  // BUGFIX - In case doesn't exist
         m_pMain->m_pSlideBottom->SlideDownTimed (1000);

      // BUGFIX - set the focus back to the command window. This also
      // ensures that the mouse wheel always works
      m_pMain->CommandSetFocus(FALSE);
      return 0;


   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CDisplayWindow::RoomAutoScrollAmount - This accepts the cursor (in m_hWnd coords)
and figures out if it's over any of the auto-scroll areas. If it is it fills in
*pfLR and *prUD with the amount of scrolling (in radians) that should be performed.
If it's not over then it fills them in with 0.

inputs
   POINT          *pRoom - Cursor location in room coords
   fp             *pfLR - LR scroll delta
   fp             *pfUD - UD scroll delta
   fp             *pfLRScroll - Location (from 0..1) along with width of the window's width.
   fp             *pfUDScroll - Location (from 0..1) along the window's height
   BOOL           *pfInScrollRegion - Set to TRUE if the cursor is in the location at the top or
                  bottom of the screen where scrolling occurs.
returns
   none
*/
void CDisplayWindow::RoomAutoScrollAmount (POINT *pRoom, fp *pfLR, fp *pfUD, fp *pfLRScroll, fp *pfUDScroll, BOOL *pfInScrollRegion)
{
   *pfLR = *pfUD = *pfLRScroll = *pfUDScroll = 0;
   *pfInScrollRegion = FALSE;

   // only care if 360 view
   if (!m_pvi || !m_pvi->IsAnyLayer360())
      return;

   RECT r;
   GetClientRect (m_hWnd, &r);
   int iWidth = (r.right - r.left)/8;
   int iHeight = (r.bottom - r.top)/8;

   // see if in the scroll region
   *pfInScrollRegion = m_fScrollCapture; // if captured then always in
   if (PtInRect (&r, *pRoom) && ((pRoom->y < r.top + m_iScrollSize) || (pRoom->y > r.bottom - m_iScrollSize) ) )
      *pfInScrollRegion = TRUE;
   *pfLRScroll = (fp)(pRoom->x - r.left) / (fp)max(r.right - r.left, 1);
   *pfUDScroll = (fp)(pRoom->y - r.top) / (fp)max(r.bottom - r.top, 1);

   fp fRoundAmount = 15.0;
   if (giCPUSpeed >= 3)
      fRoundAmount *= 4.0;
   else if (giCPUSpeed >= 0)
      fRoundAmount *= 2.0;
   *pfUDScroll = floor(*pfUDScroll * fRoundAmount + 0.5) / fRoundAmount; // round so don't scroll up/down too ofen

   fRoundAmount *= 4.0; // more detail E/W
   *pfLRScroll = floor(*pfLRScroll * fRoundAmount + 0.5) / fRoundAmount; // round so don't scroll up/down too ofen

#ifdef FEATURE_SCROLLANYWHERE
   return;  // since won't do the rotation bits
#else

   // remove the top/bottom scroll
   r.top += m_iScrollSize;
   r.bottom -= m_iScrollSize;

   if (!PtInRect (&r, *pRoom))
      return;   // not in area

   // rotation speed converted to per-tick
   fp fSpeed = m_pMain->m_f360FOV / (1000.0 / SCROLL360TIME) * m_pMain->m_f360RotSpeed;

   // scroll left or right
   BOOL fChanged = FALSE;
   if (pRoom->x >= r.right - iWidth)
      *pfLR = (fp)(pRoom->x - (r.right - iWidth)) / (fp)iWidth * fSpeed;
   else if (pRoom->x <= r.left + iWidth)
      *pfLR = -(fp)((r.left + iWidth) - pRoom->x) / (fp)iWidth * fSpeed;

   // scroll up or down
   if (pRoom->y >= r.bottom - iHeight) {
      *pfUD = -(fp)(pRoom->y - (r.bottom - iHeight)) / (fp)iHeight * fSpeed;

      // limit to how far can scroll
      if (m_pMain->m_f360Lat <= -PI/2)
         *pfUD = 0;
   }
   else if (pRoom->y <= r.top + iHeight) {
      *pfUD = (fp)((r.top + iHeight) - pRoom->y) / (fp)iHeight * fSpeed;

      // limit to how far can scroll
      if (m_pMain->m_f360Lat >= PI/2)
         *pfUD = 0;
   }
#endif
}

