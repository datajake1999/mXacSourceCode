/***************************************************************************
CGroundView - C++ object for modifying the ground data

begun 20/2/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <zmouse.h>
#include <multimon.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


/*****************************************************************************
typedefs */

#define  TBORDER           16       // extra border around thumbnail
#define  THUMBSHRINK       (TEXTURETHUMBNAIL/2)    // size to shrink thumbnail to

#define  TIMER_AIRBRUSH    121      // timer ID to cause frequent airbrush
#define  AIRBRUSHPERSEC    10       // number of times to call airbrush per second

#define  WAITTOTIP         90       // timer ID for waiting to tip

#define  SCROLLSIZE     (16)
#define  SMALLSCROLLSIZE (SCROLLSIZE / 2)
#define  VARBUTTONSIZE  (m_fSmallWindow ? M3DSMALLBUTTONSIZE : M3DBUTTONSIZE)
#define  VARSCROLLSIZE (m_fSmallWindow ? SMALLSCROLLSIZE : SCROLLSIZE)
#define  VARSCREENSIZE (VARBUTTONSIZE * 6 / 4)  // BUGFIX - Was 7/4


#define  IDC_CLOSE         1002
#define  IDC_MINIMIZE      1003
#define  IDC_NEXTMONITOR   1004
#define  IDC_SAVE          1005
#define  IDC_GROUNDWATER   1006
#define  IDC_HELPBUTTON    1010
#define  IDC_SMALLWINDOW   1012
#define  IDC_GROUNDSET     1015

#define  IDC_UNDOBUTTON    2004
#define  IDC_REDOBUTTON    2005

#define  IDC_VIEWTOPO      3001
#define  IDC_VIEWTEXT      3002
#define  IDC_VIEWFOREST    3003
#define  IDC_ZOOMIN        3006
#define  IDC_ZOOMOUT       3008

#define  IDC_GROUNDGENMTN  4000
#define  IDC_GROUNDGENVOLCANO 4001
#define  IDC_GROUNDGENHILL 4002
#define  IDC_GROUNDGENHILLROLL 4003
#define  IDC_GROUNDGENCRATER 4004
#define  IDC_GROUNDGENPLAT 4010
#define  IDC_GROUNDGENRAVINE 4011
#define  IDC_GROUNDGENRAISE 4020
#define  IDC_GROUNDGENLOWER 4021
#define  IDC_GROUNDGENFLAT 4022
#define  IDC_GROUNDAPPLYWIND 4030
#define  IDC_GROUNDAPPLYRAIN 4031
#define  IDC_GROUNDPAINT   4040

#define  IDC_BRUSHSHAPE    5000
#define  IDC_BRUSHEFFECT   5001
#define  IDC_BRUSHSTRENGTH 5002

#define  IDC_BRUSH4        6000
#define  IDC_BRUSH8        6001
#define  IDC_BRUSH16       6002
#define  IDC_BRUSH32       6003
#define  IDC_BRUSH64       6004

#define  IDC_GRIDTOPO      9001
#define  IDC_TRACING       9003
#define  IDC_GROUNDSUN     9004


#define  IDC_TEXTLIST      8000
#define  IDC_FORESTLIST    8001
#define HVDELTA      .01


/*****************************************************************************
globals */
static CListFixed    gListPCGroundView;       // list of house views
static BOOL          gfListPCGroundViewInit = FALSE;   // set to true if has been initialized

static fp gafGridMetric[] = {
   // minor, major
   100000.0, 1000000.0,
   10000.0, 100000.0,
   1000.0, 10000.0,
   100.0, 1000.0,
   10.0, 100.0,
   1.0, 10.0,
   0.1, 1,
   .01, .1
};

static fp gafGridEnglish[] = {
   // minor, major
   100.0 * FEETPERMILE * METERSPERFOOT, 1000.0 * FEETPERMILE * METERSPERFOOT,
   10.0 * FEETPERMILE * METERSPERFOOT, 100.0 * FEETPERMILE * METERSPERFOOT,
   1.0 * FEETPERMILE * METERSPERFOOT, 10.0 * FEETPERMILE * METERSPERFOOT,
   1000.0 * METERSPERFOOT, 5000.0 * METERSPERFOOT,
   100.0 * METERSPERFOOT, 1000.0 * METERSPERFOOT,
   10.0 * METERSPERFOOT, 100.0 * METERSPERFOOT,
   1.0 * METERSPERFOOT, 10.0 * METERSPERFOOT,
   1.0 / INCHESPERMETER, 12.0 / INCHESPERMETER
};



BOOL GroundWaterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL GroundPaintPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);

/****************************************************************************
GroundViewDestroy - If a ground view exists for a world, this closes it
UNLESS it's dirty, in which case it fails.

inputs
   PCWorldSocket pWorld - World
   GUID        *pgObject - Object viewed
returns
   BOOL - If the view doesn't exist then TRUE all the time. If it does
   exist AND it's not dirty, then it's closed and returns TRUE. Otherwise FALSE.
*/
BOOL GroundViewDestroy (PCWorldSocket pWorld, GUID *pgObject)
{
   DWORD i;
   PCGroundView *ppg = (PCGroundView*) gListPCGroundView.Get(0);
   for (i = 0; i < gListPCGroundView.Num(); i++) {
      if ((ppg[i]->m_pWorld == pWorld) && IsEqualGUID(ppg[i]->m_gObject, *pgObject)) {
         if (ppg[i]->m_fDirty)
            return FALSE;  // cant destroy because changed

         // else, deltete. The destructor will remove it from the list
         delete ppg[i];
         return TRUE;
      }
   }

   // doesn't exist
   return TRUE;
}

/****************************************************************************
GroundViewNew - Given a world and a ground object (idenitified by its guid) this
will see if a view exists already. If one does then that is shown. If not,
a new one is created

returns
   BOOL - TRUE if already exists or created, FALSE if not
*/
BOOL GroundViewNew (PCWorldSocket pWorld, GUID *pgObject)
{
   DWORD i;
   PCGroundView *ppg = (PCGroundView*) gListPCGroundView.Get(0);
   for (i = 0; i < gListPCGroundView.Num(); i++) {
      if ((ppg[i]->m_pWorld == pWorld) && IsEqualGUID(ppg[i]->m_gObject, *pgObject)) {
         ShowWindow (ppg[i]->m_hWnd, SW_SHOWNORMAL);
         SetForegroundWindow (ppg[i]->m_hWnd);
         return TRUE;
      }
   }

   // else if get here then create
   PCGroundView pgv;
   pgv = new CGroundView (pWorld, pgObject);
   if (!pgv)
      return FALSE;
   if (!pgv->Init ()) {
      delete pgv;
      return FALSE;
   }

   return TRUE;
}

/****************************************************************************
GroundViewShowHide - Shows or hides the ground view for the specific
object. Used to make sure they're not viislble
while the user is animating.

inputs
   PCWorldSocket pWorld - World. If NULL then any world is acceptable
   GUID        *pgObject - Object viewed. If NULL then any object acceptable
   BOOL        fShow - TRUE to show
*/
BOOL GroundViewShowHide (PCWorldSocket pWorld, GUID *pgObject, BOOL fShow)
{
   DWORD i;
   BOOL fFound = FALSE;

   // BUGFIX - Added ability to show/hide specific ground objects
   PCGroundView *ppg = (PCGroundView*) gListPCGroundView.Get(0);
   for (i = 0; i < gListPCGroundView.Num(); i++) {
      if (pWorld && (ppg[i]->m_pWorld != pWorld))
         continue;
      if (pgObject && (!IsEqualGUID (ppg[i]->m_gObject, *pgObject)))
         continue;
      ShowWindow (ppg[i]->m_hWnd, fShow ? SW_SHOW : SW_HIDE);
      fFound = TRUE;
   }
   return fFound;
}

/****************************************************************************
GroundViewModifying - Returns TRUE if a ground view editor exists for any world.

inputs
   PCWorldSocket     pWorld - World to look for. If NULL then returns true if
            one exists for any world
returns
   BOOL - TRUE if exists
*/
BOOL GroundViewModifying (PCWorldSocket pWorld)
{
   DWORD i;
   PCGroundView *ppg = (PCGroundView*) gListPCGroundView.Get(0);
   if (!pWorld)
      return (gListPCGroundView.Num() ? TRUE : FALSE);
   for (i = 0; i < gListPCGroundView.Num(); i++) {
      if (ppg[i]->m_pWorld == pWorld)
         return TRUE;
   }

   return FALSE;
}


/****************************************************************************
CGroundViewWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CGroundView, and calls that.
*/
LRESULT CALLBACK CGroundViewWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCGroundView p = (PCGroundView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCGroundView p = (PCGroundView) (size_t) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCGroundView) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/****************************************************************************
CGroundViewMapWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CGroundView, and calls that.
*/
LRESULT CALLBACK CGroundViewMapWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCGroundView p = (PCGroundView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCGroundView p = (PCGroundView) (size_t) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLong (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCGroundView) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->MapWndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/****************************************************************************
CGroundViewKeyWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CGroundView, and calls that.
*/
LRESULT CALLBACK CGroundViewKeyWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCGroundView p = (PCGroundView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCGroundView p = (PCGroundView) (size_t) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCGroundView) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->KeyWndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/****************************************************************************
Constructor and destructor

inputs
   PCWorldSocket  pWorld - World that the object is in
   GUID           *pgObject - Object
*/
CGroundView::CGroundView (PCWorldSocket pWorld, GUID *pgObject)
{
   m_pWorld = pWorld;
   m_gObject = *pgObject;
   memset (&m_gState, 0, sizeof(m_gState));

   m_hWndTextList = m_hWndElevKey = m_hWndForestList = NULL;

   // very dark
   m_cBackDark = RGB(0x40, 0x20, 0x20);
   m_cBackMed = RGB(0x80,0x40,0x40);
   m_cBackLight = RGB(0xc0,0xa0,0xa0);
   m_cBackOutline = RGB(0x80, 0x80, 0x80);

   // color scheme
   m_acColorScheme[0] = RGB(0x00, 0x40, 0x00);
   m_acColorScheme[1] = RGB(0x00, 0xc0, 0x00);
   m_acColorScheme[2] = RGB(0x40, 0xff, 0x40);
   m_acColorScheme[3] = RGB(0xe0, 0xff, 0x40);
   m_acColorScheme[4] = RGB(0xff,0xff,0xff);

   m_acColorSchemeWater[4] = RGB(0xc0, 0xc0, 0xff);
   m_acColorSchemeWater[3] = RGB(0x90, 0x90, 0xc0);
   m_acColorSchemeWater[2] = RGB(0x50, 0x50, 0x80);
   m_acColorSchemeWater[1] = RGB(0x20, 0x20, 0x60);
   m_acColorSchemeWater[0] = RGB(0x0, 0x0, 0x40);

   m_fPaintOn = FALSE;
   m_tpPaintElev.h = .5;
   m_tpPaintElev.v = .5;
   m_tpPaintSlope.h = 0;
   m_tpPaintSlope.v = .5;
   m_fPaintAmount = .5;

   m_dwView = -1;
   m_dwCurText = m_dwCurForest = 0;
   m_dwBrushAir = 2; // default to airbrush
   m_dwBrushPoint = 2;  // start out as slightly pointy
   m_dwBrushStrength = 3;
   m_dwBrushEffect = 0;
   m_fAirbrushTimer = FALSE;
   m_dwGroundGenSeed = GetTickCount();

   m_hWnd = NULL;
   m_pbarGeneral = m_pbarMisc = m_pbarView = m_pbarObject = NULL;
   m_fDirty = FALSE;
   m_fUndoDirty = FALSE;
   m_lGVSTATEUndo.Init (sizeof(GVSTATE));
   m_lGVSTATERedo.Init (sizeof(GVSTATE));
   m_fGridUDMinor = m_fGridUDMajor = 1;
   m_pLight.p[0] = -1;
   m_pLight.p[1] = 1;
   m_pLight.p[2] = .5;
   m_pLight.Normalize();

   m_fSmallWindow = FALSE;
   m_tpViewUL.h = m_tpViewUL.v = 0;
   m_fViewScale= 1;
   m_lTraceInfo.Init (sizeof(TRACEINFO));
   
   memset (m_adwPointerMode, 0 ,sizeof(m_adwPointerMode));
   m_dwPointerModeLast = 0;
   m_dwButtonDown = 0;
   m_pntButtonDown.x = m_pntButtonDown.y = 0;
   m_fCaptured = FALSE;
   m_pUndo = m_pRedo = NULL;
   m_fWorkingWithUndo = FALSE;

   LOGFONT lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -12;   // 10 pixels high MulDiv(iPointSize, EscGetDeviceCaps(hDC, LOGPIXELSY), 72); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   lf.lfWeight = FW_BOLD;
   m_hFont = CreateFontIndirect (&lf);



   // Add this to the list of views around
   if (!gfListPCGroundViewInit) { 
      gListPCGroundView.Init (sizeof(PCGroundView));
      if (!gListVIEWOBJ.Num())   // BUGFIX - so dont reinit if already initialed
         gListVIEWOBJ.Init (sizeof(VIEWOBJ));
      gfListPCGroundViewInit = TRUE;
   }
   PCGroundView ph = this;
   gListPCGroundView.Add (&ph);

}


CGroundView::~CGroundView (void)
{
   // delete the bars
   if (m_pbarGeneral)
      delete m_pbarGeneral;
   if (m_pbarMisc)
      delete m_pbarMisc;
   if (m_pbarView)
      delete m_pbarView;
   if (m_pbarObject)
      delete m_pbarObject;

   if (m_fAirbrushTimer)
      KillTimer (m_hWnd, TIMER_AIRBRUSH);

   if (m_hWnd)
      DestroyWindow (m_hWnd);

   if (m_hFont)
      DeleteObject (m_hFont);

   // remove self from list of house views
   DWORD i;
   PCGroundView ph;
   for (i = 0; i < gListPCGroundView.Num(); i++) {
      ph = *((PCGroundView*)gListPCGroundView.Get(i));
      if (ph == this) {
         gListPCGroundView.Remove(i);
         break;
      }
   }

   StateFree (&m_gState);

   // free up undo and redo
   PGVSTATE ps;
   ps = (PGVSTATE) m_lGVSTATEUndo.Get(0);
   for (i = 0; i < m_lGVSTATEUndo.Num(); i++, ps++)
      StateFree (ps);
   ps = (PGVSTATE) m_lGVSTATERedo.Get(0);
   for (i = 0; i < m_lGVSTATERedo.Num(); i++, ps++)
      StateFree (ps);
}


/***********************************************************************************
CGroundView::ButtonExists - Looks through all the buttons and sees if one with the given
ID exists. If so it returns TRUE, else FALSE

inputs
   DWORD       dwID - button ID
returns
   BOOL - TRUE if exists
*/
BOOL CGroundView::ButtonExists (DWORD dwID)
{
   if (m_pbarGeneral->ButtonExists(dwID))
      return TRUE;
   if (m_pbarMisc->ButtonExists(dwID))
      return TRUE;
   if (m_pbarObject->ButtonExists(dwID))
      return TRUE;
   if (m_pbarView->ButtonExists(dwID))
      return TRUE;

   return FALSE;
}


/***********************************************************************************
CGroundView::SetPointerMode - Changes the pointer mode to the new mode.

inputs
   DWORD    dwMode - new mode
   DWORD    dwButton - which button it's approaite for, 0 for left, 1 for middle, 2 for right
   BOOL     fInvalidate - If TRUE, invalidate the surrounding areaas
*/
void CGroundView::SetPointerMode (DWORD dwMode, DWORD dwButton, BOOL fInvalidate)
{
   if (dwButton >= 3)
      return;  // out of bounds

   // cache undo
   if (fInvalidate)
      UndoCache ();

   // keep track of last pointer mode
   if ((dwButton == 0) && (dwMode != m_adwPointerMode[dwButton]))
      m_dwPointerModeLast = m_adwPointerMode[dwButton];

   m_adwPointerMode[dwButton] = dwMode;

   // look through all the button bars and turn off all the red arrows
   // except the one that's being used for the mode
   DWORD dwFlag;
   if (dwButton == 1)
      dwFlag = IBFLAG_MBUTTON;
   else if (dwButton == 2)
      dwFlag = IBFLAG_RBUTTON;
   else
      dwFlag = IBFLAG_REDARROW;
   m_pbarGeneral->FlagsSet (m_adwPointerMode[dwButton], dwFlag);
   m_pbarMisc->FlagsSet (m_adwPointerMode[dwButton], dwFlag);
   m_pbarObject->FlagsSet (m_adwPointerMode[dwButton], dwFlag);
   m_pbarView->FlagsSet (m_adwPointerMode[dwButton], dwFlag);

   if (fInvalidate) {
      // BUGFIX - Invalidate the area around the tabs
      RECT r, rt;
      GetClientRect (m_hWnd, &r);
      // top
      rt = r;
      rt.left += VARBUTTONSIZE;
      rt.right -= VARBUTTONSIZE;
      rt.bottom = rt.top + VARSCREENSIZE;
      rt.top += VARBUTTONSIZE;
      InvalidateRect (m_hWnd, &rt, FALSE);

      // right
      rt = r;
      rt.left = rt.right - VARSCREENSIZE;
      rt.right -= VARBUTTONSIZE;
      rt.top += VARBUTTONSIZE;
      rt.bottom -= VARBUTTONSIZE;
      InvalidateRect (m_hWnd, &rt, FALSE);

      // bottom
      rt = r;
      rt.left += VARBUTTONSIZE;
      rt.right -= VARBUTTONSIZE;
      rt.top += rt.bottom - VARSCREENSIZE;
      rt.bottom -= VARBUTTONSIZE;
      InvalidateRect (m_hWnd, &rt, FALSE);

      // left
      rt = r;
      rt.right = rt.left + VARSCREENSIZE;
      rt.left += VARBUTTONSIZE;
      rt.top += VARBUTTONSIZE;
      rt.bottom -= VARBUTTONSIZE;
      InvalidateRect (m_hWnd, &rt, FALSE);

      POINT px;
      GetCursorPos (&px);
      ScreenToClient (m_hWnd, &px);
      SetProperCursor (px.x, px.y);
   }
}

/**********************************************************************************
Init - Initalize the house view object to a new one. This ends up creating a house
   view Windows.

inputs
   DWORD    dwMonitor - Monitor number to 0, from 0+. If -1 then use a monitor not
               already in use, or the one least used.
returns
   BOOL - TRUE if it succeded in creating a new view.
*/
BOOL CGroundView::Init (DWORD dwMonitor)
{
   if (m_hWnd)
      return FALSE;

   // try to get the state information
   if (!FindViewForWorld (m_pWorld))
      return FALSE;  // world no longer exists
   PCObjectSocket pos;
   pos = m_pWorld->ObjectGet(m_pWorld->ObjectFind(&m_gObject));
   if (!pos)
      return FALSE;  // cant find object
   OSMGROUNDINFOGET gi;
   memset (&gi, 0, sizeof(gi));
   if (!pos->Message (OSM_GROUNDINFOGET, &gi))
      return FALSE;  // not ground

   // load in state info
   GVSTATE s;
   memset (&s, 0, sizeof(s));
   CListFixed lTextSurf, lTextColor, lPCForest;
   s.dwHeight = gi.dwHeight;
   s.dwWidth = gi.dwWidth;
   s.fScale = gi.fScale;
   s.pawElev = gi.pawElev;
   s.tpElev = gi.tpElev;
   s.fWater = gi.fWater;
   s.fWaterElevation = gi.fWaterElevation;
   s.fWaterSize = gi.fWaterSize;
   memcpy (s.apWaterSurf, gi.apWaterSurf, sizeof(gi.apWaterSurf));
   memcpy (s.afWaterElev, gi.afWaterElev, sizeof(gi.afWaterElev));
   lTextSurf.Init (sizeof(PCObjectSurface), gi.paTextSurf, gi.dwNumText);
   lTextColor.Init (sizeof(COLORREF), gi.paTextColor, gi.dwNumText);
   lPCForest.Init (sizeof(PCForest), gi.paForest, gi.dwNumForest);
   s.pabTextSet = gi.pabTextSet;
   s.plTextColor = &lTextColor;
   s.plTextSurf = &lTextSurf;
   s.pabForestSet = gi.pabForestSet;
   s.plPCForest = &lPCForest;

   StateFree (&m_gState);
   StateClone (&m_gState, &s);

   // come up with some good default units
   fp *pafUnits;
   fp fScale;
   DWORD dwNumUnits;
   if (MeasureDefaultUnits() & MUNIT_ENGLISH) {
      pafUnits = gafGridEnglish;
      dwNumUnits = sizeof(gafGridEnglish)/sizeof(fp)/2;
   }
   else {
      pafUnits = gafGridMetric;
      dwNumUnits = sizeof(gafGridMetric)/sizeof(fp)/2;
   }
   DWORD i;
   fScale = fabs(m_gState.tpElev.v - m_gState.tpElev.h) / 10;
   for (i = 0; i < dwNumUnits; i++) {
      if (fScale >= pafUnits[i*2])  // BUGFIX - Was i*2+1
         break;
   }
   i = min(i, dwNumUnits-1);
   m_fGridUDMinor = pafUnits[i*2+0];
   m_fGridUDMajor = pafUnits[i*2+1];


   // come up with good scale
   m_fViewScale = 600.0 / (fp)m_gState.dwWidth;

   // register the window proc if it isn't alreay registered
   static BOOL fIsRegistered = FALSE;
   if (!fIsRegistered) {
      WNDCLASS wc;

      // maint window
      memset (&wc, 0, sizeof(wc));
      wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_GROUNDICON));
      wc.lpfnWndProc = CGroundViewWndProc;
      wc.style = CS_HREDRAW | CS_VREDRAW;
      wc.hInstance = ghInstance;
      wc.hbrBackground = NULL; //(HBRUSH)(COLOR_BTNFACE+1);
      wc.lpszClassName = "CGroundView";
      wc.hCursor = LoadCursor (NULL, IDC_NO);
      RegisterClass (&wc);

      // map
      wc.hCursor = NULL;
      wc.lpfnWndProc = CGroundViewMapWndProc;
      wc.lpszClassName = "CGroundViewMap";
      RegisterClass (&wc);

      // key
      wc.lpfnWndProc = CGroundViewKeyWndProc;
      wc.lpszClassName = "CGroundViewKey";
      RegisterClass (&wc);

      fIsRegistered = TRUE;
   }

   // enumerate the monitors
   DWORD dwLeast;
   dwLeast = FillXMONITORINFO ();
   PCListFixed pListXMONITORINFO;
   pListXMONITORINFO = ReturnXMONITORINFO();

   // If monitor = -1 then find one thats ok
   if (dwMonitor >= pListXMONITORINFO->Num())
      dwMonitor = dwLeast;

   // Look at the monitor to determine the location to create
   PXMONITORINFO pmi;
   pmi = (PXMONITORINFO) pListXMONITORINFO->Get(dwMonitor);
   if (!pmi)
      return FALSE;

   // set window text to whatever editing
   PWSTR psz;
   char szTemp[256];
   psz = pos->StringGet (OSSTRING_NAME);
   if (!psz)
      psz = L"Unknown";
   WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp)-32, 0, 0);
   strcat (szTemp, " - Ground editor");
   SetWindowText (m_hWnd, szTemp);

   // Create the window
   m_hWnd = CreateWindowEx (WS_EX_APPWINDOW, "CGroundView", szTemp,
      WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
      pmi->rWork.left, pmi->rWork.top, pmi->rWork.right - pmi->rWork.left, pmi->rWork.bottom - pmi->rWork.top,
      NULL, NULL, ghInstance, (LPVOID) this);
   if (!m_hWnd)
      return FALSE;

   UpdateAllButtons();

   NewView (0);   // start at topo view

   return TRUE;
}

/***********************************************************************************
CGroundView::UpdateAllButtons - Called when the user has progressed so far in the
tutorial, this first erases all the buttons there, and then updates them
to the appropriate display level.

inputs
*/
void CGroundView::UpdateAllButtons (void)
{
   // clear out all the buttons
   m_pbarGeneral->Clear();
   m_pbarView->Clear();
   m_pbarMisc->Clear();
   m_pbarObject->Clear();
   m_pRedo = m_pUndo = NULL;
   
   // general buttons
   PCIconButton pi;
   pi = m_pbarGeneral->ButtonAdd (1, IDB_SAVE, ghInstance, "Ctrl-S", "Save", "Saves the file.", IDC_SAVE);
   pi = m_pbarGeneral->ButtonAdd (1, IDB_GROUNDWATER, ghInstance, NULL,
      "Water",
      "Lets you change the water level so you can add an ocean, lake, pond, or slow-flowing river.",
      IDC_GROUNDWATER);
   pi = m_pbarGeneral->ButtonAdd (1, IDB_DIALOGBOX,ghInstance, 
      NULL, "General settings...",
      "General ground settings, such as adjusting the ground size.",
      IDC_GROUNDSET);

   pi = m_pbarGeneral->ButtonAdd (2, IDB_HELP, ghInstance, "F1", "Help...", "Brings up help and documentation.", IDC_HELPBUTTON);

   m_pbarGeneral->ButtonAdd (0, IDB_CLOSE, ghInstance, NULL, "Close",
      "Close this view.<p/>"
      "If you hold the control key down then " APPSHORTNAME " will shut down.", IDC_CLOSE);
   m_pbarGeneral->ButtonAdd (0, IDB_MINIMIZE, ghInstance, NULL, "Minimize", "Minimize the window.", IDC_MINIMIZE);
   m_pbarGeneral->ButtonAdd (0, IDB_SMALLWINDOW, ghInstance, NULL, "Shrink/expand window",
      "Shrinks a window taking up the entire screen into a small, draggable window. The "
      "opposite for small windows.<p/>"
      "Tip: Right-click in the small window to bring up the menu.", IDC_SMALLWINDOW);
   m_pbarGeneral->ButtonAdd (0, IDB_NEXTMONITOR,ghInstance,  NULL, "Next monitor", "Moves the view to the next monitor.", IDC_NEXTMONITOR);

   // miscellaneous buttons
   m_pRedo = pi = m_pbarMisc->ButtonAdd (2, IDB_REDO, ghInstance, "Ctrl-Y", "Redo", "Redo the last undo.", IDC_REDOBUTTON);
   m_pUndo = pi = m_pbarMisc->ButtonAdd (2, IDB_UNDO, ghInstance, "Ctrl-Z", "Undo", "Undo the last change.", IDC_UNDOBUTTON);


   // view choices
   pi = m_pbarView->ButtonAdd (0, IDB_GROUNDGENMTN, ghInstance, "F2", "Topography view",
      "Lets you change the elecation of the ground.", IDC_VIEWTOPO);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_LLOOP);
   pi = m_pbarView->ButtonAdd (0, IDB_OBJPAINT, ghInstance, "F3", "Texture view",
      "Lets you paint different textures.", IDC_VIEWTEXT);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_CLOOP);
   pi = m_pbarView->ButtonAdd (0, IDB_VIEWFOREST, ghInstance, "F4", "Forest view",
      "Lets you paint in different forest regions.", IDC_VIEWFOREST);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_RLOOP);

   // grid
   pi = m_pbarMisc->ButtonAdd (0, IDB_GROUNDSUN,ghInstance, 
      NULL, "Move the sun",
      "This tool moves the sunlight illuminating the ground map so you can more "
      "easily see the terrain elevation."
      "<p/>"
      "Moving the sun over the center of the image will remove all shadows."
      ,
      IDC_GROUNDSUN);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
   pi = m_pbarMisc->ButtonAdd (0, IDB_GRIDTOPO, ghInstance, NULL, "Topography lines",
      "Clicking this will bring up a menu that lets you choose how far apart to "
      "draw the topography lines.",
      IDC_GRIDTOPO);
   pi = m_pbarMisc->ButtonAdd (0, IDB_TRACING, ghInstance, NULL, "Tracing paper...",
      "Overlay scaned images of plans or photographs of the object so "
      "you can trace it into the model.", IDC_TRACING);

   pi = m_pbarView->ButtonAdd (1, IDB_ZOOMIN,ghInstance, 
      "+", "Zoom in",
      "To use this tool, click on the image. The camera will zoom in towards "
      "the point."
      ,
      IDC_ZOOMIN);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
   pi = m_pbarView->ButtonAdd (1, IDB_ZOOMOUT,ghInstance, 
      "-", "Zoom out",
      "To use this tool, click on the image. The camera will zoom out away "
      "from the point.",
      IDC_ZOOMOUT);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);


   m_pbarGeneral->AdjustAllButtons();
   m_pbarMisc->AdjustAllButtons();
   m_pbarView->AdjustAllButtons();
   m_pbarObject->AdjustAllButtons();

   // finally
   // Take this out NewView ();

   // get the undo and redo buttons set properly
   UndoUpdateButtons ();

}

