/*************************************************************************************
CHypnoManager.cpp - Hypnotic backgrounds

begun 20/4/09 by Mike Rozak.
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

#define CELLULAR_STARGATE     0
#define CELLULAR_SWIRL        1

#define NOISE_UNKNOWN         0  // unknown noise
#define NOISE_LIKE            1  // like effect
#define NOISE_DISLIKE         2
#define NOISE_TRUST           3
#define NOISE_MISTRUST        4
#define NOISE_HAPPY           5
#define NOISE_SAD             6
#define NOISE_ANGRY           7
#define NOISE_AFRAID          8
#define NOISE_BLUESKYCLOUDS   9
#define NOISE_STORMCLOUDS     10
#define NOISE_GREYDAY         11
#define NOISE_WATERLAPPING    12
#define NOISE_WATERFLOWING    13
#define NOISE_SMOKERISING     14
#define NOISE_FOG             15
#define NOISE_SICK            16
#define NOISE_FOREST          17
#define NOISE_FLAMES          18
#define NOISE_DARK            19

#define HEN_NOISESIZE         16 // number of noise points
#define HEN_COLORNOISESIZE    3  // 0 = r, 1 = g, 2 = b
#define HEN_NOISEPIXELS       1024  // how accurately the noise will be calculated
#define HEN_INTERPRGBWITHOLDTIME 0.5   // interpolate for 1/2 sec
#define HEN_INTERPCOLORMAPWITHOLDTIME  0.5   // interpolate the color map with the old one
#define HEN_INTERPSCALEWITHOLDTIME     2.0   // interpolate scaling the noise over 2 seconds

#define CELLULAR_FRAMERATEPIXELS 10.00  // pixels per quarter second
#define CELLULAR_FRAMERATEREST   0.10  // how much rest changes

#define HM_RESCALE      3     // scale so don't do as much calculations
   // BUGFIX - Changed from 4 to 3 since didn't look that great with cellular automation as 4


// HYPNOCELL - Information for each cell
typedef struct {
   short          sX;      // get from x pixels relative, per 1/10th of a second animation
   short          sY;      // get from Y pixels relative, per 1/10th of a second animation
   char           cBright; // increase/decrase brightness, per 1/10th of a second animation
   char           cColor;  // increase/decrease color, per 1/10th of a second animation
   BYTE           bBlur;   // blur speed
   BYTE           abFill[1];
} HYPNOCELL, *PHYPNOCELL;

static PCWSTR gpszCellularColon = L"cellular:";
static PCWSTR gpszNoiseColon = L"noise:";

/*************************************************************************************
CHypnoManager::Constructor and detructor
*/
CHypnoManager::CHypnoManager (void)
{
   InitializeCriticalSection (&m_CritSec);
   InitializeCriticalSection (&m_CritSecBmp);

   m_fRunning = FALSE;
   m_dwMonitors = 1;
   memset (m_apMonitorRes, 0, sizeof(m_apMonitorRes));
   m_fLightBackground = TRUE;
   m_fLowPower = FALSE;
   MemZero (&m_memEffect);
   memset (m_ahbmpBack, 0, sizeof(m_ahbmpBack));
   memset (m_apBackRes, 0, sizeof(m_apBackRes));
   memset (m_ahThread, 0, sizeof(m_ahThread));
   memset (m_apHypnoEffect, 0, sizeof(m_apHypnoEffect));

   // create all the signals now
   DWORD i;
   for (i = 0; i < HYPNOTHREADS; i++) {
      m_ahSignalToThread[i] = CreateEvent (NULL, FALSE, FALSE, NULL);
      m_ahSignalFromThread[i] = CreateEvent (NULL, FALSE, FALSE, NULL);
   } // i
}

CHypnoManager::~CHypnoManager (void)
{
   // make sure is shut down
   EnterCriticalSection (&m_CritSec);
   if (m_fRunning) {
      LeaveCriticalSection (&m_CritSec);
      ShutDown ();
      EnterCriticalSection (&m_CritSec);
   }
   LeaveCriticalSection (&m_CritSec);

   // delete
   DWORD i;
   for (i = 0; i < HYPNOTHREADS; i++) {
      CloseHandle (m_ahSignalToThread[i]);
      CloseHandle (m_ahSignalFromThread[i]);
   } // i

   DeleteCriticalSection (&m_CritSec);
   DeleteCriticalSection (&m_CritSecBmp);

   for (i = 0; i < HYPNOMONITORS; i++)
      if (m_ahbmpBack[i]) {
         DeleteObject (m_ahbmpBack[i]);
         m_ahbmpBack[i] = NULL;
      }
}


/*************************************************************************************
CHypnoManager::MonitorsSet - Set the number of monitors
*/
void CHypnoManager::MonitorsSet (DWORD dwMonitors)
{
   EnterCriticalSection (&m_CritSec);
   m_dwMonitors = dwMonitors;
   LeaveCriticalSection (&m_CritSec);
}


/*************************************************************************************
CHypnoManager::MonitorResSet - The the resolution of a specific monitor
*/
void CHypnoManager::MonitorResSet (DWORD dwMonitor, int iX, int iY)
{
   if (dwMonitor >= HYPNOMONITORS)
      return;

   EnterCriticalSection (&m_CritSec);
   m_apMonitorRes[dwMonitor].x = (iX+HM_RESCALE-1) / HM_RESCALE;
   m_apMonitorRes[dwMonitor].y = (iY+HM_RESCALE-1) / HM_RESCALE;
   LeaveCriticalSection (&m_CritSec);
}


/*************************************************************************************
CHypnoManager::LightBackgroundSet - So hypnobackground knows if background is light
*/
void CHypnoManager::LightBackgroundSet (BOOL fLightBackground)
{
   EnterCriticalSection (&m_CritSec);
   m_fLightBackground = fLightBackground;
   LeaveCriticalSection (&m_CritSec);
}

/*************************************************************************************
CHypnoManager::LowPowerSet - So hypnobackground knows if in low power mode
*/
void CHypnoManager::LowPowerSet (BOOL fLowPower)
{
   EnterCriticalSection (&m_CritSec);
   m_fLowPower = fLowPower;
   LeaveCriticalSection (&m_CritSec);
}



/*************************************************************************************
CHypnoManager::EffectSet - So hypnobackground knows what effect to use.
*/
void CHypnoManager::EffectSet (PWSTR pszEffect)
{
   EnterCriticalSection (&m_CritSec);
   MemZero (&m_memEffect);
   MemCat (&m_memEffect, pszEffect);
   LeaveCriticalSection (&m_CritSec);
}


/*************************************************************************************
CHypnoManager::BitBltImage - Do a bitblit of image from the background

inputs
   HDC         hDC - HDC to blit into
   RECT        *prClient - Client
   DWORD       dwMonitor - Monitor
*/
BOOL CHypnoManager::BitBltImage (HDC hDC, RECT *prClient, DWORD dwMonitor)
{
   if (dwMonitor >= HYPNOMONITORS)
      return FALSE;


   EnterCriticalSection (&m_CritSecBmp);
   if (!m_ahbmpBack[dwMonitor] || 
      (((prClient->right - prClient->left) + HM_RESCALE-1)/HM_RESCALE != m_apBackRes[dwMonitor].x) ||
      (((prClient->bottom - prClient->top) + HM_RESCALE-1)/HM_RESCALE != m_apBackRes[dwMonitor].y) ) {
         LeaveCriticalSection (&m_CritSecBmp);
         return FALSE;
   }

   HDC hDCBmp = CreateCompatibleDC (hDC);
   SelectObject (hDCBmp, m_ahbmpBack[dwMonitor]);
   int   iOldMode;
   iOldMode = SetStretchBltMode (hDC, COLORONCOLOR);
   StretchBlt(
      hDC, prClient->left, prClient->top,
      m_apBackRes[dwMonitor].x*HM_RESCALE, m_apBackRes[dwMonitor].y*HM_RESCALE,
      hDCBmp, 0, 0, m_apBackRes[dwMonitor].x, m_apBackRes[dwMonitor].y,
      SRCCOPY);
   SetStretchBltMode (hDC, iOldMode);
   //BitBlt (hDC, prClient->left, prClient->top, prClient->right - prClient->left, prClient->bottom - prClient->top,
   //   hDCBmp, 0, 0, SRCCOPY);
   DeleteDC (hDCBmp);

   LeaveCriticalSection (&m_CritSecBmp);

   return TRUE;
}


/*************************************************************************************
HypnoManagerThreadProc - Thread that handles the internet.
*/

DWORD WINAPI HypnoManagerThreadProc(LPVOID lpParameter)
{
   PHMTHREADINFO pThread = (PHMTHREADINFO) lpParameter;

   pThread->pThis->HypnoManagerThread (pThread->dwThread);

   return 0;
}

/*************************************************************************************
CHypnoManager::Init - Initializes the hypnomanager
*/
BOOL CHypnoManager::Init (void)
{
   EnterCriticalSection (&m_CritSec);
   if (m_fRunning) {
      LeaveCriticalSection (&m_CritSec);
      return FALSE;
   }

   // create all the threads
   DWORD i, dwID;
   memset (&m_aHMTHREADINFO, 0, sizeof(m_aHMTHREADINFO));
   for (i = 0; i < HYPNOTHREADS; i++) {
      m_aHMTHREADINFO[i].dwThread = i;
      m_aHMTHREADINFO[i].pThis = this;
      m_afThreadWantActive[i] = TRUE;   // NOTE: This is okay because still in critical section
      m_afThreadActive[i] = TRUE;

      m_ahThread[i] = CreateThread (NULL, ESCTHREADCOMMITSIZE, HypnoManagerThreadProc, &m_aHMTHREADINFO[i], 0, &dwID);
      SetThreadPriority (m_ahThread[i], VistaThreadPriorityHack(THREAD_PRIORITY_LOWEST));
   } // i

   m_fRunning = TRUE;
   LeaveCriticalSection (&m_CritSec);
   return TRUE;
}


/*************************************************************************************
CHypnoManager::ShutDown - Causes the threads to shut down
*/
BOOL CHypnoManager::ShutDown (void)
{
   // check to see if it's already running
   EnterCriticalSection (&m_CritSec);
   if (!m_fRunning) {
      LeaveCriticalSection (&m_CritSec);
      return FALSE;
   }

   // set the flag
   m_fRunning = FALSE;
   m_afThreadWantActive[0] = FALSE;
   SetEvent (m_ahSignalToThread[0]);
      // this will cascade to all the threads

   LeaveCriticalSection (&m_CritSec);

   // wait for all the theads to shut down
   DWORD i;
   for (i = 0; i < HYPNOTHREADS; i++) {
      WaitForSingleObject (m_ahThread[i], INFINITE);
      CloseHandle (m_ahThread[i]);
      m_ahThread[i] = FALSE;
   } // i

   return TRUE;
}


