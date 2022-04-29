/***************************************************************************
CWaveView - C++ object for modifying the wave data

begun 1/5/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <mmreg.h>
#include <msacm.h>
#include <zmouse.h>
#include <multimon.h>
#include <math.h>
#include <ks.h>
#include <ksmedia.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"


/*****************************************************************************
typedefs */

#define  TBORDER           16       // extra border around thumbnail
#define  THUMBSHRINK       (TEXTURETHUMBNAIL/2)    // size to shrink thumbnail to

#define  TIMER_AIRBRUSH    121      // timer ID to cause frequent airbrush
#define  TIMER_PLAYBACK    122      // playback timer
#define  TIMER_SCROLLDRAG  123      // scroll while draging

#define  AIRBRUSHPERSEC    10       // number of times to call airbrush per second

#define  WAITTOTIP         90       // timer ID for waiting to tip

#define  SCROLLSIZE     (16)
#define  SMALLSCROLLSIZE (SCROLLSIZE / 2)
#define  VARBUTTONSIZE  (m_fSmallWindow ? M3DSMALLBUTTONSIZE : M3DBUTTONSIZE)
#define  VARSCROLLSIZE (m_fSmallWindow ? SMALLSCROLLSIZE : SCROLLSIZE)
#define  VARSCREENSIZE (VARBUTTONSIZE * 6 / 4)  // BUGFIX - Was 7/4

#define  SCROLLSCALE       100000

#define  WM_MYWOMMESSAGE      (WM_USER+98)

//#ifdef _DEBUG
//#define ID_VOICEMODIFICATION_VOICECHAT 2054
//#endif


#define  IDC_NEW           1000
#define  IDC_OPEN          1001
#define  IDC_CLOSE         1002
#define  IDC_SAVE          1005
#define  IDC_SAVEAS        1006
#define  IDC_SETTINGS      1007
#define  IDC_HELPBUTTON    1010

#define  IDC_DELETEBUTTON  2000
#define  IDC_PASTEBUTTON   2001
#define  IDC_COPYBUTTON    2002
#define  IDC_CUTBUTTON     2003
#define  IDC_UNDOBUTTON    2004
#define  IDC_REDOBUTTON    2005
#define  IDC_PASTEMIX      2006


#define  IDC_ANIMPLAY      3000
#define  IDC_ANIMSTOP      3001
#define  IDC_ANIMPLAYLOOP  3002
#define  IDC_ANIMJUMPSTART 3003
#define  IDC_ANIMJUMPEND   3004
#define  IDC_ANIMRECORD    3005
#define  IDC_ZOOMIN        3006
#define  IDC_ZOOMALL       3007
#define  IDC_ANIMSTOPIFNOBUF 3008   // not really a button, but use like one
#define  IDC_ANIMPLAYSEL   3009
#define  IDC_ZOOMSEL       3010
#define  IDC_SHOWWAVE      3100
#define  IDC_SHOWFFT       3101
#define  IDC_SHOWOTHERS    3102
#define  IDC_SHOWPHONEMES  3103
#define  IDC_SHOWMOUTH     3104

#define  IDC_NEWPHONE      4000
#define  IDC_SPEECHRECOG   4001
#define  IDC_NEWWORD       4002
#define  IDC_LEXICON       4003

#define  IDC_BRUSHSHAPE    5000
#define  IDC_BRUSHEFFECT   5001
#define  IDC_BRUSHSTRENGTH 5002
#define  IDC_SELREGION     5005
#define  IDC_SELALL        5006
#define  IDC_SELNONE       5007
#define  IDC_SELPLAYBACK   5008
#define  IDC_FX            5010

#define  IDC_BRUSH4        6000
#define  IDC_BRUSH8        6001
#define  IDC_BRUSH16       6002
#define  IDC_BRUSH32       6003
#define  IDC_BRUSH64       6004


#define  IDC_MYSCROLLBAR   8000


// WVSTATE - used for undo
typedef struct {
   PCM3DWave         pWave;         // wave stored away
   int               iPlayLimitMin;     // minimum sample where playback starts
   int               iPlayLimitMax;     // maximum sample where playback starts
   int               iPlayCur;          // current playback sample (to be added at next available buffer)
   int               iViewSampleLeft;   // sample viewed on left side
   int               iViewSampleRight;  // sample viewend on right side
   int               iSelStart;         // selection start
   int               iSelEnd;           // selection end
} WVSTATE, *PWVSTATE;


// PLSORT - Used to sort list of phonemes
typedef struct {
   PLEXPHONE         plPhone;          // phoneme
   DWORD             dwColumn;         // which column it should appear in
} PLSORT, *PPLSORT;

/*****************************************************************************
globals */

char gszKeyTrainFile[] = "TrainFile";
char gszKeyLexFile[] = "LexFile";
char gszKeyTTSFile[] = "TTSFile";


/***********************************************************************
PLSORTSort */
static int _cdecl PLSORTSort (const void *elem1, const void *elem2)
{
   PLSORT *pdw1, *pdw2;
   pdw1 = (PLSORT*) elem1;
   pdw2 = (PLSORT*) elem2;

   if (pdw1->dwColumn != pdw2->dwColumn)
      return (int)pdw1->dwColumn - (int)pdw2->dwColumn;

   return _wcsicmp(pdw1->plPhone->szPhoneLong, pdw2->plPhone->szPhoneLong);
}

/****************************************************************************
FXEchoPageGenBMP - Generate the bitmap for the echo
*/
static void FXEchoPageGenBMP (HWND hWnd, PFXECHO pv)
{
   if (pv->hBit)
      DeleteObject (pv->hBit);
   pv->hBit = NULL;

   // refill the wave
   pv->pWave->FXGenerateEcho (&pv->Echo);

   RECT r;
   r.top = r.left = 0;
   r.right = 300;
   r.bottom = 100;

   // allocate the bitmap and DC
   HDC hDC, hDCDraw;
   HBITMAP hBit;
   hDC = GetDC (hWnd);
   hDCDraw = CreateCompatibleDC (hDC);
   pv->hBit = hBit = CreateCompatibleBitmap (hDC, r.right, r.bottom);
   ReleaseDC(hWnd, hDC);
   SelectObject (hDCDraw, hBit);

   // draw
   //pv->pWave->PaintWave (hDCDraw, &r, 0, pv->pWave->m_dwSamples, 32767, 0, -1, TRUE);
   pv->pWave->PaintEnergy (hDCDraw, &r, 0, pv->pWave->m_dwSamples, 0, 1, -1);

   DeleteDC (hDCDraw);
}

/****************************************************************************
FXEchoPage
*/
BOOL FXEchoPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXECHO pv = (PFXECHO) pPage->m_pUserData;
   static WCHAR sTextureTemp[16];

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"surfaces");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->Echo.dwSurfaces);
         pControl = pPage->ControlFind (L"delay");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->Echo.fDelay * 100.0));
         pControl = pPage->ControlFind (L"delayvar");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->Echo.fDelayVar * 100.0));
         pControl = pPage->ControlFind (L"decay");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->Echo.fDecay * 100.0));
         //pControl = pPage->ControlFind (L"sharpness");
         //if (pControl)
         //   pControl->AttribSetInt (Pos(), (int)(pv->Echo.fSharpness * 100.0));

         DoubleToControl (pPage, L"time", pv->Echo.fTime);
         DoubleToControl (pPage, L"seed", pv->Echo.dwSeed);
      }
      break;

   case ESCM_USER+82: // recalc bitmap
      {
         WCHAR szTemp[32];
         PCEscControl pControl;

         FXEchoPageGenBMP (pPage->m_pWindow->m_hWnd, pv);

         pControl = pPage->ControlFind (L"image");
         swprintf (szTemp, L"%lx", (__int64) pv->hBit);
         if (pControl)
            pControl->AttribSet (L"hbitmap", szTemp);
      }
      return 0;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         
         // just get all edit
         pv->Echo.dwSeed = (DWORD)DoubleFromControl (pPage, L"seed");
         pv->Echo.fTime = DoubleFromControl (pPage, L"time");
         pv->Echo.fTime = max(pv->Echo.fTime, .01);
         pv->Echo.fTime = min(pv->Echo.fTime, 10);

         // recalc bitmap
         pPage->Message (ESCM_USER+82);
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
         DWORD dwVal;
         fp fVal;
         psz = p->pControl->m_pszName;
         dwVal = (int)p->pControl->AttribGetInt (Pos());
         fVal = (fp)dwVal / 100.0;

         if (!_wcsicmp(psz, L"surfaces"))
            pv->Echo.dwSurfaces = dwVal;
         else if (!_wcsicmp(psz, L"delay"))
            pv->Echo.fDelay = fVal;
         else if (!_wcsicmp(psz, L"delayvar"))
            pv->Echo.fDelayVar = fVal;
         else if (!_wcsicmp(psz, L"decay"))
            pv->Echo.fDecay = fVal;
         //else if (!_wcsicmp(psz, L"sharpness"))
         //   pv->Echo.fSharpness = fVal;
         else
            break;   // unknown

         // recalc bitmap
         pPage->Message (ESCM_USER+82);
         return TRUE;
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Echo and reverb effect";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            swprintf (sTextureTemp, L"%lx", (__int64) pv->hBit);
            p->pszSubString = sTextureTemp;
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
FXAcCompressPage
*/
BOOL FXAcCompressPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXACCOMPRESS pv = (PFXACCOMPRESS) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"loop");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->fCompress * 100.0));
      }
      break;


   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"loop"))
            break;

         // set value
         pv->fCompress = p->pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Acoustic compression effect";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
FXBlendPage
*/
BOOL FXBlendPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXBLEND pv = (PFXBLEND) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"loop");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->fLoop * 100.0));
      }
      break;


   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"loop"))
            break;

         // set value
         pv->fLoop = p->pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Blend for loop effect";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
FXFilterPage
*/
BOOL FXFilterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXFILTER pv = (PFXFILTER) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;


         pControl = pPage->ControlFind (L"noend");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->fIgnoreEnd);

         DWORD i,j;
         WCHAR szTemp[16];
         for (i = 0; i < 2; i++) for (j = 0; j < 10; j++) {
            swprintf (szTemp, L"%c%d", i ? L'e' : L's', (int)j);
            pControl = pPage->ControlFind (szTemp);
            if (!pControl)
               continue;
            pControl->AttribSetInt (Pos(), (int)(pv->afFilter[i][j] * 100.0));
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
         if (!_wcsicmp(psz, L"noend")) {
            pv->fIgnoreEnd = p->pControl->AttribGetBOOL (Checked());
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

         DWORD i, j;
         i = (p->pControl->m_pszName[0] == L'e');
         j = _wtoi(p->pControl->m_pszName + 1);

         // set value
         pv->afFilter[i][j] = p->pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Filter effect";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
FXVolumePage
*/
BOOL FXVolumePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXVOLUME pv = (PFXVOLUME) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DoubleToControl (pPage, L"start", pv->fStart);
         DoubleToControl (pPage, L"end", pv->fEnd);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         
         // just get all edit
         pv->fStart = DoubleFromControl (pPage, L"start");
         pv->fEnd = DoubleFromControl (pPage, L"end");
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Volume effect";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
FXSinePage
*/
BOOL FXSinePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXSINE pv = (PFXSINE) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DoubleToControl (pPage, L"start", pv->fStart);
         DoubleToControl (pPage, L"end", pv->fEnd);
         DoubleToControl (pPage, L"channel", pv->dwChannel);
         ComboBoxSet (pPage, L"shape", pv->dwShape);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         
         // just get all edit
         pv->fStart = DoubleFromControl (pPage, L"start");
         pv->fStart = max(pv->fStart, .01);
         pv->fEnd = DoubleFromControl (pPage, L"end");
         pv->fEnd = max(pv->fEnd, .01);

         pv->dwChannel = (DWORD)DoubleFromControl (pPage, L"channel");
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         DWORD dwVal;
         psz = p->pControl->m_pszName;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"shape")) {
            if (dwVal == pv->dwShape)
               return TRUE;   // no change
            pv->dwShape = dwVal;
            return TRUE;
         }
      }
      return 0;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Generate sine wave effect";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
FXMonotonePage
*/
BOOL FXMonotonePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXMONOTONE pv = (PFXMONOTONE) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DoubleToControl (pPage, L"pitch", pv->fPitch);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         
         // just get all edit
         pv->fPitch = DoubleFromControl (pPage, L"pitch");
         pv->fPitch = max(pv->fPitch, .01);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Make the voice monotone";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
FXPitchSpeedPage
*/
BOOL FXPitchSpeedPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXPITCHSPEED pv = (PFXPITCHSPEED) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DoubleToControl (pPage, L"pitch", pv->fPitch);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         
         // just get all edit
         pv->fPitch = DoubleFromControl (pPage, L"pitch");
         pv->fPitch = max(pv->fPitch, .1);
         pv->fPitch = min(pv->fPitch, 10);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Pitch and speed effect";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
FXPSOLAPage
*/
BOOL FXPSOLAPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFXPSOLA pv = (PFXPSOLA) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DoubleToControl (pPage, L"pitch", pv->fPitch);
         DoubleToControl (pPage, L"duration", pv->fDuration);
         DoubleToControl (pPage, L"formants", pv->fFormants);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         
         // just get all edit
         pv->fPitch = DoubleFromControl (pPage, L"pitch");
         pv->fPitch = max(pv->fPitch, .1);
         pv->fPitch = min(pv->fPitch, 10);

         pv->fDuration = DoubleFromControl (pPage, L"duration");
         pv->fDuration = max(pv->fDuration, .1);
         pv->fDuration = min(pv->fDuration, 10);

         pv->fFormants = DoubleFromControl (pPage, L"formants");
         pv->fFormants = max(pv->fFormants, -10);
         pv->fFormants = min(pv->fFormants, 10);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"PSOLA pitch, speed, and formant change";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
CWaveViewWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CWaveView, and calls that.
*/
LRESULT CALLBACK CWaveViewWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCWaveView p = (PCWaveView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCWaveView p = (PCWaveView) (size_t) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCWaveView) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/****************************************************************************
CWaveViewMouthWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CWaveView, and calls that.
*/
LRESULT CALLBACK CWaveViewMouthWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCWaveView p = (PCWaveView) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCWaveView p = (PCWaveView) (size_t) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCWaveView) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->MouthWndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/****************************************************************************
CWaveViewDataWndProc - Window procedure called from windows. This just gets
the lparam of the window, types it to a CWaveView, and calls that.
*/
LRESULT CALLBACK CWaveViewDataWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PWAVEPANE pwp = (PWAVEPANE) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PWAVEPANE pwp = (PWAVEPANE) (size_t) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif
   PCWaveView p = pwp ? pwp->pWaveView : NULL;

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         pwp = (PWAVEPANE)lpcs->lpCreateParams;
         p = pwp ? pwp->pWaveView : NULL;
      }
      break;
   };

   if (p)
      return p->DataWndProc (pwp, hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/****************************************************************************
Constructor and destructor

inputs
*/
CWaveView::CWaveView (void)
{
   InitializeCriticalSection (&m_csWaveOut);

   // very dark
   m_cBackDark = RGB(0x0, 0x0, 0x40);
   m_cBackMed = RGB(0x20,0x20,0x60);
   m_cBackLight = RGB(0x60,0x60,0xc0);
   m_cBackOutline = RGB(0x80, 0x80, 0x80);
   m_hWndMouth = NULL;

   m_dwBrushAir = 2; // default to airbrush
   m_dwBrushPoint = 2;  // start out as slightly pointy
   m_dwBrushStrength = 3;
   m_dwBrushEffect = 0;
   m_fAirbrushTimer = FALSE;
   m_pTimeline = NULL;

   m_iTTSQuality = 3;   // maximum quality
   m_fDisablePCM = FALSE;

   m_hWnd = m_hWndScroll = NULL;
   m_pbarGeneral = m_pbarMisc = m_pbarView = m_pbarObject = NULL;
   m_fUndoDirty = FALSE;
   m_lWVSTATEUndo.Init (sizeof(WVSTATE));
   m_lWVSTATERedo.Init (sizeof(WVSTATE));

   m_fSmallWindow = FALSE;
   m_listNeedSelect.Init (sizeof(PCIconButton));
   
   memset (m_adwPointerMode, 0 ,sizeof(m_adwPointerMode));
   m_dwPointerModeLast = 0;
   m_dwButtonDown = 0;
   m_pntButtonDown.x = m_pntButtonDown.y = 0;
   m_fCaptured = FALSE;
   m_pUndo = m_pRedo = NULL;
   m_pPaste = m_pPasteMix= NULL;
   m_fWorkingWithUndo = FALSE;
   m_pStop = m_pPlay = m_pPlayLoop = m_pPlaySel = m_pRecord = NULL;
   m_pWave = NULL;
   m_iViewSampleRight = m_iViewSampleLeft = 0;
   m_iSelEnd = m_iSelStart = 0;

   memset (m_aPlayWaveHdr, 0, sizeof(m_aPlayWaveHdr));
   m_hPlayWaveOut = NULL;
   m_lPlaySync.Init (sizeof(DWORD)*2);
   m_iPlayLimitMin = m_iPlayLimitMax = m_iPlayCur = 0;
   m_dwPlayTimer = 0;
   m_hWndClipNext = NULL;

   memset (&m_FXSine, 0, sizeof(m_FXSine));
   m_FXSine.fStart = m_FXSine.fEnd = 1000;
   m_FXSine.dwShape = 0;

   memset (&m_FXMonotone, 0, sizeof(m_FXMonotone));
   m_FXMonotone.fPitch = 22050.0 / 256.0;

   memset (&m_FXPSOLA, 0, sizeof(m_FXPSOLA));
   m_FXPSOLA.fDuration = 1;
   m_FXPSOLA.fPitch = 1;
   m_FXPSOLA.fFormants = 0;

   memset (&m_FXPitchSpeed, 0, sizeof(m_FXPitchSpeed));
   m_FXPitchSpeed.fPitch = 2;
   memset (&m_FXVolume, 0, sizeof(m_FXVolume));
   m_FXVolume.fStart = m_FXVolume.fEnd = 2.0;
   memset (&m_FXFilter, 0, sizeof(m_FXFilter));
   DWORD i;
   for (i = 0; i < 10; i++)
      m_FXFilter.afFilter[0][i] = m_FXFilter.afFilter[1][i] = 1;
   m_FXFilter.fIgnoreEnd = TRUE;
   memset (&m_FXBlend, 0, sizeof(m_FXBlend));
   m_FXBlend.fLoop = .1;
   memset (&m_FXAcCompress, 0, sizeof(m_FXAcCompress));
   m_FXAcCompress.fCompress = .25;
   memset (&m_FXEcho, 0, sizeof(m_FXEcho));
   m_FXEcho.Echo.dwSeed = 0;
   m_FXEcho.Echo.dwSurfaces = 2;
   m_FXEcho.Echo.fDecay = .5;
   m_FXEcho.Echo.fDelay = .3;
   m_FXEcho.Echo.fDelayVar = 0;
   //m_FXEcho.Echo.fSharpness = .01;
   m_FXEcho.Echo.fTime = 2.0;

   // get the default training file
   //m_szTrainFile[0] = 0;
   HKEY  hKey = NULL;
   DWORD dwDisp;
   //RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
   //   KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   //if (hKey) {
   //   DWORD dw, dwType;
   //   LONG lRet;
   //   char sza[256];
   //   sza[0] = 0;
   //   dw = sizeof(sza);
   //   lRet = RegQueryValueEx (hKey, gszKeyTrainFile, NULL, &dwType, (LPBYTE) sza, &dw);
   //   RegCloseKey (hKey);

   //   if (lRet != ERROR_SUCCESS)
   //      sza[0] = 0;

   //   MultiByteToWideChar (CP_ACP, 0, sza, -1, m_szTrainFile, sizeof(m_szTrainFile)/sizeof(WCHAR));
   //}
   //if (!m_szTrainFile[0]) {
   //   MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, m_szTrainFile, sizeof(m_szTrainFile)/sizeof(WCHAR));
   //   wcscat (m_szTrainFile, L"EnglishMale.mtf");
   //}

   // get the default lexicon file
   m_szLexicon[0] = 0;
   m_pLex = NULL;
   char szTemp[256];
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      DWORD dw, dwType;
      LONG lRet;
      dw = sizeof(szTemp);
      lRet = RegQueryValueEx (hKey, gszKeyLexFile, NULL, &dwType, (LPBYTE) szTemp, &dw);
      RegCloseKey (hKey);

      if (lRet != ERROR_SUCCESS)
         szTemp[0] = 0;
   }
   if (!szTemp[0]) {
      strcpy (szTemp, gszAppDir);
      strcat (szTemp, "EnglishInstalled.mlx");
   }
   MultiByteToWideChar (CP_ACP, 0, szTemp, -1, m_szLexicon, sizeof(m_szLexicon)/sizeof(WCHAR));

   // get the default TTS file
   //m_szTTS[0] = 0;
   //RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
   //   KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   //if (hKey) {
   //   DWORD dw, dwType;
   //   LONG lRet;
   //   dw = sizeof(szTemp);
   //   lRet = RegQueryValueEx (hKey, gszKeyTTSFile, NULL, &dwType, (LPBYTE) szTemp, &dw);
   //   RegCloseKey (hKey);

   //   if (lRet != ERROR_SUCCESS)
   //      szTemp[0] = 0;
   //}
   //MultiByteToWideChar (CP_ACP, 0, szTemp, -1, m_szTTS, sizeof(m_szTTS)/sizeof(WCHAR));
}


CWaveView::~CWaveView (void)
{
   if (m_hWndMouth)
      DestroyWindow (m_hWndMouth);

   // save out the training data location
   //if (m_szTrainFile[0]) {
      // save to registry
   //   HKEY  hKey = NULL;
   //   DWORD dwDisp;
   //   char sza[256];
   //   WideCharToMultiByte (CP_ACP, 0, m_szTrainFile, -1, sza, sizeof(sza), 0, 0);
   //   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
   //      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);

   //   if (hKey) {
   //      RegSetValueEx (hKey, gszKeyTrainFile, 0, REG_SZ, (BYTE*) sza,
   //         (DWORD)strlen(sza)+1);

   //      RegCloseKey (hKey);
   //   }
   //}

   // free lexicon
   if (m_szLexicon[0]) {
      // save to registry
      HKEY  hKey = NULL;
      DWORD dwDisp;
      RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
         KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);

      if (hKey) {
         char szTemp[512];
         WideCharToMultiByte (CP_ACP, 0, m_szLexicon, -1, szTemp, sizeof(szTemp), 0, 0);
         RegSetValueEx (hKey, gszKeyLexFile, 0, REG_SZ, (BYTE*) szTemp,
            (DWORD)strlen(szTemp)+1);

         RegCloseKey (hKey);
      }
   }
   if (m_pLex)
      MLexiconCacheClose (m_pLex);
   m_pLex = NULL;

   //if (m_szTTS[0]) {
      // save to registry
   //   HKEY  hKey = NULL;
   //   DWORD dwDisp;
   //   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
   //      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);

   //   if (hKey) {
   //      char szTemp[512];
   //      WideCharToMultiByte (CP_ACP, 0, m_szTTS, -1, szTemp, sizeof(szTemp), 0, 0);
   //      RegSetValueEx (hKey, gszKeyTTSFile, 0, REG_SZ, (BYTE*) szTemp,
   //         (DWORD)strlen(szTemp)+1);

   //      RegCloseKey (hKey);
   //   }
   //}

   m_pPlay = m_pPlayLoop = m_pPlaySel = m_pRecord =m_pStop = NULL; // so dont get called in waveoutstop

   WavePaneClear(FALSE);

   if (m_pTimeline)
      delete m_pTimeline;
   m_pTimeline = NULL;

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

   if (m_pWave)
      delete m_pWave;

   UndoClear (FALSE);

   DeleteCriticalSection (&m_csWaveOut);

   PostQuitMessage (0);
}


/***********************************************************************************
CWaveView::ButtonExists - Looks through all the buttons and sees if one with the given
ID exists. If so it returns TRUE, else FALSE

inputs
   DWORD       dwID - button ID
returns
   BOOL - TRUE if exists
*/
BOOL CWaveView::ButtonExists (DWORD dwID)
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
CWaveView::SetPointerMode - Changes the pointer mode to the new mode.

inputs
   DWORD    dwMode - new mode
   DWORD    dwButton - which button it's approaite for, 0 for left, 1 for middle, 2 for right
   BOOL     fInvalidate - If TRUE, invalidate the surrounding areaas
*/
void CWaveView::SetPointerMode (DWORD dwMode, DWORD dwButton, BOOL fInvalidate)
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
      // BUGFIX - Dnot do this because dont know pwp SetProperCursor (pwp, px.x, px.y);
   }
}

/**********************************************************************************
Init - Initalize the house view object to a new one. This ends up creating a house
   view Windows.

inputs
   PCM3DWave         pWave - Wave to use. When this window closes pWave will be deleted.
returns
   BOOL - TRUE if it succeded in creating a new view.
*/
BOOL CWaveView::Init (PCM3DWave pWave)
{
   if (m_hWnd || m_pWave)
      return FALSE;

   m_pWave = pWave;
   m_iPlayLimitMax = (int)m_pWave->m_dwSamples;

   // register the window proc if it isn't alreay registered
   static BOOL fIsRegistered = FALSE;
   if (!fIsRegistered) {
      WNDCLASS wc;

      // maint window
      memset (&wc, 0, sizeof(wc));
      wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_APPICON));
      wc.lpfnWndProc = CWaveViewWndProc;
      wc.style = CS_HREDRAW | CS_VREDRAW;
      wc.hInstance = ghInstance;
      wc.hbrBackground = NULL; //(HBRUSH)(COLOR_BTNFACE+1);
      wc.lpszClassName = "CWaveView";
      wc.hCursor = LoadCursor (NULL, IDC_NO);
      RegisterClass (&wc);

      wc.lpfnWndProc = CWaveViewMouthWndProc;
      wc.lpszClassName = "CWaveViewMouth";
      RegisterClass (&wc);

      wc.hIcon = NULL;
      wc.lpfnWndProc = CWaveViewDataWndProc;
      wc.lpszClassName = "CWaveViewData";
      wc.hCursor = NULL;
      RegisterClass (&wc);

      fIsRegistered = TRUE;
   }

#if 0 // dont use
   // enumerate the monitors
   DWORD dwLeast;
   dwLeast = FillXMONITORINFO ();
   PCListFixed pListXMONITORINFO;
   pListXMONITORINFO = ReturnXMONITORINFO();

   // If monitor = -1 then find one thats ok
   DWORD dwMonitor = -1;
   if (dwMonitor >= pListXMONITORINFO->Num())
      dwMonitor = dwLeast;

   // Look at the monitor to determine the location to create
   PXMONITORINFO pmi;
   pmi = (PXMONITORINFO) pListXMONITORINFO->Get(dwMonitor);
   if (!pmi)
      return FALSE;
#endif // 0

   // Create the window
   m_hWnd = CreateWindowEx (WS_EX_APPWINDOW, "CWaveView", "",
      WS_VISIBLE | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
      //pmi->rWork.left, pmi->rWork.top, pmi->rWork.right - pmi->rWork.left, pmi->rWork.bottom - pmi->rWork.top,
      NULL, NULL, ghInstance, (LPVOID) this);
   if (!m_hWnd)
      return FALSE;

   SetTitle ();
   UpdateAllButtons();
   RebuildPanes ();

   return TRUE;
}