/***********************************************************************************
CGroundView::NewView -Change to a new type of view

inputs
   DWORD       dwView - 0 for topo, 1 for texture, 2 for forestg
*/
void CGroundView::NewView (DWORD dwView)
{
   PCIconButton pi;

   if (dwView == m_dwView)
      return;  // no change
   m_dwView = dwView;

   // set the light for the new model
   DWORD dwButton;
   switch (dwView) {
   default:
   case 0:
      dwButton = IDC_VIEWTOPO;
      break;
   case 1:
      dwButton = IDC_VIEWTEXT;
      break;
   case 2:
      dwButton = IDC_VIEWFOREST;
      break;
   }
   m_pbarView->FlagsSet (dwButton, IBFLAG_BLUELIGHT, 0);

   ShowWindow (m_hWndElevKey, (dwView == 0) ? SW_SHOW : SW_HIDE);
   ShowWindow (m_hWndTextList, (dwView == 1) ? SW_SHOW : SW_HIDE);
   ShowWindow (m_hWndForestList, (dwView == 2) ? SW_SHOW : SW_HIDE);

   // clear out
   m_pbarMisc->Clear (1);
   m_pbarObject->Clear (1);

   // tools for topo
#define GENDESC \
      "Click on the center of the landscape feature and drag the mouse left or right "\
      "to control the area. Drag up or down to control the height. "\
      "Roll the mouse-wheel to try a different version."

   char pszGenDesc[] = GENDESC;

   if (dwView == 0) {
      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDGENVOLCANO, ghInstance, NULL, "Generate a volcanic mountain",
         pszGenDesc,
         IDC_GROUNDGENVOLCANO);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDGENMTN,ghInstance,  NULL, "Generate a mountain",
         pszGenDesc,
         IDC_GROUNDGENMTN);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDGENHILL, ghInstance, NULL, "Generate an aged mountain",
         pszGenDesc,
         IDC_GROUNDGENHILL);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDGENHILLROLL,ghInstance,  NULL, "Generate rolling hills",
         pszGenDesc,
         IDC_GROUNDGENHILLROLL);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);

      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDGENPLAT,ghInstance,  NULL, "Generate a plateau",
         pszGenDesc,
         IDC_GROUNDGENPLAT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDGENRAVINE, ghInstance, NULL, "Generate a canyon",
         "Tears away the ground a bit at a time. "
         "Use this tool several times along a river path to create your canyon."
         "<p/>"
         GENDESC,
         IDC_GROUNDGENRAVINE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDGENCRATER, ghInstance, NULL, "Generate a crater",
         pszGenDesc,
         IDC_GROUNDGENCRATER);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);

      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDGENRAISE,ghInstance,  NULL, "Raise the ground level",
         pszGenDesc,
         IDC_GROUNDGENRAISE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDGENLOWER, ghInstance, NULL, "Lower the ground level",
         pszGenDesc,
         IDC_GROUNDGENLOWER);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDGENFLAT, ghInstance, NULL, "Flatten the ground",
         pszGenDesc,
         IDC_GROUNDGENFLAT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);

      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDAPPLYRAIN,ghInstance,  NULL, "Rain erosion",
         "Creates gulleys and gorges, simulating rain.",
         IDC_GROUNDAPPLYRAIN);
      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDAPPLYWIND, ghInstance, NULL, "Wind erosion",
         "Smooths out the ground like thousands of years of wind expsoure would do.",
         IDC_GROUNDAPPLYWIND);
   }
   else {
      pi = m_pbarMisc->ButtonAdd (1, IDB_GROUNDPAINT,ghInstance,  NULL, "Paint based on elevation and slope",
         "Use this button to select a region of the landscape with a specific elevation or "
         "slope and paint it with a texture or forest.",
         IDC_GROUNDPAINT);
   }

   // common to all painting
   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSHSHAPE, ghInstance, NULL, "Brush shape",
      "This menu lets you chose the shape of the brush from pointy to flat.",
      IDC_BRUSHSHAPE);
   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSHEFFECT, ghInstance, NULL, "Brush effect",
      "This menu lets you change what the brush does, such as increasing or decreasing elevation.",
      IDC_BRUSHEFFECT);
   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSHSTRENGTH, ghInstance, NULL, "Brush strength",
      "This menu lets you control how large the brush's effect is.",
      IDC_BRUSHSTRENGTH);

   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH4,ghInstance, 
      "1", "Brush size (extra small)",
      "This tool lets you paint.",
      IDC_BRUSH4);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH8,ghInstance, 
      "2", "Brush size (small)",
      "This tool lets you paint.",
      IDC_BRUSH8);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH16,ghInstance, 
      "3", "Brush size (medium)",
      "This tool lets you paint.",
      IDC_BRUSH16);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH32,ghInstance, 
      "4", "Brush size (large)",
      "This tool lets you paint.",
      IDC_BRUSH32);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH64,ghInstance, 
      "5", "Brush size (extra large)",
      "This tool lets you paint.",
      IDC_BRUSH64);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

   DWORD adwWant[3];
   adwWant[0] = IDC_BRUSH32;
   adwWant[1] = IDC_GROUNDSUN;
   adwWant[2] = IDC_ZOOMOUT;

   // if button no longer exists then set a new one
   DWORD i;
   for (i = 0; i < 3; i++)
      if (!ButtonExists(m_adwPointerMode[i]))
         SetPointerMode (adwWant[i], i);
      else {
         // BUGFIX - reset the existing one just to make sure displayed
         SetPointerMode (m_adwPointerMode[i], i, FALSE);
      }

   m_pbarMisc->AdjustAllButtons();
   m_pbarObject->AdjustAllButtons();
   InvalidateDisplay();

}

/***********************************************************************************
CGroundView::InvalidateDisplay - Redraws the display area, not the buttons.
*/
void CGroundView::InvalidateDisplay (void)
{
   InvalidateRect (m_hWndMap, NULL, FALSE);
}


/**********************************************************************************
CGroundView::SetProperCursor - Call this to set the cursor to whatever is appropriate
for the window
*/
void CGroundView::SetProperCursor (int iX, int iY)
{
   fp fX, fY;
   DWORD dwButton = m_dwButtonDown ? (m_dwButtonDown-1) : 0;

   if (PointInImage (iX, iY, &fX, &fY)) {
      switch (m_adwPointerMode[dwButton]) {
         case IDC_GROUNDGENMTN:
         case IDC_GROUNDGENVOLCANO:
         case IDC_GROUNDGENRAISE:
         case IDC_GROUNDGENFLAT:
         case IDC_GROUNDGENLOWER:
         case IDC_GROUNDGENHILL:
         case IDC_GROUNDGENCRATER:
         case IDC_GROUNDGENHILLROLL:
         case IDC_GROUNDGENPLAT:
         case IDC_GROUNDGENRAVINE:
            SetCursor (LoadCursor (NULL, IDC_SIZEALL));
            break;

         case IDC_BRUSH4:
            if (PointInImage (iX, iY))
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH4)));
            else
               SetCursor (LoadCursor (NULL, IDC_NO));
            break;
         case IDC_BRUSH8:
            if (PointInImage (iX, iY))
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH8)));
            else
               SetCursor (LoadCursor (NULL, IDC_NO));
            break;
         case IDC_BRUSH16:
            if (PointInImage (iX, iY))
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH16)));
            else
               SetCursor (LoadCursor (NULL, IDC_NO));
            break;
         case IDC_BRUSH32:
            if (PointInImage (iX, iY))
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH32)));
            else
               SetCursor (LoadCursor (NULL, IDC_NO));
            break;
         case IDC_BRUSH64:
            if (PointInImage (iX, iY))
               SetCursor((HCURSOR)LoadImage (ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH64), IMAGE_CURSOR, 64, 64,LR_SHARED));
               //SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH64)));
            else
               SetCursor (LoadCursor (NULL, IDC_NO));
            break;

         case IDC_ZOOMIN:
            if (PointInImage (iX, iY))
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORZOOMIN)));
            else
               SetCursor (LoadCursor (NULL, IDC_NO));
            break;

         case IDC_GROUNDSUN:
            SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORGROUNDSUN)));
            break;

         case IDC_ZOOMOUT:
            if (PointInImage (iX, iY))
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORZOOMOUT)));
            else
               SetCursor (LoadCursor (NULL, IDC_NO));
            break;

         default:
            SetCursor (LoadCursor (NULL, IDC_NO));
      }
   }
   else {
      // don't click cursor
      SetCursor (LoadCursor (NULL, IDC_NO));
   }
}

/***********************************************************************************
CGroundView::ButtonDown - Left button down message
*/
LRESULT CGroundView::ButtonDown (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton)
{
   DWORD dwPointerMode = m_adwPointerMode[dwButton];
   int iX, iY;
   iX = LOWORD(lParam);
   iY = HIWORD(lParam);

   // BUGFIX - If a button is already down then ignore the new one
   if (m_dwButtonDown)
      return 0;

   // if it's in one of the thumbnails then switch to that
   POINT pt;
   fp fX, fY;
   pt.x = iX;
   pt.y = iY;

   // if it's not in the image are or don't have image mode then return
   if (!dwPointerMode || !PointInImage(iX, iY, &fX, &fY)) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return 0;
   }

   // for some click operations just want to act now and not set capture of anything
   if ((dwPointerMode == IDC_ZOOMIN) || (dwPointerMode == IDC_ZOOMOUT)) {
      BOOL fZoomIn = (dwPointerMode == IDC_ZOOMIN);
      RECT r;
      GetClientRect (m_hWndMap, &r);

      if (fZoomIn) {
         m_fViewScale *= sqrt((fp)2);
         m_fViewScale = min(10, m_fViewScale);  // no more than 10 pixels per point
      }
      else {   // zoom out
         m_fViewScale /= sqrt((fp)2);
         m_fViewScale = max(100.0 / (fp)max(m_gState.dwWidth, m_gState.dwHeight), m_fViewScale);
      }

      // orient so that pooint clicked on is in center
      m_tpViewUL.h = fX - (fp)(r.right + r.left) / 2.0 / m_fViewScale;
      m_tpViewUL.v = fY - (fp)(r.bottom + r.top) / 2.0 / m_fViewScale;

      // redraw
      InvalidateDisplay ();
      MapScrollUpdate ();
      return 0;
   };

   // capture
   SetCapture (m_hWndMap);
   m_fCaptured = TRUE;

   // remember this position
   m_dwButtonDown = dwButton + 1;
   m_pntButtonDown.x = iX;
   m_pntButtonDown.y = iY;
   m_pntMouseLast = m_pntButtonDown;

   switch (dwPointerMode) {
   case IDC_GROUNDGENPLAT:
   case IDC_GROUNDGENRAVINE:
   case IDC_GROUNDGENMTN:
   case IDC_GROUNDGENVOLCANO:
   case IDC_GROUNDGENRAISE:
   case IDC_GROUNDGENFLAT:
   case IDC_GROUNDGENLOWER:
   case IDC_GROUNDGENHILL:
   case IDC_GROUNDGENCRATER:
   case IDC_GROUNDGENHILLROLL:
      {
         // allocate enough memory to store the original image
         DWORD dwSize = m_gState.dwWidth * m_gState.dwHeight * sizeof(WORD);
         if (!m_memGroundGenTemp.Required (dwSize)) {
            ReleaseCapture ();
            m_fCaptured = FALSE;
            m_dwButtonDown = 0;
            return 0;
         }

         // backup
         memcpy (m_memGroundGenTemp.p, m_gState.pawElev, dwSize);
         m_rGroundRangeLast.left = m_rGroundRangeLast.right = m_rGroundRangeLast.top = m_rGroundRangeLast.bottom = 0;
         m_dwGroundGenSeed++; // so different seed every time

         // draw it
         GroundGen (FALSE);
      }
      break;

   case IDC_GROUNDSUN:
      // send a mouse move so it gets updated
      m_pntMouseLast.x = m_pntMouseLast.y = -1000; // so will update
      MouseMove (hWnd, uMsg, wParam, lParam);
      break;
   case IDC_BRUSH4:
   case IDC_BRUSH8:
   case IDC_BRUSH16:
   case IDC_BRUSH32:
   case IDC_BRUSH64:
      // If airbrush ignore and set timer instead
      if (m_dwBrushAir == 2) {
         if (!m_fAirbrushTimer)
            SetTimer (m_hWnd, TIMER_AIRBRUSH, 1000 / AIRBRUSHPERSEC, NULL);
         m_fAirbrushTimer = TRUE;
         break;
      }

      BrushApply (dwPointerMode, &m_pntButtonDown, NULL);
      break;
   }


   // BUGFIX: update cursor and tooltip
   // update the tooltip and cursor
   SetProperCursor (iX, iY);

   return 0;
}

/***********************************************************************************
CGroundView::ButtonUp - Left button down message
*/
LRESULT CGroundView::ButtonUp (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton)
{
   DWORD dwPointerMode = m_adwPointerMode[dwButton];

   // BUGFIX - If the button is not the correct one then ignore
   if (m_dwButtonDown && (dwButton+1 != m_dwButtonDown))
      return 0;

   if (m_fCaptured)
      ReleaseCapture ();
   m_fCaptured = FALSE;

   // BUGFIX - If don't have buttondown flag set then dont do any of the following
   // operations. Otherwise have problem which createa a new object, click on wall to
   // create, and ends up getting mouse-up in this window (from the mouse down in the
   // object selection window)
   if (!m_dwButtonDown)
      return 0;


   switch (dwPointerMode) {
   case IDC_GROUNDGENMTN:
   case IDC_GROUNDGENPLAT:
   case IDC_GROUNDGENRAVINE:
   case IDC_GROUNDGENVOLCANO:
   case IDC_GROUNDGENRAISE:
   case IDC_GROUNDGENFLAT:
   case IDC_GROUNDGENLOWER:
   case IDC_GROUNDGENHILL:
   case IDC_GROUNDGENCRATER:
   case IDC_GROUNDGENHILLROLL:
      GroundGen (TRUE);
      UndoCache ();
      break;
   case IDC_BRUSH4:
   case IDC_BRUSH8:
   case IDC_BRUSH16:
   case IDC_BRUSH32:
   case IDC_BRUSH64:
      // Make sure airbrush timer is killed
      if (m_fAirbrushTimer)
         KillTimer (m_hWnd, TIMER_AIRBRUSH);
      m_fAirbrushTimer = FALSE;
      break;
   }

   // BUGFIX - Move buttondown clear to after GroundGen()
   m_dwButtonDown = 0;

   // update the tooltip and cursor
   int iX, iY;
   iX = (short) LOWORD(lParam);
   iY = (short) HIWORD(lParam);
   SetProperCursor (iX, iY);

   return 0;
}

/***********************************************************************************
CGroundView::MouseMove - Left button down message
*/
LRESULT CGroundView::MouseMove (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (GetFocus() != m_hWnd)
      SetFocus (m_hWnd);   // so list box doesnt take away focus

   DWORD dwButton = m_dwButtonDown ? (m_dwButtonDown - 1) : 0;
   DWORD dwPointerMode = m_adwPointerMode[dwButton];
   int iX, iY;
   iX = (short) LOWORD(lParam);
   iY = (short) HIWORD(lParam);
   POINT pLast;
   pLast = m_pntMouseLast;
   m_pntMouseLast.x = iX;
   m_pntMouseLast.y = iY;


   // BUGFIX - If it hasn't moved then dont go futher
   if ((pLast.x == m_pntMouseLast.x) && (pLast.y == m_pntMouseLast.y))
      return 0;

   // set the cursor
   SetProperCursor (iX, iY);


   if (m_dwButtonDown && dwPointerMode) switch (dwPointerMode) {
   case IDC_GROUNDGENMTN:
   case IDC_GROUNDGENPLAT:
   case IDC_GROUNDGENRAVINE:
   case IDC_GROUNDGENVOLCANO:
   case IDC_GROUNDGENRAISE:
   case IDC_GROUNDGENFLAT:
   case IDC_GROUNDGENLOWER:
   case IDC_GROUNDGENHILL:
   case IDC_GROUNDGENCRATER:
   case IDC_GROUNDGENHILLROLL:
      // draw it
      GroundGen (FALSE);
      break;

   case IDC_BRUSH4:
   case IDC_BRUSH8:
   case IDC_BRUSH16:
   case IDC_BRUSH32:
   case IDC_BRUSH64:
      // BUGFIX - If we're in pen mode and only moved a couple pixels then dont do
      // anything
      if ((m_dwBrushAir == 0) && (abs(m_pntMouseLast.x - pLast.x) < 4) &&
         (abs(m_pntMouseLast.y - pLast.y) < 4)) {
            m_pntMouseLast = pLast;
            break;
         }

      // If airbrush ignore because will be servied by timer
      if (m_dwBrushAir == 2)
         break;

      BrushApply (dwPointerMode, &m_pntMouseLast, &pLast);
      break;
   case IDC_GROUNDSUN:
      {
         // conver the x and y to 0..1
         RECT r;
         GetClientRect (m_hWndMap, &r);
         CPoint p;
         fp fLen;
         p.p[0] = (fp)iX / (fp) (r.right - r.left) * 2.0 - 1;
         p.p[1] = -((fp)iY / (fp) (r.bottom - r.top) * 2.0 - 1);
         fLen = sqrt(p.p[0] * p.p[0] + p.p[1] * p.p[1]);
         if (fLen < 1)
            p.p[2] = sqrt(1 - fLen * fLen);
         else
            p.p[2] = 0;
         p.Normalize();

         m_pLight.Copy (&p);
         InvalidateDisplay ();
      }
      break;
   }

   return 0;

}

/***********************************************************************************
CGroundView::Paint - WM_PAINT messge
*/
LRESULT CGroundView::Paint (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   PAINTSTRUCT ps;
   HDC   hDC;
   hDC = BeginPaint (hWnd, &ps);

   // size
   RECT  r;
   DWORD i;
   GetClientRect (hWnd, &r);


   // paint the cool border
   RECT  rt;//, inter;

   HBRUSH hbr;
   // dark grey
   hbr = (HBRUSH) CreateSolidBrush (m_cBackDark);

   // top
   rt = r;
   rt.left += VARBUTTONSIZE;
   rt.right -= VARBUTTONSIZE;
   rt.bottom = VARBUTTONSIZE;
   FillRect (hDC, &rt, hbr);

   // right
   rt = r;
   rt.left = rt.right - VARBUTTONSIZE;
   FillRect (hDC, &rt, hbr);

   // bottom
   rt = r;
   rt.top = rt.bottom - VARBUTTONSIZE;
   rt.left += VARBUTTONSIZE;
   rt.right -= VARBUTTONSIZE;
   FillRect (hDC, &rt, hbr);

   // left
   rt = r;
   rt.right = rt.left + VARBUTTONSIZE;
   FillRect (hDC, &rt, hbr);

   DeleteObject (hbr);


   // light selection color
   hbr = (HBRUSH) CreateSolidBrush (m_cBackLight);

   rt = r;
   rt.left += VARBUTTONSIZE;
   rt.right -= VARBUTTONSIZE;
   rt.bottom -= VARBUTTONSIZE;
   rt.top += VARBUTTONSIZE;
   FillRect (hDC, &rt, hbr);

   DeleteObject (hbr);

   // line around the tabs
   HPEN hPenOld, hPen;
   hPen = CreatePen (PS_SOLID, 0, m_cBackOutline);
   hPenOld = (HPEN) SelectObject (hDC, hPen);
   for (i = 0; i < 4; i++) {
      PCButtonBar pbb;
      DWORD dwDim;   // 0 for drawing along X, 1 for drawing along Y
      int iX, iY, iStart, iStop;
      iX = iY = 0;
      switch (i) {
      case 0:
         pbb = m_pbarView;
         dwDim = 0;
         iY = r.top + VARBUTTONSIZE;
         break;
      case 1:
         pbb = m_pbarGeneral;
         dwDim = 1;
         iX = r.right - VARBUTTONSIZE - 1;
         break;
      case 2:
         pbb = m_pbarObject;
         dwDim = 0;
         iY = r.bottom - VARBUTTONSIZE - 1;
         break;
      case 3:
         pbb = m_pbarMisc;
         dwDim = 1;
         iX = r.left + VARBUTTONSIZE;
         break;
      }
      iStart = (dwDim == 0) ? (r.left + VARBUTTONSIZE) : (r.top + VARBUTTONSIZE);
      iStop = (dwDim == 0) ? (r.right - VARBUTTONSIZE) : (r.bottom - VARBUTTONSIZE);

      // look for the selected one
      DWORD j, k;
      PCIconButton pBut;
      pBut = NULL;
      for (j = 0; j < 3; j++) {
         for (k = 0; k < pbb->m_alPCIconButton[j].Num(); k++) {
            pBut = *((PCIconButton*) pbb->m_alPCIconButton[j].Get(k));
            if (pBut->FlagsGet() & IBFLAG_REDARROW)
               break;
            pBut = NULL;
         }
         if (pBut)
            break;
      }

      // start and stop...
      MoveToEx (hDC, (dwDim == 0) ? iStart : iX, (dwDim == 0) ? iY : iStart, NULL);
      RECT rBut;
      POINT *pp;
      pp = (POINT*) &rBut;
      if (pBut) {
         int iMin, iMax;
         GetWindowRect (pBut->m_hWnd, &rBut);
         ScreenToClient (m_hWnd, &pp[0]);
         ScreenToClient (m_hWnd, &pp[1]);
         if (dwDim == 0) {
            iMin = rBut.left;
            iMax = rBut.right;
         }
         else {
            iMin = rBut.top;
            iMax = rBut.bottom;
         }

         if (i < 2)
            iMax++;  // so draw one past
         else
            iMin--;
         LineTo (hDC, (dwDim == 0) ? iMin : iX, (dwDim == 0) ? iY : iMin);
         MoveToEx (hDC, (dwDim == 0) ? iMax : iX, (dwDim == 0) ? iY : iMax, NULL);

      }
      LineTo (hDC, (dwDim == 0) ? iStop : iX, (dwDim == 0) ? iY : iStop);

   } // i
   SelectObject (hDC, hPenOld);
   DeleteObject (hPen);


   EndPaint (hWnd, &ps);


   return 0;
}