/*************************************************************************************
CHypnoManager::HypnoManagerThread - Do handle the thread
*/
void CHypnoManager::HypnoManagerThread (DWORD dwThread)
{
   PCImageStore apis[HYPNOMONITORS];
   DWORD i;
   for (i = 0; i < HYPNOMONITORS; i++) {
      apis[i] = dwThread ? NULL : (new CImageStore);
      if (apis[i])
         apis[i]->Init (1, 1); // so have something
   } // i

   CMem amemEffectLast[HYPNOMONITORS], memEffect;
   DWORD dwMonitors, dwMonitorsLast;
   POINT apMonitorRes[HYPNOMONITORS], apMonitorResLast[HYPNOMONITORS];
   BOOL fLightBackground, fLightBackgroundLast;
   BOOL fLowPower, fLowPowerLast;
   for (i = 0; i < HYPNOMONITORS; i++)
      MemZero (&amemEffectLast[i]);
   MemZero (&memEffect);
   dwMonitors = dwMonitorsLast = m_dwMonitors;
   memcpy (apMonitorRes, m_apMonitorRes, sizeof(m_apMonitorRes));
   memcpy (apMonitorResLast, m_apMonitorRes, sizeof(m_apMonitorRes));
   fLightBackground = fLightBackgroundLast = m_fLightBackground;
   fLowPower = fLowPowerLast = m_fLowPower;
   DWORD dwTimeLast = 0;

   while (TRUE) {
      // see if supposed to shut down or anything
      EnterCriticalSection (&m_CritSec);

      // copy over all the params
      MemZero (&memEffect);
      MemCat (&memEffect, (PWSTR)m_memEffect.p);
      dwMonitors = m_dwMonitors;
      memcpy (apMonitorRes, m_apMonitorRes, sizeof(m_apMonitorRes));
      fLightBackground = m_fLightBackground;
      fLowPower = m_fLowPower;

      if (!m_fRunning) {
stoprunning:
         // if shutting down
         
         // tell thread above that shutting down
         if (dwThread+1 < HYPNOTHREADS) {
            SetEvent (m_ahSignalToThread[dwThread+1]);

            // wait for the thread to disappear
            LeaveCriticalSection (&m_CritSec);
            WaitForSingleObject (m_ahThread[dwThread+1], INFINITE);
            EnterCriticalSection (&m_CritSec);
         }

         m_afThreadActive[dwThread] = FALSE;
         SetEvent (m_ahSignalToThread[dwThread]);
         LeaveCriticalSection (&m_CritSec);
         break;
      }

      if (m_afThreadWantActive[dwThread] != m_afThreadActive[dwThread]) {
stopactive:
         // pass on to next level
         if ((dwThread+1 < HYPNOTHREADS) && (m_afThreadWantActive[dwThread+1] != m_afThreadActive[dwThread+1])) {
            // pass the message down
            m_afThreadWantActive[dwThread+1] = m_afThreadWantActive[dwThread];
            SetEvent (m_ahSignalToThread[dwThread+1]);

            LeaveCriticalSection (&m_CritSec);
            WaitForSingleObject (m_ahSignalFromThread[dwThread+1], INFINITE);

            continue;   // go back to beginning and see if need to inactivate or activate
         }

         // change state
         m_afThreadActive[dwThread] = m_afThreadWantActive[dwThread];
         SetEvent (m_ahSignalFromThread[dwThread]);   // so know changing state
      }

      // if we're inactive then wait
      if (!m_afThreadActive[dwThread]) {
         LeaveCriticalSection (&m_CritSec);
         goto wait;
      }

      // see if the current hypno effect will work
      for (i = 0; i < dwMonitors; i++) {
         BOOL fNeedNew = FALSE;
         if (!m_apHypnoEffect[i])
            fNeedNew = TRUE;
         else if (_wcsicmp ((PWSTR)memEffect.p, (PWSTR)amemEffectLast[i].p)) {
            // have changed the effect, see if different
            if (!m_apHypnoEffect[i]->EffectUnderstand ((PWSTR)memEffect.p))
               fNeedNew = TRUE;
            else {
               // it will understand, so just set it
               if (!dwThread)
                  m_apHypnoEffect[i]->EffectUse ((PWSTR)memEffect.p);

               // remember this, not matter which thread
               MemZero (&amemEffectLast[i]);
               MemCat (&amemEffectLast[i], (PWSTR)memEffect.p);
            }
         }
         if (!fNeedNew)
            continue;   // skip the rest of the processing

         // if we need a new hypno effect and this is NOT the primary thread then go to wait
         if (dwThread) {
            LeaveCriticalSection (&m_CritSec);
            goto wait;
         }

         // tell level above to deactivate itself
         while (TRUE) {
            if (!m_fRunning)  // need to test because may have left the critical section
               goto stoprunning;
            if (!m_afThreadWantActive[dwThread])
               goto stopactive;
            if ((dwThread+1 < HYPNOTHREADS) && m_afThreadActive[dwThread+1]) {
               m_afThreadWantActive[dwThread+1] = FALSE;
               SetEvent (m_ahSignalToThread[dwThread+1]);

               LeaveCriticalSection (&m_CritSec);
               WaitForSingleObject (m_ahSignalFromThread[dwThread+1], 100 /* NOT INFINITE */);

               // re-endter the critical section
               EnterCriticalSection (&m_CritSec);
               continue;
            }

            // if get here, the other thread has stopped
            break;
         } // while TRUE

         // if the current hypno effect won't understand then create a new one
         if (m_apHypnoEffect[i] && !m_apHypnoEffect[i]->EffectUnderstand ((PWSTR)memEffect.p)) {
            m_apHypnoEffect[i]->DeleteSelf ();
            m_apHypnoEffect[i] = NULL;
         }

         // if no hypnoeffect then create
         if (!m_apHypnoEffect[i]) {
            DWORD dwEffect;
            PCHypnoEffectSocket phes;
            // try all the effects
            for (dwEffect = 0; dwEffect < 2; dwEffect++) {
               switch (dwEffect) {
                  case 0:
                     phes = new CHypnoEffectCellular;
                     break;
                  case 1:
                  default:
                     phes = new CHypnoEffectNoise;
                     break;
               } // switch
               if (!phes)
                  continue;   // not likely to happen

               if (!phes->EffectUnderstand ((PWSTR)memEffect.p)) {
                  // doesn't understand
                  phes->DeleteSelf();
                  continue;
               }

               // else, match
               m_apHypnoEffect[i] = phes;
               break;
            } // dwEffect

            if (!m_apHypnoEffect[i]) {
               LeaveCriticalSection (&m_CritSec);
               goto wait;
            }
         } // if no hypnoeffect

         // tell the hypnoeffect to use the new setting
         m_apHypnoEffect[i]->EffectUse ((PWSTR)memEffect.p);

         // remember this effect
         MemZero (&amemEffectLast[i]);
         MemCat (&amemEffectLast[i], (PWSTR)memEffect.p);
      } // i

      // if get here, have all the hypno effects working

      // make sure that lower-threads are working
      if ((dwThread+1 < HYPNOTHREADS) && !m_afThreadActive[dwThread+1]) {
         m_afThreadWantActive[dwThread+1] = TRUE;
         SetEvent (m_ahSignalToThread[dwThread+1]);
      }

      // leave critical section
      LeaveCriticalSection (&m_CritSec);

      // get the time
      DWORD dwTime = GetTickCount();
      if ((dwTime < dwTimeLast) || (dwTime > dwTimeLast + 2000))
         dwTimeLast = dwTime; // since has been too long
      fp fTimeElapsed = (fp)(dwTime - dwTimeLast) / (fp)1000.0;

      for (i = 0; i < dwMonitors; i++) {
         // potentially wipe out the image
         if (!m_apHypnoEffect[i] || (apMonitorRes[i].x <= 0) || (apMonitorRes[i].y <= 0))
            continue;   // skip this

         if (!dwThread) {
            if (!apis[i])
               continue;

            // make sure the image is the right size
            if (((int)apis[i]->Width() != apMonitorRes[i].x) || ((int)apis[i]->Height() != apMonitorRes[i].y)) {
               apis[i]->Init (apMonitorRes[i].x, apMonitorRes[i].y);

               // fill with start
               memset (apis[i]->Pixel(0,0), fLightBackground ? 0 : -1, apis[i]->Width() * apis[i]->Height() * 3);
            }
         }

         // act
         BOOL fRet = m_apHypnoEffect[i]->TimeTick (dwThread, fTimeElapsed, fLightBackground, apis[i]);
         if (fRet && !dwThread) {
            // have a new image
            HWND hWnd = i ? gpMainWindow->m_hWndSecond : gpMainWindow->m_hWndPrimary;
            HDC hDC = GetDC (hWnd);
            HBITMAP hBmp = NULL;
            if (hDC) {
               hBmp = apis[i]->ToBitmap (hDC);
               ReleaseDC (hWnd, hDC);
            }

            if (hBmp) {
               EnterCriticalSection (&m_CritSecBmp);
               if (m_ahbmpBack[i])
                  DeleteObject (m_ahbmpBack[i]);
               m_ahbmpBack[i] = hBmp;
               m_apBackRes[i].x = (int) apis[i]->Width();
               m_apBackRes[i].y = (int) apis[i]->Height();
               LeaveCriticalSection (&m_CritSecBmp);

               // cause to redraw
               InvalidateRect (hWnd, NULL, FALSE);
            }
         }

      } // i


      // remember the last parameters
      dwMonitorsLast = dwMonitors;
      memcpy (apMonitorResLast, apMonitorRes, sizeof(apMonitorRes));
      fLightBackgroundLast = fLightBackground;
      fLowPowerLast = fLowPower;
      dwTimeLast = dwTime;

      // wait
wait:
      WaitForSingleObject (m_ahSignalToThread[dwThread],
         30 * (fLowPower ? 5 : 1) * ((giCPUSpeed < 2) ? 2 : 1) * (dwThread+1) );
   } // while TRUE

   // delete the object if it still exists
   for (i = 0; i < HYPNOMONITORS; i++) {
      if (!dwThread && m_apHypnoEffect[i]) {
         m_apHypnoEffect[i]->DeleteSelf();
         m_apHypnoEffect[i] = NULL;
      }
      if (apis[i])
         delete apis[i];
   }

   // done
   return;
}


/*************************************************************************************
CHypnoEffectNoise - Constructor and destructor
*/
CHypnoEffectNoise::CHypnoEffectNoise (void)
{
   InitializeCriticalSection (&m_CritSec);
   memset (&m_pRes, 0, sizeof(m_pRes));

   m_fNoiseAlpha = 0.0;
   m_fColorNoiseAlpha = 0.0;
   m_fNoiseAlphaDirection = TRUE;
   m_fColorNoiseAlphaDirection = TRUE;
   m_fInterpRGBWithOldTime = HEN_INTERPRGBWITHOLDTIME;
   m_fInterpColorMapWithOldTime = 0.0;
   m_fInterpScaleWithOldTime = 0.0;
   memset (m_acMapOld, 0, sizeof(m_acMapOld));
   memset (m_afScaleCur, 0, sizeof(m_afScaleCur));
   memset (m_afScaleNoiseCur, 0, sizeof(m_afScaleNoiseCur));
   memset (m_afColorScale, 0, sizeof(m_afColorScale));
   memset (m_afColorScaleNoise, 0, sizeof(m_afColorScaleNoise));

   DWORD i;
   for (i = 0; i < HEN_DEPTH; i++) {
      m_afColorOffset[i] = 0.0;
      m_apNoiseOffset[i].Zero();
   }

   m_aColorNoise[0].Init (HEN_COLORNOISESIZE, HEN_COLORNOISESIZE);
   m_aColorNoise[1].Init (HEN_COLORNOISESIZE, HEN_COLORNOISESIZE);
}
CHypnoEffectNoise::~CHypnoEffectNoise (void)
{
   DeleteCriticalSection (&m_CritSec);

}


