/***************************************************************************
Register.cpp - Handle registration.

begun 9/19/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
//#include <winsock2.h>
//#include <winerror.h>
#include <objbase.h>
#include <dsound.h>
#include <shlwapi.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"


// BUGBUG - These are dragonfly registration numbers. Need to change later
#define  RANDNUM1    0x89af4535     // change for every app
#define  RANDNUM2    0xef84fd29     // change for every app
#define  RANDNUM3    0xa6f2a695     // change for every app


#define TRIALTIME       (5 * 60)    // 5 hour trial time

static DWORD   gdwKey = 0; // registration key read in
static char    gszEmail[256] = ""; // currently registered email
static BOOL    gfReadKey = FALSE; // if TRUE read key from registry
static char    gszRegKey[] = "RegKey";
static char    gszRegEmail[] = "RegEmail";
static PSTR    gpszAppName = "Circumreality";
static char        gszRegUsed[] = "SystemNum";
static int     giVScroll = 0; // scroll for registartion page

static PWSTR gpszFeatureEnabled = L"<font color=#008000><small>Enabled</small></font>";
static PWSTR gpszFeatureTempEnabled = L"<font color=#808000><small>Temporarily enabled</small></font>";
static PWSTR gpszFeatureDisabled = L"<font color=#800000><small>Disabled</small></font>";


/****************************************************************8
GetRegKey - Get the currently entered registration key and Email.

inputs
   char  *psz - Filled with E-mail
   DWORD dwSize - size
returns
   DWORD - current reg key
*/
DWORD GetRegKey (char *psz, DWORD dwSizeSz)
{
   DWORD dwKey;
   dwKey = 0;
   psz[0] = 0;

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return 0;

   DWORD dwSize, dwType;
   dwSize = sizeof(DWORD);
   RegQueryValueEx (hKey, gszRegKey, NULL, &dwType, (LPBYTE) &dwKey, &dwSize);

   dwSize = dwSizeSz;
   RegQueryValueEx (hKey, gszRegEmail, NULL, &dwType, (LPBYTE) psz, &dwSize);

   RegCloseKey (hKey);

   return dwKey;
}

/****************************************************************8
WriteRegKey - Write the currently entered registration key and Email.

inputs
   DWORD dwKey - key #
   char  *psz - Filled with E-mail
returns
   none
*/
void WriteRegKey (DWORD dwKey, char *psz)
{
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, CircumrealityRegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   RegSetValueEx (hKey, gszRegKey, 0, REG_DWORD, (BYTE*) &dwKey, sizeof(dwKey));
   RegSetValueEx (hKey, gszRegEmail, 0, REG_SZ, (BYTE*) psz, (DWORD)strlen(psz)+1);

   RegCloseKey (hKey);

   return;
}


/**********************************************************************************
MySRand, MyRand - Personal random functions.
*/
static DWORD   gdwRandSeed;

static void MySRand (DWORD dwVal)
{
   gdwRandSeed = dwVal;
}

static DWORD MyRand (void)
{
   gdwRandSeed = (gdwRandSeed ^ RANDNUM1) * (gdwRandSeed ^ RANDNUM2) +
      (gdwRandSeed ^ RANDNUM3);

   return gdwRandSeed;
}



/**********************************************************************************
HashString - Hash an E-mail (or other string) to a DWORD number. Use this as the
registration key.

inputs
   char  *psz
returns
   DWORD
*/
static DWORD HashString (char *psz)
{
   DWORD dwSum;

   DWORD i;
   dwSum = 324233;
   for (i = 0; psz[i]; i++) {
      MySRand ((DWORD) tolower (psz[i]));
      MyRand ();
      dwSum += (DWORD) MyRand();
   }

   return dwSum;
}




/***********************************************************************************
RegisterIsRegistered - Returns TRUE if the person is registered.
*/
BOOL RegisterIsRegistered (void)
{
   // if haven't read then do so
   if (!gfReadKey) {
      gfReadKey = TRUE;
      gdwKey = GetRegKey (gszEmail, sizeof(gszEmail));
   }

   return HashString(gszEmail) == gdwKey;
}

/***********************************************************************************
RegisterKeyGet - Get the current key.

inputs
   PWSTR    pszEmail - filled with E-mail
   DWORD    dwSize - Number of chars in pszEmail
returns
   DWORD - key
*/
DWORD RegisterKeyGet (WCHAR *pszEmail, DWORD dwSize)
{
   // make sure loaded in
   RegisterIsRegistered();

   MultiByteToWideChar (CP_ACP, 0, gszEmail, -1, pszEmail, dwSize);

   return gdwKey;
}

/***********************************************************************************
RegisterKeySet - Get the current key and writes to the registry.

inputs
   PWSTR    pszEmail - email
   DWORD - key
*/
void RegisterKeySet (PWSTR pszEmail, DWORD dwKey)
{
   WideCharToMultiByte (CP_ACP, 0, pszEmail, -1, gszEmail, sizeof(gszEmail), 0, 0);
   gdwKey = dwKey;
   gfReadKey = TRUE;

   WriteRegKey (dwKey, gszEmail);
}