/***********************************************************************************
CGroundView::PaintMap - WM_PAINT messge
*/
LRESULT CGroundView::PaintMap (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   PAINTSTRUCT ps;
   HDC hDC;
   hDC = BeginPaint (hWnd, &ps);
   RECT r;
   GetClientRect (hWnd, &r);
   BOOL fWhiteElev = FALSE;

   // allocate enough in the image
   if ((ps.rcPaint.right <= ps.rcPaint.left) || (ps.rcPaint.bottom <= ps.rcPaint.top))
      goto done;
   if (!m_ImageTemp.Init ((DWORD)(ps.rcPaint.right - ps.rcPaint.left)+2,
      (DWORD)(ps.rcPaint.bottom - ps.rcPaint.top)+2))
      goto done;

   DWORD x, y;
   TEXTUREPOINT tpLoc;
   WORD wElev;
   PIMAGEPIXEL pip;
   pip = m_ImageTemp.Pixel (0, 0);
   CalcWaterColorScheme ();
   for (y = 0; y < m_ImageTemp.Height(); y++) for (x = 0; x < m_ImageTemp.Width(); x++, pip++) {
      // figure out the location
      tpLoc.h = (fp)((int)x + ps.rcPaint.left - 1) / m_fViewScale + m_tpViewUL.h;
      tpLoc.v = (fp)((int)y + ps.rcPaint.top - 1) / m_fViewScale + m_tpViewUL.v;

      // get the height
      if (!HVToElev (tpLoc.h, tpLoc.v, &wElev)) {
         pip->wRed = pip->wGreen = pip->wBlue = 0;
         pip->fZ = -ZINFINITE;
         continue;
      }

      // store the height
      pip->fZ = (fp)wElev / (fp)0xffff * (m_gState.tpElev.v - m_gState.tpElev.h) +
         m_gState.tpElev.h;


      if (m_dwView == 0) { // topo
         // color to show with
         fp fColor;
         DWORD dwColor;
         COLORREF *pawScheme;
         if (wElev >= m_wWaterMark) {
            fColor = (fp) (wElev - m_wWaterMark) / (fp)(0xffff - m_wWaterMark);
            pawScheme = &m_acColorScheme[0];
         }
         else {
            fColor = (fp) wElev / (fp)m_wWaterMark;
            pawScheme = &m_acColorSchemeWater[0];
         }
         fColor = fColor * (fp)(GVCOLORSCHEME-1);
         fColor = min(fColor, (fp)GVCOLORSCHEME - 1 - CLOSE);
         dwColor = (DWORD) fColor;
         fColor -= (fp) dwColor;

         // get the two
         WORD awc[2][3];
         Gamma (pawScheme[dwColor], awc[0]);
         Gamma (pawScheme[dwColor+1], awc[1]);
         pip->wRed = (WORD) ((1.0 - fColor) * (fp) awc[0][0] + fColor * (fp)awc[1][0]);
         pip->wGreen = (WORD) ((1.0 - fColor) * (fp) awc[0][1] + fColor * (fp)awc[1][1]);
         pip->wBlue = (WORD) ((1.0 - fColor) * (fp) awc[0][2] + fColor * (fp)awc[1][2]);
      } // if topo
      else if ((m_dwView == 1) || (m_dwView == 2)) { // if texture or forest
         // need to figure out 4 surrounding colors
         int ix, iy;
         fp fh, fv;
         ix = (int) floor(tpLoc.h);
         iy = (int) floor(tpLoc.v);
         ix = min(ix, (int)m_gState.dwWidth-2);
         iy = min(iy, (int)m_gState.dwHeight-2);
         fh = tpLoc.h - ix;
         fv = tpLoc.v - iy;
         
         PBYTE pb;
         DWORD dwNum;
         DWORD dwScale;
         if (m_dwView == 1) {
            pb = &m_gState.pabTextSet[ix + iy * m_gState.dwWidth];
            dwNum = m_gState.plTextSurf->Num();
         }
         else {
            pb = &m_gState.pabForestSet[ix + iy * m_gState.dwWidth];
            dwNum = m_gState.plPCForest->Num();
         }

         DWORD adwColor[2][2][3];
         DWORD adwCount[2][2];
         WORD awGamma[3];
         BYTE abVal[2][2];
         DWORD dwLayer, cx, cy, cc;
         memset (adwColor, 0, sizeof(adwColor));
         memset (adwCount, 0, sizeof(adwCount));

         // BUGFIX - If painting forest then include a background color to blend with
         if (m_dwView == 2) {
            Gamma (RGB(0x0,0x0,0x0), awGamma);
            for (cx = 0; cx < 2; cx++) for (cy = 0; cy < 2; cy++) {
               for (cc = 0; cc < 3; cc++)
                  adwColor[cx][cy][cc] = awGamma[cc];
               adwCount[cx][cy]=256;
            }
            fWhiteElev = TRUE;   // otherwise can't see
         }

         COLORREF *pc;
         pc = (COLORREF*) m_gState.plTextColor->Get(0);
         PCForest *ppf;
         ppf = (PCForest*) m_gState.plPCForest->Get(0);
         for (dwLayer = 0; dwLayer < dwNum; dwLayer++) {
            dwScale = m_gState.dwWidth * m_gState.dwHeight * dwLayer;
            abVal[0][0] = pb[0 + dwScale];
            abVal[1][0] = pb[1 + dwScale];
            abVal[1][1] = pb[1 + m_gState.dwWidth + dwScale];
            abVal[0][1] = pb[m_gState.dwWidth + dwScale];
            if (!abVal[0][0] && !abVal[0][1] && !abVal[1][0] && !abVal[1][1])
               continue;   // nothing to show

            // get the color
            Gamma ((m_dwView == 1) ? pc[dwLayer] : ppf[dwLayer]->m_cColor, awGamma);

            for (cx = 0; cx < 2; cx++) for (cy = 0; cy < 2; cy++) {
               for (cc = 0; cc < 3; cc++)
                  adwColor[cx][cy][cc] += (DWORD)awGamma[cc] * (DWORD) abVal[cx][cy];
               adwCount[cx][cy] += (DWORD) abVal[cx][cy];
            }
         }
         // average out the colors
         for (cx = 0; cx < 2; cx++) for (cy = 0; cy < 2; cy++) {
            if (adwCount[cx][cy])
               for (cc = 0; cc < 3; cc++)
                  adwColor[cx][cy][cc] /= adwCount[cx][cy];
            else {
               // always at least a dark green
               Gamma (RGB(0x40,0x60,0x40), awGamma);
               for (cc = 0; cc < 3; cc++)
                  adwColor[cx][cy][cc] = awGamma[cc];
            }
         }

         float afc[2][3];

         // if highlighting selected areas then might want to adjust color
         if (m_fPaintOn) {
            int x, y;
            fp f;
            for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) {
               f = IsPixelPainted ((DWORD) (ix + x), (DWORD)(iy + y));
               if (!f)
                  continue;

               adwColor[x][y][0] = (DWORD)(f * (fp)0xffff + (1.0 - f) * (fp) adwColor[x][y][0]);
               adwColor[x][y][1] = (DWORD) ((1.0 - f) * (fp)adwColor[x][y][1]);
               adwColor[x][y][2] = (DWORD) ((1.0 - f) * (fp)adwColor[x][y][2]);
            }
         }

         DWORD i,j;
         for (i = 0; i < 2; i++) for (j = 0; j < 3; j++)
            afc[i][j] = (1.0 - fv) * (fp)adwColor[i][0][j] + fv * (fp)adwColor[i][1][j];
         pip->wRed = (WORD)((1.0 - fh) * afc[0][0] + fh * afc[1][0]);
         pip->wGreen = (WORD)((1.0 - fh) * afc[0][1] + fh * afc[1][1]);
         pip->wBlue = (WORD)((1.0 - fh) * afc[0][2] + fh * afc[1][2]);
      }  // if texture

   }

   // do the shadows
   int iWidth;
   fp fMetersPerPixel;
   iWidth = (int)m_ImageTemp.Width();
   fMetersPerPixel = m_gState.fScale / m_fViewScale;
   fp fLight;
   CPoint pLR, pUD, pN;
   pLR.Zero();
   pUD.Zero();
   if (m_pLight.p[2] < .95) for (y = 1; y < m_ImageTemp.Height()-1; y++) {
      pip = m_ImageTemp.Pixel (1, y);
      for (x = 1; x < (DWORD)iWidth-1; x++, pip++) {
         if (pip->fZ == -ZINFINITE)
            continue;   // out of bounds

         // left and right
         if (pip[1].fZ != -ZINFINITE) {
            pLR.p[2] = pip[1].fZ;
            pLR.p[0] = fMetersPerPixel;
         }
         else
            pLR.p[2] = pip[0].fZ;

         if (pip[-1].fZ != -ZINFINITE) {
            pLR.p[2] -= pip[-1].fZ;
            pLR.p[0] += fMetersPerPixel;
         }
         else
            pLR.p[2] -= pip[0].fZ;

         // top and bottom
         if (pip[iWidth].fZ != -ZINFINITE) {
            pUD.p[2] = pip[iWidth].fZ;
            pUD.p[1] = -fMetersPerPixel;
         }
         else
            pUD.p[2] = pip[0].fZ;

         if (pip[-iWidth].fZ != -ZINFINITE) {
            pUD.p[2] -= pip[-iWidth].fZ;
            pUD.p[1] -= fMetersPerPixel;
         }
         else
            pUD.p[2] -= pip[0].fZ;

         // calc normal
         pN.CrossProd (&pUD, &pLR);
         pN.Normalize();

         // intensity
         fLight = pN.DotProd (&m_pLight);
         fLight = max(0, fLight);
         fLight = fLight * .666 + .333;  // have ambient

         // scale
         pip->wRed = (WORD) ((fp)pip->wRed * fLight);
         pip->wGreen = (WORD) ((fp)pip->wGreen * fLight);
         pip->wBlue = (WORD) ((fp)pip->wBlue * fLight);
     } // x
   } // y

   // Paint lines of elevation
#define TESTAROUND   3
   fp afTest[TESTAROUND];
   fp fFloor, fFloor2;
   DWORD i;
   for (y = 1; y < m_ImageTemp.Height()-1; y++) {
      pip = m_ImageTemp.Pixel (1, y);
      for (x = 1; x < (DWORD)iWidth-1; x++, pip++) {
         if (pip->fZ == -ZINFINITE)
            continue;   // out of bounds

         afTest[0] = pip->fZ;
         afTest[1] = (pip[-iWidth].fZ == -ZINFINITE) ? pip->fZ : pip[-iWidth].fZ;
         afTest[2] = (pip[1].fZ == -ZINFINITE) ? pip->fZ : pip[1].fZ;
         //afTest[3] = (pip[iWidth].fZ == -ZINFINITE) ? pip->fZ : pip[iWidth].fZ;
         //afTest[4] = (pip[-1].fZ == -ZINFINITE) ? pip->fZ : pip[-1].fZ;

         if (m_fGridUDMajor) {
            fFloor = floor(afTest[0] / m_fGridUDMajor);
            for (i = 1; i < TESTAROUND; i++) {
               fFloor2 = floor(afTest[i] / m_fGridUDMajor);
               if (fFloor != fFloor2)
                  break;
            }
            if (i < TESTAROUND) {
               // crossed major line
               pip->wRed = pip->wGreen = pip->wBlue = fWhiteElev ? 0xffff : 0;
               continue;
            }
         } // m_fGridUDMajor

         if (m_fGridUDMinor) {
            fFloor = floor(afTest[0] / m_fGridUDMinor);
            for (i = 1; i < TESTAROUND; i++) {
               fFloor2 = floor(afTest[i] / m_fGridUDMinor);
               if (fFloor != fFloor2)
                  break;
            }
            if (i < TESTAROUND) {
               // crossed major line
               pip->wRed /= 2;
               pip->wGreen /= 2;
               pip->wBlue /= 2;
               if (fWhiteElev) {
                  pip->wRed += 0x8000;
                  pip->wGreen += 0x8000;
                  pip->wBlue += 0x8000;
               }
               continue;
            }
         } // m_fGridUDMinor

      }//x
   }//y

   // Paint tracing paper image on top
   m_pTraceOffset.x = ps.rcPaint.left - 1;
   m_pTraceOffset.y = ps.rcPaint.top - 1;
   TraceApply (&m_lTraceInfo, this, &m_ImageTemp);


   // conver this to a bitmpa
   POINT pDest;
   RECT rSrc;
   pDest.x = ps.rcPaint.left;
   pDest.y = ps.rcPaint.top;
   rSrc.top = rSrc.left = 1;
   rSrc.right = m_ImageTemp.Width()-1;
   rSrc.bottom = m_ImageTemp.Height()-1;
   m_ImageTemp.Paint (hDC, &rSrc, pDest);


   // paint XY grid
   RECT rTick;
   rTick = ps.rcPaint;
   rTick.left = max(r.left, rTick.left - 30);  // add a buffer around so if only painting
         // part of the screen will all be seemless
   rTick.right = min(r.right, rTick.right + 30);
   rTick.top = max(r.top, rTick.top - 30);
   rTick.bottom = min(r.bottom, rTick.bottom + 30);
   TicksVertical (hDC, &r, &rTick, RGB(0xc0,0x80,0x80), RGB(0xff,0, 0));
   TicksHorizontal (hDC, &r, &rTick, RGB(0xc0,0x80,0x80), RGB(0xff,0, 0));



done:
   EndPaint (hWnd, &ps);
   return 0;

}



/***********************************************************************************
CGroundView::PaintKey - WM_PAINT messge
*/
LRESULT CGroundView::PaintKey (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   PAINTSTRUCT ps;
   HDC hDC;
   hDC = BeginPaint (hWnd, &ps);
   RECT r;
   GetClientRect (hWnd, &r);

   // allocate enough in the image
   if ((ps.rcPaint.right <= ps.rcPaint.left) || (ps.rcPaint.bottom <= ps.rcPaint.top))
      goto done;


   // paint elevation map
   if (!m_ImageTemp.Init ((DWORD)(ps.rcPaint.right - ps.rcPaint.left),
      (DWORD)(ps.rcPaint.bottom - ps.rcPaint.top)))
      goto done;

   DWORD x, y;
   PIMAGEPIXEL pip;
   WORD awColor[3];
   pip = m_ImageTemp.Pixel (0, 0);
   CalcWaterColorScheme ();
   fp fWaterMark;
   fWaterMark = (fp) m_wWaterMark / (fp) 0xffff;
   for (y = 0; y < m_ImageTemp.Height(); y++) {
      // what's the color?
      fp fColor;
      DWORD dwColor;
      fColor = 1.0 - (fp) ((int)y + ps.rcPaint.top) / (fp) r.bottom;
      COLORREF *pawScheme;
      if (fColor >= fWaterMark) {
         fColor = (fColor - fWaterMark) / (1.0 - fWaterMark);
         pawScheme = &m_acColorScheme[0];
      }
      else {
         fColor =  fColor / fWaterMark;
         pawScheme = &m_acColorSchemeWater[0];
      }
      fColor = max(0,fColor);
      fColor = min(1,fColor);
      fColor = fColor * (fp)(GVCOLORSCHEME-1);
      fColor = min(fColor, (fp)GVCOLORSCHEME - 1 - CLOSE);
      dwColor = (DWORD) fColor;
      fColor -= (fp) dwColor;

      // get the two
      WORD awc[2][3];
      Gamma (pawScheme[dwColor], awc[0]);
      Gamma (pawScheme[dwColor+1], awc[1]);
      awColor[0] = (WORD) ((1.0 - fColor) * (fp) awc[0][0] + fColor * (fp)awc[1][0]);
      awColor[1] = (WORD) ((1.0 - fColor) * (fp) awc[0][1] + fColor * (fp)awc[1][1]);
      awColor[2] = (WORD) ((1.0 - fColor) * (fp) awc[0][2] + fColor * (fp)awc[1][2]);

      for (x = 0; x < m_ImageTemp.Width(); x++, pip++) {
         pip->wRed = awColor[0];
         pip->wGreen = awColor[1];
         pip->wBlue = awColor[2];
      } // x
   } // y


   // conver this to a bitmpa
   POINT pDest;
   pDest.x = ps.rcPaint.left;
   pDest.y = ps.rcPaint.top;
   m_ImageTemp.Paint (hDC, NULL, pDest);


   // paint XY grid
   RECT rTick;
   rTick = ps.rcPaint;
   rTick.left = max(r.left, rTick.left - 30);  // add a buffer around so if only painting
         // part of the screen will all be seemless
   rTick.right = min(r.right, rTick.right + 30);
   rTick.top = max(r.top, rTick.top - 30);
   rTick.bottom = min(r.bottom, rTick.bottom + 30);
   TicksHorizontal (hDC, &r, &rTick, RGB(0x40,0x40,0x40), RGB(0x0,0, 0), TRUE);

done:
   EndPaint (hWnd, &ps);
   return 0;

}

/***********************************************************************************
CGroundView::UpDownSample - Converts the resoltion by up/downsampling. NOTE: At the
moment this does NO antialiasing other than linear interp.

inputs
   fp       fScale - Scale the number of pixels up/down by this much
returns
   BOOL - TRUE if success
*/
BOOL CGroundView::UpDownSample (fp fScale)
{
   DWORD dwWidth, dwHeight;
   dwWidth = (DWORD) ((fp)m_gState.dwWidth * fScale);
   dwHeight = (DWORD) ((fp)m_gState.dwHeight * fScale);
   dwWidth = max(dwWidth, 2);
   dwHeight = max(dwHeight, 2);

   // enoug memory
   CMem memElev, memTextSet, memForestSet;
   if (!memElev.Required (dwWidth * dwHeight * sizeof(WORD)))
      return FALSE;
   if (!memTextSet.Required (dwWidth * dwHeight * sizeof(BYTE) * m_gState.plTextSurf->Num()))
      return FALSE;
   if (!memForestSet.Required (dwWidth * dwHeight * sizeof(BYTE) * m_gState.plPCForest->Num()))
      return FALSE;

   WORD *paw;
   BYTE *pabTextSet, *pabForestSet;
   paw = (WORD*) memElev.p;
   pabTextSet = (BYTE*) memTextSet.p;
   pabForestSet = (BYTE*) memForestSet.p;

   // convert
   DWORD x,y;
   fp fx, fy;
   DWORD dwLayer;
   for (y = 0; y < dwHeight; y++) {
      fy = (fp) y / fScale;
      fy = min(fy, (fp)m_gState.dwHeight-1);
      for (x = 0; x < dwWidth; x++, paw++, pabTextSet++, pabForestSet++) {
         fx = (fp) x / fScale;
         fx = min(fx, (fp)m_gState.dwWidth-1);

         HVToElev (fx, fy, paw);

         // copy the texture
         for (dwLayer = 0; dwLayer < m_gState.plTextSurf->Num(); dwLayer++)
            pabTextSet[dwLayer * dwWidth * dwHeight] = m_gState.pabTextSet[
               (int)(fx+.5) + (int)(fy+.5) * m_gState.dwWidth +
                  dwLayer * m_gState.dwWidth * m_gState.dwHeight];

         // copy the forest
         for (dwLayer = 0; dwLayer < m_gState.plPCForest->Num(); dwLayer++)
            pabForestSet[dwLayer * dwWidth * dwHeight] = m_gState.pabForestSet[
               (int)(fx+.5) + (int)(fy+.5) * m_gState.dwWidth +
                  dwLayer * m_gState.dwWidth * m_gState.dwHeight];
      } // x
   } // y

   // set undo
   UndoAboutToChange();

   // new values
   if (!m_gState.pmemElev->Required (dwWidth * dwHeight * sizeof(WORD)))
      return FALSE;
   m_gState.pawElev = (WORD*)m_gState.pmemElev->p;
   memcpy (m_gState.pawElev, memElev.p, dwWidth * dwHeight * sizeof(WORD));

   if (!m_gState.pmemTextSet->Required (dwWidth * dwHeight * sizeof(BYTE) * m_gState.plTextSurf->Num()))
      return FALSE;
   m_gState.pabTextSet = (BYTE*)m_gState.pmemTextSet->p;
   memcpy (m_gState.pabTextSet, memTextSet.p, dwWidth * dwHeight * sizeof(BYTE) * m_gState.plTextSurf->Num());

   // forest
   if (!m_gState.pmemForestSet->Required (dwWidth * dwHeight * sizeof(BYTE) * m_gState.plPCForest->Num()))
      return FALSE;
   m_gState.pabForestSet = (BYTE*)m_gState.pmemForestSet->p;
   memcpy (m_gState.pabForestSet, memForestSet.p, dwWidth * dwHeight * sizeof(BYTE) * m_gState.plPCForest->Num());

   m_gState.dwWidth = dwWidth;
   m_gState.dwHeight = dwHeight;
   m_gState.fScale /= fScale;

   InvalidateDisplay();
   MapScrollUpdate ();

   return TRUE;
}

/***********************************************************************************
CGroundView::ChangeSize - Changes the size to the new width and height without
chaning scale or up/down sampling.

inputs
   DWORD          dwWidth, dwHeight - new width and height
returns
   BOOL - TRUE if success
*/
BOOL CGroundView::ChangeSize (DWORD dwWidth, DWORD dwHeight)
{
   dwWidth = max(dwWidth, 2);
   dwHeight = max(dwHeight, 2);

   // enoug memory
   CMem memElev, memTextSet, memForestSet;
   if (!memElev.Required (dwWidth * dwHeight * sizeof(WORD)))
      return FALSE;
   if (!memTextSet.Required (dwWidth * dwHeight * sizeof(BYTE) * m_gState.plTextSurf->Num()))
      return FALSE;
   if (!memForestSet.Required (dwWidth * dwHeight * sizeof(BYTE) * m_gState.plPCForest->Num()))
      return FALSE;

   WORD *paw;
   BYTE *pabTextSet, *pabForestSet;
   paw = (WORD*) memElev.p;
   pabTextSet = (BYTE*) memTextSet.p;
   pabForestSet = (BYTE*) memForestSet.p;

   // convert
   DWORD x,y, dwLayer;
   fp fx, fy;
   for (y = 0; y < dwHeight; y++) {
      fy = (fp) y + ((fp)m_gState.dwHeight - (fp)dwHeight)/2;
      fy = max(fy, 0);
      fy = min(fy, (fp)m_gState.dwHeight-1);
      for (x = 0; x < dwWidth; x++, paw++, pabTextSet++, pabForestSet++) {
         fx = (fp) x + ((fp)m_gState.dwWidth - (fp)dwWidth)/2;
         fx = max(fx, 0);
         fx = min(fx, (fp)m_gState.dwWidth-1);

         HVToElev (fx, fy, paw);

         // copy the texture
         for (dwLayer = 0; dwLayer < m_gState.plTextSurf->Num(); dwLayer++)
            pabTextSet[dwLayer * dwWidth * dwHeight] = m_gState.pabTextSet[
               (int)(fx+.5) + (int)(fy+.5) * m_gState.dwWidth +
                  dwLayer * m_gState.dwWidth * m_gState.dwHeight];

         // copy the forest
         for (dwLayer = 0; dwLayer < m_gState.plPCForest->Num(); dwLayer++)
            pabForestSet[dwLayer * dwWidth * dwHeight] = m_gState.pabForestSet[
               (int)(fx+.5) + (int)(fy+.5) * m_gState.dwWidth +
                  dwLayer * m_gState.dwWidth * m_gState.dwHeight];
      } // x
   } // y

   // set undo
   UndoAboutToChange();

   // new values
   if (!m_gState.pmemElev->Required (dwWidth * dwHeight * sizeof(WORD)))
      return FALSE;
   m_gState.pawElev = (WORD*)m_gState.pmemElev->p;
   memcpy (m_gState.pawElev, memElev.p, dwWidth * dwHeight * sizeof(WORD));

   if (!m_gState.pmemTextSet->Required (dwWidth * dwHeight * sizeof(BYTE) * m_gState.plTextSurf->Num()))
      return FALSE;
   m_gState.pabTextSet = (BYTE*)m_gState.pmemTextSet->p;
   memcpy (m_gState.pabTextSet, memTextSet.p, dwWidth * dwHeight * sizeof(BYTE) * m_gState.plTextSurf->Num());

   // forest
   if (!m_gState.pmemForestSet->Required (dwWidth * dwHeight * sizeof(BYTE) * m_gState.plPCForest->Num()))
      return FALSE;
   m_gState.pabForestSet = (BYTE*)m_gState.pmemForestSet->p;
   memcpy (m_gState.pabForestSet, memForestSet.p, dwWidth * dwHeight * sizeof(BYTE) * m_gState.plPCForest->Num());

   m_gState.dwWidth = dwWidth;
   m_gState.dwHeight = dwHeight;

   InvalidateDisplay();
   MapScrollUpdate ();

   return TRUE;
}