/*************************************************************************************
CHypnoEffectNoise::EffectUnderstand - standard api
*/
BOOL CHypnoEffectNoise::EffectUnderstand (PWSTR pszEffect)
{
   // make sure has right prefix
   size_t dwLen = wcslen(gpszNoiseColon);
   if (wcslen(pszEffect) < dwLen)
      return FALSE;
   if (_wcsnicmp (pszEffect, gpszNoiseColon, dwLen))
      return FALSE;

   // anything with the prefix - assume to know
   return TRUE;
}


/*************************************************************************************
CHypnoEffectNoise::EffectUse - standard api
*/
BOOL CHypnoEffectNoise::EffectUse (PWSTR pszEffect)
{
   if (!EffectUnderstand (pszEffect))
      return FALSE;

   // skip ahead
   pszEffect += wcslen(gpszNoiseColon);

   PWSTR pszLike = L"like:", pszDislike = L"dislike:", pszTrust = L"trust:", pszMistrust = L"mistrust:",
      pszHappy = L"happy:", pszSad = L"sad:", pszAngry = L"angry:", pszAfraid = L"afraid:";
   size_t dwLikeLen = wcslen(pszLike), dwDislikeLen = wcslen(pszDislike),
      dwTrustLen = wcslen(pszTrust), dwMistrustLen = wcslen(pszMistrust),
      dwHappyLen = wcslen(pszHappy), dwSadLen = wcslen(pszSad),
      dwAngryLen = wcslen(pszAngry), dwAfraidLen = wcslen(pszAfraid);

   EnterCriticalSection (&m_CritSec);

   // default values for scaling and noise scaling
   DWORD i;
   m_dwEffect = NOISE_UNKNOWN;
   m_dwEffectSub = 0;
   for (i = 0; i < HEN_DEPTH; i++) {
      m_afScale[i] = pow (2.0, (fp)i) * 0.5;
      m_afScaleNoise[i] = pow (0.5, (fp)i) / 2.0;

      m_afColorScale[i] = pow (2.0, (fp)i);
      m_afColorScaleNoise[i] = pow (0.5, (fp)i) / 2.0;
   }
   m_fNoiseAlphaPerSec = 0.01;
   m_fColorNoiseAlphaPerSec = 0.01;
   m_fColorOffsetPerSec = 0.02;
   m_pNoiseOffsetPerSec.Zero();
   // m_pNoiseOffsetPerSec.p[0] = .025;
   // m_pNoiseOffsetPerSec.p[1] = .05;

   // remember for interp with
   BOOL fInterpWith = FALSE;
   fp afScale[HEN_DEPTH], afScaleNoise[HEN_DEPTH], afColorScale[HEN_DEPTH], afColorScaleNoise[HEN_DEPTH];
   fp fNoiseAlphaPerSec, fColorNoiseAlphaPerSec, fColorOffsetPerSec;
   CPoint pNoiseOffsetPerSec;
   memcpy (afScale, m_afScale, sizeof(m_afScale));
   memcpy (afScaleNoise, m_afScaleNoise, sizeof(m_afScaleNoise));
   memcpy (afColorScale, m_afColorScale, sizeof(m_afColorScale));
   memcpy (afColorScaleNoise, m_afColorScaleNoise, sizeof(m_afColorScaleNoise));
   fNoiseAlphaPerSec = m_fNoiseAlphaPerSec;
   fColorNoiseAlphaPerSec = m_fColorNoiseAlphaPerSec;
   fColorOffsetPerSec = m_fColorOffsetPerSec;
   pNoiseOffsetPerSec.Copy (&m_pNoiseOffsetPerSec);

   if (!_wcsicmp (pszEffect, L"neutral")) {
      m_dwEffect = NOISE_UNKNOWN;
   }
   else if (!_wcsnicmp (pszEffect, pszLike, dwLikeLen)) {  // like:x, x = 1..10
      // basically, pastel blue/green, slow down everything
      fInterpWith = TRUE;
      m_dwEffect = NOISE_LIKE;
      m_dwEffectSub = (DWORD)_wtoi(pszEffect + dwLikeLen);

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 0.5;
         m_afScaleNoise[i] /= (fp) (i+1);
      } // i

      m_fNoiseAlphaPerSec /= 2.0;
      m_fColorNoiseAlphaPerSec /= 2.0;
      m_fColorOffsetPerSec /= 2.0;
   }
   else if (!_wcsnicmp (pszEffect, pszDislike, dwDislikeLen)) {  // dislike:x, x = 1..10
      // basically, pastel blue/green, slow down everything
      fInterpWith = TRUE;
      m_dwEffect = NOISE_DISLIKE;
      m_dwEffectSub = (DWORD)_wtoi(pszEffect + dwDislikeLen);

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 2.0;
         m_afScaleNoise[i] *= (fp) (i+1);
         m_afColorScaleNoise[i] *= sqrt((fp)(i+1));   // more color noise too
      } // i

      m_fNoiseAlphaPerSec *= 2.0;
      m_fColorNoiseAlphaPerSec *= 2.0;
      m_fColorOffsetPerSec *= 2.0;
   }  
   else if (!_wcsnicmp (pszEffect, pszTrust, dwTrustLen)) {  // Trust:x, x = 1..10
      // basically, pastel blue/green, slow down everything
      fInterpWith = TRUE;
      m_dwEffect = NOISE_TRUST;
      m_dwEffectSub = (DWORD)_wtoi(pszEffect + dwTrustLen);

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 0.5;
         m_afScaleNoise[i] /= (fp) (i+1);
      } // i

      m_fNoiseAlphaPerSec /= 2.0;
      m_fColorNoiseAlphaPerSec /= 2.0;
      m_fColorOffsetPerSec /= 2.0;
   }
   else if (!_wcsnicmp (pszEffect, pszMistrust, dwMistrustLen)) {  // Mistrust:x, x = 1..10
      // basically, pastel blue/green, slow down everything
      fInterpWith = TRUE;
      m_dwEffect = NOISE_MISTRUST;
      m_dwEffectSub = (DWORD)_wtoi(pszEffect + dwMistrustLen);

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 2.0;
         m_afScaleNoise[i] *= (fp) (i+1);
         m_afColorScaleNoise[i] *= sqrt((fp)(i+1));   // more color noise too
      } // i

      m_fNoiseAlphaPerSec *= 2.0;
      m_fColorNoiseAlphaPerSec *= 2.0;
      m_fColorOffsetPerSec *= 2.0;
   }  
   else if (!_wcsnicmp (pszEffect, pszHappy, dwHappyLen)) {  // Happy:x, x = 1..10
      // basically, pastel blue/green, slow down everything
      fInterpWith = TRUE;
      m_dwEffect = NOISE_HAPPY;
      m_dwEffectSub = (DWORD)_wtoi(pszEffect + dwHappyLen);

      for (i = 0; i < HEN_DEPTH; i++) {
         // m_afScale[i] *= 2.0;
         // m_afScaleNoise[i] *= (fp) (i+1);
         // m_afColorScaleNoise[i] *= sqrt((fp)(i+1));   // more color noise too
         m_afColorScale[i] *= 2.0;
      } // i

      m_fNoiseAlphaPerSec *= 4.0;
      m_fColorNoiseAlphaPerSec *= 4.0;
      m_fColorOffsetPerSec *= 4.0;
      m_pNoiseOffsetPerSec.p[1] = -0.01;
   }
   else if (!_wcsnicmp (pszEffect, pszSad, dwSadLen)) {  // Sad:x, x = 1..10
      // basically, pastel blue/green, slow down everything
      fInterpWith = TRUE;
      m_dwEffect = NOISE_SAD;
      m_dwEffectSub = (DWORD)_wtoi(pszEffect + dwSadLen);

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 0.75;
         m_afScaleNoise[i] /= (fp) (i+1);
      } // i

      m_fNoiseAlphaPerSec /= 2.0;
      m_fColorNoiseAlphaPerSec /= 2.0;
      m_fColorOffsetPerSec /= 2.0;
      m_pNoiseOffsetPerSec.p[1] = 0.01;
   }
   else if (!_wcsnicmp (pszEffect, pszAngry, dwAngryLen)) {  // Angry:x, x = 1..10
      // basically, pastel blue/green, slow down everything
      fInterpWith = TRUE;
      m_dwEffect = NOISE_ANGRY;
      m_dwEffectSub = (DWORD)_wtoi(pszEffect + dwAngryLen);

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 2.0;
         m_afScaleNoise[i] *= (fp) (i+1);
         m_afColorScaleNoise[i] *= sqrt((fp)(i+1));   // more color noise too
      } // i

      m_fNoiseAlphaPerSec *= 4.0;
      m_fColorNoiseAlphaPerSec *= 2.0;
      m_fColorOffsetPerSec *= 4.0;
      m_pNoiseOffsetPerSec.p[1] = -0.01;
   }  
   else if (!_wcsnicmp (pszEffect, pszAfraid, dwAfraidLen)) {  // Afraid:x, x = 1..10
      // basically, pastel blue/green, slow down everything
      fInterpWith = TRUE;
      m_dwEffect = NOISE_AFRAID;
      m_dwEffectSub = (DWORD)_wtoi(pszEffect + dwAfraidLen);

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 0.75;
         m_afScaleNoise[i] *= (fp) (i*i+1);
         m_afColorScaleNoise[i] *= 2.0;   // more effects
      } // i

      m_fNoiseAlphaPerSec *= 4.0;
      m_fColorNoiseAlphaPerSec *= 16.0;
      m_fColorOffsetPerSec *= 16.0;
      m_pNoiseOffsetPerSec.p[1] = 0.01;
   }
   else if (!_wcsicmp (pszEffect, L"blueskyclouds")) {
      // basically, pastel blue/green, slow down everything
      m_dwEffect = NOISE_BLUESKYCLOUDS;

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 0.5;
         // m_afScaleNoise[i] *= (fp) (i+1);
         // m_afScaleNoise[i] /= (fp) (i+1);
         switch (i) {
            case 0:
               break;
            case 1:
            case 2:
               m_afScaleNoise[i] /= 2.0;
               break;
            default: // 3+
               m_afScaleNoise[i] *= 2.0;
               break;
         } // switch
      } // i

      // m_fNoiseAlphaPerSec /= 2.0;
      m_fColorNoiseAlphaPerSec /= 2.0;
      // m_fColorOffsetPerSec /= 2.0;
      m_pNoiseOffsetPerSec.p[0] = 0.0025;
   }
   else if (!_wcsicmp (pszEffect, L"stormclouds")) {
      // basically, pastel blue/green, slow down everything
      m_dwEffect = NOISE_STORMCLOUDS;

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 0.5;
         // m_afScaleNoise[i] *= (fp) (i+1);
         // m_afScaleNoise[i] /= (fp) (i+1);
         if (i)
            m_afScaleNoise[i] /= 2.0;
      } // i

      // m_fNoiseAlphaPerSec /= 2.0;
      m_fColorNoiseAlphaPerSec /= 2.0;
      // m_fColorOffsetPerSec /= 2.0;
      m_pNoiseOffsetPerSec.p[0] = 0.005;
   }
   else if (!_wcsicmp (pszEffect, L"greyday")) {
      // basically, pastel blue/green, slow down everything
      m_dwEffect = NOISE_GREYDAY;

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 0.5;
         // m_afScaleNoise[i] *= (fp) (i+1);
         // m_afScaleNoise[i] /= (fp) (i+1);
         if (i)
            m_afScaleNoise[i] /= 2.0;
      } // i

      // m_fNoiseAlphaPerSec /= 2.0;
      m_fColorNoiseAlphaPerSec /= 2.0;
      // m_fColorOffsetPerSec /= 2.0;
      m_pNoiseOffsetPerSec.p[0] = 0.0025;
   }
   else if (!_wcsicmp (pszEffect, L"waterlapping") || !_wcsicmp (pszEffect, L"waterflowing")) {
      // basically, pastel blue/green, slow down everything
      m_dwEffect = (!_wcsicmp (pszEffect, L"waterlapping")) ? NOISE_WATERLAPPING : NOISE_WATERFLOWING;

      for (i = 0; i < HEN_DEPTH; i++) {
         // m_afScale[i] *= 2.0;
         // m_afScaleNoise[i] *= (fp) (i+1);
         m_afScaleNoise[i] *= 2.0 / sqrt((fp)(i+1));
      } // i

      m_fNoiseAlphaPerSec *= 32.0 * ((m_dwEffect == NOISE_WATERFLOWING) ? 0.5 : 1.0);
      m_fColorNoiseAlphaPerSec *= 8.0;
      m_fColorOffsetPerSec *= 16.0;

      if (m_dwEffect == NOISE_WATERFLOWING)
         m_pNoiseOffsetPerSec.p[1] = -0.05;
   }
   else if (!_wcsicmp (pszEffect, L"smokerising")) {
      // basically, pastel blue/green, slow down everything
      m_dwEffect = NOISE_SMOKERISING;

      for (i = 0; i < HEN_DEPTH; i++) {
         // m_afScale[i] *= 2.0;
         // m_afScaleNoise[i] *= (fp) (i+1);
         // m_afScaleNoise[i] *= 2.0 / sqrt((fp)(i+1));
      } // i

      m_fNoiseAlphaPerSec *= 4.0;
      m_fColorNoiseAlphaPerSec *= 1.0;
      m_fColorOffsetPerSec *= 1.0;
      m_pNoiseOffsetPerSec.p[1] = 0.025;
   }
   else if (!_wcsicmp (pszEffect, L"fog")) {
      // basically, pastel blue/green, slow down everything
      m_dwEffect = NOISE_FOG;

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 0.5;
         // m_afScaleNoise[i] *= (fp) (i+1);
         // m_afScaleNoise[i] *= 2.0 / sqrt((fp)(i+1));
      } // i

      m_fNoiseAlphaPerSec *= 4.0;
      m_fColorNoiseAlphaPerSec *= 1.0;
      m_fColorOffsetPerSec *= 1.0;
      // m_pNoiseOffsetPerSec.p[1] = 0.025;
   }
   else if (!_wcsicmp (pszEffect, L"sick")) {
      m_dwEffect = NOISE_SICK;

      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] *= 0.75;
         m_afScaleNoise[i] *= (fp) (i*i+1);
         m_afColorScaleNoise[i] *= 2.0;   // more effects
      } // i

      m_fNoiseAlphaPerSec *= 4.0;
      m_fColorNoiseAlphaPerSec *= 16.0;
      m_fColorOffsetPerSec *= 16.0;
      m_pNoiseOffsetPerSec.p[0] = 0.01;
      m_pNoiseOffsetPerSec.p[1] = 0.01;
   }
   else if (!_wcsicmp (pszEffect, L"forest")) {
      // basically, pastel blue/green, slow down everything
      m_dwEffect = NOISE_FOREST;

      for (i = 0; i < HEN_DEPTH; i++) {
         // m_afScale[i] *= 2.0;
         // m_afScaleNoise[i] *= (fp) (i+1);
         m_afScaleNoise[i] *= 2.0 / sqrt((fp)(i+1));
      } // i

      m_fNoiseAlphaPerSec *= 8.0;
      m_fColorNoiseAlphaPerSec *= 1.0;
      m_fColorOffsetPerSec *= 2.0;
   }
   else if (!_wcsicmp (pszEffect, L"flames")) {
      // basically, pastel blue/green, slow down everything
      m_dwEffect = NOISE_FLAMES;

      for (i = 0; i < HEN_DEPTH; i++) {
         // m_afScale[i] *= 2.0;
         // m_afScaleNoise[i] *= (fp) (i+1);
         m_afScaleNoise[i] *= 2.0;
      } // i

      m_fNoiseAlphaPerSec *= 32.0;
      m_fColorNoiseAlphaPerSec *= 4.0;
      m_fColorOffsetPerSec *= 16.0;
      m_pNoiseOffsetPerSec.p[1] = 0.1;
   }
   else if (!_wcsicmp (pszEffect, L"dark")) {
      m_dwEffect = NOISE_DARK;

      for (i = 0; i < HEN_DEPTH; i++) {
         // m_afScale[i] *= 2.0;
         // m_afScaleNoise[i] *= (fp) (i+1);
         // m_afScaleNoise[i] *= 2.0;
      } // i

      m_fNoiseAlphaPerSec *= 16.0;
      // m_fColorNoiseAlphaPerSec *= 4.0;
      // m_fColorOffsetPerSec *= 16.0;
      // m_pNoiseOffsetPerSec.p[1] = 0.05;
   }

   // interpolate
   if (fInterpWith) {   // m_dwEffectSub is 0..10, affecting how much interpolate
      fp fAlpha = (fp)m_dwEffectSub / (fp)10.0;
      // fAlpha = sqrt(fAlpha);  // so quickly goes to full
      fp fAlphaInv = 1.0 - fAlpha;
      for (i = 0; i < HEN_DEPTH; i++) {
         m_afScale[i] = m_afScale[i] * fAlpha + afScale[i] * fAlphaInv;
         m_afScaleNoise[i] = m_afScaleNoise[i] * fAlpha + afScaleNoise[i] * fAlphaInv;

         m_afColorScale[i] = m_afColorScale[i] * fAlpha + afColorScale[i] * fAlphaInv;
         m_afColorScaleNoise[i] = m_afColorScaleNoise[i] * fAlpha + afColorScaleNoise[i] * fAlphaInv;
      }
      m_fNoiseAlphaPerSec = m_fNoiseAlphaPerSec * fAlpha + fNoiseAlphaPerSec * fAlphaInv;
      m_fColorNoiseAlphaPerSec = m_fColorNoiseAlphaPerSec * fAlpha + fColorNoiseAlphaPerSec * fAlphaInv;
      m_fColorOffsetPerSec = m_fColorOffsetPerSec * fAlpha + fColorOffsetPerSec * fAlphaInv;
      m_pNoiseOffsetPerSec.Scale(fAlpha);
      pNoiseOffsetPerSec.Scale(fAlphaInv);
      m_pNoiseOffsetPerSec.Add (&pNoiseOffsetPerSec);
   } // if fInterpWith


   // if this is the NOT the first time this was called, then must
   // already have a color map, so blend with that
   if (m_fInterpRGBWithOldTime != HEN_INTERPRGBWITHOLDTIME) {
      m_fInterpColorMapWithOldTime = HEN_INTERPCOLORMAPWITHOLDTIME;
      m_fInterpScaleWithOldTime = HEN_INTERPSCALEWITHOLDTIME;
   }

   LeaveCriticalSection (&m_CritSec);


   return TRUE;
}


