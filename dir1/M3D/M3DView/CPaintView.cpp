/***************************************************************************
CPaintView - C++ object for modifying the painting data

begun 5/3/2003
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
#define  AIRBRUSHPERSEC    5        // number of times to call airbrush per second, BUGFIX - Was 10

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
#define  IDC_HELPBUTTON    1010
#define  IDC_SMALLWINDOW   1012

#define  IDC_PVCOPY        2002
#define  IDC_UNDOBUTTON    2004
#define  IDC_REDOBUTTON    2005

#define  IDC_PVALL         3000
#define  IDC_PVCOLOR       3001
#define  IDC_PVGLOW        3002
#define  IDC_PVTRANS       3003
#define  IDC_PVSPEC        3004
#define  IDC_PVBUMP        3005
#define  IDC_PVMONO        3006

#define  IDC_TRACEMOVE     4000
#define  IDC_TRACEROT      4001
#define  IDC_TRACESCALE    4002
#define  IDC_PVMIRROR      4003

#define  IDC_BRUSHSHAPE    5000
#define  IDC_BRUSHEFFECT   5001
#define  IDC_BRUSHSTRENGTH 5002

#define  IDC_BRUSH4        6000
#define  IDC_BRUSH8        6001
#define  IDC_BRUSH16       6002
#define  IDC_BRUSH32       6003
#define  IDC_BRUSH64       6004
#define  IDC_PVLIMIT       6010
#define  IDC_PVDIALOG      6020

#define  IDC_PAINTSUN     9004


#define  IDC_TEXTLIST      8000
#define  IDC_BLEND         8001

// PVTEMPLATE - For iron-on info
typedef struct {
   WCHAR             szName[64];    // name
   CMatrix           mMatrix;       // matrix for rotation and translation.
                                    // convert from screen coords to location in pPaint
   PCTextureImage    pOrig;         // original texture before modifications
   PCTextureImage    pPaint;        // texture to use to paint

   // modofications to the paint texture (from the original)
   BOOL              fNoColor;      // if TRUE, dont paint any color. (NOTE: Will need to check for this when paint)
   BOOL              fColorAsGlow;  // use the color as glow
   DWORD             dwColorAsBump; // which color (RGB) to use for bumps, 0=none, 1=red, 2=green, 3=blue
   fp                fBumpHeight;   // range of bump, in meters
   DWORD             dwColorAsSpec; // which color (RGB) to use for specularity intensity
   DWORD             dwColorAsTrans;   // which color (RGB) to use for transparency
   BOOL              fUseTrans;     // if TRUE then a color (and close match) is made transparent
   COLORREF          cTransColor;   // color that is to be made transparent if fUseTrans is TRUE
   DWORD             dwTransDist;   // distance allowd
   BOOL              fPaintIgnoreTrans;   // when painting, if it's transparent, then dont paint using the colors in the area.. .good for text
   BOOL              fBumpAdds;     // if true bumps add when painted, else they just average
} PVTEMPLATE, *PPVTEMPLATE;

// BLURPIXEL - Retains information about pixel that's painted for blurring
typedef struct {
   PCTextureImage    pImage;        // to have blurring done on
   WORD              wX, wY;        // XY coords
   float             fValue;        // temporary value
   BYTE              bStrength;     // strength, 0..255
   BYTE              abFill[3];     // so DWORD aligned
} BLURPIXEL, *PBLURPIXEL;

// TEXTPAGE - Used for passing information to the dialog
typedef struct {
   PCPaintView       pv;            // view
   LOGFONT           lf;            // text to use
   PCMem             pMem;          // memory containing UNICODE version of text
   DWORD             dwHorzAlign;   // 0 for left, 1 for center, 2 for right
   COLORREF          cFont;         // color of the font
   COLORREF          cBack;         // color of the background
} TEXTPAGE, *PTEXTPAGE;

/*****************************************************************************
globals */
static CListFixed    gListPCPaintView;       // list of house views
static BOOL          gfListPCPaintViewInit = FALSE;   // set to true if has been initialized

static PWSTR gpszColor = L"Color";
static PWSTR gpszGlowColor = L"Color, glow";
static PWSTR gpszBumpAdd = L"Bump (increase height)";
static PWSTR gpszBumpMed = L"Bump (average out)";
static PWSTR gpszBumpSub = L"Bump (decrease height)";
static PWSTR gpszTransAdd = L"Transparent";
static PWSTR gpszTransSub = L"Opaque";
static PWSTR gpszSpecAdd = L"Specular (glossy)";
static PWSTR gpszSpecSub = L"Specular (matte)";

/**********************************************************************************
CreateSecondTexture - Given a PVTEMPLATE, this frees the existing pPaint, and then
creates a new pPaint based on pOrig and the settings in PVTEMPLATE, such as which colors
to carry on.

inputs
   PPVTEMPLATE       pTemp - TEmplate
*/
static BOOL CreateSecondTexture (PPVTEMPLATE pTemp)
{
   if (pTemp->pPaint) {
      delete pTemp->pPaint;
      pTemp->pPaint = NULL;
   }

   // clone
   pTemp->pPaint = pTemp->pOrig->Clone();
   if (!pTemp->pPaint)
      return FALSE;

   DWORD dw, i;
   dw = pTemp->pPaint->m_dwWidth * pTemp->pPaint->m_dwHeight;

   // see if not color setting
   if (pTemp->fNoColor) {
      pTemp->pPaint->m_pbRGB = NULL;
      // BUGFIX - Remove this so easier to identify colors in list: pTemp->pPaint->m_cDefColor = RGB(0,0,0);
   }

   if (pTemp->fColorAsGlow) {
      if (!pTemp->pPaint->m_memGlow.Required (dw * 3))
         return FALSE;
      pTemp->pPaint->m_pbGlow = (PBYTE) pTemp->pPaint->m_memGlow.p;
      if (pTemp->pOrig->m_pbRGB)
         memcpy (pTemp->pPaint->m_pbGlow, pTemp->pOrig->m_pbRGB, dw * 3);
      else
         for (i = 0; i < dw; i++) {
            pTemp->pPaint->m_pbGlow[i*3+0] = GetRValue (pTemp->pOrig->m_cDefColor);
            pTemp->pPaint->m_pbGlow[i*3+1] = GetGValue (pTemp->pOrig->m_cDefColor);
            pTemp->pPaint->m_pbGlow[i*3+2] = GetBValue (pTemp->pOrig->m_cDefColor);
         }
   }

   if (pTemp->dwColorAsBump) {
      pTemp->pPaint->m_fBumpHeight = pTemp->fBumpHeight ? pTemp->fBumpHeight : 0.01;
      if (!pTemp->pPaint->m_memBump.Required (dw))
         return FALSE;
      pTemp->pPaint->m_pbBump = (PBYTE) pTemp->pPaint->m_memBump.p;
      BYTE bColor;
      switch (pTemp->dwColorAsBump) {
      case 1:  // red
         bColor = GetRValue(pTemp->pOrig->m_cDefColor);
         break;
      case 2:  // green
         bColor = GetGValue(pTemp->pOrig->m_cDefColor);
         break;
      case 3:  // blue
         bColor = GetBValue(pTemp->pOrig->m_cDefColor);
         break;
      }
      for (i = 0; i < dw; i++)
         pTemp->pPaint->m_pbBump[i] = pTemp->pOrig->m_pbRGB ?
            pTemp->pOrig->m_pbRGB[i*3 + pTemp->dwColorAsBump - 1] : bColor;   // BUGFIX - dw to i
   }


   if (pTemp->dwColorAsSpec) {
      if (!pTemp->pPaint->m_memSpec.Required (dw * 2))
         return FALSE;
      pTemp->pPaint->m_pbSpec = (PBYTE) pTemp->pPaint->m_memSpec.p;
      BYTE bColor, bPower;
      bPower = (BYTE) (pTemp->pOrig->m_Material.m_wSpecExponent / 256);
      switch (pTemp->dwColorAsSpec) {
      case 1:  // red
         bColor = GetRValue(pTemp->pOrig->m_cDefColor);
         break;
      case 2:  // green
         bColor = GetGValue(pTemp->pOrig->m_cDefColor);
         break;
      case 3:  // blue
         bColor = GetBValue(pTemp->pOrig->m_cDefColor);
         break;
      }
      for (i = 0; i < dw; i++) {
         pTemp->pPaint->m_pbSpec[i*2 + 0] = bPower;
         pTemp->pPaint->m_pbSpec[i*2 + 1] = pTemp->pOrig->m_pbRGB ?
            pTemp->pOrig->m_pbRGB[i*3 + pTemp->dwColorAsSpec - 1] : bColor;   // BUGFIX - dw to i
      }
   }

   if (pTemp->dwColorAsTrans) {
      if (!pTemp->pPaint->m_memTrans.Required (dw))
         return FALSE;
      pTemp->pPaint->m_pbTrans = (PBYTE) pTemp->pPaint->m_memTrans.p;
      BYTE bColor;
      switch (pTemp->dwColorAsTrans) {
      case 1:  // red
         bColor = GetRValue(pTemp->pOrig->m_cDefColor);
         break;
      case 2:  // green
         bColor = GetGValue(pTemp->pOrig->m_cDefColor);
         break;
      case 3:  // blue
         bColor = GetBValue(pTemp->pOrig->m_cDefColor);
         break;
      }
      for (i = 0; i < dw; i++)
         pTemp->pPaint->m_pbTrans[i] = pTemp->pOrig->m_pbRGB ?
            pTemp->pOrig->m_pbRGB[i*3 + pTemp->dwColorAsTrans - 1] : bColor;  // BUGFIX - dw to i
   }

   // transparency based on distance?
   if (pTemp->fUseTrans) {
      if (!pTemp->pPaint->m_memTrans.Required (dw))
         return FALSE;
      pTemp->pPaint->m_pbTrans = (PBYTE) pTemp->pPaint->m_memTrans.p;

      DWORD j;
      BYTE abComp[3];
      BYTE ab[3];
      abComp[0] = GetRValue (pTemp->cTransColor);
      abComp[1] = GetGValue (pTemp->cTransColor);
      abComp[2] = GetBValue (pTemp->cTransColor);
      ab[0] = GetRValue (pTemp->pOrig->m_cDefColor);
      ab[1] = GetGValue (pTemp->pOrig->m_cDefColor);
      ab[2] = GetBValue (pTemp->pOrig->m_cDefColor);
      for (i = 0; i < dw; i++) {
         if (pTemp->pOrig->m_pbRGB)
            for (j = 0; j < 3; j++)
               ab[j] = pTemp->pOrig->m_pbRGB[i*3 + j];

         DWORD dwDist;
         dwDist = 0;
         for (j = 0; j < 3; j++)
            dwDist += (ab[j] >= abComp[j]) ? (ab[j] - abComp[j]) : (abComp[j] - ab[j]);
         
         pTemp->pPaint->m_pbTrans[i] = (dwDist <= pTemp->dwTransDist) ? 0xff : 0x00;
      }
   }

   // done
   return TRUE;
}

/****************************************************************************
PaintViewNew - Given a world and a paiting object (idenitified by its guid) this
will see if a view exists already. If one does then that is shown. If not,
a new one is created
*/
void PaintViewNew (PCWorldSocket pWorld, PCRenderTraditional pRender)
{
   PCPaintView *ppg = (PCPaintView*) gListPCPaintView.Get(0);
   if (gListPCPaintView.Num()) {
      // Pass in the world and renderer
      ppg[0]->NewScene (pWorld, pRender);

      ShowWindow (ppg[0]->m_hWnd, SW_SHOWNORMAL);
      SetForegroundWindow (ppg[0]->m_hWnd);
      return;
   }

   // else if get here then create
   PCPaintView pgv;
   pgv = new CPaintView ();
   if (!pgv)
      return;
   if (!pgv->Init ()) {
      delete pgv;
      return;
   }
   pgv->NewScene (pWorld, pRender);
}

/****************************************************************************
PaintViewShowHide - Shows or hides all. Used to make sure they're not viislble
while the user is animating.

inputs
   BOOL        fShow - TRUE to show
*/
void PaintViewShowHide (BOOL fShow)
{
   DWORD i;
   PCPaintView *ppg = (PCPaintView*) gListPCPaintView.Get(0);
   for (i = 0; i < gListPCPaintView.Num(); i++)
      ShowWindow (ppg[i]->m_hWnd, fShow ? SW_SHOW : SW_HIDE);
}

/****************************************************************************
PaintViewModifying - Returns TRUE if a painting view editor exists for any world.

inputs
   PCWorldSocket     pWorld - World to look for. If NULL then returns true if
            one exists for any world
returns
   BOOL - TRUE if exists
*/
BOOL PaintViewModifying (PCWorldSocket pWorld)
{
   DWORD i;
   PCPaintView *ppg = (PCPaintView*) gListPCPaintView.Get(0);
   if (!pWorld)
      return (gListPCPaintView.Num() ? TRUE : FALSE);
   for (i = 0; i < gListPCPaintView.Num(); i++) {
      if (ppg[i]->m_pWorld == pWorld)
         return TRUE;
   }

   return FALSE;
}


/****************************************************************************
CPaintViewWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CPaintView, and calls that.
*/
LRESULT CALLBACK CPaintViewWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCPaintView p = (PCPaintView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCPaintView p = (PCPaintView) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCPaintView) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/****************************************************************************
CPaintViewMapWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CPaintView, and calls that.
*/
LRESULT CALLBACK CPaintViewMapWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCPaintView p = (PCPaintView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCPaintView p = (PCPaintView) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCPaintView) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->MapWndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/****************************************************************************
Constructor and destructor

inputs
   PCWorldSocket  pWorld - World that the object is in
   PCRenderTraiditionl pRender - Renderer to use
*/
CPaintView::CPaintView (void)
{
   m_pRender = new CRenderTraditional (DEFAULTRENDERSHARD);

   m_pWorld = NULL;

   m_hWndTextList = m_hWndBlend = NULL;

   // very dark
   m_cBackDark = RGB(0x20, 0x40, 0x40);
   m_cBackMed = RGB(0x40,0x80,0x80);
   m_cBackLight = RGB(0xa0,0xc0,0xc0);
   m_cBackOutline = RGB(0x80, 0x80, 0x80);

   m_dwView = IDC_PVALL;
   m_dwCurTemplate = 0;
   m_dwBrushAir = 2; // default to airbrush
   m_dwBrushPoint = 2;  // start out as slightly pointy
   m_dwBrushStrength = 10;
   m_dwBrushEffect = 0;
   m_fAirbrushTimer = FALSE;
   m_pPixel = NULL;
   m_lPCTextureImage.Init (sizeof(PCTextureImage));
   m_lTextureImageDirty.Init (sizeof(BOOL));
   m_lPVTEMPLATE.Init (sizeof(PVTEMPLATE));
   m_dwBlendTemplate = 0; // BUGFIX - Was 64

   m_lUndo.Init (sizeof(PCListFixed));
   m_lRedo.Init (sizeof(PCListFixed));
   m_plCurUndo = NULL;

   m_hWnd = NULL;
   m_pbarGeneral = m_pbarMisc = m_pbarView = m_pbarObject = NULL;
   //m_fUndoDirty = FALSE;
   m_pLight.p[0] = -1;
   m_pLight.p[1] = 1;
   m_pLight.p[2] = 1;
   m_pLight.Normalize();
   m_mTemplateOrig.Identity();
   m_lBLURPIXEL.Init (sizeof(BLURPIXEL));
   m_pPaintOnly = NULL;

   m_fSmallWindow = FALSE;
   
   memset (m_adwPointerMode, 0 ,sizeof(m_adwPointerMode));
   m_dwPointerModeLast = 0;
   m_dwButtonDown = 0;
   m_pntButtonDown.x = m_pntButtonDown.y = 0;
   m_fCaptured = FALSE;
   m_pUndo = m_pRedo = NULL;

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
   if (!gfListPCPaintViewInit) { 
      gListPCPaintView.Init (sizeof(PCPaintView));
      gListVIEWOBJ.Init (sizeof(VIEWOBJ));
      gfListPCPaintViewInit = TRUE;
   }
   PCPaintView ph = this;
   gListPCPaintView.Add (&ph);

   // Create some defaults
   PVTEMPLATE pv;
   DWORD i;
   for (i = 0; i < 5; i++) {
      memset (&pv, 0, sizeof(pv));
      pv.mMatrix.Identity();
      pv.fBumpHeight = .01;
      wcscpy (pv.szName, gpszColor);
      pv.pOrig = new CTextureImage;
      pv.pOrig->m_Material.InitFromID (MATERIAL_FLAT);
      pv.pOrig->m_dwWidth = pv.pOrig->m_dwHeight = 10;

      switch (i) {
      case 0: // white
         pv.pOrig->m_cDefColor = RGB(0xff,0xff,0xff);
         break;
      case 1: // black
         pv.pOrig->m_cDefColor = RGB(0,0,0);
         break;
      case 2: // red
         pv.pOrig->m_cDefColor = RGB(0xff,0,0);
         break;
      case 3: // green
         pv.pOrig->m_cDefColor = RGB(0,0xff,0);
         break;
      case 4: // blue
         pv.pOrig->m_cDefColor = RGB(0,0,0xff);
         break;
      }
      CreateSecondTexture (&pv);
      m_lPVTEMPLATE.Add (&pv);
   }

}


CPaintView::~CPaintView (void)
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
   PCPaintView ph;
   for (i = 0; i < gListPCPaintView.Num(); i++) {
      ph = *((PCPaintView*)gListPCPaintView.Get(i));
      if (ph == this) {
         gListPCPaintView.Remove(i);
         break;
      }
   }

   // free up texture image
   PCTextureImage *ppti;
   ppti = (PCTextureImage*) m_lPCTextureImage.Get(0);
   for (i = 0; i < m_lPCTextureImage.Num(); i++)
      delete ppti[i];
   m_lPCTextureImage.Clear();

   // free up the template information
   PPVTEMPLATE pv;
   pv = (PPVTEMPLATE) m_lPVTEMPLATE.Get(0);
   for (i = 0; i < m_lPVTEMPLATE.Num(); i++, pv++) {
      if (pv->pOrig)
         delete pv->pOrig;
      if (pv->pPaint)
         delete pv->pPaint;
   }
   m_lPVTEMPLATE.Clear();

   // free up undo and redo
   DWORD j;
   for (i = 0; i < m_lUndo.Num(); i++) {
      PCListFixed pl = *((PCListFixed*) m_lUndo.Get(i));
      ppti = (PCTextureImage*) pl->Get(0);
      for (j = 0; j < pl->Num(); j++)
         delete ppti[j];
      delete pl;
   }
   m_lUndo.Clear();

   for (i = 0; i < m_lRedo.Num(); i++) {
      PCListFixed pl = *((PCListFixed*) m_lRedo.Get(i));
      ppti = (PCTextureImage*) pl->Get(0);
      for (j = 0; j < pl->Num(); j++)
         delete ppti[j];
      delete pl;
   }
   m_lRedo.Clear();

   if (m_plCurUndo) {
      PCListFixed pl = m_plCurUndo;
      ppti = (PCTextureImage*) pl->Get(0);
      for (j = 0; j < pl->Num(); j++)
         delete ppti[j];
      delete m_plCurUndo;
   }

   if (m_pRender)
      delete m_pRender;
}


/***********************************************************************************
CPaintView::ButtonExists - Looks through all the buttons and sees if one with the given
ID exists. If so it returns TRUE, else FALSE

inputs
   DWORD       dwID - button ID
returns
   BOOL - TRUE if exists
*/
BOOL CPaintView::ButtonExists (DWORD dwID)
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
CPaintView::SetPointerMode - Changes the pointer mode to the new mode.

inputs
   DWORD    dwMode - new mode
   DWORD    dwButton - which button it's approaite for, 0 for left, 1 for middle, 2 for right
   BOOL     fInvalidate - If TRUE, invalidate the surrounding areaas
*/
void CPaintView::SetPointerMode (DWORD dwMode, DWORD dwButton, BOOL fInvalidate)
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
BOOL CPaintView::Init (DWORD dwMonitor)
{
   if (m_hWnd)
      return FALSE;

   
   // register the window proc if it isn't alreay registered
   static BOOL fIsRegistered = FALSE;
   if (!fIsRegistered) {
      WNDCLASS wc;

      // maint window
      memset (&wc, 0, sizeof(wc));
      wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_PAINTICON));
      wc.lpfnWndProc = CPaintViewWndProc;
      wc.style = CS_HREDRAW | CS_VREDRAW;
      wc.hInstance = ghInstance;
      wc.hbrBackground = NULL; //(HBRUSH)(COLOR_BTNFACE+1);
      wc.lpszClassName = "CPaintView";
      wc.hCursor = LoadCursor (NULL, IDC_NO);
      RegisterClass (&wc);

      // map
      wc.hCursor = NULL;
      wc.lpfnWndProc = CPaintViewMapWndProc;
      wc.lpszClassName = "CPaintViewMap";
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
   m_hWnd = CreateWindowEx (WS_EX_APPWINDOW, "CPaintView", "Painting view",
      WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
      pmi->rWork.left, pmi->rWork.top, pmi->rWork.right - pmi->rWork.left, pmi->rWork.bottom - pmi->rWork.top,
      NULL, NULL, ghInstance, (LPVOID) this);
   if (!m_hWnd)
      return FALSE;

   UpdateAllButtons();

   NewView (IDC_PVALL);   // start viewing all

   return TRUE;
}

/***********************************************************************************
CPaintView::NewScene - Call this when a new scene is presented to be drawn in the
paint view

inputs
   PCWorldSocket     pWorld - World to use
   PCRenderTraditional pRender - Renderer to get the camera position from
returns
   BOOL - TRUE if success
*/
BOOL CPaintView::NewScene (PCWorldSocket pWorld, PCRenderTraditional pRender)
{
   m_pWorld = pWorld;

   // client rect
   RECT r;
   memset (&r, 0, sizeof(r));
   GetClientRect (m_hWndMap, &r);
   m_Image.Init ((DWORD)(r.right - r.left), (DWORD)(r.bottom - r.top));

   m_pRender->CWorldSet (pWorld);
   m_pRender->CImageSet (&m_Image);
   CPoint pCenter, pRot, pTrans;
   fp fScale, fTransX, fTransY, fFOV;
   if (pRender->CameraModelGet() == CAMERAMODEL_FLAT) {
      pRender->CameraFlatGet (&pCenter, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale, &fTransX, &fTransY);
      m_pRender->CameraFlat (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1], fScale, fTransX, fTransY);
   }
   else if (pRender->CameraModelGet() == CAMERAMODEL_PERSPWALKTHROUGH) {
      pRender->CameraPerspWalkthroughGet (&pCenter, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);
      m_pRender->CameraPerspWalkthrough (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1], fFOV);
   }
   else if (pRender->CameraModelGet() == CAMERAMODEL_PERSPOBJECT) {
      pRender->CameraPerspObjectGet (&pTrans, &pCenter,  &pRot.p[2], &pRot.p[0], &pRot.p[1], &fFOV);
      m_pRender->CameraPerspObject (&pTrans, &pCenter, pRot.p[2], pRot.p[0], pRot.p[1], fFOV);
   }
   else
      return FALSE;

   // draw this
   m_pPixel = NULL;
   CProgress Progress;
   Progress.Start (m_hWnd, "Drawing...");

   // make sure what drawing still exists
   if (FindViewForWorld (m_pWorld)) {
      if (!m_pRender->RenderForPainting (&m_memRPPIXEL, &m_lRPTEXTINFO, &m_pViewer, &m_mLightMatrix, &Progress))
         return FALSE;

      m_pPixel = (PRPPIXEL) m_memRPPIXEL.p;

      // Fill image in with render
      FillImage (NULL);
      FillTemplate ();

      // invalidate display
      InvalidateRect (m_hWndMap, NULL, FALSE);
   }

   return TRUE;
}





