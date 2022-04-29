/***************************************************************************
CSceneView - C++ object for viewing the scene data.

begun 17/1/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <zmouse.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#define VPOSMAX            100      // maximum vertical position value
#define TIMER_AUTOSCROLL   108      // so will scroll when beyond edge
#define TIMER_SYNCTOWORLD  109      // automatic timer so that will keep world in sync with changes that have just made
#define TIMER_PLAYING      110      // timer that goes off when playing

#define IDC_LIST           1028     // notification from list box

// buttons
#define  IDC_SCENEFPS      1000
#define  IDC_SCENEBAYS     1001
#define  IDC_SCENEMENU     1002
#define  IDC_SCENEBOOKMARK 1003
#define  IDC_SCENECAMERA   1004
#define  IDC_HELPBUTTON    1010

#define  IDC_CUTBUTTON     2001
#define  IDC_COPYBUTTON    2002
#define  IDC_PASTEBUTTON   2003
#define  IDC_UNDOBUTTON    2004
#define  IDC_REDOBUTTON    2005
#define  IDC_DELETEBUTTON  2006

#define  IDC_ANIMPLAY      3000
#define  IDC_ANIMSTOP      3001
#define  IDC_ANIMPLAYLOOP  3002
#define  IDC_ANIMJUMPSTART 3003
#define  IDC_ANIMJUMPEND   3004
#define  IDC_ZOOMIN        3006
#define  IDC_ANIMJUMPNEXT  3007
#define  IDC_ANIMJUMPPREV  3008
#define  IDC_ANIMPLAYAUDIO 3009

#define  IDC_NEWANIMKEYFRAME 4000
#define  IDC_NEWANIMPATH   4001
#define  IDC_NEWANIMKEYFRAMELOC 4002
#define  IDC_NEWANIMKEYFRAMEROT 4003
#define  IDC_NEWANIMBOOKMARK1 4004
#define  IDC_NEWANIMBOOKMARK2 4005
#define  IDC_NEWANIMCAMERA 4006
#define  IDC_NEWANIMWAVE   4007

#define  IDC_SELINDIVIDUAL 5000
#define  IDC_SELREGION     5001
#define  IDC_SELALL        5002
#define  IDC_SELNONE       5004
#define  IDC_ANIMTIMEINSERT 5005
#define  IDC_ANIMTIMEREMOVE 5006
#define  IDC_ANIMSELACTIVE 5007

#define  IDC_ANIMMOVELR    6000
#define  IDC_ANIMMOVEUD    6001
#define  IDC_ANIMRESIZE    6002
#define  IDC_OBJDIALOG     6003
#define  IDC_GRAPHPOINTMOVE 6004
#define  IDC_GRAPHPOINTDELETE 6005
#define  IDC_ANIMDEFAULT   6006
#define  IDC_GRAPHPOINTMOVEUD 6007


#define  IDC_POSITIONPASTE 7000  // so user can say where wants object pasted
#define  IDC_OBJECTPASTE   7001  // so user can paste object
#define  IDC_OBJECTPASTEDRAG 7002   // drag

#define  IDC_OBJDECONSTRUCT 8007
#define  IDC_OBJMERGE      8014

#define  IDC_MYSCROLLBAR   8000

#define  SCROLLSIZE     16
#define  SMALLSCROLLSIZE (SCROLLSIZE / 2)
#define  VARBUTTONSIZE  (m_fSmallWindow ? M3DSMALLBUTTONSIZE : M3DBUTTONSIZE)

/* globals */
static BOOL gfListPCSceneViewInit = FALSE;
CListFixed gListPCSceneView;
static COLORREF gacGraph[] = {
   RGB(0xff,0,0), RGB(0,0xff,0), RGB(0,0,0xff),
   RGB(0xd0,0xd0,0), RGB(0,0xff,0xff), RGB(0xff,0,0xff),
   RGB(0x80,0x80,0x80),
   RGB(0x80,0,0), RGB(0,0x80,0), RGB(0,0,0x80),
   RGB(0x80,0x80,0), RGB(0,0x80,0x80), RGB(0x80,0,0x80),
   RGB(0xc0,0xc0,0xc0),
   };


/*******************************************************************************************
ListPCSceneView - Returns a pointer to the gListPCSceneView object
*/
PCListFixed ListPCSceneView (void)
{
   return &gListPCSceneView;
}

/*******************************************************************************************
SceneViewCreate - Creates the scene view if it doesn't exist. Else, restores it
*/
void SceneViewCreate (PCWorldSocket pWorld, PCSceneSet pSceneSet)
{
   DWORD i;
   for (i = 0; i < gListPCSceneView.Num(); i++) {
      PCSceneView pv = *((PCSceneView*) gListPCSceneView.Get(i));
      if (pv->m_pWorld == pWorld) {
         ShowWindow (pv->m_hWnd, SW_RESTORE);
         SetForegroundWindow (pv->m_hWnd);
         return;
      }
   }

   // make sure there's a scene to look at
   PCScene ps;
   if (!pSceneSet->SceneNum()) {
      DWORD dwNew = pSceneSet->SceneNew ();
      if (dwNew == -1)
         return;  // errror
      ps = pSceneSet->SceneGet (dwNew);
      if (!ps)
         return;  // error
      ps->NameSet (L"New scene");
   }
   // if there's no scene current then get the first scene and use that
   fp fTime;
   pSceneSet->StateGet (&ps, &fTime);
   if (!ps) {
      ps = pSceneSet->SceneGet (0);
      if (!ps)
         return;  // error
      pSceneSet->StateSet (ps, 0);
   }

   // else create it
   PCSceneView pv;
   pv = new CSceneView;
   if (!pv)
      return;
   if (!pv->SceneSet (pWorld, pSceneSet)) {
      delete pv;
      return;
   }

   // done
}

/*******************************************************************************************
SceneViewShutDown - Called to shut down all the scene views
*/
void SceneViewShutDown (void)
{
   while (gListPCSceneView.Num()) {
      PCSceneView pv = *((PCSceneView*) gListPCSceneView.Get(0));
      delete pv;
   }
}


/*******************************************************************************************
CSceneView::Constructor and destructor */
CSceneView::CSceneView (void)
{
   m_hWnd = m_hWndScroll = NULL;
   m_pTimeline = NULL;
   m_pPurgatory = new CScene;
   m_fSelDraw = FALSE;
   m_pPasteDrag = NULL;
   m_fMCP = FALSE;

   m_dwPlaying = 0;

   m_fViewTimeStart = 0;
   m_fViewTimeEnd = 60;
   m_lSVBAY.Init (sizeof(SVBAY));
   m_pbarTop = m_pbarBottom = NULL;
   m_hWndClipNext = NULL;
   m_dwButtonDown = 0;
   m_fCaptured = FALSE;
   m_pntButtonDown.x = m_pntButtonDown.y = 0;
   m_pntMouseLast = m_pntButtonDown;
   m_pButtonDown.Zero();
   m_pMouseLast.Zero();
   memset (m_adwPointerMode, 0 ,sizeof(m_adwPointerMode));

   // greyish
   m_cBackDark = RGB(0x80,0x80,0x80);
   m_cBackMed = RGB(0xa0,0xa0,0xc0);
   m_cBackLight = RGB(0xc0,0xc0,0xff);
   m_cBackOutline = RGB(0x40, 0x40, 0x60);

   m_fSmallWindow = FALSE;
   m_pTimerAutoScroll = NULL;

   // register clibboard format
   m_dwClipFormat = RegisterClipboardFormat (APPLONGNAME " scene data");

   LOGFONT lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -10;   // 10 pixels high MulDiv(iPointSize, EscGetDeviceCaps(hDC, LOGPIXELSY), 72); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   lf.lfWeight = FW_BOLD;
   m_hFont = CreateFontIndirect (&lf);

   m_pUndo = m_pRedo = m_pPaste = NULL;
   m_pPlay = m_pStop = m_pPlayLoop = NULL;
   m_pWorld = NULL;
   m_pSceneSet = NULL;
   m_listNeedSelect.Init (sizeof(PCIconButton));


   // Add this to the list of views around
   if (!gfListPCSceneViewInit) { 
      gListPCSceneView.Init (sizeof(PCSceneView));
      gfListPCSceneViewInit = TRUE;
   }
   PCSceneView pv;
   pv = this;
   gListPCSceneView.Add (&pv);

   // register the class
   // register the window proc if it isn't alreay registered
   static BOOL fIsRegistered = FALSE;
   if (!fIsRegistered) {
      WNDCLASS wc;
      memset (&wc, 0, sizeof(wc));
      wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_ANIMICON));
      wc.lpfnWndProc = CSceneViewWndProc;
      wc.style = CS_HREDRAW | CS_VREDRAW;
      wc.hInstance = ghInstance;
      wc.hbrBackground = NULL; //(HBRUSH)(COLOR_BTNFACE+1);
      wc.lpszClassName = "CSceneView";
      wc.hCursor = LoadCursor (NULL, IDC_NO);
      RegisterClass (&wc);

      // might as well register for the bays
      wc.hIcon = NULL;
      wc.hCursor = NULL;
      wc.lpfnWndProc = CSceneViewBayInfoWndProc;
      wc.lpszClassName = "CSceneViewBayInfo";
      RegisterClass (&wc);
      wc.lpfnWndProc = CSceneViewBayDataWndProc;
      wc.lpszClassName = "CSceneViewBayData";
      RegisterClass (&wc);

      fIsRegistered = TRUE;
   }

   RECT rLoc;
   int iWidth, iHeight;
   iWidth = GetSystemMetrics (SM_CXSCREEN);
   iHeight = GetSystemMetrics (SM_CYSCREEN);
   rLoc.left = iWidth / 32;
   rLoc.right = iWidth / 32 * 31;
   rLoc.top  = iHeight * 2 / 3;
   rLoc.bottom = iHeight / 16 * 15;

   // createa the window
   m_hWnd = CreateWindowEx (
      WS_EX_APPWINDOW
#ifndef _DEBUG
      | WS_EX_TOPMOST
#endif
      , "CSceneView", "",
      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      rLoc.left, rLoc.top, rLoc.right - rLoc.left, rLoc.bottom - rLoc.top,
      NULL, NULL, ghInstance, (LPVOID) this);
   ShowWindow (m_hWnd, SW_SHOWNORMAL);
   SetTitle ();

   m_hWaveOut = NULL;
   m_pAnimWave = NULL;
   m_pObjWave = NULL;
   m_dwWaveSamplesPerSec = m_dwWaveChannels = 0;
   m_dwPlayBufSize = 0;
   m_pafPlayTemp = NULL;
   m_fPlayCur = 0;
}



CSceneView::~CSceneView (void)
{
   if (m_hWaveOut) {
      waveOutClose (m_hWaveOut);
      m_hWaveOut = NULL;
   }

   if (m_pTimeline)
      delete m_pTimeline;
   m_pTimeline = NULL;

   if (m_hFont)
      DeleteObject (m_hFont);

   if (m_pTimerAutoScroll)
      KillTimer (m_hWnd, TIMER_AUTOSCROLL);

   if (m_dwPlaying)
      KillTimer (m_hWnd, TIMER_PLAYING);
   m_dwPlaying = 0;

   if (m_pPurgatory)
      delete m_pPurgatory;

   // release capture
   if (m_fCaptured)
      ReleaseCapture ();

   // remove all the bays
   UpdateNumBays (0, FALSE);

   // unregister with world socket
   if (m_pWorld)
      m_pWorld->NotifySocketRemove (this);
   if (m_pSceneSet)
      m_pSceneSet->NotifySocketRemove (this);

   // delete the bars
   if (m_pbarTop)
      delete m_pbarTop;
   if (m_pbarBottom)
      delete m_pbarBottom;

   if (m_hWnd)
      DestroyWindow (m_hWnd);

   // remove self from list of scene views
   DWORD i;
   PCSceneView ph;
   for (i = 0; i < gListPCSceneView.Num(); i++) {
      ph = *((PCSceneView*)gListPCSceneView.Get(i));
      if (ph == this) {
         gListPCSceneView.Remove(i);
         break;
      }
   }
}