/*************************************************************************************
CHypnoEffectNoise::TimeTick - standard api
*/

BOOL CHypnoEffectNoise::TimeTick (DWORD dwThread, fp fTimeElapsed, BOOL fLightBackground, PCImageStore pis)
{
   DWORD i, j;
   if (dwThread) {
      // make sure have right size
      EnterCriticalSection (&m_CritSec);
      POINT pRes = m_pRes;

      if (m_fInterpScaleWithOldTime > CLOSE) {
         fp fAlpha = m_fInterpScaleWithOldTime / HEN_INTERPSCALEWITHOLDTIME;
         m_fInterpScaleWithOldTime -= fTimeElapsed;
         for (i = 0; i < HEN_DEPTH; i++) {
            m_afScaleCur[i] = fAlpha * m_afScaleCur[i] + (1.0 - fAlpha) * m_afScale[i];
            m_afScaleNoiseCur[i] = fAlpha * m_afScaleNoiseCur[i] + (1.0 - fAlpha) * m_afScaleNoise[i];
         } // i
      }
      else {
         _ASSERTE (sizeof(m_afScaleCur) == sizeof(m_afScale));
         _ASSERTE (sizeof(m_afScaleNoiseCur) == sizeof(m_afScaleNoise));
         memcpy (m_afScaleCur, m_afScale, sizeof(m_afScale));
         memcpy (m_afScaleNoiseCur, m_afScaleNoise, sizeof(m_afScaleNoise));
      }

      LeaveCriticalSection (&m_CritSec);

      // make sure have noise
      for (i = 0; i < 2; i++)
         if (!m_amemNoise[i].m_dwCurPosn)
            NoiseFill (i);

      if ((pRes.x <= 0) || (pRes.y <= 0))
         return FALSE;  // shouldnt happen

      // ping-pong between noises
      fp fAlphaLeft = fTimeElapsed * m_fNoiseAlphaPerSec;
      while (fAlphaLeft > CLOSE) {
         fp fMax;
         if (m_fNoiseAlphaDirection) // incrasing alpha
            fMax = (1.0 - m_fNoiseAlpha);
         else
            fMax = m_fNoiseAlpha;

         fMax = max(fMax, 0.0);
         fp fAlphaShift = min(fMax, fAlphaLeft);

         if (m_fNoiseAlphaDirection) { // incrasing alpha
            m_fNoiseAlpha += fAlphaShift;

            if (m_fNoiseAlpha >= 1.0 - CLOSE) {
               m_fNoiseAlphaDirection = !m_fNoiseAlphaDirection;
               NoiseFill (1);
            }
         }
         else {
            m_fNoiseAlpha -= fAlphaShift;

            if (m_fNoiseAlpha <= CLOSE) {
               m_fNoiseAlphaDirection = !m_fNoiseAlphaDirection;
               NoiseFill (0);
            }
         }

         fAlphaLeft -= fAlphaShift;
      } // while time left

      fp fAlpha = m_fNoiseAlpha;
      fp fAlphaInv = 1.0 - fAlpha;

      // make sure enough work memoty
      m_memHeightWork.m_dwCurPosn = pRes.x * pRes.y;
      if (!m_memHeightWork.Required (m_memHeightWork.m_dwCurPosn))
         return FALSE;

      // offset the coloring
      DWORD dwDim;
      for (i = 0; i < HEN_DEPTH; i++) {
         for (dwDim = 0; dwDim < 2; dwDim++) {
            m_apNoiseOffset[i].p[dwDim] += m_pNoiseOffsetPerSec.p[dwDim] * fTimeElapsed;
            m_apNoiseOffset[i].p[dwDim] = myfmod(m_apNoiseOffset[i].p[dwDim] * m_afScaleCur[i], 1.0) / m_afScaleCur[i];
         } // dwDim;
      }

      // fill in the height field
      int iX, iY;
      fp fX, fY, fXMod, fYMod;
      PBYTE pabHeight = (PBYTE)m_memHeightWork.p;
      fp fSum = 0;
      float *apafNoise[2];
      DWORD dwOffset;
      for (i = 0; i < 2; i++)
         apafNoise[i] = (float*)m_amemNoise[i].p;
      fp fResYInv = 1.0 / (fp)pRes.y;
      fp fResX2YInv = -(fp)pRes.x / 2.0 * fResYInv;   // when zoom in/out will be centered

      for (iY = 0; iY < pRes.y; iY++) {
         fY = (fp)iY * fResYInv - 0.5;   // when zoom in/out will be centered

         for (iX = 0; iX < pRes.x; iX++, pabHeight++) {
            fSum = 0.0;
            // fX = (fp)(iX - pRes.x/2) * fResYInv;  // intentionally dividing by y, so have square coords
            fX = (fp)iX * fResYInv + fResX2YInv;

            for (i = 0; i < HEN_DEPTH; i++) {
               fXMod = (fX + m_apNoiseOffset[i].p[0]) * m_afScaleCur[i];
               fYMod = (fY + m_apNoiseOffset[i].p[1]) * m_afScaleCur[i];
               
               fXMod = myfmod (fXMod, 1.0) * HEN_NOISEPIXELS;
               fYMod = myfmod (fYMod, 1.0) * HEN_NOISEPIXELS;

               dwOffset = (DWORD)fYMod * HEN_NOISEPIXELS + (DWORD)fXMod;

               fSum += m_afScaleNoiseCur[i] * (
                  apafNoise[0][dwOffset] * fAlpha + apafNoise[1][dwOffset] * fAlphaInv);
            } // i

            fSum = (fSum + 1.0) / 2.0 * 256.0;
            fSum = max(fSum, 0.0);
            fSum = min(fSum, 255.0);
            *pabHeight = (BYTE)fSum;
         } // iX
      } // iY

      // copy over the height field for the other thread
      EnterCriticalSection (&m_CritSec);
      if (m_memHeightField.Required (m_memHeightWork.m_dwCurPosn)) {
         m_memHeightField.m_dwCurPosn = m_memHeightWork.m_dwCurPosn;
         memcpy (m_memHeightField.p, m_memHeightWork.p, m_memHeightWork.m_dwCurPosn);
      }
      LeaveCriticalSection (&m_CritSec);
   }
   else { // dwThread = 0
      if (!pis)
         return FALSE;  // shouldnt happen


      // ping-pong between noises
      fp fAlphaLeft = fTimeElapsed * m_fColorNoiseAlphaPerSec;
      while (fAlphaLeft > CLOSE) {
         fp fMax;
         if (m_fColorNoiseAlphaDirection) // incrasing alpha
            fMax = (1.0 - m_fColorNoiseAlpha);
         else
            fMax = m_fColorNoiseAlpha;

         fMax = max(fMax, 0.0);
         fp fAlphaShift = min(fMax, fAlphaLeft);

         if (m_fColorNoiseAlphaDirection) { // incrasing alpha
            m_fColorNoiseAlpha += fAlphaShift;

            if (m_fColorNoiseAlpha >= 1.0 - CLOSE) {
               m_fColorNoiseAlphaDirection = !m_fColorNoiseAlphaDirection;
               m_aColorNoise[1].Init (HEN_COLORNOISESIZE, HEN_COLORNOISESIZE);
            }
         }
         else {
            m_fColorNoiseAlpha -= fAlphaShift;

            if (m_fColorNoiseAlpha <= CLOSE) {
               m_fColorNoiseAlphaDirection = !m_fColorNoiseAlphaDirection;
               m_aColorNoise[0].Init (HEN_COLORNOISESIZE, HEN_COLORNOISESIZE);
            }
         }

         fAlphaLeft -= fAlphaShift;
      } // while time left

      fp fAlpha = m_fColorNoiseAlpha;
      fp fAlphaInv = 1.0 - fAlpha;

      // offset the coloring
      for (i = 0; i < HEN_DEPTH; i++) {
         m_afColorOffset[i] += m_fColorOffsetPerSec * fTimeElapsed;
         m_afColorOffset[i] = myfmod(m_afColorOffset[i] / m_afColorScale[i], 1.0) * m_afColorScale[i];
      }

      // figure out the mapping
      COLORREF    acMap[256];
      DWORD k;
      fp fX, fY, fXMod, fYMod;
      fp afColor[3], fSum;
      for (i = 0; i < 256; i++) {
         fY = (fp)i / (fp)256.0;
         acMap[i] = 0;

         for (j = 0; j < 3; j++) {
            fX = (fp)j / (fp)3.0;   // colors
            fXMod = fX; // no change

            fSum = 0.0;
            for (k = 0; k < HEN_DEPTH; k++) {
               fYMod = (fY + m_afColorOffset[k]) * m_afColorScale[k];

               fSum += m_afColorScaleNoise[k] * (
                  m_aColorNoise[0].Value (fXMod,fYMod) * fAlpha + m_aColorNoise[1].Value (fXMod,fYMod) * fAlphaInv);
            } // k

            // convert to color
            fSum = (fSum + 1.0) / 2.0 * 256.0;
            afColor[j] = fSum;
         } // j

         ColorMapAdjust (i, m_dwEffect, m_dwEffectSub, fLightBackground, afColor);

         // limit check and write as bytes
         for (j = 0; j < 3; j++) {
            fSum = afColor[j];
            fSum = max(fSum, 0.0);
            fSum = min(fSum, 255.0);
            ((PBYTE)(&acMap[i]))[j] = (BYTE)fSum;
         } // j
      } // i

      // blend in the old map
      DWORD dwInterpWithOld = 0;
      DWORD dwInterpWithOldInv = 0;
      if (m_fInterpColorMapWithOldTime > CLOSE) {
         dwInterpWithOld = (DWORD)(m_fInterpColorMapWithOldTime / (fp)HEN_INTERPCOLORMAPWITHOLDTIME * 256.0);
         dwInterpWithOld = min (dwInterpWithOld, 256);
         dwInterpWithOldInv = 256 - dwInterpWithOld;
         m_fInterpColorMapWithOldTime -= fTimeElapsed;

         for (i = 0; i < 256; i++)
            acMap[i] = RGB (
               (BYTE) (((DWORD)GetRValue(m_acMapOld[i]) * dwInterpWithOld + (DWORD)GetRValue(acMap[i]) * dwInterpWithOldInv) / 256),
               (BYTE) (((DWORD)GetGValue(m_acMapOld[i]) * dwInterpWithOld + (DWORD)GetGValue(acMap[i]) * dwInterpWithOldInv) / 256),
               (BYTE) (((DWORD)GetBValue(m_acMapOld[i]) * dwInterpWithOld + (DWORD)GetBValue(acMap[i]) * dwInterpWithOldInv) / 256)
               );
      } // if interp with old map

      // remember this
      _ASSERTE (sizeof(m_acMapOld) == sizeof(acMap));
      memcpy (m_acMapOld, acMap, sizeof(acMap));

      EnterCriticalSection (&m_CritSec);

      // store the size, so second thread can use
      m_pRes.x = (int)pis->Width();
      m_pRes.y = (int)pis->Height();
      DWORD dwNum = pis->Width() * pis->Height();
      if (m_memHeightField.m_dwCurPosn != dwNum) {
         LeaveCriticalSection (&m_CritSec);
         return FALSE;
      }

      // fill in pis with map
      PBYTE pabColor = (PBYTE)pis->Pixel(0,0);
      PBYTE pabHeight = (PBYTE)m_memHeightField.p;
      COLORREF cr;
      dwInterpWithOld = dwInterpWithOldInv = 0;
      if (m_fInterpRGBWithOldTime > CLOSE) {
         dwInterpWithOld = (DWORD)(m_fInterpRGBWithOldTime / (fp)HEN_INTERPRGBWITHOLDTIME * 256.0);
         dwInterpWithOld = min (dwInterpWithOld, 256);
         dwInterpWithOldInv = 256 - dwInterpWithOld;
         m_fInterpRGBWithOldTime -= fTimeElapsed;
      }
      else
         dwInterpWithOld = 0;

      if (dwInterpWithOld)
         for (i = 0; i < dwNum; i++, pabHeight++, pabColor += 3) {
            cr = acMap[*pabHeight];
            pabColor[0] = (BYTE) (((DWORD)pabColor[0] * dwInterpWithOld + (DWORD)GetRValue(cr) * dwInterpWithOldInv) / 256);
            pabColor[1] = (BYTE) (((DWORD)pabColor[1] * dwInterpWithOld + (DWORD)GetGValue(cr) * dwInterpWithOldInv) / 256);
            pabColor[2] = (BYTE) (((DWORD)pabColor[2] * dwInterpWithOld + (DWORD)GetBValue(cr) * dwInterpWithOldInv) / 256);
         } // i
      else
         for (i = 0; i < dwNum; i++, pabHeight++, pabColor += 3) {
            cr = acMap[*pabHeight];
            pabColor[0] = GetRValue(cr);
            pabColor[1] = GetGValue(cr);
            pabColor[2] = GetBValue(cr);
         } // i
      LeaveCriticalSection (&m_CritSec);
   }

   return TRUE;
}