/***********************************************************************************
CPaintView::SameSurface - Looks at a pixel and returns TRUE if it is the same
surface (texture GUID's) as the one that looking at. If so, this also fills in
the texturepoint with the texture location of the queries pixel.

inputs
   PRPPIXEL    pCompare - Pixel to compare against
   DWORD       x, y - Location looking at
   PTEXTUREPOINT pTP - Filled in if the pixel looking at is of the same material
retursn
   BOOl - TRUE they're the same material. FALSE, they're not
*/
BOOL CPaintView::SameSurface (PRPPIXEL pCompare, DWORD x, DWORD y, PTEXTUREPOINT pTP)
{
   PRPPIXEL pWith = m_pPixel + (x + y * m_Image.Width());
   if (pWith->dwTextIndex != pCompare->dwTextIndex)
      return FALSE;

   // else, same surface
   pTP->h = pWith->afHV[0];
   pTP->v = pWith->afHV[1];
   return TRUE;
}


/*********************************************************************************
CPaintView::PixelBump - Given a pixel's texturepoint, and the texture delta
over the X and Y of the pixel, returns a bump amount.

inputs
   PCTextureImage    pInfo - Texture to look at
   PTEXTUREPOINT     pText - Texture point center
   PTEXTUREPOINT     pRight - Texture point of right side of pixel - texture point of left side
   PTEXTUREPOINT     pDown - Texture point of bottom side of pixel - texture point top
   PTEXTUREPOINT     pSlope - Fills in height change (in meters) over the distance.
                              .h = right height - left height (positive values stick out of surface)
                               v = BOTTOM height - TOP height
returns
   BOOL - TRUE if there's a bump map, FALSE if no bump maps for this one
*/
BOOL CPaintView::PixelBump (DWORD dwThread, PCTextureImage pInfo, PTEXTUREPOINT pText, PTEXTUREPOINT pRight,
                             PTEXTUREPOINT pDown, PTEXTUREPOINT pSlope, fp *pfHeight, BOOL fHighQuality)
{
   // NOTE: Ignoring fHighQuality

   if (!pInfo || !pInfo->m_pbBump) {
      if (pSlope)
         pSlope->h = pSlope->v = 0;
      if (pfHeight)
         *pfHeight = 0;
      return FALSE;
   }

   // figure out 4 texture points that define the change
   TEXTUREPOINT atp[4]; // 0=l,1=r,2=u,3=d
   DWORD i;
   for (i = 0; i < 4; i++) {
      atp[i] = *pText;
      switch (i) {
      case 0:  // left
         atp[i].h -= pRight->h / 2.0;
         atp[i].v -= pRight->v / 2.0;
         break;
      case 1:  // right
         atp[i].h += pRight->h / 2.0;
         atp[i].v += pRight->v / 2.0;
         break;
      case 2:  // top
         atp[i].h -= pDown->h / 2.0;
         atp[i].v -= pDown->v / 2.0;
         break;
      case 3:  // bottom
         atp[i].h += pDown->h / 2.0;
         atp[i].v += pDown->v / 2.0;
         break;
      }

      atp[i].h = myfmod(atp[i].h, 1);
      atp[i].v = myfmod(atp[i].v, 1);
   }

   // determine height
   fp afHeight[4];
   for (i = 0; i < 4; i++) {
      int iX, iY;
      atp[i].h *=(fp) pInfo->m_dwWidth;
      atp[i].v *=(fp) pInfo->m_dwHeight;
      iX = (int) atp[i].h;
      iY = (int) atp[i].v;
      atp[i].h -= iX;
      atp[i].v -= iY;

      PBYTE pUL, pUR, pLL, pLR;
      pUL = pInfo->m_pbBump + ((iX % (int)pInfo->m_dwWidth) + (iY % (int)pInfo->m_dwHeight) * (int) pInfo->m_dwWidth);
      pLL = pInfo->m_pbBump + ((iX % (int)pInfo->m_dwWidth) + ((iY+1) % (int)pInfo->m_dwHeight) * (int) pInfo->m_dwWidth);
      pLR = pInfo->m_pbBump + (((iX+1) % (int)pInfo->m_dwWidth) + ((iY+1) % (int)pInfo->m_dwHeight) * (int) pInfo->m_dwWidth);
      pUR = pInfo->m_pbBump + (((iX+1) % (int)pInfo->m_dwWidth) + (iY % (int)pInfo->m_dwHeight) * (int) pInfo->m_dwWidth);

      // average
      fp fTop, fBottom;
      fTop = (fp)pUL[0] * (1.0 - atp[i].h) + (fp)pUR[0] * atp[i].h;
      fBottom = (fp)pLL[0] * (1.0 - atp[i].h) + (fp)pLR[0] * atp[i].h;

      afHeight[i] = fTop * (1.0 - atp[i].v) + fBottom * (atp[i].v);
      afHeight[i] = afHeight[i] / 256.0 * pInfo->m_fBumpHeight;
   }

   BOOL fRet = FALSE;
   if (pSlope) {
      pSlope->h = afHeight[1] - afHeight[0];
      pSlope->v = afHeight[3] - afHeight[2];
      fRet = (pSlope->h || pSlope->v);
   }
   if (pfHeight)
      *pfHeight = (afHeight[1] + afHeight[0] + afHeight[3] + afHeight[2]) / 4.0;

   return fRet;
}




/***********************************************************************************
CPaintView::ShaderPixel - Fills in a pixel (RGB) using the shader info

inputs
   PIMAGEPIXEL       pip - Fill in RGB
   PCPoint           pView - Location of viewer, in world space
   PCPoint           pLoc - Location of point, in world space
   PCPoint           pNorm - Normal of surface (not including texture), in world space. Normalized.
   PCPoint           pLight - Light vector, in world space. Normalized. Can be NULL for just amient light.
   PTEXTUREPOINT     ptp - Texture location
   PCTextureImage    pInfo - Texture info. NULL if there isn't any texture
   DWORD             x, y - Location in the image for the pixel. If both are -1 then
                     this indicates that a template is being generated, so the delta
                     in texture is easily calculated
returns
   none
*/
void CPaintView::ShaderPixel (PIMAGEPIXEL pip, PCPoint pView, PCPoint pLoc, PCPoint pNorm,
                              PCPoint pLight, PTEXTUREPOINT ptp, PCTextureImage pInfo,
                              DWORD x, DWORD y)
{
   // base color
   WORD  awBase[3], awGlow[3], awGrey[3];
   WORD  wSpecExponent, wSpecReflect, wSpecPlastic;
   fp fGlowScale = 1;
   awBase[0] = awBase[1] = awBase[2] = 0x1000;
   memcpy (awGrey, awBase, sizeof(awGrey));
   memset (awGlow, 0, sizeof(awGlow));
   wSpecExponent = 30 * 100;
   wSpecReflect = 0x8000;
   wSpecPlastic = 0x8000;

   // is this a template for painting?
   BOOL fTemplate;
   PPVTEMPLATE pTemp;
   DWORD dw0, dw1, dw2;
   pTemp = NULL;
   fTemplate = ((x == -1) && (y == -1));
   if (fTemplate) {
      pTemp = (PPVTEMPLATE) m_lPVTEMPLATE.Get(m_dwCurTemplate);
      dw0 = 1;
      dw1 = 0;
      dw2 = 2;
   }
   else {
      dw0 = 0;
      dw1 = 1;
      dw2 = 2;
   }

   TEXTUREPOINT tp;
   tp.h = myfmod (ptp->h, 1);
   tp.v = myfmod (ptp->v, 1);

   if (pInfo) {
      DWORD dwX, dwY, dw;
      dwX = (DWORD) (tp.h * (fp)pInfo->m_dwWidth);
      dwX = min(dwX, pInfo->m_dwWidth-1);
      dwY = (DWORD) (tp.v * (fp)pInfo->m_dwHeight);
      dwY = min(dwY, pInfo->m_dwHeight-1);
      dw = dwX + dwY * pInfo->m_dwWidth;

      // defaults
      Gamma (pInfo->m_cDefColor, awBase);
      wSpecExponent = pInfo->m_Material.m_wSpecExponent;
      wSpecReflect = pInfo->m_Material.m_wSpecReflect;
      wSpecPlastic = pInfo->m_Material.m_wSpecPlastic;


      // Get base color from texture
      if (pInfo->m_pbRGB && ((m_dwView == IDC_PVALL) || (m_dwView == IDC_PVCOLOR)) ) {
         awBase[0] = Gamma(pInfo->m_pbRGB[dw*3 + 0]);
         awBase[1] = Gamma(pInfo->m_pbRGB[dw*3 + 1]);
         awBase[2] = Gamma(pInfo->m_pbRGB[dw*3 + 2]);
      }
      else if (pInfo->m_pbRGB) {
         // obviously was color other than all or color, so set base
         Gamma (fTemplate ? RGB(0xff, 0x00, 0xff) : RGB(0x00, 0xff, 0xff), awBase);
      }

      // if it's color only and dont have a color map then grey
      if ((m_dwView == IDC_PVCOLOR) && !pInfo->m_pbRGB)
         memcpy (awBase, awGrey, sizeof(awGrey));

      // BUGFIX: if in mono mode then set to fixed color
      if (m_dwView == IDC_PVMONO) {
         awBase[dw1] = awBase[dw2] = 0xffff;
         awBase[dw0] = 0;
      }
      // Get specular info from texture
      if (pInfo->m_pbSpec) {
         wSpecExponent = (WORD) pInfo->m_pbSpec[dw*2 + 0] * 256;
         wSpecReflect = Gamma(pInfo->m_pbSpec[dw*2 + 1]);
      }

      // glow
      if (pInfo->m_pbGlow && ((m_dwView == IDC_PVALL) || (m_dwView == IDC_PVGLOW))  ) {
         awGlow[0] = Gamma(pInfo->m_pbGlow[dw*3 + 0]);
         awGlow[1] = Gamma(pInfo->m_pbGlow[dw*3 + 1]);
         awGlow[2] = Gamma(pInfo->m_pbGlow[dw*3 + 2]);
         fGlowScale = pInfo->m_fGlowScale;
      }

      // other modes

      // if dealing with color view then base color to 0
      if (m_dwView == IDC_PVGLOW)
         memset (awBase, 0, sizeof(awBase));
      if (!pInfo->m_pbGlow && (m_dwView == IDC_PVGLOW)) {
         // BUGFIX - if view is IDC_PVGLOW but no glow map then just make grey
         memcpy (awBase, awGrey, sizeof(awGrey));
      }

      // if dealing with transparent view then color based on transparency
      if (m_dwView == IDC_PVTRANS) {
         if (pInfo->m_pbTrans) {
            awBase[dw1] = awBase[dw2] = Gamma(pInfo->m_pbTrans[dw]);
            awBase[dw0] = 0;
         }
         else  // just make grey
            memcpy (awBase, awGrey, sizeof(awGrey));
      }

      // if dealing with specular view then color based on specularity
      if (m_dwView == IDC_PVSPEC) {
         if (pInfo->m_pbSpec) {
            awBase[dw1] = awBase[dw2] = Gamma(pInfo->m_pbSpec[dw*2+1]);
            awBase[dw0] = 0;
         }
         else  // just make grey
            memcpy (awBase, awGrey, sizeof(awGrey));
      }

      // if dealing with bump view then color base don bump
      if (m_dwView == IDC_PVBUMP) {
         if (pInfo->m_pbBump) {
            awBase[dw1] = awBase[dw2] = Gamma(pInfo->m_pbBump[dw]);
            awBase[dw0] = 0;
         }
         else  // just make grey
            memcpy (awBase, awGrey, sizeof(awGrey));
      }

      // if it's mono and dont support specularity, or bump then greay
      if ((m_dwView == IDC_PVMONO) && !pInfo->m_pbBump && !pInfo->m_pbSpec)
         memcpy (awBase, awGrey, sizeof(awGrey));
   }

   // create a normal pointing to the viewer
   CPoint pV;
   pV.Subtract (pView, pLoc);
   pV.Normalize();

   // normal
   CPoint pN;
   pN.Copy (pNorm);

   // Calculate the normal based on bump map
   if (pInfo && pInfo->m_pbBump && ((m_dwView == IDC_PVALL) || (m_dwView == IDC_PVMONO)) ) {
      // find the texture deltas
      TEXTUREPOINT tpRight, tpBelow;
      DWORD dwLeft, dwRight, dwAbove, dwBelow;
      if (fTemplate) {
         CPoint p1, p1a, p2, p2a;
         p1.Zero();
         p2.Zero();
         p2.p[0] = 1;
         pTemp->mMatrix.Multiply2D (&p1, &p1a);
         pTemp->mMatrix.Multiply2D (&p2, &p2a);
         p2a.Subtract (&p1a);
         tpRight.h = p2a.p[0] / (fp) pTemp->pPaint->m_dwWidth;
         tpRight.v = p2a.p[1] / (fp) pTemp->pPaint->m_dwHeight;

         // and bottom
         p1.Zero();
         p2.Zero();
         p2.p[1] = 1;
         pTemp->mMatrix.Multiply2D (&p1, &p1a);
         pTemp->mMatrix.Multiply2D (&p2, &p2a);
         p2a.Subtract (&p1a);
         tpBelow.h = p2a.p[0] / (fp) pTemp->pPaint->m_dwWidth;
         tpBelow.v = p2a.p[1] / (fp) pTemp->pPaint->m_dwHeight;
      }
      else {
         TEXTUREPOINT tpLeft, tpAbove;
         PRPPIXEL pPix = m_pPixel + (x + y * m_Image.Width());
         if (x && SameSurface (pPix, x-1, y, &tpLeft))
            dwLeft = x-1;
         else {
            dwLeft = x;
            tpLeft = *ptp;
         }
         if ((x+1 < m_Image.Width()) && SameSurface(pPix, x+1, y, &tpRight))
            dwRight = x+1;
         else {
            dwRight = x;
            tpRight = *ptp;
         }
         if (y && SameSurface (pPix, x, y-1, &tpAbove))
            dwAbove = y-1;
         else {
            dwAbove = y;
            tpAbove = *ptp;
         }
         if ((y+1 < m_Image.Height()) && SameSurface (pPix, x, y+1, &tpBelow))
            dwBelow = y+1;
         else {
            dwBelow = y;
            tpBelow = *ptp;
         }
         tpRight.h -= tpLeft.h;
         tpRight.v -= tpLeft.v;
         tpBelow.h -= tpAbove.h;
         tpBelow.v -= tpAbove.v;
         if (dwRight == dwLeft+2) { // managed to get pixels to left and right
            tpRight.h /= 2.0;
            tpRight.v /= 2.0;
         }
         else if (dwRight == dwLeft)
            tpRight.h = tpRight.v = 1;  // so completely antialias
         if (dwBelow == dwAbove+2) { // managed to get pixels to top and bottom
            tpBelow.h /= 2.0;
            tpBelow.v /= 2.0;
         }
         else if (dwBelow == dwAbove)
            tpBelow.h = tpBelow.v = 1; // so completely antialises
      }

      // normal modified by texture
      CPoint pNText;
      pNText.Copy (&pN);

      // modify the normal based on the slop
      TEXTUREPOINT tSlope;
      if (PixelBump (0, pInfo, ptp, &tpRight, &tpBelow, &tSlope)) {
            // NOTE: Passing in fHighQuality = FALSE by default
         // find the size of the pixel...
         fp fSize;
         CPoint pRight, pUp;
         PRPPIXEL p1, p2;
         if (fTemplate) {
            // May cause problems when scale...
            pRight.Zero();
            pRight.p[0] = pTemp->pPaint->m_fPixelLen;

            CPoint p1, p1a, p2, p2a;
            p1.Zero();
            p2.Zero();
            p2.p[0] = 1;
            pTemp->mMatrix.Multiply2D (&p1, &p1a);
            pTemp->mMatrix.Multiply2D (&p2, &p2a);
            p2a.Subtract (&p1a);

            pRight.p[0] *= p2a.Length();
         }
         else {
            if (dwRight != dwLeft) {
               p1 = m_pPixel + (dwRight + y * m_Image.Width());
               p2 = m_pPixel + (dwLeft + y * m_Image.Width());
               pRight.p[0] = p1->afLoc[0] - p2->afLoc[0];
               pRight.p[1] = p1->afLoc[1] - p2->afLoc[1];
               pRight.p[2] = p1->afLoc[2] - p2->afLoc[2];
               pRight.Scale (1.0 / (fp)(dwRight - dwLeft));
            }
            else if (dwBelow != dwAbove) {
               CPoint pUp;
               p1 = m_pPixel + (x + dwAbove * m_Image.Width());
               p2 = m_pPixel + (x + dwBelow * m_Image.Width());
               pUp.p[0] = p1->afLoc[0] - p2->afLoc[0];
               pUp.p[1] = p1->afLoc[1] - p2->afLoc[1];
               pUp.p[2] = p1->afLoc[2] - p2->afLoc[2];
               pUp.Scale (1.0 / (fp)(dwBelow - dwAbove));
               pRight.CrossProd (&pUp, &pN);
            }
            else {
               // dont know, so make something up
               pRight.Zero();
               pRight.p[0] = 1;
            }
         }
         fSize = pRight.Length();
         // assuming same size in both directions

         // find points to the right and left
         pRight.Normalize();
         pUp.CrossProd (&pN, &pRight);
         // BUGFIX - if pN more or less same as pRight, was a bug where would get pUp wrong
         if (!fTemplate && (pUp.Length() < .1) && (dwBelow != dwAbove)) {
            p1 = m_pPixel + (x + dwAbove * m_Image.Width());
            p2 = m_pPixel + (x + dwBelow * m_Image.Width());
            pUp.p[0] = p1->afLoc[0] - p2->afLoc[0];
            pUp.p[1] = p1->afLoc[1] - p2->afLoc[1];
            pUp.p[2] = p1->afLoc[2] - p2->afLoc[2];
            pUp.Scale (1.0 / (fp)(dwBelow - dwAbove));
            pRight.CrossProd (&pUp, &pN);

            // the normal faces to the right/left
            pUp.Normalize();
            pRight.CrossProd (&pUp, &pN);
            pRight.Normalize();
            pUp.CrossProd (&pN, &pRight);
            pUp.Normalize();
         }
         else {
            pUp.Normalize();
            pRight.CrossProd (&pUp, &pN);
            pRight.Normalize();
         }

         // not calculalte  hold the LL corner of the pixel at 0,0,0 ans find LR and UL.
         CPoint pLR, pUL;
         pLR.Copy (&pRight);
         pLR.Scale (fSize);
         pUL.Copy (&pUp);
         pUL.Scale (fSize);

         // add the delta in elevations
         CPoint pDelta;
         pDelta.Copy (&pN);
         pDelta.Scale (tSlope.h);
         pLR.Add (&pDelta);
         pDelta.Copy (&pN);
         pDelta.Scale (-tSlope.v);
         pUL.Add (&pDelta);

         // the normal is the corss product of these two
         pNText.CrossProd (&pLR, &pUL);
         pNText.Normalize();
         pN.Copy (&pNText);
      }

   }

   // ambient light
   fp afColor[3];
   DWORD i;
   fp fAmbient;
   if ((m_dwView == IDC_PVALL) || (m_dwView == IDC_PVMONO))
      fAmbient = pLight ? (1.0 / 6.0) : 1;
   else
      fAmbient = 1;
   for (i = 0; i < 3; i++)
      afColor[i] = (fp)awBase[i] * fAmbient;

   // Add in glow
   if (pInfo && pInfo->m_pbGlow)
      for (i = 0; i < 3; i++)
         afColor[i] += (fp)awGlow[i] * fGlowScale;

   // diffuse
   fp fDiffuse;
   fDiffuse = 0;
   if (pLight && (fAmbient != 1.0)) {
      fDiffuse = pN.DotProd (pLight);
      if (fDiffuse > 0) {
         for (i = 0; i < 3; i++)
            afColor[i] += (fp)awBase[i] * fDiffuse;
      }
   }

   // specular
   if (fDiffuse > 0) {  // effectively also checks that ambient != 1
      // specular component
      CPoint pH;
      fp  fLight;
      pH.Add (&pV, pLight);
      pH.Normalize();
      fp fNDotH, fVDotH;
      fNDotH = pN.DotProd (&pH);
      if ((fNDotH > 0) && wSpecReflect) {
         fVDotH = pV.DotProd (&pH);
         fVDotH = max(0,fVDotH);
         fNDotH = pow (fNDotH, (fp)((fp)wSpecExponent / 100.0));
         fLight = fNDotH * (fp) wSpecReflect / (fp)0xffff;


         fp fPureLight, fMixed, fMax;
         if (wSpecPlastic > 0x8000)
            fVDotH = pow(fVDotH, (fp)(1.0 + (fp)(wSpecPlastic - 0x8000) / (fp)0x1000));
         else if (wSpecPlastic < 0x8000)
            fVDotH = pow(fVDotH, (fp)(1.0 / (1.0 + (fp)(0x8000 - wSpecPlastic) / (fp)0x1000)));
         fPureLight = fLight * (1.0 - fVDotH);
         fMixed = fLight * fVDotH;

         // BUGFIX - For the mixing component, offset by the maximum brightness component of
         // the object so the specularily is relatively as bright.
         //fMax = (pawColor[0] + pawColor[1] + pawColor[2]) / (fp) 0xffff;   // NOTE: Secifically not using /3.0
         fMax = (fp)max(max(awBase[0], awBase[1]),awBase[2]) / (fp) 0xffff;
         if (fMax > EPSILON)
            fMixed /= fMax;

         for (i = 0; i < 3; i++)
            afColor[i] += fPureLight * (fp)0xffff + fMixed * (fp)awBase[i];
      }


   }

   // done
   for (i = 0; i < 3; i++) {
      afColor[i] = max(0,afColor[i]);
      afColor[i] = min((fp)0xffff, afColor[i]);
      (&pip->wRed)[i] = (WORD) afColor[i];
   }
}