/****************************************************************8
RegKeyForCounter - opens/creates a regkey where the timing counter
can be written. It's specifically put in a strange place.

inputs
   char  *psz - string for book
returns
   HKEY - key
*/
HKEY RegKeyForCounter (char *psz)
{  
   BYTE  abGUID[16];
   
   // hash to guid
   memset (&abGUID, 0, sizeof(abGUID));
   int   i, j, count;
   count = 0;
   for (i = 0; i < 25; i++)
      for (j = 0; psz[j]; j++) {
         abGUID[count] += (BYTE) psz[j];
         count = (count + 1) % 16;
      }

   // create the string
   char  szTemp[256];
   strcpy (szTemp, "Software\\Microsoft\\Windows\\CurrentVersion\\TempCLSID\\{");
   for (i = 0; i < 16; i++) {
      char  sz2[4];
      sz2[0] = "0123456789abcdef"[abGUID[i] / 16];
      sz2[1] = "0123456789abcdef"[abGUID[i] % 16];
      sz2[2] = 0;
      if ((i == 3) || (i == 5) || (i == 7) || (i == 9))
         sz2[2] = '-';
      sz2[3] = 0;

      strcat (szTemp, sz2);
   }
   strcat (szTemp, "}");

   // save to registry
   HKEY  hKey;
   hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_LOCAL_MACHINE, szTemp, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   return hKey;
}


/***********************************************************************
GetAndIncreaseUsage - Gets the usage counter and increases by N
*/
DWORD GetAndIncreaseUsage (DWORD dwAmount)
{
   static DWORD dwOldValue = (DWORD)-1;

   // if just getting and have already read then dont bother with registry
   if ((dwOldValue != (DWORD)-1) && !dwAmount)
      return dwOldValue;

   DWORD dwCount = 0;
   HKEY hKey;
   hKey = RegKeyForCounter (gpszAppName);
   if (hKey) {
      DWORD dwSize, dwType;
      dwSize = sizeof(dwCount);
      RegQueryValueEx (hKey, gszRegUsed, NULL, &dwType, (LPBYTE) &dwCount, &dwSize);

      if (dwAmount) {
         dwCount += dwAmount;

         RegSetValueEx (hKey, gszRegUsed, 0, REG_DWORD, (BYTE*) &dwCount, sizeof(dwCount));
      }

      RegCloseKey (hKey);
   }

   dwOldValue = dwCount;
   return dwCount;
}

/***********************************************************************
RegisterMode - Returns how much the player has regeistered

returns
   int - 0 if not payed, and past trial period
         1 if payed
         -1 if not payed, but still in trial
*/
int RegisterMode (void)
{
   DWORD dwUsage = GetAndIncreaseUsage(0);
   BOOL fRegistered = RegisterIsRegistered();

   if (fRegistered)
      return 1;
   else if (dwUsage < TRIALTIME)
      return -1;
   else
      return 0;
}