/*************************************************************************************
CHypnoEffectNoise::ColorMapAdjust - Adjust the color coming from the randomized color map.

inputs
   DWORD          dwIndex - Color index, from 0 .. 255
   fp             fColor - 0 to 255 (sometimes less or more than this)
   DWORD          dwEffect - Effect number
   DWORD          dwEffectSub - Sub-effect
   BOOL           fLightBackground - TRUE if it's a light background
   fp             *pafRGB - 3 RGB values for [0]=red, [1]=green, [2]=blue, from 0..255. Modify in place.
returns
   none
*/
void CHypnoEffectNoise::ColorMapAdjust (DWORD dwIndex, DWORD dwEffect, DWORD dwEffectSub, BOOL fLightBackground, fp *pafRGB)
{
   // remember for interp
   BOOL fInterp = FALSE;
   fp afRGBOrig[3];
   memcpy (afRGBOrig, pafRGB, sizeof(afRGBOrig));

   fp fSum, fSumInv, fScale;

   switch (dwEffect) {
   case NOISE_LIKE:
      // light blue/green
      pafRGB[2] = 256 - pafRGB[1];  // blue/green inverse
      fInterp = TRUE;
      fSum = (pafRGB[1] + pafRGB[2]) / 2.0;
      pafRGB[1] = (pafRGB[1] - fSum) * 4.0; // make greener
      pafRGB[2] = (pafRGB[2] - fSum) * 4.0; // make blue-er

      // pastel colored
      fSum = fSum * .75 + 64;
      pafRGB[0] = 64;
      pafRGB[1] = max(0, pafRGB[1]) + fSum;
      pafRGB[2] = max(0, pafRGB[2]) + fSum;
      break;

   case NOISE_DISLIKE:
      // light blue/green
      fInterp = TRUE;
      pafRGB[0] *= 2.0 / 3.0; // dark red
      pafRGB[1] = pafRGB[0] / 6.0;  // no green
      pafRGB[2] = (pafRGB[2] + pafRGB[0]) / 4.0;  // maybe a hint of blue
      break;

   case NOISE_TRUST:
      // deep blue, maybe some red
      fInterp = TRUE;
      pafRGB[0] = (pafRGB[0] + pafRGB[2]) / 4.0;
      pafRGB[1] = 0.0;
      // normal blue pafRGB[2] *= 3.0 / 2.0; // bluer
      break;

   case NOISE_MISTRUST:
      // greeny/yellow
      fInterp = TRUE;
      pafRGB[1] *= 2.0 / 3.0; // dark green
      pafRGB[0] = pafRGB[0] / 4.0 + pafRGB[1] / 2.0;  // maybe a hint of red (to make yellowish)
      pafRGB[2] = 0.0; // no blue
      break;

   case NOISE_HAPPY:
      // very colorful
      fInterp = TRUE;
      fSum = (pafRGB[0] + pafRGB[1] + pafRGB[2]) / 3.0;
      pafRGB[0] = (pafRGB[0] - fSum) * 4.0; // make redder
      pafRGB[1] = (pafRGB[1] - fSum) * 4.0; // make greener
      pafRGB[2] = (pafRGB[2] - fSum) * 4.0; // make blue-er

      // pastel colored
      fSum = fSum * .75 + 64;
      pafRGB[0] = max (0.0, pafRGB[0]) + fSum;
      pafRGB[1] = max (0.0, pafRGB[1]) + fSum;
      pafRGB[2] = max (0.0, pafRGB[2]) + fSum;
      break;

   case NOISE_SAD:
      // grey
      fInterp = TRUE;
      fSum = pafRGB[0]; // just choose one color so keep up contrast
      pafRGB[0] = pafRGB[1] = pafRGB[2] = fSum;
      break;

   case NOISE_ANGRY:
      // red
      fInterp = TRUE;
      pafRGB[0] *= 2.0 / 3.0; // dark red
      pafRGB[1] = 0.0;
      pafRGB[2] = 0.0;
      break;

   case NOISE_AFRAID:
      // grey
      fInterp = TRUE;

      // emergency stripe of yellow
      if (pafRGB[0] >= 200)
         fScale = (pafRGB[0] - 200) / 56.0;
      else
         fScale = 0.0;

      fSum = pafRGB[1]; // green

      pafRGB[0] = pafRGB[0] * fScale * 4.0 + fSum * (1.0 - fScale);
      pafRGB[1] = pafRGB[1] * fScale * 4.0 + fSum * (1.0 - fScale);
      pafRGB[2] = fSum; // no blues, just grey
      break;

   case NOISE_BLUESKYCLOUDS:
      fSum = dwIndex; // pafRGB[2];
      // fSum = max(fSum, 0.0);
      // fSum = min(fSum, 256.0);
      fSum = sin((fSum - 128.0) / 128.0 * PI/2.0);
      fSum = pow (fabs(fSum), 0.25) * ((fSum < 0.0) ? -1.0 : 1.0);
      fSum = fSum / 2.0 + 0.5;
      fSumInv = 1.0 - fSum;

      pafRGB[0] = fSum * 256 + fSumInv * 96.0;
      pafRGB[1] = fSum * 256 + fSumInv * 120.0;
      pafRGB[2] = fSum * 256 + fSumInv * 255.0;

      break;

   case NOISE_STORMCLOUDS:
      fSum = (fp)dwIndex * 1.1; // pafRGB[2];
      // fSum = max(fSum, 0.0);
      fSum = min(fSum, 256.0);
      fSum = sin((fSum - 128.0) / 128.0 * PI/2.0);
      fSum = pow (fabs(fSum), 0.5) * ((fSum < 0.0) ? -1.0 : 1.0);
      fSum = fSum / 2.0 + 0.5;
      fSumInv = 1.0 - fSum;

      pafRGB[0] = fSum * 40 + fSumInv * 110.0;
      pafRGB[1] = fSum * 50 + fSumInv * 120.0;
      pafRGB[2] = fSum * 40 + fSumInv * 130.0;

      break;

   case NOISE_GREYDAY:
      fSum = (fp)dwIndex; // pafRGB[2];
      // fSum = max(fSum, 0.0);
      // fSum = min(fSum, 256.0);
      fSum = sin((fSum - 128.0) / 128.0 * PI/2.0);
      fSum = pow (fabs(fSum), 0.5) * ((fSum < 0.0) ? -1.0 : 1.0);
      fSum = fSum / 2.0 + 0.5;
      fSumInv = 1.0 - fSum;

      pafRGB[0] = fSum * 70 + fSumInv * 110.0;
      pafRGB[1] = fSum * 80 + fSumInv * 120.0;
      pafRGB[2] = fSum * 70 + fSumInv * 130.0;

      break;

   case NOISE_WATERLAPPING:
   case NOISE_WATERFLOWING:
      fSum = (fp)dwIndex / 256.0;
      fSumInv = 1.0 - fSum;

      pafRGB[0] = fSum * 110 + fSumInv * 64 + pafRGB[0] / 32.0;
      pafRGB[1] = fSum * 128 + fSumInv * 80 + pafRGB[1] / 16.0;
      pafRGB[2] = fSum * 256 + fSumInv * 128 + pafRGB[2] / 16.0;
      break;

   case NOISE_SMOKERISING:
      // default NPC effect is "opal"-ish greyish
      fSum = (pafRGB[0] + pafRGB[1] + pafRGB[2]) / 3.0;
      pafRGB[0] = (pafRGB[0] - fSum) / 8.0; // reduce the color
      pafRGB[1] = (pafRGB[1] - fSum) / 8.0; // reduce the color
      pafRGB[2] = (pafRGB[2] - fSum) / 8.0; // reduce the color

      // not too dark/light
      fSum = fSum/2.0 + 128;  // neutral
      pafRGB[0] += fSum;
      pafRGB[1] += fSum;
      pafRGB[2] += fSum;
      break;

   case NOISE_FOG:
      fSum = (pafRGB[0] + pafRGB[1] + pafRGB[2]) / 3.0;
      fSumInv = pafRGB[0]; // so have something which isn't as averaged
      pafRGB[0] = (pafRGB[0] - fSum) / 8.0; // reduce the color
      pafRGB[1] = (pafRGB[1] - fSum) / 8.0; // reduce the color
      pafRGB[2] = (pafRGB[2] - fSum) / 8.0; // reduce the color

      // not too dark/light
      fSum = fSumInv / 2.0 + 196;  // neutral
      pafRGB[0] += fSum;
      pafRGB[1] += fSum;
      pafRGB[2] += fSum;
      break;

   case NOISE_SICK:
      pafRGB[0] = min(pafRGB[0], pafRGB[1]); // yellow or green
      // pafRGB[1] *= 1.5;
      pafRGB[2] = 0.0;
      break;

   case NOISE_FOREST:
      fSum = (fp)dwIndex * 1.1;
      fSum = min(fSum, 256.0);
      fSum = sin((fSum - 128.0) / 128.0 * PI/2.0);
      //fSum = pow (fabs(fSum), 0.5) * ((fSum < 0.0) ? -1.0 : 1.0);
      fSum = fSum / 2.0 + 0.5;
      fSumInv = 1.0 - fSum;

      pafRGB[0] = fSum * 0 + fSumInv * 96 + pafRGB[0] / 32.0;
      pafRGB[1] = fSum * 140 + fSumInv * 120 + pafRGB[1] / 32.0;
      pafRGB[2] = fSum * 30 + fSumInv * 255 + pafRGB[2] / 32.0;
      break;

   case NOISE_FLAMES:
      pafRGB[0] = pafRGB[0] * 1.5 + 64;
      pafRGB[1] = pow(pafRGB[1] / 256.0, 2.0) * pafRGB[0];
      pafRGB[2] = 0;
      break;

   case NOISE_DARK:
      pafRGB[0] /= 8.0;
      pafRGB[1] = 0;
      pafRGB[2] /= 4.0;
      break;



   case NOISE_UNKNOWN:
   default:
      // default NPC effect is "opal"-ish greyish
      fSum = (pafRGB[0] + pafRGB[1] + pafRGB[2]) / 3.0;
      pafRGB[0] = (pafRGB[0] - fSum) / 2.0; // reduce the color
      pafRGB[1] = (pafRGB[1] - fSum) / 2.0; // reduce the color
      pafRGB[2] = (pafRGB[2] - fSum) / 2.0; // reduce the color

      // not too dark/light
      fSum = fSum/2.0 + 128;  // neutral
      pafRGB[0] += fSum;
      pafRGB[1] += fSum;
      pafRGB[2] += fSum;
      break;
   } // switch
   
   if (fInterp) {
      // get the default
      ColorMapAdjust (dwIndex, NOISE_UNKNOWN, 0, TRUE, afRGBOrig);

      // interp
      fp fAlpha = (fp)dwEffectSub / 10.0;
      fAlpha = sqrt(fAlpha);  // so quickly goes to full
      fp fAlphaInv = 1.0 - fAlpha;
      pafRGB[0] = pafRGB[0] * fAlpha + afRGBOrig[0] * fAlphaInv;
      pafRGB[1] = pafRGB[1] * fAlpha + afRGBOrig[1] * fAlphaInv;
      pafRGB[2] = pafRGB[2] * fAlpha + afRGBOrig[2] * fAlphaInv;
   } // if fInterp

   if (!fLightBackground) {
      pafRGB[0] *= 0.666;
      pafRGB[1] *= 0.666;
      pafRGB[2] *= 0.666;
   }
}