/*******************************************************************************************
CSceneViewBayInfoWndProc - Window proc. Just calls into object
*/
LRESULT CALLBACK CSceneViewBayInfoWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCSceneView p = (PCSceneView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCSceneView p = (PCSceneView) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif
   PSVBAY pb = NULL;

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCSceneView) lpcs->lpCreateParams;
         // p = (PCSceneView) GetWindowLong (hWnd, GWL_USERDATA);
      }
      break;
   };

   if (p) {
      DWORD i, dwNum;
      PSVBAY pab = (PSVBAY) p->m_lSVBAY.Get(0);
      dwNum = p->m_lSVBAY.Num();
      for (i = 0; i < dwNum; i++, pab++)
         if (pab->hWndInfo == hWnd) {
            pb = pab;
            break;
         }
   }

   if (p)
      return p->BayInfoWndProc (pb, hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*******************************************************************************************
CSceneViewBayDataWndProc - Window proc. Just calls into object
*/
LRESULT CALLBACK CSceneViewBayDataWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCSceneView p = (PCSceneView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCSceneView p = (PCSceneView) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif
   PSVBAY pb = NULL;

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCSceneView) lpcs->lpCreateParams;
         // p = (PCSceneView) GetWindowLong (hWnd, GWL_USERDATA);
      }
      break;
   };

   if (p) {
      DWORD i, dwNum;
      PSVBAY pab = (PSVBAY) p->m_lSVBAY.Get(0);
      dwNum = p->m_lSVBAY.Num();
      for (i = 0; i < dwNum; i++, pab++)
         if (pab->hWndData == hWnd) {
            pb = pab;
            break;
         }
   }

   if (p)
      return p->BayDataWndProc (pb, hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/*******************************************************************************************
CSceneViewWndProc - Window proc. Just calls into object
*/
LRESULT CALLBACK CSceneViewWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCSceneView p = (PCSceneView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCSceneView p = (PCSceneView) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCSceneView) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*******************************************************************************************
CSceneView::SceneSet - Call this to specify the CScene that the view should use, along
with the world.

inputs
   PCWorldSocket        pWorld - World to use
   PCScene              pScene - Scene to use (in the world)
retursn
   BOOL - TRUE if success
*/
BOOL CSceneView::SceneSet (PCWorldSocket pWorld, PCSceneSet pSceneSet)
{
   // free up exsiting bits
   if (m_pWorld)
      m_pWorld->NotifySocketRemove (this);
   if (m_pSceneSet)
      m_pSceneSet->NotifySocketRemove (this);
   m_pWorld = NULL;
   m_pSceneSet = NULL;

   // free up info in the bays
   DWORD dwNum;
   dwNum = m_lSVBAY.Num();
   UpdateNumBays (0,FALSE);
   UpdateNumBays (dwNum ? dwNum : 1, TRUE);  // always at least some bays


   // use new ones
   m_pWorld = pWorld;
   m_pSceneSet = pSceneSet;
   m_pWorld->NotifySocketAdd (this);
   m_pSceneSet->NotifySocketAdd (this);


   // default timeline
   m_fViewTimeStart = 0;
   m_fViewTimeEnd = 60; // always start out with 1 min

   // Call function to set up info in the bays
   // Fill with current selection?
   ModifyBaysFromSelection ();

   // Set the title
   SetTitle ();

   // Enable/disable m_pListNeedSelect buttons
   WorldChanged (0, NULL);
   BOOL fUndo, fRedo;
   
   // enable/disable undo/redo buttons
   fUndo = m_pSceneSet->UndoQuery (&fRedo);
   SceneUndoChanged (fUndo, fRedo);
   SceneChanged (0, NULL, NULL, NULL); // so enable/disable buttons properly


   // update scrollbar
   NewScrollLoc ();

   // update the slider
   fp fTime;
   PCScene pScene;
   m_pSceneSet->StateGet (&pScene, &fTime);
   if (m_pTimeline) {
      m_pTimeline->PointerSet (fTime);
      if (pScene)
         m_pTimeline->LimitsSet (0, pScene->DurationGet(), pScene->DurationGet());
   }

   return TRUE;
}

/*******************************************************************************************
CSceneView::SetTitle - Sets the window title based on the scene
*/
void CSceneView::SetTitle (void)
{
   char szTemp[256];

   PCScene pScene = NULL;
   if (m_pSceneSet)
      m_pSceneSet->StateGet (&pScene, NULL);

   if (pScene && pScene->NameGet())
      WideCharToMultiByte (CP_ACP, 0, pScene->NameGet(), -1, szTemp, sizeof(szTemp), 0,0);
   else
      strcpy (szTemp, "Unknown");

   strcat (szTemp, " - 3DOB Animation timeline");
   SetWindowText (m_hWnd, szTemp);
}

/*******************************************************************************************
CSceneView::SceneChanged - Called by m_pSceneSet to indicate that it has changed.
*/
void CSceneView::SceneChanged (DWORD dwChanged, GUID *pgScene, GUID *pgObjectWorld, GUID *pgObjectAnim)
{
   BOOL fEnable = FALSE;
   DWORD i;
   PCIconButton *ppb;

   // see what current selection is
   DWORD dwNum;
   PSCENESEL pss;
   pss = m_pSceneSet->SelectionEnum (&dwNum);
   fEnable = (dwNum ? TRUE : FALSE);
   ppb = (PCIconButton*) m_listNeedSelect.Get(0);
   for (i = 0; i < m_listNeedSelect.Num(); i++) {
      ppb[i]->Enable (fEnable);
   }


   // refresh bays?
   PCScene pScene;
   GUID g;
   m_pSceneSet->StateGet (&pScene, NULL);
   memset (&g, 0, sizeof(g));
   if (pScene)
      pScene->GUIDGet (&g);
   if (!pgScene || IsEqualGUID (g, *pgScene)) {
      PSVBAY pb;
      pb = (PSVBAY) m_lSVBAY.Get(0);
      for (i = 0; i < m_lSVBAY.Num(); i++, pb++) {
         if (!pgObjectWorld || IsEqualGUID (pb->gWorldObject, *pgObjectWorld)) {
            // obhect has changed
            InvalidateRect (pb->hWndData, NULL, FALSE);
            InvalidateRect (pb->hWndInfo, NULL, FALSE);
         }
      }
   }

}

/*******************************************************************************************
CSceneView::SceeneUndoChanged - Called by m_pWorld to indicate that the undo has changed
*/
void CSceneView::SceneUndoChanged (BOOL fUndo, BOOL fRedo)
{
   if (m_pUndo)
      m_pUndo->Enable (fUndo);
   if (m_pRedo)
      m_pRedo->Enable (fRedo);
}

/*******************************************************************************************
CSceneView::SceneStateChanged - Called by m_pWorld when the scenes state has changed
*/
void CSceneView::SceneStateChanged (BOOL fScene, BOOL fTime)
{
   if (fScene) {
      // free up info in the bays
      DWORD dwNum;
      dwNum = m_lSVBAY.Num();
      UpdateNumBays (0,FALSE);
      UpdateNumBays (dwNum ? dwNum : 1, TRUE);  // always at least some bays

      ModifyBaysFromSelection ();

      // default timeline
      m_fViewTimeStart = 0;
      m_fViewTimeEnd = 60; // always start out with 1 min

      // Enable/disable m_pListNeedSelect buttons
      WorldChanged (0, NULL);
      BOOL fUndo, fRedo;
      
      // enable/disable undo/redo buttons
      fUndo = m_pSceneSet->UndoQuery (&fRedo);
      SceneUndoChanged (fUndo, fRedo);
      SceneChanged (0, NULL, NULL, NULL); // so enable/disable buttons properly


      // update scrollbar
      NewScrollLoc ();

      PCScene pScene;
      m_pSceneSet->StateGet (&pScene, NULL);
      if (m_pTimeline && pScene)
         m_pTimeline->LimitsSet (0, pScene->DurationGet(), pScene->DurationGet());

      // update the title
      SetTitle ();
   }

   if (fTime) {
      // update the slider
      fp fTime;
      m_pSceneSet->StateGet (NULL, &fTime);
      if (m_pTimeline)
         m_pTimeline->PointerSet (fTime);

      InvalidateBayData();
   }
}


/******************************************************************************
CSceneView::WorldAboutToChange - Callback from world
*/
void CSceneView::WorldAboutToChange (GUID *pgObject)
{
   // do nothing
}

/*******************************************************************************************
CSceneView::WorldChanged - Called by m_pWorld to indicate that it has changed.
*/
void CSceneView::WorldChanged (DWORD dwChanged, GUID *pgObject)
{
   // if selection has changed update that
   if (dwChanged & (WORLDC_SELADD | WORLDC_SELREMOVE))
      ModifyBaysFromSelection ();
}

/*******************************************************************************************
CSceneView::WorldUndoChanged - Called by m_pWorld to indicate that the undo has changed
*/
void CSceneView::WorldUndoChanged (BOOL fUndo, BOOL fRedo)
{
   // do nothing
}

int _cdecl OSMBOOKMARKCompare (const void *elem1, const void *elem2)
{
   OSMBOOKMARK *pdw1, *pdw2;
   pdw1 = (OSMBOOKMARK*) elem1;
   pdw2 = (OSMBOOKMARK*) elem2;

   return _wcsicmp(pdw1->szName, pdw2->szName);
}

/*******************************************************************************************
CSceneView::WndProc - Handle the window procedure for the view
*/
LRESULT CSceneView::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (uMsg == gdwMouseWheel)
      uMsg = WM_MOUSEWHEEL;

   switch (uMsg) {
   case WM_CREATE:
      {
         if (!m_hWnd)
            m_hWnd = hWnd;

         SetFocus (hWnd);

         // Create the menus
         // make the button bars in temporary location
         RECT r;
         r.top = r.bottom = r.left = r.right = 0;
         m_pbarTop = new CButtonBar;
         m_pbarTop->Init (&r, hWnd, 2);
         m_pbarTop->ColorSet (m_cBackDark, m_cBackMed, m_cBackLight, m_cBackOutline);

         m_pbarBottom = new CButtonBar;
         m_pbarBottom->Init (&r, hWnd, 3);
         m_pbarBottom->ColorSet (m_cBackDark, m_cBackMed, m_cBackLight, m_cBackOutline);

         PCIconButton pi;

#define ANIMCONTROLTEXT \
            "<p/>" \
            "By default this creates the object at the current editing time. " \
            "To place the object at a different time hold down the control key when " \
            "you click on the object's timeline."

         // top
         pi = m_pbarTop->ButtonAdd (0, IDB_NEWANIMKEYFRAME, ghInstance, NULL,
            "New general keyframe...",
            "Creates a new keyframe event on the timeline. You can control all "
            "attributes except those for location and rotation."
            ANIMCONTROLTEXT,
            IDC_NEWANIMKEYFRAME);
         pi = m_pbarTop->ButtonAdd (0, IDB_NEWANIMKEYFRAMELOC, ghInstance, NULL,
            "New location keyframe...",
            "Creates a new keyframe event on the timeline that changes the object's location."
            ANIMCONTROLTEXT,
            IDC_NEWANIMKEYFRAMELOC);
         pi = m_pbarTop->ButtonAdd (0, IDB_NEWANIMKEYFRAMEROT, ghInstance, NULL,
            "New rotation keyframe...",
            "Creates a new keyframe event on the timeline that changes the object's rotation."
            ANIMCONTROLTEXT,
            IDC_NEWANIMKEYFRAMEROT);
         pi = m_pbarTop->ButtonAdd (0, IDB_NEWANIMBOOKMARK1, ghInstance, NULL,
            "New 0-duration bookmark...",
            "Creates a new bookmark that's good for noting important still-images "
            "in your animation."
            ANIMCONTROLTEXT,
            IDC_NEWANIMBOOKMARK1);
         pi = m_pbarTop->ButtonAdd (0, IDB_NEWANIMBOOKMARK2, ghInstance, NULL,
            "New durational bookmark...",
            "Creates a new bookmark that's good for noting time ranges in your "
            "animation."
            ANIMCONTROLTEXT,
            IDC_NEWANIMBOOKMARK2);
         pi = m_pbarTop->ButtonAdd (0, IDB_NEWANIMWAVE, ghInstance, NULL,
            "New audio object...",
            "Creates an audio object that plays sound during the animation. "
            "You will need to visit the object's settings dialog to specify the wave file to use."
            ANIMCONTROLTEXT,
            IDC_NEWANIMWAVE);
         // BUGBUG - Temporarily removing animpath because not functional in this version
         //pi = m_pbarTop->ButtonAdd (0, IDB_NEWANIMPATH, NULL,
         //  "New animation path...",
         //   "Creates an animation event that moves and orients the object along a path."
         //   ANIMCONTROLTEXT,
         //   IDC_NEWANIMPATH);
         pi = m_pbarTop->ButtonAdd (0, IDB_NEWANIMCAMERA, ghInstance, NULL,
            "Synchronize camera",
            "Moves the currently selected animation camera so it's lookin from the same "
            "location and in the same direction as the view camera.",
            IDC_NEWANIMCAMERA);

         pi = m_pbarTop->ButtonAdd (1, IDB_ANIMJUMPSTART,ghInstance, 
            NULL, "Jump to the start",
            "Moves the current viewing time to the start of the play position."
            ,
            IDC_ANIMJUMPSTART);
         pi = m_pbarTop->ButtonAdd (1, IDB_ANIMJUMPPREV,ghInstance, 
            NULL, "Jump to the previous keyframe",
            "Moves the current viewing time to the previous keyframe."
            ,
            IDC_ANIMJUMPPREV);
         pi = m_pbarTop->ButtonAdd (1, IDB_ANIMSTOP,ghInstance, 
            "S", "Stop",
            "Stops playing."
            ,
            IDC_ANIMSTOP);
         m_pStop = pi;
         m_pStop->Enable (FALSE);
         pi = m_pbarTop->ButtonAdd (1, IDB_ANIMPLAY,ghInstance, 
            "P", "Play",
            "Plays the animation within the play markers."
            ,
            IDC_ANIMPLAY);
         m_pPlay = pi;
         pi = m_pbarTop->ButtonAdd (1, IDB_ANIMPLAYLOOP,ghInstance, 
            NULL, "Play (looped)",
            "Plays the animation within the play markers. When it "
            "reaches the end the sequence is repeated."
            ,
            IDC_ANIMPLAYLOOP);
         m_pPlayLoop = pi;
         pi = m_pbarTop->ButtonAdd (1, IDB_ANIMJUMPNEXT,ghInstance, 
            NULL, "Jump to the next keyframe",
            "Moves the current viewing time to the next keyframe."
            ,
            IDC_ANIMJUMPNEXT);
         pi = m_pbarTop->ButtonAdd (1, IDB_ANIMJUMPEND,ghInstance, 
            NULL, "Jump to the end",
            "Moves the current viewing time to the end of the play position."
            ,
            IDC_ANIMJUMPEND);

         pi = m_pbarTop->ButtonAdd (1, IDB_ANIMPLAYAUDIO,ghInstance, 
            NULL, "Play audio",
            "Click on an audio object and hold the mouse down to play the object's audio. "
            "Clicking outside an audio object will play the audio for the entire object."
            ,
            IDC_ANIMPLAYAUDIO);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
         pi = m_pbarTop->ButtonAdd (1, IDB_ZOOMIN,ghInstance, 
            "+", "Zoom",
            "To use this tool to zoom in and out. "
            "Click on the image and move right/left (to zoom in/out timewise) "
            "or up/down (to zoom in/out vertically)."
            ,
            IDC_ZOOMIN);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP | IBFLAG_ENDPIECE);
         pi = m_pbarTop->ButtonAdd (1, IDB_SCENEBOOKMARK, ghInstance, NULL,
            "Jump to bookmark",
            "Lets you select a bookmark in your scene and they sets the playback "
            "position there.",
            IDC_SCENEBOOKMARK);
         pi = m_pbarTop->ButtonAdd (1, IDB_SCENECAMERA, ghInstance, NULL,
            "Select viewing camera",
            "Select the camera to view the animation from.",
            IDC_SCENECAMERA);

         // right
         pi = m_pbarTop->ButtonAdd (2, IDB_HELP, ghInstance, "F1", "Help...", "Brings up help and documentation.", IDC_HELPBUTTON);
         pi = m_pbarTop->ButtonAdd (2, IDB_SCENEMENU, ghInstance, NULL,
            "Switch scenes",
            "Brings up a menu which lets you switch to a different scene, as well as "
            "adding, removing, and renaming scenes.",
            IDC_SCENEMENU);
         pi = m_pbarTop->ButtonAdd (2, IDB_SCENEFPS, ghInstance, NULL, "Framerate",
            "Brings up a menu which lets you set the frame rate.",
            IDC_SCENEFPS);
         pi = m_pbarTop->ButtonAdd (2, IDB_SCENEBAYS, ghInstance, NULL, "Objects displayed in timeline",
            "Brings up a menu which lets you control how many objects are displayed in the timeline.",
            IDC_SCENEBAYS);

         pi = m_pbarTop->ButtonAdd (2, IDB_ANIMDEFAULT,ghInstance, 
            NULL, "Change starting attributes",
            "At the beginning of a scene an object's attributes are set to "
            "their \"starting\" values. To change an object's starting attributes, "
            "select this tool and click in the object's timeline. The attriubtes "
            "will be set to the object's current attributes.",
            IDC_ANIMDEFAULT);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP | IBFLAG_ENDPIECE);

         // left
         m_pRedo = pi = m_pbarBottom->ButtonAdd (2, IDB_REDO, ghInstance, "Ctrl-Y", "Redo", "Redo the last undo.", IDC_REDOBUTTON);

         m_pUndo = pi = m_pbarBottom->ButtonAdd (2, IDB_UNDO, ghInstance, "Ctrl-Z", "Undo", "Undo the last change.", IDC_UNDOBUTTON);

         pi = m_pbarBottom->ButtonAdd (2, IDB_DELETE, ghInstance, "Del", "Delete", "Delete the selection.", IDC_DELETEBUTTON);
         m_listNeedSelect.Add (&pi);   // enable this if have a selection

         m_pPaste = pi = m_pbarBottom->ButtonAdd (2, IDB_PASTE, ghInstance, "Ctrl-V", "Paste", "Paste from the clipboard.", IDC_PASTEBUTTON);
         // enable paste button based on clipboard
         ClipboardUpdatePasteButton ();

         pi = m_pbarBottom->ButtonAdd (2, IDB_COPY, ghInstance, "Ctrl-C", "Copy", "Copy the selection to the clipboard.", IDC_COPYBUTTON);
         m_listNeedSelect.Add (&pi);   // enable this if have a selection

         pi = m_pbarBottom->ButtonAdd (2, IDB_CUT, ghInstance, "Ctrl-X", "Cut", "Cut the selection to the clipboard.", IDC_CUTBUTTON);
         m_listNeedSelect.Add (&pi);   // enable this if have a selection

         // middle
         pi = m_pbarBottom->ButtonAdd (1, IDB_OBJDECONSTRUCT,ghInstance, 
            NULL, "Deconstruct",
            "Split the object up into its constituent parts. (For example: Click on a "
            "pose to split it into keyframes.)"
            , IDC_OBJDECONSTRUCT);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM | IBFLAG_ENDPIECE);
         pi = m_pbarBottom->ButtonAdd (1, IDB_OBJMERGE,ghInstance, 
            NULL, "Merge objects",
            "Select one or more objects you wish to merge (such as keyframes at the same time) "
            "with another object. Then, using this tool, click on the object "
            "to merge to. Not all objects "
            "will be able to merge; see the documentation for details."
            , IDC_OBJMERGE);
         pi->Enable(FALSE);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
         m_listNeedSelect.Add (&pi);   // enable this if have a selection

         pi = m_pbarBottom->ButtonAdd (1, IDB_OBJDIALOG,ghInstance, 
            "Alt-S", "Object settings",
            "Select this tool and click on an object to bring up object-specific "
            "settings."
            , IDC_OBJDIALOG);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarBottom->ButtonAdd (1, IDB_SELINDIVIDUAL,ghInstance, 
            "S", "Select individual objects",
            "Click on an object to select it. To select more than one object hold "
            "the control key down while clicking on objects."
            , IDC_SELINDIVIDUAL);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
         pi = m_pbarBottom->ButtonAdd (1, IDB_SELREGION,ghInstance, 
            NULL, "Select multiple objects",
            "Click and drag over a region to select all the objects within the region. "
            "To add to the selection hold down the control key while dragging. "
            "To select from all objects hold down the shift key.",
            IDC_SELREGION);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
         pi = m_pbarBottom->ButtonAdd (1, IDB_SELALL,ghInstance, 
            "Ctrl-A", "Select all objects",
            "Selects all the objects."
            , IDC_SELALL);
         pi = m_pbarBottom->ButtonAdd (1, IDB_SELNONE,ghInstance, 
            "0", "Clear selection",
            "De-selects all selected objects."
            , IDC_SELNONE);
         pi = m_pbarBottom->ButtonAdd (1, IDB_ANIMTIMEINSERT,ghInstance, 
            NULL, "Insert time",
            "Click and drag over a region to insert that much time. "
            "To insert time for all objects hold down the shift key. "
            "Holding down the shift key will also extend the length of the scene.",
            IDC_ANIMTIMEINSERT);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM | IBFLAG_ENDPIECE);
         pi = m_pbarBottom->ButtonAdd (1, IDB_ANIMTIMEREMOVE,ghInstance, 
            NULL, "Remove time",
            "Click and drag over a region to remove the selected time. "
            "To remove time for all objects hold down the shift key. "
            "Holding down the shift key will also reduce the length of the scene.",
            IDC_ANIMTIMEREMOVE);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
         pi = m_pbarBottom->ButtonAdd (1, IDB_ANIMSELACTIVE,ghInstance, 
            NULL, "Select changing objects",
            "Selects objects in the world that change in the visible timeline."
            , IDC_ANIMSELACTIVE);

         // left
         pi = m_pbarBottom->ButtonAdd (0, IDB_ANIMMOVELR,ghInstance, 
            NULL, "Move objects left/right",
            "Click on the selected objects and drag the mouse to move them left or "
            "right."
            "<p/>"
            "If you hold the control key down when you click, the selected objects "
            "will be duplicated.",
            IDC_ANIMMOVELR);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM | IBFLAG_ENDPIECE);
         m_listNeedSelect.Add (&pi);   // enable this if have a selection
         pi = m_pbarBottom->ButtonAdd (0, IDB_ANIMMOVEUD,ghInstance, 
            NULL, "Move objects up/down",
            "Click on the selected objects and drag the mouse to move them "
            "up or down; this is only for appearance and doesn't affect animation."
            "<p/>"
            "If you hold the control key down when you click, the selected objects "
            "will be duplicated.",
            IDC_ANIMMOVEUD);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
         m_listNeedSelect.Add (&pi);   // enable this if have a selection
         pi = m_pbarBottom->ButtonAdd (0, IDB_ANIMRESIZE,ghInstance, 
            NULL, "Resize objects",
            "Click on the left or right end of an object and drag to resize "
            "it.",
            IDC_ANIMRESIZE);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarBottom->ButtonAdd (0, IDB_GRAPHPOINTMOVE,ghInstance, 
            NULL, "Move graph point",
            "Use this when viewing an object's timeline graph. Click on a point "
            "in the graph to move it.",
            IDC_GRAPHPOINTMOVE);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
         pi = m_pbarBottom->ButtonAdd (0, IDB_OBJCONTROLUD,ghInstance, 
            NULL, "Move graph point UD only",
            "Use this when viewing an object's timeline graph. Click on a point "
            "in the graph to move it.",
            IDC_GRAPHPOINTMOVEUD);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
         pi = m_pbarBottom->ButtonAdd (0, IDB_GRAPHPOINTDELETE,ghInstance, 
            NULL, "Delete graph point",
            "Use this when viewing an object's timeline graph. Click on a point "
            "in the graph to delete it.",
            IDC_GRAPHPOINTDELETE);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);


         // add timeline
         m_pTimeline = new CTimeline;
         if (m_pTimeline)
            m_pTimeline->Init (hWnd, m_cBackLight, m_cBackMed,
               RGB(0xc0,0xff,0xff), RGB (0x40,0x80,0xff), RGB(0,0,0), WM_USER+89);

         // add scrollbars
         m_hWndScroll = CreateWindow ("SCROLLBAR", "",
               WS_VISIBLE | WS_CHILD | SBS_HORZ,
               10, 10, 10, 10,   // temporary sizes
               hWnd, (HMENU) IDC_MYSCROLLBAR, ghInstance, NULL);

         // clibboard viewer
         m_hWndClipNext = SetClipboardViewer (hWnd);

         // set the poionter more
         SetPointerMode (IDC_SELINDIVIDUAL, 0);
         SetPointerMode (IDC_ZOOMIN, 1);
         SetPointerMode (IDC_ANIMMOVELR, 2);
         
         // update the selected items
         ModifyBaysFromSelection ();

         // create a time to keep syncing to the world
         SetTimer (hWnd, TIMER_SYNCTOWORLD, 100, NULL);
      }

      return 0;

   case MM_WOM_DONE:
      {
         PWAVEHDR pwh = (PWAVEHDR) lParam;

         // try seting it back out
         PlayAddBuffer (pwh);
      }
      return 0;

   case WM_MOUSEMOVE:
      // set focus so mousewheel works - as opposed to getting taken by listbox
      SetFocus (m_hWnd);
      break;

   case WM_USER+89:
      // got a message from the timeline saying that changed
      {
         fp fPointer;
         fp fLeft, fRight, fScene;
         BOOL fChanged = FALSE;

         PCScene pScene;
         fp fTime, fDuration;
         m_pSceneSet->StateGet (&pScene, &fTime);
         fDuration = pScene->DurationGet();

         fPointer = m_pTimeline->PointerGet();
         m_pTimeline->LimitsGet (&fLeft, &fRight, &fScene);

         // apply grid
         fp f, fl, fr, fs;
         f = pScene->ApplyGrid (fPointer);
         if (f != fPointer) {
            m_pTimeline->PointerSet (f);
            fPointer = f;
         }
         fl = pScene->ApplyGrid (fLeft);
         fr = pScene->ApplyGrid (fRight);
         fs = pScene->ApplyGrid (fScene);
         if ((fl != fLeft) || (fr != fRight) || (fs != fScene)) {
            m_pTimeline->LimitsSet (fl, fr, fs);
            fLeft = fl;
            fRight = fr;
            fScene = fs;
         }
         if (fDuration != fs) {
            fDuration = fs;
            pScene->DurationSet (fDuration);
            NewScrollLoc ();
         }

         if (fPointer < 0) {
            fPointer = 0;
            fChanged = TRUE;
         }
         if (fPointer > fDuration) {
            fPointer = fDuration;
            fChanged = TRUE;
         }
         if (fLeft < 0) {
            fLeft = 0;
            fChanged = TRUE;
         }
         if (fLeft > fDuration) {
            fLeft = fDuration;
            fChanged = TRUE;
         }
         if (fRight < fLeft ) {
            fRight = fLeft;
            fChanged = TRUE;
         }
         if (fRight > fDuration) {
            fRight = fDuration;
            fChanged = TRUE;
         }

         // if changed send back
         if (fChanged) {
            m_pTimeline->PointerSet (fPointer);
            m_pTimeline->LimitsSet (fLeft, fRight, fDuration);
         }

         if (fPointer != fTime) {
            m_pSceneSet->StateSet (pScene, fPointer);

            // if playing
            if (m_dwPlaying) {
               m_fPlayLastFrame = fPointer;
               m_dwPlayTime = GetTickCount();
            }

            // update time-line display on edit window
            InvalidateBayData();
         }
      }
      return 0;

   case WM_TIMER:
      if (wParam == TIMER_PLAYING) {
         // get the current time
         DWORD dwTime = GetTickCount();
         
         // what time are we supposed to play
         if (dwTime > m_dwPlayTime)
            m_fPlayLastFrame += (fp) (dwTime - m_dwPlayTime) / 1000.0;
         m_dwPlayTime = dwTime;

         // get current info
         fp fMin, fMax, fTime, fScene;
         PCScene pScene;
         m_pSceneSet->StateGet (&pScene, &fTime);
         m_pTimeline->LimitsGet (&fMin, &fMax, &fScene);

         // if go beyond max
         if (m_fPlayLastFrame >= fMax) {
            if (m_dwPlaying != 2) {
               // if not looping then want to stop
               KillTimer (hWnd, TIMER_PLAYING);
               m_dwPlaying = 0;

               m_pPlay->Enable (TRUE);
               m_pPlayLoop->Enable (TRUE);
               m_pStop->Enable (FALSE);

               m_pSceneSet->StateSet (pScene, fMax);
               return 0;
            }

            // else do module
            fp fDelta;
            fDelta = fMax - fMin;
            if (fDelta > CLOSE)
               m_fPlayLastFrame = myfmod(m_fPlayLastFrame - fMin, fDelta) + fMin;
            else
               m_fPlayLastFrame = fMin;
         }

         // set new location
         m_pSceneSet->StateSet (pScene, pScene->ApplyGrid (m_fPlayLastFrame));

         return 0;
      }
      else if (wParam == TIMER_SYNCTOWORLD) {
         // continuall call sync to world so that any changed that have made will
         // appear in the world. Alternatively, could call have the scenes automagically
         // call on a scenechanged() notification, but this would probably call
         // SyncToWorld() more often than necessary
         if (m_pSceneSet)
            m_pSceneSet->SyncToWorld();
         return 0;
      }
      else if (wParam == TIMER_AUTOSCROLL) {
         // if no button down then ignore
         if (!m_dwButtonDown || !m_pTimerAutoScroll)
            return 0;
         DWORD dwPointerMode;
         dwPointerMode = m_adwPointerMode[m_dwButtonDown-1];

         // only autoscroll certain ones
         switch (dwPointerMode) {
         case IDC_ANIMTIMEINSERT:
         case IDC_ANIMTIMEREMOVE:
         case IDC_GRAPHPOINTMOVE:
         case IDC_GRAPHPOINTMOVEUD:
         case IDC_SELREGION:
         case IDC_ANIMMOVELR:
         case IDC_ANIMMOVEUD:
         case IDC_ANIMRESIZE:
         case IDC_OBJECTPASTEDRAG:
            break;
         default:
            return 0;
         }

         // get the mouse location
         POINT p;
         RECT r;
         GetCursorPos (&p);
         ScreenToClient (m_pTimerAutoScroll->hWndData, &p);
         GetClientRect (m_pTimerAutoScroll->hWndData, &r);

         // amount to move
         fp fDeltaLR, fDeltaUD;
         fDeltaLR = fDeltaUD = 0;
         if (p.x < r.left)
            fDeltaLR = -1;
         else if (p.x > r.right)
            fDeltaLR = 1;
         if (p.y < r.top)
            fDeltaUD = -1;
         else if (p.y > r.bottom)
            fDeltaUD = 1;
         if (!fDeltaUD && !fDeltaLR)
            return 0;   // no change

         // else, move
         PCScene pScene;
         m_pSceneSet->StateGet (&pScene, NULL);
         if (!pScene)
            return 0;
         fDeltaLR *= (m_fViewTimeEnd- m_fViewTimeStart) / 32.0;
         fDeltaUD *= (m_pTimerAutoScroll->fVPosMax - m_pTimerAutoScroll->fVPosMin) / 16.0;
         if (m_pTimerAutoScroll->fScrollFlip)
            fDeltaUD *= -1;
         if ((fDeltaLR < 0) && (fDeltaLR + m_fViewTimeStart < 0))
            fDeltaLR = -m_fViewTimeStart;
         if ((fDeltaLR > 0) && (fDeltaLR + m_fViewTimeEnd > pScene->DurationGet()))
            fDeltaLR = pScene->DurationGet() - m_fViewTimeEnd;
         if ((fDeltaUD < 0) && (fDeltaUD + m_pTimerAutoScroll->fVPosMin < m_pTimerAutoScroll->fScrollMin))
            fDeltaUD = m_pTimerAutoScroll->fScrollMin -  m_pTimerAutoScroll->fVPosMin;
         if ((fDeltaUD > 0) && (fDeltaUD + m_pTimerAutoScroll->fVPosMax > m_pTimerAutoScroll->fScrollMax))
            fDeltaUD = m_pTimerAutoScroll->fScrollMax - m_pTimerAutoScroll->fVPosMax;

         if (fDeltaLR) {
            m_fViewTimeStart += fDeltaLR;
            m_fViewTimeEnd += fDeltaLR;
            NewScrollLoc ();
         }
         if (fDeltaUD) {
            m_pTimerAutoScroll->fVPosMin += fDeltaUD;
            m_pTimerAutoScroll->fVPosMax += fDeltaUD;
            NewScrollLoc (m_pTimerAutoScroll);
            }


         // fake a mouse move
         MouseMove (m_pTimerAutoScroll, m_pTimerAutoScroll->hWndData,
            WM_MOUSEMOVE, 0, MAKELPARAM((WORD)(short)p.x,(WORD)(short)p.y));
         return 0;
      } // autoscroll timer
      break;

   case WM_DRAWCLIPBOARD:
      {
         ClipboardUpdatePasteButton ();

         if (m_hWndClipNext)
            SendMessage (m_hWndClipNext, uMsg, wParam, lParam);
      }
      return 0;


   case WM_CHANGECBCHAIN: 
      {
         // If the next window is closing, repair the chain. 

         if ((HWND) wParam == m_hWndClipNext) 
           m_hWndClipNext = (HWND) lParam; 

         // Otherwise, pass the message to the next link. 

         else if (m_hWndClipNext != NULL) 
           SendMessage(m_hWndClipNext, uMsg, wParam, lParam); 

      }
      break; 


   case WM_DESTROY:
      // kill the timer that keeps synced up
      KillTimer (hWnd, TIMER_SYNCTOWORLD);

      if (m_dwPlaying)
         KillTimer (hWnd, TIMER_PLAYING);
      m_dwPlaying = 0;

      if (m_pTimeline)
         delete m_pTimeline;
      m_pTimeline = NULL;

      // remove from the clipboard chain
      ChangeClipboardChain(hWnd, m_hWndClipNext); 
      break;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {

      case IDC_ANIMSTOP:
         if (m_dwPlaying) {
            KillTimer (hWnd, TIMER_PLAYING);
            m_dwPlaying = 0;

            m_pPlayLoop->Enable (TRUE);
            m_pPlay->Enable (TRUE);
            m_pStop->Enable (FALSE);
         }
         break;

      case IDC_ANIMJUMPNEXT:
      case IDC_ANIMJUMPPREV:
         {
            BOOL fNext = (LOWORD(wParam) == IDC_ANIMJUMPNEXT);

            // find out what the next one is
            BOOL fFoundNext = FALSE;
            fp fTime, fFound;
            PCScene pScene;
            m_pSceneSet->StateGet (&pScene, &fTime);
            fTime += 5 * CLOSE * (fNext ? 1 : -1); // so looking just a bit to left/right

            // loop through all the objects
            DWORD i,j;
            PCSceneObj pSceneObj;
            PCAnimAttrib paa;
            DWORD dwIndex;
            TEXTUREPOINT tp;
            DWORD dwLinear;
            for (i = 0; i < pScene->ObjectNum(); i++) {
               pSceneObj = pScene->ObjectGet(i);
               if (!pSceneObj)
                  continue;

               // loop through all the animations
               pSceneObj->AnimAttribGenerate();
               for (j = 0; j < pSceneObj->AnimAttribNum(); j++) {
                  paa = pSceneObj->AnimAttribGet(j);
                  if (!paa)
                     continue;

                  dwIndex = paa->PointClosest (fTime, fNext ? 1 : -1);
                  if (dwIndex == -1)
                     continue;
            
                  if (!paa->PointGet (dwIndex, &tp, &dwLinear))
                     continue;

                  if (fNext && (!fFoundNext || (tp.h < fFound))) {
                     fFoundNext = TRUE;
                     fFound = tp.h;
                  }
                  else if (!fNext && (!fFoundNext || (tp.h > fFound))) {
                     fFoundNext = TRUE;
                     fFound = tp.h;
                  }
               } // j
            }// i

            // if found then move pointer
            if (fFoundNext) {
               m_pSceneSet->StateSet (pScene, fFound);

               // if playing
               if (m_dwPlaying) {
                  m_fPlayLastFrame = fFound;
                  m_dwPlayTime = GetTickCount();
               }
            }
         }
         return 0;

      case IDC_ANIMJUMPSTART:
      case IDC_ANIMJUMPEND:
         {
            fp fMin, fMax, fTime, fScene;
            PCScene pScene;
            m_dwPlayTime = GetTickCount ();
            m_pTimeline->LimitsGet (&fMin, &fMax, &fScene);
            m_pSceneSet->StateGet (&pScene, NULL);

            fTime = (LOWORD(wParam) == IDC_ANIMJUMPSTART) ? fMin : fMax;
            fTime = pScene->ApplyGrid (fTime);
            m_pSceneSet->StateSet (pScene, fTime);

            // if playing
            if (m_dwPlaying) {
               m_fPlayLastFrame = fTime;
               m_dwPlayTime = GetTickCount();
            }
         }
         return 0;

      case IDC_ANIMPLAY:
      case IDC_ANIMPLAYLOOP:
         if (!m_dwPlaying) {
            m_pPlayLoop->Enable (FALSE);
            m_pPlay->Enable (FALSE);
            m_pStop->Enable (TRUE);

            // set current play info to whatever at
            fp fMin, fMax, fScene;
            PCScene pScene;
            m_dwPlayTime = GetTickCount ();
            m_pTimeline->LimitsGet (&fMin, &fMax, &fScene);
            m_pSceneSet->StateGet (&pScene, &m_fPlayLastFrame);
            if (m_fPlayLastFrame + CLOSE >= fMax) {
               m_fPlayLastFrame = pScene->ApplyGrid (fMin);
               m_pSceneSet->StateSet (pScene, m_fPlayLastFrame);
               // NOTE: Dont need to update timeline or invalidate bay data
               // because callback from stateset() will do that
            }

            DWORD dwFPS;
            dwFPS = pScene->FPSGet();
            if (!dwFPS)
               dwFPS = 60;
            SetTimer (m_hWnd, TIMER_PLAYING, 1000/dwFPS, NULL);
            m_dwPlaying = (LOWORD(wParam) == IDC_ANIMPLAY) ? 1 : 2;
         }
         break;


      case IDC_NEWANIMCAMERA:
         {
            // make sure there's a camera
            GUID g;
            if (!m_pSceneSet->CameraGet (&g)) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"You must specify an animation camera.",
                  L"You can select the camera to use with the \"Select viewing camera\" button.",
                  MB_ICONEXCLAMATION | MB_OK);
               return 0;
            };

            // move it
            PCHouseView pv;
            pv = FindViewForWorld (m_pWorld);
            if (!pv)
               return FALSE;

            if (!pv->CameraToObject (&g)) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"The view must be in a perspective view.",
                  L"You cannot synchronize the camera if you are viewing in a flattened view. "
                  L"Switch to the walthrough or model views.",
                  MB_ICONEXCLAMATION | MB_OK);
            }
            else
               EscChime (ESCCHIME_INFORMATION);
         }
         break;
      

      case IDC_NEWANIMKEYFRAME:
      case IDC_NEWANIMKEYFRAMELOC:
      case IDC_NEWANIMKEYFRAMEROT:
      case IDC_NEWANIMPATH:
      case IDC_NEWANIMWAVE:
      case IDC_NEWANIMBOOKMARK1:
      case IDC_NEWANIMBOOKMARK2:
         {
            // clear out purgatory
            m_pPurgatory->Clear();

            // create info for one object
            DWORD dwIndex;
            PCSceneObj pSceneObj;
            GUID g;
            memset (&g, 0, sizeof(g)); // any guid. doesnt matter
            dwIndex = m_pPurgatory->ObjectAdd (&g);
            if (dwIndex == -1)
               return 0;
            pSceneObj = m_pPurgatory->ObjectGet (dwIndex);
            if (!pSceneObj)
               return 0;

            // create one animation sequence
            PCAnimSocket pos;
            switch (LOWORD(wParam)) {
            case IDC_NEWANIMKEYFRAME:
               pos = new CAnimKeyframe (0);
               break;
            case IDC_NEWANIMKEYFRAMELOC:
               pos = new CAnimKeyframe (1);
               break;
            case IDC_NEWANIMKEYFRAMEROT:
               pos = new CAnimKeyframe (2);
               break;
            case IDC_NEWANIMPATH:
               pos = new CAnimPath;
               break;
            case IDC_NEWANIMWAVE:
               pos = new CAnimWave;
               break;
            case IDC_NEWANIMBOOKMARK1:
               pos = new CAnimBookmark (0);
               break;
            case IDC_NEWANIMBOOKMARK2:
               pos = new CAnimBookmark (1);
               break;
            default:
               pos = NULL;
            }
            if (!pos)
               return 0;
            pSceneObj->ObjectAdd (pos, TRUE);

            // does it support resize?
            BOOL fCanResize;
            fCanResize = (pos->QueryTimeMax () > 0);

            // go to paste mode
            SetPointerMode (fCanResize ? IDC_OBJECTPASTEDRAG : IDC_OBJECTPASTE, 0);
         }
         return 0;

      case IDC_SCENEFPS:
         {
            HMENU hMenu = CreatePopupMenu ();
            DWORD adwFPS[]= {0,1,2,3,4,5,8,10,12,15,16,24,25,30,50,60};

            DWORD i, dwCur;
            PCScene pScene;
            m_pSceneSet->StateGet (&pScene, NULL);
            if (!pScene)
               return 0;
            dwCur = pScene->FPSGet();
            char szTemp[32];
            for (i = 0; i < sizeof(adwFPS)/sizeof(DWORD);i++) {
               if (i)
                  sprintf (szTemp, "%d ", (int) adwFPS[i]);
               else
                  strcpy (szTemp, "No framerate");

               AppendMenu (hMenu, MF_ENABLED | MF_STRING | ((adwFPS[i] == dwCur) ? MF_CHECKED : 0),
                  100 + adwFPS[i], szTemp);
            }

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hMenu, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            if (iRet >= 100) {
               pScene->FPSSet ((DWORD)iRet - 100);

               // current time
               fp fPointer;
               m_pSceneSet->StateGet (NULL, &fPointer);
               fPointer = pScene->ApplyGrid(fPointer);
               m_pSceneSet->StateSet (pScene, fPointer);

               // reset the sliders
               fp fLeft, fRight, fScene;
               m_pTimeline->PointerSet (fPointer);
               m_pTimeline->LimitsGet (&fLeft, &fRight, &fScene);
               fScene = pScene->ApplyGrid (pScene->DurationGet());
               if (fScene != pScene->DurationGet()) {
                  pScene->DurationSet (fScene);
                  NewScrollLoc ();
               }
               m_pTimeline->LimitsSet (pScene->ApplyGrid (fLeft), pScene->ApplyGrid(fRight), fScene);

               // invalidate diplays
               InvalidateBayData ();
            }

            DestroyMenu (hMenu);
         }
         return 0;

      case IDC_SCENEBAYS:
         {
            HMENU hMenu = CreatePopupMenu ();

            DWORD i;
            char szTemp[32];
            for (i = 1; i < 10;i++) {
               sprintf (szTemp, "%d ", (int) i);

               AppendMenu (hMenu, MF_ENABLED | MF_STRING | ((m_lSVBAY.Num() == i) ? MF_CHECKED : 0),
                  100 + i, szTemp);
            }

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hMenu, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            if (iRet >= 100)
               UpdateNumBays ((DWORD)iRet - 100, TRUE);

            DestroyMenu (hMenu);
         }
         return 0;


      case IDC_SCENEBOOKMARK:
         {
            HMENU hMenu = CreatePopupMenu ();

            PCScene pScene;
            m_pSceneSet->StateGet (&pScene, NULL);
            if (!pScene)
               return 0;

            DWORD i;
            char szTemp[128];
            CListFixed lBook;
            lBook.Init (sizeof(OSMBOOKMARK));
            pScene->BookmarkEnum (&lBook);

            if (!lBook.Num())
               AppendMenu (hMenu, MF_ENABLED | MF_STRING, 50, "The scene has no bookmarks");

            // Sort by name? or time?
            qsort (lBook.Get(0), lBook.Num(), sizeof(OSMBOOKMARK), OSMBOOKMARKCompare);

            POSMBOOKMARK po;
            po = (POSMBOOKMARK) lBook.Get(0);
            for (i = 0; i < lBook.Num(); i++, po++) {
               WideCharToMultiByte (CP_ACP, 0, po->szName, -1, szTemp, sizeof(szTemp), 0, 0);
               if (!szTemp[0])
                  strcpy (szTemp, "Unknown");

               AppendMenu (hMenu, MF_ENABLED | MF_STRING, 100 + i, szTemp);
            }

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hMenu, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);

            po = NULL;
            if (iRet >= 100)
               po = (POSMBOOKMARK) lBook.Get((DWORD)iRet-100);

            if (po) {
               PCWorld pWorld;
               m_pSceneSet->StateSet (pScene, po->fStart);

               // display...
               if ((m_fViewTimeStart > po->fStart) || (m_fViewTimeEnd < po->fStart)) {
                  m_fViewTimeStart = max(0, po->fStart - 30);
                  m_fViewTimeEnd = po->fStart + 30;
                  NewScrollLoc ();
               }

               pWorld = m_pSceneSet->WorldGet();
               if (pWorld) {
                  DWORD dwFind;
                  dwFind = pWorld->ObjectFind (&po->gObjectWorld);
                  pWorld->SelectionClear();
                  if (dwFind != -1)
                     pWorld->SelectionAdd (dwFind);
               }
            }
         }
         return 0;


      case IDC_SCENECAMERA:
         {
            HMENU hMenu = CreatePopupMenu ();

            PCScene pScene;
            m_pSceneSet->StateGet (&pScene, NULL);
            if (!pScene)
               return 0;

            DWORD i;
            char szTemp[512];
            CListFixed lCamera;
            lCamera.Init (sizeof(OSMANIMCAMERA));
            pScene->CameraEnum (&lCamera);

            // get the current camera
            BOOL fCamera;
            GUID gCamera;
            fCamera = m_pSceneSet->CameraGet (&gCamera);

            // default
            AppendMenu (hMenu, MF_ENABLED | MF_STRING | ((!fCamera) ? MF_CHECKED : 0), 50, "View camera");

            if (lCamera.Num())
               AppendMenu (hMenu, MF_SEPARATOR, 0, 0);

            POSMANIMCAMERA po;
            BOOL fCheck;
            GUID g;
            po = (POSMANIMCAMERA) lCamera.Get(0);
            for (i = 0; i < lCamera.Num(); i++, po++) {
               PWSTR psz = po->poac->StringGet (OSSTRING_NAME);
               if (psz && psz[0])
                  WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);
               else
                  strcpy (szTemp, "Unnamed");

               po->poac->GUIDGet (&g);
               fCheck = fCamera && IsEqualGUID (g, gCamera);
               AppendMenu (hMenu, MF_ENABLED | MF_STRING | (fCheck ? MF_CHECKED : 0),
                  100 + i, szTemp);
            }

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hMenu, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);

            // if 50 then default camera
            if (iRet == 50) {
               m_pSceneSet->CameraSet (NULL);
               return 0;
            }

            po = NULL;
            if (iRet >= 100)
               po = (POSMANIMCAMERA) lCamera.Get((DWORD)iRet-100);
            if (!po)
               return 0;

            po->poac->GUIDGet (&gCamera);
            m_pSceneSet->CameraSet (&gCamera);
         }
         return 0;

      case IDC_SCENEMENU:
         {
            HMENU hMenu = CreatePopupMenu ();
            PCScene pScene, ps;
            m_pSceneSet->StateGet (&pScene, NULL);

            // list of scenes
            DWORD i, dwThis;
            char szTemp[256];
            dwThis = -1;
            for (i = 0; i < m_pSceneSet->SceneNum();i++) {
               ps = m_pSceneSet->SceneGet (i);
               if (!ps)
                  continue;

               if (ps->NameGet ())
                  WideCharToMultiByte (CP_ACP, 0, ps->NameGet(), -1, szTemp, sizeof(szTemp),0,0);
               else
                  strcpy (szTemp, "Unknown");

               AppendMenu (hMenu, MF_ENABLED | MF_STRING | ((ps == pScene) ? MF_CHECKED : 0),
                  100 + i, szTemp);

               if (ps == pScene)
                  dwThis = i;
            }

            // and other menu items
            AppendMenu (hMenu, MF_SEPARATOR, 0, 0);
            AppendMenu (hMenu, MF_ENABLED | MF_STRING, 50, "Add a new scene");
            if (dwThis != -1)
               AppendMenu (hMenu, MF_ENABLED | MF_STRING, 51, "Copy this scene");
            AppendMenu (hMenu, MF_ENABLED | MF_STRING, 52, "Rename this scene...");
            if (m_pSceneSet->SceneNum() >= 2)
               AppendMenu (hMenu, MF_ENABLED | MF_STRING, 53, "Delete this scene");


            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hMenu, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);
            DestroyMenu (hMenu);

            if (iRet >= 100) {
               ps = m_pSceneSet->SceneGet (iRet-100);
               if (ps && (ps != pScene))
                  m_pSceneSet->StateSet (ps, 0);   // new scene
               return 0;
            }

            DWORD dwIndex;
            switch (iRet) {
            case 50: // add a scene
               dwIndex = m_pSceneSet->SceneNew ();
               ps = m_pSceneSet->SceneGet(dwIndex);
               if (ps) {
                  ps->NameSet (L"New scene");
                  ps->FPSSet (pScene->FPSGet());   // use same framerate
                  m_pSceneSet->StateSet (ps, 0);
               }
               break;

            case 51: // copy this scene
               dwIndex = m_pSceneSet->SceneClone (dwThis);
               ps = m_pSceneSet->SceneGet(dwIndex);
               if (ps) {
                  ps->NameSet (L"Copy");
                  m_pSceneSet->StateSet (ps, 0);
               }
               break;

            case 52: // rename this scene
               {
                  // get the name
                  CEscWindow cWindow;
                  RECT r;
                  WCHAR szwTemp[256];
                  if (pScene->NameGet())
                     wcscpy (szwTemp, pScene->NameGet());
                  else
                     wcscpy (szwTemp, L"Unknown");
                  DialogBoxLocation2 (m_hWnd, &r);
                  cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
                  PWSTR pszRet;
                  pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOLRENAMEOBJECT, OLGetNamePage, szwTemp);
                  if (!pszRet || _wcsicmp(pszRet, L"ok"))
                     return 0;

                  // else, changed name
                  szwTemp[63] = 0; // just in case
                  if (!szwTemp[0])
                     wcscpy (szwTemp, L"Unknown");
                  pScene->NameSet (szwTemp);
               }
               break;

            case 53: // delete this scene
               iRet = EscMessageBox (m_hWnd, ASPString(),
                  L"Are you sure you wish to delete this scene?",
                  L"Doing so will permenantly remove it.",
                  MB_ICONQUESTION | MB_YESNO);
               if (iRet != IDYES)
                  return 0;

               // pick a different scene
               if (dwThis)
                  dwIndex = 0;
               else
                  dwIndex = 1;
               ps = m_pSceneSet->SceneGet(dwIndex);
               if (ps) {
                  m_pSceneSet->StateSet (ps, 0);
                  m_pSceneSet->SceneRemove (dwThis);
               }
               break;
            }


         }
         return 0;

      case IDC_HELPBUTTON:
         ASPHelp(555); // pull up animation tutorial
         return 0;

      case IDC_ANIMSELACTIVE:
         {
            PCWorld pWorld;
            pWorld = m_pSceneSet->WorldGet();

            // loop over all the objects in a scene
            PCScene pScene = NULL;
            PCSceneObj pSceneObj;
            PCAnimAttrib paa;
            DWORD i,j;
            BOOL fChanged;
            m_pSceneSet->StateGet (&pScene, NULL);
            if (!pScene || !pWorld)
               return 0;

            // clear selection
            pWorld->SelectionClear();

            for (i = 0; i < pScene->ObjectNum(); i++) {
               pSceneObj = pScene->ObjectGet (i);
               if (!pSceneObj)
                  continue;

               fChanged = FALSE;
               pSceneObj->AnimAttribGenerate();
               for (j = 0; !fChanged && (j < pSceneObj->AnimAttribNum()); j++) {
                  paa = pSceneObj->AnimAttribGet (j);
                  if (!paa)
                     continue;
                  fChanged |= IsAttribInteresting (pSceneObj, paa->m_szName);
               } // j

               // if it changed then select it
               if (fChanged) {
                  GUID g;
                  DWORD dwObject;
                  pSceneObj->GUIDGet (&g);
                  dwObject = pWorld->ObjectFind (&g);
                  if (dwObject != -1)
                     pWorld->SelectionAdd (dwObject);
               }
            } // i
         }
         return 0;

      case IDC_SELNONE:
         m_pSceneSet->SelectionClear();
         return 0;

      case IDC_DELETEBUTTON:
         ClipboardDelete (TRUE);

         // remember this as an undo point
         m_pSceneSet->UndoRemember();
         return 0;

      case IDC_COPYBUTTON:
         if (ClipboardCopy ())
            EscChime (ESCCHIME_INFORMATION);
         return 0;

      case IDC_CUTBUTTON:
         ClipboardCut();
         return 0;

      case IDC_PASTEBUTTON:
         ClipboardPaste();

         // remember this as an undo point
         m_pSceneSet->UndoRemember();
         return 0;

      case IDC_UNDOBUTTON:
         m_pSceneSet->Undo(TRUE);
         return 0;

      case IDC_REDOBUTTON:
         m_pSceneSet->Undo(FALSE);
         return 0;

      case IDC_SELALL:
         {
            PCScene pScene;
            GUID gScene, gObjectWorld, gObjectAnim;
            m_pSceneSet->SelectionClear();
            m_pSceneSet->StateGet (&pScene, NULL);
            if (!pScene)
               return NULL;
            pScene->GUIDGet (&gScene);

            DWORD i, j;
            PCSceneObj pSceneObj;
            PCAnimSocket pas;
            for (i = 0; i < pScene->ObjectNum(); i++) {
               pSceneObj = pScene->ObjectGet(i);
               if (!pSceneObj)
                  continue;
               pSceneObj->GUIDGet (&gObjectWorld);

               for (j = 0; j < pSceneObj->ObjectNum(); j++) {
                  pas = pSceneObj->ObjectGet(j);
                  if (!pas)
                     continue;
                  pas->GUIDGet (&gObjectAnim);
                  m_pSceneSet->SelectionAdd (&gScene, &gObjectWorld, &gObjectAnim);
               }
            }
         }
         return 0;

      case IDC_GRAPHPOINTMOVE:
      case IDC_GRAPHPOINTMOVEUD:
      case IDC_GRAPHPOINTDELETE:
      case IDC_ANIMDEFAULT:
      case IDC_ANIMTIMEINSERT:
      case IDC_ANIMTIMEREMOVE:
      case IDC_ZOOMIN:
      case IDC_ANIMPLAYAUDIO:
      case IDC_SELINDIVIDUAL:
      case IDC_SELREGION:
      case IDC_ANIMMOVELR:
      case IDC_ANIMRESIZE:
      case IDC_OBJDIALOG:
      case IDC_OBJMERGE:
      case IDC_OBJDECONSTRUCT:
      case IDC_ANIMMOVEUD:
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
      }
      break;

   case WM_PAINT:
      return Paint (hWnd, uMsg, wParam, lParam);

   case WM_MOUSEWHEEL:
      {
         short zDelta = (short) HIWORD(wParam); 

         // amount to zoom
         fp fZoomLR = pow(2.0, .25);
         if (zDelta >= 0)
            fZoomLR = 1.0 / fZoomLR;

         // center of the zoom
         fp fCenterLR;
         fCenterLR = (m_fViewTimeStart + m_fViewTimeEnd) / 2;

         // scale of zoom
         fp fScaleLR;
         fScaleLR = (m_fViewTimeEnd - m_fViewTimeStart) / 2;
         fScaleLR *= fZoomLR;
         fScaleLR = max(.001, fScaleLR);

         // new values
         PCScene pScene;
         m_pSceneSet->StateGet (&pScene, NULL);
         m_fViewTimeStart = max(0, fCenterLR - fScaleLR);
         m_fViewTimeEnd = min((pScene ? pScene->DurationGet() : 0) +60, fCenterLR + fScaleLR);

         // update scroll location
         NewScrollLoc();
      }
      return 0;

   case WM_KEYDOWN:
   case WM_SYSCHAR:
   case WM_CHAR:
      return TestForAccel (NULL, hWnd, uMsg, wParam, lParam);

   case WM_HSCROLL:
      {
         // only deal with horizontal scroll
         HWND hWndScroll = (HWND) lParam;
         if (hWndScroll != m_hWndScroll)
            break;

         // get the scrollbar info
         SCROLLINFO si;
         memset (&si, 0, sizeof(si));
         si.cbSize = sizeof(si);
         si.fMask = SIF_ALL;
         GetScrollInfo (hWndScroll, SB_CTL, &si);
         
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
         si.nPos = max(0,si.nPos);

         // new values base on this
         fp fSize, fView;
         PCScene pScene;
         m_pSceneSet->StateGet (&pScene, NULL);
         if (!pScene)
            return 0;
         fSize = max(m_fViewTimeEnd, pScene->DurationGet());
         fView = m_fViewTimeEnd - m_fViewTimeStart;
         m_fViewTimeStart = (fp) si.nPos / 10000.0 * fSize;
         m_fViewTimeEnd = m_fViewTimeStart + fView;
         NewScrollLoc ();

         return 0;
      }
      break;


   case WM_SIZE:
      {
         // called when the window is sized. Move the button bars around
         int iWidth = LOWORD(lParam);
         int iHeight = HIWORD(lParam);

         m_fSmallWindow = (iWidth < 800);

         if (!m_hWnd)
            m_hWnd = hWnd; // so calcthumbnail loc works

         // move button bar
         RECT r, rt;
         int iScroll, iHScroll;
         iScroll = GetSystemMetrics (SM_CXVSCROLL);
         iHScroll = iScroll * 2 / 3;

         // top
         r.top = 0;
         r.left = 0;
         r.right = iWidth;
         r.bottom = VARBUTTONSIZE;
         m_pbarTop->Move (&r);
         m_pbarTop->Show(TRUE);

         // bottom
         r.top = iHeight - VARBUTTONSIZE;
         r.bottom = iHeight;
         m_pbarBottom->Move (&r);
         m_pbarBottom->Show(TRUE);

         GetClientRect (hWnd, &r);
         r.top += VARBUTTONSIZE;
         r.bottom -= VARBUTTONSIZE;
         r.left += VARBUTTONSIZE / 4;  // some space on left
         r.right -= VARBUTTONSIZE / 4;  // some space on right

         // amount allocated to Data window
         int iDataWidth;
         iDataWidth = (r.right - r.left) * 4.0 / 5.0;

         // time bar on top
         r.top += VARBUTTONSIZE/8;  // so space between time bar and buttons
         if (m_pTimeline) {
            RECT rMove;
            rMove = r;
            rMove.bottom = r.top + iHScroll;
            rMove.left = r.right - iDataWidth;
            rMove.right -= iScroll;
            rMove.left -= 3;
            rMove.right += 3;
            m_pTimeline->Move (&rMove);
         }
         r.top += iHScroll;

         // scrollbar on bottom
         r.bottom -= VARBUTTONSIZE/8;
         int iExtraRight, iExtraLeft;
         iExtraRight = iExtraLeft = 2; // acount for thickness of window border
         iExtraRight += iScroll; // vertical scroll bar
         MoveWindow (m_hWndScroll, r.right - iDataWidth + iExtraLeft, r.bottom - iHScroll,
            iDataWidth - iExtraRight - iExtraLeft, iHScroll, TRUE);
         r.bottom -= iHScroll;


         // all the bays
         PSVBAY pb;
         DWORD i;
         fp fHeight;
         fHeight = (fp)(r.bottom - r.top) / (fp)m_lSVBAY.Num();
         for (i = 0; i < m_lSVBAY.Num(); i++) {
            pb = (PSVBAY) m_lSVBAY.Get(i);

            if (fHeight <= 2) {
               ShowWindow (pb->hWndData, SW_HIDE);
               ShowWindow (pb->hWndInfo, SW_HIDE);
               continue;
            }

            rt.top = (int) ((fp)r.top + fHeight * (fp) i);
            rt.bottom = (int) ((fp)r.top + fHeight * (fp) (i+1)-1);

            // info
            rt.left = r.left;
            rt.right = r.right - iDataWidth;
            MoveWindow (pb->hWndInfo, rt.left, rt.top, rt.right - rt.left,
               rt.bottom - rt.top, TRUE);
            if (!IsWindowVisible (pb->hWndInfo))
               ShowWindow (pb->hWndInfo, SW_SHOW);

            // list box
            RECT rInfo;
            GetClientRect (pb->hWndInfo, &rInfo);
            rInfo.top += 16;  // so have space for title
            MoveWindow (pb->hWndList, rInfo.left, rInfo.top, rInfo.right - rInfo.left,
               rInfo.bottom - rInfo.top, TRUE);

            // data
            rt.left = r.right - iDataWidth;
            rt.right = r.right;
            MoveWindow (pb->hWndData, rt.left, rt.top, rt.right - rt.left,
               rt.bottom - rt.top, TRUE);
            if (!IsWindowVisible (pb->hWndData))
               ShowWindow (pb->hWndData, SW_SHOW);
         }

         // need to recalc the matricies since might have sized windows
         CalcBayMatrix (NULL);
      }
      break;

   case WM_CLOSE:
      delete this;
      return 0;

   }

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*******************************************************************************************
CSceneView::BayInfoWndProc - Handle the window procedure for the view
*/
LRESULT CSceneView::BayInfoWndProc (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (uMsg == gdwMouseWheel)
      uMsg = WM_MOUSEWHEEL;

   switch (uMsg) {
   case WM_CREATE:
      {
      }
      return 0;

   case WM_DESTROY:
      // do nothing for now
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC   hDC;
         hDC = BeginPaint (hWnd, &ps);

         RECT r;
         HBRUSH hbr;
         GetClientRect (hWnd, &r);

         // paint the grey behind the button bars and lines
         // dark grey
         hbr = (HBRUSH) CreateSolidBrush (m_cBackLight);
         FillRect (hDC, &r, hbr);
         DeleteObject (hbr);

         // name of the object
         PCObjectSocket pos;
         pos = NULL;
         if (pb->dwType) {
            DWORD dwFind = m_pWorld->ObjectFind (&pb->gWorldObject);
            if (dwFind == -1) {
               // it's blank
               pb->dwType = 0;
               InvalidateRect (pb->hWndData, NULL, FALSE);
            }
            else
               pos = m_pWorld->ObjectGet (dwFind);
         }

         // convert the name to ascii
         char  szTemp[256];
         if (pos && pos->StringGet (OSSTRING_NAME))
            WideCharToMultiByte (CP_ACP, 0, pos->StringGet(OSSTRING_NAME), -1,
               szTemp, sizeof(szTemp), 0,0);
         else
            strcpy (szTemp, "Unknown");

         // text
         HFONT hFontOld;
         COLORREF crOld;
         int iOldMode;
         hFontOld = (HFONT) SelectObject (hDC, m_hFont);
         crOld = SetTextColor (hDC, RGB(0,0,0));
         iOldMode = SetBkMode (hDC, TRANSPARENT);
         RECT rDraw;
         rDraw = r;
         rDraw.right -= 32;
         if (pb->dwType == 2)
            rDraw.bottom = rDraw.top + 32;
         DrawText (hDC, pos ? szTemp : "Empty - select an object to show it here.",
            -1, &rDraw, DT_LEFT | DT_END_ELLIPSIS | DT_TOP | DT_WORDBREAK |
            ((pb->dwType == 2) ? DT_SINGLELINE : 0));
         SelectObject (hDC, hFontOld);
         SetTextColor (hDC, crOld);
         SetBkMode (hDC, iOldMode);

         // and button so know can click
         if (pb->dwType) {
            HICON hIcon;
            hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_BAYICON1));
            DrawIcon (hDC, r.right - 32, r.top, hIcon);
         }

         EndPaint (hWnd, &ps);
         return 0;
      }
      return 0;

   case WM_MEASUREITEM:
      if (wParam == IDC_LIST) {
         PMEASUREITEMSTRUCT pmi = (PMEASUREITEMSTRUCT) lParam;
         pmi->itemHeight = 16;
         return TRUE;
      }
      break;

   case WM_DRAWITEM:
      if (wParam == IDC_LIST) {
         PDRAWITEMSTRUCT pdi = (PDRAWITEMSTRUCT) lParam;

         BOOL fSel;
         fSel = pdi->itemState & ODS_SELECTED;

         // figure out the color
         PATTRIBVAL pav = (PATTRIBVAL) pb->plATTRIBVAL->Get(0);
         DWORD dwNum = pb->plATTRIBVAL->Num();
         DWORD i;
         COLORREF cColor;
         if (fSel && (pdi->itemID >= 0) && (pdi->itemID <= (int) dwNum)) {
            DWORD dwColor = 0;
            for (i = 0; i < (DWORD) pdi->itemID; i++)
               if (pav[i].fValue)
                  dwColor = (dwColor+1) % (sizeof(gacGraph) / sizeof(COLORREF));
            cColor = gacGraph[dwColor];
         }
         else {
            fSel = FALSE;  // this one can't be selected
            cColor = RGB(0xff,0xff,0xff);
         }

         // bacground
         HBRUSH hbr;
         hbr = CreateSolidBrush (cColor);
         HDC hDC;
         hDC = pdi->hDC;
         FillRect (hDC, &pdi->rcItem, hbr);
         DeleteObject (hbr);

         // text
         if ((pdi->itemID >= 0) && (pdi->itemID <= (int) dwNum)) {
            HFONT hFontOld;
            COLORREF crOld;
            int iOldMode;
            char szTemp[128];
            WideCharToMultiByte (CP_ACP, 0, pav[pdi->itemID].szName, -1, szTemp, sizeof(szTemp), 0,0);
            hFontOld = (HFONT) SelectObject (hDC, m_hFont);
            crOld = SetTextColor (hDC, fSel ? RGB(0xff,0xff,0xff) : RGB(0,0,0));
            iOldMode = SetBkMode (hDC, TRANSPARENT);
            DrawText (hDC, szTemp,
               -1, &pdi->rcItem, DT_LEFT | DT_END_ELLIPSIS | DT_VCENTER | DT_SINGLELINE);
            SelectObject (hDC, hFontOld);
            SetTextColor (hDC, crOld);
            SetBkMode (hDC, iOldMode);
         }

         return TRUE;
      }
      break;

   case WM_COMMAND:
      if ((LOWORD(wParam) == IDC_LIST) || (HIWORD(wParam) == LBN_SELCHANGE)) {
         // find out which one has changed
         PATTRIBVAL pav = (PATTRIBVAL) pb->plATTRIBVAL->Get(0);
         DWORD dwNum = pb->plATTRIBVAL->Num();
         DWORD i;
         BOOL fSel;
         for (i = 0; i < dwNum; i++) {
            fSel = (SendMessage (pb->hWndList, LB_GETSEL, i, 0) > 0);
            if ((fSel && !pav[i].fValue) || (!fSel &&  pav[i].fValue))
               break;
         }
         if (i >= dwNum) {
            // set the focus so mouse wheel works
            //SetFocus (m_hWnd);

            return 0;   // nothing has changed
         }

         // else...
         // if de-select
         if (!fSel) {
            pav[i].fValue = 0;
            InvalidateRect (pb->hWndData, NULL, FALSE);
            // make sure to redraw entire listbox since one selection affects another
            InvalidateRect (pb->hWndList, NULL, FALSE);

            // set the focus so mouse wheel works
            //SetFocus (m_hWnd);
            return 0;
         }

         // else, add selection
         BaySelectAttrib (pb, pav[i].szName);
         BaySelectToMinMax (pb);
         CalcBayMatrix (pb);
         InvalidateRect (pb->hWndData, NULL, FALSE);

         // update list box display
         SendMessage (pb->hWndList, LB_SETSEL, FALSE, -1);
         for (i = 0; i < dwNum; i++) {
            if (pav[i].fValue)
               SendMessage (pb->hWndList, LB_SETSEL, TRUE, i);
         }

         // make sure to redraw entire listbox since one selection affects another
         InvalidateRect (pb->hWndList, NULL, FALSE);

         // set the focus so mouse wheel works
         //SetFocus (m_hWnd);
         return 0;
      };

      break;

   case WM_LBUTTONDOWN:
      {
         int iX, iY;
         RECT r;
         iX = LOWORD(lParam);
         iY = HIWORD(lParam);
         GetClientRect (hWnd, &r);

         // over the buton only
         if (!pb->dwType || (iX < r.right - 32) || (iY > r.top + 16)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
            }

         // else, swap
         switch (pb->dwType) {
         default:
         case 1:  // boxes
            {
               // get the attributes
               PCWorld pWorld = NULL;
               PCObjectSocket pos = NULL;
               pb->plATTRIBVAL->Clear();
               if (m_pSceneSet)
                  pWorld = m_pSceneSet->WorldGet();
               if (pWorld)
                  pos = pWorld->ObjectGet(pWorld->ObjectFind(&pb->gWorldObject));
               if (pos)
                  pos->AttribGetAll (pb->plATTRIBVAL);
               qsort (pb->plATTRIBVAL->Get(0), pb->plATTRIBVAL->Num(), sizeof(ATTRIBVAL), ATTRIBVALCompare);

               // Need to calc what to display in list window
               BaySelectDefault (pb);

               // Need to show list window
               SendMessage (pb->hWndList, LB_RESETCONTENT, 0,0);
               PATTRIBVAL pav;
               pav = (PATTRIBVAL) pb->plATTRIBVAL->Get(0);
               DWORD i;
               for (i = 0; i < pb->plATTRIBVAL->Num(); i++, pav++) {
                  char szTemp[128];
                  WideCharToMultiByte (CP_ACP, 0, pav->szName, -1, szTemp, sizeof(szTemp),0,0);
                  SendMessage (pb->hWndList, LB_ADDSTRING, 0, (LPARAM) szTemp);
                  if (pav->fValue)
                     SendMessage (pb->hWndList, LB_SETSEL, TRUE, i);
               }
               ShowWindow (pb->hWndList, SW_SHOW);

               // Calc scroll min and max
               BaySelectToMinMax (pb);

               pb->dwType = 2;
               pb->fScrollFlip = TRUE;
            }
            break;

         case 2:  // graph
            // switch to boxes
            pb->dwType = 1;
            pb->fVPosMin = 0;
            pb->fVPosMax = VPOSMAX;
            pb->fScrollMin = 0;
            pb->fScrollMax = VPOSMAX+1;
            pb->fScrollFlip = FALSE;
            ShowWindow (pb->hWndList, SW_HIDE);
            break;
         }

         CalcBayMatrix (pb);
         NewScrollLoc (pb);
         InvalidateRect (pb->hWndData, NULL, FALSE);
         InvalidateRect (pb->hWndInfo, NULL, FALSE);
      }
      return 0;

   case WM_MOUSEMOVE:
      {
         int iX, iY;
         RECT r;
         iX = LOWORD(lParam);
         iY = HIWORD(lParam);
         GetClientRect (hWnd, &r);

         // set focus so mousewheel works - as opposed to getting taken by listbox
         SetFocus (m_hWnd);

         // if it's over the button then set to arrow
         if (pb->dwType && (iX >= r.right - 32) && (iY <= r.top + 16))
            SetCursor (LoadCursor(NULL, IDC_ARROW));
         else
            SetCursor (LoadCursor(NULL, IDC_NO));
      }

      return 0;

   case WM_MOUSEWHEEL:
      // pass it up
      return WndProc (hWnd, uMsg, wParam, lParam);

   case WM_KEYDOWN:
   case WM_SYSCHAR:
   case WM_CHAR:
      return TestForAccel (NULL, hWnd, uMsg, wParam, lParam);

   }

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*******************************************************************************************
CSceneView::BayDataWndProc - Handle the window procedure for the view
*/
LRESULT CSceneView::BayDataWndProc (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (uMsg == gdwMouseWheel)
      uMsg = WM_MOUSEWHEEL;

   switch (uMsg) {
   case WM_CREATE:
      {
      }
      return 0;

   case WM_DESTROY:
      // do nothing for now
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC   hDC;
         if (!pb)
            break;   // since dont really know what drawing

         hDC = BeginPaint (hWnd, &ps);

         RECT r;
         HBRUSH hbr;
         GetClientRect (hWnd, &r);

         HDC hNew;
         HBITMAP hBmp;
         hNew = CreateCompatibleDC (hDC);
         hBmp = CreateCompatibleBitmap (hDC, r.right, r.bottom);
         SelectObject (hNew, hBmp);

         // paint the grey behind the button bars and lines
         // dark grey
         hbr = (HBRUSH) CreateSolidBrush (pb->dwType ? RGB(0xff,0xff,0xff) : m_cBackLight);
         FillRect (hNew, &r, hbr);
         DeleteObject (hbr);

         // all the objects?
         if (pb->dwType != 0) {
            PCScene pScene = NULL;
            PCSceneObj pSceneObj = NULL;
            if (m_pSceneSet)
               m_pSceneSet->StateGet (&pScene, NULL);
            if (pScene)
               pSceneObj = pScene->ObjectGet (&pb->gWorldObject);

            // paint the selection
            if (m_fSelDraw && ((m_pSelBay == pb) || !m_pSelBay)) {
               CPoint p;
               p.p[0] = min(m_fSelStart, m_fSelEnd);
               p.p[1] = max(m_fSelStart, m_fSelEnd);
               p.p[2] = 0;
               p.p[3] = 1;
               p.MultiplyLeft (&pb->mTimeToPixel);
               p.p[0] = max(p.p[0], r.left);
               p.p[1] = min(p.p[1], r.right);
               if (p.p[0] < p.p[1]) {
                  RECT rSel;
                  rSel = r;
                  rSel.left = (int) p.p[0];
                  rSel.right = (int) p.p[1];
                  hbr = (HBRUSH) CreateSolidBrush (m_cSelColor);
                  FillRect (hNew, &rSel, hbr);
                  DeleteObject (hbr);
               }
            }

            // draw timeline ticks
            TimelineTicks (hNew, &r, &pb->mPixelToTime, &pb->mTimeToPixel,
                              RGB(0xe0,0xe0,0xe0), RGB(0xc0,0xc0,0xc0), pScene->FPSGet());

            // Will need way to indicate  that looking beyond the end of the scene's time
            // show that beyond end of scene
            fp fDuration;
            fDuration = pScene->DurationGet();
            if (m_fViewTimeEnd > fDuration) {
               CPoint p;
               p.Zero();
               p.p[0] = max(fDuration, m_fViewTimeStart);
               p.p[3] = 1;
               p.MultiplyLeft (&pb->mTimeToPixel);
               if (p.p[0] < r.right) {
                  RECT rSel;
                  rSel = r;
                  rSel.left = (int) p.p[0];
                  hbr = (HBRUSH) CreateSolidBrush (m_cBackMed);
                  FillRect (hNew, &rSel, hbr);
                  DeleteObject (hbr);
               }
            }

            if (!pSceneObj)
               goto drawplay;  // no object exists

            if (pb->dwType == 1) {
               // else, loop
               DWORD i;
               PCAnimSocket pas;
               for (i = 0; i < pSceneObj->ObjectNum(); i++) {
                  pas = pSceneObj->ObjectGet (i);
                  if (pas)
                     PaintAnimObject (hNew, &r, pb, pas);
               }
            }
            else if (pb->dwType == 2)
               BayPaintGraph (pb, hNew, &r);

drawplay:
            // draw play position
            fp fTime;
            m_pSceneSet->StateGet (NULL, &fTime);
            if ((fTime >= m_fViewTimeStart) && (fTime <= m_fViewTimeEnd)) {
               HPEN hPen, hPenOld;
               hPen = CreatePen (PS_DASH, 0, RGB(0xff,0,0));
               hPenOld = (HPEN) SelectObject (hNew, hPen);

               CPoint p;
               p.Zero();
               p.p[0] = fTime;
               p.p[3] = 1;
               p.MultiplyLeft (&pb->mTimeToPixel);

               MoveToEx (hNew, (int)p.p[0], r.top, NULL);
               LineTo (hNew, (int)p.p[0], r.bottom);

               SelectObject (hNew, hPenOld);
               DeleteObject (hPen);
            }

         }

         // bitblip
         BitBlt (hDC, 0, 0, r.right, r.bottom, hNew, 0, 0, SRCCOPY);

         DeleteDC (hNew);
         DeleteObject (hBmp);
         EndPaint (hWnd, &ps);
         return 0;
      }
      return 0;

   case WM_LBUTTONDOWN:
      return ButtonDown (pb, hWnd, uMsg, wParam, lParam, 0);
   case WM_MBUTTONDOWN:
      return ButtonDown (pb, hWnd, uMsg, wParam, lParam, 1);
   case WM_RBUTTONDOWN:
      return ButtonDown (pb, hWnd, uMsg, wParam, lParam, 2);
   case WM_LBUTTONUP:
      return ButtonUp (pb, hWnd, uMsg, wParam, lParam, 0);
   case WM_MBUTTONUP:
      return ButtonUp (pb, hWnd, uMsg, wParam, lParam, 1);
   case WM_RBUTTONUP:
      return ButtonUp (pb, hWnd, uMsg, wParam, lParam, 2);
   case WM_MOUSEMOVE:
      // set focus so mousewheel works - as opposed to getting taken by listbox
      SetFocus (m_hWnd);

      return MouseMove (pb, hWnd, uMsg, wParam, lParam);

   case WM_MOUSEWHEEL:
      // pass it up
      return WndProc (hWnd, uMsg, wParam, lParam);

   case WM_KEYDOWN:
   case WM_SYSCHAR:
   case WM_CHAR:
      return TestForAccel (NULL, hWnd, uMsg, wParam, lParam);

   case WM_VSCROLL:
      {
         // only deal with horizontal scroll
         HWND hWndScroll = hWnd;
         if (hWndScroll != pb->hWndData)
            break;
         if (!pb->dwType)
            break;   // ignore

         // get the scrollbar info
         SCROLLINFO si;
         memset (&si, 0, sizeof(si));
         si.cbSize = sizeof(si);
         si.fMask = SIF_ALL;
         GetScrollInfo (hWndScroll, SB_VERT, &si);
         
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
         si.nPos = max(0,si.nPos);

         // new values base on this
         fp fSize, fView;
         fSize = pb->fScrollMax - pb->fScrollMin;
         fView = pb->fVPosMax - pb->fVPosMin;
         fView = min(fView, fSize);
         if (pb->fScrollFlip) {
            pb->fVPosMax = pb->fScrollMax - (fp) si.nPos / 10000.0 * fSize;
            pb->fVPosMin = pb->fVPosMax - fView;
         }
         else {
            pb->fVPosMin = (fp) si.nPos / 10000.0 * fSize + pb->fScrollMin;
            pb->fVPosMax = pb->fVPosMin + fView;
         }
         NewScrollLoc (pb);

         return 0;
      }
      break;



   }

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/*******************************************************************************************
CSceneView::ButtonDown - Deal with WM_XBUTTONDOWN
*/
LRESULT CSceneView::ButtonDown (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton)
{
   DWORD dwPointerMode = m_adwPointerMode[dwButton];
   int iX, iY;
   iX = LOWORD(lParam);
   iY = HIWORD(lParam);

   CPoint pCur;
   pCur.Zero();
   pCur.p[0] = iX;
   pCur.p[2] = iY;
   pCur.p[3] = 1;
   pCur.MultiplyLeft (&pb->mPixelToTime);

   // BUGFIX - If a button is already down then ignore the new one
   if (m_dwButtonDown)
      return 0;

   // if this scene is disabled ignore
   if (!pb->dwType)
      return 0;

   if (pb->dwType == 2) {
      if ((dwPointerMode == IDC_GRAPHPOINTMOVE) || (dwPointerMode == IDC_GRAPHPOINTMOVEUD) ||
         (dwPointerMode == IDC_GRAPHPOINTDELETE)) {
         // find out what clicked on
         PCAnimAttrib paa;
         fp fTime, fValue;
         DWORD dwLinear;
         paa = GraphClosest (pb, iX, iY, &fTime, &fValue, &dwLinear,
            dwPointerMode == IDC_GRAPHPOINTMOVE);

         if (!paa) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }
         
         // if it's the right button then delete
         if (dwPointerMode == IDC_GRAPHPOINTDELETE) {
            TEXTUREPOINT tpOld;
            tpOld.h = fTime;
            tpOld.v = fValue;
            GraphMovePoint (pb, paa->m_szName, &tpOld, NULL, 0);
            return 0;
         }

         // else, moving
         m_fMCP = TRUE;
         wcscpy (m_szMCPAttrib, paa->m_szName);
         m_tpMCPOld.h = fTime;
         m_tpMCPOld.v = fValue;
         m_tpMCPDelta.h = fTime - pCur.p[0];
         m_tpMCPDelta.v = fValue - pCur.p[2];
         m_dwMCPLinear = dwLinear;

         goto capture;
      }
      else if (dwPointerMode != IDC_ZOOMIN) {
         // only allow zoomin to work in the graph
         BeepWindowBeep (ESCBEEP_DONTCLICK);
      }
   }

   if ((dwPointerMode == IDC_GRAPHPOINTMOVE) || (dwPointerMode == IDC_GRAPHPOINTMOVEUD) ||
      (dwPointerMode == IDC_GRAPHPOINTDELETE)) {
      // can only do these in type 2
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return 0;
   }
   else if (dwPointerMode == IDC_ANIMDEFAULT) {
      // set defaults
      PCScene pScene = NULL;
      PCSceneObj pSceneObj = NULL;
      m_pSceneSet->DefaultAttribForget (&pb->gWorldObject, NULL);
      m_pSceneSet->StateGet(&pScene, NULL);
      if (pScene)
         pSceneObj = pScene->ObjectGet (&pb->gWorldObject);
      if (pSceneObj)
         pSceneObj->AnimAttribDirty ();

      // Need some sort of beep to know it worked
      EscChime (ESCCHIME_INFORMATION);
      return 0;
   }
   else if ((dwPointerMode == IDC_POSITIONPASTE) || (dwPointerMode == IDC_OBJECTPASTE)) {
      // move the pasted stuff out of purgatory
      CPoint p;
      p.Zero();
      p.p[0] = iX;
      p.p[2] = iY;
      p.p[3] = 1;
      p.MultiplyLeft (&pb->mPixelToTime);
      // if no control key held down the automagically set the time to where editing
      if ((dwPointerMode == IDC_OBJECTPASTE) && !(GetKeyState (VK_CONTROL) < 0))
         m_pSceneSet->StateGet (NULL, &p.p[0]);
      MoveOutOfPurgatory(pb, p.p[0], p.p[2]);

      // remember the undo
      m_pSceneSet->UndoRemember();

      // change the pointer mode
      SetPointerMode (IDC_ANIMMOVELR, 0);
      return 0;
   }
   else if (dwPointerMode == IDC_OBJDECONSTRUCT) {
      PCAnimSocket pas = MouseToObject (pb, iX, iY, NULL);

      if (!pas || !pas->Deconstruct(FALSE)) {
         BeepWindowBeep (ESCBEEP_DONTCLICK);
         return 0;
      }

      // remember the guid
      GUID g;
      pas->GUIDGet (&g);

      // get the object socket
      PCScene pScene;
      PCSceneObj pSceneObj;
      GUID gObjectWorld;
      pScene = NULL;
      pas->WorldGet (NULL, &pScene, &gObjectWorld);
      if (!pScene)
         return 0;
      pSceneObj = pScene->ObjectGet (&gObjectWorld);
      if (!pSceneObj)
         return 0;


      // deconstruct
      if (pas->Deconstruct (TRUE)) {
         // find the object
         DWORD dwIndex;
         dwIndex = pSceneObj->ObjectFind (&g);
         pSceneObj->ObjectRemove (dwIndex);
      }

      // remember this as an undo point
      m_pSceneSet->UndoRemember();

      EscChime (ESCCHIME_INFORMATION);
      // don capture this
      return 0;
   }
   else if (dwPointerMode == IDC_OBJMERGE) {
      PCAnimSocket pas = MouseToObject (pb, iX, iY, NULL);

      if (!pas) {
         BeepWindowBeep (ESCBEEP_DONTCLICK);
         return 0;
      }

      // get the guid
      PCScene pScene = NULL;
      PCSceneObj pSceneObj = NULL;
      m_pSceneSet->StateGet (&pScene, NULL);
      if (pScene)
         pSceneObj = pScene->ObjectGet (&pb->gWorldObject);
      if (!pSceneObj)
         return 0;

      // get all the objects selected
      PSCENESEL pss;
      DWORD dwNum;
      pss = m_pSceneSet->SelectionEnum(&dwNum);
      DWORD i;
      GUID g;
      CListFixed lWith;
      lWith.Init (sizeof(GUID));
      pas->GUIDGet (&g);
      for (i = 0; i < dwNum; i++, pss++) {
         if (!IsEqualGUID (pss->gObjectWorld, pb->gWorldObject))
            continue;
         if (IsEqualGUID (pss->gObjectAnim, g))
            continue;

         // elst try to merge with
         lWith.Add (&pss->gObjectAnim);
      }
      if (!lWith.Num()) {
         EscMessageBox (m_hWnd, ASPString(),
            L"You must select at least one object to do a merge.",
            NULL,
            MB_ICONEXCLAMATION | MB_OK);
         return 0;
      }

      // set undo state
      m_pSceneSet->UndoRemember ();


      // merge
      BOOL fRet;
      fRet = pas->Merge ((GUID*) lWith.Get(0), lWith.Num());

      if (!fRet)
         EscMessageBox (m_hWnd, ASPString(),
            L"None of the objects merged.",
            L"Only certain objects will merge with one another; see the documentation.",
            MB_ICONINFORMATION | MB_OK);
      else
         EscChime (ESCCHIME_INFORMATION);

      // set undo state
      m_pSceneSet->UndoRemember ();

      return 0;
   }
   else if (dwPointerMode == IDC_OBJDIALOG) {
      PCAnimSocket pas = MouseToObject (pb, iX, iY, NULL);

      if (!pas || !pas->DialogQuery()) {
         BeepWindowBeep (ESCBEEP_DONTCLICK);
         return 0;
      }

      // bring up the dialog
      CEscWindow cWindow;
      RECT r;
      DialogBoxLocation (m_hWnd, &r);

      cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);

      pas->DialogShow (&cWindow);

      // remember this as an undo point
      m_pSceneSet->UndoRemember();

      // don capture this
      return 0;
   }
   else if ((dwPointerMode == IDC_ANIMMOVELR) || (dwPointerMode == IDC_ANIMMOVEUD))  {
      // if control is held down duplicate
      if (GetKeyState (VK_CONTROL) < 0) {
         // modify all selected objects
         DWORD dwNum, i;
         PSCENESEL pss;
         pss = m_pSceneSet->SelectionEnum (&dwNum);
         CListFixed lSel;
         lSel.Init (sizeof(SCENESEL), pss, dwNum);
         pss = (PSCENESEL) lSel.Get(0);
         dwNum = lSel.Num();

         PCScene pScene;
         pScene = NULL;
         m_pSceneSet->StateGet (&pScene, NULL);
         GUID gScene;
         if (pScene)
            pScene->GUIDGet (&gScene);
         for (i = 0; i < dwNum; i++, pss++) {
            PCSceneObj pSceneObj;
            PCAnimSocket pas;
            if (!IsEqualGUID (pss->gScene, gScene))
               continue;
            pSceneObj = pScene->ObjectGet (&pss->gObjectWorld);
            if (!pSceneObj)
               continue;
            pas = pSceneObj->ObjectGet (pSceneObj->ObjectFind(&pss->gObjectAnim));
            if (!pas)
               continue;

            // clone and add it
            pas = pas->Clone();
            if (!pas)
               continue;
            pSceneObj->ObjectAdd (pas);
         } // i

         // fall through
      }
   }
   else if (dwPointerMode == IDC_SELINDIVIDUAL) {
      // if control/shift are not held down then clear the old selection
      if (!(wParam & MK_CONTROL))
         m_pSceneSet->SelectionClear();

      // what object
      PCAnimSocket pas;
      pas = MouseToObject (pb, iX, iY);
      if (!pas)
         return FALSE;  // dont beep because is legit way of clearing

      GUID gScene, gObjectWorld, gObjectAnim;
      PCScene pScene;
      pas->GUIDGet (&gObjectAnim);
      pas->WorldGet (NULL, &pScene, &gObjectWorld);
      if (!pScene)
         return FALSE;
      pScene->GUIDGet (&gScene);

      if (m_pSceneSet->SelectionExists (&gScene, &gObjectWorld, &gObjectAnim))
         m_pSceneSet->SelectionRemove (&gScene, &gObjectWorld, &gObjectAnim);
      else
         m_pSceneSet->SelectionAdd (&gScene, &gObjectWorld, &gObjectAnim);

      return 0;   // all done
   }
   else if (dwPointerMode == IDC_ANIMRESIZE) {
      // see what clicked on
      m_pPasteDrag = MouseToObject (pb, iX, iY, &m_iResizeSide);
      if (!m_pPasteDrag || (m_iResizeSide == 0)) {
         BeepWindowBeep (ESCBEEP_DONTCLICK);
         return TRUE;
      }

      fp fStart, fEnd;
      m_pPasteDrag->TimeGet (&fStart, &fEnd);

      m_fResizeDelta = pCur.p[0] - ((m_iResizeSide < 0) ? fStart : fEnd);
   }