/***********************************************************************************
CPaintView::FillImage - Fills a given rectangle portion of the image... rendering
using the textures and shading.

inputs
   RECT        *prFill - Portion to FILL. If NULL, the entire image
returns
   none
*/
void CPaintView::FillImage (RECT *prFill)
{
   // checks
   if (!m_pPixel)
      return;

   RECT rFill;
   if (prFill) {
      rFill.left = max(0, prFill->left);
      rFill.top = max(0, prFill->top);
      rFill.right = min((int)m_Image.Width(), prFill->right);
      rFill.bottom = min((int)m_Image.Height(), prFill->bottom);
   }
   else {
      rFill.left = rFill.top = 0;
      rFill.right = (int)m_Image.Width();
      rFill.bottom = (int)m_Image.Height();
   }

   // figure out light vector
   BOOL fAmbient;
   if (m_pLight.Length() < CLOSE)
      fAmbient = TRUE;
   else {
      fAmbient = FALSE;
      m_pLightWorld.Copy (&m_pLight);
      m_pLightWorld.p[3] = 1;
      m_pLightWorld.MultiplyLeft (&m_mLightMatrix);
      m_pLightWorld.Normalize(); // just in case
   }

   // get the list of texture images
   PCTextureImage *ppti;
   ppti = (PCTextureImage*) m_lPCTextureImage.Get(0);


   // loop
   DWORD x,y,i;
   PRPPIXEL prp;
   PIMAGEPIXEL pip;
   PRPTEXTINFO pInfo;
   PCTextureImage pLast;
   pLast = NULL;
   pInfo = (PRPTEXTINFO) m_lRPTEXTINFO.Get(0);
   for (y = (DWORD)rFill.top; y < (DWORD)rFill.bottom; y++) {
      prp = m_pPixel + (y * m_Image.Width() + (DWORD)rFill.left);
      pip = m_Image.Pixel ((DWORD)rFill.left, y);
      for (x = (DWORD)rFill.left; x < (DWORD)rFill.right; x++, prp++, pip++) {
         // if nothing there then blackness
         if ((prp->afNorm[0] == 0) && (prp->afNorm[1] == 0) && (prp->afNorm[2] == 0)) {
            pip->wRed = pip->wGreen = pip->wBlue = 0;
            continue;
         }

         // set these as points
         CPoint pLoc, pNorm;
         TEXTUREPOINT tp;
         pLoc.p[0] = prp->afLoc[0];
         pLoc.p[1] = prp->afLoc[1];
         pLoc.p[2] = prp->afLoc[2];
         pLoc.p[3] = 0;
         pNorm.p[0] = prp->afNorm[0];
         pNorm.p[1] = prp->afNorm[1];
         pNorm.p[2] = prp->afNorm[2];
         pNorm.p[3] = 0;
         tp.h = prp->afHV[0];
         tp.v = prp->afHV[1];

         // get the texture image
         if (prp->dwTextIndex) {
            PRPTEXTINFO pCur = pInfo + (prp->dwTextIndex - 1);
            if (!pLast || !IsEqualGUID(pLast->m_gSub, pCur->gSub) || !IsEqualGUID(pLast->m_gCode, pCur->gCode)) {
               // look in list for new one
               for (i = 0; i < m_lPCTextureImage.Num(); i++) {
                  pLast = ppti[i];
                  if (IsEqualGUID(pLast->m_gSub, pCur->gSub) && IsEqualGUID(pLast->m_gCode, pCur->gCode))
                     break;
               }
               if (i >= m_lPCTextureImage.Num()) {
                  // couldnt find so add
                  pLast = new CTextureImage;
                  if (pLast) {
                     if (!pLast->FromTexture (DEFAULTRENDERSHARD, &pCur->gCode, &pCur->gSub)) {
                        delete pLast;
                        pLast = NULL;
                     }
                  }
                  if (pLast) {
                     // add it
                     BOOL fDirty = FALSE;
                     m_lPCTextureImage.Add (&pLast);
                     m_lTextureImageDirty.Add (&fDirty);
                     ppti = (PCTextureImage*) m_lPCTextureImage.Get(0);
                  }
               }
            }
            // else, already have what looking for
         }
         else {
            pLast = NULL;
         }

         ShaderPixel (pip, &m_pViewer, &pLoc, &pNorm, fAmbient ? NULL : &m_pLightWorld, &tp,
            (!m_pPaintOnly || (m_pPaintOnly == pLast)) ? pLast : NULL, x, y);

#if 0
         // Hack for now. do z depth color
         CPoint pDist;
         fp fLen;
         pDist.Subtract (&pLoc, &m_pViewer);
         fLen = pDist.Length();
         fLen = fLen / 10.0 * (fp)0xffff;
         fLen = min(fLen, (fp)0xffff);
         pip->wRed = pip->wGreen = pip->wBlue = 0xffff - (WORD) fLen;
#endif // 0
      } //x
   } // y
}


/***********************************************************************************
CPaintView::FillTemplate - Fills m_pTemplateImage with the current template.
*/
void CPaintView::FillTemplate (void)
{
   // set undo cache when do this since may have been called when switch colors
   UndoCache();

   m_ImageTemplate.Init (m_Image.Width(), m_Image.Height(), RGB(0,0,0));
   if (m_dwCurTemplate >= m_lPVTEMPLATE.Num())
      return;
   PPVTEMPLATE pt;
   pt = (PPVTEMPLATE) m_lPVTEMPLATE.Get(m_dwCurTemplate);


   RECT rFill;
   rFill.left = rFill.top = 0;
   rFill.right = (int)m_Image.Width();
   rFill.bottom = (int)m_Image.Height();

   // make these calcs in screen coords
   // figure out light vector
   BOOL fAmbient;
   CPoint pLight;
   if (m_pLight.Length() < CLOSE)
      fAmbient = TRUE;
   else {
      fAmbient = FALSE;
      pLight.Copy (&m_pLight);
      // NOTE: Don't multiply by matrix
   }

   // viewer
   CPoint pViewer;
   pViewer.Zero();
   pViewer.p[2] = 1000; // way away

   // loop
   DWORD x,y;
   PIMAGEPIXEL pip;
   for (y = (DWORD)rFill.top; y < (DWORD)rFill.bottom; y++) {
      pip = m_ImageTemplate.Pixel ((DWORD)rFill.left, y);
      for (x = (DWORD)rFill.left; x < (DWORD)rFill.right; x++, pip++) {
         // location
         CPoint pText, pLoc;
         pLoc.Zero();
         pLoc.p[0] = (fp)x;
         pLoc.p[1] = (fp) y;
         pt->mMatrix.Multiply2D (&pLoc, &pText);
         // now location contains points in texture
         
         // location needs scaling
         pLoc.p[0] -= (fp)(rFill.right / 2);
         pLoc.p[1] -= (fp)(rFill.bottom / 2);
         pLoc.Scale (pt->pPaint->m_fPixelLen);

         // normal is always pointing out
         CPoint pNorm;
         pNorm.Zero();
         pNorm.p[2] = 1;

         // set these as points
         TEXTUREPOINT tp;
         tp.h = pText.p[0] / (fp) pt->pPaint->m_dwWidth;
         tp.v = pText.p[1] / (fp) pt->pPaint->m_dwHeight;

         ShaderPixel (pip, &pViewer, &pLoc, &pNorm, fAmbient ? NULL : &pLight, &tp,
            pt->pPaint, -1, -1);

      } //x
   } // y
}

/***********************************************************************************
CPaintView::UpdateAllButtons - Called when the user has progressed so far in the
tutorial, this first erases all the buttons there, and then updates them
to the appropriate display level.

inputs
*/
void CPaintView::UpdateAllButtons (void)
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
   pi = m_pbarMisc->ButtonAdd (2, IDB_COPY, ghInstance, "Ctrl-C", "Copy selection",
      "Click and drag over a section of the image to copy it to the tracing paper palette.",
      IDC_PVCOPY);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);


   // view choices
   pi = m_pbarView->ButtonAdd (0, IDB_PVALL, ghInstance, "F2", "View all",
      "Shows the surface with color, glow, specularity, and bumps. No transparency.", IDC_PVALL);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_LLOOP);

   pi = m_pbarView->ButtonAdd (0, IDB_PVMONO, ghInstance, "F3", "Mono view",
      "Shows the surface with specularity and bumps, but no color or glow.", IDC_PVMONO);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_CLOOP);

   pi = m_pbarView->ButtonAdd (0, IDB_PVCOLOR, ghInstance, "F4", "Color only",
      "Shows the surface with color. No glow, specularity, bumps, or transparency.",
      IDC_PVCOLOR);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_CLOOP);

   pi = m_pbarView->ButtonAdd (0, IDB_PVBUMP, ghInstance, "F5", "Bumps only",
      "Shows the surface with bumps; higher areas are brighter. "
      "No color, glow, specularity, or transparency.",
      IDC_PVBUMP);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_CLOOP);

   pi = m_pbarView->ButtonAdd (0, IDB_PVGLOW, ghInstance, NULL, "Glow only",
      "Shows the surface with glow. No color, specularity, bumps, or transparency.",
      IDC_PVGLOW);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_CLOOP);

   pi = m_pbarView->ButtonAdd (0, IDB_PVSPEC, ghInstance, NULL, "Specular only",
      "Shows the surface with more specular (glossy) areas brighter. "
      "No color, glow, bumps, or transparency.",
      IDC_PVSPEC);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_CLOOP);

   pi = m_pbarView->ButtonAdd (0, IDB_PVTRANS, ghInstance, NULL, "Transparency only",
      "Shows the surface with transparent areas brighter. "
      "No color, glow, specularity, or bumps.",
      IDC_PVTRANS);
   pi->FlagsSet (IBFLAG_DISABLEDLIGHT | IBFLAG_SHAPE_RLOOP);

   // top
   pi = m_pbarView->ButtonAdd (1, IDB_MOVEEMBED, ghInstance, 
      NULL, "Move the tracing paper",
      "Click on the screen and drag to move the tracing paper."
      , IDC_TRACEMOVE);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);

   pi = m_pbarView->ButtonAdd (1, IDB_MOVEROTZ, ghInstance, 
      NULL, "Rotate the tracing paper",
      "Click on the screen and drag to rotate the tracing paper."
      , IDC_TRACEROT);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);

   pi = m_pbarView->ButtonAdd (1, IDB_MOVEUD, ghInstance, 
      NULL, "Scale the tracing paper",
      "Click on the screen and drag to enlarge or shrink the tracing paper."
      , IDC_TRACESCALE);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);

   pi = m_pbarView->ButtonAdd (1, IDB_PVMIRROR, ghInstance, 
      NULL, "Flip the tracing paper",
      "Click this to flip the tracing paper left to right."
      , IDC_PVMIRROR);

   // grid
   pi = m_pbarMisc->ButtonAdd (0, IDB_GROUNDSUN,ghInstance, 
      NULL, "Move the sun",
      "This tool moves the sunlight illuminating the painting canvas so you "
      "can more easily see the textures."
      ,
      IDC_PAINTSUN);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);


   // tools for topo
#define GENDESC \
      "Click on the center of the landscape feature and drag the mouse left or right "\
      "to control the area. Drag up or down to control the height. "\
      "Roll the mouse-wheel to try a different version."

   char pszGenDesc[] = GENDESC;

   // common to all painting
   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSHSHAPE, ghInstance, NULL, "Brush shape",
      "This menu lets you chose the shape of the brush from pointy to flat.",
      IDC_BRUSHSHAPE);
   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSHEFFECT, ghInstance, NULL, "Brush effect",
      "This menu lets you change what the brush does, such as painting or blurring.",
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

   pi = m_pbarObject->ButtonAdd (1, IDB_OBJPAINT, ghInstance, 
      NULL, "Limit painting to only one texture",
      "If you want to limit painting to only one texture, select this tool "
      "and click on the texture in the image. To allow painting on any texture "
      "click on a blank region of the image."
      "<p/>"
      "Use this like masking tape, so that "
      "when two textures are next to one another you can paint the edge of one without "
      "accidentally painting the other."
      , IDC_PVLIMIT);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

   pi = m_pbarObject->ButtonAdd (0, IDB_OBJDIALOG,ghInstance, 
      "Alt-S", "Texture settings",
      "Select this tool and click on a texture up texture-specific "
      "settings, such as its resolution and what maps are used."
      , IDC_PVDIALOG);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_BOTTOM);

   DWORD adwWant[3];
   adwWant[0] = IDC_BRUSH32;
   adwWant[1] = IDC_TRACEROT;
   adwWant[2] = IDC_TRACEMOVE;

   // if button no longer exists then set a new one
   DWORD i;
   for (i = 0; i < 3; i++)
      if (!ButtonExists(m_adwPointerMode[i]))
         SetPointerMode (adwWant[i], i);
      else {
         // BUGFIX - reset the existing one just to make sure displayed
         SetPointerMode (m_adwPointerMode[i], i, FALSE);
      }

   m_pbarGeneral->AdjustAllButtons();
   m_pbarMisc->AdjustAllButtons();
   m_pbarView->AdjustAllButtons();
   m_pbarObject->AdjustAllButtons();

   // finally
   // Take this out NewView ();
   m_pbarView->FlagsSet (m_dwView, IBFLAG_BLUELIGHT, 0);

   // get the undo and redo buttons set properly
   UndoUpdateButtons ();

}

/***********************************************************************************
CPaintView::NewView -Change to a new type of view

inputs
   DWORD       dwView - 0 for topo, 1 for texture, 2 for forestg
*/
void CPaintView::NewView (DWORD dwView)
{
   if (dwView == m_dwView)
      return;  // no change
   m_dwView = dwView;

   // set the light for the new model
   DWORD dwButton;
   dwButton = dwView;
   m_pbarView->FlagsSet (dwButton, IBFLAG_BLUELIGHT, 0);


   // redraw all
   FillImage (NULL);
   FillTemplate();
   InvalidateRect (this->m_hWndMap, NULL, FALSE);
}

/***********************************************************************************
CPaintView::InvalidateDisplay - Redraws the display area, not the buttons.
*/
void CPaintView::InvalidateDisplay (void)
{
   InvalidateRect (m_hWndMap, NULL, FALSE);
}

/*********************************************************************************
CPaintView::PointInImage - Given a pixel in the map's client rectangle, this
fills in pfImageX and pfImageY with the x and y ground-pixel locations.

inputs
   int         iX, iY - Point in map client's rectangle
   fp          *pfImageX, *pfImageY - Filled in with ground-pixel
returns
   BOOL - TRUE if over the ground
*/
BOOL CPaintView::PointInImage (int iX, int iY, fp *pfImageX, fp *pfImageY)
{
   if ((iX < 0) || (iY < 0) || (iX >= (int)m_Image.Width()) || (iY >= (int)m_Image.Height()))
      return FALSE;

   fp fX, fY;
   fX = fY = 0;

   if (pfImageX)
      *pfImageX = fX;
   if (pfImageY)
      *pfImageY = fY;

   return TRUE;
}



/**********************************************************************************
CPaintView::SetProperCursor - Call this to set the cursor to whatever is appropriate
for the window
*/
void CPaintView::SetProperCursor (int iX, int iY)
{
   fp fX, fY;
   DWORD dwButton = m_dwButtonDown ? (m_dwButtonDown-1) : 0;

   if (PointInImage (iX, iY, &fX, &fY)) {
      switch (m_adwPointerMode[dwButton]) {

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

         case IDC_PVLIMIT:
            SetCursor (LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORPAINT)));
            break;

         case IDC_PVDIALOG:
            {
               PRPPIXEL prp = (PRPPIXEL)m_memRPPIXEL.p + (iX + iY * (int)m_Image.Width());

               SetCursor (prp->dwTextIndex ?
                  LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORDIALOG)) :
                  LoadCursor (NULL, IDC_NO));
            }
            break;

         case IDC_TRACESCALE:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMOVEUD)));
            break;

         case IDC_TRACEMOVE:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMOVEEMBED)));
            break;

         case IDC_PVCOPY:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORSELREGION)));
            break;

         case IDC_TRACEROT:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMOVEROTZ)));
            break;

         case IDC_PAINTSUN:
            SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORGROUNDSUN)));
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


/****************************************************************************
PaintDialogPage
*/
BOOL PaintDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCPaintView pv = (PCPaintView) pPage->m_pUserData;
   PCTextureImage pti = pv->m_pTextureTemp;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"mapcolor");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pti->m_pbRGB ? TRUE : FALSE);
         pControl = pPage->ControlFind (L"mapglow");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pti->m_pbGlow ? TRUE : FALSE);
         pControl = pPage->ControlFind (L"mapbump");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pti->m_pbBump ? TRUE : FALSE);
         pControl = pPage->ControlFind (L"maptrans");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), (pti->m_pbTrans && !pti->m_fTransUse) ? TRUE : FALSE);
         pControl = pPage->ControlFind (L"mapspec");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pti->m_pbSpec ? TRUE : FALSE);
         pControl = pPage->ControlFind (L"transuse");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pti->m_fTransUse);
         pControl = pPage->ControlFind (L"cached");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pti->m_fCached);


         pControl = pPage->ControlFind (L"transdist");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pti->m_dwTransDist);

         DoubleToControl (pPage, L"width", pti->m_dwWidth);
         DoubleToControl (pPage, L"height", pti->m_dwHeight);
         MeasureToString (pPage, L"widthmeters", pti->m_fPixelLen * (fp)pti->m_dwWidth);
         MeasureToString (pPage, L"bumpheight", pti->m_fBumpHeight);

         FillStatusColor (pPage, L"transcolor", pti->m_cTransColor);
         FillStatusColor (pPage, L"defcolor", pti->m_cDefColor);

         // material
         ComboBoxSet (pPage, L"material", pti->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pti->m_Material.m_dwID ? FALSE : TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         BOOL fChecked;
         DWORD dw, i;
         psz = p->pControl->m_pszName;
         fChecked = p->pControl->AttribGetBOOL (Checked());
         dw = pti->m_dwWidth * pti->m_dwHeight;

         if (!_wcsicmp(psz, L"mapcolor")) {
            pv->UndoAboutToChange (pti);

            if (fChecked) {
               if (!pti->m_memRGB.Required (dw * 3))
                  return TRUE;
               pti->m_pbRGB = (PBYTE) pti->m_memRGB.p;
               for (i = 0; i < dw; i++) {
                  pti->m_pbRGB[i*3+0] = GetRValue(pti->m_cDefColor);
                  pti->m_pbRGB[i*3+1] = GetGValue(pti->m_cDefColor);
                  pti->m_pbRGB[i*3+2] = GetBValue(pti->m_cDefColor);
               }
            }
            else
               pti->m_pbRGB = NULL;

            // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"mapglow")) {
            pv->UndoAboutToChange (pti);

            if (fChecked) {
               if (!pti->m_memGlow.Required (dw * 3))
                  return TRUE;
               pti->m_pbGlow = (PBYTE) pti->m_memGlow.p;
               memset (pti->m_pbGlow, 0, dw*3);
            }
            else
               pti->m_pbGlow = NULL;

            // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"mapspec")) {
            pv->UndoAboutToChange (pti);

            if (fChecked) {
               if (!pti->m_memSpec.Required (dw * 2))
                  return TRUE;
               pti->m_pbSpec = (PBYTE) pti->m_memSpec.p;
               for (i = 0; i < dw; i++) {
                  pti->m_pbSpec[i*2+0] = HIBYTE(pti->m_Material.m_wSpecExponent);
                  pti->m_pbSpec[i*2+1] = UnGamma(pti->m_Material.m_wSpecReflect);
               }
            }
            else
               pti->m_pbSpec = NULL;

            // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"maptrans")) {
            pv->UndoAboutToChange (pti);

            if (fChecked) {
               pti->m_fTransUse = FALSE;
               if (!pti->m_memTrans.Required (dw))
                  return TRUE;
               pti->m_pbTrans = (PBYTE) pti->m_memTrans.p;
               for (i = 0; i < dw; i++)
                  pti->m_pbTrans[i] = UnGamma(pti->m_Material.m_wTransparency);
            }
            else
               pti->m_pbTrans = NULL;

            // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"mapbump")) {
            pv->UndoAboutToChange (pti);

            if (fChecked) {
               if (pti->m_fBumpHeight <= CLOSE)
                  pti->m_fBumpHeight = 0.01;
               if (!pti->m_memBump.Required (dw))
                  return TRUE;
               pti->m_pbBump = (PBYTE) pti->m_memBump.p;
               for (i = 0; i < dw; i++)
                  pti->m_pbBump[i] = 0x80;
            }
            else
               pti->m_pbBump = NULL;

            // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"transuse")) {
            pv->UndoAboutToChange (pti);

            pti->m_fTransUse = fChecked;

            // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"cached")) {
            pv->UndoAboutToChange (pti);

            pti->m_fCached = fChecked;

            // dont bother refreshing since wont cahnge anything
         }
         else if (!_wcsicmp(psz, L"changedef")) {
            COLORREF cr;
            COLORREF *pc;
            pc = &pti->m_cDefColor;
            cr = AskColor (pPage->m_pWindow->m_hWnd, *pc, pPage, L"defcolor");
            if (cr != *pc) {
               pv->UndoAboutToChange (pti);

               *pc = cr;

               // update display
               pv->FillImage (NULL);
               InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"changetrans")) {
            COLORREF cr;
            COLORREF *pc;
            pc = &pti->m_cTransColor;
            cr = AskColor (pPage->m_pWindow->m_hWnd, *pc, pPage, L"transcolor");
            if (cr != *pc) {
               pv->UndoAboutToChange (pti);

               *pc = cr;

               // dont bother updating display
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            pv->UndoAboutToChange (pti);

            pti->m_Material.Dialog (pPage->m_pWindow->m_hWnd);

               // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            pv->UndoAboutToChange (pti);

            pti->m_Material.Dialog (pPage->m_pWindow->m_hWnd);

               // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"usewidth")) {
            DWORD dwNewWidth, dwNewHeight;
            dwNewWidth = (DWORD)DoubleFromControl (pPage, L"width");
            dwNewHeight = (DWORD)DoubleFromControl (pPage, L"height");
            dwNewWidth = max(dwNewWidth, 2);
            dwNewHeight = max(dwNewHeight, 2);

            pv->UndoAboutToChange (pti);

            pv->ChangeResolution (pti, dwNewWidth, dwNewHeight);

            // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);
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

         if (!_wcsicmp(psz, L"widthmeters")) {
            pv->UndoAboutToChange (pti);

            MeasureParseString (pPage, L"widthmeters", &pti->m_fPixelLen);
            if (pti->m_dwWidth)
               pti->m_fPixelLen /= (fp)pti->m_dwWidth;
            pti->m_fPixelLen = max(pti->m_fPixelLen, CLOSE);

            // no point updating because wont change
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"bumpheight")) {
            pv->UndoAboutToChange (pti);

            MeasureParseString (pPage, L"bumpheight", &pti->m_fBumpHeight);
            pti->m_fBumpHeight = max(pti->m_fBumpHeight, CLOSE);

            // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         DWORD dwVal;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            if (dwVal == pti->m_Material.m_dwID)
               break; // unchanged

            pv->UndoAboutToChange (pti);

            if (dwVal)
               pti->m_Material.InitFromID (dwVal);
            else
               pti->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pti->m_Material.m_dwID ? FALSE : TRUE);

            // update display
            pv->FillImage (NULL);
            InvalidateRect (pv->m_hWndMap, NULL, FALSE);

            return TRUE;
         }
      }
      break;

   case ESCN_SCROLL:
   //case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"transdist"))
            break;

         DWORD dwVal;
         dwVal = p->pControl->AttribGetInt (Pos());
         if (dwVal == pti->m_dwTransDist)
            return TRUE;

         pv->UndoAboutToChange (pti);
         pti->m_dwTransDist = dwVal;

         // no point updating display since wont make a difference
         return TRUE;
      }
      break;



   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Texture settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************************