/*************************************************************************************
CHypnoEffectNoise::DeleteSelf - standard api
*/
void CHypnoEffectNoise::DeleteSelf (void)
{
   delete this;
}


/*************************************************************************************
CHypnoEffectNoise::NoiseFill - Fill in a noise buffer.

inputs
   DWORD       dwPingPong - Ping pong number, 0 or 1
*/
void CHypnoEffectNoise::NoiseFill (DWORD dwPingPong)
{
   DWORD dwNeed = HEN_NOISEPIXELS * HEN_NOISEPIXELS * sizeof(float);
   if (!m_amemNoise[dwPingPong].Required (dwNeed))
      return;

   CNoise2D Noise;
   Noise.Init (HEN_NOISESIZE, HEN_NOISESIZE);

   m_amemNoise[dwPingPong].m_dwCurPosn = dwNeed;
   float *pafNoise = (float*)m_amemNoise[dwPingPong].p;

   DWORD dwX, dwY;
   fp fX, fY;
   for (dwY = 0; dwY < HEN_NOISEPIXELS; dwY++) {
      fY = (fp)dwY / (fp)HEN_NOISEPIXELS;
      for (dwX = 0; dwX < HEN_NOISEPIXELS; dwX++, pafNoise++) {
         fX = (fp)dwX / (fp)HEN_NOISEPIXELS;
         *pafNoise = Noise.Value (fX, fY);
      } // dwY
   } // dwY
}