capture:
   // capture
   SetCapture (pb->hWndData);
   m_fCaptured = TRUE;

   if (!m_pTimerAutoScroll) {
      m_pTimerAutoScroll = pb;
      SetTimer (m_hWnd, TIMER_AUTOSCROLL, 125, 0);
   }

   // remember this position
   m_dwButtonDown = dwButton + 1;
   m_pntButtonDown.x = iX;
   m_pntButtonDown.y = iY;
   m_pButtonDown.Zero();
   m_pButtonDown.p[0] = iX;
   m_pButtonDown.p[2] = iY;
   m_pButtonDown.p[3] = 1;
   m_pButtonDown.MultiplyLeft (&pb->mPixelToTime);
   m_pntMouseLast = m_pntButtonDown;
   m_pMouseLast.Copy (&m_pButtonDown);

   if ((dwPointerMode == IDC_GRAPHPOINTMOVE) || (dwPointerMode == IDC_GRAPHPOINTMOVEUD)) {
      // pretend there's a mouse move since just clicked on a point/line, and if clicked
      // on a line want to add the point
      MouseMove (pb, hWnd, uMsg, wParam, lParam);
   }
   else if (dwPointerMode == IDC_SELREGION) {
      PCScene pScene;
      m_pSceneSet->StateGet (&pScene, NULL);

      m_fSelDraw = TRUE;
      m_pSelBay = (GetKeyState (VK_SHIFT) < 0) ? NULL : pb;
      CPoint p;
      p.Zero();
      p.p[0] = iX;
      p.p[1] = 1;
      p.MultiplyLeft (&pb->mPixelToTime);
      m_fSelStart = m_fSelEnd = pScene->ApplyGrid(p.p[0]);
      m_cSelColor = RGB(0xff, 0x80, 0x80);

      // refresh all the data displays
      InvalidateBayData ();
      return 0;
   }
   else if ((dwPointerMode == IDC_ANIMTIMEINSERT) || (dwPointerMode == IDC_ANIMTIMEREMOVE)) {
      PCScene pScene;
      m_pSceneSet->StateGet (&pScene, NULL);

      m_fSelDraw = TRUE;
      m_pSelBay = (GetKeyState (VK_SHIFT) < 0) ? NULL : pb;
      CPoint p;
      p.Zero();
      p.p[0] = iX;
      p.p[1] = 1;
      p.MultiplyLeft (&pb->mPixelToTime);
      m_fSelStart = m_fSelEnd = pScene->ApplyGrid(p.p[0]);
      m_cSelColor = (dwPointerMode == IDC_ANIMTIMEINSERT) ? RGB(0x80,0xff,0x80) :
         RGB(0xff, 0x80, 0x80);

      // refresh all the data displays
      InvalidateBayData ();
      return 0;
   }
   else if (dwPointerMode == IDC_ZOOMIN) {
      // remember the original zoom conditions
      m_pZoomOrig.p[0] = m_fViewTimeStart;
      m_pZoomOrig.p[1] = m_fViewTimeEnd;
      m_pZoomOrig.p[2] = pb->fVPosMin;
      m_pZoomOrig.p[3] = pb->fVPosMax;
   }
   else if (dwPointerMode == IDC_OBJECTPASTEDRAG) {
      // move the pasted stuff out of purgatory
      GUID gObjectAnim;
      CPoint p;
      p.Zero();
      p.p[0] = iX;
      p.p[2] = iY;
      p.p[3] = 1;
      p.MultiplyLeft (&pb->mPixelToTime);
      // if no control key held down the automagically set the time to where editing
      if (!(GetKeyState (VK_CONTROL) < 0))
         m_pSceneSet->StateGet (NULL, &p.p[0]);
      MoveOutOfPurgatory(pb, p.p[0], p.p[2], &gObjectAnim);

      // find the object
      PCScene pScene;
      PCSceneObj pSceneObj;
      PCAnimSocket pas;
      m_pSceneSet->StateGet (&pScene, NULL);
      if (!pScene)
         return 0;
      pSceneObj = pScene->ObjectGet (&pb->gWorldObject);
      if (!pScene)
         return 0;
      pas = pSceneObj->ObjectGet(pSceneObj->ObjectFind (&gObjectAnim));
      if (!pas)
         return 0;

      fp fStart, fEnd;
      pas->TimeGet (&fStart, &fEnd);
      pas->TimeSet (pScene->ApplyGrid(fStart), pScene->ApplyGrid(fStart));

      // remember this
      m_pPasteDrag = pas;

      // simlate a mouse move so that wont suddenly change size when move mouse
      MouseMove (pb, hWnd, uMsg, wParam, lParam);
   }
   else if (dwPointerMode == IDC_ANIMPLAYAUDIO) {
      if (m_hWaveOut)
         return 0;   // shouldnt happen, but test

      // figure out what clicked on
      // what object
      m_pAnimWave = MouseToObject (pb, iX, iY);
      m_pObjWave = NULL;

      // if we are dealing with a graph object then always do entire object
      if (pb->dwType == 2)
         m_pAnimWave = NULL;


      // get the sceneobj
      PCScene pScene;
      if (!m_pSceneSet)
         return NULL;
      m_pSceneSet->StateGet (&pScene, NULL);
      if (!pScene)
         return NULL;
      if (!m_pAnimWave)
         m_pObjWave = pScene->ObjectGet (&pb->gWorldObject);
      if (!m_pObjWave && !m_pAnimWave)
         return NULL;

      // figure out the time clicked on
      CPoint p;
      p.Zero();
      p.p[0] = iX;
      p.p[1] = 1;
      p.MultiplyLeft (&pb->mPixelToTime);
      m_fPlayCur = p.p[0];

      // figure out wave format
      m_dwWaveSamplesPerSec = m_dwWaveChannels = 0;
      if (m_pAnimWave)
         m_pAnimWave->WaveFormatGet (&m_dwWaveSamplesPerSec, &m_dwWaveChannels);
      else
         m_pObjWave->WaveFormatGet (&m_dwWaveSamplesPerSec, &m_dwWaveChannels);
      if (!m_dwWaveSamplesPerSec)
         m_dwWaveSamplesPerSec = 11025;
      if (!m_dwWaveChannels)
         m_dwWaveChannels = 1;

      // size of the buffer
      m_dwPlayBufSize = (m_dwWaveSamplesPerSec / 8) * m_dwWaveChannels * sizeof(short);

      // allocate enough memory
      if (!m_memPlay.Required (m_dwPlayBufSize * WVPLAYBUF + m_dwPlayBufSize * sizeof(float)/sizeof(short)))
         return 0;
      PBYTE pCur;
      m_pafPlayTemp = (float*) m_memPlay.p;
      pCur = (PBYTE) (m_pafPlayTemp + m_dwPlayBufSize / sizeof(short));

      // open the wave
      MMRESULT mm;
      WAVEFORMATEX WFEX;
      memset (&WFEX, 0, sizeof(WFEX));
      WFEX.cbSize = 0;
      WFEX.wFormatTag = WAVE_FORMAT_PCM;
      WFEX.nChannels = m_dwWaveChannels;
      WFEX.nSamplesPerSec = m_dwWaveSamplesPerSec;
      WFEX.wBitsPerSample = 16;
      WFEX.nBlockAlign  = WFEX.nChannels * WFEX.wBitsPerSample / 8;
      WFEX.nAvgBytesPerSec = WFEX.nBlockAlign * WFEX.nSamplesPerSec;
      mm = waveOutOpen (&m_hWaveOut, WAVE_MAPPER, &WFEX, (DWORD_PTR) m_hWnd, NULL,
         CALLBACK_WINDOW);
      if (!m_hWaveOut)
         return 0;   // error

      // prepare the headers
      memset (m_aPlayWaveHdr, 0, sizeof(m_aPlayWaveHdr));
      DWORD i;
      for (i = 0; i < WVPLAYBUF; i++) {
         m_aPlayWaveHdr[i].dwBufferLength = m_dwPlayBufSize;
         m_aPlayWaveHdr[i].lpData = (PSTR) ((PBYTE) pCur + i * m_dwPlayBufSize);
         mm = waveOutPrepareHeader (m_hWaveOut, &m_aPlayWaveHdr[i], sizeof(m_aPlayWaveHdr[i]));
      }

      // write them out
      waveOutPause(m_hWaveOut);
      for (i = 0; i < WVPLAYBUF; i++)
         PlayAddBuffer (&m_aPlayWaveHdr[i]);
      waveOutRestart (m_hWaveOut);

   }

   return 0;
}