CPaintView::ButtonDown - Left button down message
*/
LRESULT CPaintView::ButtonDown (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton)
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

   // just click once
   if (dwPointerMode == IDC_PVLIMIT) {
      // get this location
      PRPPIXEL prp = (PRPPIXEL) m_memRPPIXEL.p + (iX + iY * (int) m_Image.Width());
      PRPTEXTINFO pti = prp->dwTextIndex ? (PRPTEXTINFO) m_lRPTEXTINFO.Get(prp->dwTextIndex-1) : NULL;

      // if click on nothing then reset
      if (!pti) {
         if (m_pPaintOnly) {
            m_pPaintOnly = NULL;
            FillImage (NULL);
            InvalidateRect (m_hWndMap, NULL, FALSE);
         }
         return 0;
      }

      // find match
      PCTextureImage *ppti;
      PCTextureImage pLast;
      ppti = (PCTextureImage*) m_lPCTextureImage.Get(0);
      pLast = NULL;
      DWORD i;
      for (i = 0; i < m_lPCTextureImage.Num(); i++) {
         pLast = ppti[i];
         if (IsEqualGUID(pLast->m_gSub, pti->gSub) && IsEqualGUID(pLast->m_gCode, pti->gCode))
            break;
      }
      if (pLast != m_pPaintOnly) {
         m_pPaintOnly = pLast;
         FillImage (NULL);
         InvalidateRect (m_hWndMap, NULL, FALSE);
      }
      return 0;
   }
   else if (dwPointerMode == IDC_PVDIALOG) {
      // get this location
      PRPPIXEL prp = (PRPPIXEL) m_memRPPIXEL.p + (iX + iY * (int) m_Image.Width());
      PRPTEXTINFO pti = prp->dwTextIndex ? (PRPTEXTINFO) m_lRPTEXTINFO.Get(prp->dwTextIndex-1) : NULL;

      // if click on nothing then reset
      if (!pti) {
         BeepWindowBeep (ESCBEEP_DONTCLICK);
         return 0;
      }

      // find match
      PCTextureImage *ppti;
      PCTextureImage pLast;
      ppti = (PCTextureImage*) m_lPCTextureImage.Get(0);
      pLast = NULL;
      DWORD i;
      for (i = 0; i < m_lPCTextureImage.Num(); i++) {
         pLast = ppti[i];
         if (IsEqualGUID(pLast->m_gSub, pti->gSub) && IsEqualGUID(pLast->m_gCode, pti->gCode))
            break;
      }
      if (!pLast) {
         BeepWindowBeep (ESCBEEP_DONTCLICK);
         return 0;
      }

      // cache undo
      UndoCache();

      // Settings dialog
      CEscWindow cWindow;
      RECT r;
      DialogBoxLocation (m_hWnd, &r);

      cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
      PWSTR pszRet;

      // start with the first page
      m_pTextureTemp = pLast;
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPAINTDIALOG, PaintDialogPage, this);
      if (pszRet && !_wcsicmp(pszRet, L"discard")) {
         UndoAboutToChange (pLast);
         GUID gCode, gSub;
         BOOL *pf;
         gCode = pLast->m_gCode;
         gSub = pLast->m_gSub;
         pLast->FromTexture (DEFAULTRENDERSHARD, &gCode, &gSub);
         pf = (BOOL*)m_lTextureImageDirty.Get(i);
         *pf = FALSE;
         FillImage (NULL);
         InvalidateRect (m_hWndMap, NULL, FALSE);
      }

      return 0;
   }

   // capture
   SetCapture (m_hWndMap);
   m_fCaptured = TRUE;

   // remember this position
   m_dwButtonDown = dwButton + 1;
   m_pntButtonDown.x = iX;
   m_pntButtonDown.y = iY;
   m_pntMouseLast = m_pntButtonDown;

   switch (dwPointerMode) {
   case IDC_TRACEMOVE:
   case IDC_TRACEROT:
   case IDC_TRACESCALE:
      {
         PPVTEMPLATE pTemp = (PPVTEMPLATE) m_lPVTEMPLATE.Get(m_dwCurTemplate);
         if (!pTemp)
            break;   // nothing to do
         m_mTemplateOrig.Copy (&pTemp->mMatrix);
      }
      break;

   case IDC_PAINTSUN:
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
CPaintView::ButtonUp - Left button down message
*/
LRESULT CPaintView::ButtonUp (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton)
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

   int iX, iY;
   iX = (short) LOWORD(lParam);
   iY = (short) HIWORD(lParam);
   m_pntMouseLast.x = iX;
   m_pntMouseLast.y = iY;

   switch (dwPointerMode) {
   case IDC_PVCOPY:
      {
         // redraw
         InvalidateRect (m_hWndMap, NULL, FALSE);

         // if the mouse positions are the same then do nothing
         if ((m_pntMouseLast.x == m_pntButtonDown.x) && (m_pntMouseLast.y == m_pntButtonDown.y))
            break;   // nothing

         // copy it
         RECT r;
         r.left = min(m_pntMouseLast.x, m_pntButtonDown.x);
         r.right = max(m_pntMouseLast.x, m_pntButtonDown.x);
         r.top = min(m_pntMouseLast.y, m_pntButtonDown.y);
         r.bottom = max(m_pntMouseLast.y, m_pntButtonDown.y);
         CopyOfRegion (&r, 2);   // start off with scale of 2
      }
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
   SetProperCursor (iX, iY);

   return 0;
}

/***********************************************************************************
CPaintView::MouseMove - Left button down message
*/
LRESULT CPaintView::MouseMove (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
   case IDC_PVCOPY:
      {
         // invalidate the window around the box so its drawn
         RECT r;
         r.left = min(pLast.x, min(m_pntButtonDown.x, m_pntMouseLast.x)) - M3DBUTTONSIZE;
         r.right = max(pLast.x, max(m_pntButtonDown.x, m_pntMouseLast.x)) + M3DBUTTONSIZE;
         r.top = min(pLast.y, min(m_pntButtonDown.y, m_pntMouseLast.y)) - M3DBUTTONSIZE;
         r.bottom = max(pLast.y, max(m_pntButtonDown.y, m_pntMouseLast.y)) + M3DBUTTONSIZE;
         InvalidateRect (m_hWndMap, &r, FALSE);
      }
      break;
   case IDC_TRACEMOVE:
   case IDC_TRACEROT:
   case IDC_TRACESCALE:
      {
         PPVTEMPLATE pTemp = (PPVTEMPLATE) m_lPVTEMPLATE.Get(m_dwCurTemplate);
         if (!pTemp)
            break;   // nothing to do
         
         // invert original
         CMatrix mInv;
         m_mTemplateOrig.Invert4 (&mInv);

         // what's the delta from the original
         POINT pDelta;
         pDelta.x = m_pntMouseLast.x - m_pntButtonDown.x;
         pDelta.y = m_pntMouseLast.y - m_pntButtonDown.y;

         CMatrix mTrans, mUnTrans, mChange;
         fp f;
         switch (dwPointerMode) {
         case IDC_TRACEMOVE:
            mTrans.Translation (pDelta.x, pDelta.y, 0);
            mInv.MultiplyRight (&mTrans);
            break;

         case IDC_TRACEROT:
            mTrans.Translation (-m_pntButtonDown.x, -m_pntButtonDown.y, 0);
            mUnTrans.Translation (m_pntButtonDown.x, m_pntButtonDown.y, 0);
            mChange.RotationZ ((fp)pDelta.x / (fp)1000 * 2.0 * PI);
            mInv.MultiplyRight (&mTrans);
            mInv.MultiplyRight (&mChange);
            mInv.MultiplyRight (&mUnTrans);
            break;

         case IDC_TRACESCALE:
            f = pow (2, (fp)(pDelta.x - pDelta.y) / 250.0);
            mTrans.Translation (-m_pntButtonDown.x, -m_pntButtonDown.y, 0);
            mUnTrans.Translation (m_pntButtonDown.x, m_pntButtonDown.y, 0);
            mChange.Scale (f, f, f);
            mInv.MultiplyRight (&mTrans);
            mInv.MultiplyRight (&mChange);
            mInv.MultiplyRight (&mUnTrans);
            break;
         }

         // invert back to generate template
         mInv.Invert4 (&pTemp->mMatrix);
         FillTemplate ();
         InvalidateRect (m_hWndMap, NULL, FALSE);
      }
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
   case IDC_PAINTSUN:
      {
         // conver the x and y to 0..1
         RECT r;
         GetClientRect (m_hWndMap, &r);
         CPoint p;
         fp fLen;
         p.p[0] = (fp)iX / (fp) (r.right - r.left) * 2.0 - 1;
         p.p[1] = -((fp)iY / (fp) (r.bottom - r.top) * 2.0 - 1);
         fLen = sqrt(p.p[0] * p.p[0] + p.p[1] * p.p[1]);
         if (fLen < 1) {
            p.p[2] = sqrt(1 - fLen * fLen);
         }
         else
            p.p[2] = 0;
         p.Normalize();

         m_pLight.Copy (&p);
         FillImage (NULL);
         FillTemplate();
         InvalidateDisplay ();
      }
      break;
   }

   return 0;

}