/***********************************************************************************
CWaveView::UpdateAllButtons - Called when the user has progressed so far in the
tutorial, this first erases all the buttons there, and then updates them
to the appropriate display level.

*/
void CWaveView::UpdateAllButtons (void)
{
   // clear out all the buttons
   m_pbarGeneral->Clear();
   m_pbarView->Clear();
   m_pbarMisc->Clear();
   m_pbarObject->Clear();
   m_pRedo = m_pUndo = NULL;
   m_listNeedSelect.Clear();
   
   // general buttons
   PCIconButton pi;

   pi = m_pbarGeneral->ButtonAdd (0, IDB_NEW, ghInstance, "Ctrl-N", "New...",
      "Creates a new " APPSHORTNAME " file.", IDC_NEW);
   pi = m_pbarGeneral->ButtonAdd (0, IDB_OPEN, ghInstance, "Ctrl-O", "Open...",
      "Opens a " APPSHORTNAME " file.", IDC_OPEN);
   pi = m_pbarGeneral->ButtonAdd (0, IDB_SAVE, ghInstance, "Ctrl-S", "Save",
      "Saves the file.", IDC_SAVE);
   pi = m_pbarGeneral->ButtonAdd (0, IDB_SAVEAS, ghInstance, NULL, "Save as...",
      "Saves to a different file.", IDC_SAVEAS);
   pi = m_pbarGeneral->ButtonAdd (0, IDB_DIALOGBOX, ghInstance, NULL, "Settings...",
      "Lets you change the quality of the wave data and how much disk space it takes.",
      IDC_SETTINGS);

   pi = m_pbarGeneral->ButtonAdd (2, IDB_HELP, ghInstance, "F1", "Help...",
      "Brings up help and documentation.", IDC_HELPBUTTON);

   // miscellaneous buttons
   pi = m_pbarMisc->ButtonAdd (0, IDB_SELREGION,ghInstance, 
      NULL, "Select multiple objects",
      "Click and drag over a region to select the audio within the region.",
      IDC_SELREGION);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_LEFT);
   pi = m_pbarMisc->ButtonAdd (0, IDB_SELALL,ghInstance, 
      "Ctrl-A", "Select all",
      "Selects the entire wave."
      , IDC_SELALL);
   pi = m_pbarMisc->ButtonAdd (0, IDB_SELPLAYBACK,ghInstance, 
      NULL, "Select playback",
      "This selects the audio in the current playback area. You can use the looped "
      "play and playback start/stop knobs to isolate a sound, and use \"Select playback\" to "
      "select it for copying or modification."
      "<p/>"
      "If you hold the SHIFT key down, the tool does the opposite, and transfers the current "
      "selection to be the playback range."
      , IDC_SELPLAYBACK);
   pi = m_pbarMisc->ButtonAdd (0, IDB_SELNONE,ghInstance, 
      "0", "Clear selection",
      "Removes the selection."
      , IDC_SELNONE);

   m_pUndo = pi = m_pbarObject->ButtonAdd (0, IDB_UNDO, ghInstance, "Ctrl-Z", "Undo",
      "Undo the last change.", IDC_UNDOBUTTON);
   m_pRedo = pi = m_pbarObject->ButtonAdd (0, IDB_REDO, ghInstance, "Ctrl-Y", "Redo",
      "Redo the last undo.", IDC_REDOBUTTON);


   pi = m_pbarMisc->ButtonAdd (2, IDB_DELETE, ghInstance, "Del", "Delete",
      "Delete the selection."
      "<p/>"
      "If you hold down the CONTROL key the selection will be kept and everything around "
      "it will be deleted.",
      IDC_DELETEBUTTON);
   pi->Enable(FALSE);
   m_listNeedSelect.Add (&pi);   // enable this if have a selection

   m_pPasteMix = pi = m_pbarMisc->ButtonAdd (2, IDB_PASTEMIX, ghInstance, NULL, "Paste (mix)",
      "Paste from the clipboard, mixing the pasted audio over the current audio.", IDC_PASTEMIX);
   m_pPaste = pi = m_pbarMisc->ButtonAdd (2, IDB_PASTE, ghInstance, "Ctrl-V", "Paste",
      "Paste from the clipboard.", IDC_PASTEBUTTON);
   // enable paste button based on clipboard
   ClipboardUpdatePasteButton ();

   pi = m_pbarMisc->ButtonAdd (2, IDB_COPY, ghInstance, "Ctrl-C", "Copy",
      "Copy the selection to the clipboard.", IDC_COPYBUTTON);
   pi->Enable(FALSE);
   m_listNeedSelect.Add (&pi);   // enable this if have a selection

   pi = m_pbarMisc->ButtonAdd (2, IDB_CUT, ghInstance, "Ctrl-X", "Cut",
      "Cut the selection to the clipboard.", IDC_CUTBUTTON);
   pi->Enable(FALSE);
   m_listNeedSelect.Add (&pi);   // enable this if have a selection



   pi = m_pbarView->ButtonAdd (0, IDB_SHOWWAVE, ghInstance, 
      NULL, "Show waveform",
      "Clicking this shows or hides the waveform. If the wave is stereo, it will "
      "also switch between displaying a mono and stereo view."
      ,
      IDC_SHOWWAVE);
   pi = m_pbarView->ButtonAdd (0, IDB_SHOWFFT, ghInstance, 
      NULL, "Show spectrogram",
      "Clicking this shows or hides the spectrogram. If the wave is stereo, it will "
      "also switch between displaying a mono and stereo view."
      ,
      IDC_SHOWFFT);
   pi = m_pbarView->ButtonAdd (0, IDB_SHOWPHONEMES, ghInstance, 
      NULL, "Show phonemes",
      "Clicking this shows or hides the phonemes. Use the phonemes to set to set up "
      "lip synchronization animations."
      ,
      IDC_SHOWPHONEMES);
   pi = m_pbarView->ButtonAdd (0, IDB_SHOWMOUTH, ghInstance, 
      NULL, "Show mouth",
      "Show this while playing a wave file with phoneme markers to see how well "
      "lip synchronization works."
      ,
      IDC_SHOWMOUTH);
   pi = m_pbarView->ButtonAdd (0, IDB_SHOWOTHERS, ghInstance, 
      NULL, "Show others",
      "This button displays a menu of different types of analysis to show."
      ,
      IDC_SHOWOTHERS);

   pi = m_pbarView->ButtonAdd (1, IDB_ANIMJUMPSTART,ghInstance, 
      NULL, "Jump to the start",
      "Moves the play time to the start of the play range."
      ,
      IDC_ANIMJUMPSTART);
   pi = m_pbarView->ButtonAdd (1, IDB_ANIMSTOP,ghInstance, 
      "S", "Stop",
      "Stops playing."
      ,
      IDC_ANIMSTOP);
   m_pStop = pi;
   m_pStop->Enable (FALSE);
   pi = m_pbarView->ButtonAdd (1, IDB_ANIMPLAY,ghInstance, 
      "P", "Play",
      "Plays the audio within the play markers."
      ,
      IDC_ANIMPLAY);
   m_pPlay = pi;
   pi = m_pbarView->ButtonAdd (1, IDB_ANIMPLAYSEL,ghInstance, 
      "e", "Play (selection)",
      "Moves the play markers to the selection and plays it."
      ,
      IDC_ANIMPLAYSEL);
   m_pPlaySel = pi;
   pi = m_pbarView->ButtonAdd (1, IDB_ANIMPLAYLOOP,ghInstance, 
      NULL, "Play (looped)",
      "Plays the audio within the play markers. When it "
      "reaches the end the sequence is repeated."
      ,
      IDC_ANIMPLAYLOOP);
   m_pPlayLoop = pi;
   pi = m_pbarView->ButtonAdd (1, IDB_ANIMRECORD,ghInstance, 
      "R", "Record",
      "Records audio from a microphone."
      ,
      IDC_ANIMRECORD);
   m_pRecord = pi;
   pi = m_pbarView->ButtonAdd (1, IDB_ANIMJUMPEND,ghInstance, 
      NULL, "Jump to the end",
      "Moves the current play time to the end of the play range."
      ,
      IDC_ANIMJUMPEND);

   pi = m_pbarView->ButtonAdd (2, IDB_ZOOMALL,ghInstance, 
      NULL, "Show all",
      "Zooms out to display the entire wave."
      ,
      IDC_ZOOMALL);
   pi = m_pbarView->ButtonAdd (2, IDB_ZOOMSEL,ghInstance, 
      NULL, "Zoom into selection",
      "Zooms so the select area occupies the entire display."
      ,
      IDC_ZOOMSEL);
   pi = m_pbarView->ButtonAdd (2, IDB_ZOOMIN,ghInstance, 
      "+", "Zoom in",
      "To use this tool to zoom in and out. "
      "Click on the audio and move right/left (to zoom in/out timewise) "
      "or up/down (to zoom in/out vertically)."
      ,
      IDC_ZOOMIN);
   pi->FlagsSet (IBFLAG_DISABLEDARROW | IBFLAG_SHAPE_TOP);


   pi = m_pbarObject->ButtonAdd (2, IDB_FX, ghInstance, NULL, "Special effect",
      "Applies a special effect (such as echo) to the current selection.",
      IDC_FX);

   pi = m_pbarObject->ButtonAdd (1, IDB_NEWPHONE, ghInstance, "n",
      "New phoneme",
      "This creates a new phoneme over the time marked by the selection."
      "<p/>"
      "If you hold down the SHIFT key it uses the play range instead."
      "<p/>"
      "To change the lexicon/language visit the Lexicon dialog."
      ,
      IDC_NEWPHONE);
   pi = m_pbarObject->ButtonAdd (1, IDB_NEWWORD, ghInstance, "w",
      "New word...",
      "This creates a new word over the time marked by the selection."
      "<p/>"
      "If you hold down the SHIFT key it uses the play range instead.",
      IDC_NEWWORD);
   pi = m_pbarObject->ButtonAdd (1, IDB_SPEECHRECOG, ghInstance, NULL,
      "Speech recognition",
      "Press this to have " APPSHORTNAME " automatically determine what phonemes are spoken.",
      IDC_SPEECHRECOG);
   pi = m_pbarObject->ButtonAdd (1, IDB_DIALOGBOX, ghInstance, NULL,
      "Lexicon...",
      "Use this dialog to change what lexicon/language is used for the phoneme display. "
      "You only need to do this if you have created a lexicon for another language.",
      IDC_LEXICON);

#if 0 // no using brushes
   // common to all painting
   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSHSHAPE, ghInstance, NULL, "Brush shape",
      "This menu lets you chose the shape of the brush from pointy to flat.",
      IDC_BRUSHSHAPE);
   pi = m_pbarObject->ButtonAdd (1, IDB_BRUSHEFFECT, ghInstance, NULL, "Brush effect",
      "This menu lets you change what the brush does, such as increasing or decreasing volume.",
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
#endif // 0 - not using brushes

   DWORD adwWant[3];
   adwWant[0] = IDC_SELREGION;
   adwWant[1] = IDC_ZOOMIN;
   adwWant[2] = IDC_ZOOMIN;

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

   // get the undo and redo buttons set properly
   UndoUpdateButtons ();
   InvalidateDisplay();
   EnableButtonsThatNeedSel();
}

/***********************************************************************************
CWaveView::InvalidateDisplay - Redraws the display area, not the buttons.
*/
void CWaveView::InvalidateDisplay (void)
{
   DWORD i;
   PWAVEPANE pwp;
   for (i = 0; i < m_lWAVEPANE.Num(); i++) {
      pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);
      InvalidateRect (pwp->hWnd, NULL, FALSE);
   }

   if (m_hWndMouth)
      InvalidateRect (m_hWndMouth, NULL, FALSE);
}


/**********************************************************************************
CWaveView::SetProperCursor - Call this to set the cursor to whatever is appropriate
for the window
*/
void CWaveView::SetProperCursor (PWAVEPANE pwp, int iX, int iY)
{
   double fX, fY;
   DWORD dwButton = m_dwButtonDown ? (m_dwButtonDown-1) : 0;
   BOOL fInImage;

   fInImage = PointInImage (pwp, iX, iY, &fX, &fY);


   if (pwp->dwType == WP_PHONEMES) {
      if (m_dwButtonDown || (IsOverPhonemeDrag(pwp, fX) != -1))
         SetCursor (LoadCursor (NULL, IDC_SIZEWE));
      else
         SetCursor (LoadCursor (NULL, IDC_ARROW));
      return;
   }
   else if (pwp->dwType == WP_WORD) {
      if (m_dwButtonDown || (IsOverWordDrag(pwp, fX) != -1))
         SetCursor (LoadCursor (NULL, IDC_SIZEWE));
      else
         SetCursor (LoadCursor (NULL, IDC_ARROW));
      return;
   }

   if (fInImage) {
      switch (m_adwPointerMode[dwButton]) {
         case IDC_SELREGION:
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORSELREGION)));
            break;

         case IDC_BRUSH4:
            SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH4)));
            break;
         case IDC_BRUSH8:
            SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH8)));
            break;
         case IDC_BRUSH16:
            SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH16)));
            break;
         case IDC_BRUSH32:
            SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH32)));
            break;
         case IDC_BRUSH64:
            SetCursor((HCURSOR)LoadImage (ghInstance, MAKEINTRESOURCE(IDC_CURSORBRUSH64), IMAGE_CURSOR, 64, 64,LR_SHARED));
            break;

         case IDC_ZOOMIN:
            SetCursor(LoadCursor(ghInstance, MAKEINTRESOURCE(IDC_CURSORZOOMIN)));
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
CWaveView::IsOverPhonemeDrag - Returns TRUE if the cursor is over a phoneme drag
line.

inputs
   PWAVEPAE    pwp - Wave pane, already verified as being phoneme
   fp          fSample - Sample number that it's over, approx
returns
   DWORD - Index for phoneme, or -1 if cant find
*/
DWORD CWaveView::IsOverPhonemeDrag (PWAVEPANE pwp, fp fSample)
{
   // figure out the range... 2 pixels worth
   RECT r;
   GetClientRect (pwp->hWnd, &r);
   int iRange = (int)(2.0 / (double)(r.right - r.left) * (double)(m_iViewSampleRight - m_iViewSampleLeft))+1;

   int iMin = (int)fSample - iRange;
   int iMax = (int)fSample + iRange;

   DWORD i, dwNum;
   dwNum = m_pWave->m_lWVPHONEME.Num();
   PWVPHONEME pp = (PWVPHONEME)m_pWave->m_lWVPHONEME.Get(0);
   for (i = 0; i < dwNum; i++)
      if (((int)pp[i].dwSample >= iMin) && (int)pp[i].dwSample <= iMax)
         return i;


   // else
   return -1;
}


/***********************************************************************************
CWaveView::IsOverWordDrag - Returns TRUE if the cursor is over a phoneme drag
line.

inputs
   PWAVEPAE    pwp - Wave pane, already verified as being word
   fp          fSample - Sample number that it's over, approx
returns
   DWORD - Index for word, or -1 if cant find
*/
DWORD CWaveView::IsOverWordDrag (PWAVEPANE pwp, fp fSample)
{
   // figure out the range... 2 pixels worth
   RECT r;
   GetClientRect (pwp->hWnd, &r);
   int iRange = (int)(2.0 / (double)(r.right - r.left) * (double)(m_iViewSampleRight - m_iViewSampleLeft))+1;

   int iMin = (int)fSample - iRange;
   int iMax = (int)fSample + iRange;

   DWORD i, dwNum;
   dwNum = m_pWave->m_lWVWORD.Num();
   for (i = 0; i < dwNum; i++) {
      PWVWORD pp = (PWVWORD)m_pWave->m_lWVWORD.Get(i);
      if (((int)pp->dwSample >= iMin) && (int)pp->dwSample <= iMax)
         return i;
   }


   // else
   return -1;
}

/***********************************************************************************
CWaveView::ButtonDown - Left button down message
*/
LRESULT CWaveView::ButtonDown (PWAVEPANE pwp, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton)
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
   double fX, fY;
   pt.x = iX;
   pt.y = iY;

   // if it's not in the image are or don't have image mode then return
   if (!dwPointerMode || !PointInImage(pwp, iX, iY, &fX, &fY)) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return 0;
   }

   // if click on a phoneme, then select the phoneme
   if (pwp->dwType == WP_PHONEMES) {
      // find out what phoneme clicked on
      DWORD dwNum;
      int i;
      dwNum = m_pWave->m_lWVPHONEME.Num();
      PWVPHONEME pp = (PWVPHONEME)m_pWave->m_lWVPHONEME.Get(0);
      WVPHONEME ws, we;
      int iSample = (int) fX;
      ws.dwSample = 0;  // start
      we.dwSample = m_pWave->m_dwSamples;

      // see if clicked on a drag line
      m_dwPhonemeDrag = IsOverPhonemeDrag(pwp, fX);
      if (m_dwPhonemeDrag == -1) {
         // see if clicked on a specific phoneme
         for (i = -1; i < (int)dwNum; i++) {
            PWVPHONEME ps = (i >= 0) ? &pp[i] : &ws;
            PWVPHONEME pe = (i+1 < (int)dwNum) ? &pp[i+1] : &we;
            // BUGFIX - If have control or shift down then select range
            if ((iSample >= (int)ps->dwSample) && (iSample < (int)pe->dwSample)) {
               // set the selection
               // BUGFIX - If have control or shift down then select range
               if ((m_iSelStart != m_iSelEnd) && ((GetKeyState (VK_CONTROL) < 0) || (GetKeyState (VK_SHIFT) < 0))) {
                  m_iSelStart = min(m_iSelStart, (int)ps->dwSample);
                  m_iSelEnd = max(m_iSelEnd, (int)pe->dwSample);
               }
               else {
                  m_iSelStart = (int)ps->dwSample;
                  m_iSelEnd = (int)pe->dwSample;
               }

               m_iPlayLimitMin = m_iPlayCur = m_iSelStart;
               m_iPlayLimitMax = m_iSelEnd;

               UpdateScroll ();
               InvalidateDisplay();
               EnableButtonsThatNeedSel();
               return 0;
            }
         } // i

         return 0;
      } // didnt click on drag
   }
   else if (pwp->dwType == WP_WORD) {
      // find out what phoneme clicked on
      DWORD dwNum;
      int i;
      dwNum = m_pWave->m_lWVWORD.Num();
      WVWORD ws, we;
      int iSample = (int) fX;
      ws.dwSample = 0;  // start
      we.dwSample = m_pWave->m_dwSamples;

      // see if clicked on a drag line
      m_dwPhonemeDrag = IsOverWordDrag(pwp, fX);
      if (m_dwPhonemeDrag == -1) {
         // see if clicked on a specific word
         for (i = -1; i < (int)dwNum; i++) {
            PWVWORD pp = (PWVWORD)m_pWave->m_lWVWORD.Get(i);
            PWVWORD pp2 = (PWVWORD)m_pWave->m_lWVWORD.Get(i+1);

            PWVWORD ps = (i >= 0) ? pp : &ws;
            PWVWORD pe = (i+1 < (int)dwNum) ? pp2 : &we;
            if ((iSample >= (int)ps->dwSample) && (iSample < (int)pe->dwSample)) {
               // set the selection
               if ((m_iSelStart != m_iSelEnd) && ((GetKeyState (VK_CONTROL) < 0) || (GetKeyState (VK_SHIFT) < 0))) {
                  m_iSelStart = min(m_iSelStart, (int)ps->dwSample);
                  m_iSelEnd = max(m_iSelEnd, (int)pe->dwSample);
               }
               else {
                  m_iSelStart = (int)ps->dwSample;
                  m_iSelEnd = (int)pe->dwSample;
               }

               m_iPlayLimitMin = m_iPlayCur = m_iSelStart;
               m_iPlayLimitMax = m_iSelEnd;

               UpdateScroll ();
               InvalidateDisplay();
               EnableButtonsThatNeedSel();
               return 0;
            }
         } // i

         return 0;
      } // didnt click on drag
   }

   // capture
   SetCapture (pwp->hWnd);
   SetTimer (pwp->hWnd, TIMER_SCROLLDRAG, 100, 0);
   m_fCaptured = TRUE;

   // remember this position
   m_dwButtonDown = dwButton + 1;
   m_pntButtonDown.x = iX;
   m_pntButtonDown.y = iY;
   m_pntMouseLast = m_pntButtonDown;

   if ((pwp->dwType != WP_PHONEMES) && (pwp->dwType != WP_WORD)) switch (dwPointerMode) {
   case IDC_SELREGION:
      {
         // figure out where went down
         RECT r;
         GetClientRect (pwp->hWnd, &r);
         int iView = max(m_iViewSampleRight - m_iViewSampleLeft, 1);
         double f = (double)iX / (double) r.right * (double)iView + m_iViewSampleLeft;
         f = max(0,f);
         f = min(m_iViewSampleRight,f);
         m_iSelStart = m_iSelEnd = (int)f;
         InvalidateDisplay();
         EnableButtonsThatNeedSel();
      }
      break;

   case IDC_ZOOMIN:
      // remember the time and vertical position when clicked
      m_iZoomLeft = m_iViewSampleLeft;
      m_iZoomRight = m_iViewSampleRight;
      m_fZoomTop = pwp->fTop;
      m_fZoomBottom = pwp->fBottom;
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

      // FUTURERELEASE BrushApply (dwPointerMode, &m_pntButtonDown, NULL);
      break;
   }


   // BUGFIX: update cursor and tooltip
   // update the tooltip and cursor
   SetProperCursor (pwp, iX, iY);

   return 0;
}

