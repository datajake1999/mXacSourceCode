/***************************************************************************
Register.cpp - Handle registration.

begun 9/19/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "dragonfly.h"

#define  RANDNUM1    0x89af4535     // change for every app
#define  RANDNUM2    0xef84fd29     // change for every app
#define  RANDNUM3    0xa6f2a695     // change for every app

static DWORD   gdwKey = 0; // registration key read in
static char    gszEmail[256] = ""; // currently registered email
static BOOL    fReadKey = FALSE; // if TRUE read key from registry
static char    gszRegKey[] = "RegKey";
static char    gszRegEmail[] = "RegEmail";

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
   HANGFUNCIN;
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
   HANGFUNCIN;
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
   HANGFUNCIN;
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
   HANGFUNCIN;
   // if haven't read then do so
   if (!fReadKey) {
      fReadKey = TRUE;
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
   HANGFUNCIN;
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
   HANGFUNCIN;
   WideCharToMultiByte (CP_ACP, 0, pszEmail, -1, gszEmail, sizeof(gszEmail), 0, 0);
   gdwKey = dwKey;
   fReadKey = TRUE;

   WriteRegKey (dwKey, gszEmail);
}