/***********************************************************************************
CPaintView::Paint - WM_PAINT messge
*/
LRESULT CPaintView::Paint (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
CPaintView::PaintMap - WM_PAINT messge
*/
LRESULT CPaintView::PaintMap (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   PAINTSTRUCT ps;
   HDC hDC;
   hDC = BeginPaint (hWnd, &ps);
   RECT r;
   GetClientRect (hWnd, &r);

   // figure out what want to paint
   RECT rPaint;
   rPaint = ps.rcPaint;
   rPaint.left = max(rPaint.left, 0);
   rPaint.top = max(rPaint.top, 0);
   rPaint.right = min(rPaint.right, (int)m_Image.Width());
   rPaint.bottom = min(rPaint.bottom, (int)m_Image.Height());
   if ((rPaint.right <= rPaint.left) || (rPaint.bottom <= rPaint.top))
      goto done;

   // allocate enough of an image
   if (!m_ImagePaint.Init ((DWORD) (rPaint.right - rPaint.left), (DWORD) (rPaint.bottom - rPaint.top)))
      goto done;
   PIMAGEPIXEL pSrc, pTemp, pDst;
   DWORD x,y;
   DWORD dwBlendImage, dwBlendTemplate;
   dwBlendTemplate = m_dwBlendTemplate;
   dwBlendImage = 256 - dwBlendTemplate;
   if (m_dwBrushEffect != 0) {
      // not using paint over, so dont bother showing overlay
      dwBlendImage = 256;
      dwBlendTemplate = 0;
   }
   for (y = (DWORD)rPaint.top; y < (DWORD)rPaint.bottom; y++) {
      pSrc = m_Image.Pixel ((DWORD)rPaint.left, y);
      pTemp = m_ImageTemplate.Pixel ((DWORD)rPaint.left, y);
      pDst = m_ImagePaint.Pixel (0, y - (DWORD)rPaint.top);
      for (x = (DWORD) rPaint.left; x < (DWORD)rPaint.right; x++, pSrc++, pTemp++, pDst++) {
         pDst->wRed = (WORD) ((dwBlendImage * (DWORD) pSrc->wRed + dwBlendTemplate * (DWORD) pTemp->wRed) / 256);
         pDst->wGreen = (WORD) ((dwBlendImage * (DWORD) pSrc->wGreen + dwBlendTemplate * (DWORD) pTemp->wGreen) / 256);
         pDst->wBlue = (WORD) ((dwBlendImage * (DWORD) pSrc->wBlue + dwBlendTemplate * (DWORD) pTemp->wBlue) / 256);
      } // x
   } // y

   HBITMAP hBit;
   hBit = m_ImagePaint.ToBitmap (hDC);
   if (!hBit)
      goto done;

   // compatible dc
   HDC hDCBit;
   hDCBit = CreateCompatibleDC (hDC);
   SelectObject (hDCBit, hBit);
   BitBlt (hDC, rPaint.left, rPaint.top, rPaint.right - rPaint.left, rPaint.bottom - rPaint.top,
      hDCBit, 0, 0,SRCCOPY);
   DeleteDC (hDCBit);
   DeleteObject (hBit);

   if (m_dwButtonDown && (m_adwPointerMode[m_dwButtonDown-1] == IDC_PVCOPY)) {
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
done:
   EndPaint (hWnd, &ps);
   return 0;

}



/**********************************************************************************
CPaintView::BitmapForItem - Returns the a HBITMAP for the given item. The
size of the bitmap is THUMBSHRINK x THUMBSHRINK

inputs
   HDC         hDC - DC to use
   DWORD       dwIndex - Index
returns
   HBITMAP - Must be freed by caller. If error then null
*/
HBITMAP CPaintView::BitmapFromItem (HDC hDC, DWORD dwIndex)
{
   PPVTEMPLATE pv = (PPVTEMPLATE) m_lPVTEMPLATE.Get(dwIndex);
   if (!pv)
      return FALSE;
   PCTextureImage pti;
   pti = pv->pPaint;

   CImage Image;
   Image.Init (THUMBSHRINK, THUMBSHRINK);

   // scale
   DWORD dwSize;
   dwSize = min (pti->m_dwWidth, pti->m_dwHeight);

   DWORD x,y, xx, yy, dw, i;
   WORD awc[3];
   fp afc[3];
   PIMAGEPIXEL pip;
   pip = Image.Pixel(0,0);
   for (y = 0; y < THUMBSHRINK; y++) {
      for (x = 0; x < THUMBSHRINK; x++, pip++) {
         xx = x * dwSize / THUMBSHRINK + (pti->m_dwWidth - dwSize)/2;
         yy = y * dwSize / THUMBSHRINK + (pti->m_dwHeight - dwSize)/2;
         dw = xx + yy * pti->m_dwWidth;

         // color
         if (pti->m_pbRGB)
            for (i = 0; i < 3; i++)
               awc[i] = Gamma(pti->m_pbRGB[dw*3+i]);
         else
            Gamma(pti->m_cDefColor, awc);
         for (i = 0; i < 3; i++)
            afc[i] = awc[i];

         // glow
         if (pti->m_pbGlow) {
            for (i = 0; i < 3; i++) {
               awc[i] = Gamma(pti->m_pbGlow[dw*3+i]);
               afc[i] += (fp)awc[i] * pti->m_fGlowScale;
            }
         }

         // convert back
         for (i = 0; i < 3; i++) {
            afc[i] = min((fp)0xffff, afc[i]);
            (&pip->wRed)[i] = (WORD) afc[i];
         }

      } // x
   } // y

   return Image.ToBitmap (hDC);
}



/****************************************************************************
PaintAddPage
*/
BOOL PaintAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCPaintView pv = (PCPaintView) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PVTEMPLATE Temp;
         CTextureImage TImage;
         memset (&Temp, 0, sizeof(Temp));
         Temp.fBumpHeight = .01;
         Temp.mMatrix.Identity();
         TImage.m_Material.InitFromID (MATERIAL_FLAT);
         TImage.m_dwWidth = TImage.m_dwHeight = 10;

         if (!_wcsicmp(psz, L"color")) {
            BOOL fCancel = TRUE;
            TImage.m_cDefColor = AskColor (pPage->m_pWindow->m_hWnd, RGB(0xff,0xff,0xff), NULL, NULL, &fCancel);
            wcscpy (Temp.szName, gpszColor);
            if (fCancel)
               break;   // cancelled do nothing further
         }
         else if (!_wcsicmp(psz, L"glowcolor")) {
            BOOL fCancel = TRUE;
            TImage.m_cDefColor = AskColor (pPage->m_pWindow->m_hWnd, RGB(0xff,0xff,0xff), NULL, NULL, &fCancel);
            Temp.fColorAsGlow = TRUE;
            Temp.fNoColor = TRUE;
            wcscpy (Temp.szName, gpszGlowColor);
            if (fCancel)
               break;   // cancelled do nothing further
         }
         else if (!_wcsicmp(psz, L"bumpadd")) {
            TImage.m_cDefColor = RGB(0xff,0xff,0xff);
            Temp.dwColorAsBump = 1;
            Temp.fNoColor = TRUE;
            Temp.fBumpAdds = TRUE;
            wcscpy (Temp.szName, gpszBumpAdd);
         }
         else if (!_wcsicmp(psz, L"bumpmed")) {
            TImage.m_cDefColor = RGB(0x80,0x80,0x80);
            Temp.dwColorAsBump = 1;
            Temp.fNoColor = TRUE;
            wcscpy (Temp.szName, gpszBumpMed);
         }
         else if (!_wcsicmp(psz, L"bumpsub")) {
            TImage.m_cDefColor = RGB(0,0,0);
            Temp.dwColorAsBump = 1;
            Temp.fNoColor = TRUE;
            Temp.fBumpAdds = TRUE;
            wcscpy (Temp.szName, gpszBumpSub);
         }
         else if (!_wcsicmp(psz, L"transadd")) {
            TImage.m_cDefColor = RGB(0xff,0xff,0xff);
            Temp.dwColorAsTrans = 1;
            Temp.fNoColor = TRUE;
            wcscpy (Temp.szName, gpszTransAdd);
         }
         else if (!_wcsicmp(psz, L"transsub")) {
            TImage.m_cDefColor = RGB(0,0,0);
            Temp.dwColorAsTrans = 1;
            Temp.fNoColor = TRUE;
            wcscpy (Temp.szName, gpszTransSub);
         }
         else if (!_wcsicmp(psz, L"specadd")) {
            TImage.m_cDefColor = RGB(0xff,0xff,0xff);
            Temp.dwColorAsSpec = 1;
            Temp.fNoColor = TRUE;
            wcscpy (Temp.szName, gpszSpecAdd);
         }
         else if (!_wcsicmp(psz, L"specsub")) {
            TImage.m_cDefColor = RGB(0,0,0);
            Temp.dwColorAsSpec = 1;
            Temp.fNoColor = TRUE;
            wcscpy (Temp.szName, gpszSpecSub);
         }
         else if (!_wcsicmp(psz, L"image")) {
            OPENFILENAME   ofn;
            char  szTemp[256];
            szTemp[0] = 0;
            memset (&ofn, 0, sizeof(ofn));
   
            // BUGFIX - Set directory
            char szInitial[256];
            GetLastDirectory(szInitial, sizeof(szInitial));
            ofn.lpstrInitialDir = szInitial;

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;
            ofn.lpstrFilter = "All (*.bmp,*.jpg)\0*.bmp;*.jpg\0JPEG (*.jpg)\0*.jpg\0Bitmap (*.bmp)\0*.bmp\0\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Open image file";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = "jpg";
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               return TRUE;   // failed to specify file so go back

            // convert to unicode
            WCHAR szw[256];
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/2);

            CImage Image;
            if (!Image.Init (szw))
               return TRUE;   // failed ot load

            // allocate enough space
            DWORD dw;
            TImage.m_dwWidth = Image.Width();
            TImage.m_dwHeight = Image.Height();
            dw = TImage.m_dwWidth * TImage.m_dwHeight;
            if (!TImage.m_memRGB.Required (dw * 3))
               return TRUE;
            TImage.m_pbRGB = (PBYTE) TImage.m_memRGB.p;
            PIMAGEPIXEL pip;
            pip = Image.Pixel(0,0);
            DWORD i;
            for (i = 0; i < dw; i++, pip++) {
               TImage.m_pbRGB[i*3+0] = UnGamma(pip->wRed);
               TImage.m_pbRGB[i*3+1] = UnGamma(pip->wGreen);
               TImage.m_pbRGB[i*3+2] = UnGamma(pip->wBlue);
            }

            // name
            PWSTR pCur;
            pCur = szw;
            while (wcschr(pCur, L'\\'))
               pCur = wcschr(pCur, L'\\')+1;
            wcscpy (Temp.szName, (wcslen(pCur) < 63) ? pCur : L"File");
         }
         else if (!_wcsicmp(psz, L"texture")) {
            CObjectSurface Surf;
            Surf.m_fUseTextureMap = TRUE;
            Surf.m_gTextureCode = GTEXTURECODE_Grass;
            Surf.m_gTextureSub = GTEXTURESUB_Grass;
            if (!TextureSelDialog (DEFAULTRENDERSHARD, pPage->m_pWindow->m_hWnd, &Surf, WorldGet(DEFAULTRENDERSHARD, NULL))) {
               return TRUE;
            }

            memcpy (&TImage.m_Material, &Surf.m_Material, sizeof(Surf.m_Material));

            if (Surf.m_fUseTextureMap) {
               if (!TImage.FromTexture (DEFAULTRENDERSHARD, &Surf.m_gTextureCode, &Surf.m_gTextureSub))
                  return TRUE;   // error

               // get the name
               WCHAR szName[128];
               if (!TextureNameFromGUIDs(DEFAULTRENDERSHARD, &Surf.m_gTextureCode, &Surf.m_gTextureSub, NULL, NULL, szName))
                  wcscpy (szName, L"Texture");
               szName[63] = 0;   // make sure not too long
               wcscpy (Temp.szName, szName);
            }
            else {   // color
               TImage.m_cDefColor = Surf.m_cColor;
               wcscpy (Temp.szName, gpszColor);
            }
         }
         else if (!_wcsicmp(psz, L"clipboard")) {
            HBITMAP hBit = NULL;
            CImage Image;

            if (OpenClipboard (pPage->m_pWindow->m_hWnd)) {
               hBit = (HBITMAP) GetClipboardData (CF_BITMAP);

               if (hBit)
                  Image.Init (hBit);

               CloseClipboard();
            }

            if (!hBit) {
               pPage->MBWarning (L"There weren't any images on the clipboard.",
                  L"Please copy an image to the clipboard from another program (such as "
                  L"a paint program) and press this button again.");
               return TRUE;
            }

            // allocate enough space
            DWORD dw;
            TImage.m_dwWidth = Image.Width();
            TImage.m_dwHeight = Image.Height();
            dw = TImage.m_dwWidth * TImage.m_dwHeight;
            if (!TImage.m_memRGB.Required (dw * 3))
               return TRUE;
            TImage.m_pbRGB = (PBYTE) TImage.m_memRGB.p;
            PIMAGEPIXEL pip;
            pip = Image.Pixel(0,0);
            DWORD i;
            for (i = 0; i < dw; i++, pip++) {
               TImage.m_pbRGB[i*3+0] = UnGamma(pip->wRed);
               TImage.m_pbRGB[i*3+1] = UnGamma(pip->wGreen);
               TImage.m_pbRGB[i*3+2] = UnGamma(pip->wBlue);
            }

            // name
            wcscpy (Temp.szName, L"Clipboard");
         }
         // NOTE: "text" is handled with a link
         else
            break;   // none of the above

         // clone the existing image
         Temp.pOrig = TImage.Clone();
         if (!Temp.pOrig)
            return TRUE;

         // mods to it
         if (!CreateSecondTexture (&Temp)) {
            if (Temp.pOrig)
               delete Temp.pOrig;
            if (Temp.pPaint)
               delete Temp.pPaint;
            return TRUE;
         }

         // add it
         pv->m_lPVTEMPLATE.Add (&Temp);

         // exit
         pPage->Exit (Back());
         return TRUE;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add new tracing paper";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
PaintTextPage
*/
BOOL PaintTextPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTPAGE ptp = (PTEXTPAGE) pPage->m_pUserData;
   PCPaintView pv = ptp->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         FillStatusColor (pPage, L"fontcolor", ptp->cFont); 
         FillStatusColor (pPage, L"backcolor", ptp->cBack);
         ComboBoxSet (pPage, L"align", ptp->dwHorzAlign);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"text");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR) ptp->pMem->p);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"align")) {
            DWORD dw;
            dw = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;
            if (dw == ptp->dwHorzAlign)
               break;   // no change

            ptp->dwHorzAlign = dw;
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

         if (!_wcsicmp(psz, L"text")) {
            DWORD dwNeeded = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeeded);
            if (!ptp->pMem->Required (dwNeeded+2))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR) ptp->pMem->p, (DWORD)ptp->pMem->m_dwAllocated, &dwNeeded);
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

         if (!_wcsicmp(psz, L"changefont")) {
            COLORREF cr;
            COLORREF *pc;
            pc = &ptp->cFont;
            cr = AskColor (pPage->m_pWindow->m_hWnd, *pc, pPage, L"fontcolor");
            if (cr != *pc) {
               *pc = cr;
               pPage->Exit (RedoSamePage());
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"changeback")) {
            COLORREF cr;
            COLORREF *pc;
            pc = &ptp->cBack;
            cr = AskColor (pPage->m_pWindow->m_hWnd, *pc, pPage, L"backcolor");
            if (cr != *pc) {
               *pc = cr;
               pPage->Exit (RedoSamePage());
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"font")) {
            CHOOSEFONT cf;
            memset (&cf, 0, sizeof(cf));

            cf.lStructSize = sizeof(cf);
            cf.hwndOwner = pPage->m_pWindow->m_hWnd;
            cf.lpLogFont = &ptp->lf;
            cf.Flags = CF_EFFECTS | CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
            cf.rgbColors = ptp->cFont;

            if (ChooseFont (&cf)) {
               ptp->cFont = cf.rgbColors;
               pPage->Exit (RedoSamePage());
            }
            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Text";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"BOLDFONT")) {
            p->pszSubString = (ptp->lf.lfWeight >= FW_BOLD) ? L"<bold>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"UNBOLDFONT")) {
            p->pszSubString = (ptp->lf.lfWeight >= FW_BOLD) ? L"</bold>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ITALICFONT")) {
            p->pszSubString = ptp->lf.lfItalic ? L"<italic>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"UNITALICFONT")) {
            p->pszSubString = ptp->lf.lfItalic ? L"</italic>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"SUBFONT")) {
            MemZero (&gMemTemp);

            WCHAR szTemp[128];
            MemCat (&gMemTemp, L"face=\"");
            MultiByteToWideChar (CP_ACP, 0, ptp->lf.lfFaceName, -1, szTemp, sizeof(szTemp)/2);
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"\" size=");
            MemCat (&gMemTemp, (int) abs(ptp->lf.lfHeight));

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
PaintModPage
*/
BOOL PaintModPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCPaintView pv = (PCPaintView) pPage->m_pUserData;
   PPVTEMPLATE pt = (PPVTEMPLATE) pv->m_lPVTEMPLATE.Get(pv->m_dwCurTemplate);

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // cant delete last one
         if (pv->m_lPVTEMPLATE.Num() < 2) {
            pControl = pPage->ControlFind (L"delete");
            if (pControl)
               pControl->Enable (FALSE);
         }

         // combo
         ComboBoxSet (pPage, L"colorasbump", pt->dwColorAsBump);
         ComboBoxSet (pPage, L"colorastrans", pt->dwColorAsTrans);
         ComboBoxSet (pPage, L"colorasspec", pt->dwColorAsSpec);

         MeasureToString (pPage, L"bumpheight", pt->fBumpHeight);

         pControl = pPage->ControlFind (L"transuse");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pt->fUseTrans);
         pControl = pPage->ControlFind (L"paintignoretrans");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pt->fPaintIgnoreTrans);
         pControl = pPage->ControlFind (L"nocolor");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pt->fNoColor);
         pControl = pPage->ControlFind (L"bumpadds");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pt->fBumpAdds);
         pControl = pPage->ControlFind (L"colorasglow");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pt->fColorAsGlow);

         pControl = pPage->ControlFind (L"transdist");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pt->dwTransDist);


         FillStatusColor (pPage, L"transcolor", pt->cTransColor); 
      }
      break;

   case ESCN_SCROLL:
   //case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"transdist"))
            break;

         DWORD dwVal;
         dwVal = p->pControl->AttribGetInt (Pos());
         if (dwVal == pt->dwTransDist)
            return TRUE;

         pt->dwTransDist = dwVal;

         // update
         CreateSecondTexture (pt);
         pv->FillTemplate();
         InvalidateRect (pv->m_hWndMap, NULL, FALSE);
         return TRUE;
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // since will be one of own, just get all
         MeasureParseString (pPage, L"bumpheight", &pt->fBumpHeight);
         pt->fBumpHeight = max(.0001, pt->fBumpHeight);

         // update
         CreateSecondTexture (pt);
         pv->FillTemplate();
         InvalidateRect (pv->m_hWndMap, NULL, FALSE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"changetrans")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pt->cTransColor, pPage, L"transcolor");
            if (cr != pt->cTransColor) {
               pt->cTransColor = cr;

               // update
               CreateSecondTexture (pt);
               pv->FillTemplate();
               InvalidateRect (pv->m_hWndMap, NULL, FALSE);
            }
            return TRUE;
         }

         BOOL fCheck;
         fCheck = p->pControl->AttribGetBOOL (Checked());

         if (!_wcsicmp(psz, L"transuse"))
            pt->fUseTrans = fCheck;
         else if (!_wcsicmp(psz, L"paintignoretrans"))
            pt->fPaintIgnoreTrans = fCheck;
         else if (!_wcsicmp(psz, L"nocolor"))
            pt->fNoColor = fCheck;
         else if (!_wcsicmp(psz, L"bumpadds"))
            pt->fBumpAdds = fCheck;
         else if (!_wcsicmp(psz, L"colorasglow"))
            pt->fColorAsGlow = fCheck;
         else
            break;   // not one of ours

         // update
         CreateSecondTexture (pt);
         pv->FillTemplate();
         InvalidateRect (pv->m_hWndMap, NULL, FALSE);
         return TRUE;
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         DWORD dw;
         dw = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;

         DWORD *pdw;
         pdw = NULL;
         if (!_wcsicmp(psz, L"colorasbump"))
            pdw = &pt->dwColorAsBump;
         else if (!_wcsicmp(psz, L"colorastrans"))
            pdw = &pt->dwColorAsTrans;
         else if (!_wcsicmp(psz, L"colorasspec"))
            pdw = &pt->dwColorAsSpec;
         else
            break; // not one of ours

         if (dw == *pdw)
            return TRUE;   // no change

         *pdw = dw;

         // update
         CreateSecondTexture (pt);
         pv->FillTemplate();
         InvalidateRect (pv->m_hWndMap, NULL, FALSE);
         return TRUE;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify tracing paper";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TPSIZE")) {
            MemZero (&gMemTemp);

            MemCat (&gMemTemp, (int) pt->pOrig->m_dwWidth);
            MemCat (&gMemTemp, L" x ");
            MemCat (&gMemTemp, (int) pt->pOrig->m_dwHeight);
            MemCat (&gMemTemp, L" pixels");
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TPCONTENTS")) {
            MemZero (&gMemTemp);

            if (pt->pOrig->m_pbRGB)
               MemCat (&gMemTemp, L"<li><bold>Color map</bold></li>");
            else
               MemCat (&gMemTemp, L"<li><bold>No color map</bold></li>");

            if (pt->pOrig->m_pbGlow)
               MemCat (&gMemTemp, L"<li><bold>Glow map</bold></li>");

            if (pt->pOrig->m_pbBump)
               MemCat (&gMemTemp, L"<li><bold>Bump map</bold></li>");

            if (pt->pOrig->m_pbTrans)
               MemCat (&gMemTemp, L"<li><bold>Transparency map</bold></li>");

            if (pt->pOrig->m_pbSpec)
               MemCat (&gMemTemp, L"<li><bold>Specularity map</bold></li>");


            p->pszSubString = (PWSTR) gMemTemp.p;
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
LRESULT CPaintView::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
         m_hWndMap = CreateWindowEx (WS_EX_CLIENTEDGE, "CPaintViewMap", "",
            WS_CHILD, 0, 0, 1, 1, hWnd, NULL, ghInstance, this);
         m_hWndTextList = CreateWindowEx (WS_EX_CLIENTEDGE, "LISTBOX", "",
            WS_VISIBLE | WS_CHILD | WS_VSCROLL |
            LBS_DISABLENOSCROLL | LBS_NOTIFY |
            LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED,
            0, 0, 1, 1, hWnd,
            (HMENU) IDC_TEXTLIST, ghInstance, NULL);
         m_hWndBlend = CreateWindowEx (0, "SCROLLBAR", "",
            WS_VISIBLE | WS_CHILD|
            SBS_HORZ,
            0, 0, 1, 1, hWnd,
            (HMENU) IDC_BLEND, ghInstance, NULL);

         // set the limits
         SCROLLINFO si;
         memset (&si, 0, sizeof(si));
         si.cbSize = sizeof(si);
         si.fMask = SIF_ALL;
         si.nMax = 256;
         si.nMin = 0;
         si.nPage = 0;
         si.nPos = si.nTrackPos = (int) m_dwBlendTemplate;
         SetScrollInfo (m_hWndBlend, SB_CTL, &si, FALSE);

         // update items for texture and forest list
         UpdateTextList ();
      }

      return 0;

   case WM_MEASUREITEM:
      if (wParam == IDC_TEXTLIST) {
         PMEASUREITEMSTRUCT pmi = (PMEASUREITEMSTRUCT) lParam;
         pmi->itemHeight = THUMBSHRINK + TBORDER;
         return TRUE;
      }
      break;

   case WM_DRAWITEM:
      if (wParam == IDC_TEXTLIST) {
         PDRAWITEMSTRUCT pdi = (PDRAWITEMSTRUCT) lParam;

         DWORD dwNum;
         PPVTEMPLATE pTemp;
         dwNum = m_lPVTEMPLATE.Num();
         pTemp = (PPVTEMPLATE) m_lPVTEMPLATE.Get((DWORD) pdi->itemID);

         // because list boxes don't tell us if the selection has changed, use
         // this as a technique to know
         //DWORD dwSel;
         //dwSel = (DWORD) SendMessage (pdi->hwndItem, LB_GETCURSEL, 0, 0);
         //if (dwSel < dwNum)
         // m_dwCurTemplate = dwSel
         if ((pdi->itemState & ODS_SELECTED) && ((DWORD)pdi->itemID < dwNum)) {
            if (m_dwCurTemplate != (DWORD)pdi->itemID) {
               m_dwCurTemplate = (DWORD) pdi->itemID;
               FillTemplate ();
               InvalidateRect (m_hWndMap, NULL, FALSE);
            }
         }

         BOOL fSel;
         // NOTE: can't do the first fSel because the lsit box doesnt draw correctly
         // when do that

         //fSel = ((DWORD) pdi->itemID == m_dwCurTemplate) && ((DWORD)pdi->itemID < dwNum);
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
         if (pTemp) {
            COLORREF cr = RGB(0x40,0x40,0x40);
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
         hBit = BitmapFromItem (pdi->hDC, (DWORD) pdi->itemID);
         if (hBit) {
            HDC hDCMem = CreateCompatibleDC (pdi->hDC);
            if (hDCMem)
               SelectObject (hDCMem, hBit);
            BitBlt (pdi->hDC, pdi->rcItem.left + TBORDER/2, pdi->rcItem.top + TBORDER/2, THUMBSHRINK, THUMBSHRINK,
               hDCMem, 0, 0, SRCCOPY);
            if (hDCMem)
               DeleteDC (hDCMem);

            DeleteObject (hBit);
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
            if (pTemp)
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

   case WM_HSCROLL:
   case WM_VSCROLL:
      {
         // only deal with horizontal scroll
         HWND hWndScroll = m_hWndBlend;
         if ((HWND)lParam != hWndScroll)
            break;   // dont do thos

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
            si.nPos  -= max(si.nMax / 16, 1);
            break;

         case SB_LINEDOWN:
         //case SB_LINERIGHT:
            si.nPos  += max(si.nMax / 16, 1);
            break;

         case SB_PAGELEFT:
         //case SB_PAGEUP:
            si.nPos  -= max(si.nMax / 4, 1);
            break;

         case SB_PAGERIGHT:
         //case SB_PAGEDOWN:
            si.nPos  += max(si.nMax / 4, 1);
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

         // write back
         si.nTrackPos = si.nPos;
         si.cbSize = sizeof(si);
         si.fMask = SIF_POS;
         SetScrollInfo (hWndScroll, SB_CTL, &si, TRUE);

         m_dwBlendTemplate = (DWORD) si.nPos;
         InvalidateRect (m_hWndMap, NULL, FALSE);

         return 0;
      }

   case WM_COMMAND:
      switch (LOWORD(wParam)) {

      case IDC_TEXTLIST:
         if (HIWORD(wParam) == LBN_DBLCLK) {
            DWORD dwSel = (DWORD) SendMessage (m_hWndTextList, LB_GETCURSEL, 0,0);
            PPVTEMPLATE pTemp = (PPVTEMPLATE) m_lPVTEMPLATE.Get(dwSel);
            UndoCache();
            if (pTemp && (!_wcsicmp(pTemp->szName, gpszColor) || !_wcsicmp(pTemp->szName, gpszGlowColor))) {
               // if double click on a color then just go directly to color eiditng dialog
               m_dwCurTemplate = dwSel; // just in case

               COLORREF cr;
               cr = AskColor (m_hWnd, pTemp->pOrig->m_cDefColor, NULL, NULL);

               if (cr != pTemp->pOrig->m_cDefColor) {
                  pTemp->pOrig->m_cDefColor = cr;
                  CreateSecondTexture (pTemp);
                  UpdateTextList();
                  FillTemplate ();
                  InvalidateRect (m_hWndMap, NULL, FALSE);
               }
               return 0;
            }
            else if (pTemp) {
               // double-clicked to edit a texture
               CEscWindow cWindow;
               RECT r;
               DialogBoxLocation (m_hWnd, &r);

               cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
               PWSTR pszRet;

               // start with the first page
               m_dwCurTemplate = dwSel; // just in case
               pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPAINTMOD, PaintModPage, this);

               if (pszRet && !_wcsicmp(pszRet, L"delete")) {
                  pTemp = (PPVTEMPLATE) m_lPVTEMPLATE.Get(m_dwCurTemplate);
                  if (pTemp->pOrig)
                     delete pTemp->pOrig;
                  if (pTemp->pPaint)
                     delete pTemp->pPaint;
                  m_lPVTEMPLATE.Remove (m_dwCurTemplate);
                  m_dwCurTemplate = 0;
                  SendMessage (m_hWndTextList, LB_SETCURSEL, (WPARAM) m_dwCurTemplate, 0);
               }

               UpdateTextList();
               FillTemplate ();
               InvalidateRect (m_hWndMap, NULL, FALSE);
               return 0;
            }
            else {
               // double-clicked to edit a texture
               CEscWindow cWindow;
               RECT r;
               DialogBoxLocation (m_hWnd, &r);

               cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE, &r);
               PWSTR pszRet;

               // start with the first page
firstpage:
               pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPAINTADD, PaintAddPage, this);
               
               if (pszRet && !_wcsicmp(pszRet, L"text")) {
                  TEXTPAGE tp;
                  CMem mem;
                  MemZero (&mem);
                  MemCat (&mem, L"Type in your text.");
                  memset (&tp, 0 ,sizeof(tp));
                  tp.pMem = &mem;
                  tp.pv = this;
                  tp.lf.lfHeight = 24;
                  tp.lf.lfCharSet = DEFAULT_CHARSET;
                  tp.lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
                  tp.cFont = RGB(0,0,0);
                  tp.cBack = RGB(0xff,0xff,0xff);
                  strcpy (tp.lf.lfFaceName, "Arial");

                  // fill in all the info
                  HFONT hFont;
                  hFont = CreateFontIndirect (&tp.lf);
                  GetObject (hFont, sizeof(tp.lf), &tp.lf);
                  DeleteObject (hFont);

textpage:
                  pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPAINTTEXT, PaintTextPage, &tp);
                  if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
                     goto textpage;
                  if (pszRet && !_wcsicmp(pszRet, Back()))
                     goto firstpage;

                  // Create the new text to add
                  HDC hDCDesk;
                  HFONT hOld;
                  CMem memANSI;
                  RECT r;
                  if (!memANSI.Required (wcslen((PWSTR) mem.p)+1))
                     return 0;   // error
                  WideCharToMultiByte (CP_ACP, 0, (PWSTR)mem.p, -1, (char*) memANSI.p,
                     (DWORD)memANSI.m_dwAllocated, 0,0);
                  hDCDesk = GetDC (GetDesktopWindow());
                  tp.lf.lfHeight *= 4; // scale it up so dont have aliasing problems
                  tp.lf.lfWidth *= 4; // scale up
                  hFont = CreateFontIndirect (&tp.lf);
                  hOld = (HFONT) SelectObject (hDCDesk, hFont);
                  r.left = r.top = 0;
                  r.right = r.bottom = 2000;
                  DrawText (hDCDesk, (char*)memANSI.p, -1, &r, DT_CALCRECT | DT_WORDBREAK);
                  SelectObject (hDCDesk, hOld);

                  // create new DC
                  HDC hDC;
                  HBITMAP hBit;
                  RECT rBit;
                  rBit.left = rBit.top = 0;
                  rBit.right = r.right * 3  / 2 + 10;
                  rBit.bottom = r.bottom * 3 / 2 + 10;
                  hDC = CreateCompatibleDC (hDCDesk);
                  hBit = CreateCompatibleBitmap (hDCDesk, rBit.right, rBit.bottom);
                  SelectObject (hDC, hBit);
                  SelectObject (hDC, hFont);
                  ReleaseDC (GetDesktopWindow(), hDCDesk);

                  // draw text
                  HBRUSH hbr;
                  hbr = CreateSolidBrush (tp.cBack);
                  FillRect (hDC, &rBit, hbr);
                  DeleteObject (hbr);
                  SetTextColor (hDC, tp.cFont);
                  SetBkMode (hDC, TRANSPARENT);
                  DrawText (hDC, (char*)memANSI.p, -1, &r,
                     ((tp.dwHorzAlign == 0) ? DT_LEFT : 0) |
                     ((tp.dwHorzAlign == 1) ? DT_CENTER : 0) |
                     ((tp.dwHorzAlign == 2) ? DT_RIGHT : 0) |
                     DT_WORDBREAK);
                  DeleteObject (hFont);
                  DeleteDC (hDC);

                  // transfer over
                  PVTEMPLATE Temp;
                  CTextureImage TImage;
                  CImage Image;
                  memset (&Temp, 0, sizeof(Temp));
                  Temp.fBumpHeight = .01;
                  Temp.mMatrix.Scale (4,4,4);   // so dont have aliasing problems
                  TImage.m_Material.InitFromID (MATERIAL_FLAT);

                  Image.Init (hBit);
                  DeleteObject (hBit);

                  // allocate enough space
                  DWORD dw;
                  TImage.m_dwWidth = Image.Width();
                  TImage.m_dwHeight = Image.Height();
                  dw = TImage.m_dwWidth * TImage.m_dwHeight;
                  if (!TImage.m_memRGB.Required (dw * 3))
                     return TRUE;
                  TImage.m_pbRGB = (PBYTE) TImage.m_memRGB.p;
                  PIMAGEPIXEL pip;
                  pip = Image.Pixel(0,0);
                  DWORD i;
                  for (i = 0; i < dw; i++, pip++) {
                     TImage.m_pbRGB[i*3+0] = UnGamma(pip->wRed);
                     TImage.m_pbRGB[i*3+1] = UnGamma(pip->wGreen);
                     TImage.m_pbRGB[i*3+2] = UnGamma(pip->wBlue);
                  }

                  // name
                  wcscpy (Temp.szName, L"Text");

                  // clone the existing image
                  Temp.pOrig = TImage.Clone();
                  if (!Temp.pOrig)
                     return TRUE;

                  // use transparency
                  Temp.cTransColor = tp.cBack;
                  Temp.dwTransDist = 30;
                  Temp.fUseTrans = TRUE;
                  Temp.fPaintIgnoreTrans = TRUE;

                  // mods to it
                  if (!CreateSecondTexture (&Temp)) {
                     if (Temp.pOrig)
                        delete Temp.pOrig;
                     if (Temp.pPaint)
                        delete Temp.pPaint;
                     return TRUE;
                  }

                  // add it
                  m_lPVTEMPLATE.Add (&Temp);

                  // fall through
               }

               // assume added, so set sel to last one
               m_dwCurTemplate = m_lPVTEMPLATE.Num();
               if (m_dwCurTemplate)
                  m_dwCurTemplate--;   // so have right number
               SendMessage (m_hWndTextList, LB_SETCURSEL, (WPARAM) m_dwCurTemplate, 0);
               UpdateTextList();
               FillTemplate ();
               InvalidateRect (m_hWndMap, NULL, FALSE);

               return 0;
            } // double-click
         }
         break;

      case IDC_PVMIRROR:
         {
            PPVTEMPLATE pTemp = (PPVTEMPLATE) m_lPVTEMPLATE.Get(m_dwCurTemplate);
            if (!pTemp)
               return 0;   // nothing to do
            
            // scale
            CMatrix mScale;
            mScale.Scale (-1, 1, 1);
            pTemp->mMatrix.MultiplyRight (&mScale);

            // update
            FillTemplate ();
            InvalidateRect (m_hWndMap, NULL, FALSE);
         }
         return 0;

      case IDC_HELPBUTTON:
         ASPHelp(IDR_HTUTORIALPAINT1);
         return 0;

      case IDC_PVALL:
      case IDC_PVCOLOR:
      case IDC_PVGLOW:
      case IDC_PVTRANS:
      case IDC_PVSPEC:
      case IDC_PVBUMP:
      case IDC_PVMONO:
         NewView (LOWORD(wParam));
         return 0;

      case IDC_SAVE:
         Save (hWnd);
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

      case IDC_PAINTSUN:
      case IDC_TRACESCALE:
      case IDC_TRACEROT:
      case IDC_TRACEMOVE:
      case IDC_PVCOPY:
      case IDC_BRUSH4:
      case IDC_BRUSH8:
      case IDC_BRUSH16:
      case IDC_BRUSH32:
      case IDC_BRUSH64:
      case IDC_PVLIMIT:
      case IDC_PVDIALOG:
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
               MAKEINTRESOURCE(IDR_MENUBRUSHEFFECTPAINT));
            HMENU hSub = GetSubMenu(hMenu,0);

            // cache undo
            UndoCache ();

            DWORD dwCheck;
            dwCheck = 0;
            switch (m_dwBrushEffect) {
            case 0: // tracing paper
               dwCheck = ID_BRUSHEFFECT_LOWER;
               break;
            case 1: // blur
               dwCheck = ID_BRUSHEFFECT_BLUR;
               break;
            case 2: // sharpen
               dwCheck = ID_BRUSHEFFECT_SHARPEN;
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
            case ID_BRUSHEFFECT_LOWER:
               m_dwBrushEffect = 0;
               break;
            case ID_BRUSHEFFECT_BLUR:
               m_dwBrushEffect = 1;
               break;
            case ID_BRUSHEFFECT_SHARPEN:
               m_dwBrushEffect = 2;
               break;
            }

            // invalidate so redraw
            InvalidateRect (m_hWndMap, NULL, FALSE);
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

         int i = (int) m_dwBlendTemplate;
         i += (fZoomIn ? 32 : -32);
         i = max(i,0);
         i = min(i, 256);
         m_dwBlendTemplate = (DWORD) i;

         // move blend in and out
         // write back
         SCROLLINFO si;
         memset (&si, 0, sizeof(si));
         si.nTrackPos = si.nPos = m_dwBlendTemplate;
         si.cbSize = sizeof(si);
         si.fMask = SIF_POS;
         SetScrollInfo (m_hWndBlend, SB_CTL, &si, TRUE);
         InvalidateRect (m_hWndMap, NULL, FALSE);


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
         RECT rMapOld, rMapNew;
         GetClientRect (m_hWndMap, &rMapOld);
         MoveWindow (m_hWndMap, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
         GetClientRect (m_hWndMap, &rMapNew);
         if (!IsWindowVisible (m_hWndMap))
            ShowWindow (m_hWndMap, SW_SHOW);

         // key window
         RECT rc;
         r.left = r.right + VARSCROLLSIZE;
         r.right = iWidth - VARSCREENSIZE;
         rc = r;
         r.bottom -= 2 * VARSCROLLSIZE;
         MoveWindow (m_hWndTextList, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);

         // scrollbar
         r = rc;
         r.top = r.bottom - VARSCROLLSIZE;
         MoveWindow (m_hWndBlend, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);

         // if there's a world and get a resize then rerender
         if (m_pWorld && ((rMapOld.right != rMapNew.right) || (rMapOld.bottom != rMapNew.bottom)))
            NewScene (m_pWorld, m_pRender);
      }
      break;

   case WM_CLOSE:
      {
         // Check for dirty
         BOOL fDirty = FALSE;
         BOOL *pab;
         DWORD i;
         pab = (BOOL*) m_lTextureImageDirty.Get(0);
         for (i = 0; i < m_lPCTextureImage.Num(); i++)
            fDirty |= pab[i];

         // If dirty then ask if want to save
         if (fDirty) {
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
LRESULT CPaintView::MapWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (uMsg == gdwMouseWheel)
      uMsg = WM_MOUSEWHEEL;

   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWndMap = hWnd;
      }
      return 0;



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






/*****************************************************************************
CPaintView::UndoUpdateButtons - Update the undo/redo buttons based on their
current state.
*/
void CPaintView::UndoUpdateButtons (void)
{
   BOOL fUndo = (m_lUndo.Num() || m_plCurUndo);
   BOOL fRedo = (m_lRedo.Num() ? TRUE : FALSE);

   m_pUndo->Enable (fUndo);
   m_pRedo->Enable (fRedo);
}

/*****************************************************************************
CPaintView::UndoAboutToChange - Call this before an object is about to change.
It will remember the current state for undo, and update the buttons.
Also sets dirty flag
*/
void CPaintView::UndoAboutToChange (PCTextureImage pImage)
{
   // set dirty flag
   PCTextureImage *ppti = (PCTextureImage*) m_lPCTextureImage.Get(0);
   BOOL *pab = (BOOL*) m_lTextureImageDirty.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCTextureImage.Num(); i++) {
      if (ppti[i] == pImage) {
         pab[i] = TRUE;
         break;
      }
   }

   BOOL fNewButtons;
   fNewButtons = FALSE;
   // remember
   if (!m_plCurUndo) {
      m_plCurUndo = new CListFixed;
      if (!m_plCurUndo)
         return;
      m_plCurUndo->Init (sizeof(PCTextureImage));
      fNewButtons = TRUE;
   }
   ppti = (PCTextureImage*) m_plCurUndo->Get(0);
   for (i = 0; i < m_plCurUndo->Num(); i++)
      if (IsEqualGUID (ppti[i]->m_gSub, pImage->m_gSub) &&IsEqualGUID (ppti[i]->m_gCode, pImage->m_gCode))
         break;
   if (i >= m_plCurUndo->Num()) {
      pImage = pImage->Clone();
      if (!pImage)
         return;  // error
      m_plCurUndo->Add (&pImage);
   }

   // clear out redo
   DWORD j;
   for (i = 0; i < m_lRedo.Num(); i++) {
      PCListFixed pl = *((PCListFixed*) m_lRedo.Get(i));
      ppti = (PCTextureImage*) pl->Get(0);
      for (j = 0; j < pl->Num(); j++)
         delete ppti[j];
      delete pl;
      fNewButtons = TRUE;
   }
   m_lRedo.Clear();


   // update buttons
   if (fNewButtons)
      UndoUpdateButtons ();
}