/***********************************************************************************
CWaveView::ButtonUp - Left button down message
*/
LRESULT CWaveView::ButtonUp (PWAVEPANE pwp, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD dwButton)
{
   DWORD dwPointerMode = m_adwPointerMode[dwButton];

   // BUGFIX - If the button is not the correct one then ignore
   if (m_dwButtonDown && (dwButton+1 != m_dwButtonDown))
      return 0;

   if (m_fCaptured) {
      KillTimer (pwp->hWnd, TIMER_SCROLLDRAG);
      ReleaseCapture ();
   }
   m_fCaptured = FALSE;

   // BUGFIX - If don't have buttondown flag set then dont do any of the following
   // operations. Otherwise have problem which createa a new object, click on wall to
   // create, and ends up getting mouse-up in this window (from the mouse down in the
   // object selection window)
   if (!m_dwButtonDown)
      return 0;


   if ((pwp->dwType != WP_PHONEMES) && (pwp->dwType != WP_WORD)) switch (dwPointerMode) {
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
   SetProperCursor (pwp, iX, iY);

   return 0;
}

/***********************************************************************************
CWaveView::MouseMove - Left button down message
*/
LRESULT CWaveView::MouseMove (PWAVEPANE pwp, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
   SetProperCursor (pwp, iX, iY);


   if (m_dwButtonDown && dwPointerMode) {
      if (pwp->dwType == WP_PHONEMES) {
         // what's th elocation
         double fX;
         int iSample;
         PointInImage (pwp, iX, iY, &fX, NULL);
         iSample = (int)fX;
         iSample = max(iSample, 0);
         WordPhonemeAttach (m_dwPhonemeDrag, FALSE, (DWORD)iSample);
      }
      else if (pwp->dwType == WP_WORD) {
         // what's th elocation
         double fX;
         int iSample;
         PointInImage (pwp, iX, iY, &fX, NULL);
         iSample = (int)fX;
         iSample = max(iSample, 0);
         WordPhonemeAttach (m_dwPhonemeDrag, TRUE, (DWORD)iSample);
      }
      else switch (dwPointerMode) {

      case IDC_ZOOMIN:
         {
            RECT r;
            GetClientRect (pwp->hWnd, &r);
            if (!r.right || !r.bottom)
               break;   // nothing to scale off of

            // what was the position where originally clicked?
            CPoint pClick;
            pClick.Zero();
            pClick.p[0] = (fp)m_pntButtonDown.x / (fp)r.right * (fp)(m_iZoomRight - m_iZoomLeft) + m_iZoomLeft;
            pClick.p[1] = (fp)m_pntButtonDown.y / (fp)r.bottom * (m_fZoomBottom - m_fZoomTop) + m_fZoomTop;

            // what was the original scale?
            CPoint pScaleOrig;
            pScaleOrig.Zero();
            pScaleOrig.p[0] = m_iZoomRight - m_iZoomLeft;
            pScaleOrig.p[1] = m_fZoomBottom - m_fZoomTop;

            // what's the new scale?
            CPoint pScaleNew;
            pScaleNew.Copy (&pScaleOrig);
            pScaleNew.p[0] *= pow (2.0, -(fp)(iX - m_pntButtonDown.x) / 200.0);
            pScaleNew.p[1] *= pow (2.0, (fp)(iY - m_pntButtonDown.y) / 200.0);

            // calculate new extents
            CPoint pLR, pUD;
            pLR.p[0] = pClick.p[0] - ((fp)m_pntButtonDown.x / (fp)r.right) * pScaleNew.p[0];
            pLR.p[1] = pClick.p[0] + (1.0 - (fp)m_pntButtonDown.x / (fp)r.right) * pScaleNew.p[0];
            pUD.p[0] = pClick.p[1] - ((fp)m_pntButtonDown.y / (fp)r.bottom) * pScaleNew.p[1];
            pUD.p[1] = pClick.p[1] + (1.0 - (fp)m_pntButtonDown.y / (fp)r.bottom) * pScaleNew.p[1];

            // min and max
            m_iViewSampleLeft = (int) pLR.p[0];
            m_iViewSampleRight = (int) pLR.p[1];
            m_iViewSampleLeft = max(m_iViewSampleLeft, 0);
            m_iViewSampleRight = max(m_iViewSampleLeft+1, m_iViewSampleRight);
            m_iViewSampleRight = min(m_iViewSampleRight, (int)m_pWave->m_dwSamples+1);
            pwp->fTop = pUD.p[0];
            pwp->fBottom = pUD.p[1];
            pwp->fTop = max(pwp->fTop, 0);
            pwp->fTop = min(pwp->fTop, 1 - CLOSE);
            pwp->fBottom = max(pwp->fBottom, pwp->fTop+CLOSE);
            pwp->fBottom = min(pwp->fBottom, 1);
            
            // update the scrollbar for zoom in out
            WavePaneUpdateScroll (pwp);

            // update
            UpdateScroll();
         }
         break;
      case IDC_SELREGION:
         {
            // figure out where went down
            RECT r;
            GetClientRect (pwp->hWnd, &r);
            int iView = max(m_iViewSampleRight - m_iViewSampleLeft, 1);
            double f = (double)iX / (double) r.right * (double)iView + m_iViewSampleLeft;
            f = max(0,f);
            f = min(m_iViewSampleRight,f);
            m_iSelEnd = (int)f;
            InvalidateDisplay();
            EnableButtonsThatNeedSel();
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

         // FUTURERELEASE BrushApply (dwPointerMode, &m_pntMouseLast, &pLast);
         break;
      } // case
   }

   return 0;

}

/***********************************************************************************
CWaveView::Paint - WM_PAINT messge
*/
LRESULT CWaveView::Paint (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
DataWndProc - Window procedure for the house view object.
*/
LRESULT CWaveView::DataWndProc (PWAVEPANE pwp, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_MOUSEWHEEL:
      // pass it up
      return WndProc (hWnd, uMsg, wParam, lParam);

   case WM_TIMER:
      if (wParam == TIMER_SCROLLDRAG) {
         int iScroll = 0;
         RECT r;
         GetClientRect (hWnd, &r);
         if (m_pntMouseLast.x > r.right) {
            iScroll = (m_iViewSampleRight - m_iViewSampleLeft) / 16;

            if (iScroll + m_iViewSampleRight > (int)m_pWave->m_dwSamples)
               iScroll = (int)m_pWave->m_dwSamples - m_iViewSampleRight;
         }
         else if (m_pntMouseLast.x < 0) {
            iScroll = -(m_iViewSampleRight - m_iViewSampleLeft) / 16;
            if (iScroll + m_iViewSampleLeft < 0)
               iScroll = -m_iViewSampleLeft;
         }
         else
            return 0;   // no change

         // scroll
         m_iViewSampleRight += iScroll;
         m_iViewSampleLeft += iScroll;
         UpdateScroll ();

         // simulate a mouse move
         LPARAM lp;
         lp = MAKELPARAM((WORD)(short)m_pntMouseLast.x, (WORD)(short)m_pntMouseLast.y);
         m_pntMouseLast.x = -1000;
         MouseMove (pwp, hWnd, WM_MOUSEMOVE, 0, lp);
         return 0;
      }
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC   hDC;
         RECT r;
         hDC = BeginPaint (hWnd, &ps);
         GetClientRect (hWnd, &r);

         // figure out what's changed
         RECT rChange;
         double fLeft, fRight, fTop, fBottom, fTopWave, fBottomWave;
         rChange.left = max(r.left, ps.rcPaint.left);
         rChange.top = max(r.top, ps.rcPaint.top);
         rChange.right = min(r.right, ps.rcPaint.right);
         rChange.bottom = min(r.bottom, ps.rcPaint.bottom);
         if ((rChange.right <= rChange.left) || (rChange.bottom <= rChange.top)) {
            EndPaint (hWnd, &ps);
            return 0;
         }
         fLeft = (double)rChange.left / (double)r.right *
            (double)(m_iViewSampleRight - m_iViewSampleLeft) + (double)m_iViewSampleLeft;
         fRight = (double)rChange.right / (double)r.right *
            (double)(m_iViewSampleRight - m_iViewSampleLeft) + (double)m_iViewSampleLeft;
         fTop = (double)rChange.top / (double)r.bottom *
            (pwp->fBottom - pwp->fTop) + pwp->fTop;
         fBottom = (double)rChange.bottom / (double)r.bottom *
            (pwp->fBottom - pwp->fTop) + pwp->fTop;
         fTopWave = (.5 - fTop) * (double)0x10000;
         fBottomWave = (.5 - fBottom) * (double)0x10000;

         BOOL fDrawTimeTicks, fDrawSel;
         fDrawTimeTicks = FALSE;
         COLORREF crMajor, crMinor;
         crMinor = RGB(0xc0,0xc0,0xc0);
         crMajor = RGB(0,0,0);
         fDrawTimeTicks = fDrawSel = FALSE;

         switch (pwp->dwType) {
         case WP_WAVE:
            fDrawTimeTicks = fDrawSel = TRUE;
            m_pWave->PaintWave (hDC, &rChange, fLeft, fRight, fTopWave, fBottomWave, pwp->dwSubType);
            break;

         case WP_SRFEATURES:
         case WP_SRFEATURESVOICED:
         case WP_SRFEATURESNOISE:
         case WP_SRFEATURESPHASE:
         case WP_SRFEATURESPHASEPITCH:
         case WP_SRFEATURESPCM:
         case WP_FFTSTRETCHEXP:
         case WP_FFTSTRETCH:
         case WP_FFTFREQ:
            {
               fp fFreqRange;
               DWORD dwPassToFFT = 1;
               BOOL fShowHz = TRUE;

               switch (pwp->dwType) {
                  case WP_FFTSTRETCHEXP:
                     dwPassToFFT = 3;
                     fShowHz = FALSE;
                     break;

                  case WP_SRFEATURES:
                     dwPassToFFT = 4;
                     fShowHz = FALSE;
                     break;

                  case WP_SRFEATURESVOICED:
                     dwPassToFFT = 6;
                     fShowHz = FALSE;
                     break;

                  case WP_SRFEATURESNOISE:
                     dwPassToFFT = 7;
                     fShowHz = FALSE;
                     break;

                  case WP_SRFEATURESPHASE:
                     dwPassToFFT = 8;
                     fShowHz = FALSE;
                     break;

                  case WP_SRFEATURESPHASEPITCH:
                     dwPassToFFT = 10;
                     fShowHz = FALSE;
                     break;

                  case WP_SRFEATURESPCM:
                     dwPassToFFT = 9;
                     fShowHz = FALSE;
                     break;

                  case WP_FFTSTRETCH:
                     dwPassToFFT = 2;
                     break;
                  default:
                  case WP_FFTFREQ:
                     dwPassToFFT = 1;
                     break;
               }
               fDrawTimeTicks = fDrawSel = TRUE;
               m_pWave->PaintFFT (hDC, &rChange, fLeft, fRight, fTop, fBottom, pwp->dwSubType,
                  hWnd, dwPassToFFT, &fFreqRange);
               crMajor = RGB(0xff,0xff,0xff);

               // drraw the horizontal ticks
               CMatrix mPixelToTime, mTimeToPixel;
               fp fScale;
               fScale = -(pwp->fBottom - pwp->fTop) / (fp)r.bottom * fFreqRange;
               mPixelToTime.Scale (1, 1, fScale);
               fScale = (1.0 - pwp->fTop) * fFreqRange;
               mTimeToPixel.Translation (0, 0, fScale);
               mPixelToTime.MultiplyRight (&mTimeToPixel);
               mPixelToTime.Invert4 (&mTimeToPixel);
               TimelineHorzTicks (hDC, &r, &mPixelToTime, &mTimeToPixel,
                  crMinor, crMajor, fShowHz ? AIT_HZ : AIT_NUMBER);
            }
            break;

         case WP_FFT:
            {
               fDrawTimeTicks = fDrawSel = TRUE;
               m_pWave->PaintFFT (hDC, &rChange, fLeft, fRight, fTop, fBottom, pwp->dwSubType,
                  hWnd);
               crMajor = RGB(0xff,0xff,0xff);

               // drraw the horizontal ticks
               CMatrix mPixelToTime, mTimeToPixel;
               fp fScale;
               fScale = -(pwp->fBottom - pwp->fTop) / (fp)r.bottom * (fp) m_pWave->m_dwSamplesPerSec / 2.0;
               mPixelToTime.Scale (1, 1, fScale);
               fScale = (1.0 - pwp->fTop) * (fp)m_pWave->m_dwSamplesPerSec / 2.0;
               mTimeToPixel.Translation (0, 0, fScale);
               mPixelToTime.MultiplyRight (&mTimeToPixel);
               mPixelToTime.Invert4 (&mTimeToPixel);
               TimelineHorzTicks (hDC, &r, &mPixelToTime, &mTimeToPixel,
                    crMinor, crMajor, AIT_HZ);
            }
            break;

         case WP_PITCH:
            {
               fDrawTimeTicks = fDrawSel = TRUE;
               m_pWave->PaintPitch (hDC, &rChange, fLeft, fRight, fTop, fBottom, pwp->dwSubType, hWnd);

               // drraw the horizontal ticks
               CMatrix mPixelToTime, mTimeToPixel;
               fp fScale;
               fScale = -(pwp->fBottom - pwp->fTop) / (fp)r.bottom * (fp) m_pWave->m_dwSamplesPerSec / 4.0;
               mPixelToTime.Scale (1, 1, fScale);
               fScale = (1.0 - pwp->fTop) * (fp)m_pWave->m_dwSamplesPerSec / 4.0;
                  // BUGFIX - using 4.0 instead of / 2.0 for pitch since otherwise too large a rang
               mTimeToPixel.Translation (0, 0, fScale);
               mPixelToTime.MultiplyRight (&mTimeToPixel);
               mPixelToTime.Invert4 (&mTimeToPixel);
               TimelineHorzTicks (hDC, &r, &mPixelToTime, &mTimeToPixel,
                    crMinor, crMajor, AIT_HZ);
            }
            break;

         case WP_PHONEMES:
            {
               // load it
               if (!m_pLex)
                  m_pLex = MLexiconCacheOpen (m_szLexicon, FALSE);

               fDrawTimeTicks = fDrawSel = FALSE;
               m_pWave->PaintPhonemes (hDC, &r, m_iViewSampleLeft, m_iViewSampleRight, m_pLex);
            }
            break;

         case WP_WORD:
            {
               fDrawTimeTicks = fDrawSel = FALSE;
               m_pWave->PaintWords (hDC, &r, m_iViewSampleLeft, m_iViewSampleRight);
            }
            break;

         case WP_ENERGY:
            {
               fDrawTimeTicks = fDrawSel = TRUE;
               m_pWave->PaintEnergy (hDC, &rChange, fLeft, fRight, fTop, fBottom, pwp->dwSubType);

               // drraw the horizontal ticks
               CMatrix mPixelToTime, mTimeToPixel;
               fp fScale;
               fScale = -(pwp->fBottom - pwp->fTop) / (fp)r.bottom * 96.0;
               mPixelToTime.Scale (1, 1, fScale);
               fScale = (-pwp->fTop) * 96.0;
               mTimeToPixel.Translation (0, 0, fScale);
               mPixelToTime.MultiplyRight (&mTimeToPixel);
               mPixelToTime.Invert4 (&mTimeToPixel);
               TimelineHorzTicks (hDC, &r, &mPixelToTime, &mTimeToPixel,
                    crMinor, crMajor, AIT_NUMBER);
            }
            break;

         default:
            FillRect (hDC, &r, (HBRUSH) GetStockObject (WHITE_BRUSH));
            break;
         }

         // draw the time ticks?
         if (fDrawTimeTicks && (m_iViewSampleRight > m_iViewSampleLeft)) {
            CMatrix mPixelToTime, mTimeToPixel;
            fp fScale;
            fScale = (fp)(m_iViewSampleRight - m_iViewSampleLeft) / (fp)r.right / (fp)m_pWave->m_dwSamplesPerSec;
            mPixelToTime.Scale (fScale, fScale, 1);
            fScale = (fp)m_iViewSampleLeft / (fp)m_pWave->m_dwSamplesPerSec;
            mTimeToPixel.Translation (fScale, fScale, 0);
            mPixelToTime.MultiplyRight (&mTimeToPixel);
            mPixelToTime.Invert4 (&mTimeToPixel);
            TimelineTicks (hDC, &r, &mPixelToTime, &mTimeToPixel,
                    crMinor, crMajor,
                    ((m_iViewSampleRight - m_iViewSampleLeft) < 100) ? m_pWave->m_dwSamplesPerSec : 0);
         }

         // draw the selection
         // BUGFIX - Draw selection even if nothing
         if (fDrawSel && (m_iViewSampleRight > m_iViewSampleLeft)) {
            // convert this to pixels
            double f;
            int ai[2];
            DWORD i;
            for (i = 0; i < 2; i++) {
               f = i ? max(m_iSelEnd,m_iSelStart) : min(m_iSelEnd,m_iSelStart);

               f -= m_iViewSampleLeft;
               f /= (double)(m_iViewSampleRight - m_iViewSampleLeft);
               f *= r.right; // assume left is 0
               
               f = max(f, -5);
               f = min(f, r.right+5);
               ai[i] = (int)f;
            } // i

            // rectanle
            RECT rFill;
            rFill.left = ai[0];
            rFill.right = ai[1]+1;
            rFill.top = r.top + 4;
            rFill.bottom = r.bottom - 4;

            HBRUSH hbr, hbrOld;
            int iMode;
            hbr = (HBRUSH)GetStockObject (HOLLOW_BRUSH);
            // BUGFIX - No cross-hatch hbr = CreateHatchBrush (HS_FDIAGONAL, RGB(0xe0,0,0));
            hbrOld = (HBRUSH) SelectObject (hDC, hbr);
            HPEN hPen, hPenOld;
            hPen = CreatePen (PS_SOLID, 0, RGB(0xff,0,0));
            hPenOld = (HPEN) SelectObject (hDC, hPen);
            iMode = SetBkMode (hDC, TRANSPARENT);
            Rectangle (hDC, rFill.left, rFill.top, rFill.right, rFill.bottom);
            SetBkMode (hDC, iMode);
            SelectObject (hDC, hbrOld);
            SetBkMode (hDC, iMode);
            // BUGFIX - No crosshatch DeleteObject (hbr);
            SelectObject (hDC, hPenOld);
            DeleteObject (hPen);
         }

         EndPaint (hWnd, &ps);
      }
      return 0;


   case WM_VSCROLL:
      {
         // only deal with horizontal scroll
         HWND hWndScroll = pwp->hWnd;

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
         fp fView;
         fView = pwp->fBottom - pwp->fTop;
         pwp->fTop = (fp)si.nPos / 1000.0;
         pwp->fBottom = pwp->fTop + fView;
         WavePaneUpdateScroll (pwp);
         InvalidateRect (pwp->hWnd, NULL, FALSE);

         return 0;
      }
      break;


   case WM_LBUTTONDOWN:
      return ButtonDown (pwp, hWnd, uMsg, wParam, lParam, 0);
   case WM_MBUTTONDOWN:
      return ButtonDown (pwp, hWnd, uMsg, wParam, lParam, 1);
   case WM_RBUTTONDOWN:
      return ButtonDown (pwp, hWnd, uMsg, wParam, lParam, 2);
   case WM_LBUTTONUP:
      return ButtonUp (pwp, hWnd, uMsg, wParam, lParam, 0);
   case WM_MBUTTONUP:
      return ButtonUp (pwp, hWnd, uMsg, wParam, lParam, 1);
   case WM_RBUTTONUP:
      return ButtonUp (pwp, hWnd, uMsg, wParam, lParam, 2);
   case WM_MOUSEMOVE:
      // set focus so mousewheel works - as opposed to getting taken by listbox
      SetFocus (m_hWnd);

      return MouseMove (pwp, hWnd, uMsg, wParam, lParam);

   }

   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/****************************************************************************
HelpPage
*/
BOOL HelpPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCWaveView pv = (PCWaveView) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Help";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/**************************************************************************
WordRenamePage
*/
BOOL WordRenamePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWSTR pszWord = (PWSTR) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pszWord);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         if (!_wcsicmp(p->psz, L"ok")) {
            PCEscControl pControl;

            pszWord[0] = 0;
            DWORD dwNeeded;
            pControl = pPage->ControlFind (L"name");
            if (pControl)
               pControl->AttribGet (Text(), pszWord, 128, &dwNeeded);

            break;
         }
      }
      break;
   }

   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************************
WndProc - Window procedure for the house view object.
*/
LRESULT CWaveView::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   //if (uMsg == gdwMouseWheel)
   //   uMsg = WM_MOUSEWHEEL;

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

         // add scrollbars
         m_hWndScroll = CreateWindow ("SCROLLBAR", "",
               WS_VISIBLE | WS_CHILD | SBS_HORZ | WS_BORDER,
               10, 10, 10, 10,   // temporary sizes
               hWnd, (HMENU) IDC_MYSCROLLBAR, ghInstance, NULL);

         // add timeline
         m_pTimeline = new CTimeline;
         if (m_pTimeline) {
            m_pTimeline->Init (hWnd, m_cBackLight, m_cBackMed,
               RGB(0xc0,0xff,0xff), RGB (0x40,0x80,0xff), RGB(0,0,0), WM_USER+89);
            m_pTimeline->LimitsSet (0, 0, -1);
         }

         // clibboard viewer
         m_hWndClipNext = SetClipboardViewer (hWnd);
      }

      return 0;

   case WM_MYWOMMESSAGE:
      if (m_hPlayWaveOut)
         PlayCallback (m_hPlayWaveOut, wParam, lParam, 0);

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

   case WM_DRAWCLIPBOARD:
      {
         ClipboardUpdatePasteButton ();

         if (m_hWndClipNext)
            SendMessage (m_hWndClipNext, uMsg, wParam, lParam);
      }
      return 0;

   case WM_TIMER:
      if ((wParam == TIMER_AIRBRUSH) && m_dwButtonDown) {
         // FUTURERELEASE BrushApply (m_adwPointerMode[m_dwButtonDown-1],  &m_pntMouseLast, NULL);
      }
      else if ((wParam == TIMER_PLAYBACK) && m_hPlayWaveOut) {
         if (!m_lPlaySync.Num())
            return 0; // shouldnt happen

         MMTIME mt;
         memset (&mt, 0, sizeof(mt));
         mt.wType = TIME_BYTES;
#if 1
         // disable this because hanging in vista
         // re-enable becase did postmessage
         EnterCriticalSection (&m_csWaveOut);
         waveOutGetPosition (m_hPlayWaveOut, &mt, sizeof(mt));
         LeaveCriticalSection (&m_csWaveOut);
#else
         mt = m_mtLast;
#endif

         // eliminate positions from playback list as long as the time exceeds the 2nd one
         DWORD *padw;
         while (m_lPlaySync.Num() >= 2) {
            padw = (DWORD*) m_lPlaySync.Get(0);
            if (mt.u.cb < padw[2])
               break;   // the current play position is between the 1st and 2nd entries

            // else, the play position is past the first entry, so eliminate it
            // from the list
            m_lPlaySync.Remove (0);
         }

         // make sure mor than 1st entry
         padw = (DWORD*) m_lPlaySync.Get(0);
         if (mt.u.cb < padw[0])
            return 0;   // before first entry to dont adjust

         // if get here play position is more than first entry
         DWORD dwDiff;
         dwDiff = mt.u.cb - padw[0];
         dwDiff /= (m_pWave->m_dwChannels * sizeof(short));
         dwDiff += (DWORD) padw[1];

         // update the slider with current play position
         m_pTimeline->PointerSet ((fp)dwDiff);
         if (m_hWndMouth) {
            InvalidateRect (m_hWndMouth, NULL, FALSE);
            UpdateWindow (m_hWndMouth);
         }

         return 0;
      }
      break;

   case WM_DESTROY:

      // close mouth
      if (m_hWndMouth)
         DestroyWindow (m_hWndMouth);

      // stop playback in case playing
      Stop();

      // remove from the clipboard chain
      ChangeClipboardChain(hWnd, m_hWndClipNext); 

      if (m_pTimeline)
         delete m_pTimeline;
      m_pTimeline = NULL;

      break;

   case WM_USER+89:  // from the timeline to indicate change
      {
         if (wParam == 1) {
            // if min/max have changed then update
            fp fMin, fMax, fScene;
            m_pTimeline->LimitsGet (&fMin, &fMax, &fScene);
            if ((fMin != (fp) m_iPlayLimitMin) || (fMax != (fp)m_iPlayLimitMax)) {
               m_iPlayLimitMin = (int) fMin;
               m_iPlayLimitMax = (int) fMax;

               // control limits
               m_iPlayLimitMin = max(0,m_iPlayLimitMin);
               m_iPlayLimitMin = min((int)m_pWave->m_dwSamples, m_iPlayLimitMin);
               m_iPlayLimitMax = max(m_iPlayLimitMax,m_iPlayLimitMin);
               m_iPlayLimitMax = min((int)m_pWave->m_dwSamples, m_iPlayLimitMax);
               if (m_iPlayLimitMax == m_iPlayLimitMin) {
                  if (m_iPlayLimitMin)
                     m_iPlayLimitMin = m_iPlayLimitMax - 1;
                  else
                     m_iPlayLimitMax = min(m_iPlayLimitMin+1, (int)m_pWave->m_dwSamples);
               }

               // if changed then send up
               if ((fMin != (fp) m_iPlayLimitMin) || (fMax != (fp)m_iPlayLimitMax))
                  m_pTimeline->LimitsSet (m_iPlayLimitMin, m_iPlayLimitMax, fScene);
            }
         }
         else if (wParam == 0) {

            // play position
            int iOld;
            iOld = m_iPlayCur;
            m_iPlayCur = m_pTimeline->PointerGet();
            m_iPlayCur = max(m_iPlayCur, m_iPlayLimitMin);
            m_iPlayCur = min(m_iPlayCur, m_iPlayLimitMax);
            if (m_iPlayCur != iOld) {
               // if playing
               if (m_hPlayWaveOut) {
                  iOld = m_iPlayCur;
                  Stop ();
                  m_iPlayCur = iOld;
                  Play (hWnd, m_fPlayLooped);
               }

               // if change
               if ((fp)m_iPlayCur != m_pTimeline->PointerGet()) {
                  m_pTimeline->PointerSet (m_iPlayCur);
                  if (m_hWndMouth) {
                     InvalidateRect (m_hWndMouth, NULL, FALSE);
                     UpdateWindow (m_hWndMouth);
                  }
               }
            }
         }
      }
      return 0;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case IDC_ANIMSTOPIFNOBUF:
         if (m_dwPlayBufOut)
            return 0;   // was posted by self-stopping code, but if buffer is out then
                        // restarted playing (because did a skip to previous or next), so
                        // dont stop
         // fall through
      case IDC_ANIMSTOP:
         Stop();
         return 0;


      case ID_FILTERING_ADVANCEDFILTERING:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXFILTER, FXFilterPage, &m_FXFilter);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            // do the effect
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Filtering...");
            fp afFreqBands[10];
            DWORD adwTimeBands[2];
            DWORD i;
            for (i = 0; i < 10; i++)
               afFreqBands[i] = 1000.0 / 32 * pow ((fp)2, (fp)i);
            adwTimeBands[0] = 0;
            adwTimeBands[1] = pWave->m_dwSamples;
            pWave->Filter (10, afFreqBands,
               m_FXFilter.fIgnoreEnd ? 1 : 2, adwTimeBands,
               &m_FXFilter.afFilter[0][0], &Progress);
            FXPaste (pWave);
         }
         return 0;

      case ID_MISC_ACOUSTICCOMPRESSION:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXACCOMPRESS, FXAcCompressPage, &m_FXAcCompress);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            // do the effect
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Compressing...");
            pWave->FXAcousticCompress (1.0 - m_FXAcCompress.fCompress, &Progress);
            FXPaste (pWave);
         }
         return 0;

      case ID_MISC_ECHOANDREVERB:
         {
#if 0
            // Hack to convert to 16 khz
            DWORD i, dwNum;
            CM3DWave wave;
            char szTemp[256];
            for (i = 4012; i < 5144; i++) {
               sprintf (szTemp, "c:\\mikerozaktts\\ttsrec%.5d.wav", (int)i);
               if (!wave.Open (NULL, szTemp))
                  continue;

               wave.ConvertSamplesPerSec (16000, NULL);

               dwNum = i - 4011;
               sprintf (wave.m_szFile, "c:\\temp\\arctic_%c%.4i.wav",
                  (dwNum < 594) ? 'a' : 'b',
                  (dwNum < 594) ? (int)dwNum : (int)(dwNum - 593));
      
               wave.Save (FALSE, NULL);
            } // i
#endif

            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            // initialize echo
            CM3DWave Wave;
            Wave.New (m_pWave->m_dwSamplesPerSec, m_pWave->m_dwChannels);
            m_FXEcho.hBit = NULL;
            m_FXEcho.pWave = &Wave;
            FXEchoPageGenBMP (hWnd, &m_FXEcho);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXECHO, FXEchoPage, &m_FXEcho);
            if (m_FXEcho.hBit)
               DeleteObject (m_FXEcho.hBit);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            // do the effect
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Applying echo and reverb...");
            pWave->FXConvolve (&Wave, &Progress);
            FXPaste (pWave);
         }
         return 0;

      case ID_MISC_BLENDFORLOOP:
#if 0 // def _DEBUG // to test FFT
         {
            CSinLUT SinLUT;
            CMem memFFTScratch;
            float afOrig[4096], afNew[4096];
            int iSign;
            for (iSign = -1; iSign <= 1; iSign += 2) {
               DWORD dwNum = 64;
               DWORD i;
               srand(1);
               for (i = 0; i < dwNum; i++)
                  afOrig[i] = afNew[i] = i+1; // rand();
               for (; i < sizeof(afOrig)/sizeof(float); i++)
                  afOrig[i] = afNew[i] = 100000.0;

               FFTRecurseRealOld (afOrig-1, dwNum, iSign, &SinLUT, &memFFTScratch); // old
               FFTRecurseReal (afNew-1, dwNum, iSign, &SinLUT, &memFFTScratch);

               OutputDebugString ("\r\nFFT test");
               char szTemp[256];
               for (i = 0; i < dwNum; i++) {
                  sprintf (szTemp, "\r\n\t%g: %g to %g", (double)(afOrig[i] - afNew[i]), (double)afOrig[i], (double)afNew[i]);
                  if (fabs(afOrig[i] - afNew[i]) > 1)
                     strcat (szTemp, " ****");
                  OutputDebugString (szTemp);
               }
            } // iSign
         }
         return 0;
#endif
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXBLEND, FXBlendPage, &m_FXBlend);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            // NOTE: With BLEND always select entire wave
            m_iSelStart = 0;
            m_iSelEnd = (int)m_pWave->m_dwSamples;

            // do the effect
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Blending...");
            pWave->FXBlend (m_FXBlend.fLoop, &Progress);
            FXPaste (pWave);
         }
         return 0;

      case ID_RESYNTHESIS_CHANGETHEVOICE:
         {
            // calculate the pitch and SR features
            m_pWave->CalcSRFeaturesIfNeeded(WAVECALC_SEGMENT, hWnd);
            m_pWave->CalcPitchIfNeeded (WAVECALC_SEGMENT, hWnd);

            // window
            DWORD dwRet;
            {
               CEscWindow cWindow;
               RECT r;
               DialogBoxLocation2 (hWnd, &r);
               cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
               dwRet = m_VoiceDisguise.DialogModifyWave (&cWindow, m_pWave);
            }

            if (dwRet == 2) {
               // undo
               UndoCache();
               UndoAboutToChange();

               // calculate...
               CProgress Progress;
               BOOL fFeatures = FALSE;
               Progress.Start (hWnd, "Converting voice...");
               fFeatures = !m_pWave->m_dwSRSamples || !m_pWave->m_adwPitchSamples[PITCH_F0];
               if (fFeatures) {
                  Progress.Push (0, .1);
                  m_pWave->CalcPitch (WAVECALC_SEGMENT, &Progress);
                  Progress.Pop();
                  Progress.Push (.1, .5);
                  m_pWave->CalcSRFeatures (WAVECALC_SEGMENT, &Progress);
                  Progress.Pop ();
                  Progress.Push (.5, 1);
               }

               m_VoiceDisguise.SynthesizeFromSRFeature (4, m_pWave, NULL, 0, NULL, NULL, NULL, TRUE, &Progress);

               if (fFeatures)
                  Progress.Pop();

               InvalidateDisplay();
            }
         }
         return 0;

      case ID_PITCHANDSPEED_PICHANDSPEED:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXPITCHSPEED, FXPitchSpeedPage, &m_FXPitchSpeed);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            // do the effect
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Adjusting pitch and speed...");
            DWORD dwOrig;
            dwOrig = pWave->m_dwSamplesPerSec;
            pWave->ConvertSamplesPerSec ((DWORD)((fp)dwOrig / m_FXPitchSpeed.fPitch), &Progress);
            pWave->m_dwSamplesPerSec = dwOrig;
            FXPaste (pWave);
         }
         return 0;


      case ID_VOICEMODIFICATION_PSOLAPITCH:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXPSOLA, FXPSOLAPage, &m_FXPSOLA);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            // undo
            UndoCache();
            UndoAboutToChange();

            // calculate...
            CProgress Progress;

            BOOL fFeatures = FALSE;
            Progress.Start (hWnd, "Doing PSOLA effects...");
            m_pWave->FXPSOLAStretch (m_FXPSOLA.fPitch, m_FXPSOLA.fDuration, m_FXPSOLA.fFormants, &Progress);

            InvalidateDisplay();
         }
         return 0;

      case ID_PITCHANDSPEED_SPEEDWITHOUTPITCHSHIFT:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXSPEED, FXPitchSpeedPage, &m_FXPitchSpeed);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            m_pWave->CalcSRFeaturesIfNeeded(WAVECALC_SEGMENT, hWnd);
            m_pWave->CalcPitchIfNeeded(WAVECALC_SEGMENT, hWnd);

            // undo
            UndoCache();
            UndoAboutToChange();

            // calculate...
            CProgress Progress;

            BOOL fFeatures = FALSE;
            Progress.Start (hWnd, "Changing speed...");
            m_pWave->FXSRFEATUREStretch (m_FXPitchSpeed.fPitch);

#if 0 // def _DEBUG
            // hack so can see phase change
            UndoCache();
            UndoAboutToChange();
#endif

            CVoiceSynthesize vs;
            vs.SynthesizeFromSRFeature (4, m_pWave, NULL, 0, 0.0, NULL, TRUE, &Progress);

            // selct all
            m_iViewSampleLeft = m_iPlayLimitMin = m_iSelStart = 0;
            m_iViewSampleRight = m_iPlayLimitMax = m_iSelEnd = (int)m_pWave->m_dwSamples;
            InvalidateDisplay();
            UpdateScroll();
            EnableButtonsThatNeedSel();
         }
         return 0;

      case ID_PITCHANDSPEED_PITCHCHANGEWITHOUTSPEEDCHANGE:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXPITCH, FXPitchSpeedPage, &m_FXPitchSpeed);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            m_pWave->CalcSRFeaturesIfNeeded(WAVECALC_SEGMENT, hWnd);
            m_pWave->CalcPitchIfNeeded(WAVECALC_SEGMENT, hWnd);

            // undo
            UndoCache();
            UndoAboutToChange();

            // calculate...
            CProgress Progress;

            BOOL fFeatures = FALSE;
            Progress.Start (hWnd, "Shifting pitch...");
            CVoiceDisguise vd;
            vd.m_fPitchScale = m_FXPitchSpeed.fPitch;
            vd.SynthesizeFromSRFeature (4, m_pWave, NULL, 0, NULL, NULL, NULL, TRUE, &Progress);
            InvalidateDisplay();
         }
         return 0;

      case ID_VOICEMODIFICATION_WHISPER:
         {
            m_pWave->CalcSRFeaturesIfNeeded(WAVECALC_SEGMENT, hWnd);
            m_pWave->CalcPitchIfNeeded(WAVECALC_SEGMENT, hWnd);

            // undo
            UndoCache();
            UndoAboutToChange();

            // calculate...
            CProgress Progress;

            BOOL fFeatures = FALSE;
            Progress.Start (hWnd, "Turning into whisper...");
            CVoiceDisguise vd;
            vd.m_afVoicedToUnvoiced[SROCTAVE-4] = 0.1;
            vd.m_afVoicedToUnvoiced[SROCTAVE-3] = 0.5;
            vd.m_afVoicedToUnvoiced[SROCTAVE-2] = .8;
            vd.m_afVoicedToUnvoiced[SROCTAVE-1] = 1;
            vd.m_abOverallVolume[0] = SRABSOLUTESILENCE;
            vd.m_abOverallVolume[1] = 0;
            vd.SynthesizeFromSRFeature (4, m_pWave, NULL, 0, NULL, NULL, NULL, FALSE, &Progress);
            InvalidateDisplay();
         }
         return 0;