/****************************************************************************
GroundSetPage
*/
BOOL GroundSetPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCGroundView pv = (PCGroundView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"General settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
GroundElevPage
*/
BOOL GroundElevPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCGroundView pv = (PCGroundView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"max", pv->m_gState.tpElev.v);
         MeasureToString (pPage, L"min", pv->m_gState.tpElev.h);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;   // dont care

         if (!_wcsicmp(p->psz, L"keep")) {
            TEXTUREPOINT tp;
            fp fTemp;
            MeasureParseString (pPage, L"max", &fTemp);
            tp.v = fTemp;
            MeasureParseString (pPage, L"min", &fTemp);
            tp.h = fTemp;
            tp.v = max(tp.v, tp.h + 1.0);

            // adjust the heights
            pv->UndoAboutToChange();

            DWORD i;
            WORD *paw;
            fp f;
            paw = pv->m_gState.pawElev;
            for (i = 0; i < pv->m_gState.dwWidth * pv->m_gState.dwHeight; i++, paw++) {
               f = (fp)paw[0] / (fp)0xffff * (pv->m_gState.tpElev.v - pv->m_gState.tpElev.h) + pv->m_gState.tpElev.h;
               f = (f - tp.h) / (tp.v - tp.h) * (fp)0xffff;
               f = max(0,f);
               f = min((fp)0xffff,f);
               paw[0] = (WORD) f;
            }
            pv->m_gState.tpElev = tp;

            pv->InvalidateDisplay();
            InvalidateRect (pv->m_hWndElevKey, NULL, FALSE);
            return TRUE;   // dont pass link through
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Change the elevation range";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
GroundDetailPage
*/
BOOL GroundDetailPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCGroundView pv = (PCGroundView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DoubleToControl (pPage, L"width", pv->m_gState.dwWidth);
         DoubleToControl (pPage, L"height", pv->m_gState.dwHeight);
      }
      break;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         // get the values
         DWORD dwWidth, dwHeight;
         dwWidth = (DWORD) DoubleFromControl (pPage, L"width");
         dwHeight = (DWORD) DoubleFromControl (pPage, L"height");
         dwWidth = max(dwWidth,1);
         dwHeight = max(dwHeight,1);
         if (!_wcsicmp(psz, L"width")) {
            dwHeight = (DWORD)((fp) pv->m_gState.dwHeight * (fp) dwWidth / (fp) pv->m_gState.dwWidth);
            dwHeight = max(dwHeight, 1);
            DoubleToControl (pPage, L"height", dwHeight);
         }
         else if (!_wcsicmp(psz, L"height")) {
            dwWidth = (DWORD)((fp) pv->m_gState.dwWidth * (fp) dwHeight / (fp) pv->m_gState.dwHeight);
            dwWidth = max(dwWidth, 1);
            DoubleToControl (pPage, L"width", dwWidth);
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;   // dont care

         if (!_wcsicmp(p->psz, L"keep")) {
            DWORD dwWidth, dwHeight;
            dwWidth = (DWORD) DoubleFromControl (pPage, L"width");
            dwHeight = (DWORD) DoubleFromControl (pPage, L"height");
            dwWidth = max(dwWidth,1);
            dwHeight = max(dwHeight,1);

            pv->UpDownSample ((fp)dwWidth / (fp)pv->m_gState.dwWidth);

            pv->UndoCache();
            pv->InvalidateDisplay();
            return TRUE;   // dont pass link through
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Change the detail";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
GroundSizePage
*/
BOOL GroundSizePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCGroundView pv = (PCGroundView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DoubleToControl (pPage, L"width", pv->m_gState.dwWidth);
         DoubleToControl (pPage, L"height", pv->m_gState.dwHeight);
         MeasureToString (pPage, L"widthd", (fp)pv->m_gState.dwWidth * pv->m_gState.fScale);
         MeasureToString (pPage, L"heightd", (fp)pv->m_gState.dwHeight * pv->m_gState.fScale);
      }
      break;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         // get the values
         DWORD dwWidth, dwHeight;
         fp fWidth, fHeight;
         dwWidth = (DWORD) DoubleFromControl (pPage, L"width");
         dwHeight = (DWORD) DoubleFromControl (pPage, L"height");
         dwWidth = max(dwWidth,1);
         dwHeight = max(dwHeight,1);
         MeasureParseString (pPage, L"widthd", &fWidth);
         MeasureParseString (pPage, L"heightd", &fHeight);
         fWidth = max(fWidth, pv->m_gState.fScale);
         fHeight = max(fHeight, pv->m_gState.fScale);
         DWORD dwChanged;
         dwChanged = 0;
         if (!_wcsicmp(psz, L"width")) {
            dwChanged = 0;
         }
         else if (!_wcsicmp(psz, L"height")) {
            dwChanged = 1;
         }
         else if (!_wcsicmp(psz, L"widthd")) {
            dwChanged = 2;
            dwWidth = (DWORD)max(1,fWidth / pv->m_gState.fScale);
         }
         else if (!_wcsicmp(psz, L"heightd")) {
            dwChanged = 3;
            dwHeight = (DWORD)max(1,fHeight / pv->m_gState.fScale);
         }


      if (dwChanged != 0)
         DoubleToControl (pPage, L"width", dwWidth);
      if (dwChanged != 1)
         DoubleToControl (pPage, L"height", dwHeight);
      if (dwChanged != 2)
         MeasureToString (pPage, L"widthd", (fp)dwWidth * pv->m_gState.fScale);
      if (dwChanged != 3)
         MeasureToString (pPage, L"heightd", (fp)dwHeight * pv->m_gState.fScale);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;   // dont care

         if (!_wcsicmp(p->psz, L"keep")) {
            DWORD dwWidth, dwHeight;
            dwWidth = (DWORD) DoubleFromControl (pPage, L"width");
            dwHeight = (DWORD) DoubleFromControl (pPage, L"height");
            dwWidth = max(dwWidth,1);
            dwHeight = max(dwHeight,1);

            pv->ChangeSize (dwWidth, dwHeight);

            pv->UndoCache();
            pv->InvalidateDisplay();
            return TRUE;   // dont pass link through
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Change the map's size";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
GroundScalePage
*/
BOOL GroundScalePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCGroundView pv = (PCGroundView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"widthd", ((fp)pv->m_gState.dwWidth-1) * pv->m_gState.fScale);
         MeasureToString (pPage, L"heightd", ((fp)pv->m_gState.dwHeight-1) * pv->m_gState.fScale);
         MeasureToString (pPage, L"max", pv->m_gState.tpElev.v);
         MeasureToString (pPage, L"min", pv->m_gState.tpElev.h);
      }
      break;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         // get the values
         fp fWidth, fHeight;
         MeasureParseString (pPage, L"widthd", &fWidth);
         MeasureParseString (pPage, L"heightd", &fHeight);
         fWidth = max(fWidth, CLOSE);  // BUGFIX
         fHeight = max(fHeight, CLOSE);
         //fWidth = max(fWidth, pv->m_gState.fScale);
         //fHeight = max(fHeight, pv->m_gState.fScale);
         DWORD dwChanged;
         dwChanged = 0;
         if (!_wcsicmp(psz, L"widthd")) {
            dwChanged = 2;
            fHeight = fWidth / ((fp)pv->m_gState.dwWidth-1.0) * ((fp)pv->m_gState.dwHeight-1.0);
         }
         else if (!_wcsicmp(psz, L"heightd")) {
            dwChanged = 3;
            fWidth = fHeight / ((fp)pv->m_gState.dwHeight-1.0) * ((fp)pv->m_gState.dwWidth - 1.0);
         }


      if (dwChanged != 2)
         MeasureToString (pPage, L"widthd", fWidth);
      if (dwChanged != 3)
         MeasureToString (pPage, L"heightd", fHeight);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;   // dont care

         if (!_wcsicmp(p->psz, L"keep")) {
            fp fWidth;
            MeasureParseString (pPage, L"widthd", &fWidth);
            fWidth /= ((fp)pv->m_gState.dwWidth - 1.0);  // BUGFIX - Added 1.0
            fWidth = max(fWidth, .001);

            TEXTUREPOINT tp;
            fp fTemp;
            MeasureParseString (pPage, L"max", &fTemp);
            tp.v = fTemp;
            MeasureParseString (pPage, L"min", &fTemp);
            tp.h = fTemp;
            tp.v = max(tp.v, tp.h + 1.0);

            pv->UndoAboutToChange();
            pv->m_gState.fScale = fWidth;
            pv->m_gState.tpElev = tp;

            pv->UndoCache();
            pv->InvalidateDisplay();
            InvalidateRect (pv->m_hWndElevKey, NULL, FALSE);
            pv->MapScrollUpdate();
            return TRUE;   // dont pass link through
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Scale the map";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
AppSettings - This brings up the UI for view settings. As parameters are changed
the view object will be notified and told to refresh.

inputs
   PCGroundView    pView - view to use
returns
   none
*/
void AppSettings (PCGroundView pView)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (pView->m_hWnd, &r);

   cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

firstpage:
   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGROUNDSET, GroundSetPage, pView);
   if (!pszRet)
      return;

   if (!_wcsicmp(pszRet, L"elev")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGROUNDELEV, GroundElevPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else if (!_wcsicmp(pszRet, L"detail")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGROUNDDETAIL, GroundDetailPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else if (!_wcsicmp(pszRet, L"size")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGROUNDSIZE, GroundSizePage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else if (!_wcsicmp(pszRet, L"scale")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGROUNDSCALE, GroundScalePage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }

   return;  // exit
}


/****************************************************************************
GroundTextPage
*/
BOOL GroundTextPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCGroundView pv = (PCGroundView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         if (pv->m_gState.plTextSurf->Num() < 2) {
            pControl = pPage->ControlFind (L"delete");
            if (pControl)
               pControl->Enable (FALSE);
         }


         FillStatusColor (pPage, L"mapcolor", 
            *((COLORREF*)pv->m_gState.plTextColor->Get(pv->m_dwCurText)));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(p->pControl->m_pszName, L"changemap")) {
            COLORREF cr;
            COLORREF *pc;
            pc = (COLORREF*)pv->m_gState.plTextColor->Get(pv->m_dwCurText);
            cr = AskColor (pPage->m_pWindow->m_hWnd, *pc, pPage, L"mapcolor");
            if (cr != *pc) {
               pv->UndoAboutToChange();
               *pc = cr;
               pv->InvalidateDisplay();
               pv->UpdateTextList();
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"delete")) {
            if (!FindViewForWorld (pv->m_pWorld))
               return TRUE;   // world no longer exists

            pv->UndoAboutToChange();

            DWORD dwDelete;
            dwDelete = pv->m_dwCurText;
            pv->m_dwCurText = 0;

            // move memory down for subsequent textures
            DWORD dwScale;
            dwScale = pv->m_gState.dwWidth * pv->m_gState.dwHeight;
            memmove (pv->m_gState.pabTextSet + dwDelete * dwScale,
               pv->m_gState.pabTextSet + (dwDelete+1) * dwScale,
               (pv->m_gState.plTextSurf->Num() - dwDelete - 1) * dwScale);

            PCObjectSurface *ppos = (PCObjectSurface*) pv->m_gState.plTextSurf->Get(0);
            delete ppos[dwDelete];
            pv->m_gState.plTextSurf->Remove (dwDelete);
            pv->m_gState.plTextColor->Remove (dwDelete);

            pv->InvalidateDisplay();
            pv->UpdateTextList();

            pPage->Exit (Back());
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"change")) {
            PCObjectSurface *ppos = (PCObjectSurface*) pv->m_gState.plTextSurf->Get(0);
            PCObjectSurface pNew;
            if (!FindViewForWorld (pv->m_pWorld))
               return TRUE;   // world no longer exists
            pNew = ppos[pv->m_dwCurText]->Clone();
            if (!pNew)
               return TRUE;

            if (!TextureSelDialog (DEFAULTRENDERSHARD, pPage->m_pWindow->m_hWnd, pNew, pv->m_pWorld)) {
               delete pNew;
               return TRUE;
            }

            pv->UndoAboutToChange();
            delete ppos[pv->m_dwCurText];
            ppos[pv->m_dwCurText] = pNew;
            pv->InvalidateDisplay();
            pv->UpdateTextList();
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify the texture";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
GroundForestPage
*/
BOOL GroundForestPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCGroundView pv = (PCGroundView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         PCForest *ppf = (PCForest*) pv->m_gState.plPCForest->Get(0);;
         FillStatusColor (pPage, L"mapcolor", ppf[pv->m_dwCurForest]->m_cColor);

         pControl = pPage->ControlFind (L"Name");
         if (pControl)
            pControl->AttribSet (Text(), ppf[pv->m_dwCurForest]->m_szName);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // since all out edit controls

         // world going to change
         pv->UndoAboutToChange();
         PCForest *ppf, pf;
         ppf = (PCForest*) pv->m_gState.plPCForest->Get(0);
         pf = ppf[pv->m_dwCurForest];
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"Name");
         DWORD dwNeed;
         if (pControl)
            pControl->AttribGet (Text(), pf->m_szName, sizeof(pf->m_szName), &dwNeed);
         pv->UpdateForestList();
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(p->pControl->m_pszName, L"changemap")) {
            COLORREF cr;
            PCForest *ppf, pf;
            ppf = (PCForest*) pv->m_gState.plPCForest->Get(0);
            pf = ppf[pv->m_dwCurForest];
            cr = AskColor (pPage->m_pWindow->m_hWnd, pf->m_cColor, pPage, L"mapcolor");
            if (cr != pf->m_cColor) {
               pv->UndoAboutToChange();
               pf->m_cColor = cr;
               pv->InvalidateDisplay();
               pv->UpdateForestList();
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"delete")) {
            if (!FindViewForWorld (pv->m_pWorld))
               return TRUE;   // world no longer exists

            pv->UndoAboutToChange();

            DWORD dwDelete;
            dwDelete = pv->m_dwCurForest;

            // move memory down for subsequent textures
            DWORD dwScale;
            dwScale = pv->m_gState.dwWidth * pv->m_gState.dwHeight;
            memmove (pv->m_gState.pabForestSet + dwDelete * dwScale,
               pv->m_gState.pabForestSet + (dwDelete+1) * dwScale,
               (pv->m_gState.plPCForest->Num() - dwDelete - 1) * dwScale);

            pv->m_dwCurForest = 0;
            PCForest *ppf = (PCForest*) pv->m_gState.plPCForest->Get(0);
            delete ppf[dwDelete];
            pv->m_gState.plPCForest->Remove (dwDelete);

            pv->InvalidateDisplay();
            pv->UpdateForestList();

            pPage->Exit (Back());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify the forest";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}
/***********************************************************************************
WndProc - Window procedure for the house view object.
*/
LRESULT CGroundView::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (uMsg == gdwMouseWheel)
      uMsg = WM_MOUSEWHEEL;

   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWnd = hWnd;

         SetFocus (hWnd);

         // Create the menus
         // make the button bars in temporary location
         RECT r;
         r.top = r.bottom = r.left = r.right = 0;
         m_pbarView = new CButtonBar;
         m_pbarView->Init (&r, hWnd, 2);
         m_pbarView->ColorSet (m_cBackDark, m_cBackMed, m_cBackLight, m_cBackOutline);

         m_pbarGeneral = new CButtonBar;
         m_pbarGeneral->Init (&r, hWnd, 3);
         m_pbarGeneral->ColorSet (m_cBackDark, m_cBackMed, m_cBackLight, m_cBackOutline);

         m_pbarObject = new CButtonBar;
         m_pbarObject->Init (&r, hWnd, 0);
         m_pbarObject->ColorSet (m_cBackDark, m_cBackMed, m_cBackLight, m_cBackOutline);

         m_pbarMisc = new CButtonBar;
         m_pbarMisc->Init (&r, hWnd, 1);
         m_pbarMisc->ColorSet (m_cBackDark, m_cBackMed, m_cBackLight, m_cBackOutline);

         // create the map and key
         m_hWndMap = CreateWindowEx (WS_EX_CLIENTEDGE, "CGroundViewMap", "",
            WS_CHILD | WS_VSCROLL | WS_HSCROLL, 0, 0, 1, 1, hWnd, NULL, ghInstance, this);
         m_hWndElevKey = CreateWindowEx (WS_EX_CLIENTEDGE, "CGroundViewKey", "",
            WS_CHILD, 0, 0, 1, 1, hWnd, NULL, ghInstance, this);
         m_hWndTextList = CreateWindowEx (WS_EX_CLIENTEDGE, "LISTBOX", "",
            WS_CHILD | WS_VSCROLL |
            LBS_DISABLENOSCROLL | LBS_NOTIFY |
            LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED,
            0, 0, 1, 1, hWnd,
            (HMENU) IDC_TEXTLIST, ghInstance, NULL);
         m_hWndForestList = CreateWindowEx (WS_EX_CLIENTEDGE, "LISTBOX", "",
            WS_CHILD | WS_VSCROLL |
            LBS_DISABLENOSCROLL | LBS_NOTIFY |
            LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED,
            0, 0, 1, 1, hWnd,
            (HMENU) IDC_FORESTLIST, ghInstance, NULL);

         // update items for texture and forest list
         UpdateTextList ();
         UpdateForestList ();
      }

      return 0;

   case WM_MEASUREITEM:
      if (wParam == IDC_TEXTLIST) {
         PMEASUREITEMSTRUCT pmi = (PMEASUREITEMSTRUCT) lParam;
         pmi->itemHeight = THUMBSHRINK + TBORDER;
         return TRUE;
      }
      else if (wParam == IDC_FORESTLIST) {
         PMEASUREITEMSTRUCT pmi = (PMEASUREITEMSTRUCT) lParam;
         pmi->itemHeight = THUMBSHRINK + TBORDER;
         return TRUE;
      }
      break;

   case WM_DRAWITEM:
      if (wParam == IDC_TEXTLIST) {
         PDRAWITEMSTRUCT pdi = (PDRAWITEMSTRUCT) lParam;

         // figure out the color
         PCObjectSurface *ppos;
         PCObjectSurface pos;
         COLORREF *pcol;
         DWORD dwNum;
         ppos = (PCObjectSurface*)m_gState.plTextSurf->Get(0);
         pcol = (COLORREF*)m_gState.plTextColor->Get(0);
         dwNum = m_gState.plTextSurf->Num();
         pos = ((DWORD)pdi->itemID < dwNum) ? ppos[pdi->itemID] : NULL;

         // because list boxes don't tell us if the selection has changed, use
         // this as a technique to know
         //DWORD dwSel;
         //dwSel = (DWORD) SendMessage (pdi->hwndItem, LB_GETCURSEL, 0, 0);
         //if (dwSel < dwNum)
         // m_dwCurText = dwSel
         if ((pdi->itemState & ODS_SELECTED) && ((DWORD)pdi->itemID < dwNum))
            m_dwCurText = (DWORD)pdi->itemID;

         BOOL fSel;
         // NOTE: can't do the first fSel because the lsit box doesnt draw correctly
         // when do that

         //fSel = ((DWORD) pdi->itemID == m_dwCurText) && ((DWORD)pdi->itemID < dwNum);
         fSel = pdi->itemState & ODS_SELECTED;

         // bacground
         HBRUSH hbr;
         HDC hDC;
         hDC = pdi->hDC;
         hbr = CreateSolidBrush (fSel ? RGB(0xc0,0xc0,0xc0) : RGB(0xff,0xff,0xff));
         FillRect (hDC, &pdi->rcItem, hbr);
         DeleteObject (hbr);

         // Draw the color as it appears in the edit window
         RECT r;
         if (pos) {
            COLORREF cr;
            cr = *(pcol + pdi->itemID);
            hbr = CreateSolidBrush (cr);
            r = pdi->rcItem;
            r.left += TBORDER/4;
            r.top += TBORDER/4;
            r.bottom -= TBORDER/4;
            r.right = r.left + THUMBSHRINK + TBORDER/2;
            FillRect (hDC, &r, hbr);
            DeleteObject (hbr);
         }

         // Draw the texture
         HBITMAP hBit;
         hBit = NULL;
         COLORREF cTransparent;
         if (pos && pos->m_fUseTextureMap)
            hBit = TextureGetThumbnail (DEFAULTRENDERSHARD, &pos->m_gTextureCode, &pos->m_gTextureSub, m_hWnd, &cTransparent);
         if (hBit) {
            HDC hDCMem = CreateCompatibleDC (pdi->hDC);
            if (hDCMem)
               SelectObject (hDCMem, hBit);
            int iOldStretch;
            iOldStretch = SetStretchBltMode (pdi->hDC, COLORONCOLOR);
            StretchBlt (pdi->hDC, pdi->rcItem.left + TBORDER/2, pdi->rcItem.top + TBORDER/2, THUMBSHRINK, THUMBSHRINK,
               hDCMem, 0, 0, TEXTURETHUMBNAIL, TEXTURETHUMBNAIL, SRCCOPY);
            SetStretchBltMode (pdi->hDC, iOldStretch);
            if (hDCMem)
               DeleteDC (hDCMem);

            DeleteObject (hBit);
         }
         else if (pos) {
            // draw colid color
            hbr = CreateSolidBrush (pos->m_cColor);
            r = pdi->rcItem;
            r.left += TBORDER/2;
            r.top += TBORDER/2;
            r.bottom -= TBORDER/2;
            r.right = r.left + THUMBSHRINK;
            FillRect (hDC, &r, hbr);
            DeleteObject (hbr);
         }

         // text
         char szText[256];
         szText[0] = 0;
         if (pdi->itemID <= dwNum)
            SendMessage (pdi->hwndItem, LB_GETTEXT, (WPARAM) pdi->itemID, (LPARAM)&szText[0]);
         if (szText[0]) {
            HFONT hFontOld;
            COLORREF crOld;
            int iOldMode;

            // figure out where to draw
            RECT r;
            r = pdi->rcItem;
            if (pos)
               r.left += THUMBSHRINK + TBORDER;
            else
               r.left += TBORDER;
            r.top += TBORDER/2;
            r.bottom -= TBORDER/2;
            r.right -= TBORDER/2;

            hFontOld = (HFONT) SelectObject (hDC, m_hFont);
            crOld = SetTextColor (hDC, RGB(0,0,0));
            iOldMode = SetBkMode (hDC, TRANSPARENT);

            DrawText (hDC, szText,
               -1, &r, DT_LEFT | DT_END_ELLIPSIS | DT_TOP | DT_WORDBREAK);
            SelectObject (hDC, hFontOld);
            SetTextColor (hDC, crOld);
            SetBkMode (hDC, iOldMode);
         }

         return TRUE;
      }
      else if (wParam == IDC_FORESTLIST) {
         PDRAWITEMSTRUCT pdi = (PDRAWITEMSTRUCT) lParam;

         // figure out the color
         PCForest *ppf, pf;
         DWORD dwNum;
         ppf = (PCForest*) m_gState.plPCForest->Get(0);
         dwNum = m_gState.plPCForest->Num();
         pf = ((DWORD)pdi->itemID < dwNum) ? ppf[pdi->itemID] : NULL;

         // because list boxes don't tell us if the selection has changed, use
         // this as a technique to know
         //DWORD dwSel;
         //dwSel = (DWORD) SendMessage (pdi->hwndItem, LB_GETCURSEL, 0, 0);
         //if (dwSel < dwNum)
         // m_dwCurForest = dwSel
         if ((pdi->itemState & ODS_SELECTED) && ((DWORD)pdi->itemID < dwNum))
            m_dwCurForest = (DWORD)pdi->itemID;

         BOOL fSel;
         // NOTE: can't do the first fSel because the lsit box doesnt draw correctly
         // when do that

         //fSel = ((DWORD) pdi->itemID == m_dwCurText) && ((DWORD)pdi->itemID < dwNum);
         fSel = pdi->itemState & ODS_SELECTED;

         // bacground
         HBRUSH hbr;
         HDC hDC;
         hDC = pdi->hDC;
         hbr = CreateSolidBrush (fSel ? RGB(0xc0,0xc0,0xc0) : RGB(0xff,0xff,0xff));
         FillRect (hDC, &pdi->rcItem, hbr);
         DeleteObject (hbr);

         // Draw the color as it appears in the edit window
         RECT r;
         if (pf) {
            COLORREF cr;
            cr = pf->m_cColor;
            hbr = CreateSolidBrush (cr);
            r = pdi->rcItem;
            r.left += TBORDER/4;
            r.top += TBORDER/4;
            r.bottom -= TBORDER/4;
            r.right = r.left + THUMBSHRINK + TBORDER/2;
            FillRect (hDC, &r, hbr);
            DeleteObject (hbr);
         }

         // text
         char szText[256];
         szText[0] = 0;
         if (pdi->itemID <= dwNum)
            SendMessage (pdi->hwndItem, LB_GETTEXT, (WPARAM) pdi->itemID, (LPARAM)&szText[0]);
         if (szText[0]) {
            HFONT hFontOld;
            COLORREF crOld;
            int iOldMode;

            // figure out where to draw
            RECT r;
            r = pdi->rcItem;
            if (pf)
               r.left += THUMBSHRINK + TBORDER;
            else
               r.left += TBORDER;
            r.top += TBORDER/2;
            r.bottom -= TBORDER/2;
            r.right -= TBORDER/2;

            hFontOld = (HFONT) SelectObject (hDC, m_hFont);
            crOld = SetTextColor (hDC, RGB(0,0,0));
            iOldMode = SetBkMode (hDC, TRANSPARENT);

            DrawText (hDC, szText,
               -1, &r, DT_LEFT | DT_END_ELLIPSIS | DT_TOP | DT_WORDBREAK);
            SelectObject (hDC, hFontOld);
            SetTextColor (hDC, crOld);
            SetBkMode (hDC, iOldMode);
         }

         return TRUE;
      }
      break;

   case WM_TIMER:
      if ((wParam == TIMER_AIRBRUSH) && m_dwButtonDown) {
         BrushApply (m_adwPointerMode[m_dwButtonDown-1],  &m_pntMouseLast, NULL);
      }
      break;

   case WM_DESTROY:
      // do nothing for now
      break;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {

      case IDC_TEXTLIST:
         if (HIWORD(wParam) == LBN_DBLCLK) {
            DWORD dwSel = (DWORD) SendMessage (m_hWndTextList, LB_GETCURSEL, 0,0);
            UndoCache();
            if (dwSel < m_gState.plTextSurf->Num()) {
               // double-clicked to edit a texture
               CEscWindow cWindow;
               RECT r;
               DialogBoxLocation (m_hWnd, &r);

               cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
               PWSTR pszRet;

               // start with the first page
               m_dwCurText = dwSel; // just in case
               pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGROUNDTEXT, GroundTextPage, this);
               return 0;
            }
            else {
               // if world is no lnoger valid then exit
               if (!FindViewForWorld (m_pWorld))
                  return 0;

               UndoAboutToChange();

               // double clicked on add...
               PCObjectSurface pos;
               pos = NULL;
               if (m_gState.plTextSurf->Num()) {
                  pos = *((PCObjectSurface*)m_gState.plTextSurf->Get(0));
                  pos = pos->Clone();
               }
               if (!pos)
                  pos = new CObjectSurface;
               if (!pos)
                  return 0;


               TextureSelDialog (DEFAULTRENDERSHARD, m_hWnd, pos, m_pWorld);

               // get the color
               COLORREF cr;
               cr = pos->m_cColor;
               if (pos->m_fUseTextureMap) {
                  RENDERSURFACE rs;
                  memset (&rs, 0, sizeof(rs));
                  rs.fUseTextureMap = TRUE;
                  rs.gTextureCode = pos->m_gTextureCode;
                  rs.gTextureSub = pos->m_gTextureSub;
                  rs.TextureMods = pos->m_TextureMods;
                  memcpy (&rs.afTextureMatrix, &pos->m_afTextureMatrix, sizeof(pos->m_afTextureMatrix));
                  memcpy (&rs.abTextureMatrix, &pos->m_mTextureMatrix, sizeof(pos->m_mTextureMatrix));

                  PCTextureMapSocket pm;
                  pm = TextureCacheGet (DEFAULTRENDERSHARD, &rs, NULL, NULL);
                  if (pm) {
                     cr = pm->AverageColorGet (0, FALSE);
                     TextureCacheRelease (DEFAULTRENDERSHARD, pm);
                  }
               }

               // add it
               m_dwCurText = m_gState.plTextSurf->Num();
               m_gState.plTextSurf->Add (&pos);
               m_gState.plTextColor->Add (&cr);

               // update the memory
               DWORD dwScale;
               dwScale = m_gState.dwWidth * m_gState.dwHeight;
               m_gState.pmemTextSet->Required (dwScale * m_gState.plTextSurf->Num());
               m_gState.pabTextSet = (PBYTE) m_gState.pmemTextSet->p;
               memset (m_gState.pabTextSet + dwScale * m_dwCurText, 0, dwScale);

               UpdateTextList();
            } // double-click
         }
         break;

      case IDC_FORESTLIST:
         if (HIWORD(wParam) == LBN_DBLCLK) {
            DWORD dwSel = (DWORD) SendMessage (m_hWndForestList, LB_GETCURSEL, 0,0);
            UndoCache();
            if (dwSel < m_gState.plPCForest->Num()) {
edit:
               // double-clicked to edit a texture
               CEscWindow cWindow;
               RECT r;
               DialogBoxLocation (m_hWnd, &r);

               cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
               PWSTR pszRet;

               PWSTR pszCanopy = L"canopy";
               DWORD dwCanopyLen = (DWORD)wcslen(pszCanopy);

               // start with the first page
               m_dwCurForest = dwSel; // just in case

redo:
               pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGROUNDFOREST, GroundForestPage, this);

               if (pszRet && !wcsncmp(pszRet, pszCanopy, dwCanopyLen)) {
                  DWORD dwNum = _wtoi(pszRet + dwCanopyLen);
                  dwNum = min(dwNum, NUMFORESTCANOPIES-1);

                  UndoAboutToChange();
                  PCForest *ppf, pf;
                  ppf = (PCForest*) m_gState.plPCForest->Get(0);
                  pf = ppf[m_dwCurForest];
                  if (pf->m_apCanopy[dwNum]->Dialog (DEFAULTRENDERSHARD, &cWindow))
                     goto redo;
                  return 0;
               }
               return 0;
            }
            else {
               // if world is no lnoger valid then exit
               if (!FindViewForWorld (m_pWorld))
                  return 0;

               UndoAboutToChange();

               // double clicked on add...
               PCForest pf;
               pf = new CForest;
               if (!pf)
                  return 0;
               wcscpy (pf->m_szName, L"New forest");


               // add it
               m_dwCurForest = m_gState.plPCForest->Num();
               m_gState.plPCForest->Add (&pf);
               UpdateForestList();

               // update the memory
               DWORD dwScale;
               dwScale = m_gState.dwWidth * m_gState.dwHeight;
               m_gState.pmemForestSet->Required (dwScale * m_gState.plPCForest->Num());
               m_gState.pabForestSet = (PBYTE) m_gState.pmemForestSet->p;
               memset (m_gState.pabForestSet + dwScale * m_dwCurForest, 0, dwScale);

               // Go dirctly to edit?
               goto edit;
            } // double-click
         }
         break;

      case IDC_HELPBUTTON:
         ASPHelp(IDR_HTUTORIALGROUND1);
         return 0;

      case IDC_VIEWTOPO:
         NewView (0);
         return 0;

      case IDC_VIEWTEXT:
         NewView (1);
         return 0;

      case IDC_VIEWFOREST:
         NewView (2);
         return 0;

      case IDC_GROUNDPAINT:
         {
            UndoCache();

            // get to draw with selection
            m_fPaintOn = TRUE;
            InvalidateDisplay ();

            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (m_hWnd, &r);

            cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;

            // start with the first page
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGROUNDPAINT, GroundPaintPage, this);

            // no selection
            m_fPaintOn = FALSE;
            InvalidateDisplay ();

            UndoCache();
         }
         return 0;

      case IDC_GROUNDAPPLYRAIN:
         {
            UndoCache();
            UndoAboutToChange ();

            srand (GetTickCount());
            DWORD i;
            for (i = 0; i < 1000; i++)
               ApplyRain ();

            // redraw
            InvalidateDisplay();
            UndoCache();
         }
         return 0;

      case IDC_GROUNDAPPLYWIND:
         {
            UndoCache();
            UndoAboutToChange ();

            // allocate enough memory to store the original image
            DWORD dwSize = m_gState.dwWidth * m_gState.dwHeight * sizeof(WORD);
            if (!m_memGroundGenTemp.Required (dwSize)) {
               ReleaseCapture ();
               m_fCaptured = FALSE;
               m_dwButtonDown = 0;
               return 0;
            }

            // backup
            memcpy (m_memGroundGenTemp.p, m_gState.pawElev, dwSize);

            // smooth
            int x, y, xx, yy;
            DWORD dwSum, dwCount;
            WORD *pawCur;
            for (y = 0; y < (int)m_gState.dwHeight; y++) for (x = 0; x < (int)m_gState.dwWidth; x++) {
               dwSum = dwCount = 0;
               for (yy = max(y - 2, 0); yy <= y + 2; yy++) {
                  if (yy >= (int)m_gState.dwHeight)
                     continue;

                  xx = max(x - 2, 0);
                  pawCur = m_gState.pawElev + (xx + yy * (int)m_gState.dwWidth);
                  for (; xx <= x + 2; xx++, pawCur++) {
                     if (xx >= (int)m_gState.dwWidth)
                        continue;
                     dwSum += pawCur[0];
                     dwCount++;
                  }
               }//yy

               if (dwCount)
                  dwSum /= dwCount;
               pawCur = m_gState.pawElev + (x + y * (int)m_gState.dwWidth);
               pawCur[0] = pawCur[0] / 2 + (WORD)dwSum / 2;
            } // x and y

            // redraw
            InvalidateDisplay();
            UndoCache();
         }
         return 0;

      case IDC_GROUNDWATER:
         {
            UndoCache();

            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (m_hWnd, &r);

            cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;

            // start with the first page
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGROUNDWATER, GroundWaterPage, this);

            UndoCache();
         }
         return 0;

      case IDC_SAVE:
         Save (hWnd);
         return 0;

      case IDC_GROUNDSET:
         UndoCache();
         AppSettings (this);

         // since something may have been changed, take a snapshot for undo
         UndoCache();

         return 0;

      case IDC_CLOSE:
         SendMessage (hWnd, WM_CLOSE, 0, 0);
         return 0;

      case IDC_MINIMIZE:
         ShowWindow (hWnd, SW_MINIMIZE);
         return 0;

      case IDC_SMALLWINDOW:
         {
            m_fSmallWindow = !m_fSmallWindow;

            // get the monitor info
            HMONITOR hMon = MonitorFromWindow (hWnd, MONITOR_DEFAULTTONEAREST);

            // find it
            FillXMONITORINFO ();
            PCListFixed pListXMONITORINFO;
            pListXMONITORINFO = ReturnXMONITORINFO();
            DWORD i;
            PXMONITORINFO p;
            for (i = 0; i < pListXMONITORINFO->Num(); i++) {
               p = (PXMONITORINFO) pListXMONITORINFO->Get(i);
               if (p->hMonitor == hMon)
                  break;
            }

            // move it to the next one
            p = (PXMONITORINFO) pListXMONITORINFO->Get(i % pListXMONITORINFO->Num());
            LONG lStyle;
            //lEx = GetWindowLong (hWnd, GWL_EXSTYLE);
            lStyle = GetWindowLong (hWnd, GWL_STYLE);

            if (m_fSmallWindow) {
               RECT r;
               r.left = (p->rWork.left * 3 + p->rWork.right) / 4;
               r.right = (p->rWork.left + p->rWork.right*3) / 4;
               r.top = (p->rWork.top * 3 + p->rWork.bottom) / 4;
               r.bottom = (p->rWork.top + p->rWork.bottom*3) / 4;

               //lEx |= WS_EX_PALETTEWINDOW;
               lStyle |= WS_SIZEBOX | WS_CAPTION | WS_SYSMENU;
               //SetWindowLong (hWnd, GWL_EXSTYLE, lEx);
               SetWindowLong (hWnd, GWL_STYLE, lStyle);

               SetWindowPos (hWnd, (HWND)HWND_TOPMOST, r.left, r.top,
                  r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED);
            }
            else {
               //lEx &= ~(WS_EX_PALETTEWINDOW);
               lStyle &= ~(WS_SIZEBOX | WS_CAPTION | WS_SYSMENU);
               //SetWindowLong (hWnd, GWL_EXSTYLE, lEx);
               SetWindowLong (hWnd, GWL_STYLE, lStyle);

               SetWindowPos (hWnd, (HWND)HWND_NOTOPMOST, p->rWork.left, p->rWork.top,
                  p->rWork.right - p->rWork.left, p->rWork.bottom - p->rWork.top, SWP_FRAMECHANGED);
            }

         }
         return 0;

      case IDC_GROUNDGENMTN:
      case IDC_GROUNDGENPLAT:
      case IDC_GROUNDGENRAVINE:
      case IDC_GROUNDGENVOLCANO:
      case IDC_GROUNDGENRAISE:
      case IDC_GROUNDGENFLAT:
      case IDC_GROUNDGENLOWER:
      case IDC_GROUNDGENHILL:
      case IDC_GROUNDGENCRATER:
      case IDC_GROUNDGENHILLROLL:
      case IDC_GROUNDSUN:
      case IDC_ZOOMIN:
      case IDC_ZOOMOUT:
      case IDC_BRUSH4:
      case IDC_BRUSH8:
      case IDC_BRUSH16:
      case IDC_BRUSH32:
      case IDC_BRUSH64:
         {
            // all of these change the current pointer mode and the meaning about what it does
            DWORD dwButton;
            dwButton = 0;  // assume left button
            if (HIWORD(wParam) == 5)
               dwButton = 1;  // middle
            else if (HIWORD(wParam) == 6)
               dwButton = 2;  // right
            SetPointerMode (LOWORD(wParam), dwButton);
         }
         return 0;

      case IDC_UNDOBUTTON:
         Undo(TRUE);
         return 0;

      case IDC_REDOBUTTON:
         Undo (FALSE);
         return 0;

      case IDC_TRACING:
         TraceDialog (NULL, this);
         return 0;



      case IDC_BRUSHSHAPE:
         {
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUBRUSHSHAPE));
            HMENU hSub = GetSubMenu(hMenu,0);

            // cache undo
            UndoCache ();

            DWORD dwCheck;
            dwCheck = 0;
            switch (m_dwBrushPoint) {
            case 0: // flat
               dwCheck = ID_BRUSHSHAPE_FLAT;
               break;
            case 1: // rounded
               dwCheck = ID_BRUSHSHAPE_ROUNDED;
               break;
            case 2: // pointy
               dwCheck = ID_BRUSHSHAPE_POINTY;
               break;
            case 3: // very pointy
               dwCheck = ID_BRUSHSHAPE_VERYPOINTY;
               break;
            }
            if (dwCheck)
               CheckMenuItem (hSub, dwCheck, MF_BYCOMMAND | MF_CHECKED);
            switch (m_dwBrushAir) {
            case 0: // pen
               dwCheck = ID_BRUSHSHAPE_PEN;
               break;
            case 1: // blrush
               dwCheck = ID_BRUSHSHAPE_BRUSH;
               break;
            case 2: // airbrush
               dwCheck = ID_BRUSHSHAPE_AIRBRUSH;
               break;
            }
            if (dwCheck)
               CheckMenuItem (hSub, dwCheck, MF_BYCOMMAND | MF_CHECKED);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            switch (iRet) {
            case ID_BRUSHSHAPE_AIRBRUSH:
               m_dwBrushAir = 2;
               break;
            case ID_BRUSHSHAPE_BRUSH:
               m_dwBrushAir = 1;
               break;
            case ID_BRUSHSHAPE_PEN:
               m_dwBrushAir = 0;
               break;
            case ID_BRUSHSHAPE_FLAT:
               m_dwBrushPoint = 0;
               break;
            case ID_BRUSHSHAPE_ROUNDED:
               m_dwBrushPoint = 1;
               break;
            case ID_BRUSHSHAPE_POINTY:
               m_dwBrushPoint = 2;
               break;
            case ID_BRUSHSHAPE_VERYPOINTY:
               m_dwBrushPoint = 3;
               break;
            }
         }
         return 0;

      case IDC_BRUSHEFFECT:
         {
            HMENU hMenu = LoadMenu (ghInstance,
               MAKEINTRESOURCE((m_dwView == 0) ? IDR_MENUBRUSHEFFECT : IDR_MENUBRUSHEFFECTTEXT));
            HMENU hSub = GetSubMenu(hMenu,0);

            // cache undo
            UndoCache ();

            DWORD dwCheck;
            dwCheck = 0;
            switch (m_dwBrushEffect) {
            case 0: // raise
               dwCheck = ID_BRUSHEFFECT_RAISE;
               break;
            case 1: // lower
               dwCheck = ID_BRUSHEFFECT_LOWER;
               break;
            case 2: // raise to max
               dwCheck = ID_BRUSHEFFECT_RAISETOMAXIMUM;
               break;
            case 3: // lower to max
               dwCheck = ID_BRUSHEFFECT_LOWERTOMINIMUM;
               break;
            case 4: // blur
               dwCheck = ID_BRUSHEFFECT_BLUR;
               break;
            case 5: // sharpen
               dwCheck = ID_BRUSHEFFECT_SHARPEN;
               break;
            case 6: // nouse
               dwCheck = ID_BRUSHEFFECT_ADDB;
               break;
            }
            if (dwCheck)
               CheckMenuItem (hSub, dwCheck, MF_BYCOMMAND | MF_CHECKED);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            switch (iRet) {
            case ID_BRUSHEFFECT_RAISE:
               m_dwBrushEffect = 0;
               break;
            case ID_BRUSHEFFECT_LOWER:
               m_dwBrushEffect = 1;
               break;
            case ID_BRUSHEFFECT_RAISETOMAXIMUM:
               m_dwBrushEffect = 2;
               break;
            case ID_BRUSHEFFECT_LOWERTOMINIMUM:
               m_dwBrushEffect = 3;
               break;
            case ID_BRUSHEFFECT_BLUR:
               m_dwBrushEffect = 4;
               break;
            case ID_BRUSHEFFECT_SHARPEN:
               m_dwBrushEffect = 5;
               break;
            case ID_BRUSHEFFECT_ADDB:
               m_dwBrushEffect = 6;
               break;
            }
         }
         return 0;


      case IDC_BRUSHSTRENGTH:
         {
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUBRUSHSTRENGTH));
            HMENU hSub = GetSubMenu(hMenu,0);

            // cache undo
            UndoCache ();

            DWORD dwCheck;
            dwCheck = 0;
            switch (m_dwBrushStrength) {
            case 1:
               dwCheck = ID_BRUSHSTRENGTH_1;
               break;
            case 2:
               dwCheck = ID_BRUSHSTRENGTH_2;
               break;
            case 3:
               dwCheck = ID_BRUSHSTRENGTH_3;
               break;
            case 4:
               dwCheck = ID_BRUSHSTRENGTH_4;
               break;
            case 5:
               dwCheck = ID_BRUSHSTRENGTH_5;
               break;
            case 6:
               dwCheck = ID_BRUSHSTRENGTH_6;
               break;
            case 7:
               dwCheck = ID_BRUSHSTRENGTH_7;
               break;
            case 8:
               dwCheck = ID_BRUSHSTRENGTH_8;
               break;
            case 9:
               dwCheck = ID_BRUSHSTRENGTH_9;
               break;
            case 10:
               dwCheck = ID_BRUSHSTRENGTH_10;
               break;
            }
            if (dwCheck)
               CheckMenuItem (hSub, dwCheck, MF_BYCOMMAND | MF_CHECKED);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            switch (iRet) {
            case ID_BRUSHSTRENGTH_1:
               m_dwBrushStrength = 1;
               break;
            case ID_BRUSHSTRENGTH_2:
               m_dwBrushStrength = 2;
               break;
            case ID_BRUSHSTRENGTH_3:
               m_dwBrushStrength = 3;
               break;
            case ID_BRUSHSTRENGTH_4:
               m_dwBrushStrength = 4;
               break;
            case ID_BRUSHSTRENGTH_5:
               m_dwBrushStrength = 5;
               break;
            case ID_BRUSHSTRENGTH_6:
               m_dwBrushStrength = 6;
               break;
            case ID_BRUSHSTRENGTH_7:
               m_dwBrushStrength = 7;
               break;
            case ID_BRUSHSTRENGTH_8:
               m_dwBrushStrength = 8;
               break;
            case ID_BRUSHSTRENGTH_9:
               m_dwBrushStrength = 9;
               break;
            case ID_BRUSHSTRENGTH_10:
               m_dwBrushStrength = 10;
               break;
            }
         }
         return 0;

      case IDC_GRIDTOPO:
         {
            fp *pafUnits;
            DWORD dwNumUnits;
            if (MeasureDefaultUnits() & MUNIT_ENGLISH) {
               pafUnits = gafGridEnglish;
               dwNumUnits = sizeof(gafGridEnglish)/sizeof(fp)/2;
            }
            else {
               pafUnits = gafGridMetric;
               dwNumUnits = sizeof(gafGridMetric)/sizeof(fp)/2;
            }

            HMENU hMenu = CreatePopupMenu ();
            DWORD i;
            char szTemp[32];
            BOOL fChecked, fFoundChecked;
            fFoundChecked = FALSE;
            fp fGrid;
            for (i = 0; i <= dwNumUnits; i++) {
               fChecked = FALSE;
               fGrid = (i < dwNumUnits) ? pafUnits[i*2] : 0;
               if (fabs(m_fGridUDMinor - fGrid) < EPSILON)
                  fChecked = TRUE;
               if (fGrid)
                  MeasureToString (fGrid, szTemp);
               else
                  strcpy (szTemp, "No grid");
               AppendMenu (hMenu, MF_ENABLED | MF_STRING | (fChecked ? MF_CHECKED : 0),
                  IDC_GRIDTOPO + i, szTemp);
               fFoundChecked |= fChecked;
            }

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hMenu, TPM_LEFTALIGN | TPM_VCENTERALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);

            if (!iRet)
               return 0;

            if ((iRet >= IDC_GRIDTOPO) && (iRet <= IDC_GRIDTOPO + (int)dwNumUnits)) {
               iRet -= IDC_GRIDTOPO;

               if (iRet < (int)dwNumUnits) {
                  m_fGridUDMinor = pafUnits[iRet*2+0];
                  m_fGridUDMajor = pafUnits[iRet*2+1];
               }
               else
                  m_fGridUDMinor = m_fGridUDMajor = 0;

               // update
               InvalidateDisplay();
               InvalidateRect (m_hWndElevKey, NULL, FALSE);
               MapScrollUpdate();
            }
         }
         return 0;


      case IDC_NEXTMONITOR:
         {
            // get the monitor info
            HMONITOR hMon = MonitorFromWindow (hWnd, MONITOR_DEFAULTTONEAREST);

            // find it
            FillXMONITORINFO ();
            PCListFixed pListXMONITORINFO;
            pListXMONITORINFO = ReturnXMONITORINFO();
            DWORD i;
            PXMONITORINFO p;
            for (i = 0; i < pListXMONITORINFO->Num(); i++) {
               p = (PXMONITORINFO) pListXMONITORINFO->Get(i);
               if (p->hMonitor == hMon)
                  break;
            }

            // If only one monitor then tell user
            if (pListXMONITORINFO->Num() <= 1) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"Your computer only has one monitor connected.",
                  APPLONGNAMEW L" supports multiple monitors, letting you display "
                  L"different views on each monitor. To learn more about it, look up "
                  L"\"Multiple display support\" in windows help, or ask your local "
                  L"computer guru.",
                  MB_ICONINFORMATION | MB_OK);

               return 0;
            }

            // what's the relationship is size to this one?
            p = (PXMONITORINFO) pListXMONITORINFO->Get(i);
            RECT rCurW, rCurSize;
            rCurW = p->rWork;
            GetWindowRect (m_hWnd, &rCurSize);
            int aiCurW[2];
            aiCurW[0] = rCurW.right - rCurW.left;
            aiCurW[1] = rCurW.bottom - rCurW.top;


            // move it to the next one
            i++;
            p = (PXMONITORINFO) pListXMONITORINFO->Get(i % pListXMONITORINFO->Num());
            RECT rNew;
            int aiNew[2];
            rNew = p->rWork;
            aiNew[0] = rNew.right - rNew.left;
            aiNew[1] = rNew.bottom - rNew.top;

            if (m_fSmallWindow) {
               MoveWindow (hWnd,
                  (rCurSize.left - rCurW.left) * aiNew[0] / aiCurW[0] + rNew.left,
                  (rCurSize.top - rCurW.top) * aiNew[1] / aiCurW[1] + rNew.top,
                  (rCurSize.right - rCurSize.left) * aiNew[0] / aiCurW[0],
                  (rCurSize.bottom - rCurSize.top) * aiNew[1] / aiCurW[1],
                  TRUE);
            }
            else
               MoveWindow (hWnd, p->rWork.left, p->rWork.top,
                  p->rWork.right - p->rWork.left, p->rWork.bottom - p->rWork.top, TRUE);

         }
         return 0;
      }
      break;

   case WM_PAINT:
      return Paint (hWnd, uMsg, wParam, lParam);

   case WM_MOUSEWHEEL:
      {
         short zDelta = (short) HIWORD(wParam); 
         BOOL fZoomIn = (zDelta >= 0);

         // if we're in the mountain creating mode then dont zoom. instead switch
         // seeds
         if (m_dwButtonDown) switch (m_adwPointerMode[m_dwButtonDown-1]) {
         case IDC_GROUNDGENMTN:
         case IDC_GROUNDGENPLAT:
         case IDC_GROUNDGENRAVINE:
         case IDC_GROUNDGENVOLCANO:
         case IDC_GROUNDGENRAISE:
         case IDC_GROUNDGENFLAT:
         case IDC_GROUNDGENLOWER:
         case IDC_GROUNDGENHILL:
         case IDC_GROUNDGENCRATER:
         case IDC_GROUNDGENHILLROLL:
            if (fZoomIn)
               m_dwGroundGenSeed++;
            else
               m_dwGroundGenSeed--;
            GroundGen (FALSE);
            return 0;
         // if default fall through
         }

         fp fX, fY;
         RECT r;
         GetClientRect (m_hWndMap, &r);
         fX = (fp)(r.right + r.left) / 2.0 / m_fViewScale + m_tpViewUL.h;
         fY = (fp)(r.bottom + r.top) / 2.0 / m_fViewScale + m_tpViewUL.v;

         if (fZoomIn) {
            m_fViewScale *= sqrt((fp)2);
            m_fViewScale = min(10, m_fViewScale);  // no more than 10 pixels per point
         }
         else {   // zoom out
            m_fViewScale /= sqrt((fp)2);
            m_fViewScale = max(100.0 / (fp)max(m_gState.dwWidth, m_gState.dwHeight), m_fViewScale);
         }

         // orient so that pooint clicked on is in center
         m_tpViewUL.h = fX - (fp)(r.right + r.left) / 2.0 / m_fViewScale;
         m_tpViewUL.v = fY - (fp)(r.bottom + r.top) / 2.0 / m_fViewScale;

         // redraw
         InvalidateDisplay ();
         MapScrollUpdate ();
      }
      return 0;

   case WM_KEYDOWN:
      {
         // else try accelerators
         if (m_pbarView->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarGeneral->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarObject->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarMisc->TestAccelerator(uMsg, wParam, lParam))
            return 0;
      }
      break;

   case WM_SYSCHAR:
      {
         // else try accelerators
         if (m_pbarView->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarGeneral->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarObject->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarMisc->TestAccelerator(uMsg, wParam, lParam))
            return 0;
      }
      break;

   case WM_CHAR:
      {
         // BUGFIX - No longer have keyboard accelerators to go to next button
         // because using 'z' as a way to limit to UD
         //BOOL fControl = (GetKeyState (VK_CONTROL) < 0);
         //if (!fControl) {
         //   switch ((TCHAR) wParam) {
         //   case 'a':
         //      NextPointerMode (6);
         //      return TRUE;
         //   case 'z':
         //      NextPointerMode (7);
         //      return TRUE;
         //   }
         //}

         // else try accelerators
         if (m_pbarView->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarGeneral->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarObject->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarMisc->TestAccelerator(uMsg, wParam, lParam))
            return 0;
      }
      break;

   case WM_SIZE:
      {
         // called when the window is sized. Move the button bars around
         int iWidth = LOWORD(lParam);
         int iHeight = HIWORD(lParam);

         if (!m_hWnd)
            m_hWnd = hWnd; // so calcthumbnail loc works
         RECT  r;

         // general button bar
         r.top = 0;
         r.left = VARBUTTONSIZE;
         r.right = iWidth - VARBUTTONSIZE;
         r.bottom = VARBUTTONSIZE;
         m_pbarView->Move (&r);
         m_pbarView->Show(TRUE);

         // object button bar
         r.top = iHeight - VARBUTTONSIZE;
         r.bottom = iHeight;
         m_pbarObject->Move (&r);
         m_pbarObject->Show(TRUE);

         // view button bar
         r.left = 0;
         r.top = VARBUTTONSIZE;
         r.right = VARBUTTONSIZE;
         r.bottom = iHeight - VARBUTTONSIZE;
         m_pbarMisc->Move (&r);
         m_pbarMisc->Show(TRUE);

         // misc button bar
         r.left = iWidth - VARBUTTONSIZE;
         r.right = iWidth;
         m_pbarGeneral->Move (&r);
         m_pbarGeneral->Show(TRUE);

         // move the interal windows - map
         r.left = VARSCREENSIZE;
         r.top = VARSCREENSIZE;
         r.right = iWidth * 3 / 4;
         r.bottom = iHeight - VARSCREENSIZE;
         MoveWindow (m_hWndMap, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
         if (!IsWindowVisible (m_hWndMap))
            ShowWindow (m_hWndMap, SW_SHOW);
         MapScrollUpdate();

         // key window
         r.left = r.right + VARSCROLLSIZE;
         r.right = iWidth - VARSCREENSIZE;
         MoveWindow (m_hWndElevKey, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
         MoveWindow (m_hWndTextList, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
         MoveWindow (m_hWndForestList, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
      }
      break;

   case WM_CLOSE:
      {
         // If dirty then ask if want to save
         if (m_fDirty) {
            int iRet;
            iRet = EscMessageBox (m_hWnd, ASPString(),
               L"Do you want to save your changes?",
               L"If you don't save your "
               L"changes they will be lost.",
               MB_ICONQUESTION | MB_YESNOCANCEL);

            if (iRet == IDCANCEL)
               return 0;

            if (iRet == IDYES) {
               if (!Save(m_hWnd))
                  return FALSE;
            }
         }

         delete this;
      }
      return 0;

   };

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/***********************************************************************************
MapWndProc - Window procedure for the house view object.
*/
LRESULT CGroundView::MapWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (uMsg == gdwMouseWheel)
      uMsg = WM_MOUSEWHEEL;

   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWndMap = hWnd;
      }
      return 0;

   case WM_HSCROLL:
   case WM_VSCROLL:
      {
         BOOL fHorz = (uMsg == WM_HSCROLL);
         // only deal with horizontal scroll
         HWND hWndScroll = m_hWndMap;

         // get the scrollbar info
         SCROLLINFO si;
         memset (&si, 0, sizeof(si));
         si.cbSize = sizeof(si);
         si.fMask = SIF_ALL;
         GetScrollInfo (hWndScroll, fHorz ? SB_HORZ : SB_VERT, &si);
         
         // what's the new position?
         switch (LOWORD(wParam)) {
         default:
            return 0;
         case SB_ENDSCROLL:
            return 0;
         case SB_LINEUP:
         //case SB_LINELEFT:
            si.nPos  -= max(si.nPage / 8, 1);
            break;

         case SB_LINEDOWN:
         //case SB_LINERIGHT:
            si.nPos  += max(si.nPage / 8, 1);
            break;

         case SB_PAGELEFT:
         //case SB_PAGEUP:
            si.nPos  -= si.nPage;
            break;

         case SB_PAGERIGHT:
         //case SB_PAGEDOWN:
            si.nPos  += si.nPage;
            break;

         case SB_THUMBPOSITION:
            si.nPos = si.nTrackPos;
            break;
         case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
         }

         // don't go beyond min and max
         si.nPos = min((int)(si.nMax - si.nPage), (int)si.nPos);
         si.nPos = max(si.nPos,si.nMin);

         // new values base on this
         if (fHorz)
            m_tpViewUL.h = (int) si.nPos / 10.0;
         else
            m_tpViewUL.v = (int) si.nPos / 10.0;
         InvalidateDisplay();
         MapScrollUpdate();

         return 0;
      }
      break;



   case WM_PAINT:
      return PaintMap (hWnd, uMsg, wParam, lParam);

   case WM_MOUSEWHEEL:
      // pass it up
      return WndProc (hWnd, uMsg, wParam, lParam);


   case WM_LBUTTONDOWN:
      return ButtonDown (hWnd, uMsg, wParam, lParam, 0);
   case WM_MBUTTONDOWN:
      return ButtonDown (hWnd, uMsg, wParam, lParam, 1);
   case WM_RBUTTONDOWN:
      return ButtonDown (hWnd, uMsg, wParam, lParam, 2);

   case WM_LBUTTONUP:
      return ButtonUp (hWnd, uMsg, wParam, lParam, 0);
   case WM_MBUTTONUP:
      return ButtonUp (hWnd, uMsg, wParam, lParam, 1);
   case WM_RBUTTONUP:
      return ButtonUp (hWnd, uMsg, wParam, lParam, 2);

   case WM_MOUSEMOVE:
      return MouseMove (hWnd, uMsg, wParam, lParam);

   };

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/***********************************************************************************
KeyWndProc - Window procedure for the house view object.
*/
LRESULT CGroundView::KeyWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (uMsg == gdwMouseWheel)
      uMsg = WM_MOUSEWHEEL;

   switch (uMsg) {
   case WM_CREATE:
      m_hWndElevKey = hWnd;
      return 0;


   case WM_PAINT:
      return PaintKey (hWnd, uMsg, wParam, lParam);

   case WM_MOUSEWHEEL:
      // pass it up
      return WndProc (hWnd, uMsg, wParam, lParam);


   case WM_MOUSEMOVE:
      SetCursor (LoadCursor (NULL, IDC_NO));
      if (GetFocus() != m_hWnd)
         SetFocus (m_hWnd);   // so list box doesnt take away focus
      return 0;

   };

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}
/*********************************************************************************
CGroundView::PointInImage - Given a pixel in the map's client rectangle, this
fills in pfImageX and pfImageY with the x and y ground-pixel locations.

inputs
   int         iX, iY - Point in map client's rectangle
   fp          *pfImageX, *pfImageY - Filled in with ground-pixel
returns
   BOOL - TRUE if over the ground
*/
BOOL CGroundView::PointInImage (int iX, int iY, fp *pfImageX, fp *pfImageY)
{
   fp fX, fY;
   fX = (fp) iX / m_fViewScale + m_tpViewUL.h;
   fY = (fp) iY / m_fViewScale + m_tpViewUL.v;

   if (pfImageX)
      *pfImageX = fX;
   if (pfImageY)
      *pfImageY = fY;

   return (fX >= 0) && (fX <= (fp)m_gState.dwWidth-1) && (fY >= 0) &&
      (fY <= (fp)m_gState.dwHeight-1);
}