/*************************************************************************************
CHypnoEffectCellular - Constructor and destructor
*/
CHypnoEffectCellular::CHypnoEffectCellular (void)
{
   InitializeCriticalSection (&m_CritSec);
   memset (&m_pRes, 0, sizeof(m_pRes));

   m_dwEffect = 0;
   m_fFrameAlphaPerSec = 0.0;
   m_fFrameAlpha = 0.0;
   m_fFrameAlphaDirection = TRUE;
}
CHypnoEffectCellular::~CHypnoEffectCellular (void)
{
   DeleteCriticalSection (&m_CritSec);

}


/*************************************************************************************
CHypnoEffectCellular::EffectUnderstand - standard api
*/
BOOL CHypnoEffectCellular::EffectUnderstand (PWSTR pszEffect)
{
   // make sure has right prefix
   size_t dwLen = wcslen(gpszCellularColon);
   if (wcslen(pszEffect) < dwLen)
      return FALSE;
   if (_wcsnicmp (pszEffect, gpszCellularColon, dwLen))
      return FALSE;

   // anything with the prefix - assume to know
   return TRUE;
}


/*************************************************************************************
CHypnoEffectCellular::EffectUse - standard api
*/
BOOL CHypnoEffectCellular::EffectUse (PWSTR pszEffect)
{
   if (!EffectUnderstand (pszEffect))
      return FALSE;

   // skip ahead
   pszEffect += wcslen(gpszCellularColon);

   EnterCriticalSection (&m_CritSec);

   // default
   m_dwEffect = CELLULAR_STARGATE;
   m_fFrameAlphaPerSec = 0.02;

   if (!_wcsicmp (pszEffect, L"stargate")) {
      m_dwEffect = CELLULAR_STARGATE;
      m_fFrameAlphaPerSec = 0.1;
   }
   else if (!_wcsicmp (pszEffect, L"swirl")) {
      m_dwEffect = CELLULAR_SWIRL;
      m_fFrameAlphaPerSec = 0.1;
   }


   LeaveCriticalSection (&m_CritSec);

   return TRUE;
}


/*************************************************************************************
CHypnoEffectCellular::TimeTick - standard api
*/

BOOL CHypnoEffectCellular::TimeTick (DWORD dwThread, fp fTimeElapsed, BOOL fLightBackground, PCImageStore pis)
{
   DWORD i;
   if (dwThread) {
      // make sure have right size
      EnterCriticalSection (&m_CritSec);
      POINT pRes = m_pRes;
      DWORD dwEffect = m_dwEffect;
      LeaveCriticalSection (&m_CritSec);

      if ((pRes.x <= 0) || (pRes.y <= 0))
         return FALSE;

      // make sure have noise
      for (i = 0; i < 2; i++)
         if (m_amemHYPNOCELL[i].m_dwCurPosn != (pRes.x * pRes.y * sizeof(HYPNOCELL)) )
            CellularFill (i, pRes, dwEffect, fLightBackground);

      // ping-pong between noises
      fp fAlphaLeft = fTimeElapsed * m_fFrameAlphaPerSec;
      while (fAlphaLeft > CLOSE) {
         fp fMax;
         if (m_fFrameAlphaDirection) // incrasing alpha
            fMax = (1.0 - m_fFrameAlpha);
         else
            fMax = m_fFrameAlpha;

         fMax = max(fMax, 0.0);
         fp fAlphaShift = min(fMax, fAlphaLeft);

         if (m_fFrameAlphaDirection) { // incrasing alpha
            m_fFrameAlpha += fAlphaShift;

            if (m_fFrameAlpha >= 1.0 - CLOSE) {
               m_fFrameAlphaDirection = !m_fFrameAlphaDirection;
               CellularFill (1, pRes, dwEffect, fLightBackground);
            }
         }
         else {
            m_fFrameAlpha -= fAlphaShift;

            if (m_fFrameAlpha <= CLOSE) {
               m_fFrameAlphaDirection = !m_fFrameAlphaDirection;
               CellularFill (0, pRes, dwEffect, fLightBackground);
            }
         }

         fAlphaLeft -= fAlphaShift;
      } // while time left

      fp fAlpha = m_fFrameAlpha;
      fp fAlphaInv = 1.0 - fAlpha;

      // make sure enough work memoy
      m_memHYPNOCELLWork.m_dwCurPosn = pRes.x * pRes.y * sizeof(HYPNOCELL);
      if (!m_memHYPNOCELLWork.Required (m_memHYPNOCELLWork.m_dwCurPosn))
         return FALSE;

      // interpolate
      int iAlpha = (int)(fAlpha * 256.0);
      int iAlphaInv = 256 - iAlpha;

      PHYPNOCELL phcA = (PHYPNOCELL)m_amemHYPNOCELL[0].p;
      PHYPNOCELL phcB = (PHYPNOCELL)m_amemHYPNOCELL[1].p;
      PHYPNOCELL phcDest = (PHYPNOCELL)m_memHYPNOCELLWork.p;
      DWORD dwNum = (DWORD)(pRes.x * pRes.y);
      for (i = 0; i < dwNum; i++, phcDest++, phcA++, phcB++) {
         phcDest->sX = (short)(((int)phcA->sX * iAlpha + (int)phcB->sX * iAlphaInv) / 256);
         phcDest->sY = (short)(((int)phcA->sY * iAlpha + (int)phcB->sY * iAlphaInv) / 256);
         phcDest->cBright = (char)(((int)phcA->cBright * iAlpha + (int)phcB->cBright * iAlphaInv) / 256);
         phcDest->cColor = (char)(((int)phcA->cColor * iAlpha + (int)phcB->cColor * iAlphaInv) / 256);
         phcDest->bBlur = (BYTE)(((int)(DWORD)phcA->bBlur * iAlpha + (int)(DWORD)phcB->bBlur * iAlphaInv) / 256);
      } // i

      // copy over the height field for the other thread
      EnterCriticalSection (&m_CritSec);
      if (m_memHYPNOCELL.Required (m_memHYPNOCELLWork.m_dwCurPosn)) {
         m_memHYPNOCELL.m_dwCurPosn = m_memHYPNOCELLWork.m_dwCurPosn;
         memcpy (m_memHYPNOCELL.p, m_memHYPNOCELLWork.p, m_memHYPNOCELLWork.m_dwCurPosn);
      }
      LeaveCriticalSection (&m_CritSec);
   }
   else { // dwThread = 0
      if (!pis)
         return FALSE;  // shouldnt happen

      // copy the existing image
      if (!pis->CloneTo (&m_isCopy))
         return FALSE;  // couldnt draw


      EnterCriticalSection (&m_CritSec);

      // store the size, so second thread can use
      m_pRes.x = (int)pis->Width();
      m_pRes.y = (int)pis->Height();
      DWORD dwNum = pis->Width() * pis->Height();
      if ((m_memHYPNOCELL.m_dwCurPosn != dwNum * sizeof(HYPNOCELL)) || (m_pRes.x < 3) || (m_pRes.y < 3)) {
               // Need to check for 3 pixels so other optimizations work
         LeaveCriticalSection (&m_CritSec);
         return FALSE;
      }

      // BUGFIX - Moved this afterwards
      StimulusFill (&m_isCopy, m_dwEffect, fLightBackground, fTimeElapsed);

      fp fTimeScalePixels = fTimeElapsed / CELLULAR_FRAMERATEPIXELS;
      int iTimeScaleRest =  (int)(fTimeElapsed / CELLULAR_FRAMERATEREST * 256.0);
      fTimeScalePixels = max(fTimeScalePixels, CLOSE);
      iTimeScaleRest = max(iTimeScaleRest, 1);

      // fill in pis with map
      PBYTE pabColor = (PBYTE)pis->Pixel(0,0);
      memset (pabColor, 0, m_pRes.x * m_pRes.y * 3 * sizeof(BYTE));
      PBYTE pabColorCopy = (PBYTE)m_isCopy.Pixel(0,0);
      PHYPNOCELL phc = (PHYPNOCELL)m_memHYPNOCELL.p;
      fp fXWant, fYWant;
      int iX, iY, iXWant, iYWant, iBright, iColor, iAvg;
      int iXOffset, iYOffset;
      int aiColorSum[3];
      int aiScale[9] = {1,2,1, 2,3,2, 1,2,1};
      int aiBlur[9];
      int *paiScale;
      for (iY = 0; iY < m_pRes.y; iY++) {
         for (iX = 0; iX < m_pRes.x; iX++, pabColorCopy += 3, phc++) {
            // fill in color
            aiColorSum[0] = pabColorCopy[0];
            aiColorSum[1] = pabColorCopy[1];
            aiColorSum[2] = pabColorCopy[2];

            // brighten
            iBright = (iTimeScaleRest * phc->cBright) / 256;
            if (iBright) {
               iBright += 128;
               aiColorSum[0] = aiColorSum[0] * iBright / 128;
               aiColorSum[1] = aiColorSum[1] * iBright / 128;
               aiColorSum[2] = aiColorSum[2] * iBright / 128;
            }

            // colorisze
            iColor = (iTimeScaleRest * phc->cColor) / 256;
            if (iColor) {
               iColor += 256;
               iAvg = (aiColorSum[0] + aiColorSum[1] + aiColorSum[2]) / 3;
               aiColorSum[0] = ((aiColorSum[0] - iAvg) * iColor / 256) + iAvg;
               aiColorSum[1] = ((aiColorSum[1] - iAvg) * iColor / 256) + iAvg;
               aiColorSum[2] = ((aiColorSum[2] - iAvg) * iColor / 256) + iAvg;
            }

            // amount to blur
            int iBlur = (int)(DWORD)phc->bBlur * iTimeScaleRest / 256;
            iBlur = min(iBlur, 256);
            int iBlurInv = 256 - iBlur;

            int iSum = 0;
            for (i = 0; i < 9; i++) {
               aiBlur[i] = iBlur * aiScale[i];
               iSum += aiBlur[i];
            }
            aiBlur[4] += iBlurInv * aiScale[4];
            iSum += iBlurInv * aiScale[4];
            for (i = 0; i < 9; i++)
               aiBlur[i] = aiBlur[i] * 256 / iSum; // so sums up to 256




            // figure out where to write it to
            // make sure that don't go off screen
            fXWant = (fp)iX + fTimeScalePixels * phc->sX;
            fYWant = (fp)iY + fTimeScalePixels * phc->sY;
            iXWant = floor(fXWant);
            iYWant = floor(fYWant);
            int iXWantExtra = (int)((fXWant - (fp)iXWant) * 16.0);
            int iYWantExtra = (int)((fYWant - (fp)iYWant) * 16.0);

            int iXWantSub, iYWantSub, iXReally, iYReally, iYScale, iXScale, iXYScale;
            for (iYWantSub = 0; iYWantSub < 2; iYWantSub++) {
               iYReally = iYWant + iYWantSub;
               iYScale = iYWantSub ? iYWantExtra : (16 - iYWantExtra);
               if ((iYReally < 2) || (iYReally >= m_pRes.y - 2))
                  continue;
               for (iXWantSub = 0; iXWantSub < 2; iXWantSub++) {
                  iXReally = iXWant + iXWantSub;
                  if ((iXReally < 1) || (iXReally >= m_pRes.x-2))
                     continue;
                  iXScale = iXWantSub ? iXWantExtra : (16 - iXWantExtra);
                  iXYScale = iXScale * iYScale;

                  if (iBlur) {
                     // where to write to
                     pabColor = pis->Pixel (iXReally-1, iYReally-1);

                     paiScale = &aiBlur[0];


                     for (iYOffset = 0; iYOffset < 3; iYOffset++, pabColor += ((m_pRes.x-3)*3)) {
                        for (iXOffset = 0; iXOffset < 3; iXOffset++, paiScale++) {
                           for (i = 0; i < 3; i++, pabColor++) {
                              // add in this color
                              int iTemp = *pabColor;
                              iTemp += aiColorSum[i] * (*paiScale) * iXYScale / (256 * 256);
                              iTemp = max(iTemp, 0);
                              iTemp = min(iTemp,255);
                              *pabColor = (BYTE)(DWORD)iTemp;
                           } // i
                        } // iXOffset
                     } // iYOffset
                  } // if iBlur
                  else {
                     // !iBlur
                     pabColor = pis->Pixel (iXReally, iYReally);
                     for (i = 0; i < 3; i++, pabColor++) {
                        // add in this color
                        int iTemp = *pabColor;
                        iTemp += aiColorSum[i] * iXYScale / 256;
                        iTemp = max(iTemp, 0);
                        iTemp = min(iTemp,255);
                        *pabColor = (BYTE)(DWORD)iTemp;
                     } // i
                  }
               } // iXWantSub
            } // iYWantSub


         } // iX
      } // iY

      LeaveCriticalSection (&m_CritSec);
   }

   return TRUE;
}