#ifdef _DEBUG
      //case ID_VOICEMODIFICATION_WHISPER: // so dont need to create new menu
      case ID_VOICEMODIFICATION_VOICECHAT:
         {

            // undo
            UndoCache();
            UndoAboutToChange();


            // BUGBUG - test ADPCM compress
            CMem memADPCM, memSamples;
            ADPCMCompress (m_pWave->m_psWave, m_pWave->m_dwSamples, &memADPCM);
            ADPCMDecompress (memADPCM.p, (DWORD) memADPCM.m_dwCurPosn, &memSamples);

            short *pasADPCM = (short*)memSamples.p;
            DWORD i;
            for (i = 0; i < m_pWave->m_dwSamples; i++)
               m_pWave->m_psWave[i] -= pasADPCM[i];
            // memcpy (m_pWave->m_psWave, memSamples.p, memSamples.m_dwCurPosn);
            // BUGBUG


#if 0 // for voice chat
            DWORD dwQuality = VCH_ID_VERYBEST;
            // DWORD dwQuality = VCH_ID_BEST;
            //DWORD dwQuality = VCH_ID_MED;
            //DWORD dwQuality = VCH_ID_LOW;
            //DWORD dwQuality = VCH_ID_VERYLOW;

            CMem mem;

            char szTemp[64];
#if 1
            DWORD dwTime = GetTickCount ();
            VoiceChatCompress (dwQuality, m_pWave, NULL, &mem);
            sprintf (szTemp, "\r\nCompress time = %d", (int)GetTickCount() - (int)dwTime);
            OutputDebugString (szTemp);
            VoiceChatDeCompress ((PBYTE)mem.p, (DWORD)mem.m_dwCurPosn, m_pWave, NULL, NULL, NULL);
#endif
#if 0
            // to test streaming
            CM3DWave     Wave;
            CListFixed lEnergy;
            BOOL fHavePreviouslySent = FALSE;
            lEnergy.Init (sizeof(fp));
            Wave.ConvertSamplesAndChannels (m_pWave->m_dwSamplesPerSec, m_pWave->m_dwChannels, NULL);

            while (m_pWave->m_dwSamples) {
               DWORD dwCopyOver = (DWORD)(rand() % 100) + 50;
               dwCopyOver = min(dwCopyOver, m_pWave->m_dwSamples);

               PCM3DWave pCut = m_pWave->Copy (0, dwCopyOver);
               m_pWave->ReplaceSection (0, dwCopyOver, NULL);
               Wave.ReplaceSection (Wave.m_dwSamples, Wave.m_dwSamples, pCut);
               delete pCut;

               // see if a new wave can be cut out
               pCut = VoiceChatStream (&Wave, &lEnergy,fHavePreviouslySent);
               if (!pCut)
                  continue;   // nothing
               fHavePreviouslySent = TRUE;   // remember that have sent

               // compress this
               if (!VoiceChatCompress (dwQuality, pCut, NULL, &mem)) {
                  delete pCut;
                  continue;
               }
               delete pCut;
            } // while samples in original

            // if anything is left over in Wave then add that
            if (Wave.m_dwSamples)
               VoiceChatCompress (dwQuality, &Wave, NULL, &mem);

            // loop and decompress
            DWORD dwCur = 0;
            while (dwCur < mem.m_dwCurPosn) {
               // VoiceChatRandomize ((PBYTE)mem.p + dwCur, mem.m_dwCurPosn-dwCur);
               DWORD dwUsed = VoiceChatDeCompress ((PBYTE)mem.p + dwCur, mem.m_dwCurPosn-dwCur,
                  dwCur ? &Wave : m_pWave, NULL);
               dwCur += dwUsed;

               // if not first time around then append to this
               if (dwCur != dwUsed)
                  m_pWave->ReplaceSection (m_pWave->m_dwSamples, m_pWave->m_dwSamples, &Wave);
            } // while true
#endif // 0

            //char szTemp[64];
            sprintf (szTemp, "\r\nDatarate = %g kbyte/sec", (double)mem.m_dwCurPosn / (double)1000 /
               ((double)m_pWave->m_dwSamples / (double)m_pWave->m_dwSamplesPerSec));
            OutputDebugString (szTemp);
#endif // 0

            InvalidateDisplay();
         }
         return 0;
#endif // _DEBUG

      case ID_MISC_GENERATESINEWAVE:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXSINE, FXSinePage, &m_FXSine);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            // do the effect
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Generating sine...");
            pWave->FXSine (m_FXSine.fStart, m_FXSine.fEnd, m_FXSine.dwShape, m_FXSine.dwChannel, &Progress);
            FXPaste (pWave);
         }
         return 0;

      case ID_VOICEMODIFICATION_MONOTONE:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXMONOTONE, FXMonotonePage, &m_FXMonotone);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            // do the effect
            CProgress Progress;
            int iMin, iMax;
            iMin = min(m_iSelStart, m_iSelEnd);
            iMax = max(m_iSelStart, m_iSelEnd);
            if (iMin == iMax) {
               // if no selection then affect the entire wave
               iMin = m_iSelStart = 0;
               iMax = m_iSelEnd = (int)m_pWave->m_dwSamples;
               InvalidateDisplay();
            }
            iMin = max(iMin, 0);
            iMax = max(iMax, 0);
            iMin = min(iMin, (int)m_pWave->m_dwSamples);
            iMax = min(iMax, (int)m_pWave->m_dwSamples);

            Progress.Start (hWnd, "Generating monotone...");
            PCM3DWave pWave = m_pWave->Monotone (WAVECALC_SEGMENT, m_FXMonotone.fPitch, (DWORD)iMin, (DWORD)iMax, NULL, &Progress);
            if (!pWave)
               return 0;
            FXPaste (pWave);
         }
         return 0;

      case ID_VOLUME_ADVANCEDVOLUME:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXVolume, FXVolumePage, &m_FXVolume);
            if (!pszRet || _wcsicmp(pszRet, L"doeffect"))
               return 0;   // pressed cancel

            // do the effect
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Volume...");
            pWave->FXVolume (m_FXVolume.fStart, m_FXVolume.fEnd, &Progress);
            FXPaste (pWave);
         }
         return 0;

      case ID_STRANGE_REMOVEPHASE:
      case ID_STRANGE_SWAPPHASE:
      case ID_STRANGE_COMBTHROUGHFREQUENCIES:
      case ID_STRANGE_COMBTHROUGHFREQUENCIES2:
      case ID_STRANGE_INVERTFREQUENCIES:
      case ID_STRANGE_STRETCHFREQUENCIES:
      case ID_STRANGE_SHRINKFREQUENCIES:
         {
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Effect...");

            DWORD dwEffect;
            switch (LOWORD(wParam)) {
            default:
            case ID_STRANGE_REMOVEPHASE:
               dwEffect = 0;
               break;
            case ID_STRANGE_SWAPPHASE:
               dwEffect = 1;
               break;
            case ID_STRANGE_COMBTHROUGHFREQUENCIES:
               dwEffect = 2;
               break;
            case ID_STRANGE_COMBTHROUGHFREQUENCIES2:
               dwEffect = 6;
               break;
            case ID_STRANGE_INVERTFREQUENCIES:
               dwEffect = 3;
               break;
            case ID_STRANGE_STRETCHFREQUENCIES:
               dwEffect = 4;
               break;
            case ID_STRANGE_SHRINKFREQUENCIES:
               dwEffect = 5;
               break;
            }

            pWave->FXFrequency (dwEffect, &Progress);
            FXPaste (pWave);
         }
         return 0;
         

      case ID_VOLUME_INCREASE:
      case ID_VOLUME_DECREASE:
      case ID_VOLUME_MAKESILENT:
      case ID_VOLUME_FADEIN:
      case ID_VOLUME_FADEOUT:
      case ID_VOLUME_NORMALIZE:
      case ID_VOLUME_INVERTSIGN:
         {
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Volume...");

            fp fStart, fEnd;
            switch (LOWORD(wParam)) {
            default:
            case ID_VOLUME_INCREASE:
               fStart = fEnd = sqrt((fp)2);
               break;
            case ID_VOLUME_DECREASE:
               fStart = fEnd = 1.0 / sqrt((fp)2);
               break;
            case ID_VOLUME_MAKESILENT:
               fStart = fEnd = 0;
               break;
            case ID_VOLUME_FADEIN:
               fStart = 0;
               fEnd = 1;
               break;
            case ID_VOLUME_FADEOUT:
               fStart = 1;
               fEnd = 0;
               break;
            case ID_VOLUME_NORMALIZE:
               {
                  short sMax = pWave->FindMax();
                  sMax = max(1,sMax);
                  fStart = fEnd = 32767.0 / (fp)sMax;
               }
               break;
            case ID_VOLUME_INVERTSIGN:
               fStart = fEnd = -1;
               break;
            }

            pWave->FXVolume (fStart, fEnd, &Progress);
            FXPaste (pWave);
         }
         return 0;
         
      case ID_TEXT_TEXT:
         FXTTS (hWnd);
         return 0;

      case ID_TEXT_BATCHTEXT:
         FXTTSBatch (hWnd);
         return 0;

      case ID_TEXT_TRANSPLANTEDPROSODY:
         {
            CTTSTransPros TP;
            //wcscpy (TP.m_szTTS, m_szTTS);
            //wcscpy (TP.m_szVoiceFile, m_szTrainFile);

            UndoCache();
            UndoAboutToChange();

            DWORD dwRet;
            CMem mem;
            {
               CEscWindow cWindow;
               RECT r;
               DialogBoxLocation2 (hWnd, &r);
               cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);

               // UI
#if 0 // def _DEBUG // to test
               dwRet = TP.DialogQuick (&cWindow, L"Hello there %2 (am/are/was)", &mem);
#else
               dwRet = TP.Dialog (&cWindow, m_pWave, NULL, NULL, &mem);
#endif
            }

            // cache
            //wcscpy (m_szTTS, TP.m_szTTS);
            //wcscpy (m_szTrainFile, TP.m_szVoiceFile);

            // upodate UI
            m_iViewSampleLeft = m_iPlayLimitMin =m_iSelStart = m_iPlayCur = 0;
            m_iViewSampleRight = m_iPlayLimitMax = m_iSelEnd = (int)m_pWave->m_dwSamples;
            InvalidateDisplay();
            UpdateScroll();
            EnableButtonsThatNeedSel();
            UndoCache();

            // if user pressed next then copy the information to the clipboard
            if (dwRet != 2)
               return 0;
            CMem memAnsi;
            DWORD dwLen = (DWORD)wcslen((PWSTR)mem.p);
            if (!memAnsi.Required (dwLen*2+2))
               return 0;
            WideCharToMultiByte (CP_ACP, 0, (PWSTR)mem.p, -1, (PSTR)memAnsi.p, (DWORD)memAnsi.m_dwAllocated, 0, 0);
            dwLen = (DWORD)strlen((char*)memAnsi.p);

            // fill hmem
            HANDLE   hMem;
            hMem = GlobalAlloc (GMEM_MOVEABLE | GMEM_DDESHARE, dwLen+1);
            if (!hMem)
               return 0;
            strcpy ((char*) GlobalLock(hMem), (char*) memAnsi.p);
            GlobalUnlock (hMem);

            OpenClipboard (hWnd);
            EmptyClipboard ();
            SetClipboardData (CF_TEXT, hMem);
            CloseClipboard ();

            EscMessageBox (hWnd, ASPString(),
               L"The transplanted prosody text has been copied to the clipboard.",
               L"You can paste it into the text-to-speech engine's text to read. Make sure to set the \"tagged\" setting.",
               MB_ICONINFORMATION | MB_OK);
         }
         return 0;

      case ID_STEREOEFFECTS_SWAPCHANNELS:
         {
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Swapping channels...");

            pWave->FXSwapChannels (&Progress);
            FXPaste (pWave);
         }
         return 0;

      case ID_FILTERING_REMOVESUB:
      case ID_FILTERING_REMOVEDCOFFSET:
         {
#if 0 // def _DEBUG  // to test aligning PCM
            UndoCache();
            UndoAboutToChange();
            m_pWave->SRFEATUREAlignPCM (FALSE, NULL, NULL);
            InvalidateDisplay ();
            return 0;
#endif
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Removing DC offset...");


            pWave->FXRemoveDCOffset (LOWORD(wParam) == ID_FILTERING_REMOVESUB, &Progress);
            FXPaste (pWave);
         }
         return 0;

      case ID_SILENCE_TRIMSILENCEFROMSTARTANDEND:
         {
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Removing silence...");

            pWave->FXTrimSilence (&Progress);
            FXPaste (pWave);
         }
         return 0;

      case ID_FILTERING_BASSBOOST:
      case ID_FILTERING_BASSREDUCE:
      case ID_FILTERING_TREBLEBOOST:
      case ID_FILTERING_TREBLEREDUCE:
      case ID_FILTERING_OLD:
      case ID_FILTERING_CHEAPRADIO:
         {
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Filtering...");

            fp afFreqBand[4], afAmplify[4];
            DWORD dwTimeBand;
            DWORD dwBands;
            dwBands = 2;
            switch (LOWORD(wParam)) {
            case ID_FILTERING_BASSBOOST:
               afFreqBand[0] = 0;
               afFreqBand[1] = 300;
               afAmplify[0] = 2;
               afAmplify[1] = 1;
               break;
            case ID_FILTERING_BASSREDUCE:
               afFreqBand[0] = 0;
               afFreqBand[1] = 300;
               afAmplify[0] = 0;
               afAmplify[1] = 1;
               break;
            case ID_FILTERING_TREBLEBOOST:
               afFreqBand[0] = 2000;
               afFreqBand[1] = 8000;
               afAmplify[0] = 1;
               afAmplify[1] = 2;
               break;
            case ID_FILTERING_TREBLEREDUCE:
               afFreqBand[0] = 2000;
               afFreqBand[1] = 8000;
               afAmplify[0] = 1;
               afAmplify[1] = .5;
               break;
            case ID_FILTERING_OLD:
               dwBands = 4;
               afFreqBand[0] = 400;
               afFreqBand[1] = 500;
               afFreqBand[2] = 1000;
               afFreqBand[3] = 2000;
               afAmplify[0] = 0;
               afAmplify[1] = 1;
               afAmplify[2] = 1;
               afAmplify[3] = 0;
               break;
            default:
            case ID_FILTERING_CHEAPRADIO:
               dwBands = 4;
               afFreqBand[0] = 200;
               afFreqBand[1] = 300;
               afFreqBand[2] = 3000;
               afFreqBand[3] = 4000;
               afAmplify[0] = 0;
               afAmplify[1] = 1;
               afAmplify[2] = 1;
               afAmplify[3] = 0;
               break;
            }

            dwTimeBand = 0;
            pWave->Filter (dwBands, afFreqBand, 1, &dwTimeBand, afAmplify, &Progress);
            FXPaste (pWave);
         }
         return 0;

      case ID_SILENCE_NOISEREDUCTION:
      case ID_SILENCE_NOISEREDUCTIONSTRONG:
         {
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Reducing background hiss...");

            pWave->FXNoiseReduce (&Progress, LOWORD(wParam) == ID_SILENCE_NOISEREDUCTIONSTRONG);
            FXPaste (pWave);
         }
         return 0;

      case ID_STRANGE_REVERSE:
         {
            CProgress Progress;
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;
            Progress.Start (hWnd, "Reverse...");

            pWave->FXReverse (&Progress);
            FXPaste (pWave);
         }
         return 0;

      case IDC_SPEECHRECOG:
         {
            // might want to show phonemes if not already visible
            PWAVEPANE pwp;
            DWORD i;
            for (i = 0; i < m_lWAVEPANE.Num(); i++) {
               pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);
               if (pwp->dwType == WP_PHONEMES)
                  break;
            }
            if (i >= m_lWAVEPANE.Num())
               WndProc (hWnd, WM_COMMAND, ID_SHOWOTHERS_PHONEMES, 0);

            // also show words
            for (i = 0; i < m_lWAVEPANE.Num(); i++) {
               pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);
               if (pwp->dwType == WP_WORD)
                  break;
            }
            if (i >= m_lWAVEPANE.Num())
               WndProc (hWnd, WM_COMMAND, ID_SHOWOTHERS_WORDS, 0);

            // display recog page
            SpeechRecog (hWnd);
         }
         return 0;

      case IDC_LEXICON:
         {
            WCHAR szTemp[256];
            wcscpy (szTemp, m_szLexicon);
            if (MLexiconOpenDialog (hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), FALSE)) {
               wcscpy (m_szLexicon, szTemp);
               if (m_pLex)
                  MLexiconCacheClose (m_pLex);
               m_pLex = MLexiconCacheOpen (m_szLexicon, FALSE);
            }
         }
         return 0;

      case IDC_NEWPHONE:
         {
            BOOL fShift = (GetKeyState (VK_SHIFT) < 0);

            // range
            int iStart, iEnd;
            if (fShift) {
               iStart = m_iPlayLimitMin;
               iEnd = m_iPlayLimitMax;
            }
            else {
               iStart = min(m_iSelStart, m_iSelEnd);
               iEnd = max(m_iSelStart, m_iSelEnd);
            }
            if (iEnd <= iStart) {
               EscChime (ESCCHIME_WARNING);
               return TRUE;
            }

            // load it
            if (!m_pLex)
               m_pLex = MLexiconCacheOpen (m_szLexicon, FALSE);
            if (!m_pLex) {
               EscMessageBox (hWnd, ASPString(),
                  L"You must specify a lexicon file to use.",
                  L"A phoneme cannot be added without know what lexicon/lanuage to use. "
                  L"Press the \"Lexicon\" button to select a lexicon.",
                  MB_ICONEXCLAMATION | MB_OK);
               return 0;
            }


            // create a list of phonemes
            CListFixed lPhone;
            PLSORT pls;
            DWORD i;
            memset (&pls, 0, sizeof(pls));
            lPhone.Init (sizeof(PLSORT));
            lPhone.Required (m_pLex->PhonemeNum());
            for (i = 0; i < m_pLex->PhonemeNum(); i++) {
               pls.plPhone = m_pLex->PhonemeGetUnsort(i);
               if (!pls.plPhone)
                  continue;
               pls.dwColumn = pls.plPhone->bStress;
               lPhone.Add (&pls);
            }

            // refresh the stress info
            PPLSORT ppls;
            ppls = (PPLSORT) lPhone.Get(0);
            for (i = 0; i < lPhone.Num(); i++)
               if (ppls[i].plPhone->bStress && (ppls[i].plPhone->wPhoneOtherStress < lPhone.Num()))
                  ppls[ppls[i].plPhone->wPhoneOtherStress].dwColumn = 6;   // so unstressed
                     // BUGFIX - was 3

            // silence
            pls.dwColumn = 0;
            pls.plPhone = m_pLex->PhonemeGet(m_pLex->PhonemeSilence());
            if (pls.plPhone)
               lPhone.Add (&pls);

            qsort (lPhone.Get(0), lPhone.Num(), sizeof(PLSORT), PLSORTSort);


            // create the list
            HMENU hMenu;
            hMenu = CreatePopupMenu ();

            // consonants, then vowels, then misc
            DWORD dwGroup;
            DWORD dwFirstTime;
            for (dwGroup = 0; dwGroup < 7; dwGroup++) {  // BUFIX - Was 4, but increased because of number of stresses in chinese
               dwFirstTime = TRUE;

               for (i = 0; i < lPhone.Num(); i++) {
                  if (ppls[i].dwColumn != dwGroup)
                     continue;   // ignore

                  // make string
                  char szShort[16], szLong[30];
                  WideCharToMultiByte (CP_ACP, 0, ppls[i].plPhone->szPhoneLong, -1, szShort, sizeof(szShort), 0, 0);
                  strcpy (szLong, szShort);
                  if (ppls[i].plPhone->szSampleWord[0]) {
                     strcat (szLong, " - ");
                     WideCharToMultiByte (CP_ACP, 0, ppls[i].plPhone->szSampleWord, -1,
                        szLong + strlen(szLong), sizeof(szLong) - (DWORD)strlen(szLong), 0, 0);
                  }

                  DWORD dwFlags;
                  if (dwFirstTime && dwGroup)
                     dwFlags = MF_MENUBARBREAK;
                  else
                     dwFlags = 0;
                  dwFirstTime = FALSE;
                  AppendMenu (hMenu, dwFlags | MF_ENABLED | MF_STRING, 1 + i, szLong);
               } // i
            } // dwGroup

            // show the menu
            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hMenu, TPM_CENTERALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);
            iRet--;  // since added one
            DestroyMenu (hMenu);

            // act
            if ((iRet < 0) || ((DWORD)iRet >= lPhone.Num()))
               return 0;

            // get the phoneme
            PLEXPHONE plp = ppls[iRet].plPhone;

            // Remember this for undo
            UndoCache();
            UndoAboutToChange();

            // add
            CListFixed lAdd;
            WVPHONEME wp;
            memset (&wp, 0, sizeof(wp));
            memcpy (wp.awcNameLong, plp->szPhoneLong, min(wcslen(plp->szPhoneLong) * sizeof(WCHAR),sizeof(wp.awcNameLong)));
            wp.dwSample = 0;
            wp.dwEnglishPhone = plp->bEnglishPhone;
            lAdd.Init (sizeof(WVPHONEME), &wp, 1);
            m_pWave->PhonemeDelete ((DWORD)iStart, (DWORD)iEnd);
            m_pWave->PhonemeInsert ((DWORD)iStart, (DWORD)(iEnd - iStart), 0, &lAdd);

            // might want to show phonemes if not already visible
            PWAVEPANE pwp;
            for (i = 0; i < m_lWAVEPANE.Num(); i++) {
               pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);
               if (pwp->dwType == WP_PHONEMES)
                  break;
            }
            if (i >= m_lWAVEPANE.Num())
               WndProc (hWnd, WM_COMMAND, ID_SHOWOTHERS_PHONEMES, 0);

            // update
            InvalidateDisplay();
         }
         return 0;

      case IDC_NEWWORD:
         {
            BOOL fShift = (GetKeyState (VK_SHIFT) < 0);

            // range
            int iStart, iEnd;
            if (fShift) {
               iStart = m_iPlayLimitMin;
               iEnd = m_iPlayLimitMax;
            }
            else {
               iStart = min(m_iSelStart, m_iSelEnd);
               iEnd = max(m_iSelStart, m_iSelEnd);
            }
            if (iEnd <= iStart) {
               EscChime (ESCCHIME_WARNING);
               return TRUE;
            }

            // show the window
            BYTE abTemp[256];
            PWVWORD pw = (PWVWORD)&abTemp[0];
            PWSTR pszWord = (PWSTR)(pw+1);
            pszWord[0] = 0;
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation2 (hWnd, &r);
            cWindow.Init (ghInstance, m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLWORDRENAME, WordRenamePage, pszWord);
            if (!pszRet || !_wcsicmp(pszRet, L"cancel"))
               return TRUE;

            // Remember this for undo
            UndoCache();
            UndoAboutToChange();

            // add
            CListVariable lAdd;
            pw->dwSample = 0;
            lAdd.Add (pw, sizeof(WVWORD) + sizeof(WCHAR)*(wcslen(pszWord)+1));
            m_pWave->WordDelete ((DWORD)iStart, (DWORD)iEnd);
            m_pWave->WordInsert ((DWORD)iStart, (DWORD)(iEnd - iStart), 0, &lAdd);

            // might want to show phonemes if not already visible
            PWAVEPANE pwp;
            DWORD i;
            for (i = 0; i < m_lWAVEPANE.Num(); i++) {
               pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);
               if (pwp->dwType == WP_WORD)
                  break;
            }
            if (i >= m_lWAVEPANE.Num())
               WndProc (hWnd, WM_COMMAND, ID_SHOWOTHERS_WORDS, 0);

            // update
            InvalidateDisplay();
         }
         return 0;

      case IDC_FX:
         {
            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUFX));
            HMENU hSub = GetSubMenu(hMenu,0);

            // Disable swap channels if only mono
            if (m_pWave->m_dwChannels < 2) {
               EnableMenuItem (hSub, ID_STEREOEFFECTS_SWAPCHANNELS, MF_BYCOMMAND |
                  MF_DISABLED | MF_GRAYED);
            }

#ifdef _DEBUG
            AppendMenu (hSub, MF_STRING, ID_VOICEMODIFICATION_VOICECHAT, "Test compress");