/*********************************************************************************
CGroundView::StateClone - Copies the information from pOrig to pNew. In the process
it also allocates memory bits for pNew

inputs
   PGVSTATE       pNew - Copy into here
   PGVSTATE       pOrig - COpy from here
return
   BOOL - TRUE if succesful
*/
BOOL CGroundView::StateClone (PGVSTATE pNew, PGVSTATE pOrig)
{
   memcpy (pNew, pOrig, sizeof(*pNew));

   // allocate for elevation
   pNew->pmemElev = new CMem;
   if (!pNew->pmemElev)
      return FALSE;
   if (!pNew->pmemElev->Required (pNew->dwHeight * pNew->dwWidth * sizeof(WORD)))
      return FALSE;
   pNew->pawElev = (WORD*) pNew->pmemElev->p;
   memcpy (pNew->pawElev, pOrig->pawElev, pNew->dwHeight * pNew->dwWidth * sizeof(WORD));

   // allocate for the textures
   pNew->pmemTextSet = new CMem;
   if (!pNew->pmemTextSet)
      return FALSE;
   if (!pNew->pmemTextSet->Required (pNew->dwHeight * pNew->dwWidth * sizeof(BYTE) * pOrig->plTextSurf->Num()))
      return FALSE;
   pNew->pabTextSet = (BYTE*) pNew->pmemTextSet->p;
   memcpy (pNew->pabTextSet, pOrig->pabTextSet,
      pNew->dwHeight * pNew->dwWidth * sizeof(BYTE) * pOrig->plTextSurf->Num());

   // allocate for the textures
   pNew->pmemForestSet = new CMem;
   if (!pNew->pmemForestSet)
      return FALSE;
   if (!pNew->pmemForestSet->Required (pNew->dwHeight * pNew->dwWidth * sizeof(BYTE) * pOrig->plPCForest->Num()))
      return FALSE;
   pNew->pabForestSet = (BYTE*) pNew->pmemForestSet->p;
   memcpy (pNew->pabForestSet, pOrig->pabForestSet,
      pNew->dwHeight * pNew->dwWidth * sizeof(BYTE) * pOrig->plPCForest->Num());

   // clone the forest list
   DWORD i;
   pNew->plPCForest = new CListFixed;
   if (!pNew->plPCForest)
      return FALSE;
   pNew->plPCForest->Init (sizeof(PCForest));
   PCForest *ppf, pf;
   ppf = (PCForest*) pOrig->plPCForest->Get(0);
   for (i = 0; i < pOrig->plPCForest->Num(); i++) {
      pf = ppf[i]->Clone();
      pNew->plPCForest->Add (&pf);
   }

   // clone texture lists
   pNew->plTextColor = new CListFixed;
   pNew->plTextSurf = new CListFixed;
   if (!pNew->plTextColor || !pNew->plTextSurf)
      return FALSE;
   pNew->plTextSurf->Init (sizeof(PCObjectSurface));
   PCObjectSurface *ppos;
   PCObjectSurface pos;
   ppos = (PCObjectSurface*) pOrig->plTextSurf->Get(0);
   for (i = 0; i < pOrig->plTextSurf->Num(); i++) {
      pos = ppos[i]->Clone();
      pNew->plTextSurf->Add (&pos);
   }
   pNew->plTextColor->Init (sizeof(COLORREF*), pOrig->plTextColor->Get(0),
      pOrig->plTextColor->Num());

   // clone water
   memcpy (pNew->afWaterElev, pOrig->afWaterElev, sizeof(pOrig->afWaterElev));
   for (i = 0; i < GWATERNUM * GWATEROVERLAP; i++)
      pNew->apWaterSurf[i] = pOrig->apWaterSurf[i]->Clone();


   return TRUE;
}