/**********************************************************************************
CSceneView::PlayAddBuffer - Adds a buffer to the playlist. NOTE: This also increases
the number of buffers out counter.

inputs
   PWAVEHDR          pwh - Wave header to use. This is assumed to already be returned
returns
   BOOL - TRUE if sent out. FALSE if didnt
*/
BOOL CSceneView::PlayAddBuffer (PWAVEHDR pwh)
{
   if (!m_hWaveOut || (pwh->dwFlags & WHDR_INQUEUE))
      return FALSE;  // not opened

   // how many samples need?
   DWORD dwSamples = m_dwPlayBufSize / (m_dwWaveChannels * sizeof(short));

   // convert current location to integer
   int iSec, iFrac;
   iSec = floor (m_fPlayCur);
   iFrac = (int)((m_fPlayCur - (double)iSec) * (double)m_dwWaveSamplesPerSec);
   m_fPlayCur += (double)dwSamples / (double)m_dwWaveSamplesPerSec;

   fp fVolume;
   if (m_pAnimWave) {
      if (!m_pAnimWave->WaveGet (m_dwWaveSamplesPerSec, m_dwWaveChannels,
         iSec, iFrac, dwSamples, (short*) pwh->lpData, &fVolume))
         memset (pwh->lpData, 0, m_dwPlayBufSize);
   }
   else if (m_pObjWave) {
      // wipe out the memory to sum
      memset (m_pafPlayTemp, 0, dwSamples * m_dwWaveChannels * sizeof(float));

      // sum in
      DWORD i, j;
      for (i = 0; i < m_pObjWave->ObjectNum(); i++) {
         PCAnimSocket pas = m_pObjWave->ObjectGet (i);
         if (!pas)
            continue;
         if (!pas->WaveGet (m_dwWaveSamplesPerSec, m_dwWaveChannels,
            iSec, iFrac, dwSamples, (short*) pwh->lpData, &fVolume))
            continue;   // nothing

         // else, have sound
         for (j = 0; j < dwSamples * m_dwWaveChannels; j++)
            m_pafPlayTemp[j] += (fp)(((short*)pwh->lpData)[j]) * fVolume;
      } // i

      // store away
      for (j = 0; j < dwSamples * m_dwWaveChannels; j++) {
         m_pafPlayTemp[j] = max(-32768, m_pafPlayTemp[j]);
         m_pafPlayTemp[j] = min(32767, m_pafPlayTemp[j]);
         (((short*)pwh->lpData)[j]) = (short)m_pafPlayTemp[j];
      }
   }
   else
      return FALSE;

   pwh->dwBufferLength = m_dwPlayBufSize;

   // else, add
   MMRESULT mm;
   mm = waveOutWrite (m_hWaveOut, pwh, sizeof(WAVEHDR));

   // done
   return TRUE;
}