#endif

            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);

            if (iRet)
               PostMessage (hWnd, WM_COMMAND, iRet, 0);
         }
         return 0;

      case IDC_SHOWOTHERS:
         {
            // append menu for "show me"
            HMENU hMenu = LoadMenu (ghInstance, MAKEINTRESOURCE(IDR_MENUSHOWOTHERS));
            HMENU hSub = GetSubMenu(hMenu,0);

            if (m_hWndMouth)
               CheckMenuItem (hSub, ID_SHOWOTHERS_MOUTH, MF_BYCOMMAND | MF_CHECKED);

            // check items that appear
            PWAVEPANE pwp;
            DWORD i;
            for (i = 0; i < m_lWAVEPANE.Num(); i++) {
               pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);

               switch (pwp->dwType) {
               case WP_ENERGY:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_ENERGY, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_PHONEMES:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_PHONEMES, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_WORD:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_WORDS, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_PITCH:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_PITCH, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_WAVE:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_WAVEFORM, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_FFT:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_SPECTROGRAM, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_FFTFREQ:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_SPECTROGRAMOFSPECTROGRAM, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_FFTSTRETCH:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_SPECTROGRAMNOPITCH, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_FFTSTRETCHEXP:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_SPECTROGRAMOCTAVES, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_SRFEATURES:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_SPECTROGRAMSRFEAT, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_SRFEATURESVOICED:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_SPECTROGRAMSRFEATVOICED, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_SRFEATURESNOISE:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_SPECTROGRAMSRFEATNOISE, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_SRFEATURESPHASE:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_SPEECHRECOGNITIONFEATPHASE, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_SRFEATURESPHASEPITCH:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_SPEECHRECOGNITIONFEATPHASEPITCH, MF_BYCOMMAND | MF_CHECKED);
                  break;
               case WP_SRFEATURESPCM:
                  CheckMenuItem (hSub, ID_SHOWOTHERS_SPEECHRECOGNITIONFEATURES, MF_BYCOMMAND | MF_CHECKED);
                  break;
               }
            }


            POINT p;
            GetCursorPos (&p);
            int iRet;
            iRet = TrackPopupMenu (hSub, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
               p.x, p.y, 0, m_hWnd, NULL);

            DestroyMenu (hMenu);

            if (iRet)
               PostMessage (hWnd, WM_COMMAND, iRet, 0);
         }
         return 0;

      case IDC_SHOWMOUTH:
      case ID_SHOWOTHERS_MOUTH:
         if (m_hWndMouth) {
            DestroyWindow (m_hWndMouth);
         }
         else {
            RECT r;
            GetWindowRect (m_hWnd, &r);
            r.left = r.right - 160;
            r.top = r.bottom - 120;
            m_hWndMouth = CreateWindowEx (WS_EX_APPWINDOW | WS_EX_PALETTEWINDOW,
               "CWaveViewMouth", "Mouth", WS_SIZEBOX | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
               r.left, r.top, r.right - r.left, r.bottom - r.top, NULL, NULL, ghInstance, this);
         }
         return 0;

      case IDC_SHOWWAVE:
      case IDC_SHOWFFT:
      case IDC_SHOWPHONEMES:
      case ID_SHOWOTHERS_SPEECHRECOGNITIONPHONEMESCORES:
      case ID_SHOWOTHERS_SPECTROGRAMSRFEAT:
      case ID_SHOWOTHERS_SPECTROGRAMSRFEATVOICED:
      case ID_SHOWOTHERS_SPECTROGRAMSRFEATNOISE:
      case ID_SHOWOTHERS_SPEECHRECOGNITIONFEATPHASE:
      case ID_SHOWOTHERS_SPEECHRECOGNITIONFEATPHASEPITCH:
      case ID_SHOWOTHERS_SPEECHRECOGNITIONFEATURES:
      case ID_SHOWOTHERS_SPECTROGRAMOCTAVES:
      case ID_SHOWOTHERS_SPECTROGRAMOFSPECTROGRAM:
      case ID_SHOWOTHERS_SPECTROGRAMNOPITCH:
      case ID_SHOWOTHERS_SPECTROGRAM:
      case ID_SHOWOTHERS_ENERGY:
      case ID_SHOWOTHERS_PHONEMES:
      case ID_SHOWOTHERS_WORDS:
      case ID_SHOWOTHERS_WAVEFORM:
      case ID_SHOWOTHERS_PITCH:
         {
            // see what's visible
            BOOL fShowWave, fShowAll;
            PWAVEPANE pwp;
            DWORD i;
            fShowWave = fShowAll = FALSE;
            DWORD dwField, dwField2;
            dwField2 = -1;
            switch (LOWORD(wParam)) {
            default:
            case IDC_SHOWWAVE:
            case ID_SHOWOTHERS_WAVEFORM:
               dwField = WP_WAVE;
               break;
            case IDC_SHOWFFT:
            case ID_SHOWOTHERS_SPECTROGRAM:
               dwField = WP_FFT;
               break;
            case ID_SHOWOTHERS_SPECTROGRAMOFSPECTROGRAM:
               dwField = WP_FFTFREQ;
               break;
            case ID_SHOWOTHERS_SPECTROGRAMNOPITCH:
               dwField = WP_FFTSTRETCH;
               break;
            case ID_SHOWOTHERS_SPECTROGRAMOCTAVES:
               dwField = WP_FFTSTRETCHEXP;
               break;
            case ID_SHOWOTHERS_SPECTROGRAMSRFEAT:
               dwField = WP_SRFEATURES;
               break;
            case ID_SHOWOTHERS_SPECTROGRAMSRFEATVOICED:
               dwField = WP_SRFEATURESVOICED;
               break;
            case ID_SHOWOTHERS_SPECTROGRAMSRFEATNOISE:
               dwField = WP_SRFEATURESNOISE;
               break;
            case ID_SHOWOTHERS_SPEECHRECOGNITIONFEATPHASE:
               dwField = WP_SRFEATURESPHASE;
               break;
            case ID_SHOWOTHERS_SPEECHRECOGNITIONFEATPHASEPITCH:
               dwField = WP_SRFEATURESPHASEPITCH;
               break;
            case ID_SHOWOTHERS_SPEECHRECOGNITIONFEATURES:
               dwField = WP_SRFEATURESPCM;
               break;
            case ID_SHOWOTHERS_ENERGY:
               dwField = WP_ENERGY;
               break;
            case IDC_SHOWPHONEMES:
               dwField = WP_PHONEMES;
               dwField2 = WP_WORD;
               break;
            case ID_SHOWOTHERS_PHONEMES:
               dwField = WP_PHONEMES;
               break;
            case ID_SHOWOTHERS_WORDS:
               dwField = WP_WORD;
               break;
            case ID_SHOWOTHERS_PITCH:
               dwField = WP_PITCH;
               break;
            }

            for (i = 0; i < m_lWAVEPANE.Num(); i++) {
               pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);
               if (pwp->dwType != dwField)
                  continue;
               fShowWave = TRUE;
               fShowAll = (pwp->dwSubType == -1);
            }
            if ((m_pWave->m_dwChannels < 2) || (dwField == WP_PHONEMES)  || (dwField == WP_WORD))
               fShowAll = FALSE; // only one pane

            // if wasn't showing then add to show all
            if (!fShowWave) {
               WavePaneAdd (dwField, -1, TRUE);
               if (dwField2 != -1)
                  WavePaneAdd (dwField2, -1, TRUE);
               return 0;
            }

            // remove all
            for (i = m_lWAVEPANE.Num()-1; i < m_lWAVEPANE.Num(); i--) {
               pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);
               if ((pwp->dwType == dwField) || (pwp->dwType == dwField2))
                  WavePaneRemove (i, !fShowAll);
            }
            if (!fShowAll)
               return 0;   // wanted to remove all, and did so, so done

            // else, want to add for each of the channels
            for (i = 0; i < m_pWave->m_dwChannels; i++) {
               WavePaneAdd (dwField, i, (i+1 == m_pWave->m_dwChannels));

               if (dwField2 != -1)
                  WavePaneAdd (dwField2, i, (i+1 == m_pWave->m_dwChannels));
            }
         }
         return 0;


      case IDC_ANIMJUMPSTART:
      case IDC_ANIMJUMPEND:
         {
            BOOL fPlaying = (m_hPlayWaveOut ? TRUE : FALSE);

            // if playing then temporarily stop
            if (fPlaying)
               Stop ();

            m_iPlayCur = (LOWORD(wParam) == IDC_ANIMJUMPSTART) ? m_iPlayLimitMin : m_iPlayLimitMax;

            if (fPlaying) {
               if ((LOWORD(wParam) == IDC_ANIMJUMPSTART) || m_fPlayLooped)
                  Play (hWnd, m_fPlayLooped);
            }
            else {
               // update slider location
               TimelineUpdate();
            }
         }
         return 0;


      case IDC_ANIMRECORD:
         Record (m_hWnd);
         return 0;

      case IDC_ANIMPLAY:
         Play (hWnd, FALSE);
         return 0;

      case IDC_ANIMPLAYSEL:
         // update the play position
         if (m_iSelStart != m_iSelEnd) {
            m_iPlayLimitMin = min(m_iSelStart, m_iSelEnd);
            m_iPlayLimitMax = max(m_iSelStart, m_iSelEnd);

            m_iPlayCur = m_iPlayLimitMin;
            UpdateScroll();
         }

         Play (hWnd, FALSE);
         return 0;

      case IDC_ANIMPLAYLOOP:
         Play (hWnd, TRUE);
         return 0;

      case IDC_HELPBUTTON:
         {
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation (hWnd, &r);

            cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLHELP, HelpPage, this);

            return TRUE;
         }
         return 0;

      case IDC_NEW:
         {
            // If dirty then ask if want to save
            if (m_pWave->m_fDirty) {
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

            m_iSelEnd = m_iSelStart = 0;
            m_pWave->New();
            SetTitle();
            RebuildPanes ();
            UndoClear ();
            EnableButtonsThatNeedSel();

            // pull up UI for new sampling rate
            Settings (hWnd);

            // store new sampling rate in registry
            m_pWave->DefaultWFEXSet ((PWAVEFORMATEX) m_pWave->m_memWFEX.p);
         }
         return 0;

      case IDC_SAVE:
         Save (hWnd);
         return 0;

      case IDC_SETTINGS:
         Stop();
         Settings (hWnd);
         return 0;

      case IDC_SAVEAS:
         Save (hWnd, TRUE);
         return 0;

      case IDC_OPEN:
         Stop();
         Open (hWnd);
         return 0;

      case IDC_ZOOMALL:
         // reset the wave selection
         m_iViewSampleLeft = 0;
         m_iViewSampleRight = (int)m_pWave->m_dwSamples+1;  // just a bit extra
         m_iPlayLimitMin = 0;
         m_iPlayLimitMax = (int)m_pWave->m_dwSamples;
         m_iPlayCur = min(m_iPlayCur, m_iPlayLimitMax);

         UpdateScroll();
         return 0;

      case IDC_ZOOMSEL:
         if (m_iSelStart == m_iSelEnd) {
            m_iViewSampleLeft = 0;
            m_iViewSampleRight = (int)m_pWave->m_dwSamples+1;  // just a bit extra
         }
         else {
            m_iViewSampleLeft = min(m_iSelStart, m_iSelEnd);
            m_iViewSampleRight = max(m_iSelStart, m_iSelEnd);
         }

         UpdateScroll();
         return 0;

      case IDC_SELREGION:
      case IDC_ZOOMIN:
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

      case IDC_DELETEBUTTON:
         if (GetKeyState (VK_CONTROL) < 0) {
            PCM3DWave pWave = FXCopy ();
            if (!pWave)
               return 0;

            m_iSelStart = 0;
            m_iSelEnd = m_pWave->m_dwSamples;

            FXPaste (pWave);
         }
         else
            ReplaceRegion (NULL);
         return 0;

      case IDC_COPYBUTTON:
         Copy ();
         return 0;

      case IDC_CUTBUTTON:
         Copy ();
         ReplaceRegion (NULL);
         return 0;

      case IDC_PASTEBUTTON:
         Paste();
         return 0;

      case IDC_PASTEMIX:
         PasteMix();
         return 0;

      case IDC_SELALL:
         m_iSelStart = 0;
         m_iSelEnd = (int)m_pWave->m_dwSamples;
         InvalidateDisplay();
         UpdateScroll();
         EnableButtonsThatNeedSel();
         return 0;

      case IDC_SELPLAYBACK:
         if (GetKeyState (VK_SHIFT) < 0) {
            if (m_iSelStart == m_iSelEnd)
               return 0;   // do nothing

            m_iPlayLimitMin = min(m_iSelStart, m_iSelEnd);
            m_iPlayLimitMax = max(m_iSelStart, m_iSelEnd);

            if (m_hPlayWaveOut) {
               Stop ();
               m_iPlayCur = max(m_iPlayCur, m_iPlayLimitMin);
               m_iPlayCur = min(m_iPlayCur, m_iPlayLimitMax);
               Play (hWnd, m_fPlayLooped);
            }
            else {
               m_iPlayCur = max(m_iPlayCur, m_iPlayLimitMin);
               m_iPlayCur = min(m_iPlayCur, m_iPlayLimitMax);
            }
            UpdateScroll();
         }
         else {
            m_iSelStart = m_iPlayLimitMin;
            m_iSelEnd = m_iPlayLimitMax;
            InvalidateDisplay();
            UpdateScroll();
            EnableButtonsThatNeedSel();
         }
         return 0;

      case IDC_SELNONE:
         m_iSelStart = m_iSelEnd = 0;
         InvalidateDisplay();
         UpdateScroll();
         EnableButtonsThatNeedSel();
         return 0;

#if 0 // FUTURERELEASE - Do brush stuff
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
#endif // 0
      }  // WM_COMMAND
      break;

   case WM_PAINT:
      return Paint (hWnd, uMsg, wParam, lParam);

   case WM_MOUSEWHEEL:
      {
         short zDelta = (short) HIWORD(wParam); 
         BOOL fZoomIn = (zDelta >= 0);

         //  handle zoom for mouse wheel
         int iCenter, iWidth, iWidthOld;
         iCenter = (m_iViewSampleRight + m_iViewSampleLeft) / 2;
         iWidth = m_iViewSampleRight - m_iViewSampleLeft;
         iWidthOld = iWidth;

         if (fZoomIn)
            iWidth = (int)((double)iWidth / sqrt((fp)2));
         else {
            iWidth = (int)((double)iWidth * sqrt((fp)2));
            if (iWidth == iWidthOld)
               iWidth++;
         }

         iWidth = max(iWidth, 1);   // at least one
         iWidth = min(iWidth, (int)m_pWave->m_dwSamples+1);
         m_iViewSampleLeft = iCenter - iWidth/2;
         m_iViewSampleLeft = max(m_iViewSampleLeft, 0);
         m_iViewSampleRight = m_iViewSampleLeft + iWidth;

         // redraw
         UpdateScroll ();
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

         // else keys
         switch (wParam) {
         case VK_LEFT:
            return WndProc (hWnd, WM_HSCROLL, SB_LINEUP, (LPARAM) m_hWndScroll);
         case VK_RIGHT:
            return WndProc (hWnd, WM_HSCROLL, SB_LINEDOWN, (LPARAM) m_hWndScroll);
         case VK_PRIOR:
            return WndProc (hWnd, WM_HSCROLL, SB_PAGELEFT, (LPARAM) m_hWndScroll);
         case VK_NEXT:
            return WndProc (hWnd, WM_HSCROLL, SB_PAGERIGHT, (LPARAM) m_hWndScroll);
         case VK_HOME:
            {
               int iSize = m_iViewSampleRight - m_iViewSampleLeft;
               m_iViewSampleLeft = 0;
               m_iViewSampleRight = iSize;
               UpdateScroll ();
               InvalidateDisplay();
            }
            return 0;
         case VK_END:
            {
               int iSize = m_iViewSampleRight - m_iViewSampleLeft;
               m_iViewSampleRight = (int)m_pWave->m_dwSamples+1;
               m_iViewSampleLeft = m_iViewSampleRight - iSize;
               UpdateScroll ();
               InvalidateDisplay();
            }
            return 0;
         }

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
         double fScale;
         int iView;
         if (m_pWave->m_dwSamples)
            fScale = (fp)SCROLLSCALE / (fp)m_pWave->m_dwSamples;
         else
            fScale = SCROLLSCALE;
         iView = m_iViewSampleRight - m_iViewSampleLeft;
         m_iViewSampleLeft = (int)((double) si.nPos / fScale);
         m_iViewSampleRight = m_iViewSampleLeft + iView;
         UpdateScroll ();

         return 0;
      }
      break;

   case WM_SIZE:
      {
         // called when the window is sized. Move the button bars around
         int iWidth = LOWORD(lParam);
         int iHeight = HIWORD(lParam);

         m_fSmallWindow = ((iWidth <= 500) || (iHeight <= 300));

         if (!m_hWnd)
            m_hWnd = hWnd; // so calcthumbnail loc works
         RECT  r, rOrig;
         GetClientRect (hWnd, &rOrig);
         rOrig.top += VARBUTTONSIZE;
         rOrig.bottom -= VARBUTTONSIZE;
         rOrig.left += VARBUTTONSIZE;  // some space on left
         rOrig.right -= VARBUTTONSIZE;  // some space on right

         int iScroll, iHScroll;
         iScroll = 16; // BUGIX - For some reason this isnt linking in: GetSystemMetrics (SM_CXVSCROLL);
         iHScroll = iScroll;

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

         // indent left and right
         rOrig.left += 4;
         rOrig.right -= 4;

         // amount allocated to Data window
         int iDataWidth;
         iDataWidth = (rOrig.right - rOrig.left);

         // time bar on top
         r = rOrig;
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

         // draw the wave in-between
         int iAvail;
         rOrig = r;
         iAvail = max(r.bottom - r.top, 0);

         // how much is used by fixed...
         PWAVEPANE pwp;
         int iFixed, iVar;
         iFixed = iVar = 0;
         DWORD i;
         for (i = 0; i < m_lWAVEPANE.Num(); i++) {
            pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);
            if (pwp->iHeight > 0)
               iFixed += pwp->iHeight;
            else
               iVar += -pwp->iHeight;
         }

         // loop again, moving
         int iCur;
         iCur = r.top;
         iAvail = max(iAvail - iFixed, 0);
         for (i = 0; i < m_lWAVEPANE.Num(); i++) {
            pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);

            if (pwp->iHeight > 0)
               iHeight = pwp->iHeight;
            else if (iVar)
               iHeight = -pwp->iHeight * iAvail / iVar;
            else
               iHeight = 0;   // shouldnt happen
            MoveWindow (pwp->hWnd, r.left, iCur,
               r.right - r.left, iHeight, TRUE);
            iCur += iHeight;
         }  // i

      }
      break;

   case WM_CLOSE:
      {
         // If dirty then ask if want to save
         if (m_pWave->m_fDirty) {
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




/*********************************************************************************
CWaveView::PointInImage - Given a pixel in the map's client rectangle, this
fills in pfImageX and pfImageY with the x and y ground-pixel locations.

inputs
   int         iX, iY - Point in map client's rectangle
   fp          *pfImageX, *pfImageY - Filled in with ground-pixel
returns
   BOOL - TRUE if over the ground
*/
BOOL CWaveView::PointInImage (PWAVEPANE pwp, int iX, int iY, double *pfImageX, double *pfImageY)
{
   if (pfImageX || pfImageY) {
      RECT r;
      GetClientRect (pwp->hWnd, &r);
      if ((r.right <= r.left) || (r.bottom <= r.top))
         return FALSE;

      if (pfImageX)
         *pfImageX = (double)(iX - r.left) / (double)(r.right - r.left) *
            (double)(m_iViewSampleRight - m_iViewSampleLeft) + (double)m_iViewSampleLeft;
      if (pfImageY)
         *pfImageY = (double)(iY - r.top) / (double)(r.bottom - r.top) *
            (pwp->fBottom - pwp->fTop);
   }

   // FUTURERELEASE - handle point in image
   return TRUE;
}



/*****************************************************************************
CWaveView::UndoClear - Clears out undo and redo
*/
void CWaveView::UndoClear (BOOL fRefreshButtons)
{
   UndoCache ();

   // free up undo and redo
   PWVSTATE ps;
   DWORD i;
   ps = (PWVSTATE) m_lWVSTATEUndo.Get(0);
   for (i = 0; i < m_lWVSTATEUndo.Num(); i++, ps++)
      delete ps->pWave;
   m_lWVSTATEUndo.Clear ();

   ps = (PWVSTATE) m_lWVSTATERedo.Get(0);
   for (i = 0; i < m_lWVSTATERedo.Num(); i++, ps++)
      delete ps->pWave;
   m_lWVSTATERedo.Clear();

   if (fRefreshButtons)
      UndoUpdateButtons ();
}

/*****************************************************************************
CWaveView::UndoUpdateButtons - Update the undo/redo buttons based on their
current state.
*/
void CWaveView::UndoUpdateButtons (void)
{
   BOOL fUndo = (m_lWVSTATEUndo.Num() ? TRUE : FALSE);
   BOOL fRedo = (m_lWVSTATERedo.Num() ? TRUE : FALSE);

   m_pUndo->Enable (fUndo);
   m_pRedo->Enable (fRedo);
}

/*****************************************************************************
CWaveView::UndoAboutToChange - Call this before an object is about to change.
It will remember the current state for undo, and update the buttons.
Also sets dirty flag
*/
void CWaveView::UndoAboutToChange (void)
{
   m_pWave->m_fDirty = TRUE;

   // cant be working with undo if there's something there
   if (!m_lWVSTATEUndo.Num())
      m_fWorkingWithUndo = FALSE;

   // if already have something no change
   if (m_fWorkingWithUndo)
      return;

   // else, remember
   WVSTATE gs;
   memset (&gs, 0, sizeof(gs));
   gs.pWave = m_pWave->Clone();
   if (!gs.pWave)
      return;
   gs.iPlayCur = m_iPlayCur;
   gs.iPlayLimitMax = m_iPlayLimitMax;
   gs.iPlayLimitMin = m_iPlayLimitMin;
   gs.iViewSampleLeft = m_iViewSampleLeft;
   gs.iViewSampleRight = m_iViewSampleRight;
   gs.iSelStart = m_iSelStart;
   gs.iSelEnd = m_iSelEnd;
   m_lWVSTATEUndo.Add (&gs);

   // if there are too many undo states remembered then toss out
   PWVSTATE ps;
   if (m_lWVSTATEUndo.Num() >= 5) {
      ps = (PWVSTATE) m_lWVSTATEUndo.Get(0);
      delete ps->pWave;
      m_lWVSTATEUndo.Remove(0);
   }

   // clear out the redo
   ps = (PWVSTATE) m_lWVSTATERedo.Get(0);
   DWORD i;
   for (i = 0; i < m_lWVSTATERedo.Num(); i++, ps++)
      delete ps->pWave;
   m_lWVSTATERedo.Clear();

   // update buttons
   m_fWorkingWithUndo = TRUE;
   UndoUpdateButtons ();
}

/*****************************************************************************
CWaveView::UndoCache - If were working with an undo buffer then set m_fWorkingWithUNdo
to FALSE so that any other changes will be cached.
*/
void CWaveView::UndoCache (void)
{
   m_fWorkingWithUndo = FALSE;
}


/*****************************************************************************
CWaveView::Undo - Does an undo or redo

inputs
   BOOL     fUndo - If TRUE do an undo, else do a redo
returns
   BOOL - TRUE if success
*/
BOOL CWaveView::Undo (BOOL fUndo)
{
   WVSTATE gs;
   PCM3DWave pOld;
   memset (&gs, 0, sizeof(gs));
   gs.pWave = pOld = m_pWave;
   gs.iPlayCur = m_iPlayCur;
   gs.iPlayLimitMax = m_iPlayLimitMax;
   gs.iPlayLimitMin = m_iPlayLimitMin;
   gs.iViewSampleLeft = m_iViewSampleLeft;
   gs.iViewSampleRight = m_iViewSampleRight;
   gs.iSelStart = m_iSelStart;
   gs.iSelEnd = m_iSelEnd;

   PWVSTATE ps;
   if (fUndo) {
      // doing an undo
      if (!m_lWVSTATEUndo.Num())
         return FALSE;  // nothing to undo

      // add current state to redo
      m_lWVSTATERedo.Add (&gs);

      // get last undo
      ps = (PWVSTATE) m_lWVSTATEUndo.Get(m_lWVSTATEUndo.Num()-1);
      gs = *ps;
      m_lWVSTATEUndo.Remove (m_lWVSTATEUndo.Num()-1);
   }
   else {
      // doing a redi
      if (!m_lWVSTATERedo.Num())
         return FALSE;  // nothing to undo

      // add current state to undo
      m_lWVSTATEUndo.Add (&gs);

      // get last redo
      ps = (PWVSTATE) m_lWVSTATERedo.Get(m_lWVSTATERedo.Num()-1);
      gs = *ps;
      m_lWVSTATERedo.Remove (m_lWVSTATERedo.Num()-1);
   }

   // copy over
   m_pWave = gs.pWave;
   m_iPlayCur = gs.iPlayCur;
   m_iPlayLimitMax = gs.iPlayLimitMax;
   m_iPlayLimitMin = gs.iPlayLimitMin;
   m_iViewSampleLeft = gs.iViewSampleLeft;
   m_iViewSampleRight = gs.iViewSampleRight;
   m_iSelStart = gs.iSelStart;
   m_iSelEnd = gs.iSelEnd;


   m_pWave->m_fDirty = TRUE;  // always set dirty flag when undo/redo
   strcpy (m_pWave->m_szFile, pOld->m_szFile);  // transfer the current file name over

   // update displays
   UndoCache();
   InvalidateDisplay ();
   UndoUpdateButtons ();
   UpdateScroll ();
   EnableButtonsThatNeedSel();

   return TRUE;
}

/*****************************************************************************
CWaveView::Save - Saves the information back to the object it got it from.

inputs
   HWND     hWndParent - To show error messages on
   BOOL     fForeceNewName - The forces to ask a name even if there is one
returns
   BOOL - TRUE if saved, FALSE if error
*/
BOOL CWaveView::Save (HWND hWndParent, BOOL fForceNewName)
{
   if (!RegisterIsRegistered()) {
      EscMessageBox (hWndParent, ASPString(),
         L"Save is disabled for unregistered users.",
         L"You must register and pay for " APPSHORTNAMEW L" before you can save. "
         L"To register, run the main " APPSHORTNAMEW L" application and look in help.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

   if (!m_pWave->m_szFile[0])
      fForceNewName = TRUE;

   if (fForceNewName) {
      // save UI
      WCHAR szFile[256];
      MultiByteToWideChar (CP_ACP, 0, m_pWave->m_szFile, -1, szFile, sizeof(szFile)/sizeof(WCHAR));
      if (!WaveFileOpen (m_hWnd, TRUE, szFile))
         return FALSE;
      WideCharToMultiByte (CP_ACP, 0, szFile, -1, m_pWave->m_szFile, sizeof(m_pWave->m_szFile), 0, 0);
      SetTitle();
   }

   // try saving it
   BOOL fRet;
   {
      CProgress Progress;
      Progress.Start (hWndParent, "Saving file...");
      fRet = m_pWave->Save (FALSE, &Progress);
   }
   if (!fRet) {
      EscMessageBox (hWndParent, ASPString(),
         L"The wave file couldn't be saved.",
         L"The file name might be bad, or the file might be write protected.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }
   else
      MRUListAdd (m_pWave->m_szFile);

   UndoCache ();

   // BUGFIX - Play beep when save
   EscChime (ESCCHIME_INFORMATION);

   return TRUE;
}



/*****************************************************************************
CWaveView::Open - Opens a new file.

inputs
   HWND     hWndParent - To show error messages on
returns
   BOOL - TRUE if opened, FALSE if error
*/
BOOL CWaveView::Open (HWND hWndParent)
{
   if (m_pWave->m_fDirty) {
      int iRet;
      iRet = EscMessageBox (hWndParent, ASPString(),
         L"You have made changes to your current file. Do you want to save it?",
         NULL,
         MB_ICONQUESTION | MB_YESNOCANCEL);

      if (iRet == IDCANCEL)
         return FALSE;
      if (iRet == IDYES) {
         if (!Save (hWndParent))
            return FALSE;
      }
   }
   m_pWave->m_fDirty = FALSE; // so dont ask

   WCHAR szFile[256];
   char szaFile[256];
   szFile[0] = 0;
   if (!WaveFileOpen (hWndParent, FALSE, szFile))
      return FALSE;

   WideCharToMultiByte (CP_ACP, 0, szFile, -1, szaFile, sizeof(szaFile), 0, 0);

   
   // try to open it
   BOOL fRet;
   {
      CProgress Progress;
      Progress.Start (hWndParent, "Loading file...");
      fRet = m_pWave->Open (&Progress, szaFile);
   }
   if (!fRet) {
      m_pWave->New();   // so have something

      EscMessageBox (hWndParent, ASPString(),
         L"The wave file couldn't be opened.",
         L"It may not exist, or may not be a proper wave file. A new file will be created instead.",
         MB_ICONEXCLAMATION | MB_OK);
      // note: Dont return, fall through
   }
   else
      MRUListAdd (szaFile);

   // so playback will be entire wave
   m_iPlayLimitMin = m_iPlayCur = 0;
   m_iPlayLimitMax = (int)m_pWave->m_dwSamples;
   m_iSelEnd = m_iSelStart = 0;

   SetTitle();
   RebuildPanes();
   UndoClear();
   EnableButtonsThatNeedSel();
   return TRUE;
}


/*****************************************************************************
CWaveView::SetTitle - Sets the window title based on the file name
*/
void CWaveView::SetTitle (void)
{
   char szTemp[512];

   if (m_pWave->m_szFile[0])
      strcpy (szTemp, m_pWave->m_szFile);
   else
      strcpy (szTemp, "New file");

   strcat (szTemp, " - Audio Outside the Box");
   SetWindowText (m_hWnd, szTemp);
}


/**********************************************************************************
CWaveView::WavePaneClear - Clear all the wave panes

inputs
   BOOL     fRefresh - If TRUE then refresh the pane locations based on WM_SIZE
*/
void CWaveView::WavePaneClear (BOOL fRefresh)
{
   // free all the wave pane objects
   DWORD i;
   PWAVEPANE pwp;
   for (i = 0; i < m_lWAVEPANE.Num(); i++) {
      pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);
      if (pwp->hWnd)
         DestroyWindow (pwp->hWnd);
   }
   m_lWAVEPANE.Clear();

   if (fRefresh) {
      RECT r;
      GetClientRect (m_hWnd, &r);
      WndProc (m_hWnd, WM_SIZE, 0, MAKELPARAM (r.right-r.left,r.bottom-r.top));
   }
}


/**********************************************************************************
CWaveView::WavePaneRemove - removes a specific item

inputs
   DWORD    dwItem - Index into the wave pane list
   BOOL     fRefresh - If TRUE then refresh the pane locations based on WM_SIZE
*/
void CWaveView::WavePaneRemove (DWORD dwItem, BOOL fRefresh)
{
   // free all the wave pane objects
   PWAVEPANE pwp = (PWAVEPANE) m_lWAVEPANE.Get(dwItem);
   if (!pwp)
      return;  // not there
   if (pwp->hWnd)
      DestroyWindow (pwp->hWnd);
   m_lWAVEPANE.Remove (dwItem);

   if (fRefresh) {
      RECT r;
      GetClientRect (m_hWnd, &r);
      WndProc (m_hWnd, WM_SIZE, 0, MAKELPARAM (r.right-r.left,r.bottom-r.top));
   }
}

/**********************************************************************************
CWaveView::WavePaneUpdateScroll - Update the vertical scrollbar

inputs
   PWAVEPANE         pwp - Pane
returns
   none
*/
void CWaveView::WavePaneUpdateScroll (PWAVEPANE pwp)
{
   SCROLLINFO si;

   pwp->fTop = max (0, pwp->fTop);
   pwp->fTop = min (1 - CLOSE, pwp->fTop);
   pwp->fBottom = max(pwp->fTop + CLOSE, pwp->fBottom);
   pwp->fBottom = min(1, pwp->fBottom);

   memset (&si, 0, sizeof(si));
   si.cbSize = sizeof(si);
   si.fMask = SIF_ALL;
   si.nMax = 1000;
   si.nMin = 0;
   si.nPage = (DWORD) ((pwp->fBottom - pwp->fTop) * 1000.0);
   si.nPage = min(si.nPage, 1000);
   si.nPos = si.nTrackPos = (DWORD)(pwp->fTop * 1000.0);

   SetScrollInfo (pwp->hWnd, SB_VERT, &si, TRUE);
}

/**********************************************************************************
CWaveView::WavePaneAdd - Adds a wave pane.

inputs
   DWORD    dwType - Type of pane (WP_XXX)
   DWORD    dwSubType - Usually the channel. -1 if all channels
   BOOL     fRefresh - If TRUE then refresh the pane locations based on WM_SIZE
*/
void CWaveView::WavePaneAdd (DWORD dwType, DWORD dwSubType, BOOL fRefresh)
{
   WAVEPANE wp;
   memset (&wp, 0 ,sizeof(wp));

   wp.dwType = dwType;
   wp.dwSubType = dwSubType;
   wp.fBottom = 1;
   wp.fTop = 0;
   switch (dwType) {
   case WP_WAVE:
      wp.iHeight = -2;
      break;

   case WP_FFT:
   case WP_FFTSTRETCH:
      wp.iHeight = -2;
      wp.fTop = max(1.0 - 4000.0 / (fp)(m_pWave->m_dwSamplesPerSec/2), 0);
      break;

   case WP_FFTFREQ:
   case WP_FFTSTRETCHEXP:
   case WP_SRFEATURES:
   case WP_SRFEATURESVOICED:
   case WP_SRFEATURESNOISE:
   case WP_SRFEATURESPHASEPITCH:
      wp.iHeight = -2;
      break;

   case WP_SRFEATURESPHASE:
   case WP_SRFEATURESPCM:
      wp.iHeight = -1;
      break;

   case WP_ENERGY:
      wp.iHeight = -1;
      wp.fBottom = .5;
      break;

   case WP_PHONEMES:
   case WP_WORD:
      wp.iHeight = 24;
      break;

   case WP_PITCH:
      wp.iHeight = -1;
      wp.fTop = max(1.0 - 300.0 / (fp)(m_pWave->m_dwSamplesPerSec/4), 0);
      wp.fBottom = max(1.0 - 50.0 / (fp)(m_pWave->m_dwSamplesPerSec/4), 0);
         // BUGFIX - Using /4 for pitch because using /4 elsewhere
      break;

   default:
      wp.iHeight = -1;
      break;
   }
   wp.pWaveView = this;

   // add to the list without window
   m_lWAVEPANE.Add (&wp, sizeof(wp));

   // modify in place
   PWAVEPANE pwp;
   pwp = (PWAVEPANE) m_lWAVEPANE.Get(m_lWAVEPANE.Num()-1);

   pwp->hWnd = CreateWindowEx (WS_EX_CLIENTEDGE, "CWaveViewData", "",
         WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 0, 1, 1, m_hWnd, NULL, ghInstance, pwp);

   // Need to set scrollbar
   WavePaneUpdateScroll (pwp);

   if (fRefresh) {
      RECT r;
      GetClientRect (m_hWnd, &r);
      WndProc (m_hWnd, WM_SIZE, 0, MAKELPARAM (r.right-r.left,r.bottom-r.top));
   }
}

/**********************************************************************************
CWaveView::TimelineUpdate - Updates the display of the timeline so it's in sync
with internal variables.
*/
void CWaveView::TimelineUpdate (void)
{
   if (!m_pTimeline)
      return;

   m_iPlayLimitMax = min(m_iPlayLimitMax, (int)m_pWave->m_dwSamples);
   m_iPlayLimitMax = max(m_iPlayLimitMax, 1);
   m_iPlayLimitMin = min(m_iPlayLimitMin, m_iPlayLimitMax-1);
   m_iPlayLimitMax = min(m_iPlayLimitMax, (int)m_pWave->m_dwSamples);
   m_iPlayCur = max(m_iPlayCur, m_iPlayLimitMin);
   m_iPlayCur = min(m_iPlayCur, m_iPlayLimitMax);

   // get and set limits
   fp fLeft, fRight, fScene;
   m_pTimeline->LimitsGet (&fLeft, &fRight, &fScene);
   if ((fLeft != (fp) m_iPlayLimitMin) || (fRight != (fp)m_iPlayLimitMax))
      m_pTimeline->LimitsSet (m_iPlayLimitMin, m_iPlayLimitMax, fScene);

   // update play position as long as not playing
   if (!m_hPlayWaveOut && (m_pTimeline->PointerGet() != (fp)m_iPlayCur)) {
      m_pTimeline->PointerSet (m_iPlayCur);
      if (m_hWndMouth) {
         InvalidateRect (m_hWndMouth, NULL, FALSE);
         UpdateWindow (m_hWndMouth);
      }
   }
}

/**********************************************************************************
CWaveView::UpdateScroll - Call this when the number of samples has changed, or
when the viewport's left/right has changed
It sets some parameters in the playback slider and the scrollbar.
*/
void CWaveView::UpdateScroll (void)
{
   TimelineUpdate();

   // update horizontal scrollbar
   m_iViewSampleRight = min (m_iViewSampleRight, (int)m_pWave->m_dwSamples+1);
   m_iViewSampleLeft = min(m_iViewSampleRight-1, m_iViewSampleLeft);
   if (m_iViewSampleLeft < 0) {
      m_iViewSampleLeft = 0;
      m_iViewSampleRight = max(m_iViewSampleRight, m_iViewSampleLeft+1);
      m_iViewSampleRight = min (m_iViewSampleRight, (int)m_pWave->m_dwSamples+1);
   }

   // tell the timeline what left and right mean
   m_pTimeline->ScaleSet (m_iViewSampleLeft, m_iViewSampleRight);

   double fScale;
   if (m_pWave->m_dwSamples)
      fScale = (fp)SCROLLSCALE / (fp)m_pWave->m_dwSamples;
   else
      fScale = SCROLLSCALE;

   SCROLLINFO si;
   memset (&si, 0, sizeof(si));
   si.cbSize = sizeof(si);
   si.fMask = SIF_ALL;
   si.nMax = SCROLLSCALE;
   si.nMin = 0;
   si.nPage = (int) (fScale * (m_iViewSampleRight - m_iViewSampleLeft));
   si.nPos = (int) (m_iViewSampleLeft * fScale);
   si.nTrackPos = si.nPos;
   SetScrollInfo (m_hWndScroll, SB_CTL, &si, TRUE);

   InvalidateDisplay();
}

/**********************************************************************************
CWaveView::RebuildPanes - Rebuild all the panes for a new file
*/
void CWaveView::RebuildPanes (void)
{
   WavePaneClear(FALSE);
   WavePaneAdd (WP_WAVE, -1, TRUE);

   // BUGFIX - if have pitch, etc. then add these
   if (m_pWave->m_lWVPHONEME.Num())
      WavePaneAdd (WP_PHONEMES, -1, TRUE);
   if (m_pWave->m_lWVWORD.Num())
      WavePaneAdd (WP_WORD, -1, TRUE);
   if (m_pWave->m_dwSRSamples)
      WavePaneAdd (WP_SRFEATURES, -1, TRUE);
   if (m_pWave->m_adwPitchSamples[PITCH_F0])
      WavePaneAdd (WP_PITCH, -1, TRUE);

   // reset the wave selection
   m_iViewSampleLeft = 0;
   m_iViewSampleRight = (int)m_pWave->m_dwSamples+1;  // just a bit extra
   m_iSelStart = m_iSelEnd = 0;

   UpdateScroll();
   InvalidateDisplay();
}



/**********************************************************************************
CWaveView::PlayAddBuffer - Adds a buffer to the playlist. NOTE: This also increases
the number of buffers out counter.

inputs
   PWAVEHDR          pwh - Wave header to use. This is assumed to already be returned
   BOOL              fInCritSec - Set to TRUE if already in the crtiical section, m_csWaveOut
returns
   BOOL - TRUE if sent out. FALSE if didnt
*/
BOOL CWaveView::PlayAddBuffer (PWAVEHDR pwh, BOOL fInCritSec)
{
   if (!m_hPlayWaveOut || m_fPlayStopping)
      return FALSE;  // not opened

   // how much data do we need?
   DWORD dwLeft, dwCopy;
   short *psCopy;
   DWORD adwSyncInfo[2];
   dwLeft = m_dwPlayBufSize / (m_pWave->m_dwChannels * sizeof(short));
   pwh->dwBufferLength = 0;
   psCopy = (short*) pwh->lpData;

   while (dwLeft && (m_fPlayLooped || (m_iPlayCur < m_iPlayLimitMax))) {
      // loop around
      m_iPlayCur = max(m_iPlayCur, m_iPlayLimitMin);
      m_iPlayLimitMax = min(m_iPlayLimitMax, (int)m_pWave->m_dwSamples);
      if (m_iPlayCur >= m_iPlayLimitMax)
         m_iPlayCur = m_iPlayLimitMin;
      if (m_iPlayCur >= m_iPlayLimitMax)
         break;   // nothing to add

      dwCopy = min(dwLeft, (DWORD)m_iPlayLimitMax -(DWORD) m_iPlayCur);
      if (!dwCopy || !m_pWave->m_psWave)
         break;   // nothing to copy

      // remember this for sync info
      adwSyncInfo[0] = m_dwPlaySyncCount;
      adwSyncInfo[1] = (DWORD) m_iPlayCur;
      m_lPlaySync.Add (&adwSyncInfo[0]);

      // copy over
      memcpy (psCopy, m_pWave->m_psWave + (DWORD)m_iPlayCur * m_pWave->m_dwChannels,
         dwCopy * m_pWave->m_dwChannels * sizeof(short));
      pwh->dwBufferLength += dwCopy * m_pWave->m_dwChannels * sizeof(short);
      m_dwPlaySyncCount += dwCopy * m_pWave->m_dwChannels * sizeof(short);
      dwLeft -= dwCopy;
      psCopy += dwCopy * m_pWave->m_dwChannels;
      m_iPlayCur += (int) dwCopy;
   }

   // if nothing in buffer then return false
   if (!pwh->dwBufferLength)
      return FALSE;

   // else, add
   MMRESULT mm;
   m_dwPlayBufOut++;
   if (!fInCritSec)
      EnterCriticalSection (&m_csWaveOut);
   mm = waveOutWrite (m_hPlayWaveOut, pwh, sizeof(WAVEHDR));
   if (!fInCritSec)
      LeaveCriticalSection (&m_csWaveOut);
   if (mm) {
      m_dwPlayBufOut--;
      return FALSE;
   }

   // done
   return TRUE;
}


/**********************************************************************************
CWaveView::PlayCallback - Handles a call from the waveOutOpen function
*/
static void CALLBACK PlayCallback (
  HWAVEOUT hwo,       
  UINT uMsg,         
  DWORD_PTR dwInstance,  
  DWORD_PTR dwParam1,    
  DWORD_PTR dwParam2     
)
{
   PCWaveView pwv = (PCWaveView) dwInstance;
   // BUGFIX - Cant call callback here since hangs
   // pwv->PlayCallback (hwo, uMsg, dwParam1, dwParam2);
   PostMessage (pwv->m_hWnd, WM_MYWOMMESSAGE, uMsg, dwParam1);
}
void CWaveView::PlayCallback (HWAVEOUT hwo,UINT uMsg,         
  DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
   // do something with the buffer
   if (uMsg == MM_WOM_DONE) {
      EnterCriticalSection (&m_csWaveOut);

      // while in here, get position
      waveOutGetPosition (m_hPlayWaveOut, &m_mtLast, sizeof(m_mtLast));

      PWAVEHDR pwh = (WAVEHDR*) dwParam1;

      // just had one returned
      m_dwPlayBufOut--;

      PlayAddBuffer (pwh, TRUE);

      // if no buffers left out then end of buffer, so stop
      if (!m_dwPlayBufOut)
         PostMessage (m_hWnd, WM_COMMAND, IDC_ANIMSTOPIFNOBUF, 0); // post a stop message

      LeaveCriticalSection (&m_csWaveOut);
   }
}


/**********************************************************************************
CWaveView::Play - Start playback.

inputs
   HWND           hWnd - Bring up error messages
   BOOL           fLooped - Set to TRUE if looped
returns
   BOOL - TRUE if success
*/
BOOL CWaveView::Play (HWND hWnd, BOOL fLooped)
{
   // if already playing then fail
   if (m_hPlayWaveOut || !m_pWave->m_dwSamples)
      return FALSE;

   // if play position more >= max then set to start
   m_iPlayCur = max(m_iPlayCur, m_iPlayLimitMin);
   if (m_iPlayCur >= m_iPlayLimitMax)
      m_iPlayCur = m_iPlayLimitMin;

   // remember
   m_fPlayLooped = fLooped;
   m_dwPlayBufOut = 0;
   m_fPlayStopping = FALSE;

   // how much memory do we need
   m_dwPlayBufSize = (DWORD) (m_pWave->m_dwSamplesPerSec / 8) * m_pWave->m_dwChannels * sizeof(short);
      // keep the buffers small so can hear any changes that make right away
   if (!m_memPlay.Required (m_dwPlayBufSize * WVPLAYBUF))
      return FALSE;

   // clear the sync list
   m_lPlaySync.Clear();
   m_dwPlaySyncCount = 0;

   // create the wave
   MMRESULT mm;
   WAVEFORMATEX WFEX;
   memset (&WFEX, 0, sizeof(WFEX));
   WFEX.cbSize = 0;
   WFEX.wFormatTag = WAVE_FORMAT_PCM;
   WFEX.nChannels = m_pWave->m_dwChannels;
   WFEX.nSamplesPerSec = m_pWave->m_dwSamplesPerSec;
   WFEX.wBitsPerSample = 16;
   WFEX.nBlockAlign  = WFEX.nChannels * WFEX.wBitsPerSample / 8;
   WFEX.nAvgBytesPerSec = WFEX.nBlockAlign * WFEX.nSamplesPerSec;

#if 0 // def _DEBUG
   WAVEFORMATPCMEX wex;
   memset (&wex, 0, sizeof(wex));
   wex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
   wex.Format.nChannels = 2;
   wex.Format.nSamplesPerSec = 48000;
   wex.Format.wBitsPerSample = 16;
   wex.Format.nBlockAlign  = wex.Format.nChannels * wex.Format.wBitsPerSample / 8;
   wex.Format.nAvgBytesPerSec = wex.Format.nBlockAlign * wex.Format.nSamplesPerSec;
   wex.Samples.wValidBitsPerSample = 16;
   wex.dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
   wex.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
   mm = waveOutOpen (&m_hPlayWaveOut, WAVE_MAPPER, (PWAVEFORMATEX) &wex, (DWORD_PTR) ::PlayCallback,
      (DWORD_PTR) this, CALLBACK_FUNCTION);
   if (!mm)
      waveOutClose (m_hPlayWaveOut);
#endif

#if 0 // def _DEBUG

   //try to open up 6-channels sound
   DWORD adwSamples[] = {22050, 44100, 48000, 88200, 9600};
   DWORD dwRate;
   DWORD dwDevice;
   for (dwDevice = 0; dwDevice < waveOutGetNumDevs(); dwDevice++) {
      for (WFEX.nChannels = 3; WFEX.nChannels <= 8; WFEX.nChannels++) {
         for (dwRate = 0; dwRate < sizeof(adwSamples)/sizeof(DWORD); dwRate++) {
            WFEX.nSamplesPerSec = adwSamples[dwRate];
            for (WFEX.wBitsPerSample = 8; WFEX.wBitsPerSample <= 24; WFEX.wBitsPerSample += 8) {
               WFEX.nBlockAlign  = WFEX.nChannels * WFEX.wBitsPerSample / 8;
               WFEX.nAvgBytesPerSec = WFEX.nBlockAlign * WFEX.nSamplesPerSec;
               mm = waveOutOpen (&m_hPlayWaveOut, dwDevice, &WFEX, (DWORD_PTR) ::PlayCallback,
                  (DWORD_PTR) this, CALLBACK_FUNCTION);
               if (!mm)
                  break;
            } // bits
         } // dwRate
      } /// channels
   } // dwDevice
#endif

   EnterCriticalSection (&m_csWaveOut);
   mm = waveOutOpen (&m_hPlayWaveOut, WAVE_MAPPER, &WFEX, (DWORD_PTR) ::PlayCallback,
      (DWORD_PTR) this, CALLBACK_FUNCTION);
   if (mm) {
      LeaveCriticalSection (&m_csWaveOut);
      EscMessageBox (hWnd, ASPString(),
         L"Playback failed.",
         L"You may not have a sound card, or it might be used by another application.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }
   memset (&m_mtLast, 0, sizeof(m_mtLast));

   // prepare the headers
   DWORD i;
   memset (m_aPlayWaveHdr, 0, sizeof(m_aPlayWaveHdr));
   for (i = 0; i < WVPLAYBUF; i++) {
      m_aPlayWaveHdr[i].dwBufferLength = m_dwPlayBufSize;
      m_aPlayWaveHdr[i].lpData = (PSTR) ((PBYTE) m_memPlay.p + i * m_dwPlayBufSize);
      mm = waveOutPrepareHeader (m_hPlayWaveOut, &m_aPlayWaveHdr[i], sizeof(m_aPlayWaveHdr[i]));
   }

   // enable/disable buttons
   if (m_pPlay)
      m_pPlay->Enable (FALSE);
   if (m_pPlayLoop)
      m_pPlayLoop->Enable (FALSE);
   if (m_pPlaySel)
      m_pPlaySel->Enable (FALSE);
   if (m_pRecord)
      m_pRecord->Enable (FALSE);
   if (m_pStop)
      m_pStop->Enable (TRUE);

   // write them out
   waveOutPause(m_hPlayWaveOut);
   for (i = 0; i < WVPLAYBUF; i++)
      PlayAddBuffer (&m_aPlayWaveHdr[i], TRUE);
   waveOutRestart (m_hPlayWaveOut);
   memset (&m_mtLast, 0, sizeof(m_mtLast));

   LeaveCriticalSection (&m_csWaveOut);

   // start the timer
   m_dwPlayTimer = SetTimer (m_hWnd, TIMER_PLAYBACK, 100, NULL);

   // done
   return TRUE;
}

/**********************************************************************************
CWaveView::Stop - Stops wave play.
*/
void CWaveView::Stop (void)
{
   // if not open then fail
   if (!m_hPlayWaveOut)
      return;

   // If there are wave buffers out, which means was a forced stop, then
   // set the m_iPlayCur based on the current slider position
   if (m_pTimeline) {
      if (m_dwPlayBufOut)
         m_iPlayCur = (int) m_pTimeline->PointerGet();
      else {
         m_pTimeline->PointerSet (m_iPlayCur);
         if (m_hWndMouth) {
            InvalidateRect (m_hWndMouth, NULL, FALSE);
            UpdateWindow (m_hWndMouth);
         }
      }
   }

   // force stop
   m_fPlayStopping = TRUE;
   EnterCriticalSection (&m_csWaveOut);
   waveOutReset (m_hPlayWaveOut);
   memset (&m_mtLast, 0, sizeof(m_mtLast));

   // unprepare
   DWORD i;
   for (i = 0; i < WVPLAYBUF; i++)
      waveOutUnprepareHeader (m_hPlayWaveOut, &m_aPlayWaveHdr[i], sizeof(m_aPlayWaveHdr[i]));

   // close
   waveOutClose (m_hPlayWaveOut);
   LeaveCriticalSection (&m_csWaveOut);
   m_hPlayWaveOut = NULL;
   memset (&m_mtLast, 0, sizeof(m_mtLast));

   // kill timer
   if (m_dwPlayTimer)
      KillTimer (m_hWnd, TIMER_PLAYBACK);

   // enable/disable buttons
   if (m_pPlay)
      m_pPlay->Enable (TRUE);
   if (m_pPlayLoop)
      m_pPlayLoop->Enable (TRUE);
   if (m_pPlaySel)
      m_pPlaySel->Enable (TRUE);
   if (m_pRecord)
      m_pRecord->Enable (TRUE);
   if (m_pStop)
      m_pStop->Enable (FALSE);
}

/**********************************************************************************
CWaveView::EnableButtonsThatNeedSel - Enable or disable those buttons that need
a selection
*/
void CWaveView::EnableButtonsThatNeedSel (void)
{
   PCIconButton *ppi = (PCIconButton*) m_listNeedSelect.Get(0);
   DWORD i;
   BOOL fSel = (m_iSelStart != m_iSelEnd);
   for (i = 0; i < m_listNeedSelect.Num(); i++)
      ppi[i]->Enable (fSel);
}


/**********************************************************************************
CWaveView::ReplaceRegion - Like the other replace region except that replaces the
selection.
inputs
   PCM3DWave   pWave - Replace from start to end with this wave. pWave is not acutally touched.
                  If NULL then just deletes from start to end.
returns
   BOOL - TRUE if success
*/
BOOL CWaveView::ReplaceRegion (PCM3DWave pWave)
{
   return ReplaceRegion ((DWORD)min(m_iSelStart, m_iSelEnd),
      (DWORD)max(m_iSelStart, m_iSelEnd), pWave, TRUE);
}

/**********************************************************************************
CWaveView::ReplaceRegion - Replace a region of the current wave with a new wave.
This handles all the undo and the moving around of pointers.

inputs
   DWORD       dwStart - Start replacement
   DWORD       dwEnd - End replacement
   PCM3DWave   pWave - Replace from start to end with this wave. pWave is not acutally touched.
                  If NULL then just deletes from start to end.
   BOOL        fSelectReplace - If TRUE then select replacement
returns
   BOOL - TRUE if success
*/
BOOL CWaveView::ReplaceRegion (DWORD dwStart, DWORD dwEnd, PCM3DWave pWave, BOOL fSelectReplace)
{
   // keep in range
   dwEnd = min(dwEnd, m_pWave->m_dwSamples);
   dwStart = min(dwStart, dwEnd);

   // how many samples to repalce with
   DWORD dwWith, dwReplace;
   dwWith = pWave ? pWave->m_dwSamples : 0;
   dwReplace = dwEnd - dwStart;

   // Remember this for undo
   UndoCache();
   UndoAboutToChange();

   // selection min/max
   int iSelMin, iSelMax;
   iSelMin = min(m_iSelStart, m_iSelEnd);
   iSelMax = max(m_iSelStart, m_iSelEnd);
   m_iSelStart = iSelMin;
   m_iSelEnd = iSelMax;

   // if replace before the selection then move it up
   int iDelta;
   iDelta = (int)dwWith - (int)dwReplace;
   if (fSelectReplace) {
      m_iSelStart = (int)dwStart;
      m_iSelEnd = m_iSelStart + (int)dwWith;
   }
   else {   // dont select the replacement
      if ((m_iSelStart >= (int)dwEnd) && (dwStart != dwEnd))
         m_iSelStart += iDelta;
      if (m_iSelEnd >= (int)dwEnd)
         m_iSelEnd += iDelta;
   }

   // Stick memory on end
   m_pWave->ReplaceSection (dwStart, dwEnd, pWave);

   // update play position
   if ((m_iPlayLimitMin >= (int)dwEnd) && (dwStart != dwEnd))
      m_iPlayLimitMin += iDelta;
   if (m_iPlayLimitMax >= (int)dwEnd)
      m_iPlayLimitMax += iDelta;
   m_iPlayLimitMax = max(m_iPlayLimitMax, m_iPlayLimitMin+1);
   m_iPlayLimitMax = min(m_iPlayLimitMax, (int)m_pWave->m_dwSamples);
   m_iPlayLimitMin = min(m_iPlayLimitMin, m_iPlayLimitMax-1);
   m_iPlayLimitMin = max(m_iPlayLimitMin, 0);

   // play position
   if (m_iPlayCur >= (int)dwEnd)
      m_iPlayCur += iDelta;
   m_iPlayCur = max(m_iPlayLimitMin, m_iPlayCur);
   m_iPlayCur = min(m_iPlayLimitMax, m_iPlayCur);

   // view
   if (m_iViewSampleRight >= (int)dwEnd)
      m_iViewSampleRight += iDelta;
   if ((m_iViewSampleLeft >= (int)dwEnd) && (dwStart != dwEnd))
      m_iViewSampleLeft += iDelta;
   m_iViewSampleRight = max(m_iViewSampleRight, m_iViewSampleLeft+1);
   m_iViewSampleRight = min(m_iViewSampleRight, (int)m_pWave->m_dwSamples);
   m_iViewSampleLeft = min(m_iViewSampleLeft, m_iViewSampleRight-1);
   m_iViewSampleLeft = max(m_iViewSampleLeft, 0);


   // update display
   UpdateScroll ();
   EnableButtonsThatNeedSel();

   return TRUE;
}


/**********************************************************************************
CWaveView::WaveFromClipboard - Gets a wave from the clipboard

returns
   PCM3DWave - Wave (must be freed) or null if error
*/
PCM3DWave CWaveView::WaveFromClipboard (void)
{
   if (!OpenClipboard (m_hWnd))
      return NULL;

   HANDLE hRet;
   hRet = GetClipboardData (CF_WAVE);
   if (!hRet) {
      return NULL;
      CloseClipboard ();
   }

   PCM3DWave pWave;
   pWave = new CM3DWave;
   if (!pWave)
      goto done;

   // from global memory
   MMIOINFO mmio;
   HMMIO hmmio;
   memset (&mmio, 0, sizeof(mmio));
   mmio.pchBuffer = (HPSTR) GlobalLock (hRet);
   mmio.fccIOProc = FOURCC_MEM;
   mmio.cchBuffer = GlobalSize (hRet);
   hmmio = mmioOpen (NULL, &mmio, MMIO_READ);
   if (!hmmio) {
      GlobalUnlock (hRet);
      delete pWave;
      pWave = NULL;
      goto done;
   }
   BOOL fRet;
   fRet = pWave->Open (NULL, "", TRUE, hmmio);
   mmioClose (hmmio, 0);
   GlobalUnlock (hRet);
   if (!fRet) {
      delete pWave;
      pWave = NULL;
      goto done;
   }


done:
   CloseClipboard();
   return pWave;
}

/**********************************************************************************
CWaveView::Paste - Pastes the new wave over the current selection.
*/
BOOL CWaveView::Paste (void)
{
   PCM3DWave pWave = WaveFromClipboard();
   if (!pWave) {
      EscMessageBox (m_hWnd, ASPString(),
         L"The wave data on the clipboard can't be pasted.",
         L"The wave data may be stored as an incompatible type.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

   // If it's a different sampling rate/channels then need to convert
   if ((pWave->m_dwChannels != m_pWave->m_dwChannels) || (pWave->m_dwSamplesPerSec != m_pWave->m_dwSamplesPerSec)) {
      CProgress Progress;
      Progress.Start (m_hWnd, "Converting...");
      pWave->ConvertSamplesAndChannels (m_pWave->m_dwSamplesPerSec, m_pWave->m_dwChannels, &Progress);
   }


   // write over
   ReplaceRegion (pWave);

   delete pWave;
   return TRUE;
}

/**********************************************************************************
CWaveView::PasteMix - Pastes the new wave over the current selection.
*/
BOOL CWaveView::PasteMix (void)
{
   PCM3DWave pWave = WaveFromClipboard();
   if (!pWave) {
      EscMessageBox (m_hWnd, ASPString(),
         L"The wave data on the clipboard can't be pasted.",
         L"The wave data may be stored as an incompatible type.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

   // If it's a different sampling rate/channels then need to convert
   if ((pWave->m_dwChannels != m_pWave->m_dwChannels) || (pWave->m_dwSamplesPerSec != m_pWave->m_dwSamplesPerSec)) {
      CProgress Progress;
      Progress.Start (m_hWnd, "Converting...");
      pWave->ConvertSamplesAndChannels (m_pWave->m_dwSamplesPerSec, m_pWave->m_dwChannels, &Progress);
   }


   // create a new selection start and end based on the length
   m_iSelStart = min(m_iSelStart, m_iSelEnd);
   m_iSelEnd = min(m_iSelStart + (int)pWave->m_dwSamples, (int)m_pWave->m_dwSamples);

   // mix the values together into pWave
   DWORD i, j;
   if (pWave->m_dwChannels != m_pWave->m_dwChannels) {
      // just in case
      delete pWave;
      return FALSE;
   }
   DWORD dwFrom;
   DWORD dwStart;
   int iSum;
   dwStart = (DWORD)min(m_iSelStart, m_iSelEnd);
   for (i = 0; i < pWave->m_dwSamples; i++) {
      dwFrom = i + dwStart;
      if (dwFrom >= m_pWave->m_dwSamples)
         break;   // nothing left in original to mix
      for (j = 0; j < pWave->m_dwChannels; j++) {
         iSum = (int)pWave->m_psWave[i*pWave->m_dwChannels+j] +
            (int)m_pWave->m_psWave[dwFrom*m_pWave->m_dwChannels+j];
         iSum = max(iSum, -32768);
         iSum = min(iSum, 32767);
         pWave->m_psWave[i*pWave->m_dwChannels+j] = iSum;
      } // j
   } // i

   // write over
   ReplaceRegion (pWave);

   delete pWave;
   return TRUE;
}

/**********************************************************************************
CWaveView::Copy - Copies the current selection to the clipboard
*/
void CWaveView::Copy (void)
{
   DWORD dwStart = (DWORD)min(m_iSelStart, m_iSelEnd);
   DWORD dwEnd = (DWORD)max(m_iSelStart, m_iSelEnd);
   if (dwStart == dwEnd) {
      // do the entire wave
      dwStart = 0;
      dwEnd = m_pWave->m_dwSamples;
   }

   if (!RegisterIsRegistered()) {
      EscMessageBox (m_hWnd, ASPString(),
         L"Copy is disabled for unregistered users.",
         L"You must register and pay for " APPSHORTNAMEW L" before you can copy. "
         L"To register, run the main " APPSHORTNAMEW L" application and look in help.",
         MB_ICONEXCLAMATION | MB_OK);
      return;
   }

   // copy it
   PCM3DWave pWave;
   pWave = m_pWave->Copy (dwStart, dwEnd);
   if (!pWave)
      return;

   // fix the wfex so just write PCM
   pWave->MakePCM ();

   // from global memory
   MMIOINFO mmio;
   HMMIO hmmio;
   DWORD dwInitial;
   dwInitial = pWave->m_dwSamples * pWave->m_dwChannels * sizeof(short) + 1024;
   memset (&mmio, 0, sizeof(mmio));
   HGLOBAL hg;
   hg = GlobalAlloc (GMEM_MOVEABLE, dwInitial);
   mmio.pchBuffer = (HPSTR) GlobalLock (hg);
   mmio.fccIOProc = FOURCC_MEM;
   mmio.cchBuffer = dwInitial/4;
   mmio.adwInfo[0] = mmio.adwInfo[1] = mmio.adwInfo[2] = mmio.adwInfo[3] = dwInitial / 4;
   hmmio = mmioOpen (NULL, &mmio, MMIO_CREATE | MMIO_WRITE);
   if (!hmmio) {
      GlobalFree (mmio.pchBuffer);
      return;
   }
   BOOL fRet;
   fRet = pWave->Save (FALSE, NULL, hmmio);
   mmioClose (hmmio, 0);
   GlobalUnlock (hg);
   if (!fRet) {
      GlobalFree (mmio.pchBuffer);
      return;
   }

   // open the clibboard
   if (!OpenClipboard (m_hWnd)) {
      GlobalFree (mmio.pchBuffer);
      return;
   }
   EmptyClipboard ();
   HANDLE hRet;
   hRet = SetClipboardData (CF_WAVE, hg);
   CloseClipboard ();

   // finally
   delete pWave;
}

/**********************************************************************************
CWaveView::ClipboardUpdatePasteButton - Updates paste depending upon viewer
*/
void CWaveView::ClipboardUpdatePasteButton (void)
{
   if (!m_hWnd || !m_pPaste)
      return;  // not all initialized yet

   // open the clipboard and find the right data
   if (!OpenClipboard(m_hWnd))
      return;

   // get the data
   HANDLE hMem;
   hMem = GetClipboardData (CF_WAVE);
   CloseClipboard ();

   m_pPaste->Enable (hMem ? TRUE : FALSE);
   m_pPasteMix->Enable (hMem ? TRUE : FALSE);
}


/***************************************************************************************
ACM callbacks */
typedef struct {
   PCWaveView        pv;            // wave view
   DWORD             dwSamplesPerSec;  // samples per sec chosen
   DWORD             dwChannels;    // channels chosen
   DWORD             dwWFEX;        // index into plWFEX chosen
   PCListVariable    plFormat;      // fills with a list of ACMFORMATDETAILS
   PCListVariable    plDriver;      // fills with a list of ACMDRIVERDETAILS
   PCListVariable    plWFEX;        // filled with a list of WAVEFORMATEX for the details
} SPINFO, *PSPINFO;

static BOOL CALLBACK FormatCallback (
  HACMDRIVERID       hadid,      
  LPACMFORMATDETAILS pafd,       
  DWORD_PTR          dwInstance, 
  DWORD              fdwSupport  
)
{
   PSPINFO ps = (PSPINFO) dwInstance;
   
   // if this is just PCM then ignore
   if ((pafd->pwfx->wFormatTag == WAVE_FORMAT_PCM) && (pafd->pwfx->wBitsPerSample == 16))
      return TRUE;

   // if wave format matches what have then remember this
   PWAVEFORMATEX pwfex = (PWAVEFORMATEX) ps->pv->m_pWave->m_memWFEX.p;
   if ((pwfex->cbSize == pafd->pwfx->cbSize) && !memcmp(pwfex, pafd->pwfx, sizeof(WAVEFORMATEX)+pwfex->cbSize))
      ps->dwWFEX = ps->plWFEX->Num();

   // add this
   ps->plFormat->Add (pafd, sizeof(ACMFORMATDETAILS));
   ps->plWFEX->Add (pafd->pwfx, sizeof(WAVEFORMATEX) + pafd->pwfx->cbSize);

   return TRUE;
}

static BOOL CALLBACK DriverCallback(
  HACMDRIVERID hadid,     
  DWORD_PTR    dwInstance,
  DWORD        fdwSupport 
)
{
   HACMDRIVER hDriver = NULL;
   PSPINFO ps = (PSPINFO) dwInstance;
   if (acmDriverOpen (&hDriver, hadid, 0))
      return TRUE;

   ACMFORMATDETAILS fd;
   BYTE abTemp[1000];
   memset (&fd, 0, sizeof(fd));
   fd.cbStruct = sizeof(fd);
   fd.pwfx = (PWAVEFORMATEX) &abTemp[0];
   fd.cbwfx = sizeof(abTemp);
   fd.dwFormatTag = WAVE_FORMAT_UNKNOWN;

   // cheesy hack - know that the first entry in the list is the PCM data
   // so just just that as initial wave format
   memcpy (fd.pwfx, ps->plWFEX->Get(0), ps->plWFEX->Size(0));

   MMRESULT mm;
   mm = acmFormatEnum (hDriver, &fd, FormatCallback, dwInstance,
      ACM_FORMATENUMF_CONVERT | ACM_FORMATENUMF_NCHANNELS | ACM_FORMATENUMF_NSAMPLESPERSEC);

   // add copy of diver details
   ACMDRIVERDETAILS dd;
   memset (&dd, 0, sizeof(dd));
   dd.cbStruct = sizeof(dd);
   acmDriverDetails (hadid, &dd, 0);
   while (ps->plDriver->Num() < ps->plFormat->Num())
      ps->plDriver->Add (&dd, sizeof(dd));

   acmDriverClose (hDriver,0);
   return TRUE;
}


/****************************************************************************
SettingsPage
*/
BOOL SettingsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSPINFO ps = (PSPINFO) pPage->m_pUserData;
   PCWaveView pv = ps->pv;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         ESCMCOMBOBOXADD add;
         WCHAR szTemp[128];

         pControl = pPage->ControlFind (L"spoken");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR) pv->m_pWave->m_memSpoken.p);
         pControl = pPage->ControlFind (L"speaker");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR) pv->m_pWave->m_memSpeaker.p);

         if (!ComboBoxSet (pPage,L"samples", ps->dwSamplesPerSec)) {
            pControl = pPage->ControlFind (L"samples");
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            swprintf (szTemp, L"<elem name=%d>%d kHz</elem>",
               (int)ps->dwSamplesPerSec,
               (int)ps->dwSamplesPerSec / 1000);
            add.pszMML = szTemp;
            if (pControl)
               pControl->Message (ESCM_COMBOBOXADD, &add);
            ComboBoxSet (pPage, L"samples", ps->dwSamplesPerSec);
         }

         if (!ComboBoxSet (pPage,L"channels", ps->dwChannels)) {
            pControl = pPage->ControlFind (L"channels");
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            swprintf (szTemp, L"<elem name=%d>%d channels</elem>",
               (int)ps->dwChannels,
               (int)ps->dwChannels);
            add.pszMML = szTemp;
            if (pControl)
               pControl->Message (ESCM_COMBOBOXADD, &add);
            ComboBoxSet (pPage, L"channels", ps->dwChannels);
         }

         pPage->Message (ESCM_USER+82);   // so update the list of choices for the
            // channels and sampling rate
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"spoken")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            pv->UndoAboutToChange();
            if (!pv->m_pWave->m_memSpoken.Required (dwNeed))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR) pv->m_pWave->m_memSpoken.p,
               (DWORD)pv->m_pWave->m_memSpoken.m_dwAllocated, &dwNeed);

            // while at it, copy this to sounds like
            //MemZero(&pv->m_pWave->m_memSoundsLike);
            //MemCat(&pv->m_pWave->m_memSoundsLike, (PWSTR)pv->m_pWave->m_memSpoken.p);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"speaker")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            pv->UndoAboutToChange();
            if (!pv->m_pWave->m_memSpeaker.Required (dwNeed+2))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR) pv->m_pWave->m_memSpeaker.p,
               (DWORD)pv->m_pWave->m_memSpeaker.m_dwAllocated, &dwNeed);

            // set this as default name
            pv->m_pWave->DefaultSpeakerSet (&pv->m_pWave->m_memSpeaker);
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

         if (_wcsicmp(psz, L"convert"))
            break;   // only care about convert

         // create progress bar
         CProgress Progress;
         Progress.Start (pPage->m_pWindow->m_hWnd, "Converting...");

         PWAVEFORMATEX pwfex;
         pwfex = (PWAVEFORMATEX) ps->plWFEX->Get(ps->dwWFEX);
         if (!pwfex)
            return TRUE;   // shouldnt happen

         pv->UndoCache();
         pv->UndoAboutToChange ();
         pv->m_pWave->ConvertWFEX (pwfex, &Progress);
         pv->UndoCache();

         // redraw, showing entire wave
         pv->m_iViewSampleLeft = pv->m_iPlayCur = pv->m_iPlayLimitMin = 0;
         pv->m_iViewSampleRight = pv->m_iPlayLimitMax = (int)pv->m_pWave->m_dwSamples;
         pv->m_iSelStart = pv->m_iSelEnd = 0;
         pv->UpdateScroll ();
         pv->RebuildPanes ();

         pPage->Exit (Back());
         return TRUE;
      }
      break;

   case ESCM_USER+83:   // set the status to indicate bytes per second, and total bytes
      {
         PCEscControl pControl = pPage->ControlFind (L"status");
         if (!pControl)
            return TRUE;

         CMem gMemTemp;
         MemZero (&gMemTemp);

         // find the wave format
         PWAVEFORMATEX pwfex;
         pwfex = (PWAVEFORMATEX) ps->plWFEX->Get(ps->dwWFEX);
         if (pwfex) {
            MemCat (&gMemTemp, L"<p><bold>");
            MemCat (&gMemTemp, (int)pwfex->nAvgBytesPerSec / 1000);
            MemCat (&gMemTemp, L".");
            MemCat (&gMemTemp, (int)(pwfex->nAvgBytesPerSec / 100) % 10);
            MemCat (&gMemTemp, L" KBytes/sec</bold> for ");
            MemCat (&gMemTemp, (int)pv->m_pWave->m_dwSamples / (int)pv->m_pWave->m_dwSamplesPerSec);
            MemCat (&gMemTemp, L" seconds =<br/>");
            MemCat (&gMemTemp, (int)((__int64)pwfex->nAvgBytesPerSec * (__int64)pv->m_pWave->m_dwSamples /
               (__int64)pv->m_pWave->m_dwSamplesPerSec / 1000));
            MemCat (&gMemTemp, L" KBytes total");
            MemCat (&gMemTemp, L"</p>");
         }
         else
            MemCat (&gMemTemp, L"Can't seem to convert this wave.");

         ESCMSTATUSTEXT s;
         memset (&s, 0, sizeof(s));
         s.pszMML = (PWSTR) gMemTemp.p;
         pControl->Message (ESCM_STATUSTEXT, &s);
      }
      return TRUE;

   case ESCM_USER+82:   // called so list of choices for channels and sampling rate updated
      {
         PCEscControl pControl = pPage->ControlFind (L"compression");
         if (!pControl)
            return TRUE;

         pControl->Message (ESCM_COMBOBOXRESETCONTENT, 0);

         
         // enumerate the formats into a big list
         ps->plFormat->Clear();
         ps->plWFEX->Clear();
         ps->plDriver->Clear();

         // add no-compression
         ACMFORMATDETAILS fd;
         WAVEFORMATEX wfex;
         ACMDRIVERDETAILS dd;
         memset (&fd, 0, sizeof(fd));
         memset (&wfex, 0, sizeof(wfex));
         memset (&dd, 0, sizeof(dd));
         fd.dwFormatTag = WAVE_FORMAT_PCM;
         // strcpy (fd.szFormat, "None");
         strcpy (dd.szLongName, "No compression");
         wfex.cbSize = 0;
         wfex.wFormatTag = WAVE_FORMAT_PCM;
         wfex.nChannels = ps->dwChannels;
         wfex.nSamplesPerSec = ps->dwSamplesPerSec;
         wfex.wBitsPerSample = 16;
         wfex.nBlockAlign  = wfex.nChannels * wfex.wBitsPerSample / 8;
         wfex.nAvgBytesPerSec = wfex.nBlockAlign * wfex.nSamplesPerSec;
         ps->plFormat->Add (&fd, sizeof(fd));
         ps->plWFEX->Add (&wfex, sizeof(wfex));
         ps->plDriver->Add (&dd, sizeof(dd));

         // just in case dont match anything else, pretend matches pcm
         ps->dwWFEX = 0;

         // enumerate the formats
         acmDriverEnum (DriverCallback, (DWORD_PTR) ps, ACM_DRIVERENUMF_NOLOCAL);

         // add all the formats
         // add no compression
         ESCMCOMBOBOXADD add;
         DWORD i;
         CMem gMemTemp;
         MemZero (&gMemTemp);
         for (i = 0; i < ps->plFormat->Num(); i++) {
            PACMFORMATDETAILS pfd = (PACMFORMATDETAILS) ps->plFormat->Get(i);
            PACMDRIVERDETAILS pdd = (PACMDRIVERDETAILS) ps->plDriver->Get(i);
            if (!pdd->szLongName[0])
               continue;   // blank name


            // add 
            WCHAR szName[256];
            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L"><bold>");
            MultiByteToWideChar (CP_ACP, 0, pdd->szLongName, -1, szName, sizeof(szName)/2);
            MemCatSanitize (&gMemTemp, szName);
            MemCat (&gMemTemp, L"</bold> ");
            MultiByteToWideChar (CP_ACP, 0, pfd->szFormat, -1, szName, sizeof(szName)/2);
            MemCatSanitize (&gMemTemp, szName);
            MemCat (&gMemTemp, L"</elem>");

         }

         memset (&add, 0, sizeof(add));
         add.dwInsertBefore = -1;
         add.pszMML = (PWSTR) gMemTemp.p;
         pControl->Message (ESCM_COMBOBOXADD, &add);
         // select the string
         ComboBoxSet (pPage, L"compression", ps->dwWFEX);

         // may need to set status control (or delete it)
         pPage->Message (ESCM_USER+83);
      }
      return TRUE;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         DWORD dwVal;
         psz = p->pControl->m_pszName;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"samples")) {
            if ((dwVal == ps->dwSamplesPerSec) || !dwVal)
               return TRUE;   // no change
            ps->dwSamplesPerSec = dwVal;
            pPage->Message (ESCM_USER+82);   // to update list of possible rates
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"channels")) {
            if ((dwVal == ps->dwChannels) || !dwVal)
               return TRUE;   // no change
            ps->dwChannels = dwVal;
            pPage->Message (ESCM_USER+82);   // to update list of possible rates
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"compression")) {
            if (dwVal == ps->dwWFEX)
               return TRUE;   // no change
            ps->dwWFEX = dwVal;
            pPage->Message (ESCM_USER+83);   // to update size
            return TRUE;
         }
      }
      return 0;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Settings";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}