/*********************************************************************************
CGroundView::StateFree - Frees all the memory pointed to by pState

inputs
   PGVSTATE       pState - free memory pointed to by this
returns
   none
*/
void CGroundView::StateFree (PGVSTATE pState)
{
   if (pState->pmemElev)
      delete pState->pmemElev;
   pState->pmemElev = NULL;

   if (pState->pmemTextSet)
      delete pState->pmemTextSet;
   pState->pmemTextSet = NULL;

   if (pState->pmemForestSet)
      delete pState->pmemForestSet;
   pState->pmemForestSet = NULL;

   if (pState->plTextColor)
      delete pState->plTextColor;
   pState->plTextColor = NULL;

   DWORD i;
   if (pState->plTextSurf) {
      PCObjectSurface *ppos;
      ppos = (PCObjectSurface*) pState->plTextSurf->Get(0);
      for (i = 0; i < pState->plTextSurf->Num(); i++)
         delete ppos[i];
      delete pState->plTextSurf;
   }
   pState->plTextSurf = NULL;

   if (pState->plPCForest) {
      PCForest *ppf;
      ppf = (PCForest*) pState->plPCForest->Get(0);
      for (i = 0; i < pState->plPCForest->Num(); i++)
         delete ppf[i];
      delete pState->plPCForest;
   }
   pState->plPCForest = NULL;

   for (i = 0; i < GWATERNUM * GWATEROVERLAP; i++)
      if (pState->apWaterSurf[i])
         delete pState->apWaterSurf[i];

}


/**********************************************************************************
CGroundView::Elevation - Fills in the elevation given and X and Y.

inputs
   DWORD       dwX, dwY - x and Y. Assumed to be within m_dwWidth and m_dwHeight
   PCPoint     pElev - Filled with elevation
returns
   none
*/
void CGroundView::Elevation (DWORD dwX, DWORD dwY, PCPoint pElev)
{
   pElev->p[0] = ((fp)dwX - (fp)(m_gState.dwWidth-1)/2) * m_gState.fScale;
   pElev->p[1] = -((fp)dwY - (fp)(m_gState.dwHeight-1)/2) * m_gState.fScale;
   pElev->p[2] = (fp)m_gState.pawElev[dwX + dwY * m_gState.dwWidth] / (fp)0xffff *
      (m_gState.tpElev.v - m_gState.tpElev.h) + m_gState.tpElev.h;
}

/**********************************************************************************
CGroundView::Normal - Fills the given point with the normal at X and Y. Is normalized

inputs
   DWORD       dwX, dwY - x and Y. Assumed to be within m_dwWidth-1 and m_dwHeight-1
   PCPoint     pNorm - Filled with elevation
returns
   none
*/
void CGroundView::Nornal (DWORD dwX, DWORD dwY, PCPoint pNorm)
{
   CPoint pUL, pUR, pLL, pLR;
   Elevation (dwX, dwY, &pUL);
   Elevation (dwX+1, dwY, &pUR);
   Elevation (dwX+1, dwY+1, &pLR);
   Elevation (dwX, dwY+1, &pLL);

   pUR.Subtract (&pLL);
   pLR.Subtract (&pUL);
   pNorm->CrossProd (&pLR, &pUR);
   pNorm->Normalize();
}

/**********************************************************************************
CGroundView::HVToElev - Takes an HV value (0..m_dwWidth-1, 0..m_dwHeight-1) and
returns an elevation.

inputs
   fp       fH, fV - Horizontal and vertical values from 0..m_dwWidth-1, 0..m_dwHeight-1
   WORD     *pwElev - Filled with the elevation from 0x0000 to 0xffff
returns
   BOOL - TRUE if success. FALSE if value out of range
*/
BOOL CGroundView::HVToElev (fp fH, fp fV, WORD *pwElev)
{
   // make sure it's within the limits
   if ((fH < 0) || (fV < 0) || (fH > (fp) m_gState.dwWidth-1) || (fV > (fp)m_gState.dwHeight-1))
      return FALSE;

   fH = min((fp)m_gState.dwWidth-1-CLOSE,fH);
   fV = min((fp)m_gState.dwHeight-1-CLOSE,fV);

   // get the point
   WORD *pw;
   pw = &m_gState.pawElev[(DWORD)fH + (DWORD)fV * m_gState.dwWidth];
   fH -= floor(fH);
   fV -= floor(fV);

   // figure out top and bottom values
   fp fTop, fBottom, fValue;
   fTop = (1.0 - fH) * (fp)pw[0] + fH * (fp)pw[1];
   fBottom = (1.0 - fH) * (fp)pw[m_gState.dwWidth] + fH * (fp)pw[m_gState.dwWidth+1];
   fValue = (1.0 - fV) * fTop + fV * fBottom;

   *pwElev = (WORD) fValue;
   return TRUE;
}



/*****************************************************************************
CGroundView::TicksVertical - Draws the timeline ticks someplace.

inputs
   HDC            hDC - To draw onto - assumed to be in the map
   RECT           *prText - Rectangle to draw text in
   RECT           *pr - Rectangle on the HDC
   COLORREF       crMinor - Color of the minor ticks
   COLORREF       crMajor - Color of the major ticks
returns
   none
*/
#define BETWEENMINORTICKS     25    // number of pixels between each minor tick
#define TICKFONT              10