/*******************************************************************************************
CSceneView::ButtonUp - Deal with WM_XBUTTONUP
*/
LRESULT CSceneView::ButtonUp (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton)
{
   DWORD dwPointerMode = m_adwPointerMode[dwButton];

   // if this scene is disabled ignore
   if (!pb->dwType)
      return 0;

   // BUGFIX - If the button is not the correct one then ignore
   if (m_dwButtonDown && (dwButton+1 != m_dwButtonDown))
      return 0;

   if (m_fCaptured)
      ReleaseCapture ();
   if (m_pTimerAutoScroll) {
      m_pTimerAutoScroll = NULL;
      KillTimer (m_hWnd, TIMER_AUTOSCROLL);
   }
   m_fCaptured = FALSE;

   // kill the timer if it exists
   //if (m_dwMoveTimerID)
   //   KillTimer (m_hWnd, m_dwMoveTimerID);
   //m_dwMoveTimerID = 0;

   // BUGFIX - If don't have buttondown flag set then dont do any of the following
   // operations. Otherwise have problem which createa a new object, click on wall to
   // create, and ends up getting mouse-up in this window (from the mouse down in the
   // object selection window)
   if (!m_dwButtonDown)
      return 0;

   m_dwButtonDown = 0;

   if ((dwPointerMode == IDC_GRAPHPOINTMOVE) || (dwPointerMode == IDC_GRAPHPOINTMOVEUD)) {
      // if it's a graph then just note that no-longer dragging and done
      m_fMCP = FALSE;
      return 0;
   }
   else if (dwPointerMode == IDC_SELREGION) {
      // refresh all the data displays
      m_fSelDraw = FALSE;
      InvalidateBayData ();

      PCScene pScene;
      GUID gScene, gObjectWorld, gObjectAnim;
      if (!(wParam & MK_CONTROL))
         m_pSceneSet->SelectionClear();
      m_pSceneSet->StateGet (&pScene, NULL);
      if (!pScene)
         return NULL;
      pScene->GUIDGet (&gScene);

      // calculate the min/max
      fp fMin, fMax;
      fMin = min(m_fSelStart, m_fSelEnd);
      fMax = max(m_fSelStart, m_fSelEnd);

      // so know how much time per pixel
      CPoint p, pDelta;
      p.Zero();
      p.p[3] = 1;
      pDelta.Copy (&p);
      pDelta.p[0] += 1; // so know how large delta is
      p.MultiplyLeft (&pb->mPixelToTime);
      pDelta.MultiplyLeft (&pb->mPixelToTime);
      pDelta.Subtract (&p);

      DWORD i, j;
      PCSceneObj pSceneObj;
      PCAnimSocket pas;
      fp fStart, fEnd;
      int iMinX, iMinY;
      for (i = 0; i < pScene->ObjectNum(); i++) {
         pSceneObj = pScene->ObjectGet(i);
         if (!pSceneObj)
            continue;
         pSceneObj->GUIDGet (&gObjectWorld);
         if (m_pSelBay && !IsEqualGUID(m_pSelBay->gWorldObject, gObjectWorld))
            continue;

         for (j = 0; j < pSceneObj->ObjectNum(); j++) {
            pas = pSceneObj->ObjectGet(j);
            if (!pas)
               continue;

            // locations
            pas->TimeGet (&fStart, &fEnd);

            // out of bounds?
            if (fMax < fStart)
               continue;

            // account for wideth
            pas->QueryMinDisplay (&iMinX, &iMinY);
            fEnd = max(fEnd, fStart + (fp)iMinX * pDelta.p[0]);

            // other bounds?
            if (fMin > fEnd)
               continue;

            pas->GUIDGet (&gObjectAnim);
            m_pSceneSet->SelectionAdd (&gScene, &gObjectWorld, &gObjectAnim);
         }
      }
      return 0;
   }
   else if ((dwPointerMode == IDC_ANIMTIMEINSERT) || (dwPointerMode == IDC_ANIMTIMEREMOVE)) {
      // refresh all the data displays
      m_fSelDraw = FALSE;
      InvalidateBayData ();

      PCScene pScene;
      GUID gScene, gObjectWorld;
      m_pSceneSet->StateGet (&pScene, NULL);
      if (!pScene)
         return NULL;
      pScene->GUIDGet (&gScene);

      // calculate the min/max
      fp fMin, fMax;
      fMin = min(m_fSelStart, m_fSelEnd);
      fMax = max(m_fSelStart, m_fSelEnd);

      DWORD i, j;
      PCSceneObj pSceneObj;
      PCAnimSocket pas;
      fp fStart, fEnd, fDelta, fLen;
      fDelta = fMax - fMin;
      if (dwPointerMode == IDC_ANIMTIMEREMOVE)
         fDelta *= -1;
      for (i = 0; i < pScene->ObjectNum(); i++) {
         pSceneObj = pScene->ObjectGet(i);
         if (!pSceneObj)
            continue;
         pSceneObj->GUIDGet (&gObjectWorld);
         if (m_pSelBay && !IsEqualGUID(m_pSelBay->gWorldObject, gObjectWorld))
            continue;

         for (j = pSceneObj->ObjectNum()-1; j < pSceneObj->ObjectNum(); j--) {
            pas = pSceneObj->ObjectGet(j);
            if (!pas)
               continue;

            // locations
            pas->TimeGet (&fStart, &fEnd);
            fLen = fEnd - fStart;

            // if before changes then nothing
            if (fEnd <= fMin)
               continue;

            // if entirely after then easy
            if (fStart >= fMax) {
               fStart = max(0, fStart + fDelta);
               fEnd = fStart + fLen;
               pas->TimeSet (fStart, fEnd);
               continue;
            }

            // if the object is entirely within then remove
            if ((fDelta < 0) && (fStart >= fMin) && (fEnd <= fMax)) {
               pSceneObj->ObjectRemove (j);
               continue;
            }

            // so if get here, must be some overlap
            if (fDelta >= 0) {
               // inserting time
               if (fStart >= fMin)
                  fStart += fDelta;
               if (fEnd >= fMin)
                  fEnd += fDelta;
               pas->TimeSet (fStart, fEnd);
               continue;
            }
            else {
               // deleting time
               if (fStart >= fMin)
                  fStart = max(fStart + fDelta, fMin);
               if (fEnd >= fMin)
                  fEnd = max(fEnd + fDelta, fMin);
               fEnd = max(fStart, fEnd);
               pas->TimeSet (fStart, fEnd);
               continue;
            }
         } // j
      } // i

      // extend/shrink timeline?
      if (!m_pSelBay) {
         fp fDuration = pScene->DurationGet ();
         fp fFrame;

         // adjust delta so cand delte more than the end
         if (fDelta < 0) {
            fMin = min(fMin, fDuration);
            fMax = min(fMax, fDuration);
            fDelta = -(fMax - fMin);
         }
         fDuration += fDelta;
         fFrame = pScene->FPSGet() ? (1.0 / (fp) pScene->FPSGet()) : 1;
         fDuration = max(fDuration, fFrame);
         fDuration = pScene->ApplyGrid (fDuration);

         pScene->DurationSet(fDuration);

         // timeline extend
         fp fScene;
         m_pTimeline->LimitsGet (&fStart, &fEnd, &fScene);
         fStart = min(fStart, fDuration);
         fEnd = min(fEnd, fDuration);
         m_pTimeline->LimitsSet (pScene->ApplyGrid(fStart), pScene->ApplyGrid(fEnd), fDuration);
         
         // dont play beypnd end
         m_pSceneSet->StateGet (&pScene, &fStart);
         fStart = min(fStart, fDuration);
         m_pSceneSet->StateSet (pScene, fStart);

         // redraw
         NewScrollLoc ();
         InvalidateBayData();
      }

      return 0;
   }
   else if (dwPointerMode == IDC_OBJECTPASTEDRAG) {
      m_pPasteDrag = NULL;

      // remember the undo
      m_pSceneSet->UndoRemember();

      // change the pointer mode
      SetPointerMode (IDC_ANIMMOVELR, 0);
   }
   else if (dwPointerMode == IDC_ANIMRESIZE) {
      m_pPasteDrag = NULL;
   }
   else if (dwPointerMode == IDC_ANIMPLAYAUDIO) {
      if (m_hWaveOut) {
         waveOutReset (m_hWaveOut);
         waveOutClose (m_hWaveOut);
      }
      m_hWaveOut = NULL;
   }

   return 0;
}