/*********************************************************************************
CWaveView::Settings - Bring up the settings page.

inputs
   HWND        hWnd - window to display from
returns
   BOOL - TRUE if success
*/
BOOL CWaveView::Settings (HWND hWnd)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (hWnd, &r);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // start with the first page
   SPINFO sp;
   CListVariable lDriver, lWFEX, lFormat;
   memset (&sp, 0, sizeof(sp));
   sp.dwChannels = m_pWave->m_dwChannels;
   sp.dwSamplesPerSec = m_pWave->m_dwSamplesPerSec;
   sp.dwWFEX = 0;
   sp.plFormat = &lFormat;
   sp.plWFEX = &lWFEX;
   sp.plDriver = &lDriver;
   sp.pv = this;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLSETTINGS, SettingsPage, &sp);

   return TRUE;
}




/****************************************************************************
SpeechRecogPage
*/
BOOL SpeechRecogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCWaveView pv = (PCWaveView) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      PCEscControl pControl;
      pControl = pPage->ControlFind (L"soundslike");
      if (pControl)
         pControl->AttribSet (Text(), (PWSTR) pv->m_pWave->m_memSpoken.p);

      pPage->Message (ESCM_USER+83);   // fill in info
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"soundslike")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            pv->UndoAboutToChange();
            if (!pv->m_pWave->m_memSpoken.Required (dwNeed))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR) pv->m_pWave->m_memSpoken.p,
               (DWORD)pv->m_pWave->m_memSpoken.m_dwAllocated, &dwNeed);
            return TRUE;
         }
      }
      break;

   case ESCM_USER+83:
      {
         PCEscControl pControl;

         // fill in the file
         VoiceFileDefaultGet (pPage, L"trainfile");
         //pControl = pPage->ControlFind (L"trainfile");
         //if (pControl)
         //   pControl->AttribSet (Text(), pv->m_szTrainFile);

         // open the file
         CVoiceFile Voice;
         Voice.Open ();

         // get lexicon
         PCMLexicon pLex = Voice.Lexicon();
         if (!pLex)
            pPage->MBWarning (L"The lexicon for the voice file can't be opened.",
               L"Speech recognition won't work.");

         // identify which phonemes must be trained
         DWORD dwAdded = Voice.TrainVerifyAllTrained(NULL);

         // if not all phonemes trained then disable the recognize button
         pControl = pPage->ControlFind (L"recog");
         if (pControl)
            pControl->Enable (dwAdded ? FALSE : TRUE);


      }
      return 0;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"recog")) {

            pv->UndoCache();
            pv->UndoAboutToChange();

            CVoiceFile Voice;
            if (!Voice.Open ()) {
               pPage->MBWarning (L"Couldn't open the training file.");
               return TRUE;
            }

            // recognize
            {
               CProgress Progress;

               Progress.Start (pPage->m_pWindow->m_hWnd, "Recognizing phonemes...");

               BOOL fPush = (!pv->m_pWave->m_dwSRSamples || !pv->m_pWave->m_adwPitchSamples[PITCH_F0]);
               if (fPush)
                  Progress.Push (0, .5);
               pv->m_pWave->CalcSRFeatures (WAVECALC_SEGMENT, &Progress);
               if (fPush) {
                  Progress.Pop();
                  Progress.Push (.5, 1);
               }

               // convert this to phonemes
               Voice.Recognize ((PWSTR)pv->m_pWave->m_memSpoken.p,
                  pv->m_pWave, FALSE, &Progress);
               if (fPush)
                  Progress.Pop ();
            }


            pv->InvalidateDisplay();
            pv->UndoCache();

            pPage->MBSpeakInformation (L"Recognition complete.");

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"seetrain")) {
            CVoiceFile Voice;
            if (!Voice.Open ()) {
               pPage->MBWarning (L"Couldn't open the training file.");
               return TRUE;
            }

            pv->UndoCache();
            pv->UndoAboutToChange();
            Voice.FillWaveWithTraining (pv->m_pWave);
            pv->RebuildPanes();
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"open")) {
            if (!VoiceFileDefaultUI (pPage->m_pWindow->m_hWnd))
               return TRUE;
            //WCHAR szw[256];
            //wcscpy (szw, pv->m_szTrainFile);
            //if (!VoiceFileOpenDialog (pPage->m_pWindow->m_hWnd, szw,
            //   sizeof(szw)/sizeof(WCHAR), FALSE))
            //   return TRUE;

            // try opening
            CVoiceFile Voice;
            if (!Voice.Open ()) {
               pPage->MBWarning (L"The file couldn't be opened.",
                  L"It may not be a proper voice file.");
               return TRUE;
            }

            // save the file
            //wcscpy (pv->m_szTrainFile, szw);

            pPage->Message (ESCM_USER+83);   // so refresh display
            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Speech recognition";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}

/*********************************************************************************
CWaveView::SpeechRecog - Bring up the settings page.

inputs
   HWND        hWnd - window to display from
returns
   BOOL - TRUE if success
*/
BOOL CWaveView::SpeechRecog (HWND hWnd)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (hWnd, &r);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLSPEECHRECOG, SpeechRecogPage, this);

   return TRUE;
}