void CGroundView::TicksVertical (HDC hDC, RECT *prText, RECT *pr, COLORREF crMinor, COLORREF crMajor)
{
   // find start and stop time, and how much space betweeen times
   CPoint   pTime;
   fp fTime, fTickTime;
   pTime.Zero();
   pTime.p[0] = (m_tpViewUL.h + (fp) pr->left / m_fViewScale - (fp)m_gState.dwWidth/2) * m_gState.fScale;
   pTime.p[1] = (m_tpViewUL.h + (fp) pr->right / m_fViewScale - (fp)m_gState.dwWidth/2) * m_gState.fScale;
   fTime = pTime.p[1] - pTime.p[0];
   if ((fTime <= CLOSE) || (pr->right <= pr->left))
      return;  // no time spanned;
   fTickTime = (fp) (pr->right - pr->left) / (fp) BETWEENMINORTICKS; // number of ticks can fit it
   fTickTime = fTime / fTickTime; // amount of time allowed per minor tick

   // what units?
   double fMajor, fMinor;  // using double precision just to make sure lines up properly
   fp *pafUnits;
   DWORD dwNumUnits;
   if (MeasureDefaultUnits() & MUNIT_ENGLISH) {
      pafUnits = gafGridEnglish;
      dwNumUnits = sizeof(gafGridEnglish)/sizeof(fp)/2;
   }
   else {
      pafUnits = gafGridMetric;
      dwNumUnits = sizeof(gafGridMetric)/sizeof(fp)/2;
   }
   DWORD i;
   fTickTime *= 10;  // fudge factor
   for (i = 0; i < dwNumUnits; i++) {
      if (fTickTime >= pafUnits[i*2+0])
         break;
   }
   i = min(i, dwNumUnits-1);
   fMinor = pafUnits[i*2+0];
   fMajor = pafUnits[i*2+1];

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
   for (; fStart < fEnd; fStart += fMinor) {
      // is it a major tick?
      BOOL fIsMajor = (myfmod (fStart + fMinor/10, fMajor) < (fMinor/5));

      // convert back
      int iX;
      pTime.Zero();
      pTime.p[0] = (fStart / m_gState.fScale - m_tpViewUL.h + (fp)m_gState.dwWidth/2) * m_fViewScale;
      iX = (int) pTime.p[0];

      // draw line
      SelectObject (hDC, fIsMajor ? hPenMajor : hPenMinor);
      MoveToEx (hDC, iX, pr->top, NULL);
      LineTo (hDC, iX, pr->bottom);

      // what's the text
      char szTemp[32];
      MeasureToString (fStart, szTemp);

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
      ExtTextOut (hDC,  iX + size.cy, prText->bottom - size.cx - TICKFONT/2, 0, NULL, szTemp, iLen, NULL);
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
CGroundView::TicksHorizontal - Draws the horizontal lines in the timeline - basically the value

inputs
   HDC            hDC - To draw onto - assumed to be in the map
   RECT           *prText - Rectangle to draw text in
   RECT           *pr - Rectangle on the HDC
   COLORREF       crMinor - Color of the minor ticks
   COLORREF       crMajor - Color of the major ticks
   BOOL           fKey - If TRUE then drawing the topography key
returns
   none
*/

void CGroundView::TicksHorizontal (HDC hDC, RECT *prText, RECT *pr, COLORREF crMinor, COLORREF crMajor,
                                   BOOL fKey)
{
   // find start and stop time, and how much space betweeen times
   CPoint   pRange, pRange2;
   RECT rKey;
   fp fRange, fTickRange;
   pRange.Zero();
   BOOL  fDrawMinorText = TRUE;
   if (fKey) {
      pRange.p[2] = m_gState.tpElev.v;
      pRange2.p[2] = m_gState.tpElev.h;
      GetClientRect (m_hWndElevKey, &rKey);

      if (!m_fGridUDMajor || !m_fGridUDMinor)
         return;
   }
   else {
      pRange.p[2] = -(m_tpViewUL.v + (fp)pr->top / m_fViewScale - (fp)m_gState.dwHeight/2) * m_gState.fScale;
      pRange2.p[2] = -(m_tpViewUL.v + (fp)pr->bottom / m_fViewScale - (fp)m_gState.dwHeight/2) * m_gState.fScale;
   }
   fRange = pRange.p[2] - pRange2.p[2];
   if ((fRange < CLOSE) || (pr->bottom <= pr->top))
      return;  // no time spanned;
   fTickRange = (fp) (pr->bottom - pr->top) / (fp) BETWEENMINORTICKS; // number of ticks can fit it
   fTickRange = fRange / fTickRange; // amount of time allowed per minor tick

   // what units?
   double fMajor, fMinor;  // using double precision just to make sure lines up properly
   if (fKey) {
      fMajor = m_fGridUDMajor;
      fMinor = m_fGridUDMinor;

      if (fTickRange > fMinor)
         fDrawMinorText = FALSE;
   }
   else {
      fp *pafUnits;
      DWORD dwNumUnits;
      if (MeasureDefaultUnits() & MUNIT_ENGLISH) {
         pafUnits = gafGridEnglish;
         dwNumUnits = sizeof(gafGridEnglish)/sizeof(fp)/2;
      }
      else {
         pafUnits = gafGridMetric;
         dwNumUnits = sizeof(gafGridMetric)/sizeof(fp)/2;
      }
      DWORD i;
      fTickRange *= 10;  // fudge factor
      for (i = 0; i < dwNumUnits; i++) {
         if (fTickRange >= pafUnits[i*2+0])
            break;
      }
      i = min(i, dwNumUnits-1);
      fMinor = pafUnits[i*2+0];
      fMajor = pafUnits[i*2+1];
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
      if (fKey)
         pRange.p[2] = (m_gState.tpElev.v - fStart) / (m_gState.tpElev.v - m_gState.tpElev.h) *
            (fp)rKey.bottom;
      else
         pRange.p[2] = (-fStart / m_gState.fScale - m_tpViewUL.v + (fp)m_gState.dwHeight/2) * m_fViewScale;
      iY = (int) pRange.p[2];

      // draw line
      SelectObject (hDC, fIsMajor ? hPenMajor : hPenMinor);
      MoveToEx (hDC, pr->left, iY, NULL);
      LineTo (hDC, pr->right+1, iY);

      // if it's minor and dont want to draw minor text then dont
      if (!fIsMajor && !fDrawMinorText)
         continue;

      // what's the text
      char szTemp[32];
      MeasureToString (fStart, szTemp);

      // calculate the text size
      int iLen;
      SIZE size;
      iLen = (DWORD)strlen(szTemp);
      SelectObject (hDC, hFontNorm);
      GetTextExtentPoint32 (hDC, szTemp, iLen, &size);

      // draw text
      COLORREF crOld;
      crOld = SetTextColor (hDC, fIsMajor ? crMajor : crMinor);
      //ExtTextOut (hDC,  prText->left + TICKFONT/2, iY - size.cy, 0, NULL, szTemp, iLen, NULL);
      ExtTextOut (hDC,  prText->right - TICKFONT/2 - size.cx, iY - size.cy, 0, NULL, szTemp, iLen, NULL);
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


/*****************************************************************************
CGroundView::MapScrollUpdate - Updates the locations and sizes up the
map windows scrollbars
*/
void CGroundView::MapScrollUpdate (void)
{
   RECT  r;
   GetClientRect (m_hWndMap, &r);

   // how big is this in terms of groundpix
   fp fScreenX, fScreenY;
   fScreenX = (fp)(r.right - r.left) / m_fViewScale;
   fScreenY = (fp)(r.bottom - r.top) / m_fViewScale;

   SCROLLINFO si;
   memset (&si, 0, sizeof(si));
   si.cbSize = sizeof(si);
   si.fMask = SIF_ALL;

   // horizontal
   si.nPage = (int)(fScreenX * 10.0);
   si.nMin = -(int)si.nPage;
   si.nMax = (int)m_gState.dwWidth*10 + (int)si.nPage;
   si.nPos = (int)(m_tpViewUL.h * 10.0);
   si.nPos = max(si.nPos, si.nMin);
   si.nPos = min(si.nPos, si.nMax - (int)si.nPage);
   si.nTrackPos = si.nTrackPos;
   SetScrollInfo (m_hWndMap, SB_HORZ, &si, TRUE);

   // vertical
   si.nPage = (int)(fScreenY * 10.0);
   si.nMin = -(int)si.nPage;
   si.nMax = (int)m_gState.dwHeight*10 + (int)si.nPage;
   si.nPos = (int)(m_tpViewUL.v * 10.0);
   si.nPos = max(si.nPos, si.nMin);
   si.nPos = min(si.nPos, si.nMax - (int)si.nPage);
   si.nTrackPos = si.nTrackPos;
   SetScrollInfo (m_hWndMap, SB_VERT, &si, TRUE);

}




/*****************************************************************************
CGroundView::UndoUpdateButtons - Update the undo/redo buttons based on their
current state.
*/
void CGroundView::UndoUpdateButtons (void)
{
   BOOL fUndo = (m_lGVSTATEUndo.Num() ? TRUE : FALSE);
   BOOL fRedo = (m_lGVSTATERedo.Num() ? TRUE : FALSE);

   m_pUndo->Enable (fUndo);
   m_pRedo->Enable (fRedo);
}

/*****************************************************************************
CGroundView::UndoAboutToChange - Call this before an object is about to change.
It will remember the current state for undo, and update the buttons.
Also sets dirty flag
*/
void CGroundView::UndoAboutToChange (void)
{
   m_fDirty = TRUE;

   // cant be working with undo if there's something there
   if (!m_lGVSTATEUndo.Num())
      m_fWorkingWithUndo = FALSE;

   // if already have something no change
   if (m_fWorkingWithUndo)
      return;

   // else, remember
   GVSTATE gs;
   if (!StateClone (&gs, &m_gState))
      return;
   m_lGVSTATEUndo.Add (&gs);

   // if there are too many undo states remembered then toss out
   PGVSTATE ps;
   if (m_lGVSTATEUndo.Num() >= 10) {
      ps = (PGVSTATE) m_lGVSTATEUndo.Get(0);
      StateFree (ps);
      m_lGVSTATEUndo.Remove(0);
   }

   // clear out the redo
   ps = (PGVSTATE) m_lGVSTATERedo.Get(0);
   DWORD i;
   for (i = 0; i < m_lGVSTATERedo.Num(); i++, ps++)
      StateFree (ps);
   m_lGVSTATERedo.Clear();

   // update buttons
   m_fWorkingWithUndo = TRUE;
   UndoUpdateButtons ();
}

/*****************************************************************************
CGroundView::UndoCache - If were working with an undo buffer then set m_fWorkingWithUNdo
to FALSE so that any other changes will be cached.
*/
void CGroundView::UndoCache (void)
{
   m_fWorkingWithUndo = FALSE;
}


/*****************************************************************************
CGroundView::Undo - Does an undo or redo

inputs
   BOOL     fUndo - If TRUE do an undo, else do a redo
returns
   BOOL - TRUE if success
*/
BOOL CGroundView::Undo (BOOL fUndo)
{
   PGVSTATE ps;
   if (fUndo) {
      // doing an undo
      if (!m_lGVSTATEUndo.Num())
         return FALSE;  // nothing to undo

      // add current state to redo
      m_lGVSTATERedo.Add (&m_gState);

      // get last undo
      ps = (PGVSTATE) m_lGVSTATEUndo.Get(m_lGVSTATEUndo.Num()-1);
      m_gState = *ps;
      m_lGVSTATEUndo.Remove (m_lGVSTATEUndo.Num()-1);
   }
   else {
      // doing a redi
      if (!m_lGVSTATERedo.Num())
         return FALSE;  // nothing to undo

      // add current state to undo
      m_lGVSTATEUndo.Add (&m_gState);

      // get last redo
      ps = (PGVSTATE) m_lGVSTATERedo.Get(m_lGVSTATERedo.Num()-1);
      m_gState = *ps;
      m_lGVSTATERedo.Remove (m_lGVSTATERedo.Num()-1);
   }

   // update displays
   UndoCache();
   InvalidateDisplay ();
   InvalidateRect (m_hWndElevKey, NULL, FALSE);
   UpdateTextList ();
   UpdateForestList ();
   UndoUpdateButtons ();

   return TRUE;
}

/*****************************************************************************
CGroundView::Save - Saves the information back to the object it got it from.

inputs
   HWND     hWndParent - To show error messages on
returns
   BOOL - TRUE if saved, FALSE if error
*/
BOOL CGroundView::Save (HWND hWndParent)
{
   // try to get the state information
   if (!FindViewForWorld (m_pWorld)) {
err:
      EscMessageBox (hWndParent, ASPString(),
         L"The ground object no longer seems to exist.",
         L"You cannot save this object. You will need to shut down the window though.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;  // world no longer exists
   }
   PCObjectSocket pos;
   pos = m_pWorld->ObjectGet(m_pWorld->ObjectFind(&m_gObject));
   if (!pos)
      goto err;

   // write it
   OSMGROUNDINFOGET gi;
   memset (&gi, 0, sizeof(gi));
   gi.dwHeight = m_gState.dwHeight;
   gi.dwWidth = m_gState.dwWidth;
   gi.fScale = m_gState.fScale;
   gi.pawElev = m_gState.pawElev;
   gi.tpElev = m_gState.tpElev;
   gi.fWater = m_gState.fWater;
   gi.fWaterElevation = m_gState.fWaterElevation;
   gi.fWaterSize = m_gState.fWaterSize;
   memcpy (gi.afWaterElev, m_gState.afWaterElev, sizeof(m_gState.afWaterElev));
   memcpy (gi.apWaterSurf, m_gState.apWaterSurf, sizeof(m_gState.apWaterSurf));

   // texture
   gi.pabTextSet = m_gState.pabTextSet;
   gi.dwNumText = m_gState.plTextSurf->Num();
   gi.paTextSurf = (PCObjectSurface*) m_gState.plTextSurf->Get(0);
   gi.paTextColor = (COLORREF*) m_gState.plTextColor->Get(0);

   // forest
   gi.pabForestSet = m_gState.pabForestSet;
   gi.dwNumForest = m_gState.plPCForest->Num();
   gi.paForest = (PCForest*) m_gState.plPCForest->Get(0);


   if (!pos->Message (OSM_GROUNDINFOSET, &gi))
      goto err;

   // done
   m_fDirty = FALSE;
   UndoCache();
   EscChime (ESCCHIME_INFORMATION);
   return TRUE;
}



/*****************************************************************************
BrushGenerate - Given a pointer to memory, this allocates it so it's large enough
and then fills it in with byte values for the brush. 0 = no effect, 255 = max effect

inputs
   DWORD       dwSize - Size in pixels, from 1 .. N - center of brush is at dwSize/2
   DWORD       dwPoint - How pointy. 0 for flat, 1 for rounded, 2 for pointy, 3 for very pointy
   PCMem       pMem - Allocated to dwSize * dwSize * sizeof(BYTE)
returns
   PBYTE - Pointer ot pMem.p, which is allocated as [x + y * dwSize]
*/
PBYTE BrushGenerate (DWORD dwSize, DWORD dwPoint, PCMem pMem)
{
   if (!pMem->Required (dwSize * dwSize * sizeof(BYTE)))
      return NULL;

   fp fPow;
   switch (dwPoint) {
   case 0:  // flat
      fPow = .04;
      break;
   case 1:  // rounded
      fPow = .2;
      break;
   case 2:  // pointy
      fPow = 1;
      break;
   case 3:  // very pointy
      fPow = 5;
      break;
   default:
      return NULL;
   }

   PBYTE pb;
   pb = (PBYTE) pMem->p;
   DWORD x,y;
   fp fx, fy, fDist;
   BYTE bVal;
   for (y = 0; y < dwSize; y++) for (x = 0; x < dwSize; x++) {
      fx = (fp) x / (fp)dwSize * 2.0 - 1.0;
      fy = (fp) y / (fp)dwSize * 2.0 - 1.0;
      fDist = sqrt(fx * fx + fy * fy);
      if (fDist >= 1.0) {
         bVal = 0;
      }
      else {
         fDist = cos (fDist * PI/2);
         fDist = pow (fDist, fPow);
         bVal = (BYTE) (fDist * 255.0);
      }

      pb[x + y * dwSize] = bVal;
   }

   return pb;
}

/*****************************************************************************
BrushStamp - Given a bruch from BrushGenrate() and a pointer to memory
(which is an array of bytes for pixels), this stamps one image of the brush
onto the memory.

inputs
   PBYTE       pbBrush - Brush from BrushGenerate()
   DWORD       dwBrushSize - Size of brush - as passed into BrushGenerate()
   DWORD       dwBrushX - X location for center of brush. Must be >= dwBrushSize/2
               and < dwBufWidth - dwBrushSize/2
   DWORD       dwBrushY - Y location for center of brush. Same range limits as dwBrushX but in Y
   PBYTE       pbBuffer - Buffer of size dwBufWidth * dwBufHeight * sizeof(BYTE).
                  Ordered as (x + y * dwBufWidth)
   DWORD       dwBufWidth - Buffer width
   DWORD       dwBufHeight - Buffer height
   BOOL        fSubtract - Normally, pbBuffer[x + y * dwBufWidth] gets the max() of the
                  current value and the brush at that location. However, if fSubtract is
                  TRUE then the current value will have the brush subtracted
*/
void BrushStamp (PBYTE pbBrush, DWORD dwBrushSize, DWORD dwBrushX, DWORD dwBrushY,
                 PBYTE pbBuffer, DWORD dwBufWidth, DWORD dwBufHeight, BOOL fSubtract)
{
   DWORD x,y;
   PBYTE pInBrush, pInBuf;
   for (y = 0; y < dwBrushSize; y++) {
      pInBrush = pbBrush + (y * dwBrushSize);
      pInBuf = pbBuffer + (dwBrushX - dwBrushSize/2 + (y + dwBrushY - dwBrushSize/2) * dwBufWidth);

      if (fSubtract) {
         for (x = 0; x < dwBrushSize; x++, pInBrush++, pInBuf++) {
            *pInBuf = max(*pInBuf, *pInBrush);  // so dont go below 0
            *pInBuf -= *pInBrush;
         }
      }
      else {
         for (x = 0; x < dwBrushSize; x++, pInBrush++, pInBuf++)
            *pInBuf = max(*pInBuf, *pInBrush);
      }
   } // y
}



/*****************************************************************************
BrushDrag - Given a bruch from BrushGenrate() and a pointer to memory
(which is an array of bytes for pixels), this drags a brush from one location
to another

inputs
   PBYTE       pbBrush - Brush from BrushGenerate()
   DWORD       dwBrushSize - Size of brush - as passed into BrushGenerate()
   POINT       *pStart - Starting location. x and y must be >= dwBrushSize/2
               and < dwBufWidth/height - dwBrushSize/2
   POINT       *pEnd -Ending location
   PBYTE       pbBuffer - Buffer of size dwBufWidth * dwBufHeight * sizeof(BYTE).
                  Ordered as (x + y * dwBufWidth)
   DWORD       dwBufWidth - Buffer width
   DWORD       dwBufHeight - Buffer height
*/
void BrushDrag (PBYTE pbBrush, DWORD dwBrushSize, POINT *pStart, POINT *pEnd,
                 PBYTE pbBuffer, DWORD dwBufWidth, DWORD dwBufHeight)
{
   // find the maximum
   DWORD dwMax = (DWORD)max(abs(pStart->x - pEnd->x), abs(pStart->y - pEnd->y));
   
   // loop
   DWORD i, dwX, dwY;
   fp f;
   for (i = 0; i <= dwMax; i++) {
      if (dwMax)
         f = (fp) i / (fp) dwMax;
      else
         f = 0;
  
      dwX = (DWORD)((1.0 - f) * (fp)pStart->x + f * (fp)pEnd->x);
      dwY = (DWORD)((1.0 - f) * (fp)pStart->y + f * (fp)pEnd->y);

      // draw each bit
      BrushStamp (pbBrush, dwBrushSize, dwX, dwY, pbBuffer, dwBufWidth, dwBufHeight, FALSE);
   }
}


/*****************************************************************************
BrushFillAffected - This is used to calculate the effect of a brush being
dragged across the screen.

inputs
   DWORD       dwBrushSize - Size in pixels, from 1 .. N - center of brush is at dwSize/2
   DWORD       dwBrushPoint - How pointy. 0 for flat, 1 for rounded, 2 for pointy, 3 for very pointy
   DWORD       dwBrushAir - 0 for pen, 1 for brush, 2 for airbrush
   POINT       *pPixelPrev - Previous pixel location in window. Only used for pen's. If NULL then
                  will assume pen was just placed down
   POINT       *pPixelNew - New pixel location in window. CANNOT be NULL.
   PCMem       pMem - To be allocated to dwWidth * dwHeight * sizeof(BYTE).
                  dwWidth = pr->right - pr->left, dwHeight = pr->bottom - pr->top.
                  The memory is filled wit x + y*dwWidth of bytes. 0 meaning the brush
                  had no effect on the pixel, 255 being maximum effect
   RECT        *pr - Filled in with the rectangle (in screen coordinates) that the memory
               in pMem represents
returns
   PBYTE - return pMem.p, or NULL if error
*/
PBYTE BrushFillAffected (DWORD dwBrushSize, DWORD dwBrushPoint, DWORD dwBrushAir,
                         POINT *pPixelPrev, POINT *pPixelNew, PCMem pMem, RECT *pr)
{
   // how much memory is needed?
   pr->left = pr->right = pPixelNew->x;
   pr->top = pr->bottom = pPixelNew->y;
   if (pPixelPrev && (dwBrushAir == 0)) {
      // if it's a pen then will drag from the old location to the new
      pr->left = min(pPixelPrev->x, pr->left);
      pr->right = max(pPixelPrev->x, pr->right);
      pr->top = min(pPixelPrev->y, pr->top);
      pr->bottom = max(pPixelPrev->y, pr->bottom);
   }
   int iHalf;
   iHalf = (int) dwBrushSize/2 + 1;
   pr->left -= iHalf;
   pr->right += iHalf;
   pr->top -= iHalf;
   pr->bottom += iHalf;

   // how wide and hight
   DWORD dwWidth, dwHeight;
   PBYTE pbMem;
   dwWidth = (DWORD) (pr->right - pr->left);
   dwHeight = (DWORD) (pr->bottom - pr->top);
   if (!pMem->Required (dwWidth * dwHeight * sizeof(BYTE)))
      return NULL;
   pbMem = (PBYTE) pMem->p;
   memset (pbMem, 0, dwWidth * dwHeight * sizeof(BYTE));

   // create the brush
   CMem memBrush;
   PBYTE pbBrush;
   pbBrush = BrushGenerate (dwBrushSize, dwBrushPoint, &memBrush);
   if (!pbBrush)
      return NULL;

   // convert points into buffer space
   POINT pN, pP;
   pN.x = pPixelNew->x - pr->left;
   pN.y = pPixelNew->y - pr->top;
   if (pPixelPrev) {
      pP.x = pPixelPrev->x - pr->left;
      pP.y = pPixelPrev->y - pr->top;
   }

   // if it's a pen then drag, else just 
   if (pPixelPrev && (dwBrushAir == 0)) {
      // drag
      BrushDrag (pbBrush, dwBrushSize, &pP, &pN, pbMem, dwWidth, dwHeight);
      
      // remove the original location from the effect so don't double-dip
      BrushStamp (pbBrush, dwBrushSize, (DWORD) pP.x, (DWORD) pP.y,
         pbMem, dwWidth, dwHeight, TRUE);
   }
   else {
      // just put one stamp on
      BrushStamp (pbBrush, dwBrushSize, (DWORD) pN.x, (DWORD) pN.y,
         pbMem, dwWidth, dwHeight, FALSE);
   }

   // done
   return pbMem;
}


/*****************************************************************************
CGroundView::BrushApply - Applies a brush based on its size, etc.

inputs
   DWORD       dwPointer - Pointer being used. This is one of IDC_BRUSHxxx.
   POINT       *pNew - New location, in client coords of m_hWndMap
   POINT       *pPrev - Previous location, in client coords of m_hWndMap.
                  This is only used for pens (m_dwBrushAir == 0). If it's the
                  first time a pen is down then this should be NULL.
returns
   none
*/
void CGroundView::BrushApply (DWORD dwPointer, POINT *pNew, POINT *pPrev)
{
   // note undo and dirty
   UndoAboutToChange ();

   DWORD dwBrushSize, dwBrushSizeOrig;
   switch (dwPointer) {
   case IDC_BRUSH4:
      dwBrushSize = 4;
      break;
   case IDC_BRUSH8:
      dwBrushSize = 8;
      break;
   case IDC_BRUSH16:
      dwBrushSize = 16;
      break;
   case IDC_BRUSH32:
      dwBrushSize = 32;
      break;
   case IDC_BRUSH64:
      dwBrushSize = 64;
      break;
   default:
      return;  // not a brush
   }
   dwBrushSizeOrig = dwBrushSize;

   // translate the points from screen pixels to ground pixels
   POINT pGNew, pGPrev;
   pGNew.x = (int) ((fp)pNew->x / m_fViewScale + m_tpViewUL.h);
   pGNew.y = (int) ((fp)pNew->y / m_fViewScale + m_tpViewUL.v);
   if (pPrev) {
      pGPrev.x = (int) ((fp)pPrev->x / m_fViewScale + m_tpViewUL.h);
      pGPrev.y = (int) ((fp)pPrev->y / m_fViewScale + m_tpViewUL.v);
   }
   dwBrushSize = (DWORD) ceil((fp)dwBrushSize / m_fViewScale);
   dwBrushSize = max(1,dwBrushSize);

   // fill memory with what changed
   RECT rChange;
   PBYTE pbBuf;
   pbBuf = BrushFillAffected (dwBrushSize, m_dwBrushPoint, m_dwBrushAir,
      pPrev ? &pGPrev : NULL, &pGNew, &m_memPaintBuf, &rChange);
   if (!pbBuf)
      return;

   // what is the strength?
   fp fStrength, fCurStrength;
   fStrength = (fp) m_dwBrushStrength / 10.0;
   fStrength = pow(fStrength, (m_dwView==0) ? 2 : 1);
   // If airbrush then weaker strength
   if (m_dwBrushAir == 2)
      fStrength /= ((fp) AIRBRUSHPERSEC/2.0);

   // make sure dont fill in with wrong texture
   if (m_dwCurText >= m_gState.plTextSurf->Num())
      m_dwCurText = 0;  // just in case
   if ((m_dwView == 1) && (m_dwCurText >= m_gState.plTextSurf->Num()))
      return;
   if (m_dwCurForest >= m_gState.plPCForest->Num())
      m_dwCurForest = 0;
   if ((m_dwView == 2) && (m_dwCurForest >= m_gState.plPCForest->Num()))
      return;

   // do the effect
   DWORD dwWidth, dwHeight;
   dwWidth = (DWORD) (rChange.right - rChange.left);
   dwHeight = (DWORD) (rChange.bottom - rChange.top);
   int x,y, xx, yy;
   int ibx, iby;
   PBYTE pbCur;
   WORD *pawElev;
   BYTE *pabText, *pabBase;
   DWORD dwNumLayer, dwCurLayer, dwSize;
   fp f, fOrig;
   int iYMin, iYMax, iXMin, iXMax;
   iYMin = max(0, -rChange.top);
   iYMax = min((int)dwHeight, (int) m_gState.dwHeight - rChange.top);
   iXMin = max(0, -rChange.left);
   iXMax = min((int)dwWidth, (int) m_gState.dwWidth - rChange.left);
   dwSize = m_gState.dwWidth * m_gState.dwHeight;
   if (m_dwView == 1) { // texture
      dwNumLayer = m_gState.plTextSurf->Num();
      dwCurLayer = m_dwCurText;
   }
   else if (m_dwView == 2) { // forest
      dwNumLayer = m_gState.plPCForest->Num();
      dwCurLayer = m_dwCurForest;
   }
   else {   // elev
      dwNumLayer = 0;
      dwCurLayer = 0;
   }
   for (y = iYMin; y < iYMax; y++) {
      yy = (DWORD) ((int)y + rChange.top);

      pbCur = pbBuf + (iXMin + y * dwWidth);
      xx = iXMin + rChange.left;
      for (x = iXMin; x < iXMax; x++, xx++, pbCur++) {
         if (!pbCur[0])
            continue;   // nothing

         fCurStrength = fStrength * (fp)pbCur[0] / (fp)255.0;


         if (m_dwView == 1) { // texture
            pabBase = m_gState.pabTextSet + (xx + yy * m_gState.dwWidth);
            pabText = pabBase + m_dwCurText * m_gState.dwWidth * m_gState.dwHeight;
            f = (fp)pabText[0] * 256.0;
         }
         else if (m_dwView == 2) { // forest
            pabBase = m_gState.pabForestSet + (xx + yy * m_gState.dwWidth);
            pabText = pabBase + m_dwCurForest* m_gState.dwWidth * m_gState.dwHeight;
            f = (fp)pabText[0] * 256.0;
         }
         else if (m_dwView == 0) { // topo
            pawElev = m_gState.pawElev + (xx + yy * m_gState.dwWidth);
            f = (fp)pawElev[0];
         }
         fOrig = f;

         // apply the brush ffect
         switch (m_dwBrushEffect) {
            case 0: // raise
               f += fCurStrength * (fp)0xffff;
               break;
            case 1: // lower
               f -= fCurStrength * (fp)0xffff;
               break;
            case 2: // raise to max
            case 3: // lower to in
               {
                  WORD wMax, wMin, wVal;
                  wMax =0;
                  wMin = 0xffff;
                  for (iby = yy-(int)dwBrushSize/2; iby <= yy+(int)dwBrushSize/2; iby++) {
                     if ((iby < 0) || (iby >= (int)m_gState.dwHeight))
                        continue;

                     for (ibx = xx-(int)dwBrushSize/2; ibx <= xx+(int)dwBrushSize/2; ibx++) {
                        if ((ibx < 0) || (ibx >= (int)m_gState.dwWidth))
                           continue;


                        if (m_dwView == 1)
                           wVal = (WORD)m_gState.pabTextSet[ibx + iby * m_gState.dwWidth +
                                    m_dwCurText * m_gState.dwWidth * m_gState.dwHeight] * 256;
                        else if (m_dwView == 2) // forest
                           wVal = (WORD)m_gState.pabForestSet[ibx + iby * m_gState.dwWidth +
                                    m_dwCurForest * m_gState.dwWidth * m_gState.dwHeight] * 256;
                        else
                           wVal = m_gState.pawElev[ibx + iby * m_gState.dwWidth];

                        wMax = max(wVal, wMax);
                        wMin = min(wVal, wMin);
                     } // ibx
                  } // iby
                  wVal = (m_dwBrushEffect == 2) ? wMax : wMin;
                  f = (1.0 - fCurStrength) * f + fCurStrength * (fp) wVal;
               }
               break;
            case 4: // blur
            case 5: // sharpen
               {
                  DWORD dwSum, dwCount;
                  WORD wVal;
                  dwSum = dwCount = 0;
                  for (iby = yy-1; iby <= yy+1; iby++) {
                     if ((iby < 0) || (iby >= (int)m_gState.dwHeight))
                        continue;

                     for (ibx = xx-1; ibx <= xx+1; ibx++) {
                        if ((ibx < 0) || (ibx >= (int)m_gState.dwWidth))
                           continue;

                        if (m_dwView == 1)
                           wVal = (WORD)m_gState.pabTextSet[ibx + iby * m_gState.dwWidth +
                                    m_dwCurText * m_gState.dwWidth * m_gState.dwHeight] * 256;
                        else if (m_dwView == 2) // forest
                           wVal = (WORD)m_gState.pabForestSet[ibx + iby * m_gState.dwWidth +
                                    m_dwCurForest * m_gState.dwWidth * m_gState.dwHeight] * 256;
                        else
                           wVal = m_gState.pawElev[ibx + iby * m_gState.dwWidth];

                        dwSum += (DWORD)wVal;;
                        dwCount++;
                     } // ibx
                  } // iby
                  dwCount = max(dwCount,1);
                  dwSum /= dwCount;
                  if (m_dwBrushEffect == 4)  // blur
                     f = (1.0 - fCurStrength) * f + fCurStrength * (fp)dwSum;
                  else
                     f += fCurStrength * (f - (fp)dwSum);
               }
               break;
            case 6: // nouse
               f += fCurStrength * randf(-1,1) * (fp)0xffff;
               break;
         }

         // if added color, remove from elsewhere
         if (((m_dwView == 1) || (m_dwView == 2)) && (fOrig < f)) {
            fOrig = (f - fOrig) / 256.0;
            fOrig /= 2; // so only affect others half as much
            fOrig = min(255, fOrig);

            DWORD dwLayer;
            BYTE bSub;
            bSub = (BYTE) fOrig;
            for (dwLayer = 0; dwLayer < dwNumLayer; dwLayer++) {
               if (dwLayer == dwCurLayer)
                  continue;
               if (pabBase[dwLayer * dwSize] > bSub)
                  pabBase[dwLayer*dwSize] -= bSub;
               else
                  pabBase[dwLayer*dwSize] = 0;
            } // dwlayer
         }

         f = max(0,f);
         f = min(0xffff,f);


         if ((m_dwView == 1) || (m_dwView == 2)) // texture or forest
            pabText[0] = (BYTE)(((WORD)f) / 256);
         else if (m_dwView == 0) { // topo
            pawElev[0] = (WORD)f;
         }
      } // x
   } // y



   // invalidate the display
   RECT r;
   r.left = r.right = pNew->x;
   r.top = r.bottom = pNew->y;
   if (pPrev && (m_dwBrushAir == 0)) {
      r.left = min(pPrev->x, r.left);
      r.right = max(pPrev->x, r.right);
      r.top = min(pPrev->y, r.top);
      r.bottom = min(pPrev->y, r.bottom);
   }
   int iHalf;
   iHalf = (int)dwBrushSizeOrig/2 + 1 + (int)ceil(m_fViewScale);
   r.left -= iHalf;
   r.right += iHalf;
   r.top -= iHalf;
   r.bottom += iHalf;
   InvalidateRect (m_hWndMap, &r, FALSE);
}


/* GroundWaterPage
*/
BOOL GroundWaterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCGroundView pv = (PCGroundView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {

         // check if water
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"wateron");
         if (pv->m_gState.fWater && pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // set elevations
         MeasureToString (pPage, L"level", pv->m_gState.fWaterElevation);
         MeasureToString (pPage, L"size", pv->m_gState.fWaterSize);

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < GWATERNUM; i++) {
            swprintf (szTemp, L"waterelev%d", (int) i);
            MeasureToString (pPage, szTemp, pv->m_gState.afWaterElev[i]);
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // since all out edit controls

         // world going to change
         pv->UndoAboutToChange();
         MeasureParseString(pPage, L"level", &pv->m_gState.fWaterElevation);
         MeasureParseString(pPage, L"size", &pv->m_gState.fWaterSize);
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < GWATERNUM; i++) {
            swprintf (szTemp, L"waterelev%d", (int) i);
            MeasureParseString (pPage, szTemp, &pv->m_gState.afWaterElev[i]);
            if (i)
               pv->m_gState.afWaterElev[i] = max(pv->m_gState.afWaterElev[i], pv->m_gState.afWaterElev[i-1]+CLOSE);
         }
         pv->InvalidateDisplay();
         InvalidateRect (pv->m_hWndElevKey, NULL, FALSE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszWaterSurf = L"watersurf";
         DWORD dwLen = (DWORD)wcslen(pszWaterSurf);

         if (!wcsncmp(p->pControl->m_pszName, pszWaterSurf, dwLen)) {
            DWORD dwNum = _wtoi(p->pControl->m_pszName + dwLen);
            dwNum = min(dwNum, GWATERNUM*GWATEROVERLAP-1);
            PCObjectSurface *ppos = &pv->m_gState.apWaterSurf[dwNum];
            PCObjectSurface pNew;
            if (!FindViewForWorld (pv->m_pWorld))
               return TRUE;   // world no longer exists
            pNew = ppos[0]->Clone();
            if (!pNew)
               return TRUE;

            if (!TextureSelDialog (DEFAULTRENDERSHARD, pPage->m_pWindow->m_hWnd, pNew, pv->m_pWorld)) {
               delete pNew;
               return TRUE;
            }

            pv->UndoAboutToChange();
            delete ppos[0];
            ppos[0] = pNew;
            return TRUE;
         }
         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"wateron")) {
            // world going to change
            pv->UndoAboutToChange();
            pv->m_gState.fWater = p->pControl->AttribGetBOOL(Checked());
            pv->InvalidateDisplay();
            InvalidateRect (pv->m_hWndElevKey, NULL, FALSE);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Water level";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* GroundPaintPage
*/
BOOL GroundPaintPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCGroundView pv = (PCGroundView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"elev");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) (pv->m_tpPaintElev.h * 100));
         pControl = pPage->ControlFind (L"elevrange");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) (pv->m_tpPaintElev.v * 100));
         pControl = pPage->ControlFind (L"slope");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) (pv->m_tpPaintSlope.h * 100));
         pControl = pPage->ControlFind (L"sloperange");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) (pv->m_tpPaintSlope.v * 100));
         pControl = pPage->ControlFind (L"amount");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) (pv->m_fPaintAmount * 100));
      }
      break;

   case ESCN_SCROLL:
   //case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;


         float *pf;
         fp *pff;
         pf = NULL;
         pff = NULL;
         if (!_wcsicmp(psz, L"elev"))
            pf = &pv->m_tpPaintElev.h;
         else if (!_wcsicmp(psz, L"elevrange"))
            pf = &pv->m_tpPaintElev.v;
         else if (!_wcsicmp(psz, L"slope"))
            pf = &pv->m_tpPaintSlope.h;
         else if (!_wcsicmp(psz, L"amount"))
            pff = &pv->m_fPaintAmount;
         else if (!_wcsicmp(psz, L"sloperange"))
            pf = &pv->m_tpPaintSlope.v;
         if (!pf && !pff)
            break;

         // value
         fp fVal;
         fVal  =p->pControl->AttribGetInt (Pos()) / 100.0;
         if (pf && (fVal == *pf))
            return TRUE;   // no change
         if (pff && (fVal == *pff))
            return TRUE;   // no change

         // change it
         if (pf)
            *pf = fVal;
         if (pff)
            *pff = fVal;
         pv->InvalidateDisplay();
         return TRUE;
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"paint") || !_wcsicmp(p->pControl->m_pszName, L"unpaint")) {
            BOOL fPaint = !_wcsicmp(p->pControl->m_pszName, L"paint");
            pv->UndoAboutToChange();

            // loop
            DWORD x, y;
            PBYTE pText;
            DWORD dwLayer, dwCurLayer, dwNumLayer, dwScale;
            if (pv->m_dwView == 1) {
               dwNumLayer = pv->m_gState.plTextSurf->Num();
               dwCurLayer = pv->m_dwCurText;
               pText = pv->m_gState.pabTextSet;
            }
            else {
               dwNumLayer = pv->m_gState.plPCForest->Num();
               dwCurLayer = pv->m_dwCurForest;
               pText = pv->m_gState.pabForestSet;
            }
            dwScale = pv->m_gState.dwWidth * pv->m_gState.dwHeight;
            if (dwCurLayer >= dwNumLayer) {
               pPage->MBWarning (L"You must select a texture or forest to paint with first.");
               return TRUE;
            }
            fp f;
            BYTE bAdd, *pbCur, *pbLayer;
            BOOL fMatch;
            pbCur = pText;
            for (y = 0; y < pv->m_gState.dwHeight; y++) for (x = 0; x < pv->m_gState.dwHeight; x++, pbCur++) {
               f = pv->IsPixelPainted (x, y);
               if (!f)
                  continue;
               f *= 255.0;
               bAdd = (BYTE) f;
               for (dwLayer = 0; dwLayer < dwNumLayer; dwLayer++) {
                  pbLayer = pbCur + dwScale * dwLayer;
                  fMatch = (dwLayer == dwCurLayer);
                  if (!fPaint) {
                     fMatch = !fMatch;
                     if (fMatch)
                        continue;   // dont add if unpainting
                  }
                  if (fMatch)
                     pbLayer[0] = (BYTE)min(255, (DWORD)pbLayer[0] + (DWORD) bAdd);
                  else {
                     if (pbLayer[0] > bAdd)
                        pbLayer[0] -= bAdd;
                     else
                        pbLayer[0] = 0;
                  }
               }
            } // x and y

            pv->InvalidateDisplay();
            pPage->Exit (Back());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Paint based on elevation and slope";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
CGroundView::CalcWaterColorScheme - Should be called before using m_acColorScheme
or m_acColorSchemeWater, since this calculates at what level the water mark apperas.
Fills in m_wWaterMark. Or 0 is no water
*/
void CGroundView::CalcWaterColorScheme (void)
{
   m_wWaterMark = 0; // defaul
   if (!m_gState.fWater)
      return;

   fp fMark;
   fMark = (m_gState.fWaterElevation - m_gState.tpElev.h) / (m_gState.tpElev.v - m_gState.tpElev.h) *
      (fp) 0xffff;
   if (fMark <= 0)
      return;
   fMark = min(fMark, (fp)0xffff);
   m_wWaterMark = (WORD) fMark;
   m_wWaterMark = min(m_wWaterMark, 0xfffe); // so that dont get divide by zero when use
}

/*****************************************************************************
CGroundView::GroundGenMtn - Generates a mountain.

NOTE: Before calling this srand() should have been called.

inputs
   DWORD          dwType - 0 = volcaic, 1 = mountain, 2 = hill (big), 3=rolling hills,
                           4 = crater
   DWORD          dwWidth - width
   DWORD          dwHeight - height
   POINT          *pCenter - Center (in width and height) of where clicked
   DWORD          dwRadius - Distance from the center to the edge
   float             *pafHeight - Should be filled in by the function. Arranged as
                     x + y * dwWidth pixels. Each pixel should be filled in with -1 to 1,
                     where 0 is no change in the basic level.
*/
void CGroundView::GroundGenMtn (BOOL fDetailed, DWORD dwType, DWORD dwWidth,  DWORD dwHeight,
                                POINT *pCenter, DWORD dwRadius, float *pafHeight)
{
   // create the noise
   CNoise2D Noise;
   Noise.Init (10, 10);

   fp fDefScale, fDefScaleDelta, fConeHeight;
   BOOL fUseMax;
   BOOL fCrater;
   fConeHeight = 1;
   fUseMax = TRUE;
   fCrater = FALSE;
   switch (dwType) {
      default:
      case 0:  // volcanic
         fDefScale = .15;
         fDefScaleDelta = .4;
         break;
      case 1:  // mountain
         fDefScale = .25;
         fDefScaleDelta = .3;
         break;
      case 2:  // hills
         fDefScale = .4;
         fDefScaleDelta = .15;
         break;
      case 3:  // rolling hills
         fDefScale = .6;
         fDefScaleDelta = .1;
         fConeHeight = .5;
         fUseMax =FALSE;
         break;
      case 4:
         fDefScale = .1;
         fDefScaleDelta = .3;
         fUseMax =FALSE;
         fCrater = TRUE;
         break;
   }

   // cone for the mountain
   DWORD x,y, i, dwMaxDetail;
   float *pfCur;
   fp fx,fy,fz;
   pfCur = pafHeight;
   dwMaxDetail = fDetailed ? 8 : 3;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++, pfCur++) {
      fx = ((fp)pCenter->x - (fp)x) / (fp)dwRadius;
      fy = ((fp)pCenter->y - (fp)y) / (fp)dwRadius;
      fz = sqrt (fx * fx + fy * fy);
      if (fUseMax)
         fz *= 1.2;  // BUGFIX - So have space around cone
      if (fCrater) {
         // encourage sharp peak - modulation
         fz *= 2;
         if (fz < 1)
            fz = sqrt(fz);
         else {
            fz *= fz;
         }
         fz /= 2;
         fz = min(1,fz);

         // elevation
         fz = -1 + pow(sin (fz * PI),10) + pow(min(fz,.5)*2, 10);
         // dont bother with multiplying cone height since know is 1
      }
      else {
         fz = 1 - fz;
         fz *= fConeHeight;
         if (!fUseMax)
            fz = max(0, fz);
      }

      // apply noise
      fp fScale, fSize, fSum;
      for (i = 0, fSum = 0, fScale = fDefScale, fSize = 0.5;
         (i < dwMaxDetail) && (fScale >= .001);
         i++, fScale *= fDefScaleDelta, fSize *= 2)
         fSum += Noise.Value (fx * fSize, fy * fSize) * fScale;
      fz += fSum;

      if (fUseMax)
         fz = max(0, fz);
      pfCur[0] = fz;
   }
}




/*****************************************************************************
CGroundView::GroundGenRaise - creates a dome

inputs
   DWORD          dwType - 0 = rasie, 1 lower
   DWORD          dwWidth - width
   DWORD          dwHeight - height
   POINT          *pCenter - Center (in width and height) of where clicked
   DWORD          dwRadius - Distance from the center to the edge
   float             *pafHeight - Should be filled in by the function. Arranged as
                     x + y * dwWidth pixels. Each pixel should be filled in with -1 to 1,
                     where 0 is no change in the basic level.
*/
void CGroundView::GroundGenRaise (BOOL fDetailed, DWORD dwType, DWORD dwWidth,  DWORD dwHeight,
                                POINT *pCenter, DWORD dwRadius, float *pafHeight)
{
   fp fConeHeight;
   fConeHeight = 1;
   switch (dwType) {
      default:
      case 0:  // raise
         break;
      case 1:  // lower
         fConeHeight = -1;
         break;
   }

   // cone for the mountain
   DWORD x,y;
   float *pfCur;
   fp fx,fy,fz;
   pfCur = pafHeight;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++, pfCur++) {
      fx = ((fp)pCenter->x - (fp)x) / (fp)dwRadius;
      fy = ((fp)pCenter->y - (fp)y) / (fp)dwRadius;
      fz = sqrt (fx * fx + fy * fy);
      fz = min(1,fz);
      fz = pow(cos(fz * PI/2),2);
      fz *= fConeHeight;
      pfCur[0] = fz;
   }
}



/*****************************************************************************
CGroundView::GroundGenPlateau - Generates a plateau

NOTE: Before calling this srand() should have been called.

inputs
   DWORD          dwType - 0 = plateau, 1==ravine
   DWORD          dwWidth - width
   DWORD          dwHeight - height
   POINT          *pCenter - Center (in width and height) of where clicked
   DWORD          dwRadius - Distance from the center to the edge
   float             *pafHeight - Should be filled in by the function. Arranged as
                     x + y * dwWidth pixels. Each pixel should be filled in with -1 to 1,
                     where 0 is no change in the basic level.
*/
void CGroundView::GroundGenPlateau (BOOL fDetailed, DWORD dwType, DWORD dwWidth,  DWORD dwHeight,
                                POINT *pCenter, DWORD dwRadius, float *pafHeight)
{
   // create the noise
   CNoise2D Noise;
   Noise.Init (10, 10);

   fp fDefScale, fDefScaleDelta, fConeHeight;
   BOOL fRavine = FALSE;
   fConeHeight = 1;
   switch (dwType) {
      default:
      case 0:  // plateau
         fDefScale = .25;
         fDefScaleDelta = .3;
         break;
      case 1:  // ravine
         fDefScale = .5;
         fDefScaleDelta = .3;
         fRavine = TRUE;
         fConeHeight = -1;
         break;
   }

   // cone for the mountain
   DWORD x,y, i, dwMaxDetail;
   float *pfCur;
   fp fx,fy,fz;
   dwMaxDetail = fDetailed ? 8 : 3;
   pfCur = pafHeight;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++, pfCur++) {
      fx = ((fp)pCenter->x - (fp)x) / (fp)dwRadius;
      fy = ((fp)pCenter->y - (fp)y) / (fp)dwRadius;
      fz = sqrt (fx * fx + fy * fy);
      fz *= 1.2;  // BUGFIX - So have space around cone
      fz = 1 - fz;
      fz *= fConeHeight;

      // apply noise
      fp fScale, fSize, fSum;
      for (i = 0, fSum = 0, fScale = fDefScale, fSize = 0.5;
         (i < dwMaxDetail) && (fScale >= .001);
         i++, fScale *= fDefScaleDelta, fSize *= 2)
         fSum += Noise.Value (fx * fSize, fy * fSize) * fScale;
      fz += fSum;

#define PLATEAUSTART    .4
#define RAVINESTART     (1 - PLATEAUSTART)


      if (fRavine) {
         if (fz >= -RAVINESTART)
            fz = (fz + RAVINESTART) * 10 - RAVINESTART;
         // BUGFIX - Dont do minimum so merges better with rough land... fz = min(0, fz);
      }
      else {
         // plateu-ize
         if (fz >= PLATEAUSTART)
            fz = (fz - PLATEAUSTART) * 10 + PLATEAUSTART;
         if (fz > 1.0)
            fz = (fz - 1.0) / 100 + 1.0;
         fz = max(0, fz);  // cant go below ground level
      }

      pfCur[0] = fz;
   }
}