/*****************************************************************************
CPaintView::UndoCache - Moves current undo into the cache
*/
void CPaintView::UndoCache (void)
{
   if (!m_plCurUndo)
      return;  // nothing

   // if the undo has more than 5 levels then clear one out
   PCTextureImage *ppti;
   DWORD j;
   if (m_lUndo.Num() >= 5) {
      PCListFixed pl = *((PCListFixed*) m_lUndo.Get(0));
      ppti = (PCTextureImage*) pl->Get(0);
      for (j = 0; j < pl->Num(); j++)
         delete ppti[j];
      delete pl;
      m_lUndo.Remove (0);
   }

   // add
   m_lUndo.Add (&m_plCurUndo);
   m_plCurUndo = NULL;
}


/*****************************************************************************
CPaintView::Undo - Does an undo or redo

inputs
   BOOL     fUndo - If TRUE do an undo, else do a redo
returns
   BOOL - TRUE if success
*/
BOOL CPaintView::Undo (BOOL fUndo)
{
   // cache undo just to make life easier
   UndoCache();

   // where from
   PCListFixed plFrom, plTo;
   if (fUndo) {
      plFrom = &m_lUndo;
      plTo = &m_lRedo;
   }
   else {
      plFrom = &m_lRedo;
      plTo = &m_lUndo;
   }
   
   // if nothing from then error
   if (!plFrom->Num())
      return FALSE;

   // get the list
   PCListFixed plMove;
   plMove = *((PCListFixed*) plFrom->Get(plFrom->Num()-1));
   plFrom->Remove(plFrom->Num()-1);

   // swap
   PCTextureImage *pptiMove = (PCTextureImage*) plMove->Get(0);
   PCTextureImage *pptiCur = (PCTextureImage*) m_lPCTextureImage.Get(0);
   PCTextureImage pClone;
   DWORD i, j;
   BOOL fDirty;
   for (i = 0; i < plMove->Num(); i++) {
      // find out where match cache
      for (j = 0; j < m_lPCTextureImage.Num(); j++)
         if (IsEqualGUID (pptiMove[i]->m_gSub, pptiCur[j]->m_gSub) && IsEqualGUID (pptiMove[i]->m_gCode, pptiCur[j]->m_gCode))
            break;
      if (j >= m_lPCTextureImage.Num()) {
         // had it in the undo buffer but not anymore...
         // just make a clone of what's in the undo and add it to the cur
         pClone = pptiMove[i]->Clone();
         if (!pClone)
            continue;
         fDirty = TRUE;
         m_lPCTextureImage.Add (&pClone);
         m_lTextureImageDirty.Add (&fDirty);
         pptiCur = (PCTextureImage*) m_lPCTextureImage.Get(0); // reset in case memory moved
         continue;
      }

      // else, just swap
      pClone = pptiMove[i];
      pptiMove[i] = pptiCur[j];
      pptiCur[j] = pClone;

      // set dirty flag
      BOOL *pfDirty;
      pfDirty = (BOOL*) m_lTextureImageDirty.Get(j);
      *pfDirty = TRUE;
   } // i

   // add the move onto the end of the other one
   plTo->Add (&plMove);

   // update displays
   FillImage (NULL);
   InvalidateRect (m_hWndMap, NULL, FALSE);
   UndoUpdateButtons ();

   return TRUE;
}

/*****************************************************************************
CPaintView::Save - Saves the information back to the object it got it from.

inputs
   HWND     hWndParent - To show error messages on
returns
   BOOL - TRUE if saved, FALSE if error
*/
BOOL CPaintView::Save (HWND hWndParent)
{
   UndoCache();

   // save
   BOOL *pab;
   PCTextureImage *ppti;
   DWORD i;
   pab = (BOOL*) m_lTextureImageDirty.Get(0);
   ppti = (PCTextureImage*) m_lPCTextureImage.Get(0);
   for (i = 0; i < m_lPCTextureImage.Num(); i++) {
      if (!pab[i])
         continue;   // not dirty

      // save it
      if (!ppti[i]->ToTexture(DEFAULTRENDERSHARD)) {
         WCHAR szTemp[256];
         szTemp[0] = 0;
         TextureNameFromGUIDs (DEFAULTRENDERSHARD, &ppti[i]->m_gCode, &ppti[i]->m_gSub, NULL, NULL, szTemp);
         if (!szTemp)
            wcscpy (szTemp, L"The texture");
         wcscat (szTemp, L" failed to save. Do you wish to continue?");
         int iRet;
         iRet = EscMessageBox (m_hWnd, ASPString(),
            szTemp,
            L"The texture may not have saved because the image files might be write "
            L"protected.",
            MB_ICONQUESTION | MB_YESNO);
         if (iRet == IDNO)
            return FALSE;
      }
      else
         // set all dirty flags to false
         pab[i] = FALSE;
   }


   // done
   EscChime (ESCCHIME_INFORMATION);
   return TRUE;
}



/**********************************************************************************
CPaintView::UpdateTextList - Update the list box for all the textures.
*/
void CPaintView::UpdateTextList (void)
{
   // if the current texture is >= # textures then set it
   if (m_dwCurTemplate >= m_lPVTEMPLATE.Num()) {
      m_dwCurTemplate = 0;
      FillTemplate();
      InvalidateRect (m_hWndMap, NULL, FALSE);
   }

   // clear the list box
   SendMessage (m_hWndTextList, LB_RESETCONTENT, 0,0);

   // loop through all the surfaces and come up with a name
   DWORD i;
   PPVTEMPLATE pTemp;
   pTemp = (PPVTEMPLATE) m_lPVTEMPLATE.Get(0);
   for (i = 0; i < m_lPVTEMPLATE.Num(); i++, pTemp++) {
      char szTemp[256];
      szTemp[0] = 0;
      if (pTemp->szName[0])
         WideCharToMultiByte (CP_ACP, 0, pTemp->szName, -1, szTemp, sizeof(szTemp), 0, 0);
      if (!szTemp[0])
         strcpy (szTemp, "Not named");

      SendMessage (m_hWndTextList, LB_ADDSTRING, 0, (LPARAM) szTemp);
   }

   // add in a final option
   SendMessage (m_hWndTextList, LB_ADDSTRING, 0, (LPARAM) "Double-click to add a new tracing paper...");

   // set selection
   SendMessage (m_hWndTextList, LB_SETCURSEL, (WPARAM) m_dwCurTemplate, 0);
}


/***********************************************************************************
CPaintView::TextureImageInterp - Given a HV and a PCTextureImage, this interpolates
values to produce RGB, etc.

inputs
   PCTextureImage          pImage - Image
   PTEXTUREPOINT           ptp - H and V. Ranges 0..1 are used, although they're modulo
   WORD                    *pwRGB - Filled with RGB. Can be NULL
   float                   *pfGlow - Filled with RGB for glow (multiplied by scale). CAn be NULL
   WORD                    *pwTrans - Fills with transparency. Can be NULL
   fp                      *pfBump - Filled with bump height, in meters. Can be NULL
   WORD                    *pwSpecReflect - Filled with specularity. Can be NULL.
   WORD                    *pwSpecPower - Filled with specularity power. Can be NULL.
returns
   none
*/
void CPaintView::TextureImageInterp (PCTextureImage pImage, PTEXTUREPOINT ptp,
                                     WORD *pwRGB, float *pfGlow, WORD *pwTrans, fp *pfBump,
                                     WORD *pwSpecReflect, WORD *pwSpecPower)
{
   // figure out the location of UL corner
   TEXTUREPOINT tp;
   DWORD dwX, dwY;
   tp = *ptp;
   tp.h = myfmod(tp.h, 1) * (fp)pImage->m_dwWidth;
   tp.v = myfmod(tp.v, 1) * (fp)pImage->m_dwHeight;
   dwX = (DWORD) tp.h;
   dwY = (DWORD) tp.v;
   dwX = min(dwX, pImage->m_dwWidth-1);
   dwY = min(dwY, pImage->m_dwHeight-1);
   tp.h -= dwX;
   tp.v -= dwY;

   // XY locations or corners
   DWORD adw[4];     // 0=UL, 1=UR, 2= LL, 3=LR
   DWORD i , j;
   adw[0] = dwX + dwY * pImage->m_dwWidth;
   adw[1] = ((dwX+1)%pImage->m_dwWidth) + dwY * pImage->m_dwWidth;
   adw[2] = dwX + ((dwY+1)%pImage->m_dwHeight) * pImage->m_dwWidth;
   adw[3] = ((dwX+1)%pImage->m_dwWidth) + ((dwY+1)%pImage->m_dwHeight) * pImage->m_dwWidth;

   // weight
   fp af[4];
   af[0] = (1.0 - tp.h) * (1.0 - tp.v);
   af[1] = tp.h * (1.0 - tp.v);
   af[2] = (1.0 - tp.h) * tp.v;
   af[3] = tp.h * tp.v;

   // color
   if (pwRGB && pImage->m_pbRGB) {
      fp afRGB[3];
      WORD awRGB[3];

      memset (afRGB, 0, sizeof(afRGB));

      for (i = 0; i < 4; i++) {
         for (j = 0; j < 3; j++) {
            awRGB[j] = Gamma (pImage->m_pbRGB[adw[i]*3+j]);
            afRGB[j] += (fp)awRGB[j] * af[i];
         } // j
      } // i

      // write out
      for (j = 0; j < 3; j++)
         pwRGB[j] = (WORD) afRGB[j];
   }
   else if (pwRGB)
      Gamma (pImage->m_cDefColor, pwRGB);

   // glow
   if (pfGlow && pImage->m_pbGlow) {
      fp afRGB[3];
      WORD awRGB[3];

      memset (afRGB, 0, sizeof(afRGB));

      for (i = 0; i < 4; i++) {
         for (j = 0; j < 3; j++) {
            awRGB[j] = Gamma (pImage->m_pbGlow[adw[i]*3+j]);
            afRGB[j] += (fp)awRGB[j] * af[i];
         } // j
      } // i

      // write out
      for (j = 0; j < 3; j++)
         pfGlow[j] = afRGB[j] * pImage->m_fGlowScale;
   }
   else if (pfGlow)
      pfGlow[0] = pfGlow[1] = pfGlow[2] = 0;

   // transparency
   if (pwTrans && pImage->m_pbTrans) {
      fp fTrans = 0;
      for (i = 0; i < 4; i++)
         fTrans += af[i] * (fp)Gamma(pImage->m_pbTrans[adw[i]]);
      *pwTrans = (WORD) fTrans;
   }
   else if (pwTrans)
      *pwTrans = pImage->m_Material.m_wTransparency;

   // bump
   if (pfBump && pImage->m_pbBump) {
      fp fBump = 0;
      for (i = 0; i < 4; i++)
         fBump += af[i] * ((fp)pImage->m_pbBump[adw[i]] - (fp)0x80) / (fp)0x100 * pImage->m_fBumpHeight;
      *pfBump = fBump;
   }
   else if (pfBump)
      *pfBump = 0;

   // spec reflect
   if (pwSpecReflect && pImage->m_pbSpec) {
      fp fSpec = 0;
      for (i = 0; i < 4; i++)
         fSpec += af[i] * (fp)Gamma(pImage->m_pbSpec[adw[i]*2 + 1]);
      *pwSpecReflect = (WORD) fSpec;
   }
   else if (pwSpecReflect)
      *pwSpecReflect = pImage->m_Material.m_wSpecReflect;

   // spec exponent
   if (pwSpecPower && pImage->m_pbSpec) {
      fp fSpec = 0;
      for (i = 0; i < 4; i++)
         fSpec += af[i] * (fp)pImage->m_pbSpec[adw[i]*2 + 0] * 256.0;
      *pwSpecPower = (WORD) fSpec;
   }
   else if (pwSpecPower)
      *pwSpecPower = pImage->m_Material.m_wSpecExponent;
}

/***********************************************************************************
CPaintView::CopyOfRegion - Creates a copy of the region and adds it to the template
list.

inputs
   RECT     *pr - left,top are min, right,bottom are max.
   DWORD    dwScale - Number of sub-pixels to copy in, 1+
returns
   BOOL - TRUE if success
*/
BOOL CPaintView::CopyOfRegion (RECT *pr, DWORD dwScale)
{
   DWORD dwWidth, dwHeight;
   dwWidth = m_Image.Width();
   dwHeight = m_Image.Height();
   pr->right = min(pr->right, (int) dwWidth);
   pr->bottom = min(pr->bottom, (int) dwHeight);
   pr->left = max(pr->left, 0);
   pr->top = max(pr->top, 0);
   if ((pr->right <= pr->left) || (pr->bottom <= pr->top))
      return FALSE;

   // allocate image large enough
   PCTextureImage pImage;
   DWORD dw;
   pImage = new CTextureImage;
   if (!pImage)
      return FALSE;
   pImage->m_dwWidth = (DWORD) (pr->right - pr->left) * dwScale;
   pImage->m_dwHeight = (DWORD) (pr->bottom - pr->top) * dwScale;
   dw = pImage->m_dwWidth * pImage->m_dwHeight;

   // make sure enough bits
   if (!pImage->m_memRGB.Required (dw * 3)) {
      delete pImage;
      return FALSE;
   }
   pImage->m_pbRGB = (PBYTE) pImage->m_memRGB.p;

   if (!pImage->m_memGlow.Required (dw * 3)) {
      delete pImage;
      return FALSE;
   }
   pImage->m_pbGlow = (PBYTE) pImage->m_memGlow.p;

   if (!pImage->m_memSpec.Required (dw * 2)) {
      delete pImage;
      return FALSE;
   }
   pImage->m_pbSpec = (PBYTE) pImage->m_memSpec.p;

   if (!pImage->m_memTrans.Required (dw)) {
      delete pImage;
      return FALSE;
   }
   pImage->m_pbTrans = (PBYTE) pImage->m_memTrans.p;

   if (!pImage->m_memBump.Required (dw)) {
      delete pImage;
      return FALSE;
   }
   pImage->m_pbBump = (PBYTE) pImage->m_memBump.p;

   // loop through the current image and find what materials are used. Use this
   // for min and max
   DWORD dwLast;
   PCTextureImage pLast;
   DWORD x,y, i;
   PRPPIXEL pStart, pip;
   PCTextureImage *ppti;
   ppti = (PCTextureImage*) m_lPCTextureImage.Get(0);
   dwLast = 0;
   pLast = NULL;
   pStart = (PRPPIXEL) m_memRPPIXEL.p;
   PRPTEXTINFO pInfo;
   pInfo = (PRPTEXTINFO) m_lRPTEXTINFO.Get(0);
   for (y = (DWORD)pr->top; y < (DWORD)pr->bottom; y++) {
      pip = pStart + ((DWORD)pr->left + y * dwWidth);
      for (x = (DWORD)pr->left; x < (DWORD)pr->right; x++, pip++) {
         if (!pip->dwTextIndex)
            continue;   // zero
         if (pip->dwTextIndex == dwLast)
            continue;   // no change

         // find match
         PRPTEXTINFO pCur = pInfo + (pip->dwTextIndex - 1);
         for (i = 0; i < m_lPCTextureImage.Num(); i++) {
            pLast = ppti[i];
            if (IsEqualGUID(pLast->m_gSub, pCur->gSub) && IsEqualGUID(pLast->m_gCode, pCur->gCode))
               break;
         }
         if (i >= m_lPCTextureImage.Num())
            continue;   // out of range - shouldnt happen because will alreayd have drawn once and loaded in

         // if this is the first time then remember some stuff
         if (!dwLast) {
            pImage->m_cDefColor = pLast->m_cDefColor;
            pImage->m_fBumpHeight = pLast->m_fBumpHeight;
            pImage->m_fGlowScale = pLast->m_fGlowScale;
            memcpy (&pImage->m_Material, &pLast->m_Material, sizeof(pLast->m_Material));
         }
         else {
            // remember max
            pImage->m_fBumpHeight = max(pImage->m_fBumpHeight, pLast->m_fBumpHeight);
            pImage->m_fGlowScale = max(pImage->m_fBumpHeight, pLast->m_fGlowScale);
         }

         // remember this
         dwLast = pip->dwTextIndex;
      } //x
   } // y

   // if there was no last one then error
   if (!dwLast) {
      delete pImage;
      return FALSE;
   }

   if (!pImage->m_fGlowScale)
      pImage->m_fGlowScale = 1;
   if (!pImage->m_fBumpHeight)
      pImage->m_fBumpHeight = .01;

   // loop over all the material filling in info
   for (y = 0; y < pImage->m_dwHeight; y++) {
      for (x = 0; x < pImage->m_dwWidth; x++) {
         dw = x + y * pImage->m_dwWidth;

         // what's the index and weight into the original image...
         DWORD xx, yy;
         fp fwx, fwy;
         xx = x / dwScale + (DWORD) pr->left;
         yy = y / dwScale + (DWORD) pr->top;
         if (dwScale > 1) {
            fwx = (fp) (x % dwScale) / (fp) dwScale;
            fwy = (fp) (y % dwScale) / (fp) dwScale;
         }
         else
            fwx = fwy = 0;

         // figure out location of 4 cornrners
         DWORD adw[4];     // 0 = UL, 1 = UR, 2 = LL, 3 = LR
         adw[0] = xx + yy * dwWidth;
         adw[1] = min(xx+1,dwWidth-1) + yy * dwWidth;
         adw[2] = xx + min(yy+1,dwHeight-1) * dwWidth;
         adw[3] = min(xx+1,dwWidth-1) + min(yy+1,dwHeight-1) * dwWidth;

         // sum...
         WORD awRGB[3];
         float afGlow[3];
         fp fBump;
         WORD wTrans, wSpecReflect, wSpecExponent;
         awRGB[0] = awRGB[1] = awRGB[2] = 0x4000;  // default
         afGlow[0] = afGlow[1] = afGlow[2] = 0;
         fBump = 0;
         wTrans = pImage->m_Material.m_wTransparency;
         wSpecReflect = pImage->m_Material.m_wSpecReflect;
         wSpecExponent = pImage->m_Material.m_wSpecExponent;

         // only bother calling to check for material if there is something
         if (pStart[adw[0]].dwTextIndex) {
            TEXTUREPOINT tpTop, tpBottom, tp;

            // top HV
            tpTop.h = pStart[adw[0]].afHV[0];
            tpTop.v = pStart[adw[0]].afHV[1];
            if (pStart[adw[0]].dwTextIndex == pStart[adw[1]].dwTextIndex) {
               tpTop.h = tpTop.h * (1.0 - fwx) + fwx * pStart[adw[1]].afHV[0];
               tpTop.v = tpTop.v * (1.0 - fwx) + fwx * pStart[adw[1]].afHV[1];
            }

            // bottom HV
            tpBottom.h = pStart[adw[2]].afHV[0];
            tpBottom.v = pStart[adw[2]].afHV[1];
            if (pStart[adw[2]].dwTextIndex == pStart[adw[3]].dwTextIndex) {
               tpBottom.h = tpBottom.h * (1.0 - fwx) + fwx * pStart[adw[3]].afHV[0];
               tpBottom.v = tpBottom.v * (1.0 - fwx) + fwx * pStart[adw[3]].afHV[1];
            }
            // extra check
            if (pStart[adw[0]].dwTextIndex != pStart[adw[2]].dwTextIndex)
               tpBottom = tpTop; // different surface, so cant interp

            // inbetween
            tp.h = (1.0 - fwy) * tpTop.h + tpBottom.h * fwy;
            tp.v = (1.0 - fwy) * tpTop.v + tpBottom.v * fwy;

            // get this pixel
            pip = pStart + adw[0];
            if (pip->dwTextIndex != dwLast) {
               PRPTEXTINFO pCur = pInfo + (pip->dwTextIndex - 1);
               for (i = 0; i < m_lPCTextureImage.Num(); i++) {
                  pLast = ppti[i];
                  if (IsEqualGUID(pLast->m_gSub, pCur->gSub) && IsEqualGUID(pLast->m_gCode, pCur->gCode))
                     break;
               }
               dwLast = pip->dwTextIndex;
            }

            // know the HV, so get the values
            TextureImageInterp (pLast, &tp, awRGB, afGlow, &wTrans, &fBump,
               &wSpecReflect, &wSpecExponent);
         }

         // write the info out
         for (i = 0; i < 3; i++) {
            pImage->m_pbRGB[dw*3+i] = UnGamma (awRGB[i]);
            afGlow[i] /= pImage->m_fGlowScale;
            afGlow[i] = min(afGlow[i], (fp)0xffff);
            pImage->m_pbGlow[dw*3+i] = UnGamma ((WORD)afGlow[i]);
         }
         pImage->m_pbTrans[dw] = UnGamma(wTrans);
         pImage->m_pbSpec[dw*2+0] = HIBYTE(wSpecExponent);
         pImage->m_pbSpec[dw*2+1] = UnGamma(wSpecReflect);
         fBump = fBump / pImage->m_fBumpHeight * 256.0 + (fp)0x80;      
         fBump = max(0, fBump);
         fBump = min(255, fBump);
         pImage->m_pbBump[dw] = (BYTE) fBump;
      } // x
   } // y

   // create the template for this
   PVTEMPLATE Temp;
   memset (&Temp, 0, sizeof(Temp));
   Temp.cTransColor = RGB(0xff,0xff,0xff);
   Temp.dwTransDist = 0;
   Temp.fBumpHeight = pImage->m_fBumpHeight;
   Temp.mMatrix.Scale (dwScale, dwScale, dwScale); // so appears the same size but with higher detail
   Temp.pOrig = pImage;
   wcscpy (Temp.szName, L"Copy");

   if (!CreateSecondTexture (&Temp)) {
      if (Temp.pOrig)
         delete Temp.pOrig;
      if (Temp.pPaint)
         delete Temp.pPaint;
   }

   // add it
   m_lPVTEMPLATE.Add (&Temp);

   // new selection
   // assume added, so set sel to last one
   m_dwCurTemplate = m_lPVTEMPLATE.Num();
   if (m_dwCurTemplate)
      m_dwCurTemplate--;   // so have right number
   SendMessage (m_hWndTextList, LB_SETCURSEL, (WPARAM) m_dwCurTemplate, 0);
   UpdateTextList();
   FillTemplate ();
   InvalidateRect (m_hWndMap, NULL, FALSE);

   return TRUE;
}


