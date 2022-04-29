/*********************************************************************88
Register.cpp - Insure that the user is registered

  Begun 3/14/2000 by Mike Rozak
  Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "resource.h"
#include "oilpaint.h"

// the following variables must be modified for each application
#define  RANDNUM1    0x3498af93     // change for every app
#define  RANDNUM2    0x14f8c924     // change for every app
#define  RANDNUM3    0x5d3819c1     // change for every app
static char       gszAppName[] = "Oil Painting Assistant";
static char       gszRegBase[] = "Software\\mXac\\OilPaint";
static DWORD      gdwCountWarning = 5; // use this many times before warning
static DWORD      gdwCountStop = 25;   // use this many times and stop working




static char        gszRegUsed[] = "SystemNum";
static char        gszRegKey[] = "RegKey";
static char        gszRegEmail[] = "RegEmail";
static char        gszPasswordWeb[] = "http://www.mXac.com.au/HowRegister.htm";

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


/****************************************************************8
GetAndIncreaseUsage - Gets the usage counter and increases by 1.
*/
DWORD GetAndIncreaseUsage (void)
{
   DWORD dwCount = 0;
   HKEY hKey;
   hKey = RegKeyForCounter (gszAppName);
   if (hKey) {
      DWORD dwSize, dwType;
      dwSize = sizeof(dwCount);
      RegQueryValueEx (hKey, gszRegUsed, NULL, &dwType, (LPBYTE) &dwCount, &dwSize);

      dwCount++;

      RegSetValueEx (hKey, gszRegUsed, 0, REG_DWORD, (BYTE*) &dwCount, sizeof(dwCount));

      RegCloseKey (hKey);
   }

   return dwCount;
}


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
   RegSetValueEx (hKey, gszRegEmail, 0, REG_SZ, (BYTE*) psz, strlen(psz)+1);

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
WCHAR gszText[] = L"text";

/***********************************************************************************
RegisterIsRegistered - Returns TRUE if the person is registered.
*/
BOOL RegisterIsRegistered (void)
{
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
            pControl->AttribSet (gszText, szEmail);
         pControl = pPage->ControlFind (gwszRegKey);
         if (pControl) {
            // BUGFIX - Didn't handle passwords larger than 2 billion
            WCHAR szTemp[32];
            swprintf (szTemp, L"%u", dwKey);
            pControl->AttribSet (gszText, szTemp);
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
            pControl->AttribGet (gszText, szEmail, sizeof(szEmail), &dwNeeded);
         pControl = pPage->ControlFind (gwszRegKey);
         if (pControl) {
            // BUGFIX - Didn't handle passwords larger than 2 billion
            WCHAR szTemp[64];
            szTemp[0] = 0;
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp) / 2, &dwNeeded);
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

   case ESCM_SHOWREGISTER:
      {
         // set UI
         PWSTR psz;
         psz = !RegisterIsRegistered () ?
            L"<br><colorblend posn=background lcolor=#ffc000 rcolor=#ff0000/>You have <bold>not</bold> registered your copy of Oil Painting Assistant.</br>" :
            L"<br><colorblend posn=background lcolor=#40c040 rcolor=#40c0c0/><bold>Thank you.</bold><br/>You have <bold>registered</bold> your copy of Oil Painting Assistant.</br>";
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


#if 0
/****************************************************************************
PasswordDlgProc - dialog procedure for the registration password
*/
BOOL CALLBACK RegisterDlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_INITDIALOG:
      {
         DWORD dwKey;
         char  szEmail[256];
         dwKey = GetRegKey (szEmail, sizeof(szEmail));

         CenterDialog (hWnd);
         SetDlgItemText (hWnd, IDC_EMAIL, szEmail);
         SetDlgItemInt (hWnd, IDC_PASSWORD, dwKey,FALSE);
         SetDlgItemText (hWnd, IDC_PASSWEB, gszPasswordWeb);

         // enable/disable OK button
         EnableWindow (GetDlgItem(hWnd, IDOK), HashString(szEmail) == dwKey);
      }

      return TRUE;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case IDC_EMAIL:
      case IDC_PASSWORD:
         {
            // enable/disable OK button
            DWORD dwKey;
            char  szEmail[256];

            dwKey = (DWORD) GetDlgItemInt (hWnd, IDC_PASSWORD, NULL, FALSE);
            GetDlgItemText (hWnd, IDC_EMAIL, szEmail, sizeof(szEmail));

            // enable/disable OK button
            EnableWindow (GetDlgItem(hWnd, IDOK), HashString(szEmail) == dwKey);
         }
         break;

      case IDC_WEB:
         ShellExecute (hWnd, NULL, gszPasswordWeb, NULL, NULL, SW_SHOW);
         break;

      case IDOK:
         {
            // enable/disable OK button
            DWORD dwKey;
            char  szEmail[256];

            dwKey = (DWORD) GetDlgItemInt (hWnd, IDC_PASSWORD, NULL, FALSE);
            GetDlgItemText (hWnd, IDC_EMAIL, szEmail, sizeof(szEmail));

            // set them
            WriteRegKey (dwKey, szEmail);
         }

         // fall through
      case IDCANCEL:
         EndDialog (hWnd, LOWORD(wParam));
         return TRUE;
      }
      break;
   }
   return FALSE;
}



/********************************************************************************8
RegisterUI - Brings up the UI asking the user to register. Returns TRUE if the
user is registered, FALSE if not.

inputs
   HWND  hWnd - window
*/
BOOL RegisterUI (HINSTANCE hInstance, HWND hWnd)
{
  return DialogBox (hInstance, MAKEINTRESOURCE(IDD_REGISTER), hWnd, RegisterDlgProc) == IDOK;
}

#endif // 0
#if 0
/********************************************************************************8
RegisterCount - This increases the counter. If it's more than a certain amount a
warning is given. If it's too high this function just calls exit(-1)

inputs
   HINSTANCE      hInstance
   HWND           hWnd
*/
void RegisterCount (HINSTANCE hInstance, HWND hWnd)
{
   DWORD dwCount;
   dwCount = GetAndIncreaseUsage ();

   // if properly registered then do nothing
   DWORD dwKey;
   char  szEmail[256];
   dwKey = GetRegKey (szEmail, sizeof(szEmail));
   // enable/disable OK button
   if (HashString(szEmail) == dwKey)
      return;

   if (dwCount > gdwCountStop) {
      MessageBox (hWnd,
         "The application will not work anymore until you register it.",
         gszAppName,
         MB_OK);
      if (!RegisterUI (hInstance, hWnd))
         exit (-1);
   }
   else if (dwCount > gdwCountWarning) {
      if (IDYES == MessageBox (hWnd,
         "You've used the application several times already. Don't you "
         "think you should register your copy of it? Press YES to register, NO "
         "to use the application anyway.",
         gszAppName,
         MB_YESNO))
         RegisterUI (hInstance, hWnd);
   }

   return;
}
#endif //0