/*******************************************************************************************
CSceneView::MouseMove - Deal with WM_MOUSEMOVE.
*/
LRESULT CSceneView::MouseMove (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   DWORD dwButton = m_dwButtonDown ? (m_dwButtonDown - 1) : 0;
   DWORD dwPointerMode = m_adwPointerMode[dwButton];
   int iX, iY;
   iX = (short) LOWORD(lParam);
   iY = (short) HIWORD(lParam);

   CPoint pCur;
   pCur.Zero();
   pCur.p[0] = iX;
   pCur.p[2] = iY;
   pCur.p[3] = 1;
   pCur.MultiplyLeft (&pb->mPixelToTime);

   // if this scene is disabled ignore
   if (!pb->dwType) {
      SetCursor (LoadCursor (NULL, IDC_NO));
      return 0;
   }

   // set the cursor
   SetProperCursor (pb, iX, iY);

   if (!m_dwButtonDown)
      return 0;   // do nothing unles button down

   if ((dwPointerMode == IDC_GRAPHPOINTMOVE) || (dwPointerMode == IDC_GRAPHPOINTMOVEUD)) {
      // it's a graph display
      if (m_fMCP) {
         BOOL fGrid = (dwPointerMode != IDC_GRAPHPOINTMOVEUD);

         PCScene pScene;
         m_pSceneSet->StateGet (&pScene, NULL);

         // move
         TEXTUREPOINT tpNew;
         if (fGrid) {
            tpNew.h = pCur.p[0] + m_tpMCPDelta.h;
            tpNew.h = pScene->ApplyGrid (tpNew.h);
            tpNew.h = max(tpNew.h, 0);
         }
         else
            tpNew.h = m_tpMCPOld.h; // keep time constant
         tpNew.v = pCur.p[2] + m_tpMCPDelta.v;

         // move
         GraphMovePoint (pb, m_szMCPAttrib, &m_tpMCPOld, &tpNew, m_dwMCPLinear);
         m_tpMCPOld = tpNew;
      }
      // fall through
   }
   else if ((dwPointerMode == IDC_SELREGION) || (dwPointerMode == IDC_ANIMTIMEINSERT) ||
      (dwPointerMode == IDC_ANIMTIMEREMOVE)) {
      PCScene pScene;
      m_pSceneSet->StateGet (&pScene, NULL);
      m_pSelBay = (GetKeyState (VK_SHIFT) < 0) ? NULL : pb;
      CPoint p;
      p.Zero();
      p.p[0] = iX;
      p.p[1] = 1;
      p.MultiplyLeft (&pb->mPixelToTime);
      m_fSelEnd = pScene->ApplyGrid(p.p[0]);

      // refresh all the data displays
      InvalidateBayData ();
      // fall through
   }
   else if ((dwPointerMode == IDC_ANIMMOVELR) || (dwPointerMode == IDC_ANIMMOVEUD))  {
      // need to apply grid
      PCScene pScene;
      m_pSceneSet->StateGet (&pScene, NULL);
      if (!pScene)
         goto done;

      // apply grid to new loc
      fp fBeforeGrid;
      fBeforeGrid = pCur.p[0];
      pCur.p[0] = pScene->ApplyGrid(pCur.p[0]);

      // find the change
      BOOL fUD = (dwPointerMode == IDC_ANIMMOVEUD);
      CPoint pDelta;
      pDelta.Subtract (&pCur, &m_pMouseLast);
      if (!pDelta.p[0] && !pDelta.p[2]) {
         pCur.p[0] = fBeforeGrid;
         goto done;
      }

      // modify all selected objects
      DWORD dwNum, i;
      PSCENESEL pss;
      pss = m_pSceneSet->SelectionEnum (&dwNum);
      GUID gScene;
      pScene->GUIDGet (&gScene);
      for (i = 0; i < dwNum; i++, pss++) {
         PCSceneObj pSceneObj;
         PCAnimSocket pas;
         if (!IsEqualGUID (pss->gScene, gScene))
            continue;
         pSceneObj = pScene->ObjectGet (&pss->gObjectWorld);
         if (!pSceneObj)
            continue;
         pas = pSceneObj->ObjectGet (pSceneObj->ObjectFind(&pss->gObjectAnim));
         if (!pas)
            continue;

         fp fStart, fEnd, fVert, fDelta, fLen;
         if (!fUD) {
            pas->TimeGet (&fStart, &fEnd);
            fLen = fEnd - fStart;
            fDelta = (fStart + pDelta.p[0] < 0) ? -fStart : pDelta.p[0];
            fStart += fDelta;
            fStart = pScene->ApplyGrid (fStart);
            fEnd = fStart + fLen;
            pas->TimeSet (fStart, fEnd);
         }

         // both do vertical
         fVert = pas->VertGet();
         fVert += pDelta.p[2];
         fVert = max(0, fVert);
         fVert = min(VPOSMAX, fVert);
         pas->VertSet (fVert);
      } // i

      // fall through
      // restore current to before grid
      pCur.p[0] = fBeforeGrid;
   }
   else if (dwPointerMode == IDC_ZOOMIN) {
      // amount to zoom
      fp fZoomLR, fZoomUD;
      fZoomLR = pow (2, fabs((fp)(iX - m_pntButtonDown.x)) / 200.0);
      if (iX > m_pntButtonDown.x)
         fZoomLR = 1.0 / fZoomLR;
      fZoomUD = pow (2, fabs((fp)(iY - m_pntButtonDown.y)) / 200.0);
      if (iY < m_pntButtonDown.y)
         fZoomUD = 1.0 / fZoomUD;

      // center of the zoom
      fp fCenterLR, fCenterUD;
      fCenterLR = (m_pZoomOrig.p[0] + m_pZoomOrig.p[1]) / 2;
      fCenterUD = (m_pZoomOrig.p[2] + m_pZoomOrig.p[3]) / 2;

      // scale of zoom
      fp fScaleLR, fScaleUD;
      fScaleLR = (m_pZoomOrig.p[1] - m_pZoomOrig.p[0]) / 2;
      fScaleUD = (m_pZoomOrig.p[3] - m_pZoomOrig.p[2]) / 2;
      fScaleLR *= fZoomLR;
      fScaleUD *= fZoomUD;
      fScaleLR = max(.001, fScaleLR);
      fScaleUD = max((pb->fScrollMax - pb->fScrollMin) / 1000, fScaleUD);

      // new values
      PCScene pScene;
      m_pSceneSet->StateGet (&pScene, NULL);
      m_fViewTimeStart = max(0, fCenterLR - fScaleLR);
      m_fViewTimeEnd = min((pScene ? pScene->DurationGet() : 0) +60, fCenterLR + fScaleLR);

      pb->fVPosMin = max(pb->fScrollMin, fCenterUD - fScaleUD);
      pb->fVPosMax = min(pb->fScrollMax, fCenterUD + fScaleUD);
      pb->fVPosMin = min(pb->fScrollMax, pb->fVPosMin);
      pb->fVPosMax = max(pb->fScrollMin, pb->fVPosMax);
      fp fMin;
      fMin = (pb->fScrollMax - pb->fScrollMin) / 1000;
      pb->fVPosMax = max(pb->fVPosMax, pb->fVPosMin + fMin);
      if (pb->fVPosMax > pb->fScrollMax) {
         pb->fVPosMin -= (pb->fVPosMax - pb->fScrollMax);
         pb->fVPosMax = pb->fScrollMax;
      }

      // update scroll location
      NewScrollLoc();
      NewScrollLoc (pb);
   }
   else if (dwPointerMode == IDC_OBJECTPASTEDRAG) {
      fp fStart, fEnd;
      m_pPasteDrag->TimeGet (&fStart, &fEnd);

      PCScene pScene;
      m_pSceneSet->StateGet (&pScene, NULL);
      pCur.p[0] = pScene->ApplyGrid (pCur.p[0]);
      fEnd = max(fStart, pCur.p[0]);
      m_pPasteDrag->TimeSet (fStart, fEnd);
   }
   else if (dwPointerMode == IDC_ANIMRESIZE) {
      // current loc
      fp fCur;
      fCur = pCur.p[0] + m_fResizeDelta;
      fCur = max(0, fCur);

      // need to apply grid
      PCScene pScene;
      m_pSceneSet->StateGet (&pScene, NULL);
      if (!pScene)
         goto done;

      fp fStart, fEnd;
      if (m_pPasteDrag) {
         m_pPasteDrag->TimeGet (&fStart, &fEnd);
         if (m_iResizeSide < 0) {
            fStart = pScene->ApplyGrid(fCur);
            fStart = min(fEnd, fStart);
         }
         else {
            fEnd = pScene->ApplyGrid (fCur);
            fEnd = max(fEnd, fStart);
         }
         m_pPasteDrag->TimeSet (fStart, fEnd);
      }
   }
   else if (dwPointerMode == IDC_ANIMPLAYAUDIO) {
      if (m_hWaveOut) {
         waveOutReset (m_hWaveOut);
         waveOutPause (m_hWaveOut);
         m_fPlayCur = pCur.p[0];
         DWORD i;
         for (i = 0; i < WVPLAYBUF; i++)
            PlayAddBuffer (&m_aPlayWaveHdr[i]);
         waveOutRestart (m_hWaveOut);
      }
   }

done:
   // remember mouse last
   m_pMouseLast.Copy (&pCur);
   m_pntMouseLast.x = iX;
   m_pntMouseLast.y = iY;

   return 0;
}

/*******************************************************************************************
CSceneView::Paint - Deal with WM_PAINT
*/
LRESULT CSceneView::Paint (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   PAINTSTRUCT ps;
   HDC   hDC;
   hDC = BeginPaint (hWnd, &ps);

   RECT rt, r;
   HBRUSH hbr;
   GetClientRect (hWnd, &r);

   // paint the grey behind the button bars and lines
   // dark grey
   hbr = (HBRUSH) CreateSolidBrush (m_cBackDark);
   // top
   rt = r;
   rt.bottom = VARBUTTONSIZE;
   FillRect (hDC, &rt, hbr);
   // bottom
   rt = r;
   rt.top = rt.bottom - VARBUTTONSIZE;
   FillRect (hDC, &rt, hbr);
   DeleteObject (hbr);


   // light selection color
   hbr = (HBRUSH) CreateSolidBrush (m_cBackLight);
   // just blank entire section
   rt = r;
   rt.bottom -= VARBUTTONSIZE;
   rt.top += VARBUTTONSIZE;
   FillRect (hDC, &rt, hbr);
   DeleteObject (hbr);

   // line around the tabs
   HPEN hPenOld, hPen;
   hPen = CreatePen (PS_SOLID, 0, m_cBackOutline);
   hPenOld = (HPEN) SelectObject (hDC, hPen);
   DWORD i;
   for (i = 0; i < 2; i++) {
      PCButtonBar pbb;
      DWORD dwDim;   // 0 for drawing along X, 1 for drawing along Y
      int iX, iY, iStart, iStop;
      iX = iY = 0;
      switch (i) {
      case 0:
         pbb = m_pbarTop;
         dwDim = 0;
         iY = r.top + VARBUTTONSIZE;
         break;
      case 1:
         pbb = m_pbarBottom;
         dwDim = 0;
         iY = r.bottom - VARBUTTONSIZE - 1;
         break;
      }
      iStart = (dwDim == 0) ? (r.left) : (r.top);
      iStop = (dwDim == 0) ? (r.right) : (r.bottom);

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

   // done
   EndPaint (hWnd, &ps);
   return 0;
}

/*******************************************************************************************
CSceneView::UpdateNumBays - Creates or removes bays as necessary to produce the
right number of bays.

inputs
   DWORD       dwWant - Number of bays that want
   BOOL        fUpdateDisplay - If TRUE when fake a WM_SIZE to update the positions, else
                  dont bother (such as if just shutting down)
returns
   none
*/
void CSceneView::UpdateNumBays (DWORD dwWant, BOOL fUpdateDisplay)
{
   while (dwWant < m_lSVBAY.Num()) {
      DWORD dwNum = m_lSVBAY.Num()-1;
      PSVBAY pv = (PSVBAY) m_lSVBAY.Get(dwNum);
      
      if (pv->hWndData)
         DestroyWindow (pv->hWndData);
      if (pv->hWndInfo)
         DestroyWindow (pv->hWndInfo);
      if (pv->hWndList)
         DestroyWindow (pv->hWndList);
      if (pv->plATTRIBVAL)
         delete pv->plATTRIBVAL;

      m_lSVBAY.Remove (dwNum);
   }


   // create windows that dont exist
   while (dwWant > m_lSVBAY.Num()) {
      SVBAY b, *pb;
      memset (&b, 0, sizeof(b));
      m_lSVBAY.Add (&b);
      pb = (PSVBAY) m_lSVBAY.Get(m_lSVBAY.Num()-1);

      pb->dwType = 0;
      pb->pSceneView = this;
      pb->hWndInfo = CreateWindow ("CSceneViewBayInfo", "",
         WS_CHILD, 0, 0, 1, 1, m_hWnd, NULL, ghInstance, this);
      pb->hWndData = CreateWindowEx (WS_EX_CLIENTEDGE, "CSceneViewBayData", "",
         WS_CHILD | WS_VSCROLL, 0, 0, 1, 1, m_hWnd, NULL, ghInstance, this);
      pb->hWndList = CreateWindowEx (WS_EX_CLIENTEDGE, "LISTBOX", "",
         WS_CHILD | WS_VSCROLL |
         LBS_DISABLENOSCROLL | LBS_MULTIPLESEL |LBS_NOTIFY |
         LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED,
         0, 0, 1, 1, pb->hWndInfo,
         (HMENU) IDC_LIST, ghInstance, NULL);
      pb->plATTRIBVAL = new CListFixed();
      if (pb->plATTRIBVAL)
         pb->plATTRIBVAL->Init (sizeof(ATTRIBVAL));
      NewScrollLoc (pb);
   }

   // fake size
   if (fUpdateDisplay) {
      RECT r;
      GetClientRect (m_hWnd, &r);
      WndProc (m_hWnd, WM_SIZE, 0, MAKELPARAM (r.right - r.left, r.bottom - r.top)); 
   }
}


/**********************************************************************************
CSceneView::ClipboardUpdatePasteButton - Sets the m_pPaste button to enalbed/disabled
depending on whether the data in the clipboard is valid.
*/
void CSceneView::ClipboardUpdatePasteButton (void)
{
   if (!m_hWnd || !m_pPaste)
      return;  // not all initialized yet

   // open the clipboard and find the right data
   if (!OpenClipboard(m_hWnd))
      return;

   // get the data
   HANDLE hMem;
   hMem = GetClipboardData (m_dwClipFormat);
   CloseClipboard ();

   m_pPaste->Enable (hMem ? TRUE : FALSE);
}


/**********************************************************************************
CSceneView::TestForAccel - Look at the keyboard and figure out what to do

inputs
   PVOID       pbay - PSVBAY to use. NULL if main window
*/
LRESULT CSceneView::TestForAccel (PSVBAY pb, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_KEYDOWN:
      {
         // else try accelerators
         if (m_pbarTop->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarBottom->TestAccelerator(uMsg, wParam, lParam))
            return 0;
      }
      break;

   case WM_SYSCHAR:
      {
         // else try accelerators
         if (m_pbarTop->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarBottom->TestAccelerator(uMsg, wParam, lParam))
            return 0;
      }
      break;

   case WM_CHAR:
      {
         // else try accelerators
         if (m_pbarTop->TestAccelerator(uMsg, wParam, lParam))
            return 0;
         if (m_pbarBottom->TestAccelerator(uMsg, wParam, lParam))
            return 0;
      }
      break;

   }
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/**********************************************************************************
CSceneView::SetProperCursor - Call this to set the cursor to whatever is appropriate
for the window
*/
void CSceneView::SetProperCursor (PSVBAY pBay, int iX, int iY)
{
   DWORD dwButton = m_dwButtonDown ? (m_dwButtonDown-1) : 0;

   // if in graph mode only allow for point move and zoom
   if (pBay->dwType == 2)
      switch (m_adwPointerMode[dwButton]) {
         case IDC_GRAPHPOINTMOVE:
         case IDC_GRAPHPOINTMOVEUD:
         case IDC_GRAPHPOINTDELETE:
         case IDC_ZOOMIN:
         case IDC_ANIMPLAYAUDIO:
            break;
         default:
            SetCursor(LoadCursor (NULL, IDC_NO));
            return;
      };

   switch (m_adwPointerMode[dwButton]) {
      case IDC_ANIMDEFAULT:
         SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORANIMDEFAULT)));
         break;

      case IDC_POSITIONPASTE:
         SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORPASTE)));
         break;

      case IDC_OBJECTPASTE:
         SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSOROBJECTPASTE)));
         break;
      case IDC_OBJECTPASTEDRAG:
         SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSOROBJECTPASTEDRAG)));
         break;

      case IDC_SELINDIVIDUAL:
         {
            // take this out because distracting
            //PCAnimSocket pas = MouseToObject (pBay, iX, iY);
            //SetCursor (LoadCursor (NULL, pas ? IDC_ARROW : IDC_NO));
            SetCursor (LoadCursor (NULL, IDC_ARROW));
         }
         break;

      case IDC_OBJDIALOG:
         {
            PCAnimSocket pas = MouseToObject (pBay, iX, iY, NULL);

            if (pas && pas->DialogQuery())
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORDIALOG)));
            else
               SetCursor(LoadCursor (NULL, IDC_NO));
         }
         break;

      case IDC_OBJMERGE:
         {
            PCAnimSocket pas = MouseToObject (pBay, iX, iY, NULL);

            if (pas)
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSOROBJMERGE)));
            else
               SetCursor(LoadCursor (NULL, IDC_NO));
         }
         break;

      case IDC_OBJDECONSTRUCT:
         {
            PCAnimSocket pas = MouseToObject (pBay, iX, iY, NULL);

            if (pas && pas->Deconstruct(FALSE))
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORDECONSTRUCT)));
            else
               SetCursor(LoadCursor (NULL, IDC_NO));
         }
         break;
      case IDC_ANIMRESIZE:
         {
            if (!m_pPasteDrag) {
               PCAnimSocket pas = MouseToObject (pBay, iX, iY, &m_iResizeSide);
               if (!pas)
                  m_iResizeSide = 0;
            }

            if (!m_iResizeSide)
               SetCursor (LoadCursor (NULL, IDC_ARROW));
            else if (m_iResizeSide < 0)
               SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORANIMRESIZEL)));
            else
               SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORANIMRESIZER)));
         }
         break;

      case IDC_GRAPHPOINTMOVE:
      case IDC_GRAPHPOINTMOVEUD:
         if (pBay->dwType != 2)
            SetCursor (LoadCursor (NULL, IDC_NO));
         else if (m_fMCP)
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(
               (m_adwPointerMode[dwButton] == IDC_GRAPHPOINTMOVEUD) ? IDC_CURSOROBJCONTROLUD :
               IDC_CURSORGRAPHPOINTMOVE)));
         else {
            PCAnimAttrib paa;
            fp fTime, fValue;
            DWORD dwLinear;
            paa = GraphClosest (pBay, iX, iY, &fTime, &fValue, &dwLinear,
               (m_adwPointerMode[dwButton] != IDC_GRAPHPOINTMOVEUD));
            if (paa)
               SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(
                  (m_adwPointerMode[dwButton] == IDC_GRAPHPOINTMOVEUD) ? IDC_CURSOROBJCONTROLUD :
                  IDC_CURSORGRAPHPOINTMOVE)));
            else
               SetCursor (LoadCursor (NULL, IDC_NO));
         }
         break;

      case IDC_GRAPHPOINTDELETE:
         if (pBay->dwType != 2)
            SetCursor (LoadCursor (NULL, IDC_NO));
         else {
            PCAnimAttrib paa;
            fp fTime, fValue;
            DWORD dwLinear;
            paa = GraphClosest (pBay, iX, iY, &fTime, &fValue, &dwLinear, FALSE);
            if (paa)
               SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORGRAPHPOINTDELETE)));
            else
               SetCursor (LoadCursor (NULL, IDC_NO));
         }
         break;

      case IDC_ANIMMOVELR:
         SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORANIMMOVELR)));
         break;
      case IDC_ANIMMOVEUD:
         SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORANIMMOVEUD)));
         break;

      case IDC_ANIMTIMEINSERT:
      case IDC_ANIMTIMEREMOVE:
         SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORANIMTIME)));
         break;

      case IDC_SELREGION:
         SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORSELREGION)));
         break;

      case IDC_ANIMPLAYAUDIO:
         SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORPLAYAUDIO)));
         break;

      case IDC_ZOOMIN:
         SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORZOOMIN)));
         break;

      default:
         SetCursor (LoadCursor (NULL, IDC_NO));
   }
}

/***********************************************************************************
CSceneView::SetPointerMode - Changes the pointer mode to the new mode.

inputs
   DWORD    dwMode - new mode
   DWORD    dwButton - which button it's approaite for, 0 for left, 1 for middle, 2 for right
*/
void CSceneView::SetPointerMode (DWORD dwMode, DWORD dwButton)
{
   if (dwButton >= 3)
      return;  // out of bounds

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
   m_pbarTop->FlagsSet (m_adwPointerMode[dwButton], dwFlag);
   m_pbarBottom->FlagsSet (m_adwPointerMode[dwButton], dwFlag);

   // BUGFIX - Invalidate the area around the tabs
   RECT r, rt;
   GetClientRect (m_hWnd, &r);
   // top
   rt = r;
   rt.top += VARBUTTONSIZE;
   rt.bottom = rt.top + 2;
   InvalidateRect (m_hWnd, &rt, FALSE);

   // bottom
   rt = r;
   rt.bottom -= VARBUTTONSIZE;
   rt.top += rt.bottom - 2;
   InvalidateRect (m_hWnd, &rt, FALSE);

   // if change pointer mode then take a snapshot for undo
   if (m_pSceneSet)
      m_pSceneSet->UndoRemember();

   // see which window it's over
   DWORD i;
   POINT px;
   GetCursorPos (&px);
   for (i = 0; i < m_lSVBAY.Num(); i++) {
      PSVBAY pb = (PSVBAY) m_lSVBAY.Get(i);
      GetWindowRect (pb->hWndData, &r);
      if (!PtInRect (&r, px))
         continue;

      // else over
      ScreenToClient (pb->hWndData, &px);
      SetProperCursor (pb, px.x, px.y);
   }
}