/*********************************************************************************
CWaveView::FXCopy - Copies the selection for puposes of using for an effect.

returns
   PCM3DWave  - Wave that should be manipulated by the effect. Once manipulated pass
         it into FXPaste(). If this is NULL then an error occurred
*/
PCM3DWave CWaveView::FXCopy (void)
{
   int iMin, iMax;
   iMin = min(m_iSelStart, m_iSelEnd);
   iMax = max(m_iSelStart, m_iSelEnd);
   if (iMin == iMax) {
      // if no selection then affect the entire wave
      iMin = m_iSelStart = 0;
      iMax = m_iSelEnd = (int)m_pWave->m_dwSamples;
      InvalidateDisplay();
   }
   iMin = max(iMin, 0);
   iMax = max(iMax, 0);
   iMin = min(iMin, (int)m_pWave->m_dwSamples);
   iMax = min(iMax, (int)m_pWave->m_dwSamples);

   return m_pWave->Copy ((DWORD)iMin, (DWORD)iMax);
}

/*********************************************************************************
CWaveView::FXPaste - Takes the wave data that has just had an effect done and pastes
it into the original wave, updating all the displays.

inputs
   PCM3DWave         pWave - From FXCopy() and then modified by effect. This wave
                     will automatically be deleted by the call to FXPaste()
returns
   BOOL - TRUE if success
*/
BOOL CWaveView::FXPaste (PCM3DWave pWave)
{
   BOOL fRet = ReplaceRegion (pWave);
   delete pWave;
   return fRet;
}


/***********************************************************************************
BezierToList - Takes a set of 4 points that define a bezier curve and fills a
list with the subdivisions.

inputs
   POINT       *papBez - Pointer to 4 points
   DWORD       dwDivide - Number of times to subdivide
   BOOL        fIncLast - If TRUE then add the last point to the list, otherwise dont add it
   PCListFixed plPoints - List of POINT structures to add to
returns
   none
*/
static void BezierToList (POINT *papBez, DWORD dwDivide, BOOL fIncLast, PCListFixed plPoints)
{
   // if not more divide then done
   if (!dwDivide) {
      plPoints->Add (papBez+0);
      plPoints->Add (papBez+1);
      plPoints->Add (papBez+2);
      if (fIncLast)
         plPoints->Add (papBez+3);
      return;
   }

   // else, subdivide
   POINT pMid;
   POINT apNew[4];
   pMid.x = (papBez[1].x + papBez[2].x)/2;
   pMid.y = (papBez[1].y + papBez[2].y)/2;

   // first one
   apNew[0] = papBez[0];
   apNew[1].x = (papBez[0].x + papBez[1].x)/2;
   apNew[1].y = (papBez[0].y + papBez[1].y)/2;
   apNew[2].x = (pMid.x + papBez[1].x)/2;
   apNew[2].y = (pMid.y + papBez[1].y)/2;
   apNew[3] = pMid;
   BezierToList (apNew, dwDivide-1, FALSE, plPoints);

   // second one
   apNew[0] = pMid;
   apNew[1].x = (pMid.x + papBez[2].x)/2;
   apNew[1].y = (pMid.y + papBez[2].y)/2;
   apNew[2].x = (papBez[3].x + papBez[2].x)/2;
   apNew[2].y = (papBez[3].y + papBez[2].y)/2;
   apNew[3] = papBez[3];
   BezierToList (apNew, dwDivide-1, fIncLast, plPoints);
}


/***********************************************************************************
MouthMirror - Takes a list of of points and mirrors horizontally around the center
vertex. Assumes the points are drawn from outside the mirror plane towards the plane
and that the last point lies on the mirror plane (so it's not duplicated)

inputs
   PCListFixed       plPoints - List of POINT
   int               iMirrorX - Mirror plane
*/
static void MouthMirror (PCListFixed plPoints, int iMirrorX)
{
   DWORD dwNum, i;
   POINT *pp;
   POINT p;
   dwNum = plPoints->Num();
   for (i = dwNum-2; i < dwNum; i--) {
      pp = (POINT*) plPoints->Get(i);
      p = *pp;
      p.x = iMirrorX + (iMirrorX - p.x);
      plPoints->Add (&p);
   }
}

/***********************************************************************************
MouthFlip - Flip the order of drawing

inputs
   PCListFixed       plPoints - List of POINT
*/
static void MouthFlip (PCListFixed plPoints)
{
   DWORD dwNum, i;
   POINT *pp = (POINT*)plPoints->Get(0);
   POINT p;
   dwNum = plPoints->Num();
   for (i = 0; i < dwNum / 2; i++) {
      p = pp[i];
      pp[i] = pp[dwNum-i-1];
      pp[dwNum-i-1] = p;
   }
}

/***********************************************************************************
MouthAppend - Add the contents of the second list onto the firt

inputs
   PCListFixed       plAddTo - Add to this list
   PCListFixed       plAddFrom - Add from this list
   BOOL              fFlip - If TRUE then flips the second entry
*/
static void MouthAppend (PCListFixed plAddToo, PCListFixed plAddFrom, BOOL fFlip)
{
   DWORD dwNum, i;
   POINT *pp = (POINT*)plAddFrom->Get(0);
   dwNum = plAddFrom->Num();
   if (fFlip) {
      for (i = dwNum-1; i < dwNum; i--)
         plAddToo->Add (pp + i);
   }
   else {
      for (i = 0; i < dwNum; i++)
         plAddToo->Add (pp + i);
   }
}