/*****************************************************************************
CGroundView::GroundGen - Generates a feature (such as a mountain) based
on:
   - m_memGroundGenTemp (original elevations before do ground gen)
   - m_pntButtonDown (where button was down)
   - m_pntMouseLast (last mouse location)
   - m_dwGroundGenSeed - Seed for random function

This then:
   - Allocates enough memory for the height field
   - Calls one of the GroundGenXXX functions based on the current mode
   - Scales the values according to the mouse y values
   - Applies a softening value for the edges
   - Applies the values to the original map
   - Invalidates the window

inputs
   BOOL     fDetailed - if TRUE then generates the detailed final image, else low-detail one
*/
void CGroundView::GroundGen (BOOL fDetailed)
{
   HCURSOR hOrig = SetCursor (LoadCursor (NULL, IDC_WAIT));

   // set dirty
   UndoAboutToChange ();

   // convert the points into ground pixels
   POINT pDown, pLast;
   pDown.x = (int) ((fp)m_pntButtonDown.x / m_fViewScale + m_tpViewUL.h);
   pDown.y = (int) ((fp)m_pntButtonDown.y / m_fViewScale + m_tpViewUL.v);
   pLast.x = (int) ((fp)m_pntMouseLast.x / m_fViewScale + m_tpViewUL.h);
   pLast.y = (int) ((fp)m_pntMouseLast.y / m_fViewScale + m_tpViewUL.v);

   // determine the height scale
   fp fHeight;
   fHeight = pow ((fp)2.0, -(fp)(m_pntMouseLast.y - m_pntButtonDown.y) / (fp)50);
   fHeight *= (fp) 0x4000; // start out to affecting 1/4 full scale

   // figure out the radius
   fp fx, fy;
   DWORD dwRadius;
   dwRadius = (DWORD)abs (pDown.x - pLast.x);
   dwRadius = max(dwRadius, 1);

   // copy the original appearnace back in
   memcpy (m_gState.pawElev, m_memGroundGenTemp.p, m_gState.dwWidth * m_gState.dwHeight * sizeof(WORD));

   // figure out range
   RECT rRange;
   rRange.left = pDown.x - (int)dwRadius;
   rRange.right = pDown.x + (int)dwRadius;
   rRange.top = pDown.y - (int)dwRadius;
   rRange.bottom = pDown.y + (int)dwRadius;
   rRange.left = max(rRange.left, 0);
   rRange.top = max(rRange.top, 0);
   rRange.right = min(rRange.right, (int)m_gState.dwWidth);
   rRange.bottom = min(rRange.bottom, (int)m_gState.dwHeight);
   BOOL fValidRange, fValidLast;
   fValidRange = !((rRange.right <= rRange.left) || (rRange.bottom <= rRange.top));
   if (!fValidRange)
      goto skipchange;

   // allocate enough memory for the fp's
   DWORD dwWidth, dwHeight;
   float *paf;
   dwWidth = (DWORD) (rRange.right - rRange.left);
   dwHeight = (DWORD) (rRange.bottom - rRange.top);
   if (!m_memPaintBuf.Required (dwWidth * dwHeight * sizeof(float)))
      goto skipchange;
   paf = (float*) m_memPaintBuf.p;

   // convert the last into coords for memory
   POINT pCoord;
   pCoord = pDown;
   pCoord.x -= rRange.left;
   pCoord.y -= rRange.top;

   // seed random
   srand (m_dwGroundGenSeed);

   // figur eout height at which clicked
   WORD wElevClick;
   if ((pDown.x >= 0) && (pDown.x < (int)m_gState.dwWidth) &&
      (pDown.y >= 0) && (pDown.y < (int)m_gState.dwHeight)) {
         wElevClick = m_gState.pawElev[pDown.x + pDown.y * m_gState.dwWidth];
      }
   else
      wElevClick = 0x8000;

   if (!m_dwButtonDown)
      goto skipchange;

   // if not detailed then reduce the width and height
   DWORD dwRadiusOrig, dwWidthOrig, dwHeightOrig, dwDetailScale;
   POINT pCoordOrig;
   dwWidthOrig = dwWidth;
   dwHeightOrig = dwHeight;
   dwRadiusOrig = dwRadius;
   dwDetailScale = 1;
   pCoordOrig = pCoord;
   if (!fDetailed) {
      while ((dwWidth / dwDetailScale > 50) || (dwHeight / dwDetailScale > 50))
         dwDetailScale ++;
      dwWidth /= dwDetailScale;
      dwHeight /= dwDetailScale;
      dwRadius /= dwDetailScale;
      pCoord.x /= dwDetailScale;
      pCoord.y /= dwDetailScale;

      if (!dwWidth || !dwHeight || !dwRadius)
         goto skipchange;
   }

   // call the function
   DWORD dwMergeFunc;   // 0 for adding, 1 for canyon, 2 for ravine
   dwMergeFunc = 0;
   switch (m_adwPointerMode[m_dwButtonDown-1]) {
   case IDC_GROUNDGENMTN:
      GroundGenMtn (fDetailed, 1, dwWidth, dwHeight, &pCoord, dwRadius, paf);
      break;
   case IDC_GROUNDGENVOLCANO:
      GroundGenMtn (fDetailed, 0, dwWidth, dwHeight, &pCoord, dwRadius, paf);
      break;
   case IDC_GROUNDGENHILL:
      GroundGenMtn (fDetailed, 2, dwWidth, dwHeight, &pCoord, dwRadius, paf);
      break;
   case IDC_GROUNDGENHILLROLL:
      GroundGenMtn (fDetailed, 3, dwWidth, dwHeight, &pCoord, dwRadius, paf);
      break;
   case IDC_GROUNDGENCRATER:
      GroundGenMtn (fDetailed, 4, dwWidth, dwHeight, &pCoord, dwRadius, paf);
      dwMergeFunc = 1;
      break;
   case IDC_GROUNDGENPLAT:
      GroundGenPlateau (fDetailed, 0, dwWidth, dwHeight, &pCoord, dwRadius, paf);
      dwMergeFunc = 1;
      break;
   case IDC_GROUNDGENRAVINE:
      GroundGenPlateau (fDetailed, 1, dwWidth, dwHeight, &pCoord, dwRadius, paf);
      dwMergeFunc = 2;
      break;
   case IDC_GROUNDGENRAISE:
      GroundGenRaise (fDetailed, 0, dwWidth, dwHeight, &pCoord, dwRadius, paf);
      break;
   case IDC_GROUNDGENLOWER:
      GroundGenRaise (fDetailed, 1, dwWidth, dwHeight, &pCoord, dwRadius, paf);
      break;
   case IDC_GROUNDGENFLAT:
      memset (paf, 0, dwWidth * dwHeight * sizeof(float));
      dwMergeFunc = 1;
      break;
   default:
      goto skipchange;
   }

   // scale back up
   if (dwDetailScale > 1) {
      // copy the buffer over
      CMem mem;
      if (!mem.Required (dwWidth * dwHeight * sizeof(float)))
         goto skipchange;
      float *pafSmall, fUR, fUL, fLR, fLL, fL, fR, fAlpha;
      pafSmall = (float*) mem.p;
      paf = (float*) m_memPaintBuf.p;
      memcpy (pafSmall, paf, dwWidth * dwHeight * sizeof(float));

      DWORD x,y,xx,yy;
      // BUGFIX - Was dwHeightOrig and dwWidthOrig, but had extra junk on bottom as a result
      for (y = 0; y < dwHeight * dwDetailScale; y++) for (x = 0; x < dwWidth * dwDetailScale; x++, paf++) {
         xx = x / dwDetailScale;
         xx = min(xx, dwWidth-1);
         yy = y / dwDetailScale;
         yy = min(yy, dwHeight-1);

         pafSmall = ((float*) mem.p) + (xx + yy * dwWidth);
         fUL = pafSmall[0];
         fUR = (xx+1 < dwWidth) ? pafSmall[1] : pafSmall[0];
         fLL = (yy+1 < dwHeight) ? pafSmall[dwWidth] : pafSmall[0];
         fLR = ((yy+1 < dwHeight) && (xx+1 < dwWidth)) ? pafSmall[1+dwWidth] : pafSmall[0];
            // BUGFIX - Was yy+1 < dwWidth, xx+1 < dwHeight

         // interpolate
         fAlpha = (fp) (y - yy * dwDetailScale) / (fp)dwDetailScale;
         fL = (1.0 - fAlpha) * fUL + fAlpha * fLL;
         fR = (1.0 - fAlpha) * fUR + fAlpha * fLR;
         fAlpha = (fp) (x - xx * dwDetailScale) / (fp)dwDetailScale;
         paf[0] = (1.0 - fAlpha) * fL + fAlpha * fR;
      }

      dwWidth *= dwDetailScale;
      dwHeight *= dwDetailScale;
      dwRadius = dwRadiusOrig;
      pCoord = pCoordOrig;
   }

#define MERGEIN      .66

   // apply
   DWORD x,y;
   WORD *paw;
   fp fz, f;
   paf = (float*) m_memPaintBuf.p;
   for (y = 0; y < dwHeight; y++) {
      paw = m_gState.pawElev + (rRange.left + ((int)y + rRange.top) * m_gState.dwWidth);
      fy = ((fp)y - (fp) pCoord.y) / (fp)dwRadius;

      for (x = 0; x < dwWidth; x++, paw++, paf++) {
         // figure out how much to merge in
         fx = ((fp)x - (fp) pCoord.x) / (fp)dwRadius;
         fz = fx * fx + fy * fy;
         if (fz >= 1)
            continue;   // too far away, don't affect
         fz = sqrt(fz);
         if (fz > MERGEIN)
            fz = cos((fz - MERGEIN) / (1.0 - MERGEIN) * PI / 2);
         else
            fz = 1;

         // add this in
         if (dwMergeFunc == 1) { // plateau
            // blend in
            f = fHeight * paf[0] + (fp)wElevClick;
            fz = fz * f + (1.0 - fz) * (fp)paw[0];
         }
         else if (dwMergeFunc == 2) { // ravine
            // blend in
            f = fHeight * paf[0] + (fp)wElevClick;
            f = min(f, (fp) paw[0]);   // so ravine only cuts away
            fz = fz * f + (1.0 - fz) * (fp)paw[0];
         }
         else { // add as mountain
            fz *= fHeight * paf[0];
            fz += paw[0];
         }
         fz = max(fz, 0);
         fz = min(fz, (fp)0xffff);
         paw[0] = (WORD)fz;
      } // x
   } // y

skipchange:
   RECT rInvalid;
   fValidLast = !((m_rGroundRangeLast.right <= m_rGroundRangeLast.left) || (m_rGroundRangeLast.bottom <= m_rGroundRangeLast.top));
   if (fValidLast || fValidRange) {
      // invalidate this
      if (fValidLast && !fValidRange)
         rInvalid = m_rGroundRangeLast;
      else if (!fValidLast && fValidRange)
         rInvalid = rRange;
      else
         UnionRect (&rInvalid, &m_rGroundRangeLast, &rRange);

      // convert this to screen coords
      rInvalid.left = (int) (((fp)rInvalid.left - m_tpViewUL.h - 1) * m_fViewScale);
      rInvalid.right = (int) (((fp)rInvalid.right - m_tpViewUL.h + 1) * m_fViewScale);
      rInvalid.top = (int) (((fp)rInvalid.top - m_tpViewUL.v - 1) * m_fViewScale);
      rInvalid.bottom = (int) (((fp)rInvalid.bottom - m_tpViewUL.v + 1) * m_fViewScale);

      InvalidateRect (m_hWndMap, &rInvalid, FALSE);
   }

   m_rGroundRangeLast = rRange;
   SetCursor (hOrig);
}


/*****************************************************************************
CGroundView::ApplyRain - Simulates one rain drop.
*/
void CGroundView::ApplyRain (void)
{
   // location
   int x, y, xOld, yOld;
   fp fLastDrop;
   x = rand() % m_gState.dwWidth;
   y = rand() % m_gState.dwHeight;
   xOld = yOld = -1;
   fLastDrop = 0;

   // default momentum
   //CPoint   pMoment;
   CPoint   pCurLoc;
   //pMoment.Zero();
   //pMoment.p[0] = randf(-m_gState.fScale/4, m_gState.fScale/4);
   //pMoment.p[1] = randf(-m_gState.fScale/4, m_gState.fScale/4);
   Elevation (x, y, &pCurLoc);

   // repeat
   DWORD i;
   DWORD dwCount;
   int xx, yy;
   fp fMax, fMin;
   fMax = -ZINFINITE;
   fMin = ZINFINITE;
   fp fDirtPickedUp;
   fDirtPickedUp = 0;
   // make sure dont do more than 10 pixels at a time
   BOOL fOffScreen;
#define COUNTMAX     50
   for (dwCount = 0; ((dwCount < COUNTMAX) && (x >= 0) && (x < (int) m_gState.dwWidth) && (y >= 0) && (y < (int) m_gState.dwWidth)); dwCount++) {
      // see which direction can potentially go
      CPoint apLoc[8];
      fp afEnergy[8];
      int aiX[8], aiY[8];
      CPoint pDir;
      for (i = 0; i < 8; i++) {
         afEnergy[i] = ZINFINITE;   // just in case  cant go there
         xx = x;
         yy = y;
         if (i <= 2)
            xx--;
         else if ((i >= 4) && (i <=6))
            xx++;
         if ((i >= 2) && (i <= 4))
            yy++;
         else if ((i == 0) || (i >= 6))
            yy--;
         aiX[i] = xx;
         aiY[i] = yy;

         // can't backtrack
         if ((xx == xOld) && (yy = yOld))
            continue;

         if ((xx < 0) || (yy < 0) || (xx >= (int)m_gState.dwWidth) || (yy >= (int)m_gState.dwWidth)) {
            afEnergy[i] = -ZINFINITE;  // encourage to head off the screen
            continue;   // off screen
         }

         // get the location
         Elevation ((DWORD)xx, (DWORD)yy, &apLoc[i]);

         // how much energy to get there?
         // what's the delta
         pDir.Subtract (&apLoc[i], &pCurLoc);

         // take into account the change in z
         afEnergy[i] = pDir.p[2];
         pDir.p[2] = 0;

         // do that isn't so keen to go at diagonals, affect energy
         afEnergy[i] /= pDir.Length();

         fMin = min(fMin, afEnergy[i]);
         fMax = max(fMax, afEnergy[i]);
         
         // include friction
         //afEnergy[i] += pDir.Length() / 20;  // water will flow on a 1 in 20 fall on ground

         // takes energy to change directions
         //pDir.Normalize();
         //afEnergy[i] -= pDir.DotProd (&pMoment); // faster we go, the harder to change directions
      } // i

      // add a random value to each based on the greatest change between the energies
      // so can vary a bit in direction
      if (fMin != ZINFINITE) {
         fMax -= fMin;
         fMax /= 10;
         for (i = 0; i < 8; i++)
            afEnergy[i] += randf(-fMax, fMax);
      }

      // figure out which is the best
      DWORD dwBest;
      dwBest = 0;
      for (i = 1; i < 8; i++)
         if (afEnergy[i] < afEnergy[dwBest])
            dwBest = i;
      fOffScreen = (afEnergy[dwBest] < -ZINFINITE/10.0);
         // BUGFIX - Was check for == -ZINFINITE but not exact match

      // if going up then dont do
      BOOL fDropDirt;
      fDropDirt = (afEnergy[dwBest] > 0);
      if (fDropDirt)
         break;
      // how much energy do we have
      //fp fHave;
      //pMoment.p[2] = 0; // so dont include
      //fHave = pMoment.Length();

      //if (fHave < afEnergy[dwBest])
      //   break;   // cant go there because not enough energy


      // if need to drop dirty then figure out how far need to cast net before ground level
      // has mimumum
      WORD *pw;

      // dig in
      fp fStrength, f;
      fp fDist;
      fStrength = fLastDrop / (m_gState.tpElev.v - m_gState.tpElev.h) * 1000000;
      fStrength = sqrt(fStrength);  // so that steep drops dont gouge too much
      
      // remember the last drop before do changes
      if (!fOffScreen) {
         Elevation ((DWORD)aiX[dwBest], (DWORD) aiY[dwBest], &pDir);
         fLastDrop = max(pCurLoc.p[2] - pDir.p[2], 0);
      }

#define RAINANTI     2
      for (yy = max(y-RAINANTI,0); yy < min(y+RAINANTI+1, (int)m_gState.dwHeight); yy++)
         for (xx = max(x-RAINANTI,0); xx < min(x+RAINANTI+1, (int)m_gState.dwWidth); xx++) {
            fDist = ((xx - x) * (xx - x) + (yy - y) * (yy - y));
            if (fDist > RAINANTI * 2)
               continue;   // dont affect
            
            pw = m_gState.pawElev + (xx + yy * m_gState.dwWidth);
            f = fStrength / (fp) (fDist + 1);
            fDirtPickedUp += f;
            f = (fp) pw[0] - f;
            f = min((fp)0xffff,f);
            f = max(0,f);
            pw[0] = (WORD)f;
         } // xx yy


      // if off screen then exit here
      if (fOffScreen)
         break;

      // go in that direction
      Elevation ((DWORD)aiX[dwBest], (DWORD) aiY[dwBest], &pDir);
      //pMoment.Subtract (&pDir, &pCurLoc);
      //pMoment.p[2] = 0; // so dont include downward
      //pMoment.Normalize();
      //fHave -= afEnergy[dwBest];
      // fHave /= 2; // hack - keep reducing
      //pMoment.Scale (fHave);  // scale by the amount of energy that have
      pCurLoc.Copy (&pDir);   // move there
      xOld = x;
      yOld = y;
      x = aiX[dwBest];
      y = aiY[dwBest];
   } // whike

   // deposit dirt
#define DEPOSITSIZE     50
#if 0 // BUGFIX - Take out deposits because the math to actually deposit them correctly
   // gets very cumbersome and slow since need to worry about flow of the dirt out, etc.
   fOffScreen = !((x < (int) m_gState.dwWidth) && (y >= 0) && (y < (int) m_gState.dwWidth));
   if (!fOffScreen && fDirtPickedUp && (dwCount < COUNTMAX)) {
moredirt:
      WORD wTest, wVal, wMin, wSecondMin;
      WORD wLastMin;
      wLastMin = wTest = m_gState.pawElev[x + y * m_gState.dwWidth];
      wMin = wSecondMin = 0xffff;
      int i;
      for (i = 1; i < DEPOSITSIZE; i++) {
         for (yy = max(y-i,0); yy < min(y+i+1, (int)m_gState.dwHeight); yy++)
            for (xx = max(x-i,0); xx < min(x+i+1, (int)m_gState.dwWidth); xx++) {
               wVal = m_gState.pawElev[xx + yy * m_gState.dwWidth];
               if (wVal < wMin) {
                  wSecondMin = wMin;
                  wMin = wVal;
               }
               else if ((wVal > wMin) && (wVal < wSecondMin))
                  wSecondMin = wVal;
            }  // xx and yy

         // if the min and the max are the same then continue
         if (wMin == wSecondMin)
            continue;

         // else, break;
         break;
      }
      // if wanting to fill up really large region but still cant then just give up
      if (i >= DEPOSITSIZE)
         return;

      // now that how far out go before no longer level, need to go further out until
      // we have determined we're in a bowl
      // NOTE: This has not been done

      // if the new min is less than the last min then overflowed out of a bowl.
      // since math is too ugly, just ignore
      if (wMin < wLastMin)
         return;

      // figure out how many points are == wMin
      DWORD dwCount;
      dwCount = 0;
      for (yy = max(y-i,0); yy < min(y+i+1, (int)m_gState.dwHeight); yy++)
         for (xx = max(x-i,0); xx < min(x+i+1, (int)m_gState.dwWidth); xx++)
            if (wMin == m_gState.pawElev[xx + yy * m_gState.dwWidth])
               dwCount++;

      // how much dirt is this
      fp fUse, f;
      fUse = (fp)(wSecondMin - wMin) * (fp)dwCount;
      fUse = min(fUse, fDirtPickedUp);
      f = (fp) wMin + fUse / (fp)dwCount;
      f = max(0,f);
      f = min((fp)0xffff,f);
      wSecondMin = (WORD) f;

      // distribute the dirt
      for (yy = max(y-i,0); yy < min(y+i+1, (int)m_gState.dwHeight); yy++)
         for (xx = max(x-i,0); xx < min(x+i+1, (int)m_gState.dwWidth); xx++)
            if (wMin == m_gState.pawElev[xx + yy * m_gState.dwWidth])
               m_gState.pawElev[xx + yy * m_gState.dwWidth] = wSecondMin; // fill in

      // use up the dirty
      fDirtPickedUp -= fUse;
      if (fDirtPickedUp > CLOSE)
         goto moredirt;
   } // fDropDirt
#endif //0
}


/*****************************************************************************
CGroundView::TracePixelToWorldSpace - Function supplied for the tracing paper
functions to call. Takes and xy offset into the m_hWndMap and returns their location
in ground pixels into p.p[0] and p.p[1]

inputs
   fp       dwX, dwY - X and Y in screen pixels (m_hWndMap client coords)
   PCPoint  p - p.p[0] and p.p[1] filled with ground meter coords, rest is zeroed
*/
void CGroundView::TracePixelToWorldSpace (fp dwX, fp dwY, PCPoint p)
{
   p->Zero();
   p->p[0] = (dwX / m_fViewScale + m_tpViewUL.h) * m_gState.fScale;
   p->p[1] = (dwY / m_fViewScale + m_tpViewUL.v) * m_gState.fScale;
}

/*****************************************************************************
CGroundView::TraceWorldSpaceToPixel - FUnction supplied for the tracing paper
functions to call. Takes pWOrld->p[0] and pWOrld->p[1] as ground pixels (in meters), and
converts this into screen pixels (m_hWndMap)

inputs
   PCWorld     pWorld - Ground coords (but in meters)
   fp          *pfX, *pfY - Filled in the screeen coords
reutrns
   none
*/
void CGroundView::TraceWorldSpaceToPixel (PCPoint pWorld, fp *pfX, fp *pfY)
{
   // Need to include offset for image that currently rendering
   *pfX = (pWorld->p[0] / m_gState.fScale - m_tpViewUL.h) * m_fViewScale - (fp)m_pTraceOffset.x;
   *pfY = (pWorld->p[1] / m_gState.fScale - m_tpViewUL.v) * m_fViewScale - (fp)m_pTraceOffset.y;
}

/**********************************************************************************
CGroundView::UpdateTextList - Update the list box for all the textures.
*/
void CGroundView::UpdateTextList (void)
{
   // if the current texture is >= # textures then set it
   if (m_dwCurText >= m_gState.plTextSurf->Num())
      m_dwCurText = 0;

   // clear the list box
   SendMessage (m_hWndTextList, LB_RESETCONTENT, 0,0);

   // loop through all the surfaces and come up with a name
   DWORD i;
   PCObjectSurface *ppos;
   PCObjectSurface pos;
   ppos = (PCObjectSurface*) m_gState.plTextSurf->Get(0);
   for (i = 0; i < m_gState.plTextSurf->Num(); i++) {
      char szTemp[256];
      szTemp[0] = 0;
      pos = ppos[i];
      if (pos->m_fUseTextureMap) {
         WCHAR szMajor[128], szMinor[128], szName[128];
         szName[0] = 0;
         TextureNameFromGUIDs (DEFAULTRENDERSHARD, &pos->m_gTextureCode, &pos->m_gTextureSub, szMajor, szMinor, szName);
         if (szName[0])
            WideCharToMultiByte (CP_ACP, 0, szName, -1, szTemp, sizeof(szTemp), 0, 0);
      }
      if (!szTemp[0])
         strcpy (szTemp, "Not named");

      SendMessage (m_hWndTextList, LB_ADDSTRING, 0, (LPARAM) szTemp);
   }

   // add in a final option
   SendMessage (m_hWndTextList, LB_ADDSTRING, 0, (LPARAM) "Double-click to add a new texture...");

   // set selection
   SendMessage (m_hWndTextList, LB_SETCURSEL, (WPARAM) m_dwCurText, 0);
}

/**********************************************************************************
CGroundView::UpdateForestList - Update the list box for all the forests.
*/
void CGroundView::UpdateForestList (void)
{
   // if the current texture is >= # textures then set it
   if (m_dwCurForest >= m_gState.plPCForest->Num())
      m_dwCurForest = 0;

   // clear the list box
   SendMessage (m_hWndForestList, LB_RESETCONTENT, 0,0);

   // loop through all the surfaces and come up with a name
   DWORD i;
   PCForest *ppf, pf;
   ppf = (PCForest*) m_gState.plPCForest->Get(0);
   for (i = 0; i < m_gState.plPCForest->Num(); i++) {
      char szTemp[256];
      szTemp[0] = 0;
      pf = ppf[i];
      WideCharToMultiByte (CP_ACP, 0, pf->m_szName, -1, szTemp, sizeof(szTemp), 0, 0);
      if (!szTemp[0])
         strcpy (szTemp, "Not named");

      SendMessage (m_hWndForestList, LB_ADDSTRING, 0, (LPARAM) szTemp);
   }

   // add in a final option
   SendMessage (m_hWndForestList, LB_ADDSTRING, 0, (LPARAM) "Double-click to add a new forest...");

   // set selection
   SendMessage (m_hWndForestList, LB_SETCURSEL, (WPARAM) m_dwCurForest, 0);
}


/**********************************************************************************
CGroundView::IsPixelPainted - Returns TRUE if a pixel is to be painted by the
GroundPaint.mml dialog. Uses the m_tpPaintElev and m_tpPaintSlope variables.

inputs
   DWORD          dwX, dwY - X and Y, from 0..m_dwWidth-1, 0..m_dwHeight-1
returns
   fp - Amount that it's painted by, from 0 to 1.
*/
fp CGroundView::IsPixelPainted (DWORD dwX, DWORD dwY)
{
   // get the elevations of this and surrounding pixels
   WORD *pwCenter;
   WORD wL, wR, wU, wD, wC;
   pwCenter = &m_gState.pawElev[dwX + dwY * m_gState.dwWidth];
   wC = pwCenter[0];
   wR = (dwX+1 < m_gState.dwWidth) ? pwCenter[1] : pwCenter[0];
   wL = dwX ? pwCenter[-1] : pwCenter[0];
   wD = (dwY+1 < m_gState.dwHeight) ? pwCenter[m_gState.dwWidth] : pwCenter[0];
   wU = dwY ? pwCenter[-(int)m_gState.dwWidth] : pwCenter[0];

   // convert this to some vectors
   CPoint pLR, pDU, pN;
   pLR.Zero();
   pLR.p[0] = m_gState.fScale*2.0;
   pLR.p[2] = ((fp) wR - (fp)wL) / (fp)0xffff * (m_gState.tpElev.v - m_gState.tpElev.h);
   pDU.Zero();
   pDU.p[1] = m_gState.fScale*2.0;
   pDU.p[2] = ((fp) wD - (fp)wU) / (fp)0xffff * (m_gState.tpElev.v - m_gState.tpElev.h);
   pN.CrossProd (&pLR, &pDU);
   pN.Normalize();

   // what are the height and slope
   fp fElev, fSlope;
   fElev = (fp)wC / (fp)0xffff;
   fSlope = 1.0 - pN.p[2];

   // limits
   fp fLimitElev, fLimitSlope, fLimitSum;
   fLimitElev = max(m_tpPaintElev.v, CLOSE);
   fLimitSlope = max(m_tpPaintSlope.v, CLOSE);
   fLimitSum = 1.0 / fLimitElev + 1.0 / fLimitSlope;

   // what's the score
   fp fScore;
   fScore = (m_tpPaintElev.v - fabs(fElev - m_tpPaintElev.h)) / fLimitElev / fLimitElev / fLimitSum;
   fScore += (m_tpPaintSlope.v - fabs(fSlope - m_tpPaintSlope.h)) /fLimitSlope / fLimitSlope / fLimitSum;
   fScore *= m_fPaintAmount * 2.0;
   fScore = max(fScore, 0);
   fScore = min(fScore, 1);

   return fScore;
}