/***********************************************************************************
CSceneView::ModifyBaysFromSelection - Call this when the selection has changed or
window just created. This fills the bays based on the what has been selected in
the main world.

inputs
   none
returns
   none
*/

static int _cdecl SVBAYSort (const void *elem1, const void *elem2)
{
   SVBAY *pdw1, *pdw2;
   pdw1 = (SVBAY*) elem1;
   pdw2 = (SVBAY*) elem2;

   // by type
   if (pdw1->dwType != pdw2->dwType)
      return (int)pdw1->dwType - (int)pdw2->dwType;

   // by selected
   if (pdw1->fSelected != pdw2->fSelected)
      return (int)pdw1->fSelected - (int)pdw2->fSelected;

   // if age different, older age gets replaced first
   if (pdw1->dwAge != pdw2->dwAge)
      return (int)pdw1->dwAge - (int) pdw2->dwAge;

   // else, by original position
   return (int)pdw1->dwOrigPosn - (int)pdw2->dwOrigPosn;
}

static int _cdecl SVBAYSort2 (const void *elem1, const void *elem2)
{
   SVBAY *pdw1, *pdw2;
   pdw1 = (SVBAY*) elem1;
   pdw2 = (SVBAY*) elem2;

   // by type
   if (!pdw1->dwType || !pdw2->dwType)
      return -((int)pdw1->dwType - (int)pdw2->dwType);

   // else, by original position
   return (int)pdw1->dwOrigPosn - (int)pdw2->dwOrigPosn;
}

void CSceneView::ModifyBaysFromSelection (void)
{
   // get the selection
   CListFixed lSel;
   DWORD *padwSel, dwNum;
   if (!m_pWorld)
      return;

   padwSel = m_pWorld->SelectionEnum (&dwNum);
   lSel.Init (sizeof(DWORD), padwSel, dwNum);

   // mark the bays that are still selected
   DWORD i, j;
   PSVBAY pb;
   DWORD dwFind;
   pb = (PSVBAY) m_lSVBAY.Get(0);
   for (i = 0; i < m_lSVBAY.Num(); i++, pb++) {
      pb->fSelected = FALSE;
      pb->dwOrigPosn = i + 1;
      if (!pb->dwType)
         continue;   // nothing selected so allow to replace

      // get the object socket of this
      dwFind = m_pWorld->ObjectFind (&pb->gWorldObject);
      if (dwFind == -1) {
         // object no longer exists, so wipe out
         InvalidateRect (pb->hWndData, NULL, FALSE);
         InvalidateRect (pb->hWndInfo, NULL, FALSE);
         pb->dwType = 0;
         continue;
      }

      padwSel = (DWORD*) lSel.Get(0);
      for (j = 0; j < lSel.Num(); j++)
         if (dwFind == padwSel[j])
            break;
      pb->fSelected = (j < lSel.Num());
      if (pb->fSelected)
         lSel.Remove (j);  // already know it's selected so remove from list
   }

   // if nothing left on lSel list might as well exit now
   if (!lSel.Num())
      return;

   // sort so the blank ones are on top. Also, non-selected are on top
   qsort (m_lSVBAY.Get(0), m_lSVBAY.Num(), sizeof(SVBAY), SVBAYSort);

   // replace those on top with selection
   pb = (PSVBAY) m_lSVBAY.Get(0);
   for (i = 0; (i < m_lSVBAY.Num()) && lSel.Num(); i++) {
      if (pb[i].fSelected)
         continue;   // cant change this

      PCObjectSocket pos = m_pWorld->ObjectGet (*((DWORD*) lSel.Get(0)));
      if (!pos)
         continue;

      pb[i].dwType = 1;    // short chart
      pb[i].dwAge = GetTickCount();
      pb[i].dwOrigPosn = 0;   // since want to be on top the list since new
      pb[i].fVPosMin = 0;
      pb[i].fVPosMax = VPOSMAX;
      pb[i].fScrollMin = 0;
      pb[i].fScrollMax = VPOSMAX+1;
      pb[i].fScrollFlip = FALSE;
      pos->GUIDGet (&pb[i].gWorldObject);
      CalcBayMatrix (&pb[i]);

      // should reset scrollbar on window
      NewScrollLoc (&pb[i]);


      lSel.Remove (0);
   }

   // resort so the blank ones are on the bottom
   qsort (m_lSVBAY.Get(0), m_lSVBAY.Num(), sizeof(SVBAY), SVBAYSort2);

   // invalidate all the windows since just rearranged it all
   for (i = 0; i < m_lSVBAY.Num(); i++) {
      InvalidateRect (pb[i].hWndData, NULL, FALSE);
      InvalidateRect (pb[i].hWndInfo, NULL, FALSE);
   }


   // need to resize
   RECT r;
   GetClientRect (m_hWnd, &r);
   WndProc (m_hWnd, WM_SIZE, 0, MAKELPARAM (r.right - r.left, r.bottom - r.top)); 
   // resize will automatically recalc zoom matricies   
}


/***********************************************************************************
CSceneView::CalcBayMatrix - Calculates the matrix that translates from pixels into
time and vice-versa.

inputs
   PSVBAY      pBay - Bay to calculate. If NULL calculate all
*/
void CSceneView::CalcBayMatrix (PSVBAY pBay)
{
   DWORD i;
   PSVBAY pb;
   for (i = 0; i < (pBay ? 1 : m_lSVBAY.Num()); i++) {
      pb = (pBay ? pBay : (PSVBAY) m_lSVBAY.Get(i));
      if (!pb->dwType)
         continue;   // nothing here

      RECT r;
      GetClientRect (pb->hWndData, &r);
      r.right = max(r.right, 1);
      r.bottom = max(r.bottom, 1);

      // make sure time and V displayed more than 0
      pb->fVPosMax = max(pb->fVPosMax, pb->fVPosMin + .001);
      m_fViewTimeEnd = max(m_fViewTimeEnd, m_fViewTimeStart + .001);

      // matrix to convert from pixel to time
      CMatrix mScale, mTrans;
      mScale.Scale (
         (m_fViewTimeEnd - m_fViewTimeStart) / (fp)r.right,
         (m_fViewTimeEnd - m_fViewTimeStart) / (fp)r.right,
         (pb->fVPosMax - pb->fVPosMin) / (fp)r.bottom * (pb->fScrollFlip ? -1 : 1));
      mTrans.Translation (m_fViewTimeStart, m_fViewTimeStart,
         pb->fScrollFlip ? pb->fVPosMax : pb->fVPosMin);
      pb->mPixelToTime.Multiply (&mTrans, &mScale);
      pb->mPixelToTime.Invert4 (&pb->mTimeToPixel);
   }
}

/***********************************************************************************
CSceneView::PaintAnimObject - Paints animation object onto the HDC.

inputs
   HDC         hDC - To paint it on. This points to a bitmap which is then
                  bit-blitted.
   PSVBAY      pb - Bay it's in
   PCAnimSocket pas - Animation socket to use
returns
   none
*/
void CSceneView::PaintAnimObject (HDC hDC, RECT *prBayData, PSVBAY pb, PCAnimSocket pas)
{
   // if it's not within range then just eliminate it
   fp fTimeStart, fTimeEnd;
   int iMinX, iMinY;
   pas->TimeGet (&fTimeStart, &fTimeEnd);
   pas->QueryMinDisplay (&iMinX, &iMinY);

   // vertical psoition
   fp fVert;
   fVert = pas->VertGet ();

   // position in screen
   CPoint pScreen;
   pScreen.p[0] = fTimeStart;
   pScreen.p[1] = fTimeEnd;
   pScreen.p[2] = fVert;
   pScreen.p[3] = 1;
   pScreen.MultiplyLeft (&pb->mTimeToPixel);
   pScreen.p[1] = max(pScreen.p[1], pScreen.p[0] + (fp) iMinX);
   pScreen.p[3] = pScreen.p[2] + (int) iMinY;
   if ((pScreen.p[0] >= prBayData->right) || (pScreen.p[1] <= prBayData->left) ||
      (pScreen.p[2] >= prBayData->bottom) || (pScreen.p[3] <= prBayData->top))
      return;   // off screen

   // else, have it, so draw
   RECT r;
   r.left = (int) max(prBayData->left, pScreen.p[0]);
   r.right = (int) min((fp)prBayData->right, pScreen.p[1]);
   r.top = (int) pScreen.p[2];
   r.bottom = (int) pScreen.p[3];
   CPoint pTime;
   pTime.p[0] = r.left;
   pTime.p[1] = r.right;
   pTime.p[2] = r.top;
   pTime.p[3] = 1;
   pTime.MultiplyLeft (&pb->mPixelToTime);
   pas->Draw (hDC, &r, pTime.p[0], pTime.p[1]);

   // is it selected?
   GUID gObjectAnim;
   PSCENESEL pss;
   DWORD dwNum;
   pas->GUIDGet (&gObjectAnim);
   DWORD i;
   pss = m_pSceneSet->SelectionEnum (&dwNum);
   for (i = 0; i < dwNum; i++)
      if (IsEqualGUID (pss[i].gObjectAnim, gObjectAnim) && IsEqualGUID (pss[i].gObjectWorld, pb->gWorldObject))
         break;
      // dont need to test scene because 2 guids should make unique enough
   BOOL fSelected;
   fSelected = (i < dwNum);

   // draw the outline...
   HPEN hPen, hOldPen;
   hPen = CreatePen (PS_SOLID, 0, fSelected ? RGB(0xff,0,0) : RGB(0,0,0));
   hOldPen = (HPEN) SelectObject (hDC, hPen);

   // outline
   if (pScreen.p[0] >= prBayData->left) {
      MoveToEx (hDC, (int) pScreen.p[0]-1, r.top-1, NULL);
      LineTo (hDC, (int) pScreen.p[0]-1, r.bottom+1);

      if (fSelected) {
         MoveToEx (hDC, (int) pScreen.p[0]-2, r.top, NULL);
         LineTo (hDC, (int) pScreen.p[0]-2, r.bottom);
      }
   }
   if (pScreen.p[1] <= prBayData->right) {
      MoveToEx (hDC, (int) pScreen.p[1], r.top-1, NULL);
      LineTo (hDC, (int) pScreen.p[1], r.bottom+1);

      if (fSelected) {
         MoveToEx (hDC, (int) pScreen.p[1]+1, r.top, NULL);
         LineTo (hDC, (int) pScreen.p[1]+1, r.bottom);
      }
   }
   MoveToEx (hDC, r.left-1, r.top-1, NULL);
   LineTo (hDC, r.right+1, r.top-1);
   MoveToEx (hDC, r.left-1, r.bottom, NULL);
   LineTo (hDC, r.right+1, r.bottom);
   if (fSelected) {
      MoveToEx (hDC, r.left, r.top-2, NULL);
      LineTo (hDC, r.right, r.top-2);
      MoveToEx (hDC, r.left, r.bottom+1, NULL);
      LineTo (hDC, r.right, r.bottom+1);
   }

   // brush
   HBRUSH hBrush, hbrOld;
   hBrush = CreateSolidBrush ( fSelected ? RGB(0xff,0,0) : RGB(0,0,0));
   hbrOld = (HBRUSH)SelectObject (hDC, hBrush);

   // draw ticks so know exacly where starts and stops
   if (pScreen.p[0] + 4 >= prBayData->left) {
      // left tick
      POINT ap[3];
      ap[0].x = (int) pScreen.p[0];
      ap[0].y = (int) pScreen.p[2]-2;
      ap[1].x = ap[0].x;
      ap[1].y = ap[0].y - 4;
      ap[2].x = ap[0].x + 4;
      ap[2].y = ap[0].y;
      Polygon (hDC, ap, 3);
   }
   if ((fTimeStart != fTimeEnd) && (pScreen.p[1] - 4 <= prBayData->right)) {
      // left tick
      POINT ap[3];
      ap[0].x = (int) pScreen.p[1];
      ap[0].y = (int) pScreen.p[2]-2;
      ap[1].x = ap[0].x;
      ap[1].y = ap[0].y - 4;
      ap[2].x = ap[0].x - 4;
      ap[2].y = ap[0].y;
      Polygon (hDC, ap, 3);
   }

   SelectObject (hDC, hbrOld);
   DeleteObject (hBrush);
   SelectObject (hDC, hOldPen);
   DeleteObject (hPen);
}

/***********************************************************************************
CSceneView::MoveOutOfPurgatory - Called when there are one or more objects in
purgatory (for paste, or create new object, or create new object w/drag) and
user clicks on the data section of a view.

inputs
   PSVBAY      pb - Bay to use
   fp          fTime - Time, in seconds
   fp          fVertOrig - Vertical location
   GUID        *pgObjectAnim - This is filled with one of the objects added.
                  Used for create-new-object with drag. Can be null.
returns
   BOOL - TRUE if success
*/
BOOL CSceneView::MoveOutOfPurgatory (PSVBAY pb, fp fTime, fp fVertOrig, GUID *pgObjectAnim)
{
   // need to apply grid
   PCScene pScene;
   m_pSceneSet->StateGet (&pScene, NULL);

   // find out where clicked on
   CPoint pClickTime;
   pClickTime.p[0] = pClickTime.p[1] = pScene->ApplyGrid(fTime);
   pClickTime.p[2] = fVertOrig;
   pClickTime.p[3] = 1;

   m_pSceneSet->SelectionClear();

   // eliminate any empty objects
   DWORD i;
   PCSceneObj pSceneObj;
   for (i = m_pPurgatory->ObjectNum()-1; i < m_pPurgatory->ObjectNum(); i--) {
      pSceneObj = m_pPurgatory->ObjectGet(i);
      if (!pSceneObj || !pSceneObj->ObjectNum()) {
         m_pPurgatory->ObjectRemove (i);
         continue;
      }
   }
   if (!m_pPurgatory->ObjectNum())
      return FALSE;  // nothing to paste

   // find the far-left position
   fp fLeft, fTop, fStart, fEnd, fVert;
   BOOL fFound;
   PCAnimSocket pas;
   GUID g;
   fFound = FALSE;
   DWORD j;
   for (i = 0; i < m_pPurgatory->ObjectNum(); i++) {
      pSceneObj = m_pPurgatory->ObjectGet(i);
      if (!pSceneObj)
         continue;
      for (j = 0; j < pSceneObj->ObjectNum(); j++) {
         pas = pSceneObj->ObjectGet (j);
         if (!pas)
            continue;
         pas->TimeGet (&fStart, &fEnd);
         fVert = pas->VertGet();

         if (fFound) {
            fLeft = min(fStart, fLeft);
            fTop = min(fTop, fVert);
         }
         else {
            fLeft = fStart;
            fTop = fVert;
            fFound = TRUE;
         }

      } // j
   } // i
   if (!fFound)
      return FALSE;  // nothing to paste

   // if there is more than one object's worth in purgatory and click on an object
   // that's not one of the ones then beep and fail
   if (m_pPurgatory->ObjectNum() > 1) {
      for (i = 0; i < m_pPurgatory->ObjectNum(); i++) {
         pSceneObj = m_pPurgatory->ObjectGet(i);
         if (!pSceneObj) continue;
         pSceneObj->GUIDGet (&g);
         if (IsEqualGUID (g, pb->gWorldObject))
            break;
      }
      if (i >=m_pPurgatory->ObjectNum()) {
         BeepWindowBeep (ESCBEEP_DONTCLICK);
         return FALSE;
      }
   } // i

   // loop and paste it all in
   PCSceneObj pPasteTo;
   GUID gScene;
   m_pSceneSet->StateGet (&pScene, NULL);
   pScene->GUIDGet (&gScene);
   if (!pScene)
      return FALSE;  // no scene
   for (i = 0; i < m_pPurgatory->ObjectNum(); i++) {
      pSceneObj = m_pPurgatory->ObjectGet(i);
      if (!pSceneObj) continue;

      // get the scene object of where want to paste it
      if (m_pPurgatory->ObjectNum() <= 1)
         pPasteTo = pScene->ObjectGet (&pb->gWorldObject, TRUE);
      else {
         pSceneObj->GUIDGet (&g);
         pPasteTo = pScene->ObjectGet (&g, TRUE);
      }

      // transfer all the bits
      pPasteTo->GUIDGet (&g);
      for (j = pSceneObj->ObjectNum() - 1; j < pSceneObj->ObjectNum(); j--) {
         pas = pSceneObj->ObjectGet (j);
         if (!pas)
            continue;
         pSceneObj->ObjectRemove (j, TRUE);  // remove but dont delete

         pas->WorldSet (NULL, NULL, &g);  // so can change
         pas->TimeGet (&fStart, &fEnd);
         fVert = pas->VertGet ();
         fStart = fStart - fLeft + pClickTime.p[0];
         fEnd = fEnd - fLeft + pClickTime.p[0];
         fVert = fVert - fTop + pClickTime.p[2];
         fVert = min(fVert, VPOSMAX);
         pas->TimeSet (pScene->ApplyGrid(fStart), pScene->ApplyGrid(fEnd));
         pas->VertSet (fVert);

         // add it
         pPasteTo->ObjectAdd (pas);

         // add selection
         GUID gAnim;
         pas->GUIDGet (&gAnim);
         m_pSceneSet->SelectionAdd (&gScene, &g, &gAnim);
         if (pgObjectAnim)
            *pgObjectAnim = gAnim;
      } // j
   } // i

   return TRUE;
}


/***********************************************************************************
CSceneView::MouseToObject - Given a mouse location over the bay's data section,
this returns the object it's over.

inputs
   PSVBAY         pb - Bay it's over
   int            iX, iY - Location in the bay's data window
   int            *piOver - Filled with 0 to indicate it's over the main body, -1
                  over the left section (for leftward expansion), and 1 over the right
                  section (for rightward expansion)
returns
   PCAnimObject - Animation object it's over
*/
PCAnimSocket CSceneView::MouseToObject (PSVBAY pb, int iX, int iY, int *piOver)
{
   if (!pb)
      return NULL;

   // convert to time coords
   CPoint p, pDelta;
   p.p[0] = iX;
   p.p[1] = 0;
   p.p[2] = iY;
   p.p[3] = 1;
   pDelta.Copy (&p);
   pDelta.p[0] += 1; // so know how large delta is
   pDelta.p[2] += 1;
   p.MultiplyLeft (&pb->mPixelToTime);
   pDelta.MultiplyLeft (&pb->mPixelToTime);
   pDelta.Subtract (&p);

   // get the sceneobj
   PCScene pScene;
   PCSceneObj pSceneObj;
   if (!m_pSceneSet)
      return NULL;
   m_pSceneSet->StateGet (&pScene, NULL);
   if (!pScene)
      return NULL;
   pSceneObj = pScene->ObjectGet (&pb->gWorldObject);
   if (!pSceneObj)
      return NULL;

   // loop backwards sine those will be drawn on top
   DWORD i, dwNum;
   PCAnimSocket pas;
   fp fStart, fEnd, fEndOrig, fVert, fVertBot;
   int iMinX, iMinY;
   dwNum = pSceneObj->ObjectNum();
   for (i = dwNum-1; i < dwNum; i--) {
      pas = pSceneObj->ObjectGet(i);
      if (!pas)
         continue;

      // locations
      pas->TimeGet (&fStart, &fEnd);
      fVert = pas->VertGet ();

      // out of bounds?
      if ((p.p[0] < fStart) || (p.p[2] < fVert))
         continue;

      // account for wideth
      pas->QueryMinDisplay (&iMinX, &iMinY);
      fEndOrig = fEnd;
      fEnd = max(fEnd, fStart + (fp)iMinX * pDelta.p[0]);
      fVertBot = fVert + (fp)iMinY * pDelta.p[2];

      // other bounds?
      if ((p.p[0] > fEnd) || (p.p[2] > fVertBot))
         continue;

      // else, over
      if (piOver) {
         fp fRange;
         fRange = (fEnd - fStart) / 4;
         fRange = min(fRange, pDelta.p[0]*8);   // no more than 8 pixels

         if (fStart == fEndOrig)
            *piOver = 0;   // cant resize
         else if (p.p[0] < fStart + fRange)
            *piOver = -1;
         else if (p.p[0] > fEnd - fRange)
            *piOver = 1;
         else
            *piOver = 0;
      }

      return pas;
   }

   // else cant find
   return NULL;
}

/***********************************************************************************
CSceneView::InvalidateBayData - Invalidate all the bay data windows
*/
void CSceneView::InvalidateBayData (void)
{
   // refresh all the data displays
   DWORD i;
   PSVBAY pBay;
   pBay = (PSVBAY) m_lSVBAY.Get(0);
   for (i = 0; i < m_lSVBAY.Num(); i++, pBay++)
      if (pBay->dwType)
         InvalidateRect (pBay->hWndData, NULL, FALSE);
}

/**********************************************************************************
CSceneView::ClipboardCopy - Copies the selection to the clipboard.

returns
   BOOL - TRUE if OK. FALSE if error
*/
BOOL CSceneView::ClipboardCopy (void)
{
   // if no selection then error
   DWORD dwNum;
   PSCENESEL pss;
   pss = m_pSceneSet->SelectionEnum(&dwNum);
   if (!dwNum)
      return FALSE;

   // copy to purgatory
   m_pPurgatory->Clear();
   DWORD i;
   PCScene pScene;
   PCSceneObj pSceneObj;
   PCAnimSocket pas;
   m_pSceneSet->StateGet (&pScene, NULL);
   for (i = 0; i < dwNum; i++, pss++) {
      // get the object
      pSceneObj = pScene->ObjectGet (&pss->gObjectWorld);
      if (!pSceneObj)
         continue;
      pas = pSceneObj->ObjectGet (pSceneObj->ObjectFind (&pss->gObjectAnim));
      if (!pas)
         continue;
      
      // add it into purgatory
      pSceneObj = m_pPurgatory->ObjectGet (&pss->gObjectWorld, TRUE);
      if (!pSceneObj)
         continue;
      pas = pas->Clone();
      if (!pas)
         continue;
      pSceneObj->ObjectAdd (pas);
   }

   // convert to MML text
   PCMMLNode2 pNode;
   pNode = m_pPurgatory->MMLTo ();
   if (!pNode)
      return FALSE;
   CMem mem;
   if (!MMLToMem (pNode, &mem, TRUE)) {
      delete pNode;
      return FALSE;
   }
   delete pNode;  // dont need anymore

   // get a handle
   HANDLE hMem;
   hMem = GlobalAlloc (GMEM_MOVEABLE | GMEM_DDESHARE, mem.m_dwCurPosn);
   if (!hMem)
      return FALSE;
   PVOID ph;
   ph = GlobalLock (hMem);
   if (!ph) {
      GlobalFree (hMem);
      return FALSE;
   }
   memcpy (ph, mem.p, mem.m_dwCurPosn);
   GlobalUnlock (hMem);


   // open the clipboard
   if (!OpenClipboard (m_hWnd)) {
      GlobalFree (hMem);
      return FALSE;
   }
   EmptyClipboard ();
   SetClipboardData (m_dwClipFormat, hMem);
   CloseClipboard ();

   return TRUE;
}


/**********************************************************************************
CSceneView::ClipboardCut - Copies the current selection to the clipboard and
then deletes it.

returns
   BOOL - TRUE if OK. FALSE if error
*/
BOOL CSceneView::ClipboardCut (void)
{
   if (!ClipboardCopy())
      return FALSE;

   return ClipboardDelete (TRUE);
}