/*************************************************************************************
CHypnoEffectCellular::DeleteSelf - standard api
*/
void CHypnoEffectCellular::DeleteSelf (void)
{
   delete this;
}


/*************************************************************************************
CHypnoEffectCellular::CellularFill - Fill in a noise buffer.

inputs
   DWORD       dwPingPong - Ping pong number, 0 or 1
   POINT       pRes - Resolution
   DWORD       dwEffect - Effect number
   BOOL                 fLightBackground - TRUE if it's a light background
*/
void CHypnoEffectCellular::CellularFill (DWORD dwPingPong, POINT pRes, DWORD dwEffect, BOOL fLightBackground)
{
   DWORD dwNeed = pRes.x * pRes.y * sizeof(HYPNOCELL);
   if (!m_amemHYPNOCELL[dwPingPong].Required (dwNeed))
      return;
   m_amemHYPNOCELL[dwPingPong].m_dwCurPosn = dwNeed;
   PHYPNOCELL phc = (PHYPNOCELL)m_amemHYPNOCELL[dwPingPong].p;
   memset (phc, 0, dwNeed);   // zero it out

   fp fTwist;
   int iCenterX, iCenterY;
   switch (dwEffect) {
      case CELLULAR_SWIRL:
         fTwist = randf(0.1, 0.3);
         iCenterX = (rand() % (pRes.x / 2)) + pRes.x / 4;
         iCenterY = (rand() % (pRes.y / 2)) + pRes.y / 4;
         break;
      case CELLULAR_STARGATE:
      default:
         fTwist = randf(-0.5, 0.5);
         iCenterX = (rand() % (pRes.x / 2)) + pRes.x / 4;
         iCenterY = (rand() % (pRes.y / 2)) + pRes.y / 4;
         break;
   } // switch

   int iX, iY;
   //int iSpeed = (int)((fp)pRes.y / 5.0 * CELLULAR_FRAMERATE);  // so depends on resolution
   //iSpeed = max(iSpeed, 0);
   //iSpeed = min(iSpeed, 127);
   //char cSpeed = (char)iSpeed;
   fp fX, fY, fLen, fBlur, fBright, fColor;
   CPoint pVert, pTwist, pRot;
   pVert.Zero ();
   pTwist.Zero();
   pVert.p[2] = 1;
   for (iY = 0; iY < pRes.y; iY++) {
      for (iX = 0; iX < pRes.x; iX++, phc++) {
         // some defaults
         fBlur = 256.0;
         fBright = -64.0;
         fColor = 0.0;

         switch (m_dwEffect) {
            case CELLULAR_STARGATE:
            case CELLULAR_SWIRL:
            default:
               fX = (fp)(iX - iCenterX) / (fp)pRes.x;
               fY = (fp)(iY - iCenterY) / (fp)pRes.x;
               fLen = sqrt(fX * fX + fY * fY);

               pTwist.p[0] = fX;
               pTwist.p[1] = fY;
               pRot.CrossProd (&pVert, &pTwist);

               if (m_dwEffect == CELLULAR_SWIRL) {
                  fX /= 16.0;
                  fY /= 16.0;
                  fTwist = 0.2;
               }

               fX += pRot.p[0] * fTwist;
               fY += pRot.p[1] * fTwist;

               fX *= 0.25;
               fY *= 0.25;
               fBlur = 0;
               // fBlur = fLen * 4000.0;
               // fLen = sqrt(fLen);   // so limts out
               if (m_dwEffect == CELLULAR_SWIRL)
                  fBright = 0;
               else
                  fBright = (fLen < .1) ? -64 : 64; // fLen * 512.0 - 64.0;  // so center is dark
               // fColor
               break;
         } // m_dwEffect

         // convert fX and fY to int
         // fX and fY are originally in screen-widths per second
         fX *= (fp)pRes.x * (fp)CELLULAR_FRAMERATEPIXELS;
         fY *= (fp)pRes.x * (fp)CELLULAR_FRAMERATEPIXELS;   // inteitonally scaling by pRes.x
         fX = max(fX, -32767);
         fX = min(fX, 32767);
         fY = max(fY, -32767);
         fY = min(fY, 32767);
         phc->sX = (short)floor(fX + 0.5);
         phc->sY = (short)floor(fY + 0.5);

         // others
         fBlur *= CELLULAR_FRAMERATEREST;
         fBlur = max(fBlur, 0);
         fBlur = min(fBlur, 255);
         phc->bBlur = (BYTE)floor(fBlur + 0.5);
         fBright *= CELLULAR_FRAMERATEREST;
         fBright = max(fBright, -127);
         fBright = min(fBright, 127);
         phc->cBright = (char)floor(fBright + 0.5);
         fColor *= CELLULAR_FRAMERATEREST;
         fColor = max(fColor, -127);
         fColor = min(fColor, 127);
         phc->cColor = (char)floor(fColor + 0.5);
      } // iX
   } // iY
}


/*************************************************************************************
CHypnoEffectCellular::StimulusFill - Fills the image with bright/dark stimulus which
causes the effects.

inputs
   PCImageStore         pImage - Image
   DWORD                dwEffect - Effect number
   BOOL                 fLightBackground - TRUE if it's a light background
   fp                   fTimeElapsed - Time elapsed
returns
   none
*/
void CHypnoEffectCellular::StimulusFill (PCImageStore pImage, DWORD dwEffect, BOOL fLightBackground, fp fTimeElapsed)
{
   DWORD dwNumStars = (DWORD)(fTimeElapsed * 100.0);
      // a default

   switch (dwEffect) {
      case CELLULAR_STARGATE:
      case CELLULAR_SWIRL:
      default:
         dwNumStars *= 100;
         break;
   } // switch

   DWORD i;
   PBYTE pab;
   DWORD dwWidth = pImage->Width();
   DWORD dwHeight = pImage->Height();

   fp fAngle, fDist, fX, fY;
   int iX, iY;
   DWORD j;
   for (i = 0; i < dwNumStars; i++) {
      switch (dwEffect) {
         case CELLULAR_SWIRL:
            iX = rand() % (int)dwWidth;
            iY = rand() % (int)dwHeight;

            if ((iX >= 0) && (iX < (int)dwWidth) && (iY >= 0) && (iY < (int)dwHeight)) {
               pab = pImage->Pixel((DWORD)iX, (DWORD)iY);
               for (j = 0; j < 3; j++) {
                  int iTemp = (int)(DWORD)pab[j];
                  iTemp += (rand()%256);
                  iTemp = min(iTemp, 255);
                  pab[j] = (BYTE)(DWORD)iTemp;
               } // j
            }
            break;

         case CELLULAR_STARGATE:
         default:
            // iX = rand() % (int)dwWidth;
            //iY = rand() % (int)dwHeight;

            fAngle = randf(0, 2.0 * PI);
            fDist = randf(0, 1.0);
            // fDist = pow(randf(0, 1.0), 2.0);
            // if (fDist < 0.1)
            //   continue;   // not too close to the center
            fDist *= (fp)dwWidth/2.0;
            fX = sin(fAngle) * fDist + (fp)dwWidth/2;
            fY = cos(fAngle) * fDist + (fp)dwHeight/2;

            iX = (int)floor(fX + 0.5);
            iY = (int)floor(fY + 0.5);

            if ((iX >= 0) && (iX < (int)dwWidth) && (iY >= 0) && (iY < (int)dwHeight)) {
               pab = pImage->Pixel((DWORD)iX, (DWORD)iY);
               for (j = 0; j < 3; j++) {
                  int iTemp = (int)(DWORD)pab[j];
                  iTemp += (rand()%256);
                  iTemp = min(iTemp, 255);
                  pab[j] = (BYTE)(DWORD)iTemp;
               } // j
            }
            break;
      } // dwEffect
   } // i

}