/***********************************************************************************
CPaintView::PaintPoint - Paints one pixel in the destination image, from a pixel
in the source image.

inputs
   int               ix, iy - and and y. NOTE: This might reach beyond edge of pDest's area,
                     so need to be made modulo
   PCTextureImage      pDest - Write to this one
   PTEXTUREPOINT     pt - Location (in pixels) from the source. This may be beyond limits,
                     so will need to be modulo
   PCTextureImage      pSrc - Transfer from this one
   fp                fWeight - Weighting from 0 to 255. If 255 then maximum paint weigting
   BOOL              fNoColor - No color setting from PVTEMPLATE
   BOOL              fPaintIgnoreTrans - From PVTEMPALTE
   BOOL              fBumpAdds - If TRUE the bump maps add
returns
   none
*/
void CPaintView::PaintPoint (int ix, int iy, PCTextureImage pDest,
                             PTEXTUREPOINT pt, PCTextureImage pSrc, fp fWeight,
                             BOOL fNoColor, BOOL fPaintIgnoreTrans, BOOL fBumpAdds)
{
   // do modulo to figure out what point painting
   if (ix < 0)
      ix += ((-ix) / (int) pDest->m_dwWidth + 1) * (int) pDest->m_dwWidth; // because c modulo broken
   if (iy < 0)
      iy += ((-iy) / (int) pDest->m_dwHeight + 1) * (int) pDest->m_dwHeight; // because c modulo broken
   ix = ix % (int) pDest->m_dwWidth;
   iy = iy % (int) pDest->m_dwHeight;

   // if mode is set to blurring/sharpening then special
   if (m_dwBrushEffect != 0) {
      BLURPIXEL bp;
      bp.pImage = pDest;
      bp.wX = (WORD) ix;
      bp.wY = (WORD) iy;
      bp.bStrength = (BYTE) fWeight;
      if (bp.bStrength)
         m_lBLURPIXEL.Add (&bp);
      return;
   }

   // figure out which points want to paint
   BOOL fPaintRGB, fPaintGlow, fPaintBump, fPaintTrans, fPaintSpec;
   BOOL fWantTrans;
   fPaintRGB = (pDest->m_pbRGB && !fNoColor);
   fPaintGlow = (pDest->m_pbGlow && pSrc->m_pbGlow);
   fPaintBump = (pDest->m_pbBump && pSrc->m_pbBump);
   fPaintTrans = (pDest->m_pbTrans && !pDest->m_fTransUse && pSrc->m_pbTrans && !fPaintIgnoreTrans);
   fPaintSpec = (pDest->m_pbSpec && pSrc->m_pbSpec);
   switch (m_dwView) {
   case IDC_PVCOLOR:
      fPaintGlow = fPaintBump = fPaintTrans = fPaintSpec = FALSE;
      break;
   case IDC_PVGLOW:
      fPaintRGB = fPaintBump = fPaintTrans = fPaintSpec = FALSE;
      break;
   case IDC_PVTRANS:
      fPaintRGB = fPaintGlow = fPaintBump = fPaintSpec  =FALSE;
      break;
   case IDC_PVSPEC:
      fPaintRGB = fPaintGlow = fPaintBump = fPaintTrans = FALSE;
      break;
   case IDC_PVBUMP:
      fPaintRGB = fPaintGlow = fPaintTrans = fPaintSpec = FALSE;
      break;
   case IDC_PVMONO:
      fPaintRGB = fPaintGlow = FALSE;
      break;
   case IDC_PVALL:
   default:
      break;   // do nothing
   }
   if (!pSrc->m_pbTrans)
      fPaintIgnoreTrans = FALSE; // no map, so dont bother with ignore trans
   fWantTrans = fPaintTrans || fPaintIgnoreTrans;

   // if nothing to paint then just quit now
   if (!(fPaintRGB || fPaintGlow || fPaintBump || fPaintTrans || fPaintSpec))
      return;

   // get the points, interpolated
   TEXTUREPOINT tp;
   tp = *pt;
   if (fPaintIgnoreTrans) {
      // because probably painting text, dont interpolate color or transparencey
      tp.h = floor (tp.h);
      tp.v = floor (tp.v);
   }
   tp.h /= (fp) pSrc->m_dwWidth;
   tp.v /= (fp) pSrc->m_dwHeight;
   WORD awRGB[3];
   float afGlow[3];
   WORD wTrans, wSpecReflect, wSpecExponent;
   fp fBump;
   TextureImageInterp (pSrc, &tp,
      fPaintRGB ? awRGB : NULL,
      fPaintGlow ? afGlow : NULL,
      fWantTrans ? &wTrans : NULL,
      fPaintBump ? &fBump : NULL,
      fPaintSpec ? &wSpecReflect : NULL,
      fPaintSpec ? &wSpecExponent : NULL);

   // what is the strength?
   fp fStrength, fOneMinus;
   fStrength = (fp) m_dwBrushStrength / 10.0;
   fStrength  *= fWeight / 255.0;
   if (fPaintIgnoreTrans)
      fStrength *= (1.0 - (fp) wTrans / (fp)0xffff);
   // If airbrush then weaker strength
   if (m_dwBrushAir == 2)
      fStrength /= ((fp) AIRBRUSHPERSEC/2.0);
   if (fStrength < CLOSE)
      return;  // too weak to matter
   fOneMinus = 1.0 - fStrength;

   DWORD dw, i;
   WORD w;
   fp f;
   dw = (DWORD)ix + (DWORD)iy * pDest->m_dwWidth;
   if (fPaintRGB) {
      for (i = 0; i < 3; i++) {
         w = Gamma(pDest->m_pbRGB[dw*3+i]);
         w = (WORD) ((fp) w * fOneMinus + (fp)awRGB[i] * fStrength);
         pDest->m_pbRGB[dw*3+i] = UnGamma(w);
      }
   }

   if (fPaintGlow) {
      if (!pDest->m_fGlowScale)
         pDest->m_fGlowScale = 1;
      for (i = 0; i < 3; i++) {
         w = Gamma(pDest->m_pbGlow[dw*3+i]);
         f = (fp)w * fOneMinus + fStrength * afGlow[i] / pDest->m_fGlowScale;
         f = min(f, (fp)0xffff);
         pDest->m_pbGlow[dw*3+i] = UnGamma((WORD)f);
      }
   }

   if (fPaintTrans) {
      w = Gamma(pDest->m_pbTrans[dw]);
      w = (WORD) ((fp) w * fOneMinus + (fp)wTrans * fStrength);
      pDest->m_pbTrans[dw] = UnGamma(w);
   }

   if (fPaintSpec) {
      w = Gamma(pDest->m_pbSpec[dw*2+1]);
      w = (WORD) ((fp) w * fOneMinus + (fp)wSpecReflect * fStrength);
      pDest->m_pbSpec[dw*2+1] = UnGamma(w);

      // BUGFIX - Dont paint the exponent
      //w = pDest->m_pbSpec[dw*2+0];
      //w = (WORD) ((fp) w * fOneMinus + (fp) HIBYTE(wSpecExponent) * fStrength);
      //pDest->m_pbSpec[dw*2+0] = (BYTE)w;
   }

   if (fPaintBump) {
      if (!pDest->m_fBumpHeight)
         pDest->m_fBumpHeight = .01;
      f = ((fp) pDest->m_pbBump[dw] - (fp) 0x80) / 256.0 * pDest->m_fBumpHeight;
      if (fBumpAdds)
         f += fStrength * fBump;
      else
         f = f * fOneMinus + fStrength * fBump;
      f = (f * 256.0 / pDest->m_fBumpHeight) + (fp)0x80;
      f = max(0, f);
      f = min(255,f);
      pDest->m_pbBump[dw] = (BYTE)f;
   }
}


// BUGFIX - Put this in because wasnt able to paint in release mode
// #pragma optimize ("", off)


/***********************************************************************************
CPaintView::PaintTriRegion - Given a triangle covering a portion of the image (probably
sub-pixel), this paints it using the other image specified.

NOTE: This looks at some globals to determine what kind of painting takes place.

inputs
   PTEXTUREPOINT     patpDest - Pointer to an array of 3 verticies. These are the HV
                     values in the destination PCTextutureImage. They have already
                     been multiplied by the width and height of the destination image,
                     so they go from 0..width,0..height (in pixels). However, they can
                     extend below/beyond since modulo math will be done SO LONG as the
                     points are clustered on in the same pre-modulo area.
   PCTextureImage      pDest - Write to this one
   PTEXTUREPOINT     patpSrc - Pointer to an array of the three verticies in the source.
                     Same basic info as patpDest
   PCTextureImage      pSrc - Transfer from this one
   PBYTE             pabWeight - Pointer to an array of 3 bytes for the "weight/pressure"
                     of the paintbrush in the area. 0 is none, 255 is max.
   BOOL              fNoColor - No color setting from PVTEMPLATE
   BOOL              fPaintIgnoreTrans - From PVTEMPALTE
   BOOL              fBumpAdds - From PVTEMPLATE
returns
   none
*/
void CPaintView::PaintTriRegion (PTEXTUREPOINT patpDest, PCTextureImage pDest,
                                 PTEXTUREPOINT patpSrc, PCTextureImage pSrc,
                                 PBYTE pabWeight, BOOL fNoColor, BOOL fPaintIgnoreTrans, BOOL fBumpAdds)
{
   // figure out which vertex is in the middle (on the destination)
   DWORD dwTop, dwBottom, dwMid, dw;
   dwTop = 0;
   dwMid = 1;
   dwBottom = 2;
   if (patpDest[dwTop].v > patpDest[dwMid].v) {
      dw = dwTop;
      dwTop = dwMid;
      dwMid = dw;
   }
   if (patpDest[dwTop].v > patpDest[dwBottom].v) {
      dw = dwTop;
      dwTop = dwBottom;
      dwBottom = dw;
   }
   if (patpDest[dwMid].v > patpDest[dwBottom].v) {
      dw = dwMid;
      dwMid = dwBottom;
      dwBottom = dw;
   }

   // what range of points to explore?
   fp fTop, fBottom, fLeft, fRight;
   fTop = ceil(patpDest[dwTop].v);
   fBottom = ceil(patpDest[dwBottom].v);  // do ceil specifically so if ends exactly on bottom ignored
   fLeft = ceil(min(min(patpDest[0].h, patpDest[1].h), patpDest[2].h));
   fRight = floor(max(max(patpDest[0].h, patpDest[1].h), patpDest[2].h));

   // integer
   int iTop, iBottom, iLeft, iRight;
   iTop = (int) fTop;
   iBottom = (int) fBottom;
   iLeft = (int) fLeft;
   iRight = (int) fRight;

   // if no chance then get out now
   if ((iBottom < iTop) || (iRight < iLeft))
      return;

   // loop
   int x, y;
   DWORD dwShort, dwLong;
   TEXTUREPOINT tpTop[2], tpDelta[2];  // [0] = short, [1] = long
   TEXTUREPOINT atpSrc[2][2];  // [0=short,1=long][0=top,1=bottom]
   fp afWeight[2][2];
   BOOL afVert[2];
   fp afm[2], afmInv[2], afb[2];
   dwLong = (((dwTop+1)%3) == dwBottom) ? dwTop : dwBottom; // long size from top to bottom

   // calculate deltas
   if (patpDest[dwLong].v < patpDest[(dwLong+1)%3].v) {
      tpTop[1] = patpDest[dwLong];
      tpDelta[1] = patpDest[(dwLong+1)%3];

      atpSrc[1][0] = patpSrc[dwLong];
      atpSrc[1][1] = patpSrc[(dwLong+1)%3];
      afWeight[1][0] = (fp)pabWeight[dwLong];
      afWeight[1][1] = (fp)pabWeight[(dwLong+1)%3];
   }
   else {
      tpDelta[1] = patpDest[dwLong];
      tpTop[1] = patpDest[(dwLong+1)%3];

      atpSrc[1][1] = patpSrc[dwLong];
      atpSrc[1][0] = patpSrc[(dwLong+1)%3];
      afWeight[1][1] = (fp)pabWeight[dwLong];
      afWeight[1][0] = (fp)pabWeight[(dwLong+1)%3];
   }
   tpDelta[1].h -= tpTop[1].h;
   tpDelta[1].v -= tpTop[1].v;
   if (tpDelta[1].v < EPSILON)
      return;  // shouldnt happen, but it might
   afVert[1] = (fabs(tpDelta[1].h) < EPSILON);
   afm[1] = afVert[1] ? 0 : (tpDelta[1].v / tpDelta[1].h);
   afmInv[1] = tpDelta[1].h / tpDelta[1].v;
   afb[1] = tpTop[1].v - afm[1] * tpTop[1].h;


   DWORD dwLastShort, i;
   fp f;
   dwLastShort = -1;
   for (y = iTop; y < iBottom; y++) {
      // figure out if this is above the mid point
      if ((fp)y <= patpDest[dwMid].v)  // BUGFIX - Was >
         dwShort = (((dwMid+1)%3) == dwTop) ? dwMid : dwTop;   // short side, mid to top
      else
         dwShort = (((dwMid+1)%3) == dwBottom) ? dwMid : dwBottom; // short side, mid to bottom

      if (dwShort != dwLastShort) {
         // copy these points over so can easily have delta
         if (patpDest[dwShort].v < patpDest[(dwShort+1)%3].v) {
            tpTop[0] = patpDest[dwShort];
            tpDelta[0] = patpDest[(dwShort+1)%3];

            atpSrc[0][0] = patpSrc[dwShort];
            atpSrc[0][1] = patpSrc[(dwShort+1)%3];
            afWeight[0][0] = (fp)pabWeight[dwShort];
            afWeight[0][1] = (fp)pabWeight[(dwShort+1)%3];
         }
         else {
            tpDelta[0] = patpDest[dwShort];
            tpTop[0] = patpDest[(dwShort+1)%3];

            atpSrc[0][1] = patpSrc[dwShort];
            atpSrc[0][0] = patpSrc[(dwShort+1)%3];
            afWeight[0][1] = (fp)pabWeight[dwShort];
            afWeight[0][0] = (fp)pabWeight[(dwShort+1)%3];
         }
         tpDelta[0].h -= tpTop[0].h;
         tpDelta[0].v -= tpTop[0].v;

         // if either of the deltas in v are very small then too small to draw
         if (tpDelta[0].v < EPSILON)
            continue;
         afVert[0] = (fabs(tpDelta[0].h) < EPSILON);
         afm[0] = afVert[0] ? 0 : (tpDelta[0].v / tpDelta[0].h);
         afmInv[0] = tpDelta[0].h / tpDelta[0].v;
         afb[0] = tpTop[0].v - afm[0] * tpTop[0].h;

         dwLastShort = dwShort;
      }

      // see where the two lines cross at this y
      fp afCross[2];
      afCross[0] = afVert[0] ? tpTop[0].h : (((fp) y - afb[0]) * afmInv[0]);
      afCross[1] = afVert[1] ? tpTop[1].h : (((fp) y - afb[1]) * afmInv[1]);

      // whic is left and which is right
      DWORD dwLeft, dwRight;
      dwLeft = (afCross[0] < afCross[1]) ? 0 : 1;
      dwRight = (dwLeft+1)%2;

      // interpolate the source HV
      TEXTUREPOINT atpInterp[2];
      fp afInterpWeight[2];
      for (i = 0; i < 2; i++) {
         f = ((fp)y - tpTop[i].v) / tpDelta[i].v;
         atpInterp[i].h = (1.0 - f) * atpSrc[i][0].h + f * atpSrc[i][1].h;
         atpInterp[i].v = (1.0 - f) * atpSrc[i][0].v + f * atpSrc[i][1].v;
         afInterpWeight[i] = (1.0 - f) * afWeight[i][0] + f * afWeight[i][1];
      }

      fp fx;
      for (x = iLeft; x <= iRight; x++) {
         fx = (fp)x;

         // if beyond left or right then skip
         if ((fx < afCross[dwLeft]) || (fx >= afCross[dwRight]))
            continue;

         // interpolate across
         fp f;
         TEXTUREPOINT tpFinal;
         fp fFinalWeight;
         f = afCross[dwRight] - afCross[dwLeft];
         if (f > EPSILON) {
            f = (fx - afCross[dwLeft]) / f;
            tpFinal.h = (1.0 - f) * atpInterp[dwLeft].h + f * atpInterp[dwRight].h;
            tpFinal.v = (1.0 - f) * atpInterp[dwLeft].v + f * atpInterp[dwRight].v;
            fFinalWeight = (1.0 - f) * afInterpWeight[dwLeft] + f * afInterpWeight[dwRight];
         }
         else {
            tpFinal = atpInterp[0]; // since no distance between, take either
            fFinalWeight = afInterpWeight[0];
         }

         // have the point, so paint it
         PaintPoint (x, y, pDest, &tpFinal, pSrc, fFinalWeight,
            fNoColor, fPaintIgnoreTrans, fBumpAdds);
      } // x
   } // y
}

// BUGFIX - Put this in because wasnt able to paint in release mode
// #pragma optimize ("", on)