/**********************************************************************************
CSceneView::ClipboardPaste - Opens the clipboard and pastes the current ASP
data into the world.

inputs
   none
returns
   BOOL - TRUE if successful
*/
BOOL CSceneView::ClipboardPaste (void)
{
   BOOL fRet;

   // open the clipboard and find the right data
   if (!OpenClipboard(m_hWnd))
      return FALSE;

   // get the data
   HANDLE hMem;
   hMem = GetClipboardData (m_dwClipFormat);
   if (!hMem) {
      CloseClipboard();
      return FALSE;
   }

   // conver to MML
   PCMMLNode2 pNode;
   PWSTR psz;
   psz = (PWSTR) GlobalLock (hMem);
   if (!psz) {
      CloseClipboard();
      return FALSE;
   }
   pNode = MMLFromMem(psz);
   GlobalUnlock (hMem);
   if (!pNode) {
      CloseClipboard();
      return FALSE;
   }

   // convert this to objects
   m_pPurgatory->MMLFrom (pNode);
   delete pNode;

   CloseClipboard ();

   fRet = (m_pPurgatory->ObjectNum() ? TRUE : FALSE);

   if (fRet)
      SetPointerMode (IDC_POSITIONPASTE, 0);
   else {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
   }

   return TRUE;
}


/**********************************************************************************
CSceneView::ClipboardDelete - Deletes the current selection. It doesn't deal
directly with the clipboard but it is used by the clipboard functions.

inputs
   BOOL     fUndoSnapshot - If TRUE, then marks an undo snapshot after the delete.
               If FALSE, it's left up to the caller (such as used by ClipboardPaste())
returns
   BOOL - TRUE if successful
*/
BOOL CSceneView::ClipboardDelete (BOOL fUndoSnapshot)
{
   // if no selection then error
   DWORD dwNum;
   PSCENESEL pss;
   if (!m_pSceneSet)
      return FALSE;
   pss = m_pSceneSet->SelectionEnum(&dwNum);
   if (!dwNum)
      return TRUE;

   // store this list
   CListFixed l;
   l.Init (sizeof(SCENESEL), pss, dwNum);
   pss = (PSCENESEL) l.Get(0);
   dwNum = l.Num();

   // delete everything in the selection
   PCScene pScene;
   PCSceneObj pSceneObj;
   m_pSceneSet->StateGet (&pScene, NULL);
   if (!pScene)
      return FALSE;
   DWORD i, dwIndex;
   for (i = 0; i < dwNum; i++) {
      pSceneObj = pScene->ObjectGet (&pss[i].gObjectWorld);
      if (!pSceneObj)
         continue;

      dwIndex = pSceneObj->ObjectFind (&pss[i].gObjectAnim);
      if (dwIndex == -1)
         continue;

      pSceneObj->ObjectRemove (dwIndex);
   }

   if (fUndoSnapshot)
      m_pSceneSet->UndoRemember();

   return FALSE;
}

/**********************************************************************************
CSceneView::NewScrollLoc - Call this after changing m_fViewTimeStart or
m_fViewTimeEnd. This updates the scrollbar and redraws the contents of all the
windows
*/
void CSceneView::NewScrollLoc (void)
{
   // scrollbar
   PCScene pScene;
   if (!m_pSceneSet)
      return; // cant do this
   m_pSceneSet->StateGet (&pScene, NULL);
   if (!pScene)
      return;  // cant do this

   // how large
   fp fSize;
   m_fViewTimeStart = max (0, m_fViewTimeStart);
   m_fViewTimeEnd = max (m_fViewTimeStart + .001, m_fViewTimeEnd);
   fSize = max(m_fViewTimeEnd, pScene->DurationGet());

   SCROLLINFO si;
   memset (&si, 0, sizeof(si));
   si.cbSize = sizeof(si);
   si.fMask = SIF_ALL;
   si.nMax = 10000;
   si.nMin = 0;
   si.nPage = (int) (10000.0 * (m_fViewTimeEnd - m_fViewTimeStart) / fSize );
   si.nPos = (int) (10000.0 * m_fViewTimeStart / fSize);
   si.nTrackPos = si.nPos;
   SetScrollInfo (m_hWndScroll, SB_CTL, &si, TRUE);

   // refresh
   CalcBayMatrix (NULL);   // recalculate all the matricies
   InvalidateBayData ();

   // update the slider
   if (m_pTimeline)
      m_pTimeline->ScaleSet (m_fViewTimeStart, m_fViewTimeEnd);
}

/**********************************************************************************
CSceneView::NewScrollLoc - Call this after changing the bay's vertical display
area. This updates the scrollbar and redraws the contents of the window
*/
void CSceneView::NewScrollLoc (PSVBAY pb)
{
   // how large
   fp fSize;
   pb->fVPosMin = max (pb->fScrollMin, pb->fVPosMin);
   pb->fVPosMax = min (pb->fVPosMax, pb->fScrollMax);
   pb->fVPosMax = max (pb->fVPosMin + .001, pb->fVPosMax);
   fSize = pb->fScrollMax - pb->fScrollMin;

   SCROLLINFO si;
   memset (&si, 0, sizeof(si));
   si.cbSize = sizeof(si);
   si.fMask = SIF_ALL;
   si.nMax = 10000;
   si.nMin = 0;
   si.nPage = (int) (10000.0 * (pb->fVPosMax - pb->fVPosMin) / fSize );
   si.nPage = min(si.nPage, 10000);
   if (pb->fScrollFlip)
      si.nPos = (int) (10000.0 * (pb->fScrollMax - pb->fVPosMax) / fSize);
   else
      si.nPos = (int) (10000.0 * (pb->fVPosMin - pb->fScrollMin) / fSize);
   si.nPos = max(0,si.nPos);
   si.nPos = min(10000,si.nPos);
   si.nTrackPos = si.nPos;
   if (!pb->dwType) {
      si.nMax = si.nMin = si.nPage = si.nPos = 0;
   }
   SetScrollInfo (pb->hWndData, SB_VERT, &si, TRUE);

   // refresh
   CalcBayMatrix (pb);
   InvalidateRect (pb->hWndData, NULL, FALSE);
}

/**********************************************************************************
CSceneView::IsAttribInteresting - This returns TRUE is anything interesting happens
with the attribute over the time-frame currently viewed.

inputs
   PSVBAY         pb - Bay that it's in - and hence object viewed
   PWSTR          pszAttrib - Attribute name
returns
   BOOL - TRUE if something interesting happens - attribute changes or keyframe
*/
BOOL CSceneView::IsAttribInteresting (PCSceneObj pSceneObj, PWSTR pszAttrib)
{
   // get the attribute in the object
   PCAnimAttrib paa;
   pSceneObj->AnimAttribGenerate();
   paa = pSceneObj->AnimAttribGet (pszAttrib);
   if (!paa)
      return FALSE;  // nothing

   // else, see if changed between beginning and end
   if (fabs(paa->ValueGet(m_fViewTimeEnd) - paa->ValueGet(m_fViewTimeStart)) > CLOSE)
      return TRUE;

   // else, see if any keyframes
   DWORD dw1, dw2;
   dw1 = paa->PointClosest (m_fViewTimeStart, 1);
   dw2 = paa->PointClosest (m_fViewTimeEnd, -1);
   if ((dw1 == -1) || (dw2 == -1))
      return FALSE;  // nothing to left/right
   return (dw2 >= dw1); // if points increment then something in-between
}


/**********************************************************************************
CSceneView::BaySelectAttrib - Given a new attribute string, this selected it to
be used in the bay. (NOTE: Doesn't invalidate window or change list box.) It basically
finds the entry in pb->plATTIRBVAL and sets the value to 1. THEN, it goes through
all other selected attributes and makes sure their unit of measurement is the same;
if not, they are deselected

inputs
   PSVBAY         pb - Bay
   PWSTR          pszAttrib - Attribute being selectd
returns
   BOOL - TRUE if success
*/
BOOL CSceneView::BaySelectAttrib (PSVBAY pb, PWSTR pszAttrib)
{
   // find the sceneobj
   PCScene pScene = NULL;
   PCSceneObj pSceneObj = NULL;
   m_pSceneSet->StateGet (&pScene, NULL);
   if (pScene)
      pSceneObj = pScene->ObjectGet (&pb->gWorldObject, TRUE);
   if (!pSceneObj)
      return FALSE;

   // find the attribute
   PCAnimAttrib paa, paa2;
   pSceneObj->AnimAttribGenerate();
   paa = pSceneObj->AnimAttribGet (pszAttrib, TRUE);
   if (!paa)
      return FALSE;

   // go through all the attributes and maybe disable
   DWORD i;
   PATTRIBVAL pav;
   pav = (PATTRIBVAL) pb->plATTRIBVAL->Get(0);
   for (i = 0; i < pb->plATTRIBVAL->Num(); i++, pav++) {
      if (!_wcsicmp(pav->szName, pszAttrib)) {
         pav->fValue = 1;
         continue;
      }

      if (!pav->fValue)
         continue;   // already off

      // look at the attribute and make sure the same type
      paa2 = pSceneObj->AnimAttribGet (pav->szName, TRUE);
      if (!paa2) {
         pav->fValue = 0;
         continue;   // cant find so turn off
      }

      if (paa->m_dwType != paa2->m_dwType) {
         pav->fValue = 0;
         continue;   // wrong type
      }
   }

   return TRUE;
}


/**********************************************************************************
CSceneView::BaySelectDefault - Select attributes to start off with when first
displaying.

inputs
   PSVBAY         pb - Bay
*/
void CSceneView::BaySelectDefault (PSVBAY pb)
{
   // clear all attributes to be off
   PATTRIBVAL pav = (PATTRIBVAL) pb->plATTRIBVAL->Get(0);;
   DWORD dwNum = pb->plATTRIBVAL->Num();
   DWORD i;
   for (i = 0; i < dwNum; i++)
      pav[i].fValue = 0;

   // find the sceneobj
   PCScene pScene = NULL;
   PCSceneObj pSceneObj = NULL;
   m_pSceneSet->StateGet (&pScene, NULL);
   if (pScene)
      pSceneObj = pScene->ObjectGet (&pb->gWorldObject, TRUE);
   if (!pSceneObj)
      return;

   // go backwards and see if interesting, If so then select
   for (i = dwNum-1; i < dwNum; i--) {
      if (IsAttribInteresting (pSceneObj, pav[i].szName))
         BaySelectAttrib (pb, pav[i].szName);
      }

   // if nothing selected then select the first one
   for (i = 0; i < dwNum; i++)
      if (pav[i].fValue)
         break;
   if (i >= dwNum)
      BaySelectAttrib (pb, pav[0].szName);
}

/**********************************************************************************
CSceneView::BaySelectToMinMax - Looks through all the attributes selected in the
bay and determines the minimum and maximum from the selection. It then
changes pb->m_fScrollMin, m_fScrollMax, m_fVPosMin, m_fVPosMax

inputs
   PSVBAY         pb - Bay
returns
   none
*/
void CSceneView::BaySelectToMinMax (PSVBAY pb)
{
   fp fMin, fMax;
   BOOL fFound = FALSE;
   pb->fVPosMin = pb->fScrollMin = 0;
   pb->fVPosMax = pb->fScrollMax = 1;  // so have something

   PATTRIBVAL pav = (PATTRIBVAL) pb->plATTRIBVAL->Get(0);;
   DWORD dwNum = pb->plATTRIBVAL->Num();
   DWORD i;

   // find the sceneobj
   PCScene pScene = NULL;
   PCSceneObj pSceneObj = NULL;
   m_pSceneSet->StateGet (&pScene, NULL);
   if (pScene)
      pSceneObj = pScene->ObjectGet (&pb->gWorldObject, TRUE);
   if (!pSceneObj)
      return;

   // loop and find min/max
   PCAnimAttrib paa;
   DWORD dwType;
   for (i = 0; i < dwNum; i++, pav++) {
      if (!pav->fValue)
         continue;

      paa = pSceneObj->AnimAttribGet (pav->szName, TRUE);
      if (!paa)
         continue;

      dwType = paa->m_dwType;
      if (fFound) {
         fMin = min(fMin, paa->m_fMin);
         fMax = max(fMax, paa->m_fMax);
      }
      else {
         fFound = TRUE;
         fMin = paa->m_fMin;
         fMax = paa->m_fMax;
      }
   }

   // if found then set
   if (fFound) {
      fp fCenter;
      fCenter = (fMin + fMax) / 2;

      pb->fVPosMin = pb->fScrollMin = fMin;
      pb->fVPosMax = pb->fScrollMax = fMax;

      // widen the scope so can see beyond min/max
      pb->fScrollMin = (pb->fScrollMin - fCenter) * 2 + fCenter;
      pb->fScrollMax = (pb->fScrollMax - fCenter) * 2 + fCenter;
   }
}

/**********************************************************************************
CSceneView::BayPaintGraph - Paints the graph

inputs
   PSVBAY         pb - Bay
   HDC            hDC - To draw on
   RECT           *pr - To use in the hDC. Same coords as GetClientRect()
returns
   none
*/
void CSceneView::BayPaintGraph (PSVBAY pb, HDC hDC, RECT *pr)
{
   PATTRIBVAL pav = (PATTRIBVAL) pb->plATTRIBVAL->Get(0);;
   DWORD dwNum = pb->plATTRIBVAL->Num();
   DWORD i;

   // find the sceneobj
   PCScene pScene = NULL;
   PCSceneObj pSceneObj = NULL;
   m_pSceneSet->StateGet (&pScene, NULL);
   if (pScene)
      pSceneObj = pScene->ObjectGet (&pb->gWorldObject, TRUE);
   if (!pSceneObj)
      return;

   // loop though all the elements
   DWORD dwAttrib, dwColor;
   dwColor = 0;
   pSceneObj->AnimAttribGenerate();
   BOOL fHorzLine;
   fHorzLine = FALSE;
   for (dwAttrib = 0; dwAttrib < dwNum; dwAttrib++, pav++) {
      // if not selected dont draw
      if (!pav->fValue)
         continue;

      // get the attrib
      PCAnimAttrib paa;
      paa = pSceneObj->AnimAttribGet(pav->szName, TRUE);
      if (!paa)
         goto next;

      // draw the horizontal lines
      if (!fHorzLine) {
         TimelineHorzTicks (hDC, pr, &pb->mPixelToTime, &pb->mTimeToPixel,
                        RGB(0xe0,0xe0,0xe0), RGB(0xc0,0xc0,0xc0), paa->m_dwType);

         fHorzLine = TRUE;
      }

      // create the pen
      HPEN hPen, hPenOld;
      hPen = CreatePen (PS_SOLID, 0, gacGraph[dwColor]);
      hPenOld = (HPEN) SelectObject (hDC, hPen);

      // draw the keyframes
      DWORD dwStart, dwEnd;
      CPoint p;
      TEXTUREPOINT tp;
      DWORD dwLinear;
      int iX, iY;
      dwStart = paa->PointClosest (m_fViewTimeStart, 1);
      dwEnd = paa->PointClosest (m_fViewTimeEnd, -1);
      if ((dwStart != -1) || (dwEnd != -1)) for (i = dwStart; i <= dwEnd; i++) {
         if (!paa->PointGet (i, &tp, &dwLinear))
            continue;
         p.p[0] = tp.h;
         p.p[1] = 0;
         p.p[2] = tp.v;
         p.p[3] = 1;
         p.MultiplyLeft (&pb->mTimeToPixel);

         iX = (int) p.p[0];
         iY = (int) p.p[2];

         if (dwLinear == 0) { // boolean
            MoveToEx (hDC, iX, iY-3, NULL);
            LineTo (hDC, iX, iY+4);
            MoveToEx (hDC, iX-3, iY, NULL);
            LineTo (hDC, iX+4, iY);
         }
         else if (dwLinear == 1) { // straight
            MoveToEx (hDC, iX-3, iY-3, NULL);
            LineTo (hDC, iX+3, iY-3);
            LineTo (hDC, iX+3, iY+3);
            LineTo (hDC, iX-3, iY+3);
            LineTo (hDC, iX-3, iY-3);
         }
         else {   // curved
            MoveToEx (hDC, iX-2, iY-3, NULL);
            LineTo (hDC, iX+3, iY-3);

            MoveToEx (hDC, iX+3, iY-2, NULL);
            LineTo (hDC, iX+3, iY+3);

            MoveToEx (hDC, iX+2, iY+3, NULL);
            LineTo (hDC, iX-3, iY+3);

            MoveToEx (hDC, iX-3, iY+2, NULL);
            LineTo (hDC, iX-3, iY-3);
         }
      }

      // NOTE: Drawing every other one just to make drawing faster
      fp fLast, fCur;
      fLast = 0;
      if (pr->right > pr->left) for (iX = pr->left; iX < pr->right+2; iX += 2) {
         p.Zero();
         p.p[0] = (fp) (iX - pr->left) / (fp) (pr->right - pr->left) * (m_fViewTimeEnd - m_fViewTimeStart) +
            m_fViewTimeStart;
         p.p[2] = fCur = paa->ValueGet (p.p[0]);
         p.p[3] = 1;
         p.MultiplyLeft (&pb->mTimeToPixel);

         BOOL fMoveTo;
         fMoveTo = (iX == pr->left);

         // watch out for modulo
         if (!fMoveTo && (paa->m_dwType == AIT_ANGLE) && (fabs(fLast - fCur) > PI))
            fMoveTo = TRUE;  // not 100% accurate, but pretty good guestimate of modulo

         if (fMoveTo)
            MoveToEx (hDC, iX, (int) p.p[2], NULL);
         else
            LineTo (hDC, iX, (int) p.p[2]);

         fLast = fCur;
      }


      // free pen
      SelectObject (hDC, hPenOld);
      DeleteObject (hPen);

next:
      // update the color
      dwColor = (dwColor+1) % (sizeof(gacGraph) / sizeof(COLORREF));
   }
}

/**********************************************************************************
CSceneView::GraphClosest - Find closest point. Given an XY cursor over the graph, this
finds the closest point to it and the time of the point. If it's too far away
from anything returns NULL.

inputs
   PSVBAY         pb - Bay
   int            iX, iY - X and Y pixel locations
   fp             *pfTime - Time of the point closest to
   fp             *pfValue - Value of the point closest to
   DWORD          *pdwLinear - Fills in the linear value of the point
   BOOL           fApplyGrid - If TRUE, applies the grid
returns
   PCAnimAttrib - CAnimAttrib that closes to. NULL if none.
*/
PCAnimAttrib CSceneView::GraphClosest (PSVBAY pb, int iX, int iY, fp *pfTime, fp *pfValue,
                                       DWORD *pdwLinear,BOOL fApplyGrid)
{
   CPoint p, p2;
   p.Zero();
   p.p[0] = iX;
   p.p[2] = iY;
   p.p[3] = 1;
   p2.Copy (&p);
   p2.p[0] += 5;  // allow 5 pixel range
   p2.p[2] += 5;  // allow 5 pixel range
   p.MultiplyLeft (&pb->mPixelToTime);
   p2.MultiplyLeft (&pb->mPixelToTime);
   p2.Subtract (&p);
   p2.p[0] = fabs(p2.p[0]);
   p2.p[2] = fabs(p2.p[2]);

   // remember
   PCScene pScene;
   PCSceneObj pSceneObj;
   m_pSceneSet->StateGet (&pScene, NULL);
   if (!pScene)
      return NULL;
   fp fTime, fValue;
   if (fApplyGrid)
      fTime = pScene->ApplyGrid(p.p[0]);
   else
      fTime = p.p[0];
   fValue = p.p[2];

   // find the object
   pSceneObj = pScene->ObjectGet (&pb->gWorldObject);
   if (!pScene)
      return NULL;

   // loop through all the control points nearby
   pSceneObj->AnimAttribGenerate();
   DWORD i;
   PATTRIBVAL pav;
   PCAnimAttrib paa, pClosestAA;
   pav = (PATTRIBVAL) pb->plATTRIBVAL->Get(0);
   fp fClosestTime, fClosestValue, fClosestDist, fDist;
   DWORD dwClosestLinear;
   BOOL fFound, fClosestPoint;
   fFound = FALSE;
   fClosestPoint = FALSE;
   for (i = 0; i < pb->plATTRIBVAL->Num(); i++, pav++) {
      if (!pav->fValue)
         continue;   // only want points in graphs that showing

      // attribute
      paa = pSceneObj->AnimAttribGet (pav->szName, TRUE);
      if (!paa)
         continue;

      // find closest in time
      DWORD dwFind;
      dwFind = paa->PointClosest (fTime, 0);
      if (dwFind != -1) {
         TEXTUREPOINT tp;
         DWORD dwLinear;
         if (!paa->PointGet (dwFind, &tp, &dwLinear))
            goto tryline;
         if ((fabs (tp.h - fTime) > p2.p[0]) || (fabs(tp.v - fValue) > p2.p[2]))
            goto tryline;  // too far away
         // distance?
         fDist = fabs (tp.h - fTime) / p2.p[0] + fabs(tp.v - fValue) / p2.p[2];
         if (!fFound || !fClosestPoint || (fDist < fClosestDist)) {
            // found
            fFound = TRUE;
            fClosestPoint = TRUE;
            fClosestDist = fDist;
            fClosestTime = tp.h;
            fClosestValue = tp.v;
            dwClosestLinear = dwLinear;
            pClosestAA = paa;
            continue;
         }
      }

tryline:
      // if already found a point AND it's point then dont try to find a value on a graph
      if (fClosestPoint)
         continue;

      // always do the grid for closest point
      fTime = pScene->ApplyGrid(p.p[0]);

      // see what the graph value is
      fp fGraph, fDist;
      fGraph = paa->ValueGet (fTime);
      fDist = fabs(fGraph - fValue);
      if (fDist > p2.p[2])
         continue;   // too far away
      fDist /= p2.p[2];
      if (!fFound || (fDist < fClosestDist)) {
         // found a point
         fFound = TRUE;
         fClosestPoint = FALSE;
         fClosestDist = fDist;
         fClosestTime = fTime;
         fClosestValue = fGraph;
         pClosestAA = paa;

         // figure out if linear
         DWORD dwFind;
         dwFind = paa->PointClosest (fTime, -1);
         dwClosestLinear = 2; // assume curve
         TEXTUREPOINT tp;
         if (dwFind != -1)
            paa->PointGet (dwFind, &tp, &dwClosestLinear);
      }
   } // i

   if (!fFound)
      return NULL;   // not found

   // else
   *pfTime = fClosestTime;
   *pfValue = fClosestValue;
   *pdwLinear = dwClosestLinear;
   return pClosestAA;
}

/**********************************************************************************
CSceneView::GraphMovePoint - Moves a point that occurs on a the graph from one
time to another.

inputs
   PSVBAY            pb - Bay
   PWSTR             pszAttrib - Attribute name
   PTEXTUREPOINT     ptOld - Old/previous time/value.
   PTEXTUREPOINT     ptNew - New time/value. NOTE: If the time's match between old and new
                     do less processing. If NULL then just removed the one clicked on
   DWORD             dwLinear - What linear setting to use
returns
   BOOL - TRUE if success
*/
BOOL CSceneView::GraphMovePoint (PSVBAY pb, PWSTR pszAttrib, PTEXTUREPOINT ptOld,
                                 PTEXTUREPOINT ptNew, DWORD dwLinear)
{
   // find the object
   PCScene pScene;
   PCSceneObj pSceneObj;
   m_pSceneSet->StateGet (&pScene, NULL);
   if (!pScene)
      return NULL;
   pSceneObj = pScene->ObjectGet (&pb->gWorldObject);
   if (!pSceneObj)
      return NULL;

   return pSceneObj->GraphMovePoint (pszAttrib, ptOld, ptNew, dwLinear);
}



// BUGBUG - Need scrollbars to look like winnt scrollbars - not sure how to do this


// BUGBUG - Tried selecting multiple objects and then one at a time. The bays
// are supposed to scroll away one at a time, but it always seems to replace just
// the top bay

// BUGBUG - The "syncrhonize camera" button doesnt seem to sychonize the exposure setting
