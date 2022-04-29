/*************************************************************************************
CMainWindow.cpp - Code for displaying the main client window.

begun 29/2/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <zmouse.h>
#include <shlobj.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"


#define BLACKTRANSCRIPT          // if set, then black transcript

#define USINGSMALLDISPLAYWINDOW        // if set then 3D images will have lower resolution
   // BUGFIX - Try defaulting to larger explore window
   // BUGFIX - Turned back on so faster drawing and less memory

// VISIMAGETIMER - Information for timer
typedef struct {
   PCVisImage        pvi;     // vis-image to invalidate status of
   DWORD             dwTime;  // get-tick count
} VISIMAGETIMER, *PVISIMAGETIMER;

#define QUICKRENDERQUALITY       1     // quick renders are two render quality categories lower
   // BUGFIX - Changed to 1 quality lower since changed to mono-quality

// since not defined
#define INPUT_MOUSE     0

// #define DARKBITMAPSCALE(x)          ((x)/3*2)     // N/8th the darkness of the original
// #define DARKEXTRABITMAPSCALE(x)          ((x)/6)     // N/8th the darkness of the original

#if 0 // defined when change winver
typedef struct tagMOUSEINPUT {
  LONG    dx;
  LONG    dy;
  DWORD   mouseData;
  DWORD   dwFlags;
  DWORD   time;
  ULONG_PTR   dwExtraInfo;
} MOUSEINPUT, *PMOUSEINPUT;
typedef struct tagKEYBDINPUT {
  WORD      wVk;
  WORD      wScan;
  DWORD     dwFlags;
  DWORD     time;
  ULONG_PTR dwExtraInfo;
} KEYBDINPUT, *PKEYBDINPUT;
typedef struct tagHARDWAREINPUT {
  DWORD   uMsg;
  WORD    wParamL;
  WORD    wParamH;
} HARDWAREINPUT, *PHARDWAREINPUT;
typedef struct tagINPUT {
  DWORD   type;
  union {
      MOUSEINPUT      mi;
      KEYBDINPUT      ki;
      HARDWAREINPUT   hi;
  };
} INPUT, *PINPUT;
WINUSERAPI
UINT
WINAPI
SendInput(
    IN UINT    cInputs,     // number of input in the array
    IN PINPUT pInputs,     // array of inputs
    IN int     cbSize);     // sizeof(INPUT)
#endif // 0

#define MAXWORLDCACHE               5        // don't cache the data for any more than 5 worlds


#define TIMER_MESSAGEDISABLED       2040     // timer so know that message is disabled
#define TIMER_PROGRESS              2041     // update progress bars
#define TIMER_VERBTOOLTIPUPDATE     2042     // occaionally update the tooltip
#define TIMER_ANIMATE               2043     // user this timer for animation
#define TIMER_VOICECHAT             2044     // voice chat timer
#define TIMER_USAGE                 2045     // keep track of hoe many minutes have spent in the game
#define TIMER_IMAGECACHE            2046     // timer to release image cache
#define TIMER_HOTSPOTTOOLTIPUPDATE  2047     // occaionally update the tooltip
#define TIMER_RANDOMACTIONS         2050     // randomly send action to server

// in flash window
#define TIMER_FLASHWINDOW           2048     // flash the window

#define PREFSPEEDUNDEF              -1001
#define VCTIMER_INTERVAL            50       // every 50 milliseconds

#define WM_MIDIFROMESCARPMENT       (WM_USER+184)
#define WM_PACKETSENDERROR          (WM_USER+185)
#define WM_DIRECTSOUNDERROR         (WM_USER+186)
#define WM_RECORDDIR                (WM_USER+187)
#define WM_SHOWSETTINGSUI           (WM_USER+188)

#define TABSPACING                  25       // spacing between top and bottom of tab, excluding line

#define PROGRESSTIME                250      // for TIMER_PROGRESS
#define ANIMATETIME                 50       // number of milliseconds between animation
      // BUGFIX - Changed from 75 to 50 ms
#define USAGETIME                   (15*60*1000)   // every 15 minutes

#define TUTORIALCLOSEENOUGH         2        // 4 pixels



// ILTDOWNLOADQUEUE - Queue of load-data requests sent to the server
typedef struct {
   __int64              iTime;      // time when this was sent out
   // followed by PWSTR with the string
} ILTDOWNLOADQUEUE, *PILTDOWNLOADQUEUE;

// ILTWAVEOUTCACHE - Cache of waves that have been spoken
typedef struct {
   DWORD                dwQuality;  // quality level of the TTS
   PWSTR                pszString;  // string. Must free with ESCFREE()
   PCM3DWave            pWave;      // wave cached
} ILTWAVEOUTCACHE, *PILTWAVEOUTCACHE;

// HYPNOEFFECTINFO - Information abotu a hypno effect
typedef struct {
   fp                   fDuration;  // duration remaining, in seconds
   fp                   fPriority;  // priority level
} HYPNOEFFECTINFO, *PHYPNOEFFECTINFO;

int giCPUSpeed = 0;     // CPU speed
BOOL gfRenderCache = FALSE; // using render cache
static int     giDefaultResolution = 0;    // default resolution given the CPU speed
static int     giDefaultResolutionLow = 0; // default resolution for low-quality quick render
static DWORD   gdwDefaultShadowFlags;  // default shadow flags given the cpu speed
static DWORD   gdwDefaultShadowFlagsLow;  // default low-quality quick render
static DWORD   gdwServerSpeed = 0;         // speed sent to the server, from 0..4
static int     giDefaultTextureDetail = -2; // default texture detail, 0 = normal, -1 = low, 1 = high
static BOOL    gfLipSync = FALSE;              // default to whether or not lip sync
static BOOL    gfLipSyncLow = FALSE;           // default low-quality quick-render
static DWORD   gdwMovementTransition = 0;  // default movmement trasnitons

PWSTR gpszCircumrealityClient = L"Circumreality";
PWSTR gpszmXac = L"mXac";

PWSTR gpszNarrator = L"Narrator";

PWSTR gpszMainDisplayWindow = L"Main";
static PWSTR gpszCommandLine = L"CommandLine";
static PWSTR gpszMenu = L"Menu";
static PWSTR gpszVerb = L"Verb";
static PWSTR gpszVerbChat = L"VerbChat";
static PWSTR gpszProgress = L"Progress";
static PWSTR gpszTranscript = L"Transcript";
static PWSTR gpszPlayerAction = L"Action";
static PWSTR gpszMiscInfo = L"MiscInfo";
static PWSTR gpszResolution = L"Resolution";
static PWSTR gpszShadowsFlags = L"ShadowsFlags";
static PWSTR gpszResolutionLow = L"ResolutionLow";
static PWSTR gpszShadowsFlagsLow = L"ShadowsFlagsLow";
static PWSTR gpszVoiceDisguise = L"VoiceDisguise";
static PWSTR gpszPreferredQualityMono = L"PreferredQualityMono";
static PWSTR gpszSlideLocked = L"SlideLocked";
static PWSTR gpszServerSpeed = L"ServerSpeed";
static PWSTR gpszMovementTransition = L"MovementTransition";
static PWSTR gpszTextureDetail = L"TextureDetail";
static PWSTR gpszLipSync = L"LipSync";
static PWSTR gpszLipSyncLow = L"LipSyncLow";
static PWSTR gpszSubtitleFontSize = L"SubtitleFontSize";
static PWSTR gpszPowerSaver = L"PowerSaver";
static PWSTR gpszArtStyle = L"ArtStyle";
static PWSTR gpszMinimizeIfSwitch = L"MinimizeIfSwitch";
static PWSTR gpszSubtitleSpeech = L"SubtitleSpeech";
static PWSTR gpszSubtitleText = L"SubtitleText";
static PWSTR gpszTTSAutomute = L"TTSAutomute";
static PWSTR gpszTTSQuality = L"TTSQuality";
static PWSTR gpszDisablePCM = L"DisablePCM";

static PWSTR gapszTabNames[] = {L"explore", L"chat", L"combat", L"misc"};

PWSTR gpszSettingsTempEnabled = L"<p/><font color=#008000>Temporarily enabled for Circumreality's 5-hour trial period.</font>";
PWSTR gpszSettingsDisabled = L"<p/><font color=#800000>Disabled. Only supported in the full (paid) version of Circumreality.</font>";

static CMem gMemTemp;

BOOL gfChildLocIgnoreSave = FALSE;       // if TRUE then ignore changes to the windows size
                                 // as far as the master settings
DWORD gdwBeepWindowBeepShowDisable = 0;  // temporarily increase to disable beeps
static char gszRegRenderCache[] = "RenderCache";

/****************************************************************8
GetRegRenderCache - Get the value for whether use the render cache or not.

inputs
   none
returns
   DWORD - current reg key
*/
DWORD GetRegRenderCache (void)
{
   DWORD dwKey;
   dwKey = 1;  // default to being on

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return dwKey;

   DWORD dwSize, dwType;
   dwSize = sizeof(DWORD);
   RegQueryValueEx (hKey, gszRegRenderCache, NULL, &dwType, (LPBYTE) &dwKey, &dwSize);

   RegCloseKey (hKey);

   return dwKey;
}

/****************************************************************8
WriteRegRenderCache - Write the current value for using the render cache

inputs
   DWORD          dwKey - Value
returns
   none
*/
void WriteRegRenderCache (DWORD dwKey)
{
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   RegSetValueEx (hKey, gszRegRenderCache, 0, REG_DWORD, (BYTE*) &dwKey, sizeof(dwKey));

   RegCloseKey (hKey);

   return;
}


/************************************************************************************
AppDataDirGet - Gets the application data directory, and makes sure it's created.

inputs
   PWSTR       psz - String to fill in. Must be MAX_PATH. Will be of the form, "c:\\path\\"
returns
   BOOL - TRUE if success
*/
BOOL AppDataDirGet (PWSTR psz)
{
   if (!SUCCEEDED(SHGetFolderPathW (NULL, CSIDL_FLAG_CREATE | CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, psz)))
      return FALSE;

   // append CircumReality
   if (wcslen(psz) + wcslen(gpszCircumrealityClient) + wcslen(gpszmXac) + 4 > MAX_PATH)
      return FALSE;
   wcscat (psz, L"\\");
   wcscat (psz, gpszmXac);
   // make sure directory exists
   CreateDirectoryW (psz, NULL);

   wcscat (psz, L"\\");
   wcscat (psz, gpszCircumrealityClient);

   // make sure directory exists
   CreateDirectoryW (psz, NULL);

   // add another backslash just to make easier
   wcscat (psz, L"\\");

#if 0 // to test
   EscMessageBox (NULL, gpszCircumrealityClient,  L"Where the files are.", psz, MB_OK);
#endif

   return TRUE;
}


/************************************************************************************
CacheFilenameGet - Gets the filename of a cached file.

inputs
   DWORD          dwCache - Cache number, 0+.
   DWORD          dwMFNum - MF number, 1..4.
   PWSTR          pszFile - Filled in with the file name. Must be MAX_PATH
returns
   BOOL - TRUE if success
*/
BOOL CacheFilenameGet (DWORD dwCache, DWORD dwMFNum, PWSTR pszFile)
{
   if (!AppDataDirGet(pszFile))
      return FALSE;

   swprintf (pszFile + wcslen(pszFile), L"Cache%d.mf%d", (int)dwCache, (int)dwMFNum);

   return TRUE;
}

/************************************************************************************
CacheInfoGet - Gets information about the cache.

inputs
   DWORD          dwCache - Cache number, 0+
   PWSTR          pszFile - Filled in with the filename of the cache.
                     Must be MAX_PATH. Empty string if none.
   FILETIME       *pFT - Filled in with the filetime last used, 0 if not used
returns
   none
*/
void CacheInfoGet (DWORD dwCache, PWSTR pszFile, FILETIME *pFT)
{
   // make sure zeroed out
   pszFile[0] = 0;
   memset (pFT, 0, sizeof(*pFT));

   WCHAR szFileKey[64], szFileTimeKey[64];
   swprintf (szFileKey, L"CacheFile%d", (int)dwCache);
   swprintf (szFileTimeKey, L"CacheFileTime%d", (int)dwCache);

   // load from registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      DWORD dwSize, dwType;
      dwSize = sizeof(WCHAR) * MAX_PATH;
      if ( (ERROR_SUCCESS != RegQueryValueExW (hKey, szFileKey, NULL, &dwType, (LPBYTE) pszFile, &dwSize)) ||
         (dwType != REG_SZ) )
            pszFile[0] = 0;

      dwSize = sizeof(*pFT);
      if ( (ERROR_SUCCESS != RegQueryValueExW (hKey, szFileTimeKey, NULL, &dwType, (LPBYTE) pFT, &dwSize)) ||
         (dwType != REG_BINARY) )
         memset (pFT, 0, sizeof(*pFT));
      RegCloseKey (hKey);
   }
}


/************************************************************************************
CacheInfoSet - Sets the cache info.

inputs
   DWORD          dwCache - Cache number, 0+
   PWSTR          pszFile - File to set as. If empty string then delete cached files too
   NOTE: Filetime is automatic
returns
   none
*/
void CacheInfoSet (DWORD dwCache, PWSTR pszFile)
{
   FILETIME ft;
   if (pszFile[0])
      GetSystemTimeAsFileTime (&ft);
   else
      memset (&ft, 0, sizeof(ft));

   WCHAR szFileKey[64], szFileTimeKey[64];
   swprintf (szFileKey, L"CacheFile%d", (int)dwCache);
   swprintf (szFileTimeKey, L"CacheFileTime%d", (int)dwCache);

   // write info
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      RegSetValueExW (hKey, szFileKey, 0, REG_SZ, (LPBYTE) pszFile, ((DWORD)wcslen(pszFile)+1)*sizeof(WCHAR));
      RegSetValueExW (hKey, szFileTimeKey, 0, REG_BINARY, (LPBYTE) &ft, sizeof(ft));

      RegCloseKey (hKey);
   }

   // delete files?
   DWORD i;
   WCHAR szTemp[MAX_PATH];
   if (!pszFile[0])
      for (i = 1; i <= 4; i++) {
         if (!CacheFilenameGet (dwCache, i, szTemp))
            continue;

         DeleteFileW (szTemp);
      }
}



/************************************************************************************
CacheInfoFind - Finds a cache slot to use.

This also deletes old caches while it's at it.


inputs
   PWSTR          pszFile - File (of .mif, etc.) to use
returns
   DWORD - Cache number to use for this.
*/
DWORD CacheInfoFind (PWSTR pszFile)
{
   BOOL afWantToClear[MAXWORLDCACHE];
   memset (afWantToClear, 0, sizeof(afWantToClear));

   // oldest
   DWORD dwOldest = (DWORD)-1;
   FILETIME ftOldest;
   memset (&ftOldest, 0, sizeof(ftOldest));

   // old enough to delete
   FILETIME ftDelete;
   GetSystemTimeAsFileTime (&ftDelete);
   __int64 *puiDelete = (__int64*) &ftDelete;
   *puiDelete -= (__int64)10000000 /*tosec*/ * 60 /*tomin*/ * 60 /*tohour*/ * 24 /*today*/ * 14 /* days old*/;
      // delete if more than 14 days old

   // see if can find match
   DWORD i;
   DWORD dwMatch = (DWORD)-1;
   WCHAR szTemp[MAX_PATH];
   FILETIME ftTemp;
   for (i = 0; i < MAXWORLDCACHE; i++) {
      CacheInfoGet (i, szTemp, &ftTemp);

      // if this is oldeer than the oldest then keep
      if ((dwOldest == (DWORD)-1) || (CompareFileTime (&ftTemp, &ftOldest) < 0)) {
         dwOldest = i;
         ftOldest = ftTemp;
      }

      // if empty file then skip rest of loop because already empty
      if (!szTemp[0])
         continue;

      // see if want to clear
      if (CompareFileTime (&ftTemp, &ftDelete) < 0)
         afWantToClear[i] = TRUE;

      // see if found exact match
      if (!_wcsicmp (pszFile, szTemp)) {
         // definitely don't want to clear
         afWantToClear[i] = FALSE;
         dwMatch = i;
      }

      // potentially clear
      if (afWantToClear[i])
         CacheInfoSet (i, L"");
   } // i

   // if found a match then done
   if (dwMatch != (DWORD)-1)
      return dwMatch;

   // else, need oldest

   // clear the oldest
   CacheInfoSet (dwOldest, L"");

   // set new info
   CacheInfoSet (dwOldest, pszFile);

   return dwOldest;
}

/************************************************************************************
PlayBeep - Plays a resource sound

inputs
   DWORD          dwResource - WAV Resource
*/
//void PlayBeep (DWORD dwResource)
//{
//   PlaySound (MAKEINTRESOURCE(dwResource), ghInstance, SND_ASYNC | SND_RESOURCE);
//}

/************************************************************************************
BeepWindowBeepShow - Beep based on show

Note: if gdwBeepWindowBeepShowDisable is >= 1, then wont beep

inputs
   DWORD             dwShow - Show value, such as SW_HIDE or SW_SHOW
returns
   none
*/
void BeepWindowBeepShow (DWORD dwShow)
{
   if (gdwBeepWindowBeepShowDisable >= 1)
      return;

   BeepWindowBeep((dwShow == SW_HIDE) ? ESCBEEP_SCROLLDRAGSTOP : ESCBEEP_SCROLLDRAGSTART);
}


/************************************************************************************
CMainWindow::IsWindowObscured  - Sees if a child window is obscured to the point of being hidden.

inputs
   HWND     hWnd - WIndow
returns
   BOOL - TRUE if it's obscured or hidden
*/
BOOL CMainWindow::IsWindowObscured (HWND hWnd)
{
   if (!hWnd || !IsWindowVisible (hWnd))
      return TRUE;

   // get the rectangle
   RECT rThis;
   GetWindowRect (hWnd, &rThis);

   CListFixed lIntersect;
   lIntersect.Init (sizeof(RECT));

   HWND hWndNext = hWnd;
   RECT rNext, rInter;
   while (hWndNext = GetNextWindow (hWndNext, GW_HWNDPREV)) {
      if (!IsWindowVisible (hWndNext))
         continue;   // ignore it

      // ignore flash window
      if (m_hWndFlashWindow && (m_hWndFlashWindow == hWndNext))
         continue;

      // if it's obscured by a sliding window don't care
      if (m_pSlideTop && (m_pSlideTop->m_hWnd == hWndNext))
         continue;
      if (m_pSlideBottom && (m_pSlideBottom->m_hWnd == hWndNext))
         continue;
      if (m_pTickerTape && (m_pTickerTape->m_hWnd == hWndNext))
         continue;

      // get the rectangle
      GetWindowRect (hWndNext, &rNext);
      if (!IntersectRect (&rInter, &rNext, &rThis))
         continue;   // dont intersct

      // else intersect, so add
      lIntersect.Add (&rNext);
   } // window

   // if no intersecting rects then easy
   DWORD dwNum = lIntersect.Num();
   RECT *parInter = (RECT*)lIntersect.Get(0);
   if (!dwNum)
      return FALSE;
   
   // else, loop through an array of points, and loop through all the
   // windows, seeing if they intersect
   int iXDelta = (rThis.right - rThis.left) / 5;
   int iYDelta = (rThis.bottom - rThis.top) / 5;
   iXDelta = max(iXDelta, 1);
   iYDelta = max(iYDelta, 1);
   POINT pt;
   DWORD i;
   for (pt.y = rThis.top + iYDelta/2; pt.y < rThis.bottom; pt.y += iYDelta)
      for (pt.x = rThis.left + iXDelta/2; pt.x < rThis.right; pt.x += iXDelta)
         for (i = 0; i < dwNum; i++)
            if (!PtInRect (&parInter[i], pt))
               return FALSE;  // not obscured
   
   // else, obscured
   return TRUE;
}


/************************************************************************************
CTransInfo::Constructor and detructor
*/
CTransInfo::CTransInfo (void)
{
   m_gID = GUID_NULL;
   m_dwType = 0;
   MemZero (&m_memObjectName);
   MemZero (&m_memString);
   MemZero (&m_memLang);
   m_pVCNode = NULL;
   m_dwTimeStart = m_dwTimeLast = GetTickCount();
}

CTransInfo::~CTransInfo (void)
{
   if (m_pVCNode)
      delete m_pVCNode;
}


/************************************************************************************
MainWindowWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK MainWindowWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCMainWindow p = (PCMainWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCMainWindow p = (PCMainWindow) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCMainWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/************************************************************************************
FlashWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK FlashWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCMainWindow p = (PCMainWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCMainWindow p = (PCMainWindow) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCMainWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProcFlash (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/************************************************************************************
SecondWindowWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK SecondWindowWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCMainWindow p = (PCMainWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCMainWindow p = (PCMainWindow) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCMainWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProcSecond (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


#ifndef UNIFIEDTRANSCRIPT

/************************************************************************************
MenuWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK MenuWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   PCMainWindow p = (PCMainWindow) GetWindowLong (hWnd, GWL_USERDATA);

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLong (hWnd, GWL_USERDATA, (LONG) lpcs->lpCreateParams);
         p = (PCMainWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProcMenu (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}
#endif // !UNIFIEDTRANSCRIPT


/************************************************************************************
TranscriptWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK TranscriptWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCMainWindow p = (PCMainWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCMainWindow p = (PCMainWindow) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCMainWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProcTranscript (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*************************************************************************************
InternetThreadProc - Thread that handles the internet.
*/

DWORD WINAPI VoiceChatThreadProc(LPVOID lpParameter)
{
   PVCTHREADINFO pThread = (PVCTHREADINFO) lpParameter;

   pThread->pMain->VoiceChatThread (pThread->dwThread);

   return 0;
}


/*************************************************************************************
ImageLoadThreadProc - Thread that handles the internet.
*/

DWORD WINAPI ImageLoadThreadProc(LPVOID lpParameter)
{
   PVCTHREADINFO pThread = (PVCTHREADINFO) lpParameter;

   pThread->pMain->ImageLoadThread (pThread->dwThread);

   return 0;
}

/*************************************************************************************
CPUSpeedToResolutionQuality - Given a CPU speed, convert to a resolution and quality.

inputs
   int         iCPUSpeed - From giCPUSpeed calculations
returns
   int - A number from 0..RESQUAL_QUALITYMONOMAX-1, for recommended speed.
*/
int CPUSpeedToQualityMono (int iCPUSpeed)
{
   if (iCPUSpeed >= 8)
      return 5;   // sixteen core
   else if (iCPUSpeed >= 5)
      return 4;   // eight core
   else if (iCPUSpeed >= 2)
      return 3;   // quad core
   else if (iCPUSpeed >= -1)
      return 2;   // dual core
   else if (iCPUSpeed >= -4)
      return 1;   // single core
   else
      return 0;   // fastest
#if 0 // old code
   int iResolution = iCPUSpeed + 3;
   int iQuality, iDynamic;
   iResolution = max(iResolution, 0);
   iQuality = iResolution / 3;   // for every 3
   iDynamic = iQuality * 2 / 3;  // BUGFIX - Was iQuality / 2, but not enough dynamic then
   iResolution -= iQuality * 2;   // so every time step up in quality, keep resolution
      // BUGFIX - Was just subtracting iQuality, by subtract 2x iQuality
   // dual-core 3 ghz pentium D ends up with iResolution=2, iQuality = 1

   iResolution = min(iResolution, RESQUAL_RESOLUTIONMAX-1);
   iQuality = min(iQuality, RESQUAL_QUALITYMAX-1);

   // BUGFIX - Make sure don't start default resolution too high since player won't really notice
   // so default to half
   iResolution = min(iResolution, RESQUAL_RESOLUTIONMAX/3);


   *piResolution = iResolution;
   *piQuality = iQuality;
   *piDynamic = min(iDynamic, RESQUAL_DYNAMICMAX-1);   // have of the quality setting
#endif
}


/*************************************************************************************
ResolutionQualityToRenderSettingsInt - Given the CPUSpeedToResolutionQuality()
parameters, fill in a default resolution and
shadow flags. (Internal method.)

NOTE: Assumes that valid values are already in the variables pointed to since
not all values change.

inputs
   int         iQualityMono - From CPUSpeedToQualityMono()
   BOOL        fHasPaid - Set to TRUE if the player has paid. Otherwise, limited
   int         *piDefaultResolution - Resolution to use
   DWORD       *pdwShadowFlags - Shadow flags to use
   DWORD       *pdwServerSpeed - Filled with the speed reported to the server, from 0..4
   int         *piDefaultTextureDetail - Filled with ideal texture detail to use
   BOOL        *pfLipSync - Filled in to TRUE if should default to lip sync
   DWORD       *pdwMovementTransition - Filled in with the movement transition
returns
   none
*/
#ifdef USINGSMALLDISPLAYWINDOW
#define RESOLUTIONDECREASE       1  // small display window, so slightly lower resolution
#else
#define RESOLUTIONDECREASE       0
#endif
#define RESOLUTIONINCREASEALL    1  // bump everything up one level

void ResolutionQualityToRenderSettingsInt (int iQualityMono, BOOL fHasPaid,
                                                      int *piDefaultResolution, DWORD *pdwShadowFlags,
                                    DWORD *pdwServerSpeed, int *piDefaultTextureDetail, BOOL *pfLipSync,
                                    DWORD *pdwMovementTransition)
{
   iQualityMono = max (iQualityMono, 0);
   iQualityMono = min (iQualityMono, RESQUAL_QUALITYMONOMAX-1);
   if (!fHasPaid)
      iQualityMono = min(iQualityMono, RESQUAL_QUALITYMONOMAXIFNOTPAID-1);

   int iResolution, iQuality, iDynamic;

   switch (iQualityMono) {
   case 0:  // fastest
      iResolution = 1 /* BUGFIX - Was 0 */ + RESOLUTIONINCREASEALL;
      iQuality = 0;
      iDynamic = 0;
      break;
   case 1:  // single core
      iResolution = 2 - RESOLUTIONDECREASE + RESOLUTIONINCREASEALL;
      iQuality = 1;
      iDynamic = 1;
      break;
   case 2:  // dual core
      iResolution = 3 - RESOLUTIONDECREASE + RESOLUTIONINCREASEALL;
      iQuality = 1;  // BUGFIX - Keeping quality low for dual-core
      iDynamic = 2;
      break;
   case 3:  // quad core
      iResolution = 3 - RESOLUTIONDECREASE + RESOLUTIONINCREASEALL;
      iQuality = 2;
      iDynamic = 2;
      break;
   case 4:  // eight core
      iResolution = 3 - RESOLUTIONDECREASE + RESOLUTIONINCREASEALL;  // same as quad core
      iQuality = 3;
      iDynamic = 3;
      break;
   case 5:  // sixteen core
      iResolution = 4 - RESOLUTIONDECREASE + RESOLUTIONINCREASEALL;  // slightly better than eight core
      iQuality = 4;
      iDynamic = 4;
      break;
   // NOTE: No way (at the moment) to get iDynamic >= 5, which would allow for transitions
   } // switch

   *piDefaultResolution = iResolution - 4;
   *piDefaultTextureDetail = ((iResolution+1) / 2) - 2;
      // BUGFIX - Add 1 to iResolution so only absolute bottom quality has low textures
   *pfLipSync = fHasPaid && (iDynamic >= 2);
   *pdwServerSpeed = min((DWORD)iDynamic, 4);   // so not too high

   if (!fHasPaid)
      *pdwMovementTransition = 0;   // no transitions
   else
      *pdwMovementTransition = (iDynamic >= 5) ? 1 : 0;   // no transitions
   // BUGFIX - Leave movement transitions alone
   //else {
   //   // BUGFIX - Transitions were too slow, so don't transition until much faster computer
   //   int iTrans = (int) *pdwServerSpeed;     // BUGFIX - Was /2, but never did transitions then
   //   iTrans -= 1;   // so need faster computer
   //   if (iTrans > 1)
   //      iTrans = (iTrans - 1) / 2 + 1;   // slower increase
   //   iTrans = max(iTrans, 0);
   //   iTrans = min(iTrans, 2);
   //   *pdwMovementTransition = (DWORD)iTrans;
   //}

#define SF_COMBINEDDEFAULT       (SF_NOSUPERSAMPLE)            // default values for purely optional settings
#define SF_COMBINEDDONTTOUCH     (SF_NOSUPERSAMPLE | SF_TWOPASS360)  // see also SF_COMBINEDDEFAULT
   // keep some shadows flags
   DWORD dwFlagsMask = SF_COMBINEDDONTTOUCH;
   *pdwShadowFlags &= dwFlagsMask;

   switch (iQuality) {
   case 0:  // absolute fastest texture
      *pdwShadowFlags |= SF_NOSHADOWS | SF_NOSPECULARITY | SF_LOWTRANSPARENCY | SF_NOBUMP | SF_TEXTURESONLY | SF_LOWDETAIL ;
      break;
   case 1:  // textures
      *pdwShadowFlags |= SF_NOSHADOWS | SF_NOSPECULARITY | SF_LOWTRANSPARENCY | SF_NOBUMP | SF_TEXTURESONLY; //| SF_LOWDETAIL 
      break;
   case 2:  // fastest shadows
      *pdwShadowFlags |= SF_NOSHADOWS | SF_NOSPECULARITY | SF_LOWTRANSPARENCY | SF_NOBUMP; //  | SF_LOWDETAIL;
      break;
   case 3:  // semifast shadoes
      *pdwShadowFlags |= SF_NOSHADOWS;
      break;
   default:
   case 4:  // shadows
      *pdwShadowFlags |= 0;
      break;
   // case 4: eventually ray tracing
   } // switch
   // BUGFIX - Always default to SF_NOSUPERSAMPLE since upped the default resolution
   // BUGFIX - Default to this on since really don't like jaggies, even in fastest view
   // BUGFIX - Always have no-supersample on so it's faster
   // BUGFIX - Keep whatever was there *pdwShadowFlags |= SF_NOSUPERSAMPLE;

   // BUGFIX - If high-enough resolution then turn on low-detail flag
   // again
   if (*piDefaultResolution >= -2)
      *pdwShadowFlags |= SF_LOWDETAIL;

}

/*************************************************************************************
CMainWindow::ResolutionQualityToRenderSettings - Given a CPU speed, fill in a default resolution and
shadow flags.

NOTE: Assumes that valid values are already in the variables pointed to since
not all values change.


inputs
   BOOL        fPowerSaver - If TRUE, in power saver mode
   int         iQualityMono - From CPUSpeedToQualityMono()
   BOOL        fTestIfPaid - If TRUE then test to see if player has paid, if FALSE assume they have.
   int         *piDefaultResolution - Resolution to use
   DWORD       *pdwShadowFlags - Shadow flags to use
   DWORD       *pdwServerSpeed - Filled with the speed reported to the server, from 0..4
   int         *piDefaultTextureDetail - Filled with ideal texture detail to use
   BOOL        *pfLipSync - Filled in to TRUE if should default to lip sync
   int         *piDefaultResolutionLow - Used for low-quality quick renders
   DWORD       *pdwShadowFlagsLow - Used for low-quality quick renders
   BOOL        *pfLipSyncLow - Used for low-quality quick renders
   DWORD       *pdwMovementTransition - Filled in with the movement transition
returns
   none
*/
void CMainWindow::ResolutionQualityToRenderSettings (BOOL fPowerSaver, int iQualityMono, BOOL fTestIfPaid,
                                                     int *piDefaultResolution, DWORD *pdwShadowFlags,
                                    DWORD *pdwServerSpeed, int *piDefaultTextureDetail, BOOL *pfLipSync,
                                    int *piDefaultResolutionLow, DWORD *pdwShadowFlagsLow, BOOL *pfLipSyncLow,
                                    DWORD *pdwMovementTransition)
{
   // take into account if paid
   BOOL fPaid = (fTestIfPaid ? (RegisterMode() != 0) : TRUE);


   ResolutionQualityToRenderSettingsInt (iQualityMono, fPaid,
      piDefaultResolution, pdwShadowFlags, pdwServerSpeed, piDefaultTextureDetail, pfLipSync,
      pdwMovementTransition);

   // also figure out low quality
   int iText = *piDefaultTextureDetail;
   DWORD dwSpeed = *pdwServerSpeed, dwMovementTransition = *pdwMovementTransition;
   // BUGFIX - Don't do multiple render pass since is ANNOYING
   // if (fPowerSaver) {
      // if power saver, the low quality is normal quality
      *piDefaultResolutionLow = *piDefaultResolution;
      *pdwShadowFlagsLow = *pdwShadowFlags;
      *pfLipSyncLow = *pfLipSync;
   // BUGFIX - Don't do multiple render pass since is ANNOYING
   //}
   //else
   //   ResolutionQualityToRenderSettingsInt (iQualityMono - QUICKRENDERQUALITY, fPaid,
   //      piDefaultResolutionLow, pdwShadowFlagsLow, &dwSpeed, &iText, pfLipSyncLow, &dwMovementTransition);

   // if power saver then turn a lot of stuff off
   // BUGFIX - Don't turn any off because advanced settings that manually turned on
   //if (fPowerSaver) {
   //   *pdwShadowFlags &= ~(SF_TWOPASS360);
   //   *pdwShadowFlagsLow &= ~(SF_TWOPASS360);

   //   // no transitions
   //   *pdwMovementTransition = 0;
   //}

}


/*************************************************************************************
CMainWindow::QualityMonoGet - Gets the iQualityMono value, and verifies that the
user hasn't customized the settings at all.

inputs
   int      *piResolution - If not NULL, filled in with the target resolution value
returns
   int      iQualityMono - Returns quality value, from 0+, or -1 if user has customized
*/
int CMainWindow::QualityMonoGet (int *piResolution)
{
   int iResolution, iTextureDetail, iResolutionLow;
   DWORD dwShadowsFlags, dwServerSpeed, dwShadowsFlagsLow, dwMovementTransition;
   BOOL fLipSync, fLipSyncLow;
   dwShadowsFlags = dwShadowsFlagsLow = SF_COMBINEDDEFAULT;

   ResolutionQualityToRenderSettings (m_fPowerSaver, m_iPreferredQualityMono, FALSE, 
      &iResolution, &dwShadowsFlags,
      &dwServerSpeed, &iTextureDetail, &fLipSync,
      &iResolutionLow, &dwShadowsFlagsLow, &fLipSyncLow,
      &dwMovementTransition);

   if (piResolution)
      *piResolution = iResolution;

   if ((iResolution != m_iResolution) || (dwShadowsFlags != m_dwShadowsFlags) || (dwServerSpeed != m_dwServerSpeed) ||
      (iTextureDetail != m_iTextureDetail) || (fLipSync != m_fLipSync) || (dwMovementTransition != m_dwMovementTransition) )
      return -1;
   else
      return m_iPreferredQualityMono;
}

/*************************************************************************************
CMainWindow::Constructor and destructor
*/
CMainWindow::CMainWindow (void)
{
   // BUGFIX - if 4 core of better then start with blurring
   // BUGFIX - Changed to 8 (from 4) since plan on defaulting to NPR, so jaggies won't be as noticible
   // BUGFIX - Changed to 2 (from 8) since NPR off by default, and jaggies annyingg
   // BUGFIX - Changed back to 8 since too slow on 2
   // BUGFIX - Must be on 64-bit windows too
   // BUGFIX - Disable supersample because messes up render cache
   //if ( (sizeof(PVOID) <= sizeof(DWORD)) || (HowManyProcessors() < 8) )
      gdwDefaultShadowFlags = gdwDefaultShadowFlagsLow = SF_COMBINEDDEFAULT;
   //else
   //   gdwDefaultShadowFlags = gdwDefaultShadowFlagsLow = 0;

   int iQualityMono;
   iQualityMono = CPUSpeedToQualityMono (giCPUSpeed);
   ResolutionQualityToRenderSettings (FALSE, iQualityMono, TRUE, &giDefaultResolution, &gdwDefaultShadowFlags,
      &gdwServerSpeed, &giDefaultTextureDetail, &gfLipSync,
      &giDefaultResolutionLow, &gdwDefaultShadowFlagsLow, &gfLipSyncLow,
      &gdwMovementTransition);

   m_lVISIMAGETIMER.Init (sizeof(VISIMAGETIMER));

   m_fAllSmall = FALSE;
   m_iPreferredQualityMono = PREFSPEEDUNDEF;
   m_fPowerSaver = FALSE;
   m_fSubtitleSpeech = TRUE;  // BUGFIX - try with this on again
   m_iTTSQuality = 1;
   m_fDisablePCM = FALSE;
   m_dwSubtitleSize = 2;
   m_fSubtitleText = TRUE;
   m_fTTSAutomute = TRUE;
   m_iResolution = giDefaultResolution;
   m_iResolutionLow = giDefaultResolutionLow;
   m_iTextureDetail = giDefaultTextureDetail;
   m_fLipSync = gfLipSync;
   m_fLipSyncLow = gfLipSyncLow;
   m_dwShadowsFlags = gdwDefaultShadowFlags;
   m_dwShadowsFlagsLow = gdwDefaultShadowFlagsLow;
   m_dwServerSpeed = gdwServerSpeed;
   m_dwMovementTransition = gdwMovementTransition;   // default
   m_dwArtStyle = 0; // default

   m_iTimeLastCommand = 0;

   m_langID = GetUserDefaultLangID();  // BUGBUG - at some point save to registry and UI for this

   m_fServerNotFoundErrorShowing = FALSE;
   m_fTutorialCursor = FALSE;
   m_dwTutorialCursorTime = 0;
   m_hWndPrimary = m_hWndSecond = NULL;
   m_pIT = NULL;
   m_pRT = NULL;
   m_pAT = NULL;
   m_pHM = NULL;

   m_fMessageDiabled = FALSE;
   m_hCursorEye = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSOREYE));
   m_hCursorMouth = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMOUTH));
   m_hCursorWalk = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORWALK));
   m_hCursorWalkDont = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORWALKDONT));
   m_hCursorKey = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORKEY));
   m_hCursorDoor = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORDOOR));
   m_hCursorHand = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORHAND));
   m_hCursorZoom = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORZOOM));
   m_hCursorMenu = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORMENU));
   m_hCursorTalk = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORTALK));
   m_hCursorRotLeft = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORROTLEFT));
   m_hCursorRotRight = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORROTRIGHT));
   m_hCursorRotUp = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORROTUP));
   m_hCursorRotDown = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORROTDOWN));
   m_hCursorNo = (HCURSOR) LoadImage (ghInstance, MAKEINTRESOURCE(IDC_CURSORNO), IMAGE_CURSOR,
      32, 32, LR_DEFAULTCOLOR);
   m_hCursorNoMenu = (HCURSOR) LoadImage (ghInstance, MAKEINTRESOURCE(IDC_CURSORNOMENU), IMAGE_CURSOR,
      32, 32, LR_DEFAULTCOLOR);
   m_hCursor360Scroll = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSOR360SCROLL));
   m_hCursor360ScrollOn = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSOR360SCROLLON));
   // m_hCursorNo = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORNO));
   // m_hCursorNoMenu = LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORNOMENU));
   memset (&m_MPI, 0, sizeof(m_MPI));
   m_iBytesShow = 0;
   m_fShowingDownload = FALSE;
   m_fRenderProgress = 0;
   m_fTTSLoadProgress = 0;
   m_dwShowProgressTicks = 0;
   m_pPageTranscript = NULL;
   m_lPCVisImage.Init (sizeof(PCVisImage));
   m_lPCIconWindow.Init (sizeof(PCIconWindow));
   m_lPCDisplayWindow.Init (sizeof(PCDisplayWindow));

   m_fSettingsControl = FALSE;

   m_fRandomActions = FALSE;
   m_fRandomTime = 4.0;

   m_f360Long = 0;
   m_f360Lat = 0;
   m_f360FOV = PI/2; // 90 degrees, BUGFIX - Was 60
   m_f360RotSpeed = 1;
   m_fCurvature = 0;

   m_tpFOVMinMax.h = PI / 6;  // BUGFIX - Was PI/12, but too much zoom
   m_tpFOVMinMax.v = PI * 2 / 3;

   m_fMainAspect = m_fMainAspectSentToServer = m_f360FOVSentToServer = 0;

   m_hWndFlashWindow = NULL;
   m_dwFlashWindowTicks = 0;

   m_hWndTranscript = NULL;
   m_fTransCommandVisible = FALSE;
   m_fCombinedSpeak = FALSE;
   // m_fTransFocusCommand = TRUE;

   // m_szTransCommand[0] = m_szTransSpeak[0] = 0;
   m_szTransCombined[0] = 0;
   // m_pTransSelCommand.x = m_pTransSelCommand.y = 0;
   // m_pTransSelSpeak.x = m_pTransSelSpeak.y = 0;
   m_pTransSelCombined.x = m_pTransSelCombined.y = 0;

   m_pTranscriptWindow = NULL;
   MemZero (&m_memTutorial);

   m_lPCTransInfo.Init (sizeof(PCTransInfo));
   InitializeCriticalSection (&m_crTransInfo);
   m_iSpeakSpeed = 0;
   memset (m_aiSpeakSpeed, 0, sizeof(m_aiSpeakSpeed));
   m_aiSpeakSpeed[2] = 1;  // BUGFIX - So combat starts out faster speaking rate
   memset (m_afSlideLocked, 0, sizeof(m_afSlideLocked));
   m_afSlideLocked[2] = /*m_afSlideLocked[3] =*/ TRUE; // for combat and misc
      // BUGFIX - keep slider open for chat too
   m_fMuteAll = FALSE;
   m_iTextVsSpeech = 2;
   memset (m_afMuteAll, 0, sizeof(m_afMuteAll));
   memset (m_adwTransShow, 0, sizeof(m_adwTransShow));
   m_adwTransShow[0] = 0x04 | 0x08; // description and what do
   m_adwTransShow[1] = 0x02 | 0x08; // transcript and what do
   m_adwTransShow[2] = 0x02 | 0x08;  // transcript, what do
   // m_adwTransShow[3] = 0x01 | 0x02 | 0x04 | 0x08;  // for discoverability
   m_dwTransShow = m_adwTransShow[0];
   m_iTransSize = 0;
   m_lSpeakBlacklist.Init (sizeof(GUID));

   m_fAllSmall = EnumPreinstalledTTS ();
   TextVsSpeechSet (m_fAllSmall ? 0 : 2, FALSE);
      // BUGFIX - Was setting default with text and text-to-speech. Do only tts

   m_iFontSizeLast = -1000;
   memset (m_ahFont, 0, sizeof(m_ahFont));
   memset (m_adwFontPixelHeight, 0, sizeof(m_adwFontPixelHeight));
   memset (m_ahFontSized, 0, sizeof(m_ahFontSized));
   memset (m_adwFontSizedPixelHeight, 0, sizeof(m_adwFontSizedPixelHeight));

   InitializeCriticalSection (&m_crSpeakBlacklist);

#ifndef UNIFIEDTRANSCRIPT
   m_hWndMenu = NULL;
   m_fMenuPosSet = FALSE;
   m_pMenuWindow = NULL;
#endif

   m_lidMenu = m_lidMenuContext = 0;
   m_fMenuTimeOut = 0;
   m_fMenuTimeOutOrig = 0;
   m_fMenuExclusive = FALSE;
   m_dwMenuDefault = -1;
   m_pMenuContext = NULL;

   m_pResVerb = NULL;
   m_pResVerbChat = NULL;
   m_pResVerbChatSent = NULL;
   m_pResVerbSent = NULL;
   m_pHotSpotToolTip = NULL;

   m_fInPacketSendError = FALSE;
   m_szRecordDir[0] = 0;

   m_szConnectFile[0] = 0;
   m_fConnectRemote = FALSE;
   m_dwConnectIP = m_dwConnectPort = 0;
   m_fQuickLogon = TRUE;

   m_pMapWindow = NULL;
   m_pSlideTop = NULL;
   m_pSlideBottom = NULL;
   m_pTickerTape = NULL;
   m_fSlideLocked = FALSE;  // BUGFIX - Default to locked in place, BUGFIX - Autohide
   m_szUserName[0] = 0;
   m_szPassword[0] = 0;
   m_fNewAccount = FALSE;
   m_fMinimizeIfSwitch = TRUE;
   m_fCanAskForDownloads = FALSE;
   m_pisBackgroundCur = NULL;

   MemZero (&m_memVerbShow);
   MemZero (&m_memVerbDo);
   m_lidVerb = DEFLANGID; // so something
   m_lidCommandLast = DEFLANGID;   // so something
   m_fVerbHiddenByServer = TRUE;
   m_dwVerbSelected = -1;
   m_fVerbSelectedIcon = FALSE;
   m_pVerbToolTip = NULL;

   m_gMainLast = GUID_NULL;

   m_hbrBackground = NULL;
   m_hbrJPEGBackAll = NULL;
   m_hbrJPEGBackTop = NULL;
   m_hbrJPEGBackDarkAll = NULL;

   MemZero (&m_memHypnoEffectInfinite);
   MemCat (&m_memHypnoEffectInfinite, L"noise:neutral");
   m_lHYPNOEFFECTINFO.Init (sizeof(HYPNOEFFECTINFO));
   // m_lHypnoEffectName.Clear();

   LightBackgroundSet (TRUE, FALSE);

   // m_fProgressVisible = FALSE;

   m_VCWave.ConvertSamplesAndChannels (22050, 1, NULL);  // make sure 22050
   InitializeCriticalSection (&m_csVCStopped);
   m_dwVCWaveOut = 0;
   m_fVCStopped = TRUE;
   m_fVCRecording = FALSE;
   m_fTempDisablePTT = FALSE;
   m_lVCEnergy.Init (sizeof(fp));
   m_fVCHavePreviouslySent = FALSE;
   m_VoiceChatInfo.m_fAllowVoiceChat = FALSE;   // start out FALSE until server sets

   // create the thread
   m_dwVCTicketAvail = m_dwVCTicketPlay = 0;
   InitializeCriticalSection (&m_csVCThread);
   DWORD dwProcessors = VoiceChatHowManyProcessors();
   DWORD i;
   m_fVCThreadToShutDown = FALSE;
   m_lVCPCM3DWave.Init (sizeof(PCM3DWave));
   m_lVCPCMMLNode2.Init (sizeof(PCMMLNode2));
   memset (m_aVCTHREADINFO, 0, sizeof(m_aVCTHREADINFO));
   DWORD dwID;
   for (i = 0; i < dwProcessors; i++) {
      m_aVCTHREADINFO[i].pMain = this;
      m_aVCTHREADINFO[i].dwThread  = i;
      m_aVCTHREADINFO[i].hSignal = CreateEvent (NULL, FALSE, FALSE, NULL);
      m_aVCTHREADINFO[i].hThread = CreateThread (NULL, ESCTHREADCOMMITSIZE, VoiceChatThreadProc, &m_aVCTHREADINFO[i], 0, &dwID);
      SetThreadPriority (m_aVCTHREADINFO[i].hThread, VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));
   }

   // create the threads for background load
   InitializeCriticalSection (&m_csImageLoadThread);
   m_lILTWAVEOUTCACHE.Init (sizeof(ILTWAVEOUTCACHE));
   m_fImageLoadThreadToShutDown = FALSE;
   memset (m_aImageLoadThread, 0, sizeof(m_aImageLoadThread));
   for (i = 0; i < NUMIMAGECACHE; i++) {
      m_aImageLoadThread[i].pMain = this;
      m_aImageLoadThread[i].dwThread  = i;
      m_aImageLoadThread[i].hSignal = CreateEvent (NULL, FALSE, FALSE, NULL);
      m_aImageLoadThread[i].hThread = CreateThread (NULL, ESCTHREADCOMMITSIZE, ImageLoadThreadProc, &m_aImageLoadThread[i], 0, &dwID);
      SetThreadPriority (m_aImageLoadThread[i].hThread, VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));
   } // i

   QueryPerformanceFrequency (&m_liPerCountFreq);

   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = FontScaleByScreenSize(-10); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   m_hFont = CreateFontIndirect (&lf);

   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = FontScaleByScreenSize(-20); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   m_hFontBig = CreateFontIndirect (&lf);

   for (i = 0; i < BACKGROUND_NUM; i++) {
      MemZero (&m_amemImageBackgroundName[i]);
      m_apJPEGBack[i].x = m_apJPEGBack[i].y = 0;
      m_ahbmpJPEGBack[i] = NULL;
      m_ahbmpJPEGBackDark[i] = NULL;
      m_ahbmpJPEGBackDarkExtra[i] = NULL;
   }

   for (i = 0; i < NUMTABMONITORS; i++)
      m_alTABWINDOW[i].Init (sizeof(TABWINDOW));

   // default to uploading 0 images of 256 x 256
   m_dwUploadImageLimitsNum = 0;
   m_dwUploadImageLimitsMaxWidth = m_dwUploadImageLimitsMaxHeight = 256;

   // load from registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      DWORD dwSize, dwType;
      dwSize = sizeof(m_szRecordDir);
      if ( (ERROR_SUCCESS != RegQueryValueExW (hKey, L"RecordDir", NULL, &dwType, (LPBYTE) m_szRecordDir, &dwSize)) ||
         (dwType != REG_SZ) )
            m_szRecordDir[0] = 0;
      RegCloseKey (hKey);
   }

}

CMainWindow::~CMainWindow (void)
{
   EnterCriticalSection (&m_csImageLoadThread);
   m_fCanAskForDownloads = FALSE;
   LeaveCriticalSection (&m_csImageLoadThread);

   // call the stop recording call
   VoiceChatStop ();
   DeleteCriticalSection (&m_csVCStopped);

   // free up the VC thread
   VoiceChatThreadFree ();
   ImageLoadThreadFree ();

   DeleteCriticalSection (&m_csVCThread);
   DeleteCriticalSection (&m_csImageLoadThread);

   DestroyAllChildWindows ();

   if (m_pMapWindow)
      delete m_pMapWindow;
   m_pMapWindow = NULL;

   if (m_pSlideTop)
      delete m_pSlideTop;
   m_pSlideTop = NULL;

   if (m_pTickerTape)
      delete m_pTickerTape;
   m_pTickerTape = NULL;

   if (m_pSlideBottom)
      delete m_pSlideBottom;
   m_pSlideBottom = NULL;

#ifndef UNIFIEDTRANSCRIPT
   // delete the menu
   if (m_pMenuWindow)
      delete m_pMenuWindow;
   m_pMenuWindow = NULL;
#endif

   // delete the transcript
   if (m_pTranscriptWindow)
      delete m_pTranscriptWindow;
   m_pTranscriptWindow = NULL;

   // clear all the images
   VisImageClearAll(FALSE);

   MainClear();

   // delete the transcript
   EnterCriticalSection (&m_crTransInfo);
   PCTransInfo *ppti = (PCTransInfo*) m_lPCTransInfo.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCTransInfo.Num(); i++)
      delete ppti[i];
   m_lPCTransInfo.Clear();
   LeaveCriticalSection (&m_crTransInfo);
   DeleteCriticalSection (&m_crTransInfo);

   if (m_pVerbToolTip)
      delete m_pVerbToolTip;
   m_pVerbToolTip = NULL;
   if (m_pHotSpotToolTip)
      delete m_pHotSpotToolTip;
   m_pHotSpotToolTip = NULL;

   if (m_pResVerb)
      delete m_pResVerb;
   if (m_pResVerbSent)
      delete m_pResVerbSent;
   if (m_pResVerbChat)
      delete m_pResVerbChat;
   if (m_pResVerbChatSent)
      delete m_pResVerbChatSent;

   if (m_pHM)
      delete m_pHM;
   m_pHM = NULL;
   if (m_pIT)
      delete m_pIT;
   m_pIT = NULL;
   if (m_pAT)
      delete m_pAT;
   m_pAT = NULL;
   if (m_pRT)
      delete m_pRT;
   m_pRT = NULL;

   if (m_hWndPrimary)
      DestroyWindow (m_hWndPrimary);
   m_hWndPrimary = NULL;
   if (m_hWndSecond)
      DestroyWindow (m_hWndSecond);
   m_hWndSecond = NULL;

   DeleteObject (m_hbrBackground);
   DeleteObject (m_hbrJPEGBackTop);
   DeleteObject (m_hbrJPEGBackAll);
   DeleteObject (m_hbrJPEGBackDarkAll);

   // clear callback
   MegaFileSet (NULL, NULL, NULL);

   if (m_hFont)
      DeleteObject (m_hFont);
   if (m_hFontBig)
      DeleteObject (m_hFontBig);

   for (i = 0; i < BACKGROUND_NUM; i++) {
      if (m_ahbmpJPEGBack[i])
         DeleteObject (m_ahbmpJPEGBack[i]);
      if (m_ahbmpJPEGBackDark[i])
         DeleteObject (m_ahbmpJPEGBackDark[i]);
      if (m_ahbmpJPEGBackDarkExtra[i])
         DeleteObject (m_ahbmpJPEGBackDarkExtra[i]);
   } // i

   for (i = 0; i < NUMMAINFONT; i++)
      if (m_ahFont[i])
         DeleteObject (m_ahFont[i]);

   for (i = 0; i < NUMMAINFONTSIZED; i++)
      if (m_ahFontSized[i])
         DeleteObject (m_ahFontSized[i]);

   DeleteCriticalSection (&m_crSpeakBlacklist);

   // clear the image cache
   ImageCacheTimer (TRUE);
}


/*************************************************************************************
CMainWindow::LightBackgroundSet - Sets the light background color.

inputs
   BOOL           fLight - If TRUE then light background, FALSE then dark background
   BOOL           fUpdateImage - If TRUE then update the image while at it
returns
   none
*/
void CMainWindow::LightBackgroundSet (BOOL fLight, BOOL fUpdateImage)
{
   if (m_hbrBackground)
      DeleteObject (m_hbrBackground);
   if (m_hbrJPEGBackAll)
      DeleteObject (m_hbrJPEGBackAll);
   if (m_hbrJPEGBackTop)
      DeleteObject (m_hbrJPEGBackTop);
   if (m_hbrJPEGBackDarkAll)
      DeleteObject (m_hbrJPEGBackDarkAll);

   m_fLightBackground = fLight;

   if (m_pHM)
      m_pHM->LightBackgroundSet (m_fLightBackground);

   // these chage depending on light background
   m_cBackground = m_fLightBackground ? RGB(0xe0, 0xe0, 0xe0) : RGB(0,0,0);
   m_cBackgroundNoChange = RGB(0,0,0);
   m_cBackgroundMenuNoChange = RGB(0x40, 0x60, 0x40);
      // NOTE: BackgroundMenu is always dark
   m_cBackgroundNoTabNoChange = RGB(0x40,0x40,0x60);
      // NOTE: Background, no tab, is always dark
   m_cText = m_fLightBackground ? RGB(0x10, 0x10, 0x00) : RGB(0xff,0xff,0xff);
   m_cTextNoChange = RGB(0xff,0xff,0xff);
   m_cTextHighlight = m_fLightBackground ? RGB (0x40, 0x00, 0x00) : RGB(0xff, 0x80, 0x80);
   m_cTextHighlightNoChange = RGB(0xff, 0x80, 0x80);
   m_cTextDim = m_fLightBackground ? RGB(0x40, 0x40, 0x40) : RGB(0x80, 0x80, 0x80);
   m_cTextDimNoChange = RGB(0x80,0x80,0x80);
   m_cVerbDim = m_fLightBackground ? RGB(0xc0, 0xc0, 0xc0) : RGB(0x40, 0x40, 0x40);
   m_cVerbDimNoChange = RGB(0x40, 0x40, 0x40);
   m_crJPEGBackAll = m_crJPEGBackTop = m_fLightBackground ? RGB(0xc0, 0xc0, 0xff) : RGB(0x5, 0x5, 0x10); // slightly blue
   m_crJPEGBackDarkAll = m_fLightBackground ? RGB(0xff, 0xff, 0xff) : RGB (0, 0, 0);

   // if colors change, need new gruches
   m_hbrBackground = CreateSolidBrush (m_cBackground);
   m_hbrJPEGBackAll = CreateSolidBrush (m_crJPEGBackAll);
   m_hbrJPEGBackTop = CreateSolidBrush (m_crJPEGBackTop);
   m_hbrJPEGBackDarkAll = CreateSolidBrush (m_crJPEGBackDarkAll);

   DWORD i;
   if (fUpdateImage)
      for (i = 0; i < BACKGROUND_NUM; i++)
         BackgroundUpdate (i);
}


/*************************************************************************************
CMainWindow::TextVsSpeechSet - Sets the m_fMuteAll, AND at the same time sets
related switches that should turn on/off with mute.

inputs
   int         iTextVsSpeech - 0 then text only, 1 then text and speech, 2 speech only
   BOOL        fActNow - if TRUE, will also do invalidate and send messages
               as appropriate to act. Use FALSE when initializing
returns
   none
*/
void CMainWindow::TextVsSpeechSet (int iTextVsSpeech, BOOL fActNow)
{
   BOOL fMuteAll = !iTextVsSpeech;

   DWORD i;
   for (i = 0; i < NUMTABS; i++)
      m_afMuteAll[i] = fMuteAll;

   if (iTextVsSpeech <= 1) {
      // show transcript on all
      for (i = 0; i < NUMTABS; i++)
         m_adwTransShow[i] |= 0x02 | 0x08; // transcript and what do

      m_fSubtitleSpeech = FALSE;
      m_fSubtitleText = FALSE;
   }
   else {
      // no transcript
      for (i = 0; i < NUMTABS; i++)
         m_adwTransShow[i] |= 0x02 | 0x08; // transcript and what do
      m_adwTransShow[0] &= ~0x02; // exception, don't show
      m_adwTransShow[1] &= ~0x02; // exception, don't show
      m_adwTransShow[3] &= ~0x02; // exception, don't show

      m_fSubtitleSpeech = FALSE;
      m_fSubtitleText = TRUE;
   }

   // update the sliding locked too
   memset (m_afSlideLocked, 0, sizeof(m_afSlideLocked));
   m_afSlideLocked[2] = /*m_afSlideLocked[3] =*/ TRUE; // for combat and misc
      // BUGFIX - keep slider open for chat too

   if (fActNow)
      EnterCriticalSection (&m_crSpeakBlacklist);
   m_dwTransShow = m_adwTransShow[fActNow ? m_pSlideTop->m_dwTab : 0];
   m_fMuteAll = m_afMuteAll[fActNow ? m_pSlideTop->m_dwTab : 0];
   m_iTextVsSpeech = iTextVsSpeech;
   if (fActNow)
      LeaveCriticalSection (&m_crSpeakBlacklist);

   if (fActNow) {
      PostMessage (m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSUPDATE, 0, 0);

      // move child windows around to new locations
      for (i = 0; i < NUMTABMONITORS; i++) {
         m_alTABWINDOW[i].Clear();
         m_alTabWindowString[i].Clear();
      }
      ChildShowMoveAll ();

      // send message to server indicating that muting all
      InfoForServer (L"muteall", NULL, m_fMuteAll);
      InfoForServer (L"transcript", NULL, (m_iTextVsSpeech <= 1) );
   }
}


/*************************************************************************************
CMainWindow::VoiceChatHowManyProcessors - Returns the number of processors to use for
voice chat compress
*/
DWORD CMainWindow::VoiceChatHowManyProcessors (void)
{
   DWORD dwNum = HowManyProcessors();
   if (dwNum >= 2)
      return min(dwNum+1, MAXRAYTHREAD);
   else
      return dwNum;
}

/*************************************************************************************
CMainWindow::DestroyAllChildWindows - This destroys all the child windows.
IN the process, they save their locations to the user's megafile.

inputs
   DWORD          dwType - Bit field to indicate type to destroy
                           0x0001 = icon windows
                           0x0002 = display windows
*/
void CMainWindow::DestroyAllChildWindows (DWORD dwType)
{
   // clear all the icon windows
   DWORD i;
   if (dwType & 0x0001) {
      PCIconWindow *ppi = (PCIconWindow*) m_lPCIconWindow.Get(0);
      for (i = 0; i < m_lPCIconWindow.Num(); i++)
         delete ppi[i];
      m_lPCIconWindow.Clear();
   }

   if (dwType & 0x0002) {
      PCDisplayWindow *ppd = (PCDisplayWindow*) m_lPCDisplayWindow.Get(0);
      for (i = 0; i < m_lPCDisplayWindow.Num(); i++)
         delete ppd[i];
      m_lPCDisplayWindow.Clear();
   }
}

/*************************************************************************************
MegaFileCallback - Called by the magafile when requesting information be gotten.

Conforms to standard megafilecallback.
*/
BOOL __cdecl MegaFileCallback (DWORD dwMessage, PWSTR pszFile, PVOID pInstanceInfo)
{
   PCMainWindow pw = (PCMainWindow)pInstanceInfo;
   if (!pw->m_pIT)
      return FALSE;

   switch (dwMessage) {
   case MFCALLBACK_NEEDFILE:
   case MFCALLBACK_NEEDFILEIGNOREDIR:
      // send the request
      return pw->m_pIT->FileRequest (pszFile, (dwMessage == MFCALLBACK_NEEDFILEIGNOREDIR));

   case MFCALLBACK_STILLALIVE:
      // BUGBUG - Got a crash here when closed while download. Had pw->m_pIT be
      // non-null to get here, but when got to this check crashed with pw->m_pIT being
      // NULL. May need some extra precautions, like critical sections

      // if the connection is no longer valid then return FALSE
      PCWSTR pszErr;
      if (pw->m_pIT->m_pPacket->GetLastError(&pszErr))
         return FALSE;

      // see if still around
      return !pw->m_pIT->FileCheckRequest (pszFile);

   default:
      return FALSE; // unknown
   }
   return FALSE;
}


/*************************************************************************************
MegaFileIncludeM3DLibrary - Copies the M3D library into the megafile.

inputs
   PWSTR             pszLib - M3D library name
   PCMegaFile        pMega - Mega file to write into
   PCBTree           ptFiles - Filled with all the files in the library megafile
returns
   BOOL - TRUE if success
*/
BOOL MegaFileIncludeM3DLibrary (PWSTR pszLib, PCMegaFile pMega, PCBTree ptFiles)
{
   CMegaFile mf;
   if (!mf.Init (pszLib, &GUID_LibraryHeaderNewer, FALSE))
      return FALSE;

   // enumerate all the contents
   CListVariable lName;
   CListFixed lMFFILEINFO;
   lMFFILEINFO.Init (sizeof(MFFILEINFO));
   mf.Enum (&lName, &lMFFILEINFO);

   // go through and copy them all over
   DWORD i;
   PMFFILEINFO pmfi = (PMFFILEINFO) lMFFILEINFO.Get(0);
   for (i = 0; i < lName.Num(); i++, pmfi++) {
      // add this file to the list
      ptFiles->Add ((PWSTR)lName.Get(i), 0, 0);

      __int64 iSize;
      PBYTE pData = (PBYTE) mf.Load ((PWSTR)lName.Get(i), &iSize);
      if (!pData)
         continue;   // shouldnt happen

      // write it in
      BOOL fRet = pMega->Save ((PWSTR)lName.Get(i), pData, iSize, &pmfi->iTimeCreate,
         &pmfi->iTimeModify, &pmfi->iTimeAccess);
      MegaFileFree (pData);
      if (!fRet)
         return FALSE;
   } // i

   return TRUE;
}


/*************************************************************************************
CMainWindow::EnumPreinstalledTTS - This enumerates all the preinstalled TTS voices.

inputs
   none
returns
   BOOL - TRUE if all of the voices were below MINITTSSIZE.
*/
BOOL CMainWindow::EnumPreinstalledTTS (void)
{
   BOOL fAllSmall = TRUE;

   // enumerate all the files of the given type in the directory
   DWORD dwFileType;
   PSTR apszFileType[] = {"*.mlx", "*.tts"};
   WIN32_FIND_DATA FindFileData;
   m_lDontDownloadOrig.Clear();
   m_lDontDownloadFile.Clear();
   m_lDontDownloadDate.Init (sizeof(FindFileData));
   for (dwFileType = 0; dwFileType < sizeof(apszFileType)/sizeof(PSTR); dwFileType++) {
      char szDir[256];
      WCHAR szTemp[512], szFile[256];
      strcpy (szDir, gszAppDir);
      strcat (szDir, apszFileType[dwFileType]);

      // make a list of wave files in the directory
      HANDLE hFind;

      hFind = FindFirstFile(szDir, &FindFileData);
      if (hFind != INVALID_HANDLE_VALUE) {
         while (TRUE) {
            // query to see if this file exists
            MultiByteToWideChar (CP_ACP, 0, FindFileData.cFileName, -1, szFile, sizeof(szFile)/sizeof(WCHAR));

            MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));
            wcscat (szTemp, szFile);

            // is this a TTS file?
            BOOL fUse = TRUE;
            DWORD dwLen = (DWORD)wcslen(szFile);
            BOOL fIsTTS = (dwLen >= 4) && !_wcsicmp(szFile + (dwLen-4), L".tts");
            if (fIsTTS) {
               if (FindFileData.nFileSizeHigh || (FindFileData.nFileSizeLow >= MINITTSSIZE))
                  fAllSmall = FALSE;

               if ((FindFileData.nFileSizeHigh || (FindFileData.nFileSizeLow > MAXTTSSIZE))) {
                  // BUGFIX - Had /100 in as a test
                  // if not registered then dont use TTS >= 20 meg
                  // BUGFIX - Changed to 30 MB
                  if (RegisterMode() != 1)
                     fUse = FALSE;
               }
            }

            if (fUse) {
               // also, remember this in the list of files not to ask to download
               m_lDontDownloadOrig.Add (szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
               m_lDontDownloadFile.Add (szFile, (wcslen(szFile)+1)*sizeof(WCHAR));
               m_lDontDownloadDate.Add (&FindFileData);
            }

            if (!FindNextFile (hFind, &FindFileData))
               break;
         }

         FindClose(hFind);
      }
   } // dwType

   return fAllSmall;
}

/*************************************************************************************
CMainWindow::FillUpFiles - This makes sure the m_mfFiles megafile is pre-filled
with TTS voices and 3d model info from the application directory. That way it
may not have to download the data over the internet.

Must have called EnumPreinstalledTTS() before this (in the constructor)
*/
void CMainWindow::FillUpFiles (void)
{
   // first, enumerate that's there
   CListVariable lName;
   CListFixed lMFFILEINFO;
   lMFFILEINFO.Init (sizeof(MFFILEINFO));
   m_mfFiles.Enum (&lName, &lMFFILEINFO);

   // if it's empty then include the 3d libary
   if (!lName.Num()) {
      CBTree tFiles;
      WCHAR szFile[512];
      MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szFile, sizeof(szFile)/sizeof(WCHAR));
      wcscat (szFile, L"LibraryInstalled.me3");
      MegaFileIncludeM3DLibrary (szFile, &m_mfFiles, &tFiles);
   }


   // now loop though all the existing files and make sure the dates match
   // up properly
   lMFFILEINFO.Clear();
   lName.Clear();
   m_mfFiles.Enum (&lName, &lMFFILEINFO);
   PMFFILEINFO pfi = (PMFFILEINFO)lMFFILEINFO.Get(0);
   DWORD i;
   for (i = 0; i < lName.Num(); i++, pfi++) {
      PWSTR psz = (PWSTR)lName.Get(i);
      DWORD dwLen = (DWORD)wcslen(psz);
      DWORD j;
      for (j = 0; j < m_lDontDownloadFile.Num(); j++) {
         DWORD dwDLen = (DWORD)m_lDontDownloadFile.Size(j) / sizeof(WCHAR) - 1;
         if (dwDLen > dwLen)
            continue;   // too small

         PWSTR pszD = (PWSTR)m_lDontDownloadFile.Get(j);
         if (_wcsicmp (pszD, psz + (dwLen - dwDLen)))
            continue;   // no match
         if ((dwDLen < dwLen) && (psz[dwLen - dwDLen - 1] != L'\\'))
            continue;   // no backslash

         // else, match
         break;
      } // j
      if (j >= m_lDontDownloadFile.Num())
         continue;   // didnt match

      // if get here, found a match. verify that the dates are the same
      PWIN32_FIND_DATA pfd = (PWIN32_FIND_DATA) m_lDontDownloadDate.Get(j);

      // BUGFIX - Wasn't deleting properly
      if (memcmp(&pfd->ftCreationTime, &pfi->iTimeCreate, sizeof(pfi->iTimeCreate)) ||
         memcmp(&pfd->ftLastWriteTime, &pfi->iTimeModify, sizeof(pfi->iTimeModify)) ||
         (pfd->nFileSizeHigh != (DWORD)(pfi->iDataSize >> 32)) ||
         (pfd->nFileSizeLow != (DWORD)pfi->iDataSize))

         m_mfFiles.Delete (psz); // the file exists, but has the wrong date, so delete it


   } // i
}


/*************************************************************************************
FontScaleByScreenSize - How much to scale the font based on the screen size.

returns
   fp - 1.0 = normal scale, 0.5 = half size, etc.
*/
fp FontScaleByScreenSize (void)
{
   static fp sfValue = 0;
   if (sfValue)
      return sfValue;

#undef GetSystemMetrics
   int iWidth = GetSystemMetrics (SM_CXSCREEN);
   int iHeight = GetSystemMetrics (SM_CYSCREEN);

   fp fRet = sqrt((fp)iWidth * fp(iHeight) / (fp)1680 / (fp)1050);   // acount for difference in screen resolution

   fRet = sqrt (fRet);  // so scale changes not super huge

   sfValue = fRet;


   return fRet;
}

/*************************************************************************************
FontScaleByScreenSize - Scales a font number by the screen size

inputs
   int         iFontSize - Font size
returns
   int - New font size
*/
int FontScaleByScreenSize (int iFontSize)
{
   return (int) floor((fp)iFontSize * FontScaleByScreenSize() + 0.5);
}

/*************************************************************************************
EscWindowFontScaleByScreenSize - Changes PCEscWindow->m_fi. Call this after Init().
*/
void EscWindowFontScaleByScreenSize (PCEscWindow pWindow)
{
   pWindow->m_fi.iPointSize = FontScaleByScreenSize(12);
}

/*************************************************************************************
CMainWindow::Init - Initializes the main window

inputs
   PWSTR             pszFile - File that using
   PWSTR             pszUserName - User name. 64 chars
   PWSTR             pszPassword - Password. 64 chars.
   BOOL              fNewAccount - TRUE if want to create new account, FALSE if not
   BOOL              fRemote - If TRUE then using remote access, else local
   DWORD             dwIP - IP address, if remote
   DWORD             dwPort - Port number, if remote
   BOOL              fQuickLogon - If TRUE then bypass questionsa and log on immediately
   PCMem             pMemJPEGBack - Memory for the JPEG background. m_dwCurPosn is the size
returns
   BOOL - TRUE if success, FALSE if error
*/
// #define BACKGROUNDUPDATE_INTENSITYLIGHT        0x0000
// #define BACKGROUNDUPDATE_INTENSITYLIGHT2        0x4000
// #define BACKGROUNDUPDATE_INTENSITYLIGHT3        0x8000

BOOL CMainWindow::Init (PWSTR pszFile, PWSTR pszUserName, PWSTR pszPassword,
                        BOOL fNewAccount, BOOL fRemote, DWORD dwIP, DWORD dwPort, BOOL fQuickLogon,
                        PCMem pMemJPEGBack)
{
   // copy over params
   wcscpy (m_szConnectFile, pszFile);
   wcscpy (m_szUserName, pszUserName);
   wcscpy (m_szPassword, pszPassword);
   m_fNewAccount = fNewAccount;
   m_fConnectRemote = fRemote;
   m_dwConnectIP = dwIP;
   m_dwConnectPort = dwPort;
   m_fQuickLogon = fQuickLogon;

   // background image
   if (!m_ahbmpJPEGBack[BACKGROUND_IMAGES] && pMemJPEGBack && pMemJPEGBack->m_dwCurPosn)
      m_ahbmpJPEGBack[BACKGROUND_IMAGES] = JPegToBitmap (pMemJPEGBack->p, (DWORD)pMemJPEGBack->m_dwCurPosn);
   // PCImage pImage = NULL;
   if (m_ahbmpJPEGBack[BACKGROUND_IMAGES]) {
      // get dimensions
      CImage image;
      m_aImageBackground[BACKGROUND_IMAGES].Init (m_ahbmpJPEGBack[BACKGROUND_IMAGES]);
      MemZero (&m_amemImageBackgroundName[BACKGROUND_IMAGES]);

      m_apJPEGBack[BACKGROUND_IMAGES].x = (int) m_aImageBackground[BACKGROUND_IMAGES].Width();
      m_apJPEGBack[BACKGROUND_IMAGES].y = (int) m_aImageBackground[BACKGROUND_IMAGES].Height();


      // find color
      WORD awColorAll[3], awColorTop[3];
      AverageColorOfImage (&m_aImageBackground[BACKGROUND_IMAGES], awColorAll, awColorTop);

#ifdef MIFTRANSPARENTWINDOWS
      m_crJPEGBackAll = UnGamma (awColorAll);
      m_crJPEGBackTop = UnGamma (awColorTop);
      if (m_hbrJPEGBackAll)
         DeleteObject (m_hbrJPEGBackAll);
      m_hbrJPEGBackAll = CreateSolidBrush (m_crJPEGBackAll);
      if (m_hbrJPEGBackTop)
         DeleteObject (m_hbrJPEGBackTop);
      m_hbrJPEGBackTop = CreateSolidBrush (m_crJPEGBackTop);
#endif // #ifndef MIFTRANSPARENTWINDOWS

      // create a dark bitmap
      if (!m_ahbmpJPEGBackDark[BACKGROUND_IMAGES]) {
         m_ahbmpJPEGBackDark[BACKGROUND_IMAGES] = ImageSquashIntensityToBitmap (&m_aImageBackground[BACKGROUND_IMAGES], m_fLightBackground, 1, NULL,
            &m_crJPEGBackDarkAll, NULL);

#ifdef MIFTRANSPARENTWINDOWS
         if (m_hbrJPEGBackDarkAll)
            DeleteObject (m_hbrJPEGBackDarkAll);
         m_hbrJPEGBackDarkAll = CreateSolidBrush (m_crJPEGBackDarkAll);
#endif // #ifndef MIFTRANSPARENTWINDOWS
      }

   // extra dark
      if (!m_ahbmpJPEGBackDarkExtra[BACKGROUND_IMAGES])
         m_ahbmpJPEGBackDarkExtra[BACKGROUND_IMAGES] = ImageSquashIntensityToBitmap (&m_aImageBackground[BACKGROUND_IMAGES], m_fLightBackground, 2, NULL,
            NULL, NULL);
   }

   // initialize name
   // WCHAR szwRoot[256];
   // WCHAR szw[256];
   // wcscpy (szwRoot, m_szConnectFile);
   // DWORD dwLen = (DWORD)wcslen(szwRoot);
   // if ((dwLen < 4) || (szwRoot[dwLen-4] != L'.'))
   //    return FALSE;  // shouldnt happen
   // szwRoot[dwLen-4] = 0;   // remote suffix

   // find the cache
   WCHAR szw[MAX_PATH];
   DWORD dwCache = CacheInfoFind (m_szConnectFile);


   CacheFilenameGet (dwCache, 1, szw);
   //wcscpy (szw, szwRoot);
   //wcscat (szw, L".mf1");
   if (!m_mfFiles.Init (szw, &GUID_MegaFileCache, TRUE))
      return FALSE;
   FillUpFiles ();

   // cache
   DWORD dwQuality;
   for (dwQuality = 0; dwQuality < NUMIMAGECACHE; dwQuality++) {
      CacheFilenameGet (dwCache, dwQuality ? 4 : 2, szw);
      //wcscpy (szw, szwRoot);
      //wcscat (szw, dwQuality ? L".mf4" : L".mf2");
      _ASSERTE (NUMIMAGECACHE <= 2);
      if (!m_amfImages[dwQuality].Init (szw, &GUID_MegaImageCache, TRUE))
         return FALSE;
   } // dwQuality

   // user data
   CacheFilenameGet (dwCache, 3, szw);
   //wcscpy (szw, szwRoot);
   //wcscat (szw, L".mf3");
   if (!m_mfUser.Init (szw, &GUID_MegaUserCache, TRUE))
      return FALSE;

   // set all files to use this cache
   MegaFileSet (&m_mfFiles, MegaFileCallback, this);

   // load in all the user's window location preferences
   ChildLocLoadAll ();

   // create the secondary window if need one
   SecondCreateDestroy (gfMonitorUseSecond, FALSE);

   // create window
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = MainWindowWndProc;
   wc.lpszClassName = "CircumrealityClientMainWindow";
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_APPICON));
   wc.hCursor = NULL;
   wc.hbrBackground = NULL;
   RegisterClass (&wc);

   int iWidth = GetSystemMetrics (SM_CXSCREEN);
   int iHeight = GetSystemMetrics (SM_CYSCREEN);

   m_hWndPrimary = CreateWindowEx (WS_EX_APPWINDOW,
      wc.lpszClassName, "Circumreality",
      WS_POPUP | /*WS_OVERLAPPEDWINDOW |*/ WS_CLIPCHILDREN,
      iWidth / 8 , iHeight / 8 ,iWidth * 6 / 8 , iHeight * 6 / 8 ,
      NULL, NULL, ghInstance, (PVOID) this);
   if (!m_hWndPrimary)
      return FALSE;  // error. shouldt happen
   ShowWindow (m_hWndPrimary, SW_SHOWMAXIMIZED);
   
   // note that MIDI beeps are to be sent here
   MIDIRemapSet (m_hWndPrimary, WM_MIDIFROMESCARPMENT);

   return TRUE;
}



#ifndef UNIFIEDTRANSCRIPT

/*************************************************************************************
CMainWindow::WndProcMenu - Manages the window calls
*/
LRESULT CMainWindow::WndProcMenu (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWndMenu = hWnd;
      }
      break;

   case WM_MOUSEACTIVATE:
      {
         // see if can type here
         if (m_pMenuWindow && m_pMenuWindow->m_hWnd)
            SetFocus (m_pMenuWindow->m_hWnd);
         return MA_ACTIVATE;
      }
      break;

      // NOTE: No paint, since causes flicker

   case WM_MOUSEMOVE:
      VerbTooltipUpdate ();
      HotSpotTooltipUpdate ();
      break;

   case WM_MOVING:
      if (TrapWM_MOVING (hWnd, lParam, FALSE))
         return TRUE;
      break;
   case WM_SIZING:
      if (TrapWM_MOVING (hWnd, lParam, TRUE))
         return TRUE;
      break;

   // BUGFIX - Both WM_SIZE and WM_MOVE end up moving the cWindow
   case WM_MOVE:
   case WM_SIZE:
      {
         ChildLocSave (hWnd, TW_MENU, NULL, NULL);

         if (!m_pMenuWindow || !m_pMenuWindow->m_hWnd)
            break;

         // adjust the size
         RECT r;
         GetClientRect (hWnd, &r);
         m_pMenuWindow->PosnSet (&r);
      }
      break;

   case WM_DESTROY:
      if (m_pMenuWindow)
         delete m_pMenuWindow;
      m_pMenuWindow = NULL;
      break;

   case WM_CHAR:
      {
         // see if can type here... doesn't semto get called so no worry
         if (m_pMenuWindow && m_pMenuWindow->m_hWnd)
            break; // return SendMessage (pvi->WindowForTextGet(), uMsg, wParam, lParam);
         else  // pass it up
            return SendMessage (GetParent(hWnd), uMsg, wParam, lParam);
      }
      break;

   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}
#endif // !UNIFIEDTRANSCRIPT



/*************************************************************************************
CMainWindow::TranscriptBackground - Assign/update the background image to the transcript window.

inputs
   BOOL           fForceRefresh - Force refresh
*/
void CMainWindow::TranscriptBackground (BOOL fForceRefresh)
{
   // if not window, do nothing
   if (!m_pTranscriptWindow || !m_hWndTranscript)
      return;

   // if no bitmap then do nothing
   if (!m_ahbmpJPEGBackDarkExtra[BACKGROUND_TEXT]) {
      m_pTranscriptWindow->m_hbmpBackground = NULL;

      if (fForceRefresh && m_pTranscriptWindow->m_pPage)
         m_pTranscriptWindow->m_pPage->Invalidate ();

      return;
   }

   // get transparency
#ifdef MIFTRANSPARENTWINDOWS // dont do for transparent windows
   BOOL fTransparent = !ChildHasTitle (m_hWndTranscript);
#else
   BOOL fTransparent = FALSE;
#endif

   if (fTransparent) {
      RECT rWindow;
      if (m_pTranscriptWindow->m_hWnd)
         GetWindowRect (m_pTranscriptWindow->m_hWnd, &rWindow);
      else {
         GetClientRect (m_hWndTranscript, &rWindow);
         ClientToScreen (m_hWndTranscript, ((LPPOINT)&rWindow) + 0);
         ClientToScreen (m_hWndTranscript, ((LPPOINT)&rWindow) + 1);
      }

      m_pTranscriptWindow->m_hbmpBackground = BackgroundStretchCalc (BACKGROUND_TEXT, &rWindow, WANTDARK_DARKEXTRA, GetParent(m_hWndTranscript), 1,
         &m_pTranscriptWindow->m_rBackgroundTo, &m_pTranscriptWindow->m_rBackgroundFrom);
   }
   else
      m_pTranscriptWindow->m_hbmpBackground = NULL;

   if (fForceRefresh && m_pTranscriptWindow->m_pPage)
      m_pTranscriptWindow->m_pPage->Invalidate ();

}


/*************************************************************************************
CMainWindow::WndProcTranscript - Manages the window calls
*/
LRESULT CMainWindow::WndProcTranscript (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWndTranscript = hWnd;
      }
      break;

   case WM_MOUSEWHEEL:
      {
         // if it's over the map window then send there
         if (m_pTranscriptWindow && IsWindowVisible(m_pTranscriptWindow->m_hWnd)) {
            POINT p;
            RECT r;
            p.x = (short)LOWORD(lParam);
            p.y = (short)HIWORD(lParam);
            GetWindowRect (hWnd, &r);
            if (PtInRect (&r, p))
               return 0;// default behaviour
               // calling causes stack problems return SendMessage (m_pTranscriptWindow->m_hWnd, uMsg, wParam, lParam);
         }

         return SendMessage (GetParent(hWnd), uMsg, wParam, lParam);
      }
      break;

#ifdef UNIFIEDTRANSCRIPT
   case WM_MOUSEACTIVATE:
      {
         // see if can type here
         if (m_pTranscriptWindow && m_pTranscriptWindow->m_hWnd)
            SetFocus (m_pTranscriptWindow->m_hWnd);
         return MA_ACTIVATE;
      }
      break;
#else
      // DIsable typing into transcript window
   //case WM_MOUSEACTIVATE:
   //   {
   //      // see if can type here
   //      if (m_pMenuWindow && m_pMenuWindow->m_hWnd)
   //         SetFocus (m_pMenuWindow->m_hWnd);
   //      return MA_ACTIVATE;
   //   }
   //   break;
#endif

      // NOTE: No paint, since causes flicker

   case WM_MOUSEMOVE:
      VerbTooltipUpdate ();
      HotSpotTooltipUpdate ();
      break;

   case WM_MOVING:
      if (TrapWM_MOVING (hWnd, lParam, FALSE))
         return TRUE;
      break;
   case WM_SIZING:
      if (TrapWM_MOVING (hWnd, lParam, TRUE))
         return TRUE;
      break;

   // BUGFIX - Both WM_MOVE and WM_SIZE end up doing the same thing
   case WM_MOVE:
   case WM_SIZE:
      {
         ChildLocSave (hWnd, TW_TRANSCRIPT, NULL, NULL);

         // might need to move if background
         TranscriptBackground (FALSE);

         // BUGFIX - If no transcript window then create
         if (!m_pTranscriptWindow && IsWindowVisible (hWnd))
            TranscriptUpdate();

         if (!m_pTranscriptWindow || !m_pTranscriptWindow->m_hWnd)
            break;

         // adjust the size
         RECT r;
         GetClientRect (hWnd, &r);
         m_pTranscriptWindow->PosnSet (&r);
      }
      break;

   case WM_CLOSE:
      // just hide this
#ifndef UNIFIEDTRANSCRIPT
      // cant hide transcript window if combined
      ChildShowWindow (m_hWndTranscript, TW_TRANSCRIPT, NULL, SW_HIDE);


      // show bottom pane so player knows is closed
      if (m_pSlideBottom)
         m_pSlideBottom->SlideDownTimed (1000);
#endif
      return 0;

   case WM_DESTROY:
      if (m_pTranscriptWindow)
         delete m_pTranscriptWindow;
      m_pTranscriptWindow = NULL;
      break;

   case WM_CHAR:
      {
#ifdef UNIFIEDTRANSCRIPT
      // if in unified transcript then send keystrokes down
         if (m_pTranscriptWindow && m_pTranscriptWindow->m_hWnd)
            return SendMessage (m_pTranscriptWindow->m_hWnd, uMsg, wParam, lParam);
#else
         // see if can type here... doesn't semto get called so no worry
         //if (m_pMenuWindow && m_pMenuWindow->m_hWnd)
         //   break; // return SendMessage (pvi->WindowForTextGet(), uMsg, wParam, lParam);
         //else  // pass it up
#endif
      }
      break;

   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*************************************************************************************
CMainWindow::WndProc - Manages the window calls for the main window
*/
LRESULT CMainWindow::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWndPrimary = hWnd; // just in case

         // m_Subtitle.Init (hWnd);

         // BUGBUG - temporarily disable so that can see if causing crash
         // new hypnomanager
         // BUGBUG - renable, as it should be, for full test
         m_pHM = new CHypnoManager;
         if (!m_pHM)
            return 0;
         if (!m_pHM->Init ()) {
            delete m_pHM;
            m_pHM = NULL;
            // ignore the error
         }
         m_pHM->EffectSet ((PWSTR)m_memHypnoEffectInfinite.p);
         m_pHM->MonitorsSet (gfMonitorUseSecond ? 2 : 1);
         // BUGBUG

         if (m_hWndSecond) {
            RECT rClient;
            GetClientRect (m_hWndSecond, &rClient);
            if (m_pHM)
               m_pHM->MonitorResSet (1, rClient.right - rClient.left, rClient.bottom - rClient.top);
         }


         SetTimer (hWnd, TIMER_VERBTOOLTIPUPDATE, 200, NULL);     // BUGFIX - Changed tooltip update timers to 200 ms, from 100ms
         SetTimer (hWnd, TIMER_HOTSPOTTOOLTIPUPDATE, 200, NULL);
         SetTimer (hWnd, TIMER_ANIMATE, ANIMATETIME, NULL);
         SetTimer (hWnd, TIMER_IMAGECACHE, 1000 * 30, NULL);
         SetTimer (hWnd, TIMER_USAGE, USAGETIME, NULL);
         QueryPerformanceCounter (&m_liLastCount);

         // post a message to this so will try to log on
         PostMessage (hWnd, WM_USER+123, 0, 0);

         // misc creation
         PostMessage (hWnd, WM_USER+125, 0, 0);

      }
      break;

   case WM_SHOWSETTINGSUI:
      if (IsWindowEnabled (m_hWndPrimary))
         DialogSettings (wParam);
      return 0;

   case WM_ENABLE:
      // if have second-monitor window then disable that too
      if (m_hWndSecond)
         EnableWindow (m_hWndSecond, wParam);
      break;

   case WM_MOUSEMOVE:
      {
         HCURSOR hCursor;
         hCursor = m_hCursorNo;
         SetCursor (hCursor); // BUGFIX


         VerbTooltipUpdate ();
         HotSpotTooltipUpdate();
      }
      break;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case ID_ACCELERATORF1:
         TabSwitch (0);
         return 0;
      case ID_ACCELERATORF2:
         TabSwitch (1);
         return 0;
      case ID_ACCELERATORF3:
         TabSwitch (2);
         return 0;
      case ID_ACCELERATORF4:
         TabSwitch (3);
         return 0;

      case ID_ACCELERATOREDITFOCUS:

#ifdef UNIFIEDTRANSCRIPT
         if (m_fTransCommandVisible)
            CommandSetFocus (TRUE);
#else
         if (m_pSlideTop->CommandIsVisible()) {
            CommandSetFocus ();

            // make sure to stay open for awhile
            m_pSlideTop->SlideDownTimed (2000);
         }
#endif
         else
            BeepWindowBeep (ESCBEEP_DONTCLICK);
         return 0;

      case ID_ACCELERATORCHATFOCUS:
         {
            if (!SpeakSetFocus (TRUE))
               BeepWindowBeep (ESCBEEP_DONTCLICK);
         }
         return 0;
      }
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC = BeginPaint (hWnd, &ps);

         RECT rTab;
         //ClientRectGet (&rTab);
         GetClientRect (hWnd, &rTab);

         if (!m_pHM || !m_pHM->BitBltImage (hDC, &rTab, 0)) {
            HBRUSH hbr = CreateSolidBrush (RGB(0x80,0x80,0x80));
            FillRect (hDC, &rTab, hbr);
            DeleteObject (hbr);
         }
         // BUGFIX - No longer using background stretch because of hypno
         // BackgroundStretch (TRUE, WANTDARK_NORMAL, &rTab, hWnd, hWnd, 1, hDC, NULL);

#if 0 // handled by BackgroundStretch
#ifdef MIFTRANSPARENTWINDOWS
         if (m_hbmpJPEGBack) {
            HDC hDCBmp = CreateCompatibleDC (hDC);
            SelectObject (hDCBmp, m_hbmpJPEGBack);
            int   iOldMode;
            iOldMode = SetStretchBltMode (hDC, COLORONCOLOR);
            StretchBlt(
               hDC, rTab.left, rTab.top, rTab.right - rTab.top, rTab.bottom - rTab.top,
               hDCBmp, 0, 0, m_pJPEGBack.x, m_pJPEGBack.y,
               SRCCOPY);
            SetStretchBltMode (hDC, iOldMode);
            DeleteDC (hDCBmp);
         }
         else
#endif // MIFTRANSPARENTWINDOWS
            FillRect (hDC, &rTab, m_hbrJPEGBack); // BUGFIX - Was m_hbrBackground);
#endif // 0

         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_MIDIFROMESCARPMENT:
      if (m_pAT)
         m_pAT->PostMidiMessage (wParam, lParam);
      return 0;

   case WM_RECORDDIR:
      {
         PCMem pMem = (PCMem) lParam;

         // record wave

         // if already recording then do nothnig
         if (m_fVCRecording) {
            delete pMem;
            return TRUE;
         }

         // stop audio
         if (m_pAT) {
            m_pAT->Mute (TRUE);
            Sleep (20); // give it time to stop
         }

         CListVariable lToDo;
         PWSTR psz = (PWSTR)pMem->p;
         lToDo.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));

         RecBatch2 (WAVECALC_TRANSPROS, hWnd, NULL, FALSE, TRUE, &lToDo, m_szRecordDir, L"TTSRec");

         // restart autio
         if (m_pAT)
            m_pAT->Mute (FALSE);

         delete pMem;
      }
      return 0;

   case WM_ACTIVATEAPP:
      if (m_fMinimizeIfSwitch) {
         if (wParam) {
            // shoe this and secondary window
            if (m_hWndSecond)
               ShowWindow (m_hWndSecond, SW_SHOW);
            if (m_hWndPrimary)
               ShowWindow (m_hWndPrimary, SW_SHOW);
            return 0;
         }
         else {   //!wParam
            // minimize this and secondary window
            if (m_hWndPrimary && IsWindowEnabled(m_hWndPrimary))
               ShowWindow (m_hWndPrimary, SW_SHOWMINNOACTIVE);
            if (m_hWndSecond && IsWindowEnabled(m_hWndPrimary))
               ShowWindow (m_hWndSecond, SW_SHOWMINNOACTIVE);
         }
      }

      // if switch to another window stop voice chat
      if (!wParam)
         VoiceChatStop ();
      break; // default behavior

   case WM_ACTIVATE:
      {
         // see if the current focus window is OK - NOTE: I dont think the code does anything, but leave in just in case
         // It's supposed to set the focus to the escwindow if have one
         HWND hWndFocus = GetFocus ();
         PCVisImage *ppv = (PCVisImage*)m_lPCVisImage.Get(0);
         DWORD i;
         for (i = 0; i < m_lPCVisImage.Num(); i++) {
            if (!ppv[0]->CanGetFocus())
               continue;

            if (hWndFocus != ppv[0]->WindowGet())
               continue;   // not same window
            return DefWindowProc (hWnd, uMsg, wParam, lParam);   // allow normal processing.. doesnt seem to get called
         }

         // set focus to edit
         CommandSetFocus(FALSE);
      }
      return 0;

   case WM_TIMER:
      if (wParam == TIMER_PROGRESS) {
         // while at it, update the hypnoeffect
         HypnoEffectTimer ((fp)PROGRESSTIME / 1000.0);

         // see if anything rendered while at it
         VisImageSeeIfRendered ();

         // while at it, invalidate rects
         PVISIMAGETIMER pvit = (PVISIMAGETIMER)m_lVISIMAGETIMER.Get(0);
         DWORD i;
         DWORD dwTime = GetTickCount();
         for (i = 0; i < m_lVISIMAGETIMER.Num(); i++) {
            if (dwTime < pvit[i].dwTime)
               continue;   // not time yet

            // else, invalidate
            pvit[i].pvi->InvalidateSliders ();

            // remove
            m_lVISIMAGETIMER.Remove (i);
            i--;
            pvit = (PVISIMAGETIMER)m_lVISIMAGETIMER.Get(0); // in case changed
         } // i

         // get information about progress
         if (m_pIT && m_pIT->m_pPacket)
            m_pIT->m_pPacket->InfoGet (&m_MPI);

         // figur out if want to dispaly the progress bar
         BOOL fShow = FALSE;
         if (m_fRenderProgress)
            fShow = TRUE;
         if (m_fTTSLoadProgress)
            fShow = TRUE;
         if (m_fShowingDownload)
            fShow = TRUE;
         if (m_MPI.iReceiveBytesExpect != m_MPI.iReceiveBytes) {
            fShow = TRUE;
            m_fShowingDownload = TRUE; // so will show at least once after finished
               // to ensirethat cleared
         }
         else
            m_fShowingDownload = FALSE;   // no longer showing

         // keep track of how many times has wanted to show
         if (fShow) {
            if (!m_dwShowProgressTicks)
               m_iBytesShow = m_MPI.iReceiveBytes; // remember where started to show

            m_dwShowProgressTicks++;
         }
         else
            m_dwShowProgressTicks = 0;

         fShow = (m_dwShowProgressTicks >= 4);  // if want to show for more than 1 sec than

         // refresh if show is true, or we were visible before
         if (fShow /*|| m_fProgressVisible*/)
            m_pSlideTop->InvalidateProgress ();
         // m_fProgressVisible = fShow;
      }
      else if (wParam == TIMER_VOICECHAT) {
         m_dwVCTimeRecording += VCTIMER_INTERVAL;

         // update the wave display
         m_pSlideTop->InvalidatePTT ();

         // pull out wave if necesary
         EnterCriticalSection (&m_csVCStopped);
         PCM3DWave pCut = VoiceChatStream (&m_VCWave, &m_lVCEnergy, m_fVCHavePreviouslySent);
         LeaveCriticalSection (&m_csVCStopped);
         if (pCut) {
            m_fVCHavePreviouslySent = TRUE;

            // send this off to the compression thread
            VoiceChatWaveSnippet (pCut);
         }

         // if have been talking for more than 30 seconds then
         // automatically stop voice chat
         if (m_dwVCTimeRecording >= 30 * 1000)
            VoiceChatStop ();

      }
      if (wParam == TIMER_MESSAGEDISABLED) {
         // has been awhile since disable, so re-enable
         HotSpotEnable ();

#ifndef UNIFIEDTRANSCRIPT
         // if there's text in the edit control that's highlighted
         // then delete it. Do this because will have sent message when clicked
         // on menu, and will display it in the text
         m_pSlideTop->CommandTextSet (NULL);
#endif
      }
      else if (wParam == TIMER_USAGE)
         // keep track of how much time has spent on this
         GetAndIncreaseUsage (USAGETIME / 1000 / 60); // in minutes
      else if (wParam == TIMER_IMAGECACHE)
         ImageCacheTimer (FALSE);
      else if (wParam == TIMER_ANIMATE) {
         // if tutorial cursor then move it
         if (m_fTutorialCursor) {
            // keep track of how long running
            m_dwTutorialCursorTime += ANIMATETIME;
            if (m_dwTutorialCursorTime > 4000) {
               // stop after this one
               m_fTutorialCursor = FALSE;
               m_dwTutorialCursorTime = 0;
            }

            POINT pCur, pDelta;
            GetCursorPos (&pCur);
            pDelta.x = m_pTutorialCursor.x - pCur.x;
            pDelta.y = m_pTutorialCursor.y - pCur.y;

            fp fDist = sqrt((fp)(pDelta.x * pDelta.x + pDelta.y * pDelta.y));
            fp fMove = fDist;
            fp fMaxRate = (fp) ANIMATETIME / 1000.0 * (fp) GetSystemMetrics (SM_CXSCREEN) / 2.0;
            fMove = min(fMove, fMaxRate);

            if (fDist > TUTORIALCLOSEENOUGH) {
               pDelta.x = (int)((fp)pDelta.x * fMove / fDist);
               pDelta.y = (int)((fp)pDelta.y * fMove / fDist);

#if 0 // def _DEBUG
               POINT pDeltaOrig = pDelta;
#endif

               // if go to far then stop
               if ((pCur.x <= m_pTutorialCursor.x) && (pCur.x + pDelta.x >= m_pTutorialCursor.x))
                  pDelta.x = m_pTutorialCursor.x - pCur.x;
               else if ((pCur.x >= m_pTutorialCursor.x) && (pCur.x + pDelta.x <= m_pTutorialCursor.x))
                  pDelta.x = m_pTutorialCursor.x - pCur.x;

               if ((pCur.y <= m_pTutorialCursor.y) && (pCur.y + pDelta.y >= m_pTutorialCursor.y))
                  pDelta.y = m_pTutorialCursor.y - pCur.y;
               else if ((pCur.y >= m_pTutorialCursor.y) && (pCur.y + pDelta.y <= m_pTutorialCursor.y))
                  pDelta.y = m_pTutorialCursor.y - pCur.y;

#if 0 // def _DEBUG
               char szTemp[64];
               sprintf (szTemp, "\r\nCur = %d,%d, DeltaOrig = %d,%d, Delta = %d,%d",
                  (int)pCur.x, (int)pCur.y,
                  (int)pDeltaOrig.x, (int)pDeltaOrig.y,
                  (int)pDelta.x, (int)pDelta.y);
               OutputDebugString (szTemp);
#endif
               // BUGFIX - mouse_event busted
#if 0
               INPUT input;
               memset (&input, 0, sizeof(input));
               input.type = INPUT_MOUSE;
               input.mi.dx = (int)((fp)(pCur.x + pDelta.x) / (fp) GetSystemMetrics(SM_CXVIRTUALSCREEN) * (fp)0xffff);
               input.mi.dy = (int)((fp)(pCur.y + pDelta.y) / (fp) GetSystemMetrics(SM_CXVIRTUALSCREEN) * (fp)0xffff);
               input.mi.dwFlags = MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
               SendInput (1, &input, sizeof(input));
#endif // 0
               pDelta.x = (int)((fp)(pCur.x + pDelta.x) / (fp) GetSystemMetrics(SM_CXVIRTUALSCREEN) * (fp)0xffff);
               pDelta.y = (int)((fp)(pCur.y + pDelta.y) / (fp) GetSystemMetrics(SM_CYVIRTUALSCREEN) * (fp)0xffff);
               mouse_event (MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE,
                  pDelta.x, pDelta.y, 0, NULL);
               //mouse_event (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE,
               //   pCur.x + pDelta.x, pCur.y + pDelta.y, 0, NULL);
            }
            else {
               m_fTutorialCursor = FALSE;
               m_dwTutorialCursorTime = 0;
            }
         }

         LARGE_INTEGER iOld = m_liLastCount;
         __int64 iDelta;
         fp fDelta;
         QueryPerformanceCounter (&m_liLastCount);
         iDelta = *((__int64*)&m_liLastCount) - *((__int64*)&iOld);
         iDelta *= 1000;   // so know to milliseoncds
         iDelta /= *((__int64*)&m_liPerCountFreq);
         fDelta = (double)(int)iDelta / 1000.0;

         DWORD i;
         PCVisImage *pvi = (PCVisImage*) m_lPCVisImage.Get(0);
         for (i = 0; i < m_lPCVisImage.Num(); i++)
            pvi[i]->AnimateAdvance (fDelta);

         return 0;
      }
      else if (wParam == TIMER_RANDOMACTIONS) {
         // select a random action
         CMem memAction;
         GUID gObject;

         LANGID lid = 0;

         switch (rand() % 6) {
            case 0:  // random map
               if (rand() % 4)
                  return 0;   // dont do map actions that frequently

               // random map action
               if (!m_pMapWindow || !m_pMapWindow->RandomAction (&memAction, &gObject, &lid))
                  return 0;
               break;

            case 1: // random tab
               RandomActionTab ();
               return 0;   // return after randomly switching tab

            case 2:  // random icon
               RandomActionClickOnIcon ();
               return 0;   // return after randomly clicking icon

            case 3:  // random image
               if (!RandomActionVisImage (&memAction, &gObject, &lid))
                  return 0;
               break;

            case 4:  // random transcript
               if (!RandomActionTranscript (&memAction, &gObject, &lid))
                  return 0;
               break;

            case 5:  // random toolbar
               if (!RandomActionResVerb (&memAction, &gObject, &lid))
                  return 0;   // nothing
               break;
         } // switch

         // pick a random click (and object)
         GUID gClick = GUID_NULL;
         CListFixed lObjects;
         lObjects.Init (sizeof(GUID));
         PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
         DWORD i;
         for (i = 0; i < m_lPCVisImage.Num(); i++)
            if (!IsEqualGUID(pvi[i]->m_gID, GUID_NULL))
               lObjects.Add (&pvi[i]->m_gID);
         if (lObjects.Num()) for (i = 0; i < 2; i++) {
            if (i && (rand() % 10))
               continue;   // if doing object, only randomize it once in awhile

            GUID *pgRandom = (GUID*)lObjects.Get((DWORD)rand() % lObjects.Num());
            if (i)
               gObject = *pgRandom;
            else
               gClick = *pgRandom;
         } // i



         // do it
         SendTextCommand (lid ? lid : m_lidCommandLast, (PWSTR)memAction.p, NULL, &gObject, &gClick,
            TRUE, TRUE, TRUE);
         return 0;
      }
      else if (wParam == TIMER_VERBTOOLTIPUPDATE) {
         // if there's no verb then dont bother
         if (!((PWSTR)m_memVerbShow.p)[0])
            return 0;

         POINT p, pClient;
         GetCursorPos (&p);

         // make sure over this window
         HWND hWndDesktop = GetDesktopWindow ();
         DWORD dwMonitor = (DWORD)-1;
         pClient = p;
         ScreenToClient (hWndDesktop, &pClient);
         HWND hWndChild = RealChildWindowFromPoint (hWndDesktop, pClient);
         if (hWndChild == m_hWndPrimary)
            dwMonitor = 0;
         else if (m_hWndSecond && (hWndChild == m_hWndSecond))
            dwMonitor = 1;
         else
            return 0;

         // see what internal child window over
         pClient = p;
         HWND hWndMain = dwMonitor ? m_hWndSecond : m_hWndPrimary;
         ScreenToClient (hWndMain, &pClient);
         hWndChild = RealChildWindowFromPoint (hWndMain, pClient);
         if (!hWndChild || (hWndChild == hWndMain))
            return 0;

         // see if it's over any of the icon windows
         DWORD i;
         PCVisImage pvi = NULL;
         PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
         for (i = 0; i < m_lPCIconWindow.Num(); i++) {
            PCIconWindow pi = ppi[i];

            if (hWndChild != pi->m_hWnd)
               continue;

            // see if it's over
            RECT rClient;
            GetClientRect (pi->m_hWnd, &rClient);
            pClient = p;
            ScreenToClient (pi->m_hWnd, &pClient);

            if (!PtInRect (&rClient, pClient))
               continue;

            // else over, see
            BOOL fMenu;
            pvi = pi->MouseOver (pClient.x, pClient.y, &fMenu);
            if (pvi && !fMenu)
               break;
            else
               pvi = NULL;
         }

         // if not over icon window, see if over main window,
         // otherwise causes flickering in <Click> toolbar
         if (!pvi) {
            PCDisplayWindow *ppd = (PCDisplayWindow*)m_lPCDisplayWindow.Get(0);
            for (i = 0; i < m_lPCDisplayWindow.Num(); i++) {
               PCDisplayWindow pd = ppd[i];

               if (!pd->m_pvi || IsEqualGUID(pd->m_pvi->m_gID, GUID_NULL))
                  continue;

               // see if it's over
               RECT rClient;
               GetClientRect (pd->m_hWnd, &rClient);
               pClient = p;
               ScreenToClient (pd->m_hWnd, &pClient);

               if (PtInRect (&rClient, pClient)) {
                  pvi = pd->m_pvi;
                  break;
               }
            } // i
         } // !pvi

         // update
         if (pvi && !IsEqualGUID(pvi->m_gID, GUID_NULL))
            VerbTooltipUpdate (dwMonitor, TRUE, p.x, p.y);
         else
            VerbTooltipUpdate (); // to clear
      }
      else if (wParam == TIMER_HOTSPOTTOOLTIPUPDATE) {
         POINT p, pClient;
         GetCursorPos (&p);

         // make sure over this window
         HWND hWndDesktop = GetDesktopWindow ();
         DWORD dwMonitor = (DWORD)-1;
         pClient = p;
         ScreenToClient (hWndDesktop, &pClient);
         HWND hWndChild = RealChildWindowFromPoint (hWndDesktop, pClient);
         if (hWndChild == m_hWndPrimary)
            dwMonitor = 0;
         else if (m_hWndSecond && (hWndChild == m_hWndSecond))
            dwMonitor = 1;
         else
            return 0;

         // see what internal child window over
         pClient = p;
         HWND hWndMain = dwMonitor ? m_hWndSecond : m_hWndPrimary;
         ScreenToClient (hWndMain, &pClient);
         hWndChild = RealChildWindowFromPoint (hWndMain, pClient);
         if (!hWndChild || (hWndChild == hWndMain))
            return 0;

         // see if it's over any of the display windows
         DWORD i;
         PCVisImage pvi = NULL;
         PCDisplayWindow *ppd = (PCDisplayWindow*)m_lPCDisplayWindow.Get(0);
         for (i = 0; i < m_lPCDisplayWindow.Num(); i++) {
            PCDisplayWindow pd = ppd[i];

            if (hWndChild != pd->m_hWnd)
               continue;

            // see if it's over
            RECT rClient;
            GetClientRect (pd->m_hWnd, &rClient);
            pClient = p;
            ScreenToClient (pd->m_hWnd, &pClient);

            if (!PtInRect (&rClient, pClient))
               continue;

            pvi = pd->m_pvi;
            if (pvi)
               break;
         }

         // update
         if (pvi)
            HotSpotTooltipUpdate (pvi, dwMonitor, TRUE, pClient.x, pClient.y);
         else
            HotSpotTooltipUpdate (); // to clear
      }
      break;

   case WM_MOUSEWHEEL:
      {
         // if it's over the map window then send there
         if (m_pMapWindow && IsWindowVisible(m_pMapWindow->m_hWnd)) {
            POINT p;
            RECT r;
            p.x = (short)LOWORD(lParam);
            p.y = (short)HIWORD(lParam);
            GetWindowRect (m_pMapWindow->m_hWnd, &r);
            if (PtInRect (&r, p))
               return SendMessage (m_pMapWindow->m_hWnd, uMsg, wParam, lParam);
         }

         // BUGBUG - this is misdirecting mouse wheel, so doesn't work if secondary MML window displayed
         PCDisplayWindow pMain = FindMainDisplayWindow ();
         if (pMain)
            return SendMessage (pMain->m_hWnd, uMsg, wParam, lParam);
         else
            break;
      }

   case WM_USER+123: // try to log on
      {
         if (m_pIT || m_pRT || m_pAT)
            return 0;   // alredy have

         m_pAT = new CAudioThread;
         if (!m_pAT)
            return 0;
         if (!m_pAT->Connect (hWnd, WM_USER+127)) {
            EscMessageBox (hWnd, gpszCircumrealityClient, L"Error starting the audio thread.", NULL, MB_OK);

            delete m_pAT;
            m_pAT = NULL;
            DestroyWindow (hWnd);   // kill this
            return 0;
         }

         m_pRT = new CRenderThread;
         if (!m_pRT)
            return 0;   // shouldnt happen
         if (!m_pRT->Connect (hWnd, WM_USER+126)) {
            // alert the user that failed
            EscMessageBox (hWnd, gpszCircumrealityClient, L"Error starting the render thread.", NULL, MB_OK);

            delete m_pRT;
            m_pRT = NULL;
            DestroyWindow (hWnd);   // kill this
            return 0;
         }
         m_pRT->ResolutionSet (m_iResolution, m_dwShadowsFlags, m_iTextureDetail, m_fLipSync,
            m_iResolutionLow, m_dwShadowsFlagsLow, m_fLipSyncLow, FALSE);
         m_pRT->Vis360Changed (m_f360Long, m_f360Lat);


         // NOTE: Need to create the internet thread last
         m_pIT = new CInternetThread;
         if (!m_pIT)
            return 0;   // error that shouldn happen

         //char *pszServer = "CircumrealityServer";
         //char *pszClient = "CircumrealityClient";
         int iWinSockErr;
         if (!m_pIT->Connect (m_szConnectFile, m_fConnectRemote, m_dwConnectIP, m_dwConnectPort, hWnd, WM_USER+124, &iWinSockErr)) {
            // alert the user that failed
            PWSTR pszErr;

            if (!m_fConnectRemote)
               pszErr = L"The server application, CircumrealityWorldSim.exe, failed to run. The .CRF file might be bad.";
            else {
               switch (iWinSockErr) {
               case WSAENETDOWN:
               case WSAENETUNREACH:
                  pszErr = L"You don't seem to be connected to the Internet. You should "
                     L"connect and retry.";
                  break;

               case WSAETIMEDOUT:
               case WSAEADDRNOTAVAIL:
                  pszErr = L"The server doesn't seem to be running at the moment.";
                  break;

               case WSAEINVAL:
               default:
                  pszErr = L"You might not be connected to the Internet, the server might be "
                     L"down, or you might have a firewall preventing a connection.";
                  break;
               }

               wcscat (m_pIT->m_szError, pszErr);
               pszErr = m_pIT->m_szError;
            }

            m_fServerNotFoundErrorShowing = TRUE;
            EscMessageBox (hWnd, gpszCircumrealityClient, L"The server was not found.",
               pszErr, MB_OK);
            m_fServerNotFoundErrorShowing = FALSE;

            delete m_pIT;
            m_pIT = NULL;
            DestroyWindow (hWnd);   // kill this

            // BUGBUG - May want to return back to main login page?
            return 0;
         }

         m_fCanAskForDownloads = TRUE;

         // send some information about the system
         TIME_ZONE_INFORMATION tzi;
         memset (&tzi, 0, sizeof(tzi));
         GetTimeZoneInformation (&tzi);
         InfoForServer (L"timezone", NULL, -(fp)tzi.Bias / 60.0);
         InfoForServer (L"langid", NULL, m_langID);
         InfoForServer (L"paid", NULL, RegisterMode()); // so server knows if has registered/paid
         // NOTE: Sending later InfoForServer (L"graphspeed", NULL, m_dwServerSpeed);
         InfoForServer (L"rendercache", NULL, (fp)gfRenderCache);

         // send preferences
         InfoForServer (L"quicklogon", NULL, m_fQuickLogon);
         PCPasswordFile pPF = PasswordFileCache (FALSE);
         if (pPF) {
            if (pPF->m_dwAge)
               InfoForServer (L"age", NULL, pPF->m_dwAge);
            if (pPF->m_dwHoursPlayed)
               InfoForServer (L"hoursplayed", NULL, pPF->m_dwHoursPlayed);
            if (pPF->m_fDisableTutorial)
               InfoForServer (L"disabletutorial", NULL, pPF->m_fDisableTutorial);
            if (pPF->m_fExposeTimeZone)
               InfoForServer (L"exposetimezone", NULL, pPF->m_fExposeTimeZone);
            if (pPF->m_iPrefGender)
               InfoForServer (L"prefgender", NULL, pPF->m_iPrefGender);
            if (pPF->m_szDescLink[0])
               InfoForServer (L"desclink", pPF->m_szDescLink, NULL);
            if (pPF->m_szEmail[0])
               InfoForServer (L"email", pPF->m_szEmail, NULL);
            if (pPF->m_szPrefName[0])
               InfoForServer (L"prefname", pPF->m_szPrefName, NULL);
            if (((PWSTR)pPF->m_memPrefDesc.p)[0])
               InfoForServer (L"prefdesc", (PWSTR)pPF->m_memPrefDesc.p, NULL);
            PasswordFileRelease();
         } // if pPF

         // send a command for logon right away
         CMem mem;
         MemZero (&mem);
         MemCat (&mem, L"`logon \"");
         MemCatSanitize (&mem, m_szUserName);
         MemCat (&mem, L"\" \"");
         MemCatSanitize (&mem, m_szPassword);
         MemCat (&mem, L"\" ");
         MemCat (&mem, (int)m_fNewAccount);
         SendTextCommand (DEFLANGID, (PWSTR)mem.p, NULL, NULL, NULL, FALSE, FALSE, FALSE);

         // else, succesfully connected
      }
      return 0;

   case WM_MAINWINDOWNOTIFYTRANSUPDATE:
      TranscriptUpdate();  // called when transcript size changed
      return 0;

   case WM_MAINWINDOWNOTIFYTRANSCRIPT:
   case WM_MAINWINDOWNOTIFYTRANSCRIPTEND:
      {
         PCMMLNode2 pNode = (PCMMLNode2) lParam;

         // get the guid
         GUID gActor;
         if (sizeof(gActor) != MMLValueGetBinary (pNode, L"ID", (PBYTE)&gActor, sizeof(gActor)))
            gActor = GUID_NULL;

         // go through and get the params... do this twice in case have to
         // convert to strings
         DWORD i;
         PWSTR pszActor, pszLanguage, pszSpeak;
         for (i = 0; i < 2; i++) {
            pszActor = MMLValueGet (pNode, L"Name");
            pszLanguage = MMLValueGet (pNode, L"Language");
            pszSpeak = MMLValueGet (pNode, L"Spoken");
         } // i

         if (uMsg == WM_MAINWINDOWNOTIFYTRANSCRIPT) {
            // tell phrase that have started speaking
            if (m_fSubtitleSpeech) {
               // m_Subtitle.Phrase (&gActor, pszActor, pszSpeak, FALSE);
               if (m_pTickerTape)
                  m_pTickerTape->Phrase (&gActor, pszActor, pszSpeak, FALSE);
            }
         }
         else {
            // tell phrase that have stopped speaking, whether or not subtitle shown
            // m_Subtitle.Phrase (&gActor, pszActor, pszSpeak, TRUE);
            if (m_pTickerTape)
               m_pTickerTape->Phrase (&gActor, pszActor, pszSpeak, TRUE);

            // BUGFIX - Move the transcript to when finished speaking, so don't read ahead of time
            TranscriptString (0, pszActor, &gActor, pszLanguage, pszSpeak);
         }

         delete pNode;  // so dont leak
      }
      return 0;

   case WM_MAINWINDOWNOTIFYVISEMEMESSAGE:
      {
         DWORD dwTickCount = (DWORD)wParam;
         PCMem pMem = (PCMem) lParam;
         PVISEMEMESSAGE pvm = (PVISEMEMESSAGE) pMem->p;

         // send to all the visimages
         DWORD i;
         PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
         for (i = 0; i < m_lPCVisImage.Num(); i++, pvi++)
            if (IsEqualGUID((*pvi)->m_gID, pvm->gID))
               (*pvi)->Viseme (pvm->dwViseme, dwTickCount);

         delete pMem;
      }
      return 0;
         

   case WM_MAINWINDOWNOTIFYAUDIOSTART:
      {
         PCMem pMem = (PCMem) lParam;

         // send to all the icon windows indicating that the actor is speaking
         PCIconWindow *ppi = (PCIconWindow*) m_lPCIconWindow.Get(0);
         DWORD i;
         for (i = 0; i < m_lPCIconWindow.Num(); i++)
            ppi[i]->AudioStart ((GUID*)pMem->p);

         delete pMem;
      }
      return 0;
         
   case WM_MAINWINDOWNOTIFYAUDIOSTOP:
      {
         PCMem pMem = (PCMem) lParam;

         // send to all the icon windows indicating that the actor is speaking
         PCIconWindow *ppi = (PCIconWindow*) m_lPCIconWindow.Get(0);
         DWORD i;
         for (i = 0; i < m_lPCIconWindow.Num(); i++)
            ppi[i]->AudioStop ((GUID*)pMem->p);

         delete pMem;
      }
      
      return 0;

   case WM_USER+127: // message from audio thread
   case WM_USER+124: // new data came in, wParam = message, lParam = pNode
      {
         DWORD dwType = (DWORD)wParam;

         PCMMLNode2 pNode = (PCMMLNode2)lParam;

         switch (dwType) {
         case CIRCUMREALITYPACKET_MMLIMMEDIATE:
         case CIRCUMREALITYPACKET_MMLQUEUE:
            // NOTE: These message packets will only come from CAudioThread::RequestAudio(),
            // which can be called from the internet thread

            // if gets here, send to render request as high priority
            if (gpMainWindow->m_pRT)
               gpMainWindow->m_pRT->RequestRender (pNode, RTQUEUE_HIGH, -1, FALSE);


            if (pNode) {
               // NOTE: Expecting time
               size_t dwSize = 0;
               LARGE_INTEGER *pli = (LARGE_INTEGER *) pNode->AttribGetBinary (L"time", &dwSize);
               _ASSERTE (pli && (dwSize == sizeof(LARGE_INTEGER)));
               __int64 iTime = 0;
               if (pli && (dwSize == sizeof(LARGE_INTEGER)) )
                  iTime = pli->QuadPart;

               MessageParse (pNode, iTime);   // this will delete pNode
            }
            return 0;


         case CIRCUMREALITYPACKET_CLIENTVERSIONNEED:   // lparam contains version number
            {
               if (lParam > CLIENTVERSIONNEED) {
                  EscMessageBox (hWnd,
                     gpszCircumrealityClient,
                     L"You need a more recent version of Circumreality to access this world.",
                     L"Visit http://www.Circumreality.com for a new version.", MB_ICONWARNING | MB_OK);

                  CloseNicely (); // so save
                  DestroyWindow (hWnd);
               }
            }
            break;

         default:
            if (pNode)
               delete pNode;
            break;
         } // switch
      }
      return 0;

   case WM_USER+125: // misc creation
      {
         // cache out here so is faster to load
         PCPasswordFile pPF = PasswordFileCache (FALSE);

         // load settings
         PCMMLNode2 pNode;
         pNode = UserLoad (FALSE, gpszMiscInfo);
         m_iPreferredQualityMono = PREFSPEEDUNDEF;
         m_fPowerSaver = FALSE;
         // NOTE: Don't need to modify m_fsubtitlespeech and m_fsubtitletext here
         // m_fSubtitleSpeech = TRUE;  // BUGFIX - Try with this on again
         // m_fSubtitleText = TRUE;

         // default TTS quality based on the number of processors
         // BUGFIX - TTS quality based on CPU sped
         //DWORD dwProcessors = HowManyProcessors();
         //if (dwProcessors <= 1)
         //   m_iTTSQuality = 0;
         //else if (dwProcessors < 6)
         //   m_iTTSQuality = 1;
         //else
         //   m_iTTSQuality = 2;
         if (giCPUSpeed < 1)
            m_iTTSQuality = 0;
         else // if (giCPUSpeed < 5)
            m_iTTSQuality = 1;
         // BUGFIX - Never choose high quality by default
         // else
         //   m_iTTSQuality = 2;

         m_fDisablePCM = FALSE;
         if (pNode) {
            m_iResolution = MMLValueGetInt (pNode, gpszResolution, giDefaultResolution);
            m_iResolutionLow = MMLValueGetInt (pNode, gpszResolutionLow, giDefaultResolutionLow);
            m_dwShadowsFlags = MMLValueGetInt (pNode, gpszShadowsFlags, gdwDefaultShadowFlags);
            m_dwShadowsFlagsLow = MMLValueGetInt (pNode, gpszShadowsFlagsLow, gdwDefaultShadowFlagsLow);
            m_dwServerSpeed = MMLValueGetInt (pNode, gpszServerSpeed, gdwServerSpeed);
            m_dwMovementTransition = MMLValueGetInt (pNode, gpszMovementTransition, 1);   // default to movmement transition of 1
            m_dwArtStyle = MMLValueGetInt (pNode, gpszArtStyle, m_dwArtStyle);
            m_iTextureDetail = MMLValueGetInt (pNode, gpszTextureDetail, giDefaultTextureDetail);
            m_fLipSync = MMLValueGetInt (pNode, gpszLipSync, gfLipSync);
            m_fLipSyncLow = MMLValueGetInt (pNode, gpszLipSyncLow, gfLipSyncLow);
            m_fPowerSaver = (BOOL) MMLValueGetInt (pNode, gpszPowerSaver, FALSE);
            m_fSubtitleSpeech = (BOOL) MMLValueGetInt (pNode, gpszSubtitleSpeech, m_fSubtitleSpeech);  // BUGFIX - Try with this on again
            m_fSubtitleText = (BOOL) MMLValueGetInt (pNode, gpszSubtitleText, m_fSubtitleText);
            m_fTTSAutomute = (BOOL) MMLValueGetInt (pNode, gpszTTSAutomute, m_fTTSAutomute);
            m_iTTSQuality = MMLValueGetInt (pNode, gpszTTSQuality, m_iTTSQuality);
            m_fDisablePCM = (BOOL) MMLValueGetInt (pNode, gpszDisablePCM, m_fDisablePCM);

            m_dwSubtitleSize = (DWORD)MMLValueGetInt (pNode, gpszSubtitleFontSize, 2);
            m_dwSubtitleSize = max(m_dwSubtitleSize, 1);
            //m_Subtitle.FontSet (m_dwSubtitleSize);
                  // BUGFIX - Default subtitles are off because of transcript
                  // BUGFIX - Subtitles can't be 0 any more
            if (m_pTickerTape)
               m_pTickerTape->FontSet (m_dwSubtitleSize);

            // NOTE: May want m_iTextureDetail written into registry so don't continually
            // change texture cache resolution

            // see if have set a preferred speed. If not written then default resoltion and shadow flags
            m_iPreferredQualityMono = MMLValueGetInt (pNode, gpszPreferredQualityMono, PREFSPEEDUNDEF);

            // m_fSlideLocked = MMLValueGetInt (pNode, gpszSlideLocked, FALSE);
                  // BUGFIX - default to locked in place
                  // BUGFIX - Autohide to screen looks less cluttered

            WCHAR szTemp[64];
            DWORD i;
            for (i = 0; i < NUMTABS; i++) {
               swprintf (szTemp, L"SlideLocked%d", (int)i);
               m_afSlideLocked[i] = (DWORD) MMLValueGetInt (pNode, szTemp, m_afSlideLocked[i]);
            } // i

            m_fSlideLocked = m_afSlideLocked[0];  // first tab

            m_fMinimizeIfSwitch = MMLValueGetInt (pNode, gpszMinimizeIfSwitch, TRUE);

            delete pNode;
         }
         else {
            m_dwSubtitleSize = 2;
            // m_Subtitle.FontSet (m_dwSubtitleSize); // default
            if (m_pTickerTape)
               m_pTickerTape->FontSet (m_dwSubtitleSize);
         }

         if (m_pHM)
            m_pHM->LowPowerSet (m_fPowerSaver);

         // BUGFIX - Moves sliding windows up here
         // create new sliding window
         m_pSlideTop = new CSlidingWindow;
         if (m_pSlideTop) {
            if (!m_pSlideTop->Init (hWnd, TRUE, m_fSlideLocked, this)) {
               delete m_pSlideTop;
               m_pSlideTop = NULL;
            }
         }
         m_pSlideBottom = NULL; // BUGFIX - No bottom slider: new CSlidingWindow;
         if (m_pSlideBottom) {
            if (!m_pSlideBottom->Init (hWnd, FALSE, m_fSlideLocked, this)) {
               delete m_pSlideBottom;
               m_pSlideBottom = NULL;
            }
         }

         m_pTickerTape = new CTickerTape;
         if (m_pTickerTape) {
            if (!m_pTickerTape->Init (hWnd, FALSE, (int)m_dwSubtitleSize-2, this)) {
               delete m_pTickerTape;
               m_pTickerTape = NULL;
            }
         }

         // BUGFIX - move speed setting out of if pNode, so new users are initialized
         if ( m_iPreferredQualityMono == PREFSPEEDUNDEF ) {
            m_iPreferredQualityMono = CPUSpeedToQualityMono (giCPUSpeed);
            ResolutionQualityToRenderSettings (m_fPowerSaver, m_iPreferredQualityMono, TRUE, 
               &m_iResolution, &m_dwShadowsFlags,
               &m_dwServerSpeed, &m_iTextureDetail, &m_fLipSync,
               &m_iResolutionLow, &m_dwShadowsFlagsLow, &m_fLipSyncLow,
               &m_dwMovementTransition);
         }
         if (m_pRT) {
            m_pRT->ResolutionSet (m_iResolution, m_dwShadowsFlags, m_iTextureDetail, m_fLipSync,
               m_iResolutionLow, m_dwShadowsFlagsLow, m_fLipSyncLow, FALSE);
            m_pRT->ResolutionCheck ();
         }

         // send server speed now since finally have value
         InfoForServer (L"graphspeed", NULL, m_dwServerSpeed);
         InfoForServer (L"movementtransition", NULL, (RegisterMode() != 0) ? m_dwMovementTransition : 0);
         InfoForServer (L"artstyle", NULL, m_dwArtStyle);
               // if hasn't paid then no transition


         // tell the server
         if (m_f360FOV != m_f360FOVSentToServer) {
            m_f360FOVSentToServer = m_f360FOV;
            InfoForServer (L"fov", NULL, m_f360FOV);
         }
         if (m_fMainAspect != m_fMainAspectSentToServer) {
            m_fMainAspectSentToServer = m_fMainAspect;
            InfoForServer (L"mainaspect", NULL, m_fMainAspect);
         }

         // progress timer
         SetTimer (m_hWndPrimary, TIMER_PROGRESS, PROGRESSTIME, NULL);

         WNDCLASS wc;
         RECT r;
         BOOL fHidden, fTitle;
         DWORD dwMonitor;
         POINT pCenter;

#ifndef UNIFIEDTRANSCRIPT
         // menu window
         memset (&wc, 0, sizeof(wc));
         wc.hInstance = ghInstance;
         wc.lpfnWndProc = MenuWndProc;
         wc.lpszClassName = "CircumrealityClientMenu";
         wc.style = CS_HREDRAW | CS_VREDRAW;
         wc.hIcon = NULL;
         wc.hCursor = m_hCursorNo;
         wc.hbrBackground = NULL; // m_hbrBackground;
         RegisterClass (&wc);

         dwMonitor = 0;
         GetClientRect (m_hWndPrimary, &r);  // default to primary
         pCenter.x = (r.left + r.right)/2;
         pCenter.y = (r.top + r.bottom)/2;
         r.left = (r.left + pCenter.x*2) / 3;
         r.right = (r.right + pCenter.x*2) / 3;
         r.top = (r.top + pCenter.y*2) / 3;
         r.bottom = (r.bottom + pCenter.y*2) / 3;

#if 0 // no longer used
         pNode = UserLoad (TRUE, gpszMenu);
         if (pNode) {
         //   m_fMenuPosSet = ChildLocGet (pNode, &r, &fHidden);
            delete pNode;
         }
#endif // 0
         m_fMenuPosSet = ChildLocGet (TW_MENU, NULL, &dwMonitor, &r, &fHidden, &fTitle);

         ChildShowTitleIfOverlap (NULL, &r, dwMonitor, fHidden, &fTitle);
         m_hWndMenu = CreateWindowEx (
            (fTitle ? WS_EX_IFTITLE : WS_EX_IFNOTTITLE) | WS_EX_ALWAYS,
            wc.lpszClassName, "Menu",
            WS_ALWAYS | (fTitle ? WS_IFTITLE : 0) | WS_CLIPSIBLINGS,
            r.left , r.top , r.right - r.left , r.bottom - r.top ,
            dwMonitor ? m_hWndSecond : m_hWndPrimary,
            NULL, ghInstance, (PVOID) this);
#endif // !UNIFIEDTRANSCRIPT





         // transcript window
         memset (&wc, 0, sizeof(wc));
         wc.hInstance = ghInstance;
         wc.lpfnWndProc = TranscriptWndProc;
         wc.lpszClassName = "CircumrealityClientTranscript";
         wc.style = CS_HREDRAW | CS_VREDRAW;
         wc.hIcon = NULL;
         wc.hCursor = m_hCursorNo;
         wc.hbrBackground = NULL; // m_hbrBackground;
         RegisterClass (&wc);

         dwMonitor = 0;
         GetClientRect (m_hWndPrimary, &r);  // default to primary
         pCenter.x = (r.left + r.right)/2;
         pCenter.y = (r.top + r.bottom)/2;
         r.left = (r.left + pCenter.x*2) / 3;
         r.right = (r.right + pCenter.x*2) / 3;
         r.top = (r.top + pCenter.y*2) / 3;
         r.bottom = (r.bottom + pCenter.y*2) / 3;

         pNode = UserLoad (FALSE, gpszTranscript);
         if (pNode) {
            // ChildLocGet (pNode, &r, &fHidden);

            EnterCriticalSection (&m_crSpeakBlacklist);
            m_iTransSize = MMLValueGetInt (pNode, L"TransSize", 0);

            m_iTextVsSpeech = MMLValueGetInt (pNode, L"TextVsSpeech", m_iTextVsSpeech);

            WCHAR szTemp[64];
            DWORD i;
            for (i = 0; i < NUMTABS; i++) {
               swprintf(szTemp, L"SpeakSpeed%d", (int)i);
               m_aiSpeakSpeed[i] = MMLValueGetInt (pNode, szTemp, (i == 2) ? 1 : 0);

               swprintf(szTemp, L"MuteAll%d", (int)i);
               m_afMuteAll[i] = (BOOL) MMLValueGetInt (pNode, szTemp, m_afMuteAll[i]);

               swprintf(szTemp, L"TransShow%d", (int)i);
               m_adwTransShow[i] = (DWORD) MMLValueGetInt (pNode, szTemp, m_adwTransShow[i]);
            } // i

            m_iSpeakSpeed = m_aiSpeakSpeed[m_pSlideTop->m_dwTab];
            m_fMuteAll = m_afMuteAll[m_pSlideTop->m_dwTab];
            m_dwTransShow = m_adwTransShow[m_pSlideTop->m_dwTab];

            CMem mem;
            MMLValueGetBinary (pNode, L"SpeakBlacklist", &mem);
            if (mem.m_dwCurPosn)
               m_lSpeakBlacklist.Init (sizeof(GUID), mem.p, (DWORD)mem.m_dwCurPosn / sizeof(GUID));
            LeaveCriticalSection (&m_crSpeakBlacklist);

            delete pNode;
         }

         // send a message to server so it knows if muted
         InfoForServer (L"muteall", NULL, m_fMuteAll);
         InfoForServer (L"transcript", NULL, (m_iTextVsSpeech <= 1) );
         InfoForServer (L"speakspeed", NULL, m_iSpeakSpeed);

         // BUGFIX - Moved this to after getting transcript settings since m_fMuteAll affects location
         // if the window
         fHidden = TRUE;
         fTitle = FALSE;
         ChildLocGet (TW_TRANSCRIPT, NULL, &dwMonitor, &r, &fHidden, &fTitle);
#ifdef UNIFIEDTRANSCRIPT
         // cant hide transcript window
         fHidden = FALSE;
#endif

         // BUGFIX - temporarily turn off saving so doesnt mess up settings
         gfChildLocIgnoreSave = TRUE;

         ChildShowTitleIfOverlap (NULL, &r, dwMonitor, fHidden, &fTitle);
         m_hWndTranscript = CreateWindowEx (
            (fTitle ? WS_EX_IFTITLE : WS_EX_IFNOTITLE) | WS_EX_ALWAYS,
            wc.lpszClassName, "Transcript",
            WS_ALWAYS | (fTitle ? WS_IFTITLECLOSE : 0) | WS_CLIPSIBLINGS | (fHidden ? 0 : WS_VISIBLE),
            r.left , r.top , r.right - r.left , r.bottom - r.top ,
            dwMonitor ? m_hWndSecond : m_hWndPrimary,
            NULL, ghInstance, (PVOID) this);

         // BUGFIX - temporarily turn off saving so doesnt mess up settings
         gfChildLocIgnoreSave = FALSE;
         // BUGFIX - Resave location because may have been messed up during create
         //ChildLocSave (m_hWndTranscript, TW_TRANSCRIPT, NULL, NULL);

         TranscriptUpdate ();


         // load in the existing chat window
         pNode = UserLoad (TRUE, gpszVerbChat);
         if (pNode) {
            if (!m_pResVerbChat) {
               m_pResVerbChat = new CResVerb;
               m_pResVerbChat->MMLFrom (pNode);
            }
            // dont doChildLocGet (pNode, &r, &m_fVerbHiddenByUser);
            delete pNode;
         }

         // load in the voice disguise
         // NOTE: Loading from account first. Failing that, loads from all
         pNode = UserLoad (TRUE, gpszVoiceDisguise);
         if (!pNode)
            pNode = UserLoad (FALSE, gpszVoiceDisguise);
         if (pNode) {
            m_VoiceDisguise.MMLFrom (pNode);
            delete pNode;
         }

         // create the map window
         m_pMapWindow = new CMapWindow;
         if (m_pMapWindow) {
            if (!m_pMapWindow->Init (this)) {
               delete m_pMapWindow;
               m_pMapWindow = NULL;
            }
         }

         // release
         if (pPF)
            PasswordFileRelease();
      }
      return 0;
      
   case WM_USER+126: // called when the render thread has something to say
      switch (wParam) {
      case RENDERTHREAD_RENDERED:
      case RENDERTHREAD_RENDEREDHIGH:
         {
            PCImageStore pis = (PCImageStore) lParam;
            DWORD dwQuality = (wParam == RENDERTHREAD_RENDEREDHIGH) ? 1 : 0;

            // see if should set the background
            BackgroundUpdate (BACKGROUND_IMAGES, pis, FALSE);

            // see if anything renderd
            VisImageSeeIfRendered (pis, dwQuality);


            break;
         }

      case RENDERTHREAD_PRELOAD:
         {
            // message from render-thread that may want to pre-load this image
            // lParam is PCMem that need to be deleted
            PCMem pMem = (PCMem) lParam;
            DWORD dwQuality;
            // load all qualities, just in case
            for (dwQuality = NUMIMAGECACHE-1; dwQuality < NUMIMAGECACHE; dwQuality--) {
               if (ImageCacheExists ((PWSTR)pMem->p, dwQuality, TRUE))
                  break;   // already cached with high quality, so dont ask for less

               // else, ask it to cache (if the file exists)
               if (m_amfImages[dwQuality].Exists((PWSTR)pMem->p)) {
                  ImageLoadThreadAdd ((PWSTR)pMem->p, dwQuality, 1);
                  break;   // just cached high quality
               }
            }
            delete pMem;
         }
         break;
         
      case RENDERTHREAD_PROGRESS:
         m_fRenderProgress = (double)lParam / 1000000.0;

         // BUGFIX - If set to 0 then update, so don't leave hanging at 100%
         if (!m_fRenderProgress)
            m_pSlideTop->InvalidateProgress ();
         break;

      case RENDERTHREAD_IMBORED:
         // notification that the render thread is bored. If we're not in power saver
         // mode, then send message to server indicating that bored

         // BUGFIX - I'm bored only happens on 64-bit systems because if do
         // on 32-bit systems will overflow the usual prerender cache of surrounding
         // rooms, so ends up slowing down system too much

         if (!m_fPowerSaver && (sizeof(PVOID) > sizeof(DWORD)) )
            InfoForServer (L"imbored", NULL, 1);
         break;
      }
      return 0;

   case WM_MAINWINDOWNOTIFYTTSLOADPROGRESS:
      {
         m_fTTSLoadProgress = (double)lParam / 100.0;

         // BUGFIX - If set to 0 then update, so don't leave hanging at 100%
         if (!m_fTTSLoadProgress)
            m_pSlideTop->InvalidateProgress ();

         // BUGFIX - Make sure this is visible while loading
         m_pSlideTop->SlideDownTimed (1000);
      }
      return 0;

   case WM_SIZE:
      {
         // hypnobackground
         RECT rClient;
         GetClientRect (hWnd, &rClient);
         if (m_pHM)
            m_pHM->MonitorResSet (0, rClient.right - rClient.left, rClient.bottom - rClient.top);

         if (wParam != SIZE_MINIMIZED) {
            // update the slide windows
            if (m_pSlideTop)
               m_pSlideTop->Resize();
            if (m_pSlideBottom)
               m_pSlideBottom->Resize();
            if (m_pTickerTape)
               m_pTickerTape->Resize();

            // update all window locs
            ChildShowMoveAll ();
         }
      }
      break;

   case WM_LBUTTONDOWN:
      {
       
         // if get here then beep
         BeepWindowBeep (ESCBEEP_DONTCLICK);
      }
      return 0;

   case WM_PACKETSENDERROR:
      {
         // if already have the message box up then do nothing
         if (m_fInPacketSendError)
            return 0;

         int iError = (int)wParam;
         PCWSTR pszErr = (PCWSTR)lParam;


         m_fInPacketSendError = TRUE;

         // BUGFIX - make sure window will be displayed
         MSG msg;
         while (m_fServerNotFoundErrorShowing) {
            while (PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
               TranslateMessage( &msg );
               DispatchMessage( &msg );
            }

            // sleep shortly between peek messages
            Sleep (1);

            // NOTE: Doesn't ever seem to get past here. I'm not sure why, but it doesn't matter
            // that much since not likely to get here (and player can end-task)
         }


         // say that have disconnected
         WCHAR szTemp[512];
         swprintf (szTemp,
            L"The Internet connection to the world has been unexpectedly disconnected. "
            L"Try reconnecting. If problems persist, wait a few hours and try reconnecting. "
            L"If you still can't connect then contact the world's administrators via E-mail."
            L"\r\n"
            L"Technical: Error %d, %s", (int)iError, pszErr ? pszErr : L"");

         EscMessageBox (hWnd, gpszCircumrealityClient,
            L"Disconnected from the world!",
            szTemp, MB_OK);
         // m_fInPacketSendError = FALSE;  - Leave TRUE since closing down

         // else, stop immediately
         gfQuitBecauseUserClosed = FALSE;  // so will ask if want to reconnect

         CloseNicely ();
         DestroyWindow (m_hWndPrimary);
      }
      return 0;


   case WM_DIRECTSOUNDERROR:
      {
         // if already have the message box up then do nothing
         // NOTE: Using same error flag as for packets
         if (m_fInPacketSendError)
            return 0;

         m_fInPacketSendError = TRUE;
         EscMessageBox (hWnd, gpszCircumrealityClient,
            L"DirectSound isn't working.",
            L"You need to install DirectSound 8 for this application to work. "
            L"You can get the DirectSound 8 download from www.Microsoft.com.", MB_OK);
         // m_fInPacketSendError = FALSE;  - Leave TRUE since closing down

         // else, stop immediately
         gfQuitBecauseUserClosed = TRUE;  // so dont ask if want to reconnect

         CloseNicely ();
         DestroyWindow (m_hWndPrimary);
      }
      return 0;


   case WM_CLOSE:
      {
         // BUGFIX - Use serverinfo as a way of indicating shutdown

         // tell user should use the stop playing command
         int iRet = EscMessageBox (hWnd, gpszCircumrealityClient,
            L"Do you want to stop playing safely?",
            L"Because players sometimes cheat at combat by closing their IF program while playing, "
            L"most IF titles penalize players that shut down immediate. To avoid this penalty, "
            L"press \"Yes\" and and wait 15 to 30 seconds for the application to shut down."
            L"\r\n"
            L"However, if you wish to "
            L"exit this program immediately and incur a penalty, press \"No\".", MB_YESNOCANCEL);

         // if cancel then exit
         if (iRet == IDCANCEL)
            return 0;

         if (iRet == IDYES) {
            InfoForServer (L"shutdown", NULL, 1);
            return 0;
         }
         
         // else, stop immediately
         gfQuitBecauseUserClosed = TRUE;  // so know user closed
         m_fInPacketSendError = TRUE;  // so ignore packet errors

         CloseNicely ();
      }
      break;   // fall through to destroy



   case WM_DESTROY:
      if (m_fRandomActions)
         KillTimer (hWnd, TIMER_RANDOMACTIONS);

      KillTimer (hWnd, TIMER_VERBTOOLTIPUPDATE);
      KillTimer (hWnd, TIMER_HOTSPOTTOOLTIPUPDATE);
      KillTimer (hWnd, TIMER_ANIMATE);
      KillTimer (hWnd, TIMER_IMAGECACHE);
      KillTimer (hWnd, TIMER_USAGE);
      KillTimer (hWnd, TIMER_PROGRESS);

      // m_Subtitle.End ();

      DestroyAllChildWindows();

      if (m_fMessageDiabled) {
         KillTimer (hWnd, TIMER_MESSAGEDISABLED);
         m_fMessageDiabled = FALSE;
      }

      if (m_hWndSecond) {
         DestroyWindow (m_hWndSecond);
         m_hWndSecond = NULL;
      }
      
      // BUGFIX - Start shutting down the audio so doesn't keep playing until
      // everything else shut down
      if (m_pAT)
         m_pAT->StartDisconnect();
      if (m_pHM)
         m_pHM->ShutDown();

      PostQuitMessage (0);
      m_hWndPrimary = NULL;
      break;

   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CMainWindow::WndProcSecond - Manages the window calls for the main window
*/
LRESULT CMainWindow::WndProcSecond (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWndSecond = hWnd; // just in case
      }
      break;

   case WM_MOUSEMOVE:
      {
         HCURSOR hCursor;
         hCursor = m_hCursorNo;
         SetCursor (hCursor); // BUGFIX

         // clear tooltip
         VerbTooltipUpdate ();
         HotSpotTooltipUpdate ();
      }
      break;


   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC = BeginPaint (hWnd, &ps);

         RECT rTab;
         //ClientRectGet (&rTab);
         GetClientRect (hWnd, &rTab);

         if (!m_pHM || !m_pHM->BitBltImage (hDC, &rTab, 1)) {
            HBRUSH hbr = CreateSolidBrush (RGB(0x80,0x80,0x80));
            FillRect (hDC, &rTab, hbr);
            DeleteObject (hbr);
         }
         // BUGFIX - No longer using background stretch because of hypno
         // BackgroundStretch (TRUE, WANTDARK_NORMAL, &rTab, hWnd, hWnd, 1, hDC, NULL);

#if 0 // handled by BackgroundStretch
#ifdef MIFTRANSPARENTWINDOWS
         if (m_hbmpJPEGBack) {
            HDC hDCBmp = CreateCompatibleDC (hDC);
            SelectObject (hDCBmp, m_hbmpJPEGBack);
            int   iOldMode;
            iOldMode = SetStretchBltMode (hDC, COLORONCOLOR);
            StretchBlt(
               hDC, rTab.left, rTab.top, rTab.right - rTab.top, rTab.bottom - rTab.top,
               hDCBmp, 0, 0, m_pJPEGBack.x, m_pJPEGBack.y,
               SRCCOPY);
            SetStretchBltMode (hDC, iOldMode);
            DeleteDC (hDCBmp);
         }
         else
#endif // MIFTRANSPARENTWINDOWS
            FillRect (hDC, &rTab, m_hbrJPEGBack); // BUGFIX - was m_hbrBackground);
#endif // 0

         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_ACTIVATE:
      {
         // see if the current focus window is OK - NOTE: I dont think the code does anything, but leave in just in case
         // It's supposed to set the focus to the escwindow if have one
         HWND hWndFocus = GetFocus ();
         PCVisImage *ppv = (PCVisImage*)m_lPCVisImage.Get(0);
         DWORD i;
         for (i = 0; i < m_lPCVisImage.Num(); i++) {
            if (!ppv[0]->CanGetFocus())
               continue;

            if (hWndFocus != ppv[0]->WindowGet())
               continue;   // not same window
            return DefWindowProc (hWnd, uMsg, wParam, lParam);   // allow normal processing.. doesnt seem to get called
         }

         // set focus to edit
         CommandSetFocus(FALSE);
      }
      return 0;

   case WM_MOUSEWHEEL:
      {
         // if it's over the map window then send there
         if (m_pMapWindow && IsWindowVisible(m_pMapWindow->m_hWnd)) {
            POINT p;
            RECT r;
            p.x = (short)LOWORD(lParam);
            p.y = (short)HIWORD(lParam);
            GetWindowRect (m_pMapWindow->m_hWnd, &r);
            if (PtInRect (&r, p))
               return SendMessage (m_pMapWindow->m_hWnd, uMsg, wParam, lParam);
         }

         PCDisplayWindow pMain = FindMainDisplayWindow ();
         if (pMain)
            return SendMessage (pMain->m_hWnd, uMsg, wParam, lParam);
         else
            break;
      }

   case WM_CHAR:
      {
         return SendMessage (m_hWndPrimary, uMsg, wParam, lParam);
      }
      break;

   case WM_SIZE:
      {
         // hypnobackground
         RECT rClient;
         GetClientRect (hWnd, &rClient);
         if (m_pHM)
            m_pHM->MonitorResSet (1, rClient.right - rClient.left, rClient.bottom - rClient.top);

         if (wParam != SIZE_MINIMIZED) {
            // update all window locs
            ChildShowMoveAll ();
         }
      }
      break;

   case WM_LBUTTONDOWN:
      {
       
         // if get here then beep
         BeepWindowBeep (ESCBEEP_DONTCLICK);
      }
      return 0;

   case WM_CLOSE:
      // if get this, do to main
      PostMessage (m_hWndPrimary, uMsg, wParam, lParam);
      return 0;

   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CMainWindow::CloseNicely - Closes the window when a WM_CLOSE is called.
*/
void CMainWindow::CloseNicely (void)
{
   // stop recording
   VoiceChatStop ();

   // shut down the voicechat thread
   VoiceChatThreadFree ();

   ImageLoadThreadFree ();

   // save all the child-window locations
   ChildLocSaveAll ();

   // BUGFIX - Call destroy all windows in close so that they're still visible
   DestroyAllChildWindows();

   // delete the map window here so can save location
   if (m_pMapWindow) {
      delete m_pMapWindow;
      m_pMapWindow = NULL;
   }

   if (m_pSlideTop) {
      delete m_pSlideTop;
      m_pSlideTop = NULL;
   }
   if (m_pSlideBottom) {
      delete m_pSlideBottom;
      m_pSlideBottom = NULL;
   }
   if (m_pTickerTape) {
      delete m_pTickerTape;
      m_pTickerTape = NULL;
   }

   // cache for speech
   PCPasswordFile pPF = PasswordFileCache (FALSE);

#if 0 // no longer used
   // save the menu location
   if (m_hWndMenu && m_fMenuPosSet) {
      CMMLNode2 node;
      node.NameSet (gpszMenu);
      // ChildLocSave (m_hWndMenu, &node, FALSE);
      UserSave (TRUE, gpszMenu, &node);
   }
#endif // 0

   // save the transcript location
   WCHAR szTemp[64];
   DWORD i;
   if (m_hWndTranscript) {
      CMMLNode2 node;
      node.NameSet (gpszTranscript);
      // ChildLocSave (m_hWndTranscript, &node, !IsWindowVisible (m_hWndTranscript));


      MMLValueSet (&node, L"TransSize", m_iTransSize);
      MMLValueSet (&node, L"TextVsSpeech", m_iTextVsSpeech);
      for (i = 0; i < NUMTABS; i++) {
         swprintf(szTemp, L"SpeakSpeed%d", (int)i);
         MMLValueSet (&node, szTemp, m_aiSpeakSpeed[i]);

         swprintf(szTemp, L"MuteAll%d", (int)i);
         MMLValueSet (&node, szTemp, (int) m_afMuteAll[i]);

         swprintf(szTemp, L"TransShow%d", (int)i);
         MMLValueSet (&node, szTemp, (int) m_adwTransShow[i]);
      } // i

      // MMLValueSet (&node, L"SpeakSpeed", m_iSpeakSpeed);

      if (m_lSpeakBlacklist.Num()) {
         EnterCriticalSection (&m_crSpeakBlacklist);
         MMLValueSet (&node, L"SpeakBlacklist", (PBYTE)m_lSpeakBlacklist.Get(0),
            m_lSpeakBlacklist.Num() * sizeof(GUID));
         LeaveCriticalSection (&m_crSpeakBlacklist);
      }

      UserSave (FALSE, gpszTranscript, &node);
   }

   if (m_hWndFlashWindow) {
      DestroyWindow (m_hWndFlashWindow);
      m_hWndFlashWindow = NULL;
   }

   PCMMLNode2 pNode;
   // save the command line location
   if (m_pResVerb) {
      pNode = m_pResVerb->MMLTo();
      if (pNode) {
         // ChildLocSave (m_hWndVerb, pNode, m_fVerbHiddenByUser);
         UserSave (TRUE, gpszVerb, pNode);
         delete pNode;
      }
   }

   // save the chat verbs
   if (m_pResVerbChat) {
      pNode = m_pResVerbChat->MMLTo();
      if (pNode) {
         UserSave (TRUE, gpszVerbChat, pNode);
         delete pNode;
      }
   }

   // save the voicedisguise info
   pNode = m_VoiceDisguise.MMLTo();
   if (pNode) {
      UserSave (TRUE, gpszVoiceDisguise, pNode);
      UserSave (FALSE, gpszVoiceDisguise, pNode);  // also in main options
      delete pNode;
   }

   // save misc settings
   {
      CMMLNode2 node;
      node.NameSet (gpszMiscInfo);
      MMLValueSet (&node, gpszResolution, m_iResolution);
      MMLValueSet (&node, gpszResolutionLow, m_iResolutionLow);
      MMLValueSet (&node, gpszShadowsFlags, (int) m_dwShadowsFlags);
      MMLValueSet (&node, gpszShadowsFlagsLow, (int) m_dwShadowsFlagsLow);
      MMLValueSet (&node, gpszPreferredQualityMono, m_iPreferredQualityMono);
      MMLValueSet (&node, gpszServerSpeed, (int)m_dwServerSpeed);
      MMLValueSet (&node, gpszMovementTransition, (int)m_dwMovementTransition);
      MMLValueSet (&node, gpszArtStyle, (int)m_dwArtStyle);
      MMLValueSet (&node, gpszTextureDetail, m_iTextureDetail);
      MMLValueSet (&node, gpszLipSync, m_fLipSync);
      MMLValueSet (&node, gpszLipSyncLow, m_fLipSyncLow);
      MMLValueSet (&node, gpszSlideLocked, (int)m_fSlideLocked);
      MMLValueSet (&node, gpszSubtitleFontSize, (int)m_dwSubtitleSize);
      MMLValueSet (&node, gpszPowerSaver, (int)m_fPowerSaver);
      MMLValueSet (&node, gpszMinimizeIfSwitch, (int)m_fMinimizeIfSwitch);
      MMLValueSet (&node, gpszSubtitleSpeech, (int)m_fSubtitleSpeech);
      MMLValueSet (&node, gpszSubtitleText, (int)m_fSubtitleText);
      MMLValueSet (&node, gpszTTSAutomute, (int)m_fTTSAutomute);
      MMLValueSet (&node, gpszTTSQuality, m_iTTSQuality);
      MMLValueSet (&node, gpszDisablePCM, (int) m_fDisablePCM);

      for (i = 0; i < NUMTABS; i++) {
         swprintf(szTemp, L"SlideLocked%d", (int)i);
         MMLValueSet (&node, szTemp, (int) m_afSlideLocked[i]);
      } // i

      UserSave (FALSE, gpszMiscInfo, &node);
   }

   if (pPF)
      PasswordFileRelease();
}


/*************************************************************************************
CMainWindow::MainClear - Clears the contents of the main display
*/
void CMainWindow::MainClear (void)
{
   // clear out everything
   VisImageClearAll(TRUE);

   // kill the old timer
   if (m_fMessageDiabled) {
      HotSpotEnable();
   }
}


/*************************************************************************************
PCMMLNodeToText - Gets the text out of the PCMMLNNode2

inputs
   PCMem       pMem - To concat to
   PCMMLNode2  pNode - Node
*/
void PCMMLNodeToText (PCMem pMem, PCMMLNode2 pNode)
{
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      psz = NULL;
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (psz)
         MemCat (pMem, psz);
      else if (pSub)
         PCMMLNodeToText (pMem, pSub);
   } // i
}


/*************************************************************************************
CMainWindow::MessageParse - This sets the value of the main display based upon the MML.

inputs
   PCMMLNode2        pNode - Node for the main display. This is FREED by this function
   __int64           iTime - Time when the message came in, to make sure menu not out of order.
                        Also used for <AutoCommand> to see which action to take.
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::MessageParse (PCMMLNode2 pNode, __int64 iTime)
{
   PWSTR psz = pNode->NameGet();

   // if it's an iconwindow then either find matching one, or create
   if (!_wcsicmp(psz, CircumrealityIconWindow())) {
      // get the ID
      psz = MMLValueGet (pNode, L"ID");
      if (!psz || !psz[0]) {
         delete pNode;
         return FALSE;
      }

      BOOL fDelete = (BOOL)MMLValueGetInt (pNode, L"Delete", 0);

      // find match?
      DWORD i;
      PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
      for (i = 0; i < m_lPCIconWindow.Num(); i++)
         if (!_wcsicmp((PWSTR)ppi[i]->m_memID.p, psz)) {
            if (fDelete) {
               delete ppi[i];
               m_lPCIconWindow.Remove (i);
               delete pNode;
               return TRUE;
            }
            else
               return ppi[i]->Update (pNode, iTime);   // update
         }

      // else, cant find, so add
      if (fDelete) {
         delete pNode;
         return TRUE;   // no point adding since will delete anyway
         }

      PCIconWindow pi = new CIconWindow;
      if (!pi) {
         delete pNode;
         return FALSE;
      }
      if (!pi->Init (this, pNode, iTime))
         return FALSE;
      m_lPCIconWindow.Add (&pi);
      return TRUE;
   }

   if (!_wcsicmp(psz, CircumrealityIconWindowDeleteAll())) {
      DestroyAllChildWindows (0x0001);

      delete pNode;
      return TRUE;
   }

   // transcript MML
   if (!_wcsicmp(psz, CircumrealityTranscriptMML())) {
      // find the MML
      PCMMLNode2 pSub = NULL;
      PWSTR psz;
      pNode->ContentEnum (pNode->ContentFind (L"MML"), &psz, &pSub);
      if (!pSub) {
         delete pNode;
         return TRUE;
      }
      
      // find the guid
      GUID gID;
      BOOL fHaveGUID = FALSE; 
      memset (&gID, 0, sizeof(gID));
      fHaveGUID = (sizeof(gID) == MMLValueGetBinary (pNode, L"ID", (PBYTE)&gID, sizeof(gID)));

      // find the name
      psz = MMLValueGet (pNode, L"Name");

      // get the string
      pSub->NameSet (L"Null");   // so can add more easily
      CMem mem;
      if (!MMLToMem (pSub, &mem, TRUE)) {
         delete pNode;
         return TRUE;
      }
      mem.CharCat (0);  // just to null terminate

      TranscriptString (1, psz, fHaveGUID ? &gID : NULL, NULL, (PWSTR)mem.p);

      // potentially show transcript
      if (m_fSubtitleText) {
         MemZero (&mem);

         PCMMLNodeToText (&mem, pNode);

         PWSTR psz2 = (PWSTR)mem.p;
         if (psz2 && psz2[0]) {
            //m_Subtitle.Phrase (&gID, psz, psz2, FALSE);
            //m_Subtitle.Phrase (&gID, psz, psz2, TRUE);

            if (m_pTickerTape) {
               m_pTickerTape->Phrase (&gID, psz, psz2, FALSE);
               m_pTickerTape->Phrase (&gID, psz, psz2, TRUE);
            }
         }
      }

      delete pNode;
      return TRUE;
   }

   // automap window
   if (!_wcsicmp(psz, CircumrealityAutoMap())) {
      m_pMapWindow->AutoMapMessage (pNode, FALSE);
      delete pNode;
      return TRUE;
   }

   // automap window
   if (!_wcsicmp(psz, CircumrealityAutoMapShow())) {
      m_pMapWindow->AutoMapMessage (pNode, TRUE);
      delete pNode;
      return TRUE;
   }

   // hypno
   if (!_wcsicmp(psz, CircumrealityHypnoEffect())) {
      fp fDuration = MMLValueGetDouble (pNode, L"duration", 0.0);
      fp fPriority = MMLValueGetDouble (pNode, L"priority", 1.0);
      PWSTR pszName = MMLValueGet (pNode, L"name");

      if (pszName)
         HypnoEffect (pszName, fDuration, fPriority);

      delete pNode;
      return TRUE;
   }

   // text background
   if (!_wcsicmp(psz, CircumrealityTextBackground())) {
      // get the light setting

      // BUGFIX - Default to whatever previous light setting was
      int iLight = MMLValueGetInt (pNode, L"Light", m_fLightBackground);

      // update
      BOOL fLight = iLight ? TRUE : FALSE;
      if (fLight != m_fLightBackground)
         LightBackgroundSet (fLight, TRUE);

      // see if have an image string
      PWSTR psz = MMLValueGet (pNode, L"image");
      if (psz && psz[0])
         BackgroundUpdate (BACKGROUND_TEXT, psz);

      delete pNode;
      return TRUE;
   }

   if (!_wcsicmp(psz, CircumrealityMapPointTo())) {
      // get the location
      CPoint pLoc;
      pLoc.Zero();
      MMLValueGetPoint (pNode, L"Loc", &pLoc);

      // get the color
      PWSTR psz;
      COLORREF cColor;
      psz = MMLValueGet (pNode, L"Color");
      if (!psz || !AttribToColor (psz, &cColor))
         cColor = (COLORREF)-1;  // default

      // get the name
      psz = MMLValueGet (pNode, L"Name");
      if (!psz)
         psz = L"";

      m_pMapWindow->MapPointTo (psz, &pLoc, cColor);

      delete pNode;
      return TRUE;
   }

   // auto command
   if (!_wcsicmp(psz, CircumrealityAutoCommand())) {
      LANGID lid = (LANGID)MMLValueGetInt (pNode, L"LangID", DEFLANGID);
      BOOL fSilence = MMLValueGetInt (pNode, L"Silent", FALSE);
      BOOL fCanCancel = MMLValueGetInt (pNode, L"CanCancel", FALSE);
      PWSTR psz = MMLValueGet (pNode, L"Command");
      PWSTR pszCancel = MMLValueGet (pNode, L"CancelCommand");

      if ((iTime < m_iTimeLastCommand) && fCanCancel) {
         // did an action before the autocommand
         if (pszCancel)
            SendTextCommand (lid, pszCancel, NULL, NULL, NULL, !fSilence, !fSilence, FALSE);
      }
      else if (psz)  // no action between auto command and this
         SendTextCommand (lid, psz, NULL, NULL, NULL, !fSilence, !fSilence, FALSE);

      delete pNode;
      return TRUE;
   }

   // if it's a NotInMain command then do
   if (!_wcsicmp(psz, CircumrealityNotInMain())) {
      // see if objects exist in main
      GUID gID;
      memset (&gID, 0, sizeof(gID));
      MMLValueGetBinary (pNode, L"ID", (PBYTE)&gID, sizeof(gID));

      PCVisImage pvi = FindMainVisImage ();
      if (!pvi)
         return TRUE;
      if (!IsEqualGUID (pvi->m_gID, gID))
         return TRUE;   // not displayed

      // find the first non-chat, then chat
      PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
      PCVisImage pLast;
      PCVisImage pBest = NULL;
      DWORD dwBestScore = 0;
      DWORD i, dwScore;
      for (i = 0; i < m_lPCIconWindow.Num(); i++) {
         dwScore = 0;

         PWSTR psz = (PWSTR)ppi[i]->m_memID.p;
         if (psz && !_wcsicmp(psz, L"main"))
            dwScore = 0;
         // BUGFIX - No more chat
         //else if (psz && !_wcsicmp(psz, L"chat"))
         //   dwScore = 1;
         else
            dwScore = ppi[i]->m_fChatWindow ? 3 : 2;

         pLast = ppi[i]->FindLastIcon ();
         if (!pLast)
            continue;

         if (!pBest || (dwScore < dwBestScore)) {
            pBest = pLast;
            dwBestScore = dwScore;
         }
      } // i

      if (pBest)
         SetMainWindow (pBest);

      return TRUE;
   }


   // if it's an DisplayWindow then either find matching one, or create
   if (!_wcsicmp(psz, CircumrealityDisplayWindow())) {
      // get the ID
      psz = MMLValueGet (pNode, L"ID");
      if (!psz || !psz[0]) {
         delete pNode;
         return FALSE;
      }

      BOOL fDelete = (BOOL)MMLValueGetInt (pNode, L"Delete", 0);

      // find match?
      DWORD i;
      PCDisplayWindow *ppi = (PCDisplayWindow*)m_lPCDisplayWindow.Get(0);
      for (i = 0; i < m_lPCDisplayWindow.Num(); i++)
         if (!_wcsicmp((PWSTR)ppi[i]->m_memID.p, psz)) {
            if (fDelete) {
               delete ppi[i];
               m_lPCDisplayWindow.Remove (i);
               delete pNode;
               return TRUE;
            }
            else
               return ppi[i]->Update (pNode, iTime);   // update
         }

      // else, cant find, so add
      if (fDelete) {
         delete pNode;
         return TRUE;   // no point adding since will delete anyway
         }

      PCDisplayWindow pi = new CDisplayWindow;
      if (!pi) {
         delete pNode;
         return FALSE;
      }
      if (!pi->Init (this, pNode, iTime)) {
         delete pi;
         return FALSE;
      }
      m_lPCDisplayWindow.Add (&pi);
      return TRUE;
   }

   if (!_wcsicmp(psz, CircumrealityDisplayWindowDeleteAll())) {
      DestroyAllChildWindows (0x0002);

      delete pNode;
      return TRUE;
   }

   // If get an object update display handle it
   if (!_wcsicmp(psz, CircumrealityObjectDisplay())) {
      BOOL fRet = ObjectDisplay (pNode, NULL, NULL, TRUE, TRUE, NULL, FALSE, iTime);
         // defaulting to m_fCanChatTo = FALSE
      delete pNode;
      return fRet;
   }


   // see if for changing view poisition
   if (!_wcsicmp(psz, CircumrealityPosition360())) {
      fp fRadToDeg = 360.0 / (2.0 * PI);
      m_f360FOV = MMLValueGetDouble (pNode, L"fov", m_f360FOV * fRadToDeg) / fRadToDeg;
      m_f360Long = MMLValueGetDouble (pNode, L"lrangle", m_f360Long * fRadToDeg) / fRadToDeg;
      m_f360Lat = MMLValueGetDouble (pNode, L"udangle", m_f360Lat * fRadToDeg) / fRadToDeg;

      // BUGFIX - min max based on variable
      m_f360FOV = max(m_f360FOV, m_tpFOVMinMax.h);
      m_f360FOV = min(m_f360FOV, m_tpFOVMinMax.v);
      m_f360Lat = max(m_f360Lat, -PI/2);
      m_f360Lat = min(m_f360Lat, PI/2);

      Vis360Changed();
      delete pNode;
      return TRUE;
   }

   // see if updated an image file
   if (!_wcsicmp(psz, CircumrealityBinaryDataRefresh())) {
      PWSTR pszFile = MMLValueGet (pNode, L"File");

      // clean up the cache, making sure the file is invalidated
      PCBitmapCache pCache = EscBitmapCache ();
      if (pCache) {
         if (pszFile)
            pCache->Invalidate (pszFile);
         pCache->CacheCleanUp(TRUE);
      }

      // NOTE: Shouldnt have to go through displaywindows since will also get a
      // message requesting to redraw everything

      delete pNode;
      return TRUE;
   }

   // see if for changing view poisition
   if (!_wcsicmp(psz, CircumrealityUploadImageLimits())) {
      // just store these away
      m_dwUploadImageLimitsNum = (DWORD) MMLValueGetInt (pNode, L"num", (int)m_dwUploadImageLimitsNum);
      m_dwUploadImageLimitsMaxWidth = (DWORD) MMLValueGetInt (pNode, L"maxwidth", (int)m_dwUploadImageLimitsMaxWidth);
      m_dwUploadImageLimitsMaxHeight = (DWORD) MMLValueGetInt (pNode, L"maxheight", (int)m_dwUploadImageLimitsMaxHeight);

      delete pNode;
      return TRUE;
   }

   // voice chat settings
   if (!_wcsicmp(psz, CircumrealityVoiceChatInfo())) {
      m_VoiceChatInfo.MMLFrom (pNode);
      delete pNode;

      // update the display
      m_pSlideTop->InvalidatePTT ();

      return TRUE;
   }

   // see if for changing min/max range
   if (!_wcsicmp(psz, CircumrealityFOVRange360())) {
      fp fRadToDeg = 360.0 / (2.0 * PI);
      fp fOldCurvature = m_fCurvature;
      m_tpFOVMinMax.h = MMLValueGetDouble (pNode, L"min", m_tpFOVMinMax.h * fRadToDeg) / fRadToDeg;
      m_tpFOVMinMax.v = MMLValueGetDouble (pNode, L"max", m_tpFOVMinMax.v * fRadToDeg) / fRadToDeg;
      m_fCurvature = MMLValueGetDouble (pNode, L"curvature", m_fCurvature);

      // make sure in some range
      m_tpFOVMinMax.h = max(m_tpFOVMinMax.h, PI/50);
      m_tpFOVMinMax.v = min(m_tpFOVMinMax.v, 2.0 * PI);
      m_tpFOVMinMax.v = max(m_tpFOVMinMax.v, m_tpFOVMinMax.h);

      // change FOV
      fp fOldFOV = m_f360FOV;
      m_f360FOV = max(m_f360FOV, m_tpFOVMinMax.h);
      m_f360FOV = min(m_f360FOV, m_tpFOVMinMax.v);

      if ((fOldFOV != m_f360FOV) || (fOldCurvature != m_fCurvature))
         Vis360Changed();
      delete pNode;
      return TRUE;
   }


   // see if for changing min/max range
   if (!_wcsicmp(psz, CircumrealityThreeDSound())) {
      if (m_pAT) {
         fp f;

         m_pAT->m_fEarsBackAtten = MMLValueGetDouble (pNode, L"back", m_pAT->m_fEarsBackAtten);
         m_pAT->m_fEarsPowDistance = MMLValueGetDouble (pNode, L"power", m_pAT->m_fEarsPowDistance);
         m_pAT->m_fEarsScale = MMLValueGetDouble (pNode, L"scale", m_pAT->m_fEarsScale);
         f = MMLValueGetDouble (pNode, L"separation", m_pAT->m_fEarsSpeakerSep);
         f = max(f, 0.01);
         m_pAT->m_fEarsSpeakerSep = f;
      }
      delete pNode;
      return TRUE;
   }


   if (!_wcsicmp (psz, CircumrealityVerbChat())) {
      // find the verb window
      PCMMLNode2 pVerb = NULL;
      PWSTR psz;
      pNode->ContentEnum(pNode->ContentFind (CircumrealityVerbWindow()), &psz, &pVerb);
      if (!pVerb) {
         // unidentified
         delete pNode;
         return TRUE;
      }

      // remember what was sent by the server
      if (!m_pResVerbChatSent)
         m_pResVerbChatSent = new CResVerb;
      if (m_pResVerbChatSent)
         m_pResVerbChatSent->MMLFrom (pVerb);

      // if we already have a resource AND the versions are the same, then
      // just ignore
      if (m_pResVerbChat && !_wcsicmp((PWSTR)m_pResVerbChat->m_memVersion.p, (PWSTR)m_pResVerbChatSent->m_memVersion.p)) {
         delete pNode;
         return TRUE;
      }

      // else, reload
      if (m_pResVerbChat) {
         m_pResVerbChat->MMLFrom (pVerb);
      }
      else {
         m_pResVerbChat = new CResVerb;
         if (!m_pResVerbChat) {
            delete pNode;
            return TRUE;
         }
         m_pResVerbChat->MMLFrom (pVerb);
      }

      // will need to alert chat window that have new chat verbs
      PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
      DWORD i;
      for (i = 0; i < m_lPCIconWindow.Num(); i++)
         ppi[i]->VerbChatNew ();

      delete pNode;
      return TRUE;
   }

   if (!_wcsicmp (psz, CircumrealityVerbWindow())) {
      // if contains a delete message then just hide it
      BOOL fDelete = (BOOL)MMLValueGetInt (pNode, L"Delete", FALSE);
      m_fVerbHiddenByServer = fDelete;
      VerbWindowShow ();
      if (fDelete) {
         delete pNode;
         return TRUE;
      }

      // remember what was sent by the server
      if (!m_pResVerbSent)
         m_pResVerbSent = new CResVerb;
      if (m_pResVerbSent)
         m_pResVerbSent->MMLFrom (pNode);

      // if we already have a resource AND the versions are the same, then
      // just ignore
      if (m_pResVerb && !_wcsicmp((PWSTR)m_pResVerb->m_memVersion.p, (PWSTR)m_pResVerbSent->m_memVersion.p)) {
         delete pNode;
         return TRUE;
      }

      // else, reload
      if (m_pResVerb) {
         m_pResVerb->MMLFrom (pNode);
      }
      else {
         m_pResVerb = new CResVerb;
         if (!m_pResVerb) {
            delete pNode;
            return TRUE;
         }
         m_pResVerb->MMLFrom (pNode);
      }

      // need to reposition verb buttons
      m_pSlideTop->ToolbarShow (FALSE);   // so will cause a redraw
      VerbWindowShow ();

      delete pNode;
      return TRUE;
   }

   if (!_wcsicmp (psz, CircumrealityCommandLine())) {
      // BUGFIX - Ignoring name for command line
      // BUGFIX - Ignoring commandline position
      // show or hide
      BOOL fHidden = (BOOL)MMLValueGetInt (pNode, L"Hidden", FALSE);

#ifdef UNIFIEDTRANSCRIPT
      fHidden = !fHidden;  // since need inteverse, and make sure bool
      if (fHidden != m_fTransCommandVisible) {
         m_fTransCommandVisible = fHidden;
         TranscriptUpdate ();
      }
#else
      m_pSlideTop->CommandLineShow (!fHidden);
#endif

      delete pNode;
      return TRUE;
   }

   // handle menu option
   if (!_wcsicmp (psz, CircumrealityGeneralMenu())) {
      // get the menu info
      m_lMenuShow.Clear();
      m_lMenuExtraText.Clear();
      m_lMenuDo.Clear();
      m_lidMenu = DEFLANGID;
      m_dwMenuDefault = -1;
      MMLFromContextMenu (pNode, &m_lMenuShow, &m_lMenuExtraText, &m_lMenuDo, &m_lidMenu, &m_dwMenuDefault);

      m_fMenuExclusive = (BOOL) MMLValueGetInt (pNode, L"Exclusive", 0);
      m_fMenuTimeOut = MMLValueGetDouble (pNode, L"TimeOut", 0);
      m_fMenuTimeOut = max(m_fMenuTimeOut, 0);
      m_fMenuTimeOutOrig = m_fMenuTimeOut;
      if (m_fMenuTimeOut && (m_dwMenuDefault >= m_lMenuShow.Num()))
         m_dwMenuDefault = 0; // always have some default

#ifndef UNIFIEDTRANSCRIPT
      // get the location
      if (!m_fMenuPosSet) {
         RECT r;
         BOOL fHidden, fTitle;
         GetClientRect (m_hWndMenu, &r);
         // m_fMenuPosSet = ChildLocGet (pNode, &r, &fHidden);
         DWORD dwWindow;      // NOTE: changes not tested after add in second monitor support
         m_fMenuPosSet = ChildLocGet (TW_MENU, NULL, &dwWindow, &r, &fHidden, &fTitle);
         if (m_fMenuPosSet)
            MoveWindow (m_hWndMenu, r.left, r.top, r.right-r.left, r.bottom-r.top, TRUE);
      }
#endif // !UNIFIEDTRANSCRIPT

      MenuReDraw ();

      // refresh verb window since m_fMenuExclusve may have changed visibility
      VerbWindowShow ();

      delete pNode;
      return TRUE;
   }

   if (!_wcsicmp (psz, CircumrealityChangePassword())) {
      PWSTR psz = MMLValueGet (pNode, L"Password");
      if (psz && (wcslen(psz) < 63)) {
         wcscpy (m_szPassword, psz);

         PCPasswordFileAccount pPFA = PasswordFileAccountCache ();
         if (pPFA) {
            wcscpy (pPFA->m_szPassword, m_szPassword);
            pPFA->Dirty();
            PasswordFileRelease();
         }

#if 0 // old code

#ifndef USEPASSWORDSTORE
         // obscure the password since don't want this written
         MMLValueSet (pNode, L"Password", L"JUNK");
#endif

         // save the password
         WCHAR szMega[256];
         wcscpy (szMega, m_szUserName);
         wcscat (szMega, L"\\Password");
         // NOTE: because pNode also has a value of "password", use the same node
         MMLFileSave (&m_mfUser, szMega, &GUID_MegaUserCache, pNode);

#endif
      }

      delete pNode;
      return TRUE;
   }

   if (!_wcsicmp (psz, CircumrealityLogOff())) {
      // NOTE: This sudden showdown causes some memory loss of any messages
      // that were in the PostMessage() queue but not dealt with. The
      // loss isn't major and it's only when shutting down

      delete pNode;

      // delete this window
      CloseNicely ();   // so save locs
      DestroyWindow (m_hWndPrimary);

      // BUGBUG - may want to ask user if wish to restart once a logoff occurs?

      return TRUE;
   }

   if (!_wcsicmp (psz, CircumrealityTitleInfo())) {
      // just got link for a connecting world
      if (gpLinkWorld)
         delete gpLinkWorld;

      gpLinkWorld = new CResTitleInfoSet;
      if (!gpLinkWorld) {
         delete pNode;
         return TRUE;
      }

      PCResTitleInfo pInfo = new CResTitleInfo;
      if (!pInfo) {
         delete gpLinkWorld;
         gpLinkWorld = NULL;
         delete pNode;
         return TRUE;
      }
      pInfo->MMLFrom (pNode);
      delete pNode;
      gpLinkWorld->m_lPCResTitleInfo.Add (&pInfo);
      
      return TRUE;
   }


   if (!_wcsicmp (psz, CircumrealityTutorial())) {
      // clear out the current tutorial MML
      MemZero (&m_memTutorial);

      // if there is any content then convert this to text
      if (pNode->ContentNum()) {
         m_memTutorial.m_dwCurPosn = 0;
         if (!MMLToMem (pNode, &m_memTutorial, TRUE))
            MemZero (&m_memTutorial);
         else
            m_memTutorial.CharCat (0);   // so null terminated
      }

      // update the transcript
      TranscriptUpdate ();
      delete pNode;
      return TRUE;
   }

   if (!_wcsicmp (psz, CircumrealityPointOutWindow())) {
      PWSTR psz = MMLValueGet (pNode, L"Type");
      DWORD dwType = -1;
      if (!psz)
         dwType = -1;
      else if (!_wcsicmp(psz, L"DisplayWindow"))
         dwType = TW_DISPLAYWINDOW;
      else if (!_wcsicmp(psz, L"IconWindow"))
         dwType = TW_ICONWINDOW;
      else if (!_wcsicmp(psz, L"Map"))
         dwType = TW_MAP;
      else if (!_wcsicmp(psz, L"Transcript"))
         dwType = TW_TRANSCRIPT;
      else if (!_wcsicmp(psz, L"Menu"))
         dwType = TW_MENU;
      else if (!_wcsicmp(psz, L"Command")) {
#ifdef UNIFIEDTRANSCRIPT
         dwType = TW_TRANSCRIPT;
#else
         m_fTutorialCursor = TRUE;
         m_dwTutorialCursorTime = 0;
         m_pSlideTop->TutorialCursor (4, &m_pTutorialCursor);
         delete pNode;
         return TRUE;
#endif
      }
      else if (!_wcsicmp(psz, L"Verb")) {
         m_fTutorialCursor = FALSE; // BUGFIX - since flashing TRUE;
         m_dwTutorialCursorTime = 0;
         RECT rWindow;
         m_pSlideTop->TutorialCursor (3, &m_pTutorialCursor, &rWindow);

         // start flash
         FlashChildWindow (&rWindow);

         delete pNode;
         return TRUE;
      }
      if (dwType == -1) {
         // unknown type, so ignore
         delete pNode;
         return TRUE;
      }

      // get the ID
      psz = MMLValueGet (pNode, L"ID");
      if (!psz)
         psz = L"";

      // show the window
      if (!ChildShowToggle (dwType, psz, 1))
         return TRUE;   // couldnt actually find it

      // find the window location
      RECT rLoc;
      memset (&rLoc, 0, sizeof(rLoc)); // BUGFIX was passing in 0 instead of sizeof(rLoc)
      BOOL fHidden = FALSE, fTitle = FALSE;
      DWORD dwWindow;
      ChildLocGet (dwType, psz, &dwWindow, &rLoc, &fHidden, &fTitle);

      m_fTutorialCursor = FALSE; // BUGFIX - Since flashing TRUE;
      m_dwTutorialCursorTime = 0;
      m_pTutorialCursor.x = (rLoc.left + rLoc.right)/2;
      m_pTutorialCursor.y = (rLoc.top + rLoc.bottom)/2;
      ClientToScreen (dwWindow ? m_hWndSecond : m_hWndPrimary, &m_pTutorialCursor);
      ClientToScreen (dwWindow ? m_hWndSecond : m_hWndPrimary, ((POINT*)&rLoc) + 0);
      ClientToScreen (dwWindow ? m_hWndSecond : m_hWndPrimary, ((POINT*)&rLoc) + 1);

      // start flash
      FlashChildWindow (&rLoc);

      delete pNode;
      return TRUE;
   }


   if (!_wcsicmp (psz, CircumrealitySwitchToTab())) {
      PWSTR psz = MMLValueGet (pNode, L"ID");
      DWORD dwType = -1;
      DWORD i;
      if (!psz)
         dwType = -1;
      else for (i = 0; i < sizeof(gapszTabNames)/sizeof(PWSTR); i++)
         if (!_wcsicmp (psz, gapszTabNames[i])) {
            dwType = i;
            break;
         }

      // if have a type then switch
      if (dwType != -1)
         TabSwitch (dwType, FALSE);

      delete pNode;
      return TRUE;
   }


   if (!_wcsicmp (psz, CircumrealityPieChart())) {
      DWORD dwID = (DWORD)MMLValueGetInt(pNode, L"ID", 0);
      dwID = min(dwID, 10);   // not too many

      fp fValue = MMLValueGetDouble (pNode, L"Value", 0);
      fp fDelta = MMLValueGetDouble (pNode, L"Delta", 0);

      COLORREF cr = 0;
      PWSTR psz = MMLValueGet (pNode, L"Color");
      if (psz)
         AttribToColor (psz, &cr);
      psz = MMLValueGet (pNode, L"Name");

      m_pSlideTop->PieChartAddSet (dwID, psz, fValue, fDelta, cr);

      delete pNode;
      return TRUE;
   }



#if 0 // no longer accepting CircumrealityImage(), etc. calls directly.
   if (!_wcsicmp(psz, CircumrealityImage()) || !_wcsicmp(psz, Circumreality3DScene()) || !_wcsicmp(psz, Circumreality3DObjects()) ||
      !_wcsicmp(psz, CircumrealityTitle()) || !_wcsicmp(psz, CircumrealityText())  || !_wcsicmp(psz, CircumrealityHelp())) {
      // NOTE: The <image/> tag accepts MMLValueSet() "File"=filename, and
      // "scale"="stretchtofit", "none", "scaletofit", "scaletocover"

      RECT r;
      GetClientRect (m_hWndRoom, &r);

      // delete old one
      DWORD dwIndex = VisImageFind (VIS_MAIN);
      if (dwIndex != -1)
         VisImageDelete (dwIndex);

      GUID g = GUID_NULL;  // NOTE - eventually right icon
      VisImageNew (pNode, NULL, VIS_MAIN, m_hWndRoom, &g, FALSE, L"Main", L"", &r);

      return TRUE;
   }
#endif // 0

   // delete node
   delete pNode;
   return FALSE;
}


/*************************************************************************************
CMainWindow::SimulateMOUSEMOVE - Simulates a WM_MOUSEMOVE to update the cursor
*/
void CMainWindow::SimulateMOUSEMOVE (void)
{
   // update cursor
   POINT p;
   RECT rClient;
   DWORD i;
   for (i = 0; i < 2; i++) {
      HWND hWnd = i ? m_hWndSecond : m_hWndPrimary;
      if (!hWnd)
         continue;

      GetCursorPos (&p);
      ScreenToClient (hWnd, &p);
      GetClientRect (hWnd, &rClient);
      if (PtInRect (&rClient, p))
         SendMessage (hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
   } // i

}

/*************************************************************************************
CMainWindow::HotSpotDisable - Disable hot spots and other actions for a second
or two so that the server has time to respond and get back.
*/
void CMainWindow::HotSpotDisable (void)
{
   // kill the old timer
   if (m_fMessageDiabled) {
      KillTimer (m_hWndPrimary, TIMER_MESSAGEDISABLED);
      m_fMessageDiabled = FALSE;
   }

   // create  anew one
   SetTimer (m_hWndPrimary, TIMER_MESSAGEDISABLED, 750, 0);
      // BUGFIX - Was 1500, but seemed too long
   m_fMessageDiabled = TRUE;

   // update cursor
   SimulateMOUSEMOVE ();
}

/*************************************************************************************
CMainWindow::HotSpotEnable - Re-enable hot spots if they're not already
*/
void CMainWindow::HotSpotEnable (void)
{
   // kill the old timer
   if (m_fMessageDiabled) {
      KillTimer (m_hWndPrimary, TIMER_MESSAGEDISABLED);
      m_fMessageDiabled = FALSE;

      // update cursor
      SimulateMOUSEMOVE ();
   }
}

/*************************************************************************************
CMainWindow::VisImageDelete - Clear based on index into m_lPCVisImage

inputs
   DWORD          dwIndex - index to clear
*/
BOOL CMainWindow::VisImageDelete (DWORD dwIndex)
{
   PCVisImage *pvi = (PCVisImage*) m_lPCVisImage.Get(dwIndex);
   if (!pvi)
      return FALSE;

   delete (*pvi);
   m_lPCVisImage.Remove (dwIndex);

   return TRUE;
}

/*************************************************************************************
CMainWindow::VisImageClearAll - Clears the list of visible images, and frees
up any associated memory.

inputs
   BOOL                 fInvalidate - If TRUE then invalidate the rectangle
*/
void CMainWindow::VisImageClearAll (BOOL fInvalidate)
{
   DWORD i;
   PCVisImage *pvi = (PCVisImage*) m_lPCVisImage.Get(0);
   for (i = 0; i < m_lPCVisImage.Num(); i++, pvi++)
      delete (*pvi);

   m_lPCVisImage.Clear();
}



/*************************************************************************************
CMainWindow::VisImageSeeIfRendered - This looks through all the images that
haven't yet been rendered and sees if they are rendered. If so, their
image is invalidated so it'll be redrawn.

inputs
   PCImageStore      pis - New image that came in. This image will ultimately
                     be deleted by the code. THis can be NULL.
   DWORD             dwQuality - 0 for a low-quality quick-render image, 1 for high quality
*/
void CMainWindow::VisImageSeeIfRendered (PCImageStore pis, DWORD dwQuality)
{
   PCImageStore pisLowQuality = NULL;

   // check to see if have a new image, and add it to the cache
   if (pis) {
      CMem mem;
      PWSTR pszFile;
      if (MMLToMem (pis->MMLGet(), &mem)) {
         mem.CharCat (0);  // just to null terminate
         pszFile = (PWSTR)mem.p;

         ImageCacheAdd (pszFile, pis, dwQuality);
         // pis will be taken over or deleted by ImageCacheAdd
      }

      if (dwQuality)
         pisLowQuality = ImageCacheFindLowQuality (pszFile);

      BackgroundUpdate (BACKGROUND_IMAGES, pis, FALSE);
   } // if pis

   DWORD i;
   PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
   for (i = 0; i < m_lPCVisImage.Num(); i++, pvi++) {
      if (pisLowQuality)
         (*pvi)->UpgradeQuality (pisLowQuality);
      (*pvi)->SeeIfRendered();
   }

   if (pis)
      ImageCacheRelease (pis, FALSE);
}


/*************************************************************************************
CMainWindow::VisImageReRender - Causes all the images to be re-rendered

Should call after ResolutionSet() changes
*/
void CMainWindow::VisImageReRenderAll (void)
{
   // clear out the cache
   DWORD dwQuality;
   for (dwQuality = 0; dwQuality < NUMIMAGECACHE; dwQuality++)
      m_amfImages[dwQuality].Clear();


   DWORD i;
   PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
   for (i = 0; i < m_lPCVisImage.Num(); i++, pvi++)
      (*pvi)->ReRenderAll();
}

/*************************************************************************************
CMainWindow::VisImageFind - Given an ID, find the image

inputs
   DWORD          dwID - looking for
returns
   DWORD - index, or -1 if cant find
*/
DWORD CMainWindow::VisImageFind (DWORD dwID)
{
   DWORD i;
   PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
   for (i = 0; i < m_lPCVisImage.Num(); i++, pvi++) {
      if ((*pvi)->m_dwID == dwID)
         return i;
   }

   return -1;
}


/*************************************************************************************
CMainWindow::VisImage - Given an ID, this returns a pointer to the VISIMAGE, or
NULL if it can't find

inputs
   DWORD          dwID - ID to find
returns
   PCVisImage - image
*/
PCVisImage CMainWindow::VisImage (DWORD dwID)
{
   DWORD dwIndex = VisImageFind (dwID);
   if (dwIndex == -1)
      return NULL;
   return *((PCVisImage*)m_lPCVisImage.Get(dwIndex));
}


/*************************************************************************************
CMainWindow::VisImageNew - Adds the image with a new version. This DOESN'T check
for an existing ID around

inputs
   PCMMLNode2         pNode - Node to use. This node is KEPT and deleted by the function
   PCMMLNode2         pNodeMenu - Node to use for the menu, <ContextMenu>.
                        This node is KEPT and deleted by the function
   PCMMLNode2        pNodeSliders - Node to use for the sliders, <Sliders>.
                        This node is KEPT and deleted by the object.
   PCMMLNode2        pNodeHypnoEffect - Node to use for the hypnoeffect, <HypnoEffect>.
                        This node is KEPT and deleted by the object.
   DWORD             dwID - ID to use
   HWND              hWnd - WIndow to create as a child of
   GUID              *pg - GUID ID
   BOOL              fIconic - TRUE if is iconic, FALSE if full window
   PWSTR             pszName - Name
   PWSTR             pszOther - Other description of object
   PWSTR             pszDescription - Description of object like, "This is an expensive looking ring."
   RECT              *prClient - DEfault client location
   BOOL              fCanChatTo - Set to TRUE if can chat to this NPC
   __int64           iTime - Time when created
returns
   PCVisImage - View added if OK, FALSE if error
*/
PCVisImage CMainWindow::VisImageNew (PCMMLNode2 pNode, PCMMLNode2 pNodeMenu, PCMMLNode2 pNodeSliders,
                                     PCMMLNode2 pNodeHypnoEffect, DWORD dwID, HWND hWnd,
                                     GUID *pg, BOOL fIconic, PWSTR pszName, PWSTR pszOther, PWSTR pszDescription,
                                     RECT *prClient, BOOL fCanChatTo, __int64 iTime)
{
   // create new one....
   PCVisImage pNew;
   pNew = new CVisImage;
   if (!pNew)
      return NULL;

   if (!pNew->Init (this, dwID, pg, hWnd, fIconic, fCanChatTo, iTime)) {
      delete pNew;
      return NULL;
   }
   if (!pNew->Update (pNode, NULL, pNodeMenu, pNodeSliders, pNodeHypnoEffect, pszName, pszOther, pszDescription, FALSE, fCanChatTo, iTime)) {
      delete pNew;
      return NULL;
   }

   // add this
   DWORD dwNum = m_lPCVisImage.Num();
   m_lPCVisImage.Add (&pNew);

   // no matter what, clear the current one
   pNew->RectSet (prClient, FALSE);

   // see if can update
   pNew->SeeIfRendered ();

   // see if in main display. If it is, then update the hypno effect
   //if (FindMainVisImage () == pNew)
   //   HypnoEffect (pNew->HypnoEffectGet());

   return pNew;
}



/*************************************************************************************
CMainWindow::VisImageUpdate - Updates the image with a new version. This DOESN'T check
for an existing ID around

inputs
   PCVisImage        pvi - Image
   PCMMLNode2         pNode - Node to use. This node is KEPT and deleted by the function
   PCMMLNode2         pNodeMenu - Node to use for the menu, <ContextMenu>.
                        This node is KEPT and deleted by the function
   PCMMLNode2        pNodeSliders - Node to use for the sliders, <Sliders>.
                        This node is KEPT and deleted by the object.
   PCMMLNode2        pNodeHypnoEffect - Node to use for the hypnoeffect, <HypnoEffect>.
                        This node is KEPT and deleted by the object.
   GUID              *pg - GUID ID. If NULL then wont set GUID
   PWSTR             pszName - Name
   PWSTR             pszOther - Other description of object
   PWSTR             pszDescription - Description of object like, "This is an expensive looking ring."
   BOOL              fCanChatTo - Cant chat to this NPC
   __int64           iTime - Time when updated
returns
   PCVisImage - View added if OK, FALSE if error
*/
PCVisImage CMainWindow::VisImageUpdate (PCVisImage pvi, PCMMLNode2 pNode, PCMMLNode2 pNodeMenu, PCMMLNode2 pNodeSliders,
                                        PCMMLNode2 pNodeHypnoEffect,
                                     GUID *pg, PWSTR pszName, PWSTR pszOther, PWSTR pszDescription, BOOL fCanChatTo,
                                     __int64 iTime)
{
   // modify the GUID?
   if (pg) {
      // if changing GUIDs then wipe out old time
      if (IsEqualGUID (pvi->m_gID, GUID_NULL) || IsEqualGUID(*pg, GUID_NULL))
         pvi->m_iNodeMenuTime = 0;
      if (!IsEqualGUID (pvi->m_gID, *pg))
         pvi->m_iNodeMenuTime = 0;

      pvi->m_gID = *pg;
   }

   if (!pvi->Update (pNode, NULL, pNodeMenu, pNodeSliders, pNodeHypnoEffect, pszName, pszOther, pszDescription, FALSE, fCanChatTo, iTime))
      return NULL;

   // see if can update
   pvi->SeeIfRendered ();

   // see if in main display. If it is, then update the hypno effect
   if (FindMainVisImage () == pvi)
      HypnoEffect (pvi->HypnoEffectGet());


   return pvi;
}


/*************************************************************************************
CMainWindow::VisImagePaintAll - Render all visimages within the given window
and rectangle and window. Used to paint.

inputs
   HWND           hWnd - Window drawing too
   HDC            hDC - DC drawing to
   RECT           *prClip - Clipping rect
returns
   none
*/
void CMainWindow::VisImagePaintAll (HWND hWnd, HDC hDC, RECT *prClip)
{
   DWORD i, j;
   PCVisImage *ppvi = (PCVisImage*)m_lPCVisImage.Get(0);
   PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
   for (i = 0; i < m_lPCVisImage.Num(); i++, ppvi++) {
      PCVisImage pvi = *ppvi;

      BOOL fTextUnderImages = FALSE;
      for (j = 0; j < m_lPCIconWindow.Num(); j++)
         if (ppi[j]->ContainsPCVisImage (pvi)) {
            fTextUnderImages = ppi[j]->m_fTextUnderImages;
            break;
         }

      pvi->Paint (hWnd, hDC, prClip, !pvi->m_fIconic, FALSE, fTextUnderImages, NULL, NULL);
   }
}



/*************************************************************************************
CMainWindow::Vis360Changed - Call this whenever a 360 degree settings (such as FOV,
etc.) changes. This will rebuild the display and invalidate the rect
*/
void CMainWindow::Vis360Changed (void)
{
   DWORD i;
   PCVisImage *ppvi = (PCVisImage*)m_lPCVisImage.Get(0);
   for (i = 0; i < m_lPCVisImage.Num(); i++, ppvi++)
      (*ppvi)->Vis360Changed ();

   // tell autio
   if (m_pAT)
      m_pAT->Vis360Changed (m_f360Long, m_f360Lat);

   // tell 3d renderer
   if (m_pRT)
      m_pRT->Vis360Changed (m_f360Long, m_f360Lat);

   // tell the server
   if (m_f360FOV != m_f360FOVSentToServer) {
      m_f360FOVSentToServer = m_f360FOV;
      InfoForServer (L"fov", NULL, m_f360FOV);
   }

}


/*************************************************************************************
CMainWindow::CommandClean - Cleans up the command, replacing <object> and <click>

inputs
   PWSTR          psz - Input
   GUID           *pgObject - Object. or NULL
   GUID           *pgClick - Click. or NULL
   BOOL           fDo - If TRUE then this a do action, else a show
   PCMem          pMem - Filled with the modified string
returns
   none
*/
void CMainWindow::CommandClean (PWSTR psz, GUID *pgObject, GUID *pgClick,
                                BOOL fDo, PCMem pMem)
{
   WCHAR szGUID[64];
   PWSTR pszObject;
   DWORD dwLen;
   GUID *pgUse;

   MemZero (pMem);
   MemCat (pMem, psz);

   DWORD dwClick, j;
   for (dwClick = 0; dwClick < 2; dwClick++) {
      pszObject = dwClick ? L"<click>" : L"<object>";
      dwLen = (DWORD)wcslen(pszObject);
      pgUse = dwClick ? pgClick : pgObject;

      if (!pgUse)
         continue;   // dont bother replacing

      szGUID[0] = L'|';
      MMLBinaryToString ((PBYTE) pgUse, sizeof(*pgUse), szGUID+1);
      PWSTR pszReplaceWith = szGUID;

      // if it's the user pass, need to get the name of the object... do this
      // by looking in the list of visviews
      if (!fDo) {
         PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
         for (j = 0; j < m_lPCVisImage.Num(); j++) {
            if (IsEqualGUID (pvi[j]->m_gID, *pgUse)) {
               pszReplaceWith = (PWSTR)pvi[j]->m_memName.p;
               break;
            }
         } // j
      }


      // find it and replace
      PWSTR pszTemp = (PWSTR)pMem->p;
      while (pszTemp && pszTemp[0]) {
         pszTemp = (PWSTR) MyStrIStr (pszTemp, pszObject);
         if (!pszTemp)
            break;

         // replace the reference to object
         DWORD dwLoc = (DWORD)((PBYTE)pszTemp - (PBYTE)pMem->p) / sizeof(WCHAR);
         LinkReplace (pMem, pszReplaceWith, dwLoc, dwLoc+dwLen-1);
         pszTemp = (PWSTR)pMem->p + dwLoc;
      }
   } // dwClick
}


/*************************************************************************************
CMainWindow::SendTextCommand - Sends a text command to the server.

inputs
   LANGID         lid - Language
   PWSTR          pszDo - Command to do
   PWSTR          pszShow - Command to show on the screen. If NULL, then use pszDo
   GUID           *pgObject - If not NULL, then any reference to "<object>" will
                  be converted to "|XXX", where XXX is the GUID.
   GUID           *pgClick - Only used if <click> is found in the command. If
                     pgClick has an object, then string is replaced with either
                     the object name (For pszShow) or object guid (for pszDo).
                     If !pgClick, then VerbSelect() is called instead.
   BOOL           fShowCommand - If TRUE then shows the command in the edit window
   BOOL           fShowTranscript - If TRUE then shows the command in the transcript, else
                  doesn't put in the transcript
   BOOL           fAutomute - If TRUE then automute works. Usually use TRUE
returns
   BOOL - TRUE if success
*/

BOOL CMainWindow::SendTextCommand (LANGID lid, PWSTR pszDo, PWSTR pszShow,
                                   GUID *pgObject, GUID *pgClick,
                                   BOOL fShowCommand, BOOL fShowTranscript, BOOL fAutomute)
{
   // BUGFIX - Trap commands starting with "@map:"
   PWSTR pszMap = L"@map:";
   size_t dwMapLen = wcslen(pszMap);
   size_t dwDoLen = wcslen(pszDo);
   size_t dwMapIndexEnd = dwMapLen + sizeof(GUID)*2;
   if (!_wcsnicmp(pszDo, pszMap, dwMapLen) && (dwDoLen >= dwMapIndexEnd) ) {
      GUID g;
      CHAR szTemp = pszDo[dwMapIndexEnd];
      pszDo[dwMapIndexEnd] = 0;
      size_t dwLenRet = MMLBinaryFromString (pszDo + dwMapLen, (PBYTE)&g, sizeof(g));
      pszDo[dwMapIndexEnd] = szTemp;

      if ( (dwLenRet == sizeof(g)) && RoomCenter (&g)) {
         // quickly re-enable hot-spots so can click quickly
         HotSpotEnable ();

         return TRUE;   // pulled up the map
      }

      // else, fail, so run the command after it
      pszDo += dwMapIndexEnd;
   }

   // remember the last language that used
   m_lidCommandLast = lid;

   // if no pszShow, then use pszDo
   if (!pszShow)
      pszShow = pszDo;

   // if this string has |012345435 (GUID) in it, then try to replace the guid
   // with the object's name, at least to show the user
   CMem memCleaned;
   MemZero (&memCleaned);
   PWSTR pszCur, pszLast;
   DWORD i, j;
   PWSTR pszReplaceWith;
   for (pszCur = pszLast = pszShow; *pszCur; pszCur++) {
      if (*pszCur != L'|')
         continue;   // not a guid

      DWORD dwChars = sizeof(GUID)*2;
      // else, might be, so verify
      for (i = 1; i <= dwChars; i++)
         if (!( ((pszCur[i] >= L'0') && (pszCur[i] <= L'9')) ||
            ((pszCur[i] >= L'a') && (pszCur[i] <= L'f')) ||
            ((pszCur[i] >= L'A') && (pszCur[i] <= L'F')) ))
            break;
      if (i <= dwChars)
         continue;   // not a guid
      
      // else it is
      WCHAR cTemp = pszCur[dwChars+1];
      GUID gID;
      pszCur[dwChars+1] = 0;
      MMLBinaryFromString (pszCur+1, (PBYTE)&gID, sizeof(gID));
      pszCur[dwChars+1] = cTemp;

      // find a match
      pszReplaceWith = NULL;
      PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
      for (j = 0; j < m_lPCVisImage.Num(); j++) {
         if (IsEqualGUID (pvi[j]->m_gID, gID)) {
            pszReplaceWith = (PWSTR)pvi[j]->m_memName.p;
            break;
         }
      } // j
      if (!pszReplaceWith)
         pszReplaceWith = L"OBJECT";   // didnt find, so make something up
         // BUGFIX - Used to be continue, but cant use since see the transcript

      // else, concatenate
      cTemp = pszCur[0];
      pszCur[0] = 0;
      MemCat (&memCleaned, pszLast);
      pszCur[0] = cTemp;
      MemCat (&memCleaned, pszReplaceWith);

      // move on
      pszCur += dwChars;   // so skip over
      pszLast = pszCur+1;
   } // pszCur
   // finish adding to pszCur
   MemCat (&memCleaned, pszLast);
   pszShow = (PWSTR)memCleaned.p;


   // replace
   CMem memShowUser, memSend;
   CommandClean (pszDo, pgObject, pgClick, TRUE, &memSend);
   CommandClean (pszShow, pgObject, pgClick, FALSE, &memShowUser);
   pszDo = (PWSTR)memSend.p;
   pszShow = (PWSTR)memShowUser.p;

   // if there's a <click> then make this a verb select
   if (MyStrIStr (pszDo, L"<click>")) {
      VerbSelect (pszShow, pszDo, lid);
      return TRUE;   // pretend success
   };

#if 0 // no longer used because replaced by command clean
   WCHAR szGUID[64];
   pszReplaceWith = szGUID;
   PWSTR pszObject = L"<object>";
   DWORD dwLen = (DWORD)wcslen(pszObject);
   if (pgObject) {
      szGUID[0] = L'|';
      MMLBinaryToString ((PBYTE) pgObject, sizeof(*pgObject), szGUID+1);
   }

   for (i = 0; i < 2; i++) {
      PCMem pMem = i ? &memShowUser : &memSend;
      MemZero (pMem);
      MemCat (pMem, i ? (PWSTR)memCleaned.p : psz);

      if (!pgObject)
         continue;   // dont bother replacing

      // if it's the user pass, need to get the name of the object... do this
      // by looking in the list of visviews
      if (i == 1) {
         PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
         for (j = 0; j < m_lPCVisImage.Num(); j++) {
            if (IsEqualGUID (pvi[j]->m_gID, *pgObject)) {
               pszReplaceWith = (PWSTR)pvi[j]->m_memName.p;
               break;
            }
         } // j
      }


      // find it and replace
      PWSTR pszTemp = (PWSTR)pMem->p;
      while (pszTemp && pszTemp[0]) {
         pszTemp = (PWSTR) MyStrIStr (pszTemp, pszObject);
         if (!pszTemp)
            break;

         // replace the reference to object
         DWORD dwLoc = (DWORD)((PBYTE)pszTemp - (PBYTE)pMem->p) / sizeof(WCHAR);
         LinkReplace (pMem, pszReplaceWith, dwLoc, dwLoc+dwLen-1);
         pszTemp = (PWSTR)pMem->p + dwLoc;
      }
   } // i
#endif // 0

   // update transcript so know that sent command
   GUID gActor = CLSID_PlayerAction;
   if (fShowTranscript) // BUGFIX - Dont show in transcript if not shown
      TranscriptString (0, gpszPlayerAction, &gActor, NULL, pszShow);

#ifndef UNIFIEDTRANSCRIPT
   // will need to set text to show that sent
   if (fShowCommand)
      m_pSlideTop->CommandTextSet (pszShow);
#endif


   // prepend language ID
   PWSTR psz = (PWSTR)memSend.p;
   DWORD dwLen = ((DWORD)wcslen(psz)+1)*sizeof(WCHAR);
   if (!memSend.Required (sizeof(LANGID)+dwLen))
      return FALSE;
   memmove ((PBYTE)memSend.p + sizeof(LANGID), memSend.p, dwLen);

   LANGID *plid = (LANGID*)memSend.p;
   *plid = lid;

   // since just sent command, set the timer to set
   m_fMenuTimeOut = 0;

   BOOL fRet = m_pIT->m_pPacket->PacketSend (CIRCUMREALITYPACKET_TEXTCOMMAND, memSend.p, sizeof(LANGID)+dwLen);
   if (!fRet) {
      int iErr;
      PCWSTR pszErr;
      iErr = m_pIT->m_pPacket->GetFirstError (&pszErr);
      PacketSendError(iErr, pszErr);
   }

   // remember the current time so know when last command was sent
   LARGE_INTEGER iTime;
   QueryPerformanceCounter (&iTime);
   m_iTimeLastCommand = *((__int64*) &iTime);

   // automute
   if (fAutomute && m_fTTSAutomute) {
      LARGE_INTEGER iNegativeOne;
      iNegativeOne.QuadPart = -1;
      m_pAT->TTSMuteTime (iNegativeOne, iNegativeOne);
   }
   return fRet;
}



/*************************************************************************************
CMainWindow::VisImageFindPtr - Finds the VisImage based upon its pointer.

inputs
   PCVisImage           pvi - Image looking for
returns
   DWORD - Index, or -1 if cant find
*/
DWORD CMainWindow::VisImageFindPtr (PCVisImage pvi)
{
   DWORD i;
   PCVisImage *ppvi = (PCVisImage*)m_lPCVisImage.Get(0);
   for (i = 0; i < m_lPCVisImage.Num(); i++, ppvi++)
      if ((*ppvi) == pvi)
         return i;

   return (DWORD)-1;
}



/*************************************************************************************
CMainWindow::ObjectDisplay - An object display message came in.

inputs
   PCMMLNode2            pNode - Node passed in. This is NOT deleted.
   PCIWGroup            pGroup - If not NULL then the object is added to this group.
                        If it is NULL then no object is added.
   PCDisplayWindow      pDispWin - If not NULL then the object is set in this window.
                        pDispWin->m_pvi must be NULL.
   BOOL                 fCanSetMainView - If TRUE, calls to objectdisplay can result
                        in the main display being created/set. if FALSE then can't.
                        Used to prevent recursion.
   BOOL                 fCanDelete - Will look for delete option and potentially delete
                        objects
   PWSTR                pszID - If the object's GUID ends up being NULL, AND pszID is
                        a valid string, then the object's ID will be hacked to be
                        based on pszID. That way if pass in NULL for an auxiliary
                        display window like combat, won't have all windows changing
   BOOL                 fCanChatTo - If TRUE then players can chat to this character
   __int64              iTime - Time when updated
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::ObjectDisplay (PCMMLNode2 pNode, PCIWGroup pGroup, PCDisplayWindow pDispWin,
                                 BOOL fCanSetMainView, BOOL fCanDelete, PWSTR pszID, BOOL fCanChatTo,
                                 __int64 iTime)
{
   // if get here have an object display... see if it already exists...
   GUID gID;
   memset (&gID, 0, sizeof(gID));
   MMLValueGetBinary (pNode, L"ID", (PBYTE)&gID, sizeof(gID));
   BOOL fMainView = fCanSetMainView ? (BOOL)MMLValueGetInt (pNode, L"MainView", 0) : FALSE;

   // BUGFIX: if this ID is null then might want to fix
   if (IsEqualGUID(gID, GUID_NULL) && pszID) {
      PVOID pFind = m_tObjectDisplayID.Find (pszID);
      if (!pFind) {
         m_tObjectDisplayID.Add (pszID, &pFind, 0);
         pFind = m_tObjectDisplayID.Find (pszID);
      }

      // just use the pointer as a guid
      *((PVOID*)&gID) = pFind;
   }

   DWORD i;
   if (fCanDelete && MMLValueGetInt (pNode, L"Delete", FALSE)) {
      // delete icon windows
      PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
      for (i = 0; i < m_lPCIconWindow.Num(); i++)
         ppi[i]->ObjectDelete (&gID);

      // delete displays
      for (i = m_lPCDisplayWindow.Num()-1; i < m_lPCDisplayWindow.Num(); i--) {
         PCDisplayWindow pd = *((PCDisplayWindow*)m_lPCDisplayWindow.Get(i));
         if (!pd->m_pvi)
            continue;
         if (IsEqualGUID (pd->m_pvi->m_gID, gID)) {
            delete pd;
            m_lPCDisplayWindow.Remove (i);
         }
      }

      return TRUE;
   }

   // get the name and other
   PWSTR pszName = MMLValueGet (pNode, L"Name");
   PWSTR pszOther = MMLValueGet (pNode, L"Other");
   PWSTR pszDescription = MMLValueGet (pNode, L"Description");
   if (!pszName)
      pszName = L"";
   if (!pszOther)
      pszOther = L"";
   if (!pszDescription)
      pszDescription = L"";

   // find the stuff to draw
   PCMMLNode2 pSub, pImage = NULL, pNodeMenu = NULL, pNodeSliders = NULL, pNodeHypnoEffect = NULL;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, CircumrealityImage()) ||
         !_wcsicmp(psz, Circumreality3DScene()) ||
         !_wcsicmp(psz, Circumreality3DObjects()) ||
         !_wcsicmp(psz, CircumrealityTitle()) ||
         !_wcsicmp(psz, CircumrealityText()) ||
         !_wcsicmp(psz, CircumrealityHelp())) {

            pImage = pSub;
            continue;
         }
      else if (!_wcsicmp(psz, L"ContextMenu")) {
         pNodeMenu = pSub;
         continue;
      }
      else if (!_wcsicmp(psz, CircumrealitySliders())) {
         pNodeSliders = pSub;
         continue;
      }
      else if (!_wcsicmp(psz, CircumrealityHypnoEffect())) {
         pNodeHypnoEffect = pSub;
         continue;
      }
   } // i
   if (!pImage)
      return FALSE;  // must have an image in it

   // find the main display
   PCDisplayWindow pMain = FindMainDisplayWindow();

   // look through all the CVisImage looking for a match. If find one then
   // update it
   PCVisImage *ppi = (PCVisImage*) m_lPCVisImage.Get(0);
   PCVisImage pCanClone = NULL;
   BOOL fSetMainAlready = FALSE;
   BOOL fRet = TRUE;
   for (i = 0; i < m_lPCVisImage.Num(); i++) {
      if (!IsEqualGUID(ppi[i]->m_gID, gID))
         continue;   // not the same

      // else, have match
      pCanClone = ppi[i];
      if (pMain && (ppi[i]->WindowGet() == pMain->m_hWnd))
         fSetMainAlready = TRUE;

      // BUGFIX - Changed from Init to update - to stop flickering of all objects
      //fRet &= ppi[i]->Init (this, pImage->Clone(),
      //   pNodeMenu ? pNodeMenu->Clone() : NULL,
      //   ppi[i]->m_dwID, &gID, ppi[i]->WindowGet(),
      //   ppi[i]->m_fIconic, pszName, pszOther);
      fRet &= ppi[i]->Update (pImage->Clone(),
         NULL,
         pNodeMenu ? pNodeMenu->Clone() : NULL,
         pNodeSliders ? pNodeSliders->Clone() : NULL,
         pNodeHypnoEffect ? pNodeHypnoEffect->Clone() : NULL,
         pszName, pszOther, pszDescription, FALSE, ppi[i]->m_fCanChatTo,
         iTime);

      // BUGFIX - Invalidate a large area in case the text below changed
      RECT r;
      ppi[i]->RectGet(&r);
      r.bottom += 48;
      InvalidateRect (ppi[i]->WindowGet(), &r, FALSE);

      // BUGFIX - update right away, so no flickering
      if (fRet)
         ppi[i]->SeeIfRendered ();


      // see if in main display. If it is, then update the hypno effect
      if (FindMainVisImage () == ppi[i])
         HypnoEffect (ppi[i]->HypnoEffectGet());


   } // i

   // add a new one?
   if (pGroup) {
      // temporary image size since will be fixed later
      RECT rClient;
      rClient.top = rClient.left = 0;
      rClient.right = rClient.bottom = 10;

      PCVisImage pvi = VisImageNew (pImage->Clone(),
         pNodeMenu ? pNodeMenu->Clone() : NULL,
         pNodeSliders ? pNodeSliders->Clone() : NULL,
         pNodeHypnoEffect ? pNodeHypnoEffect->Clone() : NULL,
         VIS_ICONWINDOW,
         pGroup->m_pIW->m_hWnd, &gID, TRUE, pszName, pszOther, pszDescription, &rClient, pGroup->m_fCanChatTo,
         iTime);
      if (pvi) {
         pGroup->Add (pvi);
         pCanClone = pvi;
      }
      else
         fRet = FALSE;
   }

   // add a new one?
   if (pDispWin /* BUGFIX && !pDispWin->m_pvi*/) {
      // temporary image size since will be fixed later
      RECT rClient;
      GetClientRect (pDispWin->m_hWnd, &rClient);

      // if this is the main window then note that it's already set
      if (pDispWin == pMain)
         fSetMainAlready = TRUE;

      // BUGFIX - if already have display information then just update that
      // this will allow a blend-in
      if (pDispWin->m_pvi) {
         VisImageUpdate (pDispWin->m_pvi, pImage->Clone(),
            pNodeMenu ? pNodeMenu->Clone() : NULL,
            pNodeSliders ? pNodeSliders->Clone() : NULL,
            pNodeHypnoEffect ? pNodeHypnoEffect->Clone() : NULL,
            &gID, pszName, pszOther, pszDescription, pDispWin->m_pvi->m_fCanChatTo,
            iTime);
         pCanClone = pDispWin->m_pvi;
      }
      else {
         // must create new vis image
         PCVisImage pvi = VisImageNew (pImage->Clone(),
            pNodeMenu ? pNodeMenu->Clone() : NULL,
            pNodeSliders ? pNodeSliders->Clone() : NULL,
            pNodeHypnoEffect ? pNodeHypnoEffect->Clone() : NULL,
            VIS_DISPLAYWINDOW,
            pDispWin->m_hWnd, &gID, FALSE, pszName, pszOther, pszDescription, &rClient, pGroup ? pGroup->m_fCanChatTo : fCanChatTo,
            iTime);
         if (pvi) {
            pDispWin->m_pvi = pvi;
            pCanClone = pvi;
         }
         else
            fRet = FALSE;
      }
   }

   // if flag is passed in to show this in the main view then do so
   // NOTE: will have used flags to make sure sure than dont recurse, setting fMainView to false
   if (fMainView && !fSetMainAlready) {
      if (pCanClone)
         SetMainWindow (pCanClone);
      else
         SetMainWindow (pImage, pNodeMenu, pNodeSliders, pNodeHypnoEffect, &gID, pszName, pszOther, pszDescription, pGroup ? pGroup->m_fCanChatTo : fCanChatTo, iTime);
   }

   return fRet;
}




/*************************************************************************************
CMainWindow::SetMainWindow - Given a CVisImage that's clicked on from an icon
window, this clones is and sets it as the main image.

inputs
   PCVisImage        pImage - From iconwindow

returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::SetMainWindow (PCVisImage pImage)
{
   return SetMainWindow (pImage->NodeGet(), pImage->MenuGet(), pImage->SlidersGet(),
      pImage->HypnoEffectGet(), &pImage->m_gID,
      (PWSTR)pImage->m_memName.p, (PWSTR)pImage->m_memOther.p, (PWSTR)pImage->m_memDescription.p, pImage->m_fCanChatTo,
      pImage->m_iNodeMenuTime);
}


/*************************************************************************************
CMainWindow::SetMainWindow - Sets the main window to the given image.

inputs
   PCMMLNode2         pNode - Node describing the image type.
   PCMMLNode2         pNodeMenu - Node describing the menu, or NULL
   PCMMLNode2        pNodeSliders - Node describing the sliders, or NULL
   PCMMLNode2        pNodeHypnoEffect - Node describing the hypnoeffect, or NULL
   GUID              *pgID - GUID
   PWSTR             pszName - Name
   PWSTR             pszOther - 2nd string
   PWSTR             pszDescription - Description of object like, "This is an expensive looking ring."
   BOOL              fCanChatTo - Set to TRUE if can chat to this NPC
   __int64           iTime - Time
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::SetMainWindow (PCMMLNode2 pNode, PCMMLNode2 pNodeMenu, PCMMLNode2 pNodeSliders,
                                 PCMMLNode2 pNodeHypnoEffect, GUID *pgID,
                                 PWSTR pszName, PWSTR pszOther, PWSTR pszDescription, BOOL fCanChatTo,
                                 __int64 iTime)
{
   // remember this, and send it to the server so can notify the object
   if (!IsEqualGUID(*pgID, GUID_NULL) && !IsEqualGUID(*pgID, m_gMainLast)) {
      m_gMainLast = *pgID;

      WCHAR szTemp[64];
      MMLBinaryToString ((PBYTE) pgID, sizeof(*pgID), szTemp);

      InfoForServer (L"focus", szTemp, 0);
   }

   // hypno effect
   HypnoEffect (pNodeHypnoEffect);

   // find the main window
   PCDisplayWindow pMain = FindMainDisplayWindow ();
   if (!pMain) {
      pMain = new CDisplayWindow;
      if (!pMain)
         return FALSE;
      if (!pMain->Init (this, L"Main", gpszMainDisplayWindow, NULL)) {
         delete pMain;
         return FALSE;
      }
      m_lPCDisplayWindow.Add (&pMain);
   }

   // remember last main window
   GUID gIDLast = GUID_NULL;
   if (pMain && pMain->m_pvi)
      gIDLast = pMain->m_pvi->m_gID;

   // delete what's there
   // BUGFIX - Dont do so that can merge in
   // pMain->DeletePCVisView ();

   RECT r;
   GetClientRect (pMain->m_hWnd, &r);

   if (pNodeMenu)
      pNodeMenu = pNodeMenu->Clone();
   if (pNodeSliders)
      pNodeSliders = pNodeSliders->Clone();
   if (pNodeHypnoEffect)
      pNodeHypnoEffect = pNodeHypnoEffect->Clone();
   // BUGFIX - If already visimage use that
   if (pMain->m_pvi)
      VisImageUpdate (pMain->m_pvi, pNode->Clone(), pNodeMenu, pNodeSliders, pNodeHypnoEffect,
         pgID, pszName, pszOther, pszDescription, fCanChatTo, iTime);
   else
      pMain->m_pvi = VisImageNew (pNode->Clone(), pNodeMenu, pNodeSliders, pNodeHypnoEffect,
         VIS_DISPLAYWINDOW, pMain->m_hWnd,
         pgID, FALSE, pszName, pszOther, pszDescription, &r, fCanChatTo, iTime);
   if (!pMain->m_pvi)
      return FALSE;


   // move the window to the top
   ChildShowWindow (pMain->m_hWnd, TW_DISPLAYWINDOW, (PWSTR) pMain->m_memID.p, SW_SHOW);

   // make sure this has the context menu
   if (m_pMenuContext != pMain->m_pvi) {
      m_pMenuContext = pMain->m_pvi;
      MenuReDraw();
   }

   // sliders should already be invalidated

   // loop through all the icon windows looking for "main" chat window and show/hide icons
   DWORD i;
   if (pMain && pMain->m_pvi && !IsEqualGUID(pMain->m_pvi->m_gID, gIDLast)) {
      PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
      for (i = 0; i < m_lPCIconWindow.Num(); i++)
         if (ppi[i]->m_fChatWindow && !_wcsicmp((PWSTR)ppi[i]->m_memID.p, L"main") )
            ppi[i]->PositionIcons (TRUE, FALSE);
   }

   // update the background
   PCImageStore pisMain = (pMain && pMain->m_pvi) ? pMain->m_pvi->LayerGetImage () : NULL;
   if (pisMain)
      BackgroundUpdate (BACKGROUND_IMAGES, pisMain, FALSE);
   return TRUE;
}


/*************************************************************************************
CMainWindow::ContextMenuDisplay - Displays a context menu and returns a pointer
to the context menu string that's selected.

inputs
   HWND              hWnd - Window to display off of
   PCListVariable    plMenuPre - Additional menu items at the top of the menu.
                     If this is a "-" then add a separator. Can be NULL
   PCListVariable    plMenuShow - List of strings to show
   PCListVariable    plMenuExtraText - Extra text appended to menu
   PCListVariable    plMenuDo - List of commands for do
returns
   PWSTR - String from plMenuDo or plMenuPre, or NULL if none selected
*/
PWSTR CMainWindow::ContextMenuDisplay (HWND hWnd, PCListVariable plMenuPre, PCListVariable plMenuShow,
                                       PCListVariable plMenuExtraText, PCListVariable plMenuDo)
{
   BeepWindowBeep (ESCBEEP_MENUOPEN);

   HMENU hMenu = CreatePopupMenu ();
   CMem memAnsi;
   char *psza;
   DWORD i, dwLen;

   // pre
   if (plMenuPre) for (i = 0; i < plMenuPre->Num(); i++) {
      dwLen = (DWORD)plMenuPre->Size (i)+2;
      if (!memAnsi.Required (dwLen))
         continue;
      psza = (char*)memAnsi.p;
      WideCharToMultiByte (CP_ACP, 0, (PWSTR)plMenuPre->Get(i), -1,
         psza, (DWORD)memAnsi.m_dwAllocated, 0, 0);

      if ((psza[0] == '-') && !psza[1])
         AppendMenu (hMenu, MF_SEPARATOR, 0, NULL);
      else
         AppendMenu (hMenu, MF_ENABLED, i + 100, psza);
   } // i

   // show
   for (i = 0; i < plMenuShow->Num(); i++) {
      dwLen = (DWORD)plMenuShow->Size (i) + (DWORD)plMenuExtraText->Size(i) + 8;
      if (!memAnsi.Required (dwLen))
         continue;
      psza = (char*)memAnsi.p;
      WideCharToMultiByte (CP_ACP, 0, (PWSTR)plMenuShow->Get(i), -1,
         psza, (DWORD)memAnsi.m_dwAllocated, 0, 0);

      // extra
      PWSTR pszExtra = (PWSTR)plMenuExtraText->Get(i);
      if (pszExtra && pszExtra[0]) {
         strcat (psza, " - ");
         WideCharToMultiByte (CP_ACP, 0, pszExtra, -1,
            psza + (DWORD)strlen(psza), (DWORD)(memAnsi.m_dwAllocated - strlen(psza)), 0, 0);
      }
         
      AppendMenu (hMenu, MF_ENABLED, i + 1000, psza);
   }  // over i


   // show the menu
   POINT p;
   GetCursorPos (&p);
   int iRet;
   iRet = TrackPopupMenu (hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
      p.x, p.y, 0, hWnd, NULL);
   DestroyMenu (hMenu); // BUGFIX - added

   if (iRet < 100)
      return NULL;   // didn't click on anything
   if (iRet < 1000)
      return (PWSTR) plMenuPre->Get((DWORD)iRet-100);

   // else did...
   return (PWSTR) plMenuDo->Get((DWORD)iRet-1000);
}



/*************************************************************************************
CMainWindow::ContextMenuDisplay - Displays a context menu for a specific PCVisImage

This also adds sub-menus for various chat bits.

inputs
   HWND              hWnd - Window to display off of
   PCVisImage        pView - View
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::ContextMenuDisplay (HWND hWnd, PCVisImage pView)
{
   // find the icon window
   DWORD i;
   PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
   PCIconWindow pi = NULL;
   for (i = 0; i < m_lPCIconWindow.Num(); i++)
      if (ppi[i]->ContainsPCVisImage (pView)) {
         pi = ppi[i];
         break;
      }


   // BUGFIX - Don't do this anymore because context menus don't show tooltip
   // if have verb active then handle that
   //if (VerbClickOnObject (&pView->m_gID))
   //   return TRUE;

   // if the hot spots still disabled then beep
   if (m_fMessageDiabled || m_fMenuExclusive) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return FALSE;
   }

   // else, clicked on menu. Handle that
   PCMMLNode2 pMenu = pView->MenuGet();
   CListVariable lShow, lDo, lExtraText, lPre;
   CListFixed lAction;
   lAction.Init (sizeof(DWORD));
   LANGID lid = DEFLANGID;
   if (pMenu)
      MMLFromContextMenu (pMenu, &lShow, &lExtraText, &lDo, &lid, NULL);

   // make up pre menu
   DWORD dwAdd;
   PWSTR psz;
   
   // option for enlarge
   psz = L"Look at this (enlarge)";
   dwAdd = 100;
   lPre.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   lAction.Add (&dwAdd);

   if (pi && pView->m_fCanChatTo /*pi->m_fChatWindow*/) {
      CMem memTemp;

      // separator
      dwAdd = 0;
      psz = L"-";
      lPre.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      lAction.Add (&dwAdd);

      // talk to this NPC specifically
      dwAdd = 200;
      MemZero (&memTemp);
      MemCat (&memTemp, L"Talk to ");
      MemCat (&memTemp, (PWSTR)pView->m_memName.p);
      psz = (PWSTR)memTemp.p;
      lPre.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      lAction.Add (&dwAdd);

      // whisper to this NPC
      dwAdd = 201;
      MemZero (&memTemp);
      MemCat (&memTemp, L"Whisper to ");
      MemCat (&memTemp, (PWSTR)pView->m_memName.p);
      psz = (PWSTR)memTemp.p;
      lPre.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      lAction.Add (&dwAdd);

      // separator
      dwAdd = 0;
      psz = L"-";
      lPre.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      lAction.Add (&dwAdd);

      // talk to all NPCs
      dwAdd = 202;
      psz = L"Talk (to everyone)";
      lPre.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      lAction.Add (&dwAdd);

      // yell to everyone
      dwAdd = 203;
      psz = L"Yell (to everyone)";
      lPre.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      lAction.Add (&dwAdd);
   }

   // final seperator
   if (lShow.Num()) {
      dwAdd = 0;
      psz = L"-";
      lPre.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      lAction.Add (&dwAdd);
   }

   psz = ContextMenuDisplay (hWnd, &lPre, &lShow, &lExtraText, &lDo);
   if (!psz)
      return FALSE;

   // play a click
   BeepWindowBeep (ESCBEEP_LINKCLICK);

   // see if matches any lPre
   for (i = 0; i < lPre.Num(); i++)
      if (psz == lPre.Get(i)) switch (*((DWORD*)lAction.Get(i))) {
         case 100:   // enlarge
            SetMainWindow (pView);
            return 0;
         case 200:   // talk to this
            if (pi)
               pi->ChatToSet (0, &pView->m_gID);
            return 0;
         case 201:   // whisper to this
            if (pi)
               pi->ChatToSet (1, &pView->m_gID);
            return 0;
         case 202:   // talk to all
            if (pi)
               pi->ChatToSet (0, NULL);
            return 0;
         case 203:   // yell to all
            if (pi)
               pi->ChatToSet (-1, NULL);
            return 0;
         default:
            return 0;
      } // switch

   // else, send text command
   SendTextCommand (lid, psz, NULL, &pView->m_gID, NULL, TRUE, TRUE, TRUE);
   HotSpotDisable();

   return TRUE;
}


/*************************************************************************************
CMainWindow::UserSave - Saves the node to the user file

inputs
   BOOL              fAccount - If TRUE then this is account specific, FALSE its user specific
   PWSTR             pszName - Name of file to save
   PCMMLNode2        pNode - Information to save
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::UserSave (BOOL fAccount, PWSTR pszName, PCMMLNode2 pNode)
{
   PCMMLNode2 pNodeMisc;
   PCPasswordFile pPF = NULL;
   PCPasswordFileAccount pPFA = NULL;
   if (fAccount) {
      pPFA = PasswordFileAccountCache();
      if (!pPFA)
         return FALSE;
      pNodeMisc = pPFA->m_pMMLNodeMisc;
   }
   else {
      pPF = PasswordFileCache (FALSE);
      if (!pPF)
         return FALSE;
      pNodeMisc = pPF->m_pMMLNodeMisc;
   }

   DWORD dwIndex = pNodeMisc->ContentFind (pszName);
   if (dwIndex != (DWORD)-1)
      pNodeMisc->ContentRemove (dwIndex);

   PCMMLNode2 pClone = pNode->Clone();
   if (pClone) {
      pClone->NameSet (pszName);
      pNodeMisc->ContentAdd (pClone);
   }


   // note that it's dirty
   if (pPF)
      pPF->Dirty();
   else
      pPFA->Dirty();
   PasswordFileRelease();

   return TRUE;


#if 0 // old code
   CMem mem;
   if (!MMLToMem (pNode, &mem, TRUE))
      return FALSE;
   mem.CharCat (0);  // just to null terminate

   // prefix logon to preferences
   WCHAR szTemp[256];
   wcscpy (szTemp, m_szUserName);
   wcscat (szTemp, L"\\");
   wcscat (szTemp, pszName);

   return m_mfUser.Save (szTemp, mem.p, mem.m_dwCurPosn);
#endif
}


/*************************************************************************************
CMainWindow::UserLoad - Loads the node to the user file

inputs
   BOOL              fAccount - If TRUE then this is account specific, FALSE its user specific
   PWSTR             pszName - Name of file to save
returns
   PCMMLNode2 - New node. Must be freed by caller. NULL if couldn't load
*/
PCMMLNode2 CMainWindow::UserLoad (BOOL fAccount, PWSTR pszName)
{
   PCMMLNode2 pNodeMisc;
   PCPasswordFile pPF = NULL;
   PCPasswordFileAccount pPFA = NULL;
   if (fAccount) {
      pPFA = PasswordFileAccountCache();
      if (!pPFA)
         return FALSE;
      pNodeMisc = pPFA->m_pMMLNodeMisc;
   }
   else {
      pPF = PasswordFileCache (FALSE);
      if (!pPF)
         return FALSE;
      pNodeMisc = pPF->m_pMMLNodeMisc;
   }

   PCMMLNode2 pSub = NULL;
   PWSTR psz = NULL;
   pNodeMisc->ContentEnum (pNodeMisc->ContentFind(pszName), &psz, &pSub);
   if (!pSub) {
      PasswordFileRelease();
      return NULL;
   }

   PCMMLNode2 pClone = pSub->Clone();
   PasswordFileRelease();
   return pClone;

#if 0 // old code
   // prefix logon to preferences
   WCHAR szTemp[256];
   wcscpy (szTemp, m_szUserName);
   wcscat (szTemp, L"\\");
   wcscat (szTemp, pszName);

   PWSTR psz;
   __int64 iSize;
   psz = (PWSTR) m_mfUser.Load (szTemp, &iSize);
   if (!psz)
      return FALSE;

   PCMMLNode2 pNode = MMLFromMem (psz);
   MegaFileFree (psz);  // free up

   return pNode;
#endif // 0

}



/*************************************************************************************
CMainWindow::FindMainDisplayWindow - Finds the display window with ID == "Main"

returns
   PCDisplayWindow - Main display window
*/
PCDisplayWindow CMainWindow::FindMainDisplayWindow (void)
{
   DWORD i;
   PCDisplayWindow *ppd = (PCDisplayWindow*)m_lPCDisplayWindow.Get(0);
   for (i = 0; i < m_lPCDisplayWindow.Num(); i++)
      if (!_wcsicmp((PWSTR)ppd[i]->m_memID.p, gpszMainDisplayWindow))
         return ppd[i];

   return NULL;
}




/*************************************************************************************
CMainWindow::FindMainVisImage - Finds the display window with ID == "Main"

returns
   PCVisImage - Main display window
*/
PCVisImage CMainWindow::FindMainVisImage (void)
{
   PCDisplayWindow pdwMain = FindMainDisplayWindow ();
   return pdwMain ? pdwMain->m_pvi : NULL;
}



#ifndef UNIFIEDTRANSCRIPT
/*************************************************************************
MenuPage
*/
BOOL MenuPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMainWindow pvi = (PCMainWindow)pPage->m_pUserData;   // node to modify
   static DWORD dwTimerID = 0;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // start the timer?
         if (pvi->m_fMenuTimeOut && pvi->m_fMenuTimeOutOrig)
            dwTimerID = pPage->m_pWindow->TimerSet (100, pPage);

         // update the progress control
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCM_TIMER:
      {
         PESCMTIMER p =(PESCMTIMER) pParam;
         if (p && (p->dwID == dwTimerID)) {
            if (pvi->m_fMenuTimeOut > 0) {
               pvi->m_fMenuTimeOut -= 0.1;   // since timers 100 ms
               pvi->m_fMenuTimeOut = max(0, pvi->m_fMenuTimeOut);

               if (pvi->m_fMenuTimeOut <= 0) {
                  pPage->m_pWindow->TimerKill (dwTimerID);
                  dwTimerID = 0;
                  pvi->MenuLinkDefault ();
               }
            }
            else {
               // must have clicked, so kill the timer anyway
               pPage->m_pWindow->TimerKill (dwTimerID);
               dwTimerID = 0;
            }

            // update the progress-bar
            pPage->Message (ESCM_USER+82);
         }
      }
      break;

   case ESCM_USER+82:   // update the progress bar
      {
         PCEscControl pControl = pPage->ControlFind (L"Progress");
         pvi->m_fMenuTimeOut = min(pvi->m_fMenuTimeOut, pvi->m_fMenuTimeOutOrig);
         pvi->m_fMenuTimeOut = max(pvi->m_fMenuTimeOut, 0);
         fp fProgress = pvi->m_fMenuTimeOutOrig - pvi->m_fMenuTimeOut;
         if (pvi->m_fMenuTimeOutOrig)
            fProgress /= pvi->m_fMenuTimeOutOrig;
         else
            fProgress = 0;
         fProgress *= 1000;

         if (pControl)
            pControl->AttribSetInt (L"pos", (int)fProgress);
      }
      return TRUE;

   case ESCM_DESTRUCTOR:
      if (dwTimerID) {
         pPage->m_pWindow->TimerKill (dwTimerID);
         dwTimerID = 0;
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;
         pvi->MenuLink (p->psz);
      }
      return TRUE;

   }

   return FALSE;
}
#endif // !UNIFIEDTRANSCRIPT

#define USELINKSFORTRANSCRIPT

#ifdef UNIFIEDTRANSCRIPT
/*************************************************************************************
CMainWindow::MenuMML - Returns the MML for the menu.

inputs
   PCMem          pMem - Cleared out and filled with the menu, assuming it's
                  in its own sub-table. This might be filled in with an empty string
                  if there's no menu
   DWORD          dwColumns - Number of columns, 1+
returns
   none
*/
void CMainWindow::MenuMML (PCMem pMem, DWORD dwColumns)
{
   MemZero (pMem);

   // if it's exclusive and there's no main menu then just hide the window and be done
   // with it
   if (m_fMenuExclusive && !m_lMenuShow.Num())
      return;  // nothing
   
   // get the menu for the visview
   CListVariable lMenuContextShow, lMenuContextExtraText, lMenuContextDo;
   PCMMLNode2 pMenu = m_pMenuContext ? m_pMenuContext->MenuGet() : NULL;
   m_lidMenuContext = DEFLANGID;
   if (pMenu)
      MMLFromContextMenu (pMenu, &lMenuContextShow, &lMenuContextExtraText, &lMenuContextDo, &m_lidMenuContext, NULL);

   // if there are no menus to show then hide menu and be done with it
   if (!m_lMenuShow.Num() && !lMenuContextShow.Num())
      return; // nothing

   // BUGFIX - Made the menu on a dark background
   // create the text to display
   DWORD i, j;
   for (i = 0; i < (DWORD)(m_fMenuExclusive ? 1 : 2); i++) {
      PCListVariable plShow = i ? &lMenuContextShow : &m_lMenuShow;
      PCListVariable plExtraText = i ? &lMenuContextExtraText : &m_lMenuExtraText;
      PCListVariable plDo = i ? &lMenuContextDo : &m_lMenuDo;
      LANGID lid = i ? m_lidMenuContext : m_lidMenu;
      if (!plShow->Num())
         continue;

      // add a table
      if (i && m_lMenuShow.Num())
         MemCat (pMem, L"<p/>");

#ifdef USELINKSFORTRANSCRIPT
      MemCat (pMem, L"<table width=100% border=0 innerlines=0 tbmargin=2>");
#else
      MemCat (pMem, L"<table width=100% border=0 innerlines=0>");
#endif

      // if context menu and had general menu then break
      if (i && m_lMenuShow.Num()) {
         MemCat (pMem, L"<tr><td><bold>");
         MemCatSanitize (pMem, (PWSTR)m_pMenuContext->m_memName.p);
         MemCat (pMem, L"</bold></td></tr>");
      }

      // elements
      DWORD dwRows = (plShow->Num() + dwColumns - 1) / dwColumns;

      DWORD dwIndex;
      for (dwIndex = 0; dwIndex < dwRows * dwColumns; dwIndex++) {
         j = (dwIndex % dwColumns) * dwRows + (dwIndex / dwColumns);
         // start
         if (!(dwIndex % dwColumns))
            MemCat (pMem, L"<tr>");
         MemCat (pMem, L"<td>");

         PWSTR pszShow = (PWSTR)plShow->Get(j);
         PWSTR pszExtraText = (PWSTR)plExtraText->Get(j);
         PWSTR pszDo = (PWSTR)plDo->Get(j);
         if (!pszShow || !pszDo)
            goto doneelem;   // shouldnt happen, but might

#ifdef USELINKSFORTRANSCRIPT
         if (!i && (j == m_dwMenuDefault))
            MemCat (pMem, L"<big>");
         MemCat (pMem, L"<a ");
#else
         MemCat (pMem, L"<button style=righttriangle buttondepth=8 buttonheight=16 buttonwidth=16 width=99% ");

         if (pszExtraText && pszExtraText[0])
            MemCat (pMem, L"valign=top ");
         else
            MemCat (pMem, L"valign=center ");

         if (!i && (j == m_dwMenuDefault))
            MemCat (pMem, L"style=rightarrow ");
         //else
         //   MemCat (pMem, L"style=righttriangle ");
#endif

         if (m_fLightBackground) {
            if (m_fMenuExclusive)
               MemCat (pMem, L"color=#ff00ff ");
            else
               MemCat (pMem, L"color=#0000ff ");
         }
         else {
            if (m_fMenuExclusive)
               MemCat (pMem, L"color=#ffc0ff "); // BUGFIX - Made lighter. Was ff80ff
            else
               MemCat (pMem, L"color=#c0c0ff "); // BUGFIX - Made lighter. Was 8080ff
         }
         MemCat (pMem, L"href=\"ml:");
         MemCat (pMem, (int)lid);
         MemCat (pMem, L":");
         MemCatSanitize (pMem, pszDo);
         MemCat (pMem, L"\"><bold>");
         MemCatSanitize (pMem, pszShow);

         MemCat (pMem, L"</bold>");
#ifdef USELINKSFORTRANSCRIPT
         MemCat (pMem, L"</a>");
         if (!i && (j == m_dwMenuDefault))
            MemCat (pMem, L"</big>");
#endif
         if (pszExtraText && pszExtraText[0]) {
#ifdef USELINKSFORTRANSCRIPT
            MemCat (pMem, L"<small> - ");
#else
            MemCat (pMem, L"<br/><small>");
#endif
            MemCatSanitize (pMem, pszExtraText);
            MemCat (pMem, L"</small>");
         }

#ifndef USELINKSFORTRANSCRIPT
         MemCat (pMem, L"</button>");
#endif

doneelem:
         MemCat (pMem, L"</td>");
         if (!((dwIndex+1) % dwColumns))
            MemCat (pMem, L"</tr>");

      } // j

      // end the table
      MemCat (pMem, L"</table>");

      // if there's a time-out then display the progress bar
      if (!i && m_fMenuTimeOutOrig)
         MemCat (pMem, L"<p><ProgressBar width=100% min=0 max=1000 orient=horz pos=0 name=progress/></p>");

   } // i

}
#endif // UNIFIEDTRANSCRIPT

/*************************************************************************************
CMainWindow::MenuReDraw - Recalculates the menu and draws it.
*/
void CMainWindow::MenuReDraw (void)
{
#ifdef UNIFIEDTRANSCRIPT
   // BUGFIX - disable m_fMenuTimeOut = m_fMenuTimeOutOrig = 0;

   // if it's exclusive and there's no main menu then just hide the window and be done
   // with it
   if (m_fMenuExclusive && !m_lMenuShow.Num())
      m_fMenuTimeOut = m_fMenuTimeOutOrig = 0;

   // since it's in the transcript window, update that
   TranscriptUpdate ();
#else // UNIFIEDTRANSCRIPT
   if (!m_hWndMenu) {
      m_fMenuTimeOut = m_fMenuTimeOutOrig = 0;
      return;
   }

   // if have existing window then make sure to stop the page
   if (m_pMenuWindow)
      m_pMenuWindow->PageClose ();
   else {
      // create the new page
      m_pMenuWindow = new CEscWindow;
      if (!m_pMenuWindow) {
         m_fMenuTimeOut = m_fMenuTimeOutOrig = 0;
         return;  // error
      }

      RECT rClient;
      GetClientRect (m_hWndMenu, &rClient);
      if (!m_pMenuWindow->Init (ghInstance, m_hWndMenu,
         EWS_NOTITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_FIXEDHEIGHT | EWS_FIXEDWIDTH | EWS_CHILDWINDOW | EWS_CHILDWINDOWNOBORDER,
         &rClient)) {
            delete m_pMenuWindow;
            m_pMenuWindow = NULL;
            m_fMenuTimeOut = m_fMenuTimeOutOrig = 0;
            return;
         }
   }
   
   // if it's exclusive and there's no main menu then just hide the window and be done
   // with it
   if (m_fMenuExclusive && !m_lMenuShow.Num()) {
      m_fMenuTimeOut = m_fMenuTimeOutOrig = 0;
      ChildShowWindow (m_hWndMenu, TW_MENU, NULL, SW_HIDE);
      return;
   }

   // get the menu for the visview
   CListVariable lMenuContextShow, lMenuContextExtraText, lMenuContextDo;
   PCMMLNode2 pMenu = m_pMenuContext ? m_pMenuContext->MenuGet() : NULL;
   m_lidMenuContext = DEFLANGID;
   if (pMenu)
      MMLFromContextMenu (pMenu, &lMenuContextShow, &lMenuContextExtraText, &lMenuContextDo, &m_lidMenuContext, NULL);

   // if there are no menus to show then hide menu and be done with it
   if (!m_lMenuShow.Num() && !lMenuContextShow.Num()) {
      m_fMenuTimeOut = m_fMenuTimeOutOrig = 0;
      ChildShowWindow (m_hWndMenu, TW_MENU, NULL, SW_HIDE);
      return;
   }

   // BUGFIX - Made the menu on a dark background
   // create the text to display
   CMem mem;
   DWORD i, j;
   MemZero(&mem);
   MemCat (&mem,
      L"<colorblend posn=background tcolor=#303030 bcolor=#202020/><font color=#ffffff>"
      L"<small>");
   MemCat (&mem,
      L"<align tab=16 parlinespacing=50%>");
   
   for (i = 0; i < (DWORD)(m_fMenuExclusive ? 1 : 2); i++) {
      PCListVariable plShow = i ? &lMenuContextShow : &m_lMenuShow;
      PCListVariable plExtraText = i ? &lMenuContextExtraText : &m_lMenuExtraText;
      PCListVariable plDo = i ? &lMenuContextDo : &m_lMenuDo;
      LANGID lid = i ? m_lidMenuContext : m_lidMenu;
      if (!plShow->Num())
         continue;

      // if context menu and had general menu then break
      if (i && m_lMenuShow.Num()) {
         MemCat (&mem, L"<p/>");
         MemCat (&mem, L"<p><bold>");
         MemCatSanitize (&mem, (PWSTR)m_pMenuContext->m_memName.p);
         MemCat (&mem, L"</bold></p>");
      }

#ifdef USELIST
      MemCat (&mem, L"<ul type=pointer>");
#endif

      for (j = 0; j < plShow->Num(); j++) {
         PWSTR pszShow = (PWSTR)plShow->Get(j);
         PWSTR pszExtraText = (PWSTR)plExtraText->Get(j);
         PWSTR pszDo = (PWSTR)plDo->Get(j);
         if (!pszShow || !pszDo)
            continue;   // shouldnt happen, but might

#ifdef USELIST
         MemCat (&mem, L"<li>");
         if (!i && (j == m_dwMenuDefault))
            MemCat (&mem, L"<bold>");
         MemCat (&mem, L"<a ");
#else
         MemCat (&mem, L"<button style=righttriangle buttondepth=8 buttonheight=16 buttonwidth=16 basecolor=#c0c040 width=99% ");

         if (pszExtraText && pszExtraText[0])
            MemCat (&mem, L"valign=top ");
         else
            MemCat (&mem, L"valign=center ");

         if (!i && (j == m_dwMenuDefault))
            MemCat (&mem, L"style=rightarrow ");
         //else
         //   MemCat (&mem, L"style=righttriangle ");
#endif
         if (m_fMenuExclusive)
            MemCat (&mem, L"color=#ff80ff ");
         else
            MemCat (&mem, L"color=#8080ff ");
         MemCat (&mem, L"href=\"");
         MemCat (&mem, (int)lid);
         MemCat (&mem, L":");
         MemCatSanitize (&mem, pszDo);
         MemCat (&mem, L"\"><bold>");
         MemCatSanitize (&mem, pszShow);
#ifdef USELIST
         MemCat (&mem, L"</a>");

         if (!i && (j == m_dwMenuDefault))
            MemCat (&mem, L"</bold>");
         if (pszExtraText && pszExtraText[0]) {
            MemCat (&mem, L" - ");
            MemCatSanitize (&mem, pszExtraText);
         }
         MemCat (&mem, L"</li>");
#else
         MemCat (&mem, L"</bold>");
         if (pszExtraText && pszExtraText[0]) {
            MemCat (&mem, L"<br/>");
            MemCatSanitize (&mem, pszExtraText);
         }
         MemCat (&mem, L"</button><br/>");
#endif
      } // j

#ifdef USELIST
      MemCat (&mem, L"</ul>");
#endif

      // if there's a time-out then display the progress bar
      if (!i && m_fMenuTimeOutOrig)
         MemCat (&mem, L"<p><ProgressBar width=100% min=0 max=1000 orient=horz pos=0 name=progress/></p>");

   } // i

   MemCat (&mem, 
      L"</align>");
   MemCat (&mem,
      L"</small>"
      L"</font>"
      );
   m_pMenuWindow->PageDisplay (ghInstance, (PWSTR)mem.p, MenuPage, this);

   // show this window
   ChildShowWindow (m_hWndMenu, TW_MENU, NULL, SW_SHOWNA);
   m_fMenuPosSet = TRUE;   // since showed it
   // NOTE: Specifically not moving it to the top of the Z order
#endif
}


/*************************************************************************************
CMainWindow::MenuLink - Called when a link is pressed in the page

inputs
   PWSTR       pszLink - Link
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::MenuLink (PWSTR pszLink)
{
   if (m_fMessageDiabled) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return FALSE;
   }

   // find the language ID
   LANGID lid = _wtoi(pszLink);
   pszLink = wcschr(pszLink, L':');
   if (!pszLink)
      return FALSE;
   pszLink++;

   // send out command
   SendTextCommand (lid, pszLink, NULL, m_pMenuContext ? &m_pMenuContext->m_gID : NULL, NULL, TRUE, TRUE, TRUE);
   HotSpotDisable ();
   BeepWindowBeep (ESCBEEP_LINKCLICK);

   return TRUE;
}

/*************************************************************************************
CMainWindow::MenuLinkDefault - Called when a default link is triggered
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::MenuLinkDefault (void)
{
   PWSTR pszDo = (PWSTR) m_lMenuDo.Get (m_dwMenuDefault);

   // send out command
   if (pszDo) {
      SendTextCommand (m_lidMenu, pszDo, NULL, NULL, NULL, TRUE, TRUE, FALSE);
      HotSpotDisable ();
      BeepWindowBeep (ESCBEEP_LINKCLICK);
   }

   return TRUE;
}



/*************************************************************************************
CMainWindow::VerbWindowShow - Shows or hides the verb window.
*/
void CMainWindow::VerbWindowShow (void)
{
   m_pSlideTop->ToolbarShow (!m_fVerbHiddenByServer && !m_fMenuExclusive);

   if (m_fVerbHiddenByServer || m_fMenuExclusive) {
      // deselect the old one
      if (!m_fVerbSelectedIcon)
         VerbDeselect();
      VerbTooltipUpdate();
      HotSpotTooltipUpdate();
   }
}


/*************************************************************************************
CMainWindow::VerbTooltipUpdate - Updates the tooltip for the verb

inputs
   DWORD       dwMonitor - 0 for primary, 1 for secondary
   BOOL        fShow - If TRUE then show the tooltip, else hide it
   int         x - Cursor x (window coords), only need if fShow
   int         y - Cursor y (window coords), only need if fShow

returns
   BOOL - TRUE if a tooltip was drawn
*/
BOOL CMainWindow::VerbTooltipUpdate (DWORD dwMonitor, BOOL fShow, int iX, int iY)
{
   // if dont want to show make sure it's gone
   if (!fShow) {
      if (m_pVerbToolTip) {
         delete m_pVerbToolTip;
         m_pVerbToolTip = NULL;
      }
      return FALSE;
   }

   // if have a tooltip, but is wrong monitor, then delete
   HWND hWndMain = dwMonitor ? m_hWndSecond : m_hWndPrimary;
   if (m_pVerbToolTip && (GetParent(m_pVerbToolTip->m_hWnd) != hWndMain)) {
      delete m_pVerbToolTip;
      m_pVerbToolTip = NULL;
   }

   // else want to show
   if (!m_pVerbToolTip) {
      RECT r;
      CMem memW, memA;
      r.left = r.right = iX;
      r.top = r.bottom = iY;

      // convert the string
      PWSTR psz = (PWSTR)m_memVerbShow.p;
      if (!psz || !psz[0])
         return FALSE;
      MemZero (&memW);
      MemCat (&memW, L"<bold>");
      MemCatSanitize (&memW, psz);
      MemCat (&memW, L"</bold>");
      if (!memA.Required((wcslen((PWSTR)memW.p)+1)*sizeof(WCHAR)))
         return FALSE;
      WideCharToMultiByte (CP_ACP, 0, (PWSTR)memW.p, -1, (char*)memA.p, (DWORD)memA.m_dwAllocated, 0, 0);

      m_pVerbToolTip = new CToolTip;
      if (!m_pVerbToolTip)
         return FALSE;


      if (!m_pVerbToolTip->Init ((char*)memA.p, 0, &r, hWndMain)) {
         delete m_pVerbToolTip;
         m_pVerbToolTip = NULL;
         return FALSE;
      }
   }
   
   // want to be down and to the right a bit
   iX += 24;
   iY += 24;

   // move the window
   RECT r;
   GetWindowRect (m_pVerbToolTip->m_hWnd, &r);
   if ((r.top == iY) && (r.left == iX))
      return TRUE;  // no change

   // else move it
   MoveWindow (m_pVerbToolTip->m_hWnd, iX, iY, r.right-r.left, r.bottom-r.top, TRUE);

   return TRUE;
}



/*************************************************************************************
CMainWindow::HotSpotTooltipUpdate - Updates the tooltip for the hot spot

inputs
   PCVisImage  pvi - Image
   DWORD       dwMonitor - 0 for primary, 1 for secondary
   BOOL        fShow - If TRUE then show the tooltip, else hide it
   int         x - Cursor x (window coords), only need if fShow
   int         y - Cursor y (window coords), only need if fShow
*/
void CMainWindow::HotSpotTooltipUpdate (PCVisImage pvi, DWORD dwMonitor, BOOL fShow, int iX, int iY)
{
#if 0 // def _DEBUG
   WCHAR szTemp[64];
   swprintf (szTemp, L"\r\nHotSpot = %d,%d; pvi=%lx dwMon=%d fShow=%d", (int)iX, (int)iY, (__int64)pvi, (int)dwMonitor, (int)fShow);
   OutputDebugStringW (szTemp);
#endif

   // BUGFIX - If there's a verb tooltip then don't want hotspot tooltip
   if (m_pVerbToolTip)
      fShow = FALSE;

   // if dont want to show make sure it's gone
   if (!fShow) {
      if (m_pHotSpotToolTip) {
         delete m_pHotSpotToolTip;
         m_pHotSpotToolTip = NULL;
      }
      return;
   }

   // if have a tooltip, but is wrong monitor, then delete
   HWND hWndMain = dwMonitor ? m_hWndSecond : m_hWndPrimary;
   if (m_pHotSpotToolTip && (GetParent(m_pHotSpotToolTip->m_hWnd) != hWndMain)) {
      delete m_pHotSpotToolTip;
      m_pHotSpotToolTip = NULL;
   }

   // figure out the text
   PWSTR pszHotSpotText = NULL;
   char *pszaHotSpotText = NULL;
   PCCircumrealityHotSpot ph = pvi ? pvi->HotSpotMouseIn (iX, iY) : NULL;
   CMem memW, memA;
   if (ph && !m_fMessageDiabled && !m_fMenuExclusive && (ph->m_dwCursor != 10))
      pszHotSpotText = ph->m_ps ? ph->m_ps->Get() : NULL;
   if (pszHotSpotText) {
      // BUGFIX - If hotspot text starts with @ then skip until space
      if (pszHotSpotText[0] == L'@') {
         for (; pszHotSpotText[0] && !iswspace(pszHotSpotText[0]); pszHotSpotText++);
         if (pszHotSpotText[0])
            pszHotSpotText++;
      }

      MemZero (&memW);
      MemCat (&memW, L"<bold>");
      MemCatSanitize (&memW, pszHotSpotText);
      MemCat (&memW, L"</bold>");
      if (!memA.Required((wcslen((PWSTR)memW.p)+1)*sizeof(WCHAR)))
         return;
      WideCharToMultiByte (CP_ACP, 0, (PWSTR)memW.p, -1, (char*)memA.p, (DWORD)memA.m_dwAllocated, 0, 0);
      pszaHotSpotText = (char*)memA.p;
   }

   // if text different then wipe
   if (m_pHotSpotToolTip) {
      // delete if no text, or not the same
      if (!pszaHotSpotText || strcmp ((char*)m_pHotSpotToolTip->m_memTip.p, (char*)pszaHotSpotText) ) {
         // else, no text, so delete
         delete m_pHotSpotToolTip;
         m_pHotSpotToolTip = NULL;
      }
   } // if have tooltip

   // may not want anything
   if (!pszaHotSpotText)
      return;

   // convert to coords for hWndMain
   POINT p;
   p.x = iX;
   p.y = iY;
   ClientToScreen (pvi->m_hWnd, &p);
   // ScreenToClient (hWndMain, &p);
   iX = p.x;
   iY = p.y;

   // else want to show
   if (!m_pHotSpotToolTip) {
      RECT r;
      r.left = r.right = iX;
      r.top = r.bottom = iY;

      // convert the string
      m_pHotSpotToolTip = new CToolTip;
      if (!m_pHotSpotToolTip)
         return;


      if (!m_pHotSpotToolTip->Init (pszaHotSpotText, 0, &r, hWndMain)) {
         delete m_pHotSpotToolTip;
         m_pHotSpotToolTip = NULL;
         return;
      }
   }
   
   // want to be down and to the right a bit
   iX += 24;
   iY += 24;

   // move the window
   RECT r;
   GetWindowRect (m_pHotSpotToolTip->m_hWnd, &r);
   if ((r.top == iY) && (r.left == iX))
      return;  // no change

   // else move it
   MoveWindow (m_pHotSpotToolTip->m_hWnd, iX, iY, r.right-r.left, r.bottom-r.top, TRUE);
}


/*************************************************************************************
CMainWindow::VerbClickOnObject - Call this when the verb window has a verb
   (m_memVerbShow.p has string) and the user clicks on an object.

inputs
   GUID           *pgID - Object that was clicked on
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::VerbClickOnObject (GUID *pgID)
{
   if (!((PWSTR)m_memVerbShow.p)[0])
      return FALSE;
   if (IsEqualGUID (*pgID, GUID_NULL))
      return FALSE;  // can't click on NULL object

   PWSTR pszDo = (PWSTR)m_memVerbDo.p;
   if (!pszDo || !pszDo[0])
      return FALSE;

   CMem memDo;
   MemZero (&memDo);
   MemCat (&memDo, pszDo);
   LANGID lid = m_lidVerb;

   // disable the current command, since can click once
   VerbDeselect ();
   VerbTooltipUpdate ();
   HotSpotTooltipUpdate();


   if (m_fMessageDiabled || m_fMenuExclusive) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return TRUE;
   }

   SendTextCommand (lid, (PWSTR)memDo.p, NULL, NULL, pgID, TRUE, TRUE, TRUE);
   HotSpotDisable();
   BeepWindowBeep (ESCBEEP_LINKCLICK);
   return TRUE;
}



/*************************************************************************************
CMainWindow::VerbSelect - Call this to select a verb.

pszDo should have a <click> string in it

inputs
   PWSTR          pszShow - Command to show. Ignored if dwVerbSelected != -1.
                     If NULL, or blank then uses pszDo
   PWSTR          pszDo - Command to do. Ignored if dwVerbSelected != -1.
   LANGID         lid - Language. Ignored if dwVerbSelected != -1.
   DWORD          dwVerbSelected - Normally -1. If not, then associated with an icon
   BOOL           fVerbSelectedIcon - Icon used if dwVerbselected used
returns
   none
*/
void CMainWindow::VerbSelect (PWSTR pszShow, PWSTR pszDo, LANGID lid,
                              DWORD dwVerbSelected, BOOL fVerbSelectedIcon)
{
   // first off, deselect if selected
   VerbDeselect();

   if (dwVerbSelected != (DWORD) -1) {
      PCResVerbIcon *ppr = NULL;
      if (fVerbSelectedIcon && m_pResVerbChat)
         ppr = (PCResVerbIcon*)m_pResVerbChat->m_lPCResVerbIcon.Get(dwVerbSelected);
      else if (!fVerbSelectedIcon && m_pResVerb)
         ppr = (PCResVerbIcon*)m_pResVerb->m_lPCResVerbIcon.Get(dwVerbSelected);
      if (!ppr)
         return;  // error
      PCResVerbIcon pvi = ppr[0];

      pszShow = (PWSTR)pvi->m_memShow.p;
      pszDo = (PWSTR)pvi->m_memDo.p;
      lid = pvi->m_lid;
   }

   MemZero (&m_memVerbShow);
   MemCat (&m_memVerbShow, (pszShow && pszShow[0]) ? pszShow : pszDo);
   MemZero (&m_memVerbDo);
   MemCat (&m_memVerbDo, pszDo);
   m_lidVerb = lid;
   m_dwVerbSelected = dwVerbSelected;
   m_fVerbSelectedIcon = fVerbSelectedIcon;

   // don't call here: VerbTooltipUpdate ();
}


/*************************************************************************************
CMainWindow::VerbDeselect - Deselects any selected verbs. This also calls into
the icon windows.
*/
void CMainWindow::VerbDeselect (void)
{
   if (m_dwVerbSelected != (DWORD)-1) {
      // if it's this one then deselect
      if (!m_fVerbSelectedIcon && m_pResVerb) {
         PCResVerbIcon *ppr = (PCResVerbIcon*)m_pResVerb->m_lPCResVerbIcon.Get(m_dwVerbSelected);
         if (ppr && ppr[0] && ppr[0]->m_pButton)
            ppr[0]->m_pButton->FlagsSet (ppr[0]->m_pButton->FlagsGet() & ~(IBFLAG_REDARROW));
      }
      else if (m_fVerbSelectedIcon) {
         // call icon windows
         DWORD i;
         PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
         for (i = 0; i < m_lPCIconWindow.Num(); i++)
            ppi[i]->VerbDeselect (m_dwVerbSelected);
      }
      m_dwVerbSelected = -1;
   }

   MemZero (&m_memVerbShow);
   MemZero (&m_memVerbDo);
   if (m_pVerbToolTip) {
      delete m_pVerbToolTip;
      m_pVerbToolTip = NULL;
   }
}



/*************************************************************************
TransShowHeader - Draws the transshow header.

Appends something like:
   <tr><td>
      My title <small><a href=transshow0>(Click to hide)</a></small>
   </td></tr>

inputs
   PCMem       pMem - To append to, with MemCat()
   PWSTR       pszTitle - Title string
   DWORD       dwNumber - Transshow number, 0..3
   BOOL        fClickToHide - If TRUE, click to hide
returns
   none
*/
void TransShowHeader (PCMem pMem, PWSTR pszTitle, DWORD dwNumber, BOOL fClickToHide)
{
   // NOTE: Not used anymore

   MemCat (pMem, L"<tr><td");
   MemCat (pMem, fClickToHide ? L" bgcolor=#006000" : L" bgcolor=#004000");
   MemCat (pMem, L"><button buttonwidth=16 buttonheight=16 style=");
   MemCat (pMem, fClickToHide ? L"uptriangle" : L"downtriangle");
   MemCat (pMem, L" name=transshow");
   MemCat (pMem, (int)dwNumber);
   MemCat (pMem, L"><bold>");
   MemCatSanitize (pMem, pszTitle);
   MemCat (pMem, L"</bold></button>");
   MemCat (pMem, L"</td></tr>");
}

/*************************************************************************************
CMainWindow::TranscriptDescription - Appends the object's description, in a table
of its own, into pMem. Called by TranscriptMenuAndDesc().

inputs
   PCMem          pMem - To append to
returns
   BOOL - TRUE if there is a description. FALSE if there isn't one and nothing is returned.
*/
BOOL CMainWindow::TranscriptDescription (PCMem pMem)
{
   PCDisplayWindow pMain = FindMainDisplayWindow ();
   if (!pMain || !pMain->m_pvi)
      return FALSE;

   // must have a name
   PWSTR pszName = (PWSTR) pMain->m_pvi->m_memName.p;
   if (!pszName || !pszName[0])
      return FALSE;

   PWSTR pszOther = (PWSTR) pMain->m_pvi->m_memOther.p;
   PWSTR pszDescription = (PWSTR) pMain->m_pvi->m_memDescription.p;

   // must have an other or description
   if (!pszOther || !pszOther[0])
      pszOther = NULL;
   if (!pszDescription || !pszDescription[0])
      pszDescription = NULL;
   if (!pszOther && !pszDescription)
      return FALSE;

   // table
   MemCat (pMem,
      m_fLightBackground ?
         L"<table width=100% bordercolor=#404040>" : 
         L"<table width=100% bordercolor=#e0e0e0>");

   BOOL fClickToHide = (m_dwTransShow & 0x04) ? TRUE : FALSE;
   TransShowHeader (pMem, L"What am I looking at?", 2, fClickToHide);

   if (fClickToHide) {
      // BUGFIX - Made transparent white background
      MemCat (pMem, L"<tr><td><colorblend color=#ffffff posn=background transparent=0.25/><font color=#000000>");

      MemCat (pMem, L"<big><bold>"); // BUGFIX - Took out one big
      MemCatSanitize (pMem, pszName);
      MemCat (pMem, L"</bold></big>"); // BUGFIX - Took out one big
      // MemCat (pMem, L"<small> (What you're looking at)</small>");

      if (pszOther) {
         MemCat (pMem, L"<small><p/><italic>");
         MemCatSanitize (pMem, pszOther);
         MemCat (pMem, L"</italic></small>");
      }

      if (pszDescription) {
         MemCat (pMem, L"<p/>"); // BUGFIX - Not as large <big>");
         MemCatSanitize (pMem, pszDescription);
         // BUGFIX - Not as large MemCat (pMem, L"</big>");
      }
      MemCat (pMem, L"</font></td></tr>");
   } // !fClickToHide
   MemCat (pMem, L"</table>");

   return TRUE;
}

/*************************************************************************************
CMainWindow::TranscriptMenu - Appends the object's menu, in a table
of its own, into pMem. Called by TranscriptMenuAndDesc().

inputs
   PCMem          pMem - To append to
   DWORD             dwColumns - Number of columns to use, 2 or 1
returns
   BOOL - TRUE if there is a menu/options. FALSE if there isn't one and nothing is returned.
*/
BOOL CMainWindow::TranscriptMenu (PCMem pMem, DWORD dwColumns)
{
   MemZero (pMem);

   // see if there's a menu
   CMem memMenu;
   MenuMML (&memMenu, dwColumns);
   PWSTR pszMenu = (PWSTR)memMenu.p;
   BOOL fShowMenu = pszMenu[0] ? TRUE : FALSE;

   // see if should say click on an object
   BOOL fClickOnObject = m_lPCIconWindow.Num() ? TRUE : FALSE;
   fClickOnObject = FALSE; // BUGFIX - Disable this to save sceen real estate. Should be discoverable

   // see if have chat window
   PCIconWindow pIconChat = FindChatWindow ();
   BOOL fChatWindow = pIconChat ? pIconChat->CanChatTo() : FALSE;

   DWORD dwNumVisible = (fShowMenu ? 1 : 0) + (fClickOnObject ? 1 : 0) +
      (m_fTransCommandVisible ? 1 : 0) + (fChatWindow ? 1 : 0);

   // make sure there's something to display
   if (!dwNumVisible)
      return FALSE;

   // BUGFIX - Darker border
   MemCat (pMem, L"<table width=100% bordercolor=#404040>");

   BOOL fSomethingAbove = FALSE;

   BOOL fClickToHide = (m_dwTransShow & 0x08) ? TRUE : FALSE;
   // BUGFIX - Don't show this header in order to reduce screen real-estate
   // TransShowHeader (pMem, L"What can I do?", 3, fClickToHide);

   if (!fClickToHide)
      goto done;


   DWORD dwInTR = 0;

   if (m_fTransCommandVisible || fChatWindow) {
      if (!dwInTR) {
         MemCat (pMem, L"<tr>");
         dwInTR++;
      }
      MemCat (pMem, L"<td><small>");

      // BUGBUG - may want to set prefs about whether using command or chat

      if (!fChatWindow) { // && m_fTransCommandVisible
         MemCat (pMem,
            fSomethingAbove ?
               L"Or, type an <bold>action</bold> and press enter:" :
               L"Type in an <bold>action</bold> and press enter:"
            );
         m_fCombinedSpeak = FALSE;
      }
      else if (!m_fTransCommandVisible) { // && fChatWindow
         MemCat (pMem,
            fSomethingAbove ?
               L"Or, type in something to <bold>speak</bold> and press enter:" :
               L"Type in something to <bold>speak</bold> and press enter:"
            );
         m_fCombinedSpeak = TRUE;
      }
      else { // fChatWindow && m_fTransCommandVisible
         MemCat (pMem,
            fSomethingAbove ?
               L"Or, type in " :
               L"Type in "
            );
         MemCat (pMem,
            L"<button style=check radiobutton=true valign=center buttonheight=16 buttonwidth=16 group=actspeak0,actspeak1 name=actspeak0>"
            L"<bold>an action</bold>"
            L"</button>"
            L" or "
            L"<button style=check radiobutton=true valign=center buttonheight=16 buttonwidth=16 group=actspeak0,actspeak1 name=actspeak1>"
            L"<bold>something to speak</bold>"
            L"</button>"
            L" and press enter:"
            );
      }

      MemCat (pMem,
#ifdef BLACKTRANSCRIPT
         L"<br/></small><font color=#000000><bold>"
            // because background of edit field always white, don't need to change
#else
         L"</small></td><td><font color=#800000><bold>"
#endif
         L"<edit width=100% maxchars=256 name=combined/>"
         L"</bold></font>");

      // languages
      if (fChatWindow && (pIconChat->m_lLanguage.Num() >= 2)) {
         MemCat (pMem, L"<small><br/>");

         DWORD i, j;
         for (i = 0; i <= pIconChat->m_lLanguage.Num(); i++) {
            MemCat (pMem, L"<button style=check radiobutton=true buttonwidth=16 buttonheight=16 group=");
            for (j = 0; j <= pIconChat->m_lLanguage.Num(); j++) {
               if (j)
                  MemCat (pMem, L",");
               MemCat (pMem, L"lang");
               MemCat (pMem, (int)j);
            } // j
            MemCat (pMem, L" name=lang");
            MemCat (pMem, (int)i);
            MemCat (pMem, L">");
            if (!i)
               MemCat (pMem, L"Any language");
            else
               MemCatSanitize (pMem, (PWSTR)pIconChat->m_lLanguage.Get(i-1));
            MemCat (pMem, L"</button>");
         }
         MemCat (pMem, L"</small>");
      }

      MemCat (pMem, L"</td>");

      if ((dwColumns < 2) && dwInTR) {
         MemCat (pMem, L"</tr>");
         dwInTR--;
      }

      fSomethingAbove = TRUE;
   }

#if 0 // old code, before combine
   if (m_fTransCommandVisible) {
      if (!dwInTR) {
         MemCat (pMem, L"<tr>");
         dwInTR++;
      }
      MemCat (pMem, L"<td><small>");

      MemCat (pMem,
         fSomethingAbove ?
            L"Or, type in an <bold>action</bold> (F6) to act on and press enter:" :
            L"Type in an <bold>action</bold> (F6) to act on and press enter:"
         );

      MemCat (pMem,
#ifdef BLACKTRANSCRIPT
         L"<br/></small><font color=#004000><bold>"
            // because background of edit field always white, don't need to change
#else
         L"</small></td><td><font color=#008000><bold>"
#endif
         L"<edit width=100% maxchars=256 name=command/>"
         L"</bold></font></td>");

      if ((dwColumns < 2) && dwInTR) {
         MemCat (pMem, L"</tr>");
         dwInTR--;
      }

      fSomethingAbove = TRUE;
   }

   if (fChatWindow) {
      if (!dwInTR) {
         MemCat (pMem, L"<tr>");
         dwInTR++;
      }
      MemCat (pMem, L"<td><small>");

      MemCat (pMem,
         fSomethingAbove ?
            L"Or, type in something to <bold>speak</bold> (F7) and press enter:" :
            L"Type in something to <bold>speak</bold> (F7) and press enter:"
         );

      MemCat (pMem,
#ifdef BLACKTRANSCRIPT
         L"<br/></small><font color=#400000><bold>"
            // because background of edit field always white, don't need to change
#else
         L"</small></td><td><font color=#800000><bold>"
#endif
         L"<edit width=100% maxchars=256 name=speak/>"
         L"</bold></font>");

      // languages
      if (pIconChat->m_lLanguage.Num() >= 2) {
         MemCat (pMem, L"<small><br/>");

         DWORD i, j;
         for (i = 0; i <= pIconChat->m_lLanguage.Num(); i++) {
            MemCat (pMem, L"<button style=check radiobutton=true buttonwidth=16 buttonheight=16 group=");
            for (j = 0; j <= pIconChat->m_lLanguage.Num(); j++) {
               if (j)
                  MemCat (pMem, L",");
               MemCat (pMem, L"lang");
               MemCat (pMem, (int)j);
            } // j
            MemCat (pMem, L" name=lang");
            MemCat (pMem, (int)i);
            MemCat (pMem, L">");
            if (!i)
               MemCat (pMem, L"Any language");
            else
               MemCatSanitize (pMem, (PWSTR)pIconChat->m_lLanguage.Get(i-1));
            MemCat (pMem, L"</button>");
         }
         MemCat (pMem, L"</small>");
      }

      MemCat (pMem, L"</td>");

      if ((dwColumns < 2) && dwInTR) {
         MemCat (pMem, L"</tr>");
         dwInTR--;
      }

      fSomethingAbove = TRUE;
   }
#endif // o

   if (dwInTR) {
      MemCat (pMem, L"</tr>");
      dwInTR--;
   }

   if (fClickOnObject) {
      MemCat (pMem, L"<tr><td><small>");

      MemCat (pMem,
         fSomethingAbove ?
            L"Or, click on a character or object image for a list of options." :
            L"Click on a character or object image for a list of options."
         );

      MemCat (pMem, L"</small></td></tr>");

      fSomethingAbove = TRUE;
   }

   // BUGFIX - Moved down since transcript on top

   if (fShowMenu) {
      MemCat (pMem, L"<tr><td>");

      MemCat (pMem, L"<small>");
      MemCat (pMem,
         fSomethingAbove ?
            L"Or, select one of the following menu items:" :
            L"Select one of the following menu items:"
         );
      MemCat (pMem, L"<br/></small>");

      MemCat (pMem, pszMenu);

      MemCat (pMem, L"</td></tr>");

      fSomethingAbove = TRUE;
   }

done:
   MemCat (pMem, L"</table>");
   

   return TRUE;
}


/*************************************************************************************
CMainWindow::TranscriptMenuAndDesc - Returns a string (that uses gMemTemp) for the
menu and description substitution for transcript.mml, or NULL if none.

inputs
   none
returns
   PWSTR - String, or NULL
*/
PWSTR CMainWindow::TranscriptMenuAndDesc (void)
{

   MemZero (&gMemTemp);

   // how wide is the client compared to the main window
   RECT rClient, rMain;
   GetClientRect (m_hWndTranscript, &rClient);
   GetClientRect (GetParent(m_hWndTranscript), &rMain);
   int iClientWidth = rClient.right - rClient.left;
   int iClientHeight = rClient.bottom - rClient.top;
   int iMainWidth = rMain.right - rMain.left;
   // BUGFIX - New test for very wide
   // BOOL fVeryWide = iClientWidth > (iMainWidth * 2 / 5);
   BOOL fVeryWide = (iClientWidth > iClientHeight);
   fVeryWide = FALSE; // BUGFIX - no description now
   if (fVeryWide)
      iClientWidth /= 2;   // since will be splitting


   // if don't have both "What can I do?" and "What am I looking at?" on, then NOT very wide
   if (fVeryWide && ((m_dwTransShow & 0x0c) != 0x0c))
      fVeryWide = FALSE;

   // how many columsn
   DWORD dwMinSize = (DWORD)(400.0 * FontScaleByScreenSize());
   switch (m_iTransSize) {
   case 1:
      dwMinSize += dwMinSize/2;
      break;
   case -1:
      dwMinSize = dwMinSize * 2 / 3;
      break;
   } // switch
      // BUGFIX - minsize for double columns better

   DWORD dwColumns = (iClientWidth > (int)dwMinSize) ? 2 : 1;

   // BUGFIX - If TTS only, no text, then single column always,
   // so looks less crowded
   if (m_iTextVsSpeech >= 2)
      dwColumns = 1;

   // transcript menu
   BOOL fMenu, fDesc;
   CMem memMenu, memDesc;
   fMenu = TranscriptMenu (&memMenu, dwColumns);
   fDesc = FALSE; // BUGFIX - No transcript description since show on main: TranscriptDescription (&memDesc);

   if (!fMenu && !fDesc)
      return NULL;



   if (fVeryWide) {
      // BUGFIX - Only show <p/> if transcript above
      if (m_dwTransShow & 0x02)
         MemCat (&gMemTemp, L"<p/>");

      MemCat (&gMemTemp, L"<table width=100% innerlines=0 border=0 lrmargin=0 tbmargin=0><tr>");
   }

   // description
   if (fVeryWide) {
      MemCat (&gMemTemp, L"<td valign=top width=");
      MemCat (&gMemTemp, (DWORD)max(iClientWidth,1));
      MemCat (&gMemTemp, L">");
   }
   if (fDesc) {
      if (!fVeryWide && (m_dwTransShow & 0x02))
         MemCat (&gMemTemp, L"<p/>");
      MemCat (&gMemTemp, (PWSTR)memDesc.p);
   }
   if (fVeryWide)
      MemCat (&gMemTemp, L"</td>");

   // menu
   if (fVeryWide) {
      MemCat (&gMemTemp, L"<td width=16/><td valign=top width=");
      MemCat (&gMemTemp, (DWORD)max(iClientWidth,1));
      MemCat (&gMemTemp, L">");
   }

   if (fMenu) {
      if (!fVeryWide && (m_dwTransShow & 0x02))
         MemCat (&gMemTemp, L"<p/>");
      MemCat (&gMemTemp, (PWSTR)memMenu.p);
   }
   if (fVeryWide)
      MemCat (&gMemTemp, L"</td>");

   if (fVeryWide)
      MemCat (&gMemTemp, L"</tr></table>");



   return (PWSTR)gMemTemp.p;
}


/*************************************************************************
TranscriptPage
*/
BOOL TranscriptPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMainWindow pvi = (PCMainWindow)pPage->m_pUserData;   // node to modify
#ifdef UNIFIEDTRANSCRIPT
   static DWORD dwTimerID = 0;
#endif

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      // remember this
      pvi->m_pPageTranscript = pPage;
      break;

      // moved below
//   case ESCM_DESTRUCTOR:
//      // no page
//      pvi->m_pPageTranscript = NULL;
//      break;

   case ESCM_INITPAGE:
      pPage->VScroll (1000000);  // scroll to the bottom
      pPage->Invalidate ();   // to work around bug

      // set the font
      WCHAR szTemp[64];
      PCEscControl pControl;
      swprintf (szTemp, L"font%d", (int)max(pvi->m_iTransSize+1,0));
      if (pControl = pPage->ControlFind (szTemp))
         pControl->AttribSetBOOL (Checked(), TRUE);

      // if (pControl = pPage->ControlFind (L"muteall"))
      //   pControl->AttribSetBOOL (Checked(), pvi->m_fMuteAll);

#if 0 // remove since disabled settings on transcript
      swprintf (szTemp, L"speed%d", (int)max(pvi->m_iSpeakSpeed+2,0));
      if (pControl = pPage->ControlFind (szTemp))
         pControl->AttribSetBOOL (Checked(), TRUE);

      if (pControl = pPage->ControlFind (L"powersaver"))
         pControl->AttribSetBOOL (Checked(), pvi->m_fPowerSaver);
#endif

#ifdef UNIFIEDTRANSCRIPT
      // start the timer?
      if (pvi->m_fMenuTimeOut && pvi->m_fMenuTimeOutOrig)
         dwTimerID = pPage->m_pWindow->TimerSet (100, pPage);

      // update the progress control
      pPage->Message (ESCM_USER+82);

      // set focus to either the command or the chat
#if 0 // old code
      if (pControl = pPage->ControlFind (L"command")) {
         if (pvi->m_fTransFocusCommand)
            pPage->FocusSet (pControl);

         // set the text
         pControl->AttribSet (Text(), pvi->m_szTransCommand);
         pControl->AttribSetInt (L"selstart", pvi->m_pTransSelCommand.x);
         pControl->AttribSetInt (L"selend", pvi->m_pTransSelCommand.y);
      }
      if (pControl = pPage->ControlFind (L"speak")) {
         if (!pvi->m_fTransFocusCommand)
            pPage->FocusSet (pControl);

         // set the text
         pControl->AttribSet (Text(), pvi->m_szTransSpeak);
         pControl->AttribSetInt (L"selstart", pvi->m_pTransSelSpeak.x);
         pControl->AttribSetInt (L"selend", pvi->m_pTransSelSpeak.y);
      }
#endif
      if (pControl = pPage->ControlFind (L"combined")) {
         pPage->FocusSet (pControl);

         // set the text
         pControl->AttribSet (Text(), pvi->m_szTransCombined);
         pControl->AttribSetInt (L"selstart", pvi->m_pTransSelCombined.x);
         pControl->AttribSetInt (L"selend", pvi->m_pTransSelCombined.y);
      }
      swprintf (szTemp, L"actspeak%d", (int)(pvi->m_fCombinedSpeak ? 1 : 0));
      if (pControl = pPage->ControlFind (szTemp))
         pControl->AttribSetBOOL (Checked(), TRUE);

      // check the language
      PCIconWindow pIconChat;
      pIconChat = pvi->FindChatWindow ();
      if (pIconChat) {
         swprintf (szTemp, L"lang%d", (int)pIconChat->m_dwLanguage+1);
         if (pControl = pPage->ControlFind (szTemp))
            pControl->AttribSetBOOL (Checked(), TRUE);
      }

      swprintf (szTemp, L"rqual%d", (int)max(pvi->m_iPreferredQualityMono,0));
      if (pControl = pPage->ControlFind (szTemp))
         pControl->AttribSetBOOL (Checked(), TRUE);

      swprintf (szTemp, L"rtext%d", (int)max(pvi->m_iTextVsSpeech,0));
      if (pControl = pPage->ControlFind (szTemp))
         pControl->AttribSetBOOL (Checked(), TRUE);

#if 0 // remove since disabled settings on transcript
      // check the render quality/resolution button
      swprintf (szTemp, L"rres%d", (int)max(pvi->m_iPreferredResolution,0));
      if (pControl = pPage->ControlFind (szTemp))
         pControl->AttribSetBOOL (Checked(), TRUE);
      swprintf (szTemp, L"rqual%d", (int)max(pvi->m_iPreferredQuality,0));
      if (pControl = pPage->ControlFind (szTemp))
         pControl->AttribSetBOOL (Checked(), TRUE);
      swprintf (szTemp, L"rdyn%d", (int)max(pvi->m_iPreferredDynamic,0));
      if (pControl = pPage->ControlFind (szTemp))
         pControl->AttribSetBOOL (Checked(), TRUE);
      swprintf (szTemp, L"art%d", (int)pvi->m_dwArtStyle);
      if (pControl = pPage->ControlFind (szTemp))
         pControl->AttribSetBOOL (Checked(), TRUE);
#endif // 0

#endif
      break;

#ifdef UNIFIEDTRANSCRIPT
   case ESCM_TIMER:
      {
         PESCMTIMER p =(PESCMTIMER) pParam;
         if (p && (p->dwID == dwTimerID)) {
            if (pvi->m_fMenuTimeOut > 0) {
               pvi->m_fMenuTimeOut -= 0.1;   // since timers 100 ms
               pvi->m_fMenuTimeOut = max(0, pvi->m_fMenuTimeOut);

               if (pvi->m_fMenuTimeOut <= 0) {
                  pPage->m_pWindow->TimerKill (dwTimerID);
                  dwTimerID = 0;
                  pvi->MenuLinkDefault ();
               }
            }
            else {
               // must have clicked, so kill the timer anyway
               pPage->m_pWindow->TimerKill (dwTimerID);
               dwTimerID = 0;
            }

            // update the progress-bar
            pPage->Message (ESCM_USER+82);
         }
      }
      break;

   case ESCM_USER+82:   // update the progress bar
      {
         PCEscControl pControl = pPage->ControlFind (L"Progress");
         pvi->m_fMenuTimeOut = min(pvi->m_fMenuTimeOut, pvi->m_fMenuTimeOutOrig);
         pvi->m_fMenuTimeOut = max(pvi->m_fMenuTimeOut, 0);
         fp fProgress = pvi->m_fMenuTimeOutOrig - pvi->m_fMenuTimeOut;
         if (pvi->m_fMenuTimeOutOrig)
            fProgress /= pvi->m_fMenuTimeOutOrig;
         else
            fProgress = 0;
         fProgress *= 1000;

         if (pControl)
            pControl->AttribSetInt (L"pos", (int)fProgress);
      }
      return TRUE;

   case ESCM_DESTRUCTOR:
      {
         // no page
         pvi->m_pPageTranscript = NULL;


         // get the selection
         PCEscControl pControl;
         PCEscControl pFocus = pPage->FocusGet ();
#if 0 // old code
         if (pControl = pPage->ControlFind (L"command")) {
            pvi->m_pTransSelCommand.x = pControl->AttribGetInt (L"selstart");
            pvi->m_pTransSelCommand.y = pControl->AttribGetInt (L"selend");

            if (pFocus == pControl)
               pvi->m_fTransFocusCommand = TRUE;
         }
         else
            pvi->m_pTransSelCommand.x = pvi->m_pTransSelCommand.y = 0;
         if (pControl = pPage->ControlFind (L"speak")) {
            pvi->m_pTransSelSpeak.x = pControl->AttribGetInt (L"selstart");
            pvi->m_pTransSelSpeak.y = pControl->AttribGetInt (L"selend");

            if (pFocus == pControl)
               pvi->m_fTransFocusCommand = FALSE;
         }
         else
            pvi->m_pTransSelSpeak.x = pvi->m_pTransSelSpeak.y = 0;
#endif // 0
         if (pControl = pPage->ControlFind (L"combined")) {
            pvi->m_pTransSelCombined.x = pControl->AttribGetInt (L"selstart");
            pvi->m_pTransSelCombined.y = pControl->AttribGetInt (L"selend");
         }
         else
            pvi->m_pTransSelCombined.x = pvi->m_pTransSelCombined.y = 0;

         if (dwTimerID) {
            pPage->m_pWindow->TimerKill (dwTimerID);
            dwTimerID = 0;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;
         PWSTR psz  = p->pControl->m_pszName;

         // remember edit changes in case dialog redrawn
#if 0 // old code
         if (!_wcsicmp(psz, L"command")) {
            DWORD dwNeed;
            pvi->m_szTransCommand[0] = 0;
            p->pControl->AttribGet (Text(), pvi->m_szTransCommand, sizeof(pvi->m_szTransCommand), &dwNeed);
            return TRUE;
         }
         if (!_wcsicmp(psz, L"speak")) {
            DWORD dwNeed;
            pvi->m_szTransSpeak[0] = 0;
            p->pControl->AttribGet (Text(), pvi->m_szTransSpeak, sizeof(pvi->m_szTransSpeak), &dwNeed);
            return TRUE;
         }
#endif // 0
         if (!_wcsicmp(psz, L"combined")) {
            DWORD dwNeed;
            pvi->m_szTransCombined[0] = 0;
            p->pControl->AttribGet (Text(), pvi->m_szTransCombined, sizeof(pvi->m_szTransCombined), &dwNeed);
            return TRUE;
         }
      }
      break;

   case ESCM_CHAR:
      {
         PESCMCHAR p = (PESCMCHAR)pParam;
         if (p->wCharCode != L'\r')
            break;   // dont eat

         // only eat if focus is right
         PCEscControl pFocus = pPage->FocusGet ();
         BOOL fCommand;
#if 0 // old code
         PCEscControl pCommand = pPage->ControlFind (L"command");
         PCEscControl pSpeak = pPage->ControlFind (L"speak");

         if (pCommand && (pFocus == pCommand))
            fCommand = TRUE;
         else if (pSpeak && (pFocus == pSpeak))
            fCommand = FALSE;
         else
            break;
#endif
         PCEscControl pCombined = pPage->ControlFind (L"combined");
         fCommand = !pvi->m_fCombinedSpeak;

         p->fEaten = TRUE;

         PWSTR pszText = pvi->m_szTransCombined; // fCommand ? pvi->m_szTransCommand : pvi->m_szTransSpeak;
         if (!pszText[0]) {
            EscChime (ESCCHIME_WARNING);
            return TRUE;
         }

         // if start with quotes, then assume it's not a command, and that spoken
         if (pszText[0] == L'"') {
            fCommand = FALSE;
            pszText++;
            size_t dwLen = wcslen(pszText);
            if (dwLen && (pszText[dwLen-1] == L'"'))
               pszText[dwLen-1] = 0;   // remove last quote if started with quote
         }

         if (fCommand) {
            pvi->SendTextCommand (DEFLANGID, pszText, NULL, NULL, NULL, FALSE, TRUE, TRUE);
            // BUGBUG - right now hard code to english, but language depends on user setting
            pvi->HotSpotDisable();
         }
         else {
            PCIconWindow pIconChat = pvi->FindChatWindow ();
            if (!pIconChat || !pIconChat->Speak(pszText) )
               EscChime (ESCCHIME_WARNING);
         }

         // clear out the text
         pszText = pvi->m_szTransCombined;
         pszText[0] = 0;
#if 0 // old code
         if (fCommand)
            pCommand->AttribSet (Text(), pszText);
         else
            pSpeak->AttribSet (Text(), pszText);
#endif
         if (pCombined)
            pCombined->AttribSet (Text(), pszText);
         // BeepWindowBeep (ESCBEEP_LINKCLICK); not needed
         return TRUE;
      }
      break;

#endif // UNIFIEDTRANSCRIPT


   case ESCM_LBUTTONDOWN:
      // set this as the foreground window
      SetWindowPos (GetParent(pPage->m_pWindow->m_hWnd), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      break;

   case ESCN_BUTTONPRESS:
      {
         // BUGFIX - Make font and speaking rate be buttons instead of drop-down since
         // drop-down crashes if change while open
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszFont = L"font", pszRate = L"speed", pszLang = L"lang", pszRQual = L"rqual", pszRRes = L"rres",
            pszTransShow = L"transshow", pszArt = L"art", pszRDyn = L"rdyn", pszRText = L"rtext",
            pszActSpeak = L"actspeak";
         DWORD dwFontLen = (DWORD)wcslen(pszFont), dwRateLen = (DWORD)wcslen(pszRate), dwLangLen = (DWORD)wcslen(pszLang),
            dwRQualLen = (DWORD)wcslen(pszRQual), dwRResLen = (DWORD)wcslen(pszRRes),
            dwTransShowLen = (DWORD)wcslen(pszTransShow), dwArtLen = (DWORD)wcslen(pszArt),
            dwRDynLen = (DWORD)wcslen(pszRDyn),
            dwRTextLen = (DWORD)wcslen(pszRText),
            dwActSpeakLen = (DWORD)wcslen(pszActSpeak);

         if (!_wcsnicmp(psz, pszFont, dwFontLen)) {
            int iVal = _wtoi(psz + dwFontLen) - 1;
            if (iVal == pvi->m_iTransSize)
               return TRUE;

            // update main window so redraws description text on it
            PCDisplayWindow pMain = pvi->FindMainDisplayWindow ();
            if (pMain)
               InvalidateRect (pMain->m_hWnd, NULL, FALSE);

            pvi->m_iTransSize = iVal;
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSUPDATE, 0, 0);
            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszActSpeak, dwActSpeakLen)) {
            DWORD dwActSpeak = _wtoi(psz + dwActSpeakLen);

            pvi->m_fCombinedSpeak = dwActSpeak;

            // make sure combined has the focus
            pControl = pPage->ControlFind (L"combined");
            if (pControl)
               pPage->FocusSet (pControl);
            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszRQual, dwRQualLen)) {
            int iQualityMono = _wtoi(psz + dwRQualLen);

            if (iQualityMono == pvi->m_iPreferredQualityMono)
               return TRUE;   // no change

            pvi->m_iPreferredQualityMono = iQualityMono;

            pvi->ResolutionQualityToRenderSettings (pvi->m_fPowerSaver, pvi->m_iPreferredQualityMono, TRUE, 
               &pvi->m_iResolution, &pvi->m_dwShadowsFlags,
               &pvi->m_dwServerSpeed, &pvi->m_iTextureDetail, &pvi->m_fLipSync,
               &pvi->m_iResolutionLow, &pvi->m_dwShadowsFlagsLow, &pvi->m_fLipSyncLow,
               &pvi->m_dwMovementTransition);
            pvi->m_pRT->ResolutionSet (pvi->m_iResolution, pvi->m_dwShadowsFlags, pvi->m_iTextureDetail, pvi->m_fLipSync,
               pvi->m_iResolutionLow, pvi->m_dwShadowsFlagsLow, pvi->m_fLipSyncLow, TRUE);
            pvi->VisImageReRenderAll ();
            pvi->InfoForServer (L"graphspeed", NULL, pvi->m_dwServerSpeed);
            pvi->InfoForServer (L"movementtransition", NULL, pvi->m_dwMovementTransition);

            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszRText, dwRTextLen)) {
            int iQualityMono = _wtoi(psz + dwRTextLen);

            if (iQualityMono == pvi->m_iTextVsSpeech)
               return TRUE;   // no change

            pvi->TextVsSpeechSet (iQualityMono, TRUE);

            return TRUE;
         }
         //else if (!_wcsicmp (psz, L"muteall")) {
         //   BOOL fMute = p->pControl->AttribGetBOOL(Checked());
         //
         //   pvi->MuteAllSet (fMute, TRUE);
         //
         //   return TRUE;
         //}

#if 0 // disable since removed settings from transcritpt

         else if (!_wcsnicmp(psz, pszRDyn, dwRDynLen)) {
            int iQuality = _wtoi(psz + dwRDynLen);

            if (iQuality == pvi->m_iPreferredDynamic)
               return TRUE;   // no change

            pvi->m_iPreferredDynamic = iQuality;

            pvi->ResolutionQualityToRenderSettings (pvi->m_fPowerSaver, pvi->m_iPreferredResolution, pvi->m_iPreferredQuality,
               pvi->m_iPreferredDynamic,
               &pvi->m_iResolution, &pvi->m_dwShadowsFlags,
               &pvi->m_dwServerSpeed, &pvi->m_iTextureDetail, &pvi->m_fLipSync,
               &pvi->m_iResolutionLow, &pvi->m_dwShadowsFlagsLow, &pvi->m_fLipSyncLow,
               &pvi->m_dwMovementTransition);
            pvi->m_pRT->ResolutionSet (pvi->m_iResolution, pvi->m_dwShadowsFlags, pvi->m_iTextureDetail, pvi->m_fLipSync,
               pvi->m_iResolutionLow, pvi->m_dwShadowsFlagsLow, pvi->m_fLipSyncLow, TRUE);
            pvi->VisImageReRenderAll ();
            pvi->InfoForServer (L"graphspeed", NULL, pvi->m_dwServerSpeed);
            pvi->InfoForServer (L"movementtransition", NULL, pvi->m_dwMovementTransition);

            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszArt, dwArtLen)) {
            DWORD dwArt = _wtoi(psz + dwArtLen);

            if (dwArt == pvi->m_dwArtStyle)
               return TRUE;   // no change

            pvi->m_dwArtStyle = dwArt;

            pvi->InfoForServer (L"artstyle", NULL, pvi->m_dwArtStyle);

            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszRRes, dwRResLen) &&  (psz[dwRResLen] >= L'0') && (psz[dwRResLen] <= L'9') ) {
            int iResolution = _wtoi(psz + dwRResLen);

            if (iResolution == pvi->m_iPreferredResolution)
               return TRUE;   // no change

            pvi->m_iPreferredResolution = iResolution;

            pvi->ResolutionQualityToRenderSettings (pvi->m_fPowerSaver, pvi->m_iPreferredResolution, pvi->m_iPreferredQuality, pvi->m_iPreferredDynamic,
               &pvi->m_iResolution, &pvi->m_dwShadowsFlags,
               &pvi->m_dwServerSpeed, &pvi->m_iTextureDetail, &pvi->m_fLipSync,
               &pvi->m_iResolutionLow, &pvi->m_dwShadowsFlagsLow, &pvi->m_fLipSyncLow,
               &pvi->m_dwMovementTransition);
            pvi->m_pRT->ResolutionSet (pvi->m_iResolution, pvi->m_dwShadowsFlags, pvi->m_iTextureDetail, pvi->m_fLipSync,
               pvi->m_iResolutionLow, pvi->m_dwShadowsFlagsLow, pvi->m_fLipSyncLow, TRUE);
            pvi->VisImageReRenderAll ();
            pvi->InfoForServer (L"graphspeed", NULL, pvi->m_dwServerSpeed);
            pvi->InfoForServer (L"movementtransition", NULL, pvi->m_dwMovementTransition);

            return TRUE;
         }
         else if (!_wcsicmp (psz, L"powersaver") ) {
            pvi->m_fPowerSaver = p->pControl->AttribGetBOOL(Checked());

            pvi->ResolutionQualityToRenderSettings (pvi->m_fPowerSaver, pvi->m_iPreferredResolution, pvi->m_iPreferredQuality, pvi->m_iPreferredDynamic,
               &pvi->m_iResolution, &pvi->m_dwShadowsFlags,
               &pvi->m_dwServerSpeed, &pvi->m_iTextureDetail, &pvi->m_fLipSync,
               &pvi->m_iResolutionLow, &pvi->m_dwShadowsFlagsLow, &pvi->m_fLipSyncLow,
               &pvi->m_dwMovementTransition);
            pvi->m_pRT->ResolutionSet (pvi->m_iResolution, pvi->m_dwShadowsFlags, pvi->m_iTextureDetail, pvi->m_fLipSync,
               pvi->m_iResolutionLow, pvi->m_dwShadowsFlagsLow, pvi->m_fLipSyncLow, TRUE);
            pvi->VisImageReRenderAll ();
            pvi->InfoForServer (L"graphspeed", NULL, pvi->m_dwServerSpeed);
            pvi->InfoForServer (L"movementtransition", NULL, pvi->m_dwMovementTransition);

            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszRate, dwRateLen)) {
            int iVal = _wtoi(psz + dwRateLen) - 2;
            if (iVal == pvi->m_iSpeakSpeed)
               return TRUE;

            EnterCriticalSection (&pvi->m_crSpeakBlacklist);
            pvi->m_iSpeakSpeed = pvi->m_aiSpeakSpeed[pvi->m_pSlideTop->m_dwTab] = iVal;
            InfoForServer (L"speakspeed", NULL, pvi->m_iSpeakSpeed);
            LeaveCriticalSection (&pvi->m_crSpeakBlacklist);
            return TRUE;
         }
#endif // 0
         else if (!_wcsnicmp (psz, pszTransShow, dwTransShowLen)) {
            DWORD  dwVal = _wtoi(psz + dwTransShowLen);

            DWORD dwBit = 1 << dwVal;

            if (pvi->m_dwTransShow & dwBit)
               pvi->m_dwTransShow &= ~dwBit;
            else
               pvi->m_dwTransShow |= dwBit;
            pvi->m_adwTransShow[pvi->m_pSlideTop->m_dwTab] = pvi->m_dwTransShow;

            // since changing mute changes display of page, refresh
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSUPDATE, 0, 0);

            return TRUE;
         }
#ifdef UNIFIEDTRANSCRIPT
         if (!_wcsnicmp(psz, pszLang, dwLangLen)) {
            DWORD  dwVal = _wtoi(psz + dwLangLen);
            PCIconWindow pIconChat = pvi->FindChatWindow ();
            if (pIconChat && (dwVal <= pIconChat->m_lLanguage.Num()))
               pIconChat->m_dwLanguage = dwVal - 1;   // so -1 if first entry
            return TRUE;
         }
#endif
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

#ifdef UNIFIEDTRANSCRIPT
         if ((p->psz[0] == 'm') && (p->psz[1] == 'l') && (p->psz[2] == ':')) {
            pvi->MenuLink (p->psz + 3);
            return TRUE;
         }
#endif // UNIFIEDTRANSCRIPT

         if (!_wcsicmp(p->psz, L"imagesettings")) {
            PostMessage (pvi->m_hWndPrimary, WM_SHOWSETTINGSUI, 1, 0);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"speechsettings")) {
            PostMessage (pvi->m_hWndPrimary, WM_SHOWSETTINGSUI, 2, 0);
            return TRUE;
         }
         if ((p->psz[0] == 's') && (p->psz[1] == 'k') && (p->psz[2] == ':')) {
            DWORD dwNum = _wtoi(p->psz + 3);
            EnterCriticalSection (&pvi->m_crTransInfo);
            PCTransInfo *ppti = (PCTransInfo*)pvi->m_lPCTransInfo.Get(dwNum);
            if (ppti && (ppti[0]->m_dwType == 2)) {
               PCTransInfo pti = ppti[0];

               // play this
               DWORD dwCur = 0;
               DWORD dwUsed;
               while (dwCur < pti->m_memVCAudio.m_dwCurPosn) {
                  PBYTE pb = (PBYTE)pti->m_memVCAudio.p + dwCur;
                  dwUsed = VoiceChatDeCompress (pb, (DWORD)pti->m_memVCAudio.m_dwCurPosn - dwCur, NULL, NULL);
                  if (!dwUsed)
                     break;   // not large enough, or not valid

                  // copy for chat
                  PCMem pCopy = new CMem;
                  if (pCopy && pCopy->Required (dwUsed)) {
                     memcpy (pCopy->p, pb, dwUsed);
                     pCopy->m_dwCurPosn = dwUsed;

                     LARGE_INTEGER iTime;
                     QueryPerformanceCounter (&iTime);
                     pvi->m_pAT->VoiceChat (pti->m_pVCNode->Clone(), pCopy, iTime, TRUE);
                        // and stuff will be deleted
                  }

                  // else, advance
                  dwCur += dwUsed;
               } // while
            }

            LeaveCriticalSection (&pvi->m_crTransInfo);
            return TRUE;
         }
         else if ((p->psz[0] == 'm') && (p->psz[1] == ':')) {
            // get the guid
            GUID g;
            MMLBinaryFromString (p->psz + 2, (PBYTE)&g, sizeof(g));

            DWORD i;
            GUID *pgl;

            EnterCriticalSection (&pvi->m_crSpeakBlacklist);
            pgl = (GUID*)pvi->m_lSpeakBlacklist.Get(0);
            for (i = 0; i < pvi->m_lSpeakBlacklist.Num(); i++, pgl++)
               if (IsEqualGUID(*pgl, g))
                  break;
            if (i < pvi->m_lSpeakBlacklist.Num())
               pvi->m_lSpeakBlacklist.Remove (i);
            else {
               // just to make sure don't get an infintely long list, delete some
               while (pvi->m_lSpeakBlacklist.Num() > 500)
                  pvi->m_lSpeakBlacklist.Remove ((DWORD)rand() % pvi->m_lSpeakBlacklist.Num());
               pvi->m_lSpeakBlacklist.Add (&g);
            }

            LeaveCriticalSection (&pvi->m_crSpeakBlacklist);
            EscChime (ESCCHIME_INFORMATION);
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSUPDATE, 0, 0);
            return TRUE;
         }
         else if ((p->psz[0] == L'r') && (p->psz[1] == L'w') && (p->psz[2] == L':')) {
            PCMem pMemNew = new CMem;
            if (!pMemNew)
               return TRUE;
            MemZero (pMemNew);
            MemCat (pMemNew, p->psz + 3);
            PostMessage (pvi->m_hWndPrimary, WM_RECORDDIR, 0, (LPARAM) pMemNew);
            return TRUE;
         }
         else if (p->psz[0] != L'[') {
            // simulate a command
            pvi->SendTextCommand (pvi->m_lidCommandLast, p->psz, NULL, NULL, NULL, TRUE, TRUE, TRUE);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         if (!p->pszSubName)
            break;
         
         if (!_wcsicmp(p->pszSubName, L"IFSMALLFONT") || !_wcsicmp(p->pszSubName, L"ENDIFSMALLFONT")) {
            MemZero (&gMemTemp);

            // make up the font size
            WCHAR szFontSizeStart[64] = L"";
            WCHAR szFontSizeEnd[64] = L"";
            //PWSTR pszColor;
            int iVal = pvi->m_iTransSize;
            while (iVal > 0) {
               wcscat (szFontSizeStart, L"<big>");
               wcscat (szFontSizeEnd, L"</big>");
               iVal--;
            }
            while (iVal < 0) {
               wcscat (szFontSizeStart, L"<small>");
               wcscat (szFontSizeEnd, L"</small>");
               iVal++;
            }

            MemCat (&gMemTemp, !_wcsicmp(p->pszSubName, L"IFSMALLFONT") ? szFontSizeStart : szFontSizeEnd);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFTRANSSHOW0")) {
            MemZero (&gMemTemp);
            BOOL fClickToHide = (pvi->m_dwTransShow & 0x01) ? TRUE : FALSE;
            TransShowHeader (&gMemTemp, L"Graphics quality vs. speed, and sound", 0, fClickToHide);
            if (!fClickToHide)
               MemCat (&gMemTemp, L"<comment>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFTRANSSHOW0")) {
            MemZero (&gMemTemp);
            BOOL fClickToHide = (pvi->m_dwTransShow & 0x01) ? TRUE : FALSE;
            if (!fClickToHide)
               MemCat (&gMemTemp, L"</comment>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFTRANSSHOW1")) {
            MemZero (&gMemTemp);
            BOOL fClickToHide = (pvi->m_dwTransShow & 0x02) ? TRUE : FALSE;
            // BUGFIX - Disable header to minimize real-estate
            // TransShowHeader (&gMemTemp, L"Transcript", 1, fClickToHide);
            if (!fClickToHide)
               MemCat (&gMemTemp, L"<comment>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFTRANSSHOW1")) {
            MemZero (&gMemTemp);
            BOOL fClickToHide = (pvi->m_dwTransShow & 0x02) ? TRUE : FALSE;
            if (!fClickToHide)
               MemCat (&gMemTemp, L"</comment>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"IMAGEQUALITY")) {
            MemZero (&gMemTemp);

            // see how fast computer is
            int iQualityMono;
            iQualityMono = CPUSpeedToQualityMono (giCPUSpeed);

            // how many quality options
            DWORD dwQualOptions = min((DWORD)iQualityMono + RESQUAL_RESOLUTIONBEYONDRECOMMEND + 1, RESQUAL_QUALITYMONOMAX);
            if (!RegisterMode())
               dwQualOptions = min(dwQualOptions, RESQUAL_QUALITYMONOMAXIFNOTPAID); // if not registered, only 2
            dwQualOptions = max(dwQualOptions, 1);

            // make a string for the group
            WCHAR szGroup[256];
            wcscpy (szGroup, L"group=");
            DWORD i;
            for (i = 0; i < dwQualOptions; i++) {
               if (i)
                  wcscat (szGroup, L",");
               swprintf (szGroup + wcslen(szGroup), L"rqual%d", (int)i);
            }
            
            // create all the buttons
            for (i = 0; i < dwQualOptions; i++) {
               MemCat (&gMemTemp, L"<xRadioButton ");
               MemCat (&gMemTemp, szGroup);
               MemCat (&gMemTemp, L" name=rqual");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }


         if (!_wcsicmp(p->pszSubName, L"TUTORIAL")) {
            // make sure have something
            PWSTR pszTutorial = (PWSTR) pvi->m_memTutorial.p;
            if (!pszTutorial || !pszTutorial[0]) {
               p->pszSubString = L"";
               return TRUE;
            }

            MemZero (&gMemTemp);

            // make up the font size
            WCHAR szFontSizeStart[64] = L"";
            WCHAR szFontSizeEnd[64] = L"";
            //PWSTR pszColor;
            int iVal = pvi->m_iTransSize;
            while (iVal > 0) {
               wcscat (szFontSizeStart, L"<big>");
               wcscat (szFontSizeEnd, L"</big>");
               iVal--;
            }
            while (iVal < 0) {
               wcscat (szFontSizeStart, L"<small>");
               wcscat (szFontSizeEnd, L"</small>");
               iVal++;
            }

            MemCat (&gMemTemp, szFontSizeStart);
            MemCat (&gMemTemp, 
               L"<p><table width=100% bordercolor=#404040>"
               L"<tr><td>");
            MemCat (&gMemTemp, pszTutorial);
            MemCat (&gMemTemp, L"</td></tr></table></p>");

            MemCat (&gMemTemp, szFontSizeEnd);


            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

#ifdef UNIFIEDTRANSCRIPT
         if (!_wcsicmp(p->pszSubName, L"MENUSUB")) {
            PWSTR psz = pvi->TranscriptMenuAndDesc ();
            if (!psz)
               return FALSE;

            p->pszSubString = psz;
            return TRUE;
         } // if "MENUSUB"
#endif // UNIFIEDTRANSCRIPT
         if (!_wcsicmp(p->pszSubName, L"TRANSCRIPTCOLORBLEND")) {
            p->pszSubString =
               pvi->m_fLightBackground ?
                  L"<colorblend skipifbackground=true posn=background tcolor=#e0e0e0 bcolor=#e0e0e0/>" :
                  L"<colorblend skipifbackground=true posn=background tcolor=#000000 bcolor=#000000/>";
         }
         else if (!_wcsicmp(p->pszSubName, L"FONTCOLORSTART")) {
            p->pszSubString =
               pvi->m_fLightBackground ?
                  L"<font color=#101000>" :
                  L"<font color=#ffffff>";
         }
         else if (!_wcsicmp(p->pszSubName, L"FONTCOLOREND")) {
            p->pszSubString = L"</font>";
         }
         else if (!_wcsicmp(p->pszSubName, L"SPOKENSUB")) {
            MemZero (&gMemTemp);

            EnterCriticalSection (&pvi->m_crSpeakBlacklist);
            EnterCriticalSection (&pvi->m_crTransInfo);

            // make up the font size
            WCHAR szFontSizeStart[64] = L"";
            WCHAR szFontSizeEnd[64] = L"";
            WCHAR szStartColor[64], szEndColor[64], szTemp[64];
            //PWSTR pszColor;
            int iVal = pvi->m_iTransSize;
            while (iVal > 0) {
               wcscat (szFontSizeStart, L"<big>");
               wcscat (szFontSizeEnd, L"</big>");
               iVal--;
            }
            while (iVal < 0) {
               wcscat (szFontSizeStart, L"<small>");
               wcscat (szFontSizeEnd, L"</small>");
               iVal++;
            }


#ifdef UNIFIEDTRANSCRIPT
            MemCat (&gMemTemp, szFontSizeStart);

            // repeat while have items
            DWORD dwStart, i;
            PCTransInfo *ppti = (PCTransInfo*)pvi->m_lPCTransInfo.Get(0);
            for (dwStart = 0; dwStart < pvi->m_lPCTransInfo.Num(); dwStart++) {
               // find the end of the group
               PCTransInfo ptiS = ppti[dwStart];

               // figure out the color
               GUID *pg = &ptiS->m_gID;
               if (IsEqualGUID(*pg, CLSID_PlayerAction)) {
                  // pszColor = L"#000040";

                  if (pvi->m_fLightBackground)
                     wcscpy (szStartColor, L"<font color=#004000><italic>&tab;");
                  else
                     // BUGFIX - Was too green
                     wcscpy (szStartColor, L"<font color=#c0ffc0><italic>&tab;");
                     //wcscpy (szStartColor, L"<font color=#80ff80><italic>&tab;");
                  wcscpy (szEndColor, L"</italic></font>");
               }
               else
                  szStartColor[0] = szEndColor[0] = 0;

               MemCat (&gMemTemp, L"<p>");
               if (szStartColor[0])
                  MemCat (&gMemTemp, szStartColor);

               // display the character's name
               PWSTR pszName = (PWSTR)ptiS->m_memObjectName.p;
               PWSTR pszLang = (PWSTR)ptiS->m_memLang.p;
               BOOL fSpeaker = FALSE;
               if (pszName && pszName[0] && _wcsicmp(pszName, gpszNarrator) && !IsEqualGUID(*pg, GUID_NULL) && !IsEqualGUID(*pg, CLSID_PlayerAction)) {
                  fSpeaker = TRUE;

                  // name
                  MemCat (&gMemTemp, L"<bold>");
                  MemCatSanitize (&gMemTemp, (PWSTR)ptiS->m_memObjectName.p);
                  MemCat (&gMemTemp, L"</bold>");

                  // colon
                  if (pvi->m_fLightBackground)
                     MemCat (&gMemTemp, L": \"<font color=#400000>");
                  else
                     // BUGFIX - Was too red
                     MemCat (&gMemTemp, L": \"<font color=#ffc0c0>");
                     // MemCat (&gMemTemp, L": \"<font color=#ff8080>");
               }

               if (ptiS->m_dwType == 2) {
                  MemCat (&gMemTemp, L"<image bmpresource=");
                  MemCat (&gMemTemp, (int)IDB_SPEAKER);
                  MemCat (&gMemTemp, L" transparent=true href=sk:");
                  MemCat (&gMemTemp, (int)dwStart);   // BUGFIX - was i
                  MemCat (&gMemTemp, L"/>");
               }
               else if (ptiS->m_dwType == 1) {
                  // BUGFIX - make slightly lighter grey since not acutally spoken
                  if (pvi->m_fLightBackground)
                     MemCat (&gMemTemp, L"<font color=#404040>");
                  else
                     MemCat (&gMemTemp, L"<font color=#c0c0c0>"); // BUGFIX - Was 808080, but too dark to see

                  MemCat (&gMemTemp,  (PWSTR)ptiS->m_memString.p);

                  MemCat (&gMemTemp, L"</font>");
               }
               else {
                  MemCatSanitize (&gMemTemp, (PWSTR)ptiS->m_memString.p);
                  
                  // option to record
                  if (pvi->m_szRecordDir[0]) {
                     MemCat (&gMemTemp, L" <a color=#ff0000 href=\"rw:");
                     MemCatSanitize (&gMemTemp, (PWSTR)ptiS->m_memString.p);
                     MemCat (&gMemTemp, L"\">(record)</a>");
                  }
               }

               if (fSpeaker) {
                  MemCat (&gMemTemp, L"</font>\"");

                  // language
                  if (pszLang[0]) {
                     if (pvi->m_fLightBackground)
                        MemCat (&gMemTemp, L"<font color=#e0e0e0><italic> (in ");
                     else
                        MemCat (&gMemTemp, L"<font color=#808080><italic> (in ");   // BUGFIX - Was 404040, but too dark to see
                     MemCatSanitize (&gMemTemp, pszLang);
                     MemCat (&gMemTemp, L")</italic></font>");
                  }

               }

               // option to mute
               if (!pvi->m_fMuteAll && (ptiS->m_dwType != 1) && !IsEqualGUID(*pg, CLSID_PlayerAction) && !IsEqualGUID(*pg, GUID_NULL) ) {
                  MemCat (&gMemTemp, L"<a href=m:");
                  MMLBinaryToString ((PBYTE) pg, sizeof(*pg), szTemp);
                  MemCat (&gMemTemp, szTemp);
                  if (pvi->m_fLightBackground)
                     MemCat (&gMemTemp, L" color=#e0e0ff");
                  else
                     MemCat (&gMemTemp, L" color=#303070");

                  // see if it's muted
                  GUID *pgl = (GUID*) pvi->m_lSpeakBlacklist.Get(0);
                  for (i = 0; i < pvi->m_lSpeakBlacklist.Num(); i++, pgl++)
                     if (IsEqualGUID(*pgl, *pg))
                        break;
                  if (i < pvi->m_lSpeakBlacklist.Num())
                     MemCat (&gMemTemp, L"><xHoverHelpShort>Click to un-mute</xHoverHelpShort><italic> (un-mute)</italic>");
                  else
                     MemCat (&gMemTemp, L"><xHoverHelpShort>Click to mute</xHoverHelpShort><italic> (mute)</italic>");

                  MemCat (&gMemTemp, L"</a>");
               }

               if (szEndColor[0])
                  MemCat (&gMemTemp, szEndColor);
               MemCat (&gMemTemp, L"</p>");
            } // while
         MemCat (&gMemTemp, szFontSizeEnd);

#else // UNIFIEDTRANSCRIPT
            // repeat while have items
            DWORD dwStart = 0, dwEnd, i;
            PWSTR psz;
            PCTransInfo *ppti = (PCTransInfo*)pvi->m_lPCTransInfo.Get(0);
            while (dwStart < pvi->m_lPCTransInfo.Num()) {
               // find the end of the group
               PCTransInfo ptiS = ppti[dwStart];
               PCTransInfo ptiE;
               for (dwEnd = dwStart + 1; dwEnd < pvi->m_lPCTransInfo.Num(); dwEnd++) {
                  ptiE = ppti[dwEnd];
                  if (!IsEqualGUID (ptiS->m_gID, ptiE->m_gID))
                     break;
                  if (_wcsicmp((PWSTR)ptiS->m_memLang.p, (PWSTR)ptiE->m_memLang.p))
                     break;
                  if (_wcsicmp((PWSTR)ptiS->m_memObjectName.p, (PWSTR)ptiE->m_memObjectName.p))
                     break;
               } // to find end

               // figure out the color
               GUID *pg = &ptiS->m_gID;
               if (IsEqualGUID(*pg, CLSID_PlayerAction)) {
                  pszColor = L"#808080";

                  swprintf (szStartColor, L"<font color=%s><italic>", pszColor);
                  wcscpy (szEndColor, L"</italic></font>");
               }
               else if (IsEqualGUID(*pg, GUID_NULL)) {
                  pszColor = L"#c0c0c0";

                  swprintf (szStartColor, L"<font color=%s><italic>", pszColor);
                  wcscpy (szEndColor, L"</italic></font>");
               }
               else {
                  PBYTE pb = (PBYTE) pg;
                  DWORD dwSum = 0;
                  PWSTR asz[12] = {
                     L"#ffff00", L"#00ffff",L"#ff00ff",
                     L"#ff0000", L"#00ff00",L"#0000ff",
                     L"#c0c040", L"#40c0c0",L"#c040c0",
                     L"#c04040", L"#40c040",L"#4040c0"
                  };
                  for (i = 0; i < sizeof(*pg); i++)
                     dwSum += (DWORD)pb[i];

                  pszColor = asz[dwSum % 12];

                  swprintf (szStartColor, L"<font color=%s>", pszColor);
                  wcscpy (szEndColor, L"</font>");
               }

               // create the user name
               MemCat (&gMemTemp,
                  L"<tr>"
                  L"<td ");
               if (dwEnd >= pvi->m_lPCTransInfo.Num())
                  MemCat (&gMemTemp, L"valign=bottom "); // so last entry is visible
               MemCat (&gMemTemp, L"width=20%%><small>");
               MemCat (&gMemTemp, szStartColor);
               if (szFontSizeStart[0])
                  MemCat (&gMemTemp, szFontSizeStart);

               BOOL fMuted = FALSE;
               if (!IsEqualGUID(*pg, CLSID_PlayerAction)) {
                  MemCat (&gMemTemp, L"<a href=m:");
                  MMLBinaryToString ((PBYTE) pg, sizeof(*pg), szTemp);
                  MemCat (&gMemTemp, szTemp);
                  MemCat (&gMemTemp, L" color=");
                  MemCat (&gMemTemp, pszColor);

                  // see if it's muted
                  GUID *pgl = (GUID*) pvi->m_lSpeakBlacklist.Get(0);
                  for (i = 0; i < pvi->m_lSpeakBlacklist.Num(); i++, pgl++)
                     if (IsEqualGUID(*pgl, *pg))
                        break;
                  if (i < pvi->m_lSpeakBlacklist.Num()) {
                     MemCat (&gMemTemp, L"><xHoverHelpShort>Click to un-mute</xHoverHelpShort>");
                     fMuted = TRUE;
                  }
                  else
                     MemCat (&gMemTemp, L"><xHoverHelpShort>Click to mute</xHoverHelpShort>");
               }
               MemCatSanitize (&gMemTemp, (PWSTR)ptiS->m_memObjectName.p);
               if (!IsEqualGUID(*pg, CLSID_PlayerAction)) {
                  MemCat (&gMemTemp, L"</a>");
               }

               // if muted indicate
               if (fMuted)
                  MemCat (&gMemTemp, L"<br/><italic>(Muted)</italic>");

               // show language spoken
               psz = (PWSTR)ptiS->m_memLang.p;
               if (psz[0]) {
                  MemCat (&gMemTemp, L"<br/><italic>(");
                  MemCatSanitize (&gMemTemp, psz);
                  MemCat (&gMemTemp, L")</italic>");
               }

               if (szFontSizeEnd[0])
                  MemCat (&gMemTemp, szFontSizeEnd);
               MemCat (&gMemTemp, szEndColor);
               MemCat (&gMemTemp,
                  L"</small></td>"
                  L"<td width=80%%>");
               MemCat (&gMemTemp, szStartColor);
               if (szFontSizeStart[0])
                  MemCat (&gMemTemp, szFontSizeStart);
               
               // put in all the strings
               for (i = dwStart; i < dwEnd; i++) {
                  ptiE = ppti[i];
                  if (i != dwStart)
                     MemCat (&gMemTemp, L"<br/><align align=center><hr width=50% color=#404040/></align><br/>");
                     // a slight seperator between lines

                  if (ptiE->m_dwType == 2) {
                     MemCat (&gMemTemp, L"<image bmpresource=");
                     MemCat (&gMemTemp, (int)IDB_SPEAKER);
                     MemCat (&gMemTemp, L" transparent=true href=sk:");
                     MemCat (&gMemTemp, (int)i);
                     MemCat (&gMemTemp, L"/>");
                  }
                  else if (ptiE->m_dwType == 1)
                     MemCat (&gMemTemp,  (PWSTR)ptiE->m_memString.p);
                  else {
                     MemCatSanitize (&gMemTemp, (PWSTR)ptiE->m_memString.p);
                     
                     // option to record
                     if (pvi->m_szRecordDir[0]) {
                        MemCat (&gMemTemp, L" <a color=#ff0000 href=\"rw:");
                        MemCatSanitize (&gMemTemp, (PWSTR)ptiE->m_memString.p);
                        MemCat (&gMemTemp, L"\">(record)</a>");
                     }
                  }
               } // i

               if (szFontSizeEnd[0])
                  MemCat (&gMemTemp, szFontSizeEnd);
               MemCat (&gMemTemp, szEndColor);
               MemCat (&gMemTemp,
                  L"</td>"
                  L"</tr>");

               dwStart = dwEnd;
            } // while
#endif // UNIFIEDTRANSCRIPT

            LeaveCriticalSection (&pvi->m_crTransInfo);

            LeaveCriticalSection (&pvi->m_crSpeakBlacklist);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   }


   return FALSE;
}



/*************************************************************************************
CMainWindow::TranscriptUpdate - Updates the transcript window using the current
display information
*/
void CMainWindow::TranscriptUpdate (void)
{
   if (!IsWindowVisible (m_hWndTranscript))
      return;  // dont bother since not visible

   // if have existing window then make sure to stop the page
   if (m_pTranscriptWindow)
      m_pTranscriptWindow->PageClose ();
   else {
      // create the new page
      m_pTranscriptWindow = new CEscWindow;
      if (!m_pTranscriptWindow)
         return;  // error

      RECT rClient;
      GetClientRect (m_hWndTranscript, &rClient);
      if (!m_pTranscriptWindow->Init (ghInstance, m_hWndTranscript,
         EWS_NOTITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_FIXEDHEIGHT | EWS_FIXEDWIDTH | EWS_CHILDWINDOW | EWS_CHILDWINDOWHASNOBORDER,
         &rClient)) {
            delete m_pTranscriptWindow;
            m_pTranscriptWindow = NULL;
            return;
         }

      EscWindowFontScaleByScreenSize (m_pTranscriptWindow);

      // update the background
      TranscriptBackground (FALSE);
   }
   
   m_pTranscriptWindow->PageDisplay (ghInstance, IDR_MMLTRANSCRIPT, TranscriptPage, this);
}


/*************************************************************************************
CMainWindow::TranscriptString - String is added to the transcript.

inputs
   DWORD       dwType - 0 for normal string, 1 for MML, 2 for audio snippet
   PWSTR       pszActor - Actor doing the speaking. This can be NULL
   GUID        *pgActor - Actor's GUID. This can be NULL
   PWSTR       pszLanguage - Language being spoken. This can be NULL.
   PWSTR       pszSpeak - What's spoken. This can be NULL
   PCMMLNode2  pVCNode - Voice chat node. This is COPIED. Can be NULL
   PCMem       pVCAudio - Compressed audio. This is COPIED. Can be NULL.
returns
   none
*/
void CMainWindow::TranscriptString (DWORD dwType, PWSTR pszActor, GUID *pgActor, PWSTR pszLanguage, PWSTR pszSpeak,
                                    PCMMLNode2 pVCNode, PCMem pVCAudio)
{
   // if this is text chat, then absorb "." by itself since used to force
   // loading of tts
   if ((dwType == 0) && pszSpeak && !_wcsicmp(pszSpeak, L"."))
      return;

   PWSTR pszNull = L"";
   GUID gNULL = GUID_NULL;
   GUID gTemp;
   EnterCriticalSection (&m_crTransInfo);

   // eliminiate if too many strings
   while (m_lPCTransInfo.Num() > 200) {
      PCTransInfo *ppti = (PCTransInfo*)m_lPCTransInfo.Get(0);
      delete ppti[0];
      m_lPCTransInfo.Remove (0);
   }

   // if this is voice chat then potentiall fill values from MML
   if ((dwType == 2) && pVCNode) {
      if (!pszActor)
         pszActor = MMLValueGet (pVCNode, L"name");
      if (!pgActor) {
         if (sizeof(gTemp) == MMLValueGetBinary (pVCNode, L"ID", (PBYTE)&gTemp, sizeof(gTemp)))
            pgActor = &gTemp;
      if (!pszLanguage)
         pszLanguage = MMLValueGet (pVCNode, L"language");
      }
   }


   // if this is voice chat, then see if there's already audio from the
   // same speaker. If so, then append. Even look back a few entries in
   // case several people talking over one another
   if ((dwType == 2) && pgActor && pVCAudio) {
      DWORD dwNum = m_lPCTransInfo.Num();

      // look at the last 5 (or so) entries and see if can find the same object and type
      int iLook;
      DWORD dwNow = GetTickCount();
      DWORD dwRecent = dwNow - 10000;   // if has been updated in the last 5 seconds then add to
      DWORD dwNotTooLong = dwNow - 30000;  // no more than 30 seconds in one lump
      PCTransInfo *ppti = (PCTransInfo*)m_lPCTransInfo.Get(0);
      for (iLook = (int)dwNum-1; (iLook >= 0) && (iLook >= (int)dwNum-5); iLook--) {
         if (ppti[iLook]->m_dwType != dwType || !IsEqualGUID(ppti[iLook]->m_gID, *pgActor))
            continue;

         // if get here, may have found a match with object. look out for time
         if (ppti[iLook]->m_dwTimeLast < dwRecent)
            continue;   // happened too long ago

         // make sure not too long
         if (ppti[iLook]->m_dwTimeStart < dwNotTooLong)
            continue;   // too long, so break up

         // make sure enough memory
         if (!ppti[iLook]->m_memVCAudio.Required (ppti[iLook]->m_memVCAudio.m_dwCurPosn + pVCAudio->m_dwCurPosn))
            return; // error

         // append new stuff
         memcpy ((PBYTE)ppti[iLook]->m_memVCAudio.p + ppti[iLook]->m_memVCAudio.m_dwCurPosn,
            pVCAudio->p, pVCAudio->m_dwCurPosn);
         ppti[iLook]->m_memVCAudio.m_dwCurPosn += pVCAudio->m_dwCurPosn;
         ppti[iLook]->m_dwTimeLast = dwNow;

         goto done;
      } // iLook

      // else, fall through and add
   }

   // fill in NULLs
   if (!pszActor)
      pszActor = pszNull;
   if (!pgActor)
      pgActor = &gNULL;
   if (!pszLanguage)
      pszLanguage = pszNull;
   if ((dwType != 2) && (!pszSpeak || !pszSpeak[0])) {
      LeaveCriticalSection (&m_crTransInfo);
      return;  // nothing to speak
   }

   // add this
   PCTransInfo pti = new CTransInfo;
   if (!pti) {
      LeaveCriticalSection (&m_crTransInfo);
      return;  // error
   }
   pti->m_dwType = dwType;
   pti->m_gID = *pgActor;
   MemCat (&pti->m_memObjectName, pszActor);
   if (pszSpeak)
      MemCat (&pti->m_memString, pszSpeak);
   MemCat (&pti->m_memLang, pszLanguage);
   if (pVCNode)
      pti->m_pVCNode = pVCNode->Clone();
   if (pVCAudio && pti->m_memVCAudio.Required(pVCAudio->m_dwCurPosn)) {
      memcpy (pti->m_memVCAudio.p, pVCAudio->p, pVCAudio->m_dwCurPosn);
      pti->m_memVCAudio.m_dwCurPosn = pVCAudio->m_dwCurPosn;
   }
   m_lPCTransInfo.Add (&pti);

done:
   LeaveCriticalSection (&m_crTransInfo);

   // update the transcript
   PostMessage (m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSUPDATE, 0, 0);
   // TranscriptUpdate (); - DOnt call here because not thread safe
}



/*************************************************************************************
CMainWindow::ChildOnMonitor - Returns the monitor number that the child is on.

inputs
   HWND        hWNd - Window to check
returns
   DWORD - 0 for primary, 1 for secondary
*/
DWORD CMainWindow::ChildOnMonitor (HWND hWnd)
{
   // if only one monitor then always true
   if (!m_hWndSecond)
      return 0;

   return (GetParent(hWnd) == m_hWndSecond) ? 1 : 0;
}


/*************************************************************************************
CMainWindow::TrapWM_MOVING - Called by child windows to ensure that the aren't moved
above the tabs. Can also call for WM_SIZING

inputs
   HWND        hWnd - WIndow being moved
   LPARAM      lParam - lParam sent
   BOOL        fSizing - Set to TRUE if this is a WM_SIZING call
returns
   LRESULT - Returns TRUE if modified
*/
LRESULT CMainWindow::TrapWM_MOVING (HWND hWnd, LPARAM lParam, BOOL fSizing)
{
   DWORD dwMonitor = ChildOnMonitor (hWnd);
   HWND hWndMain = dwMonitor ? m_hWndSecond : m_hWndPrimary;

   RECT *pr = (RECT*)lParam;
   RECT rClient;
   rClient = *pr;
   ScreenToClient (hWndMain, ((POINT*)&rClient) + 0);
   ScreenToClient (hWndMain, ((POINT*)&rClient) + 1);

   RECT r;
   ClientRectGet (dwMonitor, &r);

   BOOL fChanged = FALSE;
   int iDelta;

   // dont allow to move outside of client area

   if ((iDelta = r.left - rClient.left) > 0) {
      if (fSizing) {
         pr->left += iDelta;
         rClient.left += iDelta;
      }
      else {
         OffsetRect (pr, iDelta, 0);
         OffsetRect (&rClient, iDelta, 0);
      }
      fChanged = TRUE;
   }
   if ((iDelta = rClient.bottom - r.bottom) > 0) {
      if (fSizing) {
         pr->bottom -= iDelta;
         rClient.bottom -= iDelta;
      }
      else {
         OffsetRect (pr, 0, -iDelta);
         OffsetRect (&rClient, 0, -iDelta);
      }
      fChanged = TRUE;
   }
   if ((iDelta = rClient.right - r.right) > 0) {
      if (fSizing) {
         pr->right -= iDelta;
         rClient.right -= iDelta;
      }
      else {
         OffsetRect (pr, -iDelta, 0);
         OffsetRect (&rClient, -iDelta, 0);
      }
      fChanged = TRUE;
   }

   // move this last so can never cover up tabs
   if ((iDelta = r.top - rClient.top) > 0) {
      if (fSizing) {
         pr->top += iDelta;
         rClient.top += iDelta;
      }
      else {
         OffsetRect (pr, 0, iDelta);
         OffsetRect (&rClient, 0, iDelta);
      }
      fChanged = TRUE;
   }
   return fChanged;
}



/*************************************************************************************
CMainWindow::ChildLocTABWINDOW - Returns the TABWINDOW for the child location.

inptus
   DWORD          dwType - Type information
   PWSTR          psz - String to use (only for some windows). Can be NULL for the otehrs
returns
   PTABWINDOW - Tab window
*/
PTABWINDOW CMainWindow::ChildLocTABWINDOW (DWORD dwType, PWSTR psz)
{
   // find a match?
   DWORD dwTabIndex = m_pSlideTop->m_dwTab + (gfMonitorUseSecond ? NUMTABS : 0);
   PTABWINDOW ptwLook = (PTABWINDOW) m_alTABWINDOW[dwTabIndex].Get(0);
   DWORD i;
   for (i = 0; i < m_alTABWINDOW[dwTabIndex].Num(); i++, ptwLook++) {
      if (ptwLook->dwType != dwType)
         continue;   // different fundamental types

      // if use string then compare
      if ((dwType < 0x100) && _wcsicmp((PWSTR)m_alTabWindowString[dwTabIndex].Get(i), psz))
         continue;   // different strings

      // else, match
      return ptwLook;
   } // i

   return NULL;
}


/*************************************************************************************
CMainWindow::ChildLocSave - Saves the location of a child window in m_alTABWINDOW.

inputs
   PTABWINDOW        ptw - Window information to save
   PWSTR             psz - String to use (only for some windows). Can be NULL
                           for window types that dont care about the name
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::ChildLocSave (PTABWINDOW ptw, PWSTR psz)
{
   if (gfChildLocIgnoreSave)
      return TRUE;   // just ignoring

   // find a match?
   DWORD dwTabIndex = m_pSlideTop->m_dwTab + (gfMonitorUseSecond ? NUMTABS : 0);
   PTABWINDOW ptwLook = ChildLocTABWINDOW (ptw->dwType, psz);
   if (ptwLook) {
      // else, match
      *ptwLook = *ptw;
      return TRUE;
   } // i

   // else, need to add
   m_alTABWINDOW[dwTabIndex].Add (ptw);
   if (!psz)
      psz = L"";
   m_alTabWindowString[dwTabIndex].Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   return TRUE;

}

/*************************************************************************************
CMainWindow::ChildHasTitle - Returns true if a child window has a title bar

inputs
   HWND           hChild - To Check out
returns
   BOOL - TRUE if has title bar
*/
BOOL CMainWindow::ChildHasTitle (HWND hChild)
{
   return (GetWindowLong (hChild, GWL_STYLE) & WS_CAPTION) ? TRUE : FALSE;
}

/*************************************************************************************
CMainWindow::ChildLocSave - Saves the location of a child window in m_alTABWINDOW.

inputs
   HWND           hChild - Child window
   DWORD          dwType - Child window type, TW_XXX
   PWSTR          psz - String that identifies case of specific type. Can be
                        NULL for those types that don't need
   BOOL           *pfHidden - If NULL then use the IsWindowVisible() call.
                        If not NULL, then use this value.
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::ChildLocSave (HWND hChild, DWORD dwType, PWSTR psz, BOOL *pfHidden)
{
   if (gfChildLocIgnoreSave)
      return TRUE;   // just ignoring

   TABWINDOW tw;
   memset (&tw, 0, sizeof(tw));

   tw.dwType = dwType;
   tw.fVisible = pfHidden ? !(*pfHidden) : IsWindowVisible (hChild);
   tw.fTitle = ChildHasTitle (hChild);
   tw.dwMonitor = ChildOnMonitor (hChild);

   // find the location
   RECT rIcon, rMain;
   DWORD dwMonitor = tw.dwMonitor;
   HWND hWndMain = dwMonitor ? m_hWndSecond : m_hWndPrimary;
   ClientRectGet (dwMonitor, &rMain);
   GetWindowRect (hChild, &rIcon);
   ScreenToClient (hWndMain, ((POINT*)&rIcon) + 0);
   ScreenToClient (hWndMain, ((POINT*)&rIcon) + 1);
   OffsetRect (&rIcon, -rMain.left, -rMain.top);
   OffsetRect (&rMain, -rMain.left, -rMain.top);
   rMain.right = max(rMain.right, 1);
   rMain.bottom = max(rMain.bottom, 1);

   CMMLNode2 node;
   int iWidth = rMain.right - rMain.left;
   int iHeight = rMain.bottom - rMain.top;
   iWidth = max(iWidth, 1);
   iHeight = max(iHeight, 1);
   tw.pLoc.p[0] = (fp)(rIcon.left - rMain.left) / (fp) iWidth;
   tw.pLoc.p[1] = (fp)(rIcon.right - rMain.left) / (fp) iWidth;
   tw.pLoc.p[2] = (fp)(rIcon.top - rMain.top) / (fp) iHeight;
   tw.pLoc.p[3] = (fp)(rIcon.bottom - rMain.top) / (fp) iHeight;
   tw.dwMonitor = dwMonitor;

   return ChildLocSave (&tw, psz);
}


// location info for childlocget
typedef struct {
   fp       fLeft;      // left, from 0..1. If > 1.0, then on second monitor
   fp       fTop;
   fp       fRight;
   fp       fBottom;
   BOOL     fTitle;     // if TRUE then show title
   BOOL     fVisible;    // if TRUE then hidden
} CLGINFO, *PCLGINFO;

#define EXPLOREVERTDIVIDE           0.76 // 0.666 // BUGFIX - Was 0.75
#define EXPLOREVERTDIVIDEMUTE       0.68   // where the divide is for the mute
#define EXPLOREVERTDIVIDEHALF       (EXPLOREVERTDIVIDE/2)
#define EXPLOREVERTDIVIDEMUTEHALF   (EXPLOREVERTDIVIDEMUTE/2)
#define COMBATVERTDIVIDESINGLE      0.85
#define COMBATVERTDIVIDEDUAL        0.75

// #define EXPLOREHORZPARTITIONTOP           0.25        // horizontal partition line at the top
// #define EXPLOREHORZPARTITIONBOTTOM        (1.0 - EXPLOREHORZPARTITIONTOP)  // horizontal partition line at the bottom
// #define EXPLORE_MAINDISPLAYBOTTOM      0.66   // BUGFIX - was 0.75, but made larger because no toobar

// #define OLDEXPLOREVERTDIVIDE        0.2     // where L/R icons are shown   BUGFIX - Was .15
#define EXPLOREHORZDIVIDE           (.567)     // where horizontal divide is. BUGFIX - Was .33
#define EXPLOREHORZDIVIDECOMBAT        0.8     // where horizontal divide is. BUGFIX - Was .33
#define EXPLOREHORZDIVIDECHAT        (EXPLOREHORZDIVIDE) // BUGFIX - Was 0.666     // where horizontal divide is. BUGFIX - Was .33

// #define EXPLORE2HORZDIVIDE       0.75     // horizontal divide line for second version

#define LAYOUT_DEFAULT_TITLEVISIBLE   0.25, 0.25, 0.75, 0.75, TRUE, TRUE
#define LAYOUT_DEFAULT_TITLEHIDDEN    0.25, 0.25, 0.75, 0.75, TRUE, FALSE
#define LAYOUT_DEFUALT_NOTITLEHIDDEN  0.25, 0.25, 0.75, 0.75, FALSE, FALSE

#define LRMARGIN_WIDE               0.1         // size of LR margin on 16:9 screen
#define LRMARGIN_NARROW             0.05         // size of LR margin on 4:3 screen
#define TBMARGIN                    0.05         // size of TB margin


static CLGINFO gclgSingle[10][NUMTABS] = {   // single monitor, no mute all
   /* 0 - default, dwtype < 0x100 */
      LAYOUT_DEFAULT_TITLEHIDDEN,   // explore   // BUGFIX - Set TRUE for title
      LAYOUT_DEFAULT_TITLEHIDDEN,   // chat
      LAYOUT_DEFAULT_TITLEHIDDEN,   // combat
      LAYOUT_DEFAULT_TITLEHIDDEN,   // misc

   /* 1 - default, dwtype >= 0x100 */
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,

   // 2 - transcript
      EXPLOREVERTDIVIDE, 0.0, 1.0, 1.0, FALSE, TRUE,
      EXPLOREVERTDIVIDE, 0.0, 1.0, 0.5, FALSE, TRUE,
      0.5, 0.0, COMBATVERTDIVIDESINGLE, EXPLOREHORZDIVIDECOMBAT, FALSE, TRUE,
      EXPLOREVERTDIVIDE, 0.0, 1.0, EXPLOREHORZDIVIDE, FALSE, TRUE,

   // 3 - map
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,

   // 4 - main
      0.0, 0.0, EXPLOREVERTDIVIDE, EXPLOREHORZDIVIDE, FALSE, TRUE,
      0.0, 0.0, EXPLOREVERTDIVIDE, EXPLOREHORZDIVIDECHAT, FALSE, TRUE,
      0.5, EXPLOREHORZDIVIDECOMBAT, COMBATVERTDIVIDESINGLE, 1.0, FALSE, TRUE,
      0.0, 0.0, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,

   // 5 - tempdisp
      LAYOUT_DEFAULT_TITLEHIDDEN,
      EXPLOREVERTDIVIDE, 0.5, 1.0, 1.0, FALSE, TRUE,
      1 - COMBATVERTDIVIDESINGLE, 0.0, 0.5, EXPLOREHORZDIVIDECOMBAT, FALSE, TRUE,
      LAYOUT_DEFAULT_TITLEHIDDEN,

   // 6 - statusself
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      0.0, 0.0, 1.0 - COMBATVERTDIVIDESINGLE, EXPLOREHORZDIVIDECOMBAT, FALSE, TRUE,
      LAYOUT_DEFAULT_TITLEHIDDEN,

   // 7 - statusenemy
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      COMBATVERTDIVIDESINGLE, 0.0, 1.0, EXPLOREHORZDIVIDECOMBAT, FALSE, TRUE,
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,

   // 8 - helptip
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,

   // 9 - main iconwindow
      0.0, EXPLOREHORZDIVIDE, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,
      0.0, EXPLOREHORZDIVIDECHAT, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,
      0.0, EXPLOREHORZDIVIDECOMBAT, 0.5 /* 1.0 - COMBATVERTDIVIDESINGLE*/, 1.0, FALSE, TRUE,
      EXPLOREVERTDIVIDE, EXPLOREHORZDIVIDE, 1.0, 1.0, FALSE, TRUE,
   
#if 0 // no more chat
   // 10 - chat iconwindow
      EXPLOREVERTDIVIDE, 0.75, 1.0, 1.0, FALSE, TRUE,
      0.0, .666, 1.0, 1.0, FALSE, TRUE,
      1.0 - COMBATVERTDIVIDESINGLE, 0.75, 0.5, 1.0, FALSE, TRUE,
      0.0, 0.75, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE
      //0.0, EXPLORE2HORZDIVIDE, 0.5, 1.0, FALSE, TRUE,
      // 0.0, 0.0, OLDEXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,
      //0.0, 4.0 / 6.0, VERTDIVIDE, 1.0, FALSE, TRUE,
      //0.0, EXPLOREHORZDIVIDE, 0.33, 1.0, FALSE, TRUE,
      //0.0, 0.66, VERTDIVIDE, 1.0, FALSE, TRUE,
      //1.0 - COMBATVERTDIVIDESINGLE, 0.75, 0.5, 1.0, FALSE, TRUE,
      //0.0, EXPLOREHORZDIVIDE, 0.33, 1.0, FALSE, TRUE
#endif // 0
};

static CLGINFO gclgDual[10][NUMTABS] = { // dual monitor, !m_fMuteALl
   /* 0 - default, dwtype < 0x100 */
      LAYOUT_DEFAULT_TITLEHIDDEN,   // BUGFIX - Set TRUE for title
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,

   /* 1 - default, dwtype >= 0x100 */
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,

   // 2 - transcript
      EXPLOREVERTDIVIDE, 0.0, 1.0, 1.0, FALSE, TRUE,
      EXPLOREVERTDIVIDE, 0.0, 1.0, 1.0, FALSE, TRUE,
      1.0 - COMBATVERTDIVIDEDUAL, 0.5, COMBATVERTDIVIDEDUAL, 1.0, FALSE, TRUE,
      EXPLOREVERTDIVIDE, 0.0, 1.0, EXPLOREHORZDIVIDE, FALSE, TRUE,

   // 3 - map
      1.0 + EXPLOREVERTDIVIDE, 0.0, 2.0, 1.0, FALSE, TRUE,
      1.0 + EXPLOREVERTDIVIDE, 0.0, 2.0, 1.0, FALSE, TRUE,
      1.0 + EXPLOREVERTDIVIDE, 0.0, 2.0, 1.0, FALSE, TRUE,
      1.0 + EXPLOREVERTDIVIDE, 0.0, 2.0, 1.0, FALSE, TRUE,

   // 4 - main
      0.0, 0.0, EXPLOREVERTDIVIDE, EXPLOREHORZDIVIDE, FALSE, TRUE,
      0.0, 0.0, EXPLOREVERTDIVIDE, EXPLOREHORZDIVIDECHAT, FALSE, TRUE,
      1.0, 0.0, 2.0, EXPLOREHORZDIVIDECOMBAT, FALSE, TRUE,
      0.0, 0.0, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,

   // 5 - tempdisp
      1.0, 0.0, 1.0 + EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,
      1.0, 0.0, 1.0 + EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,
      1 - COMBATVERTDIVIDEDUAL, 0.0, COMBATVERTDIVIDEDUAL, 0.5, FALSE, TRUE,
      1.0, 0.0, 1.0 + EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,

   // 6 - statusself
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      0.0, 0.0, 1.0 - COMBATVERTDIVIDEDUAL, 1.0, FALSE, TRUE,
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,

   // 7 - statusenemy
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      COMBATVERTDIVIDEDUAL, 0.0, 1.0, 1.0, FALSE, TRUE,
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,

   // 8 - helptip
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
   
   // 9 - main iconwindow
      0.0, EXPLOREHORZDIVIDE, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,
      0.0, EXPLOREHORZDIVIDECHAT, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,
      1.0, EXPLOREHORZDIVIDECOMBAT, 2.0, 1.0, FALSE, TRUE,
      EXPLOREVERTDIVIDE, EXPLOREHORZDIVIDE, 1.0, 1.0, FALSE, TRUE,
   
#if 0 // no more chat
   // 10 - chat iconwindow
      0.0, EXPLOREHORZDIVIDE, 0.5, 1.0, FALSE, TRUE,
      //0.0, EXPLORE2HORZDIVIDE, 0.5, 1.0, FALSE, TRUE,
      // 0.0, 0.0, OLDEXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,
      0.0, .75, 1.0, 1.0, FALSE, TRUE,
      //0.0, 4.0 / 6.0, 1.0, 1.0, FALSE, TRUE,
      1.0, EXPLOREHORZDIVIDE, 1.5, 1.0, FALSE, TRUE,
      0.0, 0.75, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE
      //0.0, EXPLOREHORZDIVIDE, 0.5, 1.0, FALSE, TRUE,
      //0.0, 0.66, 1.0, 1.0, FALSE, TRUE,
      //1.0, EXPLOREHORZDIVIDE, 1.5, 1.0, FALSE, TRUE,
      //0.0, EXPLOREHORZDIVIDE, 0.5, 1.0, FALSE, TRUE
#endif // 0
};

#if 0 // not used anymore
static CLGINFO gclgSingleMute[10][NUMTABS] = {   // single monitor, mute all
   /* 0 - default, dwtype < 0x100 */
      LAYOUT_DEFAULT_TITLEHIDDEN,   // explore   // BUGFIX - Set TRUE for title
      LAYOUT_DEFAULT_TITLEHIDDEN,   // chat
      LAYOUT_DEFAULT_TITLEHIDDEN,   // combat
      LAYOUT_DEFAULT_TITLEHIDDEN,   // misc

   /* 1 - default, dwtype >= 0x100 */
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,

   // 2 - transcript
      EXPLOREVERTDIVIDEMUTE, 0.0, 1.0, 1.0, FALSE, TRUE,
      EXPLOREVERTDIVIDEMUTE, 0.0, 1.0, 0.5, FALSE, TRUE,
      0.5, 0.0, COMBATVERTDIVIDESINGLE, EXPLOREHORZDIVIDE, FALSE, TRUE,
      EXPLOREVERTDIVIDEMUTE, 0.0, 1.0, 1.0, FALSE, TRUE,

   // 3 - map
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      //EXPLOREVERTDIVIDEMUTE, EXPLOREHORZDIVIDE, 1.0, 1.0, FALSE, TRUE /* BUGFIX - visible by default */,
      //LAYOUT_DEFAULT_TITLEHIDDEN,
      //COMBATVERTDIVIDESINGLE, 0.75, 1.0, 1.0, FALSE, TRUE,
      //LAYOUT_DEFAULT_TITLEHIDDEN,

   // 4 - main
      0.0, 0.0, EXPLOREVERTDIVIDEMUTE, EXPLOREHORZDIVIDE, FALSE, TRUE,
      0.0, 0.0, EXPLOREVERTDIVIDEMUTE, EXPLOREHORZDIVIDECHAT, FALSE, TRUE,
      0.5, EXPLOREHORZDIVIDE, COMBATVERTDIVIDESINGLE, 1.0, FALSE, TRUE,
      0.0, 0.0, EXPLOREVERTDIVIDEMUTE, 0.666, FALSE, TRUE,

   // 5 - tempdisp
      LAYOUT_DEFAULT_TITLEHIDDEN,
      EXPLOREVERTDIVIDEMUTE, 0.5, 1.0, 1.0, FALSE, TRUE,
      1 - COMBATVERTDIVIDESINGLE, 0.0, 0.5, EXPLOREHORZDIVIDE, FALSE, TRUE,
      LAYOUT_DEFAULT_TITLEHIDDEN,

   // 6 - statusself
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      0.0, 0.0, 1.0 - COMBATVERTDIVIDESINGLE, EXPLOREHORZDIVIDE, FALSE, TRUE,
      LAYOUT_DEFAULT_TITLEHIDDEN,

   // 7 - statusenemy
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      COMBATVERTDIVIDESINGLE, 0.0, 1.0, EXPLOREHORZDIVIDE, FALSE, TRUE,
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,

   // 8 - helptip
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,

   // 9 - main iconwindow
      0.0, EXPLOREHORZDIVIDE, EXPLOREVERTDIVIDEMUTE, 1.0, FALSE, TRUE,
      0.0, EXPLOREHORZDIVIDECHAT, EXPLOREVERTDIVIDEMUTE, 1.0, FALSE, TRUE,
      0.0, EXPLOREHORZDIVIDE, 0.5 /* 1.0 - COMBATVERTDIVIDESINGLE*/, 1.0, FALSE, TRUE,
      0.0, 0.666, EXPLOREVERTDIVIDEMUTE, 1.0, FALSE, TRUE
   
#if 0 // no more chat
   // 10 - chat iconwindow
      0.0, 0.75, EXPLOREVERTDIVIDEMUTE/2.0, 1.0, FALSE, TRUE,
      0.0, .75, EXPLOREVERTDIVIDEMUTE, 1.0, FALSE, TRUE,
      1.0 - COMBATVERTDIVIDESINGLE, 0.75, 0.5, 1.0, FALSE, TRUE,
      0.0, 0.75, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE
#endif // 0
};
#endif // 0


#if 0 // no change since same as non-muted
static CLGINFO gclgDualMute[10][NUMTABS] = { // dual monitor, mute all
   /* 0 - default, dwtype < 0x100 */
      LAYOUT_DEFAULT_TITLEHIDDEN,   // BUGFIX - Set TRUE for title
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
//      LAYOUT_DEFAULT_TITLEHIDDEN,

   /* 1 - default, dwtype >= 0x100 */
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
      LAYOUT_DEFAULT_TITLEHIDDEN,
//      LAYOUT_DEFAULT_TITLEHIDDEN,

   // 2 - transcript
      1.0, 0.0, 1.5, 1.0, FALSE, TRUE,
      //4.0 / 6.0, 0.0, 1.0, EXPLORE2HORZDIVIDE, FALSE, TRUE,
      // OLDEXPLOREVERTDIVIDE, 0.0, 1.0 - OLDEXPLOREVERTDIVIDE, EXPLOREHORZDIVIDE, FALSE, TRUE,
      1.0, 0.0, 1.5, 1.0, FALSE, TRUE,
      1.0 - COMBATVERTDIVIDEDUAL, 0.5, COMBATVERTDIVIDEDUAL, 1.0, FALSE, TRUE,
//      EXPLOREVERTDIVIDE, 0.0, 1.0, 1.0, FALSE, TRUE,
      //1.0, 0.0, 1.5, 1.0, FALSE, TRUE,
      //1.0, 0.0, 1.5, EXPLOREHORZDIVIDE, FALSE, TRUE,
      //1.0 - COMBATVERTDIVIDEDUAL, 0.5, COMBATVERTDIVIDEDUAL, 1.0, FALSE, TRUE,
      //1.0, 0.0, 1.5, 1.0, FALSE, TRUE,

   // 3 - map
      1.5, 0.0, 2.0, 1.0, FALSE, TRUE,
      1.5, 0.0, 2.0, 0.5, FALSE, TRUE,
      1.5, 0.0, 2.0, 0.5, FALSE, TRUE,
//      1.5, 0.0, 2.0, 0.5, FALSE, TRUE,
      //1.5, 0.0, 2.0, 1.0, FALSE, TRUE,
      //1.5, 0.0, 2.0, EXPLOREHORZDIVIDE, FALSE, TRUE,
      //1.5, 0.0, 2.0, EXPLOREHORZDIVIDE, FALSE, TRUE,
      //1.5, 0.0, 2.0, 1.0, FALSE, TRUE,

   // 4 - main
      0.0, 0.0, 1.0, EXPLOREHORZDIVIDE, FALSE, TRUE,
      //0.0, 0.0, 4.0 / 6.0, EXPLORE2HORZDIVIDE, FALSE, TRUE,
      // OLDEXPLOREVERTDIVIDE, EXPLOREHORZDIVIDE, 1.0 - OLDEXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,
      0.0, 0.0, 0.5, EXPLOREHORZDIVIDECHAT, FALSE, TRUE,
      //0.0, 0.0, 0.5, 4.0 / 6.0, FALSE, TRUE,
      1.0, 0.0, 2.0, EXPLOREHORZDIVIDE, FALSE, TRUE,
//      0.0, 0.25, EXPLOREVERTDIVIDE, EXPLOREHORZDIVIDE, FALSE, TRUE,
      //0.0, 0.0, 1.0, EXPLOREHORZDIVIDE, FALSE, TRUE,
      //0.0, 0.0, 0.66, 0.66, FALSE, TRUE,
      //1.0, 0.5, 1.5, EXPLOREHORZDIVIDE, FALSE, TRUE,
      //0.0, 0.0, 1.0, EXPLOREHORZDIVIDE, FALSE, TRUE,

   // 5 - tempdisp
      LAYOUT_DEFAULT_TITLEHIDDEN,
      0.5, 0.0, 1.0, EXPLOREHORZDIVIDE, FALSE, TRUE,
      1 - COMBATVERTDIVIDEDUAL, 0.0, COMBATVERTDIVIDEDUAL, 0.5, FALSE, TRUE,
//      LAYOUT_DEFAULT_TITLEHIDDEN,
      //0.5, 0.0, 1.0, 4.0 / 6.0, FALSE, TRUE,
      //LAYOUT_DEFAULT_TITLEVISIBLE,
      //0.66, 0.0, 1.0, 0.66, FALSE, TRUE,
      //1 - COMBATVERTDIVIDEDUAL, 0.0, COMBATVERTDIVIDEDUAL, 0.5, FALSE, TRUE,
      //LAYOUT_DEFAULT_TITLEVISIBLE,

   // 6 - statusself
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      0.0, 0.0, 1.0 - COMBATVERTDIVIDEDUAL, 1.0, FALSE, TRUE,
//      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,

   // 7 - statusenemy
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,
      COMBATVERTDIVIDEDUAL, 0.0, 1.0, 1.0, FALSE, TRUE,
//      1.25, 0.25, 1.75, EXPLOREHORZDIVIDE, TRUE, FALSE,

   // 8 - helptip
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
//      0.25, 0.4, 0.75, 0.6, TRUE, TRUE,
   
   // 9 - main iconwindow
      0.0, EXPLOREHORZDIVIDE, 1.0, 1.0, FALSE, TRUE,
      //0.5, EXPLORE2HORZDIVIDE, 1.0, 1.0, FALSE, TRUE,
      // 1.0 - OLDEXPLOREVERTDIVIDE, 0.0, 1.0, 1.0, FALSE, TRUE,
      0.0, EXPLOREHORZDIVIDECHAT, 1.0, 1.0, FALSE, TRUE,
      1.0, EXPLOREHORZDIVIDE, 2.0, 1.0, FALSE, TRUE,
//      0.0, 0.0, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE
      //0.5, EXPLOREHORZDIVIDE, 1.0, 1.0, FALSE, TRUE,
      //1.0, EXPLOREHORZDIVIDE, 2.0, 1.0, FALSE, TRUE,
      //1.5, EXPLOREHORZDIVIDE, 2.0, 1.0, FALSE, TRUE,
      //0.5, EXPLOREHORZDIVIDE, 1.0, 1.0, FALSE, TRUE,
   
#if 0 // no more chat
   // 10 - chat iconwindow
      0.0, EXPLOREHORZDIVIDE, 0.5, 1.0, FALSE, TRUE,
      //0.0, EXPLORE2HORZDIVIDE, 0.5, 1.0, FALSE, TRUE,
      // 0.0, 0.0, OLDEXPLOREVERTDIVIDE, 1.0, FALSE, TRUE,
      0.0, .75, 1.0, 1.0, FALSE, TRUE,
      //0.0, 4.0 / 6.0, 1.0, 1.0, FALSE, TRUE,
      1.0, EXPLOREHORZDIVIDE, 1.5, 1.0, FALSE, TRUE,
      0.0, 0.75, EXPLOREVERTDIVIDE, 1.0, FALSE, TRUE
      //0.0, EXPLOREHORZDIVIDE, 0.5, 1.0, FALSE, TRUE,
      //0.0, 0.66, 1.0, 1.0, FALSE, TRUE,
      //1.0, EXPLOREHORZDIVIDE, 1.5, 1.0, FALSE, TRUE,
      //0.0, EXPLOREHORZDIVIDE, 0.5, 1.0, FALSE, TRUE
#endif // 0
};
#endif // 0

/*************************************************************************************
CMainWindow::ChildLocGet - This does a MMLValueGet() for the given node,
getting the location of the window relative to the overall window size.
This can retrieve the information stored by ChildLocSave()

inputs
   DWORD          dwType - Child window type, TW_XXX
   PWSTR          psz - String that identifies case of specific type. Can be
                        NULL for those types that don't need
   DWORD          *pdwMonitor - Monitor that it's on
   RECT           *prLoc - Filled with the location, in client rect.
                     NOTE: This will NOT be changed if FALSE is returned
   BOOL           *pfHidden - Filled with value for whether hidden or not.
                     Should be initiallized to starting value.
   BOOL           *pfTitle - Filled with a bool for whether has a title or not.
                     Should be initialized to starting value.
returns
   BOOL - TRUE if found, FALSE if not
*/
BOOL CMainWindow::ChildLocGet (DWORD dwType, PWSTR psz, DWORD *pdwMonitor, RECT *prLoc, BOOL *pfHidden, BOOL *pfTitle)
{
   // find a match?
   DWORD dwTabIndex = m_pSlideTop->m_dwTab + (gfMonitorUseSecond ? NUMTABS : 0);
   PTABWINDOW ptwLook = ChildLocTABWINDOW (dwType, psz);

// #define PROGRESSBOTTOM     0.08

#define WINDOWBORDERSIZE      125         // bit of border space between windows
         // BUGFIX - Was 250, but too small once remove lines

#define TOOLBARTOP         0.95
#ifdef UNIFIEDTRANSCRIPT
   // fp fExploreMainDisplayBottom = EXPLORE_MAINDISPLAYBOTTOM;
   // fp fChatMainDisplayBottom = 0.5;
#else
#define EXPLORE_MAINDISPLAYBOTTOM      0.75   // BUGFIX - was 0.75, but made larger because no toobar
      // BUGFIX - Was 0.8, but changed to 0.75 so more space
   // BUGFIX - Adjust for screen aspect ratio
   fp fScreenAspect = (fp) GetSystemMetrics (SM_CXSCREEN) / (fp) GetSystemMetrics (SM_CYSCREEN);
   fScreenAspect /= 1.25;   // to normalize for my current development screen
   // fScreenAspect *= 2;  // more extreme
   fp fExploreMainDisplayBottom = pow((fp)EXPLORE_MAINDISPLAYBOTTOM, fScreenAspect);
   fp fChatMainDisplayBottom = fExploreMainDisplayBottom * 4.0 / 5.0;
#endif

   fp fScreenAspect = (fp) GetSystemMetrics (SM_CXSCREEN) / (fp) GetSystemMetrics (SM_CYSCREEN);
   fp fMidAspect = ((16.0 / 9.0 + 4.0 / 3.0) / 2.0);
   BOOL fWide = (fScreenAspect >= fMidAspect);
   
  //fp fSecondMainDisplayBottom = 0.66;
  // fp fToolBarTop = 1; // since no longer have toolbar(fExploreMainDisplayBottom + 4) / 5; // NOTE: Was related to TOOLBARTOP
  // fp fBottomPanelHeight = fToolBarTop - fExploreMainDisplayBottom;


   // make some reasonable defaults
   TABWINDOW tw;
   if (!ptwLook) {
      DWORD dwTab = m_pSlideTop->m_dwTab;
      dwTab = min(dwTab, NUMTABS-1);  // so dont go beyond edige

      // default index
      DWORD dwPCLGIndex = (dwType < 0x100) ? 0 : 1;

      // figure out default x and y spacing based on the monitor
#define BORDERSPACEX       (afSpace[tw.dwMonitor][0])
#define BORDERSPACEY       (afSpace[tw.dwMonitor][1])
      DWORD i;
      fp afSpace[2][2];
      for (i = 0; i < 2; i++) {
         RECT rClient;
         HWND hWndSize = i ? m_hWndSecond : m_hWndPrimary;
         if (hWndSize)
            GetClientRect (hWndSize, &rClient);
         else {
            rClient.left = rClient.top = 0;
            rClient.right = 1024;
            rClient.bottom = 768;
         }
         rClient.right -= rClient.left;
         rClient.bottom -= rClient.top;
         rClient.right = max(rClient.right, 1);
         rClient.bottom = max(rClient.bottom, 1);

         int iSpace = min(rClient.right, rClient.bottom) / WINDOWBORDERSIZE;   // BUGFIX - was 75; // BUGFIX - Was 150

         afSpace[i][0] = (fp)iSpace / (fp)rClient.right;
         afSpace[i][1] = (fp)iSpace / (fp)rClient.bottom;
      } // i

      // specifics
      switch (dwType) {
      //case TW_EDIT:
         // no longer supported

      //case TW_VERB:
         // no longer supported

      case TW_TRANSCRIPT:
         dwPCLGIndex = 2;
         break;

      case TW_MAP:
         dwPCLGIndex = 3;
         break;

      // case TW_MENU:
         // no longer supported

      case TW_DISPLAYWINDOW:
         if (!_wcsicmp(psz, L"main"))
            dwPCLGIndex = 4;
         else if (!_wcsicmp(psz, L"tempdisp"))
            dwPCLGIndex = 5;
         else if (!_wcsicmp(psz, L"statusself"))
            dwPCLGIndex = 6;
         else if (!_wcsicmp(psz, L"statusenemy"))
            dwPCLGIndex = 7;
         else if (!_wcsicmp(psz, L"helptip"))
            dwPCLGIndex = 8;
         break;

      case TW_ICONWINDOW:
         if (!_wcsicmp(psz, L"main"))
            dwPCLGIndex = 9;
         // BUGFIX - no more chat
         // else if (!_wcsicmp(psz, L"chat"))
         //   dwPCLGIndex = 10;

         // "inventory" no longer special window
         // "tempicon" defaults
         break;
      } // switch

      // default
      BOOL fShowText = (m_iTextVsSpeech <= 1);
      PCLGINFO pclg = gfMonitorUseSecond ?
         &gclgDual[dwPCLGIndex][dwTab] : // BUGFIX - both the same, so no dif: (fShowText ? &gclgDualMute[dwPCLGIndex][dwTab] : &gclgDualNoMute[dwPCLGIndex][dwTab]) :
         &gclgSingle[dwPCLGIndex][dwTab];

      // some generics
      tw.dwType = dwType;
      tw.fTitle = pclg->fTitle;
      // tw.fTitle = TRUE; // BUGFIX - Try always displaying title bars so new users less confused
      tw.fVisible = pclg->fVisible;
      BOOL fOnSecondMonitor = (pclg->fLeft >= 1.0);
      tw.pLoc.p[0] = (pclg->fLeft >= 1.0) ? (pclg->fLeft - 1.0) : pclg->fLeft;
      tw.pLoc.p[1] = (pclg->fLeft >= 1.0) ?  (pclg->fRight - 1.0) : pclg->fRight;
      tw.pLoc.p[2] = pclg->fTop;
      tw.pLoc.p[3] = pclg->fBottom;
      tw.dwMonitor = (pclg->fLeft >= 1.0) ? 1 : 0; // default to 0

      // BUGFIX - Deal with widescreen, different width text area
      int iExploreVertDivide = 0;
      if (!fWide)
         iExploreVertDivide++;
      if (m_pSlideTop->m_dwTab == 1)   // if chat tab, extra wide
         iExploreVertDivide++;
      int iExploreVertDivideMute = iExploreVertDivide + 1;  // one more level
      iExploreVertDivide = min(iExploreVertDivide, 2);
      iExploreVertDivideMute = min(iExploreVertDivideMute, 2);
      iExploreVertDivide = iExploreVertDivideMute = 2;   // BUGFIX - Always use 0.5
      static fp afVertDivide[3] = {0.75, 0.666, 0.5}; // BUGFIX - All the same now

      fp fCenterMargin = fWide ? (LRMARGIN_WIDE / 4.0) : 0.0;
      fp fLRMargin = (fWide ? LRMARGIN_WIDE : LRMARGIN_NARROW);
      fp fTBMargin = TBMARGIN;

      if (m_pSlideTop->m_dwTab == 2) {
         // combat
         fLRMargin = 0.0;
         fTBMargin = 0.0;
      }
      else if ((m_pSlideTop->m_dwTab == 3) && !fOnSecondMonitor) {
         // zoom
         fCenterMargin = 0.0;
         fLRMargin = 0.0;
         fTBMargin = 0.0;
         iExploreVertDivide = 0; // BUGFIX - Always use narrowest
      }

      for (i = 0; i < 2; i++) { // inteionallty doing only first two
         if (tw.pLoc.p[i] == EXPLOREVERTDIVIDE)
            tw.pLoc.p[i] = afVertDivide[iExploreVertDivide] + fCenterMargin * (i ? -1.0 : 1.0);
         else if (tw.pLoc.p[i] == EXPLOREVERTDIVIDEHALF)
            tw.pLoc.p[i] = afVertDivide[iExploreVertDivide] / 2.0 + fCenterMargin * (i ? -1.0 : 1.0);
         else if (tw.pLoc.p[i] == EXPLOREVERTDIVIDEMUTE)
            tw.pLoc.p[i] = afVertDivide[iExploreVertDivideMute] + fCenterMargin * (i ? -1.0 : 1.0);
         else if (tw.pLoc.p[i] == EXPLOREVERTDIVIDEMUTEHALF)
            tw.pLoc.p[i] = afVertDivide[iExploreVertDivideMute] / 2.0 + fCenterMargin * (i ? -1.0 : 1.0);
         else if (tw.pLoc.p[i] == 0.0)
            tw.pLoc.p[i] += fLRMargin;
         else if (tw.pLoc.p[i] == 1.0)
            tw.pLoc.p[i] -= fLRMargin;
      } // i

      for (i = 2; i < 4; i++) {   // up/down
         if (tw.pLoc.p[i] == EXPLOREHORZDIVIDE)
            tw.pLoc.p[i] = ((1.0 - 2*fTBMargin) * 0.75 + fTBMargin);
         if (tw.pLoc.p[i] == 0.0)
            tw.pLoc.p[i] += fTBMargin;
         else if (tw.pLoc.p[i] == 1.0)
            tw.pLoc.p[i] -= fTBMargin;
      }

      // put border around
      // tw.fTitle = TRUE; // BUGFIX - Always default to title bar
      tw.pLoc.p[0] += BORDERSPACEX;
      tw.pLoc.p[1] -= BORDERSPACEX;
      tw.pLoc.p[2] += BORDERSPACEY;
      tw.pLoc.p[3] -= BORDERSPACEY;

      ptwLook = &tw;

      // Add this
      ChildLocSave (&tw, psz);
   } // if need defaults

   // shouldnt happen
   //if (!ptwLook)
   //   return FALSE;

   DWORD dwMonitor = gfMonitorUseSecond ? ptwLook->dwMonitor : 0;

   if (pfHidden)
      *pfHidden = !ptwLook->fVisible;
   if (pfTitle)
      *pfTitle = ptwLook->fTitle;
   if (pdwMonitor)
      *pdwMonitor = dwMonitor;

   RECT rIcon, rMain;
   ClientRectGet (dwMonitor, &rMain);

   int iWidth = rMain.right - rMain.left;
   int iHeight = rMain.bottom - rMain.top;
   iWidth = max(iWidth, 1);
   iHeight = max(iHeight, 1);

   // make sure within bounds
   DWORD i;
   for (i = 0; i < 4; i++) {
      ptwLook->pLoc.p[i] = max(ptwLook->pLoc.p[i], 0.0);
      ptwLook->pLoc.p[i] = min(ptwLook->pLoc.p[i], 1.0);
   } // i

   rIcon.left = (int)((fp)iWidth * ptwLook->pLoc.p[0] + 0.5 + rMain.left);
   rIcon.right = (int)((fp)iWidth * ptwLook->pLoc.p[1] + 0.5 + rMain.left);
   rIcon.top = (int)((fp)iHeight * ptwLook->pLoc.p[2] + 0.5 + rMain.top);
   rIcon.bottom = (int)((fp)iHeight * ptwLook->pLoc.p[3] + 0.5 + rMain.top);


   if ((rIcon.left >= rIcon.right) || (rIcon.top >= rIcon.bottom))
      return FALSE;  // no value

   // copy over
   *prLoc = rIcon;
   return TRUE;
}

#if 0 // dead code - no longer saving this way
/*************************************************************************************
CMainWindow::ChildLocSave - This does a MMLValueSet() for the given node,
storing the location of the window relative to the overall window size.

inputs
   HWND        hChild - Child window
   PCMMLNode2   pNode - Info written here
   BOOL        fHidden - TRUE if hidden
returns
   none
*/
static PWSTR gpszWindowLoc = L"WindowLoc";
static PWSTR gpszHidden = L"Hidden";

void CMainWindow::ChildLocSave (HWND hChild, PCMMLNode2 pNode, BOOL fHidden)
{
   // find the location
   RECT rIcon, rMain;
   ClientRectGet (&rMain);
   GetWindowRect (hChild, &rIcon);
   ScreenToClient (m_hWnd, ((POINT*)&rIcon) + 0);
   ScreenToClient (m_hWnd, ((POINT*)&rIcon) + 1);
   OffsetRect (&rIcon, -rMain.left, -rMain.top);
   OffsetRect (&rMain, -rMain.left, -rMain.top);
   rMain.right = max(rMain.right, 1);
   rMain.bottom = max(rMain.bottom, 1);

   CMMLNode2 node;
   CPoint p;
   int iWidth = rMain.right - rMain.left;
   int iHeight = rMain.bottom - rMain.top;
   iWidth = max(iWidth, 1);
   iHeight = max(iHeight, 1);
   p.p[0] = (fp)(rIcon.left - rMain.left) / (fp) iWidth;
   p.p[1] = (fp)(rIcon.right - rMain.left) / (fp) iWidth;
   p.p[2] = (fp)(rIcon.top - rMain.top) / (fp) iHeight;
   p.p[3] = (fp)(rIcon.bottom - rMain.top) / (fp) iHeight;
   MMLValueSet (pNode, gpszWindowLoc, &p);
   MMLValueSet (pNode, gpszHidden, (int)fHidden);
}

/*************************************************************************************
CMainWindow::ChildLocGet - This does a MMLValueGet() for the given node,
getting the location of the window relative to the overall window size.
This can retrieve the information stored by ChildLocSave()

inputs
   PCMMLNode2   pNode - Info written here
   RECT        *prLoc - Filled with the location, in client rect.
                  NOTE: This will NOT be changed if FALSE is returned
   BOOL        *pfHidden - Filled with value for whether hidden or not.
                  Should be initiallized to starting value.
returns
   BOOL - TRUE if found, FALSE if not
*/
BOOL CMainWindow::ChildLocGet (PCMMLNode2 pNode, RECT *prLoc, BOOL *pfHidden)
{
   if (pfHidden)
      *pfHidden = (BOOL) MMLValueGetInt (pNode, gpszHidden, (int)*pfHidden);

   CPoint p;
   RECT rIcon, rMain, rInter;
   p.Zero4();
   MMLValueGetPoint (pNode, gpszWindowLoc, &p);
   ClientRectGet (&rMain);

   int iWidth = rMain.right - rMain.left;
   int iHeight = rMain.bottom - rMain.top;
   iWidth = max(iWidth, 1);
   iHeight = max(iHeight, 1);

   // make sure within bounds
   DWORD i;
   for (i = 0; i < 4; i++) {
      p.p[i] = max(p.p[i], 0.0);
      p.p[i] = min(p.p[i], 1.0);
   } // i

   rIcon.left = (int)((fp)iWidth * p.p[0] + 0.5 + rMain.left);
   rIcon.right = (int)((fp)iWidth * p.p[1] + 0.5 + rMain.left);
   rIcon.top = (int)((fp)iHeight * p.p[2] + 0.5 + rMain.top);
   rIcon.bottom = (int)((fp)iHeight * p.p[3] + 0.5 + rMain.top);


   if ((rIcon.left >= rIcon.right) || (rIcon.top >= rIcon.bottom))
      return FALSE;  // no value

   if (!IntersectRect (&rInter, &rIcon, &rMain))
      return FALSE;
   
   // extra check, for off screen
   if ((rIcon.left+1 >= rIcon.right) || (rIcon.top+1 >= rIcon.bottom))
      return FALSE;  // no value

   // copy over
   *prLoc = rInter;
   return TRUE;
}
#endif // 0



static PWSTR gpszChildLocSave = L"ChildLocSave";
static PWSTR gpszWindow = L"Window";
static PWSTR gpszString = L"String";
static PWSTR gpszType = L"Type";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszVisible = L"Visible";
static PWSTR gpszTitle = L"Title";
static PWSTR gpszMonitor = L"Monitor";

/*************************************************************************************
CMainWindow::ChildLocSaveAll - Saves all the window locations for the given user.
*/
BOOL CMainWindow::ChildLocSaveAll (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return FALSE;
   pNode->NameSet (gpszChildLocSave);

   // create a sub-node for each type of window
   PCMMLNode2 pSub, pInfo;
   DWORD i, j;
   WCHAR szTemp[64];
   for (i = 0; i < NUMTABMONITORS; i++) {
      pSub = pNode->ContentAddNewNode();
      if (!pSub)
         continue;

      swprintf (szTemp, L"Tab%d", (int)i);
      pSub->NameSet (szTemp);

      // and contents
      for (j = 0; j < m_alTABWINDOW[i].Num(); j++) {
         PTABWINDOW ptw = (PTABWINDOW) m_alTABWINDOW[i].Get(j);
         PWSTR psz = (PWSTR)m_alTabWindowString[i].Get(j);

         pInfo = pSub->ContentAddNewNode ();
         if (!pInfo)
            continue;
         pInfo->NameSet (gpszWindow);

         if (psz && psz[0])
            MMLValueSet (pInfo, gpszString, psz);
         MMLValueSet (pInfo, gpszType, (int)ptw->dwType);
         MMLValueSet (pInfo, gpszVisible, (int)ptw->fVisible);
         MMLValueSet (pInfo, gpszTitle, (int)ptw->fTitle);
         MMLValueSet (pInfo, gpszLoc, &ptw->pLoc);
         MMLValueSet (pInfo, gpszMonitor, (int)ptw->dwMonitor);
      } // j
   } // i


   BOOL fRet = UserSave (FALSE, gpszChildLocSave, pNode);
   delete pNode;
   return fRet;
}


/*************************************************************************************
CMainWindow::ChildLocLoadAll - Load all the user's child window locations from the
user database
*/
BOOL CMainWindow::ChildLocLoadAll (void)
{
   // clear what have
   DWORD i;
   for (i = 0; i < NUMTABMONITORS; i++) {
      m_alTABWINDOW[i].Clear();
      m_alTabWindowString[i].Clear();
   } // i

   // load
   PCMMLNode2 pNode = UserLoad (FALSE, gpszChildLocSave);
   if (!pNode)
      return FALSE;

   PCMMLNode2 pSub, pInfo;
   PWSTR psz;
   DWORD j;
   WCHAR szTemp[64];
   for (i = 0; i < NUMTABMONITORS; i++) {
      swprintf (szTemp, L"Tab%d", (int)i);

      pSub = NULL;
      pNode->ContentEnum (pNode->ContentFind(szTemp), &psz, &pSub);
      if (!pSub)
         continue;

      for (j = 0; j < pSub->ContentNum(); j++) {
         pInfo = NULL;
         pSub->ContentEnum (j, &psz, &pInfo);
         if (!pInfo)
            continue;
         psz = pInfo->NameGet();
         if (!psz || _wcsicmp(psz, gpszWindow))
            continue;

         // found entry
         TABWINDOW tw;
         memset (&tw, 0, sizeof(tw));
         psz = MMLValueGet (pInfo, gpszString);
         if (!psz)
            psz = L"";
         tw.dwType = (DWORD) MMLValueGetInt (pInfo, gpszType, TW_MAP);
         tw.fVisible = (BOOL) MMLValueGetInt (pInfo, gpszVisible, TRUE);
         tw.fTitle = (BOOL) MMLValueGetInt (pInfo, gpszTitle, TRUE);
         tw.dwMonitor = (DWORD) MMLValueGetInt (pInfo, gpszMonitor, 0);
         MMLValueGetPoint (pInfo, gpszLoc, &tw.pLoc);

         // add it
         m_alTABWINDOW[i].Add (&tw);
         m_alTabWindowString[i].Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      } // j

   } // i

   // done
   delete pNode;
   return TRUE;
}


/*************************************************************************************
CMainWindow::ChildShowMove - Shows/hides and moves a child window based on its
settings in ChildLocGet().

inputs
   HWND           hChild - Child window to move
   DWORD          dwType - Child window type, TW_XXX
   PWSTR          psz - String that identifies case of specific type. Can be
                        NULL for those types that don't need
returns
   none
*/
void CMainWindow::ChildShowMove (HWND hChild, DWORD dwType, PWSTR psz)
{
   if (!hChild)
      return;

   // will want to sync
   if (m_pSlideTop)
      m_pSlideTop->TaskBarSyncTimer ();
   if (m_pSlideBottom)
      m_pSlideBottom->TaskBarSyncTimer ();

   // BUGFIX - temporarily turn off saving so doesnt mess up settings
   BOOL fPrev = gfChildLocIgnoreSave;
   gfChildLocIgnoreSave = TRUE;

   // get the current info
   BOOL fHidden = !IsWindowVisible (hChild);
   BOOL fTitle = ChildHasTitle (hChild);
   DWORD dwMonitorCur = ChildOnMonitor(hChild);
   HWND hWndMain = dwMonitorCur ? m_hWndSecond : m_hWndPrimary;
   RECT rLoc;
   GetWindowRect (hChild, &rLoc);
   ScreenToClient (hWndMain, ((POINT*)&rLoc) + 0);
   ScreenToClient (hWndMain, ((POINT*)&rLoc) + 1);

   // some windows are treated specially, like verb window
   BOOL fDontHide = FALSE;
   switch (dwType) {
      //case TW_VERB:
      //   fHidden = m_fVerbHiddenByUser;
      //   fDontHide = TRUE;
      //   break;
      case TW_MAP:
         fHidden = m_pMapWindow->m_fHiddenByUser;
         fDontHide = TRUE;
         break;
#ifndef UNIFIEDTRANSCRIPT
      case TW_TRANSCRIPT:  // transcript can't be hidden
#else
      case TW_MENU:
#endif
      //case TW_EDIT:
         fDontHide = TRUE;
         break;
   } // switch

   // keep track of the old
   BOOL fHiddenOld = fHidden;
   BOOL fTitleOld = fTitle;
   RECT rLocOld = rLoc;
   BOOL fChanged = FALSE;

   // get some info
   DWORD dwMonitor;
   ChildLocGet (dwType, psz, &dwMonitor, &rLoc, &fHidden, &fTitle);

   // potentially move the window
   if (dwMonitorCur != dwMonitor) {
      SetParent (hChild, dwMonitor ? m_hWndSecond : m_hWndPrimary);
      fChanged = TRUE;
   }

   // if hidden changed to hide then hide
   if (!fDontHide && fHidden && !fHiddenOld) {
      BeepWindowBeepShow (SW_HIDE);
      ShowWindow (hChild, SW_HIDE);
      fChanged = TRUE;

      // BUGFIX - So mouse wheel gets focus
      HWND hWndFocus = GetFocus();
      if ((hWndFocus == hChild) || (GetParent(hWndFocus) == hChild))
         CommandSetFocus(FALSE);
   }

   // move window if new location, or if monitor changed
   if (memcmp(&rLoc, &rLocOld, sizeof(rLoc)) || (dwMonitorCur != dwMonitor) ) {
      MoveWindow (hChild, rLoc.left, rLoc.top, rLoc.right - rLoc.left,
         rLoc.bottom - rLoc.top, TRUE);
      fChanged = TRUE;
   }

   // will need to show/hide title bar
   if (fTitle != fTitleOld)
      ChildTitleShowHide (fTitle, hChild, dwType, psz);

   // if hidden changed to visible then show
   if (!fDontHide && !fHidden && fHiddenOld) {
      BeepWindowBeepShow (SW_SHOWNA);
      ShowWindow (hChild, SW_SHOWNA);
      InvalidateRect (hChild, NULL, FALSE);
      fChanged = TRUE;
   }

   // special cases
   //if (dwType == TW_VERB) {
   //   if (m_fVerbHiddenByUser != fHidden) {
   //      m_fVerbHiddenByUser = fHidden;
   //      VerbWindowShow(TRUE);
   //      fChanged = TRUE;
   //   }
   //}
   if (dwType == TW_MAP) {
      if (m_pMapWindow->m_fHiddenByUser != fHidden) {
         m_pMapWindow->m_fHiddenByUser = fHidden;
         m_pMapWindow->Show(TRUE);
         fChanged = TRUE;
      }
   }
   else if (dwType == TW_TRANSCRIPT) {
      // BUGFIX - If just made transcript visible then update it
      if (fHiddenOld && !fHidden)
         TranscriptUpdate();
   }

   // BUGFIX - temporarily turn off saving so doesnt mess up settings
   gfChildLocIgnoreSave = fPrev;

   // make sure to re-save just in case a temporary move caused some problems
   //if (fChanged)
   //   ChildLocSave (hChild, dwType, psz, &fHidden);
}


/*************************************************************************************
CMainWindow::ChildShowMoveAll - Updates the locations and visiblilities for all
child windows.
*/
void CMainWindow::ChildShowMoveAll (void)
{
   // built-in windows
   if (m_pMapWindow)
      ChildShowMove (m_pMapWindow->m_hWnd, TW_MAP, NULL);
   ChildShowMove (m_hWndTranscript, TW_TRANSCRIPT, NULL);
   //ChildShowMove (m_hWndVerb, TW_VERB, NULL);
#ifndef UNIFIEDTRANSCRIPT
   ChildShowMove (m_hWndMenu, TW_MENU, NULL);
#endif

   // all the icon views
   PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCIconWindow.Num(); i++)
      ChildShowMove (ppi[i]->m_hWnd, TW_ICONWINDOW, (PWSTR)ppi[i]->m_memID.p);

   // all the display windows
   PCDisplayWindow *ppd = (PCDisplayWindow*)m_lPCDisplayWindow.Get(0);
   for (i = 0; i < m_lPCDisplayWindow.Num(); i++)
      ChildShowMove (ppd[i]->m_hWnd, TW_DISPLAYWINDOW, (PWSTR)ppd[i]->m_memID.p);

   // invladate the main display
   if (m_hWndPrimary)
      InvalidateRect (m_hWndPrimary, NULL, FALSE);
   if (m_hWndSecond)
      InvalidateRect (m_hWndSecond, NULL, FALSE);
}


/*************************************************************************************
CMainWindow::ChildToHWND - Takes a type and a name and converts to a HWND

inputs
   DWORD          dwType - Window type
   PWSTR          psz - Optimzation string for some types
reutrns
   HWND - Window handle. Might be NULL
*/
HWND CMainWindow::ChildToHWND (DWORD dwType, PWSTR psz)
{
   DWORD i;

   switch (dwType) {
   case TW_MAP:
      return m_pMapWindow ? m_pMapWindow->m_hWnd : NULL;
   case TW_TRANSCRIPT:
      return m_hWndTranscript;
#ifndef UNIFIEDTRANSCRIPT
   case TW_MENU:
      return m_hWndMenu;
#endif

   case TW_DISPLAYWINDOW:
      {
         PCDisplayWindow *ppd = (PCDisplayWindow*)m_lPCDisplayWindow.Get(0);
         for (i = 0; i < m_lPCDisplayWindow.Num(); i++)
            if (!_wcsicmp((PWSTR)ppd[i]->m_memID.p, psz))
               return ppd[i]->m_hWnd;
      }
      return NULL;

   case TW_ICONWINDOW:
      {
         PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
         for (i = 0; i < m_lPCIconWindow.Num(); i++)
            if (!_wcsicmp((PWSTR)ppi[i]->m_memID.p, psz))
               return ppi[i]->m_hWnd;
      }
      return NULL;

   default:
      return NULL;
   } // switch dwType
}


/*************************************************************************************
CMainWindow::ChildTitleShowHide - Shows/hides a specific title bar.

inputs
   BOOL           fShow - If TRUE then show
   HWND           hChild - Child window to move
   DWORD          dwType - Child window type, TW_XXX
   PWSTR          psz - String that identifies case of specific type. Can be
                        NULL for those types that don't need
returns
   none
*/
void CMainWindow::ChildTitleShowHide (BOOL fShow, HWND hChild, DWORD dwType, PWSTR psz)
{
   // if the same then dont bother
   if (fShow == ChildHasTitle (hChild))
      return;  // no change

   BOOL fIsMainWindow = (hChild == m_hWndPrimary) || (hChild == m_hWndSecond);

#ifdef UNIFIEDTRANSCRIPT
   BOOL fNoSysMenu = (dwType == TW_TRANSCRIPT);
#else
   BOOL fNoSysMenu = /*(dwType == TW_EDIT) ||*/ (dwType == TW_MENU);
#endif
   LONG lStyle = GetWindowLong (hChild, GWL_STYLE);
   LONG lStyleEx = GetWindowLong (hChild, GWL_EXSTYLE);
   LONG lFlags = fNoSysMenu ? WS_IFTITLE : WS_IFTITLECLOSE;
   LONG lFlagsEx = fShow ? WS_EX_IFTITLE : WS_EX_IFNOTITLE;
   LONG lFlagsExMask = (WS_EX_IFTITLE | WS_EX_IFNOTITLE);
   LONG lFlagsExOrig = lStyleEx & lFlagsExMask;
   
   // special for main window
   if (fIsMainWindow) {
      lFlags = WS_OVERLAPPEDWINDOW;
      lFlagsEx = 0;
      lFlagsExMask = 0;
   }

   if (fShow)
      lStyle |= lFlags;
   else
      lStyle &= ~lFlags;

   lStyleEx &= ~lFlagsExMask;
   lStyleEx |= lFlagsEx;

   // BUGFIX - temporarily turn off saving so doesnt mess up settings
   BOOL fPrev = gfChildLocIgnoreSave;
   gfChildLocIgnoreSave = TRUE;

   // set
   SetWindowLong (hChild, GWL_STYLE, lStyle);
   if (lFlagsEx != lFlagsExOrig)
      SetWindowLong (hChild, GWL_EXSTYLE, lStyleEx);

   // update the window position
   SetWindowPos (hChild, NULL, 0, 0, 0, 0,
      SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOSIZE);

   // BUGFIX - If it's the main window and no title bar then
   // resize, otherwise can't get to scrolling control panels
   if (fIsMainWindow && !fShow) {
      // BUGFIX - See which window it's on mostly
      RECT rMonitorPrimaryRect, r;
      rMonitorPrimaryRect.top = rMonitorPrimaryRect.left = 0;
      rMonitorPrimaryRect.right = GetSystemMetrics (SM_CXSCREEN);
      rMonitorPrimaryRect.bottom = GetSystemMetrics (SM_CYSCREEN);

      // default to primary rect
      r = rMonitorPrimaryRect;

      if ((grMonitorSecondRect.right > grMonitorSecondRect.left) && (grMonitorSecondRect.bottom > grMonitorSecondRect.top)) {
         // see which one intersects the best
         RECT rChild, rDest;
         GetWindowRect (hChild, &rChild);

         __int64 iAreaPrimary = 0, iAreaSecondary = 0;
         if (IntersectRect (&rDest, &rChild, &rMonitorPrimaryRect))
            iAreaPrimary = (__int64)(rDest.right - rDest.left) * (__int64)(rDest.bottom - rDest.top);
         if (IntersectRect (&rDest, &rChild, &grMonitorSecondRect))
            iAreaSecondary = (__int64)(rDest.right - rDest.left) * (__int64)(rDest.bottom - rDest.top);

         if (iAreaSecondary > iAreaPrimary)
            r = grMonitorSecondRect;
      }

      //if (hChild == m_hWndSecond)
      //   r = grMonitorSecondRect;
      //else {
      //   r.top = r.left = 0;
      //   r.right = GetSystemMetrics (SM_CXSCREEN);
      //   r.bottom = GetSystemMetrics (SM_CYSCREEN);
      //}

      SetWindowPos (hChild, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_NOZORDER);
   }

   //if (fIsMainWindow && !fShow)
   //   ShowWindow (hChild, SW_SHOWMAXIMIZED);

   // if this is a trsncript then update
   if (m_hWndTranscript && (hChild == m_hWndTranscript))
      TranscriptBackground (FALSE);

   InvalidateRect (hChild, NULL, FALSE);

   // BUGFIX - temporarily turn off saving so doesnt mess up settings
   gfChildLocIgnoreSave = fPrev;

   if (fIsMainWindow) {
      // go through all the children up move them to their proper places
      ChildShowMoveAll ();
   }
   else {
      // remember this setting
      RECT rLoc;
      BOOL fHidden = FALSE, fTitle = FALSE;
      DWORD dwMonitor;
      ChildLocGet (dwType, psz, &dwMonitor, &rLoc, &fHidden, &fTitle);
      ChildLocSave (hChild, dwType, psz, &fHidden);
   }
}


/*************************************************************************************
CMainWindow::ChildTitleShowHideAll - Shows or hides all title bars.
*/
void CMainWindow::ChildTitleShowHideAll (void)
{
   // if main window has title bar use as way to decide
   BOOL fShowTitle = !ChildHasTitle (m_hWndPrimary);
      // BUGFIX - Was m_hWndProgress despite what the comment said, so changed back

   // hide all
   if (m_pMapWindow)
      ChildTitleShowHide (fShowTitle, m_pMapWindow->m_hWnd, TW_MAP, NULL);
   ChildTitleShowHide (fShowTitle, m_hWndTranscript, TW_TRANSCRIPT, NULL);
   //ChildTitleShowHide (fShowTitle, m_hWndVerb, TW_VERB, NULL);
#ifndef UNIFIEDTRANSCRIPT
   ChildTitleShowHide (fShowTitle, m_hWndMenu, TW_MENU, NULL);
#endif

   // all the icon views
   DWORD i;
   PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
   for (i = 0; i < m_lPCIconWindow.Num(); i++)
      ChildTitleShowHide (fShowTitle, ppi[i]->m_hWnd, TW_ICONWINDOW, (PWSTR)ppi[i]->m_memID.p);

   // all the display windows
   PCDisplayWindow *ppd = (PCDisplayWindow*)m_lPCDisplayWindow.Get(0);
   for (i = 0; i < m_lPCDisplayWindow.Num(); i++)
      ChildTitleShowHide (fShowTitle, ppd[i]->m_hWnd, TW_DISPLAYWINDOW, (PWSTR)ppd[i]->m_memID.p);

   // do the main window
   ChildTitleShowHide (fShowTitle, m_hWndPrimary, 0, NULL);
   if (m_hWndSecond)
      ChildTitleShowHide (fShowTitle, m_hWndSecond, 0, NULL);
}


/*************************************************************************************
CMainWindow::ChildWouldOverlap - Tests to see if a child window would overlap
the given area of client. If this child window isn't visible then this returns FALSE

inputs
   HWND        hChild - To see if overlaps
   HWND        hChildLoc - Child whose location is given by *prLoc. This could
                  be NULL. This won't intersect with itself
   RECT        *prLoc - Region of window to look for an overlap
   DWORD       dwMonitorLoc - Monitor where prLoc is
returns
   BOOL - TRUE if overlap, FALSE if none
*/
BOOL CMainWindow::ChildWouldOverlap (HWND hChild, HWND hChildLoc, RECT *prLoc, DWORD dwMonitorLoc)
{
   if (!hChild || !IsWindowVisible(hChild) || (hChild == hChildLoc))
      return FALSE;

   // get rec
   RECT rLoc, rInter;
   DWORD dwMonitor = ChildOnMonitor (hChild);
   if (dwMonitor != dwMonitorLoc)
      return FALSE;  // different monitor
   HWND hWndMain = dwMonitor ? m_hWndSecond : m_hWndPrimary;
   GetWindowRect (hChild, &rLoc);
   ScreenToClient (hWndMain, ((POINT*)&rLoc) + 0);
   ScreenToClient (hWndMain, ((POINT*)&rLoc) + 1);

   if (!IntersectRect (&rInter, prLoc, &rLoc))
      return FALSE;

   // if only a slight intersection then ok
   if ((rInter.right - rInter.left <= 2) || (rInter.bottom - rInter.top <= 2))
      return FALSE;

   // else, intersect
   return TRUE;
}


/*************************************************************************************
CMainWindow::ChildShowTitleIfOverlap - This causes the *pfTitle flag to be
set to TRUE if it overlaps with an existing window

inputs
   HWND           hChild - Child window. This might be NULL
   RECT           *prLoc - Location of the child window in m_hWnd coords
   DWORD          dwMonitorLoc - Monitor of the location
   BOOL           fHidden - Hidden flag. If TRUE then this won't bother checking
   BOOL           *pfTitle - Should be initialized to whether or not has title. Will
                     set this to TRUE if there would be an overlap
returns
   none
*/
void CMainWindow::ChildShowTitleIfOverlap (HWND hChild, RECT *prLoc, DWORD dwMonitorLoc, BOOL fHidden, BOOL *pfTitle)
{
   // if hiddden or already has title set then no big deal
   if (fHidden || *pfTitle)
      return;

   // check all the windows to see if there's an overlap
   BOOL fOverlapped = FALSE;

   if (m_pMapWindow)
      fOverlapped |= ChildWouldOverlap (m_pMapWindow->m_hWnd, hChild, prLoc, dwMonitorLoc);
   fOverlapped |= ChildWouldOverlap (m_hWndTranscript, hChild, prLoc, dwMonitorLoc);
   //fOverlapped |= ChildWouldOverlap (m_hWndVerb, hChild, prLoc);
#ifndef UNIFIEDTRANSCRIPT
   fOverlapped |= ChildWouldOverlap (m_hWndMenu, hChild, prLoc, dwMonitorLoc);
#endif

   // all the icon views
   DWORD i;
   PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
   if (!fOverlapped) for (i = 0; i < m_lPCIconWindow.Num(); i++)
      fOverlapped |= ChildWouldOverlap (ppi[i]->m_hWnd, hChild, prLoc, dwMonitorLoc);

   // all the display windows
   PCDisplayWindow *ppd = (PCDisplayWindow*)m_lPCDisplayWindow.Get(0);
   if (!fOverlapped) for (i = 0; i < m_lPCDisplayWindow.Num(); i++)
      fOverlapped |= ChildWouldOverlap (ppd[i]->m_hWnd, hChild, prLoc, dwMonitorLoc);

   // set flag
   *pfTitle = fOverlapped;
}


/*************************************************************************************
CMainWindow::ChildShowWindow - Shows/hides the child window.

inputs
   HWND           hChild - Window
   DWORD          dwType - Type of window, WT_XXX
   PWSTR          psz - String associated with the type
   int            nShow - Either SW_SHOW, SW_SHOWNA, SW_HIDE
   BOOL           *pfHidden - If not NULL, then this is the valid that's stored
                  for the new hidden setting
   BOOL           fNoChangeTitle - If TRUE then dont ever change the title setting
returns
   none
*/
void CMainWindow::ChildShowWindow (HWND hChild, DWORD dwType, PWSTR psz, int nShow,
                                   BOOL *pfHidden, BOOL fNoChangeTitle)
{
   // will want to sync this
   if (m_pSlideTop)
      m_pSlideTop->TaskBarSyncTimer ();
   if (m_pSlideBottom)
      m_pSlideBottom->TaskBarSyncTimer ();

   // if it's already invisible and want it invisible then do nothing
   BOOL fVisible = IsWindowVisible (hChild);
   switch (nShow) {
   case SW_HIDE:
      if (fVisible) {
         BeepWindowBeepShow (nShow);
         ShowWindow (hChild, nShow);

         // save the info
         ChildLocSave (hChild, dwType, psz, pfHidden);
      }
      return;

   case SW_SHOW:
      if (fVisible) {
         BeepWindowBeepShow (SW_SHOW);
         ShowWindow (hChild, SW_SHOW);
         SetWindowPos (hChild, (HWND)HWND_TOP, 0,0,0,0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
         return;
      }
      break;

   case SW_SHOWNA:
      if (fVisible)
         return;  // already there
      break;
   }

   // else, if get here need to show. Is hidden

   // get the location
   RECT rLoc;
   BOOL fHidden = TRUE, fTitle = FALSE;
   DWORD dwMonitor;
   ChildLocGet (dwType, psz, &dwMonitor, &rLoc, &fHidden, &fTitle);
   BOOL fOldTitle = fTitle;

   // see if need to update the title
   ChildShowTitleIfOverlap (hChild, &rLoc, dwMonitor, FALSE, &fTitle);

   // turn on the title
   if (!fNoChangeTitle && (fOldTitle != fTitle))
      ChildTitleShowHide (fTitle, hChild, dwType, psz);

   // show the window
   BeepWindowBeepShow (nShow);
   ShowWindow (hChild, nShow);
   SetWindowPos (hChild, (HWND)HWND_TOP, 0,0,0,0,
      SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

   // save the info
   ChildLocSave (hChild, dwType, psz, pfHidden);
}


/*************************************************************************************
CMainWindow::ChildShowToggle - Shows/hides the child window.

inputs
   DWORD          dwType - Type of window, WT_XXX. DOESNT handle progress
   PWSTR          psz - String associated with the type
   int            iToggle - If 0 then toggle. If 1 then always show, if -1 then always hide
returns
   BOOL - TRUE if found and showed. FALSE if didn't show
*/
BOOL CMainWindow::ChildShowToggle (DWORD dwType, PWSTR psz, int iToggle)
{
   BOOL fWant = (iToggle >= 0);
   DWORD i;

   // will want to sync
   if (m_pSlideTop)
      m_pSlideTop->TaskBarSyncTimer ();
   if (m_pSlideBottom)
      m_pSlideBottom->TaskBarSyncTimer ();

   switch (dwType) {
   //case TW_VERB:
   //   if (!iToggle)
   //      fWant = m_fVerbHiddenByUser;
   //   if (fWant != !m_fVerbHiddenByUser) {
   //      m_fVerbHiddenByUser = !fWant;
   //      VerbWindowShow ();
   //   }
   //
   //   return (fWant == IsWindowVisible (m_hWndVerb));

   case TW_MAP:
      if (!iToggle)
         fWant = m_pMapWindow->m_fHiddenByUser;
      if (fWant != !m_pMapWindow->m_fHiddenByUser) {
         m_pMapWindow->m_fHiddenByUser = !fWant;
         m_pMapWindow->Show(FALSE);
      }

      return (fWant == IsWindowVisible (m_pMapWindow->m_hWnd));


   case TW_TRANSCRIPT:
      if (!iToggle)
         fWant = !IsWindowVisible(m_hWndTranscript);
#ifndef UNIFIEDTRANSCRIPT
      // can't hide transcript
      if (fWant != IsWindowVisible(m_hWndTranscript)) {
         ChildShowWindow (m_hWndTranscript, TW_TRANSCRIPT, NULL, fWant ? SW_SHOW : SW_HIDE);
         if (fWant)
            TranscriptUpdate ();
      }
#endif

      return (fWant == IsWindowVisible (m_hWndTranscript));

#ifndef UNIFIEDTRANSCRIPT
   case TW_MENU:
      // cant actually show menu, but return if visible
      if (!iToggle)
         fWant = !(m_hWndMenu && IsWindowVisible(m_hWndMenu));
      return (fWant == (m_hWndMenu && IsWindowVisible (m_hWndMenu)));
#endif

   case TW_ICONWINDOW:
      {
         PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
         for (i = 0; i < m_lPCIconWindow.Num(); i++) {
            if (_wcsicmp(psz, (PWSTR) ppi[i]->m_memID.p))
               continue;

            // found it
            if (!iToggle)
               fWant = !IsWindowVisible(ppi[i]->m_hWnd);
            if (fWant != IsWindowVisible (ppi[i]->m_hWnd))
               ChildShowWindow (ppi[i]->m_hWnd, dwType, psz,
                  fWant ? SW_SHOW : SW_HIDE);
            return (fWant == IsWindowVisible (ppi[i]->m_hWnd));
         } // i
      }
      return FALSE;

   case TW_DISPLAYWINDOW:
      {
         PCDisplayWindow *ppd = (PCDisplayWindow*)m_lPCDisplayWindow.Get(0);
         for (i = 0; i < m_lPCDisplayWindow.Num(); i++) {
            if (_wcsicmp(psz, (PWSTR) ppd[i]->m_memID.p))
               continue;

            // found it
            if (!iToggle)
               fWant = !IsWindowVisible(ppd[i]->m_hWnd);
            if (fWant != IsWindowVisible (ppd[i]->m_hWnd))
               ChildShowWindow (ppd[i]->m_hWnd, dwType, psz,
                  fWant ? SW_SHOW : SW_HIDE);
            return (fWant == IsWindowVisible (ppd[i]->m_hWnd));
         } // i
      }
      return FALSE;


   default:
      return FALSE;  // unknown
   } // dwType

   return FALSE;
}


/*************************************************************************************
CMainWindow::ChildMoveMonitor - Moves/hides the child window to the other monitor

inputs
   DWORD          dwType - Type of window, WT_XXX. DOESNT handle progress
   PWSTR          psz - String associated with the type
returns
   BOOL - TRUE if found and showed. FALSE if didn't show
*/
BOOL CMainWindow::ChildMoveMonitor (DWORD dwType, PWSTR psz)
{
   PTABWINDOW ptw = ChildLocTABWINDOW (dwType, psz);
   if (!ptw) {
      DWORD dwMonitor;
      RECT rLoc;
      BOOL fHidden, fTitle;
      ChildLocGet (dwType, psz, &dwMonitor, &rLoc, &fHidden, &fTitle);
      ptw = ChildLocTABWINDOW (dwType, psz);
      if (!ptw)
         return FALSE;  // couldnt find
   }

   // move to the other monitor
   HWND hWnd = ChildToHWND (dwType, psz);
   if (!hWnd)
      return FALSE;

   ptw->dwMonitor = (ptw->dwMonitor ? 0 : 1);

   ChildShowMove (hWnd, dwType, psz);

   return TRUE;
}



/*************************************************************************************
CMainWindow::TabSwitch - Switches to a new tab

inputs
   DWORD          dwTab - Tab to switch to
   BOOL           fUserClick - Set to TRUE if the switch is because the user clicked,
                     FALSE if it's because of the server
*/
BOOL CMainWindow::TabSwitch (DWORD dwTab, BOOL fUserClick)
{
   if (dwTab >= NUMTABS)
      return FALSE;

   if (dwTab == m_pSlideTop->m_dwTab)
      return FALSE;   // nothing else to do since selected

   // BUGFIX - Move beep after check for different tab
   BeepWindowBeep (ESCBEEP_LINKCLICK);

   // temp disable show beeps
   gdwBeepWindowBeepShowDisable++;

   // else, select
   m_pSlideTop->m_dwTab = dwTab;

   // slide locked
   if (m_fSlideLocked != m_afSlideLocked[m_pSlideTop->m_dwTab]) {
      m_fSlideLocked = m_afSlideLocked[m_pSlideTop->m_dwTab];

      // tell windows
      if (m_pSlideTop)
         m_pSlideTop->Locked (m_fSlideLocked);
      if (m_pSlideBottom)
         m_pSlideBottom->Locked (m_fSlideLocked);

      // resize everything
      // do below: pv->ChildShowMoveAll ();
   }

   // rearrange windows
   ChildShowMoveAll ();

   m_pSlideTop->Invalidate();

   // if tab automatically switchd then make sure slide down
   if (!fUserClick) {
      if (m_pSlideTop)
         m_pSlideTop->SlideDownTimed (1000);
      if (m_pSlideBottom)
         m_pSlideBottom->SlideDownTimed (1000);
   }

   // if switching to the combat window then automatically send a "Combat status"
   // command
   // BUGFIX - Send a string to indicate what tab is set
   //if (fUserClick && (dwTab == 2)) {
   //   SendTextCommand (DEFLANGID, L"Combat status", NULL, TRUE, TRUE);
   //      // NOTE: Specifically use english
   //   HotSpotDisable ();
   //}
   if (fUserClick && (dwTab < sizeof(gapszTabNames)/sizeof(PWSTR)))
      InfoForServer (L"tab", gapszTabNames[dwTab], 0);


   // change the speaking speed
   if ((m_iSpeakSpeed != m_aiSpeakSpeed[m_pSlideTop->m_dwTab]) ||
      (m_fMuteAll != m_afMuteAll[m_pSlideTop->m_dwTab]) ||
      (m_dwTransShow != m_adwTransShow[m_pSlideTop->m_dwTab]) ) {

      EnterCriticalSection (&m_crSpeakBlacklist);
      m_iSpeakSpeed = m_aiSpeakSpeed[m_pSlideTop->m_dwTab];
      m_fMuteAll = m_afMuteAll[m_pSlideTop->m_dwTab];
      m_dwTransShow = m_adwTransShow[m_pSlideTop->m_dwTab];
      LeaveCriticalSection (&m_crSpeakBlacklist);

      InfoForServer (L"speakspeed", NULL, m_iSpeakSpeed);

      // update the transcript
      TranscriptUpdate ();
   }

   // renable
   gdwBeepWindowBeepShowDisable--;

   return TRUE;
}


/*************************************************************************************
CMainWindow::SecondCreateDestroy - Create or destroy the second window,
setting gfMonitorUseSecond.

inputs
   BOOL           fCreate - If TRUE want the second window, FALSE then dont
   BOOL           fMoveAlso - If TRUE then this is NOT the initialization phase and there
                     will be child windows that need moving around
retirns
   none
*/
void CMainWindow::SecondCreateDestroy (BOOL fCreate, BOOL fMoveAlso)
{
   gfMonitorUseSecond = fCreate;

   if (m_pHM)
      m_pHM->MonitorsSet (gfMonitorUseSecond ? 2 : 1);

   if (!gfMonitorUseSecond) {
      // destroying
      if (fMoveAlso)
         ChildShowMoveAll ();

      // destroy this window
      if (m_hWndSecond)
         DestroyWindow (m_hWndSecond);
      m_hWndSecond = NULL;
      return;
   }

   // else
   // creating
   // create window
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = SecondWindowWndProc;
   wc.lpszClassName = "CircumrealityClientSecondWindow";
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_APPICON));
   wc.hCursor = NULL;
   wc.hbrBackground = NULL;
   RegisterClass (&wc);

   m_hWndSecond = CreateWindowEx (WS_EX_APPWINDOW,
      wc.lpszClassName, "Circumreality (Second monitor)",
      WS_POPUP | /*WS_OVERLAPPEDWINDOW |*/ WS_CLIPCHILDREN,
      grMonitorSecondRect.left, grMonitorSecondRect.top,
      grMonitorSecondRect.right - grMonitorSecondRect.left, grMonitorSecondRect.bottom - grMonitorSecondRect.top,
      NULL, NULL, ghInstance, (PVOID) this);
   ShowWindow (m_hWndSecond, SW_SHOWMAXIMIZED);
   

   // move child windows
   if (fMoveAlso)
      ChildShowMoveAll ();
}


/*************************************************************************
RenderQualityEasyPage
*/
BOOL RenderQualityEasyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMainWindow pv = (PCMainWindow)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // check the current resolution
         PCEscControl pControl;
         WCHAR szTemp[64];
         swprintf (szTemp, L"qual%d", (int)max(pv->m_iPreferredQualityMono,0));
            // BUGFIX - So always checked
         pControl = pPage->ControlFind (szTemp);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         pControl = pPage->ControlFind (L"powersaver");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fPowerSaver);

         if (pControl = pPage->ControlFind (L"rendercache")) {
            pControl->AttribSetBOOL (Checked(), gfRenderCache);
            
            if (!RegisterMode() || !pv->m_fConnectRemote)
               pControl->Enable (FALSE);
         }

         ComboBoxSet (pPage, L"artstyle", pv->m_dwArtStyle);

         // disable
         DWORD i;
         if (!RegisterMode()) {
            for (i = RESQUAL_QUALITYMONOMAXIFNOTPAID; i < RESQUAL_QUALITYMONOMAX; i++) {
               swprintf (szTemp, L"qual%d", (int)i);
               if (pControl = pPage->ControlFind (szTemp))
                  pControl->Enable (FALSE);
            } // i


            if (pControl = pPage->ControlFind (L"advanced"))
               pControl->Enable (FALSE);
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

         PWSTR pszQual = L"qual";
         DWORD dwQualLen = (DWORD)wcslen(pszQual);

         if (!_wcsnicmp(psz, pszQual, dwQualLen)) {
            int iQualityMono = _wtoi(psz + dwQualLen);

            if (iQualityMono == pv->m_iPreferredQualityMono)
               return TRUE;   // no change

            pv->m_iPreferredQualityMono = iQualityMono;

            pv->ResolutionQualityToRenderSettings (pv->m_fPowerSaver, pv->m_iPreferredQualityMono, TRUE, 
               &pv->m_iResolution, &pv->m_dwShadowsFlags,
               &pv->m_dwServerSpeed, &pv->m_iTextureDetail, &pv->m_fLipSync,
               &pv->m_iResolutionLow, &pv->m_dwShadowsFlagsLow, &pv->m_fLipSyncLow,
               &pv->m_dwMovementTransition);
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
            pv->InfoForServer (L"graphspeed", NULL, pv->m_dwServerSpeed);
            pv->InfoForServer (L"movementtransition", NULL, pv->m_dwMovementTransition);

            return TRUE;
         }

         else if (!_wcsicmp(psz, L"powersaver")) {
            pv->m_fPowerSaver = p->pControl->AttribGetBOOL (Checked());

            if (pv->m_pHM)
               pv->m_pHM->LowPowerSet (pv->m_fPowerSaver);

            pv->ResolutionQualityToRenderSettings (pv->m_fPowerSaver, pv->m_iPreferredQualityMono, TRUE, 
               &pv->m_iResolution, &pv->m_dwShadowsFlags,
               &pv->m_dwServerSpeed, &pv->m_iTextureDetail, &pv->m_fLipSync,
               &pv->m_iResolutionLow, &pv->m_dwShadowsFlagsLow, &pv->m_fLipSyncLow,
               &pv->m_dwMovementTransition);
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
            pv->InfoForServer (L"graphspeed", NULL, pv->m_dwServerSpeed);
            pv->InfoForServer (L"movementtransition", NULL, pv->m_dwMovementTransition);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"rendercache")) {
            gfRenderCache = p->pControl->AttribGetBOOL (Checked());
            WriteRegRenderCache (gfRenderCache);
            pv->InfoForServer (L"rendercache", NULL, (fp)gfRenderCache);
            return TRUE;
         }
      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;
         
         if (!_wcsicmp(psz, L"artstyle")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwArtStyle)
               return TRUE;

            pv->m_dwArtStyle = dwVal;
            pv->InfoForServer (L"artstyle", NULL, pv->m_dwArtStyle);
               // if hasn't paid then no transition
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszTooSlow = L"TOOSLOW", pszShowPix = L"SHOWPIX", pszQualTooSlow = L"QUALTOOSLOW", pszDynTooSlow = L"DYNTOOSLOW";
         DWORD dwTooSlowLen = (DWORD)wcslen(pszTooSlow), dwShowPixLen = (DWORD)wcslen(pszShowPix),
            dwQualTooSlowLen = (DWORD)wcslen(pszQualTooSlow), dwDynTooSlowLen = (DWORD)wcslen(pszDynTooSlow);
         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Image quality";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RENDERCACHE")) {
            int iRegister = RegisterMode ();
            if (iRegister > 0)
               p->pszSubString = L"";  // since registered
            else if (iRegister < 0)
               p->pszSubString = gpszSettingsTempEnabled;
            else
               p->pszSubString = gpszSettingsDisabled;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DISABLEDOFFLINE")) {
            if (pv->m_fConnectRemote)
               p->pszSubString = L"";  // since registered
            else
               p->pszSubString = L"<p/>This has not effect on current gameplay because you're not playing over the Internet.";
            return TRUE;
         }
         else if (!_wcsnicmp(p->pszSubName, pszQualTooSlow, dwQualTooSlowLen)) {
            int iVal = _wtoi(p->pszSubName + dwQualTooSlowLen);

            MemZero (&gMemTemp);

            // figure out how fast computer it
            int iQualityMono;
            iQualityMono = CPUSpeedToQualityMono (giCPUSpeed);
            if (iVal > iQualityMono + RESQUAL_RESOLUTIONBEYONDRECOMMEND)
               MemCat (&gMemTemp,
                  L"<p/><bold>Warning:</bold> This setting may be too slow for your computer.");

            // if not allowed because haven't pad
            if (iVal >= RESQUAL_QUALITYMONOMAXIFNOTPAID)
               switch (RegisterMode()) {
                  case 0:  // disable
                     MemCat (&gMemTemp, gpszSettingsDisabled);
                     break;
                  case -1:  // temporarily enabled
                     MemCat (&gMemTemp, gpszSettingsTempEnabled);
                     break;
                  case 1:  // enabled
                     // do nothing
                     break;
               } // switch

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }     


      }      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************
RenderQualityPage
*/
BOOL RenderQualityPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMainWindow pv = (PCMainWindow)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // check the current resolution
         PCEscControl pControl;

         ComboBoxSet (pPage, L"resolution", (DWORD)(pv->m_iResolution + 4));
         ComboBoxSet (pPage, L"texturedetail", (DWORD)(pv->m_iTextureDetail + 2));
         ComboBoxSet (pPage, L"serverspeed", pv->m_dwServerSpeed);
         ComboBoxSet (pPage, L"movementtransition", pv->m_dwMovementTransition);
         ComboBoxSet (pPage, L"artstyle", pv->m_dwArtStyle);

         if (pControl = pPage->ControlFind (L"nolipsync"))
            pControl->AttribSetBOOL (Checked(), !pv->m_fLipSync);
         if (pControl = pPage->ControlFind (L"twopass"))
            pControl->AttribSetBOOL (Checked(), (pv->m_dwShadowsFlags & SF_TWOPASS360) ? TRUE : FALSE);
         if (pControl = pPage->ControlFind (L"noshadows"))
            pControl->AttribSetBOOL (Checked(), (pv->m_dwShadowsFlags & SF_NOSHADOWS) ? TRUE : FALSE);
         if (pControl = pPage->ControlFind (L"nospecularity"))
            pControl->AttribSetBOOL (Checked(), (pv->m_dwShadowsFlags & SF_NOSPECULARITY) ? TRUE : FALSE);
         if (pControl = pPage->ControlFind (L"nosupersample"))
            pControl->AttribSetBOOL (Checked(), (pv->m_dwShadowsFlags & SF_NOSUPERSAMPLE) ? TRUE : FALSE);
         if (pControl = pPage->ControlFind (L"lowtransparency"))
            pControl->AttribSetBOOL (Checked(), (pv->m_dwShadowsFlags & SF_LOWTRANSPARENCY) ? TRUE : FALSE);
         if (pControl = pPage->ControlFind (L"nobump"))
            pControl->AttribSetBOOL (Checked(), (pv->m_dwShadowsFlags & SF_NOBUMP) ? TRUE : FALSE);
         if (pControl = pPage->ControlFind (L"lowdetail"))
            pControl->AttribSetBOOL (Checked(), (pv->m_dwShadowsFlags & SF_LOWDETAIL) ? TRUE : FALSE);
         if (pControl = pPage->ControlFind (L"texturesonly"))
            pControl->AttribSetBOOL (Checked(), (pv->m_dwShadowsFlags & SF_TEXTURESONLY) ? TRUE : FALSE);

      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"resolution")) {
            int iVal = (int)(p->pszName ? _wtoi(p->pszName) : 0) - 4;
            if (iVal == pv->m_iResolution)
               return TRUE;

            pv->m_iResolution = iVal;
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"texturedetail")) {
            int iVal = (int)(p->pszName ? _wtoi(p->pszName) : 0) - 2;
            if (iVal == pv->m_iTextureDetail)
               return TRUE;

            pv->m_iTextureDetail = iVal;
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"serverspeed")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwServerSpeed)
               return TRUE;

            pv->m_dwServerSpeed = dwVal;
            pv->InfoForServer (L"graphspeed", NULL, pv->m_dwServerSpeed);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"movementtransition")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwMovementTransition)
               return TRUE;

            pv->m_dwMovementTransition = dwVal;
            pv->InfoForServer (L"movementtransition", NULL, (RegisterMode() != 0) ? pv->m_dwMovementTransition : 0);
               // if hasn't paid then no transition
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"artstyle")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwArtStyle)
               return TRUE;

            pv->m_dwArtStyle = dwVal;
            pv->InfoForServer (L"artstyle", NULL, pv->m_dwArtStyle);
               // if hasn't paid then no transition
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

         if (!_wcsicmp (psz, L"twopass")) {
            if (p->pControl->AttribGetBOOL (Checked()))
               pv->m_dwShadowsFlags |= SF_TWOPASS360;
            else
               pv->m_dwShadowsFlags &= ~SF_TWOPASS360;
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
         }
         else if (!_wcsicmp (psz, L"nolipsync")) {
            pv->m_fLipSync = !p->pControl->AttribGetBOOL (Checked());
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
         }
         else if (!_wcsicmp (psz, L"noshadows")) {
            if (p->pControl->AttribGetBOOL (Checked()))
               pv->m_dwShadowsFlags |= SF_NOSHADOWS;
            else
               pv->m_dwShadowsFlags &= ~SF_NOSHADOWS;
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
         }
         else if (!_wcsicmp (psz, L"nospecularity")) {
            if (p->pControl->AttribGetBOOL (Checked()))
               pv->m_dwShadowsFlags |= SF_NOSPECULARITY;
            else
               pv->m_dwShadowsFlags &= ~SF_NOSPECULARITY;
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
         }
         else if (!_wcsicmp (psz, L"nosupersample")) {
            if (p->pControl->AttribGetBOOL (Checked())) {
               pv->m_dwShadowsFlags |= SF_NOSUPERSAMPLE;

               // BUGFIX - also modify shadowsflags low
               pv->m_dwShadowsFlagsLow |= SF_NOSUPERSAMPLE;
            }
            else {
               pv->m_dwShadowsFlags &= ~SF_NOSUPERSAMPLE;

               // BUGFIX - also modify shadowsflags low
               pv->m_dwShadowsFlagsLow &= ~SF_NOSUPERSAMPLE;
            }
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
         }
         else if (!_wcsicmp (psz, L"lowtransparency")) {
            if (p->pControl->AttribGetBOOL (Checked()))
               pv->m_dwShadowsFlags |= SF_LOWTRANSPARENCY;
            else
               pv->m_dwShadowsFlags &= ~SF_LOWTRANSPARENCY;
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
         }
         else if (!_wcsicmp (psz, L"nobump")) {
            if (p->pControl->AttribGetBOOL (Checked()))
               pv->m_dwShadowsFlags |= SF_NOBUMP;
            else
               pv->m_dwShadowsFlags &= ~SF_NOBUMP;
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
         }
         else if (!_wcsicmp (psz, L"lowdetail")) {
            if (p->pControl->AttribGetBOOL (Checked()))
               pv->m_dwShadowsFlags |= SF_LOWDETAIL;
            else
               pv->m_dwShadowsFlags &= ~SF_LOWDETAIL;
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
         }
         else if (!_wcsicmp (psz, L"texturesonly")) {
            if (p->pControl->AttribGetBOOL (Checked()))
               pv->m_dwShadowsFlags |= SF_TEXTURESONLY;
            else
               pv->m_dwShadowsFlags &= ~SF_TEXTURESONLY;
            pv->m_pRT->ResolutionSet (pv->m_iResolution, pv->m_dwShadowsFlags, pv->m_iTextureDetail, pv->m_fLipSync,
               pv->m_iResolutionLow, pv->m_dwShadowsFlagsLow, pv->m_fLipSyncLow, TRUE);
            pv->VisImageReRenderAll ();
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Image quality (Advanced)";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}



/*************************************************************************************
CMainWindow::DialogRenderQuality - Displays the render quality dialog.

inputs
   PCEscWindow    pWindow - Window to base this off of

returns
   BOOL - TRUE if pressed back, FALSE if closed
*/
BOOL CMainWindow::DialogRenderQuality (PCEscWindow pWindow)
{
   PWSTR psz;
redo:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLRENDERQUALITYEASY, RenderQualityEasyPage, this);
   if (!psz)
      return FALSE;
   if (!_wcsicmp (psz, Back()))
      return TRUE;
   if (!_wcsicmp(psz, L"advanced"))
      goto advanced;
   if (!_wcsicmp(psz, RedoSamePage()))
      goto redo;
   return FALSE;

advanced:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLRENDERQUALITY, RenderQualityPage, this);
   if (!psz)
      return FALSE;
   if (!_wcsicmp(psz, RedoSamePage()))
      goto advanced;
   if (!_wcsicmp(psz, Back()))
      goto redo;
   return FALSE;
}




/*************************************************************************
SettingsPage
*/
BOOL SettingsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMainWindow pv = (PCMainWindow)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // enable/disable upload image page
         if (pControl = pPage->ControlFind (L"uploadimage"))
            pControl->Enable (pv->m_dwUploadImageLimitsNum && (RegisterMode() == 1));

         // voice chat sttings
         if (pControl = pPage->ControlFind (L"voicechat"))
            pControl->Enable (pv->m_VoiceChatInfo.m_fAllowVoiceChat && RegisterMode());

         if (pControl = pPage->ControlFind (L"randomactions"))
            pControl->AttribSetBOOL (Checked(), pv->m_fRandomActions);
         DoubleToControl (pPage, L"randomtime", pv->m_fRandomTime);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp (psz, L"randomtime")) {
            pv->m_fRandomTime = DoubleFromControl (pPage, L"randomtime");
            pv->m_fRandomTime = max(pv->m_fRandomTime, 2.0);   // maximum every 2 seconds

            if (pv->m_fRandomActions) {
               KillTimer (pv->m_hWndPrimary, TIMER_RANDOMACTIONS);
               SetTimer (pv->m_hWndPrimary, TIMER_RANDOMACTIONS, (int)(1000.0 * pv->m_fRandomTime), NULL);
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

         if (!_wcsicmp(psz, L"randomactions")) {
            pv->m_fRandomActions = p->pControl->AttribGetBOOL (Checked());

            if (pv->m_fRandomActions)
               SetTimer (pv->m_hWndPrimary, TIMER_RANDOMACTIONS, (int)(1000.0 * pv->m_fRandomTime), NULL);
            else
               KillTimer (pv->m_hWndPrimary, TIMER_RANDOMACTIONS);

            return TRUE;
         }
         if (!_wcsicmp(psz, L"screenshot")) {
            // remember the control key
            BOOL fControl = (GetKeyState (VK_CONTROL) < 0);

            // find the main image
            PCVisImage pvi = pv->FindMainVisImage ();
            PCImageStore pis = pvi ? pvi->LayerGetImage() : NULL;
            if (!pis) {
               pPage->MBWarning (L"The screenshot couldn't be taken.",
                  L"The main display window isn't showing an image at the moment.");

               return TRUE;
            }

            HBITMAP hBit = NULL;
            HDC hDC = GetDC (pv->m_hWndPrimary);
            if ((pis->m_dwStretch == 4) && !fControl) {
               // want to get portion of the 3D image
               fp fOldFOV, fOldLat, fOldCurvature;
               DWORD dwOldWidth, dwOldHeight;
               pis->Surround360Get (0, &fOldFOV, &fOldLat, &dwOldWidth, &dwOldHeight, &fOldCurvature);

               // determine the resolution
               DWORD dwWidth, dwHeight;
               fp fImageRes = pow (2.0, (fp)pv->m_iResolution / 2.0);

               // BUGFIX - If there's blurring then double image res
               if (!(pv->m_dwShadowsFlags & SF_NOSUPERSAMPLE))
                  fImageRes *= 2.0;

               RenderSceneAspectToPixelsInt (1 /*16:9*/, fImageRes, &dwWidth, &dwHeight);

               pis->Surround360Set (0, pv->m_f360FOV, pv->m_f360Lat, dwWidth, dwHeight, pv->m_fCurvature);
               pis->CacheSurround360 (0, pv->m_f360Long);

               PCImageStore pCache = pis->CacheGet (0, dwWidth, dwHeight, NULL, pv->m_f360Long, pv->m_cBackground);
               
               hBit = pCache ? pCache->ToBitmap (hDC) : NULL;

               // restore
               pis->Surround360Set (0, fOldFOV, fOldLat, dwOldWidth, dwOldHeight, fOldCurvature);
            }
            else
               // entire image
               hBit = pis->ToBitmap (hDC);
            ReleaseDC (pv->m_hWndPrimary, hDC);

            if (!hBit) {
               pPage->MBWarning (L"The screenshot couldn't be taken.");
               return TRUE;
            }

            // get the filename
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
            if (!GetSaveFileName(&ofn)) {
               DeleteObject (hBit);
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

            pPage->MBInformation (L"The image has been saved.");

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Settings and options";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFADMINOPTIONS")) {
            p->pszSubString = pv->m_fSettingsControl ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFADMINOPTIONS")) {
            p->pszSubString = pv->m_fSettingsControl ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"VOICECHAT")) {
            if (!pv->m_VoiceChatInfo.m_fAllowVoiceChat) {
               p->pszSubString = L"<p/><italic>Currently disabled by the virtual world you're playing in.</italic>";
               return TRUE;
            }
            else switch (RegisterMode()) {
               case 0:  // disable
                  p->pszSubString = gpszSettingsDisabled;
                  return TRUE;
               case -1:  // temporarily enabled
                  p->pszSubString = gpszSettingsTempEnabled;
                  return TRUE;
               case 1:  // enabled
                  // do nothing
                  break;
            } // switch
         }
         else if (!_wcsicmp(p->pszSubName, L"UPLOADIMAGE")) {
            if (!pv->m_dwUploadImageLimitsNum) {
               p->pszSubString = L"<p/><italic>Currently disabled by the virtual world you're playing in.</italic>";
               return TRUE;
            }
            else switch (RegisterMode()) {
               case 0:  // disable
               case -1:  // temporarily enabled
                  p->pszSubString = gpszSettingsDisabled;
                  return TRUE;
               case 1:  // enabled
                  // do nothing
                  break;
            } // switch
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CMainWindow::DialogSettings - General settings dialog

inputs
   DWORD       dwPage - If 0 then display main MML settings, 1 then render quality settings, 2 then speech settings
returns
   BOOL - TRUE if pressed back. FALSE if cancelled
*/
BOOL CMainWindow::DialogSettings (DWORD dwPage)
{
   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation2 (m_hWndPrimary, &r);
   CEscWindow Window;
   Window.Init (ghInstance, m_hWndPrimary, EWS_FIXEDSIZE /*| EWS_AUTOHEIGHT*/, &r);
   EscWindowFontScaleByScreenSize (&Window);

   m_fSettingsControl = (GetKeyState (VK_CONTROL) < 0);

redo:
   switch (dwPage) {
   case 0:  // main page
   default:
      psz = Window.PageDialog (ghInstance, IDR_MMLSETTINGS, SettingsPage, this);
      break;
   case 1:  // Render quality
      psz = DialogRenderQuality (&Window) ? Back() : NULL;
      break;
   case 2:  // voice quality
      psz = DialogSpeech (&Window) ? Back() : NULL;
      break;
   }
   if (!psz)
      return FALSE;
   else if (!_wcsicmp (psz, Back()))
      return TRUE;
   else if (!_wcsicmp (psz, L"renderquality")) {
      if (DialogRenderQuality (&Window))
         goto redo;
      else
         return FALSE;  // cancelled
   }
   else if (!_wcsicmp (psz, L"layout")) {
      if (DialogLayout (&Window))
         goto redo;
      else
         return FALSE;  // cancelled
   }
   else if (!_wcsicmp (psz, L"speech")) {
      if (DialogSpeech (&Window))
         goto redo;
      else
         return FALSE;  // cancelled
   }
   else if (!_wcsicmp (psz, L"voicechat")) {
      // if PTT down then stop
      VoiceChatStop ();

      // temporarily disable recordng
      m_fTempDisablePTT = TRUE;

      BOOL fRet = m_VoiceDisguise.Dialog (&Window, NULL, m_VoiceChatInfo.m_lWAVEBASECHOICE.Num(),
         (PWAVEBASECHOICE) m_VoiceChatInfo.m_lWAVEBASECHOICE.Get(0), 0);

      // re-enable temporarily disable recordng
      m_fTempDisablePTT = FALSE;

      // invalidate the VC menu
      m_pSlideTop->InvalidatePTT();

      // loop back
      if (fRet)
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp (psz, L"uploadimage")) {
      if (DialogUploadImage (&Window))
         goto redo;
      else
         return FALSE;  // cancelled
   }

   // else, quit
   return FALSE;
}


/*************************************************************************
UploadImagePage
*/
BOOL UploadImagePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMainWindow pv = (PCMainWindow)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (_wcsicmp(psz, L"selectfile"))
            break;   // only care about selectfile

         // get the number
         DWORD dwNum = (DWORD)DoubleFromControl (pPage, L"num");
         if (!dwNum || (dwNum > pv->m_dwUploadImageLimitsNum)) {
            pPage->MBWarning (L"The image slot number you have select is too high.",
               L"Please type in one that's within the limits.");
            return TRUE;
         }

         // get the file name
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
         ofn.lpstrFilter = "Image files (*.jpg;*.bmp)\0*.jpg;*.bmp\0\0\0";
         ofn.lpstrFile = szTemp;
         ofn.nMaxFile = sizeof(szTemp);
         ofn.lpstrTitle = "Select image file";
         ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
         ofn.lpstrDefExt = ".jpg";
         // nFileExtension 

         if (!GetOpenFileName(&ofn))
            return TRUE;   // failed to specify file so go back

         //WCHAR szw[256];
         //MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/2);

         {
            CImage Image;

            HBITMAP hBmp = JPegOrBitmapLoad(szTemp, TRUE);
            if (!hBmp) {
               pPage->MBWarning (L"The image file couldn't be opened.",
                  L"It may not be a proper bitmap (.bmp) or JPEG (.jpg) file.");
               return TRUE;   // error
            }

            if (!Image.Init (hBmp)) {
               DeleteObject (hBmp);
               return TRUE; // error, unlikely
            }
            DeleteObject (hBmp);


            // make sure it's not too large
            DWORD dwWidth = Image.Width();
            DWORD dwHeight = Image.Height();
            if ( (dwWidth > pv->m_dwUploadImageLimitsMaxWidth) ||
               (dwHeight > pv->m_dwUploadImageLimitsMaxHeight) ) {
               pPage->MBWarning (L"The image file is too large.",
                  L"Use a paint program to shrink the image down so it fits "
                  L"within the specified width and height (in pixels).");
               return TRUE;   // error
            }

            // create the binary for this... a DWORD with num, DWORD with width, a DWORD with height,
            // and 3-bytes per pixel
            CMem mem;
            DWORD dwNeed = sizeof(DWORD)*3 + 3 * dwWidth * dwHeight;
            if (!mem.Required (dwNeed))
               return TRUE;   // error
            DWORD *pdw = (DWORD*) mem.p;
            pdw[0] = dwNum;
            pdw[1] = dwWidth;
            pdw[2] = dwHeight;
            PBYTE pb = (PBYTE) (pdw + 3);

            PIMAGEPIXEL pip = Image.Pixel (0,0);
            DWORD i;
            for (i = 0; i < dwWidth * dwHeight; i++, pip++, pb += 3) {
               pb[0] = UnGamma (pip->wRed);
               pb[1] = UnGamma (pip->wGreen);
               pb[2] = UnGamma (pip->wBlue);
            } // i

            if (pv->m_pIT && pv->m_pIT->m_pPacket) {
               if (!pv->m_pIT->m_pPacket->PacketSend (CIRCUMREALITYPACKET_UPLOADIMAGE, mem.p, dwNeed)) {
                  int iErr;
                  PCWSTR pszErr;
                  iErr = pv->m_pIT->m_pPacket->GetFirstError (&pszErr);
                  pv->PacketSendError(iErr, pszErr);
               }
            }

            pPage->Exit (Back());
            return TRUE;
         } // to store the image

      } // if buttonpress
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         static WCHAR szTemp[64];

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Upload an image of your character";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"NUM")) {
            swprintf (szTemp, L"%d", (int)pv->m_dwUploadImageLimitsNum);
            p->pszSubString = szTemp;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MAXWIDTH")) {
            swprintf (szTemp, L"%d", (int)pv->m_dwUploadImageLimitsMaxWidth);
            p->pszSubString = szTemp;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MAXHEIGHT")) {
            swprintf (szTemp, L"%d", (int)pv->m_dwUploadImageLimitsMaxHeight);
            p->pszSubString = szTemp;
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}



/*************************************************************************************
CMainWindow::DialogUploadImage - Displays the render quality dialog.
*/
BOOL CMainWindow::DialogUploadImage (PCEscWindow pWindow)
{
   PWSTR psz;

redo:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLUPLOADIMAGE, UploadImagePage, this);
   if (!psz)
      return FALSE;
   if (!_wcsicmp(psz, Back()))
      return TRUE;
   if (!_wcsicmp(psz, RedoSamePage()))
      goto redo;

   return FALSE;
}




/*************************************************************************
LayoutPage
*/
BOOL LayoutPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMainWindow pv = (PCMainWindow)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         if (pControl = pPage->ControlFind (L"hidetabs"))
            pControl->AttribSetBOOL (Checked(), !pv->m_fSlideLocked);

         if (pControl = pPage->ControlFind (L"minimizeifswitch"))
            pControl->AttribSetBOOL (Checked(), pv->m_fMinimizeIfSwitch);

         if (pControl = pPage->ControlFind (L"showtitle"))
            pControl->AttribSetBOOL (Checked(), pv->ChildHasTitle (pv->m_hWndPrimary));

         if (pControl = pPage->ControlFind (L"secondmonitor")) {
            pControl->Enable ((gdwMonitorNum > 1) && RegisterMode());
            pControl->AttribSetBOOL (Checked(), gfMonitorUseSecond);
         }

         if (pControl = pPage->ControlFind (L"customizetoolbar"))
            pControl->Enable (pv->m_pSlideTop->ToolbarIsVisible());
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp (psz, L"hidetabs")) {
            pv->m_fSlideLocked = pv->m_afSlideLocked[pv->m_pSlideTop->m_dwTab] = !p->pControl->AttribGetBOOL(Checked());

            // resize everything
            pv->ChildShowMoveAll ();

            // tell windows
            if (pv->m_pSlideTop)
               pv->m_pSlideTop->Locked (pv->m_fSlideLocked);
            if (pv->m_pSlideBottom)
               pv->m_pSlideBottom->Locked (pv->m_fSlideLocked);
            
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"minimizeifswitch")) {
            pv->m_fMinimizeIfSwitch = p->pControl->AttribGetBOOL(Checked());
            
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"showtitle")) {
            BOOL fShowTitle = !pv->ChildHasTitle (pv->m_hWndPrimary);
            pv->ChildTitleShowHide (fShowTitle, pv->m_hWndPrimary, 0, NULL);
            if (pv->m_hWndSecond)
               pv->ChildTitleShowHide (fShowTitle, pv->m_hWndSecond, 0, NULL);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"secondmonitor")) {
            // show/hide second monitor
            pv->SecondCreateDestroy (!gfMonitorUseSecond, TRUE);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"customizetoolbar")) {
            if (!pv->m_pResVerb || !pv->m_pSlideTop->ToolbarIsVisible())
               return TRUE;

            // come up with a default language
            LANGID lid = DEFLANGID;
            PCResVerbIcon *ppr = (PCResVerbIcon*)pv->m_pResVerb->m_lPCResVerbIcon.Get(0);
            if (ppr)
               lid = ppr[0]->m_lid;

            pv->m_pResVerb->Edit (pPage->m_pWindow->m_hWnd, lid, FALSE, NULL, TRUE, pv->m_pResVerbSent);

            pv->m_pSlideTop->ToolbarArrange ();
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"resetwindowlocs")) {
            int iRet = pPage->MBYesNo (L"Do you want to restore all the tabs at once?",
               L"Press \"Yes\" if you wish to restore all the tabs at once. \"No\" will only restore this tab.", TRUE);
            if ((iRet != IDYES) && (iRet != IDNO))
               return TRUE;   // pressed cancel

            DWORD dwTabIndex = pv->m_pSlideTop->m_dwTab + (gfMonitorUseSecond ? NUMTABS : 0);
            DWORD dwStart = (iRet == IDYES) ? 0 : dwTabIndex;
            DWORD dwEnd = (iRet == IDYES) ? NUMTABMONITORS : (dwTabIndex+1);
            DWORD i;
            for (i = dwStart; i < dwEnd; i++) {
               pv->m_alTABWINDOW[i].Clear();
               pv->m_alTabWindowString[i].Clear();
            }
            pv->ChildShowMoveAll ();

            pPage->MBInformation (L"The window/pane locations have been restored to their original locations.");

            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         static WCHAR szTemp[64];

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Layout settings";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"SECONDMONITOR")) {
            if (gdwMonitorNum <= 1) {
               p->pszSubString = L"<p/><italic>Currently disabled because you only have one monitor plugged in.</italic>";
               return TRUE;
            }
            else switch (RegisterMode()) {
               case 0:  // disable
                  p->pszSubString = gpszSettingsDisabled;
                  return TRUE;
               case -1:  // temporarily enabled
                  p->pszSubString = gpszSettingsTempEnabled;
                  return TRUE;
               case 1:  // enabled
                  // do nothing
                  break;
            } // switch
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}

/*************************************************************************************
CMainWindow::DialogLayout - Displays the layout page.
*/
BOOL CMainWindow::DialogLayout (PCEscWindow pWindow)
{
   PWSTR psz;

redo:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLLAYOUT, LayoutPage, this);
   if (!psz)
      return FALSE;
   if (!_wcsicmp(psz, Back()))
      return TRUE;
   if (!_wcsicmp(psz, RedoSamePage()))
      goto redo;

   return FALSE;
}





/*************************************************************************
SpeechPage
*/
BOOL SpeechPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMainWindow pv = (PCMainWindow)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ComboBoxSet (pPage, L"subtitlesize", pv->m_dwSubtitleSize);
         ComboBoxSet (pPage, L"transcriptsize", (DWORD)(pv->m_iTransSize + 2) );
         ComboBoxSet (pPage, L"speakrate", (DWORD)(pv->m_aiSpeakSpeed[pv->m_pSlideTop->m_dwTab]+2));
         ComboBoxSet (pPage, L"ttsquality", (DWORD)pv->m_iTTSQuality);

         PCEscControl pControl;
         if (pControl = pPage->ControlFind (L"recorddir"))
            pControl->AttribSet (Text(), pv->m_szRecordDir);
         if (pControl = pPage->ControlFind (L"muteall"))
            pControl->AttribSetBOOL (Checked(), pv->m_afMuteAll[pv->m_pSlideTop->m_dwTab]);
         if (pControl = pPage->ControlFind (L"subtitlespeech"))
            pControl->AttribSetBOOL (Checked(), pv->m_fSubtitleSpeech);
         if (pControl = pPage->ControlFind (L"transcripthistory"))
            pControl->AttribSetBOOL (Checked(), (pv->m_adwTransShow[pv->m_pSlideTop->m_dwTab] & 0x02) ? TRUE : FALSE);
         if (pControl = pPage->ControlFind (L"subtitletext"))
            pControl->AttribSetBOOL (Checked(), pv->m_fSubtitleText);
         if (pControl = pPage->ControlFind (L"ttsautomute"))
            pControl->AttribSetBOOL (Checked(), pv->m_fTTSAutomute);
         if (pControl = pPage->ControlFind (L"ttsenablepcm"))
            pControl->AttribSetBOOL (Checked(), !pv->m_fDisablePCM);

         WCHAR szTemp[64];
         swprintf (szTemp, L"qual%d", (int)pv->m_iTextVsSpeech);
         if (pControl = pPage->ControlFind (szTemp))
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         // BUGFIX - Make font and speaking rate be buttons instead of drop-down since
         // drop-down crashes if change while open
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszQual = L"qual";
         DWORD dwQualLen = (DWORD)wcslen(pszQual);

         if (!_wcsnicmp(psz, pszQual, dwQualLen)) {
            int iVal = _wtoi(psz + dwQualLen);

            // update all
            pv->TextVsSpeechSet (iVal, TRUE);

            // refresh buttons
            PCEscControl pControl;
            if (pControl = pPage->ControlFind (L"subtitlespeech"))
               pControl->AttribSetBOOL (Checked(), pv->m_fSubtitleSpeech);
            if (pControl = pPage->ControlFind (L"subtitletext"))
               pControl->AttribSetBOOL (Checked(), pv->m_fSubtitleText);
            if (pControl = pPage->ControlFind (L"transcripthistory"))
               pControl->AttribSetBOOL (Checked(), (pv->m_adwTransShow[pv->m_pSlideTop->m_dwTab] & 0x02) ? TRUE : FALSE);
            if (pControl = pPage->ControlFind (L"muteall"))
               pControl->AttribSetBOOL (Checked(), pv->m_afMuteAll[pv->m_pSlideTop->m_dwTab]);

            return TRUE;
         }
         if (!_wcsicmp(psz, L"muteall")) {
            BOOL fMute = p->pControl->AttribGetBOOL(Checked());

            pv->m_fMuteAll = fMute;
            DWORD i;
            for (i = 0; i < NUMTABS; i++)
               pv->m_afMuteAll[i] = pv->m_fMuteAll;

            pv->InfoForServer (L"muteall", NULL, pv->m_fMuteAll);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"transcripthistory")) {
            BOOL fChecked = p->pControl->AttribGetBOOL(Checked());

            DWORD dwBit = 1 << 1;

            if (fChecked)
               pv->m_dwTransShow |= dwBit;
            else
               pv->m_dwTransShow &= ~dwBit;

            pv->m_adwTransShow[pv->m_pSlideTop->m_dwTab] = pv->m_dwTransShow;

            // since changing mute changes display of page, refresh
            PostMessage (pv->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSUPDATE, 0, 0);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"subtitlespeech")) {
            pv->m_fSubtitleSpeech = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"subtitletext")) {
            pv->m_fSubtitleText = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"ttsautomute")) {
            pv->m_fTTSAutomute = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"ttsenablepcm")) {
            pv->m_fDisablePCM = !p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"subtitlesize")) {
            int iVal = (int)(p->pszName ? _wtoi(p->pszName) : 0);
            if (iVal == (int)pv->m_dwSubtitleSize)
               return TRUE;

            pv->m_dwSubtitleSize = (DWORD)iVal;
            // pv->m_Subtitle.FontSet (pv->m_dwSubtitleSize);
            if (pv->m_pTickerTape)
               pv->m_pTickerTape->FontSet (pv->m_dwSubtitleSize);
            if (iVal) {
               GUID g;
               g = GUID_NULL;
               //pv->m_Subtitle.Phrase (&g, L"", L"New subtitle size.", FALSE);
               //pv->m_Subtitle.Phrase (&g, L"", L"New subtitle size.", TRUE);
               if (pv->m_pTickerTape) {
                  pv->m_pTickerTape->Phrase (&g, L"", L"New subtitle size.", FALSE);
                  pv->m_pTickerTape->Phrase (&g, L"", L"New subtitle size.", TRUE);
               }
            }

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"transcriptsize")) {
            int iVal = (int)(p->pszName ? _wtoi(p->pszName) : 0) - 2;
            if (iVal == (int)pv->m_iTransSize)
               return TRUE;

            pv->m_iTransSize = iVal;

            // update main window so redraws description text on it
            PCDisplayWindow pMain = pv->FindMainDisplayWindow ();
            if (pMain)
               InvalidateRect (pMain->m_hWnd, NULL, FALSE);

            pv->m_iTransSize = iVal;
            PostMessage (pv->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSUPDATE, 0, 0);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"speakrate")) {
            int iVal = (int)(p->pszName ? _wtoi(p->pszName) : 0) - 2;
            if (iVal == (int)pv->m_aiSpeakSpeed[pv->m_pSlideTop->m_dwTab])
               return TRUE;

            EnterCriticalSection (&pv->m_crSpeakBlacklist);
            pv->m_iSpeakSpeed = pv->m_aiSpeakSpeed[pv->m_pSlideTop->m_dwTab] = iVal;
            LeaveCriticalSection (&pv->m_crSpeakBlacklist);

            pv->InfoForServer (L"speakspeed", NULL, pv->m_iSpeakSpeed);


            // update the transcript so the checkboxes are OK
            pv->TranscriptUpdate ();

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"ttsquality")) {
            int iVal = (int)(p->pszName ? _wtoi(p->pszName) : 0);
            if (iVal == (int)pv->m_iTTSQuality)
               return TRUE;

            pv->m_iTTSQuality = iVal;

            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"RecordDir")) {
            pv->m_szRecordDir[0] = 0;
            DWORD dwNeeded;
            p->pControl->AttribGet (Text(), pv->m_szRecordDir, sizeof(pv->m_szRecordDir), &dwNeeded);

            // load from registry
            HKEY  hKey = NULL;
            DWORD dwDisp;
            RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
               KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
            if (hKey) {
               RegSetValueExW (hKey, L"RecordDir", 0, REG_SZ, (LPBYTE) pv->m_szRecordDir, ((DWORD)wcslen(pv->m_szRecordDir)+1)*sizeof(WCHAR));
               RegCloseKey (hKey);
            }
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         static WCHAR szTemp[64];

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Speech settings";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LOWQUALITYVOICES")) {
            p->pszSubString = L"";
            if (pv->m_fAllSmall)
               p->pszSubString = L" <italic>(Because you have the small version "
                  L"of CircumReality installed, the text-to-speech voices won't sound "
                  L"that good.</italic>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TTSREGISTERED")) {
            BOOL fCheck;
            LargeTTSRequirements (&fCheck, NULL, NULL, NULL, NULL);
            p->pszSubString = fCheck ?
               L"<font color=#008000>You have paid</font>" :
               L"<font color=#800000>You have NOT paid</font>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TTSCIRC64")) {
            BOOL fCheck;
            LargeTTSRequirements (NULL, NULL, &fCheck, NULL, NULL);
            p->pszSubString = fCheck ?
               L"<font color=#008000>You are running it</font>" :
               L"<font color=#800000>You should download the 64-bit version</font>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TTSWIN64")) {
            BOOL fCheck;
            LargeTTSRequirements (NULL, &fCheck, NULL, NULL, NULL);
            p->pszSubString = fCheck ?
               L"<font color=#008000>You are running it</font>" :
               L"<font color=#800000>You are NOT running Windows 64</font>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TTSDUALCORE")) {
            BOOL fCheck;
            LargeTTSRequirements (NULL, NULL, NULL, &fCheck, NULL);
            p->pszSubString = fCheck ?
               L"<font color=#008000>Your computer is fast enough</font>" :
               L"<font color=#800000>Your computer is NOT fast enough</font>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TTSRAM")) {
            BOOL fCheck;
            LargeTTSRequirements (&fCheck, NULL, NULL, NULL, NULL);
            p->pszSubString = fCheck ?
               L"<font color=#008000>Your computer has enough memory</font>" :
               L"<font color=#800000>Your computer does NOT have enough memory</font>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFRECORDDIR") || !_wcsicmp(p->pszSubName, L"ENDIFRECORDDIR")) {
            // load from registry
            HKEY  hKey = NULL;
            DWORD dwDisp;
            RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
               KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
            BOOL fEnableRecord = FALSE;
            if (hKey) {
               DWORD dwSize, dwType;
               dwSize = sizeof(fEnableRecord);
               RegQueryValueEx (hKey, "EnableClientTools", NULL, &dwType, (LPBYTE) &fEnableRecord, &dwSize);
               RegCloseKey (hKey);
            }

            if (fEnableRecord)
               p->pszSubString = L"";
            else
               p->pszSubString = (!_wcsicmp(p->pszSubName, L"IFRECORDDIR")) ? L"<comment>" : L"</comment>";

            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}

/*************************************************************************************
CMainWindow::DialogSpeech - Displays the speech settings page.
*/
BOOL CMainWindow::DialogSpeech (PCEscWindow pWindow)
{
   PWSTR psz;

redo:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLSPEECH, SpeechPage, this);
   if (!psz)
      return FALSE;
   if (!_wcsicmp(psz, Back()))
      return TRUE;
   if (!_wcsicmp(psz, RedoSamePage()))
      goto redo;

   return FALSE;
}

/**********************************************************************************
CMainWindow::RecordCallback - Handles a call from the waveInOpen function
*/
static void CALLBACK RecordCallback (
  HWAVEIN hwi,       
  UINT uMsg,         
  LONG_PTR dwInstance,  
  LONG_PTR dwParam1,    
  LONG_PTR dwParam2     
)
{
   PCMainWindow pri = (PCMainWindow) dwInstance;
   pri->RecordCallback (hwi, uMsg, dwParam1, dwParam2);
}

void CMainWindow::RecordCallback (HWAVEIN hwi,UINT uMsg,         
  LONG_PTR dwParam1, LONG_PTR dwParam2)
{
   // do something with the buffer
   if (uMsg == MM_WIM_DATA) {
      // BUGFIX - Wrap around critical section so dont reenter self
      EnterCriticalSection (&m_csVCStopped);

      if (!m_fVCStopped) {
         PWAVEHDR pwh = (WAVEHDR*) dwParam1;

         DWORD dwBytesRecorded = pwh->dwBytesRecorded;
         PBYTE pabData = (PBYTE)pwh->lpData;

         if (m_dwVCTossOut) {
            if (m_dwVCTossOut > dwBytesRecorded) {
               dwBytesRecorded -= dwBytesRecorded;
               dwBytesRecorded = 0;
            }
            else {
               dwBytesRecorded -= m_dwVCTossOut;
               pabData += m_dwVCTossOut;
               m_dwVCTossOut = 0;
            }
         }

         // add to wave
         DWORD dwBlockSize = m_VCWave.m_dwChannels * 2; // 16-bit data
         DWORD dwNewSamples = dwBytesRecorded / dwBlockSize;

         if (dwNewSamples * dwBlockSize < dwBytesRecorded) {
            m_dwVCTossOut += dwBlockSize - (dwBytesRecorded - dwNewSamples * dwBlockSize);
         }
         m_VCWave.AppendPCMAudio (pabData, dwNewSamples);

         // send it out again
         waveInAddBuffer (m_hVCWaveIn, pwh, sizeof(WAVEHDR));
      }
      else
         m_dwVCWaveOut--;

      // BUGFIX - Wrap around critical section so dont reenter self
      LeaveCriticalSection (&m_csVCStopped);

   }
}



/*************************************************************************************
CMainWindow::VoiceChatStart - Start recording voice chat

returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::VoiceChatStart (void)
{
   // if already recording then do nothnig
   if (m_fVCRecording)
      return TRUE;

   // stop audio
   if (m_pAT) {
      m_pAT->Mute (TRUE);
      Sleep (20); // give it time to stop
   }

   // clear out the wave, just in case
   m_VCWave.BlankWaveToSize (0, TRUE);
   m_lVCEnergy.Clear();
   m_fVCHavePreviouslySent = FALSE;
   m_fVCStopped = FALSE;

   // allocate memory to write to
   DWORD dwSingleBuf;
   dwSingleBuf = (DWORD) (m_VCWave.m_dwSamplesPerSec / 8) * m_VCWave.m_dwChannels * sizeof(short);
   if (!m_memVCWave.Required (dwSingleBuf * RECORDBUF))
      return FALSE;
   memset (m_memVCWave.p, 0, dwSingleBuf * RECORDBUF);

   // open the wave device
   MMRESULT mm;
   WAVEFORMATEX WFEX;
   memset (&WFEX, 0, sizeof(WFEX));
   WFEX.cbSize = 0;
   WFEX.wFormatTag = WAVE_FORMAT_PCM;
   WFEX.nChannels = m_VCWave.m_dwChannels;
   WFEX.nSamplesPerSec = m_VCWave.m_dwSamplesPerSec;
   WFEX.wBitsPerSample = 16;
   WFEX.nBlockAlign  = WFEX.nChannels * WFEX.wBitsPerSample / 8;
   WFEX.nAvgBytesPerSec = WFEX.nBlockAlign * WFEX.nSamplesPerSec;
   mm = waveInOpen (&m_hVCWaveIn, WAVE_MAPPER, &WFEX, (DWORD_PTR) ::RecordCallback,
      (DWORD_PTR) this, CALLBACK_FUNCTION);
   m_dwVCTossOut = 0;
   if (mm)
      return FALSE;

   // prepare all the headers
   DWORD i;
   memset (m_aVCWaveHdr, 0, sizeof(m_aVCWaveHdr));
   for (i = 0; i < RECORDBUF; i++) {
      m_aVCWaveHdr[i].dwBufferLength = dwSingleBuf;
      m_aVCWaveHdr[i].lpData = (LPSTR) ((PBYTE) m_memVCWave.p + i * dwSingleBuf);
      mm = waveInPrepareHeader (m_hVCWaveIn, &m_aVCWaveHdr[i], sizeof(WAVEHDR));
   }

   // add all the buffers
   for (i = 0; i < RECORDBUF; i++) {
      m_dwVCWaveOut++;
      mm = waveInAddBuffer (m_hVCWaveIn, &m_aVCWaveHdr[i], sizeof(WAVEHDR));
   }

   mm = waveInStart (m_hVCWaveIn);

   // start the timer
   SetTimer (m_hWndPrimary, TIMER_VOICECHAT, VCTIMER_INTERVAL, NULL);
   m_fVCRecording = TRUE;
   m_dwVCTimeRecording = 0;

   return TRUE;
}



/*************************************************************************************
CMainWindow::VoiceChatStop - Stop recording voice chat

returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::VoiceChatStop (void)
{
   // if stopped then do nothing
   if (!m_fVCRecording)
      return TRUE;

   // kill the timer
   KillTimer (m_hWndPrimary, TIMER_VOICECHAT);

   // BUGFIX - Wrap around critical section so dont reenter self
   EnterCriticalSection (&m_csVCStopped);
   m_fVCStopped = TRUE;
   LeaveCriticalSection (&m_csVCStopped);
   waveInReset (m_hVCWaveIn);

   // wait for a short while and make sure no buffers out
   while (TRUE) {
      Sleep (10);
      EnterCriticalSection (&m_csVCStopped);
      if (!m_dwVCWaveOut) {
         LeaveCriticalSection (&m_csVCStopped);
         break;
      }
      LeaveCriticalSection (&m_csVCStopped);
   }


   // unprepare all the headers
   DWORD i;
   for (i = 0; i < RECORDBUF; i++)  // BUGFIX - missing loop
      waveInUnprepareHeader (m_hVCWaveIn, &m_aVCWaveHdr[i], sizeof(WAVEHDR));
   waveInClose (m_hVCWaveIn);

   // will need to send the last of the wave into VC to send
   if (m_VCWave.m_dwSamples) {
      PCM3DWave pCut = m_VCWave.Copy (0, m_VCWave.m_dwSamples);
      m_VCWave.BlankWaveToSize (0, TRUE);
      if (pCut)
         VoiceChatWaveSnippet (pCut);
   }
   
   // update the display
   m_pSlideTop->InvalidatePTT ();


   m_fVCRecording = FALSE;

   // restart audio
   if (m_pAT)
      m_pAT->Mute (FALSE);

   return TRUE;
}



/*************************************************************************************
CMainWindow::VoiceChatThreadFree - Free up the voice chat thread
*/
void CMainWindow::VoiceChatThreadFree (void)
{
   DWORD i;
   DWORD dwProcessors = VoiceChatHowManyProcessors();
   for (i = 0; i < dwProcessors; i++)
      if (m_aVCTHREADINFO[i].hThread)
         break;
   if (i >= dwProcessors)
      return;  

   // tell it to close
   EnterCriticalSection (&m_csVCThread);
   m_fVCThreadToShutDown = TRUE;
   for (i = 0; i < dwProcessors; i++)
      SetEvent (m_aVCTHREADINFO[i].hSignal);
   LeaveCriticalSection (&m_csVCThread);

   // wait for it to close
   for (i = 0; i < dwProcessors; i++) {
      if (!m_aVCTHREADINFO[i].hThread)
         continue;

      WaitForSingleObject (m_aVCTHREADINFO[i].hThread, INFINITE);
      CloseHandle (m_aVCTHREADINFO[i].hThread);
      CloseHandle (m_aVCTHREADINFO[i].hSignal);
      m_aVCTHREADINFO[i].hSignal = NULL;
      m_aVCTHREADINFO[i].hThread = NULL;
   }

   // NOTE: All the wave files will have been freed in the thead
}


/*************************************************************************************
CMainWindow::ImageLoadThreadFree - Free up the imageload threads thread
*/
void CMainWindow::ImageLoadThreadFree (void)
{
   DWORD i;
   for (i = 0; i < NUMIMAGECACHE; i++)
      if (m_aImageLoadThread[i].hThread)
         break;
   if (i >= NUMIMAGECACHE)
      return;  

   // tell it to close
   EnterCriticalSection (&m_csImageLoadThread);
   m_fImageLoadThreadToShutDown = TRUE;
   for (i = 0; i < NUMIMAGECACHE; i++)
      SetEvent (m_aImageLoadThread[i].hSignal);
   LeaveCriticalSection (&m_csImageLoadThread);

   // wait for it to close
   for (i = 0; i < NUMIMAGECACHE; i++) {
      if (!m_aImageLoadThread[i].hThread)
         continue;

      WaitForSingleObject (m_aImageLoadThread[i].hThread, INFINITE);
      CloseHandle (m_aImageLoadThread[i].hThread);
      CloseHandle (m_aImageLoadThread[i].hSignal);
      m_aImageLoadThread[i].hSignal = NULL;
      m_aImageLoadThread[i].hThread = NULL;
   }

   // clear up the lists, just in case
   DWORD j;
   for (i = 0; i < NUMIMAGECACHE; i++)
      for (j = 0; j < IMAGELOADPRIORITY; j++)
         m_alImageLoadThreadFile[i][j].Clear();

   // clear up wave
   PILTWAVEOUTCACHE pwc = (PILTWAVEOUTCACHE) m_lILTWAVEOUTCACHE.Get(0);
   for (i = 0; i < m_lILTWAVEOUTCACHE.Num(); i++, pwc++) {
      if (pwc->pszString)
         ESCFREE (pwc->pszString);
      if (pwc->pWave)
         delete pwc->pWave;
   } // i
   m_lILTWAVEOUTCACHE.Clear();
}


/*************************************************************************************
CMainWindow::VoiceChatThread - Callback for voicechat thread
*/
int CMainWindow::VoiceChatThread (DWORD dwThread)
{
   DWORD i;
   PCMMLNode2 *ppn;
   PCM3DWave *ppw;
   PCMMLNode2 pNode;
   PCM3DWave pWave;
   PCMem pMemCompress;
   DWORD dwTicket;

   while (TRUE) {
      // see if there's anything that should do
      EnterCriticalSection (&m_csVCThread);

      // see if want to shut down
      if (m_fVCThreadToShutDown) {
         // free up all the waves
         ppn = (PCMMLNode2*) m_lVCPCMMLNode2.Get(0);
         for (i = 0; i < m_lVCPCMMLNode2.Num(); i++)
            delete ppn[i];
         m_lVCPCMMLNode2.Clear();

         ppw = (PCM3DWave*) m_lVCPCM3DWave.Get(0);
         for (i = 0; i < m_lVCPCM3DWave.Num(); i++)
            delete ppw[i];
         m_lVCPCM3DWave.Clear();

         LeaveCriticalSection (&m_csVCThread);
         break;
      }

      // see if any waves
      if (!m_lVCPCM3DWave.Num()) {
         LeaveCriticalSection (&m_csVCThread);
         WaitForSingleObject (m_aVCTHREADINFO[dwThread].hSignal, INFINITE);
         continue;
      }

      // else, found a wave
      ppn = (PCMMLNode2*) m_lVCPCMMLNode2.Get(0);
      ppw = (PCM3DWave*) m_lVCPCM3DWave.Get(0);
      pNode = ppn[0];
      pWave = ppw[0];
      m_lVCPCMMLNode2.Remove (0);
      m_lVCPCM3DWave.Remove (0);
      dwTicket = m_dwVCTicketAvail;
      m_dwVCTicketAvail++;

#ifdef _DEBUG
      char szTemp[64];
      sprintf (szTemp, "\r\nPulled ticket %d", (int)dwTicket);
      OutputDebugString (szTemp);
#endif
      LeaveCriticalSection (&m_csVCThread);

      // process the wave
      pMemCompress = new CMem;
      if (!pMemCompress) {
         delete pWave;
         delete pNode;
         pNode = NULL;
         pWave = NULL;
         goto playticket;
      }
      if (!VoiceChatCompress (m_VoiceChatInfo.m_dwQuality, pWave, &m_VoiceDisguise, pMemCompress)) {
         // error
         delete pWave;
         delete pNode;
         delete pMemCompress;
         pNode = NULL;
         pMemCompress = NULL;
         pWave = NULL;
         goto playticket;
      }
      delete pWave;
      pWave = NULL;

playticket:
      // BUGFIX - Wait until ticket is available
      while (TRUE) {
         EnterCriticalSection (&m_csVCThread);
         if (dwTicket > m_dwVCTicketPlay) {
           LeaveCriticalSection (&m_csVCThread);
           Sleep (10);
           continue;
         }

         // else, playing
         break;
      }

      // HACK - To speak it out...  m_pAT->VoiceChat (pNode, pMemCompress);
      if (pMemCompress && pMemCompress->m_dwCurPosn) {
         // send the packet
         if (!m_pIT->m_pPacket->PacketSend (CIRCUMREALITYPACKET_VOICECHAT, pNode,
            pMemCompress->p, (DWORD)pMemCompress->m_dwCurPosn)) {
               int iErr;
               PCWSTR pszErr;
               iErr = m_pIT->m_pPacket->GetFirstError (&pszErr);
               PacketSendError(iErr, pszErr);
         }

         // note that have spoken it for the log
         // clear out the ID to NULL
         GUID g = GUID_NULL;
         g.Data4[0] = 4;   // just so have a value that can use for self
         MMLValueSet (pNode, L"ID", (PBYTE)&g, sizeof(g));
         TranscriptString (2, L"Yourself", NULL, NULL, NULL, pNode, pMemCompress);

      }

#ifdef _DEBUG
      sprintf (szTemp, "\r\nPlaying ticket %d", (int)dwTicket);
      OutputDebugString (szTemp);
#endif

      // update the ticket
      m_dwVCTicketPlay = dwTicket+1;
      LeaveCriticalSection (&m_csVCThread);


      if (pNode)
         delete pNode;
      if (pMemCompress)
         delete pMemCompress;
   } // while TRUE

   return 0;
}



/*************************************************************************************
CMainWindow::ImageLoadThreadAdd - Adds a file to be loaded by the thread. Adds
to loading all image qualities.

inputs
   PCWSTR            psz - File string to add
   DWORD             dwQuality - Quality, 0 .. NUMIMAGECACHE-1
   DWORD             dwPriority - Priority, 0..IMAGELOADPRIORITY-1
*/
void CMainWindow::ImageLoadThreadAdd (PCWSTR psz, DWORD dwQuality, DWORD dwPriority)
{
   DWORD i;
   PWSTR pszCur;
   EnterCriticalSection (&m_csImageLoadThread);
   if (!m_aImageLoadThread[dwQuality].hSignal)
      goto done;   // background load thread no longer exists, so don't load

   // make sure not already in th elist
   for (i = 0; i < m_alImageLoadThreadFile[dwQuality][dwPriority].Num(); i++) {
      pszCur = (PWSTR)m_alImageLoadThreadFile[dwQuality][dwPriority].Get(i);
      if (!_wcsicmp (pszCur, psz))
         break;   // found a match
   } // i
   if (i < m_alImageLoadThreadFile[dwQuality][dwPriority].Num())
      goto done;   // already on the list, so don't bother

   // if get here then add
   m_alImageLoadThreadFile[dwQuality][dwPriority].Add ((PVOID) psz, (wcslen(psz)+1)*sizeof(WCHAR));

   // notify the thread that want to add
   SetEvent (m_aImageLoadThread[dwQuality].hSignal);

done:
   LeaveCriticalSection (&m_csImageLoadThread);
}


/*************************************************************************************
CMainWindow::ImageLoadThread - Callback for voicechat thread
*/
int CMainWindow::ImageLoadThread (DWORD dwThread)
{
   CMem mem;
   DWORD dwPriority;
   while (TRUE) {
      WaitForSingleObject (m_aImageLoadThread[dwThread].hSignal, 250);
         // BUGFIX - Poll, so can ask the server for images

      // see if there's anything that should do
      EnterCriticalSection (&m_csImageLoadThread);

      // see if want to shut down
      if (m_fImageLoadThreadToShutDown) {
         // make sure cleaed
         for (dwPriority = 0; dwPriority < IMAGELOADPRIORITY; dwPriority++)
            m_alImageLoadThreadFile[dwThread][dwPriority].Clear();

         LeaveCriticalSection (&m_csImageLoadThread);
         break;
      }

      // see if any files to load
      while (TRUE) {
         for (dwPriority = 0; dwPriority < IMAGELOADPRIORITY; dwPriority++)
            if (m_alImageLoadThreadFile[dwThread][dwPriority].Num())
               break;
         if (dwPriority >= IMAGELOADPRIORITY)
            break;

         // see if want to shut down
         if (m_fImageLoadThreadToShutDown) {
            LeaveCriticalSection (&m_csImageLoadThread);
            break;   // exit to loop above, which will cause shutdown
         }

         // else, found a file to load
         PWSTR psz = (PWSTR) m_alImageLoadThreadFile[dwThread][dwPriority].Get(0);

         LeaveCriticalSection (&m_csImageLoadThread);
         // can do this because memory pointed to by the variable list won't change

         if (!m_amfImages[dwThread].Exists (psz)) {
            // it didn't exist, so remove and try to find another file
            EnterCriticalSection (&m_csImageLoadThread);
            m_alImageLoadThreadFile[dwThread][dwPriority].Remove (0);
            continue;
         }
         
         // once in awhile set the flag so will update last access time
         BOOL fOldInfo = m_amfImages[dwThread].m_fDontUpdateLastAccess;
         if ((GetTickCount() & 0xf00) == 0)
            m_amfImages[dwThread].m_fDontUpdateLastAccess = FALSE;

         // create the image
         PCImageStore pStore = new CImageStore;
         if (!pStore || !pStore->Init (&m_amfImages[dwThread], psz)) {
            if (pStore)
               delete pStore;

            m_amfImages[dwThread].m_fDontUpdateLastAccess = fOldInfo;

            // it didn't exist, so remove and try to find another file
            EnterCriticalSection (&m_csImageLoadThread);
            m_alImageLoadThreadFile[dwThread][dwPriority].Remove (0);
            continue;
         }
         m_amfImages[dwThread].m_fDontUpdateLastAccess = fOldInfo;

         // loaded it, but remove the string
         EnterCriticalSection (&m_csImageLoadThread);
         m_alImageLoadThreadFile[dwThread][dwPriority].Remove (0);

         // post a message to main
         PostMessage (m_hWndPrimary, WM_USER+126, dwThread ? RENDERTHREAD_RENDEREDHIGH : RENDERTHREAD_RENDERED, (LPARAM)pStore);
      }
      LeaveCriticalSection (&m_csImageLoadThread);

      // only do the following processing for one thread, thread 0
      if (dwThread)
         continue;

      SendDownloadRequests ();

      // see if any data received
      while (TRUE) {
         EnterCriticalSection (&m_csImageLoadThread);
         if (!m_lCIRCUMREALITYPACKETCLIENTCACHE.Num()) {
            LeaveCriticalSection (&m_csImageLoadThread);
            break;
         }

         // see if want to shut down
         if (m_fImageLoadThreadToShutDown) {
            LeaveCriticalSection (&m_csImageLoadThread);
            break;   // exit to loop above, which will cause shutdown
         }

         size_t iSize = m_lCIRCUMREALITYPACKETCLIENTCACHE.Size(0);
         if (!mem.Required (iSize)) {
            LeaveCriticalSection (&m_csImageLoadThread);
            break;   // exit to loop above, which will cause shutdown
         }

         memcpy (mem.p, m_lCIRCUMREALITYPACKETCLIENTCACHE.Get(0), iSize);
         m_lCIRCUMREALITYPACKETCLIENTCACHE.Remove(0); // since copied
         LeaveCriticalSection (&m_csImageLoadThread);


         // no need to verify this since was verfied earlier
         PCIRCUMREALITYPACKETCLIENTCACHE pcc = (PCIRCUMREALITYPACKETCLIENTCACHE)mem.p;
         if (!pcc)
            break;   // shouldnt happen

         if (!pcc->dwDataSize)
            continue;

         // string
         PWSTR pszString = (PWSTR)(pcc+1);

         // what quality bin to store it in
         DWORD dwQuality;
         if ((m_iResolution == m_iResolutionLow) && (m_dwShadowsFlags == m_dwShadowsFlagsLow) && (m_fLipSync == m_fLipSyncLow))
            dwQuality = 0;
         else
            dwQuality = (NUMIMAGECACHE-1);

         // if it already exists then stop
         if (m_amfImages[dwQuality].Exists (pszString))
            continue;

         // NOTE: Not lowering thread priority when do InitFromBinary() even thought might
         // be slow, BECAUSE the image has arrived because requested it
         PCImageStore pStore = new CImageStore;
         if (!pStore || !pStore->InitFromBinary (TRUE, (PBYTE)(pcc+1) + pcc->dwStringSize, pcc->dwDataSize)) {
            if (pStore)
               delete pStore;

            continue;
         }

#if 0 // def _DEBUG  // Hack - to test
         PBYTE pab = pStore->Pixel (0, 0);
         DWORD i;
         for (i = 0; i < pStore->Width() * pStore->Height() * 3; i++, pab++)
            pab[0] = 255 - pab[0];
#endif

         // see if the image exists already, since it may have been rendered in the mean time
         BOOL fImageExists = FALSE;
         DWORD dwQualityTest;
         for (dwQualityTest = NUMIMAGECACHE-1; (dwQualityTest < NUMIMAGECACHE) && (dwQualityTest >= dwQuality); dwQualityTest--)
            if (m_amfImages[dwQualityTest].Exists (pszString)) {
               fImageExists = TRUE;
               break;
            }
         if (fImageExists) {
            delete pStore;
            continue;
         }

         // save it
         pStore->ToMegaFile (&m_amfImages[dwQuality], pszString);

         // post a message to main
         PostMessage (m_hWndPrimary, WM_USER+126, RENDERTHREAD_RENDEREDHIGH, (LPARAM)pStore);
      } // while m_lCIRCUMREALITYPACKETCLIENTCACHE

   } // while TRUE

   return 0;
}


/*************************************************************************************
CMainWindow::SendWaveToServer - This sends the wave to the server.

inputs
   PWSTR             pszString - String
   DWORD             dwQuality - Quality value, from 0.. IMAGEDATABASE_MAXQUALITIES-1 (and above)
returns
   BOOL - TRUE if it used
*/
BOOL CMainWindow::SendWaveToServer (PWSTR pszString, DWORD dwQuality)
{
   if (!m_fCanAskForDownloads)
      return FALSE;

   // if haven't paid or turned off then ignore
   if (!gfRenderCache || !RegisterMode())
      return FALSE;

   EnterCriticalSection (&m_csImageLoadThread);

   // just in case
   if (!m_fCanAskForDownloads) {
      LeaveCriticalSection (&m_csImageLoadThread);
      return FALSE;
   }

   // find it
   PILTWAVEOUTCACHE pwc;
   DWORD i;
   pwc = (PILTWAVEOUTCACHE) m_lILTWAVEOUTCACHE.Get(0);
   for (i = 0; i < m_lILTWAVEOUTCACHE.Num(); i++, pwc++) {
      if (_wcsicmp(pwc->pszString, pszString))
         continue;   // mismatch

      if (pwc->dwQuality != dwQuality)
         continue;

      // else, match
      break;
   } // i
   if (i >= m_lILTWAVEOUTCACHE.Num()) {
      // doesn't exist
      LeaveCriticalSection (&m_csImageLoadThread);
      return FALSE;
   }

   ILTWAVEOUTCACHE wc;
   wc = *pwc;
   m_lILTWAVEOUTCACHE.Remove (i);
   LeaveCriticalSection (&m_csImageLoadThread);


   // from global memory
   MMIOINFO mmio;
   HMMIO hmmio;
   DWORD dwInitial;
   dwInitial = wc.pWave->m_dwSamples * wc.pWave->m_dwChannels * sizeof(short) * 2 + 100240;
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
      delete wc.pWave;
      ESCFREE (wc.pszString);
      return FALSE;
   }
   BOOL fRet;
   fRet = wc.pWave->Save (FALSE, NULL, hmmio);
   mmioClose (hmmio, 0);
   if (!fRet) {
      GlobalUnlock (hg);
      GlobalFree (hg);
      delete wc.pWave;
      ESCFREE (wc.pszString);
      return FALSE;
   }

   PBYTE pbWave = (PBYTE) mmio.pchBuffer;
   // make sure riff chunk
   if ((pbWave[0] != 0x52) || (pbWave[1] != 0x49) || (pbWave[2] != 0x46) || (pbWave[3] != 0x46)) {
      GlobalUnlock (hg);
      GlobalFree (hg);
      delete wc.pWave;
      ESCFREE (wc.pszString);
      return FALSE;
   }

   CMem memAll;
   size_t dwStringSize = (wcslen(wc.pszString)+1)*sizeof(WCHAR);
   size_t dwFileSize = ((DWORD*)pbWave)[1] + 2 * sizeof(DWORD);
   size_t dwNeed = sizeof(CIRCUMREALITYPACKETSEVERCACHE) + dwStringSize + dwFileSize;
   if (memAll.Required (dwNeed) && m_pIT && m_pIT->m_pPacket) {
      PCIRCUMREALITYPACKETSEVERCACHE prw = (PCIRCUMREALITYPACKETSEVERCACHE) memAll.p;
      memset (prw, 0, sizeof(*prw));
      prw->dwQuality = pwc->dwQuality;
      prw->dwStringSize = (DWORD) dwStringSize;
      prw->dwDataSize = (DWORD) dwFileSize;

      memcpy (prw+1, pszString, dwStringSize);
      memcpy ((PBYTE)(prw+1) + prw->dwStringSize, pbWave, dwFileSize);

      m_pIT->m_pPacket->PacketSend (CIRCUMREALITYPACKET_SERVERCACHETTS, prw, (DWORD) dwNeed);

#ifdef _DEBUG
      OutputDebugStringW (L"\r\nSendWaveToServer()");
#endif
   }

   // finally free up all
   GlobalUnlock (hg);
   HGLOBAL hgFree = GlobalFree (hg); // mmio.pchBuffer);

   delete wc.pWave;
   ESCFREE (wc.pszString);

   // done

   return TRUE;
}

/*************************************************************************************
CMainWindow::OfferWaveToServer - See if the server wants a copy of this wave (as
spoken by TTS).

inputs
   PCMMLNode2        pNode - Node that described the TTS. This will be converted to a string.
   PCM3DWave         pWave - If wanted, this will be cloned.
   DWORD             dwQuality - Quality value, from 0.. IMAGEDATABASE_MAXQUALITIES-1 (and above)
returns
   BOOL - TRUE if it used
*/
BOOL CMainWindow::OfferWaveToServer (PCMMLNode2 pNode, PCM3DWave pWave, DWORD dwQuality)
{
   if (!m_fCanAskForDownloads)
      return FALSE;

   // if haven't paid or turned off then ignore
   if (!gfRenderCache || !RegisterMode())
      return FALSE;

   pNode->AttribDelete (L"quality");   // make sure there's no quality
#ifdef _DEBUG
   int iSpeed = -100;
   pNode->AttribGetInt (L"speed", &iSpeed);
   _ASSERTE (iSpeed != -100);
#endif
      // just to make sure have a speed settings

   // get the string
   CMem mem;
   if (!MMLToMem (pNode, &mem))
      return FALSE;
   mem.CharCat (0);  // just to null terminate
   PWSTR pszStringMem = (PWSTR)mem.p;

   // clone the wave
   PCM3DWave pWaveClone = pWave->Clone ();
   if (!pWaveClone)
      return FALSE;

   if (!pWaveClone->ADPCMHackCompress ()) {
      delete pWaveClone;
      return FALSE;
   }

   EnterCriticalSection (&m_csImageLoadThread);

   // just in case
   if (!m_fCanAskForDownloads) {
      LeaveCriticalSection (&m_csImageLoadThread);
      delete pWaveClone;
      return FALSE;
   }

   // wipe out offered waves if too many queued up
   PILTWAVEOUTCACHE pwc;
   while (m_lILTWAVEOUTCACHE.Num() > 10) {
      pwc = (PILTWAVEOUTCACHE) m_lILTWAVEOUTCACHE.Get(0);
      if (pwc->pszString)
         ESCFREE (pwc->pszString);
      if (pwc->pWave)
         delete pwc->pWave;
      m_lILTWAVEOUTCACHE.Remove(0);
   }

   // add this to the list
   size_t dwStringLen = (wcslen(pszStringMem)+1) * sizeof(WCHAR);
   PWSTR pszString = (PWSTR) ESCMALLOC(dwStringLen);
   if (!pszString) {
      LeaveCriticalSection (&m_csImageLoadThread);
      delete pWaveClone;
      return FALSE;
   }
   memcpy (pszString, pszStringMem, dwStringLen);

   ILTWAVEOUTCACHE wc;
   memset (&wc, 0, sizeof(wc));
   wc.dwQuality = dwQuality;
   wc.pszString = pszString;
   wc.pWave = pWaveClone;
   m_lILTWAVEOUTCACHE.Add (&wc);

   // see if server wants
   size_t dwNeed = sizeof(CIRCUMREALITYPACKETSEVERQUERYWANT) + dwStringLen; //  + memData.m_dwCurPosn;
   if (mem.Required (dwNeed) && m_pIT) {
      PCIRCUMREALITYPACKETSEVERQUERYWANT prw = (PCIRCUMREALITYPACKETSEVERQUERYWANT) mem.p;
      memset (prw, 0, sizeof(*prw));
      prw->dwQuality = dwQuality;
      prw->dwStringSize = (DWORD) dwStringLen;

      memcpy (prw+1, pszString, prw->dwStringSize);

#ifdef _DEBUG
   OutputDebugStringW (L"\r\nOfferWaveToServer()");
#endif

      m_pIT->m_pPacket->PacketSend (CIRCUMREALITYPACKET_SERVERQUERYWANTTTS, prw, (DWORD) dwNeed);
   }

   // done
   LeaveCriticalSection (&m_csImageLoadThread);

   return TRUE;
}



/*************************************************************************************
CMainWindow::GetTTSCacheWave - Finds a match in the TTS cache and returns the wave
file.

inputs
   PWSTR          pszString - String looking for
returns
   PCM3DWave - Wave, or NULL if can't find
*/
PCM3DWave CMainWindow::GetTTSCacheWave (PWSTR pszString)
{
   if (!m_fCanAskForDownloads)
      return NULL;


   EnterCriticalSection (&m_csImageLoadThread);

   // just in case
   if (!m_fCanAskForDownloads) {
      LeaveCriticalSection (&m_csImageLoadThread);
      return NULL;
   }

   // find match
   DWORD i;
   PCIRCUMREALITYPACKETCLIENTCACHE pcc;
   PWSTR pszStringCur;
   CMem mem;
   for (i = 0; i < m_lCIRCUMREALITYPACKETCLIENTCACHETTS.Num(); i++) {
      pcc = (PCIRCUMREALITYPACKETCLIENTCACHE) m_lCIRCUMREALITYPACKETCLIENTCACHETTS.Get(i);
      if (!pcc->dwDataSize)
         continue;

      // make sure not too large
      size_t iSize = m_lCIRCUMREALITYPACKETCLIENTCACHETTS.Size(i);
      if (sizeof(*pcc) + pcc->dwStringSize + pcc->dwDataSize > iSize)
         continue;

      // know that string is valid
      pszStringCur = (PWSTR)(pcc+1);

      if (_wcsicmp(pszString, pszStringCur))
         continue;   // no match

      // make a copy so can leave critsec
      if (!mem.Required (iSize))
         continue;   // error. shoulde happen
      memcpy (mem.p, pcc, iSize);
      LeaveCriticalSection (&m_csImageLoadThread);

      pcc = (PCIRCUMREALITYPACKETCLIENTCACHE) mem.p;
      pszStringCur = (PWSTR)(pcc+1);


      // else, it's arrived
      MMIOINFO mmio;
      HMMIO hmmio;
      memset (&mmio, 0, sizeof(mmio));
      mmio.pchBuffer = (HPSTR) ((PBYTE)(pcc+1) + pcc->dwStringSize);
      mmio.fccIOProc = FOURCC_MEM;
      mmio.cchBuffer = pcc->dwDataSize;
      hmmio = mmioOpen (NULL, &mmio, MMIO_READ);
      if (!hmmio)
         return NULL;

      PCM3DWave pWave = new CM3DWave;
      if (!pWave) {
         mmioClose (hmmio, 0);
         return NULL;
      }

      BOOL fRet;
      fRet = pWave->Open (NULL, "", TRUE, hmmio);
      mmioClose (hmmio, 0);
      if (!fRet) {
         delete pWave;
         return NULL;
      }

      if (!pWave->ADPCMHackDecompress ()) {
         delete pWave;
         return NULL;
      }

      return pWave;
   } // i

   // else, didnt find
   LeaveCriticalSection (&m_csImageLoadThread);

   return NULL;
}

/*************************************************************************************
CMainWindow::ReceivedTTSCache - Received data to cache for TTS so that can avoid
having to resynthesize (and to potentially get better quality)

inputs
   PVOID          pMem - Memory, with PCIRCUMREALITYPACKETCLIENTCACHE as header
   size_t         dwSize - Number of bytes
returns
   none
*/
void CMainWindow::ReceivedTTSCache (PVOID pMem, size_t dwSize)
{
   if (!m_fCanAskForDownloads)
      return;

#ifdef _DEBUG
   OutputDebugStringW (L"\r\nReceivedTTSCache ()");
#endif

   EnterCriticalSection (&m_csImageLoadThread);

   // just in case
   if (!m_fCanAskForDownloads) {
      LeaveCriticalSection (&m_csImageLoadThread);
      return;
   }

   // delete some if too many
   while (m_lCIRCUMREALITYPACKETCLIENTCACHETTS.Num() >= 25)
      m_lCIRCUMREALITYPACKETCLIENTCACHETTS.Remove (0);

   // store this away
   m_lCIRCUMREALITYPACKETCLIENTCACHETTS.Add (pMem, dwSize);

   LeaveCriticalSection (&m_csImageLoadThread);
}


/*************************************************************************************
CMainWindow::ReceivedDownloadRequest - Tells the thread that have received download requests,
so it clears m_csImageLoadThread.

inputs
   PWSTR          pszString - String
   PCMem          pMem - If not NULL, then this is memory with the image to be processed.
                  It will have a CIRCUMREALITYPACKETCLIENTCACHE header (that has been verified not bogus)
returns
   none
*/
void CMainWindow::ReceivedDownloadRequest (PWSTR pszString, PCMem pMem)
{
   if (!m_fCanAskForDownloads)
      return;

#ifdef _DEBUG
   OutputDebugStringW (pMem ? L"\r\nReceivedDownloadRequest:Data arrived" : L"\r\nReceivedDownloadRequest:No data available");
#endif

   EnterCriticalSection (&m_csImageLoadThread);

   // just in case
   if (!m_fCanAskForDownloads) {
      LeaveCriticalSection (&m_csImageLoadThread);
      return;
   }

   // wipe out download requests that haven't been answered for at least 60 seconds
   PILTDOWNLOADQUEUE pdq;
   DWORD i;
   for (i = 0; i < m_lILTDOWNLOADQUEUE.Num(); i++) {
      pdq = (PILTDOWNLOADQUEUE) m_lILTDOWNLOADQUEUE.Get(i);
      if (!_wcsicmp((PWSTR)(pdq+1), pszString)) {  // BUGFIX - Missed the ! in front of _wcsicmp
#ifdef _DEBUG
     OutputDebugStringW (L"\r\n\r\nRemoved from m_lILTDOWNLOADQUEUE: ");
     OutputDebugStringW (pszString);
#endif
         m_lILTDOWNLOADQUEUE.Remove (i);
         i--;
      }
   } // i
#ifdef _DEBUG
     OutputDebugStringW (L"\r\nDont removing");
#endif

   if (pMem) {
#ifdef _DEBUG
     OutputDebugStringW (L"\r\n\r\nReceived: ");
     OutputDebugStringW (pszString);
#endif
      m_lCIRCUMREALITYPACKETCLIENTCACHE.Add (pMem->p, pMem->m_dwCurPosn);
   }

   LeaveCriticalSection (&m_csImageLoadThread);
}

/*************************************************************************************
CMainWindow::SendDownloadRequests - Sends any download rquests that possible.
Thread safe.

inputs
   none
returns
   none
*/
void CMainWindow::SendDownloadRequests (void)
{
   DWORD i;

   // if cant ask for downloads the don't try
   if (!m_fCanAskForDownloads)
      return;

   // get the ticks per second
   LARGE_INTEGER iLI;
   __int64 iTime, iTicksPerSec;
   QueryPerformanceFrequency (&iLI);
   iTicksPerSec = *((__int64*)&iLI);
   __int64 iTicks30 = iTicksPerSec * 30;
   // get the time
   QueryPerformanceCounter (&iLI);
   iTime = *((__int64*)&iLI);

   EnterCriticalSection (&m_csImageLoadThread);

   // wipe out download requests that haven't been answered for at least 60 seconds
   PILTDOWNLOADQUEUE pdq;
   for (i = 0; i < m_lILTDOWNLOADQUEUE.Num(); i++) {
      pdq = (PILTDOWNLOADQUEUE) m_lILTDOWNLOADQUEUE.Get(i);
      if ((pdq->iTime < iTime - iTicks30) || (pdq->iTime > iTime + iTicks30)) {

#ifdef _DEBUG
         OutputDebugStringW (L"\r\n\r\nOld, no response: ");
         OutputDebugStringW ((PWSTR)(pdq+1));
#endif
         // old and hasn't received response, so delete
         m_lILTDOWNLOADQUEUE.Remove (i);
         i--;
      }
   } // i

   // if there are too many requests queued up then exit here
   if (m_lILTDOWNLOADQUEUE.Num() >= 2) {
#ifdef _DEBUG
      OutputDebugStringW (L"\r\n... m_lILTDOWNLOADQUEUE full ...");
#endif
      LeaveCriticalSection (&m_csImageLoadThread);
      return;
   }

   LeaveCriticalSection (&m_csImageLoadThread);

   // if haven't paid or turned off then ignore
   if (!gfRenderCache || !RegisterMode())
      return;

   // else, try to find something to queue up
   CMem memString;
   BOOL fHighQuality;
   while (TRUE) {
      // see if anything queued up
      if (!m_pRT || !m_pIT || !m_fCanAskForDownloads)
         break;
      if (!m_pRT->FindCanddiateToDownload (&memString, &fHighQuality))
         break; // found nothing

      // make sure right rendering quality
      int iQualityMono = QualityMonoGet();
      if (iQualityMono < 0)
         break;

      // verify that file exists
      if (m_amfImages[fHighQuality ? (NUMIMAGECACHE-1) : 0].Exists ((PWSTR)memString.p))
         continue;   // already exists, so don't bother requesting

      // add to the queue (and make sure not already there
      EnterCriticalSection (&m_csImageLoadThread);
      for (i = 0; i < m_lILTDOWNLOADQUEUE.Num(); i++) {
         pdq = (PILTDOWNLOADQUEUE) m_lILTDOWNLOADQUEUE.Get(i);
         if (!_wcsicmp((PWSTR)(pdq+1), (PWSTR)memString.p))
            break;
      } // i
      if (i < m_lILTDOWNLOADQUEUE.Num()) {
         // already queued up, so don't add again
         LeaveCriticalSection (&m_csImageLoadThread);
         continue;
      }

      // fill in the header info and add to the list
      size_t dwStringSize = (wcslen((PWSTR)memString.p)+1)*sizeof(WCHAR);
      if (!memString.Required (sizeof(ILTDOWNLOADQUEUE) + dwStringSize)) {
         // error
         LeaveCriticalSection (&m_csImageLoadThread);
         continue;
      }
      memmove ((PBYTE)memString.p + sizeof(ILTDOWNLOADQUEUE), memString.p, dwStringSize);
      pdq = (PILTDOWNLOADQUEUE) memString.p;
      memset (pdq, 0, sizeof(*pdq));
      pdq->iTime = iTime;
      m_lILTDOWNLOADQUEUE.Add (pdq, sizeof(ILTDOWNLOADQUEUE) + dwStringSize);

      // send to the server
      CMem memServer;
      if (!memServer.Required (sizeof(CIRCUMREALITYPACKETSEVERREQUEST) + dwStringSize)) {
         // error
         LeaveCriticalSection (&m_csImageLoadThread);
         continue;
      }
      memServer.m_dwCurPosn = sizeof(CIRCUMREALITYPACKETSEVERREQUEST) + dwStringSize;
      PCIRCUMREALITYPACKETSEVERREQUEST psr = (PCIRCUMREALITYPACKETSEVERREQUEST) memServer.p;
      memset (psr, 0, sizeof(*psr));
      memcpy (psr + 1, pdq + 1, dwStringSize);
      psr->dwQualityMin = (DWORD)iQualityMono;
      if (RegisterMode())
         psr->dwQualityMax = psr->dwQualityMin+2;
      else
         psr->dwQualityMax = psr->dwQualityMin+1;
      psr->dwStringSize = (DWORD) dwStringSize;

#ifdef _DEBUG
      OutputDebugStringW (L"\r\n\r\nRequest: ");
      OutputDebugStringW ((PWSTR)(psr+1));
#endif

      m_pIT->m_pPacket->PacketSend (CIRCUMREALITYPACKET_SERVERREQUESTRENDER, memServer.p, (DWORD) memServer.m_dwCurPosn);

      LeaveCriticalSection (&m_csImageLoadThread);
      break;

   } // while (TRUE)
}


/*************************************************************************************
CMainWindow::VoiceChatWaveSnippet - Call this when a wave snippet is to be
sent to the server for transmission to other players.

inputs
   PCM3DWave         pWave - Wave. This wave will be deleted
returns
   none
*/
void CMainWindow::VoiceChatWaveSnippet (PCM3DWave pWave)
{
   // make up the node describing this
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode) {
      delete pWave;
      return;
   }
   pNode->NameSet (CircumrealityVoiceChat());

   // find a chat window so can see who is speaking to
   DWORD i;
   PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
   GUID gTo;
   PWSTR pszLang = NULL, pszStyle = NULL;
   gTo = GUID_NULL;
   for (i = 0; i < m_lPCIconWindow.Num(); i++)
      if (ppi[i]->ChatWhoTalkingTo (&gTo, &pszLang, &pszStyle))
         break;

   // fill in some settings
   if (!IsEqualGUID (gTo, GUID_NULL))
      MMLValueSet (pNode, L"ID", (PBYTE)&gTo, sizeof(gTo));
   if (pszLang)
      MMLValueSet (pNode, L"language", pszLang);
   if (pszStyle)
      MMLValueSet (pNode, L"style", pszStyle);

   // send this to thread
   DWORD dwProcessors = VoiceChatHowManyProcessors();
   EnterCriticalSection (&m_csVCThread);
   m_lVCPCMMLNode2.Add (&pNode);
   m_lVCPCM3DWave.Add (&pWave);
   for (i = 0; i < dwProcessors; i++)
      SetEvent (m_aVCTHREADINFO[i].hSignal);
   LeaveCriticalSection (&m_csVCThread);
}


/*************************************************************************************
CMainWindow::InfoForServer - Sends information for the server, such as
the time zone, language ID, or graphics speed.

inputs
   PWSTR       pszName - Value name
   PWSTR       pszValue - Value string. If this is NULL then fValue will be used.
   fp          fValue - Floating point value. Only used if pszValue is NULL
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::InfoForServer (PWSTR pszName, PWSTR pszValue, fp fValue)
{
   // BUGFIX - if no internet thread then fail
   if (!m_pIT)
      return FALSE;

   CMMLNode2 node;
   node.NameSet (L"temp");
   if (pszValue)
      MMLValueSet (&node, pszName, pszValue);
   else
      MMLValueSet (&node, pszName, fValue);

   PWSTR psz = NULL;
   PCMMLNode2 pSub = NULL;
   node.ContentEnum (0, &psz, &pSub);

   if (!pSub)
      return FALSE;

   BOOL fRet = m_pIT->m_pPacket ? m_pIT->m_pPacket->PacketSend (CIRCUMREALITYPACKET_INFOFORSERVER, pSub) : 0;
   if (!fRet) {
      int iErr;
      PCWSTR pszErr;
      if (m_pIT->m_pPacket)
         iErr = m_pIT->m_pPacket->GetFirstError (&pszErr);
      else {
         iErr = 1;
         pszErr = L"InfoForServer::No m_pIT->m_pPacket";
      }
      PacketSendError(iErr, pszErr);
   }
   return fRet;
}



/*************************************************************************************
CMainWindow::ClientRectGet - Gets the client rectangle of the main window,
excluding the tab bar and task bar.

inputs
   DWORD       dwMonitor - 0 for primary, 1 for secondary
   RECT        *pr - Filled with the location
returns
   none
*/
void CMainWindow::ClientRectGet (DWORD dwMonitor, RECT *pr)
{
   HWND hWnd = dwMonitor ? m_hWndSecond : m_hWndPrimary;
   GetClientRect (hWnd, pr);

   if (!dwMonitor) {
      // sliding window affects rectangle
      RECT rSlide;
      if (m_pSlideTop) {
         m_pSlideTop->DefaultCoords (m_fSlideLocked ? 2 : 0, &rSlide);
         pr->top = max(pr->top, rSlide.bottom);
      }
      if (m_pSlideBottom) {
         m_pSlideBottom->DefaultCoords (m_fSlideLocked ? 2 : 0, &rSlide);
         pr->bottom = min(pr->bottom, rSlide.top);
      }
      // NOTE: m_pTickerTape does NOT affect rectangle
   }

   pr->bottom = max(pr->bottom, pr->top+1);
}


/*************************************************************************************
CMainWindow::PacketSendError - Call this when there's a packet send error. THis
will alert the user that the server has disconnected.

This is THREAD SAFE.

inputs
   int            iError - Error number (from internet)
   PCWSTR         psz - Error string. Cann be NULL.
*/
void CMainWindow::PacketSendError (int iError, PCWSTR psz)
{
   // post a message to be thread safe
   PostMessage (m_hWndPrimary, WM_PACKETSENDERROR, (WPARAM)iError, (LPARAM) psz);
}


/*************************************************************************************
CMainWindow::DirectSoundError - Call this when there's a direct sound error. THis
will alert the user that they need direct sound.

This is THREAD SAFE.
*/
void CMainWindow::DirectSoundError (void)
{
   // post a message to be thread safe
   PostMessage (m_hWndPrimary, WM_DIRECTSOUNDERROR, 0, 0);
}



/*************************************************************************************
CMainWindow::CommandSetFocus - Sets focus to the command.

inputs
   BOOL     fForce - If TRUE then force the focus to the speak edit.
*/
void CMainWindow::CommandSetFocus (BOOL fForce)
{
#ifdef UNIFIEDTRANSCRIPT
   // set the windows focus
   if (m_pTranscriptWindow && m_pTranscriptWindow->m_hWnd && (GetFocus() != m_pTranscriptWindow->m_hWnd) )
      SetFocus (m_pTranscriptWindow->m_hWnd);

   // set the focus to the control
   if (fForce && m_pTranscriptWindow && m_pTranscriptWindow->m_pPage) {
      PCEscControl pControl = m_pTranscriptWindow->m_pPage->FocusGet();
      PCEscControl pWant = m_pTranscriptWindow->m_pPage->ControlFind (L"combined"); // L"command");

      if (pWant && (pWant != pControl))
         m_pTranscriptWindow->m_pPage->FocusSet (pWant);

      // set the checkbox
      pControl = m_pTranscriptWindow->m_pPage->ControlFind (L"actspeak0");
      if (pControl) {
         pControl->AttribSetBOOL (Checked(), TRUE);
         m_fCombinedSpeak = FALSE;
      }
      if (pControl = m_pTranscriptWindow->m_pPage->ControlFind (L"actspeak1"))
         pControl->AttribSetBOOL (Checked(), FALSE);
   }
#else
   if (m_pSlideTop && m_pSlideTop->CommandIsVisible())
      m_pSlideTop->CommandSetFocus();
#endif
}


/*************************************************************************************
CMainWindow::SpeakSetFocus - Sets focus on the speak edit field

inputs
   BOOL     fForce - If TRUE then force the focus to the speak edit.
returns
   BOOL - TRUE if success, FALSE if cant set the focus
*/
BOOL CMainWindow::SpeakSetFocus (BOOL fForce)
{
#ifdef UNIFIEDTRANSCRIPT
   if (!m_pTranscriptWindow || !m_pTranscriptWindow->m_hWnd)
      return FALSE;

   // set the windows focus
   if ( (GetFocus() != m_pTranscriptWindow->m_hWnd) )
      SetFocus (m_pTranscriptWindow->m_hWnd);

   if (!fForce)
      return TRUE;

   // set the focus to the control
   if (!m_pTranscriptWindow->m_pPage)
      return FALSE;

   PCEscControl pControl = m_pTranscriptWindow->m_pPage->FocusGet();
   PCEscControl pWant = m_pTranscriptWindow->m_pPage->ControlFind (L"combined"); // L"speak");
   if (!pWant)
      return FALSE;

   if ((pWant != pControl))
      m_pTranscriptWindow->m_pPage->FocusSet (pWant);
   
   // set the checkbox
   pControl = m_pTranscriptWindow->m_pPage->ControlFind (L"actspeak1");
   if (pControl) {
      pControl->AttribSetBOOL (Checked(), TRUE);
      m_fCombinedSpeak = TRUE;
   }
   if (pControl = m_pTranscriptWindow->m_pPage->ControlFind (L"actspeak0"))
      pControl->AttribSetBOOL (Checked(), FALSE);

   return TRUE;
#else
   DWORD i;
   PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
   for (i = 0; i < m_lPCIconWindow.Num(); i++) {
      if (ppi[i]->m_fChatWindow) {
         // make sure its' visible
         ChildShowToggle (TW_ICONWINDOW, (PWSTR) ppi[i]->m_memID.p, 1);

         SetFocus (ppi[i]->m_hWnd);
         return TRUE;
      }
   } // i
   return FALSE;
#endif
}


/*************************************************************************************
CMainWindow::FindChatWindow - Finds the first chat window

returns
   PCIconWindow - CHat window, or NULL if can't find
*/
PCIconWindow CMainWindow::FindChatWindow (void)
{
   DWORD i;
   PCIconWindow *ppi = (PCIconWindow*)m_lPCIconWindow.Get(0);
   for (i = 0; i < m_lPCIconWindow.Num(); i++)
      if (ppi[i]->m_fChatWindow)
         return ppi[i];
   return NULL;
}



/*************************************************************************************
CMainWindow::VisImageTimer - Adds/removes a vis-image timer that will refresh
the sliders drawn.

inputs
   PCVisImage        pvi - Image
   DWORD             dwTime - Time to go off, as GetTickCount(). Or 0 to turn off
returns
   none
*/
void CMainWindow::VisImageTimer (PCVisImage pvi, DWORD dwTime)
{
   // see if can find one
   PVISIMAGETIMER pvit = (PVISIMAGETIMER)m_lVISIMAGETIMER.Get(0);
   DWORD i;
   for (i = 0; i < m_lVISIMAGETIMER.Num(); i++, pvit++) {
      if (pvit->pvi != pvi)
         continue;   // no match

      if (!dwTime) {
         // want to delete, so clear
         m_lVISIMAGETIMER.Remove (i);
         return;
      }

      // else, set new value
      pvit->dwTime = dwTime;
      return;
   }

   // if no time, then skip
   if (!dwTime)
      return;

   // else, add
   VISIMAGETIMER vit;
   memset (&vit, 0, sizeof(vit));
   vit.dwTime = dwTime;
   vit.pvi = pvi;
   m_lVISIMAGETIMER.Add (&vit);
}



#if 0 // not used
/*************************************************************************************
CMainWindow::InvalidateRectSpecial - Invalidate a child window's rect and
also invalidate's main window so draws layered.

inputs
   HWND        hWnd - Child
*/
void CMainWindow::InvalidateRectSpecial (HWND hWnd)
{
#ifdef MIFTRANSPARENTWINDOWS
   RECT rChild;
   HWND hWndParent = GetParent(hWnd);
   GetWindowRect (hWnd, &rChild);
   ScreenToClient (hWndParent, ((LPPOINT)&rChild) + 0);
   ScreenToClient (hWndParent, ((LPPOINT)&rChild) + 1);

   InvalidateRect (hWndParent, &rChild, FALSE);
#endif

   InvalidateRect (hWnd, NULL, FALSE);
}
#endif // 0


/*************************************************************************************
CMainWindow::BackgroundStretchCalc - Figure out where the background is stretched
from.

inputs
   DWORD          dwTextOrImages - BACKGROUND_XXX
   RECT           *prScreen - Rectangle of the area, in screen coordinates. It's assumed
                     the the UL rect is 0,0 in the DC.
   DWORD          dwWantDark - WANTDARK_NORMAL for light, WANTDARK_DARK for dark, WANTDARK_DARKEXTRA for very dark
   HWND           hWndMain - Either the primary or secondary hWnd.
   DWORD          dwScale - Usually 1, but can be higher if doing antialiasing
                  on the info. Scale will cause the image to be drawn 2x (etc.) as large as
                  the width/height specified by prScreen.
   RECT           *prStretch - Filled with the stretch to location, in the HDC coords
   RECT           *prOrig - Filled with the dimensions of the bitmap.
returns
   HBITMAP - Returns the bitmap used to draw, or NULL if nothing
*/
HBITMAP CMainWindow::BackgroundStretchCalc (DWORD dwTextOrImages, RECT *prScreen, DWORD dwWantDark, HWND hWndMain, DWORD dwScale,
                                            RECT *prStretch, RECT *prOrig)
{
   // if no bitmap then nothing
   HBITMAP hBitWant;
   switch (dwWantDark) {
   case WANTDARK_NORMAL:
   default:
      hBitWant = m_ahbmpJPEGBack[dwTextOrImages];
      break;
   case WANTDARK_DARK:
      hBitWant = m_ahbmpJPEGBackDark[dwTextOrImages];
      break;
   case WANTDARK_DARKEXTRA:
      hBitWant = m_ahbmpJPEGBackDarkExtra[dwTextOrImages];
      break;
   }
   // BUGFIX - No longer force to extra dark
   // hBitWant = (m_iTextVsSpeech <= 1) ? m_hbmpJPEGBackDarkExtra : m_hbmpJPEGBackDark; // BUGFIX - Always use the extra-dark bitmap to minimize window distinctions
   if (!hBitWant)
      return NULL;

   // get the display window's location
   RECT rClient;
   GetClientRect (hWndMain, &rClient);

   // convert to screen coords
   ClientToScreen (hWndMain, ((LPPOINT)&rClient) + 0);
   ClientToScreen (hWndMain, ((LPPOINT)&rClient) + 1);

   // figure out how much would be stretch
   //double afStretch[2];
   //afStretch[0] = (double)(rClient.right - rClient.left) / (double)max(m_pJPEGBack.x, 1);
   //afStretch[1] = (double)(rClient.bottom - rClient.top) / (double)max(m_pJPEGBack.y, 1);

   double afScreenSize[2], afClientSize[2];
   afScreenSize[0] = max(prScreen->right - prScreen->left, 1);
   afScreenSize[1] = max(prScreen->bottom - prScreen->top, 1);
   afClientSize[0] = max(rClient.right - rClient.left, 1);
   afClientSize[1] = max(rClient.bottom - rClient.top, 1);

   // figure out offset as a percent of original bitmap width
   double afOffsetPer[4];
   afOffsetPer[0] = (double)(prScreen->left - rClient.left) / afClientSize[0];
   afOffsetPer[1] = (double)(prScreen->top - rClient.top) / afClientSize[1];
   afOffsetPer[2] = (double)(prScreen->right - rClient.left) / afClientSize[0];
   afOffsetPer[3] = (double)(prScreen->bottom - rClient.top) / afClientSize[1];

   // figure out width/height as a precent of the original bitmap
   double afSizePer[2];
   afSizePer[0] = afScreenSize[0] / afClientSize[0];
   afSizePer[1] = afScreenSize[1] / afClientSize[1];

   // figure out stretch-to-width and height
   double afStretchTo[2];
   afStretchTo[0] = afScreenSize[0] / afSizePer[0] * (double)dwScale;
   afStretchTo[1] = afScreenSize[1] / afSizePer[1] * (double)dwScale;

   // new locations for the bitmap, in HDC coords
   double fLeft, fRight, fTop, fBottom;
   fLeft = -afOffsetPer[0] * afStretchTo[0];
   fRight = fLeft + afStretchTo[0];

   fTop = -afOffsetPer[1] * afStretchTo[1];
   fBottom = fTop + afStretchTo[1];

   if (prStretch) {
      prStretch->left = (int)floor(fLeft + 0.5);
      prStretch->right = (int)floor(fRight + 0.5);
      prStretch->top = (int)floor(fTop + 0.5);
      prStretch->bottom = (int)floor(fBottom + 0.5);
   }

   if (prOrig) {
      prOrig->top = prOrig->left = 0;
      prOrig->right = m_apJPEGBack[dwTextOrImages].x;
      prOrig->bottom = m_apJPEGBack[dwTextOrImages].y;
   }

   // return bitmap
   return hBitWant;
}


/*************************************************************************************
CMainWindow::BackgroundStretch - Does a StretchBlt() into the HDC. If there is
   no background (or if is NOT transparent), this fills with m_hbrBackground.

inputs
   DWORD          dwTextOrImages - Use BACKGROUND_XXX
   BOOL           fTransparent - If transparent draws background image, if not
                  then just fills prClient with black.
   DWORD          dwWantDark - What kind of darkness wants
   RECT           *prClient - Location as the image will appear, in client coords
                  for hWndClient.
   HWND           hWndClient - Client window.
   HWND           hWndMain - Either the primary or secondary hWnd.
   DWORD          dwScale - Usually 1, but can be higher if doing antialiasing
                  on the info. Scale will cause the image to be drawn 2x (etc.) as large as
                  the width/height specified by prScreen.
   HDC            hDC -  To draw on
   POINT          *pDCOffset - Offset of the HDC's 0,0 into the client rect. If NULL
                  then assume it's 0.
returns
   none
*/
void CMainWindow::BackgroundStretch (DWORD dwTextOrImages, BOOL fTransparent, DWORD dwWantDark, RECT *prClient, HWND hWndClient,
                                     HWND hWndMain, DWORD dwScale, HDC hDC, POINT *pDCOffset)
{
   RECT rDC;
   rDC = *prClient;
   if (pDCOffset)
      OffsetRect (&rDC, -pDCOffset->x, -pDCOffset->y);

   if (!fTransparent) {
      rDC.top *= (int)dwScale;
      rDC.bottom *= (int)dwScale;
      rDC.left *= (int)dwScale;
      rDC.right *= (int)dwScale;
      FillRect (hDC, &rDC, m_hbrBackground);
      return;
   }

#if 0 // disable
   DWORD dwWantDark = 0;
   if (hWndClient != hWndMain) {
      if ((hWndClient == m_hWndTranscript) || (GetParent(hWndClient) == m_hWndTranscript) )
         dwWantDark = 2;   // want extra dark
      else
         dwWantDark = 1;

      dwWantDark = 2;   // BUGFIX - If not hWndMain, always want extra dark
   }
#endif


   RECT rStretch;
   RECT rOrig;
   RECT rScreen;
   rScreen = *prClient;
   ClientToScreen (hWndClient, ((LPPOINT)&rScreen) + 0);
   ClientToScreen (hWndClient, ((LPPOINT)&rScreen) + 1);
   HBITMAP hBmp = BackgroundStretchCalc (dwTextOrImages,
      &rScreen, dwWantDark, hWndMain, dwScale, &rStretch, &rOrig);
   OffsetRect (&rStretch, prClient->left, prClient->top);

   // if no bitmap then fillrect
   if (!hBmp) {
      FillRect (hDC, &rDC, m_hbrBackground);
      return;
   }

   // ccount for DC offset
   if (pDCOffset)
      OffsetRect (&rStretch, -pDCOffset->x, -pDCOffset->y);

   HDC hDCBmp;
   hDCBmp = CreateCompatibleDC (hDC);
   SelectObject (hDCBmp, hBmp);
   int   iOldMode;
   iOldMode = SetStretchBltMode (hDC, COLORONCOLOR);
   StretchBlt(
      hDC, rStretch.left, rStretch.top, rStretch.right - rStretch.left, rStretch.bottom - rStretch.top,
      hDCBmp, rOrig.left, rOrig.top, rOrig.right - rOrig.left, rOrig.bottom - rOrig.top,
      SRCCOPY);
   SetStretchBltMode (hDC, iOldMode);
   DeleteDC (hDCBmp);
}


/*************************************************************************************
CMainWindow::BackgroundStretchViaBitmap - Stretch the background, but first
into a temporary bitmap so that won't excede clipping area.

inputs
   DWORD          dwTextOrImages - BACKGROUND_XXX
   BOOL           fTransparent - If transparent draws background image, if not
                  then just fills prClient with black.
   DWORD          dwWantDark - What kind of darkness want
   RECT           *prClient - Location as the image will appear, in client coords
                  for hWndClient.
   HWND           hWndClient - Client window.
   HWND           hWndMain - Either the primary or secondary hWnd.
   DWORD          dwScale - Usually 1, but can be higher if doing antialiasing
                  on the info. Scale will cause the image to be drawn 2x (etc.) as large as
                  the width/height specified by prScreen.
   HDC            hDC -  To draw on, for the client
returns
   none
*/
void CMainWindow::BackgroundStretchViaBitmap (DWORD dwTextOrImages,
                                              BOOL fTransparent, DWORD dwWantDark, RECT *prClient, HWND hWndClient,
                                     HWND hWndMain, HDC hDC)
{
#if 0 // disabled
   DWORD dwWantDark = WANTDARK_NORMAL;
   if (hWndClient != hWndMain) {
      if ((hWndClient == m_hWndTranscript) || (GetParent(hWndClient) == m_hWndTranscript) )
         dwWantDark = WANTDARK_DARKEXTRA;   // want extra dark
      else
         dwWantDark = WANTDARK_DARK;

      dwWantDark = WANTDARK_DARKEXTRA;   // BUGFIX - If not hWndMain, always want extra dark
   }
#endif // 0

   HBITMAP hbmpWant;
   switch (dwWantDark) {
   case WANTDARK_NORMAL:
   default:
      hbmpWant = m_ahbmpJPEGBack[dwTextOrImages];
      break;
   case WANTDARK_DARK:
      hbmpWant = m_ahbmpJPEGBackDark[dwTextOrImages];
      break;
   case WANTDARK_DARKEXTRA:
      hbmpWant = m_ahbmpJPEGBackDarkExtra[dwTextOrImages];
      break;
   } // switch
   // BUGFIX - Different variations of dark
   // hbmpWant = (m_iTextVsSpeech <= 1) ? m_hbmpJPEGBackDarkExtra : m_hbmpJPEGBackDark; // BUGFIX - Always use the extra-dark bitmap to minimize window distinctions

   // catch this quickly if not transparent or not bitmap
   if (!fTransparent || !hbmpWant) {
      FillRect (hDC, prClient, m_hbrBackground);
      return;
   }

   // make sure this rectangle isn't actually larger than the client
   RECT rInter, rClientNow;
   GetClientRect (hWndClient, &rClientNow);
   if (!IntersectRect (&rInter, prClient, &rClientNow))
      return;  // off screen
   prClient = &rInter;

   // width and height
   int iWidth = prClient->right - prClient->left;
   int iHeight = prClient->bottom - prClient->top;
   if ((iWidth <= 0) || (iHeight <= 0))
      return;  // nothing

   // create bitmap
   HBITMAP hBmp = CreateCompatibleBitmap (hDC, iWidth, iHeight);
   if (!hBmp)
      return;
   HDC hDCBmp;
   hDCBmp = CreateCompatibleDC (hDC);
   SelectObject (hDCBmp, hBmp);
   POINT pOffset;
   pOffset.x = prClient->left;
   pOffset.y = prClient->top;
   BackgroundStretch (dwTextOrImages, fTransparent, dwWantDark, prClient, hWndClient, hWndMain, 1, hDCBmp, &pOffset);

#if 0 // hack to test
   RECT rHack;
   rHack.left = rHack.top = 0;
   rHack.right = prClient->right - prClient->left;
   rHack.bottom = prClient->bottom - prClient->top;
   FillRect (hDCBmp, &rHack, m_hbrBackground);
#endif // 0

   // draw
   BitBlt (hDC, prClient->left, prClient->top, prClient->right - prClient->left, prClient->bottom - prClient->top,
      hDCBmp, 0, 0, SRCCOPY);

   // delete
   DeleteDC (hDCBmp);
   DeleteObject (hBmp);
}


/*************************************************************************************
CMainWindow::BackgroundUpdate - Updates the image with whatever image is in m_aImageBackground

inputs
   DWORD          dwTextOrImages - Of type BACKGROUND_XXX
*/

// #define BACKGROUNDUPDATE_CONTRAST         1.0      // BUGFIX - Was reduced contrast, but less color should be enough: 0.75
// #define BACKGROUNDUPDATE_INTENSITYDARK        0x1800

void CMainWindow::BackgroundUpdate (DWORD dwTextOrImages)
{
   // make sure have image
   if (!m_aImageBackground[dwTextOrImages].Width() || !m_aImageBackground[dwTextOrImages].Height())
      return;

   // create a bitmap out of this
   if (m_ahbmpJPEGBack[dwTextOrImages])
      DeleteObject (m_ahbmpJPEGBack[dwTextOrImages]);
   m_ahbmpJPEGBack[dwTextOrImages] = ImageSquashIntensityToBitmap (&m_aImageBackground[dwTextOrImages], m_fLightBackground, 0,
      m_hWndPrimary,
      (dwTextOrImages == BACKGROUND_IMAGES) ? &m_crJPEGBackAll : NULL,
      (dwTextOrImages == BACKGROUND_IMAGES) ? &m_crJPEGBackTop : NULL);

   // find color
   if (m_hbrJPEGBackAll)
      DeleteObject (m_hbrJPEGBackAll);
   m_hbrJPEGBackAll = CreateSolidBrush (m_crJPEGBackAll);
   if (m_hbrJPEGBackTop)
      DeleteObject (m_hbrJPEGBackTop);
   m_hbrJPEGBackTop = CreateSolidBrush (m_crJPEGBackTop);

   // also, create darker bitmap
   // create a bitmap out of this
   if (m_ahbmpJPEGBackDark[dwTextOrImages])
      DeleteObject (m_ahbmpJPEGBackDark[dwTextOrImages]);
   if (m_ahbmpJPEGBackDarkExtra[dwTextOrImages])
      DeleteObject (m_ahbmpJPEGBackDarkExtra[dwTextOrImages]);

   m_ahbmpJPEGBackDark[dwTextOrImages] = ImageSquashIntensityToBitmap (&m_aImageBackground[dwTextOrImages], m_fLightBackground, 1,
      m_hWndPrimary, NULL, NULL);

   m_ahbmpJPEGBackDarkExtra[dwTextOrImages] = ImageSquashIntensityToBitmap (&m_aImageBackground[dwTextOrImages], m_fLightBackground, 2,
      m_hWndPrimary,
      (dwTextOrImages == BACKGROUND_IMAGES) ? &m_crJPEGBackDarkAll : NULL,
      NULL);

   // NOTE : using extra dark color
   if (m_hbrJPEGBackDarkAll)
      DeleteObject (m_hbrJPEGBackDarkAll);
   m_hbrJPEGBackDarkAll = CreateSolidBrush (m_crJPEGBackDarkAll);


   // update transcript
   TranscriptBackground (TRUE);

   // update all MML in CVisView
   DWORD i;
   PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
   for (i = 0; i < m_lPCVisImage.Num(); i++)
      pvi[i]->WindowBackground (TRUE);

   // invalidate rect
   InvalidateRect (m_hWndPrimary, NULL, FALSE);
   if (m_hWndSecond)
      InvalidateRect (m_hWndSecond, NULL, FALSE);
   if (m_hWndTranscript)
      InvalidateRect (m_hWndTranscript, NULL, FALSE);
   if (m_pMapWindow)
      InvalidateRect (m_pMapWindow->m_hWnd, NULL, FALSE);
   if (m_pSlideTop)
      InvalidateRect (m_pSlideTop->m_hWnd, NULL, FALSE);
   if (m_pTickerTape)
      InvalidateRect (m_pTickerTape->m_hWnd, NULL, FALSE);

   PCIconWindow *ppi = (PCIconWindow*) m_lPCIconWindow.Get(0);
   for (i = 0; i < m_lPCIconWindow.Num(); i++)
      InvalidateRect (ppi[i]->m_hWnd, NULL, FALSE);

   PCDisplayWindow *ppd = (PCDisplayWindow*) m_lPCDisplayWindow.Get(0);
   for (i = 0; i < m_lPCDisplayWindow.Num(); i++)
      InvalidateRect (ppd[i]->m_hWnd, NULL, FALSE);
}


/*************************************************************************************
CMainWindow::BackgroundUpdate - Updates the background using an image string.

inputs
   DWORD             dwTextOrImages - BACKGROUND_XXX
   PCWSTR            psz - Image string
returns
   BOOL - TRUE if successful
*/
BOOL CMainWindow::BackgroundUpdate (DWORD dwTextOrImages, PCWSTR psz)
{
   if (!psz || !psz[0])
      return FALSE;

   if (!_wcsicmp(psz, (PWSTR)m_amemImageBackgroundName[dwTextOrImages].p))
      return TRUE;   // already set

   size_t dwResource = (size_t)-1;
   if (!_wcsicmp(psz, L"accountedit"))
      dwResource = IDR_JPGACCOUNTEDIT;
   else if (!_wcsicmp(psz, L"largettspage"))
      dwResource = IDR_JPGLARGETTSPAGE;
   else if (!_wcsicmp(psz, L"linkworldpage"))
      dwResource = IDR_JPGLINKWORLD;
   else if (!_wcsicmp(psz, L"mifopen"))
      dwResource = IDR_JPGMIFOPEN;
   else if (!_wcsicmp(psz, L"newpasswordask"))
      dwResource = IDR_JPGNEWPASSWORDASK;
   else if (!_wcsicmp(psz, L"newpasswordfile"))
      dwResource = IDR_JPGNEWPASSWORDFILE;
   else if (!_wcsicmp(psz, L"relogonpage"))
      dwResource = IDR_JPGRELOGONPAGE;
   else if (!_wcsicmp(psz, L"shardpage"))
      dwResource = IDR_JPGSHARDPAGE;
   else if (!_wcsicmp(psz, L"userpasswordask"))
      dwResource = IDR_JPGUSERPASSWORDASK;
   else if (!_wcsicmp(psz, L"userpasswordfile"))
      dwResource = IDR_JPGUSERPASSWORDFILE;
   else if (!_wcsicmp(psz, L"circumrealitylogo300"))
      dwResource = IDR_JPGLOGO300;
   else if (!_wcsicmp(psz, L"mainplayerpage"))
      dwResource = IDR_JPGMAINPLAYERPAGE;
   else if (!_wcsicmp(psz, L"playerinfo"))
      dwResource = IDR_JPGPLAYERINFO;
   else if (!_wcsicmp(psz, L"readme"))
      dwResource = IDR_JPGREADME;
   else if (!_wcsicmp(psz, L"register"))
      dwResource = IDR_JPGREGISTER;
   else if (!_wcsicmp(psz, L"grass1"))
      dwResource = IDR_JPGTEXTURE1;
   else if (!_wcsicmp(psz, L"clouds1"))
      dwResource = IDR_JPGTEXTURE2;
   else if (!_wcsicmp(psz, L"rock1"))
      dwResource = IDR_JPGTEXTURE3;
   else if (!_wcsicmp(psz, L"sand1"))
      dwResource = IDR_JPGTEXTURE4;
   else if (!_wcsicmp(psz, L"tiles1"))
      dwResource = IDR_JPGTEXTURE5;
   else if (!_wcsicmp(psz, L"woodfloor1"))
      dwResource = IDR_JPGTEXTURE6;
   else if (!_wcsicmp(psz, L"woodfloor2"))
      dwResource = IDR_JPGTEXTURE7;
   else if (!_wcsicmp(psz, L"plywood1"))
      dwResource = IDR_JPGTEXTURE8;
   else if (!_wcsicmp(psz, L"iron1"))
      dwResource = IDR_JPGTEXTURE9;
   else if (!_wcsicmp(psz, L"gravel1"))
      dwResource = IDR_JPGTEXTURE10;
   else if (!_wcsicmp(psz, L"gravel2"))
      dwResource = IDR_JPGTEXTURE11;
   else if (!_wcsicmp(psz, L"bark1"))
      dwResource = IDR_JPGTEXTURE12;
   else if (!_wcsicmp(psz, L"rock2"))
      dwResource = IDR_JPGTEXTURE13;
   else if (!_wcsicmp(psz, L"leaves1"))
      dwResource = IDR_JPGTEXTURE14;
   else if (!_wcsicmp(psz, L"leaves2"))
      dwResource = IDR_JPGTEXTURE15;
   else if (!_wcsicmp(psz, L"soil1"))
      dwResource = IDR_JPGTEXTURE16;
   else if (!_wcsicmp(psz, L"waves1"))
      dwResource = IDR_JPGTEXTURE17;

   if (dwResource == (size_t)-1)
      return FALSE;

   HBITMAP hBmp = JPegToBitmap (dwResource, ghInstance);
   if (!hBmp)
      return FALSE;
   if (!m_aImageBackground[dwTextOrImages].Init (hBmp)) {
      DeleteObject (hBmp);
      return FALSE;
   }
   DeleteObject (hBmp);
   MemZero (&m_amemImageBackgroundName[dwTextOrImages]);
   MemCat (&m_amemImageBackgroundName[dwTextOrImages], (PWSTR) psz);

   // rmember
   m_apJPEGBack[dwTextOrImages].x = m_aImageBackground[dwTextOrImages].Width();
   m_apJPEGBack[dwTextOrImages].y = m_aImageBackground[dwTextOrImages].Height();

   BackgroundUpdate (dwTextOrImages);

   return TRUE;
}

/*************************************************************************************
CMainWindow::BackgroundUpdate - Call this to update the background with a new 360
degree image that has been loaded.

inputs
   DWORD             dwTextOrImages - Background for text or images
   PCImageStore      pis - New image that's loaded. If it's not 360, if it's transient,
                        then ignore
   BOOL              fRequireForceBackground - Set to TRUE if the force-background flag MUST
                     set so that cutscenes will update the backround
*/

void CMainWindow::BackgroundUpdate (DWORD dwTextOrImages, PCImageStore pis, BOOL fRequireForceBackground)
{
   // NOTE: No longer used
   if (pis->m_dwStretch != 4)
      return;

   if (pis->m_fTransient)
      return;

   // BUGFIX - If already updated for this image then dont do again
   if (m_pisBackgroundCur == pis)
      return;

   // BUGFIX - If it has the <ForceBackground v=1/> flag set then use it anyway
   BOOL fForceBackground = FALSE;
#if 0 // may not need this anymore
   PCMMLNode2 pNode = pis->MMLGet ();
   if (pNode && MMLValueGetInt(pNode, L"ForceBackground", 0))
      fForceBackground = TRUE;
   if (fRequireForceBackground && !fForceBackground)
      return;
#endif

   DWORD i;
   if (!fForceBackground) {
      // BUGFIX - Only update the background if it matches one of the
      // other images
      CMem mem, memLayer;
      PWSTR pszFile; // , pszFileLayer;
      if (!MMLToMem (pis->MMLGet(), &mem))
         return;
      mem.CharCat (0);  // just to null terminate
      pszFile = (PWSTR)mem.p;


      PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
      for (i = 0; i < m_lPCVisImage.Num(); i++) {
         PCImageStore pisLayer = pvi[i]->LayerGetImage ();
         if (pisLayer && (pisLayer == pis))
            break;

         // see if this, or any layer matches the image
         if (pvi[i]->LayerFindMatch (pszFile, TRUE))
            break;

#if 0 // old code
         if (!pisLayer)
            continue;

         if (MMLToMem (pisLayer->MMLGet(), &memLayer)) {
            memLayer.CharCat (0);  // just to null terminate
            pszFileLayer = (PWSTR)memLayer.p;

            if (!_wcsicmp(pszFile, pszFileLayer))
               break;
         }
#endif
      } // i
      if (i >= m_lPCVisImage.Num())
         return;
   } // if !fForceBackground

   // get the size in pixels
   RECT r;
   GetClientRect (m_hWndPrimary, &r);
   int iWidth = (r.right - r.left) / 2;
   int iHeight = (r.bottom - r.top) / 2;
   if ((iWidth <= 0) || (iHeight <= 0))
      return;  // error - no size

   pis->Surround360Set (2, m_f360FOV, m_f360Lat, (DWORD)iWidth, (DWORD)iHeight, m_fCurvature);

   PCImageStore pCache = pis->CacheGet (2, iWidth, iHeight, NULL, m_f360Long, m_cBackground);
   if (!pCache)
      return;  // error

#if 0
   // reduce the contrast
   WORD awIntensity[256];
   fp f, fScale;
   for (i = 0; i < 256; i++) {
      f = Gamma (i) / (fp)0x10000;

      f = (fp)pow ((double)f, (double)BACKGROUNDUPDATE_CONTRAST); // contrast

      f *= (fp)0x10000;
      f = min(f, (fp)0xffff);

      awIntensity[i] = (WORD)floor(f + 0.5);
   } // i

   // determine sum of intensities
   __int64 aiSum = 0;
   PBYTE pabPixel = pCache->Pixel (0, 0);
   DWORD dwWidthHeight = pCache->Width() * pCache->Height() * 3;
   for (i = 0; i < dwWidthHeight; i++, pabPixel++)
      aiSum += (__int64)awIntensity[*pabPixel];
   aiSum /= (__int64)dwWidthHeight;

   // adjust level
   if (m_fLightBackground) {
      fScale = 1.0 - (fp)BACKGROUNDUPDATE_INTENSITYLIGHT / (fp)0x10000;
      for (i = 0; i < 256; i++) {
         f = (fp)awIntensity[i]; // BUGFIX - Don't modify * fScale;
         awIntensity[i] = (WORD)floor(f + 0.5) + BACKGROUNDUPDATE_INTENSITYLIGHT;
      } // i
   }
   else if (!m_fLightBackground && (aiSum > BACKGROUNDUPDATE_INTENSITYDARK)) {
      fScale = (fp)BACKGROUNDUPDATE_INTENSITYDARK / (fp)aiSum;
      for (i = 0; i < 256; i++) {
         f = (fp)awIntensity[i]; // BUGFIX - Don't modify * fScale;
         awIntensity[i] = (WORD)floor(f + 0.5);
      } // i
   }
#endif // 0

   m_apJPEGBack[dwTextOrImages].x = pCache->Width();
   m_apJPEGBack[dwTextOrImages].y = pCache->Height();

   // create the bitmaps
   m_aImageBackground[dwTextOrImages].Init (pCache->Width(), pCache->Height());
   MemZero (&m_amemImageBackgroundName[dwTextOrImages]);
   PIMAGEPIXEL pip = m_aImageBackground[dwTextOrImages].Pixel (0, 0);
   PBYTE pabPixel = pCache->Pixel (0, 0);
   DWORD dwWidthHeight = pCache->Width() * pCache->Height();
   for (i = 0; i < dwWidthHeight; i++, pabPixel += 3, pip++) {
      //pip->wRed = awIntensity[pabPixel[0]];
      //pip->wGreen = awIntensity[pabPixel[1]];
      //pip->wBlue = awIntensity[pabPixel[2]];
      pip->wRed = Gamma(pabPixel[0]);
      pip->wGreen = Gamma(pabPixel[1]);
      pip->wBlue = Gamma(pabPixel[2]);

      // BUGFIX - Grey out the m_aImageBackground[dwTextOrImages] too
      WORD wSum = (WORD)(((DWORD)pip->wRed + (DWORD)pip->wGreen + (DWORD)pip->wBlue) / 3);
      wSum /= 2;  // so half greyish
      pip->wRed = pip->wRed / 2 + wSum;
      pip->wGreen = pip->wGreen / 2 + wSum;
      pip->wBlue = pip->wBlue / 2 + wSum;
   } // i

   // remember this
   m_pisBackgroundCur = pis;

   BackgroundUpdate (dwTextOrImages);
}



/*************************************************************************************
CMainWindow::FontCreateIfNecessary - Fills in m_aFont[] and m_adwFontPixelHeight[]
ff m_iTransSize has changed. If no, these are left unchanged

inputs
   HDC      hDC - DC to use. If not specified creates own.
*/
void CMainWindow::FontCreateIfNecessary (HDC hDC)
{
   if ((m_iFontSizeLast == m_iTransSize) && m_ahFont[0])
      return;  // no change

   m_iFontSizeLast = m_iTransSize;

   // delete what's there
   DWORD i;
   for (i = 0; i < NUMMAINFONT; i++)
      if (m_ahFont[i]) {
         DeleteObject (m_ahFont[i]);
         m_ahFont[i] = NULL;
      }
   for (i = 0; i < NUMMAINFONTSIZED; i++)
      if (m_ahFontSized[i]) {
         DeleteObject (m_ahFontSized[i]);
         m_ahFontSized[i] = NULL;
      }

   // figure out the sizes
   int aiSize[NUMMAINFONT];
   BOOL afBold[NUMMAINFONT];
   BOOL afItalic[NUMMAINFONT];
   memset (afBold, 0, sizeof(afBold));
   memset (afItalic, 0, sizeof(afItalic));
   memset (aiSize, 0, sizeof(aiSize));

   // font 1 is big and bold
   aiSize[1] = 1;
   afBold[1] = 1;

   // font 2 is small and italioc
   aiSize[2] = 0; // Small italic unreadable
   afItalic[2] = TRUE;

   HDC hDCCreated = NULL;
   if (!hDC)
      hDC = hDCCreated = GetDC (m_hWndPrimary);

   // create
   fp fSize;
   for (i = 0; i < NUMMAINFONT; i++) {
      fSize = floor (pow ((fp)sqrt(2.0), (fp)aiSize[i] + (fp)m_iTransSize) * -14.0 + 0.5);
         // default font size of 12

      LOGFONT  lf;
      memset (&lf, 0, sizeof(lf));
      lf.lfHeight = FontScaleByScreenSize((int)fSize); 
      lf.lfCharSet = DEFAULT_CHARSET;
      lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
      strcpy (lf.lfFaceName, "Arial");
      if (afBold[i])
         lf.lfWeight = FW_BOLD;
      if (afItalic[i])
         lf.lfItalic = TRUE;
      m_ahFont[i] = CreateFontIndirect (&lf);

      // calculate the height
      HFONT hFontOld = (HFONT) SelectObject (hDC, m_ahFont[i]);
      RECT rDraw;
      memset (&rDraw, 0, sizeof(rDraw));
      rDraw.right = rDraw.bottom = 10000;
      DrawTextW (hDC, L"CircumReality" /* doesn't really matter*/, -1, &rDraw, DT_SINGLELINE | DT_LEFT | DT_TOP | DT_CALCRECT);
      SelectObject (hDC, hFontOld);
      m_adwFontPixelHeight[i] = (DWORD)rDraw.bottom;

   } // i

   // fSize = -20;
   fSize = pow ((fp)sqrt(2.0), 1.0 + (fp)m_iTransSize) * -14.0 + 0.5;
   for (i = 0; i < NUMMAINFONTSIZED; i++, fSize /= sqrt(2.0)) {
      LOGFONT  lf;
      memset (&lf, 0, sizeof(lf));
      lf.lfHeight = FontScaleByScreenSize((int)fSize); 
      lf.lfCharSet = DEFAULT_CHARSET;
      lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
      strcpy (lf.lfFaceName, "Arial");
      m_ahFontSized[i] = CreateFontIndirect (&lf);

      // calculate the height
      HFONT hFontOld = (HFONT) SelectObject (hDC, m_ahFontSized[i]);
      RECT rDraw;
      memset (&rDraw, 0, sizeof(rDraw));
      rDraw.right = rDraw.bottom = 10000;
      DrawTextW (hDC, L"CircumReality" /* doesn't really matter*/, -1, &rDraw, DT_SINGLELINE | DT_LEFT | DT_TOP | DT_CALCRECT);
      SelectObject (hDC, hFontOld);
      m_adwFontSizedPixelHeight[i] = (DWORD)rDraw.bottom;

   } // i

   if (hDCCreated)
      ReleaseDC (m_hWndPrimary, hDCCreated);
}


/*************************************************************************************
CMainWindow::FlashChildWindow - This flashes a specific child window so for tutorial
purposes.

inputs
   RECT        *prScreen - Location in screen coords
returns
   none
*/
#define FLASHROUNDEDSIZE      6
#define FLASHROUNDEDINDENT    (FLASHROUNDEDSIZE/2)
#define FLASHPENSIZE          FLASHROUNDEDINDENT

void CMainWindow::FlashChildWindow (RECT *prScreen)
{
   // find the parent
   RECT rPrimary, rSecondary;
   if (m_hWndPrimary)
      GetWindowRect (m_hWndPrimary, &rPrimary);
   if (m_hWndSecond)
      GetWindowRect (m_hWndSecond, &rSecondary);
   RECT rInterPrimary, rInterSecondary;
   if (!m_hWndPrimary || !IntersectRect (&rInterPrimary, &rPrimary, prScreen))
      memset (&rInterPrimary, 0, sizeof(rInterPrimary));
   if (!m_hWndSecond || !IntersectRect (&rInterSecondary, &rSecondary, prScreen))
      memset (&rInterSecondary, 0, sizeof(rInterSecondary));

   BOOL fPrimary = TRUE;
   if ( ((rInterSecondary.right - rInterSecondary.left) * (rInterSecondary.bottom - rInterSecondary.top)) >
      ((rInterPrimary.right - rInterPrimary.left) * (rInterPrimary.bottom - rInterPrimary.top)) )
      fPrimary = FALSE;

   HWND hWndParent = fPrimary ? m_hWndPrimary : m_hWndSecond;

   // convert screen to primary secondary window coords
   // POINT *paScreen = (POINT*)prScreen;
   // ScreenToClient (hWndParent, paScreen + 0);
   // ScreenToClient (hWndParent, paScreen + 1);

   // if already have flash window then delete
   if (m_hWndFlashWindow)
      DestroyWindow (m_hWndFlashWindow);

   // create window
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = FlashWndProc;
   wc.lpszClassName = "FlashWindow";
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = NULL;
   wc.hCursor = NULL;
   wc.hbrBackground = NULL;
   RegisterClass (&wc);

   // create
   m_dwFlashWindowTicks = 6;
   m_hWndFlashWindow = CreateWindowEx (
      WS_EX_NOACTIVATE | WS_EX_TRANSPARENT /*| WS_EX_TOPMOST*/,
      wc.lpszClassName, "FlashWindow",
      /*WS_CHILD | WS_CLIPSIBLINGS |*/ WS_POPUP /*| WS_VISIBLE*/,
      prScreen->left - FLASHROUNDEDINDENT , prScreen->top - FLASHROUNDEDINDENT,
      prScreen->right - prScreen->left + 2 * FLASHROUNDEDINDENT, prScreen->bottom - prScreen->top + 2 * FLASHROUNDEDINDENT,
      NULL, // hWndParent,
      NULL, ghInstance, (PVOID) this);

   // done
   return;
}



/*************************************************************************************
CMainWindow::WndProcFlash - Manages the window calls
*/
LRESULT CMainWindow::WndProcFlash (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWndFlashWindow = hWnd;
         SetTimer (hWnd, TIMER_FLASHWINDOW, 200, NULL);     // BUGFIX - Changed tooltip update timers to 200 ms, from 100ms

         // fake an immediate timer
         SendMessage (hWnd, WM_TIMER, TIMER_FLASHWINDOW, 0);
      }
      break;

   case WM_TIMER:
      if (wParam == TIMER_FLASHWINDOW) {
         // decreate the ticks
         if (m_dwFlashWindowTicks)
            m_dwFlashWindowTicks--;

         // if nothing left then destroy window
         if (!m_dwFlashWindowTicks) {
            DestroyWindow (m_hWndFlashWindow);
            m_hWndFlashWindow = NULL;
         }

         // else, alternate between show/hide
         if (m_dwFlashWindowTicks % 2) {
#if 0 // not used
            // figure out which window this is over
            RECT rThis;
            GetWindowRect (hWnd, &rThis);

            // find the parent
            RECT rPrimary, rSecondary;
            if (m_hWndPrimary)
               GetWindowRect (m_hWndPrimary, &rPrimary);
            if (m_hWndSecond)
               GetWindowRect (m_hWndSecond, &rSecondary);
            RECT rInterPrimary, rInterSecondary;
            if (!m_hWndPrimary || !IntersectRect (&rInterPrimary, &rPrimary, &rThis))
               memset (&rInterPrimary, 0, sizeof(rInterPrimary));
            if (!m_hWndSecond || !IntersectRect (&rInterSecondary, &rSecondary, &rThis))
               memset (&rInterSecondary, 0, sizeof(rInterSecondary));

            BOOL fPrimary = TRUE;
            if ( ((rInterSecondary.right - rInterSecondary.left) * (rInterSecondary.bottom - rInterSecondary.top)) >
               ((rInterPrimary.right - rInterPrimary.left) * (rInterPrimary.bottom - rInterPrimary.top)) )
               fPrimary = FALSE;

            HWND hWndParent = fPrimary ? m_hWndPrimary : m_hWndSecond;

            HWND hWndPrior = GetNextWindow (hWndParent, GW_HWNDPREV);
#endif

#if 0 // not used
            HWND hWndFore = GetForegroundWindow();
            while (hWndFore && ((hWndFore != m_hWndPrimary) && (hWndFore != m_hWndSecond)) )
               hWndFore = GetParent(hWndFore);
            if (hWndFore && ((hWndFore == m_hWndPrimary) || (hWndFore == m_hWndSecond)))
#endif // 0
               SetWindowPos (hWnd, (HWND)HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            }
         else
            SetWindowPos (hWnd, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

#if 0
         // redraw this
         RECT rWindow;
         GetWindowRect (hWnd, &rWindow);
         
         // invalidate in parent
         HWND hWndParent = GetParent (hWnd);
         RECT rInvalid;
         rInvalid = rWindow;
         ScreenToClient (hWndParent, ((POINT*)&rInvalid)+0);
         ScreenToClient (hWndParent, ((POINT*)&rInvalid)+1);
         InvalidateRect (hWndParent, &rInvalid, FALSE);

         // invalidate all the childen
         HWND hChild;
         RECT rChild;
         for (hChild = GetTopWindow(hWndParent); hChild; hChild = GetNextWindow (hChild, GW_HWNDNEXT)) {
            GetWindowRect (hChild, &rChild);
            if (!IntersectRect (&rInvalid, &rChild, &rWindow))
               continue;

            // invalidate this
            rInvalid = rChild;
            ScreenToClient (hChild, ((POINT*)&rInvalid)+0);
            ScreenToClient (hChild, ((POINT*)&rInvalid)+1);
            InvalidateRect (hChild, &rInvalid, FALSE);
         } // hChild
#endif // 0

         return 0;
      }
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC = BeginPaint (hWnd, &ps);

         //HBRUSH hbr = CreateSolidBrush (RGB(0xff, 0xff, 0x00));
         RECT rClient;
         GetClientRect (hWnd, &rClient);
         HPEN hPen = CreatePen (PS_DASH, FLASHPENSIZE, RGB(0xff,0xff,0x00));
         HPEN hPenOld = (HPEN) SelectObject (hDC, hPen);
         HBRUSH hHollow = (HBRUSH) GetStockObject (HOLLOW_BRUSH);
         HBRUSH hbrOld = (HBRUSH) SelectObject (hDC, hHollow);
         // RoundRect (hDC, rClient.left+FLASHROUNDEDINDENT, rClient.top+FLASHROUNDEDINDENT, rClient.right-FLASHROUNDEDINDENT, rClient.bottom-FLASHROUNDEDINDENT,
         //   FLASHROUNDEDSIZE, FLASHROUNDEDSIZE);
         MoveToEx (hDC, rClient.left+FLASHROUNDEDINDENT, rClient.top+FLASHROUNDEDINDENT, NULL);
         LineTo (hDC, rClient.right-FLASHROUNDEDINDENT, rClient.top+FLASHROUNDEDINDENT);
         LineTo (hDC, rClient.right-FLASHROUNDEDINDENT, rClient.bottom-FLASHROUNDEDINDENT);
         LineTo (hDC, rClient.left+FLASHROUNDEDINDENT, rClient.bottom-FLASHROUNDEDINDENT);
         LineTo (hDC, rClient.left+FLASHROUNDEDINDENT, rClient.top+FLASHROUNDEDINDENT);
         // FillRect (hDC, &rClient, hbr);
         // DeleteObject (hbr);
         SelectObject (hDC, hbrOld);
         SelectObject (hDC, hPenOld);
         DeleteObject (hPen);
         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_DESTROY:
      KillTimer (hWnd, TIMER_FLASHWINDOW);
      break;   // fall through
   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*************************************************************************************
CMainWindow::HypnoEffect - Hypno effect from MML

inputs
   PCMMLNode2        pNodeHypnoEffect - Effect
returns
   none
*/
void CMainWindow::HypnoEffect (PCMMLNode2 pNodeHypnoEffect)
{
   // hypno effect
   if (!pNodeHypnoEffect)
      return;

   fp fDuration = MMLValueGetDouble (pNodeHypnoEffect, L"duration", 0.0);
   fp fPriority = MMLValueGetDouble (pNodeHypnoEffect, L"priority", 1.0);
   PWSTR pszName = MMLValueGet (pNodeHypnoEffect, L"name");

   if (pszName)
      HypnoEffect (pszName, fDuration, fPriority);
}


/*************************************************************************************
CMainWindow::HypnoEffect - Call to set a hypno effect.

inputs
   PWSTR          psz - Hypno string
   fp             fDuration - Duration in seconds. If 0 then permanent
   fp             fPriority - Priority level. Only used for finite-duration hypno effects.
returns
   none
*/
void CMainWindow::HypnoEffect (PWSTR psz, fp fDuration, fp fPriority)
{
   if (fDuration) {
      // add
      HYPNOEFFECTINFO hei;
      memset (&hei, 0, sizeof(hei));
      hei.fDuration = fDuration;
      hei.fPriority = fPriority;

      m_lHYPNOEFFECTINFO.Add (&hei);
      m_lHypnoEffectName.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   }
   else {
      // infinite one
      MemZero (&m_memHypnoEffectInfinite);
      MemCat (&m_memHypnoEffectInfinite, psz);
   }

   // do the timer
   HypnoEffectTimer (0.0);
}


/*************************************************************************************
CMainWindow::HypnoEffectTimer - Call to update the hypno effect on a timer

inputs
   fp             fTime - Time since last called.
returns
   none
*/
void CMainWindow::HypnoEffectTimer (fp fTime)
{
   // clear out dead hypnoeffects
   PHYPNOEFFECTINFO phei = (PHYPNOEFFECTINFO) m_lHYPNOEFFECTINFO.Get(0);
   DWORD i;
   DWORD dwBest = (DWORD)-1;
   fp fBestPriority = 0;
   for (i = 0; i < m_lHYPNOEFFECTINFO.Num(); i++) {
      phei[i].fDuration -= fTime;

      // delete?
      if (phei[i].fDuration <= 0.0) {
         m_lHYPNOEFFECTINFO.Remove (i);
         m_lHypnoEffectName.Remove (i);
         i--;
         phei = (PHYPNOEFFECTINFO) m_lHYPNOEFFECTINFO.Get(0);
         continue;
      }

      // else, is this best
      if ((dwBest == (DWORD)-1) || (phei[i].fPriority > fBestPriority)) {
         dwBest = i;
         fBestPriority = phei[i].fPriority;
      }
   } // i

   // if no hypno object then stop here
   if (!m_pHM)
      return;

   // if found a best then set that
   if (dwBest != (DWORD)-1)
      m_pHM->EffectSet ((PWSTR)m_lHypnoEffectName.Get(dwBest));
   else
      m_pHM->EffectSet ((PWSTR)m_memHypnoEffectInfinite.p);
}



/*************************************************************************************
CMainWindow::RandomActionTranscript - Random action from the transcript window

inputs
   PCMem          pMemAction - Filled in with the action string
   GUID           *pgObject - Filled in with the object this is associated with, or NULL if none
   LANGID         *pLangID - Filled in with the language ID, or 0 if unknown
returns
   BOOL - TRUE if action filled in
*/
BOOL CMainWindow::RandomActionTranscript (PCMem pMemAction, GUID *pgObject, LANGID *pLangID)
{
   *pgObject = GUID_NULL;
   *pLangID = 0;

   CListVariable lLinks;
   CListFixed lLANGID;
   lLANGID.Init (sizeof(LANGID));
   LinkExtractFromPage (m_pPageTranscript, &lLinks, &lLANGID);

   if (!lLinks.Num())
      return FALSE;

   // random
   DWORD dwIndex = (DWORD)rand() % lLinks.Num();
   MemZero (pMemAction);
   MemCat (pMemAction, (PWSTR)lLinks.Get(dwIndex));
   *pLangID = *((LANGID*)lLANGID.Get(dwIndex));
   if (m_pMenuContext)
      *pgObject = m_pMenuContext->m_gID;

   return TRUE;
}


/*************************************************************************************
CMainWindow::RandomActionVisImage - Picks a random vis image and does an action on it.

inputs
   PCMem          pMemAction - Filled in with the action string
   GUID           *pgObject - Filled in with the object this is associated with, or NULL if none
   LANGID         *pLangID - Filled in with the language ID, or 0 if unknown
returns
   BOOL - TRUE if action filled in
*/
BOOL CMainWindow::RandomActionVisImage (PCMem pMemAction, GUID *pgObject, LANGID *pLangID)
{
   *pgObject = GUID_NULL;
   *pLangID = 0;

   if (!m_lPCVisImage.Num())
      return FALSE;

   PCVisImage *pvi = (PCVisImage*)m_lPCVisImage.Get(0);
   PCVisImage pv = pvi[(DWORD)rand() % m_lPCVisImage.Num()];
   *pgObject = pv->m_gID;
   MemZero (pMemAction);

   // hot spot?
   DWORD dwIndex;
   if (pv->m_plPCCircumrealityHotSpot && pv->m_plPCCircumrealityHotSpot->Num() && (rand() % 2)) {
      // hot spot
      dwIndex = (DWORD)rand() % pv->m_plPCCircumrealityHotSpot->Num();
      PCCircumrealityHotSpot *ppch = (PCCircumrealityHotSpot*)pv->m_plPCCircumrealityHotSpot->Get(dwIndex);
      PCCircumrealityHotSpot pch = ppch[0];
      *pLangID = pch->m_lid;

      if (pch->m_dwCursor == 10) {
         if (!pch->m_lMenuDo.Num())
            return FALSE;
         MemCat (pMemAction, (PWSTR)pch->m_lMenuDo.Get((DWORD)rand() % pch->m_lMenuDo.Num()));
      }
      else
         MemCat (pMemAction, pch->m_ps->Get());

      return TRUE;
   }

   // NOTE: DON'T bother with context menu since if select the object, will ahve access
   // to the context menu

   // see if there's a some MML
   CListVariable lLinks;
   CListFixed lLANGID;
   lLANGID.Init (sizeof(LANGID));
   LinkExtractFromPage (pv->m_pPage, &lLinks, &lLANGID);

   if (!lLinks.Num())
      return FALSE;

   // random
   dwIndex = (DWORD)rand() % lLinks.Num();
   MemZero (pMemAction);
   MemCat (pMemAction, (PWSTR)lLinks.Get(dwIndex));

   PVILAYER pvil = (PVILAYER)pv->m_lVILAYER.Get(0);
   *pLangID = (pvil && pvil->lid) ? pvil->lid : *((LANGID*)lLANGID.Get(dwIndex));

   return TRUE;
}

/*************************************************************************************
CMainWindow::RandomActionResVerb - Performs a random action from the toolbar.

inputs
   PCMem          pMemAction - Filled in with the action string
   GUID           *pgObject - Filled in with the object this is associated with, or NULL if none
   LANGID         *pLangID - Filled in with the language ID, or 0 if unknown
returns
   BOOL - TRUE if action filled in
*/
BOOL CMainWindow::RandomActionResVerb (PCMem pMemAction, GUID *pgObject, LANGID *pLangID)
{
   *pgObject = GUID_NULL;
   *pLangID = 0;
   if (!m_pResVerb)
      return FALSE;

   CListVariable lActions;
   CListFixed lLANGID;
   lLANGID.Init (sizeof(LANGID));

   DWORD i;
   PCResVerbIcon *ppRVI = (PCResVerbIcon*)m_pResVerb->m_lPCResVerbIcon.Get(0);
   PCResVerbIcon pRVI;
   for (i = 0; i < m_pResVerb->m_lPCResVerbIcon.Num(); i++) {
      pRVI = ppRVI[i];

      PWSTR pszCommand = (PWSTR) pRVI->m_memDo.p;
      if (pszCommand && pszCommand[0]) {
         lActions.Add (pszCommand, (wcslen(pszCommand)+1)*sizeof(WCHAR));
         lLANGID.Add (&pRVI->m_lid);
      }
   } // i

   // if no actions then done
   if (!lActions.Num())
      return FALSE;

   DWORD dwIndex = (DWORD)rand() % lActions.Num();
   MemZero (pMemAction);
   MemCat (pMemAction, (PWSTR)lActions.Get(dwIndex));
   *pLangID = *((LANGID*)lLANGID.Get(dwIndex));

   return TRUE;
}


/*************************************************************************************
CMainWindow::RandomActionClickOnIcon - Randomly click on an icon

returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::RandomActionClickOnIcon (void)
{
   if (!m_lPCIconWindow.Num())
      return FALSE;

   PCIconWindow *ppi = (PCIconWindow*) m_lPCIconWindow.Get(0);
   PCIconWindow pi = ppi[(DWORD)rand() % m_lPCIconWindow.Num()];

   if (!pi->m_lPCIWGroup.Num())
      return FALSE;

   PCIWGroup *ppIWG = (PCIWGroup*)pi->m_lPCIWGroup.Get(0);
   PCIWGroup pIWG = ppIWG[(DWORD)rand() % pi->m_lPCIWGroup.Num()];

   if (!pIWG->m_lPCVisImage.Num())
      return FALSE;

   PCVisImage *ppvi = (PCVisImage*)pIWG->m_lPCVisImage.Get(0);
   PCVisImage pvi = ppvi[(DWORD)rand() % pIWG->m_lPCVisImage.Num()];

   // set this as main
   SetMainWindow (pvi);
   return TRUE;
}



/*************************************************************************************
CMainWindow::RandomActionTab - Randomly switch tabs

returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::RandomActionTab (void)
{
   TabSwitch ((DWORD)rand() % NUMTABS, TRUE);

   return TRUE;
}


/*************************************************************************************
CMainWindow::RoomCenter - Centers the map on a given room. This also shows the map.

inputs
   GUID           *pgRoom - Room
returns
   BOOL - TRUE if success
*/
BOOL CMainWindow::RoomCenter (GUID *pgRoom)
{
   // find the room
   if (!m_pMapWindow)
      return FALSE;
   PCMapRoom pCurRoom = m_pMapWindow->RoomFind (pgRoom);
   if (!pCurRoom)
      return FALSE;

   // where is the cursor
   POINT pCur;
   GetCursorPos (&pCur);
   RECT rWindow;
   DWORD dwMonitor = 0;
   if (gfMonitorUseSecond && m_hWndSecond) {
      GetWindowRect (m_hWndSecond, &rWindow);
      if (PtInRect (&rWindow, pCur))
         dwMonitor = 1;
   }


   GetClientRect (dwMonitor ? m_hWndSecond : m_hWndPrimary, &rWindow);
   ScreenToClient (dwMonitor ? m_hWndSecond : m_hWndPrimary, &pCur);

   // actual points that want
   int iWidth = (rWindow.right - rWindow.left) / 4;
   int iHeight = iWidth;   // so square
   RECT rCover;
   rCover.left = max (pCur.x - iWidth/2, rWindow.left);
   rCover.right = min (pCur.x + iWidth/2, rWindow.right);
   rCover.top = max (pCur.y - iHeight/2, rWindow.top);
   rCover.bottom = min (pCur.y + iHeight/2, rWindow.bottom);
   if ((rCover.right <= rCover.left) || (rCover.bottom <= rCover.top))
      return FALSE;  // not likely to happen

   // save this
   TABWINDOW tw;
   DWORD dwMonitorOld;
   RECT rLocOld;
   BOOL fHiddenOld, fTitleOld;
   ChildLocGet (TW_MAP, NULL, &dwMonitorOld, &rLocOld, &fHiddenOld, &fTitleOld);
   memset (&tw, 0, sizeof(tw));
   tw.dwMonitor = dwMonitor;
   tw.dwType = TW_MAP;
   tw.fVisible = !fHiddenOld;
   tw.fTitle = TRUE;
   if ((rWindow.right <= rWindow.left) || (rWindow.bottom <= rWindow.top))
      return FALSE;
   tw.pLoc.p[0] = (fp)(rCover.left - rWindow.left) / (fp)(rWindow.right - rWindow.left);
   tw.pLoc.p[1] = (fp)(rCover.right - rWindow.left) / (fp)(rWindow.right - rWindow.left);
   tw.pLoc.p[2] = (fp)(rCover.top - rWindow.top) / (fp)(rWindow.bottom - rWindow.top);
   tw.pLoc.p[3] = (fp)(rCover.bottom - rWindow.top) / (fp)(rWindow.bottom - rWindow.top);
   ChildLocSave (&tw, NULL);

   // move it
   ChildShowMove (m_pMapWindow->m_hWnd, TW_MAP, NULL);

   // center on this room
   m_pMapWindow->RoomCenter (pCurRoom);

   // make sure it's showing
   m_pMapWindow->m_fHiddenByServer = FALSE;
   m_pMapWindow->m_fHiddenByUser = FALSE;
   m_pMapWindow->Show (TRUE);

   return TRUE;
}


/*************************************************************************************
LinkExtractFromPage - This extracts links from a page.

inputs
   PCMMLNode         pNode - Node to extract from. Can be NULL.
   PCListVariable    plLinks - Link href strings are added
   PCListFixed       plLinksLANGID - Language ID for the link, if known. 0 if unknown
returns
   none
*/
void LinkExtractFromPage (PCMMLNode pNode, PCListVariable plLinks, PCListFixed plLinksLANGID)
{
   if (!pNode)
      return;

   // sub elements
   DWORD i;
   PCMMLNode pSub;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (pSub)
         LinkExtractFromPage (pSub, plLinks, plLinksLANGID);
   } // i

   // extract the link
   PWSTR pszLink = pNode->AttribGet (L"href");
   if (!pszLink)
      return;

   // allow "ml:", which is in the transcript for a menu link
   LANGID lid = 0;
   if (!_wcsnicmp(pszLink, L"ml:", 3)) {
      lid = _wtoi(pszLink + 3);
      pszLink = wcschr (pszLink + 3, L':');
      if (!pszLink)
         return;
      pszLink++;
   }
   else if (wcschr (pszLink, L':'))
      return;  // in general, don't allow anything with a colon

   if (!_wcsnicmp(pszLink, L"attack", 6))
      return;

   plLinks->Add (pszLink, (wcslen(pszLink)+1)*sizeof(WCHAR));
   plLinksLANGID->Add (&lid);

}



/*************************************************************************************
LinkExtractFromPage - This extracts links from a page.

inputs
   PCEscPage         pPage - Page. Can be NULL.
   PCListVariable    plLinks - Link href strings are added
   PCListFixed       plLinksLANGID - Language ID for the link, if known. 0 if unknown
returns
   none
*/
void LinkExtractFromPage (PCEscPage pPage, PCListVariable plLinks, PCListFixed plLinksLANGID)
{
   if (!pPage)
      return;

   LinkExtractFromPage (pPage->m_pNode, plLinks, plLinksLANGID);
}

// BUGBUG - When size transcript window vertically, beyond maximum size,
// shows whiteness below. I think this is due to a bug in escarpment. NOt
// a big priority

// BUGBUG - watch out for error messages from server (=> disconnect) and deal with

// BUGBUG - will eventually send message to server about what language want
// to use, and have the server store that away

// BUGBUG - eventually UI for real-language choice

// BUGBUG - eventually UI option for how fast scroll

// BUGBUG - may eventually have UI settins for volume of speech vs. sounds vs. music

// BUGBUG - if an image with hotspots is still being drawn, need to generate a false
// image so that can click on the hotspots. That way player isn't out of action
// while the room is drawing or downloading

// BUGBUG - there is a race condition on startup. If break in the TTS speak thread
// then will end up redrawing all the 360-degree and other images in the test

// BUGBUG - the race condition might be caused by loading in z:\bin\CompressString.me3???

// BUGBUG - If have a href=http://www.mxac.com.au, this sends the command to the
// server to be processed. Should these commands instead to a winexec from the clients
// computer? That way can have links to other web pages?

// BUGBUG - may not wish to use comboboxes in transcript because if the window is updated
// while the combobox is open, get a crash.. either fix escarpment, or go to radio
// buttons

// BUGBUG - shutton down while still rendering causes some major memory leaks

// BUGBUG - occasionally get to point where focus on MML pages seems messed up, and cant
// click on thier controls without getting a "dont click here" beep...




// BUGBUG - MIFL - "You combine/seperate a torch with 3 matches"... 1) shouldnt be able to do this,
// 2) "You" was used when ray was doing the work.

// BUGBUG - if shut down nicely while downloading lots of data, may not finish downloading
// data because server terminates the connection