/***********************************************************************************
CWaveView::DrawMouth - Draws the mouth to a HDC.

inputs
   HDC      hDCDraw - To Draw to
   RECT     *pr - Rectangle to draw into
returns
   BOOL - TRUE if can draw
*/
BOOL CWaveView::DrawMouth (HDC hDCDraw, RECT *pr)
{
   fp fLat, fVert, fTeethTop, fTeethBottom, fTongueUp, fTongueForward;
   DWORD dwTime = (DWORD)m_pTimeline->PointerGet();
   if (m_hPlayWaveOut)
      dwTime += m_pWave->m_dwSamplesPerSec / 25;  // assume takes 100th of a sec to draw
   if (!m_pWave->PhonemeAtTime (dwTime, NULL, NULL, NULL,
      &fLat, &fVert, &fTeethTop, &fTeethBottom, &fTongueForward, &fTongueUp))
      return FALSE;

   // figure out center
   POINT pCenter;
   int iMaxWidth;
   pCenter.x = (pr->right + pr->left) / 2;
   pCenter.y = (2*pr->top + pr->bottom) / 3;
   iMaxWidth = (pr->right - pr->left) / 2;

   // figure out height of top lip, bottom lip, right corner
   int iCornerRest = iMaxWidth * 3 / 4;
   int iUpperThick = iMaxWidth / 7;
   int iLowerThick = iMaxWidth / 5;
   int iTongueWidth = iMaxWidth / 2;
   int iTeethHeight = iUpperThick;
   int iTeethTop = (int)((fp)iTeethHeight * fTeethTop);
   int iTeethBottom = (int)((fp)iTeethHeight * fTeethBottom);
   int iHeightTop = -iTeethTop;
   int iHeightBottom =  (int)(fVert * (fp)iMaxWidth/3) + iTeethBottom;
   int iCornerHeight = 0 + (iHeightTop*2 + iHeightBottom) / 6;
   int iTongueHeight = (int) ((fp)iMaxWidth * (.5 - fTongueUp) / 4.0);
   if (fTongueUp < .5)
      iTongueHeight = (int)((fp)iTongueHeight * 2.0*fTongueUp + (1.0 - 2.0*fTongueUp) * (fp)iHeightBottom);
   int iCornerWidth;
   if (fLat >= 0) {
      iCornerWidth = (int)((1.0 - fLat) * (fp)iCornerRest + fLat * (fp)iMaxWidth);
      iUpperThick -= (int)((fp)iUpperThick * fLat / 10);
      iLowerThick -= (int)((fp)iLowerThick * fLat / 10);
   }
   else {
      iCornerWidth = iCornerRest + (int)(fLat * (fp)iCornerRest / 3.0);
      iUpperThick += (int)((fp)iUpperThick * (-fLat) / 2);
      iLowerThick += (int)((fp)iLowerThick * (-fLat) / 3);
   }
   
   // create some lists for the mouth
   CListFixed lUpperTop, lUpperBottom, lLowerTop, lLowerBottom, lUpper, lLower, lTongue;
   POINT apBez[7];
   lUpperTop.Init (sizeof(POINT));
   lUpperBottom.Init (sizeof(POINT));
   lLowerTop.Init (sizeof(POINT));
   lLowerBottom.Init (sizeof(POINT));
   lTongue.Init (sizeof(POINT));


   // black inside mouth
   HBRUSH hbrInside;
   hbrInside = CreateSolidBrush (RGB(94,47,0));
   FillRect (hDCDraw, pr, hbrInside);

   HPEN hPen, hPenOld;
   HBRUSH hBrush, hBrushOld;
   hPen = (HPEN) GetStockObject (BLACK_PEN);
   hPenOld = (HPEN)SelectObject (hDCDraw, hPen);
   hBrush = CreateSolidBrush (RGB(224,153,105));
   hBrushOld = (HBRUSH) SelectObject (hDCDraw, hBrush);

   // upper lip, bottom
   apBez[0].x = pCenter.x - iCornerWidth;
   apBez[0].y = pCenter.y + iCornerHeight;
   apBez[1].x = pCenter.x - iCornerWidth * 3 /4;
   apBez[1].y = pCenter.y + (iCornerHeight + iHeightTop)/2 + iUpperThick/6;
   apBez[2].x = pCenter.x - iCornerWidth * 1 /2;
   apBez[2].y = pCenter.y + (iCornerHeight + iHeightTop*2)/3;
   apBez[3].x = pCenter.x;
   apBez[3].y = pCenter.y + iHeightTop + iUpperThick/6;
   BezierToList (apBez, 3, TRUE, &lUpperBottom);
   MouthMirror (&lUpperBottom, pCenter.x);

   // upper lip top
   apBez[0].x = pCenter.x - iCornerWidth;
   apBez[0].y = pCenter.y + iCornerHeight;
   apBez[1].x = pCenter.x - iCornerWidth * 3 /4;
   apBez[1].y = pCenter.y + (iCornerHeight + iHeightTop)/2 - iUpperThick/2;
   apBez[2].x = pCenter.x - iCornerWidth * 1 /3;
   apBez[2].y = pCenter.y + iHeightTop - iUpperThick;
   apBez[3].x = pCenter.x - iCornerWidth/7;
   apBez[3].y = pCenter.y + iHeightTop - iUpperThick * 7/6;
   apBez[4].x = pCenter.x - iCornerWidth/9;
   apBez[4].y = pCenter.y + iHeightTop - iUpperThick * 8/7;
   apBez[5].x = pCenter.x - iCornerWidth/16;
   apBez[5].y = pCenter.y + iHeightTop - iUpperThick * 9/10;
   apBez[6].x = pCenter.x;
   apBez[6].y = pCenter.y + iHeightTop - iUpperThick * 9/10;
   BezierToList (apBez, 3, FALSE, &lUpperTop);
   BezierToList (apBez + 3, 2, TRUE, &lUpperTop);
   MouthMirror (&lUpperTop, pCenter.x);
   lUpper.Init (sizeof(POINT), lUpperTop.Get(0), lUpperTop.Num());
   MouthAppend (&lUpper, &lUpperBottom, TRUE);

   // lower lip top
   apBez[0].x = pCenter.x - iCornerWidth;
   apBez[0].y = pCenter.y + iCornerHeight;
   apBez[1].x = pCenter.x - iCornerWidth * 3 /4;
   apBez[1].y = pCenter.y + (iCornerHeight + iHeightBottom)/2 - iLowerThick/4;
   apBez[2].x = pCenter.x - iCornerWidth * 1 /3;
   apBez[2].y = pCenter.y + iHeightBottom;
   apBez[3].x = pCenter.x;
   apBez[3].y = pCenter.y + iHeightBottom;
   BezierToList (apBez, 3, TRUE, &lLowerTop);
   MouthMirror (&lLowerTop, pCenter.x);

   // lower lip bototm
   apBez[0].x = pCenter.x - iCornerWidth;
   apBez[0].y = pCenter.y + iCornerHeight;
   apBez[1].x = pCenter.x - iCornerWidth * 3/4;
   apBez[1].y = pCenter.y + (iCornerHeight + iHeightBottom)/2 + iLowerThick*2/3;
   apBez[2].x = pCenter.x - iCornerWidth * 1 /3;
   apBez[2].y = pCenter.y + iHeightBottom + iLowerThick;
   apBez[3].x = pCenter.x;
   apBez[3].y = pCenter.y + iHeightBottom + iLowerThick;
   BezierToList (apBez, 3, TRUE, &lLowerBottom);
   MouthMirror (&lLowerBottom, pCenter.x);
   lLower.Init (sizeof(POINT), lLowerBottom.Get(0), lLowerBottom.Num());
   MouthAppend (&lLower, &lLowerTop, TRUE);

   // tongue
   HBRUSH hbrTongue;
   hbrTongue = CreateSolidBrush(RGB(148,64,1));
   SelectObject (hDCDraw, hbrTongue);
   apBez[0].x = pCenter.x - iTongueWidth;
   apBez[0].y = pCenter.y + iTongueWidth;
   apBez[1].x = pCenter.x - iTongueWidth * 3 /4;
   apBez[1].y = pCenter.y + (iTongueHeight*3 + iTongueWidth)/4;
   apBez[2].x = pCenter.x - iTongueWidth * 1 /2;
   apBez[2].y = pCenter.y + iTongueHeight;
   apBez[3].x = pCenter.x;
   apBez[3].y = pCenter.y + iTongueHeight;
   BezierToList (apBez, 3, TRUE, &lTongue);
   MouthMirror (&lTongue, pCenter.x);
   Polygon (hDCDraw, (POINT*) lTongue.Get(0), lTongue.Num());

   // teeth
   HBRUSH hbrTeeth;
   RECT rt;
   hbrTeeth = CreateSolidBrush (RGB(0xf0,0xf0,0xe0));
   SelectObject (hDCDraw, hbrTeeth);
   rt.left = pCenter.x - iMaxWidth;
   rt.right = pCenter.x + iMaxWidth;
   rt.top = pCenter.y - iTeethHeight;
   rt.bottom = pCenter.y;
   Rectangle (hDCDraw, rt.left, rt.top, rt.right, rt.bottom);
   rt.bottom = pCenter.y + iHeightBottom + 1;
   rt.top = pCenter.y + iHeightBottom - iTeethBottom;
   Rectangle (hDCDraw, rt.left, rt.top, rt.right, rt.bottom);

   // fill the background
   HBRUSH hbrSkin;
   HPEN hPenSkin;
   hbrSkin = CreateSolidBrush (RGB(215, 191, 138));
   hPenSkin = CreatePen (PS_SOLID, 0, RGB(215, 191, 138));
   SelectObject (hDCDraw, hPenSkin);
   SelectObject (hDCDraw, hbrSkin);
   CListFixed lSkin;
   lSkin.Init (sizeof(POINT), lLowerBottom.Get(0), lLowerBottom.Num());
   MouthAppend (&lSkin, &lUpperTop, TRUE);
   // points around
   POINT p;
   p.x = pr->left;
   p.y = pr->top;
   lSkin.Add (&p);
   p.x = pr->right;
   lSkin.Add (&p);
   p.y = pr->bottom;
   lSkin.Add (&p);
   p.x = pr->left;
   lSkin.Add (&p);
   p.y = pr->top;
   lSkin.Add (&p);
   Polygon (hDCDraw, (POINT*) lSkin.Get(0), lSkin.Num());
   SelectObject (hDCDraw, hPen);


   // draw lips
   SelectObject (hDCDraw, hBrush);
   Polygon (hDCDraw, (POINT*) lLower.Get(0), lLower.Num());
   Polygon (hDCDraw, (POINT*) lUpper.Get(0), lUpper.Num());

   SelectObject (hDCDraw, hPenOld);
   SelectObject (hDCDraw, hBrushOld);
   DeleteObject (hBrush);
   DeleteObject (hbrInside);
   DeleteObject (hbrSkin);
   DeleteObject (hPenSkin);
   DeleteObject (hbrTeeth);
   DeleteObject (hbrTongue);
   return TRUE;
}

/***********************************************************************************
CWaveView::MouthWndProc - Window procedure for the mouth display
*/
LRESULT CWaveView::MouthWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   //if (uMsg == gdwMouseWheel)
   //   uMsg = WM_MOUSEWHEEL;

   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWndMouth = hWnd;
      }
      return 0;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC = BeginPaint (hWnd, &ps);
         HDC hDCDraw = CreateCompatibleDC (hDC);
         RECT r;
         GetClientRect (hWnd, &r);
         HBITMAP hBit = CreateCompatibleBitmap (hDC, r.right - r.left, r.bottom - r.top);
         SelectObject (hDCDraw, hBit);

         // if there's no phoneme data then warn
         if ((m_pWave->m_lWVPHONEME.Num() < 2) || !DrawMouth(hDCDraw, &r)) {
            // fill the background
            HBRUSH hBrush;
            hBrush = CreateSolidBrush (RGB(215, 191, 138));
            FillRect (hDCDraw, &r, hBrush);
            DeleteObject (hBrush);

            // draw the text
            HFONT hFont, hFontOld;
            LOGFONT  lf;
            memset (&lf, 0, sizeof(lf));
            lf.lfHeight = -MulDiv(10, GetDeviceCaps(hDCDraw, LOGPIXELSY), 72); 
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
            strcpy (lf.lfFaceName, "Arial");
            hFont = CreateFontIndirect (&lf);
            hFontOld = (HFONT) SelectObject (hDCDraw, hFont);
            SetTextColor (hDCDraw, RGB(0,0,0));
            SetBkMode (hDCDraw, TRANSPARENT);
            DrawText(hDCDraw,
               "You must hand-enter phonemes or run speech recognition for the mouth to work.",
               -1, &r, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            SelectObject (hDCDraw, hFontOld);
            DeleteObject (hFont);
         }

         BitBlt (hDC, r.left, r.top, r.right - r.left, r.bottom - r.top,
            hDCDraw, 0, 0, SRCCOPY);
         DeleteDC (hDCDraw);
         DeleteObject (hBit);
         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_DESTROY:
      m_hWndMouth = NULL;
      break;
   }

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/***********************************************************************************
CWaveView::WordPhonemeAttach - This moves a word or phoneme, and attaches it to its
pair (word/phoneme).

inputs
   DWORD          dwIndex - Index to move
   BOOL           fWord - TRUE if it's a word being moved, FALSE if phoneme
   DWORD          dwNewSample - New sample that want to move it to.
returns
   none
*/
void CWaveView::WordPhonemeAttach (DWORD dwIndex, BOOL fWord, DWORD dwNewSample)
{
   // get the current one
   PWVPHONEME pPhoneCur = NULL, pPhoneLeft = NULL, pPhoneRight = NULL;
   PWVWORD pWordCur = NULL, pWordLeft = NULL, pWordRight = NULL;
   if (fWord) {
      pWordCur = (PWVWORD) m_pWave->m_lWVWORD.Get(dwIndex);
      pWordLeft = dwIndex ? (PWVWORD) m_pWave->m_lWVWORD.Get(dwIndex-1) : NULL;
      pWordRight = (PWVWORD) m_pWave->m_lWVWORD.Get(dwIndex+1);
   }
   else {
      pPhoneCur = (PWVPHONEME) m_pWave->m_lWVPHONEME.Get(dwIndex);
      pPhoneLeft = dwIndex ? (PWVPHONEME) m_pWave->m_lWVPHONEME.Get(dwIndex-1) : NULL;
      pPhoneRight = (PWVPHONEME) m_pWave->m_lWVPHONEME.Get(dwIndex+1);
   }
   if (!pWordCur && !pPhoneCur)
      return;  // shouldnt happen

   // see if has a partner
   DWORD dwPartIndex = -1;
   DWORD i;
   PWVPHONEME pPhonePart = NULL;
   PWVWORD pWordPart = NULL;
   if (pWordCur) {
      // loop through phonemes
      for (i = 0; i < m_pWave->m_lWVPHONEME.Num(); i++) {
         PWVPHONEME pp = (PWVPHONEME) m_pWave->m_lWVPHONEME.Get(i);
         if (pp->dwSample == pWordCur->dwSample) {
            pPhonePart = pp;
            dwPartIndex = i;
            pPhoneLeft = dwPartIndex ? (PWVPHONEME) m_pWave->m_lWVPHONEME.Get(dwPartIndex-1) : NULL;
            pPhoneRight = (PWVPHONEME) m_pWave->m_lWVPHONEME.Get(dwPartIndex+1);
            break;
         }
      } // i
   }
   else {
      // loop through all the words...
      for (i = 0; i < m_pWave->m_lWVWORD.Num(); i++) {
         PWVWORD pp = (PWVWORD) m_pWave->m_lWVWORD.Get(i);
         if (pp->dwSample == pPhoneCur->dwSample) {
            pWordPart = pp;
            dwPartIndex = i;
            pWordLeft = dwPartIndex ? (PWVWORD) m_pWave->m_lWVWORD.Get(dwPartIndex-1) : NULL;
            pWordRight = (PWVWORD) m_pWave->m_lWVWORD.Get(dwPartIndex+1);
            break;
         }
      } // i
   }


   // find the limits of movement...
   DWORD dwMin, dwMax;
   dwMin = 0;
   dwMax = m_pWave->m_dwSamples;

   // blocking to left in own type...
   if (pWordLeft)
      dwMin = max(dwMin, pWordLeft->dwSample+1);
   if (pPhoneLeft)
      dwMin = max(dwMin, pPhoneLeft->dwSample+1);
   if (pWordRight)
      dwMax = min(dwMax, pWordRight->dwSample-1);
   if (pPhoneRight)
      dwMax = min(dwMax, pPhoneRight->dwSample-1);

   // else, move
   UndoAboutToChange();

   // what's the sample
   dwNewSample = max(dwNewSample, dwMin);
   dwNewSample = min(dwNewSample, dwMax);
   if (pWordCur)
      pWordCur->dwSample = dwNewSample;
   if (pPhoneCur)
      pPhoneCur->dwSample = dwNewSample;
   if (pWordPart)
      pWordPart->dwSample = dwNewSample;
   if (pPhonePart)
      pPhonePart->dwSample = dwNewSample;

   // invalidate
   PWAVEPANE pwp;
   for (i = 0; i < m_lWAVEPANE.Num(); i++) {
      pwp = (PWAVEPANE) m_lWAVEPANE.Get(i);
      if ((pwp->dwType == WP_WORD) || (pwp->dwType == WP_PHONEMES))
         InvalidateRect (pwp->hWnd, NULL, FALSE);
   }
}


/**********************************************************************************
CWaveView::Record - Brings up the record UI dialog and records the audio.

inputs
   HWND           hWnd - to display on top of
*/
void CWaveView::Record (HWND hWnd)
{
   if (m_hPlayWaveOut)
      return;  // cant record while playing

   DWORD dwReplace;
   PCM3DWave pNewWave = m_pWave->Record (hWnd, FALSE, &dwReplace);
   if (!pNewWave)
      return;

   // what to replace in the current audio?
   DWORD dwReplaceStart, dwReplaceEnd;
   switch (dwReplace) {
   case 0:  // selection
      dwReplaceStart = (DWORD) min(m_iSelStart, m_iSelEnd);
      dwReplaceEnd = (DWORD)max(m_iSelStart, m_iSelEnd);

      dwReplaceStart = min(dwReplaceStart, m_pWave->m_dwSamples);
      dwReplaceEnd = min(dwReplaceEnd, m_pWave->m_dwSamples);
      break;
   case 1:  // entire wave
      dwReplaceStart = 0;
      dwReplaceEnd = m_pWave->m_dwSamples;
      break;
   case 2:  // end of wave
      dwReplaceStart = dwReplaceEnd = m_pWave->m_dwSamples;
      break;
   }

   ReplaceRegion (dwReplaceStart, dwReplaceEnd, pNewWave, TRUE);

   delete pNewWave;
}


/****************************************************************************
FXTTSPage
*/
BOOL FXTTSPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCWaveView pv = (PCWaveView) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      PCEscControl pControl;
      pControl = pPage->ControlFind (L"soundslike");
      if (pControl)
         pControl->AttribSet (Text(), (PWSTR) pv->m_pWave->m_memSpoken.p);

      pControl = pPage->ControlFind (L"nottspcm");
      if (pControl)
         pControl->AttribSetBOOL (Checked(), pv->m_fDisablePCM);

      ComboBoxSet (pPage, L"ttsquality", pv->m_iTTSQuality);

      pPage->Message (ESCM_USER+83);   // fill in info
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         DWORD dwVal;
         psz = p->pControl->m_pszName;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"ttsquality")) {
            if ((int)dwVal == pv->m_iTTSQuality)
               return TRUE;   // no change
            pv->m_iTTSQuality = (int)dwVal;
            return TRUE;
         }
      }
      return 0;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"soundslike")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            pv->UndoAboutToChange();
            if (!pv->m_pWave->m_memSpoken.Required (dwNeed))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR) pv->m_pWave->m_memSpoken.p,
               (DWORD)pv->m_pWave->m_memSpoken.m_dwAllocated, &dwNeed);
            return TRUE;
         }
      }
      break;

   case ESCM_USER+83:
      {
         TTSCacheDefaultGet (pPage, L"trainfile");
         //PCEscControl pControl;

         // fill in the file
         //pControl = pPage->ControlFind (L"trainfile");
         //PWSTR psz = pv->m_szTTS;
         //if (!psz[0])
         //   psz = L"(No TTS file)";
         //if (pControl)
         //   pControl->AttribSet (Text(), psz);

      }
      return 0;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"recog")) {

            pv->UndoCache();
            pv->UndoAboutToChange();

            PCMTTS pTTS;
            pTTS = TTSCacheOpen (NULL, TRUE, FALSE);
            if (!pTTS) {
               pPage->MBWarning (L"Couldn't open the TTS file.");
               return TRUE;
            }

            // do tts
            BOOL fRet;
            {
               CProgress Progress;

               Progress.Start (pPage->m_pWindow->m_hWnd, "Synthesizing voice...");

               PCEscControl pControl = pPage->ControlFind (L"tagged");
               BOOL fTagged = FALSE;
               if (pControl)
                  fTagged = pControl->AttribGetBOOL (Checked());

               DWORD dwTime = GetTickCount();
               fRet = pTTS->SynthGenWave (pv->m_pWave, pv->m_pWave->m_dwSamplesPerSec,
                  (PWSTR)pv->m_pWave->m_memSpoken.p, fTagged, pv->m_iTTSQuality, pv->m_fDisablePCM, &Progress);

#if 0 // to test
               WCHAR szTemp[64];
               swprintf (szTemp, L"Speak time = %d", (int)(GetTickCount()-dwTime));
               pPage->MBInformation (szTemp);
#endif

            }

            if (!fRet)
               pPage->MBWarning (L"Synthesis didn't work.",
                  L"The voice file may not have loaded, or if you have tagged text on, "
                  L"some of the tags may be wrong.");

            // release tts
            TTSCacheClose (pTTS);


            pv->m_iViewSampleLeft = pv->m_iPlayLimitMin =pv->m_iSelStart = pv->m_iPlayCur = 0;
            pv->m_iViewSampleRight = pv->m_iPlayLimitMax = pv->m_iSelEnd = (int)pv->m_pWave->m_dwSamples;
            pv->InvalidateDisplay();
            pv->UpdateScroll();
            pv->EnableButtonsThatNeedSel();
            pv->UndoCache();

            if (fRet)
               pPage->Exit (Back());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"open")) {
            if (TTSCacheDefaultUI (pPage->m_pWindow->m_hWnd))
               pPage->Message (ESCM_USER+83);   // so refresh display
            //WCHAR szw[256];
            //wcscpy (szw, pv->m_szTTS);
            //if (!TTSFileOpenDialog (pPage->m_pWindow->m_hWnd, szw,
            //   sizeof(szw)/sizeof(WCHAR), FALSE))
            //   return TRUE;

            // save the file
            //wcscpy (pv->m_szTTS, szw);

            //pPage->Message (ESCM_USER+83);   // so refresh display
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"nottspcm")) {
            pv->m_fDisablePCM = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Text-to-speech";
            return TRUE;
         }
      }
      break;



   };


   return FALSE;  // dont do DefPage (pPage, dwMessage, pParam); since captures enter
}

/*********************************************************************************
CWaveView::FXTTS - Bring up the settings page.

inputs
   HWND        hWnd - window to display from
returns
   BOOL - TRUE if success
*/
BOOL CWaveView::FXTTS (HWND hWnd)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (hWnd, &r);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXTTS, FXTTSPage, this);

   return TRUE;
}




/****************************************************************************
FXTTSBatchPage
*/
BOOL FXTTSBatchPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCWaveView pv = (PCWaveView) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      ComboBoxSet (pPage, L"digits", 3);

      PCEscControl pControl;
      pControl = pPage->ControlFind (L"nottspcm");
      if (pControl)
         pControl->AttribSetBOOL (Checked(), pv->m_fDisablePCM);

      ComboBoxSet (pPage, L"ttsquality", pv->m_iTTSQuality);

      pPage->Message (ESCM_USER+83);   // fill in info
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         DWORD dwVal;
         psz = p->pControl->m_pszName;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"ttsquality")) {
            if ((int)dwVal == pv->m_iTTSQuality)
               return TRUE;   // no change
            pv->m_iTTSQuality = (int)dwVal;
            return TRUE;
         }
      }
      return 0;

   case ESCM_USER+83:
      {
         TTSCacheDefaultGet (pPage, L"trainfile");
         //PCEscControl pControl;

         // fill in the file
         //pControl = pPage->ControlFind (L"trainfile");
         //PWSTR psz = pv->m_szTTS;
         //if (!psz[0])
         //   psz = L"(No TTS file)";
         //if (pControl)
         //   pControl->AttribSet (Text(), psz);

      }
      return 0;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"recog")) {

            PCMTTS pTTS;
            pTTS = TTSCacheOpen (NULL, TRUE, FALSE);
            if (!pTTS) {
               pPage->MBWarning (L"Couldn't open the TTS file.");
               return TRUE;
            }

            // get the text
            DWORD dwNeed = 0;
            CMem memText;
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"soundslike");
            if (!pControl)
               return TRUE;
            pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!memText.Required (dwNeed))
               return TRUE;
            pControl->AttribGet (Text(), (PWSTR) memText.p,
               (DWORD)memText.m_dwAllocated, &dwNeed);
            PWSTR pszText = (PWSTR)memText.p;
            DWORD dwLen = (DWORD)wcslen(pszText);

            // get the wave file
            WCHAR szPrefix[256];
            szPrefix[0] = 0;
            pControl = pPage->ControlFind (L"file");
            if (!pControl)
               return TRUE;
            pControl->AttribGet (Text(), szPrefix, sizeof(szPrefix), &dwNeed);

            // get the combo box value
            DWORD dwDigits = 1;
            pControl = pPage->ControlFind (L"digits");
            if (pControl)
               dwDigits = (DWORD)pControl->AttribGetInt (CurSel()) + 1;

            BOOL fNormalize = FALSE;
            if (pControl = pPage->ControlFind (L"normalize"))
               fNormalize = pControl->AttribGetBOOL (Checked());

            BOOL fAutoName = FALSE;
            if (pControl = pPage->ControlFind (L"autoname"))
               fAutoName = pControl->AttribGetBOOL (Checked());

            // do tts
            BOOL fRet = TRUE;
            DWORD dwLine = 1;
            CM3DWave Wave;
            DWORD dwTime = GetTickCount();
            {
               CProgress Progress;

               Progress.Start (pPage->m_pWindow->m_hWnd, "Synthesizing voice files...");

               while (pszText && fRet) {
                  // progress
                  Progress.Update ((fp)(DWORD)((PBYTE)pszText - (PBYTE)memText.p) / (fp)(dwLen * sizeof(WCHAR)));

                  // find the end
                  PWSTR pszN = wcschr (pszText, L'\n');
                  PWSTR pszR = wcschr (pszText, L'\r');
                  PWSTR pszMin, pszMax;
                  if (pszR && pszN)
                     pszMin = min(pszR, pszN);
                  else if (pszR)
                     pszMin = pszR;
                  else if (pszN)
                     pszMin = pszN;
                  else
                     pszMin = NULL;
                  if (pszMin) {
                     if ((pszMin[1] == L'\r') || (pszMin[1] == L'\n'))
                        pszMax = pszMin + 2; // since \r\n together
                     else
                        pszMax = pszMin + 1; // only one
                  }
                  else
                     pszMax = NULL;

                  // blank this wave
                  Wave.New (pv->m_pWave->m_dwSamplesPerSec, pv->m_pWave->m_dwChannels);

                  // isolate this
                  WCHAR wTemp = pszMin ? pszMin[0] : 0;
                  if (pszMin)
                     pszMin[0] = 0;

                  // only if not empty
                  if (pszText[0]) {
                     // potentially try to pull out the file name
                     PWSTR pszAutoFile = NULL;
                     if (fAutoName) {
                        PWSTR pszSpace = wcschr (pszText, L' ');
                        if (pszSpace) {
                           pszAutoFile = pszText;
                           pszSpace[0] = 0;
                           pszText = pszSpace + 1;
                        }
                     }

                     pTTS->WordRankHistory (NULL); // so speak each line ignoring the previous
                     fRet = pTTS->SynthGenWave (&Wave, Wave.m_dwSamplesPerSec, pszText, FALSE, pv->m_iTTSQuality, pv->m_fDisablePCM, NULL);

                     // BUGFIX - Also write the text spoken
                     MemZero (&Wave.m_memSpoken);
                     MemCat (&Wave.m_memSpoken, pszText);

                     // normalize
                     if (fNormalize) {
                        short sMax = Wave.FindMax();
                        sMax = max(1,sMax);
                        fp fStart = 32767.0 / (fp)sMax;

                        Wave.FXVolume (fStart, fStart, NULL);
                     }
                        
                     // if worked then save
                     if (fRet) {
                        WCHAR szwTemp[256], szwPrint[128];
                        if (pszAutoFile)
                           swprintf (szwTemp, L"%s%s.wav", szPrefix, pszAutoFile);
                        else {
                           swprintf (szwPrint, L"%%s%%.%dd.wav", (int)dwDigits);
                           swprintf (szwTemp, szwPrint, szPrefix, (int)dwLine);
                        }
                        dwLine++;
                        WideCharToMultiByte (CP_ACP, 0, szwTemp, -1, Wave.m_szFile, sizeof(Wave.m_szFile), 0, 0);
                        fRet = Wave.Save (FALSE, NULL);
                     }
                  }

                  if (pszMin)
                     pszMin[0] = wTemp;
                  if (!pszMax)
                     break;   // nothing left
                  pszText = pszMax;
               } // while have text
            } // progress

            WCHAR szOutput[256];
            swprintf (szOutput, L"The processing too %d seconds.", (int)(GetTickCount() - dwTime) / 1000);
            pPage->MBInformation (szOutput);

            if (!fRet)
               pPage->MBWarning (L"Synthesis didn't work.",
                  L"The voice file may not have loaded, or one or more of the files may not have saved.");

            // release tts
            TTSCacheClose (pTTS);

            if (fRet)
               pPage->Exit (Back());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"open")) {
            if (TTSCacheDefaultUI (pPage->m_pWindow->m_hWnd))
               pPage->Message (ESCM_USER+83);   // so refresh display
            //WCHAR szw[256];
            //wcscpy (szw, pv->m_szTTS);
            //if (!TTSFileOpenDialog (pPage->m_pWindow->m_hWnd, szw,
            //   sizeof(szw)/sizeof(WCHAR), FALSE))
            //   return TRUE;

            // save the file
            //wcscpy (pv->m_szTTS, szw);

            //pPage->Message (ESCM_USER+83);   // so refresh display
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"nottspcm")) {
            pv->m_fDisablePCM = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }


      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Text-to-speech";
            return TRUE;
         }
      }
      break;



   };


   return FALSE;  // dont do DefPage (pPage, dwMessage, pParam); since captures enter
}

/*********************************************************************************
CWaveView::FXTTSBatch - Bring up the settings page.

inputs
   HWND        hWnd - window to display from
returns
   BOOL - TRUE if success
*/
BOOL CWaveView::FXTTSBatch (HWND hWnd)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (hWnd, &r);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLFXTTSBATCH, FXTTSBatchPage, this);

   return TRUE;
}