/***********************************************************************
RegisterPage - Page callback for new user (not quick-add though)
*/
BOOL RegisterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // scroll to right position
         if (giVScroll > 0) {
            pPage->VScroll (giVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            giVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         // set the text
         WCHAR szEmail[256];
         DWORD dwKey;
         dwKey = RegisterKeyGet (szEmail, sizeof(szEmail)/2);

         // show it
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"email");
         if (pControl)
            pControl->AttribSet (Text(), szEmail);
         pControl = pPage->ControlFind (L"regkey");
         if (pControl) {
            // BUGFIX - Didn't handle passwords larger than 2 billion
            WCHAR szTemp[32];
            swprintf (szTemp, L"%u", dwKey);
            pControl->AttribSet (Text(), szTemp);
         }

         // update the UI
         pPage->Message (ESCM_USER+192);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         // get the old status
         int iOldStatus = RegisterMode ();

         // get the registration key and text
         WCHAR szEmail[256];
         DWORD dwNeeded, dwKey;
         PCEscControl pControl;
         szEmail[0] = 0;
         dwKey = 0;
         pControl = pPage->ControlFind (L"email");
         if (pControl)
            pControl->AttribGet (Text(), szEmail, sizeof(szEmail), &dwNeeded);
         pControl = pPage->ControlFind (L"regkey");
         if (pControl) {
            // BUGFIX - Didn't handle passwords larger than 2 billion
            WCHAR szTemp[64];
            szTemp[0] = 0;
            pControl->AttribGet (Text(), szTemp, sizeof(szTemp) / 2, &dwNeeded);
            WCHAR *pEnd;
            dwKey = wcstoul (szTemp, &pEnd, 10);
         }

         // save it away
         RegisterKeySet (szEmail, dwKey);

         // get the new status
         int iNewStatus = RegisterMode();

         // if changed then redo page
         if (iOldStatus != iNewStatus) {
            pPage->Exit (RedoSamePage());

            // re-calc monitor info
            MonitorInfoFill (TRUE);

            return TRUE;
         }
         
         // else
         // change UI
         pPage->Message (ESCM_USER+192);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"ENABLEDALWAYS")) {
            p->pszSubString = gpszFeatureEnabled;
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"ENABLEDTRIAL")) {

            switch (RegisterMode()) {
            default:
            case 0:  // free mode
               p->pszSubString = gpszFeatureDisabled;
               break;

            case 1:  // paid mode
               p->pszSubString = gpszFeatureEnabled;
               break;

            case -1: // trial mode
               p->pszSubString = gpszFeatureTempEnabled;
               break;
            } // switch

            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"ENABLEDFULL")) {

            switch (RegisterMode()) {
            default:
            case 0:  // free mode
            case -1: // trial mode
               p->pszSubString = gpszFeatureDisabled;
               break;

            case 1:  // paid mode
               p->pszSubString = gpszFeatureEnabled;
               break;
            } // switch

            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"NEXTBUTTON")) {
            switch (RegisterMode()) {
            default:
            case 0:  // free mode
               p->pszSubString = L"<font color=#800000>Play without paying</font>";
               break;

            case 1:  // paid mode
               p->pszSubString = L"<font color=#008000>Play the full version</font>";
               break;

            case -1: // trial mode
               p->pszSubString = L"<font color=#808000>Play the trial version</font>";
               break;
            } // switch

            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"SHOWIFHASPAID")) {
            switch (RegisterMode()) {
            default:
            case 0:  // free mode
               p->pszSubString = L"<font color=#800000>"
                  L"<bold>Your trial period has expired!</bold> If you wish to use any disabled features "
                  L"you'll need to pay for a full version of Circumreality. See below."
                  L"</font>";
               break;

            case 1:  // paid mode
               p->pszSubString = L"<font color=#008000>"
                  L"<bold>You have paid for Circumreality and are playing the full version.</bold> All features "
                  L"are enabled."
                  L"</font>";
               break;

            case -1: // trial mode
               int iTime;
               MemZero (&gMemTemp);
               MemCat (&gMemTemp, L"<font color=#808000>"
                  L"You are still in a <bold>trial period</bold> and most features are temporarily "
                  L"enabled so you can try them out. You only have <bold>");
               iTime = TRIALTIME -(int)GetAndIncreaseUsage();
               if (iTime >= 60) {
                  MemCat (&gMemTemp, iTime / 60);
                  MemCat (&gMemTemp, L" hours");
                  if (iTime % 60) {
                     MemCat (&gMemTemp, L" and ");
                     MemCat (&gMemTemp, iTime % 60);
                     MemCat (&gMemTemp, L" minutes");
                  }
               }
               else {
                     MemCat (&gMemTemp, iTime);
                     MemCat (&gMemTemp, L" minutes");
               }
               MemCat (&gMemTemp, L"</bold> "
                  L"left in the trial before some features will be disabled.</font>");

               p->pszSubString = (PWSTR)gMemTemp.p;
               break;
            } // switch

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
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;
         if (p->psz && !_wcsicmp(p->psz, L"playhq")) {
            PlaySound (NULL, ghInstance, SND_PURGE);  // stop playing existing
            PlaySound (MAKEINTRESOURCE (IDR_WAVEVOICESAMPLELARGE), ghInstance, SND_ASYNC | SND_RESOURCE);
            return TRUE;
         }
         if (p->psz && !_wcsicmp(p->psz, L"playlq")) {
            PlaySound (NULL, ghInstance, SND_PURGE);  // stop playing existing
            PlaySound (MAKEINTRESOURCE (IDR_WAVEVOICESAMPLESMALL), ghInstance, SND_ASYNC | SND_RESOURCE);
            return TRUE;
         }
      }
      break;

   case ESCM_USER+192:
      {
         // set UI
         PWSTR psz;
         psz = !RegisterIsRegistered () ?
            L"<br><colorblend posn=background lcolor=#ffc000 rcolor=#ff0000/>You have <bold>not</bold> paid for the full version of Circumreality.</br>" :
            L"<br><colorblend posn=background lcolor=#40c040 rcolor=#40c0c0/><bold>Thank you.</bold><br/>You have <bold>payed for</bold> the full version of Circumreality.</br>";
         WCHAR szTemp[512];
         wcscpy (szTemp, psz);

         PCEscControl pControl;
         ESCMSTATUSTEXT st;
         memset (&st, 0, sizeof(st));
         st.pszMML = szTemp;
         pControl = pPage->ControlFind (L"top");
         if (pControl)
            pControl->Message (ESCM_STATUSTEXT, &st);
         pControl = pPage->ControlFind (L"bottom");
         if (pControl)
            pControl->Message (ESCM_STATUSTEXT, &st);
      }
      return TRUE;

   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
RegisterPageDisplay - Displays the registration page.

inputs
   PCEscWindow       pWindow - Window to use
returns
   BOOL - TRUE if success, FALSE if should close
*/
BOOL RegisterPageDisplay (PCEscWindow pWindow)
{
   PWSTR pszRet;
   giVScroll = 0;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLREGISTER, RegisterPage, NULL);
   giVScroll = pWindow->m_iExitVScroll;

   if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
      goto redo;

   return (pszRet && !_wcsicmp(pszRet, L"next"));
}