/*****************************************************************************
CPaintView::PaintBlur - Looks the m_lBLURPIXEL and applies blur or sharpen for
all the pixels.
*/
void CPaintView::PaintBlur (void)
{
   if (m_dwBrushEffect == 0)
      return;  // no effect

   // NOTE: Dont need to set dirty flags because would have been set when m_lBLURPIXEL
   // was added to

   PBLURPIXEL pbOrig, pb;
   DWORD i, dwNum;
   pbOrig = (PBLURPIXEL) m_lBLURPIXEL.Get(0);
   dwNum = m_lBLURPIXEL.Num();

   // what is the strength?
   fp fStrengthOrig, fStrength, fOneMinus;
   fStrengthOrig = (fp) m_dwBrushStrength / 10.0;
   // If airbrush then weaker strength
   if (m_dwBrushAir == 2)
      fStrengthOrig /= ((fp) AIRBRUSHPERSEC/2.0);

   // what gets painted
   BOOL fPaintRGB, fPaintGlow, fPaintBump, fPaintTrans, fPaintSpec;
   fPaintRGB = fPaintGlow = fPaintBump = fPaintTrans = fPaintSpec = TRUE;
   switch (m_dwView) {
   case IDC_PVCOLOR:
      fPaintGlow = fPaintBump = fPaintTrans = fPaintSpec = FALSE;
      break;
   case IDC_PVGLOW:
      fPaintRGB = fPaintBump = fPaintTrans = fPaintSpec = FALSE;
      break;
   case IDC_PVTRANS:
      fPaintRGB = fPaintGlow = fPaintBump = fPaintSpec  =FALSE;
      break;
   case IDC_PVSPEC:
      fPaintRGB = fPaintGlow = fPaintBump = fPaintTrans = FALSE;
      break;
   case IDC_PVBUMP:
      fPaintRGB = fPaintGlow = fPaintTrans = fPaintSpec = FALSE;
      break;
   case IDC_PVMONO:
      fPaintRGB = fPaintGlow = FALSE;
      break;
   case IDC_PVALL:
   default:
      break;   // do nothing
   }

#define BLURAMT      2

   // RGB
   DWORD j, dw;
   fp f;
   int x, y;
   if (fPaintRGB) for (j = 0; j < 3; j++) {
      for (i = 0, pb = pbOrig; i < dwNum; i++, pb++) {
         if (!pb->pImage->m_pbRGB)
            continue;   // no RGB to blue
         pb->fValue = 0;
         for (y = (int)pb->wY - BLURAMT; y <= (int)pb->wY+BLURAMT; y++)
            for (x = (int)pb->wX - BLURAMT; x <= (int)pb->wX+BLURAMT; x++)
               pb->fValue += Gamma(pb->pImage->m_pbRGB[(
                     (x + (int) pb->pImage->m_dwWidth) % (int) pb->pImage->m_dwWidth +
                     ((y + (int) pb->pImage->m_dwHeight) % (int) pb->pImage->m_dwHeight) * (int) pb->pImage->m_dwWidth
                  )*3 + j]);
         pb->fValue /= ((2*BLURAMT+1) * (2*BLURAMT+1));
      }

      // go back over and average
      for (i = 0, pb = pbOrig; i < dwNum; i++, pb++) {
         if (!pb->pImage->m_pbRGB)
            continue;   // no RGB to blue

         dw = (DWORD) pb->wX + (DWORD)pb->wY * pb->pImage->m_dwWidth;

         fStrength = fStrengthOrig * (fp) pb->bStrength / 255.0;
         fOneMinus = 1.0 - fStrength;

         if (m_dwBrushEffect == 1)  // blur
            f = pb->fValue * fStrength + (fp)Gamma(pb->pImage->m_pbRGB[dw*3+j]) * fOneMinus;
         else  // sharpen
            f = (fp)Gamma(pb->pImage->m_pbRGB[dw*3+j])+ ((fp)Gamma(pb->pImage->m_pbRGB[dw*3+j]) - pb->fValue) * fStrength;

         f = max(0, f);
         f = min((fp)0xffff, f);
         pb->pImage->m_pbRGB[dw*3+j] = UnGamma((WORD)f);
      } // i
   } // j

   // glow
   if (fPaintGlow) for (j = 0; j < 3; j++) {
      for (i = 0, pb = pbOrig; i < dwNum; i++, pb++) {
         if (!pb->pImage->m_pbGlow)
            continue;   // no Glow to blue
         pb->fValue = 0;
         for (y = (int)pb->wY - BLURAMT; y <= (int)pb->wY+BLURAMT; y++)
            for (x = (int)pb->wX - BLURAMT; x <= (int)pb->wX+BLURAMT; x++)
               pb->fValue += Gamma(pb->pImage->m_pbGlow[(
                     (x + (int) pb->pImage->m_dwWidth) % (int) pb->pImage->m_dwWidth +
                     ((y + (int) pb->pImage->m_dwHeight) % (int) pb->pImage->m_dwHeight) * (int) pb->pImage->m_dwWidth
                  )*3 + j]);
         pb->fValue /= ((2*BLURAMT+1) * (2*BLURAMT+1));
      }

      // go back over and average
      for (i = 0, pb = pbOrig; i < dwNum; i++, pb++) {
         if (!pb->pImage->m_pbGlow)
            continue;   // no Glow to blue

         dw = (DWORD) pb->wX + (DWORD)pb->wY * pb->pImage->m_dwWidth;

         fStrength = fStrengthOrig * (fp) pb->bStrength / 255.0;
         fOneMinus = 1.0 - fStrength;

         if (m_dwBrushEffect == 1)  // blur
            f = pb->fValue * fStrength + (fp)Gamma(pb->pImage->m_pbGlow[dw*3+j]) * fOneMinus;
         else  // sharpen
            f = (fp)Gamma(pb->pImage->m_pbGlow[dw*3+j])+ ((fp)Gamma(pb->pImage->m_pbGlow[dw*3+j]) - pb->fValue) * fStrength;

         f = max(0, f);
         f = min((fp)0xffff, f);
         pb->pImage->m_pbGlow[dw*3+j] = UnGamma((WORD)f);
      } // i
   } // j

   if (fPaintSpec) {
      for (i = 0, pb = pbOrig; i < dwNum; i++, pb++) {
         if (!pb->pImage->m_pbSpec)
            continue;   // no Spec to blur
         pb->fValue = 0;
         for (y = (int)pb->wY - BLURAMT; y <= (int)pb->wY+BLURAMT; y++)
            for (x = (int)pb->wX - BLURAMT; x <= (int)pb->wX+BLURAMT; x++)
               pb->fValue += Gamma(pb->pImage->m_pbSpec[(
                     (x + (int) pb->pImage->m_dwWidth) % (int) pb->pImage->m_dwWidth +
                     ((y + (int) pb->pImage->m_dwHeight) % (int) pb->pImage->m_dwHeight) * (int) pb->pImage->m_dwWidth
                  )*2 + 1]);
         pb->fValue /= ((2*BLURAMT+1) * (2*BLURAMT+1));
      }

      // go back over and average
      for (i = 0, pb = pbOrig; i < dwNum; i++, pb++) {
         if (!pb->pImage->m_pbSpec)
            continue;   // no Spec to blue

         dw = (DWORD) pb->wX + (DWORD)pb->wY * pb->pImage->m_dwWidth;

         fStrength = fStrengthOrig * (fp) pb->bStrength / 255.0;
         fOneMinus = 1.0 - fStrength;

         if (m_dwBrushEffect == 1)  // blur
            f = pb->fValue * fStrength + (fp)Gamma(pb->pImage->m_pbSpec[dw*2+1]) * fOneMinus;
         else  // sharpen
            f = (fp)Gamma(pb->pImage->m_pbSpec[dw*2+1])+ ((fp)Gamma(pb->pImage->m_pbSpec[dw*2+1]) - pb->fValue) * fStrength;

         f = max(0, f);
         f = min((fp)0xffff, f);
         pb->pImage->m_pbSpec[dw*2+1] = UnGamma((WORD)f);
      } // i
   } // spec



   if (fPaintTrans) {
      for (i = 0, pb = pbOrig; i < dwNum; i++, pb++) {
         if (!pb->pImage->m_pbTrans)
            continue;   // no Trans to blur

         // if generating transparency from color then dont allow to paint transparency
         if (pb->pImage->m_fTransUse)
            continue;

         pb->fValue = 0;
         for (y = (int)pb->wY - BLURAMT; y <= (int)pb->wY+BLURAMT; y++)
            for (x = (int)pb->wX - BLURAMT; x <= (int)pb->wX+BLURAMT; x++)
               pb->fValue += Gamma(pb->pImage->m_pbTrans[(
                     (x + (int) pb->pImage->m_dwWidth) % (int) pb->pImage->m_dwWidth +
                     ((y + (int) pb->pImage->m_dwHeight) % (int) pb->pImage->m_dwHeight) * (int) pb->pImage->m_dwWidth
                  )]);
         pb->fValue /= ((2*BLURAMT+1) * (2*BLURAMT+1));
      }

      // go back over and average
      for (i = 0, pb = pbOrig; i < dwNum; i++, pb++) {
         if (!pb->pImage->m_pbTrans)
            continue;   // no Trans to blue

         dw = (DWORD) pb->wX + (DWORD)pb->wY * pb->pImage->m_dwWidth;

         fStrength = fStrengthOrig * (fp) pb->bStrength / 255.0;
         fOneMinus = 1.0 - fStrength;

         if (m_dwBrushEffect == 1)  // blur
            f = pb->fValue * fStrength + (fp)Gamma(pb->pImage->m_pbTrans[dw]) * fOneMinus;
         else  // sharpen
            f = (fp)Gamma(pb->pImage->m_pbTrans[dw])+ ((fp)Gamma(pb->pImage->m_pbTrans[dw]) - pb->fValue) * fStrength;

         f = max(0, f);
         f = min((fp)0xffff, f);
         pb->pImage->m_pbTrans[dw] = UnGamma((WORD)f);
      } // i
   } // trans




   if (fPaintBump) {
      for (i = 0, pb = pbOrig; i < dwNum; i++, pb++) {
         if (!pb->pImage->m_pbBump)
            continue;   // no Bump to blur
         pb->fValue = 0;
         for (y = (int)pb->wY - BLURAMT; y <= (int)pb->wY+BLURAMT; y++)
            for (x = (int)pb->wX - BLURAMT; x <= (int)pb->wX+BLURAMT; x++)
               pb->fValue += pb->pImage->m_pbBump[(
                     (x + (int) pb->pImage->m_dwWidth) % (int) pb->pImage->m_dwWidth +
                     ((y + (int) pb->pImage->m_dwHeight) % (int) pb->pImage->m_dwHeight) * (int) pb->pImage->m_dwWidth
                  )];
         pb->fValue /= ((2*BLURAMT+1) * (2*BLURAMT+1));
      }

      // go back over and average
      for (i = 0, pb = pbOrig; i < dwNum; i++, pb++) {
         if (!pb->pImage->m_pbBump)
            continue;   // no Bump to blue

         dw = (DWORD) pb->wX + (DWORD)pb->wY * pb->pImage->m_dwWidth;

         fStrength = fStrengthOrig * (fp) pb->bStrength / 255.0;
         fOneMinus = 1.0 - fStrength;

         if (m_dwBrushEffect == 1)  // blur
            f = pb->fValue * fStrength + (fp)pb->pImage->m_pbBump[dw] * fOneMinus;
         else  // sharpen
            f = (fp)pb->pImage->m_pbBump[dw]+ ((fp)pb->pImage->m_pbBump[dw] - pb->fValue) * fStrength;

         f = max(0, f);
         f = min((fp)0xff, f);
         pb->pImage->m_pbBump[dw] = (BYTE) f;
      } // i
   } // bump


}

/***********************************************************************************
CPaintView::PaintRegion - This takes a pointer to memory that indicates a portion of
the screen to be painted over (and with what intensity). It then paints the current
template texture onto what's there.

inputs
   RECT        *pr - Coordinates are in the m_hWndMap coordinates
   PBYTE       pbPaint - What to paint. An array of bytes of (pr->right-pr->left) x
               (pr->bottom - pr->top) pixels. 0 in a byte indicates no paint,
               255 is maximum
returns
   none
*/
void CPaintView::PaintRegion (RECT *pr, PBYTE pbPaint)
{
   // get the image that painting
   PPVTEMPLATE pTemp = (PPVTEMPLATE) m_lPVTEMPLATE.Get(m_dwCurTemplate);
   if (!pTemp)
      return; // no template
   RECT rFill;
   rFill = *pr;

   // get the list of texture images
   PCTextureImage *ppti;
   ppti = (PCTextureImage*) m_lPCTextureImage.Get(0);

   m_lBLURPIXEL.Clear();

   // loop
   int x,y;
   DWORD i;
   PRPPIXEL prp;
   PRPTEXTINFO pInfo;
   PCTextureImage pLast;
   PBYTE pbStrength;
   pInfo = (PRPTEXTINFO) m_lRPTEXTINFO.Get(0);
   pLast = NULL;
   CListFixed lPassedToUndo;
   lPassedToUndo.Init (sizeof(PCTextureImage));
   for (y = max(0,rFill.top); y+1 < min((int)m_Image.Height(), rFill.bottom); y++) {
      prp = m_pPixel + (y * m_Image.Width() + (DWORD)max(rFill.left,0));
      pbStrength = pbPaint + (((int) y - rFill.top) * (rFill.right - rFill.left) +
         max(rFill.left,0) - rFill.left);

      for (x = max(rFill.left, 0); x+1 < min(rFill.right, (int)m_Image.Width()); x++, prp++, pbStrength++) {
         if (!prp->dwTextIndex)
            continue;   // no image to paint over

         // make sure texture image to the right and bottom are also the same
         DWORD dwIR, dwIB, dwIBR;
         dwIR = 1;
         dwIB = m_Image.Width();
         dwIBR = 1 + dwIB;
         if ((prp[dwIR].dwTextIndex != prp->dwTextIndex) ||
            (prp[dwIB].dwTextIndex != prp->dwTextIndex) ||
            (prp[dwIBR].dwTextIndex != prp->dwTextIndex))
            continue;

         // if all the intensities are 0 then dont bother
         DWORD dwSR, dwSB, dwSBR;
         dwSR =1;
         dwSB = (DWORD)(rFill.right - rFill.left);
         dwSBR = 1 + dwSB;
         if (!(pbStrength[0] || pbStrength[dwSR] || pbStrength[dwSB] || pbStrength[dwSBR]))
            continue;

         // get the texture image
         PRPTEXTINFO pCur = pInfo + (prp->dwTextIndex - 1);
         if (!pLast || !IsEqualGUID(pLast->m_gSub, pCur->gSub) || !IsEqualGUID(pLast->m_gCode, pCur->gCode)) {
            // look in list for new one
            for (i = 0; i < m_lPCTextureImage.Num(); i++) {
               pLast = ppti[i];
               if (IsEqualGUID(pLast->m_gSub, pCur->gSub) && IsEqualGUID(pLast->m_gCode, pCur->gCode))
                  break;
            }
            if (i >= m_lPCTextureImage.Num())
               continue;   // shouldnt happen


            // indicate the going to change
            if (pLast) {
               // fast check to make sure dont keep on calling undo function over and
               // over. Only call it once
               PCTextureImage *pp = (PCTextureImage*) lPassedToUndo.Get(0);
               DWORD j;
               for (j = 0; j < lPassedToUndo.Num(); j++)
                  if (pp[j] == pLast)
                     break;
               if (j >= lPassedToUndo.Num()) {
                  lPassedToUndo.Add (&pTemp);
                  UndoAboutToChange (pLast);
               } // need to add
            } // if pLast
         }

         if (m_pPaintOnly && (m_pPaintOnly != pLast))
            continue; // dont paint this because not selected

         // figure out 4 corner points where painting from
         CPoint apSrc[4];  // 0=UL, 1=UR, 2=LL, 3=LR
         CPoint p;
         apSrc[0].Zero();
         apSrc[0].p[0] = x;
         apSrc[0].p[1] = y;
         apSrc[1].Zero();
         apSrc[1].p[0] = x+1;
         apSrc[1].p[1] = y;
         apSrc[2].Zero();
         apSrc[2].p[0] = x;
         apSrc[2].p[1] = y+1;
         apSrc[3].Zero();
         apSrc[3].p[0] = x+1;
         apSrc[3].p[1] = y+1;
         for (i = 0; i < 4; i++) {
            p.Copy (&apSrc[i]);
            p.p[3] = 1;
            pTemp->mMatrix.Multiply2D (&p, &apSrc[i]);
         }

         // make sure there's no wrap around in the textures
         TEXTUREPOINT atpDest[4];
         BOOL fTooLarge;
         fTooLarge = FALSE;
         for (i = 0; i < 4; i++) {
            DWORD dw;
            switch (i) {
            case 0:  // UL
               dw = 0;
               break;
            case 1:  // uR
               dw = dwIR;
               break;
            case 2:  // LL
               dw = dwIB;
               break;
            case 3:  // LR
               dw = dwIBR;
               break;
            }

            atpDest[i].h = prp[dw].afHV[0];
            atpDest[i].v = prp[dw].afHV[1];

            // if more than .5 distance then adjust
            if (!i)
               continue;
            while (atpDest[i].h + .5 < atpDest[0].h)
               atpDest[i].h += 1.0;
            while (atpDest[i].h - .5 > atpDest[0].h)
               atpDest[i].h -= 1.0;
            while (atpDest[i].v + .5 < atpDest[0].v)
               atpDest[i].v += 1.0;
            while (atpDest[i].v - .5 > atpDest[0].v)
               atpDest[i].v -= 1.0;

            // if more than .1 distance than too large to paint, so fail
            if ((fabs(atpDest[i].h - atpDest[0].h) > .1) || (fabs(atpDest[i].v - atpDest[0].v) > .1))
               fTooLarge = TRUE;
         } // i

         if (fTooLarge)
            continue; // too large to paint

         for (i = 0; i < 4; i++) {
            atpDest[i].h *= (fp)pLast->m_dwWidth;
            atpDest[i].v *= (fp)pLast->m_dwHeight;
         } // i


         // paint the two triangles
         TEXTUREPOINT atpTriDest[3];
         TEXTUREPOINT atpTriSrc[3];
         BYTE abTriStrength[3];

         // first one
         atpTriDest[0] = atpDest[0];
         atpTriDest[1] = atpDest[1];
         atpTriDest[2] = atpDest[2];
         atpTriSrc[0].h = apSrc[0].p[0];
         atpTriSrc[0].v = apSrc[0].p[1];
         atpTriSrc[1].h = apSrc[1].p[0];
         atpTriSrc[1].v = apSrc[1].p[1];
         atpTriSrc[2].h = apSrc[2].p[0];
         atpTriSrc[2].v = apSrc[2].p[1];
         abTriStrength[0] = pbStrength[0];
         abTriStrength[1] = pbStrength[dwSR];
         abTriStrength[2] = pbStrength[dwSB];
         if (abTriStrength[0] | abTriStrength[1] | abTriStrength[2])
            PaintTriRegion (atpTriDest, pLast, atpTriSrc, pTemp->pPaint, abTriStrength,
               pTemp->fNoColor, pTemp->fPaintIgnoreTrans, pTemp->fBumpAdds);

         // second one - since clockwise doesnt matter, it's easy
         atpTriDest[0] = atpDest[3];
         atpTriSrc[0].h = apSrc[3].p[0];
         atpTriSrc[0].v = apSrc[3].p[1];
         abTriStrength[0] = pbStrength[dwSBR];
         if (abTriStrength[0] | abTriStrength[1] | abTriStrength[2])
            PaintTriRegion (atpTriDest, pLast, atpTriSrc, pTemp->pPaint, abTriStrength,
               pTemp->fNoColor, pTemp->fPaintIgnoreTrans, pTemp->fBumpAdds);
      } // x
   } // y

   // apply blurring
   PaintBlur ();

   // recalc an extra pixel or two around
#define EXTRAFILL          4
   rFill.left = max(pr->left - EXTRAFILL, 0);
   rFill.right = min(pr->right + EXTRAFILL, (int)m_Image.Width());
   rFill.top = max(pr->top - EXTRAFILL, 0);
   rFill.bottom = min(pr->bottom + EXTRAFILL, (int)m_Image.Height());
   FillImage (&rFill);
   InvalidateRect (m_hWndMap, &rFill, FALSE);
}

/*****************************************************************************
CPaintView::BrushApply - Applies a brush based on its size, etc.

inputs
   DWORD       dwPointer - Pointer being used. This is one of IDC_BRUSHxxx.
   POINT       *pNew - New location, in client coords of m_hWndMap
   POINT       *pPrev - Previous location, in client coords of m_hWndMap.
                  This is only used for pens (m_dwBrushAir == 0). If it's the
                  first time a pen is down then this should be NULL.
returns
   none
*/
void CPaintView::BrushApply (DWORD dwPointer, POINT *pNew, POINT *pPrev)
{
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
   dwBrushSize = max(1,dwBrushSize);

   // fill memory with what changed
   RECT rChange;
   PBYTE pbBuf;
   pbBuf = BrushFillAffected (dwBrushSize, m_dwBrushPoint, m_dwBrushAir,
      pPrev, pNew, &m_memPaintBuf, &rChange);
   if (!pbBuf)
      return;

   // paint
   PaintRegion (&rChange, pbBuf);

}


/*****************************************************************************
CPaintView::ChangeResoltuion - Up/down sample an image.

inputs
   PCTextureImage       pti - Image to up/down sample
   DWORD                dwNewWidth - new width in pixels
   DWORD                dwNewHeight - new height in pixels
returns
   BOOL - TRUE if success
*/
BOOL CPaintView::ChangeResolution (PCTextureImage pti, DWORD dwNewWidth, DWORD dwNewHeight)
{
   // update pixel len
   pti->m_fPixelLen = (pti->m_fPixelLen * (fp)pti->m_dwWidth) / (fp) dwNewWidth;


   // clone the image for noew
   PCTextureImage pClone;
   pClone = pti->Clone();
   if (!pClone)
      return FALSE;

   // rellocate sizes
   DWORD dw;
   pti->m_dwWidth = dwNewWidth;
   pti->m_dwHeight = dwNewHeight;
   dw = pti->m_dwWidth * pti->m_dwHeight;

   if (pti->m_pbRGB) {
      if (!pti->m_memRGB.Required (dw * 3)) {
         delete pClone;
         return FALSE;
      }
      pti->m_pbRGB = (PBYTE) pti->m_memRGB.p;
   }

   if (pti->m_pbGlow) {
      if (!pti->m_memGlow.Required (dw * 3)) {
         delete pClone;
         return FALSE;
      }
      pti->m_pbGlow = (PBYTE) pti->m_memGlow.p;
   }

   if (pti->m_pbSpec) {
      if (!pti->m_memSpec.Required (dw * 2)) {
         delete pClone;
         return FALSE;
      }
      pti->m_pbSpec = (PBYTE) pti->m_memSpec.p;
   }

   if (pti->m_pbBump) {
      if (!pti->m_memBump.Required (dw)) {
         delete pClone;
         return FALSE;
      }
      pti->m_pbBump = (PBYTE) pti->m_memBump.p;
   }

   if (pti->m_pbTrans) {
      if (!pti->m_memTrans.Required (dw)) {
         delete pClone;
         return FALSE;
      }
      pti->m_pbTrans = (PBYTE) pti->m_memTrans.p;
   }

   if (!pti->m_fGlowScale)
      pti->m_fGlowScale = 1;
   if (pti->m_fBumpHeight < CLOSE)
      pti->m_fBumpHeight = CLOSE;

   // up/down sample
   // NOTE: Doing cheezy linear interpolation. This isn't the best but it will work for
   // now. I may want to improve this to invlude antialiasing later
   DWORD x,y, j;
   TEXTUREPOINT tp;
   WORD awRGB[3], wTrans, wSpecReflect, wSpecPower;
   float afGlow[3];
   fp fBump;
   dw = 0;
   for (y = 0; y < pti->m_dwHeight; y++) {
      tp.v = (fp) y / (fp)pti->m_dwHeight;

      for (x = 0; x < pti->m_dwWidth; x++, dw++) {
         tp.h = (fp) x / (fp) pti->m_dwWidth;
         TextureImageInterp (pClone, &tp,
            pti->m_pbRGB ? awRGB : NULL,
            pti->m_pbGlow ? afGlow : NULL,
            pti->m_pbTrans ? &wTrans : NULL,
            pti->m_pbBump ? &fBump : NULL,
            pti->m_pbSpec ? &wSpecReflect : NULL,
            pti->m_pbSpec ? &wSpecPower : NULL);

         // write values
         if (pti->m_pbRGB)
            for (j = 0; j < 3; j++)
               pti->m_pbRGB[dw*3+j] = UnGamma(awRGB[j]);

         if (pti->m_pbGlow)
            for (j = 0; j < 3; j++) {
               afGlow[j] /= pti->m_fGlowScale;
               afGlow[j] = max(0,afGlow[j]);
               afGlow[j] = min((float)0xffff,afGlow[j]);
               pti->m_pbGlow[dw*3+j] = UnGamma((WORD)afGlow[j]);
            }

         if (pti->m_pbSpec) {
            pti->m_pbSpec[dw*2+0] = HIBYTE(wSpecPower);
            pti->m_pbSpec[dw*2+1] = UnGamma(wSpecReflect);
         }

         if (pti->m_pbTrans)
            pti->m_pbTrans[dw] = UnGamma(wTrans);

         if (pti->m_pbBump) {
            fBump = fBump / pti->m_fBumpHeight * 256.0 + (fp)0x80;
            fBump = max(0,fBump);
            fBump = min(255, fBump);
            pti->m_pbBump[dw] = (BYTE) fBump;
         }

      } // x
   } // y

   delete pClone;
   return TRUE;
}



// FUTURE RELEASE - See what can do about paining over textyre wrap boundary... maybe some intelligence
// to see what deltas are for neighboring textures? This is only a problem if the wrap
// ends up being mis-aligned on 0..1 boundary, so works fine with polymesh where wraps
// in even integers, but not when uneven wrap.


// FUTURE RELEASE - Moire pattern showing up on patterns under win98. Since have put in
// a warning telling users not to run on win98, shouldnt be much of aproblem

// BUGBUG - Need way to flatten out surface that painting and just paint on flat surface
