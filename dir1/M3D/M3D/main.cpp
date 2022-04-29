/*********************************************************************************
main.cpp - Main controller for the application
Begun 31/8/2001
Copyright 2001 by Mike Rozak. All rights reserved
*/
#include <windows.h>
#include <zmouse.h>
#include <commctrl.h>
#include <objbase.h>
#include <initguid.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\buildnum.h"
#include <crtdbg.h>

HINSTANCE ghInstance;

/*************************************************************************
AllowToDoubleClick - Writes the registry entries so a user can fp click
*/
void AllowToDoubleClick (void)
{
   char  szFlyFox[] = APPSHORTNAME "Docoument.1";
   char  szFileName[] = APPLONGNAME " Document";
   char  gszAppPath[256];
   HKEY  hInfo = NULL;
   DWORD dwDisp;
   char  szTemp[256];

   GetModuleFileName (ghInstance, gszAppPath, sizeof(gszAppPath));

   // verify that it's not already written
   sprintf (szTemp, "\"%s\" %%1", gszAppPath);
   RegOpenKeyEx (HKEY_CLASSES_ROOT, APPSHORTNAME "Docoument.1\\shell\\open\\command", 0,
      KEY_READ, &hInfo);
   if (hInfo) {
      char  szTemp2[256];
      szTemp2[0] = 0;
      DWORD dwType;
      dwDisp = sizeof(szTemp2);
      RegQueryValueEx (hInfo, NULL, 0, &dwType, (BYTE*) szTemp2, &dwDisp);
      RegCloseKey (hInfo);

      if (!_stricmp(szTemp, szTemp2))
         return;
   }
   // else, not there, so write

   // so can fp click .M3DFILEEXT
   RegCreateKeyEx (HKEY_CLASSES_ROOT, "." M3DFILEEXT, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szFlyFox, sizeof(szFlyFox));
      RegCloseKey (hInfo);
   }

   RegCreateKeyEx (HKEY_CLASSES_ROOT, szFlyFox, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szFileName, sizeof(szFileName));
      RegCloseKey (hInfo);
   }

   sprintf (szTemp, "%s,0", gszAppPath);
   RegCreateKeyEx (HKEY_CLASSES_ROOT, APPSHORTNAME "Docoument.1\\DefaultIcon", 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szTemp, (DWORD)strlen(szTemp)+1);
      RegCloseKey (hInfo);
   }

   RegCreateKeyEx (HKEY_CLASSES_ROOT, APPSHORTNAME "Docoument.1\\protocol\\StdFileEditing\\Server", 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) gszAppPath, (DWORD)strlen(gszAppPath)+1);
      RegCloseKey (hInfo);
   }

   sprintf (szTemp, "\"%s\" %%1", gszAppPath);
   RegCreateKeyEx (HKEY_CLASSES_ROOT, APPSHORTNAME "Docoument.1\\shell\\open\\command", 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szTemp, (DWORD)strlen(szTemp)+1);
      RegCloseKey (hInfo);
   }
}



/*****************************************************************************
WinMain */
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
   // BUGFIX - So higher thread priority on vista
   VistaThreadPriorityHackSet (FALSE);
   SetThreadPriority (GetCurrentThread(), VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));  // so doesnt suck up all CPU

   // BUGFIX - Move allowtodoubleclick here so that can doubleclick m3d files and
   // jump to this exe
   ghInstance = hInstance;
   AllowToDoubleClick ();

   // BUGFIX - Write in registry entry where the app is so that will be able to find
   // the user and server library
   HKEY  hInfo = NULL;
   DWORD dwDisp;
   char  szAppDir[256];
   GetModuleFileName (ghInstance, szAppDir, sizeof(szAppDir));
   char  *pCur;
   for (pCur = szAppDir + strlen(szAppDir); pCur >= szAppDir; pCur--)
      if (*pCur == '\\') {
         pCur[1] = 0;
         break;
      }
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, "LibraryLoc", 0, REG_SZ, (BYTE*) szAppDir, (DWORD)strlen(szAppDir)+1);
      RegCloseKey (hInfo);
   }

   // BUGFIX - Turn on dynamics
   M3DDynamicSet (TRUE);

   return M3DMainLoop (lpCmdLine, nShowCmd);
}



