/***************************************************************************
CHouseView - C++ object for viewing the house data.

begun 31/8/2001
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <zmouse.h>
#define  COMPILE_MULTIMON_STUBS     // so that works win win95
#include <multimon.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


/*****************************************************************************
typedefs */

#define  IDC_MYSCROLLBAR   1500

#define  TENDEGREES        (2.0 * PI / 360.0 * 10.0)

#define  TIMERINTER        250      // 250 millisconds
#define  WAITTODRAW        89       // timer ID for waiting to draw
#define  WAITTOTIP         90       // timer ID for waiting to tip
#define  TIMER_SYNCWITHSCENE  91    // timer ID to cause frequent syncinc of the scene display with this
#define  TIMER_AIRBRUSH    121      // timer ID to cause frequent airbrush
#define  AIRBRUSHPERSEC    5        // number of times to call airbrush per second

#define  CLIPPLANE_USER    1035     // arbitrary number. Clip plane set by user by drawing line
#define  CLIPPLANE_FLOORABOVE 1036  // clip above this point

#define  SCROLLSIZE     (16)
#define  SMALLSCROLLSIZE (SCROLLSIZE / 2)
#define  VARBUTTONSIZE  (m_fSmallWindow ? M3DSMALLBUTTONSIZE : M3DBUTTONSIZE)
#define  VARSCROLLSIZE (m_fSmallWindow ? SMALLSCROLLSIZE : SCROLLSIZE)
#define  VARSCREENSIZE (VARBUTTONSIZE * 7 / 4)
#define  TBORDER           16       // extra border around thumbnail
#define  THUMBSHRINK       (TEXTURETHUMBNAIL/2)    // size to shrink thumbnail to


#define  IDC_PRINT         1001
#define  IDC_CLOSE         1002
#define  IDC_MINIMIZE      1003
#define  IDC_NEXTMONITOR   1004
#define  IDC_SAVE          1005
#define  IDC_SAVEAS        1006
#define  IDC_OPEN          1007
#define  IDC_NEW           1008
#define  IDC_CLONEVIEW     1009
#define  IDC_HELPBUTTON    1010
#define  IDC_LOCALE        1011
#define  IDC_SMALLWINDOW   1012
#define  IDC_LIBRARY       1013
#define  IDC_OBJEDITOR     1014
#define  IDC_APPSET        1015
#define  IDC_OBJECTLIST    1016
#define  IDC_ANIM          1017
#define  IDC_PAINTVIEW     1018
#define  IDC_PMMODEPOLY    1100
#define  IDC_PMMODECLAY    1101
#define  IDC_PMMODEMORPH   1102
#define  IDC_PMMODEBONE    1103
#define  IDC_PMMODETEXTURE 1104
#define  IDC_PMMODETAILOR  1105
#define  IDC_PMDIALOG      1106

#define  IDC_CUTBUTTON     2001
#define  IDC_COPYBUTTON    2002
#define  IDC_PASTEBUTTON   2003
#define  IDC_UNDOBUTTON    2004
#define  IDC_REDOBUTTON    2005
#define  IDC_DELETEBUTTON  2006

#define  IDC_VIEWFLAT      3001
#define  IDC_VIEW3D        3002
#define  IDC_VIEWWALKTHROUGH   3003
#define  IDC_VIEWSETTINGS  3004
#define  IDC_VIEWQUALITY   3005
#define  IDC_ZOOMIN        3006
#define  IDC_ZOOMINDRAG    3007
#define  IDC_ZOOMOUT       3008
#define  IDC_ZOOMAT        3009
#define  IDC_THUMBNAIL     3010

#define  IDC_VIEWFLATLRUD  3100
#define  IDC_VIEWFLATSCALE 3101
#define  IDC_VIEWFLATROTZ  3102
#define  IDC_VIEWFLATROTX  3103
#define  IDC_VIEWFLATROTY  3104
#define  IDC_VIEWFLATSETTINGS 3105
#define  IDC_VIEWSETCENTERFLAT 3106
#define  IDC_VIEWFLATLLOOKAT 3107
#define  IDC_VIEWSETCENTER3D 3108
#define  IDC_VIEWFLATQUICK 3109
#define  IDC_VIEW3DLRUD    3200
#define  IDC_VIEW3DLRFB    3201
#define  IDC_VIEW3DROTZ    3202
#define  IDC_VIEW3DROTX    3203
#define  IDC_VIEW3DROTY    3204
#define  IDC_VIEW3DSETTINGS 3205
#define  IDC_VIEWWALKFBROTZ 3300
#define  IDC_VIEWWALKROTZX  3301
#define  IDC_VIEWWALKROTY   3302
#define  IDC_VIEWWALKLRUD   3303
#define  IDC_VIEWWALKLRFB   3304
#define  IDC_VIEWWALKSETTINGS 3305

#define  IDC_CLIPLINE      4000
#define  IDC_CLIPPOINT     4001
#define  IDC_CLIPSET       4002
#define  IDC_CLIPLEVEL     4003
#define  IDC_PMSHOWBACKGROUND 4004
#define  IDC_PMTEXTTOGGLE  4005

#define  IDC_SELINDIVIDUAL 5000
#define  IDC_SELREGION     5001
#define  IDC_SELALL        5002
#define  IDC_SELSET        5003
#define  IDC_SELNONE       5004
#define  IDC_PMMIRROR      5010
#define  IDC_PMSTENCIL     5011

#define  IDC_BONEMIRROR    5100
#define  IDC_BONEDEFAULT   5101

#define  IDC_MOVENSEW      6000
#define  IDC_MOVEUD        6001
#define  IDC_MOVEROTZ      6002
#define  IDC_MOVEROTY      6003
#define  IDC_MOVEREFERENCE 6005
#define  IDC_MOVESETTINGS  6006
#define  IDC_MOVEEMBED     6007
#define  IDC_MOVENSEWUD    6008

#define  IDC_POSITIONPASTE 7000  // so user can say where wants object pasted
#define  IDC_OBJECTPASTE   7001  // so user can paste object
#define  IDC_OBJECTPASTEDRAG 7002   // drag

#define  IDC_OBJPAINT      8001
#define  IDC_OBJNEW        8002
#define  IDC_OBJDIALOG     8004
#define  IDC_OBJINTELADJUST 8006
#define  IDC_OBJDECONSTRUCT 8007
#define  IDC_OBJOPENCLOSE   8008
#define  IDC_OBJONOFF       8009
#define  IDC_OBJCONTROLNSEW 8010  // move control points around
#define  IDC_OBJCONTROLUD   8011  // move control points around
#define  IDC_OBJCONTROLDIALOG 8012
#define  IDC_OBJCONTROLNSEWUD 8013  // move control points around in any direction
#define  IDC_OBJMERGE      8014
#define  IDC_OBJCONTROLINOUT 8015
#define  IDC_OBJATTRIB     8016
#define  IDC_OBJATTACH     8017
#define  IDC_OBJSHOWEDITOR 8018
#define  IDC_PMEDITVERT    8020
#define  IDC_PMEDITEDGE    8021
#define  IDC_PMEDITSIDE    8022
#define  IDC_PMMOVEANYDIR  8100
#define  IDC_PMMOVEINOUT   8101
#define  IDC_PMMOVEPERP    8102
#define  IDC_PMMOVEPERPIND 8103
#define  IDC_PMMOVESCALE   8104
#define  IDC_PMMOVEROTPERP 8105
#define  IDC_PMMOVEROTNONPERP 8106
#define  IDC_PMMOVEDIALOG  8107
#define  IDC_PMEXTRA       8108
#define  IDC_PMCOLLAPSE    8109
#define  IDC_PMDELETE      8110
#define  IDC_PMTESSELATE   8111
#define  IDC_PMEDGESPLIT   8112
#define  IDC_PMSIDESPLIT   8113
#define  IDC_PMNEWSIDE     8114
#define  IDC_PMSIDEEXTRUDE 8115
#define  IDC_PMSIDEINSET   8116
#define  IDC_PMSIDEDISCONNECT 8117
#define  IDC_PMBEVEL       8118
#define  IDC_PMMAGANY      8119
#define  IDC_PMMAGNORM     8120
#define  IDC_PMMAGVIEWER   8121
#define  IDC_PMMAGPINCH    8122
#define  IDC_PMMAGSIZE     8123
#define  IDC_PMORGSCALE    8124
#define  IDC_PMMERGE       8125
#define  IDC_PMMORPHSCALE  8126
#define  IDC_PMTEXTPLANE   8127
#define  IDC_PMTEXTCYLINDER 8128
#define  IDC_PMTEXTSPHERE  8129
#define  IDC_PMTEXTMOVE    8130
#define  IDC_PMTEXTROT     8131
#define  IDC_PMTEXTSCALE   8132
#define  IDC_PMTEXTDISCONNECT 8133
#define  IDC_PMTEXTCOLLAPSE 8134
#define  IDC_PMTEXTMOVEDIALOG 8135
#define  IDC_PMTEXTMIRROR  8136
#define  IDC_PMTAILORNEW   8137

#define  IDC_BRUSHSHAPE    8200
#define  IDC_BRUSHEFFECT   8201
#define  IDC_BRUSHSTRENGTH 8202

#define  IDC_BRUSH4        8300
#define  IDC_BRUSH8        8301
#define  IDC_BRUSH16       8302
#define  IDC_BRUSH32       8303
#define  IDC_BRUSH64       8304

#define  IDC_BONEEDIT      8400
#define  IDC_BONEMERGE     8401
#define  IDC_BONESPLIT     8402
#define  IDC_BONEDELETE    8403
#define  IDC_BONEDISCONNECT 8404
#define  IDC_BONESCALE     8405
#define  IDC_BONEROTATE    8406
#define  IDC_BONENEW       8407

#define  IDC_GRIDSETTINGS  9000
#define  IDC_GRIDSELECT    9001
#define  IDC_GRIDFROMPOINT 9002
#define  IDC_TRACING       9003

#define  IDC_TEXTLIST      8888

#define HVDELTA      .01


#define NSEWNAME(x,y)      ((m_dwViewWhat!=VIEWWHAT_OBJECT) ? x : y)

// qualityinfo - Information about how accurate to draw
#define NUMQUALITY      6
#define IMAGESCALE      16       // default value for 1x
typedef struct {
   DWORD       dwRenderModel;    // rendering model to use
   COLORREF    cMono;            // monochromatic color
   BOOL        fBackfaceCull;    // if TRUE do backface culling
   double      fDetailPixels;    // detail that looking for
   COLORREF    cMonoBack;     // default background color for monoe
} QUALITYINFO, *PQUALITYINFO;

// OCPM - Used to pass parameters into the ObjControlPointMovePage
typedef struct {
   PCObjectSocket    pos;  // object
   DWORD             dwControl;  // control within the object
   CPoint            pObject; // location in object space
   CPoint            pViewer; // viewer location in world space
   BOOL              fWorld;  // if TRUE, show world space, else object space
   CMatrix           mObjectToWorld;   // translate from object to world space
   CMatrix           mWorldToObject;   // translate from world space to object space
   CMatrix           mWorldToGrid;  // convert to grid space
} OCPM, *POCPM;


// OAINFO - Object attribute info - used in list
typedef struct {
   ATTRIBVAL      av;      // name and value
   ATTRIBINFO     info;    // information
   BOOL           fToggleRank;      // if TRUE, toggled the rank
   PCObjectSocket pos;     // object
} OAINFO, *POAINFO;

// PMDINFO - PolyMeshDialog's info
typedef struct {
   PCObjectPolyMesh     pMesh;   // polymesh object
   PCHouseView          pView;   // view
} PMDINFO, *PPMDINFO;

HBITMAP RenderImage (PCRenderTraditional pRender, PCWorldSocket pWorld, DWORD dwQuality,
                     DWORD dwWidth, DWORD dwHeight, DWORD dwAnti, BOOL fRotate,
                     fp fIso = -1, GUID *pgEffectCode = NULL, GUID *pgEffectSub = NULL);

/*****************************************************************************
globals */
CListFixed    gListPCHouseView;       // list of house views
CListFixed    gListVIEWOBJ;      // list of views looking at objects
CListFixed    gListVIEWOBJECT;       // list of views in polymesh
static BOOL          gfListPCHouseViewInit = FALSE;   // set to true if has been initialized
static QUALITYINFO   gQualityInfo[NUMQUALITY];
static BOOL          gfQualityInitialize = FALSE;
static PSTR          gpszQualityKey = "RenderQualityA%d";
// Moved to view: static SCROLLACTION  gaScrollAction[3][4];     // scrollvar action, [0=flat,1=model,2=walk][0=top,1=right,2=bottom,3=left]
//static BOOL          gfScrollAction = FALSE;    // set to true when intialized
DWORD         gdwMouseWheel = 0;      // for mouse wheel

// DEFMOVEP - Default strings for moving selection of objects
typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY, iZ; // -1 if smaller left/bottom/front corner, 0 if mid point, 1 if right/top/back corner
} DEFMOVEP, *PDEFMOVEP;

static DEFMOVEP   gaDefMoveP[] = {
   L"Center, center, bottom", 0, 0, -1,
   L"Left, front, bottom", -1, -1, -1,
   L"Center, front, bottom", 0, -1, -1,
   L"Right, front, bottom", 1, -1, -1,
   L"Left, center, bottom", -1, 0, -1,
   L"Right, center, bottom", 1, 0, -1,
   L"Left, back, bottom", -1, 1, -1,
   L"Center, back, bottom", 0, 1, -1,
   L"Right, back, bottom", 1, 1, -1,

   L"Center, center, center", 0, 0, 0,
   L"Left, front, center", -1, -1, 0,
   L"Center, front, center", 0, -1, 0,
   L"Right, front, center", 1, -1, 0,
   L"Left, center, center", -1, 0, 0,
   L"Right, center, center", 1, 0, 0,
   L"Left, back, center", -1, 1, 0,
   L"Center, back, center", 0, 1, 0,
   L"Right, back, center", 1, 1, 0,

   L"Center, center, top", 0, 0, 1,
   L"Left, front, top", -1, -1, 1,
   L"Center, front, top", 0, -1, 1,
   L"Right, front, top", 1, -1, 1,
   L"Left, center, top", -1, 0, 1,
   L"Right, center, top", 1, 0, 1,
   L"Left, back, top", -1, 1, 1,
   L"Center, back, top", 0, 1, 1,
   L"Right, back, top", 1, 1, 1
};


/****************************************************************************
ListPCHouseView - Returns a pointer to the CListFixed containing house view objects
*/
PCListFixed ListPCHouseView (void)
{
   return &gListPCHouseView;
}

/****************************************************************************
ListVIEWOBJ - Reurns a pointer to a CListFixed containing the view objects
*/
PCListFixed ListVIEWOBJ (void)
{
   return &gListVIEWOBJ;
}

/****************************************************************************
ListVIEWOVJECT - Reurns a pointer to a CListFixed containing the view objects
*/
PCListFixed ListVIEWOBJECT (void)
{
   return &gListVIEWOBJECT;
}

/****************************************************************************
ObjectViewDestroy - If a Object view exists for a world, this closes it
UNLESS it's dirty, in which case it fails.

inputs
   PCWorldSocket pWorld - World
   GUID        *pgObject - Object viewed
returns
   BOOL - If the view doesn't exist then TRUE all the time. If it does
   exist AND it's not dirty, then it's closed and returns TRUE. Otherwise FALSE.
*/
BOOL ObjectViewDestroy (PCWorldSocket pWorld, GUID *pgObject)
{
   DWORD i;
   PVIEWOBJECT pm = (PVIEWOBJECT) gListVIEWOBJECT.Get(0);
   for (i = 0; i < gListVIEWOBJECT.Num(); i++, pm++) {
      if ((pm->pWorld == pWorld) && IsEqualGUID(pm->gObject, *pgObject)) {
         // else, deltete. The destructor will remove it from the list
         delete pm->pView;
         return TRUE;
      }
   }

   // doesn't exist
   return TRUE;
}

/****************************************************************************
ObjectViewNew - Given a world and a ground object (idenitified by its guid) this
will see if a view exists already. If one does then that is shown. If not,
a new one is created

inputs
   PCWorldSocket        pWorld - World
   GUID                 *pgObject - Object
   DWORD                dwViewWhat - VIEWWHAT_POLYMESH, VIEWWHAT_BONE, etc.
returns
   BOOL - TRUE if already exists or created, FALSE if not
*/
BOOL ObjectViewNew (PCWorldSocket pWorld, GUID *pgObject, DWORD dwViewWhat)
{
   DWORD i;
   PVIEWOBJECT pm = (PVIEWOBJECT) gListVIEWOBJECT.Get(0);
   for (i = 0; i < gListVIEWOBJECT.Num(); i++, pm++) {
      if ((pm->pWorld == pWorld) && IsEqualGUID(pm->gObject, *pgObject)) {
         ShowWindow (pm->pView->m_hWnd, SW_SHOWNORMAL);
         SetForegroundWindow (pm->pView->m_hWnd);
         return TRUE;
      }
   }

   // else if get here then create
   PCHouseView pv;
   pv = new CHouseView (pWorld, NULL, dwViewWhat, pgObject, NULL, NULL, pWorld);
   if (!pv)
      return FALSE;
   if (!pv->Init ()) {
      delete pv;
      return FALSE;
   }

   return TRUE;
}

/****************************************************************************
ObjectViewShowHide - Shows or hides the Object view for the specific
object. Used to make sure they're not viislble
while the user is animating.

inputs
   PCWorldSocket pWorld - World. If NULL then any world is acceptable
   GUID        *pgObject - Object viewed. If NULL then any object acceptable
   BOOL        fShow - TRUE to show
*/
BOOL ObjectViewShowHide (PCWorldSocket pWorld, GUID *pgObject, BOOL fShow)
{
   DWORD i;
   BOOL fFound = FALSE;

   // BUGFIX - Added ability to show/hide specific ground objects
   PVIEWOBJECT pm = (PVIEWOBJECT) gListVIEWOBJECT.Get(0);
   for (i = 0; i < gListVIEWOBJECT.Num(); i++, pm++) {
      if (pWorld && (pm->pWorld != pWorld))
         continue;
      if (pgObject && (!IsEqualGUID (pm->gObject, *pgObject)))
         continue;
      ShowWindow (pm->pView->m_hWnd, fShow ? SW_SHOW : SW_HIDE);
      fFound = TRUE;
   }
   return fFound;
}


/****************************************************************************
CloseOpenEditors - Closes all the open editors for a specific world (or all worlds)

inputs
   PCWorldSocket        pWorld - World to close for. If NULL then all closed
returns
   BOOL - TRUE if success, FALSE if fail because some worlds still open
*/
BOOL CloseOpenEditors (PCWorldSocket pWorld)
{
   BOOL fRet = TRUE;

   DWORD i;
   PCHouseView ph;
   for (i = 0; i < gListPCHouseView.Num(); i++) {
      ph = *((PCHouseView*)gListPCHouseView.Get(i));
      if (!pWorld || (ph->m_pWorld == pWorld))
         fRet &= ph->m_pWorld->ObjEditorDestroy ();
   }

   // and from list of editing objects
   for (i = 0; i < gListVIEWOBJ.Num(); i++) {
      PVIEWOBJ pvo = (PVIEWOBJ) gListVIEWOBJ.Get(i);
      if (!pWorld || (pvo->pView->m_pWorld == pWorld))
         fRet &= pvo->pView->m_pWorld->ObjEditorDestroy();
   }

   // and for OBJECT
   for (i = 0; i < gListVIEWOBJECT.Num(); i++) {
      PVIEWOBJECT pvo = (PVIEWOBJECT) gListVIEWOBJECT.Get(i);
      if (!pWorld || (pvo->pView->m_pWorld == pWorld))
         fRet &= pvo->pView->m_pWorld->ObjEditorDestroy();
   }

   if (!fRet)
      return fRet;


   // just in case didn't close all the ground editors
   return !GroundViewModifying(pWorld);
}


/****************************************************************************
FindViewForWorld - Searches through all the views and finds one that uses
the given world.

inputs
   PCWorldSocket        pWorld - World looking for
returns
   PCHouseView - One that found. NULL if error
*/
PCHouseView FindViewForWorld (PCWorldSocket pWorld)
{
   DWORD i;
   PCHouseView ph;
   for (i = 0; i < gListPCHouseView.Num(); i++) {
      ph = *((PCHouseView*)gListPCHouseView.Get(i));
      if (ph->m_pWorld == pWorld)
         return ph;
   }

   // and from list of editing objects
   for (i = 0; i < gListVIEWOBJ.Num(); i++) {
      PVIEWOBJ pvo = (PVIEWOBJ) gListVIEWOBJ.Get(i);
      if (pvo->pView->m_pWorld == pWorld)
         return pvo->pView;
   }

   // NOTE: Not doing it from VIEWOBJECT because world should be contained in FindViewForWorld

   return NULL;
}


/****************************************************************************
XYZKeyFlag - Returns a flag indicating if X, Y, and or Z keys are held down.
The flag indicates which dimensions X,Y,Z get zeroed out.

returns
   DWORD - bit 0 => X dimension zerod out with no change, 1=> Y has no change, 2=>Z has no change
*/
DWORD XYZKeyFlag (void)
{
   BOOL afPressed[3];
   DWORD i, dwCount;
   dwCount = 0;
   for (i = 0; i < 3; i++) {
      afPressed[i] = (GetKeyState ((WORD)'X' + i) < 0);
      if (afPressed[i])
         dwCount++;
   }
   if (!dwCount)
      return 0;   // none zeroed out

   // else, return inverse
   dwCount = 0;
   for (i = 0; i < 3; i++)
      if (!afPressed[i])
         dwCount |= (1 << i);
   return dwCount;
}

/****************************************************************************
CHouseView::ScrollActionDefault - Given a cameramodel, scroll info, and absolute, sets
up the global SCROLLACTION for some reasonable defaults.

inputs
   DWORD       dwModel - Camerea model, CAMERAMODEL_XXX
   DWORD       dwScroll - Scroll bar, 0=top, 1=right, 2=bottom, 3=left
   DWORD       dwAction - Action to use, SA_XXX
returns
   none
*/
void CHouseView::ScrollActionDefault (DWORD dwModel, DWORD dwScroll, DWORD dwAction)
{
   // translate the model
   switch (dwModel) {
   default:
   case CAMERAMODEL_FLAT:
      dwModel = 0;
      break;
   case CAMERAMODEL_PERSPWALKTHROUGH:
      dwModel = 2;
      break;
   case CAMERAMODEL_PERSPOBJECT:
      dwModel = 1;
      break;
   }

   // set it
   PSCROLLACTION pas;
   BOOL fFlip;
   fFlip = FALSE;
   pas = &m_aScrollAction[dwModel][dwScroll];
   pas->dwAction = dwAction;
   pas->fLog = FALSE;
   pas->fRelative = FALSE;

   switch (dwAction) {
   default:
      pas->fMin = pas->fMax = 0;
      break;
   case SA_XABSOLUTE:       // move camrera X in absolute location
   case SA_YABSOLUTE:       // move camera Y in avsoulut location
      pas->fMin = -m_fViewScale/2;
      pas->fMax = m_fViewScale/2;

      if (dwModel == 1) {
         pas->fMin = 0;
         pas->fMax = m_fViewScale;
      }
      break;
   case SA_ZABSOLUTE:       // move camera Z in absolute location
      pas->fMin = 0;
      pas->fMax = m_fViewScale/2;
      break;
   case SA_WALKZRELATIVE:
      pas->fMin = -m_fViewScale/10;
      pas->fMax = m_fViewScale/10;
      pas->fRelative = TRUE;
      break;

   case SA_XROTABSOLUTE:       // rotation around X in absolute tersm - radians
      pas->fMin = -PI/2;
      pas->fMax = PI/2;

      if (dwModel == 2)
         fFlip = !fFlip;
      break;
   case SA_YROTABSOLUTE:       // rotation around Y in absolute tersm - radians
   case SA_ZROTABSOLUTE:       // rotation around Z in absolute tersm - radians
      pas->fMin = -PI;
      pas->fMax = PI;

      if (dwModel == 2)
         fFlip = !fFlip;
      break;
   case SA_LRRELATIVE:       // move camera LR relative to looking at
   case SA_FBRELATIVE:       // move camera FB relative to looking at (positive numbers are forward)
   case SA_UDRELATIVE:       // move camera UD relative to looking at
      pas->fMin = -m_fViewScale/10;
      pas->fMax = m_fViewScale/10;
      pas->fRelative = TRUE;
      if (dwModel == 2)
         fFlip = !fFlip;

      break;
   case SA_LRROTRELATIVE:       // rotate around LR relative to looking at
   case SA_FBROTRELATIVE:       // rotate around FB relative to looking at (positive numbers are forward)
   case SA_UDROTRELATIVE:       // rotate around UD relative to looking at
      pas->fMin = -PI/2;
      pas->fMax = PI/2;
      pas->fRelative = TRUE;
      break;
   case SA_FLATZOOMABSOLUTE:       // absoluate zoom, with length being # meters visible across
      pas->fMin = m_fViewScale/100;
      pas->fMax = m_fViewScale;
      pas->fLog = TRUE;
      break;
   case SA_FLATZOOMRELATIVE:       // relative zoom, with numbers being multiplier
      pas->fMin = 1.0 / 5;
      pas->fMax = 5;
      pas->fLog = TRUE;
      pas->fRelative = TRUE;
      break;
   case SA_FLATLUDABSOLUTE:       // scroll UD, absolute
      pas->fMin = -m_fViewScale/2;
      pas->fMax = m_fViewScale/2;

      //if (dwModel == 0)
      //   fFlip = !fFlip;
      break;
   case SA_FLATLRABSOLUTE:       // scroll LR, avsolute
      pas->fMin = -m_fViewScale/2;
      pas->fMax = m_fViewScale/2;
      fFlip = !fFlip;
      break;

   case SA_PERSPFOV:       // field of view
      pas->fMin = 0;
      pas->fMax = PI;
      break;
   }

   if (fFlip) {
      pas->fMin *= -1;
      pas->fMax *= -1;
   }
}


/****************************************************************************
CHouseView::ScrollActionInit - Initialize theh scrollbar action if it isn't already
initialized.
*/
void CHouseView::ScrollActionInit (void)
{
   // BUGFIX - Swap left and right scrollbars because mouse wheel only seems to affect
   // right-most scrollbar

   // BUGFIX - Swapped back because WM_MOUSEWHEEL works properly on NT XP, but
   // not on WIn98/ME. Don't care about Win98/ME.

   ScrollActionDefault (CAMERAMODEL_FLAT, 0, SA_NONE);
   ScrollActionDefault (CAMERAMODEL_FLAT, 1, SA_FLATLUDABSOLUTE);
   ScrollActionDefault (CAMERAMODEL_FLAT, 2, SA_FLATLRABSOLUTE);
   ScrollActionDefault (CAMERAMODEL_FLAT, 3, SA_FLATZOOMABSOLUTE);

   ScrollActionDefault (CAMERAMODEL_PERSPWALKTHROUGH, 0, SA_WALKZRELATIVE);
   ScrollActionDefault (CAMERAMODEL_PERSPWALKTHROUGH, 1, SA_XROTABSOLUTE);
   ScrollActionDefault (CAMERAMODEL_PERSPWALKTHROUGH, 2, SA_ZROTABSOLUTE);
   ScrollActionDefault (CAMERAMODEL_PERSPWALKTHROUGH, 3, SA_FBRELATIVE);

   ScrollActionDefault (CAMERAMODEL_PERSPOBJECT, 0, SA_YROTABSOLUTE);
   ScrollActionDefault (CAMERAMODEL_PERSPOBJECT, 1, SA_XROTABSOLUTE);
   ScrollActionDefault (CAMERAMODEL_PERSPOBJECT, 2, SA_ZROTABSOLUTE);
   ScrollActionDefault (CAMERAMODEL_PERSPOBJECT, 3, SA_YABSOLUTE);
}


/****************************************************************************
CHouseView::ScrollActionFromScrollbar - When scrollbar is moved this calls into the camera model
and changed it.

inputs
   int         nPos - Position of scrollbar, from 0 to 1000
   DWORD       dwScroll - Scrollbar number
   DWORD       dwScroll - Scrollbar number, 0=top, 1=right, 2=bottom, 3=left
   PCRenderTraidional    pRender - Get the current render model from, and other info
returns
   BOOL - TRUE if changed anything, FALSE if no change
*/
BOOL CHouseView::ScrollActionFromScrollbar (int nPos, DWORD dwScroll, PCRenderTraditional pRender)
{
   DWORD dwModel;
   fp fVal;
   fp fFOV, fScale;
   CPoint pCenterOfRot, pRot, pCamera;
   pCenterOfRot.Zero();
   pRot.Zero();
   pCamera.Zero();
   fScale = 0;

   // not called anymore - ScrollActionInit ();

   CPoint pCenter;
   CMatrix mCenter;
   switch (pRender->CameraModelGet ()) {
   default:
   case CAMERAMODEL_FLAT:
      dwModel = 0;
      pRender->CameraFlatGet (&pCenterOfRot, &pRot.p[2], &pRot.p[0], &pRot.p[1],
         &fScale, &pCamera.p[0], &pCamera.p[1]);
      break;
   case CAMERAMODEL_PERSPWALKTHROUGH:
      dwModel = 2;
      pRender->CameraPerspWalkthroughGet (&pCamera, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);
      break;
   case CAMERAMODEL_PERSPOBJECT:
      dwModel = 1;
      pRender->CameraPerspObjectGet (&pCamera, &pCenterOfRot, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);

      // BUGFIX - Remove the center of the world from the camera location so scroll bars set ok
      pCenter.Zero();
      mCenter.FromXYZLLT (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1]);
      pCenter.Copy (&pCenterOfRot);
      pCenter.Subtract (&m_pCenterWorld);   // BUGFIX - Remove center of world from camera location so scroll bars set ok
      pCenter.p[3] = 1;
      pCenter.MultiplyLeft (&mCenter);
      pCamera.Subtract (&pCenter);
      break;
   }

   // set it
   PSCROLLACTION pas;
   pas = &m_aScrollAction[dwModel][dwScroll];

   // which value gets modified
   fp    *pfVal;    // what value will be used
   switch (pas->dwAction) {
   default:
      pfVal = NULL;
      break;
   case SA_XABSOLUTE:       // move camrera X in absolute location
      pfVal = &pCamera.p[0];
      break;
   case SA_YABSOLUTE:       // move camera Y in avsoulut location
      pfVal = &pCamera.p[1];
      break;
   case SA_ZABSOLUTE:       // move camera Z in absolute location
   case SA_WALKZRELATIVE:
      pfVal = &pCamera.p[2];
      break;
   case SA_XROTABSOLUTE:       // rotation around X in absolute tersm - radians
      pfVal = &pRot.p[0];
      break;
   case SA_YROTABSOLUTE:       // rotation around Y in absolute tersm - radians
      pfVal = &pRot.p[1];
      break;
   case SA_ZROTABSOLUTE:       // rotation around Z in absolute tersm - radians
      pfVal = &pRot.p[2];
      break;
   case SA_LRRELATIVE:       // move camera LR relative to looking at
   case SA_UDRELATIVE:       // move camera UD relative to looking at
   case SA_LRROTRELATIVE:       // rotate around LR relative to looking at
   case SA_FBROTRELATIVE:       // rotate around FB relative to looking at (positive numbers are forward)
   case SA_UDROTRELATIVE:       // rotate around UD relative to looking at
      // IMPORTANT: Not supported right now
      pfVal = NULL;
      break;
   case SA_FBRELATIVE:       // move camera FB relative to looking at (positive numbers are forward)
      pfVal = NULL;
      if (dwModel == 2) {
         // move
         if (pas->fMax != pas->fMin)
            fVal = (nPos / 1000.0) * (pas->fMax - pas->fMin) + pas->fMin;
         pCamera.p[0] += sin(-pRot.p[2]) * fVal;
         pCamera.p[1] += cos(-pRot.p[2]) * fVal;

         goto setnow;
      }
      break;
   case SA_FLATZOOMRELATIVE:       // relative zoom, with numbers being multiplier
   case SA_FLATZOOMABSOLUTE:       // absoluate zoom, with length being # meters visible across
      pfVal = &fScale;
      break;
   case SA_FLATLRABSOLUTE:       // scroll LR, avsolute
      pfVal = &pCamera.p[0];
      break;
   case SA_FLATLUDABSOLUTE:       // scroll UD, absolute
      pfVal = &pCamera.p[1];
      break;
   case SA_PERSPFOV:       // field of view
      pfVal = &fFOV;
      break;
   }
   if (!pfVal)
      return FALSE;  // cant change this

   fp fMin, fMax;
   fMin = pas->fMin;
   fMax = pas->fMax;

   // if logrithmic then convert all to log
   if (pas->fLog) {
      fMin = max(CLOSE, fMin);
      fMax = max(CLOSE, fMax);
      fMin = log (fMin);
      fMax = log (fMax);
   }

   // conver to a nuumber between min and max
   fVal = (nPos / 1000.0) * (fMax - fMin) + fMin;

   // if it's log then de-log
   if (pas->fLog)
      fVal = exp(fVal);

   if (pas->fRelative) {
      if (pas->fLog)
         fVal *= (*pfVal);
      else
         fVal += (*pfVal);
   }

   // if no change done
   if (fabs(*pfVal - fVal) < CLOSE)
      return FALSE;

   // store it and change renderer
   *pfVal = fVal;

setnow:
   switch (pRender->CameraModelGet ()) {
   default:
   case CAMERAMODEL_FLAT:
      // BUGFIX - Do the center of rotation
      pCenter.Zero();
      mCenter.FromXYZLLT (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1]);
      pCenter.Copy (&pCenterOfRot);
      pCenter.Subtract (&m_pCenterWorld);   // BUGFIX - For polymesh
      pCenter.p[3] = 1;
      pCenter.MultiplyLeft (&mCenter);
      if (pas->dwAction == SA_FLATLRABSOLUTE)
         pCamera.p[0] += pCenter.p[0];
      else if (pas->dwAction == SA_FLATLUDABSOLUTE)
         pCamera.p[1] += pCenter.p[2];

      pRender->CameraFlat (&pCenterOfRot, pRot.p[2], pRot.p[0], pRot.p[1],
         fScale, pCamera.p[0], pCamera.p[1]);
      break;
   case CAMERAMODEL_PERSPWALKTHROUGH:
      pRender->CameraPerspWalkthrough (&pCamera, pRot.p[2], pRot.p[0], pRot.p[1], fFOV);
      break;
   case CAMERAMODEL_PERSPOBJECT:
      // BUGFIX - Do the center of rotation
      // use pcenter from above
#if 0
      pCenter.Zero();
      mCenter.FromXYZLLT (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1]);
      pCenter.Copy (&pCenterOfRot);
      pCenter.Subtract (&m_pCenterWorld);   // BUGFIX - For polymesh
      pCenter.p[3] = 1;
      pCenter.MultiplyLeft (&mCenter);
#endif // 0
      pCamera.Add (&pCenter);

      pRender->CameraPerspObject (&pCamera, &pCenterOfRot, pRot.p[2], pRot.p[0], pRot.p[1], fFOV);
      break;
   }

   return TRUE;
}

/****************************************************************************
CHouseView::ScrollActionUpdateScrollbar - Given a scrollbar number, and HWND to it, this
updates the scrollbar to the correct value.

inputs
   HWND        hWnd - Scrollbar window
   DWORD       dwScroll - Scrollbar number, 0=top, 1=right, 2=bottom, 3=left
   PCRenderTraidional    pRender - Get the current render model from, and other info
returns
   none
*/
void CHouseView::ScrollActionUpdateScrollbar (HWND hWnd, DWORD dwScroll, PCRenderTraditional pRender)
{
   DWORD dwModel;
   fp fFOV, fScale;
   CPoint pCenterOfRot, pRot, pCamera;
   pCenterOfRot.Zero();
   pRot.Zero();
   pCamera.Zero();
   fScale = 0;

   // not called anymore - ScrollActionInit ();

   CPoint pCenter;
   CMatrix mCenter;
   switch (pRender->CameraModelGet ()) {
   default:
   case CAMERAMODEL_FLAT:
      dwModel = 0;
      pRender->CameraFlatGet (&pCenterOfRot, &pRot.p[2], &pRot.p[0], &pRot.p[1],
         &fScale, &pCamera.p[0], &pCamera.p[1]);

      // BUGFIX - Do the center of rotation
      pCenter.Zero();
      mCenter.FromXYZLLT (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1]);
      pCenter.Copy (&pCenterOfRot);
      pCenter.Subtract (&m_pCenterWorld);   // BUGFIX - Remove center of world from camera location so scroll bars set ok
      pCenter.p[3] = 1;
      pCenter.MultiplyLeft (&mCenter);
      pCamera.p[0] -= pCenter.p[0];
      pCamera.p[1] -= pCenter.p[2];

      break;
   case CAMERAMODEL_PERSPWALKTHROUGH:
      dwModel = 2;
      pRender->CameraPerspWalkthroughGet (&pCamera, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);
      break;
   case CAMERAMODEL_PERSPOBJECT:
      dwModel = 1;
      pRender->CameraPerspObjectGet (&pCamera, &pCenterOfRot, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);

      // BUGFIX - Remove the center of the world from the camera location so scroll bars set ok
      pCenter.Zero();
      mCenter.FromXYZLLT (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1]);
      pCenter.Copy (&pCenterOfRot);
      pCenter.Subtract (&m_pCenterWorld);   // BUGFIX - Remove center of world from camera location so scroll bars set ok
      pCenter.p[3] = 1;
      pCenter.MultiplyLeft (&mCenter);
      pCamera.Subtract (&pCenter);
      break;
   }

   // set it
   PSCROLLACTION pas;
   pas = &m_aScrollAction[dwModel][dwScroll];

   // enable/disable depenind upong what want
   BOOL fWantEnable, fEnabled;
   fWantEnable = (pas->dwAction != SA_NONE);
   fEnabled = IsWindowEnabled (hWnd);
   if (!fWantEnable != !fEnabled)
      EnableWindow (hWnd, fWantEnable);
   if (!fWantEnable)
      return;  // keep disabled

   // which value gets modified
   fp    fVal;    // what value will be used
   fp    fSlider; // slider size, in same units as fVal
   fSlider = 0;
   switch (pas->dwAction) {
   default:
      fVal = 0;
      break;
   case SA_XABSOLUTE:       // move camrera X in absolute location
      fVal = pCamera.p[0];
      break;
   case SA_YABSOLUTE:       // move camera Y in avsoulut location
      fVal = pCamera.p[1];
      break;
   case SA_ZABSOLUTE:       // move camera Z in absolute location
      fVal = pCamera.p[2];
      break;
   case SA_XROTABSOLUTE:       // rotation around X in absolute tersm - radians
      fVal = myfmod(pRot.p[0] + PI, 2 * PI) - PI;
      break;
   case SA_YROTABSOLUTE:       // rotation around Y in absolute tersm - radians
      fVal = myfmod(pRot.p[1] + PI, 2 * PI) - PI;
      break;
   case SA_ZROTABSOLUTE:       // rotation around Z in absolute tersm - radians
      fVal = myfmod(pRot.p[2] + PI, 2 * PI) - PI;
      break;
   case SA_LRRELATIVE:       // move camera LR relative to looking at
   case SA_FBRELATIVE:       // move camera FB relative to looking at (positive numbers are forward)
   case SA_UDRELATIVE:       // move camera UD relative to looking at
   case SA_LRROTRELATIVE:       // rotate around LR relative to looking at
   case SA_FBROTRELATIVE:       // rotate around FB relative to looking at (positive numbers are forward)
   case SA_UDROTRELATIVE:       // rotate around UD relative to looking at
   case SA_FLATZOOMRELATIVE:       // relative zoom, with numbers being multiplier
   case SA_WALKZRELATIVE:
      fVal = 0;   // since relative motions always show camrea at center
      break;
   case SA_FLATZOOMABSOLUTE:       // absoluate zoom, with length being # meters visible across
      fVal = fScale;
      break;
   case SA_FLATLRABSOLUTE:       // scroll LR, avsolute
      fVal = pCamera.p[0];
      fSlider = fScale;
      break;
   case SA_FLATLUDABSOLUTE:       // scroll UD, absolute
      fVal = pCamera.p[1];
      fSlider = fScale;
      break;
   case SA_PERSPFOV:       // field of view
      fVal = fFOV;
      break;
   }

   fp fMin, fMax;
   fMin = pas->fMin;
   fMax = pas->fMax;

   // if logrithmic then convert all to log
   if (pas->fLog) {
      fMin = max(CLOSE, fMin);
      fMax = max(CLOSE, fMax);
      fVal = max(CLOSE, fVal);
      fMin = log (fMin);
      fMax = log (fMax);
      fVal = log (fVal);
      fSlider = 0;
   }

   if (fabs(fMax - fMin) > CLOSE) {
      fVal = (fVal - fMin) / (fMax - fMin);
      fSlider /= (fMax - fMin);
   }
   else
      fVal = fSlider = 0;

   // keep in line
   fVal = max(0, fVal);
   fVal = min(1, fVal);
   fSlider = fabs(fSlider);
   fSlider = min(1, fSlider);
   if (!fSlider)
      fSlider = .1;  // at least this distance

   // figure out what want
   SCROLLINFO siWant;
   memset (&siWant, 0, sizeof(siWant));
   siWant.cbSize = sizeof(siWant);
   siWant.fMask = SIF_ALL;
   siWant.nPage = (DWORD) (fSlider * 1000.0);
   siWant.nMax = 1000 + siWant.nPage;
   siWant.nMin = 0;
   siWant.nPos = (int) (fVal * 1000.0);

   // get the current scroll
   SCROLLINFO si;
   memset (&si, 0, sizeof(si));
   si.cbSize = sizeof(si);
   si.fMask = SIF_ALL;
   GetScrollInfo (hWnd, SB_CTL, &si);

   if ((si.nMax != siWant.nMax) || (si.nMin != siWant.nMin) || (si.nPage != siWant.nPage) ||
      (si.nPos != siWant.nPos))
      SetScrollInfo (hWnd, SB_CTL, &siWant, TRUE);

   // done
}


/****************************************************************************
MoveSetPage
*/
BOOL MoveSetPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the combobox
         DWORD i;
         PCEscControl pControl = pPage->ControlFind (L"reference");
         WCHAR szTemp[256];
         for (i = 0; ; i++) {
            if (!pv->MoveReferenceStringQuery(i, szTemp, sizeof(szTemp), 0))
               break;
            ESCMCOMBOBOXADD cba;
            memset (&cba, 0, sizeof(cba));
            cba.dwInsertBefore = -1;
            cba.pszText = szTemp;
            if (pControl)
               pControl->Message (ESCM_COMBOBOXADD, &cba);
         }
         if (pControl) {
            pControl->AttribSetInt (CurSel(), (int) pv->MoveReferenceCurQuery());
         }
         
         
         // update buttons
         pPage->Message (ESCM_USER+82);

         // set the text for that rotation
         CPoint pLoc, pRot;
         CMatrix m;
         pv->MoveReferenceMatrixGet (&m);
         m.ToXYZLLT (&pLoc, &pRot.p[2], &pRot.p[0], &pRot.p[1]);
         AngleToControl (pPage, L"rotx", pRot.p[0], TRUE);
         AngleToControl (pPage, L"roty", pRot.p[1], TRUE);
         AngleToControl (pPage, L"rotz", pRot.p[2], TRUE);
      }
      break;

   case ESCM_USER+82:   // set the text for the location
      {
         CMatrix m;
         CPoint pRef, pc;
         pc.Zero();
         pv->MoveReferencePointQuery(pv->MoveReferenceCurQuery(), &pRef);
         pv->MoveReferenceMatrixGet (&m);

         // convert to world space
         pRef.p[3] = 1;
         pc.MultiplyLeft (&m);
         pRef.MultiplyLeft (&m);

         pRef.MultiplyLeft (&pv->m_mWorldToGrid);  // to grid space

         // display
         MeasureToString (pPage, L"transx", pRef.p[0]);
         MeasureToString (pPage, L"transy", pRef.p[1]);
         MeasureToString (pPage, L"transz", pRef.p[2]);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Object orientation";
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (_wcsicmp(p->pControl->m_pszName, L"reference"))
            break;

         // if no change do nothing
         if (p->dwCurSel == pv->MoveReferenceCurQuery())
            break;

         pv->MoveReferenceCurSet (p->dwCurSel);


         // display new settings
         pv->UpdateToolTipByPointerMode();

         // else changed, so refresh
         pPage->Message (ESCM_USER+82);
      }
      return 0;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (!(_wcsicmp(p->pControl->m_pszName, L"rotx") && _wcsicmp(p->pControl->m_pszName, L"roty") &&
            _wcsicmp(p->pControl->m_pszName, L"rotz"))) {

            // get all the values
            CPoint pNew;
            pNew.p[0] = AngleFromControl (pPage, L"rotx");
            pNew.p[1] = AngleFromControl (pPage, L"roty");
            pNew.p[2] = AngleFromControl (pPage, L"rotz");

            // find out difference between center and point
            CMatrix m, mOld;
            CPoint pRef, pc, pOld;
            pv->MoveReferencePointQuery(pv->MoveReferenceCurQuery(), &pRef);
            pRef.p[3] = 1;
            pv->MoveReferenceMatrixGet (&m);
            mOld.Copy (&m);

            // convert to world space
            pOld.Copy (&pRef);
            pRef.MultiplyLeft (&m);
            pRef.MultiplyLeft (&pv->m_mGridToWorld);

            // change the rotation
            pc.Zero();
            m.FromXYZLLT (&pc, pNew.p[2], pNew.p[0], pNew.p[1]);

            // create a translation vector that will translate the center
            // point as it is now to where it was before the rotation
            // was applied
            CMatrix mTrans;
            pOld.MultiplyLeft (&m);
            mTrans.Translation (pRef.p[0] - pOld.p[0],
               pRef.p[1] - pOld.p[1],
               pRef.p[2] - pOld.p[2]);
            mTrans.MultiplyLeft (&m);

            // set it
            pv->MoveReferenceMatrixSet (&mTrans, &mOld);
            // display new settings
            pv->UpdateToolTipByPointerMode();
         }
         else if (!(_wcsicmp(p->pControl->m_pszName, L"transx") && _wcsicmp(p->pControl->m_pszName, L"transy") &&
            _wcsicmp(p->pControl->m_pszName, L"transz"))) {

            // get all the values
            CPoint pNew;
            if (!MeasureParseString (pPage, L"transx", &pNew.p[0]))
               pNew.p[0] = 0;
            if (!MeasureParseString (pPage, L"transy", &pNew.p[1]))
               pNew.p[1] = 0;
            if (!MeasureParseString (pPage, L"transz", &pNew.p[2]))
               pNew.p[2] = 0;


            // convert this from grid to world
            pNew.p[3] = 1;
            pNew.MultiplyLeft (&pv->m_mGridToWorld);

            // find out difference between center and point
            CMatrix m;
            CPoint pRef, pc;
            pc.Zero();
            pv->MoveReferencePointQuery(pv->MoveReferenceCurQuery(), &pRef);
            pRef.p[3] = 1;
            pv->MoveReferenceMatrixGet (&m);

            // convert to world space
            pc.MultiplyLeft (&m);
            pRef.MultiplyLeft (&m);

            // translate it
            CMatrix trans;
            trans.Translation (pNew.p[0] - pRef.p[0], pNew.p[1] - pRef.p[1], pNew.p[2] - pRef.p[2]);
            trans.MultiplyLeft (&m);
            pv->MoveReferenceMatrixSet (&trans, &m);

            // display new settings
            pv->UpdateToolTipByPointerMode();
         }
      }
      return 0;

   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
MoveSettings - This brings up the UI for movemment settings. As parameters are changed
the view object will be notified and told to refresh.

inputs
   PCHouseView    pView - view to use
returns
   none
*/
void MoveSettings (PCHouseView pView)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (pView->m_hWnd, &r);

   cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLMOVESET, MoveSetPage, pView);
   return;
}


/****************************************************************************
SelectRegionPage
*/
BOOL SelectRegionPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"mustbein");
         if (pControl && pv->m_fSelRegionWhollyIn)
            pControl->AttribSetBOOL (Checked(), TRUE);
         pControl = pPage->ControlFind (L"mustbevisible");
         if (pControl && pv->m_fSelRegionVisible)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         BOOL fMustBeIn, fMustBeVisible;
         fMustBeIn = pv->m_fSelRegionWhollyIn;
         fMustBeVisible = pv->m_fSelRegionVisible;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"mustbein")) {
            pv->m_fSelRegionWhollyIn = p->pControl->AttribGetBOOL (Checked());
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"mustbevisible")) {
            pv->m_fSelRegionVisible = p->pControl->AttribGetBOOL (Checked());
         }

         // dont need to refresh

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Region selection options";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
SelectColorPage
*/
BOOL SelectColorPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         COLORREF cOutline = pv->m_apRender[0]->m_pEffectOutline ? pv->m_apRender[0]->m_pEffectOutline->m_cOutlineSelected : 0;
         FillStatusColor (pPage, L"selcolor", cOutline);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"bound");
         if (pControl && pv->m_apRender[0]->m_fSelectedBound)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         BOOL fSelectedBound;
         COLORREF cOutlineSelected;
         COLORREF cOutline = pv->m_apRender[0]->m_pEffectOutline ? pv->m_apRender[0]->m_pEffectOutline->m_cOutlineSelected : 0;
         fSelectedBound = pv->m_apRender[0]->m_fSelectedBound;
         cOutlineSelected = cOutline;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"changeselcolor")) {
            if (pv->m_apRender[0]->m_pEffectOutline)
               pv->m_apRender[0]->m_pEffectOutline->m_cOutlineSelected = AskColor (pPage->m_pWindow->m_hWnd, cOutline, pPage, L"selcolor");
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"bound")) {
            pv->m_apRender[0]->m_fSelectedBound = p->pControl->AttribGetBOOL (Checked());
         }

         // if want refresh then update
         if ((fSelectedBound != pv->m_apRender[0]->m_fSelectedBound) ||
             (cOutlineSelected != pv->m_apRender[0]->m_pEffectOutline->m_cOutlineSelected))
            pv->RenderUpdate(WORLDC_CAMERAMOVED);

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Selection color";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
SelectPage
*/
BOOL SelectPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Selection settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}
/****************************************************************************
SelectionSettings - This brings up the UI for selection settings. As parameters are changed
the view object will be notified and told to refresh.

inputs
   PCHouseView    pView - view to use
returns
   none
*/
void SelectionSettings (PCHouseView pView)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (pView->m_hWnd, &r);

   cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

firstpage:
   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLSELECT, SelectPage, pView);
   if (!pszRet)
      return;

   if (!_wcsicmp(pszRet, L"color")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLSELECTCOLOR, SelectColorPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   if (!_wcsicmp(pszRet, L"region")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLSELECTREGION, SelectRegionPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else  // probably pressed back
      return;  // exit
}

/****************************************************************************
ClipSetFloorsPage
*/
BOOL ClipSetFloorsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         RTCLIPPLANE t;
         PCEscControl pControl;

         // get the current clipping plane for floor above
         if (pv->m_apRender[0]->ClipPlaneGet (CLIPPLANE_FLOORABOVE, &t)) {
            // set the button
            pControl = pPage->ControlFind (L"clipabove");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);
         }
         else
            t.ap[0].p[2] = 3;
         MeasureToString (pPage, L"above", t.ap[0].p[2]);

      }
      break;

   case ESCM_USER+10:   // update
      {
         // get all the values
         RTCLIPPLANE t;

         // get the current clipping plane for floor above
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"clipabove");
         if (pControl && pControl->AttribGetBOOL(Checked())) {
            t.dwID = CLIPPLANE_FLOORABOVE;
            t.ap[0].Zero();
            if (!MeasureParseString (pPage, L"above", &t.ap[0].p[2]))
               t.ap[0].p[2] = 0;
            t.ap[1].Copy (&t.ap[0]);
            t.ap[1].p[0] = 1;
            t.ap[2].Copy (&t.ap[0]);
            t.ap[2].p[1] = -1;

            pv->m_apRender[0]->ClipPlaneSet (t.dwID, &t);
         }
         else
            pv->m_apRender[0]->ClipPlaneRemove(CLIPPLANE_FLOORABOVE);

         pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         pPage->Message (ESCM_USER+10);   // refresh
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!wcsncmp(p->pControl->m_pszName, L"clip", 4))
            pPage->Message (ESCM_USER+10);   // refresh

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Show and hide floors/levels";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
ClipSetPage
*/
BOOL ClipSetPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Clipping settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
ClipSetHandPage
*/
BOOL ClipSetHandPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // get the current clipping plane
         RTCLIPPLANE t;
         PCEscControl pControl;
         if (!pv->m_apRender[0]->ClipPlaneGet (CLIPPLANE_USER, &t)) {
            // not acive so make it up
            t.ap[0].Zero();
            t.ap[1].Zero();
            t.ap[1].p[0] = 1;
            t.ap[2].Zero();
            t.ap[2].p[1] = 1;
         }
         else {
            // set the button
            pControl = pPage->ControlFind (L"showplane");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);
         }

         // distances
         DWORD i, j;
         for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) {
            WCHAR szName[16];
            swprintf (szName, L"p%d%c", (int) i+1, (WCHAR) L'x' + j);
            MeasureToString (pPage, szName, t.ap[i].p[j]);
         }
      }
      break;

   case ESCM_USER+10:   // update
      {
         // get all the values
         RTCLIPPLANE t;
         memset (&t, 0, sizeof(t));
         t.dwID = CLIPPLANE_USER;

         // distances
         DWORD i, j;
         for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) {
            WCHAR szName[16];
            swprintf (szName, L"p%d%c", (int) i+1, (WCHAR) L'x' + j);
            if (!MeasureParseString (pPage, szName, &t.ap[i].p[j]))
               t.ap[i].p[j] = 0;
         }

         // set or delete
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"showplane");
         if (pControl && pControl->AttribGetBOOL(Checked()))
            pv->m_apRender[0]->ClipPlaneSet (CLIPPLANE_USER, &t);
         else
            pv->m_apRender[0]->ClipPlaneRemove (CLIPPLANE_USER);
         pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (p->pControl->m_pszName[0] == L'p')
            pPage->Message (ESCM_USER+10);   // refresh
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"showplane"))
            pPage->Message (ESCM_USER+10);   // refresh

      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Hand-modify clipping";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/* ClipLevelsPage
*/
BOOL ClipLevelsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         fp afElev[NUMLEVELS], fHigher;
         GlobalFloorLevelsGet (pv->m_pWorld, NULL, afElev, &fHigher);

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < NUMLEVELS; i++) {
            swprintf (szTemp, L"level%d", (int) i);
            MeasureToString (pPage, szTemp, afElev[i]);
         }

         MeasureToString (pPage, L"upperlevels", fHigher);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"commit")) {
            fp afElev[NUMLEVELS], fHigher;

            // get the values
            DWORD i;
            WCHAR szTemp[64];
            for (i = 0; i < NUMLEVELS; i++) {
               swprintf (szTemp, L"level%d", (int) i);
               MeasureParseString (pPage, szTemp, &afElev[i]);
            }

            MeasureParseString (pPage, L"upperlevels", &fHigher);
            fHigher = max(fHigher, .1);

            GlobalFloorLevelsSet (pv->m_pWorld, NULL, afElev, fHigher);

            pPage->Exit (Back());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Levels";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/* ClipShowPage
*/
BOOL ClipShowPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // replace all the items
         WCHAR szTemp[16];
         PCEscControl pControl;
         DWORD i;
         for (i = 0; i < 32; i++) {
            if (!(pv->m_apRender[0]->RenderShowGet() & (1 << i)))
               continue;

            // generate name
            swprintf (szTemp, L"sh%d", i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if ((p->pControl->m_pszName[0], L"s") && (p->pControl->m_pszName[1], L"h")) {
            DWORD i;
            i = _wtoi (p->pControl->m_pszName + 2);
            i = 1 << i;

            if (p->pControl->AttribGetBOOL(Checked()))
               pv->m_apRender[0]->RenderShowSet (pv->m_apRender[0]->RenderShowGet() | i);
            else
               pv->m_apRender[0]->RenderShowSet (pv->m_apRender[0]->RenderShowGet() & (~i));

            pv->RenderUpdate(WORLDC_NEEDTOREDRAW);

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Show and hide items";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
ClipSettings - This brings up the UI for view settings. As parameters are changed
the view object will be notified and told to refresh.

inputs
   PCHouseView    pView - view to use
returns
   none
*/
void ClipSettings (PCHouseView pView)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (pView->m_hWnd, &r);

   cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

firstpage:
   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLCLIPSET, ClipSetPage, pView);
   if (!pszRet)
      return;

   if (!_wcsicmp(pszRet, L"handmodify")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLCLIPSETHAND, ClipSetHandPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else if (!_wcsicmp(pszRet, L"levels")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLCLIPLEVELS, ClipLevelsPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else if (!_wcsicmp(pszRet, L"floors")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLCLIPSETFLOORS, ClipSetFloorsPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else if (!_wcsicmp(pszRet, L"items")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLCLIPSHOW, ClipShowPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else  // probably pressed back
      return;  // exit
}


/****************************************************************************
ViewWalkPage
*/
BOOL ViewWalkPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         CPoint p;
         fp fRotZ, fRotX, fRotY, fFOV;
         pv->m_apRender[0]->CameraPerspWalkthroughGet(&p, &fRotZ, &fRotX, &fRotY, &fFOV);

         AngleToControl (pPage, L"rotz", fRotZ, TRUE);
         AngleToControl (pPage, L"rotx", fRotX, TRUE);
         AngleToControl (pPage, L"roty", fRotY, TRUE);
         AngleToControl (pPage, L"grid", pv->m_fVWalkAngle);

         MeasureToString (pPage, L"transx", p.p[0]);
         MeasureToString (pPage, L"transy", p.p[1]);
         MeasureToString (pPage, L"transz", p.p[2]);

         MeasureToString (pPage, L"speed", pv->m_fVWWalkSpeed);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // since all the edit controls are controlled, just get all the values
         CPoint pt;
         fp fRotZ, fRotX, fRotY, fFOV;
         pv->m_apRender[0]->CameraPerspWalkthroughGet (&pt, &fRotZ, &fRotX, &fRotY, &fFOV);
         fRotZ = AngleFromControl (pPage, L"rotz");
         fRotX = AngleFromControl (pPage, L"rotx");
         fRotY = AngleFromControl (pPage, L"roty");
         pv->m_fVWalkAngle = AngleFromControl (pPage, L"grid");
         pv->m_fVWalkAngle = max(0.0001, pv->m_fVWalkAngle);

         if (!MeasureParseString (pPage, L"transx", &pt.p[0]))
            pt.p[0] = 0;
         if (!MeasureParseString (pPage, L"transy", &pt.p[1]))
            pt.p[1] = 0;
         if (!MeasureParseString (pPage, L"transz", &pt.p[2]))
            pt.p[2] = 0;
         if (!MeasureParseString (pPage, L"speed", &pv->m_fVWWalkSpeed))
            pv->m_fVWWalkSpeed = 0;

         pv->m_apRender[0]->CameraPerspWalkthrough (&pt, fRotZ, fRotX, fRotY, fFOV);
         pv->CameraPosition();
         pv->RenderUpdate(WORLDC_CAMERAMOVED);
      }
      break;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Walkthrough view settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
View3DPage
*/
BOOL View3DPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         CPoint p, pCenter;
         fp fRotZ, fRotX, fRotY, fFOV;
         pv->m_apRender[0]->CameraPerspObjectGet (&p, &pCenter, &fRotZ, &fRotX, &fRotY, &fFOV);

         AngleToControl (pPage, L"rotz", fRotZ);
         AngleToControl (pPage, L"rotx", fRotX);
         AngleToControl (pPage, L"roty", fRotY);

         MeasureToString (pPage, L"transx", p.p[0]);
         MeasureToString (pPage, L"transy", p.p[1]);
         MeasureToString (pPage, L"transz", p.p[2]);

         MeasureToString (pPage, L"cx", pCenter.p[0]);
         MeasureToString (pPage, L"cy", pCenter.p[1]);
         MeasureToString (pPage, L"cz", pCenter.p[2]);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // since all the edit controls are controlled, just get all the values
         CPoint pt, pCenter;
         fp fRotZ, fRotX, fRotY, fFOV;
         pv->m_apRender[0]->CameraPerspObjectGet (&pt, &pCenter, &fRotZ, &fRotX, &fRotY, &fFOV);
         fRotZ = AngleFromControl (pPage, L"rotz");
         fRotX = AngleFromControl (pPage, L"rotx");
         fRotY = AngleFromControl (pPage, L"roty");

         if (!MeasureParseString (pPage, L"cx", &pCenter.p[0]))
            pCenter.p[0] = 0;
         if (!MeasureParseString (pPage, L"cy", &pCenter.p[1]))
            pCenter.p[1] = 0;
         if (!MeasureParseString (pPage, L"cz", &pCenter.p[2]))
            pCenter.p[2] = 0;

         if (!MeasureParseString (pPage, L"transx", &pt.p[0]))
            pt.p[0] = 0;
         if (!MeasureParseString (pPage, L"transy", &pt.p[1]))
            pt.p[1] = 0;
         if (!MeasureParseString (pPage, L"transz", &pt.p[2]))
            pt.p[2] = 0;


         pv->m_apRender[0]->CameraPerspObject (&pt, &pCenter, fRotZ, fRotX, fRotY, fFOV);
         pv->CameraPosition();
         pv->RenderUpdate(WORLDC_CAMERAMOVED);
      }
      break;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Model view settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
ViewFlatPage
*/
BOOL ViewFlatPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         CPoint pCenter;
         fp fRotZ, fRotX, fRotY, fScale, fTransX, fTransY;
         pv->m_apRender[0]->CameraFlatGet (&pCenter, &fRotZ, &fRotX, &fRotY, &fScale, &fTransX, &fTransY);

         AngleToControl (pPage, L"rotz", fRotZ);
         AngleToControl (pPage, L"rotx", fRotX);
         AngleToControl (pPage, L"roty", fRotY);

         MeasureToString (pPage, L"transx", fTransX);
         MeasureToString (pPage, L"transy", fTransY);

         DoubleToControl (pPage, L"scale", fScale);

         MeasureToString (pPage, L"cx", pCenter.p[0]);
         MeasureToString (pPage, L"cy", pCenter.p[1]);
         MeasureToString (pPage, L"cz", pCenter.p[2]);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // since all the edit controls are controlled, just get all the values
         fp fRotZ, fRotX, fRotY, fScale, fTransX, fTransY;
         CPoint pCenter;
         fRotZ = AngleFromControl (pPage, L"rotz");
         fRotX = AngleFromControl (pPage, L"rotx");
         fRotY = AngleFromControl (pPage, L"roty");

         if (!MeasureParseString (pPage, L"transx", &fTransX))
            fTransX = 0;
         if (!MeasureParseString (pPage, L"transy", &fTransY))
            fTransY = 0;

         if (!MeasureParseString (pPage, L"cx", &pCenter.p[0]))
            pCenter.p[0] = 0;
         if (!MeasureParseString (pPage, L"cy", &pCenter.p[1]))
            pCenter.p[1] = 0;
         if (!MeasureParseString (pPage, L"cz", &pCenter.p[2]))
            pCenter.p[2] = 0;

         fScale = DoubleFromControl (pPage, L"scale");
         fScale = max(0.0001, fScale);

         pCenter.Zero();

         pv->m_apRender[0]->CameraFlat (&pCenter, fRotZ, fRotX, fRotY, fScale, fTransX,fTransY);
         pv->CameraPosition();
         pv->RenderUpdate(WORLDC_CAMERAMOVED);
      }
      break;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Flattened view settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
ViewSetPage
*/
// BUGFIX - Moved lighting date/time to this level so easier to get to.
BOOL ViewSetPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ComboBoxSet (pPage, L"cpsize", RendCPSizeGet());

         MeasureToString (pPage, L"viewscale", pv->m_fViewScale);

         FillStatusColor (pPage, L"monoback", pv->m_apRender[0]->BackgroundGet());
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         BOOL fWantRefresh;
         fWantRefresh = FALSE;

         if (!_wcsicmp(p->pControl->m_pszName, L"changeback")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_apRender[0]->BackgroundGet(), pPage, L"monoback");
            if (cr != pv->m_apRender[0]->BackgroundGet()) {
               DWORD i;
               for (i = 0; i < VIEWTHUMB; i++)
                  pv->m_apRender[i]->BackgroundSet (cr);
               fWantRefresh = TRUE;
            }
         }
         // if want refresh then update
         if (fWantRefresh)
            pv->RenderUpdate(WORLDC_CAMERAMOVED);

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"viewscale")) {
            MeasureParseString (pPage, p->pControl->m_pszName, &pv->m_fViewScale);
            pv->m_fViewScale = max(.01, pv->m_fViewScale);
            pv->ScrollActionInit ();
            pv->UpdateScrollBars();
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"cpsize")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == RendCPSizeGet())
               break;   // same

            RendCPSizeSet (dwVal);
            pv->m_pWorld->NotifySockets (WORLDC_NEEDTOREDRAW, NULL);
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"View settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

static PSTR gpszSaveImageHeight = "SaveImageHeight";
static PSTR gpszSaveImageWidth = "SaveImageWidth";
static PSTR gpszSaveImageAnti = "SaveImageAnti";
static PSTR gpszSaveImageQuality = "SaveImageQuality";
static PSTR gpszSaveImageEffectCode = "SaveImageEffectCode";
static PSTR gpszSaveImageEffectSub = "SaveImageEffectSub";

// PTFP - Information for page
typedef struct {
   PCHouseView       pv;      // view
   HBITMAP           hBitEffect; // effect bitmap
   GUID              gEffectCode;   // major code
   GUID              gEffectSub;    // minor code
} PTFP, *PPTFP;

/****************************************************************************
PrintToFilePage
*/
BOOL PrintToFilePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPTFP pt = (PPTFP)pPage->m_pUserData;
   PCHouseView pv = pt->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"width");
         if (pControl)
            pControl->AttribSetInt (Text(), (int)KeyGet (gpszSaveImageWidth, 1024));
         pControl = pPage->ControlFind (L"height");
         if (pControl)
            pControl->AttribSetInt (Text(), (int)KeyGet (gpszSaveImageHeight, 768));

         ComboBoxSet (pPage, L"anti", max(KeyGet(gpszSaveImageAnti, 1), 1));
         ComboBoxSet (pPage, L"quality", KeyGet(gpszSaveImageQuality, 6));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // allow for change effect
         if (!_wcsicmp(p->pControl->m_pszName, L"changeeffect")) {
            if (EffectSelDialog (DEFAULTRENDERSHARD, pPage->m_pWindow->m_hWnd, &pt->gEffectCode, &pt->gEffectSub)) {
               // new bitmap
               if (pt->hBitEffect)
                  DeleteObject (pt->hBitEffect);
               COLORREF cTrans;
               pt->hBitEffect = EffectGetThumbnail (DEFAULTRENDERSHARD, &pt->gEffectCode, &pt->gEffectSub,
                  pPage->m_pWindow->m_hWnd, &cTrans, TRUE);

               // set the bitmap
               WCHAR szTemp[32];
               swprintf (szTemp, L"%lx", (__int64)pt->hBitEffect);
               PCEscControl pControl = pPage->ControlFind (L"effectbit");
               if (pControl)
                  pControl->AttribSet (L"hbitmap", szTemp);
            }

            return TRUE;
         }

         // only care about save button
         if (_wcsicmp(p->pControl->m_pszName, L"save"))
            break;

         // get the values
         DWORD dwWidth, dwHeight;
         DWORD dwAnti;
         DWORD dwQuality;
         dwWidth = 1024;
         dwHeight = 768;
         dwAnti = 1;
         dwQuality = 5;

         PCEscControl pControl;

         pControl = pPage->ControlFind (L"width");
         if (pControl)
            dwWidth = pControl->AttribGetInt (Text());
         dwWidth = max(1, dwWidth);
         KeySet (gpszSaveImageWidth, dwWidth);

         pControl = pPage->ControlFind (L"height");
         if (pControl)
            dwHeight = pControl->AttribGetInt (Text());
         dwHeight = max(1, dwHeight);
         KeySet (gpszSaveImageHeight, dwHeight);


         pControl = pPage->ControlFind (L"anti");
         if (pControl)
            dwAnti = pControl->AttribGetInt (CurSel()) + 1;
         KeySet(gpszSaveImageAnti, dwAnti);

         pControl = pPage->ControlFind (L"quality");
         if (pControl)
            dwQuality = pControl->AttribGetInt (CurSel());
         KeySet(gpszSaveImageQuality, dwQuality);

         // get name to save as
         OPENFILENAME   ofn;
         char  szTemp[256];
         szTemp[0] = 0;
         memset (&ofn, 0, sizeof(ofn));
         char szInitial[256];
         GetLastDirectory(szInitial, sizeof(szInitial));
         ofn.lpstrInitialDir = szInitial;
         ofn.lStructSize = sizeof(ofn);
         ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
         ofn.hInstance = ghInstance;
         ofn.lpstrFilter = "JPEG file (*.jpg)\0*.jpg\0Bitmap file (*.bmp)\0*.bmp\0\0\0";
         ofn.lpstrFile = szTemp;
         ofn.nMaxFile = sizeof(szTemp);
         ofn.lpstrTitle = "Save image file";
         ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
         ofn.lpstrDefExt = ".jpg";
         // nFileExtension 
         if (!GetSaveFileName(&ofn))
            return TRUE;

         // make image
         HBITMAP hBit;
         pPage->m_pWindow->ShowWindow (SW_HIDE);   // BUGFIX
         hBit = RenderImage (pv->m_apRender[0], pv->m_pWorld, dwQuality,
            dwWidth, dwHeight, dwAnti, FALSE, -1, &pt->gEffectCode, &pt->gEffectSub);
         pPage->m_pWindow->ShowWindow (SW_SHOW);   // BUGFIX
         if (!hBit) {
            pPage->MBWarning (L"The image didn't draw.",
               L"Your computer may not have enough memory to draw an image that large.");
            return TRUE;
         }

         // save
         DWORD dwLen;
         BOOL fRet;
         dwLen = (DWORD)strlen(szTemp);
         if ((dwLen > 4) && !_stricmp(szTemp + (dwLen-4), ".jpg"))
            fRet = BitmapToJPegNoMegaFile (hBit, szTemp);
         else
            fRet = BitmapSave (hBit, szTemp);

         if (!fRet) {
            DeleteObject (hBit);
            pPage->MBWarning (L"The image didn't save.",
               L"You may not have enough disk space or the file may be write protected.");
            return TRUE;
         }

         DeleteObject (hBit);
         pPage->Exit (Back());
         return TRUE;
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Save the image";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"EFFECTBITMAP")) {
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)pt->hBitEffect);

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




static PSTR gpszPrintImageLandscape = "PrintImageLandscape";
static PSTR gpszPrintImageLRMargin = "PrintImageLRMargin";
static PSTR gpszPrintImageTBMargin = "PrintImageTBMargin";
static PSTR gpszPrintImageDetail = "PrintImageDetail";
static PSTR gpszPrintImageOnPaper = "PrintImageOnPaper";
static PSTR gpszPrintImageInModel = "PrintImageInModel";
static PSTR gpszPrintImageToScale = "PrintImageToScale";

/*****************************************************************************
BitmapToDIB - Necessary to print in color?
*/
typedef HANDLE HDIB;
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
 
    // ESCREALLOC the buffer big enough to hold all the bits 
 
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
 

/****************************************************************************
PrintToPrinterPage
*/
BOOL PrintToPrinterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPTFP pt = (PPTFP)pPage->m_pUserData;
   PCHouseView pv = pt->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         BOOL fIso;
         fIso = (pv->m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT);

         BOOL fLandscape;
         fLandscape = (BOOL)KeyGet (gpszPrintImageLandscape, TRUE);
         pControl = pPage->ControlFind (fLandscape ? L"landscape" : L"portrait");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         MeasureToString (pPage, L"lrmargin", (fp)KeyGet(gpszPrintImageLRMargin, 10) / 1000.0);
         MeasureToString (pPage, L"tbmargin", (fp)KeyGet(gpszPrintImageTBMargin, 10) / 1000.0);
         MeasureToString (pPage, L"onpaper", (fp)KeyGet(gpszPrintImageOnPaper, 10) / 1000.0);
         MeasureToString (pPage, L"inmodel", (fp)KeyGet(gpszPrintImageInModel, 1000) / 1000.0);
         if (!fIso) {
            pControl = pPage->ControlFind (L"onpaper");
            if (pControl)
               pControl->Enable (FALSE);
            pControl = pPage->ControlFind (L"inmodel");
            if (pControl)
               pControl->Enable (FALSE);
         }

         pControl = pPage->ControlFind (L"printtoscale");
         if (pControl) {
            pControl->Enable (fIso);
            pControl->AttribSetBOOL (Checked(), (BOOL)KeyGet(gpszPrintImageToScale, (DWORD) FALSE));
         }

         ComboBoxSet (pPage, L"anti", max(KeyGet(gpszSaveImageAnti, 1),1));
         ComboBoxSet (pPage, L"quality", KeyGet(gpszSaveImageQuality, 6));
         ComboBoxSet (pPage, L"detail", KeyGet(gpszPrintImageDetail, 1));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;


         // allow for change effect
         if (!_wcsicmp(p->pControl->m_pszName, L"changeeffect")) {
            if (EffectSelDialog (DEFAULTRENDERSHARD, pPage->m_pWindow->m_hWnd, &pt->gEffectCode, &pt->gEffectSub)) {
               // new bitmap
               if (pt->hBitEffect)
                  DeleteObject (pt->hBitEffect);
               COLORREF cTrans;
               pt->hBitEffect = EffectGetThumbnail (DEFAULTRENDERSHARD, &pt->gEffectCode, &pt->gEffectSub,
                  pPage->m_pWindow->m_hWnd, &cTrans, TRUE);

               // set the bitmap
               WCHAR szTemp[32];
               swprintf (szTemp, L"%lx", (__int64)pt->hBitEffect);
               PCEscControl pControl = pPage->ControlFind (L"effectbit");
               if (pControl)
                  pControl->AttribSet (L"hbitmap", szTemp);
            }

            return TRUE;
         }

         // only care about save button
         if (_wcsicmp(p->pControl->m_pszName, L"print"))
            break;

         // get the values
         DWORD dwAnti;
         DWORD dwQuality, dwDetail;
         BOOL fLandscape;
         BOOL fToScale;
         fp fLR, fTB, fOnPaper, fInModel;
         fLR = fTB = 0.01;
         dwAnti = 1;
         fOnPaper = .01;
         fInModel = 1;
         dwQuality = 5;
         dwDetail = 1;
         fLandscape = TRUE;

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"landscape");
         fLandscape = pControl && pControl->AttribGetBOOL(Checked());
         KeySet (gpszPrintImageLandscape, (DWORD) fLandscape);

         MeasureParseString (pPage, L"lrmargin", &fLR);
         fLR = max(0, fLR);
         KeySet(gpszPrintImageLRMargin, (DWORD)((fLR + .0005) * 1000));
         MeasureParseString (pPage, L"tbmargin", &fTB);
         fTB = max(0, fTB);
         KeySet(gpszPrintImageTBMargin, (DWORD)((fTB + .0005) * 1000));
         MeasureParseString (pPage, L"onpaper", &fOnPaper);
         fOnPaper = max(0.001, fOnPaper);
         KeySet(gpszPrintImageOnPaper, (DWORD)((fOnPaper + .0005) * 1000));
         MeasureParseString (pPage, L"inmodel", &fInModel);
         fInModel = max(0.001, fInModel);
         KeySet(gpszPrintImageInModel, (DWORD)((fInModel + .0005) * 1000));

         pControl = pPage->ControlFind (L"anti");
         if (pControl)
            dwAnti = pControl->AttribGetInt (CurSel()) + 1;
         KeySet(gpszSaveImageAnti, dwAnti);

         pControl = pPage->ControlFind (L"printtoscale");
         if (pControl)
            fToScale = pControl->AttribGetBOOL (Checked());
         KeySet(gpszPrintImageToScale, fToScale);

         pControl = pPage->ControlFind (L"quality");
         if (pControl)
            dwQuality = pControl->AttribGetInt (CurSel());
         KeySet(gpszSaveImageQuality, dwQuality);

         pControl = pPage->ControlFind (L"detail");
         if (pControl)
            dwDetail = pControl->AttribGetInt (CurSel());
         KeySet(gpszPrintImageDetail, dwDetail);


         // dont print to scale if not isometric
         if (pv->m_apRender[0]->CameraModelGet() != CAMERAMODEL_FLAT)
            fToScale = FALSE;

         // open up printer
         // print dialog box
         PRINTDLG pd;
         memset (&pd, 0, sizeof(pd));
         pd.lStructSize = sizeof(pd);
         pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
         pd.hwndOwner = pPage->m_pWindow->m_hWnd;
         pd.nFromPage = 1;
         pd.nToPage = 1;
         pd.nCopies = 1;

         if (!PrintDlg (&pd))
            return TRUE;  // cancelled or error

         HDC hDCPrint;
         hDCPrint = pd.hDC;

         if (!(GetDeviceCaps(pd.hDC, RASTERCAPS) & RC_BITBLT)) {
            pPage->MBWarning (L"Printer cannot display bitmaps.");
            if (hDCPrint)
               DeleteDC (hDCPrint);
            return TRUE;
         }  

         // start the document
         DOCINFO  di;
         memset (&di, 0, sizeof(di));
         di.cbSize = sizeof(di);
         di.lpszDocName = APPLONGNAME; 
         if (StartDoc(hDCPrint, &di) <= 0) {
            pPage->MBWarning (L"StartDoc() call failed.");
            if (hDCPrint)
               DeleteDC (hDCPrint);
            return TRUE;
         }
         SetICMMode (hDCPrint, ICM_ON);

         fp fPixelsPerInchX, fPixelsPerInchY;
         int   iPixX, iPixY;     // pixels across paper
         int   iXOrigin, iYOrigin;
         fPixelsPerInchX = (double) GetDeviceCaps(hDCPrint, LOGPIXELSX);
         fPixelsPerInchY = (double) GetDeviceCaps(hDCPrint, LOGPIXELSY);
         iPixX = GetDeviceCaps (hDCPrint, HORZRES);
         iPixY = GetDeviceCaps (hDCPrint, VERTRES);
         // Use the printable region of the printer to determine 
         // the margins. 
         iXOrigin = GetDeviceCaps(hDCPrint, PHYSICALOFFSETX); 
         iYOrigin = GetDeviceCaps(hDCPrint, PHYSICALOFFSETY); 

         // take into account margins
         iXOrigin = max(iXOrigin, (int) (fLR * INCHESPERMETER * fPixelsPerInchX));
         iXOrigin = min(iXOrigin, iPixX / 2 - 1);  // always at least some size
         iYOrigin = max(iYOrigin, (int) (fTB * INCHESPERMETER * fPixelsPerInchY)); 
         iYOrigin = min(iYOrigin, iPixY / 2 - 1);  // always at least some size

         // figure out which dimension is longer
         DWORD dwLongest;
         dwLongest = ((fp) iPixX / fPixelsPerInchX) > ((fp) iPixY / fPixelsPerInchY) ? 0 : 1;

         // do we need to rotate 90 degrees?
         BOOL fRotate;
         fRotate = FALSE;
         if ( ((dwLongest == 0) && !fLandscape) || ((dwLongest == 1) && fLandscape))
            fRotate = TRUE;

         // how large is the display area
         fp fWidth, fHeight, fIso;
         fWidth = (fp)(iPixX - 2 * iXOrigin) / fPixelsPerInchX / INCHESPERMETER;
         fHeight = (fp)(iPixY - 2 * iYOrigin) / fPixelsPerInchY / INCHESPERMETER;
         if (fRotate) {
            fp fTemp;
            fTemp = fWidth;
            fWidth = fHeight;
            fHeight = fTemp;
         }
         fIso = fWidth / fOnPaper * fInModel;

         // what it sht aspect ratio
         fp fAspect;
         fAspect =  fWidth / fHeight;
         if (fAspect < 1)
            fAspect  = 1.0 / fAspect;  // just so in terms of flongest

         // what resolution
         DWORD dwWidth, dwHeight, dwTemp;
         switch (dwDetail) {
            case 0: // low
               dwWidth = 512;
               break;
            default:
            case 1: // med
               dwWidth = 1024;
               break;
            case 2:  // high detail
               dwWidth = 2048;
               break;
         }
         dwHeight = (DWORD) ((fp)dwWidth / fAspect);
         dwHeight = max(1,dwHeight);
         if (!fLandscape) {
            dwTemp = dwHeight;
            dwHeight = dwWidth;
            dwWidth = dwTemp;
         }

         // make image
         HBITMAP hBit;
         pPage->m_pWindow->ShowWindow (SW_HIDE);   // BUGFIX
         hBit = RenderImage (pv->m_apRender[0], pv->m_pWorld, dwQuality,
            dwWidth, dwHeight, dwAnti, fRotate, fToScale ? fIso : -1,
            &pt->gEffectCode, &pt->gEffectSub);
         pPage->m_pWindow->ShowWindow (SW_SHOW);   // BUGFIX
         if (!hBit) {
            pPage->MBWarning (L"The image didn't draw.",
               L"Your computer may not have enough memory to draw an image that large.");

            // end the document
            EndDoc (hDCPrint);
            DeleteDC (hDCPrint);
            return TRUE;
         }

         // try the dib approach
         HDIB hd;
         hd = BitmapToDIB(hBit);
         DeleteObject (hBit);
         if (!hd) {
            pPage->MBWarning (L"BitmapToDIB() returns NULL.");
            EndDoc (hDCPrint);
            DeleteDC (hDCPrint);
            return TRUE;
         }

         if (StartPage(hDCPrint) <= 0) {
            pPage->MBWarning (L"StartPage() failed.");
            GlobalFree (hd);
            EndDoc (hDCPrint);
            DeleteDC (hDCPrint);
            return TRUE;
         }

         // try dib
         BITMAPINFOHEADER  *pi;
         pi = (BITMAPINFOHEADER*) GlobalLock (hd);
         if (!pi) {
            pPage->MBWarning (L"GlobalLock (hd) returns NULL.");
            GlobalFree (hd);
            EndPage(hDCPrint);
            EndDoc (hDCPrint);
            DeleteDC (hDCPrint);
            return TRUE;
         }
         if (GDI_ERROR == StretchDIBits(  hDCPrint,                // handle to device context
            iXOrigin, iYOrigin, iPixX - 2*iXOrigin, iPixY - 2*iYOrigin,
            0, 0, fRotate ? dwHeight : dwWidth, fRotate ? dwWidth : dwHeight,
            (LPSTR)pi +(WORD)pi->biSize,
            (BITMAPINFO*) pi,
            DIB_RGB_COLORS, 
            SRCCOPY)) {
               pPage->MBWarning (L"StretchDIBits() failed.");
         }

         GlobalUnlock (hd);
         GlobalFree (hd);

         EndPage(hDCPrint);
         EndDoc (hDCPrint);
         DeleteDC (hDCPrint);

         pPage->Exit (Back());
         return TRUE;
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Print the image";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"EFFECTBITMAP")) {
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)pt->hBitEffect);

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
ViewSetGridPage
*/
BOOL ViewSetGridPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         FillStatusColor (pPage, L"majorcolor", pv->m_apRender[0]->m_crGridMajor);
         FillStatusColor (pPage, L"minorcolor", pv->m_apRender[0]->m_crGridMinor);

         // set the checkbox
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"showgrid");
         if (pControl && pv->m_fGridDisplay)
            pControl->AttribSetBOOL (Checked(), TRUE);
         pControl = pPage->ControlFind (L"showgrid2d");
         if (pControl && pv->m_fGridDisplay2D)
            pControl->AttribSetBOOL (Checked(), TRUE);
         pControl = pPage->ControlFind (L"showud");
         if (pControl && pv->m_apRender[0]->m_fGridDrawUD)
            pControl->AttribSetBOOL (Checked(), TRUE);
         pControl = pPage->ControlFind (L"showdots");
         if (pControl && pv->m_apRender[0]->m_fGridDrawDots)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         BOOL fWantRefresh;
         fWantRefresh = FALSE;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"changemajor")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_apRender[0]->m_crGridMajor, pPage, L"majorcolor");
            if (cr != pv->m_apRender[0]->m_crGridMajor) {
               pv->m_apRender[0]->m_crGridMajor = cr;
               fWantRefresh = pv->m_fGridDisplay || pv->m_fGridDisplay2D;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changeminor")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_apRender[0]->m_crGridMinor, pPage, L"minorcolor");
            if (cr != pv->m_apRender[0]->m_crGridMinor) {
               pv->m_apRender[0]->m_crGridMinor = cr;
               fWantRefresh = pv->m_fGridDisplay || pv->m_fGridDisplay2D;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"showgrid")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_fGridDisplay) {
               pv->m_fGridDisplay = fNew;
               fWantRefresh = TRUE;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"showgrid2d")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_fGridDisplay2D) {
               pv->m_fGridDisplay2D = fNew;
               fWantRefresh = TRUE;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"showud")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_apRender[0]->m_fGridDrawUD) {
               pv->m_apRender[0]->m_fGridDrawUD = fNew;
               fWantRefresh = pv->m_fGridDisplay | pv->m_fGridDisplay2D;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"showdots")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_apRender[0]->m_fGridDrawDots) {
               pv->m_apRender[0]->m_fGridDrawDots = fNew;
               fWantRefresh = pv->m_fGridDisplay | pv->m_fGridDisplay2D;
            }
         }

         // if want refresh then update
         if (fWantRefresh)
            pv->RenderUpdate(WORLDC_CAMERAMOVED);


      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Grid display";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
ViewSetLightingPage
*/
BOOL ViewSetLightingPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         // light intensities
         pControl = pPage->ControlFind (L"ambientintensity");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_apRender[0]->AmbientExtraGet()));
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         DWORD  dwVal;
         if (!_wcsicmp(p->pControl->m_pszName, L"ambientintensity")) {
            dwVal = (DWORD) p->pControl->AttribGetInt (Pos());
            if (dwVal != pv->m_apRender[0]->AmbientExtraGet()) {
               pv->m_apRender[0]->AmbientExtraSet ( dwVal);
               pv->RenderUpdate(WORLDC_CAMERAMOVED);
            }
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Lighting";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}
/****************************************************************************
ViewSetMeasurementPage
*/
BOOL ViewSetMeasurementPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the radio button
         PWSTR pszName;
         PCEscControl pControl;
         switch (MeasureDefaultUnits()) {
         default:
         case MUNIT_METRIC_METERS:
            pszName = L"m";
            break;
         case MUNIT_METRIC_CENTIMETERS:
            pszName = L"cm";
            break;
         case MUNIT_METRIC_MILLIMETERS:
            pszName = L"mm";
            break;
         case MUNIT_ENGLISH_FEET:
            pszName = L"ft";
            break;
         case MUNIT_ENGLISH_INCHES:
            pszName = L"in";
            break;
         case MUNIT_ENGLISH_FEETINCHES:
            pszName = L"ftin";
            break;
         }
         pControl = pPage->ControlFind (pszName);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // measurement
         if (!_wcsicmp(p->pControl->m_pszName, L"m")) {
            MeasureDefaultUnitsSet (MUNIT_METRIC_METERS);
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"cm")) {
            MeasureDefaultUnitsSet (MUNIT_METRIC_CENTIMETERS);
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"mm")) {
            MeasureDefaultUnitsSet (MUNIT_METRIC_MILLIMETERS);
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"ft")) {
            MeasureDefaultUnitsSet (MUNIT_ENGLISH_FEET);
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"in")) {
            MeasureDefaultUnitsSet (MUNIT_ENGLISH_INCHES);
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"ftin")) {
            MeasureDefaultUnitsSet (MUNIT_ENGLISH_FEETINCHES);
         }


      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Measurement";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
ViewSetPerspectivePage
*/
BOOL ViewSetPerspectivePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // get the field of view
         CPoint p;
         fp fLat, fLong, fTilt, fFOV;
         pv->m_apRender[0]->CameraPerspWalkthroughGet (&p, &fLong, &fLat, &fTilt, &fFOV);

         // latitude
         AngleToControl (pPage, L"fov", fFOV);

         // typical monitor is .333m
         fp fWidth;
         fWidth = 0.333;
         MeasureToString (pPage, L"width", fWidth);

         // distance is
         MeasureToString (pPage, L"distance", 0.5 * fWidth / tan(max(fFOV,1) * 0.5));

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // see if want to refresh
         fp fFOV, fWidth, fDistance;
         fFOV = 30;
         fWidth = fDistance = 1;

         // get FOV
         fFOV = AngleFromControl (pPage, L"fov");

         // get the width and distance
         if (!MeasureParseString (pPage, L"width", &fWidth))
            fWidth = 1;
         if (!MeasureParseString (pPage, L"distance", &fDistance))
            fDistance = 1;
         fWidth = max(.01, fWidth);
         fDistance = max(.01, fDistance);

         if (!_wcsicmp(p->pControl->m_pszName, L"fov")) {
            // changed fov. Therefore, change distance measurement
            MeasureToString (pPage, L"distance",
               fDistance = 0.5 * fWidth / tan(max(fFOV,.0001) * 0.5));
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"width") || !_wcsicmp(p->pControl->m_pszName, L"distance")) {
            // change FOV
            fFOV = 2.0 * atan(fWidth / fDistance / 2.0);
            AngleToControl (pPage, L"fov", fFOV);
         }

         // tell the renderer
         switch (pv->m_apRender[0]->CameraModelGet()) {
         case CAMERAMODEL_PERSPWALKTHROUGH:
            {
               CPoint p;
               fp fLong, fLat, fTilt, f2;
               pv->m_apRender[0]->CameraPerspWalkthroughGet (&p, &fLong, &fLat, &fTilt, &f2);
               pv->m_apRender[0]->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, fFOV);
               pv->CameraPosition();
               pv->RenderUpdate(WORLDC_CAMERAMOVED);
            }
            break;
         case CAMERAMODEL_PERSPOBJECT:
            {
               CPoint p, pCenter;
               fp fZ, fX, fY, f2;
               pv->m_apRender[0]->CameraPerspObjectGet (&p, &pCenter, &fZ, &fX, &fY, &f2);
               pv->m_apRender[0]->CameraPerspObject (&p, &pCenter, fZ, fX, fY, fFOV);
               pv->CameraPosition();
               pv->RenderUpdate(WORLDC_CAMERAMOVED);
            }
            break;
         default:
            // do nothing
            break;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Perspective";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

#if 0 // no more RB glasses
/****************************************************************************
ViewSetRBGlassesPage
*/
BOOL ViewSetRBGlassesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the checkbox
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"threedon");
         if (pControl && pv->m_apRender[0]->m_fRBGlasses)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // combo box
         pControl = pPage->ControlFind (L"eye");
         if (pControl)
            pControl->AttribSetInt (CurSel(), pv->m_apRender[0]->m_fRBRightEyeDominant ? 1 : 0);

         // distance
         MeasureToString (pPage, L"distance", pv->m_apRender[0]->m_fRBEyeDistance);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // see if want to refresh
         BOOL fOK;
         fp fVal;

         if (!_wcsicmp(p->pControl->m_pszName, L"distance")) {
            fOK = MeasureParseString (pPage, p->pControl->m_pszName, &fVal);
            if (fOK && (fVal != pv->m_apRender[0]->m_fRBEyeDistance)) {
               pv->m_apRender[0]->m_fRBEyeDistance = fVal;
               if (pv->m_apRender[0]->m_fRBGlasses)
                  pv->RenderUpdate(WORLDC_CAMERAMOVED);
            }
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         BOOL fWantRefresh;
         fWantRefresh = FALSE;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"threedon")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_apRender[0]->m_fRBGlasses) {
               pv->m_apRender[0]->m_fRBGlasses = fNew;
               fWantRefresh = TRUE;
            }
         };

         // if want refresh then update
         if (fWantRefresh)
            pv->RenderUpdate(WORLDC_CAMERAMOVED);


      }
      break;   // default

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (!_wcsicmp(p->pControl->m_pszName, L"eye")) {
            BOOL fVal;
            fVal = p->pszName && (p->pszName[0] == L'1');
            if (fVal != pv->m_apRender[0]->m_fRBRightEyeDominant) {
               pv->m_apRender[0]->m_fRBRightEyeDominant = fVal;
               if (pv->m_apRender[0]->m_fRBGlasses)
                  pv->RenderUpdate(WORLDC_CAMERAMOVED);
            }
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Red/blue colored glasses";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}
#endif // 0 no more RB glasses

/****************************************************************************
QualityPage
*/
BOOL QualityPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;
   PQUALITYINFO pq = &gQualityInfo[pv->m_dwQuality];

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         //backface
         pControl = pPage->ControlFind (L"backface");
         if (pControl && pq->fBackfaceCull)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // dessired detail
         pControl = pPage->ControlFind (L"detail");
         if (pControl)
            pControl->AttribSetInt (Text(), (int) pq->fDetailPixels);

         // set the radio button
         PWSTR pszName;
         switch (pq->dwRenderModel & 0xf0) {
         case 0x00:  // wireframe
            pszName = L"wire";
            break;
         default:
         case 0x10:  // solid
            pszName = L"soliddraw";
            break;
         }
         pControl = pPage->ControlFind (pszName);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
         FillStatusColor (pPage, L"monocolor", pq->cMono);

         // set the radio button
         switch (pq->dwRenderModel & 0x0f) {
         case RM_MONO:  // mono
            pszName = L"mono";
            break;
         case RM_AMBIENT:  // solid
            pszName = L"solid";
            break;
         case RM_TEXTURE:
            pszName = L"texture";
            break;
         case RM_SHADOWS:
            pszName = L"shadow";
            break;
         default:
         case RM_SHADED:  // shaded
            pszName = L"shaded";
         }
         pControl = pPage->ControlFind (pszName);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // see if want to refresh
         fp fVal;

         if (!_wcsicmp(p->pControl->m_pszName, L"detail")) {
            fVal = p->pControl->AttribGetInt (Text());
            fVal = max(1,fVal);
            if (fVal != pq->fDetailPixels) {
               pq->fDetailPixels = fVal;
               pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
            }
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         BOOL fWantRefresh;
         fWantRefresh = FALSE;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"backface")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pq->fBackfaceCull) {
               pq->fBackfaceCull = fNew;
               fWantRefresh = TRUE;
            }
         };

         DWORD dwRenderModel;
         dwRenderModel = pq->dwRenderModel;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"wire")) {
            dwRenderModel = (dwRenderModel & (~0xf0)) | 0;
            if (dwRenderModel != pq->dwRenderModel) {
               pq->dwRenderModel = dwRenderModel;
               fWantRefresh = TRUE;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"soliddraw")) {
            dwRenderModel = (dwRenderModel & (~0xf0)) | 0x10;
            if (dwRenderModel != pq->dwRenderModel) {
               pq->dwRenderModel = dwRenderModel;
               fWantRefresh = TRUE;
            }
         }

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"changemono")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pq->cMono, pPage, L"monocolor");
            if (cr != pq->cMono) {
               pq->cMono = cr;
               fWantRefresh = TRUE;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"mono")) {
            dwRenderModel = (dwRenderModel & (~0x0f)) | RM_MONO;
            if (dwRenderModel != pq->dwRenderModel) {
               pq->dwRenderModel = dwRenderModel;
               fWantRefresh = TRUE;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"solid")) {
            dwRenderModel = (dwRenderModel & (~0x0f)) | RM_AMBIENT;
            if (dwRenderModel != pq->dwRenderModel) {
               pq->dwRenderModel = dwRenderModel;
               fWantRefresh = TRUE;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"shaded")) {
            dwRenderModel = (dwRenderModel & (~0x0f)) | RM_SHADED;
            if (dwRenderModel != pq->dwRenderModel) {
               pq->dwRenderModel = dwRenderModel;
               fWantRefresh = TRUE;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"texture")) {
            dwRenderModel = (dwRenderModel & (~0x0f)) | RM_TEXTURE;
            if (dwRenderModel != pq->dwRenderModel) {
               pq->dwRenderModel = dwRenderModel;
               fWantRefresh = TRUE;
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"shadow")) {
            dwRenderModel = (dwRenderModel & (~0x0f)) | RM_SHADOWS;
            if (dwRenderModel != pq->dwRenderModel) {
               pq->dwRenderModel = dwRenderModel;
               fWantRefresh = TRUE;
            }
         }

         // if want refresh then update
         if (fWantRefresh)
            pv->RenderUpdate(WORLDC_NEEDTOREDRAW);


      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Drawing quality";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
ViewSettings - This brings up the UI for view settings. As parameters are changed
the view object will be notified and told to refresh.

inputs
   PCHouseView    pView - view to use
returns
   none
*/
void ViewSettings (PCHouseView pView)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (pView->m_hWnd, &r);

   cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

firstpage:
   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLVIEWSET, ViewSetPage, pView);
   if (!pszRet)
      return;
   if (!_wcsicmp(pszRet, L"fog")) {
      BOOL fRet = FALSE;
      if (pView->m_apRender[0]->m_pEffectFog) {
         fRet = pView->m_apRender[0]->m_pEffectFog->Dialog (&cWindow,
            NULL, &pView->m_aImage[0], pView->m_apRender[0], pView->m_pWorld);

         // If change want to update render
         pView->RenderUpdate(WORLDC_CAMERAMOVED);
      }


      if (fRet)
         goto firstpage;

      return;
   }
   else if (!_wcsicmp(pszRet, L"lighting")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLVIEWSETLIGHTING, ViewSetLightingPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else if (!_wcsicmp(pszRet, L"outline")) {
      BOOL fRet = FALSE;
      if (pView->m_apRender[0]->m_pEffectOutline) {
         fRet = pView->m_apRender[0]->m_pEffectOutline->Dialog (&cWindow,
            NULL, &pView->m_aImage[0], pView->m_apRender[0], pView->m_pWorld);

         // If change want to update render
         pView->RenderUpdate(WORLDC_CAMERAMOVED);
      }


      if (fRet)
         goto firstpage;

      return;
   }
   else if (!_wcsicmp(pszRet, L"perspective")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLVIEWSETPERSPECTIVE, ViewSetPerspectivePage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
#if 0 // no more rb glasses
   else if (!_wcsicmp(pszRet, L"rbglasses")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLVIEWSETRBGLASSES, ViewSetRBGlassesPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
#endif // 0 no more rb glasses
   else  // probably pressed back
      return;  // exit
}




/****************************************************************************
GridSetPage
*/
BOOL GridSetPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Grid settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
GridValuesPage
*/
BOOL GridValuesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      MeasureToString (pPage, L"grid", pv->m_fGrid);
      MeasureToString (pPage, L"transx", pv->m_pGridCenter.p[0]);
      MeasureToString (pPage, L"transy", pv->m_pGridCenter.p[1]);
      MeasureToString (pPage, L"transz", pv->m_pGridCenter.p[2]);
      AngleToControl (pPage, L"gridangle", pv->m_fGridAngle);
      AngleToControl (pPage, L"rotx", pv->m_pGridRotation.p[0],TRUE);
      AngleToControl (pPage, L"roty", pv->m_pGridRotation.p[1],TRUE);
      AngleToControl (pPage, L"rotz", pv->m_pGridRotation.p[2],TRUE);
      break;

   case ESCN_EDITCHANGE:
      {
         // since if any edit box changes we need to recalc basically everything,
         // just get them all

         if (!MeasureParseString (pPage, L"grid", &pv->m_fGrid))
            pv->m_fGrid = 0;
         if (!MeasureParseString (pPage, L"transx", &pv->m_pGridCenter.p[0]))
            pv->m_pGridCenter.p[0] = 0;
         if (!MeasureParseString (pPage, L"transy", &pv->m_pGridCenter.p[1]))
            pv->m_pGridCenter.p[1] = 0;
         if (!MeasureParseString (pPage, L"transz", &pv->m_pGridCenter.p[2]))
            pv->m_pGridCenter.p[2] = 0;

         pv->m_fGridAngle = AngleFromControl (pPage, L"gridangle");
         pv->m_pGridRotation.p[0] = AngleFromControl (pPage, L"rotx");
         pv->m_pGridRotation.p[1] = AngleFromControl (pPage, L"roty");
         pv->m_pGridRotation.p[2] = AngleFromControl (pPage, L"rotz");

         // new matrix
         pv->m_mGridToWorld.FromXYZLLT (&pv->m_pGridCenter,
            pv->m_pGridRotation.p[2], pv->m_pGridRotation.p[0], pv->m_pGridRotation.p[1]);
         pv->m_mGridToWorld.Invert4 (&pv->m_mWorldToGrid);

         // because this may have caused the other grid to need redrawing, re-render
         pv->RenderUpdate(WORLDC_CAMERAMOVED);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Grid values";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
GridSettings - This brings up the UI for view settings. As parameters are changed
the view object will be notified and told to refresh.

inputs
   PCHouseView    pView - view to use
returns
   none
*/
void GridSettings (PCHouseView pView)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (pView->m_hWnd, &r);

   cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

firstpage:
   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGRIDSET, GridSetPage, pView);
   if (!pszRet)
      return;
   if (!_wcsicmp(pszRet, L"grid")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLVIEWSETGRID, ViewSetGridPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else if (!_wcsicmp(pszRet, L"values")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGRIDVALUES, GridValuesPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else  // probably pressed back
      return;  // exit
}

/****************************************************************************
QualitySettings - This brings up the UI for quality settings. As parameters are changed
the view object will be notified and told to refresh.

inputs
   PCHouseView    pView - view to use
returns
   none
*/
void QualitySettings (PCHouseView pView)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (pView->m_hWnd, &r);

   cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLQUALITY, QualityPage, pView);

   DWORD i;
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (!hKey)
      return;

   char szTemp[64];
   for (i = 0; i < NUMQUALITY; i++) {
      sprintf (szTemp, gpszQualityKey, (int) i);
      RegSetValueEx (hKey, szTemp, 0, REG_BINARY, (BYTE*) &gQualityInfo[i], sizeof(gQualityInfo[i]));
   }
   RegCloseKey (hKey);

}

/****************************************************************************
LocaleSetPage
*/
BOOL LocaleSetPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Location settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
AppSetPage
*/
BOOL AppSetPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
#if 0 // dead code - no longer have option
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"compress");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), MMLCompressMaxGet());
#endif // 0
      }
      break;

#if 0 // dead code - no longer have option
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"compress")) {
            MMLCompressMaxSet ( p->pControl->AttribGetBOOL (Checked()));
            return TRUE;
         }
      }
      break;
#endif // 0

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
LocaleInfoPage
*/
BOOL LocaleInfoPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         double fLatitude;
         PWSTR psz;
         psz = pv->m_pWorld->VariableGet (WSLatitude());
         if (!psz || !AttribToDouble(psz, &fLatitude))
            fLatitude = 0;
         AngleToControl (pPage, L"latitude", fLatitude, TRUE);

         // climate
         PCEscControl pControl;
         DWORD dwClimate;
         psz = pv->m_pWorld->VariableGet (WSClimate());
         dwClimate = 0;
         if (psz)
            dwClimate = (DWORD) _wtoi(psz);
         else
            DefaultBuildingSettings (NULL, NULL, NULL, &dwClimate);  // BUGFIX - Def settings
         pControl = pPage->ControlFind (L"climate");
         WCHAR szTemp[32];
         swprintf (szTemp, L"%d", (int) dwClimate);
         ESCMLISTBOXSELECTSTRING lss;
         memset (&lss, 0, sizeof(lss));
         lss.fExact = TRUE;
         lss.iStart = -1;
         lss.psz = szTemp;
         if (pControl)
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &lss);

         DWORD dwRoofing;
         psz = pv->m_pWorld->VariableGet (WSRoofing());
         dwRoofing = 0;
         if (psz)
            dwRoofing = (DWORD) _wtoi(psz);
         else
            DefaultBuildingSettings (NULL, NULL, NULL, NULL, &dwRoofing);  // BUGFIX - Default building
         pControl = pPage->ControlFind (L"roofmaterial");
         swprintf (szTemp, L"%d", (int) dwRoofing);
         ESCMCOMBOBOXSELECTSTRING css;
         memset (&css, 0, sizeof(css));
         css.fExact = TRUE;
         css.iStart = -1;
         css.psz = szTemp;
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);

         DWORD dwExternalWalls;
         psz = pv->m_pWorld->VariableGet (WSExternalWalls());
         dwExternalWalls = 0;
         if (psz)
            dwExternalWalls = (DWORD) _wtoi(psz);
         else
            DefaultBuildingSettings (NULL, NULL, NULL, NULL, NULL, &dwExternalWalls);  // BUGFIX - extenral
         pControl = pPage->ControlFind (L"externalwalls");
         swprintf (szTemp, L"%d", (int) dwExternalWalls);
         memset (&css, 0, sizeof(css));
         css.fExact = TRUE;
         css.iStart = -1;
         css.psz = szTemp;
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);

         DWORD dwFoundation;
         psz = pv->m_pWorld->VariableGet (WSFoundation());
         dwFoundation = 1;
         if (psz)
            dwFoundation = (DWORD) _wtoi(psz);
         else
            DefaultBuildingSettings (NULL, NULL, NULL, NULL, NULL, NULL, &dwFoundation);  // BUGFIX - extenral
         pControl = pPage->ControlFind (L"foundation");
         swprintf (szTemp, L"%d", (int) dwFoundation);
         memset (&css, 0, sizeof(css));
         css.fExact = TRUE;
         css.iStart = -1;
         css.psz = szTemp;
         if (pControl)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &css);
      }
      break;


   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (!_wcsicmp(p->pControl->m_pszName, L"climate")) {
            PWSTR psz;
            DWORD dwClimate;
            psz = pv->m_pWorld->VariableGet (WSClimate());
            dwClimate = 0;
            if (psz)
               dwClimate = (DWORD) _wtoi(psz);
            else
               DefaultBuildingSettings (NULL, NULL, NULL, &dwClimate);  // BUGFIX - Default building settings

            DWORD dwVal;
            dwVal = _wtoi(p->pszName);
            if (dwVal == dwClimate)
               return TRUE;   // no change

            // set the new value
            WCHAR szTemp[16];
            swprintf (szTemp, L"%d", (int) dwVal);
            pv->m_pWorld->VariableSet (WSClimate(), szTemp);

            // adjust elevations
            pPage->Message (ESCM_USER+86);
            return TRUE;
         }
      }
      break;



   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (!_wcsicmp(p->pControl->m_pszName, L"roofmaterial")) {
            PWSTR psz;
            DWORD dwRoofing;
            psz = pv->m_pWorld->VariableGet (WSRoofing());
            dwRoofing = 0;
            if (psz)
               dwRoofing = (DWORD) _wtoi(psz);
            else
               DefaultBuildingSettings (NULL, NULL, NULL, NULL, &dwRoofing);  // BUGFIX

            DWORD dwVal;
            dwVal = _wtoi(p->pszName);
            if (dwVal == dwRoofing)
               return TRUE;   // no change

            // set the new value
            WCHAR szTemp[16];
            swprintf (szTemp, L"%d", (int) dwVal);
            pv->m_pWorld->VariableSet (WSRoofing(), szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"foundation")) {
            PWSTR psz;
            DWORD dwFoundation;
            psz = pv->m_pWorld->VariableGet (WSFoundation());
            dwFoundation = 1;
            if (psz)
               dwFoundation = (DWORD) _wtoi(psz);
            else
               DefaultBuildingSettings (NULL, NULL, NULL, NULL, NULL, NULL, &dwFoundation);  // BUGFIX - extenral

            DWORD dwVal;
            dwVal = _wtoi(p->pszName);
            if (dwVal == dwFoundation)
               return TRUE;   // no change

            // set the new value
            WCHAR szTemp[16];
            swprintf (szTemp, L"%d", (int) dwVal);
            pv->m_pWorld->VariableSet (WSFoundation(), szTemp);

            // if the foundation has changed from pad to something else, need to deal with this
            if ( ((dwFoundation == 2) && (dwVal != 2)) || ((dwFoundation != 2) && (dwVal == 2)))
               pPage->Message (ESCM_USER+86);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"ExternalWalls")) {
            PWSTR psz;
            DWORD dwExternalWalls;
            psz = pv->m_pWorld->VariableGet (WSExternalWalls());
            dwExternalWalls = 0;
            if (psz)
               dwExternalWalls = (DWORD) _wtoi(psz);
            else
               DefaultBuildingSettings (NULL, NULL, NULL, NULL, NULL, &dwExternalWalls);  // BUGFIX - extenral

            DWORD dwVal;
            dwVal = _wtoi(p->pszName);
            if (dwVal == dwExternalWalls)
               return TRUE;   // no change

            // set the new value
            WCHAR szTemp[16];
            swprintf (szTemp, L"%d", (int) dwVal);
            pv->m_pWorld->VariableSet (WSExternalWalls(), szTemp);
            return TRUE;
         }
      }
      break;

   case ESCM_USER+86:   // asjust elevations
      {
         // floor height
         DWORD dwClimate;
         PWSTR psz;
         psz = pv->m_pWorld->VariableGet (WSClimate());
         dwClimate = 0;
         if (psz)
            dwClimate = (DWORD) _wtoi(psz);
         else
            DefaultBuildingSettings (NULL, NULL, NULL, &dwClimate);
         fp fFloorHeight;
         switch (dwClimate) {
         case 0:  // tropical
            fFloorHeight = FLOORHEIGHT_TROPICAL;
            break;
         case 1:  // sub-tropical
         case 5:  // arid
         case 6:  // mediterraneous
            fFloorHeight = (FLOORHEIGHT_TEMPERATE + FLOORHEIGHT_TROPICAL) / 2;
            break;
         default:
            fFloorHeight = FLOORHEIGHT_TEMPERATE;
            break;
         }

         // ground floor is usually at 1m, except when pad, then .1m
         DWORD dwFoundation;
         psz = pv->m_pWorld->VariableGet (WSFoundation());
         dwFoundation = 1;
         if (psz)
            dwFoundation = (DWORD) _wtoi(psz);
         else
            DefaultBuildingSettings (NULL, NULL, NULL, NULL, NULL, NULL, &dwFoundation);  // BUGFIX - extenral
         fp fGround;
         fGround = (dwFoundation == 2) ? .1 : 1;

         // make the levels
         fp afLevels[NUMLEVELS];
         afLevels[0] = fGround - fFloorHeight;
         DWORD i;
         for (i = 1; i < NUMLEVELS; i++)
            afLevels[i] = afLevels[i-1] + fFloorHeight;

         // set it out
         GlobalFloorLevelsSet (pv->m_pWorld, NULL, afLevels, fFloorHeight);

      }
      return TRUE;

   case ESCM_USER + 108:   // send OSM_NEWLATITUDE to every object
      {
         DWORD i;
         for (i = 0; i < pv->m_pWorld->ObjectNum(); i++) {
            PCObjectSocket pos = pv->m_pWorld->ObjectGet(i);
            if (pos)
               pos->Message (OSM_NEWLATITUDE, NULL);
         }
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"latitude")) {
            fp fLat;
            double fLatitude;
            PWSTR psz;
            psz = pv->m_pWorld->VariableGet (WSLatitude());
            if (!psz || !AttribToDouble(psz, &fLatitude))
               fLatitude = 0;

            fLat = AngleFromControl (pPage, p->pControl->m_pszName);
            if (fabs(fLat - fLatitude) > EPSILON) {
               WCHAR szTemp[32];
               swprintf (szTemp, L"%g", (double)fLat);
               pv->m_pWorld->VariableSet (WSLatitude(), szTemp);
               pv->m_pWorld->NotifySockets (WORLDC_LIGHTCHANGED, NULL);
            }

            // will need to sent OSM_NEWLATITUDE
            pPage->Message (ESCM_USER + 108);
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Building site information";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* LocaleLevelsPage
*/
BOOL LocaleLevelsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         fp afLevels[NUMLEVELS], fHigher;
         GlobalFloorLevelsGet (pv->m_pWorld, NULL,
            afLevels, &fHigher);

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < NUMLEVELS; i++) {
            swprintf (szTemp, L"level%d", (int) i);
            MeasureToString (pPage, szTemp, afLevels[i]);
         }

         MeasureToString (pPage, L"upperlevels", fHigher);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"commit")) {
            // get the values
            DWORD i;
            WCHAR szTemp[64];
            fp afLevels[NUMLEVELS], fHigher;
            for (i = 0; i < NUMLEVELS; i++) {
               swprintf (szTemp, L"level%d", (int) i);
               MeasureParseString (pPage, szTemp, &afLevels[i]);
            }

            MeasureParseString (pPage, L"upperlevels", &fHigher);
            fHigher = max(fHigher, .1);
            GlobalFloorLevelsSet (pv->m_pWorld, NULL, afLevels, fHigher);

            pPage->Exit (Back());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Levels/floors";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/* ObjEdiorPage
*/
BOOL ObjEditorPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PWSTR psz;
         DWORD dw;
         PCEscControl pControl;

         // location
         psz = pv->m_pWorld->VariableGet (WSObjLocation());
         dw = psz ? (DWORD)_wtoi(psz) : 0;
         if (dw == 1)
            psz = L"wall";
         else if (dw == 2)
            psz = L"ceiling";
         else
            psz = L"ground";
         pControl = pPage->ControlFind (psz);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // embed
         WCHAR szTemp[32];
         psz = pv->m_pWorld->VariableGet (WSObjEmbed());
         dw = psz ? (DWORD)_wtoi(psz) : 0;
         swprintf (szTemp, L"embed%d", (int) dw);
         pControl = pPage->ControlFind (szTemp);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // show plane
         psz = pv->m_pWorld->VariableGet (WSObjShowPlain());
         dw = psz ? (DWORD)_wtoi(psz) : 1;
         pControl = pPage->ControlFind (L"drawplane");
         if (dw && pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // type of object
         psz = pv->m_pWorld->VariableGet (WSObjType());
         dw = psz ? (DWORD)_wtoi(psz) : RENDERSHOW_FURNITURE;
         ComboBoxSet (pPage, L"objtype", dw);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"objtype")) {
            DWORD dw;
            psz = pv->m_pWorld->VariableGet (WSObjType());
            dw = psz ? (DWORD)_wtoi(psz) : RENDERSHOW_FURNITURE;

            if (dw != (DWORD)_wtoi(p->pszName))
               pv->m_pWorld->VariableSet (WSObjType(), p->pszName);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszEmbed = L"embed";
         DWORD dwEmbed = (DWORD)wcslen(pszEmbed);
         if (!wcsncmp (p->pControl->m_pszName, pszEmbed, dwEmbed)) {
            pv->m_pWorld->VariableSet (WSObjEmbed(), p->pControl->m_pszName + dwEmbed);
            pv->m_pWorld->NotifySockets (WORLDC_NEEDTOREDRAW, NULL);
            return TRUE;
         }

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"ground")) {
            pv->m_pWorld->VariableSet (WSObjLocation(), L"0");
            pv->m_pWorld->NotifySockets (WORLDC_NEEDTOREDRAW, NULL);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"wall")) {
            pv->m_pWorld->VariableSet (WSObjLocation(), L"1");
            pv->m_pWorld->NotifySockets (WORLDC_NEEDTOREDRAW, NULL);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"ceiling")) {
            pv->m_pWorld->VariableSet (WSObjLocation(), L"2");
            pv->m_pWorld->NotifySockets (WORLDC_NEEDTOREDRAW, NULL);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"drawplane")) {
            pv->m_pWorld->VariableSet (WSObjShowPlain(),
               p->pControl->AttribGetBOOL (Checked()) ? L"1" : L"0");
            pv->m_pWorld->NotifySockets (WORLDC_NEEDTOREDRAW, NULL);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Location and embedding";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* ObjEdiorSetPage
*/
BOOL ObjEditorSetPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Object settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
IsAttribNameUnique - Makes sure the attribute name is unique.

inputs
   PWSTR    pszName - name to check
   PCListFixed plCOEAttrib - List of attributes
   DWORD    dwExclude - Don't look at this index into plCOEAttrib when comparing names.
                  Use -1 to ignore
returns
   BOOl - TRUE if unique
*/
BOOL IsAttribNameUnique (PWSTR pszName, PCListFixed plCOEAttrib,
                         DWORD dwExclude)
{
   if (!pszName[0])
      return FALSE;
   if (IsTemplateAttribute (pszName, FALSE)) // cant be from template, exclude open/close on/off
      return FALSE;

   PCOEAttrib *pap;
   DWORD dwNum, i;
   dwNum = plCOEAttrib->Num();
   pap = (PCOEAttrib*) plCOEAttrib->Get (0);
   for (i = 0; i < dwNum; i++) {
      if (i == dwExclude)
         continue;

      // dont care about unique name with delete
      if (pap[i]->m_dwType == 1)
         continue;

      if (!_wcsicmp(pszName, pap[i]->m_szName))
         return FALSE;
   }

   return TRUE;
}

/****************************************************************************
MakeAttribNameUnique - Makes sure an attribute (or morh) name is unique.

inputs
   PWSTR    pszName - name to check. Must be 64 chars long buffer, string no more than 64 chars.
   PCListFixed plCOEAttrib - List of attributes
   DWORD    dwExclude - Don't look at this index into plCOEAttrib when comparing names.
                  Use -1 to ignore
returns
   none
*/
void MakeAttribNameUnique (PWSTR pszName, PCListFixed plCOEAttrib, DWORD dwExclude)
{
   // make sure the name is unique
   WCHAR szTemp[64];
   DWORD i;
   if (!IsAttribNameUnique (pszName, plCOEAttrib, dwExclude)) {
      for (i = 0; ; i++) {
         // remove last number and space
         wcscpy (szTemp, pszName);
         szTemp[58] = 0;   // so have enough space for digits
         while (wcslen(szTemp) && (iswdigit (szTemp[wcslen(szTemp)-1]) ||
            (szTemp[wcslen(szTemp)-1] == L' ')))
            szTemp[wcslen(szTemp)-1] = 0;

         // write new one
         swprintf (szTemp + wcslen(szTemp), L" %d", (int)i+1);

         if (IsAttribNameUnique (szTemp, plCOEAttrib, dwExclude))
            break;
      }
      wcscpy (pszName, szTemp);
   }
}


/* ObjEdiorAttribPage
*/
typedef struct {
   PCHouseView    pv;
   PCListFixed    plCOEAttrib;
   GUID           gObject;          // filled in by ObjEditorAttribSel with the object selected
   WCHAR          szAttrib[64];       // filled in by ObjEditorAttribSel with the attribute selected
   DWORD          dwEdit;           // filled in to indicate which one is being edited
   int            iVScroll;         // so ObjEditorAttribEditPage retursn to the right place
} OEAPAGE, *POEAPAGE;

BOOL ObjEditorAttribPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POEAPAGE po = (POEAPAGE)pPage->m_pUserData;
   PCHouseView pv = po->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the list box
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCM_USER+82:
      {
         // fill in the list box
         MemZero (&gMemTemp);
         DWORD i, dwNum;
         PCOEAttrib *pap;
         dwNum = po->plCOEAttrib->Num();
         pap = (PCOEAttrib*) po->plCOEAttrib->Get(0);
         for (i = 0; i < dwNum; i++) {
            if (pap[i]->m_dwType != 0)
               continue;   // dont show remove types

            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int) i);
            MemCat (&gMemTemp, L">");

            // name
            MemCat (&gMemTemp, L"<bold>");
            MemCatSanitize (&gMemTemp, pap[i]->m_szName);
            MemCat (&gMemTemp, L"</bold>");
            if (pap[i]->m_fAutomatic)
               MemCat (&gMemTemp, L" <italic>(Automatic)</italic>");
            MemCat (&gMemTemp, L"<br/>");

            // description and other bits
            MemCat (&gMemTemp, L"<p parindent=32 wrapindent=32>");
            if ((pap[i]->m_memInfo.p) && ((PWSTR)pap[i]->m_memInfo.p)[0]) {
               MemCatSanitize (&gMemTemp, (PWSTR)pap[i]->m_memInfo.p);
               MemCat (&gMemTemp, L"<p/>");
            }

            // what attributes it effects
            DWORD dwElem;
            PCOEATTRIBCOMBO pc;
            DWORD j;
            pc = (PCOEATTRIBCOMBO) pap[i]->m_lCOEATTRIBCOMBO.Get(0);
            dwElem = pap[i]->m_lCOEATTRIBCOMBO.Num();
            for (j = 0; j < dwElem; j++, pc++) {
               // get the object name
               DWORD dwFind;
               PWSTR pszName;
               PCObjectSocket pos = NULL;
               dwFind = pv->m_pWorld->ObjectFind (&pc->gFromObject);
               if (dwFind != -1)
                  pos = pv->m_pWorld->ObjectGet (dwFind);
               if (!pos)
                  continue;
               pszName = pos->StringGet (OSSTRING_NAME);
               if (!pszName)
                  continue;

               // write name
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L" : ");
               MemCatSanitize (&gMemTemp, pc->szName);
               if (j+1 < dwElem)
                  MemCat (&gMemTemp, L"<br/>");
            }

            MemCat (&gMemTemp, L"</p>");

            MemCat (&gMemTemp, L"</elem>");
         }

         // add
         ESCMLISTBOXADD la;
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"attrib");
         memset (&la, 0, sizeof(la));
         la.dwInsertBefore = -1;
         la.pszMML = (PWSTR) gMemTemp.p;
         if (pControl)
            pControl->Message (ESCM_LISTBOXRESETCONTENT, NULL);
         if (la.pszMML[0] && pControl) {
            pControl->Message (ESCM_LISTBOXADD, &la);
            pControl->AttribSet (CurSel(), 0);
         }
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"edit")) {
            // get the one
            PCEscControl pControl = pPage->ControlFind (L"attrib");
            if (!pControl)
               return TRUE;
            DWORD dwIndex;
            dwIndex = pControl->AttribGetInt (CurSel());

            // get the value
            ESCMLISTBOXGETITEM lgi;
            memset (&lgi, 0, sizeof(lgi));
            lgi.dwIndex = dwIndex;
            pControl->Message (ESCM_LISTBOXGETITEM, &lgi);
            if (!lgi.pszName) {
               pPage->MBWarning (L"You must select an attribute in the list box.");
               return TRUE;
            }
            dwIndex = _wtoi (lgi.pszName);
            dwIndex = min(dwIndex, po->plCOEAttrib->Num()-1);

            // can only edit certain types
            PCOEAttrib pa;
            pa = *((PCOEAttrib*) po->plCOEAttrib->Get(dwIndex));
            if ((pa->m_dwType != 0) || pa->m_fAutomatic) {
               pPage->MBWarning (L"You cannot edit \"automatic\" attributes.",
                  L"If you wish to use an attribute of the same name, delete the current "
                  L"automatic attribute first.");
               return TRUE;
            }

            po->dwEdit = dwIndex;
            pPage->Exit (L"edit");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"remove")) {
            // get the one
            PCEscControl pControl = pPage->ControlFind (L"attrib");
            if (!pControl)
               return TRUE;
            DWORD dwIndex;
            dwIndex = pControl->AttribGetInt (CurSel());

            // get the value
            ESCMLISTBOXGETITEM lgi;
            memset (&lgi, 0, sizeof(lgi));
            lgi.dwIndex = dwIndex;
            pControl->Message (ESCM_LISTBOXGETITEM, &lgi);
            if (!lgi.pszName) {
               pPage->MBWarning (L"You must select an attribute in the list box.");
               return TRUE;
            }
            dwIndex = _wtoi (lgi.pszName);
            dwIndex = min(dwIndex, po->plCOEAttrib->Num()-1);

            // verify
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the attribute?",
               L"Deleting it will permenantly remove the attribute."))
               return TRUE;

            // remove it
            PCOEAttrib pa;
            pa = *((PCOEAttrib*) po->plCOEAttrib->Get(dwIndex));
            if ((pa->m_dwType == 0) && (pa->m_fAutomatic)) {
               // add flags COEAttrib to remove all the ones listed
               DWORD dwElem = pa->m_lCOEATTRIBCOMBO.Num();
               PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
               DWORD i;

               for (i = 0; i < dwElem; i++, pc++) {
                  PCOEAttrib pNew = new COEAttrib;
                  if (!pNew)
                     continue;

                  pNew->m_dwType =  1; // remove
                  wcscpy (pNew->m_szName, pc->szName);
                  pNew->m_gRemove = pc->gFromObject;

                  po->plCOEAttrib->Add (&pNew);
               }
            }
            delete pa;
            po->plCOEAttrib->Remove (dwIndex);



            // update
            ObjEditWritePCOEAttribList (pv->m_pWorld, po->plCOEAttrib);
            ObjEditClearPCOEAttribList (po->plCOEAttrib);
            ObjEditCreatePCOEAttribList (pv->m_pWorld, po->plCOEAttrib);
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"restoreauto")) {
            // see if any auto attributes have been delted
            PCOEAttrib pa;
            DWORD i;
            for (i = 0; i < po->plCOEAttrib->Num(); i++) {
               pa = *((PCOEAttrib*) po->plCOEAttrib->Get(i));
               if (pa->m_dwType == 1) //  && pa->m_fAutomatic)
                  break;
            }
            if (i >= po->plCOEAttrib->Num()) {
               pPage->MBInformation (L"You haven't deleted any automatic attributes for this object.");
               return TRUE;
            }


            if (IDYES != pPage->MBYesNo (L"Are you sure you want to restore deleted automatic attributes?"))
               return TRUE;

            // restore
            for (i = 0; i < po->plCOEAttrib->Num(); i++) {
               pa = *((PCOEAttrib*) po->plCOEAttrib->Get(i));
               if (pa->m_dwType != 1) // || !pa->m_fAutomatic)
                  continue;

               // found one
               po->plCOEAttrib->Remove (i);
               i--;
            }

            // update
            ObjEditWritePCOEAttribList (pv->m_pWorld, po->plCOEAttrib);
            ObjEditClearPCOEAttribList (po->plCOEAttrib);
            ObjEditCreatePCOEAttribList (pv->m_pWorld, po->plCOEAttrib);
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Attributes";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* ObjEdiorAttribSelPage
*/
BOOL ObjEditorAttribSelPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POEAPAGE po = (POEAPAGE)pPage->m_pUserData;
   PCHouseView pv = po->pv;

   switch (dwMessage) {
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         if ((p->psz[0] == L's') && (p->psz[1] == L':')) {
            GUID  gObject;
            DWORD dwFind;
            MMLBinaryFromString (p->psz + 2, (PBYTE) &gObject, sizeof(gObject));
            dwFind = pv->m_pWorld->ObjectFind (&gObject);

            if (dwFind != -1) {
               pv->m_pWorld->SelectionClear();
               pv->m_pWorld->SelectionAdd (dwFind);
            }

            return TRUE;
         }
         else if ((p->psz[0] == L'a') && (p->psz[1] == L':')) {
            WCHAR szTemp[128];
            if (wcslen(p->psz) > 128)
               break; // cant deal with this
            wcscpy (szTemp, p->psz + 2);
            PWSTR pszColon;
            pszColon = wcschr (szTemp, L':');
            if (!pszColon)
               break;   // cant deal with this
            pszColon[0] = 0;
            pszColon++;

            MMLBinaryFromString (szTemp, (PBYTE) &po->gObject, sizeof(po->gObject));
            wcscpy (po->szAttrib, pszColon);

            pPage->Exit (L"next");
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Select object and attribute";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OBJECTS")) {
            MemZero (&gMemTemp);

            DWORD i;
            WCHAR szTemp[64];
            GUID gGUID;
            PWSTR psz;
            CListFixed lAttrib;
            lAttrib.Init (sizeof(ATTRIBVAL));
            for (i = 0; i < pv->m_pWorld->ObjectNum(); i++) {
               PCObjectSocket pos;
               pos = pv->m_pWorld->ObjectGet (i);
               if (!pos)
                  continue;
               pos->GUIDGet (&gGUID);
               MMLBinaryToString ((PBYTE) &gGUID, sizeof(gGUID), szTemp);

               // table
               MemCat (&gMemTemp, L"<xtablecenter width=100%>");

               // title
               MemCat (&gMemTemp, L"<xtrheader><a color=#80ffff href=s:");
               MemCatSanitize (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"><xHoverHelpShort>Click on this to select the object.</xHoverHelpShort>");
               psz = pos->StringGet (OSSTRING_NAME);
               if (!psz)
                  psz = L"Unknown";
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</a></xtrheader>");

               // attributes within
               DWORD j;
               lAttrib.Clear();
               pos->AttribGetAll (&lAttrib);
               PATTRIBVAL pav;
               pav = (PATTRIBVAL) lAttrib.Get(0);
               ATTRIBINFO ai;
               MemCat (&gMemTemp, L"<tr><td>");
               for (j = 0; j < lAttrib.Num(); j++, pav++) {
                  MemCat (&gMemTemp, L"<a href=\"a:");
                  MemCatSanitize (&gMemTemp, szTemp);
                  MemCat (&gMemTemp, L":");
                  MemCatSanitize (&gMemTemp, pav->szName);
                  MemCat (&gMemTemp, L"\">");
                  MemCatSanitize (&gMemTemp, pav->szName);
                  MemCat (&gMemTemp, L"</a>");
                  
                  // descriptions
                  memset (&ai, 0, sizeof(ai));
                  pos->AttribInfo (pav->szName, &ai);
                  MemCat (&gMemTemp, L"<br/>");
                  if (ai.pszDescription && ai.pszDescription[0]) {
                     MemCat (&gMemTemp, L"<align parindent=32 wrapindent=32>");
                     MemCatSanitize (&gMemTemp, (PWSTR) ai.pszDescription);
                     MemCat (&gMemTemp, L"</align><br/>");
                  }
               }
               MemCat (&gMemTemp, L"</td></tr>");

               MemCat (&gMemTemp, L"</xtablecenter>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/* ObjEdiorAttribEditPage
*/
BOOL ObjEditorAttribEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POEAPAGE po = (POEAPAGE)pPage->m_pUserData;
   PCHouseView pv = po->pv;
   PCOEAttrib pa = *((PCOEAttrib*) po->plCOEAttrib->Get(po->dwEdit));

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // if we have somplace to scroll to then do so
         if (po->iVScroll >= 0) {
            pPage->VScroll (po->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            po->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         // edit
         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pa->m_szName);

         pControl = pPage->ControlFind (L"desc");
         if (pControl && pa->m_memInfo.p && ((PWSTR)pa->m_memInfo.p)[0])
            pControl->AttribSet (Text(), (PWSTR)pa->m_memInfo.p);

         pPage->Message (ESCM_USER+82);   // set the values based on infotype

         // combo
         ComboBoxSet (pPage, L"infotype", pa->m_dwInfoType);

         // bools
         pControl = pPage->ControlFind (L"deflowrank");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pa->m_fDefLowRank);

         pControl = pPage->ControlFind (L"defpassup");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pa->m_fDefPassUp);

         // do all the from (t:) values
         WCHAR szTemp[32];
         DWORD i, j;
         DWORD dwNum;
         PCOEATTRIBCOMBO pc;
         pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
         dwNum = pa->m_lCOEATTRIBCOMBO.Num();
         for (i = 0; i < dwNum; i++, pc++) {
            // set limit controls
            swprintf (szTemp, L"c0:%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), (pc->dwCapEnds & 0x01) ? TRUE : FALSE);
            swprintf (szTemp, L"c1:%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), (pc->dwCapEnds & 0x02) ? TRUE : FALSE);

            DWORD dwFind;
            PCObjectSocket pos = NULL;
            ATTRIBINFO ai;
            memset (&ai, 0, sizeof(ai));
            dwFind = pv->m_pWorld->ObjectFind (&pc->gFromObject);
            if (dwFind != -1)
               pos = pv->m_pWorld->ObjectGet (dwFind);
            if (pos)
               pos->AttribInfo (pc->szName, &ai);

            for (j = 0; j < pc->dwNumRemap; j++) {
               swprintf (szTemp, L"t:%d%d", (int)j, (int)i);
               switch (ai.dwType) {
               case AIT_DISTANCE:
                  MeasureToString (pPage, szTemp, pc->afpObjectValue[j]);
                  break;
               case AIT_ANGLE:
                  AngleToControl (pPage, szTemp, pc->afpObjectValue[j]);
                  break;
               default:
                  DoubleToControl (pPage, szTemp, pc->afpObjectValue[j]);
                  break;
               }
            } // j
         } // i
      }
      break;

   case ESCM_USER+82:   // set the values based on m_dwInfoType
      {
         switch (pa->m_dwInfoType) {
         case AIT_DISTANCE:
            MeasureToString (pPage, L"min", pa->m_fMin);
            MeasureToString (pPage, L"max", pa->m_fMax);
            MeasureToString (pPage, L"defvalue", pa->m_fDefValue);
            break;
         case AIT_ANGLE:
            AngleToControl (pPage, L"min", pa->m_fMin);
            AngleToControl (pPage, L"max", pa->m_fMax);
            AngleToControl (pPage, L"defvalue", pa->m_fDefValue);
            break;
         default:
            DoubleToControl (pPage, L"min", pa->m_fMin);
            DoubleToControl (pPage, L"max", pa->m_fMax);
            DoubleToControl (pPage, L"defvalue", pa->m_fDefValue);
            break;
         }

         // do all the from (f:) values
         WCHAR szTemp[32];
         DWORD i, j;
         DWORD dwNum;
         PCOEATTRIBCOMBO pc;
         pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
         dwNum = pa->m_lCOEATTRIBCOMBO.Num();
         for (i = 0; i < dwNum; i++, pc++) {
            for (j = 0; j < pc->dwNumRemap; j++) {
               swprintf (szTemp, L"f:%d%d", (int)j, (int)i);
               switch (pa->m_dwInfoType) {
               case AIT_DISTANCE:
                  MeasureToString (pPage, szTemp, pc->afpComboValue[j]);
                  break;
               case AIT_ANGLE:
                  AngleToControl (pPage, szTemp, pc->afpComboValue[j]);
                  break;
               default:
                  DoubleToControl (pPage, szTemp, pc->afpComboValue[j]);
                  break;
               }
            }
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if ((psz[0] == L'f') && (psz[1] == L':')) {
            // change the from field
            DWORD i, j;
            PCOEATTRIBCOMBO pc;
            j = psz[2] - L'0';
            i = _wtoi(psz + 3);
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;
            if (j >= pc->dwNumRemap)
               return TRUE;

            switch (pa->m_dwInfoType) {
            case AIT_DISTANCE:
               MeasureParseString (pPage, p->pControl->m_pszName, &pc->afpComboValue[j]);
               break;
            case AIT_ANGLE:
               pc->afpComboValue[j] = AngleFromControl (pPage, p->pControl->m_pszName);
               break;
            default:
               pc->afpComboValue[j] = DoubleFromControl (pPage, p->pControl->m_pszName);
               break;
            }
            return TRUE;
         }
         else if ((psz[0] == L't') && (psz[1] == L':')) {
            // change the from field
            DWORD i, j;
            PCOEATTRIBCOMBO pc;
            j = psz[2] - L'0';
            i = _wtoi(psz + 3);
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;
            if (j >= pc->dwNumRemap)
               return TRUE;

            DWORD dwFind;
            PCObjectSocket pos;
            ATTRIBINFO ai;
            pos = NULL;
            memset (&ai, 0, sizeof(ai));
            dwFind = pv->m_pWorld->ObjectFind (&pc->gFromObject);
            if (dwFind != -1)
               pos = pv->m_pWorld->ObjectGet (dwFind);
            if (pos)
               pos->AttribInfo (pc->szName, &ai);

            switch (ai.dwType) {
            case AIT_DISTANCE:
               MeasureParseString (pPage, p->pControl->m_pszName, &pc->afpObjectValue[j]);
               break;
            case AIT_ANGLE:
               pc->afpObjectValue[j] = AngleFromControl (pPage, p->pControl->m_pszName);
               break;
            default:
               pc->afpObjectValue[j] = DoubleFromControl (pPage, p->pControl->m_pszName);
               break;
            }
            return TRUE;
         }

         // else cheap - since one edit changed just get all the values at once
         PCEscControl pControl;
         DWORD dwNeeded;

         // edit
         pControl = pPage->ControlFind (L"name");
         if (pControl) {
            pControl->AttribGet (Text(), pa->m_szName, sizeof(pa->m_szName), &dwNeeded);
         }

         pControl = pPage->ControlFind (L"desc");
         if (pControl && pa->m_memInfo.Required (256 * 2))
            pControl->AttribGet (Text(), (PWSTR)pa->m_memInfo.p, (DWORD)pa->m_memInfo.m_dwAllocated, &dwNeeded);

         switch (pa->m_dwInfoType) {
         case AIT_DISTANCE:
            MeasureParseString (pPage, L"min", &pa->m_fMin);
            MeasureParseString (pPage, L"max", &pa->m_fMax);
            MeasureParseString (pPage, L"defvalue", &pa->m_fDefValue);
            break;
         case AIT_ANGLE:
            pa->m_fMin = AngleFromControl (pPage, L"min");
            pa->m_fMax = AngleFromControl (pPage, L"max");
            pa->m_fDefValue = AngleFromControl (pPage, L"defvalue");
            break;
         default:
            pa->m_fMin = DoubleFromControl (pPage, L"min");
            pa->m_fMax = DoubleFromControl (pPage, L"max");
            pa->m_fDefValue = DoubleFromControl (pPage, L"defvalue");
            break;
         }

      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"infotype")) {
            DWORD dw;
            dw = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;
            if (dw == pa->m_dwInfoType)
               break;   // no change

            pa->m_dwInfoType = dw;

            // redisplay the values
            pPage->Message (ESCM_USER+82);   // set the values based on infotype
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;


         if (psz[0] == L'r' && psz[1] == L'c' && psz[2] == L':') {
            DWORD i;
            i = _wtoi(psz + 3);
            PCOEATTRIBCOMBO pc;
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            if (IDYES != pPage->MBYesNo (L"Are you sure you want to remove the effect?"))
               return TRUE;

            pa->m_lCOEATTRIBCOMBO.Remove (i);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (psz[0] == L'a' && psz[1] == L'm' && psz[2] == L':') {
            DWORD i;
            i = _wtoi(psz + 3);
            PCOEATTRIBCOMBO pc;
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            pc->afpComboValue[pc->dwNumRemap] = pc->afpComboValue[pc->dwNumRemap-1];
            pc->afpObjectValue[pc->dwNumRemap] = pc->afpObjectValue[pc->dwNumRemap-1];
            pc->dwNumRemap++;

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (psz[0] == L'r' && psz[1] == L'm' && psz[2] == L':') {
            DWORD i;
            i = _wtoi(psz + 3);
            PCOEATTRIBCOMBO pc;
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            pc->dwNumRemap--;

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] = L'c') && ((psz[1] == L'0') || (psz[1] == L'1')) && (psz[2] == L':')) {
            DWORD i, j;
            PCOEATTRIBCOMBO pc;
            j = psz[1] - L'0';
            i = _wtoi(psz + 3);
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            if (p->pControl->AttribGetBOOL (Checked())) {
               pc->dwCapEnds |= (1 << j);
            }
            else {
               pc->dwCapEnds &= ~(1<<j);
            }

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"deflowrank")) {
            pa->m_fDefLowRank = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"defpassup")) {
            pa->m_fDefPassUp = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify an attribute";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ATTRIBLIST")) {
tryagain:
            MemZero (&gMemTemp);

            DWORD i, j;
            DWORD dwNum;
            PCOEATTRIBCOMBO pc;
            dwNum = pa->m_lCOEATTRIBCOMBO.Num();
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
            for (i = 0; i < dwNum; i++, pc++) {
               DWORD dwFind;
               PCObjectSocket pos = NULL;
               ATTRIBINFO ai;
               dwFind = pv->m_pWorld->ObjectFind (&pc->gFromObject);
               if (dwFind != -1)
                  pos = pv->m_pWorld->ObjectGet (dwFind);
               if (!pos) {
                  // do some house cleaning since object no longer exists
                  pa->m_lCOEATTRIBCOMBO.Remove (i);
                  goto tryagain;
               }
               memset (&ai, 0, sizeof(ai));
               if (!pos->AttribInfo (pc->szName, &ai)) {
                  // do some house cleaning since object no longer exists
                  pa->m_lCOEATTRIBCOMBO.Remove (i);
                  goto tryagain;
               }
               PWSTR pszName;
               pszName = pos->StringGet(OSSTRING_NAME);
               if (!pszName)
                  pszName = L"Unknown";

               MemCat (&gMemTemp, L"<xtablecenter width=100%%><xtrheader>");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L" : ");
               MemCatSanitize (&gMemTemp, pc->szName);
               MemCat (&gMemTemp, L"</xtrheader>");
               MemCat (&gMemTemp, L"<tr><td>");
               MemCat (&gMemTemp, L"Below is a table that describes how changes to "
                  L"the current attribute affect <italic>");
               MemCatSanitize (&gMemTemp, pc->szName);
               MemCat (&gMemTemp, L"</italic> in <italic>");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L"</italic>. The left column is the value of the edited attribute "
                  L"and the right is what the value is changed to for the sub-object.<p/>");
               MemCat (&gMemTemp, L"<p align=center><table width=66%%>");
               MemCat (&gMemTemp, L"<tr><td><bold>This</bold></td><td><bold>");
               MemCatSanitize (&gMemTemp, pc->szName);
               MemCat (&gMemTemp, L"</bold></td></tr>");
               for (j = 0; j < pc->dwNumRemap; j++) {
                  MemCat (&gMemTemp, L"<tr>");
                  MemCat (&gMemTemp, L"<td><edit maxchars=32 width=100% selall=true name=f:");
                  MemCat (&gMemTemp, (int) j);  // since only one char
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");

                  MemCat (&gMemTemp, L"<td><edit maxchars=32 width=100% selall=true name=t:");
                  MemCat (&gMemTemp, (int) j);  // since only one char
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");
                  MemCat (&gMemTemp, L"</tr>");
               }

               // button for add
               if (pc->dwNumRemap + 1 < COEMAXREMAP) {
                  MemCat (&gMemTemp, L"<tr><td><xChoiceButton style=righttriangle name=am:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Add another row</bold><br/>"
                     L"Adds a row to the end of the list, providing you with more "
                     L"control over how changing this attributes affect the sub-object."
                     L"</xChoiceButton></td></tr>");
               }
               if (pc->dwNumRemap > 2) {
                  MemCat (&gMemTemp, L"<tr><td><xChoiceButton style=righttriangle name=rm:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Remove last row</bold><br/>"
                     L"Removes the last row."
                     L"</xChoiceButton></td></tr>");
               }

               MemCat (&gMemTemp, L"</table></p>");

               MemCat (&gMemTemp, L"<xChoiceButton checkbox=true style=x name=c0:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Limit low values</bold><br/>"
                  L"If checked, then the attribute cannot be set below the minimum value "
                  L"(first row, left column). If unchecked, the attribute can be set "
                  L"below the minimum value shown."
                  L"</xChoiceButton>");

               MemCat (&gMemTemp, L"<xChoiceButton checkbox=true style=x name=c1:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Limit high values</bold><br/>"
                  L"If checked, then the attribute cannot be set above the maximum value "
                  L"(last row, left column). If unchecked, the attribute can be set "
                  L"above the maximum value shown."
                  L"</xChoiceButton>");

               if (dwNum > 1) {
                  MemCat (&gMemTemp, L"<xChoiceButton style=rightarrow name=rc:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Delete this effect</bold><br/>"
                     L"If you do this changing this attribute will no longer affect ");
                  MemCatSanitize (&gMemTemp, pc->szName);
                  MemCat (&gMemTemp, L" in ");
                  MemCatSanitize (&gMemTemp, pszName);
                  MemCat (&gMemTemp, L".</xChoiceButton>");
               }
               MemCat (&gMemTemp, L"</td></tr>");
               MemCat (&gMemTemp, L"</xtablecenter>");
            }

            MemCat (&gMemTemp, L"<xChoiceButton href=addatt>"
               L"<bold>Affect an additonal attribute in a sub-object</bold><br/>"
               L"Press this to have this attribute affect one or more other "
               L"sub-objects.</xChoiceButton>");
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
LocaleSettings - This brings up the UI for view settings. As parameters are changed
the view object will be notified and told to refresh.

inputs
   PCHouseView    pView - view to use
returns
   none
*/
void LocaleSettings (PCHouseView pView)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (pView->m_hWnd, &r);

   cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

firstpage:
   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLLOCALESET, LocaleSetPage, pView);
   if (!pszRet)
      return;
   if (!_wcsicmp(pszRet, L"info")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLLOCALEINFO, LocaleInfoPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else if (!_wcsicmp(pszRet, L"levels")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLLOCALELEVELS, LocaleLevelsPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else  // probably pressed back
      return;  // exit
}


/****************************************************************************
AppSettings - This brings up the UI for view settings. As parameters are changed
the view object will be notified and told to refresh.

inputs
   PCHouseView    pView - view to use
returns
   none
*/
void AppSettings (PCHouseView pView)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (pView->m_hWnd, &r);

   cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

firstpage:
   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLAPPSET, AppSetPage, pView);
   if (!pszRet)
      return;
   if (!_wcsicmp(pszRet, L"measurement")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLVIEWSETMEASUREMENT, ViewSetMeasurementPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      return;
   }
   else  // probably pressed back
      return;  // exit
}

/****************************************************************************
ObjEditorSettings - This brings up the UI for view settings. As parameters are changed
the view object will be notified and told to refresh.

inputs
   PCHouseView    pView - view to use
returns
   none
*/
void ObjEditorSettings (PCHouseView pView)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (pView->m_hWnd, &r);

   cWindow.Init (ghInstance, pView->m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

mainpage:
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJEDITORSET, ObjEditorSetPage, pView);
   if (!pszRet)
      return;
   if (!_wcsicmp (pszRet, L"misc")) {
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJEDITOR, ObjEditorPage, pView);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto mainpage;
      return;
   }
   else if (!_wcsicmp (pszRet, L"attrib")) {
      OEAPAGE oe;
      CListFixed lOEAttrib;
      lOEAttrib.Init (sizeof(PCOEAttrib));
      memset (&oe, 0, sizeof(oe));
      oe.pv = pView;
      oe.plCOEAttrib = &lOEAttrib;
      

attribpage:
      // refresh the list here so dont worry about with other code
      ObjEditClearPCOEAttribList (oe.plCOEAttrib);
      ObjEditCreatePCOEAttribList (pView->m_pWorld, oe.plCOEAttrib);

      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJEDITORATTRIB, ObjEditorAttribPage, &oe);
      if (pszRet && !_wcsicmp(pszRet, Back())) {
         ObjEditClearPCOEAttribList (&lOEAttrib);
         goto mainpage;
      }
      else if (pszRet && !_wcsicmp(pszRet, L"edit")) {
         oe.iVScroll = 0;

editpage:
         pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJEDITORATTRIBEDIT, ObjEditorAttribEditPage, &oe);
         oe.iVScroll = cWindow.m_iExitVScroll;
         if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
            goto editpage;

         if (pszRet && !_wcsicmp(pszRet, L"addatt")) {
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJEDITORATTRIBSEL, ObjEditorAttribSelPage, &oe);
            if (!pszRet)
               goto cleanup;
            if (!_wcsicmp (pszRet, Back()))
               goto editpage;
            if (!_wcsicmp (pszRet, L"next")) {
               PCOEAttrib pa = *((PCOEAttrib*) oe.plCOEAttrib->Get(oe.dwEdit));
               
               // get the existing info
               ATTRIBINFO ai;
               DWORD dwFind;
               PCObjectSocket pos;
               memset (&ai, 0, sizeof(ai));
               dwFind = pView->m_pWorld->ObjectFind (&oe.gObject);
               pos =NULL;
               if (dwFind != -1)
                  pos = pView->m_pWorld->ObjectGet (dwFind);
               if (pos)
                  pos->AttribInfo (oe.szAttrib, &ai);

               COEATTRIBCOMBO ac;
               memset (&ac, 0, sizeof(ac));
               ac.afpComboValue[0] = ac.afpObjectValue[0] = ai.fMin;
               ac.afpComboValue[1] = ac.afpObjectValue[1] = ai.fMax;
               ac.dwCapEnds = 0;
               ac.dwNumRemap = 2;
               ac.gFromObject = oe.gObject;
               wcscpy (ac.szName, oe.szAttrib);
               pa->m_lCOEATTRIBCOMBO.Add (&ac);

               // edit
               oe.iVScroll = 0;
               goto editpage;
            }
         }  // if adding new - addatt

         // make sure the name is unique
         PCOEAttrib pa;
         pa = *((PCOEAttrib*) oe.plCOEAttrib->Get(oe.dwEdit));
         MakeAttribNameUnique (pa->m_szName, oe.plCOEAttrib, oe.dwEdit);

         // make sure the mapped values are sequential
         PCOEATTRIBCOMBO pc;
         DWORD i,j;
         pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
         for (i = 0; i < pa->m_lCOEATTRIBCOMBO.Num(); i++, pc++) {
            for (j = 1; j < pc->dwNumRemap; j++)
               pc->afpComboValue[j] = max(pc->afpComboValue[j], pc->afpComboValue[j-1]);
         }
         

         // write it out
         ObjEditWritePCOEAttribList (pView->m_pWorld, oe.plCOEAttrib);

         if (!pszRet)
            goto cleanup;
         if (!_wcsicmp (pszRet, Back()))
            goto attribpage;
      }
      else if (pszRet && !_wcsicmp(pszRet, L"add")) {
         pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJEDITORATTRIBSEL, ObjEditorAttribSelPage, &oe);
         if (!pszRet)
            goto cleanup;
         if (!_wcsicmp (pszRet, Back()))
            goto attribpage;
         if (!_wcsicmp (pszRet, L"next")) {
            // create the new entry
            PCOEAttrib pNew = new COEAttrib;
            if (!pNew)
               goto cleanup;  // error
            
            // get the existing info
            ATTRIBINFO ai;
            DWORD dwFind;
            PCObjectSocket pos;
            memset (&ai, 0, sizeof(ai));
            dwFind = pView->m_pWorld->ObjectFind (&oe.gObject);
            pos =NULL;
            if (dwFind != -1)
               pos = pView->m_pWorld->ObjectGet (dwFind);
            if (pos)
               pos->AttribInfo (oe.szAttrib, &ai);

            // fill in new
            pNew->m_dwInfoType = ai.dwType;
            pNew->m_dwType = 0;
            pNew->m_fAutomatic = FALSE;
            pNew->m_fCurValue = pNew->m_fDefValue = 0;
            pNew->m_fDefLowRank = ai.fDefLowRank;
            pNew->m_fDefPassUp = ai.fDefPassUp;
            pNew->m_fMax = ai.fMax;
            pNew->m_fMin = ai.fMin;
            COEATTRIBCOMBO ac;
            memset (&ac, 0, sizeof(ac));
            ac.afpComboValue[0] = ac.afpObjectValue[0] = ai.fMin;
            ac.afpComboValue[1] = ac.afpObjectValue[1] = ai.fMax;
            ac.dwCapEnds = 0;
            ac.dwNumRemap = 2;
            ac.gFromObject = oe.gObject;
            wcscpy (ac.szName, oe.szAttrib);
            pNew->m_lCOEATTRIBCOMBO.Add (&ac);
            if (ai.pszDescription && pNew->m_memInfo.Required((wcslen(ai.pszDescription)+1)*2))
               wcscpy ((PWSTR) pNew->m_memInfo.p, ai.pszDescription);

            // unique name
            DWORD i;
            WCHAR szTemp[64];
            for (i = 0; ; i++) {
               if (i)
                  swprintf (szTemp, L"New attribute %d", (int) i+1);
               else
                  wcscpy (szTemp, L"New attribute");
               if (IsAttribNameUnique (szTemp, oe.plCOEAttrib))
                  break;
            }
            wcscpy (pNew->m_szName, szTemp);

            // add it
            oe.dwEdit = oe.plCOEAttrib->Num();
            oe.plCOEAttrib->Add (&pNew);

            // update
            ObjEditWritePCOEAttribList (pView->m_pWorld, oe.plCOEAttrib);

            // edit
            oe.iVScroll = 0;
            goto editpage;
         }
      }  // if adding new

cleanup:
      ObjEditClearPCOEAttribList (&lOEAttrib);
      return;
   }
}

/****************************************************************************
CHouseViewWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CHouseView, and calls that.
*/
LRESULT CALLBACK CHouseViewWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCHouseView p = (PCHouseView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCHouseView p = (PCHouseView) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCHouseView) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*****************************************************************************
QualityApply - Applies a specific quality to a render object

inputs
   DWORD          dwQuality - Quality to use
   PCRenderTraditional pRender - Render object
*/
void QualityApply (DWORD dwQuality, PCRenderTraditional pRender)
{
   // BUGFIX - MOve quality initialize to here
   // if quality is not initialized then do so now
   if (!gfQualityInitialize) {
      gfQualityInitialize = TRUE;

      memset (&gQualityInfo, 0, sizeof(gQualityInfo));

      // very fastest
      gQualityInfo[0].cMono = RGB(0xc0, 0xc0, 0xc0);
      gQualityInfo[0].cMonoBack = RGB(0x80, 0x80, 0xff);
      gQualityInfo[0].dwRenderModel = RENDERMODEL_LINEAMBIENT;
      gQualityInfo[0].fBackfaceCull = TRUE;
      gQualityInfo[0].fDetailPixels = 40;

      // fastest
      gQualityInfo[1] = gQualityInfo[0];
      gQualityInfo[1].dwRenderModel = RENDERMODEL_SURFACEMONO;

      // fast
      gQualityInfo[2] = gQualityInfo[1];
      gQualityInfo[2].dwRenderModel = RENDERMODEL_SURFACEAMBIENT;

      // medium
      gQualityInfo[3] = gQualityInfo[2];
      gQualityInfo[3].dwRenderModel = RENDERMODEL_SURFACESHADED;

      // slow
      gQualityInfo[4] = gQualityInfo[3];
      gQualityInfo[4].dwRenderModel = RENDERMODEL_SURFACETEXTURE;
      gQualityInfo[4].fDetailPixels = 20;

      // slowest
      gQualityInfo[5] = gQualityInfo[4];
      gQualityInfo[5].dwRenderModel = RENDERMODEL_SURFACESHADOWS;
      gQualityInfo[5].fDetailPixels = 10;

      // BUGFIX - Only load in if dynamics are set
      if (M3DDynamicGet()) {
         // load them in
         DWORD i;
         HKEY  hKey = NULL;
         DWORD dwDisp;
         RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
            KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
         if (hKey) {
            char szTemp[64];
            for (i = 0; i < NUMQUALITY; i++) {
               sprintf (szTemp, gpszQualityKey, (int) i);
               DWORD dwSize, dwType;
               dwSize = sizeof(gQualityInfo[i]);
               RegQueryValueEx (hKey, szTemp, 0, &dwType, (BYTE*) &gQualityInfo[i], &dwSize);
            }
            RegCloseKey (hKey);
         }
      }
   }

   dwQuality = min(dwQuality, NUMQUALITY-1);

   PQUALITYINFO pq = &gQualityInfo[dwQuality];

   pRender->m_cMono = pq->cMono;
   //pRender->m_cBackgroundMono = pq->cMonoBack;
   pRender->m_dwRenderModel = pq->dwRenderModel;
   pRender->m_fBackfaceCull = pq->fBackfaceCull;
   pRender->m_fDetailPixels = pq->fDetailPixels;
}

/*****************************************************************************
RenderImage - Given a specific renderer, clones it, and then sets the quality
of the new cloned object, and renders and image.

inputs
   //PCProgressSocket     pProgress - Progress bar to use
   PCRenderTraditional pRender - Render to clone
   PCWorldSocket  pWorld - World to use
   DWORRD         dwQuality - Quality to use (from m_dwQUality)
   DWORD          dwWidth - Width of the image
   DWORD          dwHeight - Height of the image
   DWORD          dwAnti - 1..N divisions
   BOOL           fRotate - If TRUE, rotate the image 90 degrees
   fp             fIso - If > 0, this will set the isometric camera to a view of this size.
                     If <= 0 then just ignores.
   GUID           *pgEffectCode - If not NULL, this is the effect to render with
   GUID           *pgEffectSub - If not NULL, this is the effect to render with
retursn
   HBITMAP - Bitmap. THis must be freed by the caller. NULL if error or user pressed cancel
*/
HBITMAP RenderImage (PCRenderTraditional pRender, PCWorldSocket pWorld, DWORD dwQuality,
                     DWORD dwWidth, DWORD dwHeight, DWORD dwAnti, BOOL fRotate, fp fIso,
                     GUID *pgEffectCode, GUID *pgEffectSub)
{
   ANIMINFO ai;
   memset (&ai, 0, sizeof(ai));
   ai.dwAnti = max(dwAnti,1);
   ai.dwExposureStrobe = 1;
   ai.dwPixelsX = dwWidth;
   ai.dwPixelsY = dwHeight;
   ai.dwQuality = dwQuality;
   ai.fToBitmap = TRUE;
   ai.pRender = pRender;
   ai.pWorld = pWorld;
   if (pgEffectCode)
      ai.gEffectCode = *pgEffectCode;
   if (pgEffectSub)
      ai.gEffectSub = *pgEffectSub;

   //CImage Image, ImageRot;
   //DWORD dwAnti = (fAnti ? 2 : 1);

   DWORD dwStart;
   dwStart = GetTickCount();

   //pRender->CloneTo (&Render);
   //dwWidth = max(1,dwWidth);
   //dwHeight = max(1,dwHeight);
   //if (!Image.Init (dwWidth * dwAnti, dwHeight * dwAnti))
   //   return NULL;
   //Render.CImageSet (&Image);
   //::QualityApply (dwQuality, &Render);
   //Render.m_fFinalRender = TRUE;

   // if isometric then change
   PCRenderTraditional pRenderTemp = NULL;
   if (fIso > 0) {
      pRenderTemp = pRender->Clone ();
      if (pRenderTemp) {
         CPoint pCenter;
         fp fLong, fTiltX, fTiltY, fScale, fTransX, fTransY;
         pRenderTemp->CameraFlatGet (&pCenter, &fLong, &fTiltX, &fTiltY, &fScale, &fTransX, &fTransY);
         pRenderTemp->CameraFlat (&pCenter, fLong, fTiltX, fTiltY, fIso, fTransX, fTransY);

         ai.pRender = pRender;
      }
   }

#ifdef _DEBUG
   size_t iMallocStart = EscMemoryAllocateTimes ();
#endif

   MALLOCOPT_INIT;
   MALLOCOPT_BADMALLOC;

   //Render.Render (-1, pProgress);
   if (!AnimAnimateWait (DEFAULTRENDERSHARD, &ai) || !ai.hToBitmap) {
      MALLOCOPT_RESTORE;
      if (pRenderTemp)
         delete pRenderTemp;
      return NULL;
   }

   MALLOCOPT_RESTORE;

#ifdef _DEBUG
   size_t iMallocEnd = EscMemoryAllocateTimes ();
   WCHAR szwTemp[64];
   swprintf (szwTemp, L"\r\nEscMemoryAllocateTimes delta = %d", (int)(iMallocEnd - iMallocStart));
   OutputDebugStringW (szwTemp);
#endif

   // if rotate 90 degrees for printing then do so
   if (fRotate) {
      // BUGFIX - Had to rotate differently
      CImage Image, ImageRot;
      ImageRot.Init (ai.hToBitmap);
      ImageRot.Init (Image.Height(), Image.Width());
      PIMAGEPIXEL ps, pd;
      DWORD x,y;
      for (x = 0; x < Image.Width(); x++) for (y = 0; y < Image.Height(); y++) {
         ps = Image.Pixel (Image.Width() - x  -1, y);
         pd = ImageRot.Pixel (y, x);
         *pd = *ps;
      }

      DeleteObject (ai.hToBitmap);
      HDC hDC;
      hDC = GetDC (GetDesktopWindow());
      ai.hToBitmap = ImageRot.ToBitmap (hDC);
      ReleaseDC (GetDesktopWindow(), hDC);
   }

   // create bitmap
   //HBITMAP hBit;
   //HDC hDC;
   //PCImage pImage;
   //pImage = (fRotate ? &ImageRot : &Image);
   //hDC = GetDC (NULL);
   //if (fAnti)
   //   hBit = pImage->ToBitmapAntiAlias (hDC, dwAnti);
   //else
   //   hBit = pImage->ToBitmap (hDC);
   //ReleaseDC (NULL, hDC);

   char szTemp[128];
   sprintf (szTemp, "Render time = %d ms\r\n", GetTickCount() - dwStart);
   OutputDebugString (szTemp);

   if (pRenderTemp)
      delete pRenderTemp;

   return ai.hToBitmap;
}


/*****************************************************************************
CHouseView::QualityApply - Looks at the current quality setting, gets the values
from the global settings, and writes those into their proper place.
*/
void CHouseView::QualityApply (void)
{
   if (m_dwQuality >= NUMQUALITY)
      m_dwQuality = NUMQUALITY-1;

   ::QualityApply (m_dwQuality, m_apRender[0]);
}


/*****************************************************************************
CHouseView::QualitySync - Looks at the current view mode and syncs the quality
setting (m_dwQuality) so it matches.
*/
void CHouseView::QualitySync (void)
{
   if (m_dwQuality >= NUMQUALITY)
      m_dwQuality = NUMQUALITY-1;
   PQUALITYINFO pq = &gQualityInfo[m_dwQuality];

   // dont change if the same
   if (m_apRender[0]->m_dwRenderModel == pq->dwRenderModel)
      return;

   // else, find closest fit
   DWORD i;
   for (i = 0; i < NUMQUALITY; i++)
      if (m_apRender[0]->m_dwRenderModel == gQualityInfo[i].dwRenderModel) {
         m_dwQuality = i;
         return;
      }

   // if get here, just pick a quality
   m_dwQuality = NUMQUALITY / 2;
}

/****************************************************************************
Constructor and destructor

inputs
   PCWorldSocket     pWorld - World looking at
   PCSceneSet        pSceneSet - Animation scene working on. If NULL no animation
   DWORD             dwViewWhat - What lookinf at, VIEWWHAT_XXX
   GUID              *pgCode - Object code. Used if VIEWWHAT_OBJECT or VIEWWHAT_POLYMESH.
   GUID              *pgSub - Object sub-code. Used if VIEWWHAT_OBJECT. NOT used for the others
   PCWorld           pWorldObj - Object edited. This will be deleted when
                     the house view is destroyed if VIEWWHAT_OBJECT. NOT for the others
   PCWorldSocket     pWorldPolyMesh - World for the polymesh object. Used for VIEWWHAT_OBJECT
*/
CHouseView::CHouseView (PCWorldSocket pWorld, PCSceneSet pSceneSet, DWORD dwViewWhat,
                        GUID *pgCode, GUID *pgSub, PCWorld pWorldObj, PCWorldSocket pWorldPolyMesh)
{
   DWORD i;
   for (i = 0; i < VIEWTHUMB; i++)
      m_apRender[i] = new CRenderTraditional (DEFAULTRENDERSHARD);

   m_dwViewWhat = dwViewWhat;
   m_dwViewSub = VSPOLYMODE_POLYEDIT;  // default
   m_Purgatory.RenderShardSet (DEFAULTRENDERSHARD);

   switch (m_dwViewWhat) {
   default:
   case VIEWWHAT_POLYMESH:
      m_cBackDark = RGB(0x70,0x50,0x70);
      m_cBackMed = RGB(0xc0,0x80,0xc0);
      m_cBackLight = RGB(0xf0,0xb0,0xf0);
      m_cBackOutline = RGB(0x60, 0x40, 0x60);
      break;

   case VIEWWHAT_BONE:
      // whitish
      m_cBackDark = RGB(0xb0,0xb0,0xb0);
      m_cBackMed = RGB(0xd0,0xd0,0xd0);
      m_cBackLight = RGB(0xff,0xff,0xff);
      m_cBackOutline = RGB(0x40, 0x40, 0x40);
      break;

   case VIEWWHAT_WORLD:
      // greyish
      m_cBackDark = RGB(0x80,0x80,0x80);
      m_cBackMed = RGB(0xa0,0xa0,0xc0);
      m_cBackLight = RGB(0xc0,0xc0,0xff);
      m_cBackOutline = RGB(0x40, 0x40, 0x60);

      // very dark
      //m_cBackDark = RGB(0x0,0x0,0x0);
      //m_cBackMed = RGB(0x40,0x40,0x80);
      //m_cBackLight = RGB(0xa0,0xa0,0xc0);
      //m_cBackOutline = RGB(0x80, 0x80, 0x80);

      // light
      //m_cBackDark = RGB(0xff,0xff,0xff);
      //m_cBackMed = RGB(0xf0,0xf0,0xff);
      //m_cBackLight = RGB(0xe0,0xe0,0xff);
      //m_cBackOutline = RGB(0xe0, 0xe0, 0xff);
      break;
   case VIEWWHAT_OBJECT:
      // greyish
      m_cBackDark = RGB(0x80,0x80,0x80);
      m_cBackMed = RGB(0xa0,0xc0,0xa0);
      m_cBackLight = RGB(0xc0,0xff,0xc0);
      m_cBackOutline = RGB(0x40, 0x60, 0x40);

      // very dark
      //m_cBackDark = RGB(0, 0, 0);
      //m_cBackMed = RGB(0x40,0x80,0x40);
      //m_cBackLight = RGB(0xa0,0xc0,0xa0);
      //m_cBackOutline = RGB(0x80, 0x80, 0x80);

      // light
      //m_cBackDark = RGB(0xff,0xff,0xff);
      //m_cBackMed = RGB(0xf0,0xff,0xf0);
      //m_cBackLight = RGB(0xe0,0xff,0xe0);
      //m_cBackOutline = RGB(0xe0, 0xff, 0xe0);
      break;
   }

   m_fOpenStart = 0;
   m_fTimerWaitToDraw = FALSE;
   m_fTimerWaitToTip = FALSE;
   m_fWaitToDrawForce = FALSE;
   m_plZGrid = NULL;
   m_pWorld = pWorld;
   m_pSceneSet = pSceneSet;
   m_pCamera = NULL;
   m_hWnd = NULL;
   m_pbarGeneral = m_pbarMisc = m_pbarView = m_pbarObject = NULL;
   memset (m_ahWndScroll, 0, sizeof(m_ahWndScroll));
   m_fScrollRelCache = FALSE;
   m_fDisableChangeScroll = FALSE;
   m_dwImageScale = IMAGESCALE;  // BUGFIX - Was IMAGESCALE x 2, but since PCs getting faster, default to higher

   // BUGFIX - If views already available then use the scaling from that
   if (gListPCHouseView.Num()) {
      PCHouseView pv = *((PCHouseView*) gListPCHouseView.Get(0));
      m_dwImageScale = pv->m_dwImageScale;
   }

   m_fSmallWindow = FALSE;
   m_fHideButtons = FALSE;

   m_pCenterWorld.Zero();
   m_fWorldChanged = 0;
   m_dwCameraButtons = 0xffff;
   memset (m_adwPointerMode, 0 ,sizeof(m_adwPointerMode));
   m_dwPointerModeLast = 0;
   m_dwButtonDown = 0;
   m_pntButtonDown.x = m_pntButtonDown.y = 0;
   //m_fScaleScroll = 0;
   m_dwMoveTimerID = 0;
   m_dwMoveTimerInterval = 0;
   m_pCameraCenter.Zero();
   m_fCaptured = FALSE;
   m_fClipTempAngle = 0;
   m_fClipAngleValid = FALSE;

   m_fSelRegionWhollyIn = TRUE;
   m_fSelRegionVisible = FALSE;

   m_listNeedSelect.Init (sizeof(PCIconButton));
   m_listNeedSelectNoEmbed.Init (sizeof(PCIconButton));
   m_listNeedSelectOnlyOneEmbed.Init (sizeof(PCIconButton));
   m_listNeedSelectOneEmbed.Init(sizeof(PCIconButton));
   m_dwMoveReferenceCur = 0;
   m_pToolTip = NULL;
   m_dwToolTip = 2;  // lower right
   m_pUndo = m_pRedo = NULL;
   m_hWndClipNext = NULL;
   m_pPaste = NULL;
   m_pObjControlNSEW = m_pObjControlUD = m_pObjControlNSEWUD = m_pObjControlInOut = NULL;

   m_dwObjControlObject = (DWORD)-1;
   m_dwObjControlID = 0;
   m_fObjControlOrigZ = 0;
   m_pObjControlClick.Zero();
   m_pObjControlOrig.Zero();

   // grid
   m_fGridDisplay = FALSE;
   m_dwPolyMeshSelMode = -1;
   m_lPolyMeshVert.Init (sizeof(DWORD));
   // DWORD i;
   switch (m_dwViewWhat) {
      case VIEWWHAT_BONE:
         m_fGrid = (MeasureDefaultUnits() & MUNIT_ENGLISH) ? (METERSPERFOOT/12.0/10.0) : 0.005;
         m_fGridDisplay2D = TRUE;
         m_dwThumbnailShow = 1;
         m_fShowOnlyPolyMesh = FALSE;  // BUGIF - Default to showing polymesh
         break;

      case VIEWWHAT_POLYMESH:
         // BUGFIX - Selregion defaults different
         m_fSelRegionWhollyIn = FALSE;
         m_fSelRegionVisible = TRUE;

         // BUGFIX - Smaller grid for polymesh
         m_fGrid = (MeasureDefaultUnits() & MUNIT_ENGLISH) ? (METERSPERFOOT/12.0/10.0) : 0.005;
         m_fGridDisplay2D = TRUE;
         m_dwThumbnailShow = 1;
         m_fShowOnlyPolyMesh = TRUE;

         // set outlines
         for (i = 0; i < VIEWTHUMB; i++)
            if (m_apRender[i]->m_pEffectOutline)
               m_apRender[i]->m_pEffectOutline->m_dwLevel = 3; // so that identifies the edges of polys before subdivide
         break;

      case VIEWWHAT_OBJECT:
         m_fGrid = (MeasureDefaultUnits() & MUNIT_ENGLISH) ? (METERSPERFOOT/12.0) : 0.050;
         m_fGridDisplay2D = TRUE;
         m_dwThumbnailShow = 1;
         m_fShowOnlyPolyMesh = FALSE;
         break;
      default:
      case VIEWWHAT_WORLD:
         m_fGridDisplay2D = FALSE;
         m_fGrid = (MeasureDefaultUnits() & MUNIT_ENGLISH) ? METERSPERFOOT : 0.10;
         m_dwThumbnailShow = 0;
         m_fShowOnlyPolyMesh = FALSE;
         break;
   }

   m_fGridAngle = TENDEGREES;
   m_pGridCenter.Zero();
   m_pGridRotation.Zero();
   m_mGridToWorld.Identity();
   m_mWorldToGrid.Identity();

   m_dwThumbnailCur = 1;
   memset (m_arThumbnailLoc, 0, sizeof(m_arThumbnailLoc));
   memset (m_afThumbnailVis, 0, sizeof(m_afThumbnailVis));


   // view preferences
   m_fVWWalkSpeed = 4.0 * 1000.0 / 60.0 / 60.0; // 4 kmph; (per second)
   m_fVWalkAngle = PI / 4;
   m_fViewScale = 40.0;
   // BUGFIX - If object then slower walking speed
   switch (m_dwViewWhat) {
      case VIEWWHAT_POLYMESH:
      case VIEWWHAT_OBJECT:
      case VIEWWHAT_BONE:
         {
            // remember what object editing
            m_gObject = *pgCode;

            // if object, make scale based on object size
            CPoint b[2];
            if (m_dwViewWhat == VIEWWHAT_POLYMESH) {
               PolyMeshBoundBox (b);
               m_pCenterWorld.Average (&b[0], &b[1]);
            }
            else if (m_dwViewWhat == VIEWWHAT_BONE) {
               PolyMeshBoundBox (b);
               m_pCenterWorld.Average (&b[0], &b[1]);
            }
            else
               m_pWorld->WorldBoundingBoxGet (&b[0], &b[1], FALSE);
            fp fMax;
            fMax = 0;
            DWORD x;
            for (x = 0; x < 3; x++)
               fMax = max(fMax, fabs(b[1].p[x] - b[0].p[x]));
            fMax *= 3;

            fMax = max (2, fMax);   // always at least 2 m
            m_fViewScale = fMax;

            // walking speed slower
            m_fVWWalkSpeed /= 4.0;
         }
         break;
      default:
      case VIEWWHAT_WORLD:
         // do nothing
         break;
   }
   ScrollActionInit ();

   // register clibboard format
   m_dwClipFormat = RegisterClipboardFormat (APPLONGNAME " data");

   m_fPolyMeshTextDisp = FALSE;
   m_tpPolyMeshTextDispHV. h = m_tpPolyMeshTextDispHV.v = 0;
   m_fPolyMeshTextDispScale = 100;
   m_pPolyMeshTextDispPoint.x = m_pPolyMeshTextDispPoint.y = m_fPolyMeshTextDispScale / 2;
   m_dwPolyMeshTextDispLastVert = -1;

   // Add this to the list of views around
   if (!gfListPCHouseViewInit) { 
      gListPCHouseView.Init (sizeof(PCHouseView));
      gListVIEWOBJ.Init (sizeof(VIEWOBJ));
      gListVIEWOBJECT.Init (sizeof(VIEWOBJECT));
      gfListPCHouseViewInit = TRUE;
   }
   switch (m_dwViewWhat) {
   case VIEWWHAT_POLYMESH:
   case VIEWWHAT_BONE:
      {
         VIEWOBJECT vo;
         memset (&vo, 0, sizeof(vo));
         vo.pView = this;
         vo.pWorld = pWorldPolyMesh;
         vo.gObject = *pgCode;
         gListVIEWOBJECT.Add (&vo);
         m_fPolyMeshTextDisp = TRUE;
      }
      break;
   case VIEWWHAT_OBJECT:
      {
         VIEWOBJ vo;
         memset (&vo, 0, sizeof(vo));
         vo.pView = this;
         vo.pWorld = pWorldObj;
         vo.gCode = *pgCode;
         vo.gSub = *pgSub;
         ObjectCFNameFromGUIDs(DEFAULTRENDERSHARD, pgCode, pgSub, vo.szMajor, vo.szMinor, vo.szName);
         gListVIEWOBJ.Add (&vo);

         // set the name for the world
         WCHAR szTemp[256];
         // wcscpy (szTemp, L"Object: ");
         wcscpy (szTemp, vo.szName);
         pWorld->NameSet (szTemp);
      }
      break;
   case VIEWWHAT_WORLD:
   default:
      PCHouseView ph = this;
      gListPCHouseView.Add (&ph);
      break;
   }

   m_hWndPolyMeshList = NULL;
   memset (&m_rPolyMeshList, 0, sizeof(m_rPolyMeshList));
   m_hPolyMeshFont = NULL;
   m_dwPolyMeshCurMorph = -1;
   m_dwPolyMeshCurText = 0;
   m_dwPolyMeshMaskColor = 0;
   m_fPolyMeshMaskInvert = FALSE;
   m_dwPolyMeshBrushAffectOrganic = 0;
   m_dwPolyMeshBrushAffectBone = 0;
   m_dwPolyMeshBrushAffectTailor = 0;
   m_dwPolyMeshBrushAffectMorph = 0;
   m_dwPolyMeshBrushStrength = 5;
   m_dwPolyMeshBrushPoint = 1;
   m_dwPolyMeshMagPoint = 2;
   m_dwPolyMeshMagSize = 4;
   m_fAirbrushTimer = FALSE;
   m_fMoveEmbedded = FALSE;
   m_MoveEmbedHV.h = m_MoveEmbedHV.v = 0;
   m_fMoveEmbedRotation = 0;
   m_dwMoveEmbedSurface = 0;
   m_MoveEmbedClick.h = m_MoveEmbedClick.v = 0;
   m_MoveEmbedOld = m_MoveEmbedClick;
   m_fPastingEmbedded = FALSE;
   m_pPolyMeshOrig = NULL;
   m_adwPolyMeshMove[0] = m_adwPolyMeshMove[1] = -1;
   m_dwPolyMeshMoveSide = -1;
   memset (m_adwPolyMeshSplit, 0, sizeof(m_adwPolyMeshSplit));
   memset (&m_rPolyMeshLastSplitLine, 0, sizeof(m_rPolyMeshLastSplitLine));

   // quality
   m_dwQuality = 4;  // BUGFIX: start out with texture quality
   QualityApply ();
   CalcThumbnailLoc ();

}


CHouseView::~CHouseView (void)
{
   if (m_hWndPolyMeshList) {
      DestroyWindow (m_hWndPolyMeshList);
      m_hWndPolyMeshList = NULL;
   }

   if (m_hPolyMeshFont) {
      DeleteObject (m_hPolyMeshFont);
      m_hPolyMeshFont = NULL;
   }

   if (m_pPolyMeshOrig) {
      delete m_pPolyMeshOrig;
      m_pPolyMeshOrig = NULL;
   }

   if (m_fAirbrushTimer)
      KillTimer (m_hWnd, TIMER_AIRBRUSH);

   // kill the wait to draw timer
   if (m_fTimerWaitToDraw)
      KillTimer (m_hWnd, WAITTODRAW);
   m_fTimerWaitToDraw = NULL;

   // kill the tip timer
   if (m_fTimerWaitToTip)
      KillTimer (m_hWnd, WAITTOTIP);
   m_fTimerWaitToTip = NULL;

   if (m_plZGrid)
      delete m_plZGrid;
   m_plZGrid = NULL;

   if (m_pCamera) {
      m_pCamera->m_pView = NULL; // so doenst call back
      if (!m_pWorld)
         m_pCamera->Delete ();
      else {
         GUID gID;
         m_pCamera->GUIDGet (&gID);
         DWORD dwID;
         dwID = m_pWorld->ObjectFind (&gID);
         m_pWorld->ObjectRemove (dwID);
      }
      m_pCamera = NULL;
   }

   // unregister with world socket
   if (m_pWorld) {
      m_pWorld->NotifySocketRemove (this);
   }

   if (m_pToolTip)
      delete m_pToolTip;

   // delete the bars
   if (m_pbarGeneral)
      delete m_pbarGeneral;
   m_pbarGeneral = NULL;

   if (m_pbarMisc)
      delete m_pbarMisc;
   m_pbarMisc = NULL;

   if (m_pbarView)
      delete m_pbarView;
   m_pbarView = NULL;

   if (m_pbarObject)
      delete m_pbarObject;
   m_pbarObject = NULL;

   if (m_dwMoveTimerID)
      KillTimer (m_hWnd, m_dwMoveTimerID);
   m_dwMoveTimerID = 0;

   if (m_hWnd)
      DestroyWindow (m_hWnd);

   // remove self from list of house views
   DWORD i;
   PCHouseView ph;
   for (i = 0; i < gListPCHouseView.Num(); i++) {
      ph = *((PCHouseView*)gListPCHouseView.Get(i));
      if (ph == this) {
         gListPCHouseView.Remove(i);
         break;
      }
   }

   // and from list of editing objects
   for (i = 0; i < gListVIEWOBJ.Num(); i++) {
      PVIEWOBJ pvo = (PVIEWOBJ) gListVIEWOBJ.Get(i);
      if (pvo->pView == this) {
         // close the list of objects for this
         ListViewRemove (pvo->pWorld);

         delete pvo->pWorld;
         gListVIEWOBJ.Remove (i);
         break;
      }
   }

   // and from list of polymesh
   for (i = 0; i < gListVIEWOBJECT.Num(); i++) {
      PVIEWOBJECT pvo = (PVIEWOBJECT) gListVIEWOBJECT.Get(i);
      if (pvo->pView == this) {
         gListVIEWOBJECT.Remove (i);
         break;
      }
   }

   // If no more views availablet then call PostQuitMessage()
   if (!gListPCHouseView.Num() && !gListVIEWOBJ.Num() && !gListVIEWOBJECT.Num()) {
      // close the list view for this
      ListViewRemove (m_pWorld);

      SceneViewShutDown ();


      PostQuitMessage (0);
   }

   for (i = 0; i < VIEWTHUMB; i++)
      if (m_apRender[i]) {
         delete m_apRender[i];
         m_apRender[i] = NULL;
      }

}

/***********************************************************************************
CHouseView::NextPointerModeGiven the current point mode. Switches to the next (or previous) one.

inputs
   DWORD       dwMode - 0 move selection up, 1 down, 2 right, 3 left,
                        4 rotate clockwise to next group, 5 rotate counterclockwise to previous group,
                        6 move clockwise, 7 move counterclockwise
returns
   none
*/
typedef struct {
   DWORD dwID;
   PCButtonBar pbb;  //group it's in
} NPM, *PNPM;

void CHouseView::NextPointerMode (DWORD dwMode)
{

   // make a list
   DWORD i,j,k;
   CListFixed lMode;
   NPM npm;
   PCButtonBar pbb;
   lMode.Init (sizeof(npm));
   for (i = 0; i < 4; i++) {
      BOOL fReverse;
      fReverse = FALSE;
      switch (i) {
      case 0:
         pbb = m_pbarView;
         break;
      case 1:
         pbb = m_pbarGeneral;
         break;
      case 2:
         pbb = m_pbarObject;
         fReverse = TRUE;
         break;
      case 3:
         pbb = m_pbarMisc;
         fReverse = TRUE;
         break;
      }

      for (j = 0; j < 3; j++) {
         DWORD dwGroup;
         dwGroup = fReverse ? (2 - j) : j;

         for (k = 0; k < pbb->m_alPCIconButton[dwGroup].Num(); k++) {
            BOOL fRev2 = fReverse;
            if (dwGroup == 2)
               fRev2 = !fRev2;
            PCIconButton pb= *((PCIconButton*) pbb->m_alPCIconButton[dwGroup].Get (
               fRev2 ? (pbb->m_alPCIconButton[dwGroup].Num() - k - 1) : k));

            if (!IsWindowEnabled (pb->m_hWnd))
               continue;
            if (!(pb->FlagsGet () & (IBFLAG_REDARROW | IBFLAG_DISABLEDARROW)))
               continue;

            // else, add this
            npm.dwID = pb->m_dwID;
            npm.pbb = pbb;
            lMode.Add(&npm);
         }
      }

   }  // i


   // find the current pointer mode
   DWORD dwNum;
   PNPM pn;
   pn = (PNPM) lMode.Get(0);
   dwNum = lMode.Num();

   for (i = 0; i < dwNum; i++)
      if (pn[i].dwID == m_adwPointerMode[0])
         break;
   if (i >= dwNum)
      return;  // couldnt find

   PNPM pc;
   pc = pn + i;


#if 0 // not doing it this way anymore
   // Modes 6 and 7 change the appearance depending upon where we are
   if (dwMode == 6) {
      if ((pc->pbb == m_pbarMisc) || (pc->pbb == m_pbarGeneral))
         dwMode = 0;
      else
         dwMode = 2;
   }
   else if (dwMode == 7) {
      if ((pc->pbb == m_pbarMisc) || (pc->pbb == m_pbarGeneral))
         dwMode = 1;
      else
         dwMode = 3;
   }
#endif // 0

   BOOL fUp, fRight;
   switch (dwMode) {
   case 0:  // up arrow
   case 1:  // down arrow
      fUp = (dwMode == 0);
      i += dwNum; // for modulo reasons
      if (pc->pbb == m_pbarMisc) {
         // on left, going
         if (fUp)
            i++;
         else
            i--;
      }
      else if (pc->pbb == m_pbarGeneral) {
         // on right
         if (fUp)
            i--;
         else
            i++;
      }
      else if (pc->pbb == m_pbarView) {
         // on top
         if (!fUp) {
            for (i = 0; i < dwNum; i++)
               if (pn[i].pbb == m_pbarObject)
                  break;
         }
      }
      else if (pc->pbb == m_pbarObject) {
         // on bottom
         if (fUp) {
            for (i = 0; i < dwNum; i++)
               if (pn[i].pbb == m_pbarView)
                  break;
         }
      }
      break;
   case 2:  // right arrow
   case 3:  // left arrow
      fRight = (dwMode == 2);
      i += dwNum; // for modulo reasons
      if (pc->pbb == m_pbarView) {
         // on top
         if (fRight)
            i++;
         else
            i--;
      }
      else if (pc->pbb == m_pbarObject) {
         // on bottom
         if (fRight)
            i--;
         else
            i++;
      }
      else if (pc->pbb == m_pbarGeneral) {
         // on right
         if (!fRight) {
            for (i = 0; i < dwNum; i++)
               if (pn[i].pbb == m_pbarMisc)
                  break;
         }
      }
      else if (pc->pbb == m_pbarMisc) {
         // on left
         if (fRight) {
            for (i = 0; i < dwNum; i++)
               if (pn[i].pbb == m_pbarGeneral)
                  break;
         }
      }
      break;
   case 4:  // clockwise to next group
      for (j = 0; j < dwNum; j++)
         if (pc->pbb != pn[(i+j+1)%dwNum].pbb)
            break;
      i = i + j + 1;
      break;
   case 5:  // counterclockwise to next group
      i += dwNum;
      for (j = 0; j < dwNum; j++)
         if (pc->pbb != pn[(i-j-1)%dwNum].pbb)
            break;
      i = i - j - 1;
      break;

   case 6:
      i++;
      break;

   case 7:
      i = i + dwNum - 1;
      break;
   }

   i = i % dwNum;
   SetPointerMode (pn[i].dwID, 0);
}



/***********************************************************************************
CHouseView::ButtonExists - Looks through all the buttons and sees if one with the given
ID exists. If so it returns TRUE, else FALSE

inputs
   DWORD       dwID - button ID
   DWORD       dwEnabledFor - What this should be enbled for, 0=left, 1 for middle, 2 for right
returns
   DWORD - 0 if the button doesn't exist at all, 1 if it exists but isn't selected for dwEnabled for, 2 if exists and selected
*/
DWORD CHouseView::ButtonExists (DWORD dwID, DWORD dwEnabledFor)
{
   PCIconButton pb;

   if (pb = m_pbarGeneral->ButtonExists(dwID))
      goto verifyenabled;
   if (pb = m_pbarMisc->ButtonExists(dwID))
      goto verifyenabled;
   if (pb = m_pbarObject->ButtonExists(dwID))
      goto verifyenabled;
   if (pb = m_pbarView->ButtonExists(dwID))
      goto verifyenabled;

   return 0;

verifyenabled:
   DWORD dwFlag;
   switch (dwEnabledFor) {
   case 0: // left
   default:
      dwFlag = IBFLAG_REDARROW;
      break;
   case 1: // middle
      dwFlag = IBFLAG_MBUTTON;
      break;
   case 2:  // right
      dwFlag = IBFLAG_RBUTTON;
      break;
   }
   return (pb->FlagsGet() & dwFlag) ? 2 : 1;
}


/***********************************************************************************
CHouseView::SetPointerMode - Changes the pointer mode to the new mode.

inputs
   DWORD    dwMode - new mode
   DWORD    dwButton - which button it's approaite for, 0 for left, 1 for middle, 2 for right
*/
void CHouseView::SetPointerMode (DWORD dwMode, DWORD dwButton)
{
   if (dwButton >= 3)
      return;  // out of bounds

   // if change pointer mode make sure no polygon shown
   if (m_lPolyMeshVert.Num()) {
      // moving mouse around while in create new poly mode...
      PCObjectPolyMesh pm = PolyMeshObject2();
      if (pm) {
         RECT r;
         PolyMeshVertToRect (pm, m_pntMouseLast, &r);
         if (r.left != r.right)
            InvalidateRect (m_hWnd, &r, FALSE);
      }
      m_lPolyMeshVert.Clear();
   }

   // keep track of last pointer mode
   if ((dwButton == 0) && (dwMode != m_adwPointerMode[dwButton]))
      m_dwPointerModeLast = m_adwPointerMode[dwButton];

   m_adwPointerMode[dwButton] = dwMode;

   if (!dwButton)
      UpdateToolTipByPointerMode();

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


   // if change pointer mode then take a snapshot for undo
   m_pWorld->UndoRemember();

   POINT px;
   GetCursorPos (&px);
   ScreenToClient (m_hWnd, &px);
   SetProperCursor (px.x, px.y);
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
BOOL CHouseView::Init (DWORD dwMonitor)
{
   if (m_hWnd)
      return FALSE;

   PCObjectSocket pos = NULL;
   if ((m_dwViewWhat == VIEWWHAT_POLYMESH) || (m_dwViewWhat == VIEWWHAT_BONE)) {
      pos = PolyMeshObject ();
      if (!pos)
         return FALSE;
   }

   // set the rendering world
   DWORD i;
   for (i = 0; i < VIEWTHUMB; i++)
      m_apRender[i]->CWorldSet (m_pWorld);

   // get the trace info
   TraceInfoFromWorld (m_pWorld, &m_lTraceInfo);


   // register the window proc if it isn't alreay registered
   static BOOL fIsRegistered = FALSE;
   if (!fIsRegistered) {
      WNDCLASS wc;
      memset (&wc, 0, sizeof(wc));
      wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_APPICON));
      wc.lpfnWndProc = CHouseViewWndProc;
      wc.style = CS_HREDRAW | CS_VREDRAW;
      wc.hInstance = ghInstance;
      wc.hbrBackground = NULL; //(HBRUSH)(COLOR_BTNFACE+1);
      wc.lpszClassName = "CHouseView";
      
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

   // Create the window
   char szTitle[256];
   MakeWindowText (DEFAULTRENDERSHARD, szTitle);
   m_hWnd = CreateWindowEx (WS_EX_APPWINDOW, "CHouseView", APPSHORTNAME,
      WS_POPUP | WS_VISIBLE,
      pmi->rWork.left, pmi->rWork.top, pmi->rWork.right - pmi->rWork.left, pmi->rWork.bottom - pmi->rWork.top,
      NULL, NULL, ghInstance, (LPVOID) this);
   if (!m_hWnd)
      return FALSE;

   UpdateAllButtons();
   // initialize with the world to receive notifications
   m_pWorld->NotifySocketAdd (this);

   // call the perspective cameras just to set it
   CPoint pCenter, pOffset;
   pCenter.Zero();
   pOffset.Zero();
   pOffset.p[2] = -CM_EYEHEIGHT;  // walking at 1.8 m
   m_apRender[0]->CameraPerspObject (&pOffset, &pCenter, 0, 0, 0, PI/2);
   pOffset.p[2] = CM_EYEHEIGHT;  // walking at 1.8 m
   m_apRender[0]->CameraPerspWalkthrough (&pOffset, 0, 0, 0, PI/2);

   // view from the top
   // world size
   CPoint b[2];
   fp fMax[3];
   pCenter.Zero();
   switch (m_dwViewWhat) {
      case VIEWWHAT_POLYMESH:
      case VIEWWHAT_BONE:
         PolyMeshBoundBox (b);
         break;

      case VIEWWHAT_OBJECT:
         m_pWorld->WorldBoundingBoxGet (&b[0], &b[1], FALSE);
         break;

      case VIEWWHAT_WORLD:
      default:
         m_pWorld->WorldBoundingBoxGet (&b[0], &b[1], TRUE);
         break;
   }
   DWORD x;
   for (x = 0; x < 3; x++) {
      fMax[x] = fabs(b[1].p[x] - b[0].p[x]) * 1.2; // so see just a little bit more
      fMax[x] = max(fMax[x], m_fViewScale / 2.0);
         // always show at least 20m for world view, 2 m for object view
   }

   pCenter.Add (&b[0], &b[1]);
   pCenter.Scale (.5);
   fp fScale;
   fScale = max(fMax[0], fMax[1]);
   m_apRender[0]->CameraFlat (&pCenter, 0, PI/2, 0, fScale, 0, 0);
   // BUGFIX - Take out not showing weather in OE because is now part of skydome
   //if (m_dwViewWhat == VIEWWHAT_OBJECT)
   //   m_apRender[0]->m_dwRenderShow &= (~RENDERSHOW_WEATHER); // dont show the weather in object editor

   // set up all the other cameras the same
   for (i = 1; i < VIEWTHUMB; i++) {
      m_apRender[0]->CloneTo (m_apRender[i]);

      switch (i) {
      case 2:  // from south
         m_apRender[i]->CameraFlat (&pCenter, 0, 0, 0, fScale, 0, 0);
         break;
      case 3:  // from east
         m_apRender[i]->CameraFlat (&pCenter, -PI/2, 0, 0, fScale, 0, 0);
         break;
      case 4:  // perspective
         CPoint pTranslate;

         switch (m_dwViewWhat) {
            case VIEWWHAT_POLYMESH:
            case VIEWWHAT_OBJECT:
            case VIEWWHAT_BONE:
               // BUGFIX - On object editing mode use model mode
               pTranslate.Zero();
               pTranslate.p[1] = fScale * tan((PI/2) / 2.0) / 2;
               pTranslate.p[2] = 0; // center on object
               m_apRender[i]->CameraPerspObject (&pTranslate, &pCenter, 0, 0, 0, PI/2);
               break;
            case VIEWWHAT_WORLD:
            default:
               // BUGFIX - Make this walkthrough instead of model
               pTranslate.Copy (&pCenter);
               pTranslate.p[1] -= fScale * tan((PI/2) / 2.0) / 2;
               pTranslate.p[2] = CM_EYEHEIGHT;  // walking at 1.8 m
               m_apRender[i]->CameraPerspWalkthrough (&pTranslate, 0, 0, 0, PI/2);
               break;
         }
         break;
      }
   }

   CameraPosition();
   UpdateCameraButtons ();

   // set window text
   char szaTemp[256];
   switch (m_dwViewWhat) {
      case VIEWWHAT_POLYMESH:
      case VIEWWHAT_BONE:
         {
            PWSTR pszName = pos->StringGet (OSSTRING_NAME);
            if (!pszName)
               pszName = L"Unknown";
            WideCharToMultiByte (CP_ACP, 0, pszName, -1, szaTemp, sizeof(szaTemp), 0, 0);

            switch (m_dwViewWhat) {
            default:
            case VIEWWHAT_POLYMESH:
               strcat (szaTemp, " - Polygon mesh");
               break;
            case VIEWWHAT_BONE:
               strcat (szaTemp, " - Bone");
               break;
            }
            SetWindowText (m_hWnd, szaTemp);
         }
         break;

      case VIEWWHAT_OBJECT:
         // set the name for the title
         WideCharToMultiByte (CP_ACP, 0, m_pWorld->NameGet(), -1, szaTemp, sizeof(szaTemp), 0, 0);
         strcat (szaTemp, " - Object editor");
         SetWindowText (m_hWnd, szaTemp);
         break;

      case VIEWWHAT_WORLD:
      default:
         SetWindowText (m_hWnd, szTitle);
         break;
   }

   return TRUE;
}

/***********************************************************************************
Clone - Clones the current house view object into a new one. It calls Init() and
   then sets all the parameters in the new one to be the same as the old one.

inputs
inputs
   DWORD    dwMonitor - Monitor number to 0, from 0+. If -1 then use a monitor not
               already in use, or the one least used.
returns
   PCHouseView - New house view object, 0 if cant create
*/
PCHouseView CHouseView::Clone (DWORD dwMonitor)
{
   PCHouseView phv;
   phv = new CHouseView (m_pWorld, m_pSceneSet, m_dwViewWhat);
   if (!phv)
      return NULL;


   phv->m_fWorldChanged = -1;
   phv->m_pWorld = m_pWorld;
   phv->m_pSceneSet = m_pSceneSet;
   phv->m_dwViewSub = m_dwViewSub;

   phv->m_dwThumbnailShow = m_dwThumbnailShow;
   phv->m_dwThumbnailCur = m_dwThumbnailCur;
   phv->m_fShowOnlyPolyMesh = m_fShowOnlyPolyMesh;
   // note: dont copy this phv->m_dwPolyMeshSelMode = m_dwPolyMeshSelMode;

   phv->m_fGridDisplay = m_fGridDisplay;
   phv->m_fGridDisplay2D = m_fGridDisplay2D;
   phv->m_fGrid = m_fGrid;
   phv->m_fGridAngle = m_fGridAngle;
   phv->m_pGridCenter.Copy (&m_pGridCenter);
   phv->m_pGridRotation.Copy (&m_pGridRotation);
   phv->m_mGridToWorld.Copy (&m_mGridToWorld);
   phv->m_mWorldToGrid.Copy (&m_mWorldToGrid);
   phv->m_pCenterWorld.Copy (&m_pCenterWorld);

   phv->m_dwImageScale = m_dwImageScale;
   phv->m_fSmallWindow = FALSE;  // when create a new window, always make it full size.
   phv->m_fHideButtons = FALSE;
   phv->m_dwQuality = m_dwQuality;
   phv->m_fVWWalkSpeed = m_fVWWalkSpeed;
   phv->m_fViewScale = m_fViewScale;
   phv->m_fVWalkAngle = m_fVWalkAngle;
   phv->m_fClipAngleValid = m_fClipAngleValid;
   phv->m_fSelRegionWhollyIn = m_fSelRegionWhollyIn;
   phv->m_fSelRegionVisible = m_fSelRegionVisible;
   phv->m_dwMoveReferenceCur = m_dwMoveReferenceCur;
   memcpy (phv->m_aScrollAction, m_aScrollAction, sizeof(m_aScrollAction));

   phv->m_lTraceInfo.Init (sizeof(TRACEINFO), m_lTraceInfo.Get(0), m_lTraceInfo.Num());

   if (!phv->Init ()) {
      delete phv;
      return NULL;
   }

   // copy over renderer stuff
   DWORD i;
   for (i = 0; i < VIEWTHUMB; i++) {
      m_apRender[i]->CloneTo (phv->m_apRender[i]);

      // BUGFIX - Had code to do CameraXXXGet from the original and then CameraXXXSet
      // to the new one just to make sure camera matrices transferred. Moved this
      // into RenderTraditional::CloneTo
   }

   phv->CameraPosition();
   phv->UpdateCameraButtons();

   // set the pointer mode and redrwa
   phv->SetPointerMode (m_adwPointerMode[0], 0);
   phv->SetPointerMode (m_adwPointerMode[1], 1);
   phv->SetPointerMode (m_adwPointerMode[2], 2);
   phv->RenderUpdate(WORLDC_CAMERAMOVED);

   // since creating a new window with exactly the same size is confusing, post
   // a message to reduce the size
   PostMessage (phv->m_hWnd, WM_COMMAND, IDC_SMALLWINDOW, 0);

   phv->CalcThumbnailLoc();

   return phv;
}



static PWSTR gpszGrid = L"Grid";
static PWSTR gpszGridDisplay = L"GridDisplay";
static PWSTR gpszGridDisplay2D = L"GridDisplay2D";
static PWSTR gpszGridAngle = L"GridAngle";
static PWSTR gpszGridCenter = L"GridCenter";
static PWSTR gpszGridRotation = L"GridRotation";
static PWSTR gpszGridToWorld = L"GridToWorld";
static PWSTR gpszWorldToGrid = L"WorldToGrid";
static PWSTR gpszImageScale = L"ImageScale";
static PWSTR gpszSmallWindow = L"SmallWindow";
static PWSTR gpszHideButtons = L"HideButtons";
static PWSTR gpszQuality = L"Quality";
static PWSTR gpszVWWalkSpeed = L"VWWalkSpeed";
static PWSTR gpszVWalkAngle = L"VWalkAngle";
static PWSTR gpszSelRegionWhollyIn = L"SelRegionWhollyIn";
static PWSTR gpszSelRegionVisible = L"SelRegionVisible";
static PWSTR gpszMoveReferenceCur = L"MoveReferneceCur";
static PWSTR gpszRender = L"Render";
static PWSTR gpszMonitor = L"Montor";
static PWSTR gpszWindowLeft = L"WindowLeft";
static PWSTR gpszWindowRight = L"WindowRight";
static PWSTR gpszWindowTop = L"WindowTop";
static PWSTR gpszWindowBottom = L"WindowBottom";
static PWSTR gpszThumbnailShow = L"ThumbnailShow";
static PWSTR gpszThumbnailCur = L"ThumbnailCur";
static PWSTR gpszViewScale = L"ViewScale";

/***********************************************************************************
CHouseView::MMLTo - Writes the view's information to MML.

returns
   PCMMLNode2   - Node with information
*/
PCMMLNode2 CHouseView::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszThumbnailCur, (int)m_dwThumbnailCur);
   MMLValueSet (pNode, gpszThumbnailShow, (int)m_dwThumbnailShow);
   MMLValueSet (pNode, gpszGrid, m_fGrid);
   MMLValueSet (pNode, gpszGridDisplay, (int) m_fGridDisplay);
   MMLValueSet (pNode, gpszGridDisplay2D, (int) m_fGridDisplay2D);
   MMLValueSet (pNode, gpszGridAngle, m_fGridAngle);
   MMLValueSet (pNode, gpszGridCenter, &m_pGridCenter);
   MMLValueSet (pNode, gpszGridRotation, &m_pGridRotation);
   MMLValueSet (pNode, gpszGridToWorld, &m_mGridToWorld);
   MMLValueSet (pNode, gpszWorldToGrid, &m_mWorldToGrid);
   MMLValueSet (pNode, gpszImageScale, (int) m_dwImageScale);
   MMLValueSet (pNode, gpszSmallWindow, (int) m_fSmallWindow);
   MMLValueSet (pNode, gpszHideButtons, (int) m_fHideButtons);
   MMLValueSet (pNode, gpszQuality, (int) m_dwQuality);
   MMLValueSet (pNode, gpszVWWalkSpeed, m_fVWWalkSpeed);
   MMLValueSet (pNode, gpszVWalkAngle, m_fVWalkAngle);
   MMLValueSet (pNode, gpszViewScale, m_fViewScale);
   MMLValueSet (pNode, gpszSelRegionWhollyIn, (int) m_fSelRegionWhollyIn);
   MMLValueSet (pNode, gpszSelRegionVisible, (int) m_fSelRegionVisible);
   MMLValueSet (pNode, gpszMoveReferenceCur, (int) m_dwMoveReferenceCur);
   // NOTE: Not bothering with m_fShowOnlyPolyMesh
   // NOTE: Not writing m_dwViewSub

   PCMMLNode2 pSub;
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < VIEWTHUMB; i++) {
      wcscpy (szTemp, gpszRender);
      if (i)
         swprintf (szTemp + wcslen(szTemp), L"%d", (int) i);
      pSub = m_apRender[i]->MMLTo ();
      if (pSub) {
         pSub->NameSet (szTemp);
         pNode->ContentAdd (pSub);
      }
   }

   // find the current display and location
   // get the monitor info
   HMONITOR hMon = MonitorFromWindow (m_hWnd, MONITOR_DEFAULTTONEAREST);
   FillXMONITORINFO ();
   PCListFixed pListXMONITORINFO;
   pListXMONITORINFO = ReturnXMONITORINFO();
   PXMONITORINFO p;
   for (i = 0; i < pListXMONITORINFO->Num(); i++) {
      p = (PXMONITORINFO) pListXMONITORINFO->Get(i);
      if (p->hMonitor == hMon)
         break;
   }
   RECT r;
   GetWindowRect (m_hWnd, &r);
   fp fLeft, fRight, fTop, fBottom;
   fLeft = (fp) (r.left - p->rWork.left) / (fp) (p->rWork.right - p->rWork.left);
   fRight = (fp) (r.right - p->rWork.left) / (fp) (p->rWork.right - p->rWork.left);
   fTop = (fp) (r.top - p->rWork.top) / (fp) (p->rWork.bottom - p->rWork.top);
   fBottom = (fp) (r.bottom - p->rWork.top) / (fp) (p->rWork.bottom - p->rWork.top);
   MMLValueSet (pNode, gpszMonitor, (int) i);
   MMLValueSet (pNode, gpszWindowLeft, fLeft);
   MMLValueSet (pNode, gpszWindowRight, fRight);
   MMLValueSet (pNode, gpszWindowTop, fTop);
   MMLValueSet (pNode, gpszWindowBottom, fBottom);

   return pNode;
}


/***********************************************************************************
CHouseView::MMLFrom - Reads in all the parameters from the MML. This is different
than most MMLFrom because it creates a new house view.

NOTE: This is a substitute for Init().

inputs
   PCMMLNode2      pNode - To read from
returns
   BOOL - TRUE if success
*/
BOOL CHouseView::MMLFrom (PCMMLNode2 pNode)
{
   m_fWorldChanged = -1;

   CPoint Zero;
   CMatrix Ident;
   Zero.Zero();
   Ident.Identity();

   m_dwThumbnailCur = (DWORD) MMLValueGetInt (pNode, gpszThumbnailCur, 1);
   m_dwThumbnailShow = (DWORD) MMLValueGetInt (pNode, gpszThumbnailShow, 0);
   m_fGridDisplay = (BOOL) MMLValueGetInt (pNode, gpszGridDisplay, (int) m_fGridDisplay);
   m_fGridDisplay2D = (BOOL) MMLValueGetInt (pNode, gpszGridDisplay2D, (int) m_fGridDisplay2D);
   m_fGrid = MMLValueGetDouble (pNode, gpszGrid, m_fGrid);
   m_fGridAngle = MMLValueGetDouble (pNode, gpszGridAngle, (int) m_fGridAngle);
   MMLValueGetPoint (pNode, gpszGridCenter, &m_pGridCenter, &Zero);
   MMLValueGetPoint (pNode, gpszGridRotation, &m_pGridRotation, &Zero);
   MMLValueGetMatrix (pNode, gpszGridToWorld, &m_mGridToWorld, &Ident);
   MMLValueGetMatrix (pNode, gpszWorldToGrid, &m_mWorldToGrid, &Ident);

   m_dwImageScale = (DWORD) MMLValueGetInt (pNode, gpszImageScale, (int) m_dwImageScale);
   m_fSmallWindow = (BOOL) MMLValueGetInt (pNode, gpszSmallWindow, (int) m_fSmallWindow);
   m_fHideButtons = (BOOL) MMLValueGetInt (pNode, gpszHideButtons, (int) m_fHideButtons);
   m_dwQuality = (DWORD) MMLValueGetInt (pNode, gpszQuality, (int) m_dwQuality);

   m_fViewScale = MMLValueGetDouble (pNode, gpszViewScale, 40);
   m_fVWWalkSpeed = MMLValueGetDouble (pNode, gpszVWWalkSpeed, m_fVWWalkSpeed);
   m_fVWalkAngle = MMLValueGetDouble (pNode, gpszVWalkAngle, m_fVWalkAngle);
   m_fSelRegionWhollyIn = (BOOL) MMLValueGetInt (pNode, gpszSelRegionWhollyIn, (int) m_fSelRegionWhollyIn);
   m_fSelRegionVisible = (BOOL) MMLValueGetInt (pNode, gpszSelRegionVisible, (int) m_fSelRegionVisible);
   m_dwMoveReferenceCur = (DWORD) MMLValueGetInt (pNode, gpszMoveReferenceCur, (int) m_dwMoveReferenceCur);
   ScrollActionInit (); // so set based on view scale


   if (!Init ()) {
      return FALSE;
   }

   DWORD i;
   for (i = 0; pNode->ContentNum(); i++) {
      PWSTR psz;
      PCMMLNode2 pSub;
      pSub = NULL;
      if (!pNode->ContentEnum (i, &psz, &pSub))
         break;
      if (!pSub)
         continue;
      
      psz = pSub->NameGet();
      if (!psz)
         continue;

      DWORD j;
      WCHAR szTemp[64];
      for (j = 0; j < VIEWTHUMB; j++) {
         wcscpy (szTemp, gpszRender);
         if (j)
            swprintf (szTemp + wcslen(szTemp), L"%d", (int)j);
         if (!_wcsicmp(psz, szTemp))
            m_apRender[j]->MMLFrom (pSub);
      }
   }

   // find the current display and location
   // get the monitor info
   fp fLeft, fRight, fTop, fBottom;
   i = (int) MMLValueGetInt (pNode, gpszMonitor, 0);
   fLeft = MMLValueGetDouble (pNode, gpszWindowLeft, 0);
   fRight = MMLValueGetDouble (pNode, gpszWindowRight, 1);
   fTop = MMLValueGetDouble (pNode, gpszWindowTop, 0);
   fBottom = MMLValueGetDouble (pNode, gpszWindowBottom, 1);
   LONG lStyle;
   //lEx = GetWindowLong (hWnd, GWL_EXSTYLE);
   lStyle = GetWindowLong (m_hWnd, GWL_STYLE);
   FillXMONITORINFO ();
   PCListFixed pListXMONITORINFO;
   pListXMONITORINFO = ReturnXMONITORINFO();
   PXMONITORINFO p;
   p = (PXMONITORINFO) pListXMONITORINFO->Get(i % pListXMONITORINFO->Num());

   RECT r;
   if (m_fSmallWindow) {
      r.left = (int) (fLeft * (p->rWork.right - p->rWork.left)) + p->rWork.left;
      r.right = (int) (fRight * (p->rWork.right - p->rWork.left)) + p->rWork.left;
      r.top = (int) (fTop * (p->rWork.bottom - p->rWork.top)) + p->rWork.top;
      r.bottom = (int) (fBottom * (p->rWork.bottom - p->rWork.top)) + p->rWork.top;

      //lEx |= WS_EX_PALETTEWINDOW;
      lStyle |= WS_SIZEBOX | WS_CAPTION | WS_SYSMENU;
      //SetWindowLong (hWnd, GWL_EXSTYLE, lEx);
      SetWindowLong (m_hWnd, GWL_STYLE, lStyle);

      SetWindowPos (m_hWnd, (HWND)HWND_TOPMOST, r.left, r.top,
         r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED);
   }
   else {
      //lEx &= ~(WS_EX_PALETTEWINDOW);
      lStyle &= ~(WS_SIZEBOX | WS_CAPTION | WS_SYSMENU);
      //SetWindowLong (hWnd, GWL_EXSTYLE, lEx);
      SetWindowLong (m_hWnd, GWL_STYLE, lStyle);

      SetWindowPos (m_hWnd, (HWND)HWND_NOTOPMOST, p->rWork.left, p->rWork.top,
         p->rWork.right - p->rWork.left, p->rWork.bottom - p->rWork.top, SWP_FRAMECHANGED);
   }

   for (i = 0; i < VIEWTHUMB; i++) {
      CPoint pP, pCenter;
      fp fX, fY, fZ, fFOV;
      fp fLong, fLat;
      fp fLongitude, fTilt, fTiltY, fScale, fTransX, fTransY;
      switch (m_apRender[i]->CameraModelGet()) {
      case CAMERAMODEL_FLAT:
         m_apRender[i]->CameraFlatGet (&pCenter, &fLongitude, &fTilt, &fTiltY, &fScale, &fTransX, &fTransY);
         m_apRender[i]->CameraFlat (&pCenter, fLongitude, fTilt, fTiltY, fScale, fTransX, fTransY);
         break;

      case CAMERAMODEL_PERSPOBJECT:
         m_apRender[i]->CameraPerspObjectGet (&pP, &pCenter, &fZ, &fX, &fY, &fFOV);
         m_apRender[i]->CameraPerspObject (&pP, &pCenter, fZ, fX, fY, fFOV);
         break;

      case CAMERAMODEL_PERSPWALKTHROUGH:
         m_apRender[i]->CameraPerspWalkthroughGet (&pP, &fLong, &fLat, &fTilt, &fFOV);
         m_apRender[i]->CameraPerspWalkthrough (&pP, fLong, fLat, fTilt, fFOV);
         break;
      }
   }

   CameraPosition();
   UpdateCameraButtons();

   // update the quality since may have saved in different quality
   QualityApply ();
   CalcThumbnailLoc();

   // set the pointer mode and redrwa
   SetPointerMode (m_adwPointerMode[0], 0);
   // dont need to set the other pointer modes - I dont think
   RenderUpdate(WORLDC_NEEDTOREDRAW);


   return TRUE;
}

/***********************************************************************************
CHouseView::UpdateAllButtons - Called when the user has progressed so far in the
tutorial, this first erases all the buttons there, and then updates them
to the appropriate display level.

inputs
*/
void CHouseView::UpdateAllButtons (void)
{
   // clear out all the buttons
   m_pbarGeneral->Clear();
   m_pbarView->Clear();
   m_pbarMisc->Clear();
   m_pbarObject->Clear();
   m_pRedo = m_pUndo = m_pPaste = NULL;
   m_pObjControlNSEW = m_pObjControlUD = m_pObjControlNSEWUD = m_pObjControlInOut = NULL;
   m_listNeedSelect.Clear();
   m_listNeedSelectNoEmbed.Clear();
   m_listNeedSelectOnlyOneEmbed.Clear();
   m_listNeedSelectOneEmbed.Clear();

   // general buttons
   PCIconButton pi;
   switch (m_dwViewWhat) {
      default:
      case VIEWWHAT_WORLD:
         pi = m_pbarGeneral->ButtonAdd (1, IDB_NEW, ghInstance, "Ctrl-N", "New...",
            "Creates a new " APPSHORTNAME " file.", IDC_NEW);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_OPEN, ghInstance, "Ctrl-O", "Open...",
            "Opens a " APPSHORTNAME " file.", IDC_OPEN);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_SAVE, ghInstance, "Ctrl-S", "Save", "Saves the file.", IDC_SAVE);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_SAVEAS, ghInstance, NULL, "Save as...", "Saves to a different file.", IDC_SAVEAS);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_LOCALE,ghInstance, 
            NULL, "Location settings",
            "Before you do any work with " APPSHORTNAME ", click this and provide some infromation "
            "about the building site.", IDC_LOCALE);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_DIALOGBOX,ghInstance, 
            NULL, "General settings...",
            "General application settings such as whether to use metric or Enlgish measurement.",
            IDC_APPSET);
         break;

      case VIEWWHAT_BONE:
         // nothing
         break;

      case VIEWWHAT_OBJECT:
         pi = m_pbarGeneral->ButtonAdd (1, IDB_OBJEDITOR,ghInstance, 
            NULL, "Object settings",
            "Before you add to your new object you should provide some information "
            "about it in this dialog.", IDC_OBJEDITOR);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_SAVE, ghInstance, "Ctrl-S", "Save", "Saves the object.", IDC_SAVE);
         break;

      case VIEWWHAT_POLYMESH:
         pi = m_pbarGeneral->ButtonAdd (1, IDB_PMMODEPOLY, ghInstance, NULL, "Polygon editing",
            "In this mode you can edit indivual polygons within the polygon mesh.",
            IDC_PMMODEPOLY);
         pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_TLOOP);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_PMMODECLAY, ghInstance, NULL, "Organic editing",
            "You can modify the mesh like you would a piece of clay.",
            IDC_PMMODECLAY);
         pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_VLOOP);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_PMMODEMORPH, ghInstance, NULL, "Morph editing",
            "This mode lets you modify a polygon mesh's morphs, such as creating "
            "different shapes for lips.",
            IDC_PMMODEMORPH);
         pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_VLOOP);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_PMMODETEXTURE, ghInstance, NULL, "Texture editing",
            "This mode lets you control how textures will be applied to the polygon mesh.",
            IDC_PMMODETEXTURE);
         pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_VLOOP);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_PMMODEBONE, ghInstance, NULL, "Bone association",
            "Use this to connect the polygon mesh to a skeleton.",
            IDC_PMMODEBONE);
         pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_VLOOP);
         pi = m_pbarGeneral->ButtonAdd (1, IDB_PMMODETAILOR, ghInstance, NULL, "Cut out clothes",
            "This mode lets you cut out portions of the polygon mesh (of a person) "
            "that will then become a new clothing polygon mesh.",
            IDC_PMMODETAILOR);
         pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_BLOOP);

         // Will need to set the modify mode highlight based on ccurent mode
         DWORD dwMode;
         switch (m_dwViewSub) {
         case VSPOLYMODE_POLYEDIT:  // poly
            dwMode = IDC_PMMODEPOLY;
            break;
         case VSPOLYMODE_ORGANIC:  // organic
            dwMode = IDC_PMMODECLAY;
            break;
         case VSPOLYMODE_BONE:  // bone
            dwMode = IDC_PMMODEBONE;
            break;
         case VSPOLYMODE_MORPH:  // morph
            dwMode = IDC_PMMODEMORPH;
            break;
         case VSPOLYMODE_TEXTURE:  // texture
            dwMode = IDC_PMMODETEXTURE;
            break;
         case VSPOLYMODE_TAILOR:  // tailor
            dwMode = IDC_PMMODETAILOR;
            break;
         }
         m_pbarGeneral->FlagsSet (dwMode, IBFLAG_BLUELIGHT, 1);

         pi = m_pbarGeneral->ButtonAdd (1, IDB_DIALOGBOX, ghInstance, 
            NULL, "General settings...",
            "General polygon mesh settings, such as the number of subdivisions to use.",
            IDC_PMDIALOG);
         break;
   }


   pi = m_pbarGeneral->ButtonAdd (2, IDB_HELP, ghInstance, "F1", "Help...", "Brings up help and documentation.", IDC_HELPBUTTON);

   if ((m_dwViewWhat != VIEWWHAT_POLYMESH) && (m_dwViewWhat != VIEWWHAT_BONE))
      m_pbarGeneral->ButtonAdd (2, IDB_OBJECTLIST, ghInstance, NULL, "Object list", "Shows a list of objects in the world.", IDC_OBJECTLIST);

   if (m_dwViewWhat == VIEWWHAT_WORLD) {
      if (CanModifyLibrary()) {
         pi = m_pbarGeneral->ButtonAdd (2, IDB_LIBRARY, ghInstance, NULL, "Library",
            "Allows you to add new textures and objects to your library.",
            IDC_LIBRARY);
      }

      if (m_pSceneSet) {
         pi = m_pbarGeneral->ButtonAdd (2, IDB_ANIM, ghInstance, NULL, "Animation",
            "Animation features are controlled from here.",
            IDC_ANIM);
      }

      pi = m_pbarGeneral->ButtonAdd (2, IDB_PRINTER, ghInstance, NULL, "Print or save image...",
         "Print a copy of the image you're looking at, or saves it to disk.", IDC_PRINT);
   }

   m_pbarGeneral->ButtonAdd (0, IDB_CLOSE, ghInstance, NULL, "Close",
      "Close this view.<p/>"
      "If you hold the control key down then " APPSHORTNAME " will shut down.", IDC_CLOSE);
   m_pbarGeneral->ButtonAdd (0, IDB_MINIMIZE, ghInstance, NULL, "Minimize", "Minimize the window.", IDC_MINIMIZE);
   m_pbarGeneral->ButtonAdd (0, IDB_SMALLWINDOW, ghInstance, NULL, "Shrink/expand window",
      "Shrinks a window taking up the entire screen into a small, draggable window. The "
      "opposite for small windows.<p/>"
      "Tip: Right-click in the small window to bring up the menu.", IDC_SMALLWINDOW);
   m_pbarGeneral->ButtonAdd (0, IDB_NEXTMONITOR, ghInstance, NULL, "Next monitor", "Moves the view to the next monitor.", IDC_NEXTMONITOR);

   if (m_dwViewWhat == VIEWWHAT_WORLD)
      m_pbarGeneral->ButtonAdd (0, IDB_CLONEVIEW, ghInstance, NULL, "New view", "Create a new view using the same settings as this view.", IDC_CLONEVIEW);

   m_pbarGeneral->ButtonAdd (0, IDB_PAINTVIEW, ghInstance, NULL,
      "Painting view",
      "Use this to paint textures directly onto your object surfaces.",
      IDC_PAINTVIEW);

   // miscellaneous buttons
   m_pRedo = pi = m_pbarMisc->ButtonAdd (2, IDB_REDO, ghInstance, "Ctrl-Y", "Redo", "Redo the last undo.", IDC_REDOBUTTON);
   m_pUndo = pi = m_pbarMisc->ButtonAdd (2, IDB_UNDO, ghInstance, "Ctrl-Z", "Undo", "Undo the last change.", IDC_UNDOBUTTON);

   if ((m_dwViewWhat != VIEWWHAT_POLYMESH) && (m_dwViewWhat != VIEWWHAT_BONE)) {
      pi = m_pbarMisc->ButtonAdd (2, IDB_DELETE, ghInstance, "Del", "Delete", "Delete the selection.", IDC_DELETEBUTTON);
      pi->Enable(FALSE);
      m_listNeedSelect.Add (&pi);   // enable this if have a selection

      m_pPaste = pi = m_pbarMisc->ButtonAdd (2, IDB_PASTE, ghInstance, "Ctrl-V", "Paste", "Paste from the clipboard.", IDC_PASTEBUTTON);
      // enable paste button based on clipboard
      ClipboardUpdatePasteButton ();

      pi = m_pbarMisc->ButtonAdd (2, IDB_COPY, ghInstance, "Ctrl-C", "Copy", "Copy the selection to the clipboard.", IDC_COPYBUTTON);
      pi->Enable(FALSE);
      m_listNeedSelect.Add (&pi);   // enable this if have a selection
      pi = m_pbarMisc->ButtonAdd (2, IDB_CUT, ghInstance, "Ctrl-X", "Cut", "Cut the selection to the clipboard.", IDC_CUTBUTTON);
      pi->Enable(FALSE);
      m_listNeedSelect.Add (&pi);   // enable this if have a selection
   }

   // view choices
   pi = m_pbarView->ButtonAdd (0, IDB_VIEWFLAT, ghInstance, "F2", "Flattened view",
      "Shows an isotropic view of the building.", IDC_VIEWFLAT);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_LLOOP);
   pi = m_pbarView->ButtonAdd (0, IDB_VIEW3D, ghInstance, "F3", "Model view",
      "Shows a model view, treating the house as a model that can be rotated around.", IDC_VIEW3D);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_CLOOP);
   pi = m_pbarView->ButtonAdd (0, IDB_VIEWWALKTHROUGH, ghInstance, "F4", "Walkthrough view",
      "Shows a view of the house simulating a walkthrough.", IDC_VIEWWALKTHROUGH);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_RLOOP);
   pi = m_pbarView->ButtonAdd (0, IDB_THUMBNAIL,ghInstance, 
      NULL, "Thumbail",
      "Click on this button and select the thumbnail layout so you can see "
      "the object from several directions at once."
      "<p/>"
      "If you select \"Setup thumbnails\" you can click on an object and all "
      "four thumbnails will automatically be created to view the object.",
      IDC_THUMBNAIL);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
   pi = m_pbarView->ButtonAdd (0, IDB_VIEWQUALITY,ghInstance, 
      NULL, "Image quality...",
      "Pressing this will display a drop down letting you trade off image quality "
      "for drawing speed. If you press control while selecting an item you'll "
      "be able to customize the settings.", IDC_VIEWQUALITY);
   pi = m_pbarView->ButtonAdd (0, IDB_DIALOGBOX, ghInstance, NULL, "View settings...",
      "Lets you adjust the lighting and other settings.", IDC_VIEWSETTINGS);

   // clipping
   if ((m_dwViewWhat != VIEWWHAT_POLYMESH) && (m_dwViewWhat != VIEWWHAT_BONE)) {
      pi = m_pbarView->ButtonAdd (2, IDB_CLIPSET,ghInstance, 
         NULL, "Clipping settings...",
         "Show/hide types of objects and hand-enter the clipping plane."
         , IDC_CLIPSET);
   }
   pi = m_pbarView->ButtonAdd (2, IDB_CLIPPOINT,ghInstance, 
      NULL, "Clip based on a point",
      "Use this to visually slice away part of the house and "
      "see what's inside. "
      "Click on a point on the screen; a dialog box will appear asking you how "
      "you want the clipping areao oriented."
      , IDC_CLIPPOINT);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
   pi = m_pbarView->ButtonAdd (2, IDB_CLIPLINE,ghInstance, 
      NULL, "Clip to a line",
      "Use this to visually slice away part of the house and "
      "see what's inside. "
      "Click on a point on the screen and drag that mouse to specify what angle "
      "to use for clipping. "
      "A single click will clear the clipping area."
      "<p/>"
      "If you hold CONTROL down when you lift the mouse button the "
      "camera will NOT be moved perpendicular to the clipping plane.", IDC_CLIPLINE);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
   if ((m_dwViewWhat != VIEWWHAT_POLYMESH) && (m_dwViewWhat != VIEWWHAT_BONE)) {
      pi = m_pbarView->ButtonAdd (2, IDB_CLIPLEVEL,ghInstance, 
         NULL, "View a level...",
         "Cuts off the roof and everything above the floor.", IDC_CLIPLEVEL);
   }
   if ((m_dwViewWhat == VIEWWHAT_POLYMESH) || (m_dwViewWhat == VIEWWHAT_BONE)) {
      pi = m_pbarView->ButtonAdd (2, IDB_PMSHOWBACKGROUND,ghInstance, 
         NULL, "Show/hide other objects",
         "Pressing this alternatively shows or hides the objects in the world "
         "other than the one being modified.", IDC_PMSHOWBACKGROUND);
   }

   // grid
   pi = m_pbarMisc->ButtonAdd (0, IDB_GRIDSELECT, ghInstance, NULL, "Grid size",
      "Clicking this will bring up a menu that lets you choose the grid's size.",
      IDC_GRIDSELECT);
   pi = m_pbarMisc->ButtonAdd (0, IDB_GRIDFROMPOINT, ghInstance, NULL, "Grid from a point",
      "With this tool, click in a surface. You'll be able to use the point where "
      "you clicked as the center-point of the grid or to control the grid's orientation.",
      IDC_GRIDFROMPOINT);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
   pi = m_pbarMisc->ButtonAdd (0, IDB_TRACING, ghInstance, NULL, "Tracing paper...",
      "Overlay scaned images of plans or photographs of the object so "
      "you can trace it into the model.", IDC_TRACING);
   pi = m_pbarMisc->ButtonAdd (0, IDB_DIALOGBOX, ghInstance, NULL, "Grid settings...",
      "Show/hide the grid.", IDC_GRIDSETTINGS);
   if ((m_dwViewWhat == VIEWWHAT_POLYMESH) && (m_dwViewSub == VSPOLYMODE_TEXTURE)) {  // display in texture editing
      pi = m_pbarMisc->ButtonAdd (0, IDB_PMTEXTTOGGLE, ghInstance, 
         NULL, "Show/hide flattened texture mapping",
         "Pressing this alternatively shows or hides the flattened texture mapping. "
         "The texture mapping shows you how the surface's texture is flattened onto "
         "the texture. Blue lines represent sides, while organge lines show "
         "the texture's edges."
         , IDC_PMTEXTTOGGLE);
   }

   // selection - if it's a polymesh with viewsub > 0 then dont allow selection
   if (m_dwViewWhat == VIEWWHAT_BONE) {
      pi = m_pbarMisc->ButtonAdd (1, IDB_SELINDIVIDUAL,ghInstance, 
         "S", "Select bone",
         "Click on an bone to select it so that the bone's envelope will be shown. "
         "If you click off a bone then the selection will be cleared."
         , IDC_SELINDIVIDUAL);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
   }
   else if (!((m_dwViewWhat == VIEWWHAT_POLYMESH) && ((m_dwViewSub > VSPOLYMODE_POLYEDIT) && (m_dwViewSub != VSPOLYMODE_TEXTURE)))) {
      pi = m_pbarMisc->ButtonAdd (1, IDB_SELINDIVIDUAL,ghInstance, 
         "S", "Select individual objects",
         "Click on an object to select it. To select more than one object hold either "
         "the control or shift key down while clicking on objects."
         , IDC_SELINDIVIDUAL);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
      pi = m_pbarMisc->ButtonAdd (1, IDB_SELREGION,ghInstance, 
         NULL, "Select multiple objects",
         "Click and drag over a region to select all the objects within the region. "
         "To add to the selection hold down the shift or control key while dragging. "
         "This tool's behavior is affected by Selection Settings.",
         IDC_SELREGION);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
      pi = m_pbarMisc->ButtonAdd (1, IDB_SELALL,ghInstance, 
         "Ctrl-A", "Select all objects",
         "Selects all the objects in the world. "
         "Press Shift and click to select all asymmetrical objects."
         , IDC_SELALL);
      pi = m_pbarMisc->ButtonAdd (1, IDB_SELNONE,ghInstance, 
         "0", "Clear selection",
         "De-selects all selected objects."
         , IDC_SELNONE);
      pi = m_pbarMisc->ButtonAdd (1, IDB_DIALOGBOX,ghInstance, 
         NULL, "Selection settings...",
         "Use this to control how selection behaves."
         , IDC_SELSET);
   }

   switch (m_dwViewWhat) {
   case VIEWWHAT_BONE:
      pi = m_pbarObject->ButtonAdd (0, IDB_BONENEW, ghInstance, 
         NULL, "Create a new bone",
         "Click on the end of a bone and drag to create the new bone."
         , IDC_BONENEW);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (0, IDB_OBJDECONSTRUCT,ghInstance, 
         NULL, "Disconnect",
         "Click on a bone to disconnect it from the bone it's attached to. "
         "The bone may appear to disappear from the bone editor unless you "
         "turn on \"Show/hide other objects\" because it will no longer be "
         "part of this bone object."
         , IDC_BONEDISCONNECT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (0, IDB_OBJMERGE, ghInstance, 
         NULL, "Merge object",
         "Click on the bone object that you wish to merge with the bone "
         "you're editing. You may need to turn on \"Show/hide other objects\" so "
         "you can see the other bones."
         , IDC_BONEMERGE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (0, IDB_OBJCONTROLDIALOG,ghInstance, 
         NULL, "Edit bone",
         "Select this tool and click on a bone to bring up a dialog that lets "
         "you modify it."
         , IDC_BONEEDIT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (0, IDB_OBJATTRIB,ghInstance, 
         NULL, "Attributes",
         "Click on a object to change its attributes, such as opening or closing it, "
         "or turning it on. If an object has a large number, clicking "
         "close to the source of the attribute (such as the joint in a skeleton)."
         , IDC_OBJATTRIB);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (0, IDB_BONEDEFAULT, ghInstance, 
         NULL, "Return all bones to default position",
         "Pressing this will restore all bones to their original position. "
         "Some tools will only work when the bones are in their default "
         "position."
         , IDC_BONEDEFAULT);


      pi = m_pbarObject->ButtonAdd (1, IDB_BONEROTATE, ghInstance, 
         NULL, "Rotate the bone",
         "Click on a bone and drag to the right to rotate the bone so that it "
         "hinges in a different direction. Each of the four sides of the bones "
         "has a different color so you can tell how much you've rotated. "
         "The blue side is the top of the hinge."
         , IDC_BONEROTATE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_PMMOVESCALE, ghInstance, 
         NULL, "Scale the bone",
         "Click on a bone and drag to the right to scale the bone and its children. "
         "To scale all the bones hold the control key down when you click and drag."
         "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit scaling to EW, NS, or UD."
         , IDC_BONESCALE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_PMEDGESPLIT, ghInstance, 
         NULL, "Split the bone",
         "Click on a bone to split it in two."
         , IDC_BONESPLIT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_GRAPHPOINTDELETE, ghInstance, 
         NULL, "Delete the bone",
         "Click on a bone to delete it."
         , IDC_BONEDELETE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarMisc->ButtonAdd (1, IDB_PMMIRROR,ghInstance, 
         NULL, "Automatic mirroring",
         "Use this to turn on/off automatic mirroring of the bone while "
         "you edit it."
         , IDC_BONEMIRROR);
      break;
   case VIEWWHAT_POLYMESH:
      pi = m_pbarMisc->ButtonAdd (1, IDB_PMMIRROR,ghInstance, 
         NULL, "Automatic mirroring",
         "Use this to turn on/off automatic mirroring of the polygon mesh while "
         "you edit it."
         , IDC_PMMIRROR);

      if (m_dwViewSub == VSPOLYMODE_ORGANIC) {
         pi = m_pbarMisc->ButtonAdd (1, IDB_PMSTENCIL, ghInstance, 
            NULL, "Stenciling",
            "If you turn stencilling on then the texture of your surface will affect "
            "the strength of organic height painting or magnet deformations. The "
            "lighter the texture is on the vertex, the greater the effect of the tool."
            , IDC_PMSTENCIL);

      }

      if ((m_dwViewSub == VSPOLYMODE_POLYEDIT) || (m_dwViewSub == VSPOLYMODE_TEXTURE)) {
         pi = m_pbarObject->ButtonAdd (0, IDB_PMEDITVERT, ghInstance, "F6", "Vertex manipulation",
            "Click this to show tools that modify the vertices of the polygon mesh.", IDC_PMEDITVERT);
         pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_LLOOP);
         pi = m_pbarObject->ButtonAdd (0, IDB_PMEDITEDGE, ghInstance, "F7", "Edge maniptulation",
            "Click this to show tools that modify the edges of the polygon mesh.",
            IDC_PMEDITEDGE);
         pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_CLOOP);
         pi = m_pbarObject->ButtonAdd (0, IDB_PMEDITSIDE, ghInstance, "F8", "Side manipulation",
            "Click this to show tools that modify the side of the polygon mesh.", IDC_PMEDITSIDE);
         pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_RLOOP);
      }
      else if (m_dwViewSub == VSPOLYMODE_ORGANIC) {
         pi = m_pbarObject->ButtonAdd (0, IDB_PMMOVESCALE, ghInstance, 
            NULL, "Scale the entire object",
            "Click on the object and drag to scale the entire object. "
            "If you have a stencil on then only the stenciled areas will be scaled."
            "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit scaling to EW, NS, or UD."
            , IDC_PMORGSCALE);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarObject->ButtonAdd (0, IDB_OBJMERGE, ghInstance, 
            NULL, "Merge object",
            "Click on the polygon mesh object that you wish to merge with the polygon "
            "mesh you're editing. You may need to turn on \"Show/hide other objects\" so "
            "you can see the other polygon meshes."
            , IDC_PMMERGE);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarObject->ButtonAdd (0, IDB_PMEXTRA, ghInstance, 
            NULL, "Extra effects",
            "This will display a menu of additional effects that allow you "
            "to flip the object or add more detail to it."
            , IDC_PMEXTRA);
      }
      else if (m_dwViewSub == VSPOLYMODE_MORPH) {
         pi = m_pbarObject->ButtonAdd (0, IDB_PMMOVESCALE, ghInstance, 
            NULL, "Scale the entire morph",
            "Click on the object and drag to scales the active morphs. You must have at least "
            "one morph active to scale."
            "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit scaling to EW, NS, or UD."
            , IDC_PMMORPHSCALE);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      }
      else if (m_dwViewSub == VSPOLYMODE_TAILOR) {
         pi = m_pbarObject->ButtonAdd (0, IDB_PMTAILORNEW, ghInstance, 
            NULL, "Create a new article of clothing",
            "Use this tool after you have selected the region of the character you wish to clothe using "
            "the brushes."
            "<p/>"
            "Click on the character and drag the mouse to the right to enlarge "
            "the clothing so it no longer touches the character's skin. Let go and a new "
            "clothing object will be created. You will need to pull up a new polygon mesh "
            "editing window to modify it."
            , IDC_PMTAILORNEW);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      }
      else if (m_dwViewSub == VSPOLYMODE_BONE) {
         pi = m_pbarObject->ButtonAdd (0, IDB_PMEXTRA, ghInstance, 
            NULL, "Extra effects",
            "This will display a menu of additional effects."
            , IDC_PMEXTRA);
      }

      if (m_dwViewSub == VSPOLYMODE_TEXTURE) {
         pi = m_pbarObject->ButtonAdd (0, IDB_OBJPAINT,ghInstance, 
            NULL, "Change object's color or texture",
            "Clicking on an object will bring up a dialog box asking you "
            "what color or texture you want to use for the object."
            "<p/>"
            "If you press CONTROL while clicking on an object, the dialog will "
            "be skipped and the color set to the last one you used."
            "<p/>"
            "If you press SHIFT while clicking on a wall, just the wall/floor visible in the room will be painted. "
            "Otherwise, the entire wall/floor will be painted."
            , IDC_OBJPAINT);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      }

      if ((m_dwViewSub >= VSPOLYMODE_ORGANIC) && (m_dwViewSub != VSPOLYMODE_TEXTURE)) {
         if (m_dwViewSub != VSPOLYMODE_TAILOR) // dont need shape with clothing
            pi = m_pbarObject->ButtonAdd (1, IDB_BRUSHSHAPE, ghInstance, NULL, "Brush shape",
               "This menu lets you chose the shape of the brush from pointy to flat.",
               IDC_BRUSHSHAPE);

         pi = m_pbarObject->ButtonAdd (1, IDB_BRUSHEFFECT, ghInstance, NULL, "Brush effect",
            "This menu lets you change what the brush does, such as increasing or decreasing elevation.",
            IDC_BRUSHEFFECT);

         if (m_dwViewSub != VSPOLYMODE_TAILOR) // dont need strength with clothing
            pi = m_pbarObject->ButtonAdd (1, IDB_BRUSHSTRENGTH, ghInstance, NULL, "Brush strength",
               "This menu lets you control how large the brush's effect is.",
               IDC_BRUSHSTRENGTH);

         pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH4,ghInstance, 
            "1", "Brush size (extra small)",
            "This tool lets you paint. "
            "If you hold down the CONTROL key the brush will have the opposite effect to what you chose.",
            IDC_BRUSH4);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH8,ghInstance, 
            "2", "Brush size (small)",
            "This tool lets you paint. "
            "If you hold down the CONTROL key the brush will have the opposite effect to what you chose.",
            IDC_BRUSH8);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH16,ghInstance, 
            "3", "Brush size (medium)",
            "This tool lets you paint. "
            "If you hold down the CONTROL key the brush will have the opposite effect to what you chose.",
            IDC_BRUSH16);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH32,ghInstance, 
            "4", "Brush size (large)",
            "This tool lets you paint. "
            "If you hold down the CONTROL key the brush will have the opposite effect to what you chose.",
            IDC_BRUSH32);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH64,ghInstance, 
            "5", "Brush size (extra large)",
            "This tool lets you paint. "
            "If you hold down the CONTROL key the brush will have the opposite effect to what you chose.",
            IDC_BRUSH64);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      }  // viewsub >= 1


      if (m_dwViewSub == VSPOLYMODE_ORGANIC) {
         // only provide magnets in organic deform mode
         pi = m_pbarObject->ButtonAdd (2, IDB_PMMAGSIZE, ghInstance, 
            NULL, "Magnetic size and shape",
            "This menu lets you control the size and shape of the magnetic tools."
            , IDC_PMMAGSIZE);

         pi = m_pbarObject->ButtonAdd (2, IDB_PMMAGPINCH, ghInstance, 
            NULL, "Magnetic pinch",
            "Click and drag the object to pinch a group of points together."
            , IDC_PMMAGPINCH);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarObject->ButtonAdd (2, IDB_PMMAGNORM, ghInstance, 
            NULL, "Magnetic move along normal",
            "Click on the object and drag to pull a group of points in or out, "
            "perpendicular to the surface."
            , IDC_PMMAGNORM);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarObject->ButtonAdd (2, IDB_PMMAGVIEWER, ghInstance, 
            NULL, "Magnetic move forwards/backwards",
            "Click on the object and drag to push or pull a group of points in or out."
            "<p/>"
            "Hold down the 'x', 'y', or 'z' key while moving to limit scaling to EW, NS, or UD."
            , IDC_PMMAGVIEWER);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarObject->ButtonAdd (2, IDB_PMMAGANY, ghInstance, 
            NULL, "Magnetic move in any direction",
            "Click on the object and drag to pull a group of points in any direction."
            "<p/>"
            "Hold down the 'x', 'y', or 'z' key while moving to limit scaling to EW, NS, or UD."
            , IDC_PMMAGANY);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);


      }
      break;

   case VIEWWHAT_OBJECT:
   case VIEWWHAT_WORLD:
   default:
      // move buttons
      pi = m_pbarObject->ButtonAdd (1, IDB_MOVENSEWUD,ghInstance, 
         "Alt-M", "Move object in any direction",
         "Click on an object and hold the mouse button down while moving the mouse. "
         "This will move the object in any direction, north, south, east, west, up, and down."
         "<p/>Press 'Ctrl' when you first click with a mouse to duplicate the selected objects before moving them."
         "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit movement to EW, NS, or UD."
         , IDC_MOVENSEWUD);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_listNeedSelectNoEmbed.Add (&pi);   // enable this if have a selection
      pi = m_pbarObject->ButtonAdd (1, IDB_MOVENSEW,ghInstance, 
         NULL, "Move object NSEW",
         "Click on an object and hold the mouse button down while moving the mouse. "
         "This will move the object NSEW. It will NOT move it up/down (elevation)."
         "<p/>Press 'Ctrl' when you first click with a mouse to duplicate the selected objects before moving them."
         "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit movement to EW, NS, or UD."
         , IDC_MOVENSEW);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_listNeedSelectNoEmbed.Add (&pi);   // enable this if have a selection
      pi = m_pbarObject->ButtonAdd (1, IDB_MOVEUD,ghInstance, 
         NULL, "Move object up/down",
         "Click on an object and hold the mouse button down while moving the mouse. "
         "This will move the object up/down (vertically). "
         "It will not move it NSEW."
         "<p/>If the 'Shift' key is pressed, the object's reference point "
         "will snap to the nearest flat plane (aka: floor), ignoring the normal grid. "
         "<p/>Press 'Ctrl' when you first click with a mouse to duplicate the selected objects before moving them."
         , IDC_MOVEUD);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_listNeedSelectNoEmbed.Add (&pi);   // enable this if have a selection
      pi = m_pbarObject->ButtonAdd (1, IDB_MOVEEMBED,ghInstance, 
         "Alt-M", "Move embedded object",
         "Click on an object and hold the mouse button down while moving the mouse. "
         "This will move the object left, right, up, or down in the surface."
         "<p/>Press 'Ctrl' when you first click with a mouse to duplicate the selected objects before moving them."
         , IDC_MOVEEMBED);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_listNeedSelectOneEmbed.Add (&pi);   // enable this if have a selection
      pi = m_pbarObject->ButtonAdd (1, IDB_MOVEROTZ,ghInstance, 
         "Alt-R", "Rotate object around vertical",
         "Click on an object and hold the mouse button down while moving the mouse. "
         "The object will be rotated around its vertical axis."
         "<p/>Press 'Ctrl' when you first click with a mouse to duplicate the selected objects before moving them."
         , IDC_MOVEROTZ);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_listNeedSelectNoEmbed.Add (&pi);   // enable this if have a selection
      pi = m_pbarObject->ButtonAdd (1, IDB_MOVEROTY,ghInstance, 
         NULL, "Rotate object around horizontal",
         "Click on an object and hold the mouse button down while moving the mouse. "
         "The object will be rotate around front/back axis."
         "<p/>Press 'Ctrl' when you first click with a mouse to duplicate the selected objects before moving them."
         , IDC_MOVEROTY);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_listNeedSelectOnlyOneEmbed.Add (&pi);   // enable this if have a selection

      pi = m_pbarObject->ButtonAdd (1, IDB_MOVEREFERENCE,ghInstance, 
         NULL, "Change movement reference...",
         "Click on this button to display a menu allowing you to select a movement "
         "reference point for the object. Both movement and rotation will occur "
         "around this point."
         , IDC_MOVEREFERENCE);
      m_listNeedSelect.Add (&pi);   // enable this if have a selection
      pi = m_pbarObject->ButtonAdd (1, IDB_DIALOGBOX,ghInstance, 
         NULL, "Movement settings...",
         "Entre the object's location and orientation by hand."
         , IDC_MOVESETTINGS);
      m_listNeedSelect.Add (&pi);   // enable this if have a selection

      // object control
      pi = m_pbarObject->ButtonAdd (0, IDB_OBJNEW,ghInstance, 
         "O", "New object...",
         "Click here to create a new object.<p/>"
         "If you hold the control-key down when you press the button a new copy "
         "of the last object you created will be made."
         , IDC_OBJNEW);

      pi = m_pbarObject->ButtonAdd (0, IDB_OBJPAINT,ghInstance, 
         NULL, "Change object's color or texture",
         "Clicking on an object will bring up a dialog box asking you "
         "what color or texture you want to use for the object."
         "<p/>"
         "If you press CONTROL while clicking on an object, the dialog will "
         "be skipped and the color set to the last one you used."
         "<p/>"
         "If you press SHIFT while clicking on a wall, just the wall/floor visible in the room will be painted. "
         "Otherwise, the entire wall/floor will be painted."
         , IDC_OBJPAINT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      pi = m_pbarObject->ButtonAdd (0, IDB_OBJINTELADJUST,ghInstance, 
         "I", "Intelligent adjust",
         "Clicking on an object tells it to adjust its shape or other characteristics "
         "so it fits in better with nearby objects. For example: Clicking on a wall "
         "will cut it down so it fits under any roofs above it."
         , IDC_OBJINTELADJUST);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      pi = m_pbarObject->ButtonAdd (0, IDB_OBJDECONSTRUCT,ghInstance, 
         NULL, "Deconstruct",
         "Split the object up into its constituent parts. (For example: Click on a "
         "building block to split it up into walls, ceilings, and floors.) Once you "
         "split and object you cannot rebuild it."
         , IDC_OBJDECONSTRUCT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (0, IDB_OBJMERGE,ghInstance, 
         NULL, "Merge objects",
         "Select one or more objects you wish to merge (such as intersecting floors) "
         "with another object. Then, using this tool, click on the object "
         "to merge to. Not all objects "
         "will be able to merge; see the documentation for details."
         , IDC_OBJMERGE);
      pi->Enable(FALSE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_listNeedSelect.Add (&pi);   // enable this if have a selection

      pi = m_pbarObject->ButtonAdd (0, IDB_OBJATTACH,ghInstance, 
         NULL, "Attach objects",
         "If you attach an object to another, moving the other object will also move "
         "the attached object."
         "<p/>"
         "Select one or more objects you wish to attach "
         "to another object. Then, using this tool, click on the object "
         "to attach to. (If attaching to a character, click on the specific limb "
         "you wish to attach to.)"
         "<p/>"
         "Using attach on an already attached object will detach it."
         , IDC_OBJATTACH);
      pi->Enable(FALSE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_listNeedSelect.Add (&pi);   // enable this if have a selection

      pi = m_pbarObject->ButtonAdd (0, IDB_OBJOPENCLOSE, ghInstance,
         NULL, "Open/close",
         "Click on a object to open or close it. Click and drag to the right or left "
         "top partially open or close it."
         , IDC_OBJOPENCLOSE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      pi = m_pbarObject->ButtonAdd (0, IDB_OBJONOFF, ghInstance,
         NULL, "On/off",
         "Click on a object to turn it on or off."
         , IDC_OBJONOFF);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (0, IDB_OBJATTRIB,ghInstance, 
         NULL, "Attributes",
         "Click on a object to change its attributes, such as opening or closing it, "
         "or turning it on. If an object has a large number, clicking "
         "close to the source of the attribute (such as the joint in a skeleton)."
         , IDC_OBJATTRIB);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      // object buttons
      pi = m_pbarObject->ButtonAdd (0, IDB_OBJDIALOG,ghInstance, 
         "Alt-S", "Object settings",
         "Select this tool and click on an object to bring up object-specific "
         "settings."
         , IDC_OBJDIALOG);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (0, IDB_OBJSHOWEDITOR,ghInstance, 
         NULL, "Object editor",
         "Select this tool and click on an object to bring up object-specific "
         "editor. Only certain types of objects (like ground and polygon meshs) "
         "have editors."
         , IDC_OBJSHOWEDITOR);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      break;
   } // switch

   // objectm right-most bin
   switch (m_dwViewWhat) {
   default: // BUGFIX - Default added other buttons
      break;
   case VIEWWHAT_BONE:
   case VIEWWHAT_OBJECT:
   case VIEWWHAT_WORLD:
      if (m_dwViewWhat != VIEWWHAT_BONE) {
         pi = m_pbarObject->ButtonAdd (2, IDB_OBJCONTROLDIALOG,ghInstance, 
            NULL, "Which control points are displayed",
            "Select this tool and click on an object to bring up a dialog "
            " that lets you choose which control points are shown."
            , IDC_OBJCONTROLDIALOG);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      }

      m_pObjControlInOut = pi = m_pbarObject->ButtonAdd (2, IDB_OBJCONTROLINOUT,ghInstance, 
         NULL, "Drag object's control forwards/backwards",
         "Lets you change a selected object's control points. "
         "Click on a control point and drag it to resize or reshape "
         "the object. "
         "NOTE: This only moves the control points fowards and backwards, not "
         "left, right, up, or down.<p/>"
         "If the control button is pressed while you click a dialog will "
         "appear letting you hand-enter a new value."
         "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit movement to EW, NS, or UD."
         , IDC_OBJCONTROLINOUT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_pObjControlUD = pi = m_pbarObject->ButtonAdd (2, IDB_OBJCONTROLUD,ghInstance, 
         NULL, "Drag object's control points up/down",
         "Lets you change a selected object's control points. "
         "Click on a control point and drag it to resize or reshape "
         "the object. "
         "NOTE: This only moves the control points up or down, not NSEW.<p/>"
         "If the control button is pressed while you click a dialog will "
         "appear letting you hand-enter a new value."
         , IDC_OBJCONTROLUD);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_pObjControlNSEW = pi = m_pbarObject->ButtonAdd (2, IDB_OBJCONTROLNSEW,ghInstance, 
         NULL, "Drag object's control points NSEW",
         "Lets you change a selected object's control points. "
         "Click on a control point and drag it to resize or reshape "
         "the object. "
         "NOTE: This only moves the control points NSEW, not vertically.<p/>"
         "If the control button is pressed while you click a dialog will "
         "appear letting you hand-enter a new value."
         "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit movement to EW, NS, or UD."
         , IDC_OBJCONTROLNSEW);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      m_pObjControlNSEWUD = pi = m_pbarObject->ButtonAdd (2, IDB_OBJCONTROLNSEWUD,ghInstance, 
         "Alt-C", "Drag object's control points in any direction",
         "Lets you change a selected object's control points. "
         "Click on a control point and drag it to resize or reshape "
         "the object. "
         "NOTE: This only moves the control points in any direction.<p/>"
         "If the control button is pressed while you click a dialog will "
         "appear letting you hand-enter a new value."
         "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit movement to EW, NS, or UD."
         , IDC_OBJCONTROLNSEWUD);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      break;
   } // switch

   UpdatePolyMeshButton ();
     
   m_pbarGeneral->AdjustAllButtons();
   m_pbarMisc->AdjustAllButtons();
   m_pbarView->AdjustAllButtons();
   m_pbarObject->AdjustAllButtons();

   // finally
   m_dwCameraButtons = -1; // to enforce an update
   UpdateCameraButtons ();

   // call WorldChanged() so that butons are updated
   WorldChanged (0, NULL);

   // get the undo and redo buttons set properly
   BOOL fUndo, fRedo;
   fUndo = m_pWorld->UndoQuery (&fRedo);
   WorldUndoChanged (fUndo, fRedo);

}


/***********************************************************************************
CHouseView::UpdatePolyMeshButtons - Get the current polymesh selection mode
from the polymesh object and updates the buttons
*/
void CHouseView::UpdatePolyMeshButton (void)
{
   DWORD adwWant[3];
   if ((m_dwViewWhat != VIEWWHAT_POLYMESH) || !m_pbarView->m_alPCIconButton[0].Num())
      return;
   // BUGFIX - If there aren't any buttons yet then don't bother

   // If not specifically modify poly mode then dont do this either
   if ((m_dwViewSub != VSPOLYMODE_POLYEDIT) && (m_dwViewSub != VSPOLYMODE_TEXTURE))
      return;


   // get the polymesh
   PCObjectPolyMesh pm = PolyMeshObject2 ();
   if (!pm)
      return;

   if (pm->m_PolyMesh.SelModeGet() == m_dwPolyMeshSelMode)
      return;  // nothing has changed

   // else, change
   m_dwPolyMeshSelMode = pm->m_PolyMesh.SelModeGet();

   // what want?
   adwWant[0] = IDC_SELINDIVIDUAL;
   switch (m_apRender[0]->CameraModelGet()) {
   case CAMERAMODEL_FLAT:
      adwWant[1] = IDC_VIEWFLATROTZ;  // BUGFIX - middle rotates
      adwWant[2] = IDC_VIEWFLATLRUD;  // right moves
      break;
   case CAMERAMODEL_PERSPWALKTHROUGH:
      adwWant[1] = IDC_VIEWWALKROTZX;  // BUGFIX - middle rotates
      adwWant[2] = IDC_VIEWWALKLRUD;  // right moves
      break;
   case CAMERAMODEL_PERSPOBJECT:
   default:
      adwWant[1] = IDC_VIEW3DROTZ;  // BUGFIX - middle rotates
      adwWant[2] = IDC_VIEW3DLRUD;  // right moves
      break;
   }

   // set light on for the button
   DWORD dwButton;
   switch (m_dwPolyMeshSelMode) {
   case 0:
      dwButton = IDC_PMEDITVERT;
      break;
   case 1:
      dwButton = IDC_PMEDITEDGE;
      break;
   case 2:
   default:
      dwButton = IDC_PMEDITSIDE;
      break;
   }
   m_pbarObject->FlagsSet (dwButton, IBFLAG_BLUELIGHT, 0);

   // wipe out buttons
   m_pbarObject->Clear (1);
   m_pbarObject->Clear (2);

   PCIconButton pi;

   // operation buttons
   if (m_dwViewSub == VSPOLYMODE_TEXTURE) {
      pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH4,ghInstance, 
         "1", "Brush size (extra small)",
         "This tool lets you paint the currently selected texture onto the polygon mesh.",
         IDC_BRUSH4);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH8,ghInstance, 
         "2", "Brush size (small)",
         "This tool lets you paint the currently selected texture onto the polygon mesh.",
         IDC_BRUSH8);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH16,ghInstance, 
         "3", "Brush size (medium)",
         "This tool lets you paint the currently selected texture onto the polygon mesh.",
         IDC_BRUSH16);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH32,ghInstance, 
         "4", "Brush size (large)",
         "This tool lets you paint the currently selected texture onto the polygon mesh.",
         IDC_BRUSH32);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_BRUSH64,ghInstance, 
         "5", "Brush size (extra large)",
         "This tool lets you paint the currently selected texture onto the polygon mesh.",
         IDC_BRUSH64);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);


      pi = m_pbarObject->ButtonAdd (2, IDB_PMTEXTMIRROR, ghInstance, 
         NULL, "Mirror texture map...",
         "Mirrors the texture map from right to left, top to bottom, etc. "
         "Using this, you only have to adjust your texture maps on one side of "
         "a symmetrical character/object. When you have finished, press this "
         "button to mirror to the other side."
         , IDC_PMTEXTMIRROR);

      pi = m_pbarObject->ButtonAdd (2, IDB_PMTEXTSPHERE, ghInstance, 
         NULL, "Rebuild texture map using spherical coordinates",
         "This will rebuild the texture map using spherical coordinates, mapping "
         "the texture as though it were wrapped around a globe."
         "<p/>"
         "To use this, orient your camera so the axis is up and down, and the equator "
         "is left and right. Select the texture you wish to modify and set the "
         "checkbox indicating how you want the scale to be applied. Then, press "
         "this button."
         , IDC_PMTEXTSPHERE);
      pi = m_pbarObject->ButtonAdd (2, IDB_PMTEXTCYLINDER, ghInstance, 
         NULL, "Rebuild texture map using cylindrical coordinates",
         "This will rebuild the texture map using cylindrical coordinates, mapping "
         "the texture as though it were wrapped around a tube."
         "<p/>"
         "To use this, orient your camera so the axis (length of the tube) is up and down, and the equator "
         "is left and right. Select the texture you wish to modify and set the "
         "checkbox indicating how you want the scale to be applied. Then, press "
         "this button."
         , IDC_PMTEXTCYLINDER);
      pi = m_pbarObject->ButtonAdd (2, IDB_PMTEXTPLANE, ghInstance, 
         NULL, "Rebuild texture map using planar coordinates",
         "This will rebuild the texture map using a flat plane, mapping "
         "the texture as though it were projected using a slide projector."
         "<p/>"
         "To use this, orient your camera. "
         "Select the texture you wish to modify and set the "
         "checkbox indicating how you want the scale to be applied. Then, press "
         "this button."
         , IDC_PMTEXTPLANE);

      if (m_dwPolyMeshSelMode == 2) {
         pi = m_pbarObject->ButtonAdd (2, IDB_PMSIDEDISCONNECT, ghInstance, 
            NULL, "Disconnect the selection",
            "Disconnects the selected sides from any other sides in the polygon mesh. "
            "You can the move the disconnected sides without affecting the textures "
            "of the neighboring sides."
            , IDC_PMTEXTDISCONNECT);
      }
      else if (m_dwPolyMeshSelMode == 0) {
         pi = m_pbarObject->ButtonAdd (2, IDB_OBJCONTROLDIALOG, ghInstance, 
            NULL, "Type in a vertex's texture location",
            "Clicking on a vertex will display a dialog which lets you type in "
            "the vertex's texture location."
            , IDC_PMTEXTMOVEDIALOG);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      }

      pi = m_pbarObject->ButtonAdd (2, IDB_PMCOLLAPSE, ghInstance, 
         NULL, "Collapse the selection",
         "Collapses the selection's texture into just one point. "
         "You can also use this to reconnect disconnected textures at a point."
         , IDC_PMTEXTCOLLAPSE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (2, IDB_PMMOVESCALE, ghInstance, 
         NULL, "Scale the texture selection",
         "Lets you scale the textures on the selected verticies, edges, or sides in any direction. "
         "Click on vertex, edge, or side and drag it to scale the textures."
         "<p/>If you hold down the CONTROL key while moving the textures on the "
         "vertices will be disconnected from one another so that adjacent textures "
         "won't be affected."
         "<p/>Hold down the 'x' or 'y' key while moving to limit scaling."
         , IDC_PMTEXTSCALE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (2, IDB_PMTEXTROT, ghInstance, 
         NULL, "Rotate the texture selection",
         "Lets you rotate the textures on the selected verticies, edges, or sides. "
         "Click on vertex, edge, or side and drag it to rotate."
         "<p/>If you hold down the CONTROL key while moving the textures on the "
         "vertices will be disconnected from one another so that adjacent textures "
         "won't be affected."
         , IDC_PMTEXTROT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (2, IDB_PMTEXTMOVE, ghInstance, 
         NULL, "Move the texture selection",
         "Lets you move the textures on the selected verticies, edges, or sides in any direction. "
         "Click on vertex, edge, or side and drag it to change the textures."
         "<p/>If you hold down the CONTROL key while moving the textures on the "
         "vertices will be disconnected from one another so that adjacent textures "
         "won't be affected."
         "<p/>Hold down the 'x' or 'y' key while moving to limit movement."
         , IDC_PMTEXTMOVE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
   } // viewsub == 4
   else if (m_dwViewSub == VSPOLYMODE_POLYEDIT) {
      if (m_dwPolyMeshSelMode == 1) {
         pi = m_pbarObject->ButtonAdd (2, IDB_PMEDGESPLIT, ghInstance, 
            NULL, "Split the selected edge(s)",
            "Splits the selected edge(s) in two."
            , IDC_PMEDGESPLIT);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      }
      else if (m_dwPolyMeshSelMode == 2) {
         pi = m_pbarObject->ButtonAdd (2, IDB_PMEXTRA, ghInstance, 
            NULL, "Extra effects",
            "This will display a menu of additional effects that you can "
            "apply to the selected vertices, edges, or sides."
            , IDC_PMEXTRA);

         pi = m_pbarObject->ButtonAdd (2, IDB_PMTESSELATE, ghInstance, 
            NULL, "Split sides into smaller parts",
            "This menu will let you split the selected side(s) into smaller sides."
            , IDC_PMTESSELATE);

         pi = m_pbarObject->ButtonAdd (2, IDB_PMSIDEDISCONNECT, ghInstance, 
            NULL, "Disconnect the selection",
            "Disconnects the selected sides from any other sides in the polygon mesh."
            "<p/>"
            "If you hold down the CONTROL key when you press this then the disconnected "
            "sides will be moved into a new polygon mesh. If you have chosen to only view "
            "the polygon mesh in the editor then the disconnected sides may seem to "
            "disappear, although they will be visible in the main view."
            , IDC_PMSIDEDISCONNECT);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);


         pi = m_pbarObject->ButtonAdd (2, IDB_PMSIDEINSET, ghInstance, 
            NULL, "Inset selected sides",
            "Click on a side and drag to inset the side. "
            "If you hold the control key down while insetting multiple sides "
            "then then each side will individually be insetted."
            , IDC_PMSIDEINSET);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

         pi = m_pbarObject->ButtonAdd (2, IDB_PMSIDEEXTRUDE, ghInstance, 
            NULL, "Extrude selected sides",
            "Click on a side and drag to extrude (or intrude) the side. "
            "If you hold the control key down while extruding multiple sides "
            "then then each side will individually be extruded."
            , IDC_PMSIDEEXTRUDE);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      }

      pi = m_pbarObject->ButtonAdd (2, IDB_PMSIDESPLIT, ghInstance, 
         NULL, "Split the side",
         "To split a side into two polygons, click on one of its vertices or edges "
         "and drag to another vertex or edge."
         , IDC_PMSIDESPLIT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (2, IDB_PMNEWSIDE, ghInstance, 
         NULL, "Create a new side",
         "To create a new polygon, click on a series of points to incate the edge "
         "of the polygon. When you're finished with the polygon click on the starting "
         "point to create it. Try to use a clockwise direction."
         , IDC_PMNEWSIDE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (2, IDB_PMCOLLAPSE, ghInstance, 
         NULL, "Collapse the selection",
         "Collapses the selection (or the object that's clicked on) into "
         "just one point."
         , IDC_PMCOLLAPSE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (2, IDB_GRAPHPOINTDELETE, ghInstance, 
         NULL, "Delete the selection",
         "Deletes the vertex, side, or edge that's clicked on."
         , IDC_PMDELETE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);




      // movement buttons
      pi = m_pbarObject->ButtonAdd (1, IDB_OBJCONTROLNSEWUD, ghInstance, 
         NULL, "Move the selection in any direction",
         "Lets you move the selected verticies, edges, or sides in any direction. "
         "Click on vertex, edge, or side and drag it to resize or reshape "
         "the object. "
         "NOTE: This only moves the elements in any direction."
         "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit movement to EW, NS, or UD."
         , IDC_PMMOVEANYDIR);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_OBJCONTROLINOUT,ghInstance, 
         NULL, "Move the selection forwards/backwards",
         "Lets you move the selected verticies, edges, or sides forwards and backwards. "
         "Click on vertex, edge, or side and drag it to resize or reshape "
         "the object. "
         "NOTE: This only moves the elements fowards and backwards, not "
         "left, right, up, or down."
         "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit movement to EW, NS, or UD."
         , IDC_PMMOVEINOUT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_PMMOVEPERP, ghInstance, 
         NULL, "Move the selection along normal",
         "Lets you move the selected verticies, edges, or sides. "
         "Click on vertex, edge, or side and drag it to resize or reshape "
         "the object. "
         "This moves the elements along the normal (perpendicular part) of the surface."
         , IDC_PMMOVEPERP);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_PMMOVEPERPIND, ghInstance, 
         NULL, "Move the selection along normals",
         "Lets you move the selected verticies, edges, or sides. "
         "Click on vertex, edge, or side and drag it to resize or reshape "
         "the object. "
         "This moves the elements along the normals of the vertices."
         , IDC_PMMOVEPERPIND);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_PMMOVESCALE, ghInstance, 
         NULL, "Scale the selection",
         "Click on vertex, edge, or side and drag it to resize or reshape "
         "the object. "
         "Increases or decreases the size of the selected elements."
         "<p/>Hold down the 'x', 'y', or 'z' key while moving to limit scaling to EW, NS, or UD."
         , IDC_PMMOVESCALE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_PMMOVEROTPERP, ghInstance, 
         NULL, "Twist the selection",
         "Click on vertex, edge, or side and drag it to resize or reshape "
         "the object. "
         "Rotates the elements around the normal."
         , IDC_PMMOVEROTPERP);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_PMMOVEROTNONPERP, ghInstance, 
         NULL, "Rotate the selection",
         "Click on vertex, edge, or side and drag it to resize or reshape "
         "the object. "
         "Rotates the elements."
         , IDC_PMMOVEROTNONPERP);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      pi = m_pbarObject->ButtonAdd (1, IDB_PMBEVEL, ghInstance, 
         NULL, "Bevel the selection",
         "Click and drag on a vertex, edge, or side to bevel it."
         , IDC_PMBEVEL);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

      if (m_dwPolyMeshSelMode == 0) {
         pi = m_pbarObject->ButtonAdd (1, IDB_OBJCONTROLDIALOG, ghInstance, 
            NULL, "Type in a vertex's location",
            "Clicking on a vertex will display a dialog which lets you type in "
            "the vertex's location."
            , IDC_PMMOVEDIALOG);
         pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);
      }
   }  // if m_dwViewSub == VSPOLYMODE_POLYEDIT

   // if button no longer exists then set a new one
   DWORD i;
   for (i = 0; i < 3; i++)
      switch (ButtonExists(m_adwPointerMode[i], i)) {
      case 0: // doesnt exist
         SetPointerMode (adwWant[i], i);
         break;
      case 1:  // exists but not selected
         SetPointerMode (m_adwPointerMode[i], i);
         break;
      }

   m_pbarObject->AdjustAllButtons();

}

/***********************************************************************************
CHouseView::UpdateCameraButtons - Gets the current camera model from the rendering
object. If the camera model is different than the buttons shown then its buttons
are updated.
*/
void CHouseView::UpdateCameraButtons (void)
{
   DWORD dwModel = m_apRender[0]->CameraModelGet ();
   if (dwModel == m_dwCameraButtons)
      return;
   m_dwCameraButtons = dwModel;

   // else, changed model

   // set the light for the new model
   DWORD dwButton;
   switch (dwModel) {
   case CAMERAMODEL_FLAT:
      dwButton = IDC_VIEWFLAT;
      break;
   case CAMERAMODEL_PERSPWALKTHROUGH:
      dwButton = IDC_VIEWWALKTHROUGH;
      break;
   case CAMERAMODEL_PERSPOBJECT:
   default:
      dwButton = IDC_VIEW3D;
      break;
   }
   m_pbarView->FlagsSet (dwButton, IBFLAG_BLUELIGHT, 0);

   // Delete all the mid-section buttons for scrolling
   m_pbarView->Clear (1);

   // common ones
   PCIconButton pi;
   DWORD adwWant[3];
   memset (adwWant, 0, sizeof(adwWant));

   if ((m_dwViewWhat == VIEWWHAT_POLYMESH) && (m_dwViewSub > VSPOLYMODE_POLYEDIT)) {
      switch (m_dwViewSub) {
      case VSPOLYMODE_POLYEDIT:  // poly moce
      case VSPOLYMODE_TEXTURE:  // texture
         adwWant[0] = IDC_SELINDIVIDUAL;
         break;
      default:
         adwWant[0] = IDC_BRUSH64;
         break;
      }
   }
   else
      adwWant[0] = IDC_SELINDIVIDUAL;

   pi = m_pbarView->ButtonAdd (1, IDB_VIEWFLATQUICK,ghInstance, 
      "Q", "Quick view...",
      "Click this an select a view to quickly jump to. (If you press control "
      "when you click the button, the view will include the entire world, not "
      "just the house.)",
      IDC_VIEWFLATQUICK);
   pi = m_pbarView->ButtonAdd (1, IDB_ZOOMAT,ghInstance, 
      "@", "Look from different angle",
      "Lets you see the object from a different angle. "
      "To use this tool, click on an object in the image. "
      "A menu will appear asking you what direction you wish to view the object from."
      ,
      IDC_ZOOMAT);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
   pi = m_pbarView->ButtonAdd (1, IDB_ZOOMIN,ghInstance, 
      "+", "Zoom in",
      "To use this tool, click on the image. The camera will zoom in towards "
      "the point."
      "<p/>"
      "If you hold the CONTROL key while zooming, the camera will be moved so it is "
      "perpendicular to the surface"
      ,
      IDC_ZOOMIN);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
   pi = m_pbarView->ButtonAdd (1, IDB_ZOOMOUT,ghInstance, 
      "-", "Zoom out",
      "To use this tool, click on the image. The camera will zoom out away "
      "from the point."
      "<p/>"
      "If you hold the CONTROL key while zooming, the camera will be moved so it is "
      "perpendicular to the surface",
      IDC_ZOOMOUT);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);

   // Add new mid-section buttons for scrolling, and then choose one
   switch (dwModel) {
   case CAMERAMODEL_FLAT:
      pi = m_pbarView->ButtonAdd (1, IDB_ZOOMINDRAG,ghInstance, 
         "=", "Zoom in by dragging",
         "Click and drag a region of the house you wish to zoom in on.",
         IDC_ZOOMINDRAG);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWFLATSCALE,ghInstance, 
         NULL, "Scale",
         "Click on the image and drag to scale the image larger or smaller.",
         IDC_VIEWFLATSCALE);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWFLATLRUD,ghInstance, 
         "M", "Left/right and up/down",
         "Click on the image and drag to scroll the image left/right or up/down.",
         IDC_VIEWFLATLRUD);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWFLATROTZ,ghInstance, 
         "R", "Rotate around up/down",
         "Click on the image and drag to rotate the image around an imaginary "
         "vertical line in the center of rotation.",
         IDC_VIEWFLATROTZ);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWFLATROTX,ghInstance, 
         NULL, "Rotate around east/west",
         "Click on the image and drag to rotate the image around an imaginary "
         "east/west horizontal line in the center of rotation.",
         IDC_VIEWFLATROTX);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEW3DROTY,ghInstance, 
         NULL, "Rotate around north/south",
         "Click on the image and drag to rotate the image around an imaginary "
         "north/south horizontal line in the center of rotation.",
         IDC_VIEWFLATROTY);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWSETCENTER,ghInstance, 
         NULL, "Change rotation point",
         "Click on a point in the image to set that as the center of rotation "
         "for the flattened view. If you click on open-air then the center "
         "of rotation will be set to the center of the world.",
         IDC_VIEWSETCENTERFLAT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWFLATLOOKAT,ghInstance, 
         "L", "Move walkthrough position",
         "Click on a point to move the walkthrough camera to that point, and continue "
         "dragging to indicate what direction the camera should look. "
         "This works best when you're looking straight down on the house.",
         IDC_VIEWFLATLLOOKAT);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_DIALOGBOX,ghInstance, 
         NULL, "Position and rotation settings...",
         "Enter numbers for position and rotation.", IDC_VIEWFLATSETTINGS);

      //adwWant[0] = IDC_VIEWFLATLRUD;
      adwWant[1] = IDC_VIEWFLATROTZ;  // BUGFIX - middle rotates
      adwWant[2] = IDC_VIEWFLATLRUD;  // right moves
      break;
   case CAMERAMODEL_PERSPWALKTHROUGH:
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWWALKFBROTZ,ghInstance, 
         "M", "Look left/right or move forwards/backwards",
         "Click on the image and drag to rotate left/right or move forwards/backwards.",
         IDC_VIEWWALKFBROTZ);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWWALKROTZX,ghInstance, 
         "R", "Look left/right and up/down",
         "Click on the image and drag to look up/down and left/right.",
         IDC_VIEWWALKROTZX);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWWALKROTY,ghInstance, 
         NULL, "Tilt head",
         "Click on the image and drag to tilt your head.",
         IDC_VIEWWALKROTY);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWFLATLRUD,ghInstance,    //BUGFIX - Two icons were the same visually
         NULL, "Left/right up/down",
         "Click on the image and drag to move up/down or left/right.",
         IDC_VIEWWALKLRUD);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWWALKLRFB,ghInstance, 
         NULL, "Left/right forwards/back",
         "Click on the image and drag to move forwards/backwards or left/right.",
         IDC_VIEWWALKLRFB);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);

      pi = m_pbarView->ButtonAdd (1, IDB_DIALOGBOX,ghInstance, 
         NULL, "Position and rotation settings...",
         "Enter numbers for position and rotation.", IDC_VIEWWALKSETTINGS);

      //adwWant[0] = IDC_VIEWWALKFBROTZ;
      adwWant[1] = IDC_VIEWWALKROTZX;  // BUGFIX - middle rotates
      adwWant[2] = IDC_VIEWWALKLRUD;  // right moves
      break;
   case CAMERAMODEL_PERSPOBJECT:
   default:
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWFLATLRUD,ghInstance, 
         "M", "Left/right up/down",
         "Click on the image and drag to move the model left/right or up/down.",
         IDC_VIEW3DLRUD);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEW3DLRFB,ghInstance, 
         NULL, "Left/right forwards/backwards",
         "Click on the image and drag to move the model left/right or forwards/backwards.",
         IDC_VIEW3DLRFB);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWFLATROTZ,ghInstance, 
         "R", "Rotate around up/down",
         "Click on the image and drag to rotate the image around an imaginary "
         "vertical line in the center of rotation.",
         IDC_VIEW3DROTZ);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWFLATROTX,ghInstance, 
         NULL, "Rotate around east/west",
         "Click on the image and drag to rotate the image around an imaginary "
         "east/west horizontal line in the center of rotation.",
         IDC_VIEW3DROTX);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEW3DROTY,ghInstance, 
         NULL, "Rotate around north/south",
         "Click on the image and drag to rotate the image around an imaginary "
         "north/south horizontal line in the center of rotation.",
         IDC_VIEW3DROTY);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_VIEWSETCENTER,ghInstance, 
         NULL, "Change rotation point",
         "Click on a point in the image to set that as the center of rotation "
         "for the model view. If you click on open-air then the center "
         "of rotation will be set to the center of rotation.",
         IDC_VIEWSETCENTER3D);
      pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);
      pi = m_pbarView->ButtonAdd (1, IDB_DIALOGBOX,ghInstance, 
         NULL, "Position and rotation settings...",
         "Enter numbers for position and rotation.", IDC_VIEW3DSETTINGS);

      //adwWant[0] = IDC_VIEW3DLRUD;
      adwWant[1] = IDC_VIEW3DROTZ;  // BUGFIX - middle rotates
      adwWant[2] = IDC_VIEW3DLRUD;  // right moves
      break;
   }

   // if button no longer exists then set a new one
   DWORD i;
   for (i = 0; i < 3; i++)
      switch (ButtonExists(m_adwPointerMode[i], i)) {
      case 0: // doesnt exist
         SetPointerMode (adwWant[i], i);
         break;
      case 1:  // exists but not selected
         SetPointerMode (m_adwPointerMode[i], i);
         break;
      }

   m_pbarView->AdjustAllButtons();

}

/***********************************************************************************
ColorBlend - Blend colors and paints onth the screen

inputs
   HDC      hDC - Draw on
   RECT     *pPaint - PAINTSTRUCT.rcPaint - Edges that need to paint
   RECT     *pr - Rectangle to use
   COLORREF ul, ll, ur, lr - Colors at the corners
*/
void ColorBlend (HDC hDC, RECT *pPaint, RECT *pr, COLORREF ul, COLORREF ll, COLORREF ur, COLORREF lr)
{
   // paint
   HBRUSH   hbr;

   #define  COLORDIST(x,y)    (DWORD)(max(x,y)-min(x,y))

   // figure out how many divions across & down need
   DWORD dwAcross, dwDown;
   dwAcross = COLORDIST(GetRValue(ul), GetRValue(ur));
   dwAcross = max(dwAcross, COLORDIST(GetGValue(ul), GetGValue(ur)));
   dwAcross = max(dwAcross, COLORDIST(GetBValue(ul), GetBValue(ur)));
   dwAcross = max(dwAcross, COLORDIST(GetRValue(ll), GetRValue(lr)));
   dwAcross = max(dwAcross, COLORDIST(GetGValue(ll), GetGValue(lr)));
   dwAcross = max(dwAcross, COLORDIST(GetBValue(ll), GetBValue(lr)));

   dwDown = COLORDIST(GetRValue(ul), GetRValue(ll));
   dwDown = max(dwDown, COLORDIST(GetGValue(ul), GetGValue(ll)));
   dwDown = max(dwDown, COLORDIST(GetBValue(ul), GetBValue(ll)));
   dwDown = max(dwDown, COLORDIST(GetRValue(ur), GetRValue(lr)));
   dwDown = max(dwDown, COLORDIST(GetGValue(ur), GetGValue(lr)));
   dwDown = max(dwDown, COLORDIST(GetBValue(ur), GetBValue(lr)));

   if (((dwAcross+1)*(dwDown+1)) > 100) {
      // divide by 4 - mach banding
      dwAcross /= 4;
      dwDown /= 4;
   }
   else {
      dwAcross /= 2;
      dwDown /= 2;
   }

   // also, no more than a trasition every 4? pixels
   dwAcross = min((DWORD) (pr->right - pr->left) / 4, dwAcross);
   dwDown = min((DWORD) (pr->bottom - pr->top) / 4, dwDown);

   // at least 2
   dwAcross = max(dwAcross,2);
   dwDown = max(dwDown, 2);

   // loop
   DWORD x, y;
   fp   iLeft, iTop;
   fp   iDeltaX, iDeltaY;
   RECT  r;
   iTop = pr->top;
   iDeltaX = (fp) (pr->right - pr->left) / (fp) dwAcross;
   iDeltaY = (fp) (pr->bottom - pr->top) / (fp) dwDown;

   #define ALPHA(x,y,amt)     (BYTE)((1.0-(amt))*(x) + (amt)*(y))
   for (y = 0; y < dwDown; y++, iTop += iDeltaY ) {
      // rectangle
      iLeft = pr->left;
      r.top = (int) iTop;
      r.bottom = (int) (iTop + iDeltaY);
      if (y+1 == dwDown)
         r.bottom = pr->bottom;  // ensure theres no roundoff error

      // if not in refresh area then skip
      if ((r.bottom < pPaint->top) || (r.top > pPaint->bottom))
         continue;

      COLORREF cLeft, cRight;
      fp   fAlpha;
      fAlpha = (fp) y / (fp) (dwDown-1);
      cLeft = RGB(
         ALPHA(GetRValue(ul),GetRValue(ll),fAlpha),
         ALPHA(GetGValue(ul),GetGValue(ll),fAlpha),
         ALPHA(GetBValue(ul),GetBValue(ll),fAlpha)
         );
      cRight = RGB(
         ALPHA(GetRValue(ur),GetRValue(lr),fAlpha),
         ALPHA(GetGValue(ur),GetGValue(lr),fAlpha),
         ALPHA(GetBValue(ur),GetBValue(lr),fAlpha)
         );

      for (x = 0; x < dwAcross; x++, iLeft += iDeltaX) {
         // rectangle
         r.left = (int) iLeft;
         r.right = (int) (iLeft + iDeltaX);
         if (x+1 == dwAcross)
            r.right = pr->right;  // ensure theres no roundoff error

         // if not in refresh area then skip
         if ((r.right < pPaint->left) || (r.left > pPaint->right))
            continue;

         COLORREF c;
         fp   fAlpha;
         fAlpha = (fp) x / (fp) (dwAcross-1);
         c = RGB(
            ALPHA(GetRValue(cLeft),GetRValue(cRight),fAlpha),
            ALPHA(GetGValue(cLeft),GetGValue(cRight),fAlpha),
            ALPHA(GetBValue(cLeft),GetBValue(cRight),fAlpha)
         );

         // paint it
         hbr = CreateSolidBrush (c);
         FillRect (hDC, &r, hbr);
         DeleteObject (hbr);
      }
   }
}


/***********************************************************************************
CHouseView::InvalidateDisplay - Redraws the display area, not the buttons.
*/
void CHouseView::InvalidateDisplay (void)
{
   // invalidate
   DWORD i;
   for (i = 0; i < VIEWTHUMB; i++)
      if (m_afThumbnailVis[i])
         InvalidateRect (m_hWnd, &m_arThumbnailLoc[i], FALSE);
}

/***********************************************************************************
CHouseView::RenderUpdate - Re-renders the scene (probably because some parameters have
changed somewhere) and invalidates the portion of the window where the rendering
is drawn.

inputs
   BOOL     fForece - If TRUE, it forces a refresh right away. Otherwise, it will
               delay a refresh for non-active windows
   DWORD    dwReason - Reason for the change - passed into WorldChanged()
*/
void CHouseView::RenderUpdate (DWORD dwReason, BOOL fForce)
{
   // display new settings
   UpdateToolTipByPointerMode();

   // just make sure the quality is right
   QualityApply ();

   // BUGFIX - Speed up - if the camera location moved dont bother updating the
   // small thumbnails
   BOOL fCameraMoved = (dwReason == WORLDC_CAMERAMOVED);

   // used to call m_apRender[0]->Render(). Instead, call WorldChanged(-1) to force
   // a redraw
   DWORD i;
   for (i = 0; i < (DWORD)(fCameraMoved ? 1 : VIEWTHUMB); i++)
      m_apRender[i]->CacheClear();  // just to make sure that when call this will redraw cached objects
   m_fWaitToDrawForce = fForce;
   WorldChanged (dwReason, NULL);
   m_fWaitToDrawForce = FALSE;

}

/**********************************************************************************
CHouseView::PointInImage - Returns TRUE if the point is in the image. FALSE if the
point is outside the rendering image area.

inputs
   int         iX, iY - Point in client coordinates
   DWORD       *pdwImageX, *pdwImageY - Filled with a pointer to the image X,Y. Can be NULL
returns
   BOOL - TRUE if the point is in the iamge
*/
BOOL CHouseView::PointInImage (int iX, int iY, DWORD *pdwImageX, DWORD *pdwImageY)
{
   RECT  r;
   r = m_arThumbnailLoc[0];
   //GetClientRect (m_hWnd, &r);
   //if (!m_fSmallWindow || (m_fSmallWindow && !m_fHideButtons)) {
   //   r.left += VARSCREENSIZE+1;
   //   r.top += VARSCREENSIZE+1;
   //   r.bottom -= (VARSCREENSIZE+1);
   //   r.right -= (VARSCREENSIZE+1);
   //}

   POINT p;
   p.x = iX;
   p.y = iY;
   if (!PtInRect (&r, p))
      return FALSE;

   // else, it's in the image
   if (pdwImageX)
      *pdwImageX = (DWORD) (iX - r.left) * IMAGESCALE / m_dwImageScale;
   if (pdwImageY)
      *pdwImageY = (DWORD) (iY - r.top) * IMAGESCALE/ m_dwImageScale;

   return TRUE;
 }

/**********************************************************************************
CHouseView::SetProperCursor - Call this to set the cursor to whatever is appropriate
for the window
*/
void CHouseView::SetProperCursor (int iX, int iY)
{
   DWORD dwX, dwY;
   DWORD dwButton = m_dwButtonDown ? (m_dwButtonDown-1) : 0;

   // if the point is over one of the thumbnails then show the cursor
   DWORD i;
   POINT pt;
   pt.x = iX;
   pt.y = iY;
   for (i = 1; i < VIEWTHUMB; i++) {
      if (!m_afThumbnailVis[i] || (i == m_dwThumbnailCur))
         continue;
      if (PtInRect (&m_arThumbnailLoc[i], pt)) {
         // can click on this
         SetCursor (LoadCursor (NULL, IDC_ARROW));
         return;
      }
   }

   if (PointInImage (iX, iY, &dwX, &dwY)) {
      switch (m_adwPointerMode[dwButton]) {
         case IDC_VIEWFLATLRUD:
         case IDC_VIEW3DLRUD:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWFLATLRUD)));
            break;

         case IDC_VIEW3DLRFB:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEW3DLRFB)));
            break;

         case IDC_VIEWWALKFBROTZ:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWWALKFBROTZ)));
            break;

         case IDC_VIEWWALKROTZX:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWWALKROTZX)));
            break;

         case IDC_VIEWWALKLRUD:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWWALKLRUD)));
            break;

         case IDC_VIEWWALKLRFB:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWWALKLRFB)));
            break;

         case IDC_MOVENSEW:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMOVENSEW)));
            break;

         case IDC_MOVENSEWUD:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMOVENSEWUD)));
            break;

         case IDC_MOVEEMBED:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMOVEEMBED)));
            break;

         case IDC_MOVEUD:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMOVEUD)));
            break;

         case IDC_BRUSH4:
         case IDC_BRUSH8:
         case IDC_BRUSH16:
         case IDC_BRUSH32:
         case IDC_BRUSH64:
         case IDC_PMMOVEPERP:
         case IDC_PMMOVEPERPIND:
         case IDC_PMTAILORNEW:
         case IDC_PMMOVESCALE:
         case IDC_PMORGSCALE:
         case IDC_PMMORPHSCALE:
         case IDC_PMMOVEROTPERP:
         case IDC_PMMOVEROTNONPERP:
         case IDC_PMMOVEINOUT:
         case IDC_PMMOVEANYDIR:
         case IDC_PMTEXTSCALE:
         case IDC_PMTEXTMOVE:
         case IDC_PMTEXTROT:
         case IDC_PMMOVEDIALOG:
         case IDC_PMTEXTMOVEDIALOG:
         case IDC_PMCOLLAPSE:
         case IDC_PMTEXTCOLLAPSE:
         case IDC_PMDELETE:
         case IDC_PMEDGESPLIT:
         case IDC_PMSIDESPLIT:
         case IDC_PMNEWSIDE:
         case IDC_PMSIDEINSET:
         case IDC_PMSIDEEXTRUDE:
         case IDC_PMSIDEDISCONNECT:
         case IDC_PMBEVEL:
         case IDC_PMMAGANY:
         case IDC_PMMAGNORM:
         case IDC_PMMAGVIEWER:
         case IDC_PMMAGPINCH:
            {
               BOOL fDisable = FALSE;
               if (!m_dwButtonDown) {
                  // make sure over object
                  PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
                  DWORD dwFind;
                  PCObjectPolyMesh pm = PolyMeshObject2(&dwFind);
                  if (!pm || (HIWORD(pip->dwID) != dwFind+1))
                     fDisable = TRUE;
               }

               DWORD dwCursor;
               switch (m_adwPointerMode[dwButton]) {
               case IDC_BRUSH4:
                  dwCursor =IDC_CURSORBRUSH4;
                  break;
               case IDC_BRUSH8:
                  dwCursor =IDC_CURSORBRUSH8;
                  break;
               case IDC_BRUSH16:
                  dwCursor =IDC_CURSORBRUSH16;
                  break;
               case IDC_BRUSH32:
                  dwCursor =IDC_CURSORBRUSH32;
                  break;
               case IDC_BRUSH64:
                  dwCursor =IDC_CURSORBRUSH64;
                  break;
               case IDC_PMEDGESPLIT:
                  dwCursor =IDC_CURSORPMEDGESPLIT;
                  break;
               case IDC_PMDELETE:
                  dwCursor =IDC_CURSORGRAPHPOINTDELETE;
                  break;
               case IDC_PMCOLLAPSE:
               case IDC_PMTEXTCOLLAPSE:
                  dwCursor =IDC_CURSORPMCOLLAPSE;
                  break;
               case IDC_PMSIDEDISCONNECT:
                  dwCursor = IDC_CURSORPMSIDEDISCONNECT;
                  break;
               case IDC_PMMOVEDIALOG:
               case IDC_PMTEXTMOVEDIALOG:
                  dwCursor =IDC_CURSOROBJCONTROLDIALOG;
                  break;
               default:
               case IDC_PMMOVEPERP:
                  dwCursor = IDC_CURSORPMMOVEPERP;
                  break;
               case IDC_PMSIDEINSET:
                  dwCursor = IDC_CURSORPMSIDEINSET;
                  break;
               case IDC_PMSIDEEXTRUDE:
                  dwCursor = IDC_CURSORPMSIDEEXTRUDE;
                  break;
               case IDC_PMTAILORNEW:
                  dwCursor = IDC_CURSORPMTAILORNEW;
                  break;
               case IDC_PMMOVEPERPIND:
                  dwCursor = IDC_CURSORPMMOVEPERPIND;
                  break;
               case IDC_PMMOVESCALE:
               case IDC_PMORGSCALE:
               case IDC_PMMORPHSCALE:
               case IDC_PMTEXTSCALE:
                  dwCursor = IDC_CURSORPMMOVESCALE;
                  break;
               case IDC_PMMAGANY:
                  dwCursor = IDC_CURSORPMMAGANY;
                  break;
               case IDC_PMMAGNORM:
                  dwCursor = IDC_CURSORPMMAGNORM;
                  break;
               case IDC_PMMAGVIEWER:
                  dwCursor = IDC_CURSORPMMAGVIEWER;
                  break;
               case IDC_PMMAGPINCH:
                  dwCursor = IDC_CURSORPMMAGPINCH;
                  break;
               case IDC_PMMOVEROTPERP:
                  dwCursor = IDC_CURSORPMMOVEROTPERP;
                  break;
               case IDC_PMMOVEROTNONPERP:
                  dwCursor = IDC_CURSORPMMOVEROTNONPERP;
                  break;
               case IDC_PMTEXTMOVE:
                  dwCursor = IDC_CURSORPMTEXTMOVE;
                  break;
               case IDC_PMTEXTROT:
                  dwCursor = IDC_CURSORPMTEXTROT;
                  break;
               case IDC_PMMOVEANYDIR:
                  dwCursor = IDC_CURSOROBJCONTROLNSEWUD;
                  break;
               case IDC_PMBEVEL:
                  dwCursor = IDC_CURSORPMBEVEL;
                  break;
               case IDC_PMSIDESPLIT:
                  dwCursor = IDC_CURSORPMSIDESPLIT;
                  break;
               case IDC_PMNEWSIDE:
                  dwCursor = IDC_CURSORPMNEWSIDE;
                  break;
               case IDC_PMMOVEINOUT:
                  dwCursor = IDC_CURSOROBJCONTROLINOUT;
                  break;
               }

               HCURSOR hCursor;
               if (fDisable)
                  hCursor = LoadCursor (NULL, IDC_NO);
               else {
                  if (dwCursor == IDC_CURSORBRUSH64)
                     hCursor = (HCURSOR)LoadImage (ghInstance, MAKEINTRESOURCE(dwCursor), IMAGE_CURSOR, 64, 64,LR_SHARED);
                  else
                     hCursor = LoadCursor(ghInstance, MAKEINTRESOURCE(dwCursor));
               }
               SetCursor (hCursor);
               UpdateToolTipByPointerMode();
            }
            break;

         case IDC_OBJCONTROLINOUT:
         case IDC_OBJCONTROLNSEWUD:
         case IDC_OBJCONTROLNSEW:
         case IDC_OBJCONTROLUD:
            {
               // only set the cursor if over drag points
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);

               // if the button isn't down then set the object and control
               // ID so that when show tool tip it's right
               if (!m_dwButtonDown) {
                  BOOL fOK = (LOWORD(pip->dwID) >= 0xf000);

                  if (fOK) {
                     m_dwObjControlObject = (DWORD)HIWORD(pip->dwID) - 1;
                     m_dwObjControlID = (DWORD) LOWORD(pip->dwID) - 0xf000;
                  }
                  else {
                     // BUGFIX - if not initially OK then see if can find closest
                     DWORD dwClosest = -1;
                     if (HIWORD(pip->dwID))
                        dwClosest = NearestCP (dwX, dwY, HIWORD(pip->dwID) - 1);

                     m_dwObjControlObject = (dwClosest != -1) ? (DWORD) (HIWORD(pip->dwID)-1) : (DWORD)-1;
                     m_dwObjControlID = dwClosest;
                  }
               }

               DWORD dwCursor;
               switch (m_adwPointerMode[dwButton]) {
               default:
               case IDC_OBJCONTROLINOUT:
                  dwCursor = IDC_CURSOROBJCONTROLINOUT;
                  break;
               case IDC_OBJCONTROLNSEWUD:
                  dwCursor = IDC_CURSOROBJCONTROLNSEWUD;
                  break;
               case IDC_OBJCONTROLNSEW:
                  dwCursor = IDC_CURSOROBJCONTROLNSEW;
                  break;
               case IDC_OBJCONTROLUD:
                  dwCursor = IDC_CURSOROBJCONTROLUD;
                  break;
               }

               SetCursor ((m_dwObjControlObject != (DWORD)-1) ?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(dwCursor)) :
                  LoadCursor (NULL, IDC_NO));
               UpdateToolTipByPointerMode();
            }
            break;

         case IDC_ZOOMIN:
            {
               // see if can't zoom in
               if (m_apRender[0]->CameraModelGet() != CAMERAMODEL_FLAT) {
                  PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
                  if (pip->fZ >= ZINFINITE) {
                     SetCursor (LoadCursor (NULL, IDC_NO));
                     break;
                  }
               }

               // else ok
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORZOOMIN)));
            }
            break;

         case IDC_ZOOMAT:
            {
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
               if (pip->fZ >= ZINFINITE) {
                  SetCursor (LoadCursor (NULL, IDC_NO));
                  break;
               }

               // else ok
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORZOOMAT)));
            }
            break;

         case IDC_THUMBNAIL:
            {
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
               if (pip->fZ >= ZINFINITE) {
                  SetCursor (LoadCursor (NULL, IDC_NO));
                  break;
               }

               // else ok
               SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORTHUMBNAIL)));
            }
            break;

         case IDC_ZOOMOUT:
            SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORZOOMOUT)));
            break;

         case IDC_ZOOMINDRAG:
            SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORZOOMINDRAG)));
            break;

         case IDC_OBJPAINT:
            {
               // only set the cursor if over something can paint
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
               BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

               // if the button isn't down then set the object and control
               // ID so that when show tool tip it's right
               if (!m_dwButtonDown) {
                  if (fOK) {
                     m_dwObjControlObject = (DWORD)HIWORD(pip->dwID) - 1;
                     m_dwObjControlID = (DWORD) LOWORD(pip->dwID);
                  }
                  else {
                     m_dwObjControlObject = (DWORD)-1;
                  }
               }

               // BUGFIX - Change cursor to reflect if can paint or not
               BOOL fCanPaint;
               fCanPaint = (m_dwObjControlObject != (DWORD)-1);
               if (fCanPaint) {
                  // get the color just to make sure it works
                  PCObjectSurface ps = NULL;
                  PCObjectSocket pos = m_pWorld->ObjectGet (m_dwObjControlObject);

                  if (pos)
                     ps = pos->SurfaceGet (m_dwObjControlID);
                  if (!ps)
                     fCanPaint = FALSE;
                  else
                     delete ps;
               }

               SetCursor ( fCanPaint?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORPAINT)) :
                  LoadCursor (NULL, IDC_NO));
               UpdateToolTipByPointerMode();
            }
            break;

         case IDC_OBJMERGE:
         case IDC_PMMERGE:
         case IDC_BONEMERGE:
            {
               // only set the cursor if over something can paint
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);

               // if the button isn't down then set the object and control
               // ID so that when show tool tip it's right
               if (!m_dwButtonDown) {
                  m_dwObjControlObject = (DWORD)HIWORD(pip->dwID) - 1;
                  m_dwObjControlID = (DWORD) LOWORD(pip->dwID);
               }

               BOOL fCanPaint;
               fCanPaint = (m_dwObjControlObject != (DWORD)-1);
               SetCursor ( fCanPaint?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSOROBJMERGE)) :
                  LoadCursor (NULL, IDC_NO));
            }
            break;

         case IDC_OBJATTACH:
            {
               // only set the cursor if over something can paint
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);

               // if the button isn't down then set the object and control
               // ID so that when show tool tip it's right
               if (!m_dwButtonDown) {
                  m_dwObjControlObject = (DWORD)HIWORD(pip->dwID) - 1;
                  m_dwObjControlID = (DWORD) LOWORD(pip->dwID);
               }

               BOOL fCanPaint;
               fCanPaint = (m_dwObjControlObject != (DWORD)-1);
               SetCursor ( fCanPaint?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSOROBJATTACH)) :
                  LoadCursor (NULL, IDC_NO));
            }
            break;

         case IDC_OBJINTELADJUST:
            {
               // only set the cursor if over something can paint
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
               BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

               // if the button isn't down then set the object and control
               // ID so that when show tool tip it's right
               if (!m_dwButtonDown) {
                  if (fOK) {
                     m_dwObjControlObject = (DWORD)HIWORD(pip->dwID) - 1;
                     m_dwObjControlID = (DWORD) LOWORD(pip->dwID);

                     // see if it can adjust
                     PCObjectSocket pos;
                     pos = m_pWorld->ObjectGet(m_dwObjControlObject);
                     if (!pos || !pos->IntelligentAdjust(FALSE))
                        m_dwObjControlObject = (DWORD)-1;
                  }
                  else {
                     m_dwObjControlObject = (DWORD)-1;
                  }
               }

               SetCursor ((m_dwObjControlObject != (DWORD)-1) ?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORINTELADJUST)) :
                  LoadCursor (NULL, IDC_NO));
               UpdateToolTipByPointerMode();
            }
            break;

         case IDC_GRIDFROMPOINT:
            {
               // only set the cursor if over something can paint
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
               BOOL fOK = (pip->fZ < ZINFINITE);

               SetCursor (fOK ?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORGRIDFROMPOINT)) :
                  LoadCursor (NULL, IDC_NO));
               UpdateToolTipByPointerMode();
            }
            break;

         case IDC_OBJDECONSTRUCT:
            {
               // only set the cursor if over something can paint
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
               BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

               // if the button isn't down then set the object and control
               // ID so that when show tool tip it's right
               if (!m_dwButtonDown) {
                  if (fOK) {
                     m_dwObjControlObject = (DWORD)HIWORD(pip->dwID) - 1;
                     m_dwObjControlID = (DWORD) LOWORD(pip->dwID);

                     // see if it can adjust
                     PCObjectSocket pos;
                     pos = m_pWorld->ObjectGet(m_dwObjControlObject);
                     // Test to see if can descontruct
                     if (!pos || !pos->Deconstruct(FALSE))
                        m_dwObjControlObject = (DWORD)-1;
                  }
                  else {
                     m_dwObjControlObject = (DWORD)-1;
                  }
               }

               SetCursor ((m_dwObjControlObject != (DWORD)-1) ?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORDECONSTRUCT)) :
                  LoadCursor (NULL, IDC_NO));
               UpdateToolTipByPointerMode();
            }
            break;

         case IDC_OBJATTRIB:
            {
               // only set cursor if over something can open
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
               BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

               // if the button isn't down then set the object and control
               // ID so that when show tool tip it's right
               if (!m_dwButtonDown) {
                  if (fOK) {
                     m_dwObjControlObject = (DWORD)HIWORD(pip->dwID) - 1;
                     m_dwObjControlID = (DWORD) LOWORD(pip->dwID) - 0xf000;
                  }
                  else {
                     m_dwObjControlObject = (DWORD)-1;
                  }
               }

               SetCursor ((m_dwObjControlObject != (DWORD)-1) ?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSOROBJATTRIB)) :
                  LoadCursor (NULL, IDC_NO));
               UpdateToolTipByPointerMode();
            }
            break;

         case IDC_OBJOPENCLOSE:
            {
               // only set cursor if over something can open
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
               BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

               // if the button isn't down then set the object and control
               // ID so that when show tool tip it's right
               if (!m_dwButtonDown) {
                  if (fOK) {
                     m_dwObjControlObject = (DWORD)HIWORD(pip->dwID) - 1;
                     m_dwObjControlID = (DWORD) LOWORD(pip->dwID) - 0xf000;
                  }
                  else {
                     m_dwObjControlObject = (DWORD)-1;
                  }
               }

               // veritfy that can open
               if (m_dwObjControlObject != (DWORD)-1) {
                  PCObjectSocket pos = m_pWorld->ObjectGet (m_dwObjControlObject);
                  if (!pos || (pos->OpenGet() < 0))
                     m_dwObjControlObject = -1;
               }

               SetCursor ((m_dwObjControlObject != (DWORD)-1) ?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSOROBJOPENCLOSE)) :
                  LoadCursor (NULL, IDC_NO));
               UpdateToolTipByPointerMode();
            }
            break;

         case IDC_OBJONOFF:
            {
               // only set cursor if over something can turn onoff
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
               BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

               // if the button isn't down then set the object and control
               // ID so that when show tool tip it's right
               if (!m_dwButtonDown) {
                  if (fOK) {
                     m_dwObjControlObject = (DWORD)HIWORD(pip->dwID) - 1;
                     m_dwObjControlID = (DWORD) LOWORD(pip->dwID) - 0xf000;
                  }
                  else {
                     m_dwObjControlObject = (DWORD)-1;
                  }
               }

               // veritfy that can turn on off
               if (m_dwObjControlObject != (DWORD)-1) {
                  PCObjectSocket pos = m_pWorld->ObjectGet (m_dwObjControlObject);
                  if (!pos || (pos->TurnOnGet() < 0))
                     m_dwObjControlObject = -1;
               }

               SetCursor ((m_dwObjControlObject != (DWORD)-1) ?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSOROBJONOFF)) :
                  LoadCursor (NULL, IDC_NO));
               UpdateToolTipByPointerMode();
            }
            break;

         case IDC_BONEEDIT:
         case IDC_BONESPLIT:
         case IDC_BONEDELETE:
         case IDC_BONEDISCONNECT:
         case IDC_BONESCALE:
         case IDC_BONEROTATE:
         case IDC_BONENEW:
            {
               BOOL fDisable = FALSE;
               if (!m_dwButtonDown) {
                  // make sure over object
                  PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
                  DWORD dwFind;
                  PCObjectBone pb = BoneObject2(&dwFind);
                  if (!pb || (HIWORD(pip->dwID) != dwFind+1))
                     fDisable = TRUE;
               }

               DWORD dwCursor;
               switch (m_adwPointerMode[dwButton]) {
               default:
               case IDC_BONEEDIT:
                  dwCursor =IDC_CURSOROBJCONTROLDIALOG;
                  break;
               case IDC_BONESPLIT:
                  dwCursor =IDC_CURSORPMEDGESPLIT;
                  break;
               case IDC_BONEDELETE:
                  dwCursor =IDC_CURSORGRAPHPOINTDELETE;
                  break;
               case IDC_BONEDISCONNECT:
                  dwCursor = IDC_CURSORDECONSTRUCT;
                  break;
               case IDC_BONESCALE:
                  dwCursor = IDC_CURSORPMMOVESCALE;
                  break;
               case IDC_BONEROTATE:
                  dwCursor = IDC_CURSORBONEROTATE;
                  break;
               case IDC_BONENEW:
                  dwCursor = IDC_CURSORBONENEW;
                  break;
               }

               HCURSOR hCursor;
               if (fDisable)
                  hCursor = LoadCursor (NULL, IDC_NO);
               else
                  hCursor = LoadCursor(ghInstance, MAKEINTRESOURCE(dwCursor));

               SetCursor (hCursor);
               UpdateToolTipByPointerMode();
            }
            break;


         case IDC_OBJSHOWEDITOR:
         case IDC_OBJDIALOG:
         case IDC_OBJCONTROLDIALOG:
            {
               // only set the cursor if over something can paint
               PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
               BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

               // if the button isn't down then set the object and control
               // ID so that when show tool tip it's right
               if (!m_dwButtonDown) {
                  if (fOK) {
                     m_dwObjControlObject = (DWORD)HIWORD(pip->dwID) - 1;
                     m_dwObjControlID = (DWORD) LOWORD(pip->dwID);
                  }
                  else {
                     m_dwObjControlObject = (DWORD)-1;
                  }
               }

               PCObjectSocket pos;
               BOOL fCanClick;
               pos = NULL;
               fCanClick = FALSE;
               if (m_dwObjControlObject != (DWORD)-1)
                  pos = m_pWorld->ObjectGet (m_dwObjControlObject);
               if (pos) switch (m_adwPointerMode[dwButton]) {
                  default:
                  case IDC_OBJDIALOG:
                     fCanClick = pos->DialogQuery();
                     break;
                  case IDC_OBJCONTROLDIALOG:
                     fCanClick = pos->DialogCPQuery();
                     break;
                  case IDC_OBJSHOWEDITOR:
                     fCanClick = pos->EditorCreate (FALSE);
                     break;
               }

               // cursor
               DWORD dwCursor;
               dwCursor = 0;
               if (fCanClick) switch (m_adwPointerMode[dwButton]) {
                  default:
                  case IDC_OBJDIALOG:
                     dwCursor = IDC_CURSORDIALOG;
                     break;
                  case IDC_OBJCONTROLDIALOG:
                     dwCursor =IDC_CURSOROBJCONTROLDIALOG;
                     break;
                  case IDC_OBJSHOWEDITOR:
                     dwCursor = IDC_CURSOROBJSHOWEDITOR;
                     break;
               }

               SetCursor (dwCursor ? LoadCursor(ghInstance, MAKEINTRESOURCE(dwCursor)) :
                  LoadCursor (NULL, IDC_NO));
               //UpdateToolTipByPointerMode();
            }
            break;

         case IDC_VIEWFLATSCALE:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWFLATSCALE)));
            break;

         case IDC_VIEWFLATROTZ:
         case IDC_VIEW3DROTZ:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWFLATROTZ)));
            break;

         case IDC_VIEWWALKROTY:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWWALKROTY)));
            break;

         case IDC_MOVEROTZ:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMOVEROTZ)));
            break;

         case IDC_VIEWFLATROTX:
         case IDC_VIEW3DROTX:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWFLATROTX)));
            break;

         case IDC_VIEWFLATROTY:
         case IDC_VIEW3DROTY:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEW3DROTY)));
            break;

         case IDC_MOVEROTY:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMOVEROTY)));
            break;

         case IDC_VIEWFLATLLOOKAT:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWFLATLOOKAT)));
            break;

         case IDC_CLIPLINE:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORCLIPLINE)));
            break;

         case IDC_SELREGION:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORSELREGION)));
            break;

         case IDC_VIEWSETCENTERFLAT:
         case IDC_VIEWSETCENTER3D:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORVIEWCENTERFLAT)));
            break;

         case IDC_CLIPPOINT:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORCLIPPOINT)));
            break;

         case IDC_SELINDIVIDUAL:
            SetCursor (LoadCursor (NULL, IDC_ARROW));
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

         default:
            SetCursor (LoadCursor (NULL, IDC_NO));
      }
   }
   else {
      // don't click cursor
      SetCursor (LoadCursor (NULL, IDC_NO));
   }
}


typedef struct {
   PCHouseView pv;         // view object
   CPoint      pOld;       // old location
   CPoint      pNew;       // new location
   CPoint      pOldRot;    // old rotation
   CPoint      pNewRot;    // new rotation
} GFP, *PGFP;


/****************************************************************************
GridFromPointPage
*/
BOOL GridFromPointPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PGFP pg = (PGFP) pPage->m_pUserData;
   PCHouseView pv = pg->pv;

   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszTrans = L"trans";
         PWSTR pszRot = L"rot";
         // only care if buttons start with rot or trans
         if (wcsncmp(p->pControl->m_pszName, pszTrans, wcslen(pszTrans)) &&
            wcsncmp(p->pControl->m_pszName, pszRot, wcslen(pszRot)))
            break;

         // get all the values
         PCEscControl pControl;
         DWORD i;
         PWSTR psz;
         for (i = 0; i < 3; i++) {
            // XYZ locations
            if (i == 0)
               psz = L"transx";
            else if (i == 1)
               psz = L"transy";
            else
               psz = L"transz";
            pControl = pPage->ControlFind (psz);
            if (pControl)
               pv->m_pGridCenter.p[i] = pControl->AttribGetBOOL(Checked()) ? pg->pNew.p[i] : pg->pOld.p[i];

            // angles
            if (i == 0)
               psz = L"rotx";
            else if (i == 1)
               psz = L"roty";
            else
               psz = L"rotz";
            pControl = pPage->ControlFind (psz);
            if (pControl)
               pv->m_pGridRotation.p[i] = pControl->AttribGetBOOL(Checked()) ? pg->pNewRot.p[i] : pg->pOldRot.p[i];
         }

         // new matrix
         pv->m_mGridToWorld.FromXYZLLT (&pv->m_pGridCenter,
            pv->m_pGridRotation.p[2], pv->m_pGridRotation.p[0], pv->m_pGridRotation.p[1]);
         pv->m_mGridToWorld.Invert4 (&pv->m_mWorldToGrid);

         // redraw
         pv->RenderUpdate(WORLDC_CAMERAMOVED);
      }

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Grid from point";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"NEWEW")) {
            MemZero (&gMemTemp);
            WCHAR szTemp[64];
            MeasureToString (pg->pNew.p[0], szTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"NEWNS")) {
            MemZero (&gMemTemp);
            WCHAR szTemp[64];
            MeasureToString (pg->pNew.p[1], szTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"NEWUD")) {
            MemZero (&gMemTemp);
            WCHAR szTemp[64];
            MeasureToString (pg->pNew.p[2], szTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OLDEW")) {
            MemZero (&gMemTemp);
            WCHAR szTemp[64];
            MeasureToString (pg->pOld.p[0], szTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OLDNS")) {
            MemZero (&gMemTemp);
            WCHAR szTemp[64];
            MeasureToString (pg->pOld.p[1], szTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OLDUD")) {
            MemZero (&gMemTemp);
            WCHAR szTemp[64];
            MeasureToString (pg->pOld.p[2], szTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"NEWLO")) {
            MemZero (&gMemTemp);
            char szTemp[64];
            AngleToString (szTemp, pg->pNewRot.p[2], TRUE);
            WCHAR szwTemp[64];
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szwTemp, 64);
            MemCat (&gMemTemp, szwTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"NEWLA")) {
            MemZero (&gMemTemp);
            char szTemp[64];
            AngleToString (szTemp, pg->pNewRot.p[0], TRUE);
            WCHAR szwTemp[64];
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szwTemp, 64);
            MemCat (&gMemTemp, szwTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"NEWTI")) {
            MemZero (&gMemTemp);
            char szTemp[64];
            AngleToString (szTemp, pg->pNewRot.p[1], TRUE);
            WCHAR szwTemp[64];
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szwTemp, 64);
            MemCat (&gMemTemp, szwTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OLDLO")) {
            MemZero (&gMemTemp);
            char szTemp[64];
            AngleToString (szTemp, pg->pOldRot.p[2], TRUE);
            WCHAR szwTemp[64];
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szwTemp, 64);
            MemCat (&gMemTemp, szwTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OLDLA")) {
            MemZero (&gMemTemp);
            char szTemp[64];
            AngleToString (szTemp, pg->pOldRot.p[0], TRUE);
            WCHAR szwTemp[64];
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szwTemp, 64);
            MemCat (&gMemTemp, szwTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OLDTI")) {
            MemZero (&gMemTemp);
            char szTemp[64];
            AngleToString (szTemp, pg->pOldRot.p[1], TRUE);
            WCHAR szwTemp[64];
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szwTemp, 64);
            MemCat (&gMemTemp, szwTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************************
CHouseView::GridFromPointUI - Brings up UI asking the user what he wants to do with
the point he clicked on.
*/
void CHouseView::GridFromPointUI (DWORD dwX, DWORD dwY)
{
   // find out the point where clicked
   fp fZ, fOldZ;
   CPoint pClick;
   fZ = m_apRender[0]->PixelToZ (dwX, dwY);
   if (fZ > 1000) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return;
   }

   GFP g;
   memset (&g, 0, sizeof(g));
   g.pv = this;
   g.pOld.Copy (&m_pGridCenter);
   g.pOldRot.Copy (&m_pGridRotation);

   m_apRender[0]->PixelToWorldSpace (dwX, dwY, fOldZ = fZ, &pClick);
   g.pNew.Copy (&pClick);

   // get points left/right and up/down to find the slope of the line
   CPoint pLeft, pRight, pUp, pDown;
   BOOL fLeftValid, fRightValid, fUpValid, fDownValid;
   fLeftValid = fRightValid = fUpValid = fDownValid = FALSE;
   if (dwX) {
      fZ = m_apRender[0]->PixelToZ (dwX-1, dwY);
      if (fZ < 1000) {
         m_apRender[0]->PixelToWorldSpace (dwX-1, dwY, fZ, &pLeft);
         fLeftValid = TRUE;
      }
   }
   if (dwX+1 < m_aImage[0].Width()) {
      fZ = m_apRender[0]->PixelToZ (dwX+1, dwY);
      if (fZ < 1000) {
         m_apRender[0]->PixelToWorldSpace (dwX+1, dwY, fZ, &pRight);
         fRightValid = TRUE;
      }
   }
   if (dwY) {
      fZ = m_apRender[0]->PixelToZ (dwX, dwY-1);
      if (fZ < 1000) {
         m_apRender[0]->PixelToWorldSpace (dwX, dwY-1, fZ, &pUp);
         fUpValid = TRUE;
      }
   }
   if (dwY+1 < m_aImage[0].Height()) {
      fZ = m_apRender[0]->PixelToZ (dwX, dwY+1);
      if (fZ < 1000) {
         m_apRender[0]->PixelToWorldSpace (dwX, dwY+1, fZ, &pDown);
         fDownValid = TRUE;
      }
   }
   if ((fLeftValid || fRightValid) && (fUpValid || fDownValid)) {
      CPoint A, B, C;

      m_fClipPlaneSurfaceValid = TRUE;
      A.Subtract (fRightValid ? &pRight : & pClick,
         fLeftValid ? &pLeft : &pClick);
      B.Subtract (fUpValid ? &pUp : &pClick,
         fDownValid ? &pDown : &pClick);
      A.Normalize();
      B.Normalize();
      C.CrossProd (&A, &B);
      C.Normalize();

      DWORD i;
      for (i = 0; i < 3; i++) {
         if (i < 2) {
            // only care about longitude
            g.pNewRot.p[i] = 0;
            continue;
         }

         // have a plane defined by three points, A, B, and zero. Find out
         // where the line formed by A and B interescts Z = 0
         CPoint Z, U, I;
         Z.Zero();
         U.Zero();
         U.p[i] = 1;
         if (!IntersectLinePlane (&A, &B, &Z, &U, &I)) {
            // if fails to intersect A and B must run parallel, so just throw
            // out Z component
            I.Copy (&A);
            I.p[i] = 0;
         }

         // atan2 will get the result
         g.pNewRot.p[i] = -atan2 (I.p[(i+1)%3], I.p[(i+2)%3]);
      }

      // if it's basically up/down then longitude = 0
      C.p[2] = 0;
      if (C.Length() < CLOSE)
         g.pNewRot.p[2] = 0;

   }

   // bring up the ui
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (m_hWnd, &r);

   cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGRIDFROMPOINT, GridFromPointPage, &g);
}

/****************************************************************************
ClipPointPage
*/
BOOL ClipPointPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // check the perpendicular button
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"camera");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // if we don't have measurements for the surface orientation then
         // disabled that
         if (!pv->m_fClipPlaneSurfaceValid) {
            pControl = pPage->ControlFind (L"surface");
            if (pControl)
               pControl->Enable(FALSE);
         }

         MeasureToString (pPage, L"offset", pv->m_fClipPlaneOffset);

         // change clipping
         pPage->Message (ESCM_USER+10);
      }
      break;

   case ESCM_USER+10:   // values changed
      {
         // picj H & V depending upon the settings
         CPoint pH, pV;
         PCEscControl pControl;
         if ((pControl = pPage->ControlFind(L"camera")) && pControl->AttribGetBOOL(Checked())) {
            pH.Copy (&pv->m_pClipPlaneCameraH);
            pV.Copy (&pv->m_pClipPlaneCameraV);
         }
         else if ((pControl = pPage->ControlFind(L"surface")) && pControl->AttribGetBOOL(Checked())) {
            pH.Copy (&pv->m_pClipPlaneSurfaceH);
            pV.Copy (&pv->m_pClipPlaneSurfaceV);
         }
         else if ((pControl = pPage->ControlFind(L"nsew")) && pControl->AttribGetBOOL(Checked())) {
            pH.Zero();
            pH.p[0] = 1;
            pV.Zero();
            pV.p[1] = 1;
         }
         else if ((pControl = pPage->ControlFind(L"nsud")) && pControl->AttribGetBOOL(Checked())) {
            pH.Zero();
            pH.p[1] = 1;
            pV.Zero();
            pV.p[2] = 1;
         }
         else {   //ewud
            pH.Zero();
            pH.p[2] = 1;
            pV.Zero();
            pV.p[0] = 1;
         }

         // figure out the normal. Know it's length 1 because pH and pV are length 1
         CPoint pN;
         pN.CrossProd (&pH, &pV);

         // figure out three points
         CPoint p1, p2, p3;
         p2.Copy (&pN);
         p2.Scale (pv->m_fClipPlaneOffset);  // distance
         p2.Add (&pv->m_pClipPlaneClick);
         p1.Add (&p2, &pH);
         pControl = pPage->ControlFind (L"otherside");
         if (pControl && pControl->AttribGetBOOL (Checked()))
            p3.Subtract (&p2, &pV);
         else
            p3.Add (&p2, &pV);

         // have the planes, so add it
         RTCLIPPLANE t;
         memset (&t, 0, sizeof(t));
         t.ap[0].Copy (&p1);
         t.ap[1].Copy (&p2);
         t.ap[2].Copy (&p3);
         t.dwID = CLIPPLANE_USER;
         pv->m_apRender[0]->ClipPlaneSet (CLIPPLANE_USER, &t);

         // redraw
         pv->RenderUpdate(WORLDC_NEEDTOREDRAW);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"camera") || 
            !_wcsicmp(p->pControl->m_pszName, L"surface") || 
            !_wcsicmp(p->pControl->m_pszName, L"nsew") || 
            !_wcsicmp(p->pControl->m_pszName, L"nsud") || 
            !_wcsicmp(p->pControl->m_pszName, L"ewud") || 
            !_wcsicmp(p->pControl->m_pszName, L"otherside"))

            pPage->Message (ESCM_USER+10);

      }

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"offset")) {
            if (!MeasureParseString (pPage, L"offset", &pv->m_fClipPlaneOffset))
               pv->m_fClipPlaneOffset = 0;
            pPage->Message (ESCM_USER+10);
         }

      }
      break;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Clip based on a point";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/***********************************************************************************
CHouseView::NormalFromPoint - Given a pixel that was clicked on, this returns
a normal, H, and V vector (all in world space) for the point. Used so that when
clip to a point know the surface's normal, and so click-to-look perpendicular
also works.

inputs
   DWORD       dwX, dwY - XY point clicked on
   PCPoint     pClick - where clicked
   PCPoint     pN - Filled with normal. Can be NULL
   PCPoint     pH, pV - Filled with H and V. Can be NULL.
                  NOTE: If clicked on just a point, where cant calculate surface,
                  then pN, pH, pV are o
returns
   BOOL - TRUE if clicked on a surface, FALSE if clicked on emptiness
*/
BOOL CHouseView::NormalFromPoint (DWORD dwX, DWORD dwY, PCPoint pClick, PCPoint pN, PCPoint pH, PCPoint pV)
{
   // find out the point where clicked
   fp fZ, fOldZ;
   fZ = m_apRender[0]->PixelToZ (dwX, dwY);
   if (fZ > 1000) {
      return FALSE;
   }
   CPoint pTempClick, pTempN, pTempH, pTempV;
   pTempN.Zero();
   pTempH.Zero();
   pTempV.Zero();
   m_apRender[0]->PixelToWorldSpace (dwX, dwY, fOldZ = fZ, &pTempClick);
   m_pClipPlaneClick.Copy (&pTempClick);

   // get points left/right and up/down to find the slope of the line
   CPoint pLeft, pRight, pUp, pDown;
   BOOL fLeftValid, fRightValid, fUpValid, fDownValid;
   fLeftValid = fRightValid = fUpValid = fDownValid = FALSE;
   if (dwX) {
      fZ = m_apRender[0]->PixelToZ (dwX-1, dwY);
      if (fZ < 1000) {
         m_apRender[0]->PixelToWorldSpace (dwX-1, dwY, fZ, &pLeft);
         fLeftValid = TRUE;
      }
   }
   if (dwX+1 < m_aImage[0].Width()) {
      fZ = m_apRender[0]->PixelToZ (dwX+1, dwY);
      if (fZ < 1000) {
         m_apRender[0]->PixelToWorldSpace (dwX+1, dwY, fZ, &pRight);
         fRightValid = TRUE;
      }
   }
   if (dwY) {
      fZ = m_apRender[0]->PixelToZ (dwX, dwY-1);
      if (fZ < 1000) {
         m_apRender[0]->PixelToWorldSpace (dwX, dwY-1, fZ, &pUp);
         fUpValid = TRUE;
      }
   }
   if (dwY+1 < m_aImage[0].Height()) {
      fZ = m_apRender[0]->PixelToZ (dwX, dwY+1);
      if (fZ < 1000) {
         m_apRender[0]->PixelToWorldSpace (dwX, dwY+1, fZ, &pDown);
         fDownValid = TRUE;
      }
   }
   if ((fLeftValid || fRightValid) && (fUpValid || fDownValid)) {
      pTempH.Subtract (fRightValid ? &pRight : & pTempClick,
         fLeftValid ? &pLeft : &pTempClick);
      pTempV.Subtract (fUpValid ? &pUp : &pTempClick,
         fDownValid ? &pDown : &pTempClick);
      pTempH.Normalize();
      pTempV.Normalize();
      pTempN.CrossProd (&pTempH, &pTempV);
      pTempN.Normalize();
   }

   if (pClick)
      pClick->Copy (&pTempClick);
   if (pH)
      pH->Copy (&pTempH);
   if (pV)
      pV->Copy (&pTempV);
   if (pN)
      pN->Copy (&pTempN);

   return TRUE;
}

/***********************************************************************************
CHouseView::ClipPointUI - Brings up UI asking the user what he wants to do with
the point he clipped on.
*/
void CHouseView::ClipPointUI (DWORD dwX, DWORD dwY)
{
   // BUGFIX - Use NormalFromPoint() function
   CPoint pClick;
   if (!NormalFromPoint(dwX, dwY, &pClick, NULL, &m_pClipPlaneSurfaceH, &m_pClipPlaneSurfaceV)) {
      m_apRender[0]->ClipPlaneRemove (CLIPPLANE_USER);
      RenderUpdate(WORLDC_NEEDTOREDRAW);
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return;
   }
   m_fClipPlaneSurfaceValid = (m_pClipPlaneSurfaceH.Length() > CLOSE);

   // and surfaces parallel to screen
   double fOldZ;
   CPoint pRight, pUp;
   fOldZ = m_apRender[0]->PixelToZ (dwX, dwY);
   m_apRender[0]->PixelToWorldSpace (dwX+1, dwY, fOldZ, &pRight);
   m_apRender[0]->PixelToWorldSpace (dwX, (fp)dwY-1, fOldZ, &pUp);
   m_pClipPlaneCameraH.Subtract (&pRight, &pClick);
   m_pClipPlaneCameraH.Normalize();
   m_pClipPlaneCameraV.Subtract (&pUp, &pClick);
   m_pClipPlaneCameraV.Normalize();

   m_fClipPlaneOffset = 0;

   // bring up the ui
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (m_hWnd, &r);

   cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLCLIPPOINT, ClipPointPage, this);
}

static int _cdecl BCompare (const void *elem1, const void *elem2)
{
   DWORD *pdw1, *pdw2;
   pdw1 = (DWORD*) elem1;
   pdw2 = (DWORD*) elem2;

   return (int) (*pdw1) - (int)(*pdw2);
}


/***********************************************************************************
CHouseView::ObjectCreateMenu - Shows a menu of all the objects the user can
create. If the user selects one it creates it in purgatory and switches to a paste mode.

inputs
   none
*/
void CHouseView::ObjectCreateMenu (void)
{
   static GUID gLastMajor, gLastMinor;
   GUID gCode, gSub;
   PWSTR pszName;

   BOOL fControl = (GetKeyState (VK_CONTROL) < 0);
   if (fControl) {
      gCode = gLastMajor;
      gSub = gLastMinor;

      goto additnow;
   }

   if (ObjectCFNewDialog (DEFAULTRENDERSHARD, m_hWnd, &gCode, &gSub)) {
      // remember this
      gLastMajor = gCode;
      gLastMinor = gSub;
      goto additnow;
   }
   return;


additnow:
   // get the name for the tooltip
   WCHAR szMajor[128], szMinor[128], szName[128];
   pszName = szName;
   if (ObjectCFNameFromGUIDs (DEFAULTRENDERSHARD, &gCode, &gSub, szMajor, szMinor, szName))
      wcscpy (m_szObjName, pszName);

   // have the two guids, so create
   // convert this to objects
   m_Purgatory.Clear();

   // set the floor levels in purgatory
   fp afElev[NUMLEVELS], fHigher;
   GlobalFloorLevelsGet (m_pWorld, NULL, afElev, &fHigher);
   GlobalFloorLevelsSet (&m_Purgatory, NULL, afElev, fHigher);

   PCObjectSocket pNew;
   pNew = ObjectCFCreate (DEFAULTRENDERSHARD, &gCode, &gSub);
   if (!pNew)
      return;  // error that shouldnt happen
   m_Purgatory.ObjectAdd (pNew);

   // ask if it can be dragged
   m_fPastingEmbedded = pNew->EmbedQueryCanEmbed();
   if (pNew->IntelligentPositionDrag(NULL,NULL,NULL))
      SetPointerMode (IDC_OBJECTPASTEDRAG, 0);
   else
      // set the pointer mode to placing the pasted object
      SetPointerMode (IDC_OBJECTPASTE, 0);



   return;
}


/**************************************************************************************
CHouseView::CreateApplicationMenu - Used it the user right clicks, this creates a popup
menu with the name of all the buttons. And shows it to the user.
*/
void CHouseView::CreateApplicationMenu (void)
{
   HMENU hMenu = CreatePopupMenu ();
   DWORD i, j,k;

   for (i = 0; i < 4; i++) {
      PCButtonBar pbb;
      char *pszMajorName;
      switch (i) {
      case 0:
         pszMajorName = "View";
         pbb = m_pbarView;
         break;
      case 1:
         pszMajorName = "Object 1";
         pbb = m_pbarMisc;
         break;
      case 2:
         pszMajorName = "Object 2";
         pbb = m_pbarObject;
         break;
      case 3:
         pszMajorName = "General";
         pbb = m_pbarGeneral;
         break;
      }

      // create a sub-menu here
      HMENU hSub = CreatePopupMenu ();
      if (!hSub)
         continue;

      for (j = 0; j < 3; j++) for (k = 0; k < pbb->m_alPCIconButton[j].Num(); k++) {
         PCIconButton pBut;
         pBut = *((PCIconButton*) pbb->m_alPCIconButton[j].Get((j == 2) ?
            ( pbb->m_alPCIconButton[j].Num() - k - 1) : k));

         // put a separator in?
         if (j && !k)
            AppendMenu (hSub, MF_SEPARATOR, 0, 0);

         // flags for the item
         DWORD dwFlags;
         dwFlags = 0;

         if (IsWindowEnabled (pBut->m_hWnd))
            dwFlags |= MF_ENABLED;
         else
            dwFlags |= MF_DISABLED | MF_GRAYED;

         if (pBut->FlagsGet() & (IBFLAG_BLUELIGHT | IBFLAG_REDARROW))
            dwFlags |= MF_CHECKED;

         // add it
         AppendMenu (hSub, dwFlags, pBut->m_dwID, (char*) pBut->m_memName.p);
      }

      // append this tot he main menu
      AppendMenu (hMenu, MF_ENABLED | MF_POPUP, (UINT_PTR) hSub, pszMajorName);
   }



   POINT p;
   GetCursorPos (&p);
   int iRet;
   iRet = TrackPopupMenu (hMenu, TPM_LEFTALIGN | TPM_VCENTERALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
      p.x, p.y, 0, m_hWnd, NULL);

   DestroyMenu (hMenu);
   if (!iRet)
      return;

   PostMessage (m_hWnd, WM_COMMAND, iRet, 0);
}

/****************************************************************************
ObjControlPointMovePage
*/
BOOL ObjControlPointMovePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POCPM pv = (POCPM)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      // set the values
      pPage->Message (ESCM_USER+82);
      break;

   case ESCM_USER+82:   // refresh display
      {
         // get the current values, since they may not be what we think
         OSCONTROL oc;
         pv->pos->ControlPointQuery (pv->dwControl, &oc);
         pv->pObject.Copy (&oc.pLocation);
         pv->pos->ObjectMatrixGet (&pv->mObjectToWorld);
         pv->mObjectToWorld.MultiplyLeft (&pv->mWorldToGrid);
         pv->mObjectToWorld.Invert4 (&pv->mWorldToObject);

         // convert coords?
         CPoint p;
         p.Copy (&pv->pObject);
         p.p[3] = 1;
         if (pv->fWorld)
            p.MultiplyLeft (&pv->mObjectToWorld);

         // write them out
         MeasureToString (pPage, L"transx", p.p[0]);
         MeasureToString (pPage, L"transy", p.p[1]);
         MeasureToString (pPage, L"transz", p.p[2]);

         // radio button
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"world");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->fWorld);
         pControl = pPage->ControlFind (L"object");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), !pv->fWorld);
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's the radio button then change coordinates
         if (!_wcsicmp(p->pControl->m_pszName, L"world") || !_wcsicmp(p->pControl->m_pszName, L"object")) {
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"world");
            if (pControl)
               pv->fWorld = pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+82);
         };
      }
      break;   // default

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (_wcsicmp(p->pControl->m_pszName, L"transx") && _wcsicmp(p->pControl->m_pszName, L"transy") &&
            _wcsicmp(p->pControl->m_pszName, L"transz"))
            break;

         // get all the values
         CPoint pNew;
         if (!MeasureParseString (pPage, L"transx", &pNew.p[0]))
            pNew.p[0] = 0;
         if (!MeasureParseString (pPage, L"transy", &pNew.p[1]))
            pNew.p[1] = 0;
         if (!MeasureParseString (pPage, L"transz", &pNew.p[2]))
            pNew.p[2] = 0;

         // convert to object space
         pNew.p[3] = 1;
         if (pv->fWorld)
            pNew.MultiplyLeft (&pv->mWorldToObject);

         CPoint pView;
         pView.Copy (&pv->pViewer);
         pView.MultiplyLeft (&pv->mWorldToObject);

         // set it
         pv->pos->ControlPointSet (pv->dwControl, &pNew, &pView);
      }
      return 0;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Set the object's control point";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
PMMoveVertPage
*/
BOOL PMMoveVertPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // get the object
         if (!pv->m_pPolyMeshOrig)
            return TRUE;
         
         // get the coords
         CPoint pLoc;
         if (!pv->m_pPolyMeshOrig->VertLocGet (pv->m_dwObjControlObject, &pLoc))
            return TRUE;

         // write them out
         MeasureToString (pPage, L"transx", pLoc.p[0]);
         MeasureToString (pPage, L"transy", pLoc.p[1]);
         MeasureToString (pPage, L"transz", pLoc.p[2]);

         // disable if have symmetry
         PCEscControl pControl;
         DWORD dwSym = pv->m_pPolyMeshOrig->SymmetryGet();
         if ((dwSym & 0x01) && (pLoc.p[0] == 0) && (pControl = pPage->ControlFind (L"transx")))
            pControl->Enable (FALSE);
         if ((dwSym & 0x02) && (pLoc.p[1] == 0) && (pControl = pPage->ControlFind (L"transy")))
            pControl->Enable (FALSE);
         if ((dwSym & 0x04) && (pLoc.p[2] == 0) && (pControl = pPage->ControlFind (L"transz")))
            pControl->Enable (FALSE);
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (_wcsicmp(p->pControl->m_pszName, L"transx") && _wcsicmp(p->pControl->m_pszName, L"transy") &&
            _wcsicmp(p->pControl->m_pszName, L"transz"))
            break;

         // get all the values
         CPoint pNew, pCur;
         if (!MeasureParseString (pPage, L"transx", &pNew.p[0]))
            pNew.p[0] = 0;
         if (!MeasureParseString (pPage, L"transy", &pNew.p[1]))
            pNew.p[1] = 0;
         if (!MeasureParseString (pPage, L"transz", &pNew.p[2]))
            pNew.p[2] = 0;
         if (!pv->m_pPolyMeshOrig->VertLocGet (pv->m_dwObjControlObject, &pCur))
            return TRUE;
         pNew.Subtract (&pCur);

         // clone over
         PCObjectPolyMesh pm = pv->PolyMeshObject2();
         if (!pm || !pv->m_pPolyMeshOrig)
            return TRUE;   // nothing

         pm->m_pWorld->ObjectAboutToChange (pm);
         pv->m_pPolyMeshOrig->CloneTo (&pm->m_PolyMesh);
         pm->m_PolyMesh.SelModeSet (0);   // just to make sure groupvertmove works
         pm->m_PolyMesh.GroupVertMove (pv->m_dwObjControlObject, &pNew, 1, &pv->m_dwObjControlObject);
         pm->m_pWorld->ObjectChanged (pm);
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Move polygon-mesh vertex";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
PMTextMoveVertPage
*/
BOOL PMTextMoveVertPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCObjectPolyMesh pm = pv->PolyMeshObject2();
         if (!pm)
            return TRUE;
         DWORD dwSurface = pm->ObjectSurfaceGetIndex (pv->m_dwPolyMeshCurText);

         // get the coords
         PCPMVert pVert = pm->m_PolyMesh.VertGet (pv->m_dwObjControlObject);
         if (!pVert)
            return TRUE;
         PTEXTUREPOINT ptp = pVert->TextureGet (pv->m_dwPolyMeshMoveSide);

         // write them out
         if (pm->m_PolyMesh.SurfaceInMetersGet(dwSurface)) {
            MeasureToString (pPage, L"transx", ptp->h);
            MeasureToString (pPage, L"transy", ptp->v);
         }
         else {
            DoubleToControl (pPage, L"transx", ptp->h);
            DoubleToControl (pPage, L"transy", ptp->v);
         }

      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (_wcsicmp(p->pControl->m_pszName, L"transx") && _wcsicmp(p->pControl->m_pszName, L"transy"))
            break;

         PCObjectPolyMesh pm = pv->PolyMeshObject2();
         if (!pm)
            return TRUE;
         DWORD dwSurface = pm->ObjectSurfaceGetIndex (pv->m_dwPolyMeshCurText);


         // get all the values
         TEXTUREPOINT tpNew;
         if (pm->m_PolyMesh.SurfaceInMetersGet(dwSurface)) {
            fp fTemp;
            if (!MeasureParseString (pPage, L"transx", &fTemp))
               tpNew.h = 0;
            else
               tpNew.h = fTemp;
            if (!MeasureParseString (pPage, L"transy", &fTemp))
               tpNew.v = 0;
            else
               tpNew.v = fTemp;
         }
         else {
            tpNew.h = DoubleFromControl (pPage, L"transx");
            tpNew.v = DoubleFromControl (pPage, L"transy");
         }

         PCPMVert pVert = pm->m_PolyMesh.VertGet (pv->m_dwObjControlObject);
         if (!pVert)
            return TRUE;
         PTEXTUREPOINT ptp = pVert->TextureGet (pv->m_dwPolyMeshMoveSide);

         pm->m_pWorld->ObjectAboutToChange (pm);
         *ptp = tpNew;
         pm->m_PolyMesh.m_fDirtyRender = TRUE;
         pm->m_pWorld->ObjectChanged (pm);
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Move polygon-mesh texture";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
OAINFOSort */
static PCPoint gpOAInfoSort = NULL; // if non-null then sort by distance
static int _cdecl OAINFOSort (const void *elem1, const void *elem2)
{
   OAINFO *pdw1, *pdw2;
   pdw1 = (OAINFO*) elem1;
   pdw2 = (OAINFO*) elem2;

   if (pdw1->info.fDefLowRank != pdw2->info.fDefLowRank)
      return (int)pdw1->info.fDefLowRank - (int)pdw2->info.fDefLowRank;

   if (gpOAInfoSort) {
      CPoint p1, p2;
      fp f1, f2;
      p1.Subtract (&pdw1->info.pLoc, gpOAInfoSort);
      p2.Subtract (&pdw2->info.pLoc, gpOAInfoSort);
      f1 = p1.Length();
      f2 = p2.Length();
      if (f1 < f2)
         return -1;
      else if (f1 > f2)
         return 1;
   }

   return _wcsicmp(pdw1->av.szName, pdw2->av.szName);
}

/****************************************************************************
ObjAttribPage
*/
BOOL ObjAttribPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCListFixed plOAINFO = (PCListFixed) pPage->m_pUserData;
   POAINFO poa = (POAINFO) plOAINFO->Get(0);
   DWORD dwNumOA = plOAINFO->Num();

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DWORD i;
         WCHAR szTemp[64];
         PCEscControl pControl;
         for (i = 0; i < dwNumOA; i++) {
            // get attribute afresh
            fp fVal = 0;
            poa[i].pos->AttribGet (poa[i].av.szName, &fVal);

            if (poa[i].info.dwType == AIT_BOOL) {
               swprintf (szTemp, L"b:%d", (int)i);
               pControl = pPage->ControlFind(szTemp);
               if (pControl)
                  pControl->AttribSetBOOL (Checked(), fVal > (poa[i].info.fMax + poa[i].info.fMin)/2);
            }
            else {
               // set edit
               swprintf (szTemp, L"e:%d", (int) i);
               if (poa[i].info.dwType == AIT_DISTANCE)
                  MeasureToString (pPage, szTemp, fVal);
               else if (poa[i].info.dwType == AIT_ANGLE)
                  AngleToControl (pPage, szTemp, fVal, TRUE);
               else
                  DoubleToControl (pPage, szTemp, fVal);

               // set the slider
               fVal = (fVal - poa[i].info.fMin) /
                  max(CLOSE, poa[i].info.fMax - poa[i].info.fMin) * 100.0;
               fVal = max(0, fVal);
               fVal = min(100, fVal);
               swprintf (szTemp, L"s:%d", (int) i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSetInt (Pos(), (int) fVal);
            }
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if ((psz[0] == L'b') && (psz[1] == L':')) {
            // index
            DWORD dwIndex = _wtoi(psz + 2);
            if (dwIndex >= dwNumOA)
               return TRUE;   // shouldnt happen

            // set the attribute
            poa[dwIndex].av.fValue = p->pControl->AttribGetBOOL(Checked()) ?
               poa[dwIndex].info.fMax : poa[dwIndex].info.fMin;
            poa[dwIndex].pos->AttribSetGroup (1, &poa[dwIndex].av);

            return TRUE;
         }
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;


         if ((psz[0] == L's') && (psz[1] == L':')) {
            // index
            DWORD dwIndex = _wtoi(psz + 2);
            if (dwIndex >= dwNumOA)
               return TRUE;   // shouldnt happen

            // get the value
            fp fVal;
            fVal = p->pControl->AttribGetInt (Pos()) / 100.0 *
               (poa[dwIndex].info.fMax - poa[dwIndex].info.fMin) + poa[dwIndex].info.fMin;

            // set the attribute
            poa[dwIndex].av.fValue = fVal;
            poa[dwIndex].pos->AttribSetGroup (1, &poa[dwIndex].av);

            // set the edit
            WCHAR szTemp[64];
            swprintf (szTemp, L"e:%d", (int) dwIndex);
            if (poa[dwIndex].info.dwType == AIT_DISTANCE)
               MeasureToString (pPage, szTemp, fVal);
            else if (poa[dwIndex].info.dwType == AIT_ANGLE)
               AngleToControl (pPage, szTemp, fVal, TRUE);
            else
               DoubleToControl (pPage, szTemp, fVal);


            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if ((psz[0] == L'e') && (psz[1] == L':')) {
            // index
            DWORD dwIndex = _wtoi(psz + 2);
            if (dwIndex >= dwNumOA)
               return TRUE;   // shouldnt happen

            // get the value
            fp fVal;
            fVal = 0;
            if (poa[dwIndex].info.dwType == AIT_DISTANCE)
               MeasureParseString (pPage, psz, &fVal);
            else if (poa[dwIndex].info.dwType == AIT_ANGLE)
               fVal = AngleFromControl (pPage, psz);
            else
               fVal = DoubleFromControl (pPage, psz);

            // set the attribute
            poa[dwIndex].av.fValue = fVal;
            poa[dwIndex].pos->AttribSetGroup (1, &poa[dwIndex].av);

            // set the slider
            PCEscControl pControl;
            WCHAR szTemp[64];
            fVal = (fVal - poa[dwIndex].info.fMin) /
               max(CLOSE, poa[dwIndex].info.fMax - poa[dwIndex].info.fMin) * 100.0;
            fVal = max(0, fVal);
            fVal = min(100, fVal);
            swprintf (szTemp, L"s:%d", (int) dwIndex);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int) fVal);


            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         PWSTR psz;
         psz = p->psz;

         if ((psz[0] == L'a') && (psz[1] == L':')) {
            // index
            DWORD dwIndex = _wtoi(psz + 2);
            if (dwIndex >= dwNumOA)
               return TRUE;   // shouldnt happen

            poa[dwIndex].fToggleRank = !poa[dwIndex].fToggleRank;
            poa[dwIndex].info.fDefLowRank = !poa[dwIndex].info.fDefLowRank;

            // Save changes to object so remember which attributes toggled
            CMem mem;
            MemZero (&mem);
            DWORD i;
            for (i = 0; i < dwNumOA; i++) {
               if (!poa[i].fToggleRank)
                  continue;   // only care about ones toggled

               // if not zero then prepend slash
               if (((PWSTR)mem.p)[0])
                  MemCat (&mem, L"\\");

               // name
               MemCat (&mem, poa[i].av.szName);
            }
            poa[dwIndex].pos->StringSet (OSSTRING_ATTRIBRANK, (PWSTR)mem.p);

            // exit and redraw
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Attributes";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TABLE")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < dwNumOA; i++) {
               // separator between high and low priority
               if (i && (poa[i].info.fDefLowRank != poa[i-1].info.fDefLowRank))
                  MemCat (&gMemTemp, L"<tr><td><br/></td></tr>");

               // row
               MemCat (&gMemTemp, L"<tr>");

               // show the name
               MemCat (&gMemTemp, L"<td width=50% valign=center>");

               // arrow down/up
               MemCat (&gMemTemp, L"<image transparent=true bmpresource=");
               MemCat (&gMemTemp, (int) (poa[i].info.fDefLowRank ? IDB_ARROWUP : IDB_ARROWDOWN));
               MemCat (&gMemTemp, L" href=\"a:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"\"><xHoverHelpShort>Click the arrow to move the attribute to the ");
               MemCat (&gMemTemp, poa[i].info.fDefLowRank ? L"top" : L"bottom");
               MemCat (&gMemTemp, L" group.</xHoverHelpShort></image>");


               // name
               if (poa[i].info.pszDescription)
                  MemCat (&gMemTemp, L"<a>");
               MemCatSanitize (&gMemTemp, poa[i].av.szName);
               if (poa[i].info.pszDescription) {
                  MemCat (&gMemTemp, L"<xHoverHelp>");
                  MemCatSanitize (&gMemTemp, (PWSTR) poa[i].info.pszDescription);
                  MemCat (&gMemTemp, L"</xHoverHelp>");
                  MemCat (&gMemTemp, L"</a>");
               }
               MemCat (&gMemTemp, L"</td>");

               // scrollbar or on/off
               if (poa[i].info.dwType == AIT_BOOL) {
                  MemCat (&gMemTemp, L"<td width=75% align=right><button checkbox=true style=x name=b:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");
               }
               else {   // edit and scrollbar
                  // edit
                  MemCat (&gMemTemp, L"<td width=25%><edit maxchars=32 width=100% selall=true name=e:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");

                  // scroll
                  MemCat (&gMemTemp, L"<td width=50%><scrollbar orient=horz min=0 max=100 width=100% name=s:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");
               }

               // end row
               MemCat (&gMemTemp, L"</tr>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************************
CHouseView::IsEmbeddedObjectSelected - This is a test to see if an embedded
object is selected. It's used for the move and rotate commands. If no embedded
objects are in the selection it returns 0. If an embedded object is selected,
but only one, then it returns 1, if more than one object selected and one or
more are embedded then 2

returns
   DWORD - 0 for no embedded, 1 for one object selected and its embedded, 2
      for many objects selected with one or more embedded
*/
DWORD CHouseView::IsEmbeddedObjectSelected (void)
{
   DWORD i, dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum (&dwNum);

   // find out if any are embeded
   GUID  gEmbed;
   for (i = 0; i < dwNum; i++) {
      PCObjectSocket pObj = m_pWorld->ObjectGet (pdw[i]);

      if (pObj->EmbedContainerGet (&gEmbed)) {
         // this is embedded
         if (dwNum == 1)
            return 1;
         else
            return 2;
      }
   }

   // else, none are
   return 0;
}


/***********************************************************************************
CHouseView::NumEmbeddedObjectsSelected - Returns the number of selected objects
that are also embedded objects.

returns
   DWORD - Number of selected objects that are embedded.
*/
DWORD CHouseView::NumEmbeddedObjectsSelected (void)
{
   DWORD i, dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum (&dwNum);

   // find out if any are embeded
   GUID  gEmbed;
   DWORD dwCount;
   dwCount = 0;
   for (i = 0; i < dwNum; i++) {
      PCObjectSocket pObj = m_pWorld->ObjectGet (pdw[i]);

      if (pObj->EmbedContainerGet (&gEmbed)) {
         dwCount++;
      }
   }

   // else, none are
   return dwCount;
}

/***********************************************************************************
CHouseView::LookAtPointFromDistance - Takes a point to look at, a point to look
from, and moves the camera so looking at the point.

inputs
   PCPoint     pAt - Look at
   PCPoint     pFrom - Look from
   double      fNewScale - Used if working with the flat camera model. 1.0 for same. .5 for zoom in 2x, etc.
returns
   none
*/
void CHouseView::LookAtPointFromDistance (PCPoint pAt, PCPoint pFrom, double fNewScale)
{
   // cheap solution, fill in a matrix to look at that point and call
   // CameraMoved();
   CMatrix m, mTrans;
   CPoint pDir, pUp, pRight;
   DWORD dwModel = m_apRender[0]->CameraModelGet();
   pDir.Subtract (pAt, pFrom);
   pDir.Normalize();
   pUp.Zero();
   pUp.p[2] = 1;
   pRight.CrossProd (&pDir, &pUp);
   if (pRight.Length() < CLOSE) {   // BUGFIX - Changed from esilon to close so zoomin from above would be ok
      // choose another direciton
      pUp.Zero();
      pUp.p[1] = 1;
      pRight.CrossProd (&pDir, &pUp);
   }
   pRight.Normalize();
   pUp.CrossProd (&pRight, &pDir);
   m.RotationFromVectors (&pRight, &pDir, &pUp);
   mTrans.Translation (pFrom->p[0], pFrom->p[1], pFrom->p[2]);
   m.MultiplyRight (&mTrans);

   // NOTE - Not dealing with center of rotation?

   if (dwModel == CAMERAMODEL_FLAT) {
      // get the camera info for the scale
      fp fLongitude, fTilt, fTiltY, fScale, fTransX, fTransY;
      CPoint pCenter;
      m_apRender[0]->CameraFlatGet (&pCenter, &fLongitude, &fTilt, &fTiltY, &fScale, &fTransX, &fTransY);

      // convert this
      CMatrix mInv;
      m.Invert4 (&mInv);
      CPoint p, p2;
      p.Zero();
      p.MultiplyLeft (&m);

      // convert this
      mInv.ToXYZLLT (&p2, &fLongitude, &fTilt, &fTiltY);

      p2.Copy (pAt);
      p2.p[3] = 1;
      p2.MultiplyLeft (&mInv);
      p.Zero();
      p.p[3] = 1;
      p.MultiplyLeft (&mInv);
      p2.Subtract (&p);
      fTransX = -p2.p[0];
      fTransY = -p2.p[2];

      pCenter.Zero();
      m_apRender[0]->CameraFlat (&pCenter, fLongitude, fTilt, fTiltY, fScale * fNewScale, fTransX, fTransY);
      RenderUpdate(WORLDC_CAMERAMOVED);
   }
   else
      CameraMoved(&m);

   CameraPosition();
   UpdateToolTipByPointerMode();
}

/***********************************************************************************
CHouseView::ButtonDown - Left button down message
*/
LRESULT CHouseView::ButtonDown (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton)
{
   DWORD dwPointerMode = m_adwPointerMode[dwButton];
   int iX, iY;
   iX = LOWORD(lParam);
   iY = HIWORD(lParam);
   DWORD dwX, dwY;

   // BUGFIX - If a button is already down then ignore the new one
   if (m_dwButtonDown)
      return 0;

   // if it's in one of the thumbnails then switch to that
   DWORD i;
   POINT pt;
   pt.x = iX;
   pt.y = iY;
   for (i = 1; i < VIEWTHUMB; i++) {
      if (!m_afThumbnailVis[i] || (i == m_dwThumbnailCur))
         continue;
      if (PtInRect (&m_arThumbnailLoc[i], pt)) {
         // clicked on a point in the thumbnail
         BOOL fControl = (GetKeyState (VK_CONTROL) < 0);

         // just in case, copy the old main to the old current thumbnail
         m_apRender[0]->CloneTo (m_apRender[m_dwThumbnailCur]);
         m_dwThumbnailCur = i;
         if (fControl)  // copy to the current view
            m_apRender[0]->CloneTo (m_apRender[m_dwThumbnailCur]);
         else
            m_apRender[m_dwThumbnailCur]->CloneTo (m_apRender[0]);

         // cause it to redraw
         QualitySync();
         UpdateCameraButtons ();
         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);

         return 0;
      }
   }

   // if it's not in the image are or don't have image mode then return
   if (!dwPointerMode || !PointInImage(iX, iY, &dwX, &dwY)) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return 0;
   }

   // for some click operations just want to act now and not set capture of anything
   switch (dwPointerMode) {
   case IDC_VIEWSETCENTERFLAT:
   case IDC_VIEWSETCENTER3D:
      {
         // expect user to click on point and then we adjust the center

         // find out what point clicked on
         fp fZ;
         CPoint pWorld;
         fZ = m_apRender[0]->PixelToZ (dwX, dwY);
         if (fZ > 1000)
            pWorld.Copy (&m_pCenterWorld); // click off object so reset point
         else
            m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pWorld);

         // get the old rendering info and change both the location of camera
         // and center of rotation
         CPoint pLoc, pCenter;
         pCenter.Zero();
         fp fX, fY, fFOV;
         if (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT)
            m_apRender[0]->CameraFlatGet (&pCenter, &fX, &fY, &fZ, &fFOV, &pLoc.p[0], &pLoc.p[2]);
         else
            m_apRender[0]->CameraPerspObjectGet (&pLoc, &pCenter, &fX, &fY, &fZ, &fFOV);

         // invert the rotation matrix
         CMatrix mRInv, mTrans;
         CMatrix x;
         m_apRender[0]->m_CameraMatrixRotOnlyAfterTrans.Invert (&mRInv);

         // multiply rotation by inverse translation that doing followed by rotation
         mTrans.Translation (pCenter.p[0] - pWorld.p[0], pCenter.p[1] - pWorld.p[1], pCenter.p[2] - pWorld.p[2]);
         x.Multiply (&m_apRender[0]->m_CameraMatrixRotOnlyAfterTrans, &mTrans);
         x.MultiplyLeft (&mRInv);

         // get the translation amounts
         pLoc.p[0] -= x.p[3][0];
         pLoc.p[1] -= x.p[3][1];
         pLoc.p[2] -= x.p[3][2];

         // set it
         if (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT)
            m_apRender[0]->CameraFlat (&pWorld, fX, fY, fZ, fFOV, pLoc.p[0], pLoc.p[2]);
         else
            m_apRender[0]->CameraPerspObject (&pLoc, &pWorld, fX, fY, fZ, fFOV);
         CameraPosition ();

         // Need some sort of beep to know it worked
         EscChime (ESCCHIME_INFORMATION);

         // redraw - even though should be exactly the same
         RenderUpdate(WORLDC_CAMERAMOVED);
         return 0;
      }
      break;

   case IDC_CLIPPOINT:
      ClipPointUI (dwX, dwY);
      return 0;

   case IDC_GRIDFROMPOINT:
      GridFromPointUI (dwX, dwY);
      return 0;


   case IDC_POSITIONPASTE:
   case IDC_OBJECTPASTE:
      {

         // clear out current selection
         // ClipboardDelete (FALSE);- Dont do this because it doesnt seem intuitive

         // move the pasted stuff out of purgatory
         MoveOutOfPurgatory(dwX, dwY, !(GetKeyState (VK_CONTROL) < 0));

         // remember the undo
         m_pWorld->UndoRemember();

         // change the pointer mode
         SetPointerMode (IsEmbeddedObjectSelected() ? IDC_MOVEEMBED : IDC_MOVENSEW, 0);
         return 0;
      }
      break;

   case IDC_SELINDIVIDUAL:
      {
         if (m_dwViewWhat == VIEWWHAT_POLYMESH) {
            PolyMeshSelIndividual (dwX, dwY, (wParam & (MK_CONTROL | MK_SHIFT)) ? TRUE : FALSE);
            return 0; // done
         }
         else if (m_dwViewWhat == VIEWWHAT_BONE) {
            BoneSelIndividual (dwX, dwY);
            return 0; // done
         }

         // if control/shift are not held down then clear the old selection
         if (!(wParam & (MK_CONTROL | MK_SHIFT)))
            m_pWorld->SelectionClear();

         // BUGFIX - so that clickin on a wall is easier
         DWORD dwAllID;
         dwAllID = NearSelect (dwX, dwY);
         // find out the object and select it
         //PIMAGEPIXEL pip;
         //pip = m_aImage[0].Pixel(dwX, dwY);
         if (HIWORD(dwAllID/*pip->dwID*/)) {
            DWORD dwID;
            dwID = HIWORD(dwAllID/*pip->dwID*/)-1;   // since image object ID's 1-based

            if (m_pWorld->SelectionExists (dwID))
               m_pWorld->SelectionRemove (dwID);
            else
               m_pWorld->SelectionAdd (dwID);
         }

         return 0;   // all done
      }
      break;

   case IDC_SELREGION:
      // if control/shift are not held down then clear the old selection
      if (!(wParam & (MK_CONTROL | MK_SHIFT))) {
         if (m_dwViewWhat == VIEWWHAT_POLYMESH) {
            PCObjectPolyMesh pm = PolyMeshObject2 ();
            if (pm) {
               pm->m_pWorld->ObjectAboutToChange (pm);
               pm->m_PolyMesh.SelClear();
               pm->m_pWorld->ObjectChanged (pm);
            }
         }
         else
            m_pWorld->SelectionClear();
      }
      // continue on
      break;

   case IDC_PMDELETE:
      {
         // get what clicked on
         if (!PolyMeshObjectFromPixel(dwX, dwY))
            return 0;

         // object
         PCObjectPolyMesh pm = PolyMeshObject2();
         if (!pm)
            return 0;

         // clone and modify the clone
         if (!m_pPolyMeshOrig)
            m_pPolyMeshOrig = new CPolyMesh;
         if (!m_pPolyMeshOrig)
            return 0;
         pm->m_PolyMesh.CloneTo (m_pPolyMeshOrig);

         // figure out the number of points...
         DWORD dwNum;
         DWORD *padw;
         if (m_adwPolyMeshMove[0] == -1)
            padw = m_pPolyMeshOrig->SelEnum(&dwNum);
         else {
            padw = m_adwPolyMeshMove;
            dwNum = 1;
         }

         m_pPolyMeshOrig->GroupDelete (m_pPolyMeshOrig->SelModeGet(), dwNum, padw);

         // make sure there are sides left
         if (!m_pPolyMeshOrig->SideGet(0)) {
            EscMessageBox (m_hWnd, ASPString(),
               L"The delete attempt failed because it would have deleted the entire polygon mesh.",
               NULL,
               MB_ICONWARNING | MB_OK);
            return 0;
         }

         // accept
         //pm->m_pWorld->UndoRemember ();
         pm->m_pWorld->ObjectAboutToChange(pm);
         m_pPolyMeshOrig->CloneTo (&pm->m_PolyMesh);
         pm->m_pWorld->ObjectChanged(pm);
         //pm->m_pWorld->UndoRemember ();

      }
      return 0;

   case IDC_BONESPLIT:
      {
         DWORD dwFind;
         PCObjectBone pb = BoneObject2(&dwFind);
         if (!pb)
            return 0;

         // pixel
         PIMAGEPIXEL pip = m_aImage[0].Pixel (dwX, dwY);
         if (HIWORD(pip->dwID) != dwFind+1) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         if (!pb->SplitBone (LOWORD(pip->dwID)))
            BeepWindowBeep (ESCBEEP_DONTCLICK);

      }
      return 0;

   case IDC_BONEDELETE:
      {
         DWORD dwFind;
         PCObjectBone pb = BoneObject2(&dwFind);
         if (!pb)
            return 0;

         // pixel
         PIMAGEPIXEL pip = m_aImage[0].Pixel (dwX, dwY);
         if (HIWORD(pip->dwID) != dwFind+1) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         if (!pb->DeleteBone (LOWORD(pip->dwID)))
            BeepWindowBeep (ESCBEEP_DONTCLICK);

      }
      return 0;

   case IDC_BONEDISCONNECT:
      {
         DWORD dwFind;
         PCObjectBone pb = BoneObject2(&dwFind);
         if (!pb)
            return 0;

         // pixel
         PIMAGEPIXEL pip = m_aImage[0].Pixel (dwX, dwY);
         if (HIWORD(pip->dwID) != dwFind+1) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         if (!pb->DisconnectBone (LOWORD(pip->dwID)))
            BeepWindowBeep (ESCBEEP_DONTCLICK);
         else
            EscChime (ESCCHIME_INFORMATION);

      }
      return 0;

   case IDC_PMEDGESPLIT:
      {
         // get what clicked on
         if (!PolyMeshObjectFromPixel(dwX, dwY))
            return 0;

         // object
         PCObjectPolyMesh pm = PolyMeshObject2();
         if (!pm)
            return 0;

         // figure out the number of points...
         DWORD dwNum;
         DWORD *padw;
         if (m_adwPolyMeshMove[0] == -1)
            padw = m_pPolyMeshOrig->SelEnum(&dwNum);
         else {
            padw = m_adwPolyMeshMove;
            dwNum = 1;
         }

         // accept
         //pm->m_pWorld->UndoRemember ();
         pm->m_pWorld->ObjectAboutToChange(pm);
         pm->m_PolyMesh.EdgeSplit (dwNum, padw);
         pm->m_pWorld->ObjectChanged(pm);
         //pm->m_pWorld->UndoRemember ();

      }
      return 0;

   case IDC_PMSIDEDISCONNECT:
      {
         BOOL fControl = (GetKeyState (VK_CONTROL) < 0);

         // get what clicked on
         if (!PolyMeshObjectFromPixel(dwX, dwY))
            return 0;

         // object
         PCObjectPolyMesh pm = PolyMeshObject2();
         if (!pm)
            return 0;

         // figure out the number of points...
         CListFixed lPoints;
         DWORD dwNum;
         DWORD *padw;
         lPoints.Init (sizeof(DWORD));
         if (m_adwPolyMeshMove[0] == -1)
            padw = pm->m_PolyMesh.SelEnum(&dwNum);
         else {
            padw = m_adwPolyMeshMove;
            dwNum = 1;
         }

         // clone and modify the clone
         if (!m_pPolyMeshOrig)
            m_pPolyMeshOrig = new CPolyMesh;
         if (!m_pPolyMeshOrig)
            return 0;
         pm->m_PolyMesh.CloneTo (m_pPolyMeshOrig);

         BOOL fRet;
         if (fControl) {
            // clone the polygon mesh
            PCObjectSocket pos = pm->Clone();
            if (!pos)
               return 0;

            OSMPOLYMESH os;
            memset (&os, 0, sizeof(os));
            pos->Message (OSM_POLYMESH, &os);
            PCObjectPolyMesh pmNew = os.ppm;
            if (!pmNew) {
               pos->Delete();
               return 0;
            }
            pmNew->WorldSet (NULL); // just so doesn't notify

            // in pmNew keep the given objects
            fRet = pmNew->m_PolyMesh.SideKeep (dwNum, padw);
            if (!fRet) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"The side(s) could not be disconnected.",
                  L"You may have selected a combination of sides that would break symmetry, or you "
                  L"may have tried to disconnect the entire object, leaving nothing.",
                  MB_ICONWARNING | MB_OK);
               pmNew->Delete();
               return 0;
            }

            // delete the old objects
            m_pPolyMeshOrig->GroupDelete (2, dwNum, padw);
            if (!m_pPolyMeshOrig->SideGet(0)) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"The side(s) could not be disconnected.",
                  L"You may have selected a combination of sides that would break symmetry, or you "
                  L"may have tried to disconnect the entire object, leaving nothing.",
                  MB_ICONWARNING | MB_OK);
               pmNew->Delete();
               return 0;
            }

            // create the new object
            pm->m_pWorld->ObjectAdd (pmNew);
         }
         else {
            fRet = m_pPolyMeshOrig->SideDisconnect (dwNum, padw);
            // make sure there are sides left
            if (!fRet) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"The side(s) could not be disconnected.",
                  L"You may have selected a combination of sides that would break symmetry.",
                  MB_ICONWARNING | MB_OK);
               return 0;
            }
         }

         // accept
         //pm->m_pWorld->UndoRemember ();
         pm->m_pWorld->ObjectAboutToChange(pm);
         m_pPolyMeshOrig->CloneTo (&pm->m_PolyMesh);
         pm->m_pWorld->ObjectChanged(pm);
         //pm->m_pWorld->UndoRemember ();


      }
      return 0;

   case IDC_PMTEXTCOLLAPSE:
      {
         // get what clicked on
         if (!PolyMeshObjectFromPixel(dwX, dwY))
            return 0;

         // object
         PCObjectPolyMesh pm = PolyMeshObject2();
         if (!pm)
            return 0;

         // figure out the number of points...
         CListFixed lPoints;
         DWORD dwNum;
         DWORD *padw;
         lPoints.Init (sizeof(DWORD));
         if (m_adwPolyMeshMove[0] == -1)
            padw = pm->m_PolyMesh.SelEnum(&dwNum);
         else {
            padw = m_adwPolyMeshMove;
            dwNum = 1;
         }
         switch (pm->m_PolyMesh.SelModeGet()) {
         case 0: // vertex
         default:
            lPoints.Init (sizeof(DWORD), padw, dwNum);
            break;
         case 1:  // edge
            pm->m_PolyMesh.EdgesToVert (padw, dwNum, &lPoints);
            break;
         case 2:  // side
            pm->m_PolyMesh.SidesToVert (padw, dwNum, &lPoints);
            break;
         }

         DWORD dwSurface = pm->ObjectSurfaceGetIndex (m_dwPolyMeshCurText);

         // accept
         BOOL fRet;
         //pm->m_pWorld->UndoRemember ();
         pm->m_pWorld->ObjectAboutToChange(pm);
         fRet = pm->m_PolyMesh.TextureCollapse (lPoints.Num(), (DWORD*)lPoints.Get(0), dwSurface);
         pm->m_pWorld->ObjectChanged(pm);
         //pm->m_pWorld->UndoRemember ();

         if (!fRet) {
            EscMessageBox (m_hWnd, ASPString(),
               L"Collapse failed.",
               L"The points that you chose to collapse may not be on the current texture.",
               MB_ICONWARNING | MB_OK);
            return 0;
         }
      }
      return 0;

   case IDC_PMCOLLAPSE:
      {
         // get what clicked on
         if (!PolyMeshObjectFromPixel(dwX, dwY))
            return 0;

         // object
         PCObjectPolyMesh pm = PolyMeshObject2();
         if (!pm)
            return 0;

         // clone and modify the clone
         if (!m_pPolyMeshOrig)
            m_pPolyMeshOrig = new CPolyMesh;
         if (!m_pPolyMeshOrig)
            return 0;
         pm->m_PolyMesh.CloneTo (m_pPolyMeshOrig);

         // figure out the number of points...
         CListFixed lPoints;
         DWORD dwNum;
         DWORD *padw;
         lPoints.Init (sizeof(DWORD));
         if (m_adwPolyMeshMove[0] == -1)
            padw = m_pPolyMeshOrig->SelEnum(&dwNum);
         else {
            padw = m_adwPolyMeshMove;
            dwNum = 1;
         }
         switch (m_pPolyMeshOrig->SelModeGet()) {
         case 0: // vertex
         default:
            lPoints.Init (sizeof(DWORD), padw, dwNum);
            break;
         case 1:  // edge
            m_pPolyMeshOrig->EdgesToVert (padw, dwNum, &lPoints);
            break;
         case 2:  // side
            m_pPolyMeshOrig->SidesToVert (padw, dwNum, &lPoints);
            break;
         }

         // if there aren't enough points then error
         if (lPoints.Num() < 2) {
            EscMessageBox (m_hWnd, ASPString(),
               L"You need at least two points to collapse them.",
               NULL,
               MB_ICONWARNING | MB_OK);
            return 0;
         }

         // try to collapse them
         if (!m_pPolyMeshOrig->VertCollapse (lPoints.Num(), (DWORD*)lPoints.Get(0))) {
            EscMessageBox (m_hWnd, ASPString(),
               L"The collapse attempt failed because it would have broken your symmetry settings.",
               NULL,
               MB_ICONWARNING | MB_OK);
            return 0;
         }

         // make sure there are sides left
         if (!m_pPolyMeshOrig->SideGet(0)) {
            EscMessageBox (m_hWnd, ASPString(),
               L"The collapse attempt failed because it would have deleted the entire polygon mesh.",
               NULL,
               MB_ICONWARNING | MB_OK);
            return 0;
         }

         // accept
         //pm->m_pWorld->UndoRemember ();
         pm->m_pWorld->ObjectAboutToChange(pm);
         m_pPolyMeshOrig->CloneTo (&pm->m_PolyMesh);
         pm->m_pWorld->ObjectChanged(pm);
         //pm->m_pWorld->UndoRemember ();

      }
      return 0;

   case IDC_PMNEWSIDE:
      {
         // figure out where clicked on
         DWORD dwVert = PolyMeshVertFromPixel (dwX, dwY);

         // if it's the same as the last object then beep
         DWORD *padwVert = (DWORD*) m_lPolyMeshVert.Get(0);
         if (m_lPolyMeshVert.Num() && (padwVert[m_lPolyMeshVert.Num()-1] == dwVert))
            dwVert = -1;

         if (dwVert == -1) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         PCObjectPolyMesh pm = PolyMeshObject2();
         if (!pm)
            return 0;

         // first, invalidate old rect
         RECT r;
         POINT pt;
         pt.x = iX;
         pt.y = iY;
         PolyMeshVertToRect (pm, pt, &r);
         if (r.left != r.right)
            InvalidateRect (m_hWnd, &r, FALSE);

         
         if (m_lPolyMeshVert.Num() && (padwVert[0] == dwVert)) {
            // just clicked on starting vertex so create polygon
            pm->m_pWorld->ObjectAboutToChange(pm);
            BOOL fRet = pm->m_PolyMesh.SideAddChecks (m_lPolyMeshVert.Num(), (DWORD*)m_lPolyMeshVert.Get(0));
            pm->m_pWorld->ObjectChanged(pm);
            m_lPolyMeshVert.Clear();

            if (!fRet)
               EscMessageBox (m_hWnd, ASPString(),
                  L"The polygon couldn't be created.",
                  L"The edges you clicked on may already be connected on both sides, "
                  L"or you may have only clicked on two points.",
                  MB_ICONWARNING | MB_OK);
         }
         else  // new vert for list
            m_lPolyMeshVert.Add (&dwVert);

         // invalidate new rect
         PolyMeshVertToRect (pm, pt, &r);
         if (r.left != r.right)
            InvalidateRect (m_hWnd, &r, FALSE);
      }
      return 0;

   case IDC_PMSIDESPLIT:
      {
         // see what click on
         m_adwPolyMeshSplit[0][0] = m_adwPolyMeshSplit[1][0] = -1;
         m_adwPolyMeshSplit[1][1] = m_adwPolyMeshSplit[0][1] = -1;

         m_adwPolyMeshSplit[0][0] = PolyMeshVertFromPixel (dwX, dwY);
         if (m_adwPolyMeshSplit[0][0] == -1)
            if (!PolyMeshEdgeFromPixel (dwX, dwY, &m_adwPolyMeshSplit[0][0])) {
               // clicked on wrong thing so beep
               BeepWindowBeep (ESCBEEP_DONTCLICK);
               return 0;
            }

         m_rPolyMeshLastSplitLine.left = -12345;
      }
      break;

   case IDC_BONESCALE:
   case IDC_BONEROTATE:
   case IDC_BONENEW:
      {
         if (!BonePrepForDrag (dwX, dwY, FALSE, FALSE))
            return 0;

         BOOL fControl = (GetKeyState (VK_CONTROL) < 0);
         if (fControl && (dwPointerMode == dwPointerMode))
            m_dwObjControlObject = -1; // so modify everything

         if (dwPointerMode == IDC_BONENEW) {
            PCObjectBone pb = BoneObject2();
            if (!pb)
               return 0;

            // note for bone clicked on so know where last is
            m_rPolyMeshLastSplitLine.left = -12345;   

            // figure out if clicked on start or end of bone
            CPoint p;
            PIMAGEPIXEL pip = m_aImage[0].Pixel (dwX, dwY);
            m_apRender[0]->PixelToWorldSpace (dwX, dwY, pip->fZ, &p);
            p.p[3] = 1;

            // convert from world to object space
            CMatrix m, mInv;
            pb->ObjectMatrixGet (&m);
            m.Invert4 (&mInv);
            p.MultiplyLeft (&mInv);

            // figure out which is closest to, start or end...
            PCBone *ppb = (PCBone*) pb->m_lBoneList.Get(0);
            DWORD dwNum = pb->m_lBoneList.Num();
            if (m_dwObjControlObject >= dwNum)
               return 0;
            PCBone pBone = ppb[m_dwObjControlObject];
            CPoint pDist;
            fp f1, f2;
            pDist.Subtract (&pBone->m_pStartOS, &p);
            f1 = pDist.Length();
            pDist.Subtract (&pBone->m_pEndOS, &p);
            f2 = pDist.Length();
            if (f1 < f2) {
               // go to parent
               DWORD i;
               m_dwObjControlObject = -1;
               for (i = 0; i < dwNum; i++)
                  if (ppb[i] == pBone->m_pParent) {
                     m_dwObjControlObject = i;
                     break;
                  }
            }  // if closesr to start
         } // if pointer bone
      }
      break;

   case IDC_PMMOVEPERP:
   case IDC_PMMOVEPERPIND:
   case IDC_PMTAILORNEW:
   case IDC_PMMOVESCALE:
   case IDC_PMMOVEROTPERP:
   case IDC_PMMOVEROTNONPERP:
   case IDC_PMMOVEINOUT:
   case IDC_PMMOVEANYDIR:
   case IDC_PMSIDEINSET:
   case IDC_PMSIDEEXTRUDE:
   case IDC_PMBEVEL:
   case IDC_PMTEXTSCALE:
   case IDC_PMTEXTMOVE:
   case IDC_PMTEXTROT:
      {
         if (!PolyMeshObjectFromPixel(dwX, dwY))
            return 0;

         BOOL fInOut = ((dwPointerMode == IDC_PMMOVEINOUT) || (dwPointerMode == IDC_PMMAGVIEWER));
         BOOL fIgnoreCantMove = ((dwPointerMode == IDC_PMSIDEINSET) ||
            (dwPointerMode == IDC_PMSIDEEXTRUDE) ||
            (dwPointerMode == IDC_PMBEVEL) ||
            (dwPointerMode == IDC_PMTEXTMOVE) ||
            (dwPointerMode == IDC_PMTEXTROT) ||
            (dwPointerMode == IDC_PMTEXTSCALE));

         if (!PolyMeshPrepForDrag (dwX, dwY, fInOut, !fIgnoreCantMove))
            return 0;
      }
      break;

      
   case IDC_PMMAGANY:
   case IDC_PMMAGNORM:
   case IDC_PMMAGVIEWER:
   case IDC_PMMAGPINCH:
   case IDC_PMORGSCALE:
   case IDC_PMMORPHSCALE:
      {

         // make sure clicked on right object
         PIMAGEPIXEL pip = m_aImage[0].Pixel (dwX, dwY);
         DWORD dwFind;
         PCObjectPolyMesh pm = PolyMeshObject2 (&dwFind);
         if (!pm || (HIWORD(pip->dwID) != dwFind+1)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return FALSE;
         }

         BOOL fInOut = ((dwPointerMode == IDC_PMMOVEINOUT) || (dwPointerMode == IDC_PMMAGVIEWER));

         BOOL fScale;
         fScale = ((dwPointerMode == IDC_PMORGSCALE) || (dwPointerMode == IDC_PMMORPHSCALE));
         if (!PolyMeshPrepForDrag (dwX, dwY, fInOut, !fScale))
            return 0;
      }
      break;

   case IDC_OBJCONTROLUD:
   case IDC_OBJCONTROLNSEW:
   case IDC_OBJCONTROLINOUT:
   case IDC_OBJCONTROLNSEWUD:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
         DWORD dwCP = LOWORD(pip->dwID) - 0xf000;
         BOOL fOK = (LOWORD(pip->dwID) >= 0xf000);

         // BUGFIX - If not directly over CP then find closest one
         if (!fOK && HIWORD(pip->dwID)) {
            dwCP = NearestCP (dwX, dwY, HIWORD(pip->dwID)-1);
            if (dwCP != -1)
               fOK = TRUE;
         }

         if (!fOK || !HIWORD(pip->dwID)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // else, remember this
         PCObjectSocket pos;
         m_dwObjControlObject = HIWORD(pip->dwID)-1;
         pos = m_pWorld->ObjectGet (m_dwObjControlObject);
         if (!pos)
            return 0;   // cant use this either
         m_dwObjControlID = dwCP;

         // get the position
         OSCONTROL osc;
         if (!pos->ControlPointQuery (m_dwObjControlID, &osc))
            return 0; // error

         // BUGFIX - If it's a button then don't go into drag mode - just send click no
         if (osc.fButton) {
            CPoint pViewer;
            m_apRender[0]->PixelToWorldSpace (dwX, dwY,
               (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pViewer);
            pos->ControlPointSet (m_dwObjControlID, &osc.pLocation, &pViewer);
            return 0;
         }


         // and where we click, converted to object space
         m_pObjControlOrig.Copy (&osc.pLocation);
         
         // and where is this pixel in psace
         fp fZ;
         m_fObjControlOrigZ = fZ = m_apRender[0]->PixelToZ (dwX,dwY);
         m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &m_pObjControlClick);
         CMatrix m, m2;
         pos->ObjectMatrixGet (&m2);
         m2.Invert4(&m);
         m_pObjControlClick.MultiplyLeft (&m);

         if (wParam & MK_CONTROL) {
            OCPM ocpm;
            ocpm.pos = pos;
            ocpm.dwControl = m_dwObjControlID;
            ocpm.pObject.Copy (&osc.pLocation);
            ocpm.fWorld = TRUE;
            pos->ObjectMatrixGet (&ocpm.mObjectToWorld);
            ocpm.mWorldToGrid.Copy (&m_mWorldToGrid);
            ocpm.mObjectToWorld.MultiplyLeft (&m_mWorldToGrid);
            ocpm.mObjectToWorld.Invert4 (&ocpm.mWorldToObject);
            //ocpm.mWorldToObject.Copy (&m2);
            m_apRender[0]->PixelToWorldSpace (dwX, dwY,
               (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &ocpm.pViewer);

            // turn off the tooltip
            m_dwObjControlObject = (DWORD)-1;
            UpdateToolTipByPointerMode();

            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (m_hWnd, &r);

            cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;

            // start with the first page
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJCONTROLPOINTMOVE, ObjControlPointMovePage, &ocpm);
            return 0;   // had control down
         }

      }
      break;


   case IDC_OBJPAINT:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
         BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

         if (!fOK || !HIWORD(pip->dwID)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // else, remember this
         PCObjectSocket pos;
         m_dwObjControlObject = HIWORD(pip->dwID)-1;
         pos = m_pWorld->ObjectGet (m_dwObjControlObject);
         if (!pos)
            return 0;   // cant use this either
         m_dwObjControlID = LOWORD(pip->dwID);

         // get the color just to make sure it works
         PCObjectSurface ps;
         ps = pos->SurfaceGet (LOWORD(pip->dwID));
         if (!ps)
            return 0;

         // set the object to -1 so no tooltips popping up
         m_dwObjControlObject = -1;

         // BUGFIX - If press SHIFT then paint only section
         BOOL fMakeSmall;
         fMakeSmall = (GetKeyState (VK_SHIFT) < 0);
         if (fMakeSmall) {
            OSMCLICKONOVERLAY oco;
            memset (&oco,0 ,sizeof(oco));
            oco.dwID = LOWORD(pip->dwID);
            if (pos->Message (OSM_CLICKONOVERLAY, &oco))
               fMakeSmall = FALSE;
         }
         if (fMakeSmall) {
            // have clicked on something other than an overlay, see where the
            // intersection occurs
            fp fZ;
            CPoint pEye, pClick;
            fZ = m_apRender[0]->PixelToZ (dwX,dwY);
            m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pClick);
            m_apRender[0]->PixelToWorldSpace (dwX, dwY,
               (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);
            CMatrix m, m2;
            pos->ObjectMatrixGet (&m2);
            m2.Invert4(&m);
            pEye.MultiplyLeft (&m);
            pClick.MultiplyLeft (&m);

            // NOTE: This is a bit of a leap calling the container and asking for HV locaiton
            // of where clicked, but will work since really only use this functionality
            // for surfaces -where have embedded objects

            // see where clicking intersects the surface
            if (!pos->ContHVQuery (&pEye, &pClick, LOWORD(pip->dwID), NULL, &m_MoveEmbedClick))
               fMakeSmall = FALSE;
         }
         
         DWORD dwIDToPaint;
         dwIDToPaint = LOWORD(pip->dwID);
         if (fMakeSmall) {
            // if got here then clicked on a surface. Clicked outside an overlay area.
            // want to find where the overlay intersects with the surrounding walls then
            // and have a new overlay created
            OSMNEWOVERLAYFROMINTER ofi;
            memset (&ofi, 0, sizeof(ofi));
            ofi.dwIDClick = LOWORD(pip->dwID);
            ofi.tpClick = m_MoveEmbedClick;
            if (pos->Message (OSM_NEWOVERLAYFROMINTER, &ofi)) {
               dwIDToPaint = ofi.dwIDNew;
               ps->m_dwID = dwIDToPaint;
               ps->m_szScheme[0] = 0;  // so dont have a scheme to this
               pos->SurfaceSet (ps);   // so that will default to same color as previous
            }
         }

         // BUGFIX - If press CONTROL then just use the same texture as last time
         ObjPaintDialog (DEFAULTRENDERSHARD, dwIDToPaint, pos, this, GetKeyState (VK_CONTROL) < 0);

         // delete the object
         delete ps;

         // remember this as an undo point
         m_pWorld->UndoRemember();
         // don capture this
         return 0;
      }
      break;

   case IDC_OBJMERGE:
   case IDC_PMMERGE:
   case IDC_BONEMERGE:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);

         if (!HIWORD(pip->dwID)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // else, remember this
         PCObjectSocket pos;
         m_dwObjControlObject = HIWORD(pip->dwID)-1;
         pos = m_pWorld->ObjectGet (m_dwObjControlObject);
         if (!pos)
            return 0;   // cant use this either
         m_dwObjControlID = LOWORD(pip->dwID);

         if (dwPointerMode == IDC_PMMERGE)
            PolyMeshObjectMerge (m_dwObjControlObject);
         else if (dwPointerMode == IDC_BONEMERGE)
            BoneObjectMerge (m_dwObjControlObject);
         else
            ObjectMerge(m_dwObjControlObject);

         // remember this as an undo point
         m_pWorld->UndoRemember();
         // don capture this
         return 0;
      }
      break;


   case IDC_OBJATTACH:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);

         if (!HIWORD(pip->dwID)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // else, remember this
         PCObjectSocket pos;
         m_dwObjControlObject = HIWORD(pip->dwID)-1;
         pos = m_pWorld->ObjectGet (m_dwObjControlObject);
         if (!pos)
            return 0;   // cant use this either
         m_dwObjControlID = LOWORD(pip->dwID);

         ObjectAttach (m_dwObjControlObject, dwX, dwY);

         // remember this as an undo point
         m_pWorld->UndoRemember();
         // don capture this
         return 0;
      }
      break;

   case IDC_OBJONOFF:
      {
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
         BOOL fOK = (LOWORD(pip->dwID) < 0x1000);
         if (fOK)
            m_dwObjControlObject = HIWORD(pip->dwID)-1;
         else
            m_dwObjControlObject = -1;

         if (m_dwObjControlObject) {
            PCObjectSocket pos;
            pos = m_pWorld->ObjectGet (m_dwObjControlObject);
            if (!pos || ((m_fOpenStart = pos->TurnOnGet()) < 0))
               m_dwObjControlObject = -1;
         }

         if (m_dwObjControlObject == -1) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // remember this as an undo point
         m_pWorld->UndoRemember();
      }
      break;


   case IDC_OBJOPENCLOSE:
      {
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
         BOOL fOK = (LOWORD(pip->dwID) < 0x1000);
         if (fOK)
            m_dwObjControlObject = HIWORD(pip->dwID)-1;
         else
            m_dwObjControlObject = -1;

         if (m_dwObjControlObject) {
            PCObjectSocket pos;
            pos = m_pWorld->ObjectGet (m_dwObjControlObject);
            if (!pos || ((m_fOpenStart = pos->OpenGet()) < 0))
               m_dwObjControlObject = -1;
         }

         if (m_dwObjControlObject == -1) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // remember this as an undo point
         m_pWorld->UndoRemember();

      }
      break;


   case IDC_OBJINTELADJUST:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
         BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

         if (!fOK || !HIWORD(pip->dwID)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // else, remember this
         PCObjectSocket pos;
         m_dwObjControlObject = HIWORD(pip->dwID)-1;
         pos = m_pWorld->ObjectGet (m_dwObjControlObject);
         if (!pos) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }
         if (!pos->IntelligentAdjust(FALSE)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         HCURSOR hCursor;
         hCursor = SetCursor (LoadCursor (NULL, IDC_WAIT));
         pos->IntelligentAdjust (TRUE);
         SetCursor (hCursor);


         // remember this as an undo point
         m_pWorld->UndoRemember();

         EscChime (ESCCHIME_INFORMATION);

         // don capture this
         return 0;
      }
      break;


   case IDC_OBJDECONSTRUCT:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
         BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

         if (!fOK || !HIWORD(pip->dwID)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // else, remember this
         PCObjectSocket pos;
         m_dwObjControlObject = HIWORD(pip->dwID)-1;
         pos = m_pWorld->ObjectGet (m_dwObjControlObject);
         if (!pos) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }
         // test to see if can deconstruct
         if (!pos->Deconstruct(FALSE)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // deconstruct
         if (pos->Deconstruct (TRUE)) {
            m_pWorld->ObjectRemove (m_dwObjControlObject);
         }

         // remember this as an undo point
         m_pWorld->UndoRemember();

         EscChime (ESCCHIME_INFORMATION);
         // don capture this
         return 0;
      }
      break;

   case IDC_PMTEXTMOVEDIALOG:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PCObjectPolyMesh pm = PolyMeshObject2();
         m_dwObjControlObject  = PolyMeshVertFromPixel (dwX, dwY);
         if (m_dwObjControlObject == -1) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }
         m_dwPolyMeshMoveSide = (m_aImage[0].Pixel(dwX,dwY))->dwIDPart;

         // if the side isn't on the currently selected list then dont allow
         DWORD dwSurface = pm->ObjectSurfaceGetIndex (m_dwPolyMeshCurText);
         PCPMSide ps = pm->m_PolyMesh.SideGet (m_dwPolyMeshMoveSide);
         if (!ps || (ps->m_dwSurfaceText != dwSurface)) {
            EscMessageBox (m_hWnd, ASPString(),
               L"You can only modify vertices that use the current texture.",
               NULL,
               MB_ICONINFORMATION | MB_OK);
            return 0;
         }

         pm->m_pWorld->UndoRemember();

         UpdateToolTipByPointerMode();

         CEscWindow cWindow;
         RECT r;
         DialogBoxLocation (m_hWnd, &r);

         cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
         PWSTR pszRet;

         // start with the first page
         pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPMTEXTMOVEVERT, PMTextMoveVertPage, this);
         m_dwObjControlObject = -1;
         pm->m_pWorld->UndoRemember();
         return 0;   // had control down
      }
      break;



   case IDC_PMMOVEDIALOG:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PCObjectPolyMesh pm = PolyMeshObject2();
         m_dwObjControlObject  = PolyMeshVertFromPixel (dwX, dwY);
         if (m_dwObjControlObject == -1) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         if (!pm->m_PolyMesh.CanModify()) {
            EscMessageBox (m_hWnd, ASPString(),
               L"You can't do this when more than one morph is turned on.",
               L"To turn the morph off, visit the \"Organic editing\" pane, "
               L"select which morph you wish to modify, and then return here.",
               MB_ICONINFORMATION | MB_OK);
            return FALSE;
         }

         if (!m_pPolyMeshOrig)
            m_pPolyMeshOrig = new CPolyMesh;
         if (!m_pPolyMeshOrig)
            return 0;
         pm->m_PolyMesh.CloneTo (m_pPolyMeshOrig);
         pm->m_pWorld->UndoRemember();

         UpdateToolTipByPointerMode();

         CEscWindow cWindow;
         RECT r;
         DialogBoxLocation (m_hWnd, &r);

         cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
         PWSTR pszRet;

         // start with the first page
         pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPMMOVEVERT, PMMoveVertPage, this);
         m_dwObjControlObject = -1;
         pm->m_pWorld->UndoRemember();
         return 0;   // had control down
      }
      break;

   case IDC_BONEEDIT:
      BoneEdit (dwX, dwY);
      return 0;

   case IDC_OBJDIALOG:
   case IDC_OBJCONTROLDIALOG:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
         BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

         if (!fOK || !HIWORD(pip->dwID)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // else, remember this
         PCObjectSocket pos;
         m_dwObjControlObject = HIWORD(pip->dwID)-1;
         pos = m_pWorld->ObjectGet (m_dwObjControlObject);
         if (!pos) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }
         BOOL fRet;
         fRet = (dwPointerMode == IDC_OBJDIALOG) ? pos->DialogQuery() : pos->DialogCPQuery();
         if (!fRet) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // BUGFIX - get the location
         CPoint pLoc;
         CMatrix mObjectToWorld, mWorldToObject;
         m_apRender[0]->PixelToWorldSpace (dwX, dwY, pip->fZ, &pLoc);
         pLoc.p[3] = 1;
         pos->ObjectMatrixGet (&mObjectToWorld);
         mObjectToWorld.Invert4 (&mWorldToObject);
         pLoc.MultiplyLeft (&mWorldToObject);

         // bring up the dialog
         CEscWindow cWindow;
         RECT r;
         DialogBoxLocation (m_hWnd, &r);

         cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);

         if (dwPointerMode == IDC_OBJDIALOG)
            pos->DialogShow (LOWORD(pip->dwID), &cWindow, &pLoc);
         else
            pos->DialogCPShow (LOWORD(pip->dwID), &cWindow, &pLoc);

         // remember this as an undo point
         m_pWorld->UndoRemember();

         // don capture this
         return 0;
      }
      break;

   case IDC_OBJSHOWEDITOR:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);

         if (!HIWORD(pip->dwID)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // else, remember this
         PCObjectSocket pos;
         m_dwObjControlObject = HIWORD(pip->dwID)-1;
         pos = m_pWorld->ObjectGet (m_dwObjControlObject);
         if (!pos) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }
         BOOL fRet;
         fRet = pos->EditorCreate (TRUE);
         if (!fRet) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // remember this as an undo point
         m_pWorld->UndoRemember();

         // don capture this
         return 0;
      }
      break;

   case IDC_OBJATTRIB:
      {
         // if it's not over a control point then ignore
         // only set the cursor if over drag points
         PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
         BOOL fOK = (LOWORD(pip->dwID) < 0x1000);

         if (!fOK || !HIWORD(pip->dwID)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // else, remember this
         PCObjectSocket pos;
         m_dwObjControlObject = HIWORD(pip->dwID)-1;
         pos = m_pWorld->ObjectGet (m_dwObjControlObject);
         if (!pos) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }


         // figure out all the paramers
         OAINFO oaInfo;
         memset (&oaInfo, 0, sizeof(oaInfo));
         oaInfo.pos = pos;
         CListFixed lAttrib, lOAI;
         lOAI.Init (sizeof(OAINFO));
         pos->AttribGetAll (&lAttrib);
         DWORD i;
         for (i = 0; i < lAttrib.Num(); i++) {
            oaInfo.av = *((ATTRIBVAL*)lAttrib.Get(i));
            pos->AttribInfo (oaInfo.av.szName, &oaInfo.info);
            lOAI.Add (&oaInfo);
         }

         // Go through toggle list. If name there then set toggle bit and flip priority
         PWSTR pszToggle;
         pszToggle = pos->StringGet (OSSTRING_ATTRIBRANK);
         PWSTR pszSlash;
         WCHAR szTemp[64];
         POAINFO poa;
         poa = (POAINFO) lOAI.Get(0);
         for (pszSlash = NULL; pszToggle && pszToggle[0]; pszToggle = pszSlash ? (pszSlash + 1) : NULL) {
            // find the next slash
            pszSlash = wcschr (pszToggle, L'\\');

            // length?
            DWORD dwLen;
            if (pszSlash)
               dwLen = (DWORD) ((size_t)pszSlash - (size_t)pszToggle) / sizeof(WCHAR);
            else
               dwLen = (DWORD)wcslen(pszToggle);

            // if too long tinue
            if (dwLen > 63)
               continue;

            // copy over
            memmove (szTemp, pszToggle, dwLen * sizeof(WCHAR));
            szTemp[dwLen] = 0;

            // find a match?
            for (i = 0; i < lOAI.Num(); i++) {
               if (!_wcsicmp(poa[i].av.szName, szTemp)) {
                  // match
                  poa[i].fToggleRank = TRUE;
                  poa[i].info.fDefLowRank = !poa[i].info.fDefLowRank;
                  break;
               }
            }
         }
         
         // find the location where clicked
         fp fZ;
         CPoint pLoc;
         CMatrix m, mInv;
         fZ = m_apRender[0]->PixelToZ (dwX, dwY);
         m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pLoc);
         pLoc.p[3] = 1;
         pos->ObjectMatrixGet (&m);
         m.Invert4 (&mInv);
         pLoc.MultiplyLeft (&mInv);


         // bring up the dialog
         CEscWindow cWindow;
         RECT r;
         DialogBoxLocation (m_hWnd, &r);

         cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);

         PWSTR psz;
   redo:
         // sort the list
         gpOAInfoSort = &pLoc;
         qsort (lOAI.Get(0), lOAI.Num(), sizeof(OAINFO), OAINFOSort);

         // show page
         psz = cWindow.PageDialog (ghInstance, IDR_MMLOBJATTRIB, ObjAttribPage, &lOAI);
         if (psz && !_wcsicmp(psz, RedoSamePage()))
            goto redo;

         // remember this as an undo point
         m_pWorld->UndoRemember();

         // don capture this
         return 0;
      }
      break;

   case IDC_ZOOMIN:
   case IDC_ZOOMOUT:
      {
         DWORD dwModel = m_apRender[0]->CameraModelGet();
         BOOL fZoomIn = (dwPointerMode == IDC_ZOOMIN);

         // BUGFIX - Modifiued to use NormalFromPoint andLookAtPointFromDistance,
         // and handle control to look perp

         // find out what clicked on
         CPoint pClick, pN;
         if (!NormalFromPoint (dwX, dwY, &pClick, &pN, NULL, NULL)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }

         // where are we looking from
         CPoint pEye;
         m_apRender[0]->PixelToWorldSpace (dwX, dwY, (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);

         // move half way there
         double fScale;
         if (fZoomIn)
            fScale = 1.0 / sqrt((fp)2);
         else
            fScale = sqrt((fp)2); // note that sending in > 1, but OK

         // move the eye
         if ((GetKeyState (VK_CONTROL) < 0) && (pN.Length() > CLOSE)) {
            // pressed control
            double fDist;
            pEye.Subtract (&pClick);
            fDist = pEye.Length() * fScale;
            pEye.Copy (&pN);
            pEye.Scale (fDist);
            pEye.Add (&pClick);
         }
         else {
            pEye.Average (&pClick, 1 - fScale); // note that sending in > 1, but OK
         }

         LookAtPointFromDistance (&pClick, &pEye, fScale);
         return 0;
      }
      break;

   case IDC_ZOOMAT:
      {
         DWORD dwModel = m_apRender[0]->CameraModelGet();
         switch (dwModel) {
         default:
         case CAMERAMODEL_FLAT:
            dwModel = 0;   // flat
            break;
         case CAMERAMODEL_PERSPOBJECT:
            dwModel = 1;   // objec
            break;
         case CAMERAMODEL_PERSPWALKTHROUGH:
            dwModel = 2;
            break;
         }

         // find out the point where clicked
         fp fZ;
         fZ = m_apRender[0]->PixelToZ (dwX, dwY);
         if (fZ > 1000) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }
         CPoint pTempClick;
         m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pTempClick);

         // order of the popup menu
         DWORD adwOrder[3];
         adwOrder[0] = dwModel;
         DWORD i, j;
         j = 0;
         for (i = 1; i < 3; i++, j++) {
            // dont duplicate current model
            if (j == dwModel)
               j++;
            adwOrder[i] = j;
         }

         // create the menu
         HMENU hMenu = CreatePopupMenu ();
         char *psz;

         for (i = 0; i < 3; i++) {
            // add the title, but disabled
            switch (adwOrder[i]) {
            case 0:
               psz = "Flat";
               break;
            case 1:
               psz = "Model";
               break;
            case 2:
               psz = "Walkthrough";
               break;
            }
            AppendMenu (hMenu, MF_DISABLED | MF_GRAYED | (i ? MF_MENUBARBREAK : 0), 1, psz);
            AppendMenu (hMenu, MF_SEPARATOR, 0, 0);

            // add the directions
            AppendMenu (hMenu, MF_ENABLED, i * 100 + 10, NSEWNAME ("North", "Back"));
            AppendMenu (hMenu, MF_ENABLED, i * 100 + 11, NSEWNAME ("East", "Right"));
            AppendMenu (hMenu, MF_ENABLED, i * 100 + 12, NSEWNAME ("South", "Front"));
            AppendMenu (hMenu, MF_ENABLED, i * 100 + 13, NSEWNAME ("West", "Left"));
            AppendMenu (hMenu, MF_ENABLED, i * 100 + 14, "Above");
            AppendMenu (hMenu, MF_ENABLED, i * 100 + 15, "Below");
         }  // over i


         // show the menu
         POINT p;
         GetCursorPos (&p);
         int iRet;
         iRet = TrackPopupMenu (hMenu, TPM_LEFTALIGN | TPM_VCENTERALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
            p.x, p.y, 0, m_hWnd, NULL);
         
         DestroyMenu (hMenu);
         DWORD dwNewModel, dwDirection;
         dwNewModel = ((DWORD)iRet / 100);
         if (dwNewModel >= 3)
            return 0;
         dwNewModel = adwOrder[dwNewModel];
         dwDirection = (DWORD)iRet % 100;
         if ((dwDirection < 10) || (dwDirection > 15))
            return 0;

         // find out the distance
         fp fFOV;
         CPoint pTrans, pCenter, pRot;
         fp fLong, fTiltX, fTiltY, fTransX, fTransY;
         fp fDist, fScale;
         fDist = CLOSE;
         fScale = CLOSE;
         if (dwModel == 0 /*flat*/) {
            m_apRender[0]->CameraFlatGet (&pCenter, &fLong, &fTiltX, &fTiltY, &fScale, &fTransX, &fTransY);

            if (dwNewModel != 0) {
               if (dwNewModel == 1)
                  m_apRender[0]->CameraPerspObjectGet (&pTrans, &pCenter, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);
               else
                  m_apRender[0]->CameraPerspWalkthroughGet (&pTrans, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);

               fFOV = max(CLOSE, fFOV);
               fFOV = min(PI*.99, fFOV);
               fDist = fScale * tan(fFOV / 2.0) / 2;
            }
         }
         else {   // perspective model
            CPoint pEye;
            m_apRender[0]->PixelToWorldSpace (dwX, dwY,
               (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);

            // distance away
            CPoint pDist;
            pDist.Subtract (&pEye, &pTempClick);
            fDist = pDist.Length();
            
            if (dwNewModel == 0) {  // going to flat view
               if (dwNewModel == 1)
                  m_apRender[0]->CameraPerspObjectGet (&pTrans, &pCenter, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);
               else
                  m_apRender[0]->CameraPerspWalkthroughGet (&pTrans, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);

               fFOV = max(CLOSE, fFOV);
               fFOV = min(PI*.99, fFOV);
               fScale = fDist / tan(fFOV / 2.0) * 2.0;
            }
         }
         fDist = max(CLOSE,fDist);
         fScale = max(CLOSE, fScale);

         pCenter.Copy (&pTempClick);
         fLong = fTiltX = fTiltY = fTransX = fTransY = 0;

         fp fTiltAngle;
         fTiltAngle = 0;

         switch (dwDirection) {
         case 10: // from north
            fLong = PI;
            break;
         case 11: // from east
            fLong = PI/2;
            break;
         case 12: // from south
            fLong = 0;
            break;
         case 13: // from west
            fLong = -PI/2;
            break;

         case 14: // from above
            fTiltX = PI / 2;
            break;
         case 15: // from brlow
            fTiltX = -PI / 2;
            break;

         }
   #if 0 // for looking down, which dont do
         if ((dwDirection != 14) && (dwDirection != 15)) {  // not from above or below
            fTiltX = cos(fLong) * fTiltAngle;
            fTiltY = sin(fLong) * fTiltAngle;
            //if (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) {
            //   fTiltX = fTiltAngle;
            //}
         }
   #endif // 0

         if (dwNewModel == 0) // flat camera model
            m_apRender[0]->CameraFlat (&pCenter, -fLong, fTiltX, fTiltY, fScale, fTransX, fTransY);
         else if (dwNewModel == 2) {   // walkthrough
            CPoint pTranslate;
            fp fRotateZ, fRotateX, fRotateY, fFOV;

            m_apRender[0]->CameraPerspWalkthroughGet (&pTranslate, &fRotateZ, &fRotateX, &fRotateY, &fFOV);

            pTranslate.Zero();

            // invert this

            CMatrix m, mInv;
            m.FromXYZLLT (&pTranslate, -fLong, fTiltX, fTiltY);
            m.Invert4 (&mInv);
            mInv.ToXYZLLT (&pTranslate, &fLong, &fTiltX, &fTiltY);
            pTranslate.Zero();

            // figure out translation point
            fFOV = max(.01, fFOV);
            fFOV = min(PI/2 - .01, fFOV);
            pTranslate.Zero();
            pTranslate.p[1] = -fDist;
            pTranslate.p[3] = 1;
            pTranslate.MultiplyLeft (&mInv);
            pTranslate.Add (&pCenter);

            m_apRender[0]->CameraPerspWalkthrough (&pTranslate, fLong, fTiltX, fTiltY, fFOV);
         }
         else if (dwNewModel == 1) {   // object
            CPoint pTranslate, pCenter2;
            fp fRotateZ, fRotateX, fRotateY, fFOV;

            m_apRender[0]->CameraPerspObjectGet (&pTranslate, &pCenter2, &fRotateZ, &fRotateX, &fRotateY, &fFOV);

            pTranslate.Zero();
            fFOV = max(.01, fFOV);
            fFOV = min(PI/2 - .01, fFOV);
            pTranslate.p[1] = fDist;

            m_apRender[0]->CameraPerspObject (&pTranslate, &pCenter, -fLong, fTiltX, fTiltY, fFOV);
         }
         if (dwNewModel != dwModel)
            UpdateCameraButtons ();

         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);
         return 0;
      }
      break;

   case IDC_THUMBNAIL:
      {
         DWORD dwModel = m_apRender[0]->CameraModelGet();
         switch (dwModel) {
         default:
         case CAMERAMODEL_FLAT:
            dwModel = 0;   // flat
            break;
         case CAMERAMODEL_PERSPOBJECT:
            dwModel = 1;   // objec
            break;
         case CAMERAMODEL_PERSPWALKTHROUGH:
            dwModel = 2;
            break;
         }

         // find out the point where clicked
         fp fZ;
         fZ = m_apRender[0]->PixelToZ (dwX, dwY);
         if (fZ > 1000) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            return 0;
         }
         CPoint pTempClick;
         m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pTempClick);

         // find out the distance
         fp fFOV;
         CPoint pTrans, pCenter, pRot;
         fp fLong, fTiltX, fTiltY, fTransX, fTransY;
         fp fDist, fScale;
         fDist = CLOSE;
         fScale = CLOSE;
         m_apRender[0]->CameraPerspObjectGet (&pTrans, &pCenter, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);
         fFOV = max(CLOSE, fFOV);
         fFOV = min(PI*.99, fFOV);
         if (dwModel == 0 /*flat*/) {
            m_apRender[0]->CameraFlatGet (&pCenter, &fLong, &fTiltX, &fTiltY, &fScale, &fTransX, &fTransY);

            fDist = fScale * tan(fFOV / 2.0) / 2;
         }
         else {   // perspective model
            CPoint pEye;
            m_apRender[0]->PixelToWorldSpace (dwX, dwY,
               (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);

            // distance away
            CPoint pDist;
            pDist.Subtract (&pEye, &pTempClick);
            fDist = pDist.Length();
            
            fScale = fDist / tan(fFOV / 2.0) * 2.0;
         }
         fDist = max(CLOSE,fDist);
         fScale = max(CLOSE, fScale);

         // create the perspective view
         m_apRender[0]->CloneTo (m_apRender[4]);
         if (dwModel == 0) {
            m_dwThumbnailCur = 1;   // default to the top down

            // BUGFIX - Use walkthrough insead of modeller view
            CPoint pTranslate;
            pTranslate.Copy (&pTempClick);
            pTranslate.p[1] -= fDist;
            m_apRender[4]->CameraPerspWalkthrough (&pTranslate, 0, 0, 0, fFOV);
            //pTranslate.Zero();
            //pTranslate.p[1] = fDist;
            //m_apRender[4]->CameraPerspObject (&pTranslate, &pTempClick, 0, 0, 0, fFOV);
         }
         else {
            m_dwThumbnailCur = 4;   // default to the same perspective as looking at
            // already perspective, so keep this
         }

         // create the other views
         m_apRender[0]->CloneTo (m_apRender[1]);
         m_apRender[1]->CameraFlat (&pTempClick, 0, PI/2, 0, fScale, 0, 0);  // above
         m_apRender[0]->CloneTo (m_apRender[2]);
         m_apRender[2]->CameraFlat (&pTempClick, 0, 0, 0, fScale, 0, 0);  // front
         m_apRender[0]->CloneTo (m_apRender[3]);
         m_apRender[3]->CameraFlat (&pTempClick, -PI/2, 0, 0, fScale, 0, 0);  // right

         // copy the current thumbnail back
         m_apRender[m_dwThumbnailCur]->CloneTo (m_apRender[0]);

         if (!m_dwThumbnailShow) {
            // if not already looking at thumbnails then do so
            m_dwThumbnailShow = 1;
            InvalidateRect (m_hWnd, NULL, FALSE);

            // fake a size message
            RECT r;
            GetClientRect (m_hWnd, &r);
            SendMessage (m_hWnd, WM_SIZE, 0, MAKELPARAM(r.right-r.left,r.bottom-r.top));
         }

         // cause it to redraw
         QualitySync();
         UpdateCameraButtons ();
         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);
         return 0;
      }
      break;
   } // switch

   // capture
   SetCapture (m_hWnd);
   m_fCaptured = TRUE;

   // remember this position
   m_dwButtonDown = dwButton + 1;
   m_pntButtonDown.x = iX;
   m_pntButtonDown.y = iY;
   m_pntMouseLast = m_pntButtonDown;

   switch (dwPointerMode) {
   case IDC_BRUSH4:
   case IDC_BRUSH8:
   case IDC_BRUSH16:
   case IDC_BRUSH32:
   case IDC_BRUSH64:
      // dont set the timer if in texture mode
      if ((m_dwViewSub != VSPOLYMODE_TEXTURE) && !m_fAirbrushTimer) {
         SetTimer (m_hWnd, TIMER_AIRBRUSH, 1000 / AIRBRUSHPERSEC, NULL);
         m_fAirbrushTimer = TRUE;
      }

      PolyMeshBrushApply (dwPointerMode, &m_pntButtonDown);
      break;

   case IDC_VIEWFLATLRUD:
   case IDC_VIEWFLATSCALE:
      m_apRender[0]->CameraFlatGet (&m_pCameraCenter, &m_fCFLong, &m_fCFTilt, &m_fCFTiltY, &m_fCFScale, &m_fCFTransX, &m_fCFTransY);
      break;

   case IDC_VIEW3DLRUD:
   case IDC_VIEW3DLRFB:
      m_apRender[0]->CameraPerspObjectGet (&m_pV3D, &m_pCameraCenter, &m_fV3DRotZ, &m_fV3DRotX, &m_fV3DRotY, &m_fV3DFOV);
      break;

   case IDC_VIEWFLATROTZ:
   case IDC_VIEWFLATROTX:
   case IDC_VIEWFLATROTY:
   case IDC_VIEW3DROTZ:
   case IDC_VIEW3DROTX:
   case IDC_VIEW3DROTY:
      {
         BOOL f3d = (dwPointerMode == IDC_VIEW3DROTZ) ||(dwPointerMode == IDC_VIEW3DROTY) ||(dwPointerMode == IDC_VIEW3DROTX);

         if (f3d)
            m_apRender[0]->CameraPerspObjectGet (&m_pV3D, &m_pCameraCenter, &m_fV3DRotZ, &m_fV3DRotX, &m_fV3DRotY, &m_fV3DFOV);
         else
            m_apRender[0]->CameraFlatGet (&m_pCameraCenter, &m_fCFLong, &m_fCFTilt, &m_fCFTiltY, &m_fCFScale, &m_fCFTransX, &m_fCFTransY);

#if FAILEDCODE // tried to get click and drag on point to rotate
         CPoint pVector;
         CMatrix m, mInv;
         pVector.Zero();
         if ((dwPointerMode == IDC_VIEWFLATROTZ) || (dwPointerMode == IDC_VIEW3DROTZ))
            pVector.p[2] = 1;
         else {
            pVector.p[1] = 1;
            // get it from the screen
            //CPoint pViewer, pAt;
            //m_apRender[0]->PixelToViewerSpace (m_aImage[0].Width()/2.0+.5, m_aImage[0].Height()/2.0+.5,
            //   .001, &pViewer);
            //m_apRender[0]->PixelToViewerSpace (m_aImage[0].Width()/2.0+.5, m_aImage[0].Height()/2.0+.5,
            //   1, &pAt);
            //pVector.Subtract (&pViewer, &pAt);
         }
         m.FromXYZLLT (&m_pCameraCenter,
            f3d ? m_fV3DRotZ : m_fCFLong,
            f3d ? m_fV3DRotX : m_fCFTilt,
            f3d ? m_fV3DRotY : m_fCFTiltY);
         //m.Invert4 (&mInv);
         mInv.Copy (&m);
         if (!RotationRemember(&m_pCameraCenter, need this, &pVector, &mInv, dwX, dwY, TRUE)) {
            ReleaseCapture ();
            m_fCaptured = FALSE;
            m_dwButtonDown = 0;
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            goto alldone;
         }
#endif   // FAILEDCODE
      }
      break;

   case IDC_VIEWWALKROTZX:
   case IDC_VIEWWALKROTY:
   case IDC_VIEWWALKLRUD:
   case IDC_VIEWWALKLRFB:
      m_apRender[0]->CameraPerspWalkthroughGet(&m_pVW, &m_fVWLong, &m_fVWLat, &m_fVWTilt, &m_fVWFOV);
      break;

   case IDC_VIEWFLATLLOOKAT:
   case IDC_OBJECTPASTEDRAG:  // take advantage of invalidation of screen
      {
         // invalidate the window around the camera so causes it to draw indicator
         RECT r;
         r.left = min(m_pntButtonDown.x, m_pntMouseLast.x) - M3DBUTTONSIZE;
         r.right = max(m_pntButtonDown.x, m_pntMouseLast.x) + M3DBUTTONSIZE;
         r.top = min(m_pntButtonDown.y, m_pntMouseLast.y) - M3DBUTTONSIZE;
         r.bottom = max(m_pntButtonDown.y, m_pntMouseLast.y) + M3DBUTTONSIZE;
         InvalidateRect (m_hWnd, &r, FALSE);
      }
      break;

   case IDC_CLIPLINE:
      {
         m_fClipTempAngle = 0;
         m_fClipAngleValid = FALSE;

         // clear out old clip plane
         m_apRender[0]->ClipPlaneRemove (CLIPPLANE_USER);

         // render
         RenderUpdate(WORLDC_NEEDTOREDRAW);
      }
      break;

   case IDC_MOVENSEWUD:
   case IDC_MOVENSEW:
   case IDC_MOVEEMBED:
   case IDC_MOVEUD:
   case IDC_MOVEROTZ:
   case IDC_MOVEROTY:
      {
         DWORD dwEOS = IsEmbeddedObjectSelected();

         // if the control button is heald down then clone all the current
         // select objects and add them
         if ((wParam & MK_CONTROL) && (dwEOS < 2)) {
            // This deals with the case of selecting one or more container objects
            // and ensuring all their embedded objects (and children thereof) are
            // also embedded

            DWORD i, dwNum, *pdw;
            pdw = m_pWorld->SelectionEnum (&dwNum);

            // need to include all embedded objects when duplicate
            PCListFixed pl;
            pl = IncludeEmbeddedObjects (pdw, dwNum);
            if (!pl)
               goto alldone;
            pdw = (DWORD*) pl->Get(0);
            dwNum = pl->Num();

            // keep a list of GUIDs that have changed
            CListFixed  lOld, lNew,lRemove;
            lOld.Init (sizeof(GUID));
            lNew.Init (sizeof(GUID));
            lRemove.Init (sizeof(GUID));
            GUID gTemp;

            // keep track of the objects that have done world set to
            CListFixed lWorldSet;
            lWorldSet.Init (sizeof(PCObjectSocket));

            for (i = 0; i < dwNum; i++) {
               PCObjectSocket pObj = m_pWorld->ObjectGet (pdw[i]);
               PCObjectSocket pNew;
               pNew = pObj->Clone();
               if (!pNew)
                  continue;
               if (dwEOS == 1) {
                  pNew->WorldSet (NULL);  // so can do container set without problems
                  pNew->EmbedContainerSet (NULL);
               }
               if (pNew) {
                  pNew->GUIDGet (&gTemp);
                  lOld.Add (&gTemp);
                  m_pWorld->ObjectAdd (pNew);
                  lWorldSet.Add (&pNew);
                  pNew->GUIDGet (&gTemp);
                  lNew.Add (&gTemp);
               }

               // if it's embedded then re-embed then new one
               GUID gCont;
               if ((dwEOS == 1) && pObj->EmbedContainerGet (&gCont)) {
                  GUID gOld, gNew;
                  pObj->GUIDGet (&gOld);
                  pNew->GUIDGet (&gNew);
                  
                  PCObjectSocket pCont;
                  pCont = m_pWorld->ObjectGet (m_pWorld->ObjectFind (&gCont));
                  if (pCont) {
                     TEXTUREPOINT HV;
                     fp fRotation;
                     DWORD dwSurface;
                     pCont->ContEmbeddedLocationGet (&gOld, &HV, &fRotation, &dwSurface);
                     pCont->ContEmbeddedAdd (&gNew, &HV, fRotation, dwSurface);
                  }
               }  // EmbedContainerGet()

            }

            // go through all the objects we've added and remap their guid names
            // dont do this if dwEOS == 1 since in that case only had one
            // object moved, and already dealt with it.
            if (dwEOS != 1) {
               // loop through all the objects that added
               GUID *pCur, *pCur2, *pCur3;
               DWORD j, k;
               for (i = 0; i < lNew.Num(); i++) {
                  pCur = (GUID*) lNew.Get(i);
                  PCObjectSocket pOSCur;
                  pOSCur = m_pWorld->ObjectGet(m_pWorld->ObjectFind (pCur));
                  if (!pOSCur)
                     continue;   // shouldnt happen

                  // if it's embedded then tell it of the rename
                  GUID gTemp;
                  if (pOSCur->EmbedContainerGet (&gTemp)) {
                     // find this in the list
                     for (j = 0; j < lOld.Num(); j++) {
                        pCur2 = (GUID*) lOld.Get(j);
                        if (IsEqualGUID(*pCur2, gTemp)) {
                           pCur2 = (GUID*) lNew.Get(j);
                           pOSCur->EmbedContainerRenamed (pCur2);
                           break;
                        }
                     }

                     // if its parent didn't get transferred over then
                     // it's all alone, so tell it its no longer owned
                     if (j >= lOld.Num()) {
                        pOSCur->EmbedContainerSet (NULL);
                     }
                  }

                  // BUGFIX - loop through all the attached objects and rename
                  for (k = 0; k < pOSCur->AttachNum(); k++) {
                     if (!pOSCur->AttachEnum (k, &gTemp))
                        continue;

                     // just go through all the pasted objects and rename them
                     for (j = 0; j < lOld.Num(); j++) {
                        pCur2 = (GUID*) lOld.Get(j);
                        pCur3 = (GUID*) lNew.Get(j);
                        if (IsEqualGUID(*pCur2, gTemp)) {
                           pOSCur->AttachRenamed (pCur2, pCur3);
                           break;
                        }
                     }  // j
                  }

                  // loop through the guids
                  lRemove.Clear();
                  for (k = 0; k < pOSCur->ContEmbeddedNum(); k++) {
                     if (!pOSCur->ContEmbeddedEnum(k, &gTemp))
                        continue;

                     // just go through all the pasted objects and rename them
                     for (j = 0; j < lOld.Num(); j++) {
                        pCur2 = (GUID*) lOld.Get(j);
                        pCur3 = (GUID*) lNew.Get(j);
                        if (IsEqualGUID(*pCur2, gTemp)) {
                           pOSCur->ContEmbeddedRenamed (pCur2, pCur3);
                           break;
                        }
                     }  // j

                     // if we didn't find this then remove it
                     if (j >= lOld.Num())
                        lRemove.Add (&gTemp);
                  }

                  // remove any guids contained in object that no longer exist
                  for (j = 0; j < lRemove.Num(); j++) {
                     pCur2 = (GUID*) lRemove.Get(j);
                     pOSCur->ContEmbeddedRemove (pCur2);
                  }

               }
            }

            // go through all the ones that have added and do WorldSetFInished()
            // so they can update
            for (i = 0; i < lWorldSet.Num(); i++) {
               PCObjectSocket p = *((PCObjectSocket*) lWorldSet.Get(i));
               p->WorldSetFinished();
            }


            delete pl;
         }

         PCObjectSocket pEmbed, pContain;
         m_fMoveEmbedded = FALSE;
         if (dwEOS == 1) {
            // get this object and find its position on the surface
            DWORD dwNum, *pdw;
            pdw = m_pWorld->SelectionEnum(&dwNum);
            if (dwNum != 1)
               break;   // shouldn't happen
            pEmbed = m_pWorld->ObjectGet (pdw[0]);
            if (!pEmbed)
               break;   // shouldnt happen
            GUID gContain, gEmbed;
            pEmbed->GUIDGet (&gEmbed);
            if (!pEmbed->EmbedContainerGet (&gContain))
               break;   // shouldn't happen
            pContain = m_pWorld->ObjectGet (m_pWorld->ObjectFind(&gContain));
            if (!pContain)
               break;   // shouldn't happen
            
            if (!pContain->ContEmbeddedLocationGet (&gEmbed, &m_MoveEmbedHV,
               &m_fMoveEmbedRotation, &m_dwMoveEmbedSurface))
               break;   // shouldn't happen

            // moving/rotating an embedded object
            m_fMoveEmbedded = TRUE;

            fp fZ;
            CPoint pEye, pClick;
            fZ = m_apRender[0]->PixelToZ (dwX,dwY);
            m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pClick);
            m_apRender[0]->PixelToWorldSpace (dwX, dwY,
               (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);
            CMatrix m, m2;
            pContain->ObjectMatrixGet (&m2);
            m2.Invert4(&m);
            pEye.MultiplyLeft (&m);
            pClick.MultiplyLeft (&m);

            // see where clicking intersects the surface
            if ( ((dwPointerMode == IDC_MOVENSEWUD) || (dwPointerMode == IDC_MOVENSEW) || (dwPointerMode == IDC_MOVEUD) || (dwPointerMode == IDC_MOVEEMBED)) &&
               !pContain->ContHVQuery (&pEye, &pClick, m_dwMoveEmbedSurface, NULL, &m_MoveEmbedClick)) {
               BeepWindowBeep (ESCBEEP_DONTCLICK);
               ReleaseCapture ();
               m_fCaptured = FALSE;
               m_dwButtonDown = 0;
               goto alldone;
            }

            m_MoveEmbedOld = m_MoveEmbedClick;

         }
         else {
            // moving everything
            m_fMoveEmbedded = FALSE;

            // get the current move point and trans/rot matrix for the object
            DWORD dwPoint;
            dwPoint = MoveReferenceCurQuery ();
            MoveReferenceMatrixGet (&m_pMoveRotReferenceMatrix);
            m_pMoveRotReferenceWas.Copy (&m_pMoveRotReferenceMatrix);
            MoveReferencePointQuery (dwPoint, &m_pMoveRotReferenceInObj);
            m_pMoveRotReferenceInObj.p[3] =1;
            m_pMoveRotReferenceInWorld.Copy(&m_pMoveRotReferenceInObj);
            m_pMoveRotReferenceInWorld.MultiplyLeft (&m_pMoveRotReferenceMatrix);

            // convert to latitude, longitude, and tilt for rotation
            CPoint pt;
            m_pMoveRotReferenceMatrix.ToXYZLLT (&pt, &m_pMoveRotReferenceLLT.p[2],
               &m_pMoveRotReferenceLLT.p[0], &m_pMoveRotReferenceLLT.p[1]);
         }

         // remember this info
         if ((dwPointerMode == IDC_MOVEROTZ) || (dwPointerMode == IDC_MOVEROTY)) {
            CPoint pCenter,pCenterOrig,pVector;
            CMatrix mOrig;
            MoveReferenceMatrixGet (&mOrig);
            MoveReferencePointQuery(MoveReferenceCurQuery(), &pCenter);
            pCenter.p[3] = 1;
            pCenterOrig.Copy (&pCenter);
            pCenter.MultiplyLeft (&mOrig);
            pVector.Zero();
            if (m_fMoveEmbedded) {
               MoveReferencePointQuery(MoveReferenceCurQuery(), &pVector);
               pVector.p[1] -= 1.0; // since embedded flat in x and z
               pVector.p[3] = 1;
               pVector.MultiplyLeft (&mOrig);
               pVector.Subtract (&pCenter);
            }
            else if (dwPointerMode == IDC_MOVEROTZ)
               pVector.p[2] = 1;
            else {
               // get it from the screen
               CPoint pViewer, pAt;
               m_apRender[0]->PixelToWorldSpace (m_aImage[0].Width()/2.0+.5, m_aImage[0].Height()/2.0+.5,
                  .001, &pViewer);
               m_apRender[0]->PixelToWorldSpace (m_aImage[0].Width()/2.0+.5, m_aImage[0].Height()/2.0+.5,
                  1, &pAt);
               pVector.Subtract (&pViewer, &pAt);
            }

            if (!RotationRemember (&pCenter, &pCenterOrig, &pVector, &mOrig, dwX, dwY)) {
               BeepWindowBeep (ESCBEEP_DONTCLICK);
               goto alldone;
            }
         }

      }
      break;

   }

   switch (dwPointerMode) {

   case IDC_VIEWFLATLRUD:
   case IDC_VIEW3DLRUD:
   case IDC_VIEW3DLRFB:
   case IDC_VIEWWALKLRUD:
   case IDC_VIEWWALKLRFB:
   case IDC_MOVENSEWUD:
   case IDC_MOVENSEW:
   case IDC_MOVEEMBED:
   case IDC_MOVEUD:
      {
         if (((dwPointerMode == IDC_MOVENSEWUD) || (dwPointerMode == IDC_MOVENSEW) || (dwPointerMode == IDC_MOVEEMBED) || (dwPointerMode == IDC_MOVEUD)) && m_fMoveEmbedded)
            break;   // dont bother if embedded

         // get the render model info
         BOOL fWalk;
         fWalk = ((dwPointerMode == IDC_VIEWWALKLRFB) || (dwPointerMode == IDC_VIEWWALKLRUD));

         // find out the z depth where clicked, that combined with the image width
         // and field of view will say how much need to scroll every pixel
         DWORD dwX, dwY;
         fp fZ;
         PointInImage (iX, iY, &dwX, &dwY);
         fZ = m_apRender[0]->PixelToZ (dwX, dwY);

         // if click on empty space the use the center of the house for the z distance
         if (fZ > 1000) {
            // if it's a moving object pointer mode then must click on an object
            if ((dwPointerMode == IDC_MOVENSEWUD) || (dwPointerMode == IDC_MOVENSEW) || (dwPointerMode == IDC_MOVEEMBED) || (dwPointerMode == IDC_MOVEUD)) {
               if (m_fCaptured)
                  ReleaseCapture ();
               m_fCaptured = FALSE;
               m_dwButtonDown = 0;

               BeepWindowBeep (ESCBEEP_DONTCLICK);
               goto alldone;
            }

            fZ = fWalk ? .1 : 1; // changed so if click on nothing assume clicked on 1m away
         }
         fZ = max(.1,fZ);   // at least .1 meter distance

         // if it's moving up/down and the shift key is pressed then snap to that
         if (m_plZGrid)
            delete m_plZGrid;
         m_plZGrid = NULL;
         if ((dwPointerMode == IDC_MOVEUD) && (GetKeyState (VK_SHIFT) < 0)) {
            DWORD *padwIgnore;
            DWORD dwIgnoreNum;
            padwIgnore = m_pWorld->SelectionEnum (&dwIgnoreNum);
            CPoint apVolume[2][2][2];
            DWORD x,y,z;
            
            // use the reference point for the object as the center
            CPoint pRef;
            CMatrix m;
            MoveReferencePointQuery(MoveReferenceCurQuery(), &pRef);
            MoveReferenceMatrixGet (&m);
            pRef.p[3] = 1;
            pRef.MultiplyLeft (&m);

            // BUGFIX - Wasn't working because of roundoff error, so had to make
            // the numbers larger, 0.001 to .01, z of 1000 not 10000
            for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
               apVolume[x][y][z].p[0] = pRef.p[0] + (x ? .01 : -.01);
               apVolume[x][y][z].p[1] = pRef.p[1] + (y ? .01 : -.01);
               apVolume[x][y][z].p[2] = 1000 * (z ? (fp)1 : (fp)-1);
               apVolume[x][y][z].p[3] = 1;
            }
            m_plZGrid = ObjectsInVolume (m_pWorld, apVolume, FALSE, TRUE,
               padwIgnore, dwIgnoreNum);
         }

         // figure out how much to move object if move cursor one pixel
         //m_fScaleScroll = 2* fZ * tan(fFOV / 2.0) / ((fp) m_aImage[0].Width() * m_dwImageScale);

         // Need this to convert from pixel to world space
         CPoint p1, p2;
         m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &p1);
         m_apRender[0]->PixelToWorldSpace (dwX+1, dwY, fZ, &p2);

         // don't convert back to camera space if moving object
         if (!((dwPointerMode == IDC_MOVENSEWUD) || (dwPointerMode == IDC_MOVENSEW) || (dwPointerMode == IDC_MOVEEMBED) || (dwPointerMode == IDC_MOVEUD))) {
            // convert from world space to rotation space (but not translation) for camera
            p1.MultiplyLeft (&m_apRender[0]->m_CameraMatrixRotAfterTrans);
            p2.MultiplyLeft (&m_apRender[0]->m_CameraMatrixRotAfterTrans);
         }

         m_pScaleScrollX.Subtract (&p2, &p1);
         m_pScaleScrollX.Scale (1.0 * IMAGESCALE/ m_dwImageScale);
         m_apRender[0]->PixelToWorldSpace (dwX, dwY+1, fZ, &p2);

         // don't convert back to camera space if moving object
         if (!((dwPointerMode == IDC_MOVENSEWUD) || (dwPointerMode == IDC_MOVENSEW) || (dwPointerMode == IDC_MOVEEMBED) || (dwPointerMode == IDC_MOVEUD))) {
            p2.MultiplyLeft (&m_apRender[0]->m_CameraMatrixRotAfterTrans);
         }

         m_pScaleScrollY.Subtract (&p2, &p1);
         m_pScaleScrollY.Scale (1.0 * IMAGESCALE / m_dwImageScale);

#ifdef _DEBUG
         char szTemp[128];
         sprintf (szTemp, "WS click: %g,%g,%g\r\n", (double)p1.p[0], (double)p1.p[1], (double)p1.p[2]);
         OutputDebugString (szTemp);
#endif
         // if we're moving Z front and back, then do cross-produce and scale
         if ((dwPointerMode ==IDC_VIEW3DLRFB) || (dwPointerMode == IDC_VIEWWALKLRFB)) {
            // I'm specifically doing it the opposite way since up should
            // push it further back
            p2.CrossProd (&m_pScaleScrollY, &m_pScaleScrollX);
            p2.Scale (m_pScaleScrollX.Length() / p2.Length());
            m_pScaleScrollY.Copy (&p2);
         }

         // if this is moving NSEW then don't allow vertical movement
         if (dwPointerMode == IDC_MOVENSEW) {
            m_pScaleScrollX.p[2] = 0;
            m_pScaleScrollY.p[2] = 0;
         }

         // if moving the object UD, then dont allow horizontal movmenet
         if (dwPointerMode == IDC_MOVEUD) {
            m_pScaleScrollX.p[0] = m_pScaleScrollX.p[1] = 0;
            m_pScaleScrollY.p[0] = m_pScaleScrollY.p[1] = 0;
         }
      }
      break;

   case IDC_VIEWWALKFBROTZ:
      if (!m_dwMoveTimerID) {
         m_dwMoveTimerID = 42;
         m_dwMoveTimerInterval = TIMERINTER;
         SetTimer (m_hWnd, m_dwMoveTimerID, m_dwMoveTimerInterval, NULL);
      }
      break;

   }

alldone:
   // BUGFIX: update cursor and tooltip
   // update the tooltip and cursor
   UpdateToolTipByPointerMode();
   SetProperCursor (iX, iY);

   return 0;
}

/***********************************************************************************
CHouseView::RectImageToScreen - Conerts a rectangle from image coords to window
(m_hWnd) coords

inptus
   RECT           *pr - Rectangle to modify
   BOOL           fExtra - If TRUE then add some buffer to left and right
*/
void CHouseView::RectImageToScreen (RECT *pr, BOOL fExtra)
{
   // scale
   pr->left  = pr->left * (int)m_dwImageScale / IMAGESCALE;
   pr->right  = pr->right * (int)m_dwImageScale / IMAGESCALE;
   pr->top  = pr->top * (int)m_dwImageScale / IMAGESCALE;
   pr->bottom  = pr->bottom * (int)m_dwImageScale / IMAGESCALE;

   if (fExtra) {
      RECT r;
      r.left = min(pr->left, pr->right);
      r.right = max(pr->left, pr->right);
      r.top = min(pr->top, pr->bottom);
      r.bottom = max(pr->top, pr->bottom);
      *pr = r;

      // add some extra
      pr->left -= 5;
      pr->right += 5;
      pr->top -= 5;
      pr->bottom += 5;
   }

   // reposition
   pr->left += m_arThumbnailLoc[0].left;
   pr->right += m_arThumbnailLoc[0].left;
   pr->top += m_arThumbnailLoc[0].top;
   pr->bottom += m_arThumbnailLoc[0].top;

}

/***********************************************************************************
CHouseView::ButtonUp - Left button down message
*/
LRESULT CHouseView::ButtonUp (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton)
{
   DWORD dwPointerMode = m_adwPointerMode[dwButton];

   // BUGFIX - If the button is not the correct one then ignore
   if (m_dwButtonDown && (dwButton+1 != m_dwButtonDown))
      return 0;

   if (m_fCaptured)
      ReleaseCapture ();
   m_fCaptured = FALSE;

   // kill the timer if it exists
   if (m_dwMoveTimerID)
      KillTimer (m_hWnd, m_dwMoveTimerID);
   m_dwMoveTimerID = 0;

   // BUGFIX - If don't have buttondown flag set then dont do any of the following
   // operations. Otherwise have problem which createa a new object, click on wall to
   // create, and ends up getting mouse-up in this window (from the mouse down in the
   // object selection window)
   if (!m_dwButtonDown)
      return 0;

   m_dwButtonDown = 0;

   // if it's the look at pointer mode then just lifted up, so need to invalidate
   // and change views
   switch (dwPointerMode) {
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

   case IDC_VIEWFLATLLOOKAT:
      {
         // invalidate the window around the camera so causes it to draw indicator
         RECT r;
         r.left = min(m_pntButtonDown.x, m_pntMouseLast.x) - M3DBUTTONSIZE;
         r.right = max(m_pntButtonDown.x, m_pntMouseLast.x) + M3DBUTTONSIZE;
         r.top = min(m_pntButtonDown.y, m_pntMouseLast.y) - M3DBUTTONSIZE;
         r.bottom = max(m_pntButtonDown.y, m_pntMouseLast.y) + M3DBUTTONSIZE;
         InvalidateRect (m_hWnd, &r, FALSE);

         // use the closest Z
         CPoint pLF, pLA;
         fp fZ;
         DWORD dwX, dwY;
         fZ = 1000;
         if (PointInImage(m_pntButtonDown.x, m_pntButtonDown.y, &dwX, &dwY)) {
            fZ = m_apRender[0]->PixelToZ (dwX, dwY);
         }
         if (PointInImage(m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY)) {
            fZ = min(fZ, m_apRender[0]->PixelToZ (dwX, dwY));
         }

         // if Z still isnt found then use 0 (can do these because flat view)
         BOOL fClickedOnEmptiness;
         fClickedOnEmptiness = FALSE;
         if (fZ >= 1000) {
            fZ = 0;
            fClickedOnEmptiness = TRUE;
         }
         
         // get the locations
         // assume dwX, dwY filled with lastbutton position
         m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pLA);
         PointInImage(m_pntButtonDown.x, m_pntButtonDown.y, &dwX, &dwY);
         m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pLF);

         // Person is 1.8 meters high
         if (fClickedOnEmptiness) {
            pLA.p[2] = max(pLA.p[2], CM_EYEHEIGHT);
            pLF.p[2] = max(pLA.p[2], CM_EYEHEIGHT);
         }
         else {
            pLA.p[2] += CM_EYEHEIGHT;
            pLF.p[2] += CM_EYEHEIGHT;
         }

         // calculate longitude and latitude
         CPoint pTo;
         fp fLat, fLong;
         pTo.Subtract (&pLA, &pLF);
         pTo.Normalize();
         if (pTo.Length() > EPSILON) {
            fLong = -atan2 (pTo.p[0], pTo.p[1]);

            // flatten out XY to figure out latitude
            fp fLen;
            fLen = sqrt(pTo.p[0] * pTo.p[0] + pTo.p[1] * pTo.p[1]);
            if ((fLen > EPSILON) || (fabs(pTo.p[2]) > EPSILON))
               fLat = atan2 (pTo.p[2], fLen);
            else
               fLat = 0;
         }
         else {
            fLong = 0;
            fLat = 0;
         }

         // look
         CPoint pOldFrom;
         fp fOldLong, fOldLat, fTilt, fFOV;
         m_apRender[0]->CameraPerspWalkthroughGet (&pOldFrom, &fOldLong, &fOldLat, &fTilt, &fFOV);
         m_apRender[0]->CameraPerspWalkthrough (&pLF, fLong, fLat, 0, fFOV);
         CameraPosition();
         UpdateCameraButtons ();
         RenderUpdate(WORLDC_CAMERAMOVED);
      }
      break;

   case IDC_OBJOPENCLOSE:
      {
         PCObjectSocket pos;
         if ((m_dwObjControlObject != -1) && (pos = m_pWorld->ObjectGet(m_dwObjControlObject))) {
            int iDist;
            iDist = (int)(short)LOWORD(lParam) - m_pntButtonDown.x;
            if (abs(iDist) < 10) {
               // didn't move much so flip
               pos->OpenSet ((m_fOpenStart > .5) ? 0 : 1);
            }

            // BUGFIX - Refresh tooltip
            UpdateToolTipByPointerMode();

         }
      }
      break;

   case IDC_OBJONOFF:
      {
         PCObjectSocket pos;
         if ((m_dwObjControlObject != -1) && (pos = m_pWorld->ObjectGet(m_dwObjControlObject))) {
            int iDist;
            iDist = (int)(short)LOWORD(lParam) - m_pntButtonDown.x;
            if (abs(iDist) < 10) {
               // didn't move much so flip
               pos->TurnOnSet ((m_fOpenStart > .5) ? 0 : 1);
            }

            // BUGFIX - Refresh tooltip
            UpdateToolTipByPointerMode();

         }
      }
      break;


   case IDC_CLIPLINE:
      {
         RTCLIPPLANE t;
         // dont do this if control is held down
         if (!(GetKeyState (VK_CONTROL) < 0) && m_apRender[0]->ClipPlaneGet (CLIPPLANE_USER, &t)) {
            // look at this
            CPoint pAt, pFrom;
            CPoint v1, v2;
            v1.Subtract (&t.ap[1], &t.ap[0]);
            v2.Subtract (&t.ap[2], &t.ap[0]);
            pAt.Copy (&t.ap[0]); // look at point clicked on, or 0 if off
            pFrom.CrossProd (&v2, &v1);
            pFrom.Normalize();

            // see how far the point that clicked on was from the camerea position
            CPoint pViewer;
            m_apRender[0]->PixelToWorldSpace (m_aImage[0].Width()/2, m_aImage[0].Height()/2,
               .001, &pViewer);
            pViewer.Subtract (&pAt);
            pFrom.Scale (pViewer.Length()); // same distance as was before
            
            pFrom.Add (&pAt);
            LookAtPointFromDistance(&pAt, &pFrom);
         }

         // dont need to do anything because have already set the clip plane prior
         InvalidateDisplay();
      }
      break;

   case IDC_OBJECTPASTEDRAG:
      {
         InvalidateDisplay();

         // clear out current selection
         // ClipboardDelete (FALSE);- Dont do this because it doesnt seem intuitive

         // NOTE - Was a bug but dont know what it means anymore
         // If paste drag then special flat to move out of purgatory
         DWORD dwX, dwY, dwXOrig, dwYOrig;
         if (!PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY) ||
            !PointInImage (m_pntButtonDown.x, m_pntButtonDown.y, &dwXOrig, &dwYOrig)) {
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            goto alldone;
         }
         
         // get all the points in world coords
         CListFixed     lDrag;
         lDrag.Init (sizeof(CPoint));
         DWORD dwNum, i;
         dwNum = max(max(dwX,dwXOrig) - min(dwX,dwXOrig), max(dwY,dwYOrig) - min(dwY,dwYOrig));
         dwNum = max(1,dwNum);   // at least 1
         for (i = 0; i <= dwNum; i++) {
            fp fx, fy;
            CPoint p;
            DWORD dwX2, dwY2;

            fx = ((fp)i / (fp)dwNum) * ((fp)dwX - (fp)dwXOrig) + (fp)dwXOrig;
            fy = ((fp)i / (fp)dwNum) * ((fp)dwY - (fp)dwYOrig) + (fp)dwYOrig;
            dwX2 = (DWORD) fx;
            dwY2 = (DWORD) fy;
            if ((dwX2 >= m_aImage[0].Width()) || (dwY2 >= m_aImage[0].Height()))
               continue;

            // get the pixel
            fp fZ;
            fZ = m_apRender[0]->PixelToZ (dwX2, dwY2);
            if (fZ > 1000)
               continue;
            
            m_apRender[0]->PixelToWorldSpace (dwX2, dwY2, fZ, &p);

            // apply the grid
            GridMove (&p, 0x01 | 0x02);   // no Z grid

            lDrag.Add (&p);
         }

         // move the pasted stuff out of purgatory
         MoveOutOfPurgatory(dwX, dwY, !(GetKeyState (VK_CONTROL) < 0), (PCPoint) lDrag.Get(0), lDrag.Num());

         // remember the undo
         m_pWorld->UndoRemember();

         // change the pointer mode
         SetPointerMode (IsEmbeddedObjectSelected() ? IDC_MOVEEMBED : IDC_MOVENSEW, 0);
         goto alldone;
      }
      break;

   case IDC_PMMOVEINOUT:
   case IDC_PMMOVEANYDIR:
   case IDC_PMMOVEPERP:
   case IDC_PMMOVEPERPIND:
   case IDC_PMMOVESCALE:
   case IDC_PMMOVEROTPERP:
   case IDC_PMMOVEROTNONPERP:
   case IDC_PMSIDEINSET:
   case IDC_PMSIDEEXTRUDE:
   case IDC_PMBEVEL:
   case IDC_PMMAGANY:
   case IDC_PMMAGNORM:
   case IDC_PMMAGVIEWER:
   case IDC_PMMAGPINCH:
   case IDC_PMORGSCALE:
   case IDC_PMMORPHSCALE:
   case IDC_PMTEXTSCALE:
   case IDC_PMTEXTMOVE:
   case IDC_PMTEXTROT:
   case IDC_BONESCALE:
   case IDC_BONEROTATE:
      // set m_dwObjControlObject so we know we're not over an object
      m_dwObjControlObject = (DWORD)-1;
      break;

   case IDC_BONENEW:
      {
         if (m_rPolyMeshLastSplitLine.left != -12345)
            InvalidateRect (m_hWnd, &m_rPolyMeshLastSplitLine, FALSE);

         // add bone
         DWORD dwX, dwY;
         if (PointInImage (LOWORD(lParam), HIWORD(lParam), &dwX, &dwY))
            BoneAdd (m_dwObjControlObject, dwX, dwY);

         // set m_dwObjControlObject so we know we're not over an object
         m_dwObjControlObject = (DWORD)-1;
      }
      break;

   case IDC_PMTAILORNEW:
      {
         // set m_dwObjControlObject so we know we're not over an object
         m_dwObjControlObject = (DWORD)-1;
         // make sure have polymesh
         if (!m_pPolyMeshOrig)
            break;

         // object
         PCObjectPolyMesh pm = PolyMeshObject2();
         if (!pm)
            break;

         // figure out the number of points...
         CListFixed lPoints;
         DWORD dwNum;
         DWORD *padw;
         lPoints.Init (sizeof(DWORD));
         if (m_adwPolyMeshMove[0] == -1)
            padw = pm->m_PolyMesh.SelEnum(&dwNum);
         else {
            padw = m_adwPolyMeshMove;
            dwNum = 1;
         }

         // already have the modifications made to the current object
         // clone the polygon mesh
         PCObjectSocket pos = pm->Clone();
         if (!pos)
            break;

         OSMPOLYMESH os;
         memset (&os, 0, sizeof(os));
         pos->Message (OSM_POLYMESH, &os);
         PCObjectPolyMesh pmNew = os.ppm;
         if (!pmNew) {
            pos->Delete();
            break;
         }
         pmNew->WorldSet (NULL); // just so doesn't notify

         // in pmNew keep the given objects
         BOOL fRet;
         fRet = pmNew->m_PolyMesh.SideKeep (dwNum, padw);
         if (!fRet) {
            EscMessageBox (m_hWnd, ASPString(),
               L"The side(s) could not be disconnected.",
               L"You may have selected a combination of sides that would break symmetry, or you "
               L"may have tried to disconnect the entire object, leaving nothing.",
               MB_ICONWARNING | MB_OK);
            pmNew->Delete();

            // restore original shape
            pm->m_pWorld->ObjectAboutToChange(pm);
            m_pPolyMeshOrig->CloneTo (&pm->m_PolyMesh);
            pm->m_pWorld->ObjectChanged(pm);
            break;
         }

         // clear out the surfaces in the new object
         while (pmNew->ObjectSurfaceNumIndex())
            pmNew->ObjectSurfaceRemove (pmNew->ObjectSurfaceGetIndex(0));

         // add a new default surface
         pmNew->ObjectSurfaceAdd (0, RGB(0x80,0x80,0xff), MATERIAL_CLOTHSMOOTH);

         // set surface
         DWORD i;
         dwNum = pmNew->m_PolyMesh.SideNum();
         for (i = 0; i < dwNum; i++) {
            PCPMSide ps = pmNew->m_PolyMesh.SideGet(i);
            ps->m_dwSurfaceText = 0;
         }
         pmNew->m_PolyMesh.m_fDirtyRender = TRUE;

         // create the new object
         pm->m_pWorld->ObjectAdd (pmNew);

         // restore old object
         //pm->m_pWorld->UndoRemember ();
         pm->m_pWorld->ObjectAboutToChange(pm);
         m_pPolyMeshOrig->CloneTo (&pm->m_PolyMesh);
         pm->m_pWorld->ObjectChanged(pm);
         //pm->m_pWorld->UndoRemember ();

         // play beep so know did it
         EscChime (ESCCHIME_INFORMATION);
      }
      break;

   case IDC_PMSIDESPLIT:
      {
         if (m_rPolyMeshLastSplitLine.left != -12345)
            InvalidateRect (m_hWnd, &m_rPolyMeshLastSplitLine, FALSE);

         PCObjectPolyMesh pm = PolyMeshObject2();
         if (!pm)
            return FALSE;
         DWORD dwSide;
         dwSide = PolyMeshSideSplit (pm);
         if (dwSide != -1) {
            pm->m_pWorld->ObjectAboutToChange (pm);
            pm->m_PolyMesh.SideSplit (dwSide, &m_adwPolyMeshSplit[0][0], &m_adwPolyMeshSplit[1][0]);
            pm->m_pWorld->ObjectChanged (pm);
         }
      }
      break;

   case IDC_OBJCONTROLUD:
   case IDC_OBJCONTROLNSEW:
   case IDC_OBJCONTROLINOUT:
   case IDC_OBJCONTROLNSEWUD:
      {
         // set m_dwObjControlObject so we know we're not over an object
         m_dwObjControlObject = (DWORD)-1;
      }
      break;

   case IDC_SELREGION:
      {
         InvalidateDisplay();
         // if the mouse positions are the same then do nothing
         if ((m_pntMouseLast.x == m_pntButtonDown.x) && (m_pntMouseLast.y == m_pntButtonDown.y)) {
            goto alldone;
         }

         // from the rectangle figure out bounding box and stuff inside
         fp zNear, zFar;
         zFar = 1000;
         zNear = (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000.0 : 0.01;
         CPoint   ap[2][2][2];   //[x][y][z]

         // convert to a rectangle
         RECT r;
         r.left = min(m_pntMouseLast.x, m_pntButtonDown.x);
         r.right = max(m_pntMouseLast.x, m_pntButtonDown.x);
         r.top = min(m_pntMouseLast.y, m_pntButtonDown.y);
         r.bottom = max(m_pntMouseLast.y, m_pntButtonDown.y);

         DWORD x,y,z, dwX, dwY;
         for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
            // convert to image corrindates
            if (!PointInImage (x ? r.right : r.left, z ? r.top : r.bottom, &dwX, &dwY))
               goto alldone;

            // convert to world space
            m_apRender[0]->PixelToWorldSpace (dwX, dwY, y ? zFar : zNear, &ap[x][y][z]);
         }

         if (m_dwViewWhat == VIEWWHAT_POLYMESH) {
            RECT rImage;
            PointInImage (r.left, r.top, &dwX, &dwY);
            rImage.left = (int)dwX;
            rImage.top = (int)dwY;
            PointInImage (r.right, r.bottom, &dwX, &dwY);
            rImage.right = (int)dwX;
            rImage.bottom = (int)dwY;
            PolyMeshSelRegion (&rImage, ap);
         }
         else {
            // Call into the clip stuff
            CListFixed *plf;
            DWORD i;
            plf = ObjectsInVolume (this->m_pWorld, ap, m_fSelRegionWhollyIn);

            if (plf) {
               // prune list by requiring object to be visible?
               CMem memVis;
               if (!memVis.Required (plf->Num() * sizeof(BOOL))) {
                  delete plf;
                  goto alldone;
               }
               BOOL *pfVis;
               pfVis = (BOOL*) memVis.p;
               memset (pfVis, 0, plf->Num() * sizeof(BOOL));
               // prune this by requiring the object be visible
               if (m_fSelRegionVisible) {
                  // bounds
                  DWORD dwXL, dwXR, dwYT, dwYB;
                  // I know the points are in bounds because did checks above
                  PointInImage (r.left, r.top, &dwXL, &dwYT);
                  PointInImage (r.right, r.bottom, &dwXR, &dwYB);

                  for (y = dwYT; y <= dwYB; y++) {
                     PIMAGEPIXEL pip;
                     pip = m_aImage[0].Pixel (dwXL, y);
                     for (x = dwXL; x <= dwXR; x++, pip++) {
                        // if find it mark it so
                        DWORD *pdw, dwObject;
                        dwObject = HIWORD(pip->dwID);
                        if (!dwObject)
                           continue;   // background
                        dwObject -= 1;
                        pdw = (DWORD*) bsearch (&dwObject, plf->Get(0), plf->Num(), sizeof(DWORD), BCompare);
                        if (!pdw)
                           continue;   // didn't find

                        // else, found, so convert back to index into objects
                        i = (PBYTE) pdw - (PBYTE) plf->Get(0);
                        i /= sizeof(DWORD);
                        pfVis[i] = TRUE;

                     }
                  }
               } // if (m_fSelRegionVisible)
               else {
                  memset (pfVis, 1, plf->Num() * sizeof(BOOL));
               }

               // loop throught all the objects still left and add them
               for (i = 0; i < plf->Num(); i++) {
                  DWORD *pdw = (DWORD*) plf->Get(i);

                  // if it's not visible then skip
                  if (!pfVis[i])
                     continue;

                  // add
                  m_pWorld->SelectionAdd (*pdw);
               }
               delete plf;
            }
         } // if not polymesh

         // invalidate the display
         InvalidateDisplay();
      }
      break;


   case IDC_ZOOMINDRAG:
      {
         InvalidateDisplay();
         // if the mouse positions are the same then do nothing
         if ((m_pntMouseLast.x == m_pntButtonDown.x) && (m_pntMouseLast.y == m_pntButtonDown.y)) {
            goto alldone;
         }

         // convert to a rectangle
         RECT r;
         r.left = min(m_pntMouseLast.x, m_pntButtonDown.x);
         r.right = max(m_pntMouseLast.x, m_pntButtonDown.x);
         r.top = min(m_pntMouseLast.y, m_pntButtonDown.y);
         r.bottom = max(m_pntMouseLast.y, m_pntButtonDown.y);


         // into image
         DWORD dwL, dwR, dwT, dwB;
         if (!PointInImage (r.left, r.top, &dwL, &dwT) || !PointInImage (r.right, r.bottom, &dwR, &dwB)) {
            // out of the screen
            BeepWindowBeep (ESCBEEP_DONTCLICK);
            goto alldone;
         }

         // get the camera info
         fp fLongitude, fTilt, fTiltY, fScale, fTransX, fTransY;
         CPoint pCenter;
         m_apRender[0]->CameraFlatGet (&pCenter, &fLongitude, &fTilt, &fTiltY, &fScale, &fTransX, &fTransY);

         // new center
         m_apRender[0]->PixelToWorldSpace ((dwL + dwR) / 2, (dwT + dwB) / 2, 0, &pCenter);

         // new scale
         fScale *= fabs ((fp)dwR - (fp)dwL) /  (fp)m_aImage[0].Width();

         // write it out
         m_apRender[0]->CameraFlat (&pCenter, fLongitude, fTilt, fTiltY, fScale, 0,0);
         // BUGFIX - Was ,fTransX, fTransY);, and cause problems with had offset then
         // zoomed in
         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);

         // invalidate the display
         InvalidateDisplay();
         UpdateToolTipByPointerMode();
      }
      break;
   } // switch

alldone:
   // update the tooltip and cursor
   UpdateToolTipByPointerMode();
   int iX, iY;
   iX = (short) LOWORD(lParam);
   iY = (short) HIWORD(lParam);
   SetProperCursor (iX, iY);

   return 0;
}

/***********************************************************************************
CHouseView::MouseMove - Left button down message
*/
LRESULT CHouseView::MouseMove (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   DWORD dwButton = m_dwButtonDown ? (m_dwButtonDown - 1) : 0;
   DWORD dwPointerMode = m_adwPointerMode[dwButton];
   int iX, iY;
   iX = (short) LOWORD(lParam);
   iY = (short) HIWORD(lParam);
   POINT pLast;
   pLast = m_pntMouseLast;
   m_pntMouseLast.x = iX;
   m_pntMouseLast.y = iY;


   // BUGFIX
   if (GetFocus() != m_hWnd)
      SetFocus (m_hWnd);   // so list box doesnt take away focus

   // BUGFIX - If it hasn't moved then dont go futher
   if ((pLast.x == m_pntMouseLast.x) && (pLast.y == m_pntMouseLast.y))
      return 0;

   // set the cursor
   SetProperCursor (iX, iY);

   // if the mouse cursor is over the tooltip then move the tooltip
   if (m_pToolTip) {
      POINT p;
      GetCursorPos (&p);
      RECT r;
      GetWindowRect (m_pToolTip->m_hWnd, &r);
      r.left -= 100;
      r.right += 100;
      r.top -= 100;
      r.bottom += 100;
      if (PtInRect (&r, p)) {
         m_dwToolTip = (m_dwToolTip+1)%4;
         delete m_pToolTip;
         m_pToolTip = NULL;
         UpdateToolTipByPointerMode ();
      }
   }

   // if over the thumbnail buttons then show tooltip with instructions

   // if it's in one of the thumbnails then switch to that
   // NOTE: Disabling this because have instructions in the thumbnail box
   //DWORD i;
   //for (i = 1; i < VIEWTHUMB; i++) {
   //   if (!m_afThumbnailVis[i] || (i == m_dwThumbnailCur))
   //      continue;
   //   if (PtInRect (&m_arThumbnailLoc[i], m_pntMouseLast)) {
   //      UpdateToolTip ("Click on the thumbnail to switch to it. "
   //         "Control-click to copy the current view to it.");
   //      return 0;
   //   }
   //}


   if (m_lPolyMeshVert.Num()) {
      // moving mouse around while in create new poly mode...
      PCObjectPolyMesh pm = PolyMeshObject2();
      if (pm) {
         RECT r;
         PolyMeshVertToRect (pm, pLast, &r);
         if (r.left != r.right)
            InvalidateRect (m_hWnd, &r, FALSE);

         PolyMeshVertToRect (pm, m_pntMouseLast, &r);
         if (r.left != r.right)
            InvalidateRect (m_hWnd, &r, FALSE);
      }
   }

   // if moving around and no button down, and in the texture editing mode then
   // update the display
   if (!m_dwButtonDown && (m_dwViewSub == VSPOLYMODE_TEXTURE) && (m_dwViewWhat == VIEWWHAT_POLYMESH)) {
      DWORD dwX, dwY;
      if (PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY))
         PolyMeshTextureMouseMove (dwX, dwY);
   }

   if (m_dwButtonDown && dwPointerMode) switch (dwPointerMode) {
      case IDC_BRUSH4:
      case IDC_BRUSH8:
      case IDC_BRUSH16:
      case IDC_BRUSH32:
      case IDC_BRUSH64:
         // do nothing for now since it's an airbrush and serviced by timer
         
         // if were in the texture mode then do paint on move
         if (m_dwViewSub == VSPOLYMODE_TEXTURE)
            PolyMeshBrushApply (dwPointerMode, &m_pntMouseLast);
         break;

      case IDC_PMSIDESPLIT:
      {
         RECT r;
         PCObjectPolyMesh pm = PolyMeshObject2();
         if (!pm)
            return FALSE;

         DWORD dwX, dwY;
         if (!PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY))
            break;   // dont move

         // invalidate what currently have
         if (m_rPolyMeshLastSplitLine.left != -12345)
            InvalidateRect (m_hWnd, &m_rPolyMeshLastSplitLine, FALSE);

         // invalidate what will figure out what over
         m_adwPolyMeshSplit[1][0] = PolyMeshVertFromPixel (dwX, dwY);
         m_adwPolyMeshSplit[1][1] = -1;
         if (m_adwPolyMeshSplit[1][0] == -1)
            if (!PolyMeshEdgeFromPixel (dwX, dwY, &m_adwPolyMeshSplit[1][0])) {
               m_adwPolyMeshSplit[1][0] = m_adwPolyMeshSplit[1][1] = -1;
            }

         // invalidate what fill have
         PolyMeshSideSplitRect (pm, &r);
         RectImageToScreen (&r, TRUE);
         InvalidateRect (m_hWnd, &r, FALSE);
         m_rPolyMeshLastSplitLine = r;
      }
      break;

      case IDC_BONENEW:
      {
         RECT r;
         PCObjectBone pb = BoneObject2();
         if (!pb)
            return FALSE;

         DWORD dwX, dwY;
         if (!PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY))
            break;   // dont move

         // invalidate what currently have
         if (m_rPolyMeshLastSplitLine.left != -12345)
            InvalidateRect (m_hWnd, &m_rPolyMeshLastSplitLine, FALSE);

         // invalidate what fill have
         BoneAddRect (pb, m_dwObjControlObject, &r);
         RectImageToScreen (&r, TRUE);
         InvalidateRect (m_hWnd, &r, FALSE);
         m_rPolyMeshLastSplitLine = r;
      }
      break;

      case IDC_BONESCALE:
      case IDC_BONEROTATE:
         {
            PCObjectBone pb = BoneObject2();
            if (!pb)
               break;

            // get the location in the image
            DWORD dwX, dwY;
            if (!PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY))
               break;   // dont move

            // figure out how much to move
            POINT pDelta;
            pDelta.x = m_pntMouseLast.x - pLast.x;
            pDelta.y = m_pntMouseLast.y - pLast.y;
            CPoint pMove, pMoveX, pMoveY;
            pMoveX.Copy (&m_pScaleScrollX);
            pMoveX.Scale (pDelta.x);
            pMoveY.Copy (&m_pScaleScrollY);
            pMoveY.Scale (pDelta.y);
            pMove.Add (&pMoveX, &pMoveY);

            if (dwPointerMode == IDC_BONESCALE) {
               DWORD dwKeepSame = XYZKeyFlag ();
               pMove.p[0] = pMove.p[1] = pMove.p[2] = pow (2.0, (fp)pDelta.x / 250.0);
               if (dwKeepSame) {
                  if (dwKeepSame & 0x01)
                     pMove.p[0] = 1;
                  if (dwKeepSame & 0x02)
                     pMove.p[1] = 1;
                  if (dwKeepSame & 0x04)
                     pMove.p[2] = 1;
               }

               pb->ScaleBone (m_dwObjControlObject, &pMove);
            } // if bonescale
            else if (dwPointerMode == IDC_BONEROTATE) {
               fp fRot = (fp)pDelta.x / 250.0 * PI;

               pb->RotateBone (m_dwObjControlObject, -fRot);
            } // if bonescale

         }
         break;

      case IDC_PMMOVEPERP:
      case IDC_PMMOVEPERPIND:
      case IDC_PMTAILORNEW:
      case IDC_PMMOVESCALE:
      case IDC_PMMOVEROTPERP:
      case IDC_PMMOVEROTNONPERP:
      case IDC_PMMOVEINOUT:
      case IDC_PMMOVEANYDIR:
      case IDC_PMSIDEINSET:
      case IDC_PMSIDEEXTRUDE:
      case IDC_PMBEVEL:
      case IDC_PMMAGANY:
      case IDC_PMMAGNORM:
      case IDC_PMMAGVIEWER:
      case IDC_PMMAGPINCH:
      case IDC_PMORGSCALE:
      case IDC_PMMORPHSCALE:
      case IDC_PMTEXTSCALE:
      case IDC_PMTEXTMOVE:
      case IDC_PMTEXTROT:
         {
            PCObjectPolyMesh pm = PolyMeshObject2 ();
            if (!pm || !m_pPolyMeshOrig)
               break;

            // get the location in the image
            DWORD dwX, dwY;
            if (!PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY))
               break;   // dont move

            // figure out how much to move
            CPoint pMove, pMoveX, pMoveY;
            pMoveX.Copy (&m_pScaleScrollX);
            pMoveX.Scale (m_pntMouseLast.x - m_pntButtonDown.x);
            pMoveY.Copy (&m_pScaleScrollY);
            pMoveY.Scale (m_pntMouseLast.y - m_pntButtonDown.y);
            pMove.Add (&pMoveX, &pMoveY);

            DWORD dwMoveMode, dwKeepSame;
            DWORD i;
            TEXTUREPOINT tp;
            dwMoveMode = 0;
            tp.h = tp.v = 0;

            switch (dwPointerMode) {
            case IDC_PMTEXTSCALE:
               dwMoveMode = 2;
               dwKeepSame = XYZKeyFlag ();
               tp.h = tp.v = pow (2.0, (m_pntMouseLast.x - m_pntButtonDown.x) / 250.0);
               if (dwKeepSame) {
                  if (dwKeepSame & 0x01)
                     tp.h = 1;
                  if (dwKeepSame & 0x02)
                     tp.v = 1;
               }
               break;

            case IDC_PMTEXTMOVE:
               dwMoveMode = 0;

               m_fPolyMeshTextDispScale = max(m_fPolyMeshTextDispScale, CLOSE);
               tp.h = (fp)(m_pntMouseLast.x - m_pntButtonDown.x) / m_fPolyMeshTextDispScale;
               tp.v = (fp)(m_pntMouseLast.y - m_pntButtonDown.y) / m_fPolyMeshTextDispScale;

               dwKeepSame = XYZKeyFlag ();
               if (dwKeepSame & 0x01)
                  tp.h = 0;
               if (dwKeepSame & 0x02)
                  tp.v = 0;
               break;
            case IDC_PMTEXTROT:
               dwMoveMode = 1;
               tp.h = -(m_pntMouseLast.x - m_pntButtonDown.x) / 250.0 * PI;
               tp.h = ApplyGrid (tp.h, m_fGridAngle);
               break;

            case IDC_PMMORPHSCALE:
            case IDC_PMORGSCALE:
               dwKeepSame = XYZKeyFlag ();
               pMove.p[0] = pMove.p[1] = pMove.p[2] = pow (2.0, (m_pntMouseLast.x - m_pntButtonDown.x) / 250.0);
               if (dwKeepSame) {
                  if (dwKeepSame & 0x01)
                     pMove.p[0] = 1;
                  if (dwKeepSame & 0x02)
                     pMove.p[1] = 1;
                  if (dwKeepSame & 0x04)
                     pMove.p[2] = 1;
               }
               break;

            case IDC_PMMAGANY:
            case IDC_PMMAGVIEWER:
               dwMoveMode = 2;

               dwKeepSame = XYZKeyFlag ();
               if (dwKeepSame & 0x01)
                  pMove.p[0] = 0;
               if (dwKeepSame & 0x02)
                  pMove.p[1] = 0;
               if (dwKeepSame & 0x04)
                  pMove.p[2] = 0;

               // apply the grid - this isn't the perfect solution since will ultimately
               // incur error in the movement,but it should work ok
               for (i = 0; i < 3; i++)
                  pMove.p[i] = ApplyGrid (pMove.p[i], m_fGrid);
               break;

            case IDC_PMMAGNORM:
               dwMoveMode = 0;
               pMove.Zero();

               // BUGFIX - Should be able to go in reverse
               pMove.p[0] = m_pScaleScrollX.Length() * (m_pntMouseLast.x - m_pntButtonDown.x) - // use - on purpse
                  m_pScaleScrollY.Length() * (m_pntMouseLast.y - m_pntButtonDown.y);
               if (pMove.p[0] < 0) {
                  pMove.p[0] *= -1;
                  dwMoveMode = 1;
               }
               break;

            case IDC_PMMAGPINCH:
               dwMoveMode = 6;
               pMove.Zero();
               pMove.p[0] = pow (2.0, (m_pntMouseLast.x - m_pntButtonDown.x) / 250.0);
               break;

            case IDC_PMSIDEINSET:
               dwMoveMode = 1;
               pMove.p[0] = pow (2.0, (m_pntMouseLast.x - m_pntButtonDown.x) / 250.0);
               break;

            case IDC_PMBEVEL:
               pMove.p[0] = m_pScaleScrollX.Length() * fabs((fp)(m_pntMouseLast.x - m_pntButtonDown.x)) - // use - on purpse
                  m_pScaleScrollY.Length() * fabs((fp)(m_pntMouseLast.y - m_pntButtonDown.y));
               break;

            case IDC_PMSIDEEXTRUDE:
               dwMoveMode = 0;
               pMove.p[0] = m_pScaleScrollX.Length() * (m_pntMouseLast.x - m_pntButtonDown.x) - // use - on purpse
                  m_pScaleScrollY.Length() * (m_pntMouseLast.y - m_pntButtonDown.y);
               break;

            default:
            case IDC_PMMOVEPERP:
               dwMoveMode = 1;
               // BUGFIX - Should be able to go in reverse
               pMove.p[0] = m_pScaleScrollX.Length() * (m_pntMouseLast.x - m_pntButtonDown.x) - // use - on purpse
                  m_pScaleScrollY.Length() * (m_pntMouseLast.y - m_pntButtonDown.y);
               break;

            case IDC_PMMOVEPERPIND:
               dwMoveMode = 5;
               // BUGFIX - Should be able to go in reverse
               pMove.p[0] = m_pScaleScrollX.Length() * (m_pntMouseLast.x - m_pntButtonDown.x) - // use - on purpse
                  m_pScaleScrollY.Length() * (m_pntMouseLast.y - m_pntButtonDown.y);
               break;

            case IDC_PMTAILORNEW:
               dwMoveMode = 5;
               pMove.p[0] = m_pScaleScrollX.Length() * fabs((fp)(m_pntMouseLast.x - m_pntButtonDown.x)) +
                  m_pScaleScrollY.Length() * fabs((fp)(m_pntMouseLast.y - m_pntButtonDown.y));
               pMove.p[0] /= 10.0;  // so doesnt enlarge as fast
               break;

            case IDC_PMMOVESCALE:
               dwMoveMode = 2;
               dwKeepSame = XYZKeyFlag ();
               pMove.p[0] = pMove.p[1] = pMove.p[2] = pow (2.0, (m_pntMouseLast.x - m_pntButtonDown.x) / 250.0);
               if (dwKeepSame) {
                  if (dwKeepSame & 0x01)
                     pMove.p[0] = 1;
                  if (dwKeepSame & 0x02)
                     pMove.p[1] = 1;
                  if (dwKeepSame & 0x04)
                     pMove.p[2] = 1;
               }
               break;

            case IDC_PMMOVEROTPERP:
               dwMoveMode = 3;
               pMove.p[0] = -(m_pntMouseLast.x - m_pntButtonDown.x) / 250.0 * PI;
               pMove.p[0] = ApplyGrid (pMove.p[0], m_fGridAngle);
               break;

            case IDC_PMMOVEROTNONPERP:
               dwMoveMode = 4;
               pMove.p[0] = -(m_pntMouseLast.x - m_pntButtonDown.x) / 250.0 * PI;
               pMove.p[1] = (m_pntMouseLast.y - m_pntButtonDown.y) / 250.0 * PI;
               pMove.p[0] = ApplyGrid (pMove.p[0], m_fGridAngle);
               pMove.p[1] = ApplyGrid (pMove.p[1], m_fGridAngle);
               break;

            case IDC_PMMOVEANYDIR:
            case IDC_PMMOVEINOUT:
               dwMoveMode = 0;

               dwKeepSame = XYZKeyFlag ();
               if (dwKeepSame & 0x01)
                  pMove.p[0] = 0;
               if (dwKeepSame & 0x02)
                  pMove.p[1] = 0;
               if (dwKeepSame & 0x04)
                  pMove.p[2] = 0;

               // apply the grid - this isn't the perfect solution since will ultimately
               // incur error in the movement,but it should work ok
               for (i = 0; i < 3; i++)
                  pMove.p[i] = ApplyGrid (pMove.p[i], m_fGrid);
               break;

            }

            // get selection
            DWORD dwNum;
            DWORD *padw;
            if (m_adwPolyMeshMove[0] != -1) {
               padw = m_adwPolyMeshMove;
               dwNum = 1;
            }
            else
               padw = m_pPolyMeshOrig->SelEnum (&dwNum);

            BOOL fControl = (GetKeyState (VK_CONTROL) < 0);

            // else, change
            pm->m_pWorld->ObjectAboutToChange (pm);
            m_pPolyMeshOrig->CloneTo (&pm->m_PolyMesh);

            switch (dwPointerMode) {
            case IDC_PMSIDEINSET:
            case IDC_PMSIDEEXTRUDE:
               if (!pm->m_PolyMesh.SideExtrude (dwNum, padw, dwMoveMode, fControl, pMove.p[0]))
                  BeepWindowBeep (ESCBEEP_DONTCLICK); // error
               break;

            case IDC_PMBEVEL:
               pm->m_PolyMesh.GroupBevel (pMove.p[0], pm->m_PolyMesh.SelModeGet(),
                  dwNum, padw);
               break;

            case IDC_PMORGSCALE:
               pm->m_PolyMesh.OrganicScale (&pMove, pm, m_dwPolyMeshMaskColor, m_fPolyMeshMaskInvert);
               break;

            case IDC_PMMORPHSCALE:
               pm->m_PolyMesh.MorphScale (&pMove);
               break;

            case IDC_PMMAGANY:
            case IDC_PMMAGVIEWER:
            case IDC_PMMAGNORM:
            case IDC_PMMAGPINCH:
               {
                  // brush power
                  fp fPow;
                  switch (m_dwPolyMeshMagPoint) {
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
                  default:
                     fPow = 5;
                     break;
                  }

                  fp fRadius;
                  fRadius = (fp)(8 << m_dwPolyMeshMagSize) *
                     max(m_pScaleScrollX.Length(), m_pScaleScrollY.Length());

                  pm->m_PolyMesh.OrganicMove (&m_pPolyMeshClick, fRadius, fPow, 1.0,
                     0, NULL, dwMoveMode, &pMove,
                     pm, m_dwPolyMeshMaskColor, m_fPolyMeshMaskInvert);
               }
               break;

            case IDC_PMTEXTSCALE:
            case IDC_PMTEXTMOVE:
            case IDC_PMTEXTROT:
               {
                  DWORD dwSelMode = pm->m_PolyMesh.SelModeGet();

                  // create a list of sides
                  DWORD dwNumSide;
                  DWORD *padwSides;
                  if (dwSelMode == 2) {
                     // selected sides, so the list of sides is pretty easy
                     dwNumSide = dwNum;
                     padwSides = padw;
                  }
                  else if ((dwSelMode == 0) && (m_adwPolyMeshMove[0] != -1) && (m_dwPolyMeshMoveSide != -1)) {
                     // clicked on a single point so know the side
                     dwNumSide = 1;
                     padwSides = &m_dwPolyMeshMoveSide;
                  }
                  else {
                     dwNumSide = 0;
                     padwSides = NULL;
                  }

                  // figure out the surface
                  DWORD dwSurface = pm->ObjectSurfaceGetIndex (m_dwPolyMeshCurText);

                  // create a list of vertices
                  CListFixed lVert;
                  DWORD dwNumVert = 0;
                  DWORD *padwVert = 0;
                  if (dwSelMode == 2) {
                     // convert sides to vertices
                     pm->m_PolyMesh.SidesToVert (padw, dwNum, &lVert);
                     dwNumVert = lVert.Num();
                     padwVert = (DWORD*)lVert.Get(0);

                     if (fControl)
                        pm->m_PolyMesh.TextureDisconnect (dwNumSide, padwSides, dwSurface);
                  }
                  else if (dwSelMode == 1) {
                     // convert edges to vertices
                     pm->m_PolyMesh.EdgesToVert (padw, dwNum, &lVert);
                     dwNumVert = lVert.Num();
                     padwVert = (DWORD*)lVert.Get(0);

                     if (fControl)
                        pm->m_PolyMesh.TextureDisconnectVert (dwNumVert, padwVert,
                           dwNumSide, padwSides, dwSurface);
                  }
                  else if (dwSelMode == 0) {
                     // have points selected, use that
                     dwNumVert = dwNum;
                     padwVert = padw;

                     if (fControl)
                        pm->m_PolyMesh.TextureDisconnectVert (dwNumVert, padwVert,
                           dwNumSide, padwSides, dwSurface);
                  }

                  // move
                  pm->m_PolyMesh.TextureMove (dwNumVert, padwVert, dwNumSide, padwSides,
                     dwSurface, dwMoveMode, &tp);
               }
               break;

            default:
               pm->m_PolyMesh.GroupMove (dwMoveMode, &pMove, pm->m_PolyMesh.SelModeGet(),
                  dwNum, padw);
               break;

            } // switch dwPointerMode
            pm->m_pWorld->ObjectChanged (pm);

         }
         break;

      case IDC_OBJCONTROLINOUT:
      case IDC_OBJCONTROLUD:
      case IDC_OBJCONTROLNSEW:
      case IDC_OBJCONTROLNSEWUD:
         {
            // get the location in the image
            DWORD dwX, dwY;
            if (!PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY))
               break;   // dont move

            // find out where this would be in world space
            CPoint pClick, pViewer;

            // if the is for in/out then convert pClick to an in/out direction
            if (dwPointerMode == IDC_OBJCONTROLINOUT) {
               // look at where originally clicked
               DWORD dwOrigX, dwOrigY;
               if (!PointInImage (m_pntButtonDown.x, m_pntButtonDown.y, &dwOrigX, &dwOrigY))
                  break;   // dont move

               // find the distance
               fp fLen;
               CPoint pOrig;
               m_apRender[0]->PixelToWorldSpace (dwX, dwOrigY, m_fObjControlOrigZ, &pClick);
               m_apRender[0]->PixelToWorldSpace (dwOrigX, dwOrigY, m_fObjControlOrigZ, &pOrig);
               pClick.Subtract (&pOrig);
               fLen = pClick.Length();
               if (dwOrigX < dwX)
                  fLen *= -1;

               // go back to where originally clicked
               m_apRender[0]->PixelToWorldSpace (dwOrigX, dwOrigY,
                  (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pViewer);
               pClick.Subtract (&pViewer, &pOrig);
               pClick.Normalize();
               pClick.Scale (fLen);
               pClick.Add (&pOrig);
            }
            else {
               // find out where cusor is now
               m_apRender[0]->PixelToWorldSpace (dwX, dwY, m_fObjControlOrigZ, &pClick);
               m_apRender[0]->PixelToWorldSpace (dwX, dwY,
                  (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pViewer);
            }

            // convert to object space
            PCObjectSocket pos;
            pos = m_pWorld->ObjectGet (m_dwObjControlObject);
            if (!pos)
               break;   // cant use this either
            CMatrix m, m2;
            pos->ObjectMatrixGet (&m2);
            m2.Invert4(&m);
            pClick.MultiplyLeft (&m);
            pViewer.MultiplyLeft (&m);

            // take out the bias
            pClick.Subtract (&m_pObjControlClick);
            pClick.Add (&m_pObjControlOrig);
            pViewer.Subtract (&m_pObjControlClick);
            pViewer.Add (&m_pObjControlOrig);

            // re-rotate the pClick and fit to grid
            //pClick.p[3] = 1;
            pClick.MultiplyLeft (&m2);

            // BUGFIX - Grid for moving control point in any direction had some problems
            DWORD dwGrid;
            dwGrid = 0x07; // any direction
            if (dwPointerMode == IDC_OBJCONTROLUD)
               dwGrid = 0x04;
            else if (dwPointerMode == IDC_OBJCONTROLNSEW)
               dwGrid = 0x03;
            GridMove (&pClick,  dwGrid);

            //pClick.p[3] = 1;
            pClick.MultiplyLeft (&m);  // move it back

            // see what mode the drag point wants to move in
            OSCONTROL osc;
            if (!pos->ControlPointQuery (m_dwObjControlID, &osc))
               break;

            // where want to set
            CPoint pSet;
            BOOL fWantToSet;
            fWantToSet = TRUE;

#if 0 // no longer used
            if (osc.dwFreedom == 2) {  // line
               // this is a heap of work to make sure that if the cursor
               // is moved in line with the direction of the control point
               // then the control point stays under the cursor

               // find a vector that's perpendicular to the direction vector
               // and the viewer->click vector
               CPoint pVN, pDN, pOrth;
               pDN.Copy (&osc.pV1);
               pDN.Normalize();
               pVN.Subtract (&pClick, &pViewer);
               pVN.Normalize();
               pOrth.CrossProd (&pDN, &pVN);
               if (pOrth.Length() < .001)
                  break;   // the're too close together so dont move
               pOrth.Normalize();

               // use the direction of the line, and the new orthogonal
               // vetor (which is at right angles to the line and the viewer)
               // as a second plane. Find the intersection of the viewer->click
               // line with that plane
               CPoint pInter;
               CPoint pN;
               pN.CrossProd (&pOrth, &pDN);
               if (!IntersectLinePlane (&pViewer, &pClick, &osc.pLocation, &pN, &pInter))
                  break;   // parallel so dont intersect


               // now, that have intersection on that plane, map the intersection
               // onto the line
               pInter.Subtract (&osc.pLocation);
               if (pInter.Length() < EPSILON)
                  break;
               pClick.Copy (&pDN);
               pClick.Scale (pInter.DotProd(&pDN));
               pClick.Add (&osc.pLocation);

               pSet.Copy (&pClick);


            }
            else if (osc.dwFreedom == 1) {   // plane
               // find out where the line intersects with the plane and use this
               CPoint pInter;
               CPoint pN;
               pN.CrossProd (&osc.pV1, &osc.pV2);
               if (IntersectLinePlane (&pViewer, &pClick, &osc.pLocation,
                  &pN, &pInter))
                     pSet.Copy (&pInter);
               else
                  fWantToSet = FALSE;
            }
#endif // 0
            //else {   // total freedom
               // set the control point
               pSet.Copy (&pClick);
            //}

            // limit movement
            pSet.MultiplyLeft (&m2);
            osc.pLocation.p[3] = 1;
            osc.pLocation.MultiplyLeft (&m2);
            DWORD dwKeepSame;
            dwKeepSame = XYZKeyFlag ();
            if (dwPointerMode == IDC_OBJCONTROLNSEW) {
               dwKeepSame |= (1 << 2);
               //pSet.p[2] = osc.pLocation.p[2];
            }
            else if (dwPointerMode == IDC_OBJCONTROLUD) {
               dwKeepSame |= (1 << 0) | (1 << 1);
               //pSet.p[0] = osc.pLocation.p[0];
               //pSet.p[1] = osc.pLocation.p[1];
            }
            if (dwKeepSame & 0x01)
               pSet.p[0] = osc.pLocation.p[0];
            if (dwKeepSame & 0x02)
               pSet.p[1] = osc.pLocation.p[1];
            if (dwKeepSame & 0x04)
               pSet.p[2] = osc.pLocation.p[2];

            pSet.MultiplyLeft (&m);
            osc.pLocation.MultiplyLeft (&m);

            // set it
            if (fWantToSet)
               pos->ControlPointSet (m_dwObjControlID, &pSet, &pViewer);
         }
         break;

      case IDC_VIEWFLATLRUD:
         {
            // get current camera information
            CPoint p;
            p.Zero();
            p.p[0] = m_fCFTransX;
            p.p[2] = m_fCFTransY;
            
            // add the scaling
            CPoint ps;
            ps.Copy (&m_pScaleScrollX);
            ps.Scale (iX - m_pntButtonDown.x);
            p.Add (&ps);
            ps.Copy (&m_pScaleScrollY);
            ps.Scale (iY - m_pntButtonDown.y);
            p.Add (&ps);

            // find the point and see how far back it is
            //p.p[0] += (iX - m_pntButtonDown.x) * m_fScaleScroll;
            //if (dwPointerMode == IDC_VIEW3DLRUD)
            //   p.p[2] -= (iY - m_pntButtonDown.y) * m_fScaleScroll;
            //else
            //   p.p[1] -= (iY - m_pntButtonDown.y) * m_fScaleScroll;

            // change the camera and re-render
            m_apRender[0]->CameraFlat (&m_pCameraCenter, m_fCFLong, m_fCFTilt, m_fCFTiltY, m_fCFScale, p.p[0], p.p[2]);
            CameraPosition();
            RenderUpdate(WORLDC_CAMERAMOVED);


            // display new settings
            UpdateToolTipByPointerMode();
#if 0 // old way of doing this
            // move the image
            fp fTransX, fTransY;
            fTransX = m_fCFTransX + (iX - m_pntButtonDown.x) * (fp) IMAGESCALE/ (fp) m_dwImageScale
               / (fp)m_aImage[0].Width() * m_fCFScale;
            fTransY = m_fCFTransY - (iY - m_pntButtonDown.y) * (fp) IMAGESCALE/ (fp) m_dwImageScale
               / (fp)m_aImage[0].Width() * m_fCFScale;

            // change the camera and re-render
            m_apRender[0]->CameraFlat (m_fCFLong, m_fCFTilt, m_fCFTiltY, m_fCFScale, fTransX, fTransY);
            CameraPosition();
            RenderUpdate(WORLDC_CAMERAMOVED);
#endif // 0
         }
         break;

      case IDC_VIEWFLATLLOOKAT:
      case IDC_OBJECTPASTEDRAG:  // take advantage of invalidation of scren
         {
            // invalidate the window around the camera so causes it to draw indicator
            RECT r;
            r.left = min(pLast.x, min(m_pntButtonDown.x, m_pntMouseLast.x)) - M3DBUTTONSIZE;
            r.right = max(pLast.x, max(m_pntButtonDown.x, m_pntMouseLast.x)) + M3DBUTTONSIZE;
            r.top = min(pLast.y, min(m_pntButtonDown.y, m_pntMouseLast.y)) - M3DBUTTONSIZE;
            r.bottom = max(pLast.y, max(m_pntButtonDown.y, m_pntMouseLast.y)) + M3DBUTTONSIZE;
            InvalidateRect (m_hWnd, &r, FALSE);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;

      case IDC_OBJOPENCLOSE:
         {
            if (m_dwObjControlObject == -1)
               break;
            PCObjectSocket pos;
            pos = m_pWorld->ObjectGet(m_dwObjControlObject);
            if (!pos)
               break;

            // distance
            int iDist;
            iDist = m_pntMouseLast.x - m_pntButtonDown.x;
            pos->OpenSet (m_fOpenStart + (fp)iDist / 200);
         }
         break;
      case IDC_OBJONOFF:
         {
            if (m_dwObjControlObject == -1)
               break;
            PCObjectSocket pos;
            pos = m_pWorld->ObjectGet(m_dwObjControlObject);
            if (!pos)
               break;

            // distance
            int iDist;
            iDist = m_pntMouseLast.x - m_pntButtonDown.x;
            pos->TurnOnSet (m_fOpenStart + (fp)iDist / 200);
         }
         break;

      case IDC_SELREGION:
      case IDC_ZOOMINDRAG:
         {
            // invalidate the window around the box so its drawn
            RECT r;
            r.left = min(pLast.x, min(m_pntButtonDown.x, m_pntMouseLast.x)) - M3DBUTTONSIZE;
            r.right = max(pLast.x, max(m_pntButtonDown.x, m_pntMouseLast.x)) + M3DBUTTONSIZE;
            r.top = min(pLast.y, min(m_pntButtonDown.y, m_pntMouseLast.y)) - M3DBUTTONSIZE;
            r.bottom = max(pLast.y, max(m_pntButtonDown.y, m_pntMouseLast.y)) + M3DBUTTONSIZE;
            InvalidateRect (m_hWnd, &r, FALSE);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;

      case IDC_CLIPLINE:
         {
            // get the new value
            if ((m_pntMouseLast.x == m_pntButtonDown.x) && (m_pntMouseLast.y == m_pntButtonDown.y)) {
               m_fClipAngleValid = FALSE;

               // clear out old clip plane
               m_apRender[0]->ClipPlaneRemove (CLIPPLANE_USER);

               // render
               RenderUpdate(WORLDC_NEEDTOREDRAW);

               // display new settings
               UpdateToolTipByPointerMode();
               break;   // dont do anything since cant test plan
            }

            fp fLast;
            fLast = m_fClipTempAngle;

            m_fClipTempAngle = atan2 ( (fp)(m_pntMouseLast.x - m_pntButtonDown.x),
               (fp)(m_pntMouseLast.y - m_pntButtonDown.y) );
            m_fClipTempAngle = ApplyGrid (m_fClipTempAngle, m_fGridAngle);

            if ((fLast != m_fClipTempAngle) || !m_fClipAngleValid) {
               m_fClipAngleValid = TRUE;

               // calculate new clip plane. Get three points in world space
               fp fX, fY;
               DWORD dwX, dwY;
               PointInImage (m_pntButtonDown.x, m_pntButtonDown.y, &dwX, &dwY);
               fX = sin(m_fClipTempAngle);
               fY = cos(m_fClipTempAngle);
               RTCLIPPLANE t;
               memset (&t, 0, sizeof(t));
               double fZ;
               fZ = m_apRender[0]->PixelToZ (dwX,dwY);
               if (fZ > 1000)
                  fZ  = 0; // clicked on blank
               m_apRender[0]->PixelToWorldSpace ((fp)dwX, (fp)dwY, fZ, &t.ap[0]);
               m_apRender[0]->PixelToWorldSpace ((fp)dwX + fX*2, (fp)dwY + fY*2, fZ, &t.ap[1]);
               m_apRender[0]->PixelToWorldSpace ((fp)dwX + fX, (fp)dwY + fY, fZ+1, &t.ap[2]);

               // normalize vectors to 1 meter for more accuracy and
               // so that when show as numbers on screen is more readable
               CPoint v1, v2;
               v1.Subtract (&t.ap[1], &t.ap[0]);
               v2.Subtract (&t.ap[2], &t.ap[0]);
               v1.Normalize();
               v2.Normalize();
               t.ap[1].Add (&t.ap[0], &v1);
               t.ap[2].Add (&t.ap[0], &v2);

               // add it
               t.dwID = CLIPPLANE_USER;
               m_apRender[0]->ClipPlaneSet (CLIPPLANE_USER, &t);

               RenderUpdate(WORLDC_NEEDTOREDRAW);
            }

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;

      case IDC_VIEWFLATSCALE:
         {
            // move the image
            fp fScale;
            fScale = m_fCFScale * pow (2.0, (m_pntButtonDown.x - iX) / 250.0);
               // moving 250 pixels to right doubles size, 250 to left halves

            // change the camera and re-render
            m_apRender[0]->CameraFlat (&m_pCameraCenter, m_fCFLong, m_fCFTilt, m_fCFTiltY, fScale, m_fCFTransX, m_fCFTransY);
            CameraPosition();
            RenderUpdate(WORLDC_CAMERAMOVED);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;
      case IDC_VIEW3DROTZ:
      case IDC_VIEW3DROTX:
      case IDC_VIEW3DROTY:
         {
            // get current camera information
            fp fRotZ, fRotX, fRotY;
            fRotZ = m_fV3DRotZ;
            fRotY = m_fV3DRotY;
            fRotX = m_fV3DRotX;

            // find the point and see how far back it is
            if (dwPointerMode == IDC_VIEW3DROTZ)
               fRotZ += (iX - m_pntButtonDown.x) * 2 * PI / 750;
            else if (dwPointerMode == IDC_VIEW3DROTX)
               fRotX += (iY - m_pntButtonDown.y) * 2 * PI / 750;
            else
               fRotY += (iY - m_pntButtonDown.y) * 2 * PI / 750;

            fRotZ = ApplyGrid(fmod(fRotZ, (fp)(2 * PI)), m_fGridAngle);
            fRotX = ApplyGrid(fmod(fRotX, (fp)(2 * PI)), m_fGridAngle);
            fRotY = ApplyGrid(fmod(fRotY, (fp)(2 * PI)), m_fGridAngle);

            // change the camera and re-render
            m_apRender[0]->CameraPerspObject (&m_pV3D, &m_pCameraCenter, fRotZ, fRotX, fRotY, m_fV3DFOV);
            CameraPosition ();
            RenderUpdate(WORLDC_CAMERAMOVED);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;
      case IDC_VIEWFLATROTZ:
         {
            // move the image
            fp fLong;
            fLong = m_fCFLong + (iX - m_pntButtonDown.x) * 2.0 * PI / 750;
               // BUGFIX - Changed from m_fCFLong MINUS to m_CFLong PLUS
               // 750 pixels is a full rotation
            fLong = ApplyGrid(fmod(fLong, (fp)(2 * PI)), m_fGridAngle);

            // change the camera and re-render
            m_apRender[0]->CameraFlat (&m_pCameraCenter, fLong, m_fCFTilt, m_fCFTiltY, m_fCFScale, m_fCFTransX, m_fCFTransY);
            CameraPosition();
            RenderUpdate(WORLDC_CAMERAMOVED);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;

      case IDC_VIEWFLATROTX:
         {
            // move the image
            fp fTilt;
            fTilt = m_fCFTilt + (iY - m_pntButtonDown.y) * 2 * PI / 750;
               // 750 pixels is a full rotation
            fTilt = ApplyGrid(fmod(fTilt, (fp)(2 * PI)), m_fGridAngle);

            // change the camera and re-render
            m_apRender[0]->CameraFlat (&m_pCameraCenter, m_fCFLong, fTilt, m_fCFTiltY, m_fCFScale, m_fCFTransX, m_fCFTransY);
            CameraPosition();
            RenderUpdate(WORLDC_CAMERAMOVED);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;

      case IDC_VIEWFLATROTY:
         {
            // move the image
            fp fTiltY;
            fTiltY = m_fCFTiltY + (iY - m_pntButtonDown.y) * 2 * PI / 750;
               // 750 pixels is a full rotation
            fTiltY = ApplyGrid(fmod(fTiltY, (fp)(2 * PI)), m_fGridAngle);

            // change the camera and re-render
            m_apRender[0]->CameraFlat (&m_pCameraCenter, m_fCFLong, m_fCFTilt, fTiltY, m_fCFScale, m_fCFTransX, m_fCFTransY);
            CameraPosition();
            RenderUpdate(WORLDC_CAMERAMOVED);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;


#if FAILEDCODE // tried to get click and drag on point to rotate
      case IDC_VIEWFLATROTZ:
      case IDC_VIEWFLATROTX:
      case IDC_VIEWFLATROTY:
      case IDC_VIEW3DROTZ:
      case IDC_VIEW3DROTX:
      case IDC_VIEW3DROTY:
         {
            BOOL f3d = (dwPointerMode == IDC_VIEW3DROTZ) ||(dwPointerMode == IDC_VIEW3DROTY) ||(dwPointerMode == IDC_VIEW3DROTX);

            // found out where moved to
            DWORD dwX, dwY;
            if (!PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY))
               break;   // dont move

            // find new matrix
            CMatrix mNew, mInv;
            if (!RotationDrag (dwX, dwY, &mNew, TRUE, FALSE, TRUE))
               break;   // dont move
            //mNew.Invert4 (&mInv);
            mInv.Copy (&mNew);

            CPoint p;
            fp fx, fy, fz;
            mInv.ToXYZLLT (&p, &fz, &fx, &fy);

            // apply the grid
            fx = ApplyGrid(fmod(fx, 2 * PI), m_fGridAngle);
            fy = ApplyGrid(fmod(fy, 2 * PI), m_fGridAngle);
            fz = ApplyGrid(fmod(fz, 2 * PI), m_fGridAngle);

            // change the camera and re-render
            if (f3d)
               m_apRender[0]->CameraPerspObject (&m_pV3D, &m_pCameraCenter, fz, fx, fy, m_fV3DFOV);
            else
               m_apRender[0]->CameraFlat (&m_pCameraCenter, fz, fx, fy, m_fCFScale, m_fCFTransX, m_fCFTransY);
            CameraPosition();
            RenderUpdate(WORLDC_CAMERAMOVED);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;
#endif // FAILEDCODE
      case IDC_VIEW3DLRUD:
      case IDC_VIEW3DLRFB:
         {
            // get current camera information
            CPoint p;
            p.Copy (&m_pV3D);

            // add the scaling
            CPoint ps;
            ps.Copy (&m_pScaleScrollX);
            ps.Scale (iX - m_pntButtonDown.x);
            p.Add (&ps);
            ps.Copy (&m_pScaleScrollY);
            ps.Scale (iY - m_pntButtonDown.y);
            p.Add (&ps);

            // find the point and see how far back it is
            //p.p[0] += (iX - m_pntButtonDown.x) * m_fScaleScroll;
            //if (dwPointerMode == IDC_VIEW3DLRUD)
            //   p.p[2] -= (iY - m_pntButtonDown.y) * m_fScaleScroll;
            //else
            //   p.p[1] -= (iY - m_pntButtonDown.y) * m_fScaleScroll;

            // change the camera and re-render
            m_apRender[0]->CameraPerspObject (&p, &m_pCameraCenter, m_fV3DRotZ, m_fV3DRotX, m_fV3DRotY, m_fV3DFOV);
            CameraPosition ();
            RenderUpdate(WORLDC_CAMERAMOVED);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;

      case IDC_MOVENSEWUD:
      case IDC_MOVENSEW:
      case IDC_MOVEEMBED:
      case IDC_MOVEUD:
         {
            if (m_fMoveEmbedded) {
               // moving embedded object
               DWORD dwNum, *pdw;
               pdw = m_pWorld->SelectionEnum(&dwNum);
               if (dwNum != 1)
                  break;   // shouldn't happen
               PCObjectSocket pEmbed, pContain;
               pEmbed = m_pWorld->ObjectGet (pdw[0]);
               if (!pEmbed)
                  break;   // shouldnt happen
               GUID gContain, gEmbed;
               pEmbed->GUIDGet (&gEmbed);
               if (!pEmbed->EmbedContainerGet (&gContain))
                  break;   // shouldn't happen
               pContain = m_pWorld->ObjectGet (m_pWorld->ObjectFind(&gContain));
               if (!pContain)
                  break;   // shouldn't happen

               fp fZ;
               CPoint pEye, pClick;
               DWORD dwX, dwY;
               if (!PointInImage (iX, iY, &dwX, &dwY))
                  break;   // dont move
               fZ = m_apRender[0]->PixelToZ (dwX,dwY);
               m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pClick);
               m_apRender[0]->PixelToWorldSpace (dwX, dwY,
                  (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);
               CMatrix m, m2;
               pContain->ObjectMatrixGet (&m2);
               m2.Invert4(&m);
               pEye.MultiplyLeft (&m);
               pClick.MultiplyLeft (&m);

               // see where clicking intersects the surface
               TEXTUREPOINT tp;
               if (!pContain->ContHVQuery (&pEye, &pClick, m_dwMoveEmbedSurface, &m_MoveEmbedOld, &tp))
                  break;   // cant go here
               m_MoveEmbedOld = tp;

               // readjust the texture point based on offset
               tp.h = tp.h - m_MoveEmbedClick.h + m_MoveEmbedHV.h;
               tp.v = tp.v - m_MoveEmbedClick.v + m_MoveEmbedHV.v;
               tp.h = min(1,tp.h);
               tp.h = max(0, tp.h);
               tp.v = min(1, tp.v);
               tp.v = max(0, tp.v);

               SnapEmbedToGrid (pContain, m_dwMoveEmbedSurface, &tp);

               // write it out
               pContain->ContEmbeddedLocationSet (&gEmbed, &tp, m_fMoveEmbedRotation, m_dwMoveEmbedSurface);


            }
            else {
               // moving non-embedded object

               // get original object location and move based on that
               CPoint p;
               p.Copy (&m_pMoveRotReferenceInWorld);

               // if hold down xyz can only move in certain direction
               DWORD dwKeepSame, i;
               CPoint pScale;
               dwKeepSame = XYZKeyFlag ();
               pScale.p[0] = (dwKeepSame & 0x01) ? 0 : 1;
               pScale.p[1] = (dwKeepSame & 0x02) ? 0 : 1;
               pScale.p[2] = (dwKeepSame & 0x04) ? 0 : 1;

               // add the scaling
               CPoint ps;
               ps.Copy (&m_pScaleScrollX);
               for (i = 0; i < 3; i++)
                  ps.p[i] *= pScale.p[i];
               ps.Scale (iX - m_pntButtonDown.x);
               p.Add (&ps);
               ps.Copy (&m_pScaleScrollY);
               for (i = 0; i < 3; i++)
                  ps.p[i] *= pScale.p[i];
               ps.Scale (iY - m_pntButtonDown.y);
               p.Add (&ps);

               // if we have points to fit to then use those
               if (m_plZGrid && m_plZGrid->Num()) {
                  // find closest points
                  DWORD dwClosest, dwCur;
                  fp fClosest, fClosestDist, fCur, fDist;
                  dwClosest = (DWORD)-1;
                  for (dwCur = 0; dwCur < m_plZGrid->Num(); dwCur++) {
                     fCur = *((fp*) m_plZGrid->Get(dwCur));
                     fDist = fabs(fCur - p.p[2]);
                     if ((dwClosest == -1) || (fDist < fClosestDist)) {
                        dwClosest = dwCur;
                        fClosestDist = fDist;
                        fClosest = fCur;
                     }
                  }
                  if (dwClosest != -1)
                     p.p[2] = fClosest;
               }
               else {
                  // Apply grid
                  DWORD dwBits;
                  dwBits = -1;
                  if (dwPointerMode == IDC_MOVENSEW)
                     dwBits = 0x03; // NSEW
                  else if (dwPointerMode == IDC_MOVEUD)
                     dwBits = 0x04; // only apply grid up/down
                  GridMove (&p, dwBits);
               }

               // subtract the original location
               p.Subtract (&m_pMoveRotReferenceInWorld);

               // create a transform for this translation
               CMatrix mMove;
               mMove.Translation (p.p[0], p.p[1], p.p[2]);

               // pre-apply it to the original trans/rot matrix
               mMove.MultiplyLeft (&m_pMoveRotReferenceMatrix);

               // set this new matrix for the object
               // through callbacks the view will automagically be refreshed
               MoveReferenceMatrixSet (&mMove, &m_pMoveRotReferenceWas);
            }

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;

      case IDC_MOVEROTZ:
      case IDC_MOVEROTY:
         {
            // found out where moved to
            DWORD dwX, dwY;
            CPoint pt;
            fp fRotZ, fRotX, fRotY;
            if (!PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY))
               break;   // dont move

            // find new matrix
            CMatrix mNew;
            if (!RotationDrag (dwX, dwY, &mNew, TRUE, m_fMoveEmbedded))
               break;   // dont move

            if (m_fMoveEmbedded) {
               // multipley by
               // rotating embedded object
               DWORD dwNum, *pdw;
               pdw = m_pWorld->SelectionEnum(&dwNum);
               if (dwNum != 1)
                  break;   // shouldn't happen
               PCObjectSocket pEmbed, pContain;
               pEmbed = m_pWorld->ObjectGet (pdw[0]);
               if (!pEmbed)
                  break;   // shouldnt happen
               GUID gContain, gEmbed;
               pEmbed->GUIDGet (&gEmbed);
               if (!pEmbed->EmbedContainerGet (&gContain))
                  break;   // shouldn't happen
               pContain = m_pWorld->ObjectGet (m_pWorld->ObjectFind(&gContain));
               if (!pContain)
                  break;   // shouldn't happen

               // get the embedded object's matrix and invert
               CMatrix m, mInv;
               m.Copy(&m_mRotOrig);
               m.Invert4 (&mInv);
               mInv.MultiplyLeft (&mNew);

               // mNew just says how much rotated, so make a vector and see how much rotated
               CPoint p1, p2;
               p1.Zero();
               p1.p[3] = 1;
               p2.Copy (&p1);
               p2.p[0] = 1;
               p1.MultiplyLeft (&mInv);
               p2.MultiplyLeft (&mInv);
               p2.Subtract (&p1);
               fRotY = -atan2 (p2.p[2], p2.p[0]);

#ifdef _DEBUG
               char szTemp[64];
               sprintf (szTemp, "Angle = %g\r\n", (double) fRotY);
               OutputDebugString (szTemp);
#endif
               fp fRotation;
               fRotation = m_fMoveEmbedRotation + fRotY;
               fRotation = ApplyGrid(fmod(fRotation, (fp)(2 * PI)), m_fGridAngle);

               // write it out
               pContain->ContEmbeddedLocationSet (&gEmbed, &m_MoveEmbedHV, fRotation, m_dwMoveEmbedSurface);


            }
            else {
               // moving non-embedded object

               // BUGFIX - Applyting the grid is a bit more complicated that I'd like
               // need to readjust the center
               if (m_fGridAngle) {
                  // find out where the centter point would have been
                  CPoint pOld, pNew;
                  pOld.Copy (&m_pRotCenterOrig);
                  pOld.p[3] = 1;
                  pOld.MultiplyLeft (&mNew);

                  // Should apply grid, but dont bother now
                  // set it
                  mNew.ToXYZLLT (&pt, &fRotZ, &fRotX, &fRotY);
                  fRotZ = ApplyGrid(fmod(fRotZ, (fp)(2 * PI)), m_fGridAngle);
                  fRotX = ApplyGrid(fmod(fRotX, (fp)(2 * PI)), m_fGridAngle);
                  fRotY = ApplyGrid(fmod(fRotY, (fp)(2 * PI)), m_fGridAngle);
                  mNew.FromXYZLLT (&pt, fRotZ, fRotX, fRotY);

                  // find out where the center is now
                  pNew.Copy (&m_pRotCenterOrig);
                  pNew.p[3] = 1;
                  pNew.MultiplyLeft (&mNew);

                  // take the differences
                  pNew.Subtract (&pOld);

                  // add this to the matrix
                  CMatrix mTrans;
                  mTrans.Translation (-pNew.p[0], -pNew.p[1], -pNew.p[2]);
                  mNew.MultiplyRight (&mTrans);
               }

               MoveReferenceMatrixSet (&mNew, &m_pMoveRotReferenceWas);

#if 0 // old code
               // get current camera information
               fp fRotZ, fRotX, fRotY;
               fRotZ = m_pMoveRotReferenceLLT.p[2];
               fRotY = m_pMoveRotReferenceLLT.p[1];
               fRotX = m_pMoveRotReferenceLLT.p[0];

               // find the point and see how far back it is
               if (dwPointerMode == IDC_MOVEROTZ)
                  fRotZ += (iX - m_pntButtonDown.x) * 2 * PI / 750;
               else if (dwPointerMode == IDC_MOVEROTX)
                  fRotX += (iY - m_pntButtonDown.y) * 2 * PI / 750;
               else
                  fRotY += (iY - m_pntButtonDown.y) * 2 * PI / 750;

               fRotZ = ApplyGrid(fmod(fRotZ, 2 * PI), m_fGridAngle);
               fRotX = ApplyGrid(fmod(fRotX, 2 * PI), m_fGridAngle);
               fRotY = ApplyGrid(fmod(fRotY, 2 * PI), m_fGridAngle);

               // create this matrix
               CMatrix mRot;
               CPoint pt;
               pt.Zero();
               mRot.FromXYZLLT (&pt, fRotZ, fRotX, fRotY);

               // apply the center point to that and see where it ends up
               pt.Copy (&m_pMoveRotReferenceInObj);
               pt.MultiplyLeft (&mRot);

               // create a translation vector that will translate the center
               // point as it is now to where it was before the rotation
               // was applied
               CMatrix mTrans;
               mTrans.Translation (m_pMoveRotReferenceInWorld.p[0] - pt.p[0],
                  m_pMoveRotReferenceInWorld.p[1] - pt.p[1],
                  m_pMoveRotReferenceInWorld.p[2] - pt.p[2]);
               mTrans.MultiplyLeft (&mRot);
               // set it
               MoveReferenceMatrixSet (&mTrans, &m_pMoveRotReferenceWas);
#endif // 0 - old code
            }

            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;

      // case IDC_VIEWWALKFBROTZ: - not used here
      case IDC_VIEWWALKROTZX:
      case IDC_VIEWWALKROTY:
      case IDC_VIEWWALKLRUD:
      case IDC_VIEWWALKLRFB:
         {
            // get the info
            CPoint p, pFacing, pRight;
            fp fLong, fLat, fTilt;
            p.Copy (&m_pVW);
            fLong = m_fVWLong;
            fLat = m_fVWLat;
            fTilt = m_fVWTilt;

            // BUGFIX - Flipped direction of longitude
            pFacing.p[0] = sin(-fLong);
            pFacing.p[1] = cos (-fLong);
            pRight.p[0] = cos(-fLong); //sin(fLong+PI/2);
            pRight.p[1] = -sin(-fLong); // cos(fLong+PI/2);

            if (dwPointerMode == IDC_VIEWWALKROTZX) {
               fLong = ApplyGrid (fmod(fLong - (iX - m_pntButtonDown.x) * PI / 750, 2*PI), m_fGridAngle);
                  // BUGFIX - Flipped direction of longitude
               fLat = ApplyGrid (fmod(fLat - (iY - m_pntButtonDown.y) * PI / 750, 2*PI), m_fGridAngle);
            }
            else if (dwPointerMode == IDC_VIEWWALKROTY) {
               fTilt = ApplyGrid (fmod(fTilt + (iX - m_pntButtonDown.x) * 2 * PI / 750, 2*PI), m_fGridAngle);
            }
            else {
               CPoint ps;
               ps.Copy (&m_pScaleScrollX);
               ps.Scale (m_pntButtonDown.x - iX);  // do the opposite movement
               p.Add (&ps);
               ps.Copy (&m_pScaleScrollY);
               ps.Scale (m_pntButtonDown.y - iY);  // do the opposite movement
               p.Add (&ps);
            }
            //if (dwPointerMode == IDC_VIEWWALKLRFB) {
               // shift left/right
            //   p.p[0] -= pRight.p[0] * (iX - m_pntButtonDown.x) * m_fScaleScroll;
            //   p.p[1] -= pRight.p[1] * (iX - m_pntButtonDown.x) * m_fScaleScroll;

               // front back
            //   p.p[0] += pFacing.p[0] * (iY - m_pntButtonDown.y) * m_fScaleScroll;
            //   p.p[1] += pFacing.p[1] * (iY - m_pntButtonDown.y) * m_fScaleScroll;
            //}
            //else {   // walk LR, UD
            //   p.p[0] -= pRight.p[0] * (iX - m_pntButtonDown.x) * m_fScaleScroll;
            //   p.p[1] -= pRight.p[1] * (iX - m_pntButtonDown.x) * m_fScaleScroll;
            //   p.p[2] += (iY - m_pntButtonDown.y) * m_fScaleScroll;
            //}


            // write it out
            m_apRender[0]->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, m_fVWFOV);
            CameraPosition ();
            RenderUpdate(WORLDC_CAMERAMOVED);


            // display new settings
            UpdateToolTipByPointerMode();
         }
         break;
   }

   return 0;

}


/***********************************************************************************
CHouseView::PaintFlatGrid - This paints the grid over the bitmap drawn by the
flattened renderer. It uses the grid settings.

inputs
   DWORD       dwThumb - Thumbnail number
   HDC         hDC - HDC to draw to
returns
   void
*/
void CHouseView::PaintFlatGrid (DWORD dwThumb, HDC hDC)
{
   PCRenderTraditional pRender = m_apRender[dwThumb];
   if (pRender->CameraModelGet () != CAMERAMODEL_FLAT)
      return;  // dont draw this
   if (!pRender->m_fGridMajorSize && !pRender->m_fGridMinorSize)
      return;  // no grid to use

   // find the extends
   int iWidth, iHeight;
   iWidth = (int) m_aImage[dwThumb].Width();
   iHeight = (int) m_aImage[dwThumb].Height();
   if (!iWidth || !iHeight)
      return;

   // corner locs min and max
   DWORD i;
   CPoint pLoc, pMin, pMax;
   for (i = 0; i < 4; i++) {
      // location in world space
      pRender->PixelToWorldSpace ((i % 2) ? (fp)iWidth : 0, (i / 2) ? (fp)iHeight : 0, 0, &pLoc);

      // convert to grid space
      pLoc.p[3] = 1;
      pLoc.MultiplyLeft (&pRender->m_mGridFromWorld);

      // min and max this
      if (i) {
         pMin.Min (&pLoc);
         pMax.Max (&pLoc);
      }
      else {
         pMin.Copy (&pLoc);
         pMax.Copy (&pLoc);
      }
   }

   // subtract min and max to find out which changes the least and most
   pLoc.Subtract (&pMax, &pMin);
   DWORD dwNo, dwMax;
   dwNo = 0;
   dwMax = 0;
   for (i = 1; i < 3; i++) {
      if (pLoc.p[i] < pLoc.p[dwNo])
         dwNo = i;
      if (pLoc.p[i] > pLoc.p[dwMax])
         dwMax = i;
   }
   if (pLoc.p[dwMax] / 1000.0 < pLoc.p[dwNo])
      return;  // not looking straight on a dimension

   // which gridlines are shown?
   BOOL fShowMinor, fShowMajor;
   int iWH;
   iWH = min(iWidth, iHeight) / 10;
   fShowMinor = pRender->m_fGridMinorSize ? TRUE : FALSE;
   fShowMajor = pRender->m_fGridMajorSize ? TRUE : FALSE;
   if (fShowMinor && (pLoc.p[dwMax] / pRender->m_fGridMinorSize >= iWH))
      fShowMinor = FALSE;
   if (fShowMajor && (pLoc.p[dwMax] / pRender->m_fGridMajorSize >= iWH))
      fShowMajor = FALSE;

   // invert matrix for grid from world
   CMatrix mGridToWorld;
   pRender->m_mGridFromWorld.Invert4 (&mGridToWorld);

   // loop
   fp fX, fY;
   TEXTUREPOINT tPixel[2];
   DWORD dwX, dwY, dwMajor;
   dwX = (dwNo + 1) % 3;   // dimension for fX
   dwY = (dwNo + 2) % 3;   // dimension for fY
   for (dwMajor = 0; dwMajor < 2; dwMajor++) {
      if (dwMajor && !fShowMajor)
         continue;
      if (!dwMajor && !fShowMinor)
         continue;
      fp fInc;
      fInc = (dwMajor ? pRender->m_fGridMajorSize : pRender->m_fGridMinorSize);

      // create the pen
      HPEN hPen, hPenOld, hPenThick;
      hPen = CreatePen(PS_SOLID, 0, dwMajor ? pRender->m_crGridMajor : pRender->m_crGridMinor);
      hPenThick =  CreatePen(PS_SOLID, 3, dwMajor ? pRender->m_crGridMajor : pRender->m_crGridMinor);
      hPenOld = (HPEN) SelectObject (hDC, hPen);

      // loop over two sets of grids, vertical and horizontal
      DWORD dwVert, j;
      for (dwVert = 0; dwVert < 2; dwVert++) {
         DWORD dwDim = (dwVert ? dwX : dwY);
         DWORD dwDimOther = (dwVert ? dwY : dwX);

         for (fX = ApplyGrid(pMin.p[dwDim] - fInc, fInc); fX < pMax.p[dwDim] + fInc; fX += fInc) {

            if (pRender->m_fGridDrawDots) {  // draw dots
               for (fY = ApplyGrid(pMin.p[dwDimOther] - fInc, fInc); fY < pMax.p[dwDimOther] + fInc; fY += fInc) {
                  pLoc.Zero();
                  pLoc.p[dwDim] = fX;
                  pLoc.p[dwDimOther] = fY;
                  pLoc.p[3] = 1;
                  pLoc.MultiplyLeft (&mGridToWorld);
                  fp fTempH, fTempV;
                  pRender->WorldSpaceToPixel (&pLoc, &fTempH, &fTempV);
                  tPixel[0].h = fTempH;
                  tPixel[0].v = fTempV;

                  // if not on screen don't draw
                  if ((tPixel[0].h < 2 || (tPixel[0].h+2 > iWidth) ||
                     (tPixel[0].v < 2) || (tPixel[0].v+2 > iHeight)) )
                     continue;

                  int iThick;
                  iThick = 1;
                  if ( ((fX > -fInc/2) && (fX < fInc/2)) || ((fY > -fInc/2) && (fY < fInc/2)))
                     iThick = 2;
                  if (!dwMajor)
                     iThick--;

                  if (!dwThumb && (m_dwImageScale != IMAGESCALE)) {
                     tPixel[0].h *= (fp) m_dwImageScale / IMAGESCALE;
                     tPixel[0].v *= (fp) m_dwImageScale / IMAGESCALE;
                  }

                  int iX, iY;
                  iX = (int) tPixel[0].h + m_arThumbnailLoc[dwThumb].left;
                  iY = (int) tPixel[0].v + m_arThumbnailLoc[dwThumb].top;

                  // line
                  if (iThick) {
                     MoveToEx (hDC, iX - iThick, iY - iThick, NULL);
                     LineTo (hDC, iX + iThick, iY - iThick);
                     LineTo (hDC, iX + iThick, iY + iThick);
                     LineTo (hDC, iX - iThick, iY + iThick);
                     LineTo (hDC, iX - iThick, iY - iThick);
                  }
                  else
                     SetPixel (hDC, iX, iY, dwMajor ? pRender->m_crGridMajor : pRender->m_crGridMinor);
               }  // fY
            }
            else {   // draw lines
               // first point
               pLoc.Zero();
               pLoc.p[dwDim] = fX;
               pLoc.p[dwDimOther] = pMin.p[dwDimOther];
               pLoc.p[3] = 1;
               pLoc.MultiplyLeft (&mGridToWorld);
               fp fTempH, fTempV;
               pRender->WorldSpaceToPixel (&pLoc, &fTempH, &fTempV);
               tPixel[0].h = fTempH;
               tPixel[0].v = fTempV;

               // end point
               pLoc.Zero();
               pLoc.p[dwDim] = fX;
               pLoc.p[dwDimOther] = pMax.p[dwDimOther];
               pLoc.p[3] = 1;
               pLoc.MultiplyLeft (&mGridToWorld);
               pRender->WorldSpaceToPixel (&pLoc, &fTempH, &fTempV);
               tPixel[1].h = fTempH;
               tPixel[1].v = fTempV;

               // intersect
               TEXTUREPOINT aNew[2];
               for (j = 0; j < 2; j++) {
                  tPixel[j].h /= (fp) iWidth;
                  tPixel[j].v /= (fp) iHeight;
               }
               if (!Intersect2DLineWithBox (&tPixel[0], &tPixel[1], &aNew[0], &aNew[1]))
                  continue;
               tPixel[0] = aNew[0];
               tPixel[1] = aNew[1];
               for (j = 0; j < 2; j++) {
                  tPixel[j].h *= (fp) iWidth;
                  tPixel[j].v *= (fp) iHeight;
               }

               // if it's thumbnail 0 then need to scale
               if (!dwThumb && (m_dwImageScale != IMAGESCALE)) {
                  tPixel[0].h *= (fp) m_dwImageScale / IMAGESCALE;
                  tPixel[1].h *= (fp) m_dwImageScale / IMAGESCALE;
                  tPixel[0].v *= (fp) m_dwImageScale / IMAGESCALE;
                  tPixel[1].v *= (fp) m_dwImageScale / IMAGESCALE;
               }

               // select the pen
               if ((fX > -fInc/2) && (fX < fInc/2))
                  SelectObject (hDC, hPenThick);

               // line
               MoveToEx (hDC, (int) tPixel[0].h + m_arThumbnailLoc[dwThumb].left, (int) tPixel[0].v + m_arThumbnailLoc[dwThumb].top, NULL);
               LineTo (hDC, (int) tPixel[1].h + m_arThumbnailLoc[dwThumb].left, (int) tPixel[1].v + m_arThumbnailLoc[dwThumb].top);

               // unselect thick pen
               if ((fX > -fInc/2) && (fX < fInc/2))
                  SelectObject (hDC, hPen);
            }  // draw lines
         }  // over fX
      }  // dwVert

      // delete the pen
      SelectObject (hDC, hPenOld);
      DeleteObject (hPen);
      DeleteObject (hPenThick);
   } // over dwMajor and dwMinor
}


/***********************************************************************************
CHouseView::Paint - WM_PAINT messge
*/
LRESULT CHouseView::Paint (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   PAINTSTRUCT ps;
   HDC   hDC;
   hDC = BeginPaint (hWnd, &ps);

   // get the object
   DWORD dwPolyObject;
   PCObjectPolyMesh pm;
   PCObjectBone pBone = NULL;
   dwPolyObject = -1;
   pm = NULL;
   if ((m_dwViewWhat == VIEWWHAT_POLYMESH)) {
      pm = PolyMeshObject2 (&dwPolyObject);
      if (!pm)
         goto done;
   }
   else if ((m_dwViewWhat == VIEWWHAT_BONE)) {
      pBone = BoneObject2 (&dwPolyObject);
      if (!pBone)
         goto done;
   }

   // size
   RECT  r;
   DWORD i;
   GetClientRect (hWnd, &r);

   // make sure the current thumbnail number is always valid
   m_dwThumbnailCur = max(1,m_dwThumbnailCur);
   m_dwThumbnailCur = min(VIEWTHUMB-1,m_dwThumbnailCur);

   // render the image if necessary
   if (m_fWorldChanged && !m_fTimerWaitToDraw) {
#ifdef _DEBUG
      {
      char szTemp[256];
      sprintf (szTemp, "Paint: Render %lx\r\n", (__int64) m_hWnd);
      OutputDebugString (szTemp);
      }
#endif
      // if there's a timer running then kill it
//      if (m_fTimerWaitToDraw) {
//         // kill the timer
//         KillTimer (m_hWnd, WAITTODRAW);
//         m_fTimerWaitToDraw = NULL;
//      }

#ifndef _TIMERS
// #define _TIMERS      // Do this to test render time in release
#endif

   #ifdef _TIMERS
      DWORD dwStart = GetTickCount();
   #endif

      // reset the light vector
      for (i = 0; i < VIEWTHUMB; i++) {
         if (!m_afThumbnailVis[i])
            continue;   // dont bother setting
         m_apRender[i]->LightVectorSetFromSun ();

         // transfer over exposure
         m_apRender[i]->ExposureSet (m_apRender[0]->ExposureGet());

         // update the grid settings
         if ((m_fGridDisplay | m_fGridDisplay2D) && (m_fGrid > EPSILON)) {
            m_apRender[i]->m_fGridDraw = m_fGridDisplay;
            m_apRender[i]->m_fGridMinorSize = m_fGrid;
            m_apRender[i]->m_mGridFromWorld.Copy (&m_mWorldToGrid);
            m_apRender[i]->m_fGridDrawDots = m_apRender[0]->m_fGridDrawDots;   // so consistent

            if (MeasureDefaultUnits() & MUNIT_ENGLISH) {
               if (m_fGrid <= 1.0 / INCHESPERMETER * 6.0 + EPSILON)
                  m_apRender[i]->m_fGridMajorSize = 1.0 / FEETPERMETER;
               else if (m_fGrid < 1.0 / FEETPERMETER * 1.5 + EPSILON)
                  m_apRender[i]->m_fGridMajorSize = 3.0 / FEETPERMETER;
               else if (m_fGrid < 1.0 / FEETPERMETER * 5.0 + EPSILON)
                  m_apRender[i]->m_fGridMajorSize = 10.0 / FEETPERMETER;
               else
                  m_apRender[i]->m_fGridMajorSize = 0;
            }
            else {
               if (m_fGrid <= .04 + EPSILON)
                  m_apRender[i]->m_fGridMajorSize = .10;
               else if (m_fGrid <= .5 + EPSILON)
                  m_apRender[i]->m_fGridMajorSize = 1;
               else if (m_fGrid <= 5 + EPSILON)
                  m_apRender[i]->m_fGridMajorSize = 10;
               else if (m_fGrid <= 50 + EPSILON)
                  m_apRender[i]->m_fGridMajorSize = 100;
               else if (m_fGrid <= 500 + EPSILON)
                  m_apRender[i]->m_fGridMajorSize = 1000;
               else
                  m_apRender[i]->m_fGridMajorSize = 0;
            }
         }
         else
            m_apRender[i]->m_fGridDraw = FALSE;
      }

      // use the same plane settings for all of them
      if (m_dwViewWhat == VIEWWHAT_OBJECT) {
         PWSTR psz;
         DWORD dwLoc, dwShow, dwPlane;

         // location
         psz = m_pWorld->VariableGet (WSObjLocation());
         dwLoc = psz ? (DWORD)_wtoi(psz) : 0;
         if (dwLoc == 1)
            dwPlane = 1;
         else  // for floor and ceiling use z = 0 plane
            dwPlane = 2;

         // show plane
         psz = m_pWorld->VariableGet (WSObjShowPlain());
         dwShow = psz ? (DWORD)_wtoi(psz) : 1;
         if (!dwShow)
            dwPlane = (DWORD)-1;

         // set them all
         for (i = 0; i < VIEWTHUMB; i++)
            m_apRender[i]->m_dwDrawPlane = dwPlane;
      }

      // always clone from the main image to the current thumbnail
      // so that it's always up to date
      // NOTE: Disabling this and just drawing blank in the currently selected thumbnail
      //if (m_afThumbnailVis[m_dwThumbnailCur])
      //   m_apRender[0]->CloneTo (m_apRender[m_dwThumbnailCur]);

      // progress bar
      CProgress Progress;
      Progress.Start (m_hWnd, "Drawing...");

      // figure out how many visible
      DWORD dwNumVis;
      dwNumVis = 0;
      for (i = 0; i < VIEWTHUMB; i++)
         if (m_afThumbnailVis[i] && (i != m_dwThumbnailCur))
            dwNumVis += (i == 0) ? 4 : 1;

      DWORD dwCur, dwSize;
      dwCur = 0;
      dwSize = 0;
      for (i = 0; i < VIEWTHUMB; i++, dwCur += dwSize) {
         dwSize = 0;
         if (!m_afThumbnailVis[i])
            continue;
         if (i == m_dwThumbnailCur)
            continue;   // dont draw currently seleted thumbnail

         dwSize = (i == 0) ? 4 : 1;

         // set flags so will only show the main edited object
         if (m_dwViewWhat == VIEWWHAT_POLYMESH) {
            m_apRender[i]->m_fIgnoreSelection = TRUE;   // never draw selection here
            m_apRender[i]->m_dwShowOnly = m_fShowOnlyPolyMesh ? dwPolyObject : -1;
            switch (m_dwViewSub) {
            default:
               m_apRender[i]->m_dwSpecial = ORSPECIAL_POLYMESH_EDITPOLY;
               break;
            case VSPOLYMODE_MORPH:  // morph
               m_apRender[i]->m_dwSpecial = ORSPECIAL_POLYMESH_EDITMORPH;
               break;
            case VSPOLYMODE_BONE:  // bone
               m_apRender[i]->m_dwSpecial = ORSPECIAL_POLYMESH_EDITBONE;
               break;
            }
         }
         else if (m_dwViewWhat == VIEWWHAT_BONE) {
            m_apRender[i]->m_fBlendSelAbove = TRUE;   // never draw selection here
            m_apRender[i]->m_dwShowOnly = m_fShowOnlyPolyMesh ? dwPolyObject : -1;
            m_apRender[i]->m_dwSelectOnly = dwPolyObject;
         }

         Progress.Push ((fp)dwCur / (fp) dwNumVis, (fp)(dwCur+dwSize) / (fp)dwNumVis);  // since last 10% is some downsampling
         // re-render
         // don't show the camera in the small views
         DWORD dwTemp;
         dwTemp = m_apRender[i]->RenderShowGet();
         if (i)
            m_apRender[i]->RenderShowSet (m_apRender[i]->RenderShowGet() & ~(RENDERSHOW_VIEWCAMERA));
         m_apRender[i]->Render(m_fWorldChanged, NULL, &Progress);
         m_apRender[i]->RenderShowSet (dwTemp);

         // draw selection
         if (m_dwViewWhat == VIEWWHAT_POLYMESH)
            PolyMeshSelRender (pm, dwPolyObject, m_apRender[i], &m_aImage[i]);

         // merge in the tracing paper
         TraceApply (&m_lTraceInfo, m_apRender[i], &m_aImage[i]);

         Progress.Pop();
      }
      m_fWorldChanged = 0;
   #ifdef _TIMERS
      char szTemp[128];
      sprintf (szTemp, "Render time = %d\r\n", (int) (GetTickCount() - dwStart));
      OutputDebugString (szTemp);
   #endif

   }  // re-render

   // paint the cool border
   RECT  rt;//, inter;

   HBRUSH hbr;
   if (!m_fSmallWindow || (m_fSmallWindow && (!m_fHideButtons || m_hWndPolyMeshList))) {
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

      // between list
      if (m_hWndPolyMeshList) {
         rt = m_rPolyMeshList;
         rt.right = rt.left;
         rt.left -= VARSCROLLSIZE;
         FillRect (hDC, &rt, hbr);
      }

      // top
      rt = r;
      rt.left += VARBUTTONSIZE;
      rt.right -= VARBUTTONSIZE;
      rt.bottom = rt.top + VARSCREENSIZE;
      rt.top += VARBUTTONSIZE;
      if (m_dwThumbnailShow == 1)
         rt.bottom -= VARSCROLLSIZE;
      FillRect (hDC, &rt, hbr);

      // right
      rt = r;
      rt.left = rt.right - VARSCREENSIZE;
      rt.right -= VARBUTTONSIZE;
      if ((m_dwThumbnailShow == 1) || (m_dwThumbnailShow == 2))
         rt.left += VARSCROLLSIZE;
      rt.top += VARBUTTONSIZE;
      rt.bottom -= VARBUTTONSIZE;
      FillRect (hDC, &rt, hbr);

      // bottom
      rt = r;
      rt.left += VARBUTTONSIZE;
      rt.right -= VARBUTTONSIZE;
      rt.top = rt.bottom - VARSCREENSIZE;
      rt.bottom -= VARBUTTONSIZE;
      if ((m_dwThumbnailShow == 1) || (m_dwThumbnailShow == 2))
         rt.top += VARSCROLLSIZE;
      FillRect (hDC, &rt, hbr);

      // left
      rt = r;
      rt.right = rt.left + VARSCREENSIZE;
      if (m_dwThumbnailShow == 2)
         rt.right -= VARSCROLLSIZE;
      rt.left += VARBUTTONSIZE;
      rt.top += VARBUTTONSIZE;
      rt.bottom -= VARBUTTONSIZE;
      FillRect (hDC, &rt, hbr);

      // between the main image and the thumbnails
      switch (m_dwThumbnailShow) {
      case 1:  // thumbnails on the right
         rt = r;
         rt.top += VARBUTTONSIZE;
         rt.bottom -= VARBUTTONSIZE;
         rt.left = m_arThumbnailLoc[0].right;
         rt.right = rt.left + 2 * VARSCROLLSIZE;
         FillRect (hDC, &rt, hbr);

         // BUGFIX - Fill betweent thumbnails
         for (i = 1; i+1 < VIEWTHUMB; i++) {
            rt.top = m_arThumbnailLoc[i].bottom+1;
            rt.bottom = m_arThumbnailLoc[i+1].top-1;
            rt.left = m_arThumbnailLoc[i].left-1;
            rt.right = m_arThumbnailLoc[i].right+1;
            FillRect (hDC, &rt, hbr);
         }

         break;
      case 2:  // thumbnails on the bottom
         rt = r;
         rt.left += VARBUTTONSIZE;
         rt.right -= VARBUTTONSIZE;
         rt.top = m_arThumbnailLoc[0].bottom;
         rt.bottom = rt.top + 2 * VARSCROLLSIZE;
         FillRect (hDC, &rt, hbr);

         // BUGFIX - Fill betweent thumbnails
         for (i = 1; i+1 < VIEWTHUMB; i++) {
            rt.left = m_arThumbnailLoc[i].right+1;
            rt.right = m_arThumbnailLoc[i+1].left-1;
            rt.top = m_arThumbnailLoc[i].top-1;
            rt.bottom = m_arThumbnailLoc[i].bottom+1;
            FillRect (hDC, &rt, hbr);
         }

         break;
      }

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
   }

   // Line around thumbnails
   for (i = 1; i < VIEWTHUMB; i++) {
      if (!m_afThumbnailVis[i])
         continue;
      RECT r;
      r = m_arThumbnailLoc[i];
      r.left -= 1;
      //r.right += 1;
      r.top -= 1;
      //r.bottom += 1;

      HPEN hPenOld, hPen;
      hPen = CreatePen (PS_SOLID, 0, RGB(0,0,0));
      hPenOld = (HPEN) SelectObject (hDC, hPen);
      MoveToEx (hDC, r.left, r.top, NULL);
      LineTo (hDC, r.right, r.top);
      LineTo (hDC, r.right, r.bottom);
      LineTo (hDC, r.left, r.bottom);
      LineTo (hDC, r.left, r.top);
      SelectObject (hDC, hPenOld);
      DeleteObject (hPen);
   }


   // font for the images
   HFONT hFont;
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -MulDiv(9, GetDeviceCaps(hDC, LOGPIXELSY), 72); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   hFont = CreateFontIndirect (&lf);
   HFONT hFontOld;
   hFontOld = (HFONT) SelectObject (hDC, hFont);
   SetBkMode (hDC, TRANSPARENT);

   // paint the images
   for (i = 0; i < VIEWTHUMB; i++) {
      RECT rImage, rD;
      if (!m_afThumbnailVis[i])
         continue;

      if (i == m_dwThumbnailCur) {
         // special case - this is a thumbnail of the larger image, so to save CPU,
         // just put some text here
         int iX, iY;
         rImage = m_arThumbnailLoc[i];

         HBRUSH hBrush;
         hBrush = CreateSolidBrush (RGB(0x40, 0x40, 0x80));
         FillRect (hDC, &rImage, hBrush);
         DeleteObject (hBrush);

         // text
         iX = (rImage.right - rImage.left) / 16;
         iY = (rImage.bottom - rImage.top) / 16;
         rImage.left += iX;
         rImage.right -= iX;
         rImage.top += iY;
         rImage.bottom -= iY;
         PSTR pszText = "This thumbnail is currently being shown in the main view. "
            "Click on a different thumbnail to display that one. "
            "Or, control-click to copy the main view settings to the other thumbnail.";

         // draw the text
         SetTextColor (hDC, RGB(0xff,0xff,0xff));

         // calculate the size
         RECT r;
         r = rImage;
         DrawText (hDC, pszText, -1, &r, DT_CALCRECT | DT_CENTER | DT_WORDBREAK);

         iY = ((rImage.bottom - rImage.top) - (r.bottom - r.top)) / 2;
         iY = max(0,iY);
         r = rImage;
         r.top += iY;
         r.bottom += iY;
         DrawText (hDC, pszText, -1, &r, DT_CENTER | DT_WORDBREAK);
         continue;
      }

      POINT pc;
      pc.x = m_arThumbnailLoc[i].left;
      pc.y = m_arThumbnailLoc[i].top;

      rImage.left = pc.x;
      rImage.top = pc.y;
      if (i) {
         rImage.right = rImage.left + (int) m_aImage[i].Width();
         rImage.bottom = rImage.top + (int) m_aImage[i].Height();
      }
      else {
         rImage.right = rImage.left + (int) m_aImage[i].Width() * m_dwImageScale / IMAGESCALE;
         rImage.bottom = rImage.top + (int) m_aImage[i].Height() * m_dwImageScale / IMAGESCALE;
      }
      if (IntersectRect (&rD, &rImage, &ps.rcPaint)) {
         //if (m_dwImageScale == IMAGESCALE)
         //   m_aImage[i].Paint (hDC, NULL, pc);
         //else

         // if we have a polygonmesh that is being edited for textures, and they're
         // displayed then need to paint that display over the top
         if ((i==0) && m_fPolyMeshTextDisp && (m_dwViewSub == VSPOLYMODE_TEXTURE) && (m_dwViewWhat == VIEWWHAT_POLYMESH)) {
            HDC hDCScratch = CreateCompatibleDC (hDC);
            RECT rTo;
            HBITMAP hBitScratch = CreateCompatibleBitmap (hDC,
               rTo.right = rImage.right - rImage.left,
               rTo.bottom = rImage.bottom - rImage.top);
            SelectObject (hDCScratch, hBitScratch);

            rTo.top = rTo.left = 0;
            m_aImage[i].Paint (hDCScratch, NULL, &rTo);

            PolyMeshPaintTexture (hDCScratch);

            BitBlt (hDC, rImage.left, rImage.top, rTo.right, rTo.bottom,
               hDCScratch, 0, 0, SRCCOPY);
            DeleteDC (hDCScratch);
            DeleteObject (hBitScratch);
         }
         else
            m_aImage[i].Paint (hDC, NULL, &rImage);

         // draw the grid
         if (m_fGridDisplay2D)
            PaintFlatGrid (i, hDC);

         // figure out what text to diplay
         char szText[256];
         szText[0] = 0;
         PCRenderTraditional pRender;
         pRender = m_apRender[i];
         DWORD dwMode;
         dwMode = pRender->CameraModelGet ();

         fp fFOV, fScale;
         CPoint pCenterOfRot, pRot, pCamera;
         pCenterOfRot.Zero();
         pRot.Zero();
         pCamera.Zero();
         fScale = 0;
         switch (dwMode) {
         default:
         case CAMERAMODEL_FLAT:
            pRender->CameraFlatGet (&pCenterOfRot, &pRot.p[2], &pRot.p[0], &pRot.p[1],
               &fScale, &pCamera.p[0], &pCamera.p[1]);
            break;
         case CAMERAMODEL_PERSPWALKTHROUGH:
            pRender->CameraPerspWalkthroughGet (&pCamera, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);
            break;
         case CAMERAMODEL_PERSPOBJECT:
            pRender->CameraPerspObjectGet (&pCamera, &pCenterOfRot, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);
            break;
         }

         // round the angle to the nearest degree
         DWORD j;
         for (j = 0; j < 3; j++) {
            pRot.p[j] = ApplyGrid(myfmod(pRot.p[j] / 2 / PI * 360.0, 360), 1);
            if (pRot.p[j] == 360)
               pRot.p[j] = 0;
         }

         // text to display
         switch (dwMode) {
         default:
         case CAMERAMODEL_FLAT:
         case CAMERAMODEL_PERSPOBJECT:
            if ((pRot.p[1] == 0) && (pRot.p[0] == 0)) {
               if (pRot.p[2] == 0)
                  strcpy (szText, NSEWNAME ("From south","From front"));
               else if (pRot.p[2] == 45)
                  strcpy (szText, NSEWNAME ("From south-west","From front-left"));
               else if (pRot.p[2] == 90)
                  strcpy (szText, NSEWNAME ("From west","From left"));
               else if (pRot.p[2] == 135)
                  strcpy (szText, NSEWNAME ("From north-west","From back-left"));
               else if (pRot.p[2] == 180)
                  strcpy (szText, NSEWNAME ("From north","From back"));
               else if (pRot.p[2] == 225)
                  strcpy (szText, NSEWNAME ("From north-east","From back-right"));
               else if (pRot.p[2] == 270)
                  strcpy (szText, NSEWNAME ("From east","From right"));
               else if (pRot.p[2] == 315)
                  strcpy (szText, NSEWNAME ("From south-east","From front-right"));
            }
            else if ((pRot.p[0] == 90) && (pRot.p[1] == 0) && (pRot.p[2] == 0))
               strcpy (szText, "From above");
            else if ((pRot.p[0] == 270) && (pRot.p[1] == 0) && (pRot.p[2] == 0))
               strcpy (szText, "From below");
            break;
         case CAMERAMODEL_PERSPWALKTHROUGH:
            if ((pRot.p[1] == 0) && (pRot.p[0] == 0)) {
               if (pRot.p[2] == 0)
                  strcpy (szText, NSEWNAME ("Looking north", "Looking towards back"));
               else if (pRot.p[2] == 315)
                  strcpy (szText, NSEWNAME ("Looking north-east", "Looking towards back-right"));
               else if (pRot.p[2] == 270)
                  strcpy (szText, NSEWNAME ("Looking east", "Looking right"));
               else if (pRot.p[2] == 225)
                  strcpy (szText, NSEWNAME ("Looking south-east", "Looking towards front-right"));
               else if (pRot.p[2] == 180)
                  strcpy (szText, NSEWNAME ("Looking south", "Looking towards front"));
               else if (pRot.p[2] == 135)
                  strcpy (szText, NSEWNAME ("Looking south-west", "Looking towards front-left"));
               else if (pRot.p[2] == 90)
                  strcpy (szText, NSEWNAME ("Looking west", "Looking left"));
               else if (pRot.p[2] == 45)
                  strcpy (szText, NSEWNAME ("Looking north-west", "Looking towards back-left"));
            }
            else if ((pRot.p[0] == 270) && (pRot.p[1] == 0) && (pRot.p[2] == 0))
               strcpy (szText, "Looking down");
            else if ((pRot.p[0] == 90) && (pRot.p[1] == 0) && (pRot.p[2] == 0))
               strcpy (szText, "Looking up");
            break;
         }
         if (szText[0]) {
            // draw the text
            PIMAGEPIXEL pip;
            pip = m_aImage[i].Pixel(0,0);
            if (pip && (pip->wRed + pip->wGreen + pip->wBlue > 0x4000 * 3))
               SetTextColor (hDC, RGB(0,0,0));
            else
               SetTextColor (hDC, RGB(0xff,0xff,0xff));

            // calculate the size
            RECT r;
            r = m_arThumbnailLoc[i];
            r.left += 4;
            r.top += 4;
            // draw the text
            DrawText(hDC, szText, -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE);
         }

         // draw the title for the big one
         if (!i) {
            PWSTR pszName;
            pszName = m_pWorld->NameGet();
            char szTemp[256];
            // BUGFIX - If polymesh show the window title instead
            if (pszName[0] && (m_dwViewWhat != VIEWWHAT_POLYMESH) && (m_dwViewWhat != VIEWWHAT_BONE))
               WideCharToMultiByte (CP_ACP, 0, pszName, -1, szTemp, sizeof(szTemp), 0,0);
            else
               GetWindowText (m_hWnd, szTemp, sizeof(szTemp));
            PIMAGEPIXEL pip;
            pip = m_aImage[i].Pixel(m_aImage[i].Width()-1,m_aImage[i].Height()-1);
            if (pip && (pip->wRed + pip->wGreen + pip->wBlue > 0x4000 * 3))
               SetTextColor (hDC, RGB(0,0,0));
            else
               SetTextColor (hDC, RGB(0xff,0xff,0xff));

            // calculate the size
            RECT r;
            r = m_arThumbnailLoc[i];
            r.right -= 4;
            r.bottom -= 4;
            // draw the text
            DrawText(hDC, szTemp, -1, &r, DT_RIGHT | DT_BOTTOM); // BUGFIX - Take out DT_SINGLELINE

         }

         // Highlight current thumbnail
         //if (i == m_dwThumbnailCur) {
         //   HPEN hPen, hPenOld;
         //   HBRUSH hBrush;
         //   hPen = CreatePen (PS_SOLID, 2, RGB(255, 0, 0));
         //   hPenOld = (HPEN) SelectObject (hDC, hPen);
         //   hBrush = (HBRUSH) SelectObject (hDC, GetStockObject(NULL_BRUSH));
         //   RECT r;
         //   r = m_arThumbnailLoc[i];
         //   RoundRect (hDC,
         //      r.left+1, r.top+1 ,
         //      r.right, r.bottom,
         //      8, 8);
         //   SelectObject (hDC, hPenOld);
         //   SelectObject (hDC, hBrush);
         //   DeleteObject (hPen);
         //}

      }
   }
   SelectObject (hDC, hFontOld);
   DeleteObject (hFont);

   if (!m_fSmallWindow || (m_fSmallWindow && !m_fHideButtons)) {
      // anything from here on out should apply the clipping region
      rt = m_arThumbnailLoc[0];
      IntersectClipRect (hDC, rt.left, rt.top, rt.right, rt.bottom);
   }

   // depending on the pointer mode, may want to paint some stuff
   if (pm && m_lPolyMeshVert.Num()) {
      CListFixed lPoints;
      BOOL fRed;
      fRed = PolyMeshVertToPixel (pm, m_pntMouseLast, &lPoints);
      if (lPoints.Num()) {
         POINT *pp = (POINT*) lPoints.Get(0);
         DWORD i;

         HPEN hOld, hRed, hGrey;
         hRed = CreatePen (PS_SOLID, 3, RGB(0xff,0,0));
         hGrey = CreatePen (PS_SOLID, 3, RGB(0x40, 0x40, 0x40));
         hOld = (HPEN) SelectObject (hDC, hRed);

         for (i = 0; i < lPoints.Num(); i++, pp++) {
            if (!fRed && (i+1 == lPoints.Num()))
               SelectObject (hDC, hGrey);

            if (i)
               LineTo (hDC, pp->x, pp->y);
            else
               MoveToEx (hDC, pp->x, pp->y, NULL);

         } // i
         SelectObject (hDC, hOld);
         DeleteObject (hRed);
         DeleteObject (hGrey);
      }
   }
   else if (pm && m_dwButtonDown && (m_adwPointerMode[m_dwButtonDown-1] == IDC_PMSIDESPLIT)) {
      BOOL fRed;
      fRed = PolyMeshSideSplitRect (pm, &r);
      RectImageToScreen (&r);

      HPEN hOld, hRed;
      hRed = CreatePen (PS_SOLID, 3, fRed ? RGB(0xff,0,0) : RGB(0x40, 0x40, 0x40));
      hOld = (HPEN) SelectObject (hDC, hRed);

      MoveToEx (hDC, r.left, r.top, NULL);
      LineTo (hDC, r.right, r.bottom);
      SelectObject (hDC, hOld);
      DeleteObject (hRed);
   }
   else if (pBone && m_dwButtonDown && (m_adwPointerMode[m_dwButtonDown-1] == IDC_BONENEW)) {
      BoneAddRect (pBone, m_dwObjControlObject, &r);
      RectImageToScreen (&r);

      HPEN hOld, hRed;
      hRed = CreatePen (PS_SOLID, 3, RGB(0xff,0,0));
      hOld = (HPEN) SelectObject (hDC, hRed);

      MoveToEx (hDC, r.left, r.top, NULL);
      LineTo (hDC, r.right, r.bottom);
      SelectObject (hDC, hOld);
      DeleteObject (hRed);
   }
   else if (m_dwButtonDown && (m_adwPointerMode[m_dwButtonDown-1] == IDC_VIEWFLATLLOOKAT)) {
      HPEN hOld, hRed;
      hRed = CreatePen (PS_SOLID, 3, RGB(0xff,0,0));
      hOld = (HPEN) SelectObject (hDC, hRed);

      // draw two circles
      DWORD i, j;
      for (i = 0; i < 2; i++) {
         for (j = 0; j <= 16; j++) {
            int iX, iY;
            iX = (int) (sin(j / 16.0 * 2 * PI) * (i ? (M3DBUTTONSIZE/2) : (M3DBUTTONSIZE/4))) +
               m_pntButtonDown.x;
            iY = (int) (cos(j / 16.0 * 2 * PI) * (i ? (M3DBUTTONSIZE/2) : (M3DBUTTONSIZE/4))) +
               m_pntButtonDown.y;

            if (j)
               LineTo (hDC, iX, iY);
            else
               MoveToEx (hDC, iX, iY, NULL);
         }
      }

      // draw the pointer line
      MoveToEx (hDC, m_pntButtonDown.x, m_pntButtonDown.y, NULL);
      LineTo (hDC, m_pntMouseLast.x, m_pntMouseLast.y);

      SelectObject (hDC, hOld);
      DeleteObject (hRed);
   }
   else if (m_dwButtonDown && (m_adwPointerMode[m_dwButtonDown-1] == IDC_OBJECTPASTEDRAG)) {
      // draw a line figure out rise/run, just use a really large number
      // to make sure it's clipped
      HPEN hOld, hRed;
      hRed = CreatePen (PS_SOLID, 3, RGB(0xff,0,0));
      hOld = (HPEN) SelectObject (hDC, hRed);

      MoveToEx (hDC, m_pntButtonDown.x, m_pntButtonDown.y, NULL);
      LineTo (hDC, m_pntMouseLast.x, m_pntMouseLast.y);
      SelectObject (hDC, hOld);
      DeleteObject (hRed);
   }
   else if (m_dwButtonDown && (m_adwPointerMode[m_dwButtonDown-1] == IDC_CLIPLINE) && m_fClipAngleValid) {
      // draw a line figure out rise/run, just use a really large number
      // to make sure it's clipped
      HPEN hOld, hRed;
      hRed = CreatePen (PS_SOLID, 3, RGB(0xff,0,0));
      hOld = (HPEN) SelectObject (hDC, hRed);

      int iX, iY;
      iX = (int) (sin(m_fClipTempAngle) * 2000);
      iY = (int) (cos(m_fClipTempAngle) * 2000);

      MoveToEx (hDC, m_pntButtonDown.x - iX, m_pntButtonDown.y - iY, NULL);
      LineTo (hDC, m_pntButtonDown.x + iX, m_pntButtonDown.y + iY);
      SelectObject (hDC, hOld);
      DeleteObject (hRed);
   }
   else if (m_dwButtonDown && ((m_adwPointerMode[m_dwButtonDown-1] == IDC_SELREGION) || (m_adwPointerMode[m_dwButtonDown-1] == IDC_ZOOMINDRAG))) {
      // draw a box around the area
      HPEN hOld, hRed;
      hRed = CreatePen (PS_SOLID, 2, RGB(0xff,0,0));
      hOld = (HPEN) SelectObject (hDC, hRed);

      MoveToEx (hDC, m_pntButtonDown.x, m_pntButtonDown.y, NULL);
      LineTo (hDC, m_pntMouseLast.x, m_pntButtonDown.y);
      LineTo (hDC, m_pntMouseLast.x, m_pntMouseLast.y);
      LineTo (hDC, m_pntButtonDown.x, m_pntMouseLast.y);
      LineTo (hDC, m_pntButtonDown.x, m_pntButtonDown.y);
      SelectObject (hDC, hOld);
      DeleteObject (hRed);

   }

#if 0 // DEADCODE
   // draw the direction that looking
   fp   fAngle;
   switch (m_apRender[0]->CameraModelGet()) {
   case CAMERAMODEL_PERSPWALKTHROUGH:
      {
         CPoint p;
         fp fLat, fTilt, f2;
         m_apRender[0]->CameraPerspWalkthroughGet (&p, &fAngle, &fLat, &fTilt, &f2);
      }
      break;
   case CAMERAMODEL_PERSPOBJECT:
      {
         CPoint p, pCenter;
         fp fX, fY, f2;
         m_apRender[0]->CameraPerspObjectGet (&p, &pCenter, &fAngle, &fX, &fY, &f2);
         fAngle *= -1;
      }
      break;
   case CAMERAMODEL_FLAT:
      {
         fp fRotX, fRotY, fScale, fTransX, fTransY;
         CPoint pCenter;
         m_apRender[0]->CameraFlatGet (&pCenter, &fAngle, &fRotX, &fRotY, &fScale, &fTransX, &fTransY);
         fAngle *= -1;
      }
      break;
   }
#define  COMPASSTOTAL   (m_fSmallWindow ? 48 : 96)       // total size
#define  COMPASSLINES   (COMPASSTOTAL / 3 * 2)       // length of compass lines
   HPEN hOld, hBlack, hRed;
   hRed = CreatePen (PS_SOLID, 1, RGB(0xff,0,0));
   hBlack = CreatePen (PS_SOLID, 1, RGB(0x40,0x40,0x40));
   hOld = (HPEN) SelectObject (hDC, hBlack);
   for (i = 0; i <= 16; i++) {
      SelectObject (hDC, ((i <= 1) || (i == 16)) ? hRed : hBlack);
      // location
      fp   x, y;
      x = (fp)sin(fAngle + (double)i / 16.0 * PI * 2.0) * COMPASSLINES / 2;
      y = -(fp)cos(fAngle + (double)i / 16.0 * PI * 2.0) * COMPASSLINES / 2;
      if ((i == 4) || (i == 12)) {   // east and west
         x /= 2.0;
         y /= 2.0;
      }
      else if ((i == 0) || (i == 16) || (i == 8)) {
         // do nothing
      }
      else if (i % 2) {
         x /= 20.0;
         y /= 20.0;
      }
      else {
         x /= 3.0;
         y /= 3.0;
      }

      int ix, iy;
      ix = (int) (rt.right - COMPASSTOTAL/2 + x);
      iy = (int) (rt.top + COMPASSTOTAL/2 + y);

      if (i)
         LineTo (hDC, ix, iy);
      else
         MoveToEx (hDC, ix, iy, NULL);
   }
   SelectObject (hDC, hOld);
   DeleteObject (hRed);
   DeleteObject (hBlack);
#endif // DEADCODE

done:
   EndPaint (hWnd, &ps);

   return 0;
}


/***********************************************************************
ViewFavoriteSort */
static int _cdecl ViewFavoriteSort (const void *elem1, const void *elem2)
{
   FAVORITEVIEW *pdw1, *pdw2;
   pdw1 = (FAVORITEVIEW*) elem1;
   pdw2 = (FAVORITEVIEW*) elem2;

   return _wcsicmp(pdw1->szName, pdw2->szName);
}

/****************************************************************************
ViewFavoritePage
*/
BOOL ViewFavoritePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz || _wcsicmp(p->psz, L"ok"))
            break;   // default behaviour

         // make sure it doesn't already exit
         WCHAR szTemp[64];
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"scheme");
         szTemp[0] = 0;
         DWORD dwNeeded;
         pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
         if (!szTemp[0]) {
            pPage->MBWarning (L"Please type in a name first.");
            return TRUE;
         }

         // see if it's a duplicate
         DWORD i;
         for (i = 0; i < pv->FavoriteViewNum(); i++) {
            FAVORITEVIEW fv;
            if (!pv->FavoriteViewGet (i, &fv))
               continue;
            if (!_wcsicmp(fv.szName, szTemp)) {
               pPage->MBWarning (L"The name already exists. Please type in a new one.");
               return TRUE;
            }
         }

         FAVORITEVIEW fv;
         pv->FavoriteViewGen (szTemp, &fv);
         pv->FavoriteViewAdd (&fv);
         EscChime (ESCCHIME_INFORMATION);
         pv->m_pWorld->DirtySet (TRUE); // so will prompt to save

         // BUGFIX - Show the cursor
         while (ShowCursor (TRUE) < 1);
         while (ShowCursor (FALSE) > 0);  // don't want a really huge cursor count

         // fallthrough
      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************************
CHouseView::ArrowKey - Called when the one of the arrow keys is pressed.

inputs
   int         iX - -1 for left, 1 for right, 0 for UD
   int         iY - -1 for down, 1 for up, 0 for LR
returns
   none
*/
#define  CTLSCALE    4  // BUGFIX - Was 5, but changed to 4 so rotation in even amounts
void CHouseView::ArrowKeys (int iX, int iY)
{
   if (GetKeyState (VK_CONTROL) < 0) {
      iX *= CTLSCALE;
      iY *= CTLSCALE;
   }

   if (m_dwButtonDown)
      goto beep;

   DWORD dwCamera;
   dwCamera = m_apRender[0]->CameraModelGet();

   // find out which way is right and left
   fp fx, fy;
   fx = (fp)m_aImage[0].Width()/2 + .5;
   fy = (fp)m_aImage[0].Height()/2 + .5;
   CPoint pC, pR, pU, pAway, pTowards;
   m_apRender[0]->PixelToWorldSpace (fx, fy, 1, &pC);
   m_apRender[0]->PixelToWorldSpace (fx+1, fy, 1, &pR);
   m_apRender[0]->PixelToWorldSpace (fx, fy-1, 1, &pU);
   m_apRender[0]->PixelToWorldSpace (fx, fy, 2, &pAway);
   pR.Subtract (&pC);
   pU.Subtract (&pC);
   pAway.Subtract (&pC);
   pR.Normalize();
   pU.Normalize();
   pAway.Normalize();   // away from camera
   pTowards.Copy (&pAway);
   pTowards.Scale(-1);

   switch (m_adwPointerMode[0]) {
   case IDC_MOVENSEWUD:
   case IDC_MOVENSEW:
   case IDC_MOVEUD:
      {
         // BUGFIX - Zero out stuff, then normalize
         if (m_adwPointerMode[0] == IDC_MOVENSEW) {
            pR.p[2] = 0;   // cant move up
            pU.p[2] = 0;   // cant move up
         }
         else if (m_adwPointerMode[0] == IDC_MOVEUD) {   // it's MOVEUD
            pR.p[0] = pR.p[1] = 0;  // cane move right/left
            pU.p[0] = pU.p[1] = 0;  // cane move right/left
         }
         fp fMax;
         fMax = max(max(fabs(pU.p[0]), fabs(pU.p[1])), fabs(pU.p[2]));
         if (fMax > EPSILON)
            pU.Scale (1.0 / fMax);
         fMax = max(max(fabs(pR.p[0]), fabs(pR.p[1])), fabs(pR.p[2]));
         if (fMax > EPSILON)
            pR.Scale (1.0 / fMax);

         pR.Scale ((fp)iX * max(m_fGrid, .001));
         pU.Scale ((fp)iY * max(m_fGrid, .001));
         pR.Add (&pU);


         // don't need to apply grid since already applied by arrows

         // move
         MoveReferenceMatrixGet (&m_pMoveRotReferenceMatrix);
         m_pMoveRotReferenceWas.Copy (&m_pMoveRotReferenceMatrix);
         CMatrix mTrans;
         GridMove (&pR);
         mTrans.Translation (pR.p[0], pR.p[1], pR.p[2]);
         mTrans.MultiplyLeft (&m_pMoveRotReferenceMatrix);
         MoveReferenceMatrixSet (&mTrans, &m_pMoveRotReferenceWas);

         // display new settings
         UpdateToolTipByPointerMode();
      }
      return;

   case IDC_MOVEROTZ:
   case IDC_MOVEROTY:
      {
         if (IsEmbeddedObjectSelected() == 1) {
            // embedded object that need to show just the angle
            // get this object and find its position on the surface
            DWORD dwNum, *pdw;
            pdw = m_pWorld->SelectionEnum(&dwNum);
            if (dwNum != 1)
               break;   // shouldn't happen
            PCObjectSocket pEmbed, pContain;
            pEmbed = m_pWorld->ObjectGet (pdw[0]);
            if (!pEmbed)
               break;   // shouldnt happen
            GUID gContain, gEmbed;
            pEmbed->GUIDGet (&gEmbed);
            if (!pEmbed->EmbedContainerGet (&gContain))
               break;   // shouldn't happen
            pContain = m_pWorld->ObjectGet (m_pWorld->ObjectFind(&gContain));
            if (!pContain)
               break;   // shouldn't happen
            
            TEXTUREPOINT tp;
            fp fRotation;
            DWORD dwSurface;
            if (!pContain->ContEmbeddedLocationGet (&gEmbed, &tp,
               &fRotation, &dwSurface))
               break;   // shouldn't happen

            fRotation += (fp)(iX + iY) * m_fGridAngle;
            pContain->ContEmbeddedLocationSet (&gEmbed, &tp, fRotation, dwSurface);

            // display new settings
            UpdateToolTipByPointerMode();

            return;
         }

         // find out the vector it's around
         CPoint pVector;
         CPoint pRight;
         pVector.Zero();
         pRight.Zero();
         if (m_adwPointerMode[0] == IDC_MOVEROTZ)
            pVector.p[2] = 1;
         else {
            pVector.Copy (&pTowards);
         }
         pVector.Normalize();

         // see if can take point in direction of movement
         CPoint pDir, pOpposite;
         CPoint pBack;
         pDir.Zero();
         if (iX > 0)
            pDir.Add (&pR);
         else if (iX < 0)
            pDir.Subtract (&pR);
         if (iY > 0)
            pDir.Add (&pU);
         else if (iY < 0)
            pDir.Subtract (&pU);
         pDir.Normalize();
         if (iX)
            pOpposite.Copy (&pU);
         else
            pOpposite.Copy (&pR);

         // two sets of vectors, one pointing far right, the other slightly
         CMatrix mStart, mEnd;
         pRight.Average (&pDir, &pTowards, .01);
         pRight.Average (&pOpposite, .01);
         pRight.Normalize();
         pBack.CrossProd (&pVector, &pRight);
         pBack.Normalize();
         if (pBack.Length() < CLOSE)
            break;   // doesn't make any sense rotating in this direction
         pRight.CrossProd (&pBack, &pVector);
         pRight.Normalize();
         mStart.RotationFromVectors (&pRight, &pBack, &pVector);

         // and a bit more
         pRight.Average (&pDir, &pTowards, .05);
         pRight.Average (&pOpposite, .01);
         pRight.Normalize();
         pBack.CrossProd (&pVector, &pRight);
         pBack.Normalize();
         if (pBack.Length() < CLOSE)
            break;   // doesn't make any sense rotating in this direction
         pRight.CrossProd (&pBack, &pVector);
         pRight.Normalize();
         mEnd.RotationFromVectors (&pRight, &pBack, &pVector);

         // find out how much really rotated
         CMatrix mInv;
         mStart.Invert (&mInv);
         mEnd.MultiplyLeft (&mInv);
         CPoint p;
         fp fRotX, fRotY, fRotZ;
         mEnd.ToXYZLLT (&p, &fRotZ, &fRotX, &fRotY);

         // find the largest amount rotated and scale by that
         fp fLargest, fScale;
         fLargest = max(max(fabs(fRotZ), fabs(fRotX)), fabs(fRotY));
         if (fLargest < EPSILON)
            break;
         fScale = max(iX,-iX) + max(iY,-iY);
         fScale /= fLargest;
         fScale *= m_fGridAngle;
         fRotZ *= fScale;
         fRotX *= fScale;
         fRotY *= fScale;
         mEnd.FromXYZLLT (&p, fRotZ, fRotX, fRotY);

         // cetner point of rotation
         CPoint pCenter;
         CMatrix m;
         MoveReferencePointQuery(MoveReferenceCurQuery(), &pCenter);
         MoveReferenceMatrixGet (&m);
         m_pMoveRotReferenceWas.Copy (&m);
         pCenter.p[3] = 1;
         pCenter.MultiplyLeft (&m);

         // apply mEnd rotation
         CMatrix mTrans;
         mTrans.Translation (-pCenter.p[0], -pCenter.p[1], -pCenter.p[2]);
         m.MultiplyRight (&mTrans);
         m.MultiplyRight (&mEnd);
         mTrans.Translation (pCenter.p[0], pCenter.p[1], pCenter.p[2]);
         m.MultiplyRight (&mTrans);

         // grid
         m.ToXYZLLT (&p, &fRotZ, &fRotX, &fRotY);
         fRotZ = ApplyGrid(fmod(fRotZ, (fp)(2 * PI)), m_fGridAngle);
         fRotX = ApplyGrid(fmod(fRotX, (fp)(2 * PI)), m_fGridAngle);
         fRotY = ApplyGrid(fmod(fRotY, (fp)(2 * PI)), m_fGridAngle);
         m.FromXYZLLT (&p, fRotZ, fRotX, fRotY);

         MoveReferenceMatrixSet (&m, &m_pMoveRotReferenceWas);

         // display new settings
         UpdateToolTipByPointerMode();
      }
      return;

   case IDC_MOVEEMBED:
      {
         // embedded object that need to show just the angle
         // get this object and find its position on the surface
         DWORD dwNum, *pdw;
         pdw = m_pWorld->SelectionEnum(&dwNum);
         if (dwNum != 1)
            break;   // shouldn't happen
         PCObjectSocket pEmbed, pContain;
         pEmbed = m_pWorld->ObjectGet (pdw[0]);
         if (!pEmbed)
            break;   // shouldnt happen
         GUID gContain, gEmbed;
         pEmbed->GUIDGet (&gEmbed);
         if (!pEmbed->EmbedContainerGet (&gContain))
            break;   // shouldn't happen
         pContain = m_pWorld->ObjectGet (m_pWorld->ObjectFind(&gContain));
         if (!pContain)
            break;   // shouldn't happen
         
         TEXTUREPOINT tHV;
         fp fRotation;
         DWORD dwSurface;
         if (!pContain->ContEmbeddedLocationGet (&gEmbed, &tHV,
            &fRotation, &dwSurface))
            break;   // shouldn't happen

         TEXTUREPOINT tp;
         CPoint   pC, pR, pU;
         CMatrix  m;
         double   fLenR, fLenU;

         //center
         tp = tHV;
         if (!pContain->ContMatrixFromHV (dwSurface, &tp, 0, &m))
            break;
         pC.Zero();
         pC.p[3] = 1;
         pC.MultiplyLeft (&m);

         // convert to screen space...
         fp fX, fY;
         if (!m_apRender[0]->WorldSpaceToPixel (&pC, &fX, &fY))
            return;  // behind

         // send rays for right and left
         CPoint pEye, pRight, pUp, pCenter;
         m_apRender[0]->PixelToWorldSpace (fX, fY,
            (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);
         // BUGFIX - Change to only 1m, instead of 1000m, because of roundoff error
         m_apRender[0]->PixelToWorldSpace (fX+1, fY, 1, &pRight); // any distance will do
         m_apRender[0]->PixelToWorldSpace (fX, fY-1, 1, &pUp);
         // BUGFIX - Add pCenter to work around roundoff error
         m_apRender[0]->PixelToWorldSpace (fX, fY, 1, &pCenter); // so know where center points to
         pEye.p[3] = pRight.p[3] = pUp.p[3] = pCenter.p[3] = 1;
         CMatrix mInv;
         pContain->ObjectMatrixGet (&m);
         m.Invert4 (&mInv);
         pEye.MultiplyLeft (&mInv);
         pRight.MultiplyLeft (&mInv);
         pUp.MultiplyLeft (&mInv);
         pCenter.MultiplyLeft (&mInv);


         TEXTUREPOINT tpRight, tpUp;
         if (!pContain->ContHVQuery (&pEye, &pRight, dwSurface, NULL, &tpRight) ||
            !pContain->ContHVQuery (&pEye, &pUp, dwSurface, NULL, &tpUp) ||
            !pContain->ContHVQuery (&pEye, &pCenter, dwSurface, NULL, &tp)) {
               BeepWindowBeep (ESCBEEP_DONTCLICK);
               return;
         }

         // BUGFIX - to be nice to user, always move along grid lines in HV, so determine
         // which direction will be right
         BOOL fRightIsH;
         TEXTUREPOINT tpToRight, tpToUp;
         tpToRight.h = tpToRight.v = tpToUp.h = tpToUp.v = 0;
         tpRight.h -= tp.h;
         tpRight.v -= tp.v;
         tpUp.h -= tp.h;
         tpUp.v -= tp.v;
         fRightIsH = (fabs(tpRight.h) > fabs(tpRight.v));
         if (fRightIsH) {
            tpToRight.h = ((tpRight.h >= 0) ? 1 : -1) * HVDELTA;
            tpToUp.v = ((tpUp.v >= 0) ? 1 : -1) * HVDELTA;
         }
         else {
            tpToRight.v = ((tpRight.v >= 0) ? 1 : -1) * HVDELTA;
            tpToUp.h = ((tpUp.h >= 0) ? 1 : -1) * HVDELTA;
         }

         // right
         BOOL fInvert;
         fInvert = FALSE;
         tp = tHV;
         tp.h += tpToRight.h;
         tp.v += tpToRight.v;
         if (tp.h > 1.0) {
            tp.h -= 2*HVDELTA;
            fInvert = TRUE;
         }
         if (tp.v > 1.0) {
            tp.v -= 2*HVDELTA;
            fInvert = TRUE;
         }
         if (tp.h < 0) {
            tp.h += 2*HVDELTA;
            fInvert = TRUE;
         }
         if (tp.v < 0) {
            tp.v += 2*HVDELTA;
            fInvert = TRUE;
         }
         if (!pContain->ContMatrixFromHV (dwSurface, &tp, 0, &m))
            break;
         pR.Zero();
         pR.p[3] = 1;
         pR.MultiplyLeft (&m);
         pR.Subtract (&pC);
         if (fInvert)
            pR.Scale (-1);
         fLenR = pR.Length();

         // up
         tp = tHV;
         tp.h += tpToUp.h;
         tp.v += tpToUp.v;
         fInvert = FALSE;
         if (tp.h > 1.0) {
            tp.h -= 2*HVDELTA;
            fInvert = TRUE;
         }
         if (tp.v > 1.0) {
            tp.v -= 2*HVDELTA;
            fInvert = TRUE;
         }
         if (tp.h < 0) {
            tp.h += 2*HVDELTA;
            fInvert = TRUE;
         }
         if (tp.v < 0) {
            tp.v += 2*HVDELTA;
            fInvert = TRUE;
         }
         if (!pContain->ContMatrixFromHV (dwSurface, &tp, 0, &m))
            break;
         pU.Zero();
         pU.p[3] = 1;
         pU.MultiplyLeft (&m);
         pU.Subtract (&pC);
         if (fInvert)
            pU.Scale (-1);
         fLenU = pU.Length();

         // move
         if (fLenR > EPSILON) {
            tHV.h += (fp)iX * max(m_fGrid, .001) / fLenR * tpToRight.h;
            tHV.v += (fp)iX * max(m_fGrid, .001) / fLenR * tpToRight.v;
         }
         if (fLenU > EPSILON) {
            tHV.h += (fp)iY * max(m_fGrid, .001) / fLenU * tpToUp.h;
            tHV.v += (fp)iY * max(m_fGrid, .001) / fLenU * tpToUp.v;
         }
         tHV.h = max(0,tHV.h);
         tHV.h = min(1,tHV.h);
         tHV.v = max(0,tHV.v);
         tHV.v = min(1,tHV.v);

         SnapEmbedToGrid (pContain, dwSurface, &tHV);

         pContain->ContEmbeddedLocationSet (&gEmbed, &tHV, fRotation, dwSurface);

         // display new settings
         UpdateToolTipByPointerMode();
      }
      return;

   case IDC_VIEWFLATLRUD:
      {
         m_apRender[0]->CameraFlatGet (&m_pCameraCenter, &m_fCFLong, &m_fCFTilt, &m_fCFTiltY, &m_fCFScale, &m_fCFTransX, &m_fCFTransY);

         m_fCFTransX += (fp)iX * m_fCFScale / CTLSCALE;
         m_fCFTransY += (fp)iY * m_fCFScale / CTLSCALE;

         m_apRender[0]->CameraFlat (&m_pCameraCenter, m_fCFLong, m_fCFTilt, m_fCFTiltY, m_fCFScale, m_fCFTransX, m_fCFTransY);

         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);
         UpdateToolTipByPointerMode();
      }
      return;

   case IDC_VIEWFLATSCALE:
      {
         m_apRender[0]->CameraFlatGet (&m_pCameraCenter, &m_fCFLong, &m_fCFTilt, &m_fCFTiltY, &m_fCFScale, &m_fCFTransX, &m_fCFTransY);

         fp fAmt;
         fAmt = -(iX + iY);
         if (fAmt > 0)
            m_fCFScale *= pow((fp)2.0, (fp)(fAmt / CTLSCALE));
         else
            m_fCFScale /= pow((fp)2.0, (fp)(-fAmt / CTLSCALE) );

         m_apRender[0]->CameraFlat (&m_pCameraCenter, m_fCFLong, m_fCFTilt, m_fCFTiltY, m_fCFScale, m_fCFTransX, m_fCFTransY);

         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);
         UpdateToolTipByPointerMode();
      }
      return;
   case IDC_VIEWFLATROTZ:
   case IDC_VIEWFLATROTX:
   case IDC_VIEWFLATROTY:
      {
         m_apRender[0]->CameraFlatGet (&m_pCameraCenter, &m_fCFLong, &m_fCFTilt, &m_fCFTiltY, &m_fCFScale, &m_fCFTransX, &m_fCFTransY);

         fp fAmt;
         fAmt = (fp)(iX + iY) * m_fGridAngle;

         if (m_adwPointerMode[0] == IDC_VIEWFLATROTZ)
            m_fCFLong += fAmt;
         else if (m_adwPointerMode[0] == IDC_VIEWFLATROTX)
            m_fCFTilt -= fAmt;
         else
            m_fCFTiltY -= fAmt;

         m_apRender[0]->CameraFlat (&m_pCameraCenter, m_fCFLong, m_fCFTilt, m_fCFTiltY, m_fCFScale, m_fCFTransX, m_fCFTransY);

         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);
         UpdateToolTipByPointerMode();
      }
      return;

   case IDC_VIEW3DLRUD:
   case IDC_VIEW3DLRFB:
      {
         m_apRender[0]->CameraPerspObjectGet (&m_pV3D, &m_pCameraCenter, &m_fV3DRotZ, &m_fV3DRotX, &m_fV3DRotY, &m_fV3DFOV);

         pR.Scale ((fp)iX * max(m_fGrid, .001));
         m_pCameraCenter.Subtract (&pR);
         if (m_adwPointerMode[0] == IDC_VIEW3DLRUD) {
            pU.Scale ((fp)iY * max(m_fGrid, .001));
            m_pCameraCenter.Subtract (&pU);
         }
         else {
            pAway.Scale ((fp)iY * max(m_fGrid, .001));
            m_pCameraCenter.Subtract (&pAway);
         }

         m_apRender[0]->CameraPerspObject (&m_pV3D, &m_pCameraCenter, m_fV3DRotZ, m_fV3DRotX, m_fV3DRotY, m_fV3DFOV);

         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);
         UpdateToolTipByPointerMode();
      }
      return;

   case IDC_VIEW3DROTZ:
   case IDC_VIEW3DROTX:
   case IDC_VIEW3DROTY:
      {
         m_apRender[0]->CameraPerspObjectGet (&m_pV3D, &m_pCameraCenter, &m_fV3DRotZ, &m_fV3DRotX, &m_fV3DRotY, &m_fV3DFOV);

         fp fAmt;
         fAmt = (fp)(iX + iY) * m_fGridAngle;

         if (m_adwPointerMode[0] == IDC_VIEW3DROTZ)
            m_fV3DRotZ += fAmt;
         else if (m_adwPointerMode[0] == IDC_VIEW3DROTX)
            m_fV3DRotX -= fAmt;
         else
            m_fV3DRotY -= fAmt;

         m_apRender[0]->CameraPerspObject (&m_pV3D, &m_pCameraCenter, m_fV3DRotZ, m_fV3DRotX, m_fV3DRotY, m_fV3DFOV);

         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);
         UpdateToolTipByPointerMode();
      }
      return;

   case IDC_VIEWWALKFBROTZ:
      {
         // get the info
         CPoint p;
         fp fLong, fLat, fTilt, fFOV;
         m_apRender[0]->CameraPerspWalkthroughGet(&p, &fLong, &fLat, &fTilt, &fFOV);

         fLong -= PI * iX / (fp) CTLSCALE;

         
         // move forwards/backwards
         fp fWalkSpeed = m_fVWWalkSpeed * iY / (fp)CTLSCALE;

         // move
         p.p[0] += sin(-fLong) * fWalkSpeed;
         p.p[1] += cos(-fLong) * fWalkSpeed;

         // write it out
         m_apRender[0]->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, fFOV);
         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);
         UpdateToolTipByPointerMode();

      }
      return;

   case IDC_VIEWWALKROTZX:
   case IDC_VIEWWALKROTY:
      {
         CPoint p;
         fp fLong, fLat, fTilt, fFOV;
         m_apRender[0]->CameraPerspWalkthroughGet(&p, &fLong, &fLat, &fTilt, &fFOV);

         if (m_adwPointerMode[0] == IDC_VIEWWALKROTZX) {
            fLong -= (fp)iX * m_fGridAngle;
            fLat += (fp)iY * m_fGridAngle;
         }
         else if (m_adwPointerMode[0] == IDC_VIEWWALKROTY) {
            fTilt += (fp)(iX+iY) * m_fGridAngle;
         }
         fLong = ApplyGrid(fmod(fLong, (fp)(2 * PI)), m_fGridAngle);
         fLat = ApplyGrid(fmod(fLat, (fp)(2 * PI)), m_fGridAngle);
         fTilt = ApplyGrid(fmod(fTilt, (fp)(2 * PI)), m_fGridAngle);


         // write it out
         m_apRender[0]->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, fFOV);
         CameraPosition ();
         RenderUpdate(WORLDC_CAMERAMOVED);
         UpdateToolTipByPointerMode();
      }
      return;

   case IDC_VIEWWALKLRUD:
   case IDC_VIEWWALKLRFB:
      {
         CPoint p;
         fp fLong, fLat, fTilt, fFOV;
         m_apRender[0]->CameraPerspWalkthroughGet(&p, &fLong, &fLat, &fTilt, &fFOV);

         pR.Scale ((fp)iX * max(m_fGrid, .001));
         p.Subtract (&pR);
         if (m_adwPointerMode[0] == IDC_VIEWWALKLRUD) {
            pU.Scale ((fp)iY * max(m_fGrid, .001));
            p.Subtract (&pU);
         }
         else {
            pAway.Scale ((fp)iY * max(m_fGrid, .001));
            p.Subtract (&pAway);
         }

         m_apRender[0]->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, fFOV);

         CameraPosition();
         RenderUpdate(WORLDC_CAMERAMOVED);
         UpdateToolTipByPointerMode();
      }
      return;
   }

beep:
      BeepWindowBeep (ESCBEEP_DONTCLICK);
}

/***********************************************************************************
CHouseView::MakeSureAllWindowsClosed - Makes sure all additional windows (such
as object editing, and ground view) are closed.

inputs
   BOOL        fCloseAll - If TRUE, then closing all the windows that are open.
               Else, only closing this particular view
returns
   BOOL - TRUE if success, FALSE if not all closed
*/
BOOL CHouseView::MakeSureAllWindowsClosed (BOOL fCloseAll)
{
   // if polymesh no subwindows
   if ((m_dwViewWhat == VIEWWHAT_POLYMESH) || (m_dwViewWhat == VIEWWHAT_BONE))
      return TRUE;

   // if any ground views are open for this then fail
   if (!CloseOpenEditors (fCloseAll ? NULL : m_pWorld)) {
      EscMessageBox (m_hWnd, ASPString(),
         L"You must close all your object editing windows first.",
         NULL,
         MB_ICONWARNING | MB_OK);
      return FALSE;
   }

   // if any paint view open then dont close
   if (PaintViewModifying (fCloseAll ? NULL : m_pWorld)) {
      EscMessageBox (m_hWnd, ASPString(),
         L"You must close all your painting windows first.",
         NULL,
         MB_ICONWARNING | MB_OK);
      return FALSE;
   }

   // if going to close the last window (or has control) and there are any
   // object editing left then error out
   BOOL fClosingHouse;
   DWORD i;
   fClosingHouse = FALSE;
   for (i = 0; i < gListPCHouseView.Num(); i++) {
      PCHouseView pv = *((PCHouseView*) gListPCHouseView.Get(i));
      if (pv == this)
         fClosingHouse = TRUE;
   }
   if ( ( (fClosingHouse && (gListPCHouseView.Num() <= 1)) || fCloseAll) && gListVIEWOBJ.Num()) {
      EscMessageBox (m_hWnd, ASPString(),
         L"You must close all your object editing windows first.",
         L"You cannot close the last view of the house until you have closed all of the "
         L"views of the objects you're editing.",
         MB_ICONINFORMATION | MB_OK);
      return FALSE;
   }

   return TRUE;
}


/****************************************************************************
PMTextMirrorPage
*/
BOOL PMTextMirrorPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCObjectPolyMesh pm = pv->PolyMeshObject2();
         if (!pm)
            return TRUE;

         // default option
         PCEscControl pControl = NULL;
         DWORD dwSym = pm->m_PolyMesh.SymmetryGet();
         if (dwSym & 0x01)
            pControl = pPage->ControlFind (L"rtol");
         else if (dwSym & 0x02)
            pControl = pPage->ControlFind (L"btof");
         else if (dwSym & 0x04)
            pControl = pPage->ControlFind (L"ttob");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);


         // assume HV symmetry
         pControl = pPage->ControlFind (L"mirrorx");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         DWORD dwSurface = pm->ObjectSurfaceGetIndex (pv->m_dwPolyMeshCurText);
         TEXTUREPOINT tp;

         tp.h = tp.v = .5;

         // write them out
         if (pm->m_PolyMesh.SurfaceInMetersGet(dwSurface)) {
            tp.h = tp.v = 0;
            MeasureToString (pPage, L"transx", tp.h);
            MeasureToString (pPage, L"transy", tp.v);
         }
         else {
            tp.h = tp.v = .5;
            DoubleToControl (pPage, L"transx", tp.h);
            DoubleToControl (pPage, L"transy", tp.v);
         }

      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         // only care about the button to cause the mirroring
         if (_wcsicmp(psz, L"mirror"))
            break;

         // get the info
         PCEscControl pControl;
         DWORD dwDim;
         BOOL fUsePos;
         dwDim = 0;
         fUsePos = TRUE;
         if ((pControl = pPage->ControlFind (L"rtol")) && pControl->AttribGetBOOL(Checked())) {
            dwDim = 0;
            fUsePos = TRUE;
         }
         else if ((pControl = pPage->ControlFind (L"ltor")) && pControl->AttribGetBOOL(Checked())) {
            dwDim = 0;
            fUsePos = FALSE;
         }
         else if ((pControl = pPage->ControlFind (L"btof")) && pControl->AttribGetBOOL(Checked())) {
            dwDim = 1;
            fUsePos = TRUE;
         }
         else if ((pControl = pPage->ControlFind (L"ftob")) && pControl->AttribGetBOOL(Checked())) {
            dwDim = 1;
            fUsePos = FALSE;
         }
         else if ((pControl = pPage->ControlFind (L"ttob")) && pControl->AttribGetBOOL(Checked())) {
            dwDim = 2;
            fUsePos = TRUE;
         }
         else if ((pControl = pPage->ControlFind (L"btot")) && pControl->AttribGetBOOL(Checked())) {
            dwDim = 2;
            fUsePos = FALSE;
         }

         PCObjectPolyMesh pm = pv->PolyMeshObject2();
         if (!pm)
            return TRUE;
         DWORD dwSurface = pm->ObjectSurfaceGetIndex (pv->m_dwPolyMeshCurText);

         // get all the values
         TEXTUREPOINT tpNew;
         if (pm->m_PolyMesh.SurfaceInMetersGet(dwSurface)) {
            fp fTemp;
            if (!MeasureParseString (pPage, L"transx", &fTemp))
               tpNew.h = 0;
            else
               tpNew.h = fTemp;
            if (!MeasureParseString (pPage, L"transy", &fTemp))
               tpNew.v = 0;
            else
               tpNew.v = fTemp;
         }
         else {
            tpNew.h = DoubleFromControl (pPage, L"transx");
            tpNew.v = DoubleFromControl (pPage, L"transy");
         }

         // which checked
         BOOL fMirrorH = FALSE, fMirrorV = FALSE;
         if ((pControl = pPage->ControlFind (L"mirrorx")) && pControl->AttribGetBOOL(Checked()))
            fMirrorH = TRUE;
         if ((pControl = pPage->ControlFind (L"mirrory")) && pControl->AttribGetBOOL(Checked()))
            fMirrorV = TRUE;
         
         BOOL fRet;
         pm->m_pWorld->ObjectAboutToChange(pm);
         fRet = pm->m_PolyMesh.TextureMirror (dwSurface, dwDim, fUsePos, &tpNew, fMirrorH, fMirrorV);
         pm->m_pWorld->ObjectChanged(pm);

         if (!fRet) {
            pPage->MBWarning (L"Mirroring didn't work.");
            return TRUE;
         }

         // done
         pPage->Exit(Back());
         return TRUE;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         PWSTR pszComment = L"<comment>";
         PWSTR pszEndComment = L"</comment>";
         PWSTR pszBlank = L"";
         PCObjectPolyMesh pm = pv->PolyMeshObject2();
         if (!pm)
            break;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Mirror a texture";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFHIDEX")) {
            p->pszSubString = (!(pm->m_PolyMesh.SymmetryGet() & 0x01)) ? pszComment : pszBlank;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFHIDEX")) {
            p->pszSubString = (!(pm->m_PolyMesh.SymmetryGet() & 0x01)) ? pszEndComment : pszBlank;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFHIDEY")) {
            p->pszSubString = (!(pm->m_PolyMesh.SymmetryGet() & 0x02)) ? pszComment : pszBlank;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFHIDEY")) {
            p->pszSubString = (!(pm->m_PolyMesh.SymmetryGet() & 0x02)) ? pszEndComment : pszBlank;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFHIDEZ")) {
            p->pszSubString = (!(pm->m_PolyMesh.SymmetryGet() & 0x04)) ? pszComment : pszBlank;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFHIDEZ")) {
            p->pszSubString = (!(pm->m_PolyMesh.SymmetryGet() & 0x04)) ? pszEndComment : pszBlank;
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
LRESULT CHouseView::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (uMsg == gdwMouseWheel)
      uMsg = WM_MOUSEWHEEL;

   switch (uMsg) {
   case WM_CREATE:
      {
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

         DWORD i;
         for (i = 0; i < 4; i++)
            m_ahWndScroll[i] = CreateWindow ("SCROLLBAR", "",
               WS_VISIBLE | WS_CHILD | (((i==0) || (i == 2)) ? SBS_HORZ : SBS_VERT),
               10, 10, 10, 10,   // temporary sizes
               hWnd, (HMENU) (LONG_PTR) (IDC_MYSCROLLBAR+i), ghInstance, NULL);

         // clibboard viewer
         m_hWndClipNext = SetClipboardViewer (hWnd);

         // set a timer so sync with scene
         if (m_pSceneSet)
            SetTimer (hWnd, TIMER_SYNCWITHSCENE, 250, NULL);
      }

      return 0;

   case WM_HSCROLL:
   case WM_VSCROLL:
      {
         // find out which control it is
         DWORD dwScroll;
         for (dwScroll = 0; dwScroll < 4; dwScroll++)
            if (m_ahWndScroll[dwScroll] == (HWND) lParam)
               break;
         if (dwScroll >= 4)
            break;   // not this one

         // Deal with relative values
         DWORD dwModel;
         dwModel = m_apRender[0]->CameraModelGet();
         switch (dwModel) {
         default:
         case CAMERAMODEL_FLAT:
            dwModel = 0;
            break;
         case CAMERAMODEL_PERSPWALKTHROUGH:
            dwModel = 2;
            break;
         case CAMERAMODEL_PERSPOBJECT:
            dwModel = 1;
            break;
         }
         // not called anymore ScrollActionInit();
         BOOL fRelative;
         fRelative = m_aScrollAction[dwModel][dwScroll].fRelative;
         if ((LOWORD(wParam) != SB_THUMBPOSITION) && (LOWORD(wParam) != SB_THUMBTRACK))
            m_fScrollRelCache = FALSE; // for relative position info
         else if (fRelative) {
            // doing a track, which is a pain with relative, so may need to cache
            if (m_fScrollRelCache == FALSE) {
               m_fScrollRelCache = TRUE;
               // need to cache the current location
               switch (m_apRender[0]->CameraModelGet ()) {
               case CAMERAMODEL_FLAT:
                  m_apRender[0]->CameraFlatGet (&m_pScrollCacheCenterOfRot, &m_pScrollCacheRot.p[2], &m_pScrollCacheRot.p[0], &m_pScrollCacheRot.p[1],
                     &m_fScrollCacheScale, &m_pScrollCacheCamera.p[0], &m_pScrollCacheCamera.p[1]);
                  break;
               case CAMERAMODEL_PERSPWALKTHROUGH:
                  m_apRender[0]->CameraPerspWalkthroughGet (&m_pScrollCacheCamera, &m_pScrollCacheRot.p[2], &m_pScrollCacheRot.p[0], &m_pScrollCacheRot.p[1], &m_fScrollCacheFOV);
                  break;
               case CAMERAMODEL_PERSPOBJECT:
                  m_apRender[0]->CameraPerspObjectGet (&m_pScrollCacheCamera, &m_pScrollCacheCenterOfRot, &m_pScrollCacheRot.p[2], &m_pScrollCacheRot.p[0], &m_pScrollCacheRot.p[1], &m_fScrollCacheFOV);
                  break;
               }
            }
            else {
               // write out the cache    
               switch (m_apRender[0]->CameraModelGet ()) {
               case CAMERAMODEL_FLAT:
                  m_apRender[0]->CameraFlat (&m_pScrollCacheCenterOfRot, m_pScrollCacheRot.p[2], m_pScrollCacheRot.p[0], m_pScrollCacheRot.p[1],
                     m_fScrollCacheScale, m_pScrollCacheCamera.p[0], m_pScrollCacheCamera.p[1]);
                  break;
               case CAMERAMODEL_PERSPWALKTHROUGH:
                  m_apRender[0]->CameraPerspWalkthrough (&m_pScrollCacheCamera, m_pScrollCacheRot.p[2], m_pScrollCacheRot.p[0], m_pScrollCacheRot.p[1], m_fScrollCacheFOV);
                  break;
               case CAMERAMODEL_PERSPOBJECT:
                  m_apRender[0]->CameraPerspObject (&m_pScrollCacheCamera, &m_pScrollCacheCenterOfRot, m_pScrollCacheRot.p[2], m_pScrollCacheRot.p[0], m_pScrollCacheRot.p[1], m_fScrollCacheFOV);
                  break;
               }
            }
         }

         // get the scrollbar info
         SCROLLINFO si;
         memset (&si, 0, sizeof(si));
         si.cbSize = sizeof(si);
         si.fMask = SIF_ALL;
         GetScrollInfo (m_ahWndScroll[dwScroll], SB_CTL, &si);
         
#ifdef _DEBUG
         char szTemp[64];
         sprintf (szTemp, "WM_SCROLL: msg=%d, npos=%d, nTrack=%d\r\n", (int) LOWORD(wParam),
            si.nPos, si.nTrackPos);
         OutputDebugString (szTemp);
#endif
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
            m_fScrollRelCache = FALSE; // so reache next time
            break;
         case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
         }

         // don't go beyond min and max
         si.nPos = max(0,si.nPos);
         si.nPos = min(1000, si.nPos);

         // adjust the real-world values
         if (fRelative && (LOWORD(wParam) == SB_THUMBTRACK)) {
            m_fDisableChangeScroll = TRUE;   // disable while chaning

            // set the position
            si.fMask = SIF_POS;
            SetScrollInfo (m_ahWndScroll[dwScroll], SB_CTL, &si, TRUE);
         }
         if (ScrollActionFromScrollbar (si.nPos, dwScroll, m_apRender[0])) {
            CameraPosition();
            RenderUpdate(WORLDC_CAMERAMOVED);
         }
         m_fDisableChangeScroll = FALSE;

         return 0;
      }
      break;

   case WM_MEASUREITEM:
   case WM_DRAWITEM:
      return PolyMeshListBox (hWnd, uMsg, wParam, lParam);

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

   case WM_DRAWCLIPBOARD:
      {
         ClipboardUpdatePasteButton ();

         if (m_hWndClipNext)
            SendMessage (m_hWndClipNext, uMsg, wParam, lParam);
      }
      return 0;

   case WM_ACTIVATE:
      // BUGFIX - So dont bother when shutting down
      if ((LOWORD(wParam) == WA_ACTIVE) || (LOWORD(wParam) == WA_CLICKACTIVE)) {
         // refresh the tracing paper
         TraceInfoFromWorld (m_pWorld, &m_lTraceInfo);

         // refresh display of list
         PolyMeshCreateAndFillList ();
      }
      break;

   case WM_DESTROY:
      // take out timer for syncing
      if (m_pSceneSet)
         KillTimer (hWnd, TIMER_SYNCWITHSCENE);

      // remove from the clipboard chain
      ChangeClipboardChain(hWnd, m_hWndClipNext); 
      break;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {

      case IDC_TEXTLIST:
         return PolyMeshListBox (hWnd, uMsg, wParam, lParam);

      case IDC_OPEN:
         FileOpen ();
         return 0;

      case IDC_HELPBUTTON:
         switch (m_dwViewWhat) {
         case VIEWWHAT_POLYMESH:
            ASPHelp(IDR_HTUTORIALPM1);
            break;
         case VIEWWHAT_BONE:
            ASPHelp(IDR_HTUTORIALBONE1);
            break;
         case VIEWWHAT_OBJECT:
         default:
            ASPHelp();
            break;
         }
         return 0;

      case IDC_NEW:
         FileNew ();
         return 0;

      case IDC_SAVE:
         switch (m_dwViewWhat) {
            case VIEWWHAT_POLYMESH:
            case VIEWWHAT_BONE:
               // do nothing
               break;
            case VIEWWHAT_OBJECT:
               ObjectSave ();
               break;
            case VIEWWHAT_WORLD:
            default:
               FileSave (FALSE);
               break;
         }
         return 0;

      case IDC_SAVEAS:
         FileSave (TRUE);
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

      case IDC_PMTESSELATE:
         {
            // if there's no selection dont menu
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;
            DWORD dwNum;
            DWORD *padw;
            padw = pm->m_PolyMesh.SelEnum (&dwNum);
            if (!dwNum) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"You must select at least one side to use this.",
                  NULL,
                  MB_ICONEXCLAMATION | MB_OK);
               return 0;
            }

            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUPMTESSELATE));
            HMENU hSub = GetSubMenu(hMenu,0);

            POINT p;
            int iRet;
            GetCursorPos (&p);
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);
            if (!iRet)
               return 0;

            pm->m_pWorld->UndoRemember();
            pm->m_pWorld->ObjectAboutToChange (pm);
            DWORD dwTess;
            switch (iRet) {
            default:
            case ID_TESSELATE_DIVIDEQUADRILATERALSINTOTRIANGLES:
               dwTess = 0;
               break;
            case ID_TESSELATE_DIVIDEUSINGEXISTINGVERTICES:
               dwTess = 1;
               break;
            case ID_TESSELATE_DIVIDETHROUGHTHEEDGECENTERS:
               dwTess = 2;
               break;
            }
            pm->m_PolyMesh.SideTesselate (dwNum, padw, dwTess);
            pm->m_pWorld->ObjectChanged (pm);
            pm->m_pWorld->UndoRemember();

            DestroyMenu (hMenu);
         }
         return 0;

      case IDC_PMTEXTMIRROR:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return FALSE;

            // if not mirrored then error
            if (!pm->m_PolyMesh.SymmetryGet()) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"Mirroring won't work.",
                  L"You need to select some symmetry options, such as \"Left/right\" first.",
                  MB_ICONINFORMATION | MB_OK);
               return 0;
            }

            pm->m_pWorld->UndoRemember();

            UpdateToolTipByPointerMode();

            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (m_hWnd, &r);

            cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;

            // start with the first page
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPMTEXTMIRROR, PMTextMirrorPage, this);
            pm->m_pWorld->UndoRemember();
         }
         return 0;

      case IDC_PMTEXTPLANE:
      case IDC_PMTEXTCYLINDER:
      case IDC_PMTEXTSPHERE:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;
            CMatrix mObjectToWorld, mWorldToObject;
            pm->ObjectMatrixGet (&mObjectToWorld);
            mObjectToWorld.Invert4 (&mWorldToObject);

            // get the matrix for orientation...
            CPoint pA, pB, pC;
            m_apRender[0]->PixelToWorldSpace (0, 0, 100, &pB);   // any non-zero z will do
            m_apRender[0]->PixelToWorldSpace (1, 0, 100, &pA);
            m_apRender[0]->PixelToWorldSpace (0, -1, 100, &pC);
            pA.p[3] = pB.p[3] = pC.p[3] = 1;
            pA.MultiplyLeft (&mWorldToObject);
            pB.MultiplyLeft (&mWorldToObject);
            pC.MultiplyLeft (&mWorldToObject);

            // make matrix out of
            pA.Subtract (&pB);
            pC.Subtract (&pB);
            pA.Normalize();
            pC.Normalize();
            pB.CrossProd (&pC, &pA);
            pB.Normalize();
            pC.CrossProd (&pA, &pB);

            // new matrix
            CMatrix m, mInv;
            m.RotationFromVectors (&pA, &pB, &pC);
            m.Invert (&mInv); // only need 3x3 inverse

            // get the surface
            DWORD dwSurface = pm->ObjectSurfaceGetIndex (m_dwPolyMeshCurText);
            if (dwSurface == -1)
               return -1;

            // get all the sides in the surface
            CListFixed lSide;
            DWORD dwNum = pm->m_PolyMesh.SideNum();
            lSide.Init (sizeof(DWORD));
            DWORD i;
            for (i = 0; i < dwNum; i++) {
               PCPMSide ps = pm->m_PolyMesh.SideGet(i);
               if (ps->m_dwSurfaceText == dwSurface)
                  lSide.Add (&i);
            } // i

            if (!lSide.Num()) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"You need to have at least one side using the texture.",
                  NULL,
                  MB_ICONEXCLAMATION | MB_OK);
               return 0;
            }

            // is it in meters
            BOOL fInMeters = pm->m_PolyMesh.SurfaceInMetersGet(dwSurface);

            // call
            pm->m_pWorld->UndoRemember ();
            pm->m_pWorld->ObjectAboutToChange (pm);
            switch (LOWORD(wParam)) {
            default:
            case IDC_PMTEXTPLANE:
               pm->m_PolyMesh.TextureLinear (lSide.Num(), (DWORD*)lSide.Get(0), &mInv, fInMeters);
               break;
            case IDC_PMTEXTCYLINDER:
               pm->m_PolyMesh.TextureCylindrical (lSide.Num(), (DWORD*)lSide.Get(0), &mInv, fInMeters);
               break;
            case IDC_PMTEXTSPHERE:
               pm->m_PolyMesh.TextureSpherical (lSide.Num(), (DWORD*)lSide.Get(0), &mInv, fInMeters);
               break;
            }
            pm->m_pWorld->ObjectChanged(pm);
            pm->m_pWorld->UndoRemember ();

         }
         return 0;

      case IDC_PMEXTRA:
         {
            // if there's no selection dont menu
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;
            DWORD dwNum;
            DWORD *padw;
            padw = pm->m_PolyMesh.SelEnum (&dwNum);
            if ((m_dwViewSub == VSPOLYMODE_POLYEDIT) && !dwNum) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"You must select at least one vertex, edge, or side to use this.",
                  NULL,
                  MB_ICONEXCLAMATION | MB_OK);
               return 0;
            }

            // append menu for "show me"
            DWORD dwMenu;
            switch (m_dwViewSub) {
            default:
            case VSPOLYMODE_POLYEDIT:
               dwMenu = IDR_MENUPMEXTRA;
               break;
            case VSPOLYMODE_ORGANIC:
               dwMenu = IDR_MENUPMORGEXTRA;
               break;
            case VSPOLYMODE_BONE:
               dwMenu = IDR_MENUPMBONEEXTRA;
               break;
            }
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(dwMenu));
            HMENU hSub = GetSubMenu(hMenu,0);

            POINT p;
            GetCursorPos (&p);
            TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
         }
         return 0;

      case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMRIGHTTOLEFT:
      case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMLEFTTORIGHT:
      case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMBACKTOFRONT:
      case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMFRONTTOBACK:
      case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMTOPTOBOTTOM:
      case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMBOTTOMTOTOP:
      case ID_BONEEXTRA_COPYBONEWEIGHTSFROMRIGHTTOLEFT:
      case ID_BONEEXTRA_COPYBONEWEIGHTSFROMLEFTTORIGHT:
      case ID_BONEEXTRA_COPYBONEWEIGHTSFROMBACKTOFRONT:
      case ID_BONEEXTRA_COPYBONEWEIGHTSFROMFRONTTOBACK:
      case ID_BONEEXTRA_COPYBONEWEIGHTSFROMTOPTOBOTTOM:
      case ID_BONEEXTRA_COPYBONEWEIGHTSFROMBOTTOMTOTOP:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;

            DWORD dwDim = 0;
            BOOL fKeepPos = TRUE;
            BOOL fCopy = FALSE;
            switch (LOWORD(wParam)) {
            case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMRIGHTTOLEFT:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMRIGHTTOLEFT:
               break;
            case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMLEFTTORIGHT:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMLEFTTORIGHT:
               fKeepPos = FALSE;
               break;
            case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMBACKTOFRONT:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMBACKTOFRONT:
               dwDim = 1;
               break;
            case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMFRONTTOBACK:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMFRONTTOBACK:
               dwDim = 1;
               fKeepPos = FALSE;
               break;
            case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMTOPTOBOTTOM:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMTOPTOBOTTOM:
               dwDim = 2;
               break;
            case ID_BONEEXTRA_MIRRORBONEWEIGHTSFROMBOTTOMTOTOP:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMBOTTOMTOTOP:
               dwDim = 2;
               fKeepPos = FALSE;
               break;
            }

            switch (LOWORD(wParam)) {
            default:
               break;
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMRIGHTTOLEFT:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMLEFTTORIGHT:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMBACKTOFRONT:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMFRONTTOBACK:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMTOPTOBOTTOM:
            case ID_BONEEXTRA_COPYBONEWEIGHTSFROMBOTTOMTOTOP:
               fCopy = TRUE;
               break;
            }
            pm->m_pWorld->UndoRemember();
            pm->m_pWorld->ObjectAboutToChange (pm);
            BOOL fRet = pm->m_PolyMesh.BoneMirror (dwDim, fKeepPos, pm->m_pWorld, fCopy);
            pm->m_pWorld->ObjectChanged (pm);
            pm->m_pWorld->UndoRemember();

            // rebuild the list
            if (fRet)
               EscChime (ESCCHIME_INFORMATION);
            else {
               EscMessageBox (m_hWnd, ASPString(),
                  L"The mirroring failed.",
                  L"You may have tried to mirror along an axis without having turned on "
                  L"the symmetry for the mirroring for the polygon mesh or the bones.",
                  MB_ICONEXCLAMATION | MB_OK);
            }
         }
         return 0;

      case ID_BONEEXTRA_RESTOREAUTOMATICBONEWEIGHTS:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;

            pm->m_pWorld->UndoRemember();
            pm->m_pWorld->ObjectAboutToChange (pm);
            pm->m_PolyMesh.BoneClear();
            pm->m_PolyMesh.BoneSyncNamesOrFindOne (pm);
            pm->m_pWorld->ObjectChanged (pm);
            pm->m_pWorld->UndoRemember();

            // rebuild the list
            PolyMeshCreateAndFillList ();
            EscChime (ESCCHIME_INFORMATION);
         }
         return 0;

      case ID_EXTRA_MIRRORFROMTHERIGHTSIDETOTHELEFT:
      case ID_EXTRA_MIRRORFROMTHELEFTSIDETOTHERIGHT:
      case ID_EXTRA_MIRRORFROMTHEBACKTOTHEFRONT:
      case ID_EXTRA_MIRRORFROMTHEFRONTTOTHEBACK:
      case ID_EXTRA_MIRRORFROMTHETOPTOTHEBOTTOM:
      case ID_EXTRA_MIRRORFROMTHEBOTTOMTOTHETOP:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;

            // dim and direction
            DWORD dwDim = 0;
            BOOL fKeepPos = TRUE;
            switch (LOWORD(wParam)) {
            case ID_EXTRA_MIRRORFROMTHERIGHTSIDETOTHELEFT:
               dwDim = 0;
               fKeepPos = TRUE;
               break;
            case ID_EXTRA_MIRRORFROMTHELEFTSIDETOTHERIGHT:
               dwDim = 0;
               fKeepPos = FALSE;
               break;
            case ID_EXTRA_MIRRORFROMTHEBACKTOTHEFRONT:
               dwDim = 1;
               fKeepPos = TRUE;
               break;
            case ID_EXTRA_MIRRORFROMTHEFRONTTOTHEBACK:
               dwDim = 1;
               fKeepPos = FALSE;
               break;
            case ID_EXTRA_MIRRORFROMTHETOPTOTHEBOTTOM:
               dwDim = 2;
               fKeepPos = TRUE;
               break;
            case ID_EXTRA_MIRRORFROMTHEBOTTOMTOTHETOP:
               dwDim = 2;
               fKeepPos = FALSE;
               break;
            }

            pm->m_pWorld->UndoRemember();
            pm->m_pWorld->ObjectAboutToChange (pm);
            BOOL fRet;
            fRet = pm->m_PolyMesh.MirrorToOtherSide (dwDim, fKeepPos);
            pm->m_pWorld->ObjectChanged (pm);
            pm->m_pWorld->UndoRemember();

            if (!fRet) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"The mirroring failed.",
                  L"You may have tried to mirror from a side without any polygons.",
                  MB_ICONEXCLAMATION | MB_OK);
            }
         }
         return 0;


      case ID_EXTRA_FLIPLEFTANDRIGHT:
      case ID_EXTRA_FLIPFRONTANDBACK:
      case ID_EXTRA_FLIPTOPANDBOTTOM:
      case ID_EXTRA_ADDMOREDETAIL:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;
            CPoint pScale;
            pScale.p[0] = pScale.p[1] = pScale.p[2] = 1;

            pm->m_pWorld->UndoRemember();
            pm->m_pWorld->ObjectAboutToChange (pm);
            switch (LOWORD(wParam)) {
            case ID_EXTRA_FLIPLEFTANDRIGHT:
               pScale.p[0] = -1;
               pm->m_PolyMesh.OrganicScale (&pScale, NULL, 0, 0);
               break;
            case ID_EXTRA_FLIPFRONTANDBACK:
               pScale.p[1] = -1;
               pm->m_PolyMesh.OrganicScale (&pScale, NULL, 0, 0);
               break;
            case ID_EXTRA_FLIPTOPANDBOTTOM:
               pScale.p[2] = -1;
               pm->m_PolyMesh.OrganicScale (&pScale, NULL, 0, 0);
               break;
            case ID_EXTRA_ADDMOREDETAIL:
               pm->m_PolyMesh.Subdivide (TRUE);
               break;
            }
            pm->m_pWorld->ObjectChanged (pm);
            pm->m_pWorld->UndoRemember();
         }
         return 0;

      case ID_PMEXTRA_MAKETHESIDEPLANAR:
      case ID_PMEXTRA_FLIPTHESIDE:
      case ID_PMEXTRA_ROTATETHESIDE:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;
            DWORD dwNum;
            DWORD *padw;
            padw = pm->m_PolyMesh.SelEnum (&dwNum);

            pm->m_pWorld->UndoRemember();
            pm->m_pWorld->ObjectAboutToChange (pm);
            BOOL fRet = TRUE;
            switch (LOWORD(wParam)) {
            case ID_PMEXTRA_MAKETHESIDEPLANAR:
               fRet = pm->m_PolyMesh.SideMakePlanar (dwNum, padw);
               break;
            case ID_PMEXTRA_FLIPTHESIDE:
               pm->m_PolyMesh.SideFlipNormal (dwNum, padw);
               break;
            case ID_PMEXTRA_ROTATETHESIDE:
               pm->m_PolyMesh.SideRotateVert (dwNum, padw);
               break;
            }
            pm->m_pWorld->ObjectChanged (pm);
            pm->m_pWorld->UndoRemember();

            if (!fRet) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"The change failed.",
                  L"You may have too many morphs active to modify the shape. "
                  L"Try activating only one morph.",
                  MB_ICONEXCLAMATION | MB_OK);
            }
         }
         return 0;

      case IDC_PMTEXTDISCONNECT:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;

            // make sure have sides selected
            DWORD dwNum, *padw;
            padw = pm->m_PolyMesh.SelEnum (&dwNum);
            if (!dwNum || (pm->m_PolyMesh.SelModeGet() != 2)) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"You must select at least one side to disconnect it.",
                  NULL,
                  MB_ICONEXCLAMATION | MB_OK);
               return 0;
            }
            
            // disconnect
            BOOL fRet;
            DWORD dwSurface = pm->ObjectSurfaceGetIndex (m_dwPolyMeshCurText);
            pm->m_pWorld->UndoRemember();
            pm->m_pWorld->ObjectAboutToChange(pm);
            fRet = pm->m_PolyMesh.TextureDisconnect (dwNum, padw, dwSurface);
            pm->m_pWorld->ObjectChanged(pm);
            pm->m_pWorld->UndoRemember();

            if (!fRet) {
               EscMessageBox (m_hWnd, ASPString(),
                  L"Disconnection failed.",
                  L"The sides you selected may not have been on the current texture.",
                  MB_ICONEXCLAMATION | MB_OK);
               return 0;
            }
         }
         return 0;

      case IDC_VIEWFLATLRUD:
      case IDC_VIEWFLATSCALE:
      case IDC_VIEWFLATROTZ:
      case IDC_VIEWFLATROTX:
      case IDC_VIEWFLATROTY:
      case IDC_VIEW3DLRUD:
      case IDC_VIEW3DLRFB:
      case IDC_VIEW3DROTZ:
      case IDC_VIEW3DROTX:
      case IDC_VIEW3DROTY:
      case IDC_VIEWWALKFBROTZ:
      case IDC_VIEWWALKROTZX:
      case IDC_VIEWWALKROTY:
      case IDC_VIEWWALKLRUD:
      case IDC_VIEWWALKLRFB:
      case IDC_VIEWSETCENTERFLAT:
      case IDC_VIEWSETCENTER3D:
      case IDC_VIEWFLATLLOOKAT:
      case IDC_CLIPLINE:
      case IDC_CLIPPOINT:
      case IDC_SELINDIVIDUAL:
      case IDC_SELREGION:
      case IDC_MOVENSEW:
      case IDC_MOVENSEWUD:
      case IDC_MOVEEMBED:
      case IDC_MOVEUD:
      case IDC_MOVEROTZ:
      case IDC_MOVEROTY:
      case IDC_POSITIONPASTE:
      case IDC_OBJECTPASTE:
      case IDC_OBJECTPASTEDRAG:
      case IDC_OBJCONTROLUD:
      case IDC_OBJCONTROLNSEW:
      case IDC_OBJCONTROLINOUT:
      case IDC_OBJCONTROLNSEWUD:
      case IDC_OBJPAINT:
      case IDC_OBJMERGE:
      case IDC_PMMERGE:
      case IDC_BONEMERGE:
      case IDC_OBJATTACH:
      case IDC_ZOOMAT:
      case IDC_ZOOMIN:
      case IDC_ZOOMOUT:
      case IDC_ZOOMINDRAG:
      case IDC_OBJINTELADJUST:
      case IDC_GRIDFROMPOINT:
      case IDC_OBJDECONSTRUCT:
      case IDC_OBJOPENCLOSE:
      case IDC_OBJONOFF:
      case IDC_OBJDIALOG:
      case IDC_OBJSHOWEDITOR:
      case IDC_OBJATTRIB:
      case IDC_OBJCONTROLDIALOG:
      case IDC_BONEEDIT:
      case IDC_PMMOVEANYDIR:
      case IDC_PMTEXTSCALE:
      case IDC_PMTEXTMOVE:
      case IDC_PMTEXTROT:
      case IDC_PMBEVEL:
      case IDC_PMSIDESPLIT:
      case IDC_PMNEWSIDE:
      case IDC_PMMOVEINOUT:
      case IDC_PMMOVEPERP:
      case IDC_PMMOVEPERPIND:
      case IDC_PMTAILORNEW:
      case IDC_PMMOVESCALE:
      case IDC_PMORGSCALE:
      case IDC_PMMORPHSCALE:
      case IDC_PMMOVEROTPERP:
      case IDC_PMMOVEROTNONPERP:
      case IDC_PMMOVEDIALOG:
      case IDC_PMTEXTMOVEDIALOG:
      case IDC_PMCOLLAPSE:
      case IDC_PMTEXTCOLLAPSE:
      case IDC_PMDELETE:
      case IDC_PMEDGESPLIT:
      case IDC_PMSIDEINSET:
      case IDC_PMSIDEEXTRUDE:
      case IDC_PMSIDEDISCONNECT:
      case IDC_BRUSH4:
      case IDC_BRUSH8:
      case IDC_BRUSH16:
      case IDC_BRUSH32:
      case IDC_BRUSH64:
      case IDC_PMMAGANY:
      case IDC_PMMAGNORM:
      case IDC_PMMAGVIEWER:
      case IDC_PMMAGPINCH:
      case IDC_BONESPLIT:
      case IDC_BONEDELETE:
      case IDC_BONEDISCONNECT:
      case IDC_BONESCALE:
      case IDC_BONEROTATE:
      case IDC_BONENEW:
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
         m_pWorld->Undo(TRUE);
         // refresh display of list
         PolyMeshCreateAndFillList ();
         return 0;

      case IDC_REDOBUTTON:
         m_pWorld->Undo(FALSE);
         // refresh display of list
         PolyMeshCreateAndFillList ();
         return 0;

      case IDC_DELETEBUTTON:
         ClipboardDelete (TRUE);

         // remember this as an undo point
         m_pWorld->UndoRemember();
         return 0;

      case IDC_PMMODEPOLY:
         m_dwViewSub = VSPOLYMODE_POLYEDIT;
         m_dwPolyMeshSelMode = -1;  // to force update
         UpdateAllButtons ();
         PolyMeshCreateAndFillList ();
         RenderUpdate(WORLDC_NEEDTOREDRAW);
         break;

      case IDC_PMMODECLAY:
         m_dwViewSub = VSPOLYMODE_ORGANIC;
         RenderUpdate(WORLDC_NEEDTOREDRAW);
         PolyMeshCreateAndFillList ();
         UpdateAllButtons ();
         break;

      case IDC_PMMODEMORPH:
         m_dwViewSub = VSPOLYMODE_MORPH;
         RenderUpdate(WORLDC_NEEDTOREDRAW);
         PolyMeshCreateAndFillList ();
         UpdateAllButtons ();
         break;

      case IDC_PMMODEBONE:
         {
            m_dwViewSub = VSPOLYMODE_BONE;

            // reconnect bone to object if necessary
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (pm)
               pm->m_PolyMesh.BoneSyncNamesOrFindOne (pm);

            RenderUpdate(WORLDC_NEEDTOREDRAW);
            PolyMeshCreateAndFillList ();
            UpdateAllButtons ();
         }
         break;

      case IDC_PMMODETEXTURE:
         m_dwViewSub = VSPOLYMODE_TEXTURE;
         PolyMeshCreateAndFillList ();
         m_dwPolyMeshSelMode = -1;  // to force update
         UpdateAllButtons ();
         RenderUpdate(WORLDC_NEEDTOREDRAW);  // BUGFIX - moved to later
         break;

      case IDC_PMMODETAILOR:
         m_dwViewSub = VSPOLYMODE_TAILOR;
         RenderUpdate(WORLDC_NEEDTOREDRAW);
         PolyMeshCreateAndFillList ();
         UpdateAllButtons ();
         break;

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
         m_pWorld->UndoRemember();
         return 0;

      case IDC_SELALL:
         if (m_dwViewWhat == VIEWWHAT_POLYMESH) {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;
            pm->m_pWorld->ObjectAboutToChange (pm);
            pm->m_PolyMesh.SelAll(GetKeyState (VK_SHIFT) < 0);
            pm->m_pWorld->ObjectChanged (pm);
         }
         else {   // select objects
            DWORD i;
            OSMIGNOREWORLDBOUNDINGBOXGET bb;
            for (i = 0; i < m_pWorld->ObjectNum(); i++) {
               // dont select the cameras
               PCObjectSocket pos = m_pWorld->ObjectGet (i);
               bb.fIgnoreCompletely = FALSE;
               if (pos->Message (OSM_IGNOREWORLDBOUNDINGBOXGET, &bb) && bb.fIgnoreCompletely)
                  continue;

               m_pWorld->SelectionAdd (i);
            }
         }
         return 0;

      case IDC_SELNONE:
         if (m_dwViewWhat == VIEWWHAT_POLYMESH) {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;
            pm->m_pWorld->ObjectAboutToChange (pm);
            pm->m_PolyMesh.SelClear();
            pm->m_pWorld->ObjectChanged (pm);
         }
         else
            m_pWorld->SelectionClear();
         return 0;

      case IDC_SELSET:
         SelectionSettings(this);

         // since something may have been changed, take a snapshot for undo
         m_pWorld->UndoRemember();

         return 0;

      case IDC_VIEWFLAT:
         if (m_apRender[0]->CameraModelGet() != CAMERAMODEL_FLAT) {
            fp fLongitude, fTilt, fTiltY, fScale, fTransX, fTransY;
            CPoint pCenter;
            m_apRender[0]->CameraFlatGet (&pCenter, &fLongitude, &fTilt, &fTiltY, &fScale, &fTransX, &fTransY);
            m_apRender[0]->CameraFlat (&pCenter, fLongitude, fTilt, fTiltY, fScale, fTransX, fTransY);
            CameraPosition();
            UpdateCameraButtons ();
            RenderUpdate(WORLDC_CAMERAMOVED);
         }
         return 0;
      case IDC_VIEW3D:
         if (m_apRender[0]->CameraModelGet() != CAMERAMODEL_PERSPOBJECT) {
            CPoint p, pCenter;
            fp fX, fY, fZ, fFOV;
            m_apRender[0]->CameraPerspObjectGet (&p, &pCenter, &fZ, &fX, &fY, &fFOV);
            m_apRender[0]->CameraPerspObject (&p, &pCenter, fZ, fX, fY, fFOV);
            CameraPosition ();
            UpdateCameraButtons ();
            RenderUpdate(WORLDC_CAMERAMOVED);
         }
         return 0;
      case IDC_VIEWWALKTHROUGH:
         if (m_apRender[0]->CameraModelGet() != CAMERAMODEL_PERSPWALKTHROUGH) {
            CPoint p;
            fp fLong, fLat, fTilt, fFOV;
            m_apRender[0]->CameraPerspWalkthroughGet (&p, &fLong, &fLat, &fTilt, &fFOV);
            m_apRender[0]->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, fFOV);
            CameraPosition();
            UpdateCameraButtons ();
            RenderUpdate(WORLDC_CAMERAMOVED);
         }
         return 0;

      case IDC_VIEWSETTINGS:
         ViewSettings (this);

         // since something may have been changed, take a snapshot for undo
         m_pWorld->UndoRemember();

         return 0;

      case IDC_BRUSHSTRENGTH:
         {
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUBRUSHSTRENGTH));
            HMENU hSub = GetSubMenu(hMenu,0);

            // cache undo
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (pm)
               pm->m_pWorld->UndoRemember ();

            DWORD dwCheck;
            dwCheck = 0;
            switch (m_dwPolyMeshBrushStrength) {
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
               m_dwPolyMeshBrushStrength = 1;
               break;
            case ID_BRUSHSTRENGTH_2:
               m_dwPolyMeshBrushStrength = 2;
               break;
            case ID_BRUSHSTRENGTH_3:
               m_dwPolyMeshBrushStrength = 3;
               break;
            case ID_BRUSHSTRENGTH_4:
               m_dwPolyMeshBrushStrength = 4;
               break;
            case ID_BRUSHSTRENGTH_5:
               m_dwPolyMeshBrushStrength = 5;
               break;
            case ID_BRUSHSTRENGTH_6:
               m_dwPolyMeshBrushStrength = 6;
               break;
            case ID_BRUSHSTRENGTH_7:
               m_dwPolyMeshBrushStrength = 7;
               break;
            case ID_BRUSHSTRENGTH_8:
               m_dwPolyMeshBrushStrength = 8;
               break;
            case ID_BRUSHSTRENGTH_9:
               m_dwPolyMeshBrushStrength = 9;
               break;
            case ID_BRUSHSTRENGTH_10:
               m_dwPolyMeshBrushStrength = 10;
               break;
            }
         }
         return 0;



      case IDC_PMMAGSIZE:
         {
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUPMMAGSIZE));
            HMENU hSub = GetSubMenu(hMenu,0);

            // cache undo
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (pm)
               pm->m_pWorld->UndoRemember ();

            DWORD dwCheck;
            dwCheck = 0;
            switch (m_dwPolyMeshMagPoint) {
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

            dwCheck = 0;
            switch (m_dwPolyMeshMagSize) {
            case 0:
               dwCheck = ID_MAGSIZE_EXTRASMALL;
               break;
            case 1:
               dwCheck = ID_MAGSIZE_SMALL;
               break;
            case 2:
               dwCheck = ID_MAGSIZE_MEDIUM;
               break;
            case 3:
               dwCheck = ID_MAGSIZE_LARGE;
               break;
            case 4:
               dwCheck = ID_MAGSIZE_EXTRALARGE;
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
            case ID_MAGSIZE_EXTRASMALL:
               m_dwPolyMeshMagSize = 0;
               break;
            case ID_MAGSIZE_SMALL:
               m_dwPolyMeshMagSize = 1;
               break;
            case ID_MAGSIZE_MEDIUM:
               m_dwPolyMeshMagSize = 2;
               break;
            case ID_MAGSIZE_LARGE:
               m_dwPolyMeshMagSize = 3;
               break;
            case ID_MAGSIZE_EXTRALARGE:
               m_dwPolyMeshMagSize = 4;
               break;
            case ID_BRUSHSHAPE_FLAT:
               m_dwPolyMeshMagPoint = 0;
               break;
            case ID_BRUSHSHAPE_ROUNDED:
               m_dwPolyMeshMagPoint = 1;
               break;
            case ID_BRUSHSHAPE_POINTY:
               m_dwPolyMeshMagPoint = 2;
               break;
            case ID_BRUSHSHAPE_VERYPOINTY:
               m_dwPolyMeshMagPoint = 3;
               break;
            }
         }
         return 0;

      case IDC_BRUSHSHAPE:
         {
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUBRUSHSHAPEPM));
            HMENU hSub = GetSubMenu(hMenu,0);

            // cache undo
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (pm)
               pm->m_pWorld->UndoRemember ();

            DWORD dwCheck;
            dwCheck = 0;
            switch (m_dwPolyMeshBrushPoint) {
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

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            switch (iRet) {
            case ID_BRUSHSHAPE_FLAT:
               m_dwPolyMeshBrushPoint = 0;
               break;
            case ID_BRUSHSHAPE_ROUNDED:
               m_dwPolyMeshBrushPoint = 1;
               break;
            case ID_BRUSHSHAPE_POINTY:
               m_dwPolyMeshBrushPoint = 2;
               break;
            case ID_BRUSHSHAPE_VERYPOINTY:
               m_dwPolyMeshBrushPoint = 3;
               break;
            }
         }
         return 0;

      case IDC_BRUSHEFFECT:
         {
            DWORD dwMenu;
            switch (m_dwViewSub) {
            case VSPOLYMODE_ORGANIC:  // organic
            default:
               dwMenu = IDR_MENUBRUSHEFFECTPM;
               break;
            case VSPOLYMODE_MORPH:  // morph
               dwMenu = IDR_MENUBRUSHEFFECTPMMORPH;
               break;
            case VSPOLYMODE_TAILOR:  // tailor
               dwMenu = IDR_MENUBRUSHEFFECTPMTAILOR;
               break;
            case VSPOLYMODE_BONE:
               dwMenu = IDR_MENUBRUSHEFFECTPMBONE;
               break;
            }
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(dwMenu));
            HMENU hSub = GetSubMenu(hMenu,0);

            // cache undo
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (pm)
               pm->m_pWorld->UndoRemember ();

            DWORD dwCheck;
            dwCheck = 0;
            if (dwMenu == IDR_MENUBRUSHEFFECTPMTAILOR) switch (m_dwPolyMeshBrushAffectTailor) {
               case 0:
                  dwCheck = ID_BRUSHEFFECTTAILOR_USETHEPAINTEDAREFORTHECLOTHING;
                  break;
               case 1:
                  dwCheck = ID_BRUSHEFFECTTAILOR_REMOVETHEPAINTEDAREAFROMTHECLOTHING;
                  break;
            }
            else if (dwMenu == IDR_MENUBRUSHEFFECTPM) switch (m_dwPolyMeshBrushAffectOrganic) {
               case 0:
                  dwCheck = ID_BRUSHEFFECTPOLYMESH_ADDHEIGHT;
                  break;
               case 1:
                  dwCheck = ID_BRUSHEFFECTPOLYMESH_REMOVEHEIGHT;
                  break;
               case 2:
                  dwCheck = ID_BRUSHEFFECTPOLYMESH_PULLTOWARDSYOU;
                  break;
               case 3:
                  dwCheck = ID_BRUSHEFFECTPOLYMESH_PUSHAWAYFROMYOU;
                  break;
               case 4:
                  dwCheck = ID_BRUSHEFFECTPOLYMESH_SMOOTH;
                  break;
               case 5:
                  dwCheck = ID_BRUSHEFFECTPOLYMESH_ROUGHEN;
                  break;
               }
            else if (dwMenu == IDR_MENUBRUSHEFFECTPMMORPH) switch (m_dwPolyMeshBrushAffectMorph) {
               case 0:
                  dwCheck = ID_BRUSHEFFECTMORPH_INCREASEMORPH;
                  break;
               case 1:
                  dwCheck = ID_BRUSHEFFECTMORPH_DECREASEMORPH;
                  break;
               case 2:
                  dwCheck = ID_BRUSHEFFECTMORPH_ERASEMORPH;
                  break;
            }
            else if (dwMenu == IDR_MENUBRUSHEFFECTPMBONE) switch (m_dwPolyMeshBrushAffectBone) {
               case 0:
                  dwCheck = ID_BRUSHEFFECTBONE_INCREASEBONEWEIGHT;
                  break;
               case 1:
                  dwCheck = ID_BRUSHEFFECTBONE_DECREASEBONEWEIGHT;
                  break;
               case 2:
                  dwCheck = ID_BRUSHEFFECTBONE_SETBONEWEIGHT;
                  break;
               case 3:
                  dwCheck = ID_BRUSHEFFECTBONE_CLEARBONEWEIGHT;
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
            case ID_BRUSHEFFECTTAILOR_USETHEPAINTEDAREFORTHECLOTHING:
               m_dwPolyMeshBrushAffectTailor = 0;
               break;
            case ID_BRUSHEFFECTTAILOR_REMOVETHEPAINTEDAREAFROMTHECLOTHING:
               m_dwPolyMeshBrushAffectTailor = 1;
               break;
            case ID_BRUSHEFFECTMORPH_INCREASEMORPH:
               m_dwPolyMeshBrushAffectMorph = 0;
               break;
            case ID_BRUSHEFFECTMORPH_DECREASEMORPH:
               m_dwPolyMeshBrushAffectMorph = 1;
               break;
            case ID_BRUSHEFFECTMORPH_ERASEMORPH:
               m_dwPolyMeshBrushAffectMorph = 2;
               break;
            case ID_BRUSHEFFECTBONE_INCREASEBONEWEIGHT:
               m_dwPolyMeshBrushAffectBone = 0;
               break;
            case ID_BRUSHEFFECTBONE_DECREASEBONEWEIGHT:
               m_dwPolyMeshBrushAffectBone = 1;
               break;
            case ID_BRUSHEFFECTBONE_SETBONEWEIGHT:
               m_dwPolyMeshBrushAffectBone = 2;
               break;
            case ID_BRUSHEFFECTBONE_CLEARBONEWEIGHT:
               m_dwPolyMeshBrushAffectBone = 3;
               break;
            case ID_BRUSHEFFECTPOLYMESH_ADDHEIGHT:
               m_dwPolyMeshBrushAffectOrganic = 0;
               break;
            case ID_BRUSHEFFECTPOLYMESH_REMOVEHEIGHT:
               m_dwPolyMeshBrushAffectOrganic = 1;
               break;
            case ID_BRUSHEFFECTPOLYMESH_PULLTOWARDSYOU:
               m_dwPolyMeshBrushAffectOrganic = 2;
               break;
            case ID_BRUSHEFFECTPOLYMESH_PUSHAWAYFROMYOU:
               m_dwPolyMeshBrushAffectOrganic = 3;
               break;
            case ID_BRUSHEFFECTPOLYMESH_SMOOTH:
               m_dwPolyMeshBrushAffectOrganic = 4;
               break;
            case ID_BRUSHEFFECTPOLYMESH_ROUGHEN:
               m_dwPolyMeshBrushAffectOrganic = 5;
               break;
            }
         }
         return 0;

      case IDC_TRACING:
         // refresh the tracing paper
         TraceInfoFromWorld (m_pWorld, &m_lTraceInfo);

         TraceDialog (this);

         // refresh the tracing paper
         TraceInfoFromWorld (m_pWorld, &m_lTraceInfo);
         return 0;

      case IDC_GRIDSETTINGS:
         GridSettings (this);

         // since something may have been changed, take a snapshot for undo
         m_pWorld->UndoRemember();

         return 0;

      case IDC_LOCALE:
         LocaleSettings (this);

         // since something may have been changed, take a snapshot for undo
         m_pWorld->UndoRemember();

         return 0;

      case IDC_APPSET:
         AppSettings (this);

         // since something may have been changed, take a snapshot for undo
         m_pWorld->UndoRemember();

         return 0;

      case IDC_OBJEDITOR:
         ObjEditorSettings (this);

         // since something may have been changed, take a snapshot for undo
         m_pWorld->UndoRemember();

         return 0;

      case IDC_MOVEREFERENCE:
         {
            HMENU hMenu = CreatePopupMenu ();
            DWORD i, dwCur;
            dwCur = MoveReferenceCurQuery();
            WCHAR szTemp[256];
            char szaTemp[256];
            for (i = 0; ;i++) {
               if (!MoveReferenceStringQuery(i, szTemp, sizeof(szTemp), NULL))
                  break;
               MemZero(&gMemTemp);
               MemCatSanitize (&gMemTemp, szTemp);
               WideCharToMultiByte (CP_ACP, 0, (PWSTR)gMemTemp.p, -1, szaTemp, sizeof(szaTemp), 0, 0);

               AppendMenu (hMenu, MF_ENABLED | MF_STRING | ((i == dwCur) ? MF_CHECKED : 0),
                  IDC_MOVEREFERENCE + i, szaTemp);
            }

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hMenu, TPM_CENTERALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            if ((iRet >= IDC_MOVEREFERENCE) && (iRet <= IDC_MOVEREFERENCE + (int)i))
               MoveReferenceCurSet ((DWORD)iRet - IDC_MOVEREFERENCE);

            DestroyMenu (hMenu);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         return 0;


      case IDC_GRIDSELECT:
         {
            // make a list of sizes that want
            fp      afGrid[16], afAngle[16];
            DWORD       dwNum, dwNumAngle;
            dwNum = 0;
            afGrid[dwNum++] = 0; // no grid
            if (MeasureDefaultUnits() & MUNIT_ENGLISH) {
               afGrid[dwNum++] = 1.0 / INCHESPERMETER / 16.0;   // 1/16"
               afGrid[dwNum++] = 1.0 / INCHESPERMETER / 4.0;   // 1/4"
               afGrid[dwNum++] = 1.0 / INCHESPERMETER;   // 1"
               afGrid[dwNum++] = 1.0 / INCHESPERMETER * 6.0;   // 6"
               afGrid[dwNum++] = 1.0 / FEETPERMETER;  // 1'
               afGrid[dwNum++] = 1.0 / FEETPERMETER * 3.0;  // 3'
            }
            else {
               afGrid[dwNum++] = 0.001;
               afGrid[dwNum++] = 0.005;
               afGrid[dwNum++] = 0.01;
               afGrid[dwNum++] = 0.025;
               afGrid[dwNum++] = 0.05;
               afGrid[dwNum++] = 0.10;
               afGrid[dwNum++] = 0.25;
               afGrid[dwNum++] = 1.00;
            }
            
            dwNumAngle = 0;
            afAngle[dwNumAngle++] = 0;
            afAngle[dwNumAngle++] = PI / 180.0 * 1;
            afAngle[dwNumAngle++] = PI / 180.0 * 5;
            afAngle[dwNumAngle++] = PI / 180.0 * 10;
            afAngle[dwNumAngle++] = PI / 180.0 * 15;
            afAngle[dwNumAngle++] = PI / 180.0 * 45;

            HMENU hMenu = CreatePopupMenu ();
            DWORD i;
            char szTemp[32];
            BOOL fChecked, fFoundChecked;
            fFoundChecked = FALSE;
            for (i = 0; i < dwNum; i++) {
               fChecked = FALSE;
               if (fabs(m_fGrid - afGrid[i]) < EPSILON)
                  fChecked = TRUE;
               if (afGrid[i])
                  MeasureToString (afGrid[i], szTemp);
               else
                  strcpy (szTemp, "No grid");
               AppendMenu (hMenu, MF_ENABLED | MF_STRING | (fChecked ? MF_CHECKED : 0),
                  IDC_GRIDSELECT + i, szTemp);
               fFoundChecked |= fChecked;
            }

            // add custom
            AppendMenu (hMenu, MF_ENABLED | MF_STRING | ((!fFoundChecked) ? MF_CHECKED : 0),
               IDC_GRIDSELECT + 99, "Custom...");

            // separator
            AppendMenu (hMenu, MF_SEPARATOR, 0, 0);

            // Angles
            fFoundChecked = FALSE;
            for (i = 0; i < dwNumAngle; i++) {
               fChecked = FALSE;
               if (fabs(m_fGridAngle - afAngle[i]) < EPSILON)
                  fChecked = TRUE;
               if (afAngle[i]) {
                  AngleToString (szTemp, afAngle[i], TRUE);
                  strcat (szTemp, " degrees");
               }
               else
                  strcpy (szTemp, "No angle grid");
               AppendMenu (hMenu, MF_ENABLED | MF_STRING | (fChecked ? MF_CHECKED : 0),
                  IDC_GRIDSELECT + i + 100, szTemp);
               fFoundChecked |= fChecked;
            }
            AppendMenu (hMenu, MF_ENABLED | MF_STRING | ((!fFoundChecked) ? MF_CHECKED : 0),
               IDC_GRIDSELECT + 199, "Custom...");

            // BUGFIX - Show/hide grid
            AppendMenu (hMenu, MF_SEPARATOR, 0, 0);
            AppendMenu (hMenu, MF_ENABLED | MF_STRING | (m_fGridDisplay2D ? MF_CHECKED : 0),
               IDC_GRIDSELECT + 200, "Draw grid in flattened view");

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hMenu, TPM_LEFTALIGN | TPM_VCENTERALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);

            if (!iRet)
               return 0;

            if ((iRet >= IDC_GRIDSELECT) && (iRet < IDC_GRIDSELECT + (int)dwNum)) {
               // chose a new measurement
               m_fGrid = afGrid[iRet - IDC_GRIDSELECT];
            }
            else if ((iRet >= IDC_GRIDSELECT + 100) && (iRet < IDC_GRIDSELECT + 100 + (int)dwNum)) {
               m_fGridAngle = afAngle[iRet - IDC_GRIDSELECT - 100];
            }
            else if (iRet == IDC_GRIDSELECT+200) {
               m_fGridDisplay2D = !m_fGridDisplay2D;
            }
            else {
               // chose to custom modify
               CEscWindow cWindow;
               RECT r;
               DialogBoxLocation (m_hWnd, &r);

               cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
               PWSTR pszRet;

               pszRet = cWindow.PageDialog (ghInstance, IDR_MMLGRIDVALUES, GridValuesPage, this);
            }

            // since this may have caused the visuals to change, (drawing grid lines),
            // re-render
            RenderUpdate(WORLDC_CAMERAMOVED);

            // display new settings
            UpdateToolTipByPointerMode();
         }
         return 0;

      case IDC_OBJNEW:
         ObjectCreateMenu ();

         // remember this as an undo point
         m_pWorld->UndoRemember();
         return 0;

      case IDC_MOVESETTINGS:
         MoveSettings (this);

         // since something may have been changed, take a snapshot for undo
         m_pWorld->UndoRemember();

         return 0;

      case IDC_CLIPSET:
         ClipSettings(this);

         // since something may have been changed, take a snapshot for undo
         m_pWorld->UndoRemember();

         return 0;

      case IDC_VIEWFLATQUICK:
         {
            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUQUICK));
            HMENU hSub = GetSubMenu(hMenu,0);

            // add favorites to menu
            DWORD i;
            CListFixed lf;
            lf.Init (sizeof(FAVORITEVIEW));
            for (i = 0; i < FavoriteViewNum(); i++) {
               FAVORITEVIEW fv;
               if (!FavoriteViewGet (i, &fv))
                  continue;
               lf.Add (&fv);
            }
            qsort (lf.Get(0), lf.Num(), sizeof(FAVORITEVIEW), ViewFavoriteSort);
            for (i = 0; i < lf.Num(); i++) {
               PFAVORITEVIEW pfv = (PFAVORITEVIEW) lf.Get(i);
               char szTemp[128];
               MemZero(&gMemTemp);
               MemCatSanitize (&gMemTemp, pfv->szName);
               WideCharToMultiByte (CP_ACP, 0, (PWSTR)gMemTemp.p, -1, szTemp, sizeof(szTemp), 0, 0);
               AppendMenu (hSub, MF_ENABLED | MF_STRING, 0x8000 + i, szTemp);
            }

            BOOL fControl = (GetKeyState (VK_CONTROL) < 0);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            fControl |= (GetKeyState (VK_CONTROL) < 0);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            if (iRet == ID_QUICK_ADDTOFAVORITES) {
               CEscWindow cWindow;
               RECT r;
               DialogBoxLocation2 (hWnd, &r);
               cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
               cWindow.PageDialog (ghInstance, IDR_MMLVIEWFAVORITE, ViewFavoritePage, this);
               return 0;
            }
            else if (iRet == ID_QUICK_REMOVEFROMFAVORITES) {
               FAVORITEVIEW fv1, fv2;
               DWORD i;

               // get the current
               FavoriteViewGen (L"Test", &fv1);

               // find a match
               DWORD dwNum;
               dwNum = FavoriteViewNum();
               for (i = 0; i < dwNum; i++) {
                  if (!FavoriteViewGet (i, &fv2))
                     continue;
                  if ((fv1.dwCamera != fv2.dwCamera) || (fv1.fFOV != fv2.fFOV) ||
                     !fv1.pCenter.AreClose (&fv2.pCenter) ||
                     !fv1.pRot.AreClose (&fv2.pRot) ||
                     !fv1.pTrans.AreClose (&fv2.pTrans))
                     continue;

                  // else match
                  FavoriteViewRemove (i);
                  EscMessageBox (m_hWnd, ASPString(),
                     L"The camera view has been removed.",
                     NULL,
                     MB_ICONINFORMATION | MB_OK);

                  return TRUE;
               }
               EscMessageBox (m_hWnd, ASPString(),
                  L"This camera view is not on your favorites list.",
                  L"If you want to delete an item from your favorite camera angles then "
                  L"first select the camera angle, then select \"Remove camera angle\".",
                  MB_ICONINFORMATION | MB_OK);
               return TRUE;
            }
            else if ((iRet >= 0x8000) && (iRet <= 0x8fff)) {
               PFAVORITEVIEW pfv = (PFAVORITEVIEW) lf.Get (iRet - 0x8000);
               if (pfv)
                  FavoriteViewApply (pfv);
               return TRUE;
            }

            // world size
            CPoint b[2];
            CPoint pCenter;
            CPoint pMax;
            b[0].Zero();
            b[1].Zero();
            if ( ((m_dwViewWhat == VIEWWHAT_POLYMESH) || (m_dwViewWhat == VIEWWHAT_BONE)) && !fControl)
               PolyMeshBoundBox (b);
            else
               m_pWorld->WorldBoundingBoxGet (&b[0], &b[1], !fControl);

            DWORD x;
            for (x = 0; x < 3; x++) {
               pMax.p[x] = fabs(b[1].p[x] - b[0].p[x]) * 1.2; // so see just a little bit more
               // BUGFIX - If editing object then can zoom in as much as 1m
               pMax.p[x] = max(pMax.p[x], m_fViewScale / 2.0);   // always show at least 20m
               pMax.p[x] = min(pMax.p[x], m_fViewScale);  // never go beyond the view scale

               pMax.p[x] /= 2;
            }
            pCenter.Average (&b[0], &b[1]);
            b[0].Subtract (&pCenter, &pMax);
            b[1].Add (&pCenter, &pMax);

            LookAtArea (&b[0], &b[1], (DWORD)iRet);
         }
         return 0;

      case IDC_PMTEXTTOGGLE:
         m_fPolyMeshTextDisp = !m_fPolyMeshTextDisp;
         InvalidateRect (m_hWnd, &m_arThumbnailLoc[0], FALSE);
         return 0;

      case IDC_PMSHOWBACKGROUND:
         m_fShowOnlyPolyMesh = !m_fShowOnlyPolyMesh;
         RenderUpdate(WORLDC_NEEDTOREDRAW);
         return 0;

      case IDC_CLIPLEVEL:
         {
            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENULEVEL));
            HMENU hSub = GetSubMenu(hMenu,0);

            // offset clip plane this far below floor cutting out
            fp fOffset = .25;

            // what are the current floors
            fp afElev[NUMLEVELS], fHigher;
            GlobalFloorLevelsGet (m_pWorld, NULL, afElev, &fHigher);

            // what is it currently?
            fp fElevation;
            BOOL fRoof, fClip;
            DWORD dwFloor;
            RTCLIPPLANE plane;
            DWORD i;
            DWORD dwCheck;
            dwCheck = 0;
            if (!m_apRender[0]->ClipPlaneGet (CLIPPLANE_FLOORABOVE, &plane)) {
               dwCheck = ID_LEVEL_ALL;

               // Test to see if roof is shwn. If not then dwCheck = 0
               if (!(m_apRender[0]->RenderShowGet() & RENDERSHOW_ROOFS))
                  dwCheck = 0;
            }
            else {
               fElevation = plane.ap[0].p[2] + fOffset;
               for (i = 0; i < 11; i++) {
                  fp fLevel;
                  if (i+1 < NUMLEVELS)
                     fLevel = afElev[i+1];
                  else
                     fLevel = afElev[NUMLEVELS-1] + fHigher * (i+1 - NUMLEVELS + 1);
                  fLevel -= fOffset;

                  if (fabs(fLevel - plane.ap[0].p[2]) < .01)
                     break;
               }
               switch (i) {
               case 0:
                  dwCheck = ID_LEVEL_BASEMENT;
                  break;
               case 1:
                  dwCheck = ID_LEVEL_GROUNDFLOOR;
                  break;
               case 2:
                  dwCheck = ID_LEVEL_1STFLOOR;
                  break;
               case 3:
                  dwCheck = ID_LEVEL_2NDFLOOR;
                  break;
               case 4:
                  dwCheck = ID_LEVEL_3RDFLOOR;
                  break;
               case 5:
                  dwCheck = ID_LEVEL_4THFLOOR;
                  break;
               case 6:
                  dwCheck = ID_LEVEL_5THFLOOR;
                  break;
               case 7:
                  dwCheck = ID_LEVEL_6THFLOOR;
                  break;
               case 8:
                  dwCheck = ID_LEVEL_7THFLOOR;
                  break;
               case 9:
                  dwCheck = ID_LEVEL_8THFLOOR;
                  break;
               case 10:
                  dwCheck = ID_LEVEL_9THFLOOR;
                  break;
               }
            }
            if (dwCheck)
               CheckMenuItem (hSub, dwCheck, MF_BYCOMMAND | MF_CHECKED);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            // what elevation and roof status want
            fRoof = FALSE;
            fClip = TRUE;
            dwFloor = 0;
            switch (iRet) {
            case ID_LEVEL_BASEMENT:
               dwFloor = 1;
               break;
            case ID_LEVEL_GROUNDFLOOR:
               dwFloor = 2;
               break;
            case ID_LEVEL_1STFLOOR:
               dwFloor = 3;
               break;
            case ID_LEVEL_2NDFLOOR:
               dwFloor = 4;
               break;
            case ID_LEVEL_3RDFLOOR:
               dwFloor = 5;
               break;
            case ID_LEVEL_4THFLOOR:
               dwFloor = 6;
               break;
            case ID_LEVEL_5THFLOOR:
               dwFloor = 7;
               break;
            case ID_LEVEL_6THFLOOR:
               dwFloor = 8;
               break;
            case ID_LEVEL_7THFLOOR:
               dwFloor = 9;
               break;
            case ID_LEVEL_8THFLOOR:
               dwFloor = 10;
               break;
            case ID_LEVEL_9THFLOOR:
               dwFloor = 11;
               break;
            case ID_LEVEL_ALL:
               fClip = FALSE;
               fRoof = TRUE;
               break;
            }
            if (dwFloor < NUMLEVELS)
               fElevation = afElev[dwFloor];
            else
               fElevation = afElev[NUMLEVELS-1] + fHigher * (dwFloor - NUMLEVELS + 1);
            fElevation -= fOffset;

            // Set info about whether roof is to be displayed or not
            if (fRoof)
               m_apRender[0]->RenderShowSet (m_apRender[0]->RenderShowGet() | RENDERSHOW_ROOFS);
            else
               m_apRender[0]->RenderShowSet (m_apRender[0]->RenderShowGet() & ~RENDERSHOW_ROOFS);

            // make three points
            // get all the values
            RTCLIPPLANE t;

            if (fClip) {
               t.dwID = CLIPPLANE_FLOORABOVE;
               t.ap[0].Zero();
               t.ap[0].p[2] = fElevation;
               t.ap[1].Copy (&t.ap[0]);
               t.ap[1].p[0] = 1;
               t.ap[2].Copy (&t.ap[0]);
               t.ap[2].p[1] = -1;

               m_apRender[0]->ClipPlaneSet (t.dwID, &t);
            }
            else
               m_apRender[0]->ClipPlaneRemove(CLIPPLANE_FLOORABOVE);

            // redraw
            RenderUpdate(WORLDC_NEEDTOREDRAW);
         }
         return 0;


      case IDC_PMDIALOG:
         PolyMeshSettings ();
         return 0;

      case IDC_PMEDITVERT:
      case IDC_PMEDITEDGE:
      case IDC_PMEDITSIDE:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            DWORD dwMode;
            switch (LOWORD(wParam)) {
            case IDC_PMEDITVERT:
               dwMode = 0;
               break;
            case IDC_PMEDITEDGE:
               dwMode = 1;
               break;
            case IDC_PMEDITSIDE:
            default:
               dwMode = 2;
               break;
            }

            if (pm) {
               pm->m_pWorld->ObjectAboutToChange (pm);
               pm->m_PolyMesh.SelModeSet (dwMode);
               pm->m_pWorld->ObjectChanged (pm);
               }
            UpdatePolyMeshButton();
         }
         return 0;

      case IDC_LIBRARY:
         {
            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENULIBRARY));
            HMENU hSub = GetSubMenu(hMenu,0);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_RIGHTALIGN | TPM_VCENTERALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            switch (iRet) {
            case ID_LIBRARY_TEXTURES:
               TextureLibraryDialog (DEFAULTRENDERSHARD, hWnd);
               break;
            case ID_LIBRARY_OBJECTS:
               ObjectLibraryDialog (DEFAULTRENDERSHARD, hWnd);
               break;
            case ID_LIBRARY_EFFECTS:
               EffectLibraryDialog (DEFAULTRENDERSHARD, hWnd, &m_aImage[0], m_apRender[0], m_pWorld);
               break;
            }
         }
         return 0;

      case IDC_ANIM:
         {
            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUANIM));
            HMENU hSub = GetSubMenu(hMenu,0);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_RIGHTALIGN | TPM_VCENTERALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            switch (iRet) {
            case ID_ANIMATION_SHOWTIMELINE:
               SceneViewCreate (m_pWorld, m_pSceneSet);
               break;
            case ID_ANIMATION_CREATEMOVIE:
               {
                  ANIMINFO ai;
                  CMem memWFEX;
                  if (!AnimInfoDialog (m_hWnd, m_pWorld, m_pSceneSet, m_apRender[0], &memWFEX, &ai))
                     return 0;

                  // actually intiate animation
                  AnimAnimate (DEFAULTRENDERSHARD, &ai);
               }
               break;
            }
         }
         return 0;



      case IDC_PRINT:
         {
            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUPRINT));
            HMENU hSub = GetSubMenu(hMenu,0);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_RIGHTALIGN | TPM_VCENTERALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            // Switch on iret
            switch (iRet) {
            case ID_PRINT_SAVEIMAGE:
               {
                  PTFP tf;
                  memset (&tf, 0, sizeof(tf));
                  tf.pv = this;
                  KeyGet (gpszSaveImageEffectCode, &tf.gEffectCode);
                  KeyGet (gpszSaveImageEffectSub, &tf.gEffectSub);

                  COLORREF cTrans;
                  tf.hBitEffect = EffectGetThumbnail (DEFAULTRENDERSHARD, &tf.gEffectCode, &tf.gEffectSub,
                     m_hWnd, &cTrans, TRUE);

                  CEscWindow cWindow;
                  RECT r;
                  DialogBoxLocation (m_hWnd, &r);

                  cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
                  PWSTR pszRet;
redosave:
                  pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPRINTTOFILE, PrintToFilePage, &tf);

                  if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
                     goto redosave;

                  if (tf.hBitEffect)
                     DeleteObject (tf.hBitEffect);

                  KeySet (gpszSaveImageEffectCode, &tf.gEffectCode);
                  KeySet (gpszSaveImageEffectSub, &tf.gEffectSub);

               }
               break;
            case ID_PRINT_PRINT:
               {
                  PTFP tf;
                  memset (&tf, 0, sizeof(tf));
                  tf.pv = this;
                  KeyGet (gpszSaveImageEffectCode, &tf.gEffectCode);
                  KeyGet (gpszSaveImageEffectSub, &tf.gEffectSub);


                  COLORREF cTrans;
                  tf.hBitEffect = EffectGetThumbnail (DEFAULTRENDERSHARD, &tf.gEffectCode, &tf.gEffectSub,
                     m_hWnd, &cTrans, TRUE);

                  CEscWindow cWindow;
                  RECT r;
                  DialogBoxLocation (m_hWnd, &r);

                  cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
                  PWSTR pszRet;
redoprint:
                  pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPRINTTOPRINTER, PrintToPrinterPage, &tf);

                  if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
                     goto redoprint;

                  if (tf.hBitEffect)
                     DeleteObject (tf.hBitEffect);

                  KeySet (gpszSaveImageEffectCode, &tf.gEffectCode);
                  KeySet (gpszSaveImageEffectSub, &tf.gEffectSub);

               }
               break;
            }
         }
         return 0;


      case IDC_THUMBNAIL:
         {
            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUTHUMBNAIL));
            HMENU hSub = GetSubMenu(hMenu,0);

            DWORD dwCheck = 0;
            switch (m_dwThumbnailShow) {
            case 0:
            default:
               dwCheck = ID_THUMBNAIL_NOTHUMBNAILS;
               break;
            case 1:
               dwCheck = ID_THUMBNAIL_THUMBAILSONTHERIGHT;
               break;
            case 2:
               dwCheck = ID_THUMBNAIL_THUMBNAILSONTHEBOTTOM;
               break;
            }
            if (dwCheck)
               CheckMenuItem (hSub, dwCheck, MF_BYCOMMAND | MF_CHECKED);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            // deal with selected quality
            DWORD dwOldThumbnail;
            dwOldThumbnail = m_dwThumbnailShow;
            switch (iRet) {
            case ID_THUMBNAIL_NOTHUMBNAILS:
               m_dwThumbnailShow = 0;
               break;
            case ID_THUMBNAIL_THUMBAILSONTHERIGHT:
               m_dwThumbnailShow = 1;
               break;
            case ID_THUMBNAIL_THUMBNAILSONTHEBOTTOM:
               m_dwThumbnailShow = 2;
               break;
            case ID_THUMBNAIL_SETUPTHUMBNAILSTOOL:
               // all of these change the current pointer mode and the meaning about what it does
               SetPointerMode (LOWORD(wParam), 0);
               return 0;
            }

            // if the scale has changed need to deal with this
            if (dwOldThumbnail != m_dwThumbnailShow) {
               InvalidateRect (m_hWnd, NULL, FALSE);

               // fake a size message
               RECT r;
               GetClientRect (m_hWnd, &r);
               SendMessage (m_hWnd, WM_SIZE, 0, MAKELPARAM(r.right-r.left,r.bottom-r.top));

               // Need to cause thumbnails to be re-rendered
               RenderUpdate(WORLDC_CAMERAMOVED);
            }
         }
         return 0;

      case IDC_BONEDEFAULT:
         {
            PCObjectBone pb = BoneObject2();
            if (!pb)
               return 0;

            pb->DefaultPosition ();

            EscChime (ESCCHIME_INFORMATION);
         }
         return 0;

      case IDC_BONEMIRROR:
         {
            PCObjectBone pb = BoneObject2();
            if (!pb)
               return 0;

            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUPOLYMESHSYM));
            HMENU hSub = GetSubMenu(hMenu,0);

            DWORD dwSym;
            dwSym = pb->SymmetryGet();

            if (dwSym & 0x01)
               CheckMenuItem (hSub, ID_SYMMETRY_LEFT, MF_BYCOMMAND | MF_CHECKED);
            if (dwSym & 0x02)
               CheckMenuItem (hSub, ID_SYMMETRY_FRONT, MF_BYCOMMAND | MF_CHECKED);
            if (dwSym & 0x04)
               CheckMenuItem (hSub, ID_SYMMETRY_TOP, MF_BYCOMMAND | MF_CHECKED);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            // deal with selected quality
            switch (iRet) {
            case ID_SYMMETRY_LEFT:
               dwSym = dwSym ^ 0x01;
               break;
            case ID_SYMMETRY_FRONT:
               dwSym = dwSym ^ 0x02;
               break;
            case ID_SYMMETRY_TOP:
               dwSym = dwSym ^ 0x04;
               break;
            }

            pb->SymmetrySet (dwSym);
         }
         return 0;

      case IDC_PMMIRROR:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;

            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUPOLYMESHSYM));
            HMENU hSub = GetSubMenu(hMenu,0);

            DWORD dwSym;
            dwSym = pm->m_PolyMesh.SymmetryGet();

            if (dwSym & 0x01)
               CheckMenuItem (hSub, ID_SYMMETRY_LEFT, MF_BYCOMMAND | MF_CHECKED);
            if (dwSym & 0x02)
               CheckMenuItem (hSub, ID_SYMMETRY_FRONT, MF_BYCOMMAND | MF_CHECKED);
            if (dwSym & 0x04)
               CheckMenuItem (hSub, ID_SYMMETRY_TOP, MF_BYCOMMAND | MF_CHECKED);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            // deal with selected quality
            switch (iRet) {
            case ID_SYMMETRY_LEFT:
               dwSym = dwSym ^ 0x01;
               break;
            case ID_SYMMETRY_FRONT:
               dwSym = dwSym ^ 0x02;
               break;
            case ID_SYMMETRY_TOP:
               dwSym = dwSym ^ 0x04;
               break;
            }

            pm->m_pWorld->UndoRemember ();
            pm->m_pWorld->ObjectAboutToChange(pm);
            pm->m_PolyMesh.SymmetrySet (dwSym);
            pm->m_pWorld->ObjectChanged(pm);
            pm->m_pWorld->UndoRemember ();
         }
         return 0;

      case IDC_PMSTENCIL:
         {
            PCObjectPolyMesh pm = PolyMeshObject2();
            if (!pm)
               return 0;

            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUPMSTENCIL));
            HMENU hSub = GetSubMenu(hMenu,0);

            DWORD dwCheck;
            dwCheck = 0;
            switch (m_dwPolyMeshMaskColor) {
            case 0:
               dwCheck = ID_STENCIL_NOSTENCILING;
               break;
            case 1:
               dwCheck = ID_STENCIL_STENCILBASEDONRED;
               break;
            case 2:
               dwCheck = ID_STENCIL_STENCILBASEDONGREEN;
               break;
            case 3:
               dwCheck = ID_STENCIL_STENCILBASEDONBLUE;
               break;
            case 4:
               dwCheck = ID_STENCIL_STENCILBASEDONBRIGHTNESS;
               break;
            }
            if (dwCheck)
               CheckMenuItem (hSub, dwCheck, MF_BYCOMMAND | MF_CHECKED);

            if (m_fPolyMeshMaskInvert)
               CheckMenuItem (hSub, ID_STENCIL_REVERSESTENCIL, MF_BYCOMMAND | MF_CHECKED);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            // deal with selected quality
            switch (iRet) {
            case ID_STENCIL_NOSTENCILING:
               m_dwPolyMeshMaskColor = 0;
               break;
            case ID_STENCIL_STENCILBASEDONRED:
               m_dwPolyMeshMaskColor = 1;
               break;
            case ID_STENCIL_STENCILBASEDONGREEN:
               m_dwPolyMeshMaskColor = 2;
               break;
            case ID_STENCIL_STENCILBASEDONBLUE:
               m_dwPolyMeshMaskColor = 3;
               break;
            case ID_STENCIL_STENCILBASEDONBRIGHTNESS:
               m_dwPolyMeshMaskColor = 4;
               break;
            case ID_STENCIL_REVERSESTENCIL:
               m_fPolyMeshMaskInvert = !m_fPolyMeshMaskInvert;
               break;
            }

         }
         return 0;

      case IDC_VIEWQUALITY:
         {
            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUQUALITY));
            HMENU hSub = GetSubMenu(hMenu,0);

            DWORD dwCheck = 0;
            DWORD dwCheckRes = 0;
            DWORD dwCheckExposure = 0;
            switch (m_dwQuality) {
            case 0:
               dwCheck = ID_QUALITY_VERYLOW;
               break;
            case 1:
               dwCheck = ID_QUALITY_LOW;
               break;
            case 2:
               dwCheck = ID_QUALITY_POOR;
               break;
            case 3:
               dwCheck = ID_QUALITY_GOOD;
               break;
            case 4:
               dwCheck = ID_QUALITY_HIGH;
               break;
            case 5:
               dwCheck = ID_QUALITY_BEST;
               break;
            }
            if (m_dwImageScale > IMAGESCALE)
               dwCheckRes = ID_QUALITY_LOWRES;
            else if (m_dwImageScale == IMAGESCALE)
               dwCheckRes = ID_QUALITY_NORMALRES;
            else
               dwCheckRes = ID_QUALITY_HIGHRES;
            fp fExposure;
            fExposure = CM_LUMENSSUN / max(m_apRender[0]->ExposureGet(), CLOSE);
            fExposure = floor (log(fExposure) + .5);
            fExposure = max(0,fExposure);
            fExposure = min(6, fExposure);
            switch ((DWORD) fExposure) {
            case 0:
               dwCheckExposure = ID_QUALITY_LIGHTINGEXPOSURE_UNDEREXPOSEDINSUNLIGHT;
               break;
            case 1:
               dwCheckExposure = ID_QUALITY_LIGHTINGEXPOSURE_FULLSUNLIGHT;
               break;
            case 2:
               dwCheckExposure = ID_QUALITY_LIGHTINGEXPOSURE_OVERCAST;
               break;
            case 3:
               dwCheckExposure = ID_LIGHTINGEXPOSURE_DAYTIMEINTERIOR;
               break;
            case 4:
               dwCheckExposure = ID_QUALITY_LIGHTINGEXPOSURE_DARKINDOORS;
               break;
            case 5:
               dwCheckExposure = ID_QUALITY_LIGHTINGEXPOSURE_DARKNIGHTTIMEINTERIOR;
               break;
            case 6:
               dwCheckExposure = ID_QUALITY_LIGHTINGEXPOSURE_MOONLIGHT;
               break;
            }
            if (dwCheck)
               CheckMenuItem (hSub, dwCheck, MF_BYCOMMAND | MF_CHECKED);
            if (dwCheckRes)
               CheckMenuItem (hSub, dwCheckRes, MF_BYCOMMAND | MF_CHECKED);
            if (dwCheckExposure)
               CheckMenuItem (hSub, dwCheckExposure, MF_BYCOMMAND | MF_CHECKED);

            BOOL fControl = (GetKeyState (VK_CONTROL) < 0);

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);
            fControl |= (GetKeyState (VK_CONTROL) < 0);

            DestroyMenu (hMenu);
            if (!iRet)
               return 0;

            // deal with selected quality
            DWORD dwOldScale;
            dwOldScale = m_dwImageScale;
            switch (iRet) {
            case ID_QUALITY_VERYLOW:
               m_dwQuality = 0;
               break;
            case ID_QUALITY_LOW:
               m_dwQuality = 1;
               break;
            case ID_QUALITY_POOR:
               m_dwQuality = 2;
               break;
            case ID_QUALITY_GOOD:
               m_dwQuality = 3;
               break;
            case ID_QUALITY_HIGH:
               m_dwQuality = 4;
               break;
            case ID_QUALITY_BEST:
               m_dwQuality = 5;
               break;
            case ID_QUALITY_LOWRES:
               m_dwImageScale = IMAGESCALE * 2;
               fControl = FALSE;
               break;
            case ID_QUALITY_NORMALRES:
               m_dwImageScale = IMAGESCALE;
               fControl = FALSE;
               break;
            case ID_QUALITY_HIGHRES:
               m_dwImageScale = IMAGESCALE / 2;
               fControl = FALSE;
               break;
            case ID_QUALITY_LIGHTINGEXPOSURE_UNDEREXPOSEDINSUNLIGHT:
               m_apRender[0]->ExposureSet (CM_LUMENSSUN / exp((fp)0));
               fControl = FALSE;
               break;
            case ID_QUALITY_LIGHTINGEXPOSURE_FULLSUNLIGHT:
               m_apRender[0]->ExposureSet (CM_LUMENSSUN / exp((fp)1));
               fControl = FALSE;
               break;
            case ID_QUALITY_LIGHTINGEXPOSURE_OVERCAST:
               m_apRender[0]->ExposureSet (CM_LUMENSSUN / exp((fp)2));
               fControl = FALSE;
               break;
            case ID_LIGHTINGEXPOSURE_DAYTIMEINTERIOR:
               m_apRender[0]->ExposureSet (CM_LUMENSSUN / exp((fp)3));
               fControl = FALSE;
               break;
            case ID_QUALITY_LIGHTINGEXPOSURE_DARKINDOORS:
               m_apRender[0]->ExposureSet (CM_LUMENSSUN / exp((fp)4));
               fControl = FALSE;
               break;
            case ID_QUALITY_LIGHTINGEXPOSURE_DARKNIGHTTIMEINTERIOR:
               m_apRender[0]->ExposureSet (CM_LUMENSSUN / exp((fp)5));
               fControl = FALSE;
               break;
            case ID_QUALITY_LIGHTINGEXPOSURE_MOONLIGHT:
               m_apRender[0]->ExposureSet (CM_LUMENSSUN / exp((fp)6));
               fControl = FALSE;
               break;
            }

            // if the scale has changed need to deal with this
            if (dwOldScale != m_dwImageScale) {
               // fake a size message
               RECT r;
               GetClientRect (m_hWnd, &r);
               SendMessage (m_hWnd, WM_SIZE, 0, MAKELPARAM(r.right-r.left,r.bottom-r.top));
            }

            // apply the quality
            QualityApply();

            // redraw
            RenderUpdate(WORLDC_CAMERAMOVED);
            // if control down then edit
            if (fControl) {
               QualitySettings (this);
            }

         }
         return 0;
      case IDC_VIEWFLATSETTINGS:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (m_hWnd, &r);

            cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLVIEWFLAT, ViewFlatPage, this);

            // since something may have been changed, take a snapshot for undo
            m_pWorld->UndoRemember();

         }
         return 0;

      case IDC_VIEW3DSETTINGS:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (m_hWnd, &r);

            cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLVIEW3D, View3DPage, this);

            // since something may have been changed, take a snapshot for undo
            m_pWorld->UndoRemember();

         }
         return 0;

      case IDC_VIEWWALKSETTINGS:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (m_hWnd, &r);

            cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLVIEWWALK, ViewWalkPage, this);

            // since something may have been changed, take a snapshot for undo
            m_pWorld->UndoRemember();

         }
         return 0;

      case IDC_PAINTVIEW:
         PaintViewNew (m_pWorld, m_apRender[0]);
         return 0;

      case IDC_CLONEVIEW:
         Clone();
         EscChime (ESCCHIME_INFORMATION);
         return 0;

      case IDC_OBJECTLIST:
         ListViewNew (m_pWorld, hWnd);
         EscChime (ESCCHIME_INFORMATION);
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
         WndProc (m_hWnd, WM_HSCROLL,
            (zDelta > 0) ? SB_PAGEUP : SB_PAGEDOWN, (LPARAM) m_ahWndScroll[3]);
      }
      return 0;

   case WM_KEYDOWN:
      {
         switch (wParam) {
         case VK_ESCAPE:
            SetPointerMode (m_dwPointerModeLast, 0);
            return TRUE;

         case VK_UP:
            ArrowKeys (0, 1);
            return TRUE;
         case VK_DOWN:
            ArrowKeys (0, -1);
            return TRUE;
         case VK_LEFT:
            ArrowKeys (-1, 0);
            return TRUE;
         case VK_RIGHT:
            ArrowKeys (1, 0);
            return TRUE;

//         case VK_UP:
//            NextPointerMode (0);
//            return TRUE;
//         case VK_DOWN:
//            NextPointerMode (1);
//            return TRUE;
//         case VK_LEFT:
//            NextPointerMode (3);
//            return TRUE;
//         case VK_RIGHT:
//            NextPointerMode (2);
//            return TRUE;
         case VK_TAB:
            NextPointerMode ((GetKeyState (VK_SHIFT) < 0) ? 5 : 4);
            return TRUE;
         }

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

   case WM_LBUTTONDOWN:
      return ButtonDown (hWnd, uMsg, wParam, lParam, 0);
   case WM_MBUTTONDOWN:
      return ButtonDown (hWnd, uMsg, wParam, lParam, 1);
   case WM_RBUTTONDOWN:
      // if no buttons then right menu is overtaken
      if (m_fSmallWindow && m_fHideButtons)
         CreateApplicationMenu ();
      else
         return ButtonDown (hWnd, uMsg, wParam, lParam, 2);
      return 0;

   case WM_TIMER:
      if ((wParam == TIMER_AIRBRUSH) && m_dwButtonDown) {
         PolyMeshBrushApply (m_adwPointerMode[m_dwButtonDown-1],  &m_pntMouseLast);
      }
      else if (wParam == TIMER_SYNCWITHSCENE) {
         // sync with the scene
         if (m_pSceneSet)
            m_pSceneSet->SyncFromWorld ();
         return 0;
      }
      else if (wParam == WAITTODRAW) {
#ifdef _DEBUG
         char szTemp[256];
         sprintf (szTemp, "WM_TIMER: Draw now %lx\r\n", (__int64) m_hWnd);
         OutputDebugString (szTemp);
#endif
         // kill the timer
         KillTimer (m_hWnd, WAITTODRAW);
         m_fTimerWaitToDraw = NULL;

         // invalidate
         InvalidateDisplay ();
      }
      else if (wParam == WAITTOTIP) {
         // BUGFIX - only update the tooltip if this window is not disabled, because will
         // be disabled when dialog box up
         if (IsWindowEnabled (hWnd))
            UpdateToolTip ((char*) m_memTip.p, TRUE);
      }
      else if (m_dwMoveTimerID && (wParam == m_dwMoveTimerID)) {
         // BUGFIX
         if (!m_dwButtonDown)
            return 0;   // no button down

         POINT p;
         GetCursorPos (&p);

         POINT pc;
         pc.x = pc.y = 0;
         ClientToScreen (m_hWnd, &pc);

         int iX, iY;
         iX = p.x - pc.x;
         iY = p.y - pc.y;

         int iDistX, iDistY, iAbsDistX, iAbsDistY, iDist;
         iDistX = iX - m_pntButtonDown.x;
         iDistY = iY - m_pntButtonDown.y;
         iAbsDistX = max(iDistX, -iDistX);
         iAbsDistY = max(iDistY, -iDistY);
         iDist = max(iAbsDistX, iAbsDistY);

#define NOMOVEDIST      10

         switch (m_adwPointerMode[m_dwButtonDown-1]) {
         case IDC_VIEWWALKFBROTZ:
            {
               // if we're within 20 pixels then don't do anything
               if (iDist < 2 * NOMOVEDIST)
                  return 0;

               // get the info
               CPoint p;
               fp fLong, fLat, fTilt, fFOV;
               m_apRender[0]->CameraPerspWalkthroughGet(&p, &fLong, &fLat, &fTilt, &fFOV);

               fp fRotSpeed = m_fVWalkAngle;
               fRotSpeed = fRotSpeed * m_dwMoveTimerInterval / 1000.0;
               fp fMaxRotSpeed = 2 * m_fVWalkAngle;
               fRotSpeed *= ((fp) iAbsDistX / (fp)m_aImage[0].Width() * 2);
               fRotSpeed = min(fRotSpeed, fMaxRotSpeed);
               if (iDistX > NOMOVEDIST)
                  fLong -= fRotSpeed;
               else if (iDistX < -NOMOVEDIST)
                  fLong += fRotSpeed;

               
               // move forwards/backwards
               fp fWalkSpeed = m_fVWWalkSpeed;
               fWalkSpeed = fWalkSpeed * m_dwMoveTimerInterval / 1000.0;
               fp fMaxWalkSpeed = 2 * m_fVWWalkSpeed;
               fWalkSpeed *= ((fp) iAbsDistY / (fp)m_aImage[0].Height() * 2);
               fWalkSpeed = min(fWalkSpeed, fMaxWalkSpeed);

               if (iDistY > NOMOVEDIST)
                  fWalkSpeed *= -1;
               else if (iDistY <- NOMOVEDIST)
                  fWalkSpeed *= 1;
               else
                  fWalkSpeed = 0;

               // move
               p.p[0] += sin(-fLong) * fWalkSpeed;
               p.p[1] += cos(-fLong) * fWalkSpeed;

               // write it out
               m_apRender[0]->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, fFOV);
               CameraPosition();
               RenderUpdate(WORLDC_CAMERAMOVED);

            }
            break;
         }
      }
      break;

   case WM_LBUTTONUP:
      return ButtonUp (hWnd, uMsg, wParam, lParam, 0);
   case WM_MBUTTONUP:
      return ButtonUp (hWnd, uMsg, wParam, lParam, 1);
   case WM_RBUTTONUP:
      return ButtonUp (hWnd, uMsg, wParam, lParam, 2);

   case WM_MOUSEMOVE:
      return MouseMove (hWnd, uMsg, wParam, lParam);

   case WM_SIZE:
      {
         // called when the window is sized. Move the button bars around
         int iWidth = LOWORD(lParam);
         int iHeight = HIWORD(lParam);
         DWORD i;

         if (!m_hWnd)
            m_hWnd = hWnd; // so calcthumbnail loc works
         CalcThumbnailLoc ();

         m_fHideButtons = (iWidth < 350) || (iHeight < 280);

         if (m_hWndPolyMeshList)
            MoveWindow (m_hWndPolyMeshList, m_rPolyMeshList.left, m_rPolyMeshList.top,
               m_rPolyMeshList.right - m_rPolyMeshList.left,
               m_rPolyMeshList.bottom - m_rPolyMeshList.top, TRUE);

         if (m_fSmallWindow && m_fHideButtons) {
            m_pbarView->Show(FALSE);
            m_pbarGeneral->Show(FALSE);
            m_pbarMisc->Show(FALSE);
            m_pbarObject->Show(FALSE);

            // scrollbar
            for (i = 0; i < 4; i++)
               ShowWindow (m_ahWndScroll[i], SW_HIDE);

            // dont show tooltip
            UpdateToolTip (NULL);

            // dont change iWidth or iHeight
         }
         else {
            RECT  r;

            // scrollbar location
            for (i = 0; i < 4; i++) {
               if (!IsWindowVisible (m_ahWndScroll[i]))
                  ShowWindow (m_ahWndScroll[i], SW_SHOW);

               int ix, iy, iScrollWidth, iScrollHeight;
               ix = iy = VARSCREENSIZE;
               iScrollWidth = iScrollHeight = VARSCROLLSIZE;
               switch (i) {
               case 0:  // top
                  ix = m_arThumbnailLoc[0].left;
                  iy = m_arThumbnailLoc[0].top - VARSCROLLSIZE;
                  iScrollWidth = m_arThumbnailLoc[0].right - m_arThumbnailLoc[0].left;
                  break;
               case 1:  // right
                  ix = m_arThumbnailLoc[0].right;
                  iy = m_arThumbnailLoc[0].top;
                  iScrollHeight = m_arThumbnailLoc[0].bottom - m_arThumbnailLoc[0].top;
                  break;
               case 2:  // bottom
                  ix = m_arThumbnailLoc[0].left;
                  iy = m_arThumbnailLoc[0].bottom;
                  iScrollWidth = m_arThumbnailLoc[0].right - m_arThumbnailLoc[0].left;
                  break;
               case 3:  // left
                  ix = m_arThumbnailLoc[0].left - VARSCROLLSIZE;
                  iy = m_arThumbnailLoc[0].top;
                  iScrollHeight = m_arThumbnailLoc[0].bottom - m_arThumbnailLoc[0].top;
                  break;
               }

               MoveWindow (m_ahWndScroll[i], ix, iy, iScrollWidth, iScrollHeight, TRUE);
            }

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

            // resize the image
            //iWidth -= (VARSCREENSIZE+1)*2;
            //iHeight -= (VARSCREENSIZE+1)*2;
         }

         // add a bit so rounds up instead of down
         BOOL fUpdate;
         fUpdate = FALSE;
         for (i = 0; i < VIEWTHUMB; i++) {
            if (!m_afThumbnailVis[i])
               continue;

            iWidth = m_arThumbnailLoc[i].right - m_arThumbnailLoc[i].left;
            iHeight = m_arThumbnailLoc[i].bottom - m_arThumbnailLoc[i].top;
            if (!i) {
               iWidth = (iWidth * IMAGESCALE + (int)m_dwImageScale-1) / (int) m_dwImageScale;
               iHeight = (iHeight * IMAGESCALE + (int)m_dwImageScale-1) / (int) m_dwImageScale;
            }
            iWidth = max(1,iWidth);
            iHeight = max(1, iHeight);
            if ((iWidth != (int) m_aImage[i].Width()) || (iHeight != (int) m_aImage[i].Height())) {
               // defininitely need to resize
               m_aImage[i].Init ((DWORD)iWidth, (DWORD)iHeight, RGB(0xff,0xff,0xff));

               // test rendering
               m_apRender[i]->CImageSet (&m_aImage[i]);
               fUpdate = TRUE;
            }
         }  // over thumbnails
         if (fUpdate)
            RenderUpdate(WORLDC_CAMERAMOVED);
      }
      break;

   case WM_CLOSE:
      {
         BOOL fControl = (GetKeyState (VK_CONTROL) < 0);

         if (!MakeSureAllWindowsClosed (fControl))
            return 0;

         BOOL fClosingHouse;
         DWORD i;
         fClosingHouse = FALSE;
         for (i = 0; i < gListPCHouseView.Num(); i++) {
            PCHouseView pv = *((PCHouseView*) gListPCHouseView.Get(i));
            if (pv == this)
               fClosingHouse = TRUE;
         }

         // ask if really want to close if this is the last one around
         BOOL fAskSave;
         if (fClosingHouse)
            fAskSave = ((gListPCHouseView.Num() <= 1) || fControl);
         else
            fAskSave = TRUE;

         //  if polygon mesh then dont ask if want to close
         if ((m_dwViewWhat == VIEWWHAT_POLYMESH) || (m_dwViewWhat == VIEWWHAT_BONE)) {
            fAskSave = FALSE;
            fControl = FALSE;
         }

         if (fAskSave && m_pWorld->DirtyGet()) {
            int iRet;
            iRet = EscMessageBox (m_hWnd, ASPString(),
               L"Do you want to save your changes?",
               L"Closing this view will close the file. If you don't save your "
               L"changes they will be lost.",
               MB_ICONQUESTION | MB_YESNOCANCEL);

            if (iRet == IDCANCEL)
               return 0;

            if (iRet == IDYES) {
               switch (m_dwViewWhat) {
                  case VIEWWHAT_POLYMESH:
                  case VIEWWHAT_BONE:
                     // shouldnt get here
                     break;
                  case VIEWWHAT_OBJECT:
                     if (!ObjectSave())
                        return FALSE;
                     break;
                  case VIEWWHAT_WORLD:
                  default:
                     if (!FileSave (FALSE))
                        return 0;   // dont close
                     break;
               }
            }

            // BUGFIX - Moved after ask save so wont unset dirty flag if cancel from save
            m_pWorld->DirtySet (FALSE); // so dont get asked again
         }

         // if really want to close, then delete this
         if (fControl)
            CloseAllViewsDown();
         else
            delete this;
      }
      return 0;

   };

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/************************************************************************************
CHouseView::CalcThumbnailLoc -  Calculate the location of the thumbnails.

Fills in m_arThumbnailLoc[] and m_afThumbnailVis[]

inputs
   none
returns
   noen
*/
void CHouseView::CalcThumbnailLoc (void)
{
   memset (m_arThumbnailLoc, 0, sizeof(m_arThumbnailLoc));
   memset (m_afThumbnailVis, 0, sizeof(m_afThumbnailVis));

   // window rect
   if (!m_hWnd)
      return;  // can't calculate since no window
   RECT r;
   GetClientRect (m_hWnd, &r);

   m_fHideButtons = ((r.right - r.left) < 350) || ((r.bottom - r.top) < 280);

   // if have the list box then shrink down further
   if (m_hWndPolyMeshList) {
      m_rPolyMeshList = r;
      r.right = r.right / 4 * 3;
      m_rPolyMeshList.left = r.right + VARSCROLLSIZE;
   }

   // if not hide buttons then account for button size
   BOOL fShowScroll;
   fShowScroll = TRUE;
   if (!m_fHideButtons) {
      int   iBorder = (VARSCREENSIZE - VARSCROLLSIZE);
      r.left += iBorder;
      if (!m_hWndPolyMeshList)
         r.right -= iBorder;
      r.top += iBorder;
      r.bottom -= iBorder;

      m_rPolyMeshList.right -= iBorder;
      m_rPolyMeshList.top += iBorder;
      m_rPolyMeshList.bottom -= iBorder;
      // note: Dont modify m_rPolyMeshList.left
   }
   else if (m_fSmallWindow)
      fShowScroll = FALSE;

   // main window (including scroll)
   RECT rScroll;
   rScroll = r;
   switch (m_dwThumbnailShow) {
   case 1:  // to the right
      rScroll.right = (rScroll.left + rScroll.right * 4) / 5;  // BUGFIX - Not as square
      break;
   case 2:  // below
      rScroll.bottom = (rScroll.top + rScroll.bottom * 3) / 4;
      break;
   default: // whole window
      // do nothing
      break;
   }

   // remove the scrollbars from the main window
   m_arThumbnailLoc[0] = rScroll;
   if (fShowScroll) {
      m_arThumbnailLoc[0].left += VARSCROLLSIZE;
      m_arThumbnailLoc[0].right -= VARSCROLLSIZE;
      m_arThumbnailLoc[0].top += VARSCROLLSIZE;
      m_arThumbnailLoc[0].bottom -= VARSCROLLSIZE;
   }
   m_afThumbnailVis[0] = (m_arThumbnailLoc[0].right > m_arThumbnailLoc[0].left) &&
      (m_arThumbnailLoc[0].bottom > m_arThumbnailLoc[0].top);

   // the other ones?
   if (!m_dwThumbnailShow)
      return;  // no more
   DWORD i;
   if (m_dwThumbnailShow == 1) {
      rScroll.left = rScroll.right + VARSCROLLSIZE;
      rScroll.right = r.right;
   }
   else {
      rScroll.top = rScroll.bottom + VARSCROLLSIZE;
      rScroll.bottom = r.bottom;
   }
   rScroll.left += 1;
   rScroll.right -= 1;
   rScroll.top += 1;
   rScroll.bottom -= 1;

   int iThumbSeparate;
   iThumbSeparate = VARSCROLLSIZE;

   for (i = 1; i < VIEWTHUMB; i++) {
      int iRange, iStart;
      DWORD dwDim;
      switch (m_dwThumbnailShow) {
      case 1:  // up and down
         iStart = rScroll.top;
         iRange = rScroll.bottom - rScroll.top;
         dwDim = 1;
         break;
      case 2:  // right and left
         iStart = rScroll.left;
         iRange = rScroll.right - rScroll.left;
         dwDim = 0;
         break;
      }

      iRange -= (VIEWTHUMB-2) * iThumbSeparate;

      // location...
      fp fStart, fEnd;
      fStart = ((fp)i - 1) / (fp) (VIEWTHUMB-1) * (fp)iRange + iStart;
      fEnd = (fp)i / (fp) (VIEWTHUMB-1) * (fp)iRange + iStart;
      fStart += (i-1) * iThumbSeparate;  // so have line inbetween
      fEnd += (i-1) * iThumbSeparate;    // so have line inbetween

      m_arThumbnailLoc[i] = rScroll;
      if (dwDim == 0) {
         m_arThumbnailLoc[i].left = (int) fStart;
         m_arThumbnailLoc[i].right = (int) fEnd;
      }
      else {
         m_arThumbnailLoc[i].top = (int) fStart;
         m_arThumbnailLoc[i].bottom = (int) fEnd;
      }

      // is it visible
      m_afThumbnailVis[i] = (m_arThumbnailLoc[i].right > m_arThumbnailLoc[i].left) &&
         (m_arThumbnailLoc[i].bottom > m_arThumbnailLoc[i].top);
   }


}

/******************************************************************************
CHouseView::WorldAboutToChange - Callback from world
*/
void CHouseView::WorldAboutToChange (GUID *pgObject)
{
   // do nothing
}

/************************************************************************************
CHouseView::CWorldChanged - Called by the CWorldSocket object when the world has changed.
Also called internally when want to re-render.

inputs
   DWORD    dwChanged - Bits of WORLDC_XXX indicating what has changed
returns
   none
*/
void CHouseView::WorldChanged (DWORD dwChanged, GUID *pgObject)
{
   // If selection has been added to, removed, etc. then need
   // to change some of the buttons, because some functionality isn't available
   // unless selected
   DWORD i, dwNum, dwEmbedSel, dwNumEmbed;
   m_pWorld->SelectionEnum (&dwNum);
   dwEmbedSel = IsEmbeddedObjectSelected();
   dwNumEmbed = NumEmbeddedObjectsSelected();
   
   // if polymesh object changes then want to update the buttons
   UpdatePolyMeshButton ();
//   PolyMeshCreateAndFillList (); - take out because updates too often

   DWORD k;
   for (k = 0; k < 4; k++) {
      PCListFixed pl;
      BOOL fEnable;

      switch (k) {
      case 0:  // m_listNeedSeelct
         pl = &m_listNeedSelect;
         fEnable = (dwNum ? TRUE : FALSE);
         break;
      case 1:  // m_listNeedSelectNoEmbed
         pl = &m_listNeedSelectNoEmbed;
         // BUGFIX - Allow to have embedded as long as one non-embedded selcted
         //fEnable = dwNum && (dwEmbedSel == 0);
         fEnable = dwNum > dwNumEmbed;
         break;
      case 2:  // m_listNeedSelectOnlyOneEmbed
         pl = &m_listNeedSelectOnlyOneEmbed;
         fEnable = dwNum && (dwEmbedSel < 2);
         break;
      case 3:  // m_listNeedSelectOneEmbed
         pl = &m_listNeedSelectOneEmbed;
         fEnable = (dwNum==1) && (dwEmbedSel == 1);
         break;
      }

      for (i = 0; i < pl->Num(); i++) {
         PCIconButton *ppb = (PCIconButton*) pl->Get(i);
         PCIconButton pb = *ppb;

         // enable/disabled
         pb->Enable (fEnable);

         // if it has the flag set for the current control then turn this off
         if (!fEnable && (pb->FlagsGet() & IBFLAG_REDARROW))
            SetPointerMode (IDC_SELINDIVIDUAL, 0);
      }
   }

   // special case for object moving. only allow contorl points if there's one
   // selection
   if ((dwNum == 1) || (m_dwViewWhat == VIEWWHAT_BONE)) {
      m_apRender[0]->m_fDrawControlPointsOnSel = TRUE;
      if (m_pObjControlNSEW) {
         m_pObjControlNSEWUD->Enable(TRUE);
         m_pObjControlNSEW->Enable(TRUE);
         m_pObjControlUD->Enable(TRUE);
         m_pObjControlInOut->Enable(TRUE);
      }
   }
   else {
      m_apRender[0]->m_fDrawControlPointsOnSel = FALSE;
      if (m_pObjControlNSEW) {
         m_pObjControlNSEWUD->Enable(FALSE);
         m_pObjControlNSEW->Enable(FALSE);
         m_pObjControlUD->Enable(FALSE);
         m_pObjControlInOut->Enable(FALSE);
         for (i = 0; i < 3; i++)
            if ((m_adwPointerMode[i] == IDC_OBJCONTROLUD) || (m_adwPointerMode[i] == IDC_OBJCONTROLNSEW) ||
               (m_adwPointerMode[i] == IDC_OBJCONTROLINOUT) || (m_adwPointerMode[i] == IDC_OBJCONTROLNSEWUD))
               SetPointerMode (IDC_SELINDIVIDUAL, i); // for want of anything better to do
      }
   }

   // note what bits have changed
   m_fWorldChanged |= dwChanged;

   // if there's a timer running then kill it
   if (m_fTimerWaitToDraw) {
      // kill the timer
      KillTimer (m_hWnd, WAITTODRAW);
      m_fTimerWaitToDraw = NULL;
   }

   // if this window does not have the focus then set a timer so it will change
   // in a couple of seconds. This makes sure that the active window gets refreshed
   // all the time, while the inactive ones delay the refresh
   HWND hWndActive, hWndParent;
   hWndActive = GetActiveWindow();
   hWndParent = GetParent (hWndActive);
   // BUGFIX - Also change if the parent matches, so that if in dialog and
   // changing things the changes are reflected right away
   if (m_fWaitToDrawForce || (hWndActive == m_hWnd) || (hWndParent == m_hWnd)) {
#ifdef _DEBUG
      char szTemp[256];
      sprintf (szTemp, "World changed: Draw now %lx\r\n", (__int64) m_hWnd);
      OutputDebugString (szTemp);
#endif
      // invalidate the rectangle
      InvalidateDisplay ();
   }
   else {
#ifdef _DEBUG
      char szTemp[256];
      sprintf (szTemp, "World changed: Delay draw %lx\r\n", (__int64) m_hWnd);
      OutputDebugString (szTemp);
#endif
      SetTimer (m_hWnd, WAITTODRAW, 2000, NULL);
      m_fTimerWaitToDraw = TRUE;
   }

   // if the world changed and we're in the control point drag mode then
   // update the tooltip
   if ((m_adwPointerMode[0] == IDC_OBJCONTROLUD) || (m_adwPointerMode[0] == IDC_OBJCONTROLNSEW) ||
      (m_adwPointerMode[0] == IDC_OBJCONTROLNSEWUD) ||(m_adwPointerMode[0] == IDC_OBJCONTROLINOUT) ||
      (m_adwPointerMode[0] == IDC_OBJPAINT))
      UpdateToolTipByPointerMode ();
}

/********************************************************************************
CHouseView::WorldUndoChanged - Called by the world object when the undo buffer
has changed.
*/
void CHouseView::WorldUndoChanged (BOOL fUndo, BOOL fRedo)
{
   if (m_pUndo)
      m_pUndo->Enable (fUndo);
   if (m_pRedo)
      m_pRedo->Enable (fRedo);
}

/********************************************************************************
CHouseView::MoveReferenceCurQuery - Just like the CObjectTemplate... If only
one object is selected this calls into the object. If more than one is selected
it uses a member variable of CHouseView to simulate a conglomeration of objects.
*/
DWORD CHouseView::MoveReferenceCurQuery (void)
{
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum(&dwNum);
   if (!dwNum)
      return 0;
   if (dwNum == 1) {
      PCObjectSocket pObj = m_pWorld->ObjectGet (pdw[0]);
      return pObj->MoveReferenceCurQuery ();
   }

   // else, use internal info
   return m_dwMoveReferenceCur;
}

/********************************************************************************
CHouseView::MoveReferenceCurSet - Just like the CObjectTemplate... If only
one object is selected this calls into the object. If more than one is selected
it uses a member variable of CHouseView to simulate a conglomeration of objects.
*/
BOOL CHouseView::MoveReferenceCurSet (DWORD dwIndex)
{
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum(&dwNum);
   if (!dwNum)
      return 0;
   if (dwNum == 1) {
      PCObjectSocket pObj = m_pWorld->ObjectGet (pdw[0]);
      return pObj->MoveReferenceCurSet (dwIndex);
   }

   // else, use internal info
   // check to make sure it's ok
   WCHAR szTemp[256];
   if (!MoveReferenceStringQuery (dwIndex, szTemp, sizeof(szTemp), NULL))
      return FALSE;

   m_dwMoveReferenceCur = dwIndex;
   return TRUE;
}

/********************************************************************************
CHouseView::MoveReferencePointQuery - Just like the CObjectTemplate... If only
one object is selected this calls into the object. If more than one is selected
it uses a member variable of CHouseView to simulate a conglomeration of objects.
*/
BOOL CHouseView::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum(&dwNum);
   if (!dwNum)
      return 0;
   if (dwNum == 1) {
      PCObjectSocket pObj = m_pWorld->ObjectGet (pdw[0]);
      return pObj->MoveReferencePointQuery (dwIndex, pp);
   }

   // else, use internal info
   // Need to create bounding box in an interesting way...
   if (dwIndex >= sizeof(gaDefMoveP) / sizeof(DEFMOVEP))
      return FALSE;

   // get the bounding box of everything
   CPoint p1, p2;
   BOOL  fPValid;
   fPValid = FALSE;
   // get the matrix to use
   CMatrix mGrp, mGrpInv;
   MoveReferenceMatrixGet(&mGrp);
   mGrp.Invert4 (&mGrpInv);

   // loop through all the objects. Get their bounding boxes. Then,
   // convert their points into group points and use that for bounding
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      CMatrix mObj, mGrpInvObj;
      CPoint c[2];
      m_pWorld->BoundingBoxGet (pdw[i], &mObj, &c[0], &c[1]);
      mGrpInvObj.Multiply (&mGrpInv, &mObj);

      DWORD x,y,z,w;
      CPoint pt;
      for (x=0;x<2;x++) for(y=0;y<2;y++) for (z=0;z<2;z++) {
         pt.p[0] = c[x].p[0];
         pt.p[1] = c[y].p[1];
         pt.p[2] = c[z].p[2];
         pt.p[3] = 1;
         pt.MultiplyLeft (&mGrpInvObj);

         if (fPValid) {
            for (w = 0; w < 3; w++) {
               p1.p[w] = min(p1.p[w], pt.p[w]);
               p2.p[w] = max(p2.p[w], pt.p[w]);
            }
         }
         else {
            fPValid = TRUE;
            p1.Copy (&pt);
            p2.Copy (&pt);
         }
      }
   }

   pp->p[3] = 1;
   for (i = 0; i < 3; i++) {
      switch ((i == 0) ? gaDefMoveP[dwIndex].iX : ((i == 1) ? gaDefMoveP[dwIndex].iY : gaDefMoveP[dwIndex].iZ) ) {
      case -1:
         pp->p[i] = min(p1.p[i], p2.p[i]);
         break;
      case 1:
         pp->p[i] = max(p1.p[i], p2.p[i]);
         break;
      case 0:
      default:
         pp->p[i] = (p1.p[i] + p2.p[i]) / 2;
         break;
      }
   }

#ifdef _DEBUG
   char szTemp[256];
   sprintf (szTemp, "MoveReferencePointQuery: %g,%g,%g\r\n", (double)pp->p[0], (double)pp->p[1], (double)pp->p[2]);
   OutputDebugString (szTemp);
#endif

   return TRUE;
}

/********************************************************************************
CHouseView::MoveReferencePointQuery - Just like the CObjectTemplate... If only
one object is selected this calls into the object. If more than one is selected
it uses a member variable of CHouseView to simulate a conglomeration of objects.
*/
BOOL CHouseView::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum(&dwNum);
   if (!dwNum)
      return 0;
   if (dwNum == 1) {
      PCObjectSocket pObj = m_pWorld->ObjectGet (pdw[0]);
      return pObj->MoveReferenceStringQuery (dwIndex, psz, dwSize, pdwNeeded);
   }

   // else, use internal info
   if (dwIndex >= sizeof(gaDefMoveP) / sizeof(DEFMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   DWORD dwNeeded;
   dwNeeded = ((DWORD)wcslen (gaDefMoveP[dwIndex].pszName) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, gaDefMoveP[dwIndex].pszName);
      return TRUE;
   }
   else
      return FALSE;
}

/**************************************************************************************
CHouseView::MoveReferenceMatrixGet - Called to get the object's translation and rotation
matrix when moving/rotating a selection. If only one object is used then that object's
ObjectMatrixGet() function is called. Otherwise, this returns identity for multiple objects.

inputs
   PCMatrix    pm - Filled with the matrix
*/
void CHouseView::MoveReferenceMatrixGet (PCMatrix pm)
{
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum(&dwNum);
   if (!dwNum) {
      pm->Identity();
      return;
   }
   if (dwNum == 1) {
      PCObjectSocket pObj = m_pWorld->ObjectGet (pdw[0]);
      pObj->ObjectMatrixGet (pm);
      return;
   }

   // else, use internal info
   // use the first matrix of the largest bounding box (by length)
   fp fAreaMax = -1, fArea;
   DWORD dwArea = -1;
   DWORD i;
   CMatrix m, mBest;
   mBest.Identity();
   CPoint c1, c2;
   for (i = 0; i < dwNum; i++) {
      m_pWorld->BoundingBoxGet (pdw[i], &m, &c1, &c2);
      c1.Subtract (&c2);
      fArea = c1.Length();
      if (fArea > fAreaMax + EPSILON) {
         fAreaMax = fArea;
         dwArea = i;
         mBest.Copy (&m);
      }
   }
   pm->Copy (&mBest);

#ifdef _DEBUG
   char szTemp[256];
   sprintf (szTemp, "MoveReferenceMatrixGet: %g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g\r\n",
      (double)pm->p[0][0], (double)pm->p[1][0], (double)pm->p[2][0], (double)pm->p[3][0],
      (double)pm->p[0][1], (double)pm->p[1][1], (double)pm->p[2][1], (double)pm->p[3][1],
      (double)pm->p[0][2], (double)pm->p[1][2], (double)pm->p[2][2], (double)pm->p[3][2],
      (double)pm->p[0][3], (double)pm->p[1][3], (double)pm->p[2][3], (double)pm->p[3][3]
      );
   OutputDebugString (szTemp);
#endif
}

/**************************************************************************************
CHouseView::MoveReferenceMatrixSet - Called to set the object's translation and rotation
matrix when moving/rotating a selection. If only one object is used then that object's
ObjectMatrixSet() function is called. Otherwise, this moved all the objects.

inputs
   PCMatrix    pm - New matrix.
   PCMatrix    pmWas - Only used if multiple objects selected. Rather than re-calculate
               what the largest object's matrix is and then do inverses to figure out
               how much everything moves, this just does some inverses on pmWas.
               This shoudl start out that the old matrix, but it will be modified every
               time this function is called.
*/
void CHouseView::MoveReferenceMatrixSet (PCMatrix pm, PCMatrix pmWas)
{
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum(&dwNum);
   if (!dwNum)
      return;
   if (dwNum == 1) {
      PCObjectSocket pObj = m_pWorld->ObjectGet (pdw[0]);
      pObj->ObjectMatrixSet (pm);
      return;
   }

   // else, use internal info
   // find out what has changed and go move everything by that much
   CMatrix  mT, mWasInv;
   pmWas->Invert4 (&mWasInv);
   mT.Multiply (pm, &mWasInv);

   // transform all selected objects by the same matrix
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      PCObjectSocket pObj = m_pWorld->ObjectGet (pdw[i]);
      CMatrix m, m2;
      pObj->ObjectMatrixGet (&m);
      m2.Multiply (&mT, &m);
      pObj->ObjectMatrixSet (&m2);
   }
   pmWas->Copy (pm);
}


/**************************************************************************************
CHouseView::UpdateToolTip - Update tooltip according to a string.

inputs
   char        *psz - String
   BOOL        fForce - Defaults to FALSE. In this case, if the tooltip has changed
               will set a timer to redraw the tooltip in .5 seconds. If FORCE is set
               will automatically redraw.
returns
   none
*/
void CHouseView::UpdateToolTip (char *psz, BOOL fForce)
{
   // if there's a timer then kill the timer
   if (m_fTimerWaitToTip)
      KillTimer (m_hWnd, WAITTOTIP);
   m_fTimerWaitToTip = NULL;

   // delete it if it exists and we're not supposed to have
   if (!psz || !psz[0]) {
      if (m_pToolTip)
         delete m_pToolTip;
      m_pToolTip = NULL;
      return;
   }

   // if it exists and has the same text then do nothing
   if (m_pToolTip && !strcmp((char*)m_pToolTip->m_memTip.p, psz))
      return;

   // if we're not forced then set a timer for half a second and store away
   // the string.
   // BUGFIX - This will speed up moving objects around since tips will wait before
   // drawing
   if (!fForce) {
      if (!m_memTip.Required (strlen(psz)+1))
         return;
      strcpy ((char*)m_memTip.p, psz);
      SetTimer (m_hWnd, WAITTOTIP, 500, NULL);
      m_fTimerWaitToTip = TRUE;
      return;
   }

   // else, need to erase the tooltip are start over because there's new text
   if (m_pToolTip)
      delete m_pToolTip;
   m_pToolTip = NULL;

   // what corner should the tooltip be in
   DWORD dwDir;
   RECT r;
   GetWindowRect (m_hWnd, &r);
   if (m_fSmallWindow) {
      switch (m_dwToolTip) {
      case 1:  // UR
      case 2:  // LR
         r.left = r.right;
         r.bottom = r.top;
         dwDir = 1;
         break;
      case 0:  // UL
      case 3:  // LL
      default:
         r.right = r.left;
         r.bottom = r.top;
         dwDir = 3;
         break;
      }
   }
   else {
      // BUGFIX - Dont indent the tooltips so much, was *2 for everything
      r.right -= VARBUTTONSIZE;// + VARBUTTONSIZE/2;
      r.left += VARBUTTONSIZE;// + VARBUTTONSIZE/2;
      r.top += VARBUTTONSIZE;// + VARBUTTONSIZE/2;
      r.bottom -= VARBUTTONSIZE;// + VARBUTTONSIZE/2;
      switch (m_dwToolTip) {
      case 0:  // UL
         r.right = r.left;
         r.bottom = r.top;
         dwDir = 1;
         break;
      case 1:  // UR
         r.left = r.right;
         r.bottom = r.top;
         dwDir = 3;
         break;
      case 2:  // LR
         r.left = r.right;
         r.top = r.bottom;
         dwDir = 4;
         break;
      case 3:  // LL
      default:
         r.right = r.left;
         r.top = r.bottom;
         dwDir = 0;
         break;
      }
   }

   // if it doesn't exist and it's supposed to then set it
   if (!m_pToolTip) {
      m_pToolTip = new CToolTip;
      m_pToolTip->Init (psz, dwDir, &r, m_hWnd);
      return;
   }
}


/**************************************************************************************
CHouseView::UpdateToolTipByPointerMode - Update tooltip according to the pointer mode.

inputs
   none
returns
   none
*/
#define TTTMARGIN  "lrmargin=2 tbmargin=2"

void CHouseView::UpdateToolTipByPointerMode (void)
{
   DWORD dwButton = (m_dwButtonDown ? (m_dwButtonDown - 1) : 0);
   char  szHuge[10000];
   szHuge[0] = 0;

   // if small view dont show
   if (m_fSmallWindow && m_fHideButtons) {
      UpdateToolTip (NULL);
      return;
   }

   switch (m_adwPointerMode[dwButton]) {
   case IDC_OBJOPENCLOSE:
      {
         BOOL fInfo = FALSE;
         strcat (szHuge, "Click on an object (such as a window or door) to open or close it.<p/>");
         strcat (szHuge, "<table width=100%% " TTTMARGIN "><tr><td>Amount open</td><td><bold>");
         if (m_dwObjControlObject != (DWORD)-1) {

            PCObjectSocket pos = m_pWorld->ObjectGet (m_dwObjControlObject);
            fp fOpen;
            if (pos && ((fOpen = pos->OpenGet()) >= 0)) {
               fInfo = TRUE;

               sprintf (szHuge + strlen(szHuge),
                  "%d%%",
                  (int) (fOpen * 100));
            }
         }

         if (!fInfo)
            strcat (szHuge, "Can't open/close");

         strcat (szHuge, "</bold></td></tr></table>");
      }
      break;
   case IDC_OBJONOFF:
      {
         BOOL fInfo = FALSE;
         strcat (szHuge, "Click on an object (such as a light) to turn it on or off.<p/>");
         strcat (szHuge, "<table width=100%% " TTTMARGIN "><tr><td>Amount on</td><td><bold>");
         if (m_dwObjControlObject != (DWORD)-1) {

            PCObjectSocket pos = m_pWorld->ObjectGet (m_dwObjControlObject);
            fp fOpen;
            if (pos && ((fOpen = pos->TurnOnGet()) >= 0)) {
               fInfo = TRUE;

               if (fOpen)
                  sprintf (szHuge + strlen(szHuge), "%d%%", (int) (fOpen * 100));
               else
                  strcat (szHuge, "Off");
            }
         }

         if (!fInfo)
            strcat (szHuge, "Can't turn on/off");

         strcat (szHuge, "</bold></td></tr></table>");
      }
      break;

   case IDC_VIEWFLATLRUD:
   case IDC_VIEWFLATSCALE:
   case IDC_VIEWFLATROTZ:
   case IDC_VIEWFLATROTX:
   case IDC_VIEWFLATROTY:
   case IDC_VIEWSETCENTERFLAT:
   case IDC_ZOOMINDRAG:
   case IDC_ZOOMAT:
   case IDC_ZOOMIN:
   case IDC_ZOOMOUT:
   case IDC_VIEW3DLRUD:
   case IDC_VIEW3DLRFB:
   case IDC_VIEW3DROTZ:
   case IDC_VIEW3DROTX:
   case IDC_VIEW3DROTY:
   case IDC_VIEWSETCENTER3D:
   case IDC_VIEWWALKFBROTZ:
   case IDC_VIEWWALKROTZX:
   case IDC_VIEWWALKROTY:
   case IDC_VIEWWALKLRUD:
   case IDC_VIEWWALKLRFB:
      switch (m_apRender[0]->CameraModelGet()) {
      case CAMERAMODEL_FLAT:
         {
            strcat (szHuge, "<bold>Flattened view settings</bold><p/>");
            CPoint pCameraCenter;
            fp fLong, fTilt, fLat, fScale, fTransX, fTransY;

            m_apRender[0]->CameraFlatGet (&pCameraCenter, &fLong, &fTilt, &fLat, &fScale, &fTransX, &fTransY);

            strcat (szHuge, "<table width=100%% " TTTMARGIN ">");

            strcat (szHuge, "<tr><td>Cent X</td><td><bold>");
            MeasureToString (-fTransX, szHuge + strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");
            strcat (szHuge, "<tr><td>Cent X</td><td><bold>");
            MeasureToString (-fTransY, szHuge + strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");
            strcat (szHuge, "<tr><td>Width</td><td><bold>");
            MeasureToString (fScale, szHuge + strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");

            strcat (szHuge, "<tr><td>X rotation</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fTilt, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");
            strcat (szHuge, "<tr><td>Y rotation</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fLat, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");
            strcat (szHuge, "<tr><td>Z rotation</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fLong, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");

            strcat (szHuge, "<tr><td>Rotation center</td><td><bold>");
            MeasureToString (pCameraCenter.p[0], szHuge+strlen(szHuge));
            strcat (szHuge, ", ");
            MeasureToString (pCameraCenter.p[1], szHuge+strlen(szHuge));
            strcat (szHuge, ", ");
            MeasureToString (pCameraCenter.p[2], szHuge+strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");
            
            strcat (szHuge, "</table>");
         }
         break;
      case CAMERAMODEL_PERSPOBJECT:
         {
            strcat (szHuge, "<bold>Model view settings</bold><p/>");
            CPoint pCameraCenter, pV3D;
            fp fX, fY, fZ, fFOV;

            m_apRender[0]->CameraPerspObjectGet (&pV3D, &pCameraCenter, &fZ, &fX, &fY, &fFOV);

            strcat (szHuge, "<table width=100%% " TTTMARGIN ">");

            strcat (szHuge, "<tr><td>Cent X</td><td><bold>");
            MeasureToString (pV3D.p[0], szHuge + strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");
            strcat (szHuge, "<tr><td>Cent Y</td><td><bold>");
            MeasureToString (pV3D.p[1], szHuge + strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");
            strcat (szHuge, "<tr><td>Cent X</td><td><bold>");
            MeasureToString (pV3D.p[2], szHuge + strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");

            strcat (szHuge, "<tr><td>X rotation</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fX, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");
            strcat (szHuge, "<tr><td>Y rotation</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fY, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");
            strcat (szHuge, "<tr><td>Z rotation</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fZ, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");

            strcat (szHuge, "<tr><td>Rotation center</td><td><bold>");
            MeasureToString (pCameraCenter.p[0], szHuge+strlen(szHuge));
            strcat (szHuge, ", ");
            MeasureToString (pCameraCenter.p[1], szHuge+strlen(szHuge));
            strcat (szHuge, ", ");
            MeasureToString (pCameraCenter.p[2], szHuge+strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");
            
            strcat (szHuge, "</table>");
         }
         break;
      case CAMERAMODEL_PERSPWALKTHROUGH:
         {
            strcat (szHuge, "<bold>Walkthrough view settings</bold><p/>");
            CPoint pV3D;
            fp fLong, fLat, fTilt, fFOV;

            m_apRender[0]->CameraPerspWalkthroughGet(&pV3D, &fLong, &fLat, &fTilt, &fFOV);

            strcat (szHuge, "<table width=100%% " TTTMARGIN ">");

            strcat (szHuge, "<tr><td>EW</td><td><bold>");
            MeasureToString (pV3D.p[0], szHuge + strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");
            strcat (szHuge, "<tr><td>NS</td><td><bold>");
            MeasureToString (pV3D.p[1], szHuge + strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");
            strcat (szHuge, "<tr><td>UD</td><td><bold>");
            MeasureToString (pV3D.p[2], szHuge + strlen(szHuge));
            strcat (szHuge, "</bold></td></tr>");

            strcat (szHuge, "<tr><td>Long</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fLong, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");
            strcat (szHuge, "<tr><td>Lat</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fLat, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");
            strcat (szHuge, "<tr><td>Tilt</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fTilt, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");

            strcat (szHuge, "</table>");
         }
         break;
      }  // switch on camera model
      break;


   case IDC_VIEWFLATLLOOKAT:
   case IDC_CLIPLINE:
   case IDC_CLIPPOINT:
   case IDC_SELINDIVIDUAL:
   case IDC_SELREGION:
      // do nothing for these
      break;

   case IDC_MOVENSEWUD:
   case IDC_MOVENSEW:
   case IDC_MOVEEMBED:
   case IDC_MOVEUD:
      {
         // if there aren't any objects selected then empty
         DWORD dwNum, *pdw;
         pdw = m_pWorld->SelectionEnum(&dwNum);
         if (!dwNum)
            break;

         strcat (szHuge, "<bold>Move object</bold><p/>");
         CMatrix m;
         CPoint pRef;
         MoveReferencePointQuery(MoveReferenceCurQuery(), &pRef);
         pRef.p[3] = 1;
         MoveReferenceMatrixGet (&m);

         // convert to world space
         pRef.MultiplyLeft (&m);

         // convert to grid space
         pRef.MultiplyLeft (&m_mWorldToGrid);

         // display
         strcat (szHuge, "<table width=100%% " TTTMARGIN ">");

         strcat (szHuge, "<tr><td>EW (grid)</td><td><bold>");
         MeasureToString (pRef.p[0], szHuge + strlen(szHuge));
         strcat (szHuge, "</bold></td></tr>");
         strcat (szHuge, "<tr><td>NS (grid)</td><td><bold>");
         MeasureToString (pRef.p[1], szHuge + strlen(szHuge));
         strcat (szHuge, "</bold></td></tr>");
         strcat (szHuge, "<tr><td>UD (grid)</td><td><bold>");
         MeasureToString (pRef.p[2], szHuge + strlen(szHuge));
         strcat (szHuge, "</bold></td></tr>");

         // reference
         strcat (szHuge, "<tr><td>Reference</td><td><bold>");
         WCHAR szw[256];
         MoveReferenceStringQuery (MoveReferenceCurQuery(), szw, sizeof(szw), NULL);
         MemZero(&gMemTemp);
         MemCatSanitize (&gMemTemp, szw);
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)gMemTemp.p,-1, szHuge+strlen(szHuge),256,0,0);
         strcat (szHuge, "</bold></td></tr>");
         strcat (szHuge, "</table>");
      }
      break;

   case IDC_MOVEROTZ:
   case IDC_MOVEROTY:
      {
         // if there aren't any objects selected then empty
         DWORD dwNum, *pdw;
         pdw = m_pWorld->SelectionEnum(&dwNum);
         if (!dwNum)
            break;

         strcat (szHuge, "<bold>Rotate object</bold><p/>");

         if (IsEmbeddedObjectSelected() == 1) {
            // embedded object that need to show just the angle
            // get this object and find its position on the surface
            DWORD dwNum, *pdw;
            pdw = m_pWorld->SelectionEnum(&dwNum);
            if (dwNum != 1)
               break;   // shouldn't happen
            PCObjectSocket pEmbed, pContain;
            pEmbed = m_pWorld->ObjectGet (pdw[0]);
            if (!pEmbed)
               break;   // shouldnt happen
            GUID gContain, gEmbed;
            pEmbed->GUIDGet (&gEmbed);
            if (!pEmbed->EmbedContainerGet (&gContain))
               break;   // shouldn't happen
            pContain = m_pWorld->ObjectGet (m_pWorld->ObjectFind(&gContain));
            if (!pContain)
               break;   // shouldn't happen
            
            TEXTUREPOINT tp;
            fp fRotation;
            DWORD dwSurface;
            if (!pContain->ContEmbeddedLocationGet (&gEmbed, &tp,
               &fRotation, &dwSurface))
               break;   // shouldn't happen

            strcat (szHuge, "<table width=100%% " TTTMARGIN ">");

            strcat (szHuge, "<tr><td>Angle</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fRotation, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");
            strcat (szHuge, "</table>");
         }
         else {
            // non-embedded

            CMatrix m;
            CPoint pRef;
            fp fX, fY, fZ;
            MoveReferenceMatrixGet (&m);
            m.ToXYZLLT (&pRef, &fZ, &fX, &fY);

            // display
            strcat (szHuge, "<table width=100%% " TTTMARGIN ">");
            strcat (szHuge, "<tr><td>X rotation</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fX, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");
            strcat (szHuge, "<tr><td>Y rotation</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fY, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");
            strcat (szHuge, "<tr><td>Z rotation</td><td><bold>");
            AngleToString (szHuge+strlen(szHuge), fZ, TRUE);
            strcat (szHuge, " deg</bold></td></tr>");

            // reference
            strcat (szHuge, "<tr><td>Reference</td><td><bold>");
            WCHAR szw[256];
            MoveReferenceStringQuery (MoveReferenceCurQuery(), szw, sizeof(szw), NULL);
            MemZero(&gMemTemp);
            MemCatSanitize (&gMemTemp, szw);
            WideCharToMultiByte (CP_ACP, 0, (PWSTR)gMemTemp.p,-1, szHuge+strlen(szHuge),256,0,0);
            strcat (szHuge, "</bold></td></tr>");
            strcat (szHuge, "</table>");
         }
      }
      break;

   case IDC_POSITIONPASTE:
      strcat (szHuge, "Click on the location where you want to paste the object.");
      if (m_fPastingEmbedded)
         strcat (szHuge, "<p/>If you don't want to embed the new object in whatever "
            "you click on then hold down the <bold>control</bold> key when you click.");
      break;

   case IDC_THUMBNAIL:
      strcat (szHuge, "Click on the object you want to look at. The thumnails will "
         "automatically be set to look at the object from above, the right, in front, "
         "and a perspective view.");
      break;

   case IDC_OBJECTPASTE:
   case IDC_OBJECTPASTEDRAG:
      strcat (szHuge, "<bold>Add ");
      MemZero(&gMemTemp);
      MemCatSanitize (&gMemTemp, m_szObjName);
      WideCharToMultiByte (CP_ACP, 0, (PWSTR) gMemTemp.p, -1,
         szHuge+strlen(szHuge),128, 0, 0);
      strcat (szHuge, (m_adwPointerMode[dwButton] == IDC_OBJECTPASTEDRAG) ?
         "<p/>Click and drag</bold> where you want the object." :
         "<p/>Click</bold> on the location where you want to add the object.");
      if (m_fPastingEmbedded)
         strcat (szHuge, "<p/>If you don't want to embed the new object in whatever "
            "you click on then hold down the <bold>control</bold> key when you click.");
      break;

   case IDC_BRUSH4:
   case IDC_BRUSH8:
   case IDC_BRUSH16:
   case IDC_BRUSH32:
   case IDC_BRUSH64:
      strcat (szHuge, "Click and drag to paint using the selected brush.");
      break;

   case IDC_PMTAILORNEW:
      strcat (szHuge, "Click and drag on the polygon mesh to expand the selected clothing surface so it "
         "reflects the bagginess of the clothing, and no longer touches the skin.");
      break;

   case IDC_PMMOVEANYDIR:
   case IDC_PMMOVEINOUT:
   case IDC_PMMOVEPERP:
   case IDC_PMMOVEPERPIND:
   case IDC_PMMOVESCALE:
   case IDC_PMMOVEROTPERP:
   case IDC_PMMOVEROTNONPERP:
   case IDC_PMSIDEINSET:
   case IDC_PMSIDEEXTRUDE:
   case IDC_PMBEVEL:
   case IDC_PMTEXTSCALE:
   case IDC_PMTEXTMOVE:
   case IDC_PMTEXTROT:
      strcat (szHuge, "Click and drag on the polygon mesh to move the selected vertecies, edges, or sides.");
      break;

   case IDC_BONESCALE:
      strcat (szHuge, "Click and drag on the bone to scale it.");
      break;

   case IDC_BONEROTATE:
      strcat (szHuge, "Click and drag on the bone to rotate it.");
      break;

   case IDC_BONENEW:
      strcat (szHuge, "Click and drag on the end of a bone to create a new one.");
      break;

   case IDC_PMMAGANY:
   case IDC_PMMAGNORM:
   case IDC_PMMAGVIEWER:
   case IDC_PMMAGPINCH:
      strcat (szHuge, "Click and drag on the object to use the magnet tool.");
      break;

   case IDC_PMORGSCALE:
   case IDC_PMMORPHSCALE:
      strcat (szHuge, "Click and drag on the object to scale the object.");
      break;

   case IDC_PMSIDESPLIT:
      strcat (szHuge, "Click on on a vertex or side and drag to another vertex or side.");
      break;

   case IDC_PMNEWSIDE:
      strcat (szHuge, "Click on three or more points to create a polygon. "
         "To finish the polygon, click on the starting point.");
      break;

   case IDC_PMCOLLAPSE:
   case IDC_PMTEXTCOLLAPSE:
      strcat (szHuge, "Click on the vertex, edge, or side you wish to collapse.");
      break;

   case IDC_PMSIDEDISCONNECT:
      strcat (szHuge, "Click on the side(s) you wish to disconnect.");
      break;

   case IDC_PMEDGESPLIT:
      strcat (szHuge, "Click on the edge you wish to split.");
      break;

   case IDC_BONESPLIT:
      strcat (szHuge, "Click on the bone you wish to split.");
      break;

   case IDC_BONEDELETE:
      strcat (szHuge, "Click on the bone you wish to delete.");
      break;

   case IDC_BONEDISCONNECT:
      strcat (szHuge, "Click on the bone you wish to disconnect.");
      break;

   case IDC_PMDELETE:
      strcat (szHuge, "Click on the vertex, edge, or side you wish to collapse.");
      break;

   case IDC_OBJCONTROLUD:
   case IDC_OBJCONTROLNSEW:
   case IDC_OBJCONTROLINOUT:
   case IDC_OBJCONTROLNSEWUD:
      {
         // get the object and the point
         if (m_dwObjControlObject == (DWORD)-1)
            break;
         PCObjectSocket pos = m_pWorld->ObjectGet (m_dwObjControlObject);

         if (!pos)
            break;

         // point
         OSCONTROL osc;
         if (!pos->ControlPointQuery (m_dwObjControlID, &osc))
            break;

         // write the name and stuff
         strcat (szHuge, "<bold>");
         if (osc.szName[0]) {
            MemZero(&gMemTemp);
            MemCatSanitize (&gMemTemp, osc.szName);
            WideCharToMultiByte (CP_ACP,0,(PWSTR)gMemTemp.p, -1,
               szHuge + strlen(szHuge), 500, 0,0);
         }
         else
            strcat (szHuge+strlen(szHuge), "Moving control point");
         strcat (szHuge, "</bold><p/>");

         strcat (szHuge, "<table width=100%% " TTTMARGIN ">");
         if (osc.szMeasurement[0]) {
            strcat (szHuge, "<tr><td><bold>");
            MemZero(&gMemTemp);
            MemCatSanitize (&gMemTemp, osc.szMeasurement);
            WideCharToMultiByte (CP_ACP,0, (PWSTR) gMemTemp.p, -1,
               szHuge + strlen(szHuge), 500, 0,0);
            strcat (szHuge+strlen(szHuge), "\r\n");
            strcat (szHuge, "</bold></td></tr>");
         }

         // coordinates
         CMatrix m;
         pos->ObjectMatrixGet (&m);
         osc.pLocation.p[3] = 1;
         osc.pLocation.MultiplyLeft (&m); // convert to world space
         osc.pLocation.MultiplyLeft (&m_mWorldToGrid);
         strcat (szHuge, "<tr><td>EW (grid)</td><td><bold>");
         MeasureToString (osc.pLocation.p[0], szHuge + strlen(szHuge));
         strcat (szHuge, "</bold></td></tr>");
         strcat (szHuge, "<tr><td>NS (grid)</td><td><bold>");
         MeasureToString (osc.pLocation.p[1], szHuge + strlen(szHuge));
         strcat (szHuge, "</bold></td></tr>");
         strcat (szHuge, "<tr><td>UD (grid)</td><td><bold>");
         MeasureToString (osc.pLocation.p[2], szHuge + strlen(szHuge));
         strcat (szHuge, "</bold></td></tr>");
         strcat (szHuge, "</table>");
      }
      break;

   case IDC_OBJATTRIB:
      strcat (szHuge, "Click on the object whose attributes you wish to change.");
      break;

   case IDC_OBJDIALOG:
      strcat (szHuge, "Click on the object whose settings you wish to change.");
      break;

   case IDC_OBJSHOWEDITOR:
      strcat (szHuge, "Click on the object whose editor you wish to pull up. "
         "Not all objects will support their own editors.");
      break;

   case IDC_PMMOVEDIALOG:
   case IDC_PMTEXTMOVEDIALOG:
      strcat (szHuge, "Click on the vertex whose location you wish to change.");
      break;

   case IDC_BONEEDIT:
      strcat (szHuge, "Click on the bone you wish to edit.");
      break;

   case IDC_OBJCONTROLDIALOG:
      strcat (szHuge, "Click on the object whose control points you wish to change.");
      break;

   case IDC_OBJINTELADJUST:
      strcat (szHuge, "Click on an object to have it adjust to its surroundings. "
         "For example: Clicking on a wall will trim it's top so it doesn't stick "
         "above the roof line.");
      break;

   case IDC_OBJDECONSTRUCT:
      strcat (szHuge, "Click on an object to split it into the pieces that make it up. "
         "Caution: Once you split an object up you cannot put it together. Only "
         "split an object up when you want more control that you're currently "
         "getting.");
      break;

   case IDC_OBJMERGE:
      strcat (szHuge, "Click on the object you wish to merge the selected object to.");
      break;

   case IDC_PMMERGE:
      strcat (szHuge, "Click on the object you wish to merge into the current polygon mesh.");
      break;

   case IDC_BONEMERGE:
      strcat (szHuge, "Click on the object you wish to merge into the current bone.");
      break;

   case IDC_OBJATTACH:
      strcat (szHuge, "Click on the object you wish to attach the selected object to.");
      break;


   case IDC_OBJPAINT:
      {
         // get the object and the point
         if (m_dwObjControlObject == (DWORD)-1)
            break;
         PCObjectSocket pos = m_pWorld->ObjectGet (m_dwObjControlObject);

         if (!pos)
            break;

         PCObjectSurface ps;
         ps = pos->SurfaceGet(m_dwObjControlID);
         if (!ps)
            break;

         strcat (szHuge, "<table width=100%% " TTTMARGIN ">");
         strcat (szHuge, "<tr><td>Based on scheme</td><td><bold>");
         if (ps->m_szScheme[0]) {
            MemZero(&gMemTemp);
            MemCatSanitize (&gMemTemp, ps->m_szScheme);
            WideCharToMultiByte (CP_ACP, 0, (PWSTR) gMemTemp.p, -1,
               szHuge+strlen(szHuge), 100, 0, 0);

            // get the scheme
            PCObjectSurface p2;
            p2 = (m_pWorld->SurfaceSchemeGet())->SurfaceGet (ps->m_szScheme, NULL, FALSE);
            if (p2) {
               delete ps;
               ps = p2;
            }
         }
         else
            strcat (szHuge+strlen(szHuge), "<italic>No scheme</italic>");
         strcat (szHuge, "</bold></td></tr>");


         if (!ps->m_fUseTextureMap) {
            sprintf (szHuge+strlen(szHuge), 
               "<tr><td>Red</td><td><bold>%d</bold></td></tr>"
               "<tr><td>Green</td><td><bold>%d</bold></td></tr>"
               "<tr><td>Blue</td><td><bold>%d</bold></td></tr>",
               (int) GetRValue(ps->m_cColor),(int) GetGValue(ps->m_cColor),(int) GetBValue(ps->m_cColor));
         }
         else {
            WCHAR szName[128];
            if (TextureNameFromGUIDs(DEFAULTRENDERSHARD, &ps->m_gTextureCode, &ps->m_gTextureSub, NULL, NULL, szName)) {
               strcat (szHuge, "<tr><td>Texture</td><td><bold>");
               MemZero(&gMemTemp);
               MemCatSanitize (&gMemTemp, szName);
               WideCharToMultiByte (CP_ACP,0, (PWSTR)gMemTemp.p, -1, szHuge+strlen(szHuge),100,0,0);
               strcat (szHuge, "</bold></td></tr>");
            }
         }
         strcat (szHuge, "</table>");


         delete ps;
      }
      break;
   }

   UpdateToolTip (szHuge);
}


/**********************************************************************************
CHouseView::IncludeEmbededObjects - This takes a pointer to a list of DWORDs, which
represent the index into the world for object numbers. It creates a fixed list of
DWORDs and adds all these to it. It then goes through all of these objects and
sees if they contain any objects. if they do, these objects are also added to
the list, (unless they're already selected), and their children. etc.

inputs
   DWORD       *padw - Pointer to an array of DWORDs for the object index in m_pWorld
   DWORD       dwNum - Number of objects
returns
   PCListFixed - List containing contents of padw and more. This MUST be freed
               by the caller
*/
PCListFixed CHouseView::IncludeEmbeddedObjects (DWORD *padw, DWORD dwNum)
{
   PCListFixed pl;
   pl = new CListFixed;
   if (!pl)
      return NULL;
   pl->Init (sizeof(DWORD), padw, dwNum);

   // loop
   DWORD i, j, k;
   for (i = 0; i < pl->Num(); i++) {
      DWORD dwObject = *((DWORD*) pl->Get(i));
      PCObjectSocket pos = m_pWorld->ObjectGet (dwObject);
      if (!pos)
         continue;

      // loop through all embedded objects
      for (j = 0; j < pos->ContEmbeddedNum(); j++) {
         GUID  gEmbed;
         if (!pos->ContEmbeddedEnum(j, &gEmbed))
            continue;

         DWORD dwEmbed;
         dwEmbed = m_pWorld->ObjectFind (&gEmbed);
         if (dwEmbed == (DWORD)-1)
            continue;

         // make sure this isn't already on the list
         for (k = 0; k < pl->Num(); k++)
            if (dwEmbed == *((DWORD*)pl->Get(k)))
               break;
         if (k >= pl->Num())
            pl->Add (&dwEmbed);
      }
   }

   return pl;
}


/**********************************************************************************
CHouseView::ObjectMerge - Merges together the objects in the selection - or at least
gives them a change.

inputs
   DWORD    dwMergeTo - Merges all the objects to this one

returns
   BOOL - TRUE if OK. FALSE if error
*/
BOOL CHouseView::ObjectMerge (DWORD dwMergeTo)
{
   // if no selection then error
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum(&dwNum);
   if (dwNum < 1) {
      EscMessageBox (m_hWnd, ASPString(),
         L"You must select at least one object to do a merge.",
         NULL,
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
      }

   // create a list of the guids for these
   GUID g;
   CListFixed lGUID;
   lGUID.Init (sizeof(GUID));
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      PCObjectSocket pos = m_pWorld->ObjectGet (pdw[i]);
      if (pdw[i] == dwMergeTo)
         continue;   // dont merge with itself
      if (!pos)
         continue;
      pos->GUIDGet (&g);
      lGUID.Add (&g);
   }

   // set undo state
   m_pWorld->UndoRemember ();

   // Call into each and tell to merge
   GUID *pag;
   BOOL fRet;
   pag = (GUID*) lGUID.Get(0);
   dwNum = lGUID.Num();
   fRet = FALSE;
   PCObjectSocket pos;
   pos = m_pWorld->ObjectGet (dwMergeTo);
   if (pos) {
      fRet |= pos->Merge (pag, dwNum);
   }

   if (!fRet)
      EscMessageBox (m_hWnd, ASPString(),
         L"None of the objects merged.",
         L"Only certain objects will merge with one another; see the documentation.",
         MB_ICONINFORMATION | MB_OK);
   else
      EscChime (ESCCHIME_INFORMATION);

   // set undo state
   m_pWorld->UndoRemember ();

   return TRUE;
}


/**********************************************************************************
CHouseView::ObjectAttach - Attachs together the objects in the selection - or at least
gives them a change.

inputs
   DWORD    dwAttachTo - Attachs all the objects to this one
   DWORD    dwX, dwY - XY locations

returns
   BOOL - TRUE if OK. FALSE if error
*/
BOOL CHouseView::ObjectAttach (DWORD dwAttachTo, DWORD dwX, DWORD dwY)
{
   // if no selection then error
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum(&dwNum);
   if (dwNum < 1) {
      EscMessageBox (m_hWnd, ASPString(),
         L"You must select at least one object to do an attach.",
         NULL,
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
      }

   PCObjectSocket pWith;
   pWith = m_pWorld->ObjectGet (dwAttachTo);
   if (!pWith)
      return FALSE;

   // where clicked on in object space
   fp fZ;
   CPoint pClick;
   CMatrix m, mInv;
   WCHAR szBone[64];
   fZ = m_apRender[0]->PixelToZ (dwX, dwY);
   m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pClick);
   pWith->ObjectMatrixGet (&m);
   m.Invert4 (&mInv);
   pClick.p[3] = 1;
   pClick.MultiplyLeft (&mInv);
   pWith->AttachClosest (&pClick, szBone);


   // set undo state
   m_pWorld->UndoRemember ();

   // loop through the objects
   GUID g, g2;
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      PCObjectSocket pos = m_pWorld->ObjectGet (pdw[i]);
      if (pdw[i] == dwAttachTo)
         continue;   // dont Attach with itself
      if (!pos)
         continue;
      pos->GUIDGet (&g);

      // see if it's already attached
      for (j = 0; j < pWith->AttachNum(); j++) {
         pWith->AttachEnum (j, &g2);
         if (IsEqualGUID (g, g2))
            break;   // already attached
      }

      // ask user if already attached
      if (j < pWith->AttachNum()) {
         int iRet;
         iRet = EscMessageBox (m_hWnd, ASPString(),
            L"The object is already attached. Do you wish to detach it?",
            L"Pressing \"Yes\" will detach the object. \"No\" will create a second "
            L"attachment (which can be useful for animations).",
            MB_ICONQUESTION | MB_YESNOCANCEL);
         if (iRet == IDYES) {
            for (j = pWith->AttachNum()-1; j < pWith->AttachNum(); j--) {
               pWith->AttachEnum (j, &g2);
               if (IsEqualGUID (g, g2))
                  pWith->AttachRemove (j);   // already attached
            }
            continue;
         }
         else if (iRet != IDNO)
            continue;   // pressed cancel
      }

      // else, attach
      pos->ObjectMatrixGet (&m);
      pWith->AttachAdd (&g, szBone, &m);
   }
   EscChime (ESCCHIME_INFORMATION);

   // set undo state
   m_pWorld->UndoRemember ();

   return TRUE;
}

/**********************************************************************************
CHouseView::ClipboardCopy - Copies the selection to the clipboard.

returns
   BOOL - TRUE if OK. FALSE if error
*/
BOOL CHouseView::ClipboardCopy (void)
{
   // if no selection then error
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum(&dwNum);
   if (!dwNum)
      return FALSE;

   // if any objects contain other objects make sure to include those
   PCListFixed pl;
   pl = IncludeEmbeddedObjects (pdw, dwNum);
   if (!pl)
      return FALSE;
   pdw = (DWORD*) pl->Get(0);
   dwNum = pl->Num();

   // convert to MML text
   PCMMLNode2 pNode;
   pNode = MMLFromObjects (m_pWorld, pdw, dwNum);
   delete pl;

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
CHouseView::ClipboardCut - Copies the current selection to the clipboard and
then deletes it.

returns
   BOOL - TRUE if OK. FALSE if error
*/
BOOL CHouseView::ClipboardCut (void)
{
   if (!ClipboardCopy())
      return FALSE;

   return ClipboardDelete (TRUE);
}

/**********************************************************************************
CHouseView::ClipboardDelete - Deletes the current selection. It doesn't deal
directly with the clipboard but it is used by the clipboard functions.

inputs
   BOOL     fUndoSnapshot - If TRUE, then marks an undo snapshot after the delete.
               If FALSE, it's left up to the caller (such as used by ClipboardPaste())
returns
   BOOL - TRUE if successful
*/
BOOL CHouseView::ClipboardDelete (BOOL fUndoSnapshot)
{
   // if no selection then error
   DWORD dwNum, *pdw;
   pdw = m_pWorld->SelectionEnum(&dwNum);
   if (!dwNum)
      return TRUE;

   // if any objects contain other objects make sure to include those
   PCListFixed pl;
   pl = IncludeEmbeddedObjects (pdw, dwNum);
   if (!pl)
      return FALSE;

   pdw = (DWORD*) pl->Get(0);
   dwNum = pl->Num();

   // convert to guids to can delete safely
   CListFixed lg;
   DWORD i;
   GUID gTemp;
   lg.Init (sizeof(GUID));
   for (i = 0; i < dwNum; i++) {
      PCObjectSocket pos = m_pWorld->ObjectGet (pdw[i]);
      if (pos) {
         pos->GUIDGet (&gTemp);
         lg.Add (&gTemp);
      }
   }

   // delete everything in the selection
   GUID *pg;
   for (i = 0; i < lg.Num(); i++) {
      pg = (GUID*) lg.Get(i);
      DWORD dwIndex;
      dwIndex = m_pWorld->ObjectFind (pg);
      if (dwIndex == (DWORD)-1)
         continue;
      m_pWorld->ObjectRemove (dwIndex);
   }

   if (fUndoSnapshot)
      m_pWorld->UndoRemember();

   delete pl;

   return FALSE;
}

/**********************************************************************************
CHouseView::ClipboardPaste - Opens the clipboard and pastes the current ASP
data into the world.

inputs
   none
returns
   BOOL - TRUE if successful
*/
BOOL CHouseView::ClipboardPaste (void)
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
   m_Purgatory.Clear();

   // set the floor levels in purgatory
   fp afElev[NUMLEVELS], fHigher;
   GlobalFloorLevelsGet (m_pWorld, NULL, afElev, &fHigher);
   GlobalFloorLevelsSet (&m_Purgatory, NULL, afElev, fHigher);

   fRet = MMLToObjects (&m_Purgatory, pNode, TRUE);
   delete pNode;

   CloseClipboard ();

   // set the pointer mode to placing the pasted object
   m_fPastingEmbedded = FALSE;
   if (m_Purgatory.ObjectNum() == 1) {
      PCObjectSocket pos = m_Purgatory.ObjectGet(0);
      if (!pos)
         return FALSE;
      m_fPastingEmbedded = pos->EmbedQueryCanEmbed();
   }

   if (fRet)
      SetPointerMode (IDC_POSITIONPASTE, 0);
   else {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
   }

   return TRUE;
}

/**********************************************************************************
CHouseView::ClipboardUpdatePasteButton - Sets the m_pPaste button to enalbed/disabled
depending on whether the data in the clipboard is valid.
*/
void CHouseView::ClipboardUpdatePasteButton (void)
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
CHouseView::SnapEmbedToGrid - Given a PCObjectSocket of a container, and an HV location
in the container, this will modify the HV so that it's close to the grid location
supplied in the world.

inputs
   PCObjectSocket    pCont - Container
   DWORD             dwSurf - Surface in the container
   PTTEXTUREPOINT    pHV - HV that's used, and modified
returns
   none
*/
void CHouseView::SnapEmbedToGrid (PCObjectSocket pCont, DWORD dwSurf, PTEXTUREPOINT pHV)
{
   // NOTE - May need to iterate twice on curved surfaces? Doesn't seem to be any point
   // since on a curved surface there really isn't much of a grid to snape to,
   // and besides, isnn't that innacurate on curved unless very curved

   // NTOE - May be problem if wall at angle to grid - Yes, it happens, but then
   // again, if the grid doesn't align up perfectly not really sure what the user
   // wants

   // get the orientation of teh surface at this point
   TEXTUREPOINT tp;
   CPoint   pC, pR, pU, pRN, pUN;
   CMatrix  m;
   double   fLenR, fLenU;

   //center
   tp = *pHV;
   if (!pCont->ContMatrixFromHV (dwSurf, &tp, 0, &m))
      return;
   pC.Zero();
   pC.p[3] = 1;
   pC.MultiplyLeft (&m);

   // right
   tp = *pHV;
   tp.h += HVDELTA;
   if (tp.h > 1.0)
      tp.h -= 2*HVDELTA;
   if (!pCont->ContMatrixFromHV (dwSurf, &tp, 0, &m))
      return;
   pR.Zero();
   pR.p[3] = 1;
   pR.MultiplyLeft (&m);
   pR.Subtract (&pC);
   if (tp.h < pHV->h)
      pR.Scale (-1);
   pRN.Copy (&pR);
   pRN.Normalize();
   fLenR = pR.Length();

   // up
   tp = *pHV;
   tp.v -= HVDELTA;
   if (tp.v < 0)
      tp.v += 2*HVDELTA;
   if (!pCont->ContMatrixFromHV (dwSurf, &tp, 0, &m))
      return;
   pU.Zero();
   pU.p[3] = 1;
   pU.MultiplyLeft (&m);
   pU.Subtract (&pC);
   if (tp.v > pHV->v)
      pU.Scale (-1);
   pUN.Copy (&pU);
   pUN.Normalize();
   fLenU = pU.Length();

   // apply pC to the grid...
   CPoint pGrid;
   pGrid.Copy (&pC);
   GridMove (&pGrid);
   
   // assume pR and pU are reasonably perpendicular, which they should be
   pGrid.Subtract (&pC);
   double fRight, fUp;
   fRight = pGrid.DotProd (&pRN);
   if (fLenR > EPSILON)
      fRight = fRight / fLenR * HVDELTA;
   else
      fRight = 0; // cant calculate
   fUp = pGrid.DotProd (&pUN);
   if (fLenU > EPSILON)
      fUp = fUp / fLenU * HVDELTA;
   else
      fUp = 0; // cant calculate


   pHV->h += fRight;
   pHV->v -= fUp;
   pHV->h = min(pHV->h, 1);
   pHV->h = max(pHV->h, 0);
   pHV->v = min(pHV->v, 1);
   pHV->v = max(pHV->v, 0);

   // done
}

/**********************************************************************************
CHouseView::MoveOutOfPurgatory - The user has clicked on a location in space.
Use this to determine what they want and see about moving the objects in purgatory
into the real world using this.

inputs
   DWORD       dwX, dwY - position in the CImage that was clicked on. The points
               are bounded within the image.
   BOOL        fCanEmbed - If this is FALSE, then won't embed purgatory object
               into clicked on, no matter what. If TRUE, will ebmed.
   PCPoint     papDrag - If not NULL, the user has dragged a line. This is the
                  set of points (in world space) for the drag, and will use IntelligentPosnDrag()
   DWORD       dwDrag - Number of points in papDrag. If not use will use IntelligentPosnDrag()
returns
   none
*/
void CHouseView::MoveOutOfPurgatory (DWORD dwX, DWORD dwY, BOOL fCanEmbed, PCPoint papDrag, DWORD dwDrag)
{
   OSINTELLIPOS ip;
   memset (&ip, 0, sizeof(ip));

   // get the object and surface that clicked on
   PIMAGEPIXEL pip;
   pip = m_aImage[0].Pixel (dwX, dwY);
   if (!pip)
      return;  // error
   DWORD dwObject;
   dwObject = (DWORD)HIWORD(pip->dwID);
   PCObjectSocket pos, posClick;
   posClick = NULL;
   if (dwObject)
      posClick = m_pWorld->ObjectGet (dwObject-1);
   if (posClick) {
      ip.fClickedOnObject = TRUE;
      posClick->GUIDGet (&ip.gClickObject);
      ip.dwClickSurface = (DWORD)LOWORD(pip->dwID);
   }


   ip.fMoveOnly = TRUE;
   ip.pWorld = m_pWorld;
   switch (m_apRender[0]->CameraModelGet()) {
   case CAMERAMODEL_PERSPWALKTHROUGH:
      {
         CPoint p;
         fp fLong, fLat, fTilt, f2;
         m_apRender[0]->CameraPerspWalkthroughGet (&p, &fLong, &fLat, &fTilt, &f2);
         ip.fViewFlat = FALSE;
         ip.fFOV = f2;
      }
      break;
   case CAMERAMODEL_PERSPOBJECT:
      {
         CPoint p, pCenter;
         fp fZ, fX, fY, f2;
         m_apRender[0]->CameraPerspObjectGet (&p, &pCenter, &fZ, &fX, &fY, &f2);
         ip.fViewFlat = FALSE;
         ip.fFOV = f2;
      }
      break;
   case CAMERAMODEL_FLAT:
      {
         fp fRotZ, fRotX, fRotY, fScale, fTransX, fTransY;
         CPoint pCenter;
         m_apRender[0]->CameraFlatGet (&pCenter, &fRotZ, &fRotX, &fRotY, &fScale, &fTransX, &fTransY);
         ip.fViewFlat = TRUE;
         ip.fWidth = fScale;
      }
      break;
   default:
      // error
      return;
   }

   // find out where the camera is and where it's looking
   if (ip.fViewFlat) {
      m_apRender[0]->PixelToWorldSpace (dwX, dwY, -1000.0, &ip.pCamera);
      m_apRender[0]->PixelToWorldSpace (dwX, dwY, 10.0, &ip.pLookAt);
   }
   else {
      m_apRender[0]->PixelToWorldSpace (m_aImage[0].Width() / 2.0, m_aImage[0].Height() / 2.0,
         .0001, &ip.pCamera);
      m_apRender[0]->PixelToWorldSpace (m_aImage[0].Width() / 2.0, m_aImage[0].Height() / 2.0,
         10.0, &ip.pLookAt);
   }
   ip.pLookAt.Subtract (&ip.pCamera);

   // find out the point where clicked
   fp fZ, fOldZ;
   CPoint pClick;
   fZ = m_apRender[0]->PixelToZ (dwX, dwY);
   if (fZ > 1000) {
      // clicked on nothing, so leave some points invalid
      fZ = 1000;
   }
   else {
      ip.fClickOnValid = TRUE;
      ip.fClickNormalValid = TRUE;
   }
   fOldZ = fZ;

   // find out where the pixel clicked on is in world space
   ip.pClickLineStart.Copy (&ip.pCamera);
   m_apRender[0]->PixelToWorldSpace (dwX, dwY, fOldZ, &ip.pClickLineVector);
   ip.pClickOn.Copy (&ip.pClickLineVector);
   ip.pClickLineVector.Subtract (&ip.pClickLineStart);

   GridMove (&ip.pClickOn, 0x3); // only align NS and EW, not Z

   // if we have a point then try to figure out the normal
   ip.pClickNormal.Zero();
   if (ip.fClickOnValid) {
      m_apRender[0]->PixelToWorldSpace (dwX, dwY, fOldZ = fZ, &pClick);
      m_pClipPlaneClick.Copy (&pClick);

      // get points left/right and up/down to find the slope of the line
      CPoint pLeft, pRight, pUp, pDown;
      BOOL fLeftValid, fRightValid, fUpValid, fDownValid;
      fLeftValid = fRightValid = fUpValid = fDownValid = FALSE;
      if (dwX) {
         fZ = m_apRender[0]->PixelToZ (dwX-1, dwY);
         if (fZ < 1000) {
            m_apRender[0]->PixelToWorldSpace (dwX-1, dwY, fZ, &pLeft);
            fLeftValid = TRUE;
         }
      }
      if (dwX+1 < m_aImage[0].Width()) {
         fZ = m_apRender[0]->PixelToZ (dwX+1, dwY);
         if (fZ < 1000) {
            m_apRender[0]->PixelToWorldSpace (dwX+1, dwY, fZ, &pRight);
            fRightValid = TRUE;
         }
      }
      if (dwY) {
         fZ = m_apRender[0]->PixelToZ (dwX, dwY-1);
         if (fZ < 1000) {
            m_apRender[0]->PixelToWorldSpace (dwX, dwY-1, fZ, &pUp);
            fUpValid = TRUE;
         }
      }
      if (dwY+1 < m_aImage[0].Height()) {
         fZ = m_apRender[0]->PixelToZ (dwX, dwY+1);
         if (fZ < 1000) {
            m_apRender[0]->PixelToWorldSpace (dwX, dwY+1, fZ, &pDown);
            fDownValid = TRUE;
         }
      }
      if ((fLeftValid || fRightValid) && (fUpValid || fDownValid)) {
         ip.fClickNormalValid = TRUE;
         m_pClipPlaneSurfaceH.Subtract (fRightValid ? &pRight : & pClick,
            fLeftValid ? &pLeft : &pClick);
         m_pClipPlaneSurfaceV.Subtract (fUpValid ? &pUp : &pClick,
            fDownValid ? &pDown : &pClick);
         m_pClipPlaneSurfaceH.Normalize();
         m_pClipPlaneSurfaceV.Normalize();
         ip.pClickNormal.CrossProd (&m_pClipPlaneSurfaceH, &m_pClipPlaneSurfaceV);
      }
      else {
         ip.fClickNormalValid = FALSE;
      }

   }

   // if there's only one object then see if it wwants to move itself.
   // if more than one object we'll have to do movement intelligence ourselves
   if (!m_Purgatory.ObjectNum())
      return;  // nothing to do
   CMatrix  mBound;         // bounding box, and position
   CPoint   apCorner[2];   // bounding box corners, in object space
   CPoint   pRefInObject;  // reference point (in object space)
   CPoint   pRefInWorld;   // renferece point (in world space)
   DWORD i;

   // BUGFIX - If more than one object, but it's the only non-embedded one, then
   // act as if one object. This fixes bug where copy building block with basement and
   // embedded objects. Paste back and the basement is taller.
   DWORD dwNumNon, dwNonEmbed;
   dwNumNon = 0;
   dwNonEmbed = 0;
   for (i = 0; i < m_Purgatory.ObjectNum(); i++) {
      GUID gCont;
      pos = m_Purgatory.ObjectGet(i);
      if (!pos)
         continue;
      if (pos->EmbedContainerGet (&gCont))
         continue;
      dwNumNon++;
      dwNonEmbed = i;
   }

   if ((dwNumNon == 1) || (m_Purgatory.ObjectNum() == 1)) {
      if (m_Purgatory.ObjectNum() == 1)
         dwNonEmbed = 0;   // since that means there's only one object
      pos = m_Purgatory.ObjectGet(dwNonEmbed);
      if (!pos)
         return;
      OSINTELLIPOSRESULT res;

      BOOL  fRet;
      fRet = FALSE;

      // if only one object, skip all the bits in-between
      if (m_Purgatory.ObjectNum() != 1)
         goto cantembed;

      // if can drag try that
      if (papDrag && dwDrag) {
         OSINTELLIPOSDRAG ipd;
         memset (&ipd, 0, sizeof(ipd));
         ipd.dwNumWorldCoord = dwDrag;
         ipd.fFOV = ip.fFOV;
         ipd.fViewFlat = ip.fViewFlat;
         ipd.fWidth = ip.fWidth;
         ipd.paWorldCoord = papDrag;
         ipd.pCamera.Copy (&ip.pCamera);
         ipd.pLookAt.Copy (&ip.pLookAt);
         ipd.pWorld = ip.pWorld;

         fRet = pos->IntelligentPositionDrag (m_pWorld, &ipd, &res);
      }
      if (!fRet)
         fRet = pos->IntelligentPosition (m_pWorld, &ip, &res);

      if (fRet) {
         if (res.fEmbedded) {
            // set the default matrix anyway, because embedding may not work
            pos->ObjectMatrixSet (&res.mObject);

            // transfer out of purgatory into the real world
            m_pWorld->SelectionClear();
            m_pWorld->UsurpWorld (&m_Purgatory, TRUE);

            GUID gThis;
            pos->GUIDGet (&gThis);
            PCObjectSocket pCont;
            pCont = m_pWorld->ObjectGet(m_pWorld->ObjectFind(&res.gContainer));
            if (pCont) {
               pCont->ContEmbeddedAdd (&gThis, &res.tpEmbed, res.fEmbedRotation, res.dwEmbedSurface);
            }

            // note: if this fails then do nothing
            return;
         }
         else {
            // have location
            pos->ObjectMatrixSet (&res.mObject);
            goto transferout;
         }
      }
      // IMPORTANT: This call to IntelligentPosition may not work if object decides to embed

      // if it's an embeddable object (and by iteself) then try to embed it
      // BUGFIX - If user had pressed control when clicking then wont embed
      if (fCanEmbed && pos->EmbedQueryCanEmbed() && posClick && posClick->ContQueryCanContain(ip.dwClickSurface) && ip.fClickOnValid) {
         // find out where it would go
         CPoint pEye, pClick;
         TEXTUREPOINT HV;

         GUID gContain;
         if (pos->EmbedContainerGet (&gContain))
            goto cantembed;

         // convert the eye and click from world space to object space
         CMatrix m, mInv;
         posClick->ObjectMatrixGet (&m);
         m.Invert4 (&mInv);
         mInv.Multiply (&ip.pCamera, &pEye);
         mInv.Multiply (&ip.pClickOn, &pClick);

         if (!posClick->ContHVQuery(&pEye, &pClick, ip.dwClickSurface, NULL, &HV))
            goto cantembed;

         SnapEmbedToGrid(posClick, ip.dwClickSurface, &HV);

         // have location, and can embed, so do so
         m_pWorld->SelectionClear();
         m_pWorld->UsurpWorld (&m_Purgatory, TRUE);

         GUID gEmbed;
         pos->GUIDGet (&gEmbed);
         posClick->ContEmbeddedAdd (&gEmbed, &HV, 0, ip.dwClickSurface);

         return;
      }
cantembed:

      // get the bounding box and reference point
      pos->QueryBoundingBox (&apCorner[0], &apCorner[1], (DWORD)-1);
      pos->ObjectMatrixGet (&mBound);
      pos->MoveReferencePointQuery(0, &pRefInObject); // use point 0 as the default
      pRefInObject.p[3] = 1;
   }
   else {
      // there are several objects, so need to create a bounding box based upon all
      // of them

      BOOL fCorner;
      fCorner = FALSE;
      mBound.Identity();

      // lopp through all the objects and get their corners
      CMatrix mSub;
      CPoint apSub[2];
      DWORD x,y,z;
      CPoint pt;
      for (i = 0; i < m_Purgatory.ObjectNum(); i++) {
         pos = m_Purgatory.ObjectGet(i);
         if (!pos)
            continue;

         pos->QueryBoundingBox (&apSub[0], &apSub[1], (DWORD)-1);
         pos->ObjectMatrixGet (&mSub);

         for (x =0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
            pt.p[0] = apSub[x].p[0];
            pt.p[1] = apSub[y].p[1];
            pt.p[2] = apSub[z].p[2];
            pt.p[3] = 1;
            pt.MultiplyLeft (&mSub);

            if (fCorner) {
               for (x = 0; x < 3; x++) {
                  apCorner[0].p[x] = min(apCorner[0].p[x], pt.p[x]);
                  apCorner[1].p[x] = max(apCorner[1].p[x], pt.p[x]);
               }
            }
            else {
               apCorner[0].Copy (&pt);
               apCorner[1].Copy (&pt);
               fCorner = TRUE;
            }
         }
      }

      // reference point
      pRefInObject.Add (&apCorner[0], &apCorner[1]);
      pRefInObject.Scale(.5);
      pRefInObject.p[2] = apCorner[0].p[2];  // lowest Z
      pRefInObject.p[3] = 1;

   }  // end multiple objects
   pRefInWorld.Copy (&pRefInObject);
   pRefInWorld.MultiplyLeft (&mBound);

   // how large is the object
   CPoint pLen;
   fp fLen;
   pLen.Subtract (&apCorner[0], &apCorner[1]);
   fLen = pLen.Length();

   CMatrix mTrans;
   mTrans.Identity();

   // know how big object is now and where it is, which enough ifnormation to place
   if (ip.fClickOnValid) {
      // clicked on a valid surface

      // NOTE - At some point may want to rotate the object around (assuming its
      // only one object) so that the if we click on a wall the default move surface
      // will be placed up against the wall, flush with it. IE: Put painting up and
      // it better not be sticking half in and half out of the wall

      // since know where the reference ended up, move the reference point to the
      // point where clicked
      mTrans.Translation (ip.pClickOn.p[0] - pRefInWorld.p[0], 
         ip.pClickOn.p[1] - pRefInWorld.p[1], ip.pClickOn.p[2] - pRefInWorld.p[2]);
   }
   else {
      // clicked on empty space

      if (ip.fViewFlat) {
         // move it to a base of z = 0, else go for y = 0; or x = 0
         fp fAlpha;
         CPoint pt;
         pt.Copy (&ip.pClickLineVector);
         pt.Normalize();

         if (fabs(pt.p[2]) > .01)
            fAlpha = -ip.pClickLineStart.p[2] / pt.p[2];
         else if (fabs(pt.p[1]) > .01)
            fAlpha = -ip.pClickLineStart.p[1] / pt.p[1];
         else if (fabs(pt.p[0]) > .01)
            fAlpha = -ip.pClickLineStart.p[0] / pt.p[0];
         else
            fAlpha = 0; // do nothing

         pt.Scale (fAlpha);
         pt.Add (&ip.pClickLineStart);

         // move the object so it ends up there
         mTrans.Translation (pt.p[0] - pRefInWorld.p[0], 
            pt.p[1] - pRefInWorld.p[1], pt.p[2] - pRefInWorld.p[2]);
      }
      else {   // perspective
         // how far away does it need to be put to be visible?
         fp fDist;
         fDist = fLen / tan(ip.fFOV / 2.0);

         // go the distance
         CPoint pt;
         pt.Copy (&ip.pClickLineVector);
         pt.Normalize();
         pt.Scale (fDist);
         pt.Add (&ip.pClickLineStart);

         // move it there
         mTrans.Translation (pt.p[0] - pRefInWorld.p[0], 
            pt.p[1] - pRefInWorld.p[1], pt.p[2] - pRefInWorld.p[2]);
      }

      // NOTE - At some point might find the distance that's far enough away
      // to be visible, using that X and Y, and then lower it in Z until it
      // sits on top of something. Can tell how much need to lower it by creating
      // a clipping region of very small in X and Y, but very tall
   }

   // move all the objects using mTrans
   // BUGFIX - Move the ones without attachments first...
   DWORD dwAttach, dwNumAttach;
   for (dwAttach = 0; dwAttach < 2; dwAttach++) {
      for (i = 0; i < m_Purgatory.ObjectNum(); i++) {
         pos = m_Purgatory.ObjectGet(i);
         if (!pos)
            continue;

         // so move the ones without attachments first, and then the ones with attachment
         // otherwise, when paste an object with attachments the attachments arent in the
         // right place
         dwNumAttach = pos->AttachNum();
         if (dwNumAttach && !dwAttach)
            continue;
         else if (!dwNumAttach && dwAttach)
            continue;

         CMatrix m;
         pos->ObjectMatrixGet (&m);
         m.MultiplyRight (&mTrans);
         pos->ObjectMatrixSet (&m);
      }
   }

transferout:
   // transfer out of purgatory into the real world
   m_pWorld->SelectionClear();

   m_pWorld->UsurpWorld (&m_Purgatory, TRUE);
   m_pWorld->NotifySockets (WORLDC_OBJECTADD | WORLDC_OBJECTCHANGESEL |
      WORLDC_OBJECTCHANGENON, NULL);
      // BUGFIX - Need to notify when move out of purgatory
   return;
}


/************************************************************************************
CHouseView::CameraDeleted - Called by the camera object to tell its house view
that it has been deleted.
*/
void CHouseView::CameraDeleted (void)
{
   m_pCamera = NULL;
   PostMessage (m_hWnd, WM_CLOSE, 0, 0);
}

/************************************************************************************
CHouseView::CameraMoved - Called by the camera to tell the house view that it's
been moved or rotated.

inputs
   PCMatrix    pm - new rotatin matrix
*/
void CHouseView::CameraMoved (PCMatrix pm)
{
   // if it's flat don't care
   DWORD dwModel;
   dwModel = m_apRender[0]->CameraModelGet ();
   if (dwModel == CAMERAMODEL_FLAT)
      return;  // no more to do

   // convert this
   CMatrix mInv;
   CPoint pCenter;
   fp fLong, fLat, fTilt, f2;
   fp fZ, fX, fY;
   pm->Invert4 (&mInv);
   CPoint p, p2;
   p.Zero();
   p.MultiplyLeft (pm);

   // convert this
   if (dwModel == CAMERAMODEL_PERSPWALKTHROUGH) {
      m_apRender[0]->CameraPerspWalkthroughGet (&p2, &fLong, &fLat, &fTilt, &f2);
      pm->ToXYZLLT (&p2, &fLong, &fLat, &fTilt);
      m_apRender[0]->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, f2);
      RenderUpdate (WORLDC_CAMERAMOVED, TRUE);
   }
   else {// (dwMode == CAMERAMODEL_PERSPOBJECT)
      m_apRender[0]->CameraPerspObjectGet (&p2, &pCenter, &fZ, &fX, &fY, &f2);

      // NOTE: Throwing out center of rotation info for now. I think
      // this is an acceptable thing to do. Solving the center would be
      // really nasty too.
      pCenter.Zero();

      mInv.ToXYZLLT (&p2, &fZ, &fX, &fY);

      p.Scale (-1);
      m_apRender[0]->CameraPerspObject (&p2, &pCenter, fZ, fX, fY, f2);
      RenderUpdate(WORLDC_CAMERAMOVED, TRUE);
   }

}

/************************************************************************************
CHouseView::CameraPosition - Positions the camera according to the render model.
If it doesn't exist then it's created.

NOTE: Also updates the scorllbar locations
*/
void CHouseView::CameraPosition (void)
{
   UpdateScrollBars();

   // BUGFIX - Only do camera if in world mode
   if (m_dwViewWhat != VIEWWHAT_WORLD)
      return;

   // if not there then create
   if (!m_pCamera) {
      OSINFO OI;
      memset (&OI, 0, sizeof(OI));
      OI.gCode = CLSID_Camera;
      // dont bother with OI.gSub
      OI.dwRenderShard = DEFAULTRENDERSHARD;
      m_pCamera = new CObjectCamera(0, &OI);
      if (!m_pCamera)
         return;

      // BUGFIX - Since new camera sets the name
      m_pCamera->StringSet (OSSTRING_NAME, L"View camera");
      m_pCamera->StringSet (OSSTRING_GROUP, L"Cameras");

      m_pWorld->ObjectAdd (m_pCamera, TRUE);

   }

   // temporarily disable the camera's knowlege of us so don't get callback
   m_pCamera->m_pView = NULL;

   BOOL fOldVisible, fVisible;
   DWORD dwModel;
   dwModel = m_apRender[0]->CameraModelGet ();
   fOldVisible = m_pCamera->m_fVisible;
   fVisible = (dwModel != CAMERAMODEL_FLAT);

   // if it's a flat rendermodel then hide camera and that's it
   if (fOldVisible != fVisible) {
      m_pWorld->ObjectAboutToChange (m_pCamera);
      m_pCamera->m_fVisible = fVisible;
      m_pWorld->ObjectChanged (m_pCamera);
   }
   if (dwModel == CAMERAMODEL_FLAT) {
      m_pCamera->m_pView = this;
      return;  // no more to do
   }

   // find location and orientation
   CPoint p, pCenter;
   fp fLong, fLat, fTilt, f2;
   fp fZ, fX, fY;
   CMatrix m, mInv;

   if (dwModel == CAMERAMODEL_PERSPWALKTHROUGH) {
      m_apRender[0]->CameraPerspWalkthroughGet (&p, &fLong, &fLat, &fTilt, &f2);
      //p.Scale(-1);
      m.FromXYZLLT (&p, fLong, fLat, fTilt);
      //mInv.Invert4(&m);
   }
   else {// (dwMode == CAMERAMODEL_PERSPOBJECT)
      m_apRender[0]->CameraPerspObjectGet (&p, &pCenter, &fZ, &fX, &fY, &f2);
      p.Subtract (&pCenter);
      //p.Scale(-1);
      mInv.FromXYZLLT (&p, fZ, fX, fY);
      mInv.Invert (&m); // note: Specifically NOT doing invert4

      // being cheap, using this to figure out where camera is
      DWORD dwX, dwY;
      CPoint pLF;
      dwX = m_aImage[0].Width() / 2;
      dwY = m_aImage[0].Height() / 2;
      m_apRender[0]->PixelToWorldSpace (dwX, dwY, 0, &pLF);

      CMatrix mTrans;
      mTrans.Translation (pLF.p[0], pLF.p[1], pLF.p[2]);
      m.MultiplyRight (&mTrans);
   }


   // only send if the matricies are differen
   CMatrix mObject;
   m_pCamera->ObjectMatrixGet (&mObject);
   if (!mObject.AreClose (&m))
      m_pCamera->ObjectMatrixSet (&m);

   // renable
   m_pCamera->m_pView = this;
}


/************************************************************************************
CHouseView::CameraToObject - Moves the given object so it faces the same direction
as the camre (object's 0,-1,0 points in same direction as looking) and so it's in
in the same position. Used by animation to keep animation camera in sync with what
looking at.

inputs
   GUID        *pgObject - Object to move
returns
   BOOL - TRUE if success. Fails if cant find object or if camera model is flat
*/
BOOL CHouseView::CameraToObject (GUID *pgObject)
{
   PCObjectSocket pos = m_pWorld->ObjectGet(m_pWorld->ObjectFind (pgObject));
   if (!pos)
      return FALSE;

   DWORD dwModel;
   dwModel = m_apRender[0]->CameraModelGet ();
   if (dwModel == CAMERAMODEL_FLAT)
      return FALSE;

   // find location and orientation
   CPoint p, pCenter;
   fp fLong, fLat, fTilt, f2;
   fp fZ, fX, fY;
   CMatrix m, mInv;

   if (dwModel == CAMERAMODEL_PERSPWALKTHROUGH) {
      m_apRender[0]->CameraPerspWalkthroughGet (&p, &fLong, &fLat, &fTilt, &f2);
      //p.Scale(-1);
      m.FromXYZLLT (&p, fLong, fLat, fTilt);
      //mInv.Invert4(&m);
   }
   else {// (dwMode == CAMERAMODEL_PERSPOBJECT)
      m_apRender[0]->CameraPerspObjectGet (&p, &pCenter, &fZ, &fX, &fY, &f2);
      p.Subtract (&pCenter);
      //p.Scale(-1);
      mInv.FromXYZLLT (&p, fZ, fX, fY);
      mInv.Invert (&m); // note: Specifically NOT doing invert4

      // being cheap, using this to figure out where camera is
      CPoint pLF;
      m_apRender[0]->PixelToWorldSpace ((fp) m_aImage[0].Width() / 2.0,
         (fp) m_aImage[0].Height() / 2.0, 0, &pLF);

      CMatrix mTrans;
      mTrans.Translation (pLF.p[0], pLF.p[1], pLF.p[2]);
      m.MultiplyRight (&mTrans);
   }

   // since we are looking at 0,-1,0 but the normal camera looks at 0,1,0,
   // invert y
   CMatrix mFlip;
   mFlip.Scale (-1, -1, 1);   // BUGFIX - Flip more than just y
   m.MultiplyLeft (&mFlip);

   // set it
   pos->ObjectMatrixSet (&m);

#if 0 // not sure I want to do this
   // set field of view and exsosure
   OSMANIMCAMERA oac;
   memset (&oac, 0, sizeof(oac));
   if (pos->Message (OSM_ANIMCAMERA, &oac) && oac.poac) {
      oac.poac->m_pWorld->ObjectAboutToChange (oac.poac);
      oac.poac->m_fFOV = f2;
      oac.poac->m_fExposure = log (m_apRender[0]->m_fExposure / CM_LUMENSSUN);
      oac.poac->m_pWorld->ObjectChanged (oac.poac);
      // BUGBUG - In future transfer other settings too
   }
#endif // 0

   return TRUE;
}


/************************************************************************************
CHouseView::CameraFromObject - Sets the current view camera so it's lookin from the
object's 0,0,0 point toward the object's 0,-1,0.

inputs
   GUID        gObject - Object to use
*/
void CHouseView::CameraFromObject (GUID *pgObject)
{
   // if it's flat don't care
   DWORD dwModel;
   BOOL fChangedFromFlat = FALSE;
   dwModel = m_apRender[0]->CameraModelGet ();
   if (dwModel == CAMERAMODEL_FLAT) {
      dwModel = CAMERAMODEL_PERSPWALKTHROUGH; // force a perspective camera mdel
      fChangedFromFlat = TRUE;
   }

   // get the camera
   PCObjectSocket pos;
   pos = m_pWorld->ObjectGet(m_pWorld->ObjectFind(pgObject));
   if (!pos)
      return;

   // get FOV from camera
   // BUGBUG - in future get more info from camera - such as after effects
   OSMANIMCAMERA oac;
   fp fFOV;
   memset (&oac, 0, sizeof(oac));
   pos->Message (OSM_ANIMCAMERA, &oac);
   fFOV = PI/4;
   if (oac.poac)
      fFOV = oac.poac->m_fFOV;
   fFOV = max(fFOV, .001);
   fFOV = min(fFOV, PI * .99);

   // set the exposure
   if (oac.poac) {
      m_apRender[0]->ExposureSet (CM_LUMENSSUN / exp(oac.poac->m_fExposure));
   }

   // get the matrix
   CMatrix m;
   pos->ObjectMatrixGet (&m);

   // since we are looking at 0,-1,0 but the normal camera looks at 0,1,0,
   // invert y
   CMatrix mFlip;
   mFlip.Scale (-1, -1, 1);   // BUGFIX - Flip more than just y
   m.MultiplyLeft (&mFlip);

   // convert this
   CMatrix mInv;
   CPoint pCenter;
   fp fLong, fLat, fTilt, f2;
   fp fZ, fX, fY;
   m.Invert4 (&mInv);
   CPoint p, p2;
   p.Zero();
   p.MultiplyLeft (&m);

   // convert this
   if (dwModel == CAMERAMODEL_PERSPWALKTHROUGH) {
      m_apRender[0]->CameraPerspWalkthroughGet (&p2, &fLong, &fLat, &fTilt, &f2);
      m.ToXYZLLT (&p2, &fLong, &fLat, &fTilt);
      m_apRender[0]->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, oac.poac ? fFOV : f2);
      RenderUpdate (WORLDC_CAMERAMOVED, TRUE);
   }
   else {// (dwMode == CAMERAMODEL_PERSPOBJECT)
      m_apRender[0]->CameraPerspObjectGet (&p2, &pCenter, &fZ, &fX, &fY, &f2);

      // NOTE: Throwing out center of rotation info for now. I think
      // this is an acceptable thing to do. Solving the center would be
      // really nasty too.
      pCenter.Zero();

      mInv.ToXYZLLT (&p2, &fZ, &fX, &fY);

      p.Scale (-1);
      m_apRender[0]->CameraPerspObject (&p2, &pCenter, fZ, fX, fY, oac.poac ? fFOV : f2);
      RenderUpdate(WORLDC_CAMERAMOVED, TRUE);
   }

   // if changed from flat other things
   if (fChangedFromFlat) {
      UpdateCameraButtons ();
      //RenderUpdate(WORLDC_CAMERAMOVED);
   }
   CameraPosition();
}

/************************************************************************************
CHouseView::GridMove - Given a point in world space, this applies the movement
grid (including rotations) so that it's aligned.

inputs
   PCPoint     p - Point. Changed in place.
   DWORD       dwChangeBits - 0x01 for change X, 0x02 for Y, 0x04 for z.
                     If bit is not set then grid will not be applied.
returns
   none
*/
void CHouseView::GridMove (PCPoint p, DWORD dwChangeBits)
{
   CPoint pOld;
   pOld.Copy (p);

   // translate into grid space
   p->p[3] = 1;
   p->MultiplyLeft (&m_mWorldToGrid);

   // apply the grid
   DWORD i;
   for (i = 0; i < 3; i++)
      p->p[i] = ApplyGrid (p->p[i], m_fGrid);

   // convert baCK
   p->MultiplyLeft (&m_mGridToWorld);

   // restore
   if (!(dwChangeBits & 0x01))
      p->p[0] = pOld.p[0];
   if (!(dwChangeBits & 0x02))
      p->p[1] = pOld.p[1];
   if (!(dwChangeBits & 0x04))
      p->p[2] = pOld.p[2];
}


/************************************************************************************
CHouseView::ObjectSave - Saves the object. (if m_dwWhatView == WHATVIEW_OBJECT)
*/
BOOL CHouseView::ObjectSave (void)
{
   if ((m_dwViewWhat == VIEWWHAT_POLYMESH) || (m_dwViewWhat == VIEWWHAT_BONE))
      return FALSE;  // cant do this

   // if not registered than bug the user
   if (!RegisterIsRegistered()) {
      if (m_pWorld->ObjectNum() < 10) {
         EscMessageBox (m_hWnd, ASPString(),
            L"You have not registered " APPSHORTNAMEW,
            L"Because you haven't registered " APPSHORTNAMEW L" yet, you will "
            L"only be able to save small objects. (Less than 10 objects.) You cannot "
            L"save large objects until you have registered.",
            MB_ICONINFORMATION | MB_OK);
         ASPHelp (IDR_MMLREGISTER);
      }
      else {   // too large
         EscMessageBox (m_hWnd, ASPString(),
            L"The file won't save because you have not registered " APPSHORTNAMEW,
            L"Because you haven't registered " APPSHORTNAMEW L" yet, you will "
            L"only be able to save small objects. (Less than 10 objects.) You cannot "
            L"save large objects until you have registered.",
            MB_ICONEXCLAMATION | MB_OK);
         ASPHelp (IDR_MMLREGISTER);
         return FALSE;
      }
   }

   // find it
   DWORD i;
   PVIEWOBJ pvo;
   for (i = 0; i < gListVIEWOBJ.Num(); i++) {
      pvo = (PVIEWOBJ) gListVIEWOBJ.Get(i);
      if (pvo->pView == this)
         break;
   }
   if (i >= gListVIEWOBJ.Num())
      return FALSE;  // cant find

   // pass it on
   if (!ObjectEditorSave (pvo))
      return FALSE;

   // BUGFIX - Save the actual data
   {
      CProgress Progress;
      Progress.Start (m_hWnd, "Saving...");
      LibrarySaveFiles (DEFAULTRENDERSHARD, FALSE, &Progress, TRUE); // BUGFIX - Dont save installed since gets slow when building objects
   }

   EscMessageBox (m_hWnd, ASPString(),
      L"The object has been saved.",
      NULL,
      MB_ICONINFORMATION | MB_OK);

   return TRUE;
}

/************************************************************************************
CHouseView::FileSave - Saves the file.

inputs
   BOOL     fForceAsk - If TRUE, then always ask for the file name. If FALSE, only
            ask if it's not already set.
returns
   BOOL - TRUE if saved
*/
BOOL CHouseView::FileSave (BOOL fForceAsk)
{
   char szFile[256];
   WideCharToMultiByte (CP_ACP, 0, m_pWorld->NameGet(), -1, szFile, sizeof(szFile),0,0);

   if (!szFile[0])
      fForceAsk = TRUE;

   // if not registered than bug the user
   if (!RegisterIsRegistered()) {
      if (m_pWorld->ObjectNum() < 50) {
         EscMessageBox (m_hWnd, ASPString(),
            L"You have not registered " APPSHORTNAMEW,
            L"Because you haven't registered " APPSHORTNAMEW L" yet, you will "
            L"only be able to save small files. (Less than 50 objects.) You cannot "
            L"save large files until you have registered.",
            MB_ICONINFORMATION | MB_OK);
         ASPHelp (IDR_MMLREGISTER);
      }
      else {   // too large
         EscMessageBox (m_hWnd, ASPString(),
            L"The file won't save because you have not registered " APPSHORTNAMEW,
            L"Because you haven't registered " APPSHORTNAMEW L" yet, you will "
            L"only be able to save small files. (Less than 50 objects.) You cannot "
            L"save large files until you have registered.",
            MB_ICONEXCLAMATION | MB_OK);
         ASPHelp (IDR_MMLREGISTER);
         return FALSE;
      }
         
   }

   // ask file name
   if (fForceAsk) {
      // save UI
      OPENFILENAME   ofn;
      char  szTemp[256];
      szTemp[0] = 0;
      strcpy (szTemp, szFile);
      memset (&ofn, 0, sizeof(ofn));

      // BUGFIX - Set directory
      char szInitial[256];
      GetLastDirectory(szInitial, sizeof(szInitial));

      ofn.lpstrInitialDir = szInitial;
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = m_hWnd;
      ofn.hInstance = ghInstance;
      ofn.lpstrFilter = APPLONGNAME " file (*." M3DFILEEXT ")\0*." M3DFILEEXT "\0\0\0";
      ofn.lpstrFile = szTemp;
      ofn.nMaxFile = sizeof(szTemp);
      ofn.lpstrTitle = "Save file";
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
      ofn.lpstrDefExt = M3DFILEEXT;
      // nFileExtension 
      if (!GetSaveFileName(&ofn))
         return FALSE;

      // BUGFIX - Save diretory
      strcpy (szInitial, ofn.lpstrFile);
      szInitial[ofn.nFileOffset] = 0;
      SetLastDirectory(szInitial);

      WCHAR szw[256];
      MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, szw, sizeof(szw)/2);
      m_pWorld->NameSet (szw);
      SetViewTitles(DEFAULTRENDERSHARD);
   }

   WideCharToMultiByte (CP_ACP, 0, m_pWorld->NameGet(), -1, szFile, sizeof(szFile),0,0);
   // try to save it
   if (!::FileSave (DEFAULTRENDERSHARD, szFile)) {
      WCHAR szTemp[256];
      wcscpy (szTemp, APPSHORTNAMEW L" could not save ");
      wcscat (szTemp, m_pWorld->NameGet());
      wcscat (szTemp, L".");

      EscMessageBox (m_hWnd, ASPString(),
         szTemp,
         L"The file may be write protected or in use.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

   m_pWorld->DirtySet (FALSE);
   EscChime (ESCCHIME_INFORMATION);

   return TRUE;
}

/************************************************************************************
CHouseView::FileOpen - Asks the user what file they want to open. If specify a file
then check to see if want to save existing file. once saved, closes all view windows
and sets the MainFileWantToOpen() global so that will be opened.
*/
BOOL CHouseView::FileOpen (void)
{
   // BUGFIX - can't open open if ground view open
   if (!MakeSureAllWindowsClosed (TRUE))
      return FALSE;

   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   memset (&ofn, 0, sizeof(ofn));
   
   // BUGFIX - Set directory
   char szInitial[256];
   GetLastDirectory(szInitial, sizeof(szInitial));
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = m_hWnd;
   ofn.hInstance = ghInstance;
      ofn.lpstrFilter = APPLONGNAME " file (*." M3DFILEEXT ")\0*." M3DFILEEXT "\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = "Open " APPLONGNAME " file";
   ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = M3DFILEEXT;
   // nFileExtension 

   if (!GetOpenFileName(&ofn))
      return FALSE;

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // have a name, so see if should ask if want to sace
   // ask if really want to close if this is the last one around
   if (m_pWorld->DirtyGet()) {
      WCHAR szTemp[256];
      wcscpy (szTemp, L"Do you want to save your changes to ");
      if ((m_pWorld->NameGet())[0])
         wcscat (szTemp, m_pWorld->NameGet()); // BUGFIX - Was wcscpy
      else
         wcscat (szTemp, L"the new file");
      wcscat (szTemp, L" first?");
      int iRet;
      iRet = EscMessageBox (m_hWnd, ASPString(),
         szTemp,
         L"If you don't save them any changes will be lost when the new file "
         L"is opened.",
         MB_ICONQUESTION | MB_YESNOCANCEL);

      if (iRet == IDCANCEL)
         return FALSE;

      if (iRet == IDYES) {
         if (!FileSave (FALSE))
            return FALSE;   // dont close
      }
   }

   // else go agead
   m_pWorld->DirtySet (FALSE); // so dont get asksed again
   strcpy (MainFileWantToOpen(), szTemp);

   CloseAllViewsDown();
   return TRUE;

}

/************************************************************************************
CHouseView::FileNew - Clears everything and opens a new file.
*/
BOOL CHouseView::FileNew (void)
{
   // have a name, so see if should ask if want to sace
   // ask if really want to close if this is the last one around
   if (m_pWorld->DirtyGet()) {
      WCHAR szTemp[256];
      wcscpy (szTemp, L"Do you want to save your changes to ");
      if ((m_pWorld->NameGet())[0])
         wcscat (szTemp, m_pWorld->NameGet());
      else
         wcscat (szTemp, L"the new file");
      wcscat (szTemp, L" first?");
      int iRet;
      iRet = EscMessageBox (m_hWnd, ASPString(),
         szTemp,
         L"If you don't save them any changes will be lost when the new file "
         L"is created.",
         MB_ICONQUESTION | MB_YESNOCANCEL);

      if (iRet == IDCANCEL)
         return FALSE;

      if (iRet == IDYES) {
         if (!FileSave (FALSE))
            return FALSE;   // dont close
      }
   }

   // else go agead
   m_pWorld->DirtySet (FALSE); // so dont get asksed again
   strcpy (MainFileWantToOpen(), "*");

   CloseAllViewsDown();
   return TRUE;

}


static PWSTR gpszFavoriteView = L"FavoriteView";
static PWSTR gpszFavorite = L"Favorite";
static PWSTR gpszName = L"Name";
static PWSTR gpszCamera = L"Camera";
static PWSTR gpszCenter = L"Center";
static PWSTR gpszTrans = L"Trans";
static PWSTR gpszRot = L"Rot";
static PWSTR gpszFOV = L"FOV";

/************************************************************************************
CHouseView::FavoriteViewGen - Fills in the FAVORITEVIEW structure from the current
view settings.

inputs
   PWSTR          pszName - Name. max 64 chars
   PFAVORITEVIEW  pfv - Filled in
returns
   none
*/
void CHouseView::FavoriteViewGen (PWSTR pszName, PFAVORITEVIEW pfv)
{
   DWORD dwCamera = m_apRender[0]->CameraModelGet ();
   memset (pfv,0,sizeof(*pfv));
   wcscpy (pfv->szName, pszName);
   pfv->dwCamera = dwCamera;

   switch (dwCamera) {
   case CAMERAMODEL_FLAT:
      m_apRender[0]->CameraFlatGet (&pfv->pCenter,
         &pfv->pRot.p[2], &pfv->pRot.p[0], &pfv->pRot.p[1],
         &pfv->pTrans.p[2], &pfv->pTrans.p[0], &pfv->pTrans.p[1]);
      break;

   case CAMERAMODEL_PERSPOBJECT:
      m_apRender[0]->CameraPerspObjectGet (&pfv->pTrans, &pfv->pCenter,
         &pfv->pRot.p[2], &pfv->pRot.p[0], &pfv->pRot.p[1],
         &pfv->fFOV);
      break;

   case CAMERAMODEL_PERSPWALKTHROUGH:
      m_apRender[0]->CameraPerspWalkthroughGet (&pfv->pTrans,
         &pfv->pRot.p[2], &pfv->pRot.p[0], &pfv->pRot.p[1],
         &pfv->fFOV);
      break;
   }
}

/************************************************************************************
CHouseView::FavoriteViewApply - Given the FAVORITEVIEW structure, sets the camera model.

inputs
   PFAVORITEVIEW  pfv - Filled in
returns
   none
*/
void CHouseView::FavoriteViewApply (PFAVORITEVIEW pfv)
{
   switch (pfv->dwCamera) {
   case CAMERAMODEL_FLAT:
      m_apRender[0]->CameraFlat (&pfv->pCenter,
         pfv->pRot.p[2], pfv->pRot.p[0], pfv->pRot.p[1],
         pfv->pTrans.p[2], pfv->pTrans.p[0], pfv->pTrans.p[1]);
      break;

   case CAMERAMODEL_PERSPOBJECT:
      m_apRender[0]->CameraPerspObject (&pfv->pTrans, &pfv->pCenter,
         pfv->pRot.p[2], pfv->pRot.p[0], pfv->pRot.p[1],
         pfv->fFOV);
      break;

   case CAMERAMODEL_PERSPWALKTHROUGH:
      m_apRender[0]->CameraPerspWalkthrough (&pfv->pTrans,
         pfv->pRot.p[2], pfv->pRot.p[0], pfv->pRot.p[1],
         pfv->fFOV);
      break;
   }

   CameraPosition();
   UpdateCameraButtons ();
   RenderUpdate(WORLDC_CAMERAMOVED);
}

/************************************************************************************
CHouseView::FavoriteViewNum - Returns the number of favorite views.

returns
   DWORD - number
*/
DWORD CHouseView::FavoriteViewNum (void)
{
   PCMMLNode2 pNode = m_pWorld->AuxGet();
   PWSTR psz;
   PCMMLNode2 pFav = NULL;
   pNode->ContentEnum(pNode->ContentFind (gpszFavoriteView), &psz, &pFav);;
   if (!pFav)
      return 0;


   return pFav->ContentNum();
}

/************************************************************************************
CHouseView::FavoriteViewGet - Gets a favorite view.

inputs
   DWORD       dwIndex - Index
   PFAVORITEVIEW pfv - Filled with the favorite view information
returns
   BOOL - TRUE if success and pfv filled in
*/
BOOL CHouseView::FavoriteViewGet (DWORD dwIndex, PFAVORITEVIEW pfv)
{
   PCMMLNode2 pNode = m_pWorld->AuxGet();
   PWSTR psz;
   PCMMLNode2 pFav = NULL;
   pNode->ContentEnum(pNode->ContentFind (gpszFavoriteView), &psz, &pFav);;
   if (!pFav)
      return FALSE;
   PCMMLNode2 pNew;
   pNew = NULL;
   pFav->ContentEnum (dwIndex, &psz, &pNew);
   if (!pNew)
      return FALSE;

   memset (pfv, 0, sizeof(*pfv));
   psz = MMLValueGet (pNew, gpszName);
   if (psz)
      wcscpy (pfv->szName, psz);
   pfv->dwCamera = (DWORD) MMLValueGetInt (pNew, gpszCamera, CAMERAMODEL_FLAT);
   CPoint pZero;
   pZero.Zero();
   MMLValueGetPoint (pNew, gpszCenter, &pfv->pCenter, &pZero);
   MMLValueGetPoint (pNew, gpszTrans, &pfv->pTrans, &pZero);
   MMLValueGetPoint (pNew, gpszRot, &pfv->pRot, &pZero);
   pfv->fFOV = MMLValueGetDouble (pNew, gpszFOV, PI/8);
   pfv->dwIndex = dwIndex;

   return TRUE;
}

/************************************************************************************
CHouseView::FavoriteViewAdd - Adds a favorite view.

inputs
   PFAVORITEVIEW pfv - The favorite view information to add
returns
   BOOL - TRUE if success and pfv filled in
*/
BOOL CHouseView::FavoriteViewAdd (PFAVORITEVIEW pfv)
{
   PCMMLNode2 pNode = m_pWorld->AuxGet();
   PWSTR psz;
   PCMMLNode2 pFav = NULL;
   pNode->ContentEnum(pNode->ContentFind (gpszFavoriteView), &psz, &pFav);;
   if (!pFav) {
      pFav = pNode->ContentAddNewNode ();
      if (!pFav)
         return FALSE;
      pFav->NameSet (gpszFavoriteView);
   }

   PCMMLNode2 pNew;
   pNew = pFav->ContentAddNewNode ();
   if (!pNew)
      return FALSE;
   pNew->NameSet (gpszFavorite);
   MMLValueSet (pNew, gpszName, pfv->szName);
   MMLValueSet (pNew, gpszCamera, (int)pfv->dwCamera);
   MMLValueSet (pNew, gpszCenter, &pfv->pCenter);
   MMLValueSet (pNew, gpszTrans, &pfv->pTrans);
   MMLValueSet (pNew, gpszRot, &pfv->pRot);
   MMLValueSet (pNew, gpszFOV, pfv->fFOV);

   return TRUE;
}

/************************************************************************************
CHouseView::FavoriteViewRemove - Deletes the favorite view

inputs
   DWORD       dwIndex - Index
returns
   BOOL - TRUE if success and pfv filled in
*/
BOOL CHouseView::FavoriteViewRemove (DWORD dwIndex)
{
   PCMMLNode2 pNode = m_pWorld->AuxGet();
   PWSTR psz;
   PCMMLNode2 pFav = NULL;
   pNode->ContentEnum(pNode->ContentFind (gpszFavoriteView), &psz, &pFav);;
   if (!pFav)
      return FALSE;


   return pFav->ContentRemove(dwIndex);
}

/************************************************************************************
CHouseView::RotationRemember - Remember important information used in the rotation
around a vector. This is used when rotating objects or even the world.

inputs
   PCPoint     pCenter - Center point - point on the rotation vector
   PCPoint     pCenterOrig - Orignal center, in object coords
   PCPoint     pVector - Rotate around this vector, anchored by the center point
   PCMatrix    pmOrig - Original matrix
   DWORD       dwX, dwY - X and Y location where clicked on screen
   BOOL        fCamera - Set to true if dragging the camera - which means different space
returns
   BOOL - TRUE if success, FALSE if clicked on empty space
*/
BOOL CHouseView::RotationRemember (PCPoint pCenter, PCPoint pCenterOrig, PCPoint pVector, PCMatrix pmOrig,
      DWORD dwX, DWORD dwY, BOOL fCamera)
{
   // find out wher clicked
   fp fZ;
   CPoint pTemp;
   fZ = m_apRender[0]->PixelToZ (dwX, dwY);
   if (fZ > 1000)
      return FALSE;
   if (fCamera)
      m_apRender[0]->PixelToViewerSpace (dwX, dwY, fZ, &pTemp);
   else
      m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &pTemp);

   // find the radius of the sphere
   CPoint pr;
   pr.Subtract (&pTemp, pCenter);
   m_fRotRadius = pr.Length();

   m_pRotCenter.Copy (pCenter);
   m_pRotCenterOrig.Copy (pCenterOrig);
   m_pRotVector.Copy (pVector);
   m_pRotVector.Normalize();
   m_mRotOrig.Copy (pmOrig);

   // make where clicked co-planar
   if (!MakePointCoplanar (&pTemp, &m_pRotCenter, &m_pRotVector, &m_pRotClickOrig))
      return FALSE;

   // clicked on the front of back of the sphere
   CPoint pEye, pl1, pl2;
   if (fCamera)
      m_apRender[0]->PixelToViewerSpace (dwX, dwY, (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);
   else
      m_apRender[0]->PixelToWorldSpace (dwX, dwY, (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);
   pl1.Copy (&pEye);
   pl2.Subtract (&pTemp, &pl1);
   DWORD dwNum;
   CPoint pi1, pi2;
   dwNum = IntersectLineSphere (&pl1, &pl2, &m_pRotCenter, m_fRotRadius, &pi1, &pi2);
   m_fRotClickFront = TRUE;
   if (dwNum == 2) {
      CPoint pTemp1, pTemp2;

      // find the closest point to the eye
      pTemp1.Subtract (&pi1, &pl1);
      pTemp2.Subtract (&pi2, &pl1);
      BOOL fFirstClosest;
      fFirstClosest = (pTemp1.Length() < pTemp2.Length());

      // find closest point to where actually clicked
      pTemp1.Subtract (&pi1, &pTemp);
      pTemp2.Subtract (&pi2, &pTemp);
      if (pTemp1.Length() > pTemp2.Length())
         fFirstClosest = !fFirstClosest;  // clicked on the back side
      m_fRotClickFront = fFirstClosest;
   }

   // basically, now have a normal (which we'll call Z, a point perpendicular to the
   // normal (call it X), and need one more. Then can make rotation/translation matrix)
   CPoint pX, pY, pZ;
   pZ.Copy (&m_pRotVector);
   pX.Subtract (&m_pRotClickOrig, &m_pRotCenter);
   pX.Normalize();
   if (pX.Length() < CLOSE)
      return FALSE;  // clicked right over - so cant tell where rotating
   pY.CrossProd (&pZ, &pX);
   pY.Normalize();   // shouldnt need this
   CMatrix mRot;
   mRot.RotationFromVectors (&pX, &pY, &pZ);
   mRot.Invert (&m_mRotInv); // only need invert 3 since is just a rotation

   return TRUE;
}

/************************************************************************************
CHouseView::RotationDrag - Given that RotationRemember() was called, this
takes a dwX and dwY - where the mouse is now, and figures out a vector from the
eye to where the mouse is over. This is flattened into the center of rotation plane
to form a point. Which is used to generate a rotation matrix, then applied to the
original rotation matrix. The results are returned - note that no grid has been
applied.

inputs
   DWORD       dwX, dwY - Pixel locations
   PCMatrix    pmNew - Filled with the new matrix, combination of pmOrig and rotation
   BOOL        fIncludeOrig - If TRUE, multiply by the original (usually the case),
               but may not want to
   BOOL        fSkipSphere - If TRUE, intersect with the plane only. Do this for embedded.
   BOOL        fCamera - If TRUE, rotating the camera around, so use viewer space
returns
   BOOL - TRUE if successful. FALSE if dwX and dwY are not in the rotation spehere
*/
BOOL CHouseView::RotationDrag (DWORD dwX, DWORD dwY, PCMatrix pNew, BOOL fIncludeOrig, BOOL fSkipSphere,
                               BOOL fCamera)
{
   // where are we looking from and at
   CPoint pEye, pAt, p1, p2;
   if (fCamera) {
      m_apRender[0]->PixelToViewerSpace (dwX, dwY, (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);
      m_apRender[0]->PixelToViewerSpace (dwX, dwY, (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? 0 : 10, &pAt);
   }
   else {
      m_apRender[0]->PixelToWorldSpace (dwX, dwY, (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pEye);
      m_apRender[0]->PixelToWorldSpace (dwX, dwY, (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? 0 : 10, &pAt);
   }

   // find out where this intersects the sphere
   CPoint pi1, pi2, pTemp1, pTemp2;
   DWORD dwNum;
   p1.Copy (&pEye);
   p2.Copy (&pAt);
   p2.Subtract (&p1);
   if (fSkipSphere)
      dwNum = 0;
   else
      dwNum = IntersectLineSphere (&p1, &p2, &m_pRotCenter, m_fRotRadius, &pi1, &pi2);
   if (!dwNum) {
      // if it dosn't intersect with the sphere, then intersect it with the plane
      // that the sphere is on and use that. This way can move mouse outside the sphere
      // and be ok
      p2.Add (&p1);
      if (!IntersectLinePlane (&p1, &p2, &m_pRotCenter, &m_pRotVector, &pi1))
         return FALSE;
   }
   if (dwNum == 2) {
      // find the closest point
      pTemp1.Subtract (&pi1, &p1);
      pTemp2.Subtract (&pi2, &p1);
      BOOL fFirstClosest;
      fFirstClosest = (pTemp1.Length() < pTemp2.Length());
      if (m_fRotClickFront != fFirstClosest)
         pi1.Copy (&pi2);  // want the second point
   }
   pi2.Copy (&pi1);
   // make  this coplanar
   if (!MakePointCoplanar (&pi2, &m_pRotCenter, &m_pRotVector, &pi1))
      return FALSE;


   // make XYZ vectors
   CPoint pX, pY, pZ;
   pZ.Copy (&m_pRotVector);
   pX.Subtract (&pi1, &m_pRotCenter);
   pX.Normalize();
   if (pX.Length() < CLOSE)
      return FALSE;
   pY.CrossProd (&pZ, &pX);
   pY.Normalize();   // shouldnt need this

   CMatrix mRot;
   mRot.RotationFromVectors (&pX, &pY, &pZ);

   // multiply this on the left by the inverse that did earlier
   mRot.MultiplyLeft (&m_mRotInv); // only need invert 3 since is just a rotation

   // create a translation vector to center the original matrix
   CMatrix mTrans;
   mTrans.Translation (-m_pRotCenter.p[0], -m_pRotCenter.p[1], -m_pRotCenter.p[2]);
   mRot.MultiplyLeft (&mTrans);  // add this to the right

   // and translate back
   mTrans.Translation (m_pRotCenter.p[0], m_pRotCenter.p[1], m_pRotCenter.p[2]);
   mRot.MultiplyRight (&mTrans);  // add this to the right

   // then multiply this matrix by the original one
   if (fIncludeOrig)
      pNew->Multiply (&mRot, &m_mRotOrig);
   else
      pNew->Copy (&mRot);

   return TRUE;
}

/************************************************************************************
CHouseView::UpdateScrollBars - Update all 4 scrollbars because something just changed
which may affect their position.
*/
void CHouseView::UpdateScrollBars (void)
{
   if (m_fDisableChangeScroll)
      return;

   DWORD i;
   for (i = 0; i < 4; i++)
      ScrollActionUpdateScrollbar (m_ahWndScroll[i], i, m_apRender[0]);
}


/************************************************************************************
CHouseView::NearSelect - Given an XY location (in pixels), this returns a dwID (from
pip->dwID) for the the pixel clicked on. Except, this looks for thin objects nearby (within
2 pixels) and returns those in preference. This makes it easier to click on walls
or other very thin things.

inputs
   DWORD       dwX, dwY - x and y coords in m_dwImage[0]
returns
   DWORD - pip->dwID of pixel chosen
*/
typedef struct {
   DWORD       dwID;    // ID
   DWORD       dwCountNear;   // count in the CLICKNEAR range
   DWORD       dwCountFar;    // count in the CLICKFAR range
} NSINFO, *PNSINFO;

static int _cdecl NearSort (const void *elem1, const void *elem2)
{
   NSINFO *pdw1, *pdw2;
   pdw1 = (NSINFO*) elem1;
   pdw2 = (NSINFO*) elem2;

   return (int) pdw2->dwCountFar - (int) pdw1->dwCountNear;
}

DWORD CHouseView::NearSelect (DWORD dwX, DWORD dwY)
{
#define  CLICKNEAR      5     // 2 pixels to right and 2 to left
#define  CLICKFAR       7     // used to see how common paricular pixel is in larger area


   // keep track of how often occurs
   DWORD dwNum,i ;
   NSINFO ans[CLICKNEAR*CLICKNEAR];
   int x, y;
   PIMAGEPIXEL pip;
   dwNum = 0;
   for (y = (int)dwY - (CLICKNEAR-1)/2; y < (int)dwY + (CLICKNEAR-1)/2; y++)
      for (x = (int)dwX - (CLICKNEAR-1)/2; x < (int)dwX + (CLICKNEAR-1)/2; x++) {
         if ((x < 0) || (y < 0) || (x >= (int)m_aImage[0].Width()) || (y >= (int)m_aImage[0].Height()))
            continue;

         pip = m_aImage[0].Pixel((DWORD) x, (DWORD) y);
         if (!pip->dwID)
            continue;   // clicked on empty space so ignore

         // see if already have
         for (i = 0; i < dwNum; i++)
            if (ans[i].dwID == pip->dwID) {
               ans[i].dwCountNear++;
               ans[i].dwCountFar++;
               break;
            }
         if (i >= dwNum) {
            // need to add
            ans[dwNum].dwCountFar = ans[dwNum].dwCountNear = 1;
            ans[dwNum].dwID = pip->dwID;
            dwNum++;
         }
      }

   // see how many of the points occur in the far region
   for (y = (int)dwY - (CLICKFAR-1)/2; y < (int)dwY + (CLICKFAR-1)/2; y++)
      for (x = (int)dwX - (CLICKFAR-1)/2; x < (int)dwX + (CLICKFAR-1)/2; x++) {
         if ((x < 0) || (y < 0) || (x >= (int)m_aImage[0].Width()) || (y >= (int)m_aImage[0].Height()))
            continue;
         if ( (abs(x - (int)dwX) <= (CLICKNEAR-1)/2) && (abs(y - (int)dwY) <= (CLICKNEAR-1)/2) )
            continue;   // already tested

         pip = m_aImage[0].Pixel((DWORD) x, (DWORD) y);
         if (!pip->dwID)
            continue;   // clicked on empty space so ignore

         // see if already have
         for (i = 0; i < dwNum; i++)
            if (ans[i].dwID == pip->dwID) {
               ans[i].dwCountFar++;
               break;
            }
         // dont add if out of range
      }

   // take the ID which happens in the near range but which is 2nd most
   // common in the far range
   qsort (&ans[0], dwNum, sizeof(NSINFO), NearSort);
   if (dwNum >= 2)
      return ans[1].dwID;
   else if (dwNum >= 1)
      return ans[0].dwID;  // only one
   else
      return 0;   // nothing

}


/***************************************************************************************
CHouseView::LookAtArea - A bounding box is passed into the function. Then, using
the current camera, it positions it so that it's looking at the given area.

inputs
   PCPoint     pMin, pMax - Min and max of the bounding box
   DWORD       dwFrom - Use the flags for the "look from" menu, such as ID_TOP
reutrns
   BOOL - TRUE if success
*/
BOOL CHouseView::LookAtArea (PCPoint pMin, PCPoint pMax, DWORD dwFrom)
{
   CPoint pCenter;
   fp fLong, fTiltX,fTiltY, fTransX, fTransY, fScale;
   pCenter.Zero();
   fLong = fTiltX = fTiltY = fTransX = fTransY = 0;
   fScale = 10;

   // world size
   CPoint b[2];
   fp fMax[3];
   b[0].Copy (pMin);
   b[1].Copy (pMax);
   DWORD x;
   for (x = 0; x < 3; x++)
      fMax[x] = b[1].p[x] - b[0].p[x];

   pCenter.Add (&b[0], &b[1]);
   pCenter.Scale (.5);

   fScale = max(max(fMax[0], fMax[1]), fMax[2]);
   fp fTiltAngle;
   fTiltAngle = PI/4;

   switch (dwFrom) {
   case ID_TOP:
      fTiltAngle = 0;
      fTiltX = PI / 2;
      fScale = max(fMax[0], fMax[1]);
      break;
   case ID_FROMTHENORTH:
      fTiltAngle = 0;
      // BUGFIX - Go for the center of the object, dont care about eye height
      //pCenter.p[2] = 0; // BUGFIX - So always view at ground level
   case ID_QUICK_NORTHELEVATED:
      fLong = PI;
      //fScale = max(fMax[0], fMax[2]);
      break;

   case ID_FROMTHEEAST:
      fTiltAngle = 0;
      // BUGFIX - Go for the center of the object, dont care about eye height
      //pCenter.p[2] = 0; // BUGFIX - So always view at ground level
   case ID_QUICK_EASTELEVATED:
      fLong = PI/2;
      //fScale = max(fMax[1], fMax[2]);
      break;

   case ID_FROMTHESOUTH:
      fTiltAngle = 0;
      // BUGFIX - Go for the center of the object, dont care about eye height
      //pCenter.p[2] = 0; // BUGFIX - So always view at ground level
   case ID_QUICK_SOUTHELEVATED:
      fLong = 0;
      //fScale = max(fMax[0], fMax[2]);
      break;

   case ID_FROMTHEWEST:
      fTiltAngle = 0;
      // BUGFIX - Go for the center of the object, dont care about eye height
      //pCenter.p[2] = 0; // BUGFIX - So always view at ground level
   case ID_QUICK_WESTELEVATED:
      fLong = -PI/2;
      //fScale = max(fMax[1], fMax[2]);
      break;

   case ID_QUICK_NORTHEAST:
      fTiltAngle = 0;
      // BUGFIX - Go for the center of the object, dont care about eye height
      //pCenter.p[2] = 0; // BUGFIX - So always view at ground level
   case ID_QUICK_NORTHEASTELEVATED:
      fLong = (PI + PI/2)/2;
      break;

   case ID_QUICK_NORTHWEST:
      fTiltAngle = 0;
      // BUGFIX - Go for the center of the object, dont care about eye height
      //pCenter.p[2] = 0; // BUGFIX - So always view at ground level
   case ID_QUICK_NORTHWESTELEVATED:
      fLong = (PI + 3*PI/2)/2;
      break;

   case ID_QUICK_SOUTHEAST:
      fTiltAngle = 0;
      // BUGFIX - Go for the center of the object, dont care about eye height
      //pCenter.p[2] = 0; // BUGFIX - So always view at ground level
   case ID_QUICK_SOUTHEASTELEVATED:
      fLong = (0 + PI/2)/2;
      break;

   case ID_QUICK_SOUTHWEST:
      fTiltAngle = 0;
      // BUGFIX - Go for the center of the object, dont care about eye height
      //pCenter.p[2] = 0; // BUGFIX - So always view at ground level
   case ID_QUICK_SOUTHWESTELEVATED:
      fLong = (0 - PI/2)/2;
      break;
   }
   if (dwFrom != ID_TOP) {
      fTiltX = cos(fLong) * fTiltAngle;
      fTiltY = sin(fLong) * fTiltAngle;
      //if (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) {
      //   fTiltX = fTiltAngle;
      //}
   }

   if (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT)
      m_apRender[0]->CameraFlat (&pCenter, -fLong, fTiltX, fTiltY, fScale, fTransX, fTransY);
   else if (m_apRender[0]->CameraModelGet() == CAMERAMODEL_PERSPWALKTHROUGH) {
      CPoint pTranslate;
      fp fRotateZ, fRotateX, fRotateY, fFOV;

      m_apRender[0]->CameraPerspWalkthroughGet (&pTranslate, &fRotateZ, &fRotateX, &fRotateY, &fFOV);

      pTranslate.Zero();

      // invert this
      CMatrix m, mInv;
      m.FromXYZLLT (&pTranslate, -fLong, fTiltX, fTiltY);
      m.Invert4 (&mInv);
      mInv.ToXYZLLT (&pTranslate, &fLong, &fTiltX, &fTiltY);
      pTranslate.Zero();

      // figure out translation point
      pTranslate.Zero();
      fFOV = max(.01, fFOV);
      fFOV = min(PI/2 - .01, fFOV);
      fScale = max(fMax[0], max(fMax[1], fMax[2]));
      pTranslate.p[1] = -fScale / tan(fFOV/2) * (2.0/3.0);
      pTranslate.MultiplyLeft (&mInv);
      pTranslate.Add (&pCenter);
      // BUGFIX - Go for the center of the object, dont care about eye height
      //pTranslate.p[2] = max(CM_EYEHEIGHT, pTranslate.p[2]);

      m_apRender[0]->CameraPerspWalkthrough (&pTranslate, fLong, fTiltX, fTiltY, fFOV);
   }
   else if (m_apRender[0]->CameraModelGet() == CAMERAMODEL_PERSPOBJECT) {
      CPoint pTranslate, pCenter2;
      fp fRotateZ, fRotateX, fRotateY, fFOV;

      m_apRender[0]->CameraPerspObjectGet (&pTranslate, &pCenter2, &fRotateZ, &fRotateX, &fRotateY, &fFOV);

      pTranslate.Zero();
      fFOV = max(.01, fFOV);
      fFOV = min(PI/2 - .01, fFOV);
      fScale = max(fMax[0], max(fMax[1], fMax[2]));
      pTranslate.p[1] = fScale / tan(fFOV/2) * (2.0/3.0);
      // BUGFIX - Go for the center of the object, dont care about eye height
      //pTranslate.p[2] = -max(CM_EYEHEIGHT - pCenter.p[2], 0);  // at least 1.8m

      m_apRender[0]->CameraPerspObject (&pTranslate, &pCenter, -fLong, fTiltX, fTiltY, fFOV);
   }
   CameraPosition();
   RenderUpdate(WORLDC_CAMERAMOVED);


   return TRUE;
}


/***************************************************************************************
CHouseView::NearestCP - Given a pixel dwX and dwY, along with an object ID, this
finds the closest CP to it.

inputs
   DWORD       dwX, dwY - Pixel the cursor is over
   DWORD       dwID - Object ID (index)
returns
   DWORD - Closest control point number, or -1 if error
*/
DWORD CHouseView::NearestCP (DWORD dwX, DWORD dwY, DWORD dwID)
{
   PIMAGEPIXEL pip = m_aImage[0].Pixel (dwX, dwY);

   // make sure this is in the selected list and only one selected
   DWORD dwNum;
   DWORD *padwSel;
   padwSel = m_pWorld->SelectionEnum (&dwNum);
   if (dwNum != 1)
      return -1;  // too many object selected
   if (padwSel[0] != dwID)
      return -1;  // wrong object selected

   fp fZ;
   fZ = m_apRender[0]->PixelToZ (dwX, dwY);
   if (fZ > 1000)
      return -1;  // background

   CPoint p;
   m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &p);

   // convert this into object space
   PCObjectSocket pos;
   CMatrix mObj, mObjInv;
   pos = m_pWorld->ObjectGet (dwID);
   if (!pos)
      return -1;
   pos->ObjectMatrixGet (&mObj);
   mObj.Invert4 (&mObjInv);
   p.p[3] = 1;
   p.MultiplyLeft (&mObjInv);

   // loop through all the control points and find closest
   CListFixed l;
   l.Init (sizeof(DWORD));
   pos->ControlPointEnum (&l);
   DWORD i, *padw;
   fp fLen, fClosest;
   DWORD dwClosest;
   dwClosest = -1;
   padw = (DWORD*) l.Get(0);
   OSCONTROL Info;
   for (i = 0; i < l.Num(); i++) {
      if (!pos->ControlPointQuery (padw[i], &Info))
         continue;

      Info.pLocation.Subtract (&p);
      fLen = Info.pLocation.Length();
      if ((dwClosest == -1) || (fLen < fClosest)) {
         dwClosest = padw[i];
         fClosest = fLen;
      }
   }

   return dwClosest;
}

/***************************************************************************************
CHouseView::ForcePaint - Forces the world to be redrawn. This is called when the animation
slider is moved so drawing occurs right away.
*/
void CHouseView::ForcePaint (void)
{
   if (!m_fTimerWaitToDraw || !m_fWorldChanged)
      return;  // nothing to redraw

   KillTimer (m_hWnd, WAITTODRAW);
   m_fTimerWaitToDraw = NULL;
   InvalidateDisplay ();
}


/****************************************************************************
CHouseView::PolyMeshObject - Returns a pointer to the polymesh object that
this view modifies, or NULL if cant find

NOTE: Can also be called for bone and other objects

inputs
   DWORD       *pdwFind - If not NULL, will fill in with the object number.
*/
PCObjectSocket CHouseView::PolyMeshObject (DWORD *pdwFind)
{
   // veritfy that the world still exists
   if (!FindViewForWorld(m_pWorld))
      return NULL;


   DWORD dwFind = m_pWorld->ObjectFind (&m_gObject);
   if (dwFind == -1)
      return NULL;
   if (pdwFind)
      *pdwFind = dwFind;

   return m_pWorld->ObjectGet (dwFind);
}

/****************************************************************************
CHouseView::PolyMeshObject2 - Returns a pointer to the polymesh object that
this view modifies, or NULL if cant find

inputs
   DWORD       *pdwFind - If not NULL, will fill in with the object number.
*/
PCObjectPolyMesh CHouseView::PolyMeshObject2 (DWORD *pdwFind)
{
   PCObjectSocket pos = PolyMeshObject (pdwFind);
   if (!pos)
      return NULL;

   OSMPOLYMESH os;
   memset (&os, 0, sizeof(os));
   pos->Message (OSM_POLYMESH, &os);
   return os.ppm;
}


/****************************************************************************
CHouseView::PolyMeshBoundBox - Gets the polygon mesh's bounding box.
Can also call for bones

inputs
   PCPoint        pab - Pointer to an array of 2 points for min and max
returns
   BOOL - TRUE if success
*/
BOOL CHouseView::PolyMeshBoundBox (PCPoint pab)
{
   DWORD dwFind;
   if (!PolyMeshObject (&dwFind))
      return FALSE;

   CMatrix m;
   CPoint abBound[2];
   m_pWorld->BoundingBoxGet (dwFind, &m, &abBound[0], &abBound[1]);

   pab[0].Zero();
   pab[1].Zero();
   DWORD x,y,z;
   CPoint pTemp;
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
      pTemp.p[0] = abBound[x].p[0];
      pTemp.p[1] = abBound[x].p[1];
      pTemp.p[2] = abBound[x].p[2];
      pTemp.p[3] = 1;
      pTemp.MultiplyLeft (&m);

      if (x || y || z) {
         pab[0].Min (&pTemp);
         pab[1].Max (&pTemp);
      }
      else {
         pab[0].Copy (&pTemp);
         pab[1].Copy (&pTemp);
      }

   } // xyz

   return TRUE;
}



/* PolyMeshDialogPage
*/
static BOOL PolyMeshDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPMDINFO ppmd = (PPMDINFO) pPage->m_pUserData;
   PCObjectPolyMesh pMesh = ppmd->pMesh;
   PCHouseView pView = ppmd->pView;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"backface");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pMesh->m_PolyMesh.m_fCanBackface);

         ComboBoxSet (pPage, L"SubWork", pMesh->m_dwSubdivideWork+1); 
         ComboBoxSet (pPage, L"SubFinal", pMesh->m_dwSubdivideFinal+1); 
      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // if the major ID changed then pick the first minor and texture that
         // comes to mind
         if (!_wcsicmp(p->pControl->m_pszName, L"SubWork")) {
            DWORD dwVal = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;

            // if it hasn't reall change then ignore
            if (dwVal == pMesh->m_dwSubdivideWork+1)
               return TRUE;

            pMesh->m_pWorld->ObjectAboutToChange (pMesh);
            pMesh->m_dwSubdivideWork = dwVal - 1;
            pMesh->m_pWorld->ObjectChanged (pMesh);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"SubFinal")) {
            DWORD dwVal = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;

            // if it hasn't reall change then ignore
            if (dwVal == pMesh->m_dwSubdivideFinal+1)
               return TRUE;

            pMesh->m_pWorld->ObjectAboutToChange (pMesh);
            pMesh->m_dwSubdivideFinal = dwVal - 1;
            pMesh->m_pWorld->ObjectChanged (pMesh);

            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"backface")) {
            pMesh->m_pWorld->ObjectAboutToChange (pMesh);
            pMesh->m_PolyMesh.m_fCanBackface = p->pControl->AttribGetBOOL (Checked());
            pMesh->m_pWorld->ObjectChanged (pMesh);
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Polygon mesh settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
CHouseView::PolyMeshSettings - This brings up the UI for polymesh settings. As parameters are changed
the view object will be notified and told to refresh.
*/
void CHouseView::PolyMeshSettings (void)
{
   CEscWindow cWindow;
   RECT r;
   PMDINFO mdi;
   memset (&mdi, 0, sizeof(mdi));
   mdi.pMesh = PolyMeshObject2();
   if (!mdi.pMesh)
      return;
   mdi.pView = this;
   DialogBoxLocation (m_hWnd, &r);

   // remember unto
   mdi.pMesh->m_pWorld->UndoRemember();

   cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPOLYMESHDIALOG, PolyMeshDialogPage, &mdi);

   // remember unto
   mdi.pMesh->m_pWorld->UndoRemember();

   return;
}




/****************************************************************************
CHouseView::PolyMeshEdgeFromSide - Fills in a list of edges from two sides.

inputs
   PCObjectPolyMesh        pm - Object
   DWORD                   dwSide1, dwSide2 - Two side number
   PCListFixed             plEdge - Must have been initialized to 2* sizeof(DWORD) by this function.
                           Filled in with a list of edges (2 vertices) that intersect
returns
   none
*/
void CHouseView::PolyMeshEdgeFromSide (PCObjectPolyMesh pm, DWORD dwSide1, DWORD dwSide2,
                                       PCListFixed plEdge)
{
   PCPMSide aps[2];
   aps[0] = pm->m_PolyMesh.SideGet (dwSide1);
   aps[1] = pm->m_PolyMesh.SideGet (dwSide2);
   if (!aps[0] || !aps[1])
      return;

   plEdge->Clear();

   WORD i, j, wINext, wJNext;
   DWORD *padw1, *padw2;
   DWORD padwEdge[2];
   padw1 = aps[0]->VertGet();
   padw2 = aps[1]->VertGet();
   for (i = 0; i < aps[0]->m_wNumVert; i++) {
      wINext = (i+1)%aps[0]->m_wNumVert;
      for (j = 0; j < aps[1]->m_wNumVert; j++) {
         wJNext = (j+1)%aps[1]->m_wNumVert;
         if ((padw1[i] == padw2[j]) && (padw1[wINext] == padw2[wJNext])) {
            padwEdge[0] = min(padw1[i], padw1[wINext]);
            padwEdge[1] = max(padw1[i], padw1[wINext]);
            plEdge->Add (padwEdge);
            continue;
         }

         // also try reverse order
         if ((padw1[i] == padw2[wJNext]) && (padw1[wINext] == padw2[j])) {
            padwEdge[0] = min(padw1[i], padw1[wINext]);
            padwEdge[1] = max(padw1[i], padw1[wINext]);
            plEdge->Add (padwEdge);
            continue;
         }
      } // j
   } // i
}


/****************************************************************************
CHouseView::PolyMeshValidEdgeFromSide - Given a side, this determines what
edges are attached to it.

inputs
   DWORD          dwSide - Side
   PCListFixed    plValidEdge - Initialized to sizeof(PCPMEdge) and filled in
                  with any edges attached to side
returns
   BOOL - TRUE if any of the edges are discnnected (not attached to other sides)
*/
BOOL CHouseView::PolyMeshValidEdgeFromSide (PCObjectPolyMesh pm, DWORD dwSide, PCListFixed plValidEdge)
{
   pm->m_PolyMesh.CalcEdges();

   // given the side we clicked on, what are the possible edges
   plValidEdge->Init (sizeof(PCPMEdge));
   PCPMSide ps = pm->m_PolyMesh.SideGet (dwSide);
   if (!ps)
      return FALSE;
   DWORD adw[2], i;
   DWORD *padwVert, dwFind;
   PCPMEdge pPMEdge;
   BOOL fCliff;
   fCliff = FALSE;
   padwVert = ps->VertGet();
   for (i = 0; i < ps->m_wNumVert; i++) {
      adw[0] = min(padwVert[i], padwVert[(i+1)%ps->m_wNumVert]);
      adw[1] = max(padwVert[i], padwVert[(i+1)%ps->m_wNumVert]);

      // find the edge
      dwFind = pm->m_PolyMesh.EdgeFind (adw[0], adw[1]);
      if (dwFind == -1)
         continue;   // shouldnt happen

      // add
      pPMEdge = pm->m_PolyMesh.EdgeGet (dwFind);
      if (!pPMEdge)
         continue;   // shouldnt happen

      plValidEdge->Add (&pPMEdge);

      if ((pPMEdge->m_adwSide[0] == -1) || (pPMEdge->m_adwSide[1] == -1))
         fCliff = TRUE; // so know that could be on edge of polymesh without conntect side
   } // i

   return fCliff;
}

/****************************************************************************
CHouseView::PolyMeshEdgeFromPixel - Given an X and Y in the image, this
returns what edge is indicted.

inputs
   DWORD          dwX, dwY - X and Y in image. within image bounds
   DWORD          *padwEdge - Pointer to an array of 2 vertices that are filled in
                  with the edge's two vertices (if successful)
returns
   BOOL - TRUE if found edge, FALSE if not
*/
BOOL CHouseView::PolyMeshEdgeFromPixel (DWORD dwX, DWORD dwY, DWORD *padwEdge)
{
   // NOTE - Allows to get edge from pixel even if have multiple morphs or bone
   // mods, but other calls (further down track) will fail in this case

   // get the pixel
   PIMAGEPIXEL pip;
   pip = m_aImage[0].Pixel (dwX, dwY);
   if (!pip)
      return FALSE;  // error
   DWORD dwObject;
   dwObject = (DWORD)HIWORD(pip->dwID);
   if (!dwObject)
      return FALSE;
   dwObject--;

   // get the polymorph
   PCObjectPolyMesh pm;
   DWORD dwFind;
   pm = PolyMeshObject2 (&dwFind);
   if (!pm || (dwFind != dwObject))
      return FALSE;

   CListFixed lValidEdge;
   BOOL fCliff;
   fCliff = PolyMeshValidEdgeFromSide (pm, pip->dwIDPart, &lValidEdge);
   PCPMEdge *ppe = (PCPMEdge*) lValidEdge.Get(0);
   DWORD dwNumEdge = lValidEdge.Num();

   // loop over nearby pixels and find nearest pixel that's not the one we clicked on
   int x, y;
   int iWidth = (int)m_aImage[0].Width();
   int iHeight = (int)m_aImage[0].Height();
   int iClosest, iDist;
   DWORD dwClosestX, dwClosestY;
   dwClosestX = -1;
   PIMAGEPIXEL pip2;
#define EDGESELRANGE 5
   for (y = (int)dwY - EDGESELRANGE; y <= (int)dwY + EDGESELRANGE; y++) {
      if ((y < 0) || (y >= iWidth))
         continue;

      for (x = (int)dwX - EDGESELRANGE; x <= (int)dwX + EDGESELRANGE; x++) {
         if ((x < 0) || (x >= iHeight))
            continue;
         if ((x == (int)dwX) && (y == (int)dwY))
            continue;
  
         pip2 = m_aImage[0].Pixel((DWORD)x, (DWORD)y);
         // BUGFIX - If side has an edge not connected to another side then allow even if ID different
         if (HIWORD(pip->dwID) != HIWORD(pip2->dwID)) {
            if (!fCliff)
               continue;
         }
         else if (pip->dwIDPart == pip2->dwIDPart)   // same sub object
            continue;

         // distance?
         iDist = (x - (int)dwX) * (x - (int)dwX) + (y - (int)dwY) * (y - (int)dwY);
         if ((dwClosestX == -1) || (iDist < iClosest)) {
            dwClosestX = (DWORD)x;
            dwClosestY = (DWORD)y;
            iClosest = iDist;
         }
      } // x
   }//y

   // if nothing close then error
   if (dwClosestX == -1)
      return FALSE;

   // else, have two sides...
   pip2 = m_aImage[0].Pixel(dwClosestX, dwClosestY);

   // loop through the sides and figure out vertices in common

   CListFixed lEdge;
   lEdge.Init (sizeof(DWORD)*2);

   // what is the ID of the closest
   DWORD dwClosestID, i;
   if (HIWORD(pip->dwID) != HIWORD(pip2->dwID))
      dwClosestID = -1;
   else
      dwClosestID = pip2->dwIDPart;
   // loop through list of edges and see which ones are valid
   for (i = 0; i < dwNumEdge; i++) {
      DWORD dwCompare;
      if (ppe[i]->m_adwSide[0] != pip->dwIDPart)
         dwCompare = 0;
      else if (ppe[i]->m_adwSide[1] != pip->dwIDPart)
         dwCompare = 1;
      else
         continue;   // shouldnt happen

      if (dwClosestID == ppe[i]->m_adwSide[dwCompare])
         lEdge.Add (ppe[i]->m_adwVert);
   }
   // if no edges found AND have a cliff, then assume that clicked on the edge
   if (!lEdge.Num() && fCliff) for (i = 0; i < dwNumEdge; i++) {
      if ((ppe[i]->m_adwSide[0] == -1) || (ppe[i]->m_adwSide[1] == -1))
         lEdge.Add (ppe[i]->m_adwVert);
   }

   // old code: PolyMeshEdgeFromSide (pm, pip->dwIDPart, pip2->dwIDPart, &lEdge);

   // if no edges then false
   if (!lEdge.Num())
      return FALSE;

   // if only one edge then trivial
   DWORD *padw;
   padw = (DWORD*)lEdge.Get(0);
   if (lEdge.Num() == 1) {
      padwEdge[0] = padw[0];
      padwEdge[1] = padw[1];
      return TRUE;
   }

   // convert the location to object space
   CMatrix m, mInv;
   CPoint pLoc;
   m_apRender[0]->PixelToWorldSpace (dwX, dwY, pip->fZ, &pLoc);
   pm->ObjectMatrixGet (&m);
   m.Invert4 (&mInv);
   pLoc.p[3] = 1;
   pLoc.MultiplyLeft (&mInv);

   // else, figure out which edge is closest
   fp fClosest, fDist;
   CPoint pEdge;
   DWORD dwClosest;
   dwClosest = -1;
   for (i = 0; i < lEdge.Num(); i++) {
      PCPMVert pv1 = pm->m_PolyMesh.VertGet(padw[i*2+0]);
      PCPMVert pv2 = pm->m_PolyMesh.VertGet(padw[i*2+1]);
      pEdge.Average (&pv1->m_pLocSubdivide, &pv2->m_pLocSubdivide);
      pEdge.Subtract (&pLoc);
      fDist = pEdge.Length();
      if ((dwClosest == -1) || (fDist < fClosest)) {
         dwClosest = i;
         fClosest = fDist;
      }
   }

   // found closest
   padwEdge[0] = padw[dwClosest*2+0];
   padwEdge[1] = padw[dwClosest*2+1];
   return TRUE;
}


/****************************************************************************
CHouseView::PolyMeshVertFromPixel - Given an X and Y in the image, this
returns what vertex is indicted. Or, -1 if none.

inputs
   DWORD          dwX, dwY - X and Y in image. within image bounds
returns
   DWORD - Vertex number closest to that location, or -1 if none
*/
DWORD CHouseView::PolyMeshVertFromPixel (DWORD dwX, DWORD dwY)
{
   // NOTE - Allows to get edge from pixel even if have multiple morphs or bone
   // mods, but other calls (further down track) will fail in this case

   // get the pixel
   PIMAGEPIXEL pip;
   pip = m_aImage[0].Pixel (dwX, dwY);
   if (!pip)
      return -1;  // error
   DWORD dwObject;
   dwObject = (DWORD)HIWORD(pip->dwID);
   if (!dwObject)
      return -1;
   dwObject--;

   // get the polymorph
   PCObjectPolyMesh pm;
   DWORD dwFind;
   pm = PolyMeshObject2 (&dwFind);
   if (!pm || (dwFind != dwObject))
      return -1;

   // convert the location to object space
   CMatrix m, mInv;
   CPoint pLoc;
   m_apRender[0]->PixelToWorldSpace (dwX, dwY, pip->fZ, &pLoc);
   pm->ObjectMatrixGet (&m);
   m.Invert4 (&mInv);
   pLoc.p[3] = 1;
   pLoc.MultiplyLeft (&mInv);

   // closest?
   return pm->m_PolyMesh.VertFindFromClick (&pLoc, pip->dwIDPart & IDPARTBITS_MASK);
}

/****************************************************************************
CHouseView::PolyMeshSideFromPixel - Given an X and Y in the image, this
returns what side is indicted. Or, -1 if none.

inputs
   DWORD          dwX, dwY - X and Y in image. within image bounds
returns
   DWORD - Vertex number closest to that location, or -1 if none
*/
DWORD CHouseView::PolyMeshSideFromPixel (DWORD dwX, DWORD dwY)
{
   // NOTE - Allows to get edge from pixel even if have multiple morphs or bone
   // mods, but other calls (further down track) will fail in this case

   // get the pixel
   PIMAGEPIXEL pip;
   pip = m_aImage[0].Pixel (dwX, dwY);
   if (!pip)
      return -1;  // error
   DWORD dwObject;
   dwObject = (DWORD)HIWORD(pip->dwID);
   if (!dwObject)
      return -1;
   dwObject--;

   // get the polymorph
   PCObjectPolyMesh pm;
   DWORD dwFind;
   pm = PolyMeshObject2 (&dwFind);
   if (!pm || (dwFind != dwObject))
      return -1;

   return pip->dwIDPart;
}

/****************************************************************************
CHouseView::PolyMeshSelIndividual - Used when clicking on an individual
vertex, edge, or side.

inputs
   DWORD       dwX, dwY - X and Y pixels where clicked
   BOOL        fControl - TRUE if control held down
returns
   none
*/
void CHouseView::PolyMeshSelIndividual (DWORD dwX, DWORD dwY, BOOL fControl)
{
   // get the object and where clicked
   PCObjectPolyMesh pm = PolyMeshObject2 ();
   if (!pm)
      return;
   DWORD dwObject, dwObject2 = 0, dwSelNum, dwFind;
   DWORD adwEdge[2];
   DWORD *padwSel;
   padwSel = pm->m_PolyMesh.SelEnum (&dwSelNum);
   switch (pm->m_PolyMesh.SelModeGet()) {
   case 0:  // vert
      dwObject = PolyMeshVertFromPixel (dwX, dwY);
      break;
   case 1:  // edge
      if (PolyMeshEdgeFromPixel (dwX, dwY, adwEdge)) {
         dwObject = adwEdge[0];
         dwObject2 = adwEdge[1];
      }
      else
         dwObject = -1;
      break;
   case 2:  // side
   default:
      dwObject = PolyMeshSideFromPixel (dwX, dwY);
      break;
   }


   // if clicked on nothing then clear
   if (dwObject == -1) {
      if (!dwSelNum)
         return;  // clearing polymesh, but nothing to clear, so no change

      pm->m_pWorld->ObjectAboutToChange (pm);
      pm->m_PolyMesh.SelClear();
      pm->m_pWorld->ObjectChanged (pm);
      return;
   }

   // else, clicked on a point
   pm->m_pWorld->ObjectAboutToChange (pm);

   // if dont hold down control then clear
   if (!fControl)
      pm->m_PolyMesh.SelClear();

   // if already selected then unselect
   dwFind = pm->m_PolyMesh.SelFind (dwObject, dwObject2);
   if (dwFind != -1)
      pm->m_PolyMesh.SelRemove (dwFind);  // existed so remove
   else
      pm->m_PolyMesh.SelAdd (dwObject, dwObject2); // didnt exist so add

   pm->m_pWorld->ObjectChanged (pm);
}


/*******************************************************************************
CHouseView::PolyMeshSelRender - Modifies the render image (after the fact) so
that the sleection is drawn.

inputs
   PCObjectPolyMesh        pm - Mesh object to use
   DWORD                   dwObjectID - Object ID
   PCRenderTraditional     pRender - Draw to this
   PCImage                 pImage - Image associated with render
returns
   none
*/
void CHouseView::PolyMeshSelRender (PCObjectPolyMesh pm, DWORD dwObjectID,
                                        PCRenderTraditional pRender, PCImage pImage)
{
   DWORD *padwSel;
   DWORD dwNumSel, i;
   CMatrix m;
   pm->ObjectMatrixGet (&m);
   padwSel = pm->m_PolyMesh.SelEnum(&dwNumSel);
   DWORD dwSelMode = pm->m_PolyMesh.SelModeGet();

   if ((m_dwViewSub != VSPOLYMODE_POLYEDIT) && (m_dwViewSub != VSPOLYMODE_TEXTURE) && (m_dwViewSub != VSPOLYMODE_TAILOR))
      dwNumSel = 0;

   // in tailor mode only show if selecting surfaces
   if ((m_dwViewSub == VSPOLYMODE_TAILOR) && (dwSelMode != 2))
      dwNumSel = 0;


   if (!dwNumSel)
      return;
   switch (dwSelMode) {
   case 0:  // vertex
      for (i = 0; i < dwNumSel; i++) {
         // get the point
         PCPMVert pv = pm->m_PolyMesh.VertGet (padwSel[i]);
         if (!pv)
            continue;

         // convert this to world
         CPoint pWorld;
         pWorld.Copy (&pv->m_pLocSubdivide);
         pWorld.p[3] = 1;
         pWorld.MultiplyLeft (&m);

         // draw it
         PolyMeshSelRenderVert (pRender, pImage, &pWorld);
      }
      break;
   case 1:  // edge
      {
         PIMAGEPIXEL pip = pImage->Pixel(0,0);
         PIMAGEPIXEL pip2;
         BOOL fCliff;
         CListFixed lValidEdge;
         BOOL fSel;
         CListFixed lEdge;
         DWORD x,y, xx, yy, dwWidth, dwHeight, i, dwLastSide;
         lEdge.Init (sizeof(DWORD)*2);
         dwWidth = pImage->Width();
         dwHeight = pImage->Height();
         dwObjectID++;  // so easier to compare
         dwLastSide = -1;
         PCPMEdge *ppe = NULL;
         DWORD dwNumEdge = 0;
         DWORD dwLastOther;
         for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++, pip++) {
            // only care about points dealing with this object
            if (HIWORD(pip->dwID) != dwObjectID)
               continue;

            fSel = FALSE;

            // figure out the valid edges for this
            if (dwLastSide != pip->dwIDPart) {
               fCliff = PolyMeshValidEdgeFromSide (pm, pip->dwIDPart, &lValidEdge);
               ppe = (PCPMEdge*) lValidEdge.Get(0);
               dwNumEdge = lValidEdge.Num();
               dwLastSide = pip->dwIDPart;
            }

            dwLastOther = -2;

            for (yy = (y ? (y-1) : 0); yy < min(y+2,dwHeight); yy++) {
               for (xx = (x ? (x-1) : 0); xx < min(x+2,dwWidth); xx++) {
                  DWORD dwOther;
                  pip2 = pImage->Pixel (xx,yy);
                  if (HIWORD(pip2->dwID) != dwObjectID) {
                     if (!fCliff)
                        continue;
                     dwOther = -1;
                  }
                  else
                     dwOther = pip2->dwIDPart;
                  if (dwOther == dwLastOther)
                     continue;   // already did this and it didnt work, so wont work again
                  dwLastOther = dwOther;

                  if (pip->dwIDPart == dwOther)
                     continue;   // same

                  // figure out all the polygons that match
                  // loop through list of edges and see which ones are valid
                  lEdge.Clear();
                  for (i = 0; i < dwNumEdge; i++) {
                     DWORD dwCompare;
                     if (ppe[i]->m_adwSide[0] != pip->dwIDPart)
                        dwCompare = 0;
                     else if (ppe[i]->m_adwSide[1] != pip->dwIDPart)
                        dwCompare = 1;
                     else
                        continue;   // shouldnt happen

                     if (dwOther == ppe[i]->m_adwSide[dwCompare])
                        lEdge.Add (ppe[i]->m_adwVert);
                  }
                  // if no edges found AND have a cliff, then assume that clicked on the edge
                  if (!lEdge.Num() && fCliff) for (i = 0; i < dwNumEdge; i++) {
                     if ((ppe[i]->m_adwSide[0] == -1) || (ppe[i]->m_adwSide[1] == -1))
                        lEdge.Add (ppe[i]->m_adwVert);
                  }
                  // old code - PolyMeshEdgeFromSide (pm, pip->dwIDPart, dwOther, &lEdge);
                  if (!lEdge.Num())
                     continue;

                  // see if it's on the list
                  DWORD *padw;
                  padw = (DWORD*) lEdge.Get(0);
                  for (i = 0; i < lEdge.Num(); i++)
                     if (-1 != pm->m_PolyMesh.SelFind (padw[i*2+0], padw[i*2+1])) {
                        fSel = TRUE;
                        break;
                     }
               } // xx
               if (fSel)
                  break;
            } // yy

            // if selected then make red
            if (fSel) {
               COLORREF cOutline = pRender->m_pEffectOutline ? pRender->m_pEffectOutline->m_cOutlineSelected : RGB(0xff,0,0);
               pImage->Gamma (cOutline, &pip->wRed);
            }
         } // xy
      }
      break;
   default:
      {
         PIMAGEPIXEL pip = pImage->Pixel(0,0);
         DWORD x,y, dwWidth, dwHeight;
         WORD aw[3];
         dwWidth = pImage->Width();
         dwHeight = pImage->Height();
         COLORREF cOutline = pRender->m_pEffectOutline ? pRender->m_pEffectOutline->m_cOutlineSelected : RGB(0xff,0,0);
         pImage->Gamma (cOutline, aw);
         dwObjectID++;  // so easier to compare
         DWORD dwLastFind, dwLastPart;
         dwLastPart = dwLastFind = -1;
         for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++, pip++) {
            // only care about points dealing with this object
            if (HIWORD(pip->dwID) != dwObjectID)
               continue;

            // check to see if on selection list. Optimize though
            if (dwLastPart != pip->dwIDPart) {
               dwLastPart = pip->dwIDPart;
               dwLastFind = pm->m_PolyMesh.SelFind (dwLastPart);
            }
            if (dwLastFind == -1)
               continue;   // not selected

            // else, selected
            pip->wRed = pip->wRed / 2 + aw[0]/2;
            pip->wGreen = pip->wGreen / 2 + aw[1]/2;
            pip->wBlue = pip->wBlue / 2 + aw[2]/2;
         } // xy
      }
      break;
   }
}
   


/*******************************************************************************
CHouseView::PolyMeshSelRenderVert - Draws a selected vertex
object.

inputs
   PCRenderTraditional     pRender - Draw to this
   PCImage                 pImage - Image associated with render
   PCPoint                 pWorld - Point location in world space
returns
   none
*/
#define  MAXCPSIZE      64
void CHouseView::PolyMeshSelRenderVert (PCRenderTraditional pRender, PCImage pImage, PCPoint pWorld)
{
   CPoint pLoc;
   if (!pRender->WorldSpaceToPixel (pWorld, &pLoc.p[0], &pLoc.p[1], &pLoc.p[2]))
      return; // nothing to draw

   // is this off the screen?
   DWORD dwCPSize, dwWidth, dwHeight;
   dwWidth = pImage->Width();
   dwHeight = pImage->Height();
   dwCPSize = max(dwWidth / 48, 4);
   dwCPSize = min(dwCPSize, MAXCPSIZE);

   // use clobal CP size scale
   switch (RendCPSizeGet()) {
      case 0:
         dwCPSize /= 2;
         break;
      case 2:
         dwCPSize = (dwCPSize * 3) / 2;
         break;
      default:
         // do nothing
         break;
   }
   dwCPSize = max(4,dwCPSize);

   dwCPSize = (dwCPSize / 2) * 2;      // make even

   if ((pLoc.p[0] < -(fp)dwCPSize) || (pLoc.p[0] >= (fp)(dwWidth + dwCPSize)) ||
      (pLoc.p[1] <= -(fp)dwCPSize) || (pLoc.p[1] >= (fp)(dwHeight + dwCPSize)) )
      return;  // off the screen

   // figure out the bounds for the objects
   int      aiBound[MAXCPSIZE];
   DWORD i;
   for (i = 0; i < dwCPSize; i++)
      aiBound[i] = (int)dwCPSize / 2;

   // pregenerate
   WORD awColor[3];
   int iStartX, iStartY;
   COLORREF cOutline = pRender->m_pEffectOutline ? pRender->m_pEffectOutline->m_cOutlineSelected : RGB(0xff,0,0);
   pImage->Gamma (cOutline, awColor);
   iStartX = (int)pLoc.p[0] - (int)dwCPSize / 2;
   iStartY = (int)pLoc.p[1] - (int)dwCPSize / 2;

   DWORD dwX, dwY;
   int iX, iY;
   PIMAGEPIXEL pip;
   DWORD dwCheck;
   BOOL fVisible;
   fVisible = FALSE;
   for (dwCheck = 0; dwCheck < 2; dwCheck++) {
      for (dwY = 0; dwY < dwCPSize; dwY++) {
         iY = (int)dwY + iStartY;
         if ((iY < 0) || (iY >= (int)dwHeight))
            continue;   // out of bounds

         for (dwX = 0; dwX < dwCPSize; dwX++) {
            iX = (int)dwX + iStartX;
            if ((iX < 0) || (iX >= (int)dwWidth))
               continue;   // out of bounds

            pip = pImage->Pixel((DWORD)iX, (DWORD) iY);

            // if find one pixel visible then draw entire thing
            if (!dwCheck) {
               if (pip->fZ < pLoc.p[2])
                  continue;   // it's in front

               fVisible = TRUE;
               break;
            }

            // else, can draw, maybe...
            if ( ((int)dwX < ((int) dwCPSize/2 - aiBound[dwY]-1)) ||
               ((int)dwX > ((int) dwCPSize/2 + aiBound[dwY])) )
               continue;   // beyond the bounds

            // set the color
            BOOL fBlack;
            fBlack = FALSE;
            if (!dwY || (dwY+1 >= dwCPSize))
               fBlack = TRUE; // top and bottom
            else if (!dwX || (dwX+1 >= dwCPSize))
               fBlack = TRUE;
            else if ( ((int)dwX < ((int) dwCPSize/2 - aiBound[dwY-1]-1)) ||
               ((int)dwX > ((int) dwCPSize/2 + aiBound[dwY-1])) )
               fBlack = TRUE; // go up one an it's out of bounds
            else if ( ((int)dwX < ((int) dwCPSize/2 - aiBound[dwY+1]-1)) ||
               ((int)dwX > ((int) dwCPSize/2 + aiBound[dwY+1])) )
               fBlack = TRUE; // go up one an it's out of bounds
            else if ( ((int)dwX-1 < ((int) dwCPSize/2 - aiBound[dwY] - 1)) ||
               ((int)dwX+1 > ((int) dwCPSize/2 + aiBound[dwY])) )
               fBlack = TRUE;

            if (fBlack)
               pip->wRed = pip->wGreen = pip->wBlue = 0;
            else
               memcpy (&pip->wRed, awColor, sizeof(awColor));
         }  // over x
      } // over y

      // if nothing of pixel is bisible then give up
      if (!dwCheck && !fVisible)
         break;
   } // dwCheck
}


/*************************************************************************************
DWORDSearch - Given a pointer to a CListFixed containing DWORDs, sorted in
ascending order, this finds the index of the matching DWORD. Or, -1 if cant find

inputs
   DWORD       dwFind - Item to find
   DWORD       dwNum - Number of items in the list
   DWORD       *padwElem - Pointer to list of elements. MUST be sorted.
returns
   DWORD - Index of found, -1 if can't find
*/
static int _cdecl BDWORDCompare (const void *elem1, const void *elem2)
{
   DWORD *pdw1, *pdw2;
   pdw1 = (DWORD*) elem1;
   pdw2 = (DWORD*) elem2;

   if (*pdw1 > *pdw2)
      return 1;
   else if (*pdw1 < *pdw2)
      return -1;
   else
      return 0;
   // BUGFIX - Was return (int) (*pdw1) - (int)(*pdw2);
}

static int _cdecl B2DWORDCompare (const void *elem1, const void *elem2)
{
   DWORD *pdw1, *pdw2;
   pdw1 = (DWORD*) elem1;
   pdw2 = (DWORD*) elem2;

   if (pdw1[0] > pdw2[0])
      return 1;
   else if (pdw1[0] < pdw2[0])
      return -1;
   else if (pdw1[1] > pdw2[1])
      return 1;
   else if (pdw1[1] < pdw2[1])
      return -1;
   else
      return 0;
}

static DWORD DWORDSearch (DWORD dwFind, DWORD dwNum, DWORD *padwElem)
{
   DWORD *pdw;
   pdw = (DWORD*) bsearch (&dwFind, padwElem, dwNum, sizeof(DWORD), BDWORDCompare);
   if (!pdw)
      return -1;
   return (DWORD) ((PBYTE) pdw - (PBYTE) padwElem) / sizeof(DWORD);
}


/*******************************************************************************
CHouseView::PolyMeshSelRegion - Selects all the points in the region.

inputs
   RECT        *pr - Bounding rectangle in pixels
   CPoint      apVolume[2][2][2] - Bounding box for the volume.
returns
   none
*/
void CHouseView::PolyMeshSelRegion (RECT *pr, CPoint apVolume[2][2][2])
{
   // get objects
   DWORD dwFind;
   PCObjectPolyMesh pm = PolyMeshObject2 (&dwFind);
   if (!pm)
      return;

   // create the clip from the 6 sides
   CRenderClip rc;
   DWORD i;
   CPoint pNormal, p1, p2;
   PCPoint pA, pB, pC;
   for (i = 0; i < 6; i++) {
      // depending on the side
      switch (i) {
      case 0:  // left
         pB = &apVolume[0][0][1];
         pA = &apVolume[0][1][1];
         pC = &apVolume[0][0][0];
         break;
      case 1:  // right
         pB = &apVolume[1][0][1];
         pC = &apVolume[1][1][1];
         pA = &apVolume[1][0][0];
         break;
      case 2:  // front
         pB = &apVolume[1][0][1];
         pA = &apVolume[0][0][1];
         pC = &apVolume[1][0][0];
         break;
      case 3:  // back
         pB = &apVolume[1][1][1];
         pC = &apVolume[0][1][1];
         pA = &apVolume[1][1][0];
         break;
      case 4:  // bottom
         pB = &apVolume[1][0][0];
         pA = &apVolume[0][0][0];
         pC = &apVolume[1][1][0];
         break;
      case 5:  // top
         pB = &apVolume[1][0][1];
         pC = &apVolume[0][0][1];
         pA = &apVolume[1][1][1];
      }

      p1.Subtract (pA, pB);
      p2.Subtract (pC, pB);
      pNormal.CrossProd (&p1, &p2);
      pNormal.Normalize();
      pNormal.p[3] = 0; // set W=0
      p1.Copy (pB);
      p1.p[3] = 0;   // set W=0

      rc.AddPlane (&pNormal, &p1);
   }

   // keep a list of objects that find
   CListFixed lInVolume;
   DWORD j;
   DWORD dwSelMode = pm->m_PolyMesh.SelModeGet();
   lInVolume.Init (sizeof(DWORD));

   // matrix for conversion
   CMatrix m;
   CPoint pLoc, apLoc[2];
   DWORD dwClip, adwClip[2];
   pm->ObjectMatrixGet (&m);
   CListFixed lPoint;
   CLIPPOLYPOINT cpp;
   CMem mem;
   memset (&cpp, 0, sizeof(cpp));
   lPoint.Init (sizeof(CLIPPOLYPOINT));

   switch (dwSelMode) {
   case 0:  // vert
      for (i = 0; ; i++) {
         PCPMVert pv = pm->m_PolyMesh.VertGet(i);
         if (!pv)
            break; // end of list

         // see how this does compared to clip
         pLoc.Copy (&pv->m_pLocSubdivide);
         pLoc.p[3] = 1;
         pLoc.MultiplyLeft (&m);
         rc.CalcClipBits (-1, 1, &pLoc, &dwClip);
         if (dwClip)
            continue;   // out of range

         // add to list
         lInVolume.Add (&i);
      } // i
      break;

   case 1:  // edge
      for (i = 0; ; i++) {
         PCPMEdge pe = pm->m_PolyMesh.EdgeGet (i);
         if (!pe)
            break; // end of list

         // get the two sides
         PCPMVert pv;
         for (j = 0; j < 2; j++) {
            pv = pm->m_PolyMesh.VertGet(pe->m_adwVert[j]);
            if (!pv)
               continue;
            apLoc[j].Copy (&pv->m_pLocSubdivide);
            apLoc[j].p[3] = 1;
            apLoc[j].MultiplyLeft (&m);
            rc.CalcClipBits (-1, 1, &apLoc[j], &adwClip[j]);
         }
         if (j < 2)
            continue;   // error

         // trivial clip
         if (adwClip[0] & adwClip[1])
            continue; // completely out

         if (adwClip[0] | adwClip[1]) {
            if (m_fSelRegionWhollyIn)
               continue;   // if must be entirely in then continue

            // figure out the bit to look at
            DWORD dwBit, dwClipPlane, dwOr;
            dwOr = adwClip[0] | adwClip[1];
            for (dwClipPlane = 0; (DWORD) (1 << dwClipPlane) < dwOr; dwClipPlane++) {
               dwBit = (1 << dwClipPlane);
               if (!(dwBit & dwOr))
                  continue;   // not this one

               rc.ClipLine (dwClipPlane, &apLoc[(adwClip[0] & dwBit) ? 1 : 0],
                  NULL, NULL, NULL, &apLoc[(adwClip[0] & dwBit) ? 0 : 1], NULL, NULL, NULL);

               // retduce bits
               dwOr &= (~dwBit);
               rc.CalcClipBits (adwClip[0] & (~dwBit), 1, &apLoc[0], &adwClip[0]);
               rc.CalcClipBits (adwClip[1] & (~dwBit), 1, &apLoc[1], &adwClip[1]);
               dwOr = adwClip[0] | adwClip[1];
               if (adwClip[0] & adwClip[1])
                  break;
            } // dwBit
            if (adwClip[0] & adwClip[1])
               continue;   // ends up being entirely clipped
         } // if should clip

         // add to list
         lInVolume.Add (&i);
      } // i
      break;

   case 2:  // side
   default:
      for (i = 0; ; i++) {
         PCPMSide ps = pm->m_PolyMesh.SideGet (i);
         if (!ps)
            break; // end of list

         // if doesn't require wholly in AND only selecting visible then automatically
         // add since will prune out later
         if (!m_fSelRegionWhollyIn && m_fSelRegionVisible) {
            lInVolume.Add (&i);
            continue;
         }

         // get all the points
         DWORD *padwVert = ps->VertGet();
         DWORD dwOr, dwAnd;
         dwOr = 0;
         dwAnd = -1;
         lPoint.Clear();
         for (j = 0; j < (DWORD)ps->m_wNumVert; j++) {
            PCPMVert pv = pm->m_PolyMesh.VertGet (padwVert[j]);
            if (!pv)
               continue;
            pLoc.Copy (&pv->m_pLocSubdivide);
            pLoc.p[3] = 1;
            pLoc.MultiplyLeft (&m);
            rc.CalcClipBits (-1, 1, &pLoc, &dwClip);
            cpp.dwOrigIndex = j;
            cpp.Point.Copy (&pLoc);
            cpp.dwClipBits = dwClip;
            lPoint.Add (&cpp);

            dwOr |= dwClip;
            dwAnd &= dwClip;
         } // j

         // if all clipped then trivial regect
         if (dwAnd)
            continue;

         // if need to do some clipping then do so
         if (dwOr) {
            if (m_fSelRegionWhollyIn)
               continue;   // if must be entirely in then continue
            if (!mem.Required (lPoint.Num() * sizeof(CLIPPOLYPOINT) * 60))
               continue;   // not enough memory

            DWORD dwDest;
            rc.ClipPolygon (FALSE, FALSE, FALSE, lPoint.Num(), (PCLIPPOLYPOINT) lPoint.Get(0),
               &dwDest, (PCLIPPOLYPOINT) mem.p);

            // if no points left entirely clipped
            if (!dwDest)
               continue;
         }

         // add
         lInVolume.Add (&i);

      } // i
      break;
   } // dwSelMode

   // if have flag set that should only select visible then do so
   if (m_fSelRegionVisible) {
      CListFixed lVis;
      lVis.Init (sizeof(DWORD));
      DWORD dwLast = -1;
      DWORD *padw;
      DWORD x,y;
      PIMAGEPIXEL pip;
      for (y = (DWORD)pr->top; y < (DWORD)pr->bottom; y++) {
         pip = m_aImage[0].Pixel ((DWORD)pr->left, y);
         for (x = (DWORD)pr->left; x < (DWORD)pr->right; x++, pip++) {
            if (HIWORD(pip->dwID) != dwFind+1)
               continue;   // not this object

            if (dwLast == pip->dwIDPart)
               continue;   // right object, but already checked this out so no diff

            dwLast = pip->dwIDPart;

            // see if the side is visible
            padw = (DWORD*) lVis.Get(0);
            for (j = 0; j < lVis.Num(); j++) {
               if (dwLast == padw[j])
                  break;
            } // j
            if (j >= lVis.Num())
               lVis.Add (&dwLast);  // new itam
         } // x
      } // y

      // sort so can do search
      DWORD dwNum;
      dwNum = lVis.Num();
      padw = (DWORD*) lVis.Get(0);
      qsort (lVis.Get(0), dwNum, sizeof(DWORD), BDWORDCompare);

      // go through all the elemnts inthe volume. They must be associated with a visible
      // side to be used...
      for (i = lInVolume.Num()-1; i < lInVolume.Num(); i--) {
         DWORD dwElem = *((DWORD*) lInVolume.Get(i));
         BOOL fRemove = FALSE;

         switch (dwSelMode) {
         case 0:  // vert
            {
               PCPMVert pv = pm->m_PolyMesh.VertGet (dwElem);
               DWORD *padwSide = pv->SideGet();
               for (j = 0; j < (DWORD)pv->m_wNumSide; j++) {
                  if (-1 == DWORDSearch (padwSide[j], dwNum, padw)) {
                     // cant find side
                     fRemove = TRUE;
                     break;
                  }
               } // j
            }
            break;

         case 1:  // edge
            {
               PCPMEdge pe = pm->m_PolyMesh.EdgeGet (dwElem);
               for (j = 0; j < 2; j++) {
                  if (pe->m_adwSide[j] == -1)
                     continue;   // not connected
                  if (-1 == DWORDSearch (pe->m_adwSide[j], dwNum, padw)) {
                     // cant find side
                     fRemove = TRUE;
                     break;
                  }
               } // j
            }
            break;

         case 2:  // side
         default:
            if (-1 == DWORDSearch (dwElem, dwNum, padw))
               fRemove = TRUE;
            break;
         }

         if (fRemove)
            lInVolume.Remove (i);

      } // i
   } // m_fSelRegionVIsible

   // whats left is a list of all the elements that need to be added or removed...
   // make two lists, one for add and one for remove, depending upon whether they're
   // in the list already
   CListFixed alAR[2];
   DWORD *padw;
   DWORD dwNum;
   alAR[0].Init (sizeof(DWORD) * ((dwSelMode == 1) ? 2 : 1));
   alAR[1].Init (sizeof(DWORD) * ((dwSelMode == 1) ? 2 : 1));
   padw = (DWORD*) lInVolume.Get(0);
   DWORD adwFind[2];
   for (i = 0; i < lInVolume.Num(); i++) {
      if (dwSelMode == 1) {
         PCPMEdge pe = pm->m_PolyMesh.EdgeGet (padw[i]);
         adwFind[0] = pe->m_adwVert[0];
         adwFind[1] = pe->m_adwVert[1];
      }
      else {
         adwFind[0] = padw[i];
         adwFind[1] = 0;
      }

      // see if can find
      if (-1 == pm->m_PolyMesh.SelFind (adwFind[0], adwFind[1]))
         alAR[0].Add (adwFind);
      else
         alAR[1].Add (adwFind);  // remove
   }

   pm->m_pWorld->ObjectAboutToChange (pm);

   // remove all elements that need to remove
   padw = (DWORD*) alAR[1].Get(0);
   dwNum = alAR[1].Num();
   for (i = 0; i < dwNum; i++) {
      if (dwSelMode == 1) {
         adwFind[0] = padw[i*2+0];
         adwFind[1] = padw[i*2+1];
      }
      else {
         adwFind[0] = padw[i];
         adwFind[1] = 0;
      }
      pm->m_PolyMesh.SelRemove (pm->m_PolyMesh.SelFind (adwFind[0], adwFind[1]));
   }

   // add ones supposed to add
   padw = (DWORD*) alAR[0].Get(0);
   dwNum = alAR[0].Num();
   for (i = 0; i < dwNum; i++) {
      if (dwSelMode == 1) {
         adwFind[0] = padw[i*2+0];
         adwFind[1] = padw[i*2+1];
      }
      else {
         adwFind[0] = padw[i];
         adwFind[1] = 0;
      }
      pm->m_PolyMesh.SelAdd (adwFind[0], adwFind[1]);
   }

   pm->m_pWorld->ObjectChanged (pm);

   // done
}


/*******************************************************************************
CHouseView::PolyMeshObjectFromPixel - This returns the object clicked on.
The type of object it's looking for depends upon whether or not the selection
mode is for vertices, edges, or sides.

This function (usually) first checks to see if there's a selection. If so,
m_adwPolyMeshMove will indicate that the selection is to be used. If not
then it uses the point clicked on.

This fills in: m_adwPolyMeshMove[2] with the information for the object.

inputs
   DWORD          dwX, dwY - X and Y where clicked on
   BOOL           fPrefSel - If TRUE then as long as the right object is clicked
                     on AND a selection exists, this function will return TRUE and fill m_adwPolyMeshMove
                     in with -1 to indicate the selection should be used.
                     If FALSE, the clicked on area is always used.
returns
   BOOL - TRUE if clicked on something valid and should continue, FALSE if not
*/
BOOL CHouseView::PolyMeshObjectFromPixel (DWORD dwX, DWORD dwY, BOOL fPrefSel)
{
   // make sure clicked on right object
   PIMAGEPIXEL pip = m_aImage[0].Pixel (dwX, dwY);
   DWORD dwFind;
   PCObjectPolyMesh pm = PolyMeshObject2 (&dwFind);
   if (!pm || (HIWORD(pip->dwID) != dwFind+1)) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return FALSE;
   }

   // if fPrefSel and selection then done
   DWORD dwNum;
   pm->m_PolyMesh.SelEnum (&dwNum);
   if (fPrefSel && dwNum) {
      m_adwPolyMeshMove[0] = m_adwPolyMeshMove[1] = -1;
      m_dwPolyMeshMoveSide = -1;
      return TRUE;
   }

   // see what clickes on
   m_adwPolyMeshMove[0] = m_adwPolyMeshMove[1] = -1;
   m_dwPolyMeshMoveSide = -1;
   switch (pm->m_PolyMesh.SelModeGet()) {
   case 0: // vert
      m_adwPolyMeshMove[0] = PolyMeshVertFromPixel (dwX, dwY);
      m_dwPolyMeshMoveSide = pip->dwIDPart;  // keep track of side
      if (m_adwPolyMeshMove[0] == -1) {
         BeepWindowBeep (ESCBEEP_DONTCLICK);
         return FALSE;
      }
      break;
   case 1: // edge
      if (!PolyMeshEdgeFromPixel (dwX, dwY, m_adwPolyMeshMove)) {
         BeepWindowBeep (ESCBEEP_DONTCLICK);
         return FALSE;
      }
      break;
   case 2: // side
      m_adwPolyMeshMove[0] = PolyMeshSideFromPixel (dwX, dwY);
      if (m_adwPolyMeshMove[0] == -1) {
         BeepWindowBeep (ESCBEEP_DONTCLICK);
         return FALSE;
      }
      break;
   default:
      return FALSE;
   }

   return TRUE;
}

/*******************************************************************************
CHouseView::PolyMeshPrepForDrag - Call this when a mouse is clicked on the
polymesh and things are about to be dragged. This:
   - Calculates the m_pScaleScrollX and m_pScaleScrollY values

inputs
   DWORD          dwX, dwY - X and Y where clicked on
   BOOL           fInOut - If TRUE then do in/out movement based on Y
   BOOL           fFailIfCantChange - If polymesh has more than one morph on then
                     this will fail
returns
   BOOL - TRUE if success, FALSE if should fail drag
*/
BOOL CHouseView::PolyMeshPrepForDrag (DWORD dwX, DWORD dwY, BOOL fInOut, BOOL fFailIfCantChange)
{
   // find out the z depth where clicked, that combined with the image width
   // and field of view will say how much need to scroll every pixel
   fp fZ;
   fZ = m_apRender[0]->PixelToZ (dwX, dwY);

   // get the object
   DWORD dwFind;
   PCObjectPolyMesh pm = PolyMeshObject2 (&dwFind);
   CMatrix m, mInv;
   if (!pm)
      return FALSE;
   pm->ObjectMatrixGet (&m);
   m.Invert4 (&mInv);

   // if click on empty space the use the center of the house for the z distance
   if (fZ > 1000) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return FALSE;
   }
   fp fZMax;
   fZMax = max(.01,fZ);   // at least .01 meter distance

   if (fFailIfCantChange && !pm->m_PolyMesh.CanModify()) {
      EscMessageBox (m_hWnd, ASPString(),
         L"You can't do this when more than one morph is turned on or bones are deforming the shape.",
         L"To turn the morph off, visit the \"Organic editing\" pane, "
         L"select which morph you wish to modify, and then return here. "
         L"To turn the bone deformation off modify the attributes of the skeleton "
         L"to their default values.",
         MB_ICONINFORMATION | MB_OK);
      return FALSE;
   }

   // clone the polymesh
   if (!m_pPolyMeshOrig)
      m_pPolyMeshOrig = new CPolyMesh;
   if (!m_pPolyMeshOrig)
      return FALSE;
   pm->m_PolyMesh.CloneTo (m_pPolyMeshOrig);

   m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &m_pPolyMeshClick);
   m_pPolyMeshClick.p[3] = 1;
   m_pPolyMeshClick.MultiplyLeft (&mInv);

   // Need this to convert from pixel to world space
   CPoint p1, p2;
   m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZMax, &p1);
   m_apRender[0]->PixelToWorldSpace (dwX+1, dwY, fZMax, &p2);
   p1.p[3] = p2.p[3] = 1;
   p1.MultiplyLeft (&mInv);
   p2.MultiplyLeft (&mInv);
   m_pScaleScrollX.Subtract (&p2, &p1);
   m_pScaleScrollX.Scale (1.0 * IMAGESCALE / (fp) m_dwImageScale);

   m_apRender[0]->PixelToWorldSpace (dwX, dwY+1, fZMax, &p2);
   p2.p[3] = 1;
   p2.MultiplyLeft (&mInv);
   m_pScaleScrollY.Subtract (&p2, &p1);
   m_pScaleScrollY.Scale (1.0 * IMAGESCALE / m_dwImageScale);

   if (fInOut) {
      // I'm specifically doing it the opposite way since up should
      // push it further back
      p2.CrossProd (&m_pScaleScrollY, &m_pScaleScrollX);
      p2.Scale (m_pScaleScrollX.Length() / p2.Length());
      m_pScaleScrollY.Copy (&p2);
      m_pScaleScrollX.Zero();
   }

   // remember this
   m_dwObjControlObject = dwFind;

   return TRUE;
}


/*******************************************************************************
CHouseView::PolyMeshSideSplitRect - Fills in a rectangle where the start and end
pixels (in CImage pixels) should be. Use this to draw the line.

It looks at the member variables m_adwPolyMeshSplit[2][2] to figure out where
the split is coming from and where it is going to. It also uses m_pntButtonDown
and m_pntButtonLast if no reasonable line can be drawn

inputs
   PCObjectPolyMesh     pm - Polymesh object
   RECT                 *pr - Filled with rectangle for start (UL) to end (BR)
returns
   BOOL - TRUE if should be red (indicating good match), FALSE if not good match
*/
BOOL CHouseView::PolyMeshSideSplitRect (PCObjectPolyMesh pm, RECT *pr)
{
   // default to location where originally clicked
   memset (pr, 0, sizeof(*pr));
   DWORD dwX, dwY;
   if (PointInImage (m_pntButtonDown.x, m_pntButtonDown.y, &dwX, &dwY)) {
      pr->left = (int)dwX;
      pr->top = (int)dwY;
   }
   if (PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY)) {
      pr->right = (int)dwX;
      pr->bottom = (int)dwY;
   }

   // see if can use coords from real point
   DWORD i;
   BOOL fGoodMatch = FALSE;
   for (i = 0; i < 2; i++) {
      if (m_adwPolyMeshSplit[i][0] == -1)
         continue;   // cant use

      // if it's the 2nd pass through then see if the splits have a common edge.
      // if not, draw the line to anywhere
      if (i) {
         fGoodMatch = (PolyMeshSideSplit(pm) != -1);
         if (!fGoodMatch)
            continue;
      }

      CPoint pLoc;

      if (m_adwPolyMeshSplit[i][1] == -1) {
         // point
         PCPMVert pv = pm->m_PolyMesh.VertGet (m_adwPolyMeshSplit[i][0]);
         if (!pv)
            continue;
         pLoc.Copy (&pv->m_pLocSubdivide);
      }
      else {
         // edge
         DWORD dwFind = pm->m_PolyMesh.EdgeFind (m_adwPolyMeshSplit[i][0], m_adwPolyMeshSplit[i][1]);
         if (dwFind == -1)
            continue;
         PCPMEdge pe = pm->m_PolyMesh.EdgeGet (dwFind);
         if (!pe)
            continue;
         PCPMVert pv1 = pm->m_PolyMesh.VertGet (pe->m_adwVert[0]);
         PCPMVert pv2 = pm->m_PolyMesh.VertGet (pe->m_adwVert[1]);
         if (!pv1 || !pv2)
            continue;
         pLoc.Average (&pv1->m_pLocSubdivide, &pv2->m_pLocSubdivide);
      }
      pLoc.p[3] = 1;

      // apply matrix to convert to world space
      CMatrix m;
      pm->ObjectMatrixGet (&m);
      pLoc.MultiplyLeft (&m);

      // convert this to screen coords
      fp fx, fy;
      m_apRender[0]->WorldSpaceToPixel (&pLoc, &fx, &fy);
      fx = max(0, fx);
      fx = min(m_aImage[0].Width()-1, fx);
      fy = max(0, fy);
      fy = min(m_aImage[0].Height()-1, fy);
      if (i) {
         pr->right = (int)fx;
         pr->bottom = (int)fy;
      }
      else {
         pr->left = (int)fx;
         pr->top = (int)fy;
      }
   } // i

   return fGoodMatch;
}

   
/*******************************************************************************
CHouseView::PolyMeshSideSplit - This returns the side that the given split
occurs on, or -1 if it's not on a side.

It looks at the member variables m_adwPolyMeshSplit[2][2] to figure out where
the split is coming from and where it is going to.
*/
DWORD CHouseView::PolyMeshSideSplit (PCObjectPolyMesh pm)
{
   // if either -1 then none
   if ((m_adwPolyMeshSplit[0][0] == -1) || (m_adwPolyMeshSplit[1][0] == -1))
      return -1;

   // if any of the sides are equal then dont do because would result in split
   // within nothing in split polygon
   DWORD i;
   for (i = 0; i < 2; i++) {
      if (m_adwPolyMeshSplit[i][1] == -1) {
         // compare point to edge
         if ((m_adwPolyMeshSplit[i][0] == m_adwPolyMeshSplit[!i][0]) ||
            (m_adwPolyMeshSplit[i][0] == m_adwPolyMeshSplit[!i][1]))
            return -1;
      }
      else {
         // compare entire edge
         if ((m_adwPolyMeshSplit[i][0] == m_adwPolyMeshSplit[!i][0]) &&
            (m_adwPolyMeshSplit[i][1] == m_adwPolyMeshSplit[!i][1]))
            return -1;
      }
   }

   // get the pointers for each
   PDWORD padwSide[2];
   DWORD adwNum[2];
   DWORD j;
   for (i = 0; i < 2; i++) {
      if (m_adwPolyMeshSplit[i][1] == -1) {
         // first is point
         PCPMVert pv = pm->m_PolyMesh.VertGet(m_adwPolyMeshSplit[i][0]);
         if (!pv)
            return FALSE;
         padwSide[i] = pv->SideGet();
         adwNum[i] = pv->m_wNumSide;
      }
      else {
         // first is edge
         pm->m_PolyMesh.CalcEdges();
         DWORD dwFind =pm->m_PolyMesh.EdgeFind (m_adwPolyMeshSplit[i][0], m_adwPolyMeshSplit[i][1]);
         if (dwFind == -1)
            return FALSE;  // error
         PCPMEdge pe = pm->m_PolyMesh.EdgeGet (dwFind);
         if (!pe)
            return FALSE;
         padwSide[i] = pe->m_adwSide;
         adwNum[i] = (pe->m_adwSide[1] == -1) ? 1 : 2;
         if (pe->m_adwSide[0] == -1) {
            padwSide[i] += 1;
            adwNum[i] += 1;
         }
      }
   } // i

   // now know what sides they're in, so figure out if there's a match
   DWORD dwMatch = -1;
   for (i = 0; i < adwNum[0]; i++)
      for (j = 0; j < adwNum[1]; j++)
         if ((padwSide[0])[i] == (padwSide[1])[j]) {
            dwMatch = (padwSide[0])[i];
            break;
         }

   if (dwMatch == -1)
      return -1;

   // if either is an edge then done
   if ((m_adwPolyMeshSplit[0][1] != -1) || (m_adwPolyMeshSplit[1][1] != -1))
      return dwMatch;

   // else, make sure that points not next to each other
   PCPMSide ps = pm->m_PolyMesh.SideGet(dwMatch);
   if (!ps)
      return -1;
   DWORD *padwVert;
   padwVert = ps->VertGet();
   for (i = 0; i < ps->m_wNumVert; i++) {
      if (padwVert[i] != m_adwPolyMeshSplit[0][0])
         continue;

      if ((padwVert[(i+1)%ps->m_wNumVert] == m_adwPolyMeshSplit[1][0]) ||
         (padwVert[(i+ps->m_wNumVert-1)%ps->m_wNumVert] == m_adwPolyMeshSplit[1][0]))
         return -1;  // adjacent points
      break;
   }

   // else, nothing in common
   return dwMatch;
}

/*******************************************************************************
CHouseView::PolyMeshVertToPixel - Takes a list of vertices and fills in another
list of a set of screen coordinates (in hWnd) for where the pixels are.

Uses m_lPolyMeshVert for the list of existing vertices.

inputs
   PCObjectPolyMesh  pm - Polymesh object
   POINT             pCursor - Current cursor point (in m_hWnd coords). This
                     sees if a polygon vertex is under it. If it is then it will
                     add another point to the polygon, else it will just add a
                     point to underneath the cursor.
   PCListFixed       plPoint - Initialized to sizeof (POINT), and filled in with
                     a list of points.
returns
   BOOL - TRUE if point at pCursor is over a vertex, FALSE if it isn't
*/
BOOL CHouseView::PolyMeshVertToPixel (PCObjectPolyMesh pm, POINT pCursor, PCListFixed plPoint)
{
   plPoint->Init (sizeof(POINT));
   if (!m_lPolyMeshVert.Num())
      return FALSE;  // done because without any points cant draw any lines...

   // get the object matrix
   CMatrix m;
   pm->ObjectMatrixGet (&m);

   // loop through all the points
   DWORD *padwVert = (DWORD*) m_lPolyMeshVert.Get(0);
   DWORD i;
   CPoint pLoc;
   fp fx, fy;
   POINT pt;
   for (i = 0; i < m_lPolyMeshVert.Num(); i++) {
      PCPMVert pv = pm->m_PolyMesh.VertGet (padwVert[i]);
      if (!pv)
         continue;

      pLoc.Copy (&pv->m_pLocSubdivide);
      pLoc.p[3] = 1;
      pLoc.MultiplyLeft (&m);

      m_apRender[0]->WorldSpaceToPixel (&pLoc, &fx, &fy);
      pt.x = (int)fx;
      pt.y = (int)fy;
      plPoint->Add (&pt);
   }

   // is current cursor over vertex
   DWORD dwVert = -1;
   DWORD dwX, dwY;
   if (PointInImage (pCursor.x, pCursor.y, &dwX, &dwY))
      dwVert = PolyMeshVertFromPixel (dwX, dwY);

   if (dwVert != -1) {
      PCPMVert pv = pm->m_PolyMesh.VertGet (dwVert);
      if (pv)
         pLoc.Copy (&pv->m_pLocSubdivide);
      else
         pLoc.Zero();

      pLoc.p[3] = 1;
      pLoc.MultiplyLeft (&m);

      m_apRender[0]->WorldSpaceToPixel (&pLoc, &fx, &fy);
      pt.x = (int)fx;
      pt.y = (int)fy;
      plPoint->Add (&pt);
   }

   // loop through and adjust all the points
   POINT *pp = (POINT*) plPoint->Get(0);
   for (i = 0; i < plPoint->Num(); i++, pp++) {
      pp->x  = pp->x * (int)m_dwImageScale / IMAGESCALE;
      pp->y  = pp->y * (int)m_dwImageScale / IMAGESCALE;

      // reposition
      pp->x += m_arThumbnailLoc[0].left;
      pp->y += m_arThumbnailLoc[0].top;
   }

   // finally, add last point
   if (dwVert == -1)
      plPoint->Add (&pCursor);
   return (dwVert != -1);
}


/*******************************************************************************
CHouseView::PolyMeshVertToRect - Takes a list of vertices and fills in a rectangle
that surrounds the vertices.

Uses m_lPolyMeshVert for the list of existing vertices.

inputs
   PCObjectPolyMesh  pm - Polymesh object
   POINT             pCursor - Current cursor point (in m_hWnd coords). This
                     sees if a polygon vertex is under it. If it is then it will
                     add another point to the polygon, else it will just add a
                     point to underneath the cursor.
   RECT              *pr - Rectangle
returns
   none
*/
void CHouseView::PolyMeshVertToRect (PCObjectPolyMesh pm, POINT pCursor, RECT *pr)
{
   pr->left = pr->right = pr->top = pr->bottom = 0;
   if (!m_lPolyMeshVert.Num())
      return;

   CListFixed lPoint;
   PolyMeshVertToPixel (pm, pCursor, &lPoint);
   POINT *pp;
   pp = (POINT*) lPoint.Get(0);
   DWORD i;

   for (i = 0; i < lPoint.Num(); i++, pp++) {
      if (i) {
         pr->left = min(pr->left, pp->x);
         pr->right = max(pr->right, pp->x);
         pr->top = min(pr->top, pp->y);
         pr->bottom = max(pr->bottom, pp->y);
      }
      else {
         pr->left = pr->right = pp->x;
         pr->top = pr->bottom = pp->y;
      }
   }

   if (lPoint.Num()) {
      // some extra
      pr->left -= 5;
      pr->right += 5;
      pr->top -= 5;
      pr->bottom += 5;
   }
}



/*****************************************************************************
CHouseView::PolyMeshBrushApply - Applies a brush based on its size, etc.

inputs
   DWORD       dwPointer - Pointer being used. This is one of IDC_BRUSHxxx.
   POINT       *pNew - New location, in client coords of m_hWnd
returns
   none
*/
void CHouseView::PolyMeshBrushApply (DWORD dwPointer, POINT *pNew)
{
   PIMAGEPIXEL pip;
   DWORD dwX, dwY;
   if (!PointInImage (pNew->x, pNew->y, &dwX, &dwY))
       return;
   pip = m_aImage[0].Pixel (dwX, dwY);
   BOOL fControl = (GetKeyState (VK_CONTROL) < 0);

   // get objectD
   DWORD dwFind;
   PCObjectPolyMesh pm = PolyMeshObject2 (&dwFind);
   if (!pm || (HIWORD(pip->dwID) != dwFind+1)) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return;
   }

   // get point in world space
   CPoint p, p2;
   fp fPixelLen;
   m_apRender[0]->PixelToWorldSpace (dwX, dwY, pip->fZ, &p);
   m_apRender[0]->PixelToWorldSpace (dwX+1, dwY, pip->fZ, &p2);

   // convert this point to object space
   CMatrix m, mInv;
   pm->ObjectMatrixGet (&m);
   m.Invert4 (&mInv);
   p.p[3] = p2.p[3] = 1;
   p.MultiplyLeft (&mInv);
   p2.MultiplyLeft (&mInv);
   p2.Subtract (&p); // so know length
   fPixelLen = p2.Length();

   // figure out the list of sides around the point...
   DWORD dwBrushSize;
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
   dwBrushSize = dwBrushSize * IMAGESCALE / m_dwImageScale;

   int x, y, xx, yy;
   DWORD dwWidth, dwHeight;
   dwWidth = m_aImage[0].Width();
   dwHeight = m_aImage[0].Height();
   CListFixed lSide;
   DWORD dwLast = -1;
   lSide.Init (sizeof(DWORD));
   for (y = 0; y < (int) dwBrushSize; y++) for (x = 0; x < (int)dwBrushSize; x++) {
      // circle around
      xx = x - (int)dwBrushSize/2;
      yy = y - (int)dwBrushSize/2;
      xx = xx * xx + yy * yy;
      if  (xx > (int)dwBrushSize * (int)dwBrushSize / 4)
         continue;

      // figure out pixel
      xx = x - (int)dwBrushSize/2 + (int)dwX;
      yy = y - (int)dwBrushSize/2 + (int)dwY;
      if ((xx < 0) || (xx >= (int)dwWidth) || (yy < 0) || (yy >= (int)dwHeight))
         continue;

      pip = m_aImage[0].Pixel((DWORD)xx, (DWORD)yy);
      if (HIWORD(pip->dwID) != dwFind+1)
         continue;
      if (pip->dwIDPart == dwLast)
         continue;   // already did
      dwLast = pip->dwIDPart;
      lSide.Add (&dwLast);
   }

   // figure out radius
   fp fRadius = fPixelLen * (fp)dwBrushSize / 2.0;

   // brush power
   fp fPow;
   switch (m_dwPolyMeshBrushPoint) {
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
   default:
      fPow = 5;
      break;
   }

   // brush strength
   fp fStrength;
   fStrength = (fp) m_dwPolyMeshBrushStrength / 10.0;
   fStrength = pow(fStrength, 2);
   // If airbrush then weaker strength
   fStrength /= ((fp) AIRBRUSHPERSEC/2.0);

   CPoint pMove;
   pMove.Zero();
   DWORD dwMovePix = max(dwWidth, dwHeight) / 16;
   DWORD dwAffect;
   switch (m_dwViewSub) {
   case VSPOLYMODE_ORGANIC:  // Organic
   default:
      dwAffect = m_dwPolyMeshBrushAffectOrganic;
      break;
   case VSPOLYMODE_MORPH:  // morph
      dwAffect = m_dwPolyMeshBrushAffectMorph;
      break;
   case VSPOLYMODE_TEXTURE:  // texture
      dwAffect = 0;
      break;
   case VSPOLYMODE_TAILOR:  // tailor
      dwAffect = m_dwPolyMeshBrushAffectTailor;
      break;
   case VSPOLYMODE_BONE:
      dwAffect = m_dwPolyMeshBrushAffectBone;
      break;
   }
   

   if (fControl)
      dwAffect = (dwAffect / 2 * 2) + !(dwAffect % 2);   // flip lower bit
   if (m_dwViewSub == VSPOLYMODE_ORGANIC) switch (dwAffect) {
      case 0:  // out by normal
      case 1:  // in by normal
         pMove.p[0] = fPixelLen * dwMovePix; // full strength
         break;
      case 2:  // towards viewer
      case 3:  // away from viewer
         {
            m_apRender[0]->PixelToWorldSpace (dwX, dwY,
               (m_apRender[0]->CameraModelGet() == CAMERAMODEL_FLAT) ? -1000 : .001, &pMove);
            pMove.p[3] = 1;
            pMove.MultiplyLeft (&mInv);

            pMove.Subtract (&p);
            if (dwAffect == 3)   // if move away from viewer then opposite direction
               pMove.Scale(-1);
            pMove.Normalize();
            pMove.Scale (fPixelLen * dwMovePix); // full strength
         }
         break;
      };

   // do the effect
   BOOL fRet = FALSE;
   if (m_dwViewSub != VSPOLYMODE_TAILOR)
      pm->m_pWorld->ObjectAboutToChange (pm);
   switch (m_dwViewSub) {
   case VSPOLYMODE_ORGANIC:  // organic
   default:
      fRet = pm->m_PolyMesh.OrganicMove (&p, fRadius, fPow, fStrength,
         lSide.Num(), (DWORD*)lSide.Get(0), dwAffect, &pMove,
         pm, m_dwPolyMeshMaskColor, m_fPolyMeshMaskInvert);
      break;
   case VSPOLYMODE_MORPH:  // morph
      fRet = pm->m_PolyMesh.MorphPaint (&p, fRadius, fPow, fStrength,
         lSide.Num(), (DWORD*)lSide.Get(0), dwAffect, 2);
      break;
   case VSPOLYMODE_BONE:
      if ((dwAffect == 2) || (dwAffect == 3))
         fStrength *= ((fp) AIRBRUSHPERSEC/2.0);   // counteract weakening due to airbrush
      fRet = pm->m_PolyMesh.BonePaint (&p, fRadius, fPow, fStrength,
         lSide.Num(), (DWORD*)lSide.Get(0), dwAffect, pm->m_pWorld);
      break;
   case VSPOLYMODE_TEXTURE:  // texture
      {
         DWORD dwSurface = pm->ObjectSurfaceGetIndex (m_dwPolyMeshCurText);
         if (dwSurface != -1)
            fRet = pm->m_PolyMesh.SurfacePaint (lSide.Num(), (DWORD*)lSide.Get(0), dwSurface);
      }
      break;
   case VSPOLYMODE_TAILOR:  // tailor
      {
         fRet = TRUE;

         // get the current selection
         DWORD i;
         DWORD dwSelMode = pm->m_PolyMesh.SelModeGet();
         if (dwSelMode != 2) {
            // if not in side selection then set that
            pm->m_pWorld->ObjectAboutToChange(pm);
            pm->m_PolyMesh.SelModeSet (2);
            pm->m_pWorld->ObjectChanged(pm);
         }

         DWORD dwSelNum, *padwSel;
         padwSel = pm->m_PolyMesh.SelEnum (&dwSelNum);

         // convert current list of sides to symmetry
         CListFixed lSym;
         lSym.Init (sizeof(DWORD));
         pm->m_PolyMesh.MirrorSide ((DWORD*)lSide.Get(0), lSide.Num(), &lSym);

         // compare this against the current selection and figure out if should add/remove
         CListFixed lModify;
         lModify.Init (sizeof(DWORD));
         DWORD *padwSym, dwSymNum;
         padwSym = (DWORD*)lSym.Get(0);
         dwSymNum = lSym.Num();
         DWORD dwSelIndex = 0;
         while (dwSelNum || dwSymNum) {
            if (!dwSymNum)
               break;   // no point because nothing left in list to add/remove

            // if no selection left, but have symmetry then need to add the
            // symmetry to list of things that want to select
            if (!dwSelNum) {
               if (dwAffect == 0) // adding, so anything not in list gets added
                  lModify.Add (&padwSym[0]);
               // if dwAffect ==1 then removing, but no point because not selected
               padwSym++;
               dwSymNum--;
               continue;
            }

            // if get here, have both selectin and symmetry
            if (padwSel[0] < padwSym[0]) {
               // selection doesnt exist in symmetry list so skip
               padwSel++;
               dwSelNum--;
               dwSelIndex++;
               continue;
            }
            else if (padwSel[0] > padwSym[0]) {
               if (dwAffect == 0)
                  // symmetry doesnt exist in selection so add
                  lModify.Add (&padwSym[0]);
               // but if removing then nothing to remove so ignore
               padwSym++;
               dwSymNum--;
               continue;
            }

            // else, if get here it's an exact match, so skip both
            if (dwAffect == 1) // if removing then want to add to removal list
               lModify.Add (&dwSelIndex); // note: adding selection index
            // else if adding do nothing
            padwSel++;
            padwSym++;
            dwSelNum--;
            dwSymNum--;
            dwSelIndex++;
         } // while

         // lModify is the list of sides to add/remove
         if (!lModify.Num())
            return;  // nothing changed

         // else, going to change
         padwSel = (DWORD*)lModify.Get(0);
         dwSelNum = lModify.Num();
         pm->m_pWorld->ObjectAboutToChange (pm);
         if (dwAffect == 0) {
            for (i = 0; i < dwSelNum; i++)
               pm->m_PolyMesh.SelAdd (padwSel[i], 0);
         }
         else {
            // loop backwards
            for (i = dwSelNum-1; i < dwSelNum; i--)
               pm->m_PolyMesh.SelRemove (padwSel[i]);
         }
      }
      break;
   }
   pm->m_pWorld->ObjectChanged (pm);

   if (!fRet) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return;
   }
}

/**********************************************************************************
CHouseView::PolyMeshObjectMerge - Merges the selected object with the current polymesh.

inputs
   DWORD    dwMerge - Merges this object with the polymesh object

returns
   BOOL - TRUE if OK. FALSE if error
*/
BOOL CHouseView::PolyMeshObjectMerge (DWORD dwMerge)
{
   DWORD dwFind;
   PCObjectPolyMesh pm = PolyMeshObject2(&dwFind);
   if (!pm)
      return FALSE;
   if (dwFind == dwMerge) {
      EscMessageBox (m_hWnd, ASPString(),
         L"Click on a different object.",
         L"You must click on the object you wish to merge into the polygon mesh.",
         MB_ICONINFORMATION | MB_OK);
      return FALSE;
   }

   // create a list of the guids for these
   GUID g;
   PCObjectSocket pos = m_pWorld->ObjectGet (dwMerge);
   if (!pos)
      return FALSE;
   pos->GUIDGet (&g);

   // set undo state
   m_pWorld->UndoRemember ();

   // Call into each and tell to merge
   BOOL fRet;
   fRet = pm->Merge (&g, 1);

   if (!fRet)
      EscMessageBox (m_hWnd, ASPString(),
         L"The object didn't merge.",
         L"It may not have been a polygon mesh object.",
         MB_ICONINFORMATION | MB_OK);
   else
      EscChime (ESCCHIME_INFORMATION);

   // set undo state
   m_pWorld->UndoRemember ();

   return TRUE;
}


/**********************************************************************************
CHouseView::PolyMeshCreateAndFillList - Creates (or destroys) and fills the list
box based upon the current sub mode
*/
void CHouseView::PolyMeshCreateAndFillList (void)
{
   // determine if actually want the list
   BOOL fWant = FALSE;
   if (m_dwViewWhat == VIEWWHAT_POLYMESH)
      switch (m_dwViewSub) {
         case VSPOLYMODE_MORPH:  // morph
         case VSPOLYMODE_BONE:  // bone
         case VSPOLYMODE_TEXTURE:  // texture
            fWant = TRUE;
            break;
      }

   // if dont want it displayed and it is then hide
   if (!fWant && m_hWndPolyMeshList) {
      DestroyWindow (m_hWndPolyMeshList);
      m_hWndPolyMeshList = NULL;
      InvalidateRect (m_hWnd, NULL, FALSE);

      RECT r;
      GetClientRect (m_hWnd, &r);
      SendMessage (m_hWnd, WM_SIZE, 0, MAKELPARAM(r.right-r.left,r.bottom-r.top));
      return;
   }

   // if want it displayed and it's not then show
   if (fWant && !m_hWndPolyMeshList) {
      // create the font
      if (!m_hPolyMeshFont) {
         LOGFONT lf;
         memset (&lf, 0, sizeof(lf));
         lf.lfHeight = -12;   // 10 pixels high MulDiv(iPointSize, EscGetDeviceCaps(hDC, LOGPIXELSY), 72); 
         lf.lfCharSet = DEFAULT_CHARSET;
         lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
         lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
         strcpy (lf.lfFaceName, "Arial");
         lf.lfWeight = FW_BOLD;
         m_hPolyMeshFont = CreateFontIndirect (&lf);
      }

      m_hWndPolyMeshList = CreateWindowEx (WS_EX_CLIENTEDGE, "LISTBOX", "",
         WS_CHILD | WS_VSCROLL | WS_VISIBLE |
         LBS_DISABLENOSCROLL | LBS_NOTIFY |
         LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED,
         0, 0, 1, 1, m_hWnd,
         (HMENU) IDC_TEXTLIST, ghInstance, NULL);

      InvalidateRect (m_hWnd, NULL, FALSE);

      RECT r;
      GetClientRect (m_hWnd, &r);
      SendMessage (m_hWnd, WM_SIZE, 0, MAKELPARAM(r.right-r.left,r.bottom-r.top));
   }

   if (!m_hWndPolyMeshList)
      return;

   // get the polymesh
   PCObjectPolyMesh pm = PolyMeshObject2 ();
   if (!pm)
      return;

   // clear the list box
   DWORD i;
   SendMessage (m_hWndPolyMeshList, LB_RESETCONTENT, 0,0);


   // assuming m_dwViewSub == VSPOLYMODE_MORPH, for morph
   int iRet;
   char szTemp[128];
   // Set the selection based on current attributes...
   if (m_dwViewSub == VSPOLYMODE_MORPH) { // morph
      PCOEAttrib *ppa = (PCOEAttrib*) pm->m_PolyMesh.m_lPCOEAttrib.Get(0);
      for (i = 0; i < pm->m_PolyMesh.m_lPCOEAttrib.Num(); i++) {
         WideCharToMultiByte (CP_ACP, 0, ppa[i]->m_szName, -1, szTemp, sizeof(szTemp), 0,0);

         if (ppa[i]->m_dwType != 2)
            strcat (szTemp, "\r\n(Combination morph)");

         SendMessage (m_hWndPolyMeshList, LB_ADDSTRING, 0, (LPARAM) szTemp);
      } // i

      // option for modifying base
      iRet = SendMessage (m_hWndPolyMeshList, LB_ADDSTRING, 0, (LPARAM) "(Modify the basic shape without any morphs.)");

      // and options to add
      iRet = SendMessage (m_hWndPolyMeshList, LB_ADDSTRING, 0, (LPARAM) "(Double-click to add a new morph...)");

      m_dwPolyMeshCurMorph = pm->m_PolyMesh.MorphStateGet ();
      // take outif (dwMorph == -1)
      //   dwMorph = pm->m_PolyMesh.m_lPCOEAttrib.Num();
      SendMessage (m_hWndPolyMeshList, LB_SETCURSEL, (WPARAM) m_dwPolyMeshCurMorph, 0);
   }
   else if (m_dwViewSub == VSPOLYMODE_TEXTURE) { // texture
      DWORD dwNum = pm->ObjectSurfaceNumIndex();
      for (i = 0; i < dwNum; i++) {
         DWORD dwID = pm->ObjectSurfaceGetIndex (i);
         PCObjectSurface pos = pm->ObjectSurfaceFind (dwID);

         strcpy (szTemp, "(Solid color)");

         WCHAR szName[128];
         if (pos->m_fUseTextureMap)
            if (TextureNameFromGUIDs(DEFAULTRENDERSHARD, &pos->m_gTextureCode, &pos->m_gTextureSub, NULL, NULL, szName))
               WideCharToMultiByte (CP_ACP, 0, szName, -1, szTemp, sizeof(szTemp), 0,0);

         SendMessage (m_hWndPolyMeshList, LB_ADDSTRING, 0, (LPARAM) szTemp);
      } // i

      // and options to add
      iRet = SendMessage (m_hWndPolyMeshList, LB_ADDSTRING, 0, (LPARAM) "(Double-click to add a new texture...)");

      if (dwNum)
         dwNum--;
      m_dwPolyMeshCurText = min(m_dwPolyMeshCurText, dwNum);
      SendMessage (m_hWndPolyMeshList, LB_SETCURSEL, (WPARAM) m_dwPolyMeshCurText, 0);

      // if displaying the mesh should invalidate the window while here
      if (m_fPolyMeshTextDisp)
         InvalidateRect (m_hWnd, &m_arThumbnailLoc[0], FALSE);
   }
   else if (m_dwViewSub == VSPOLYMODE_BONE) {
      DWORD dwNum;
      PPMBONEINFO pbi = pm->m_PolyMesh.BoneEnum (&dwNum);
      for (i = 0; i < dwNum; i++) {
         strcpy (szTemp, "(Unknown)");

         if (pbi[i].szName[0])
            WideCharToMultiByte (CP_ACP, 0, pbi[i].szName, -1, szTemp, sizeof(szTemp), 0,0);

         SendMessage (m_hWndPolyMeshList, LB_ADDSTRING, 0, (LPARAM) szTemp);
      } // i

      // and options to add
      iRet = SendMessage (m_hWndPolyMeshList, LB_ADDSTRING, 0,
         dwNum ?
            (LPARAM) "Click to display all bones." :
            (LPARAM) "No bones available.");

      DWORD dwSel = pm->m_PolyMesh.BoneDisplayGet();
      dwSel = min(dwSel, dwNum);
      SendMessage (m_hWndPolyMeshList, LB_SETCURSEL, (WPARAM) dwSel, 0);
   }
}




/****************************************************************************
PolyMeshTexturePage
*/
BOOL PolyMeshTexturePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCHouseView pv = (PCHouseView)pPage->m_pUserData;
   PCObjectPolyMesh pm = pv->PolyMeshObject2();

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         if (!pm)
            return TRUE;

         PCEscControl pControl;
         if (pm->ObjectSurfaceNumIndex()<2) {
            pControl = pPage->ControlFind (L"delete");
            if (pControl)
               pControl->Enable (FALSE);
            pControl = pPage->ControlFind (L"combodel");
            if (pControl)
               pControl->Enable (FALSE);
         }

         pControl = pPage->ControlFind (L"inmeters");
         DWORD dwSurface = pm->ObjectSurfaceGetIndex(pv->m_dwPolyMeshCurText);
         pControl->AttribSetBOOL (Checked(), pm->m_PolyMesh.SurfaceInMetersGet(dwSurface));


         // will need to fill list box for delete
         DWORD i;
         pControl = pPage->ControlFind (L"combodel");
         MemZero (&gMemTemp);
         for (i = 0; i < pm->ObjectSurfaceNumIndex(); i++) {
            if (i == pv->m_dwPolyMeshCurText)
               continue;   // cant change to self
            dwSurface = pm->ObjectSurfaceGetIndex(i);
            if (dwSurface == -1)
               continue;
            PCObjectSurface pSurf = pm->ObjectSurfaceFind (dwSurface);
            if (!pSurf)
               continue;

            WCHAR szName[64];
            wcscpy (szName, L"(Solid color)");
            if (pSurf)
               TextureNameFromGUIDs(DEFAULTRENDERSHARD, &pSurf->m_gTextureCode, &pSurf->m_gTextureSub, NULL, NULL, szName);

            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L">");
            MemCatSanitize (&gMemTemp, szName);
            MemCat (&gMemTemp, L"</elem>");
         } // i
         ESCMCOMBOBOXADD add;
         memset (&add, 0, sizeof(add));
         add.dwInsertBefore = 0;
         add.pszMML = (PWSTR)gMemTemp.p;
         if (pm->ObjectSurfaceNumIndex() && pControl) {
            pControl->Message (ESCM_COMBOBOXADD, &add);
            pControl->AttribSetInt (CurSel(), 0);
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!pm)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"delete")) {
            // get the current selection
            PCEscControl pControl = pPage->ControlFind (L"combodel");
            if (!pControl)
               return TRUE;
            ESCMCOMBOBOXGETITEM get;
            memset (&get, 0, sizeof(get));
            get.dwIndex = (DWORD)pControl->AttribGetInt (CurSel());
            if (!pControl->Message (ESCM_COMBOBOXGETITEM, &get))
               return TRUE;
            if (!get.pszName)
               return TRUE;
            DWORD dwIndex = _wtoi(get.pszName);

            // which is which surface
            DWORD dwSurfaceTo = pm->ObjectSurfaceGetIndex(dwIndex);
            if (dwSurfaceTo == -1)
               return TRUE;

            pm->m_pWorld->ObjectAboutToChange (pm);

            // rename existing
            pm->m_PolyMesh.SurfaceRename (pv->m_dwPolyMeshCurText, dwSurfaceTo);

            // delete more
            pm->m_PolyMesh.SurfaceInMetersSet (pv->m_dwPolyMeshCurText, TRUE);
            pm->ObjectSurfaceRemove (pv->m_dwPolyMeshCurText);
            pv->m_dwPolyMeshCurText = 0;  // reset to top

            pm->m_pWorld->ObjectChanged (pm);

            pPage->Exit (Back());
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"change")) {
            // add
            DWORD dwSurface = pm->ObjectSurfaceGetIndex(pv->m_dwPolyMeshCurText);
            PCObjectSurface pSurf = pm->ObjectSurfaceFind(dwSurface);
            if (pSurf)
               pSurf = pSurf->Clone();
            if (!pSurf)
               return TRUE;
            if (!TextureSelDialog (DEFAULTRENDERSHARD, pPage->m_pWindow->m_hWnd, pSurf, pm->m_pWorld)) {
               delete pSurf;
               return TRUE;
            }

            // add it
            pm->m_pWorld->ObjectAboutToChange(pm);
            pSurf->m_dwID = dwSurface;
            pm->ObjectSurfaceRemove (dwSurface);
            pm->ObjectSurfaceAdd (pSurf);
            pm->m_pWorld->ObjectChanged(pm);
            delete pSurf;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"inmeters")) {
            // add
            DWORD dwSurface = pm->ObjectSurfaceGetIndex(pv->m_dwPolyMeshCurText);

            // add it
            pm->m_pWorld->ObjectAboutToChange(pm);
            pm->m_PolyMesh.SurfaceInMetersSet (dwSurface, p->pControl->AttribGetBOOL(Checked()));
            pm->m_pWorld->ObjectChanged(pm);
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


/**********************************************************************************
CHouseView::PolyMeshListBox - Handles the list box calls for polymesh.
*/
LRESULT CHouseView::PolyMeshListBox (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   PCObjectPolyMesh pm = PolyMeshObject2();

   switch (uMsg) {
   case WM_MEASUREITEM:
      if (wParam == IDC_TEXTLIST) {
         PMEASUREITEMSTRUCT pmi = (PMEASUREITEMSTRUCT) lParam;
         pmi->itemHeight = THUMBSHRINK + TBORDER;
         return TRUE;
      }
      break;

   case WM_DRAWITEM:
      if (wParam == IDC_TEXTLIST) {
         // BUGFIX - If no polymesh object then exit
         if (!pm)
            break;

         PDRAWITEMSTRUCT pdi = (PDRAWITEMSTRUCT) lParam;

         // get the item
         PCOEAttrib *ppa = NULL;
         PCObjectSurface pos = NULL;
         PPMBONEINFO pbi = NULL;
         DWORD dwNum = 0;
         PCOEAttrib poa = NULL;
         if (m_dwViewSub == VSPOLYMODE_MORPH) { // morph
            dwNum = pm->m_PolyMesh.m_lPCOEAttrib.Num();
            ppa = (PCOEAttrib*) pm->m_PolyMesh.m_lPCOEAttrib.Get(0);;
            poa = ((DWORD)pdi->itemID < dwNum) ? ppa[pdi->itemID] : NULL;
         }
         else if (m_dwViewSub == VSPOLYMODE_TEXTURE) { // texture
            dwNum = pm->ObjectSurfaceNumIndex();
            pos = (pdi->itemID < dwNum) ? pm->ObjectSurfaceFind(pm->ObjectSurfaceGetIndex(pdi->itemID)) : NULL;
         }
         else if (m_dwViewSub == VSPOLYMODE_BONE) {
            pbi = pm->m_PolyMesh.BoneEnum (&dwNum);
            if (pdi->itemID < dwNum)
               pbi += pdi->itemID;
            else
               pbi = NULL;
         }

         BOOL fSel;
         // NOTE: can't do the first fSel because the lsit box doesnt draw correctly
         // when do that

         // because list boxes don't tell us if the selection has changed, use
         // this as a technique to know
         //DWORD dwSel;
         //dwSel = (DWORD) SendMessage (pdi->hwndItem, LB_GETCURSEL, 0, 0);
         //if (dwSel < dwNum)
         // m_dwPolyMeshCurMorph = dwSel
         if (m_dwViewSub == VSPOLYMODE_MORPH) { // morph
            if ((pdi->itemState & ODS_SELECTED) && ((DWORD)pdi->itemID <= dwNum) && (m_dwPolyMeshCurMorph != -2))
               m_dwPolyMeshCurMorph = (DWORD)pdi->itemID;

            // if single click on state then need to to attrib set
            DWORD dwMorph = pm->m_PolyMesh.MorphStateGet ();
            if ((m_dwPolyMeshCurMorph <= dwNum) && (m_dwPolyMeshCurMorph != dwMorph)) {
               // change object
               pm->m_pWorld->ObjectAboutToChange (pm);
               pm->m_PolyMesh.MorphStateSet (dwMorph = m_dwPolyMeshCurMorph);
               pm->m_pWorld->ObjectChanged (pm);
               InvalidateRect (m_hWndPolyMeshList, NULL, FALSE);   // refresh display
            }
            //fSel = ((DWORD) pdi->itemID == m_dwPolyMeshCurMorph) && ((DWORD)pdi->itemID < dwNum);
            //fSel = pdi->itemState & ODS_SELECTED;
            fSel = (dwMorph == (DWORD) pdi->itemID);   // must be selected morph
         }
         else if (m_dwViewSub == VSPOLYMODE_TEXTURE) { // texture
            if ((pdi->itemState & ODS_SELECTED) && ((DWORD)pdi->itemID < dwNum) && (m_dwPolyMeshCurText != (DWORD)pdi->itemID)) {
               m_dwPolyMeshCurText = (DWORD)pdi->itemID;

               // if displaying the mesh should invalidate the window while here
               if (m_fPolyMeshTextDisp)
                  InvalidateRect (m_hWnd, &m_arThumbnailLoc[0], FALSE);
            }

            fSel = pdi->itemState & ODS_SELECTED;
         }
         else if (m_dwViewSub == VSPOLYMODE_BONE) {
            DWORD dwCurBone = pm->m_PolyMesh.BoneDisplayGet();
            DWORD dwBoneWant = dwCurBone;
            if (pdi->itemState & ODS_SELECTED) {
               dwBoneWant = (DWORD)pdi->itemID;
               if (dwBoneWant >= dwNum)
                  dwBoneWant = -1;
            }

            // if single click on state then need to to attrib set
            if (dwCurBone != dwBoneWant) {
               // change object
               pm->m_pWorld->ObjectAboutToChange (pm);
               pm->m_PolyMesh.BoneDisplaySet (dwBoneWant);
               pm->m_pWorld->ObjectChanged (pm);
               InvalidateRect (m_hWndPolyMeshList, NULL, FALSE);   // refresh display
            }
            //fSel = ((DWORD) pdi->itemID == m_dwPolyMeshCurMorph) && ((DWORD)pdi->itemID < dwNum);
            fSel = pdi->itemState & ODS_SELECTED;
            //fSel = (dwMorph == (DWORD) pdi->itemID);   // must be selected morph
         }


         // bacground
         HBRUSH hbr;
         HDC hDC;
         hDC = pdi->hDC;
         hbr = CreateSolidBrush (fSel ? RGB(0xc0,0xc0,0xc0) : RGB(0xff,0xff,0xff));
         FillRect (hDC, &pdi->rcItem, hbr);
         DeleteObject (hbr);

         // Draw the color as it appears in the edit window
         RECT r;
         if (m_dwViewSub == VSPOLYMODE_MORPH) { // morph
            if (poa && (poa->m_dwType == 2) && poa->m_lCOEATTRIBCOMBO.Num()) {
               PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) poa->m_lCOEATTRIBCOMBO.Get(0);

               COLORREF cr;
               cr = MapColorPicker (pc->dwMorphID);
               hbr = CreateSolidBrush (cr);
               r = pdi->rcItem;
               r.left += TBORDER/4;
               r.top += TBORDER/4;
               r.bottom -= TBORDER/4;
               r.right = r.left + THUMBSHRINK + TBORDER/2;
               FillRect (hDC, &r, hbr);
               DeleteObject (hbr);
            }
            else
               poa = NULL; // so ont draw box
         }
         else if (m_dwViewSub == VSPOLYMODE_BONE) {
            if (pbi) {
               COLORREF cr;
               cr = MapColorPicker (pdi->itemID);
               hbr = CreateSolidBrush (cr);
               r = pdi->rcItem;
               r.left += TBORDER/4;
               r.top += TBORDER/4;
               r.bottom -= TBORDER/4;
               r.right = r.left + THUMBSHRINK + TBORDER/2;
               FillRect (hDC, &r, hbr);
               DeleteObject (hbr);
            }
            else
               pbi = NULL; // so ont draw box
         }
         else if ((m_dwViewSub == VSPOLYMODE_TEXTURE) && pos) { // texture
            // outline
            COLORREF cr;
            cr = pos->m_fUseTextureMap ? RGB(0x40,0x40,0x40) : pos->m_cColor;
            hbr = CreateSolidBrush (cr);
            r = pdi->rcItem;
            r.left += TBORDER/4;
            r.top += TBORDER/4;
            r.bottom -= TBORDER/4;
            r.right = r.left + THUMBSHRINK + TBORDER/2;
            FillRect (hDC, &r, hbr);
            DeleteObject (hbr);

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
         } // if display texture

         // text
         char szText[256];
         szText[0] = 0;
         if (pdi->itemID <= dwNum+1)
            SendMessage (pdi->hwndItem, LB_GETTEXT, (WPARAM) pdi->itemID, (LPARAM)&szText[0]);
         if (szText[0]) {
            HFONT hFontOld;
            COLORREF crOld;
            int iOldMode;

            // figure out where to draw
            RECT r;
            r = pdi->rcItem;
            if (poa || pos || pbi)
               r.left += THUMBSHRINK + TBORDER;
            else
               r.left += TBORDER;
            r.top += TBORDER/2;
            r.bottom -= TBORDER/2;
            r.right -= TBORDER/2;

            hFontOld = (HFONT) SelectObject (hDC, m_hPolyMeshFont);
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

   case WM_COMMAND:
      switch (LOWORD(wParam)) {

      case IDC_TEXTLIST:
         if (HIWORD(wParam) == LBN_DBLCLK) {
            DWORD dwSel = (DWORD) SendMessage (m_hWndPolyMeshList, LB_GETCURSEL, 0,0);
            DWORD dwNum = 0;
            BOOL fCreateDialog = TRUE;
            if (m_dwViewSub == VSPOLYMODE_MORPH) { // morph
               dwNum = pm->m_PolyMesh.m_lPCOEAttrib.Num();
               if (dwSel == dwNum)
                  return 0;   // cant double click on this one and expect to do anything
            }
            else if (m_dwViewSub == VSPOLYMODE_TEXTURE) { // texture
               dwNum = pm->ObjectSurfaceNumIndex ();

               if (dwSel >= dwNum)
                  fCreateDialog = FALSE;
            }
            else if (m_dwViewSub == VSPOLYMODE_BONE) {
               pm->m_PolyMesh.BoneEnum (&dwNum);
               if (dwSel >= dwNum)
                  return 0;   // cant double click on this one and expect to do anything
            }

            // bring up the dialog
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (m_hWnd, &r);

            if (fCreateDialog)
               cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);

            pm->m_pWorld->UndoRemember();

            if (m_dwViewSub == VSPOLYMODE_MORPH) { // morph
               // because will be bringing up dialog, set m_dwPolyMeshCurMorph to special
               // flag so that won't make any state changes
               m_dwPolyMeshCurMorph = -2;

               // call into dialogs
               if (dwSel < dwNum)
                  pm->m_PolyMesh.DialogEditMorph (&cWindow, pm->m_pWorld, pm, dwSel);
               else
                  pm->m_PolyMesh.DialogAddMorph (&cWindow, pm->m_pWorld, pm);
            }
            else if (m_dwViewSub == VSPOLYMODE_BONE) {
               pm->m_PolyMesh.DialogEditBone (&cWindow, pm->m_pWorld, pm, dwSel);
            }
            else if (m_dwViewSub == VSPOLYMODE_TEXTURE) { // texture
               if (dwSel < dwNum) {
                  m_dwPolyMeshCurText = dwSel;
                  cWindow.PageDialog (ghInstance, IDR_MMLPOLYMESHTEXTURE, PolyMeshTexturePage, this);
               }
               else {
                  // add
                  PCObjectSurface pSurf = pm->ObjectSurfaceFind(pm->ObjectSurfaceGetIndex(0));
                  if (pSurf)
                     pSurf = pSurf->Clone();
                  if (!pSurf) {
                     pSurf = new CObjectSurface;
                     pSurf->m_fUseTextureMap = FALSE;
                     pSurf->m_cColor = RGB(0xff,0xff,0xff);
                  }
                  if (!TextureSelDialog (DEFAULTRENDERSHARD, m_hWnd, pSurf, pm->m_pWorld)) {
                     delete pSurf;
                     return TRUE;
                  }

                  // loop until find empty slot
                  DWORD i;
                  for (i = 0; ; i++)
                     if (!pm->ObjectSurfaceFind (i))
                        break;
                  
                  // add it
                  pm->m_pWorld->ObjectAboutToChange(pm);
                  pSurf->m_dwID = i;
                  pm->ObjectSurfaceAdd (pSurf);
                  pm->m_pWorld->ObjectChanged(pm);

                  // set the selection
                  m_dwPolyMeshCurText = pm->ObjectSurfaceNumIndex()-1;
                  delete pSurf;
               }
            }

            // rebuild the list
            PolyMeshCreateAndFillList ();

            pm->m_pWorld->UndoRemember();
            return 0;
         }
         break;
      } // switch loword wparam
      break;
   }  // switch uMsg

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/**************************************************************************************
CHouseView::PolyMeshPaintTexture - Paints the texture HV lines onto the given hDC.
It's assumed that the HDC has a clipping region set around it (so that this function
can draw outside the eddge and expect it clipped) and that the region is the
m_aImage[0] and m_aRender[0] image.

inputs
   HDC         hDC - To Draw in. 0,0 is UL corner. Width and height are m_aImage[0]
                  width x stretch info
returns
   none
*/
void CHouseView::PolyMeshPaintTexture (HDC hDC)
{
   if (!m_fPolyMeshTextDispScale)
      return;  // must have some scale
   PCObjectPolyMesh pm = PolyMeshObject2();
   if (!pm)
      return;

   // figure out the current surface...
   DWORD dwSurface = pm->ObjectSurfaceGetIndex(m_dwPolyMeshCurText);
   if (dwSurface == -1)
      return;

   // figure out which vertices are selected
   DWORD dwSelMode = pm->m_PolyMesh.SelModeGet ();
   DWORD *padwSelVert;
   DWORD dwSelNum;
   CListFixed lSel;
   lSel.Init (sizeof(DWORD));
   padwSelVert = pm->m_PolyMesh.SelEnum(&dwSelNum);
   switch (dwSelMode) {
      default:
      case 0: // vert
         break;
      case 1:  // edges
         pm->m_PolyMesh.EdgesToVert (padwSelVert, dwSelNum, &lSel);
         padwSelVert = (DWORD*)lSel.Get(0);
         dwSelNum = lSel.Num();
         break;
      case 2:  // sides
         pm->m_PolyMesh.SidesToVert (padwSelVert, dwSelNum, &lSel);
         padwSelVert = (DWORD*)lSel.Get(0);
         dwSelNum = lSel.Num();
         break;
   }

   // precalculate all the pixels to be used for points...
   CMem memPixel;
   DWORD dwNum = pm->m_PolyMesh.VertNum();
   if (!memPixel.Required (dwNum * (sizeof(POINT) + sizeof(BOOL))))
      return;
   POINT *paPoint = (POINT*) memPixel.p;
   BOOL *pafSel = (BOOL*) (paPoint + dwNum);
   DWORD i, j;
   fp fScale = m_fPolyMeshTextDispScale; // dont need * (fp)m_dwImageScale / (fp)IMAGESCALE;
   for (i = 0; i < dwNum; i++) {
      PCPMVert pv = pm->m_PolyMesh.VertGet(i);

      paPoint[i].x = (int)((pv->m_tText.h - m_tpPolyMeshTextDispHV.h) * fScale) +
         m_pPolyMeshTextDispPoint.x;
      paPoint[i].y = (int)((pv->m_tText.v - m_tpPolyMeshTextDispHV.v) * fScale) +
         m_pPolyMeshTextDispPoint.y;
   } // i

   // transfer selected list to bools
   memset (pafSel, 0, dwNum * sizeof(BOOL));
   for (i = 0; i < dwSelNum; i++)
      if (padwSelVert[i] < dwNum)
         pafSel[padwSelVert[i]] = TRUE;

   // create the pen
   HPEN hPenOld, hBlue, hRed, hYellow;
   hBlue = CreatePen(PS_SOLID, 0, RGB(0,0,0xff));
   hRed = CreatePen(PS_SOLID, 2, RGB(0xff,0,0));
   hYellow = CreatePen(PS_SOLID, 2, RGB(0xff,0xc0,0));

   // draw the lines at 0, and 1... but only if not in meters
   hPenOld = (HPEN) SelectObject (hDC, hYellow);
   BOOL fSurfInMeters = pm->m_PolyMesh.SurfaceInMetersGet(dwSurface);
   if (!fSurfInMeters) {
      fp fStart, fScaleLine, f;
      fScaleLine = m_fPolyMeshTextDispScale;  // pixels per meter
      if ((MeasureDefaultUnits() & MUNIT_ENGLISH) && fSurfInMeters)
         fScaleLine *= FEETPERMETER;
      fStart = m_tpPolyMeshTextDispHV.h - (fp)m_pPolyMeshTextDispPoint.x / fScaleLine;  // edge of screen in meters
      fStart = (floor(fStart) - m_tpPolyMeshTextDispHV.h) * fScaleLine + m_pPolyMeshTextDispPoint.x; // back into pixels
      for (f = fStart; f < m_arThumbnailLoc[0].right - m_arThumbnailLoc[0].left; f += fScaleLine) {
         MoveToEx (hDC, (int)f, 0,NULL);
         LineTo (hDC, (int)f, m_arThumbnailLoc[0].bottom - m_arThumbnailLoc[0].top);
      }
      fStart = m_tpPolyMeshTextDispHV.v - (fp)m_pPolyMeshTextDispPoint.y / fScaleLine;  // edge of screen in meters
      fStart = (floor(fStart) - m_tpPolyMeshTextDispHV.v) * fScaleLine + m_pPolyMeshTextDispPoint.y; // back into pixels
      for (f = fStart; f < m_arThumbnailLoc[0].bottom - m_arThumbnailLoc[0].top; f += fScaleLine) {
         MoveToEx (hDC, 0, (int)f, NULL);
         LineTo (hDC, m_arThumbnailLoc[0].right - m_arThumbnailLoc[0].left, (int)f);
      }
   }

   // loop over all sides, using only those with this surface
   SelectObject (hDC, hBlue);
   CListFixed lVert;
   lVert.Init (sizeof(POINT));
   dwNum = pm->m_PolyMesh.SideNum();
   for (i = 0; i < dwNum; i++) {
      PCPMSide ps = pm->m_PolyMesh.SideGet(i);

      // if it's not for the right surface then ignore
      if (ps->m_dwSurfaceText != dwSurface)
         continue;

      lVert.Clear();

      // loop through all the vertices
      DWORD *padwVert = ps->VertGet();
      for (j = 0; j < ps->m_wNumVert; j++) {
         PCPMVert pv = pm->m_PolyMesh.VertGet (padwVert[j]);
         PTEXTUREPOINT ptp1, ptp2;
         ptp1 = pv->TextureGet (-1);
         ptp2 = pv->TextureGet (i);
         if (ptp1 == ptp2) {
            // matches an original ppoint
            lVert.Add (&paPoint[padwVert[j]]);
            continue;
         }

         // else, variation on original data, so get new
         POINT pt;
         pt.x = (int)((ptp2->h - m_tpPolyMeshTextDispHV.h) * fScale) +
            m_pPolyMeshTextDispPoint.x;
         pt.y = (int)((ptp2->v - m_tpPolyMeshTextDispHV.v) * fScale) +
            m_pPolyMeshTextDispPoint.y;
         lVert.Add (&pt);
      } // j

      // loop and draw
      POINT *pp = (POINT*) lVert.Get(0);
      DWORD dwPoints = lVert.Num();
      if (dwPoints < 2)
         continue;   // shouldnt happen, but check
      MoveToEx (hDC, pp[dwPoints-1].x, pp[dwPoints-1].y, NULL);
      for (j = 0; j < dwPoints; j++)
         LineTo (hDC, pp[j].x, pp[j].y);

      // draw selected sides?
      for (j = 0; j < dwPoints; j++) {
         if (!pafSel[padwVert[j]])
            continue;

         // box around
         SelectObject (hDC, hRed);
         MoveToEx (hDC, pp[j].x - 2, pp[j].y-2, NULL);
         LineTo (hDC, pp[j].x + 2, pp[j].y-2);
         LineTo (hDC, pp[j].x + 2, pp[j].y+2);
         LineTo (hDC, pp[j].x - 2, pp[j].y+2);
         LineTo (hDC, pp[j].x - 2, pp[j].y-2);
         SelectObject (hDC, hBlue);
      } // j
   } // i

   SelectObject (hDC, hPenOld);
   DeleteObject (hBlue);
   DeleteObject (hRed);
   DeleteObject (hYellow);

}

/**************************************************************************************
CHouseView::PolyMeshTextureMouseMove - Call this when the mouse moves (as long
as the button isn't down). What it does is update the location of the texture HV
overlay.

NOTE: Need to call this even if the texture graph isn't available because need to
figure out the scale to use when dragging points

inputs
   DWORD             dwX, dwY - Current mouse location in image X and y
returns
   none
*/
void CHouseView::PolyMeshTextureMouseMove (DWORD dwX, DWORD dwY)
{
   if ((m_dwViewWhat != VIEWWHAT_POLYMESH) || (m_dwViewSub != VSPOLYMODE_TEXTURE))
      return;  // dont use here

   // see what vertex it's over
   DWORD dwVert = PolyMeshVertFromPixel (dwX, dwY);
   if ((dwVert == -1) || (dwVert == m_dwPolyMeshTextDispLastVert))
      return;
   m_dwPolyMeshTextDispLastVert = dwVert; // do this here so remembers last loc

   // get the vertex
   PCObjectPolyMesh pm = PolyMeshObject2();
   if (!pm)
      return;
   PCPMVert pv = pm->m_PolyMesh.VertGet (dwVert);

   // make sure one of the sides is the correct surface
   DWORD dwSurface = pm->ObjectSurfaceGetIndex(m_dwPolyMeshCurText);
   if (dwSurface == -1)
      return;
   DWORD *padwSide = pv->SideGet();
   DWORD i, dwSide;
   PCPMSide ps;
   for (i = 0; i < pv->m_wNumSide; i++) {
      ps = pm->m_PolyMesh.SideGet (padwSide[i]);
      if (ps->m_dwSurfaceText == dwSurface)
         break;
   } // i
   if (i >= pv->m_wNumSide)
      return;  // not attached to the right side
   dwSide = padwSide[i];
   // note that ps is still valid

   // convert the pixels location to screen coords
   CPoint pLoc;
   CMatrix m;
   pLoc.Copy (&pv->m_pLocSubdivide);
   pLoc.p[3] = 1;
   pm->ObjectMatrixGet (&m);
   pLoc.MultiplyLeft (&m);
   fp fx, fy, fz;
   if (!m_apRender[0]->WorldSpaceToPixel (&pLoc, &fx, &fy, &fz))
      return;  // some sort of error

   // figure out the scale, so go get the right number of meters across
   // in meters
   CPoint p1, p2;
   m_apRender[0]->PixelToWorldSpace (fx, fy, fz, &p1);
   m_apRender[0]->PixelToWorldSpace (fx+1, fy, fz, &p2);
   p2.Subtract (&p1);
   m_fPolyMeshTextDispScale = p2.Length();
   m_fPolyMeshTextDispScale = max(CLOSE, m_fPolyMeshTextDispScale);
   m_fPolyMeshTextDispScale = 1.0 / m_fPolyMeshTextDispScale;
   m_fPolyMeshTextDispScale *= (fp) m_dwImageScale / IMAGESCALE;
   if (!pm->m_PolyMesh.SurfaceInMetersGet (dwSurface)) {
      // now need to figure out how many texture units per meter...
      DWORD *padwVert = ps->VertGet();
      fp fTotalLen = 0, fTotalLenText = 0;
      for (i = 0; i < ps->m_wNumVert; i++) {
         PCPMVert pv1 = pm->m_PolyMesh.VertGet(padwVert[i]);
         PCPMVert pv2 = pm->m_PolyMesh.VertGet(padwVert[(i+1)%ps->m_wNumVert]);

         // distance between the two
         fp fLen;
         p1.Subtract (&pv2->m_pLocSubdivide, &pv1->m_pLocSubdivide);
         fLen = p1.Length();
         fTotalLen += fLen;

         // distance in texture points...
         PTEXTUREPOINT ptp1, ptp2;
         TEXTUREPOINT tp;
         fp fLenText;
         ptp1 = pv1->TextureGet (dwSide);
         ptp2 = pv1->TextureGet (dwSide);
         tp.h = ptp1->h - ptp2->h;
         tp.v = ptp1->v - ptp2->v;
         fLenText = sqrt(tp.h * tp.h + tp.v * tp.v);
         fTotalLenText += fLenText;
      } // i

      // adjust
      if ((fTotalLenText > CLOSE) && (fTotalLen > CLOSE))
         m_fPolyMeshTextDispScale = m_fPolyMeshTextDispScale * fTotalLen / fTotalLenText;
   }

   // new change
   m_pPolyMeshTextDispPoint.x = (int) (fx * (fp) m_dwImageScale / IMAGESCALE);
   m_pPolyMeshTextDispPoint.y = (int) (fy * (fp) m_dwImageScale / IMAGESCALE);;
   m_tpPolyMeshTextDispHV = *(pv->TextureGet(dwSide));
   if (m_fPolyMeshTextDisp)
      InvalidateRect (m_hWnd, &m_arThumbnailLoc[0], FALSE);
}




/****************************************************************************
CHouseView::BoneObject2 - Returns a pointer to the Bone object that
this view modifies, or NULL if cant find

inputs
   DWORD       *pdwFind - If not NULL, will fill in with the object number.
*/
PCObjectBone CHouseView::BoneObject2 (DWORD *pdwFind)
{
   PCObjectSocket pos = PolyMeshObject (pdwFind);
   if (!pos)
      return NULL;

   OSMBONE os;
   memset (&os, 0, sizeof(os));
   pos->Message (OSM_BONE, &os);
   return os.pb;
}


/****************************************************************************
CHouseView::BoneSelIndividual - Used when clicking a bone

inputs
   DWORD       dwX, dwY - X and Y pixels where clicked
returns
   none
*/
void CHouseView::BoneSelIndividual (DWORD dwX, DWORD dwY)
{
   // get the object and where clicked
   DWORD dwFind;
   PCObjectBone pb = BoneObject2 (&dwFind);
   if (!pb)
      return;

   // pixel
   PIMAGEPIXEL pip = m_aImage[0].Pixel (dwX, dwY);
   DWORD dwSel = -1;
   if (HIWORD(pip->dwID) == dwFind+1)
      dwSel = LOWORD(pip->dwID);

   pb->CurBoneSet (dwSel);
}


/****************************************************************************
CHouseView::BoneEdit - UI to bring up editing a specific bone

inputs
   DWORD       dwX, dwY - X and Y pixels where clicked
returns
   none
*/
void CHouseView::BoneEdit (DWORD dwX, DWORD dwY)
{
   // get the object and where clicked
   DWORD dwFind;
   PCObjectBone pb = BoneObject2 (&dwFind);
   if (!pb)
      return;

   // pixel
   PIMAGEPIXEL pip = m_aImage[0].Pixel (dwX, dwY);
   if ((HIWORD(pip->dwID) != dwFind+1) || (LOWORD(pip->dwID) >= pb->m_lBoneList.Num())) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return;
   }

   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (m_hWnd, &r);

   cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);

   pb->DialogBoneEdit (LOWORD(pip->dwID), &cWindow);
}


/**********************************************************************************
CHouseView::BoneObjectMerge - Merges the selected object with the current Bone.

inputs
   DWORD    dwMerge - Merges this object with the Bone object

returns
   BOOL - TRUE if OK. FALSE if error
*/
BOOL CHouseView::BoneObjectMerge (DWORD dwMerge)
{
   DWORD dwFind;
   PCObjectBone pm = BoneObject2(&dwFind);
   if (!pm)
      return FALSE;
   if (dwFind == dwMerge) {
      EscMessageBox (m_hWnd, ASPString(),
         L"Click on a different object.",
         L"You must click on the object you wish to merge into the bone.",
         MB_ICONINFORMATION | MB_OK);
      return FALSE;
   }

   // create a list of the guids for these
   GUID g;
   PCObjectSocket pos = m_pWorld->ObjectGet (dwMerge);
   if (!pos)
      return FALSE;
   pos->GUIDGet (&g);

   // set undo state
   m_pWorld->UndoRemember ();

   // Call into each and tell to merge
   BOOL fRet;
   fRet = pm->Merge (&g, 1);

   if (!fRet)
      EscMessageBox (m_hWnd, ASPString(),
         L"The object didn't merge.",
         L"It may not have been a bone object.",
         MB_ICONINFORMATION | MB_OK);
   else
      EscChime (ESCCHIME_INFORMATION);

   // set undo state
   m_pWorld->UndoRemember ();

   return TRUE;
}


/*******************************************************************************
CHouseView::BonePrepForDrag - Call this when a mouse is clicked on the
Bone and things are about to be dragged. This:
   - Calculates the m_pScaleScrollX and m_pScaleScrollY values

inputs
   DWORD          dwX, dwY - X and Y where clicked on
   BOOL           fInOut - If TRUE then do in/out movement based on Y
   BOOL           fFailIfCantChange - If Bone has more than one morph on then
                     this will fail
returns
   BOOL - TRUE if success, FALSE if should fail drag
*/
BOOL CHouseView::BonePrepForDrag (DWORD dwX, DWORD dwY, BOOL fInOut, BOOL fFailIfCantChange)
{
   // find out the z depth where clicked, that combined with the image width
   // and field of view will say how much need to scroll every pixel
   fp fZ;
   fZ = m_apRender[0]->PixelToZ (dwX, dwY);

   // get the object
   DWORD dwFind;
   PCObjectBone pb = BoneObject2 (&dwFind);
   CMatrix m, mInv;
   if (!pb)
      return FALSE;

   // if not right surface then fail
   PIMAGEPIXEL pip = m_aImage[0].Pixel(dwX, dwY);
   if ((HIWORD(pip->dwID) != dwFind+1) || (LOWORD(pip->dwID) >= pb->m_lBoneList.Num())) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return FALSE;
   }

   // invert
   pb->ObjectMatrixGet (&m);
   m.Invert4 (&mInv);

   // if click on empty space the use the center of the house for the z distance
   if (fZ > 1000) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return FALSE;
   }
   fp fZMax;
   fZMax = max(.01,fZ);   // at least .01 meter distance

   m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZ, &m_pPolyMeshClick);
   m_pPolyMeshClick.p[3] = 1;
   m_pPolyMeshClick.MultiplyLeft (&mInv);

   // Need this to convert from pixel to world space
   CPoint p1, p2;
   m_apRender[0]->PixelToWorldSpace (dwX, dwY, fZMax, &p1);
   m_apRender[0]->PixelToWorldSpace (dwX+1, dwY, fZMax, &p2);
   p1.p[3] = p2.p[3] = 1;
   p1.MultiplyLeft (&mInv);
   p2.MultiplyLeft (&mInv);
   m_pScaleScrollX.Subtract (&p2, &p1);
   m_pScaleScrollX.Scale (1.0 * IMAGESCALE / (fp) m_dwImageScale);

   m_apRender[0]->PixelToWorldSpace (dwX, dwY+1, fZMax, &p2);
   p2.p[3] = 1;
   p2.MultiplyLeft (&mInv);
   m_pScaleScrollY.Subtract (&p2, &p1);
   m_pScaleScrollY.Scale (1.0 * IMAGESCALE / m_dwImageScale);

   if (fInOut) {
      // I'm specifically doing it the opposite way since up should
      // push it further back
      p2.CrossProd (&m_pScaleScrollY, &m_pScaleScrollX);
      p2.Scale (m_pScaleScrollX.Length() / p2.Length());
      m_pScaleScrollY.Copy (&p2);
      m_pScaleScrollX.Zero();
   }

   // remember this
   m_dwObjControlObject = LOWORD(pip->dwID);

   return TRUE;
}



/*******************************************************************************
CHouseView::BoneAddRect - Fills in a rectangle where the start and end
pixels (in CImage pixels) should be. Use this to draw the line.

It also uses m_pntButtonDown
and m_pntButtonLast if no reasonable line can be drawn

inputs
   PCObjectBone         pb - Bone
   DWORD                dwStartBone - Starting bone index, 0..m_lBoneList.Num()-1,
                           or -1 start at root (0,0,0 in bone)
   RECT                 *pr - Filled with rectangle for start (UL) to end (BR)
returns
   none
*/
void CHouseView::BoneAddRect (PCObjectBone pb, DWORD dwStartBone, RECT *pr)
{
   // default to location where originally clicked
   memset (pr, 0, sizeof(*pr));
   DWORD dwX, dwY;
   if (PointInImage (m_pntButtonDown.x, m_pntButtonDown.y, &dwX, &dwY)) {
      pr->left = (int)dwX;
      pr->top = (int)dwY;
   }
   if (PointInImage (m_pntMouseLast.x, m_pntMouseLast.y, &dwX, &dwY)) {
      pr->right = (int)dwX;
      pr->bottom = (int)dwY;
   }

   // get original location
   PCBone *ppb = (PCBone*) pb->m_lBoneList.Get(0);
   CMatrix m;
   CPoint pStart;
   if (dwStartBone < pb->m_lBoneList.Num())
      pStart.Copy (&ppb[dwStartBone]->m_pEndOS);
   else
      pStart.Zero();
   pStart.p[3] = 1;
   pb->ObjectMatrixGet (&m);
   pStart.MultiplyLeft (&m);  // so in world space

   fp fx, fy;
   m_apRender[0]->WorldSpaceToPixel (&pStart, &fx, &fy);
   fx = max(0, fx);
   fx = min(m_aImage[0].Width()-1, fx);
   fy = max(0, fy);
   fy = min(m_aImage[0].Height()-1, fy);
   pr->left = (int)fx;
   pr->top = (int)fy;
}


/*******************************************************************************
CHouseView::BoneAdd - Adds a new bone (and mirrors)

inputs
   DWORD          dwBone - Bone to add to, or -1 to base
   DWORD          dwX, dwY - Point that let up on
returns
   BOOL - TRUE if success
*/
BOOL CHouseView::BoneAdd (DWORD dwBone, DWORD dwX, DWORD dwY)
{
   PCObjectBone pb = BoneObject2();
   if (!pb)
      return FALSE;

   // get the position
   // get original location
   PCBone *ppb = (PCBone*) pb->m_lBoneList.Get(0);
   CMatrix m;
   CPoint pStart, pStartWorld;
   if (dwBone < pb->m_lBoneList.Num())
      pStart.Copy (&ppb[dwBone]->m_pEndOS);
   else
      pStart.Zero();
   pStartWorld.Copy (&pStart);
   pStartWorld.p[3] = 1;
   pb->ObjectMatrixGet (&m);
   pStartWorld.MultiplyLeft (&m);  // so in world space

   fp fx, fy, fz;
   m_apRender[0]->WorldSpaceToPixel (&pStartWorld, &fx, &fy, &fz);

   // now convert end point from world to object
   CPoint pEnd;
   CMatrix mInv;
   m.Invert4 (&mInv);
   m_apRender[0]->PixelToWorldSpace (dwX, dwY, fz, &pEnd);
   pEnd.p[3] = 1;
   pEnd.MultiplyLeft (&mInv);

   // subtract
   pEnd.Subtract (&pStart);

   // add
   return pb->AddBone (dwBone, &pEnd);
}









// FUTURERELEASE - Might want an easier way of making split level sections... dont know how

// FUTURERELEASE - Load in other 3D file formats: Autocad's 3D studio is the most common .sd3 or Autocad's .dfx

// FUTURERELEASE - If someone hasn't saved file after so many changes/minutes then
// recommend that they back it up? Or back it up oneself?

// FUTURERELEASE - Store name, web-site, and password with custom textures so that know
// where it's from. That way can't modify another person's furniture unless
// have password. But, can copy and modify. Something to encourage widespread
// distribution of furniture and texture libraries. Maybe an option that when
// shut down show user list of new items and ask them which they're interested
// in keeping. And maybe a way that can be password protected so can't save
// files with the furniture unless have the password.




// BUGBUG - In the objects that have scale, may want to have another control point
// that scales all equally.

// BUGBUG - In objects that have scale, may want do have buttons on both sides? Or
// something becuase sometimes the control for scale is on the wrong side and is
// not visible.


// BUGBUG - Sides of microwave and gas oven are marked as "Cabinet". 500l Fridge also has
// some "cabinet"

// BUGBUG - Should creating a wall snap to an angular grid too? If a conflict with angle
// then use location or angle?

// BUGBUG - Save image should change current directory to wherever the image is.

// BUGBUG - Better dolly abilities - no not only can rotate around z axis, but also around
// one perpendicular to what looking at

// BUGBUG - Cycad leaves need to be a bit thicker

// BUGBUG - Create a cube polymesh with 3x3. View from above. Select region
// over the entire polymesh. Note that some of the vertices are not selected.
// I think this is due to an optimization (or something else?) that causes
// them not to be seen as part of the visible sides

// BUGBUG - Warn people that if have internet popup killer may prevent tooltips
// from appearing. Do so 1st time run and when run tutotial

// BUGBUG - If save file as, and the file already exists, then warn that overwriting

