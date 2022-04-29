/*********************************************************************88
Register.cpp - Insure that the user is registered

  Begun 3/14/2000 by Mike Rozak
  Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// the following variables must be modified for each application
#define  RANDNUM1    0x8a3498af     // change for every app
#define  RANDNUM2    0x1bf8c924     // change for every app
#define  RANDNUM3    0x5d388cc1     // change for every app
static char       gszAppName[] = APPLONGNAME;
static char       gszRegBase[] = "Software\\mXac\\" APPLONGNAME;




static char        gszRegKey[] = "RegKey";
static char        gszRegEmail[] = "RegEmail";
static char        gszPasswordWeb[] = "http://www.mXac.com.au/HowRegister.htm";


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
   RegCreateKeyEx (HKEY_CURRENT_USER, gszRegBase, 0, 0, REG_OPTION_NON_VOLATILE,
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
   RegCreateKeyEx (HKEY_CURRENT_USER, gszRegBase, 0, 0, REG_OPTION_NON_VOLATILE,
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

void MySRand (DWORD dwVal)
{
   gdwRandSeed = dwVal;
}

DWORD MyRand (void)
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
DWORD HashString (char *psz)
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



#if 0
int main (int argc, char **argv)
{
   if (argc != 2) {
      printf ("SiliconPageKey.exe <EmailName>");
      return -1;
   }

   DWORD dw;
   dw = HashString (argv[1]);
   printf ("%u\n", dw);

   return 0;

}
#endif // 0


/********************************************************************************
CenterDialog - Centers the dialog.

inputs
   HWND hWnd - window
*/
static void CenterDialog (HWND hWnd)
{
   RECT r, rWork;

   GetWindowRect (hWnd, &r);
   SystemParametersInfo (SPI_GETWORKAREA, 0, &rWork, 0);

   int   cx, cy;
   cx = (rWork.right - rWork.left) / 2 + rWork.left;
   cy = (rWork.bottom - rWork.top) / 2 + rWork.top;
   MoveWindow (hWnd, cx - (r.right-r.left)/2, cy - (r.bottom-r.top)/2, r.right-r.left, r.bottom-r.top, TRUE);
}

#define  ESCM_SHOWREGISTER       (ESCM_USER+342)

static WCHAR gszEmail[] = L"Email";
static WCHAR gwszRegKey[] = L"RegKey";

/***********************************************************************************
RegisterIsRegistered - Returns TRUE if the person is registered.
*/
BOOL RegisterIsRegistered (void)
{
   return TRUE;   // BUGBUG - For now always returning true that registered
   char szEmail[256];
   DWORD dwKey;

   dwKey = GetRegKey (szEmail, sizeof(szEmail));

   // BUGFIX - If no Email name then not registered
   if (!szEmail[0])
      return FALSE;

   return HashString(szEmail) == dwKey;
}


/***********************************************************************
RegisterPage - Page callback for new user (not quick-add though)
*/
BOOL RegisterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the text
         WCHAR szEmail[256];
         DWORD dwKey;
         //dwKey = RegisterKeyGet (szEmail, sizeof(szEmail)/2);
         char  szaEmail[256];
         dwKey = GetRegKey (szaEmail, sizeof(szaEmail));
         MultiByteToWideChar (CP_ACP, 0, szaEmail, -1, szEmail, sizeof(szEmail)/2);

         // show it
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszEmail);
         if (pControl)
            pControl->AttribSet (Text(), szEmail);
         pControl = pPage->ControlFind (gwszRegKey);
         if (pControl) {
            // BUGFIX - Didn't handle passwords larger than 2 billion
            WCHAR szTemp[32];
            swprintf (szTemp, L"%u", dwKey);
            pControl->AttribSet (Text(), szTemp);
         }

         // update the UI
         pPage->Message (ESCM_SHOWREGISTER);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         // get the registration key and text
         WCHAR szEmail[256];
         DWORD dwNeeded, dwKey;
         PCEscControl pControl;
         szEmail[0] = 0;
         dwKey = 0;
         pControl = pPage->ControlFind (gszEmail);
         if (pControl)
            pControl->AttribGet (Text(), szEmail, sizeof(szEmail), &dwNeeded);
         pControl = pPage->ControlFind (gwszRegKey);
         if (pControl) {
            // BUGFIX - Didn't handle passwords larger than 2 billion
            WCHAR szTemp[64];
            szTemp[0] = 0;
            pControl->AttribGet (Text(), szTemp, sizeof(szTemp) / 2, &dwNeeded);
            WCHAR *pEnd;
            dwKey = wcstoul (szTemp, &pEnd, 10);
         }

         // save it away
         //RegisterKeySet (szEmail, dwKey);
         char  szaEmail[128];
         WideCharToMultiByte (CP_ACP, 0, szEmail, -1, szaEmail, sizeof(szaEmail),0,0);
         WriteRegKey (dwKey, szaEmail);

         // change UI
         pPage->Message (ESCM_SHOWREGISTER);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Registration";
            return TRUE;
         }
      }
      break;

   case ESCM_SHOWREGISTER:
      {
         // set UI
         PWSTR psz;
         psz = !RegisterIsRegistered () ?
            L"<br><colorblend posn=background lcolor=#ffc000 rcolor=#ff0000/>You have <bold>not</bold> registered your copy of " APPLONGNAMEW L".</br>" :
            L"<br><colorblend posn=background lcolor=#40c040 rcolor=#40c0c0/><bold>Thank you.</bold><br/>You have <bold>registered</bold> your copy of " APPLONGNAMEW L".</br>";
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


   return HelpDefPage (pPage, dwMessage, pParam);
}
