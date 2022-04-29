/*******************************************************************************
Main.cpp - Main setup code
*/


#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <shlobj.h>
#include <commctrl.h>
#include <stdio.h>
#include <shlguid.h>
#include <math.h>
#include "resource.h"
#include "z:\setup\buildsetup\common.h"

#define  PI    (3.14159265358979323846)
#define  TWOPI (2.0 * PI)


char  gszUninstallFile[256];
char  gszAppDir[256] = "c:\\";
char  gszMainClass[] = "mXacSetup";
char  gszAppName[256] = "Unknown";
char  gszTitle[256] = "";
char  gszSubDir[256] = "";
char  gszMfg[128] = "mXac";
BOOL  gfShowMfg = TRUE;
PSTR gpszmXac = "mXac";
HINSTANCE   ghInstance;
CChunk   gChunk;
char  *gpszInf;   // inf text
DWORD gdwInfSize; // number of bytes
FILE  *gfUninstall = NULL;
double gfPie = 0;


/******************************************************************************
InfFind - Find a value in inf.

inputs
   char     *pszValue - such as "file="
   char     *pszFillIn - Fill in pszFillIn with the data, such as "c:\test.txt"
   DWORD    dwStart - start position in file
   DWORD    *pdwEnd - filled in with the end, which is the next start
returns
   BOOL - TRUE if succede
*/
BOOL InfFind (char *pszValue, char *pszFillIn, DWORD dwStart = 0, DWORD *pdwEnd = NULL)
{
   DWORD dwLen;
   dwLen = strlen(pszValue);

   char  *pMax;
   pMax = gpszInf + gdwInfSize;

   char  *pCur;
   for (pCur = gpszInf + dwStart; pCur < pMax; ) {
      // find the end of the line
      char  *pEOL;
      for (pEOL = pCur; pEOL < pMax; pEOL++) {
         if ((*pEOL == '\n') || (*pEOL == '\r'))
            break;
      }

      // see if find
      if (pCur + dwLen > pMax)
         break;   // beyond edge
      if (!_strnicmp (pCur, pszValue, dwLen)) {
         // match
         DWORD i;
         for (i = 0; pCur + i + dwLen < pEOL; i++)
            pszFillIn[i] = pCur[i+dwLen];
         pszFillIn[i] = 0;
         if (pdwEnd)
            *pdwEnd = (DWORD)(pEOL - gpszInf) +1;
         return TRUE;
      }

      // go to next line
      pCur = pEOL + 1;
   }

   // cant find
   if (pdwEnd)
      *pdwEnd = (DWORD) (pMax - gpszInf) + 1;
   return FALSE;
}
/********************************************************************************
CenterDialog - Centers the dialog.

inputs
   HWND hWnd - window
*/
void CenterDialog (HWND hWnd)
{
   RECT r, rWork;

   GetWindowRect (hWnd, &r);
   SystemParametersInfo (SPI_GETWORKAREA, 0, &rWork, 0);

   int   cx, cy;
   cx = (rWork.right - rWork.left) / 2 + rWork.left;
   cy = (rWork.bottom - rWork.top) / 2 + rWork.top;
   MoveWindow (hWnd, cx - (r.right-r.left)/2, cy - (r.bottom-r.top)/2, r.right-r.left, r.bottom-r.top, TRUE);
}



/****************************************************************************
LicenseDialogProc - dialog procedure for the intro
*/
BOOL CALLBACK LicenseDialogProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_INITDIALOG:
      {
         CenterDialog (hWnd);

         // find the inf chunk
         CChunk   *pInf;
         pInf = gChunk.SubChunkGet (gChunk.SubChunkFind(3));   // 3 = license agreement
         if (!pInf) {
            EndDialog (hWnd, IDCANCEL);
            return TRUE;
         }

         char  *p;
         DWORD dwSize;
         p = (char*)pInf->DataGet ();
         dwSize = pInf->DataSize();

         char  *psz;
         psz = (char*) malloc (dwSize+1);
         memcpy (psz, p, dwSize);
         psz[dwSize] = 0;
         SetDlgItemText (hWnd, IDC_TEXT, psz);
         free (psz);

      }
      return TRUE;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case IDOK:  // IDOK shouldn't happen but...
      case IDCANCEL:
         EndDialog (hWnd, LOWORD(wParam));
         return TRUE;

      }
      break;
   }
   return FALSE;
}


/****************************************************************************
RunSelfExe - Runs the selfexe now

inputs
   HWND        hWnd - WIndow to execute from
*/
void RunSelfExe (HWND hWnd)
{
   char  szShort[256];
   InfFind ("icon=", szShort);
   if (strchr(szShort, ','))
      *(strchr(szShort, ',')) = 0;

   // change the working directory
   SetCurrentDirectory (gszAppDir);

   // see if can find a space in szshort
   char  *pszSpace;
   pszSpace = strchr (szShort, ' ');   // note - may eventually want this .exe_
   if (pszSpace) {
      *pszSpace = 0;
      pszSpace++;
   }
   char  szTemp[256];
   sprintf (szTemp, "%s\\%s", gszAppDir, szShort);
   ShellExecute (hWnd, NULL, szTemp, pszSpace, gszAppDir, SW_SHOW);
   if (pszSpace)
      pszSpace[-1] = ' ';
#if 0
   STARTUPINFO si;
   PROCESS_INFORMATION  pi;
   memset (&si, 0, sizeof(si));
   memset (&pi, 0, sizeof(pi));
   si.cb = sizeof(si);
   if (!CreateProcess (szTemp, NULL, NULL, NULL, FALSE,
      0, NULL, gszAppDir, &si, &pi)) {
      char  szTemp2[512];
      sprintf (szTemp2, "Can't run %s.", szTemp);
      MessageBox (hWnd, szTemp2, NULL, MB_OK);
   }
#endif // 0
}

/****************************************************************************
CompleteDialogProc - dialog procedure for the intro
*/
BOOL CALLBACK CompleteDialogProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_INITDIALOG:
      {
         CenterDialog (hWnd);

         // BUGFIX - Changed installation complete
         // set the text
         //char szTemp[256];
         //strcpy (szTemp, "Installation is complete. You can find the application's shortcut under \"");
         //strcat (szTemp, gszMfg);
         //strcat (szTemp, "\".");
         //SetDlgItemText (hWnd, IDC_STEXT, szTemp);

         // BUGFIX - Install under program files
         SetDlgItemText (hWnd, IDC_STEXT, 
            "Finished installing. You can find the application's shortcut in the Start/Programs menu.");

         // find an icon=
         char  szShort[256];
         if (InfFind ("icon=", szShort))
            CheckDlgButton (hWnd, IDC_RUNNOW, TRUE);
         else
            ShowWindow (GetDlgItem (hWnd, IDC_RUNNOW), SW_HIDE);

      }
      return TRUE;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case IDOK:
      case IDCANCEL:
         // if it's checked
         if (IsDlgButtonChecked (hWnd, IDC_RUNNOW))
            RunSelfExe (hWnd);

         EndDialog (hWnd, LOWORD(wParam));
         return TRUE;

      }
      break;
   }
   return FALSE;
}



/****************************************************************************
DirectoryDialogProc - dialog procedure for the intro
*/
BOOL CALLBACK DirectoryDialogProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_INITDIALOG:
      {
         CenterDialog (hWnd);

         // figure out desktop
         char  szDesktop[256] = "c:\\program files";
         HKEY hKey;
         RegOpenKeyEx (HKEY_LOCAL_MACHINE,
            "Software\\Microsoft\\Windows\\CurrentVersion",
            0, KEY_READ, &hKey);
         if (hKey) {
            DWORD dwSize, dwType;
            dwSize = sizeof(szDesktop);
            RegQueryValueEx (hKey, "ProgramFilesDir", NULL, &dwType, (LPBYTE) szDesktop, &dwSize);
            RegCloseKey (hKey);
         }

         strcat (szDesktop, "\\");
         strcat (szDesktop, gszMfg);
         strcat (szDesktop, "\\");
         strcat (szDesktop, gszAppName);

         SetDlgItemText (hWnd, IDC_TEXT, szDesktop);

      }
      return TRUE;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case IDOK:  
         {
            GetDlgItemText (hWnd, IDC_TEXT, gszAppDir, sizeof(gszAppDir));
            DWORD dw;
            char  szTemp[512];

            // create the sub directories
            char  *pCur, c;
            pCur = strchr (gszAppDir, '\\'); // first backslash
            if (pCur)
               pCur = strchr (pCur+1, '\\');   // second baclslash
            for (; pCur; pCur = strchr(pCur+1, '\\')) {
               c = *pCur;
               *pCur = 0;

               if (!CreateDirectory (gszAppDir, NULL)) {
                  dw = GetLastError();
                  if (dw != 183) {
                     sprintf (szTemp, "Unable to create the directory, %s.", gszAppDir);
                     MessageBox (hWnd, szTemp, "Create Directory", MB_OK);
                     return TRUE;
                  }
               }

               *pCur = c;
            }

            // create the last direcotry
            if (!CreateDirectory (gszAppDir, NULL)) {
               dw = GetLastError();
               if (dw != 183) {
                  sprintf (szTemp, "Unable to create the directory, %s.", gszAppDir);
                  MessageBox (hWnd, szTemp, "Create Directory", MB_OK);
                  return TRUE;
               }
            }

            // write out the fact that need to delete this direcotyr
            // when uninstall
            strcpy (szTemp, gszAppDir);
            strcat (szTemp, "\\uninstall.inf");
            strcpy (gszUninstallFile, szTemp);
            gfUninstall = fopen (szTemp, "wt");
            if (gfUninstall) {
               fprintf (gfUninstall, "file=%s\n", szTemp);
               fprintf (gfUninstall, "dir=%s\n", gszAppDir);
            }
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

/*****************************************************************************
CreateIt - Create a shortcut
*/
HRESULT CreateIt(LPCSTR pszShortcutFile, LPCSTR pszParams, LPSTR pszWorkingDir, LPSTR pszLink, 
  LPSTR pszDesc)
{
    HRESULT hres;
    IShellLink* psl;

    // Get a pointer to the IShellLink interface.
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                            IID_IShellLink, (void**) &psl);
    if (SUCCEEDED(hres))
    {
       IPersistFile* ppf;

       // Query IShellLink for the IPersistFile interface for 
       // saving the shell link in persistent storage.
       hres = psl->QueryInterface(IID_IPersistFile, (void**) &ppf);
       if (SUCCEEDED(hres))
       {   
         WCHAR wsz[MAX_PATH];

         // Set the path to the shell link target.
         hres = psl->SetPath(pszShortcutFile);

//         if (!SUCCEEDED(hres))
//           AfxMessageBox("SetPath failed!");

         if (pszParams)
            psl->SetArguments(pszParams);

         if (pszWorkingDir)
            psl->SetWorkingDirectory(pszWorkingDir);

         // Set the description of the shell link.
         hres = psl->SetDescription(pszDesc);

//         if (!SUCCEEDED(hres))
//           AfxMessageBox("SetDescription failed!");

         // Ensure string is ANSI.
         MultiByteToWideChar(CP_ACP, 0, pszLink, -1, wsz, MAX_PATH);

         // Save the link via the IPersistFile::Save method.
         hres = ppf->Save(wsz, TRUE);
    
         // Release pointer to IPersistFile.
         ppf->Release();
       }
       // Release pointer to IShellLink.
       psl->Release();
    }
    return hres;
}


/*****************************************************************************
Shortcut - Creates a shortcut.

inputs
   char     *pszExe - EXE name, such as flyfox.exe
   char     *pszName - friendly name, such as "flying fox"
returns
   BOOL - TRUE if success
*/
BOOL Shortcut (char *pszExe, char *pszName)
{
   // figure out the directory
   char  szDesktop[256] = "c:\\windows\\start menu\\programs";
   HKEY hKey;
   RegOpenKeyEx (HKEY_CURRENT_USER,
      "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
      0, KEY_READ, &hKey);
   if (hKey) {
      DWORD dwSize, dwType;
      dwSize = sizeof(szDesktop);
      RegQueryValueEx (hKey, "Programs", NULL, &dwType, (LPBYTE) szDesktop, &dwSize);
      RegCloseKey (hKey);
   }

   // look for space in exe name, which indicates a parameter follows
   char  *pszSpace;
   pszSpace = 0;
   pszSpace = strchr(pszExe, ' ');
   if (pszSpace) {
      pszSpace[0] = 0;
      pszSpace++;
   }

   // Create the direcotry
   // BUGFIX - Just stick in program files, not under manufacturer
   //strcat (szDesktop, "\\");
   //strcat (szDesktop, gszMfg);
   CreateDirectory (szDesktop, NULL);
   if (gfUninstall)
      fprintf (gfUninstall, "dir=%s\n", szDesktop);

   // some more names
   strcat (szDesktop, "\\");
   strcat (szDesktop, pszName);
   strcat (szDesktop, ".lnk");

   // remember to delete this
   if (gfUninstall)
      fprintf (gfUninstall, "file=%s\n", szDesktop);

   // link to
   char  szTemp[256];
   sprintf (szTemp, "%s\\%s", gszAppDir, pszExe);
   
   if (pszSpace)
      pszSpace[-1] = ' ';

   return !CreateIt (szTemp, pszSpace, gszAppDir, szDesktop, pszName);
}


/****************************************************************************
MyGetFileVersion - Given a file name, gets the version as a __int64
*/
__int64 MyGetFileVersion (char *szFullPath)
{
   //LPSTR   lpVersion;
   DWORD   dwVerInfoSize;
   DWORD   dwVerHnd;
   //UINT    uVersionLen;
   //WORD    wRootLen;
   //BOOL    bRetCode;
   //char    szGetName[256];
   __int64 iVersion = 0;

   // get the ise
   dwVerInfoSize = 0;
   dwVerInfoSize = GetFileVersionInfoSize(szFullPath, &dwVerHnd);
   if (!dwVerInfoSize)
      return 0;

                   
   // If we were able to get the information, process it:
   LPSTR   lpstrVffInfo;
   lpstrVffInfo  = (char*) malloc(dwVerInfoSize);
   if (!lpstrVffInfo)
      return 0;

   if (!GetFileVersionInfo(szFullPath, dwVerHnd, dwVerInfoSize, lpstrVffInfo)) {
      free (lpstrVffInfo);
      return 0;    // error
   }

   VS_FIXEDFILEINFO *lpInfo;
   lpInfo = NULL;
   //DWORD  dw;
   UINT ui;
   VerQueryValue (lpstrVffInfo, "\\", (LPVOID*)&lpInfo, &ui); 
   if (lpInfo) {
      iVersion = ((__int64) lpInfo->dwFileVersionMS) << 32 |
         (__int64) lpInfo->dwFileVersionLS;
   }

#if 0
   //lstrcpy(szGetName, "\\StringFileInfo\\040904B0\\ProductVersion");
   lstrcpy(szGetName, "\\StringFileInfo\\040904e4\\ProductVersion");
   wRootLen = lstrlen(szGetName);

   // Walk through the dialog items that we want to replace:
   uVersionLen   = 0;
   lpVersion     = NULL;
   bRetCode      =  0;

   bRetCode = VerQueryValue((LPVOID)lpstrVffInfo,
     (LPSTR)szGetName,
     (void**)&lpVersion,
     (unsigned int*)&uVersionLen); // For MIPS strictness

   if ( bRetCode && uVersionLen && lpVersion) {
      // get the version info
      DWORD i;
      char *pc = lpVersion;
      for (i = 48; i <= 48; i-= 16) {
         __int64  iCur;
         iCur = 0;
         while ((*pc >= '0') && (*pc <= '9')) {
            iCur = (iCur * 10) + (*pc - '0');
            pc++;
         }
         // if not NULL go on past the separator
         if (*pc)
            pc++;

         // get version number
         iVersion = iVersion | (iCur << i);
      }
   }
#endif // 0
   free (lpstrVffInfo);

   return iVersion;
}

/*****************************************************************************
VersionCheck - Check the version at the dest <= src

inputs
   char     *pszDest - Destination file
   char     *pszSrc - Source file
returns
   TRUE - it's ok to copy
*/
BOOL VersionCheck (char *pszDest, PVOID pSrcData, DWORD dwSrcSize)
{
   __int64 iVersionDest, iVersionSrc;
   iVersionDest = MyGetFileVersion (pszDest);
   if (!iVersionDest)
      return TRUE;   // nothing at the destination

   // write out the source to a temp
   char szDir[256], szFile[256];
   szDir[0] = szFile[0] = 0;
   GetTempPath (sizeof(szDir), szDir);
   GetTempFileName (szDir, "mxa", 0, szFile);
   // write
   FILE  *f;
   f = fopen (szFile, "wb");
   iVersionSrc = 0;
   if (f) {
      fwrite (pSrcData, 1, dwSrcSize, f);
      fclose (f);
      iVersionSrc = MyGetFileVersion (szFile);
      DeleteFile (szFile);
   }
   // don't copy over if same or older version
   if (iVersionSrc < iVersionDest)  // BUGFIX - Was <=
      return FALSE;

   // return diff
   return TRUE;
}


/******************************************************************************
PieLocation - Location of the pie chart.

inputs
   HWND           hWnd - Main window
   RECT           *prPie - Filled with the pie chart location, in client. Can be NULL
   POINT          *ppCenter - Filled with center. Can be NULL
   int            *piRadius - Filled with radius
returns
   none
*/
void PieLocation (HWND hWnd, RECT *prPie, POINT *ppCenter, int *piRadius)
{
   RECT r;
   GetClientRect (hWnd, &r);

   int iRadius = min(r.right - r.left, r.bottom - r.top) / 10;
   POINT pCenter;
   pCenter.x = (r.right + r.left) / 2;
   pCenter.y = (r.bottom + r.top) / 2;

   if (prPie) {
      prPie->left = pCenter.x - iRadius;
      prPie->right = pCenter.x + iRadius;
      prPie->top = pCenter.y - iRadius;
      prPie->bottom = pCenter.y + iRadius;
   }

   if (ppCenter)
      *ppCenter = pCenter;

   if (piRadius)
      *piRadius = iRadius;
}


/******************************************************************************
MainWndProc
*/
long CALLBACK MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

   switch (uMsg) {
   case WM_CREATE:
      PostMessage (hWnd, WM_USER+82, 0, 0);
      return 0;


   case WM_USER+82:  // license
      if (IDOK != DialogBox (ghInstance, MAKEINTRESOURCE(IDD_LICENSE),
         hWnd, LicenseDialogProc))
            DestroyWindow (hWnd);
      else
         PostMessage (hWnd, WM_USER+83, 0, 0);
      return 0;

   case WM_USER+83:  // directory
      if (IDOK != DialogBox (ghInstance, MAKEINTRESOURCE(IDD_DIRECTORY),
         hWnd, DirectoryDialogProc))
            DestroyWindow (hWnd);
      else
         PostMessage (hWnd, WM_USER+84, 0, 0);
      return 0;

   case WM_USER+84:  // install files
      {
         DWORD dwIndex;
         CChunk   *p, *p2;

         // install the uninstall
         // uninstall file name
         char  szUninstall[256];
         GetWindowsDirectory (szUninstall, sizeof(szUninstall));
         strcat (szUninstall, "\\mXacUninstall.exe");
         p = gChunk.SubChunkGet (gChunk.SubChunkFind(2));
         if (p) {
            // write
            FILE  *f;
            f = fopen (szUninstall, "wb");
            if (f) {
               fwrite (p->DataGet(), 1, p->DataSize(), f);
               fclose (f);
            }

            // dont uninstall this
         }

         // write the uninstall registry
         HKEY  hInfo = NULL;
         DWORD dwDisp;
         char  szUnReg[512];
         sprintf (szUnReg, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s", gszTitle);
         RegCreateKeyEx (HKEY_LOCAL_MACHINE, szUnReg, 0, 0, REG_OPTION_NON_VOLATILE,
            KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
         fprintf (gfUninstall, "regkey=%s\n", szUnReg);
         if (hInfo) {
            strcat (szUninstall, " \"");
            strcat (szUninstall, gszUninstallFile);
            strcat (szUninstall, "\"");
            RegSetValueEx (hInfo, "DisplayName", 0, REG_SZ, (BYTE*) gszTitle, strlen(gszTitle)+1);
            RegSetValueEx (hInfo, "UninstallString", 0, REG_SZ, (BYTE*) szUninstall, strlen(szUninstall)+1);
            RegCloseKey (hInfo);
         }

         dwIndex = 0;
         while (TRUE) {
            dwIndex = gChunk.SubChunkFind (100, dwIndex);  // 100 => normal file

            // redraw the pie
            RECT rPie;
            gfPie = (double)min(dwIndex, gChunk.SubChunkNum()) / (double)max(gChunk.SubChunkNum (), 1);
            PieLocation (hWnd, &rPie, NULL, NULL);
            InvalidateRect (hWnd, &rPie, FALSE);
            UpdateWindow (hWnd);

            if (dwIndex == (DWORD)-1)
               break;

            // get it
            p = gChunk.SubChunkGet (dwIndex);
            if (!p)
               goto next;
            p2 = p->SubChunkGet(p->SubChunkFind(100));
            if (!p2)
               goto next;
            char  *pszFile, *pszDir;
            pszDir = pszFile = (char*) p2->DataGet();
            if (!pszFile)
               goto next;
            // go to file name
            while (strchr(pszFile, '\\'))
               pszFile = strchr(pszFile, '\\') + 1;

            // create the directory
            char  szDir[256];
            szDir[0] = 0;
            if (pszFile > pszDir) {
               char  c;
               c = pszFile[-1];
               pszFile[-1] = 0;

               // remember
               strcpy (szDir, pszDir);

               pszFile[-1] = c;
            }

            // if it has a $ in front of it it's special
            BOOL fSystemDir;
            fSystemDir = FALSE;
            if (!_stricmp(szDir, "$system"))
               fSystemDir = TRUE;

            // create the diretory
            char  szTemp[512];
            if (!fSystemDir && szDir[0]) {
               strcpy (szTemp, gszAppDir);
               strcat (szTemp, "\\");
               strcat (szTemp, szDir);
               CreateDirectory (szTemp, NULL);
            }
            // where write to?
            if (fSystemDir) {
               GetWindowsDirectory (szTemp, sizeof(szTemp));
               strcat (szTemp, "\\system\\");
            }
            else {
               strcpy (szTemp, gszAppDir);
               strcat (szTemp, "\\");
               if (szDir[0]) {
                  strcat (szTemp, szDir);
                  strcat (szTemp, "\\");
               }
            }
            strcat (szTemp, pszFile);

            // if installing into system directory do version checks
            if (fSystemDir) {
               if (!VersionCheck (szTemp, p->DataGet(),p->DataSize()))
                  goto next;
            }
            else if (strstr (szTemp, ".exe")) {
               // BUGIFX - if it's an exe to a version cehcke
               if (!VersionCheck (szTemp, p->DataGet(), p->DataSize())) {
                  char  szTemp2[1024];
                  sprintf (szTemp2, "You already have a newer version of %s "
                     "installed. Setup will stop installing now. If you still want to install "
                     "the older version, uninstall the software and then reinstall.", szTemp);
                  MessageBox (hWnd, szTemp2, NULL, MB_OK);

                  DestroyWindow (hWnd);
                  return 0;
               }
            }

            // write
            FILE  *f;
            f = fopen (szTemp, "wb");
            if (!f) {
               if (fSystemDir) {
                  // dont report copy error
                  goto next;
               }

               char  szTemp2[512];
               sprintf (szTemp2, "Can't write the file %s. Please choose a different directory.", szTemp);
               MessageBox (hWnd, szTemp2, NULL, MB_OK);
               PostMessage (hWnd, WM_USER+83, 0, 0);
               return 0;
            }

            if (p->DataSize() != fwrite (p->DataGet(), 1, p->DataSize(), f)) {
               char  szTemp2[512];
               sprintf (szTemp2, "Can't write the file %s. You may be out of disk space. Please choose a different disk or free up some space.", szTemp);
               MessageBox (hWnd, szTemp2, NULL, MB_OK);
               PostMessage (hWnd, WM_USER+83, 0, 0);
               return 0;
            }
            fclose (f);

            // note that have written this
            // dont uninstall from the system
            if (!fSystemDir && gfUninstall) {
               fprintf (gfUninstall, "file=%s\n", szTemp);
               if (szDir[0])
                  fprintf (gfUninstall, "dir=%s\\%s\n", gszAppDir, szDir);
            }


next:
            // continue on
            dwIndex++;
         }

         //  create icons
         char  szShort[256];
         DWORD dwCur;
         dwCur = 0;
         while (InfFind ("icon=", szShort, dwCur, &dwCur)) {
            char  *pc;
            pc = strchr (szShort, ',');
            if (pc) {
               *pc = 0;
               pc++;
            }
            else
               pc = szShort;

            Shortcut (szShort, pc);
         }

         // write out files to delete
         char  szDel[512];
         dwCur = 0;
         while (InfFind ("del=", szDel, dwCur, &dwCur)) {
            fprintf (gfUninstall, "file=%s\\%s\n", gszAppDir, szDel);
         }

         // write out the files to run during uninstall
         dwCur = 0;
         while (InfFind ("uninstallexe=", szDel, dwCur, &dwCur)) {
            fprintf (gfUninstall, "uninstallexe=%s\n", szDel);
         }

         // uninstall for main dir
         if (gfUninstall) {
            fprintf (gfUninstall, "dir=%s\n", gszAppDir);
         }

         // run the applications
         dwCur = 0;
         while (InfFind ("installexe=", szDel, dwCur, &dwCur)) {
            STARTUPINFO si;
            PROCESS_INFORMATION  pi;
            memset (&si, 0, sizeof(si));
            si.cb = sizeof(si);
            memset (&pi, 0, sizeof(pi));
            CreateProcess (
               NULL, szDel,
               NULL, NULL,
               FALSE,
               0,
               NULL, // environment
               NULL,
               &si,
               &pi
               );
         }

         // potentially autoexecute
         dwCur = 0;
         if (InfFind("selfexeauto=", szDel, dwCur, &dwCur)) {
            RunSelfExe (hWnd);
            DestroyWindow (hWnd);
            return 0;
         }

         // goto done
         PostMessage (hWnd, WM_USER+85, 0, 0);
      }
      return 0;

   case WM_USER+85:  // done
      {
         // ask user if wishes to run app
         DialogBox (ghInstance, MAKEINTRESOURCE(IDD_COMPLETE),
            hWnd, CompleteDialogProc);

         // all done
         DestroyWindow (hWnd);
      }
      return 0;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC   hDC;
         hDC = BeginPaint (hWnd, &ps);

         HBRUSH   hBrush, hBrushOld;
         RECT     r;
         GetClientRect (hWnd, &r);
#define  GRADIENT 32
         DWORD i;
         for (i = 0; i < GRADIENT; i++) {
            RECT  r2;
            r2 = r;
            r2.top = r.bottom * i / GRADIENT;
            r2.bottom = r.bottom * (i+1) / GRADIENT;

            hBrush = CreateSolidBrush (RGB(0, 0, 128 + 128 / GRADIENT * i));
            hBrushOld = (HBRUSH) SelectObject (hDC, hBrush);
            FillRect (hDC, &r2, hBrush);
            SelectObject (hDC, hBrushOld);
            DeleteObject (hBrush);
         }

         // paint the pie
         if (gfPie) {
            HBRUSH hbrPie = CreateSolidBrush (RGB(0xff, 0x40, 0x40));
            HBRUSH hbrOld = (HBRUSH)SelectObject (hDC, hbrPie);
            HPEN hPen = CreatePen (PS_SOLID, 0, RGB(0, 0, 0));
            HPEN hPenOld = (HPEN)SelectObject (hDC, hPen);

            RECT rPie;
            POINT pCenter;
            int iRadius;
            PieLocation (hWnd, &rPie, &pCenter, &iRadius);

            double fRadius = iRadius;
            POINT pStart;
            pStart.x = (int)(sin(gfPie * 2.0 * PI) * fRadius) + pCenter.x;
            pStart.y = (int)(-cos(gfPie * 2.0 * PI) * fRadius) + pCenter.y;

            if (gfPie < 0.99)
               Pie (hDC, rPie.left, rPie.top, rPie.right, rPie.bottom,
                  pStart.x, pStart.y,
                  pCenter.x, r.top);
            else
               Ellipse (hDC, rPie.left, rPie.top, rPie.right, rPie.bottom);

            SelectObject (hDC, hPenOld);
            SelectObject (hDC, hbrOld);
            DeleteObject (hbrPie);
            DeleteObject (hPen);
         }

         // BUGFIX - So can display "gridart" in setup
         BOOL fmXac;
         fmXac = !_stricmp(gpszmXac, gszMfg);

         // paint the app title
         HFONT hfOld, hf, hfBig, hfSymbol;
         LOGFONT  lf;
         RECT  rDraw;
         memset (&lf, 0, sizeof(lf));
         lf.lfHeight = -64 * GetSystemMetrics(SM_CXSCREEN) / 1024; 
         lf.lfCharSet = DEFAULT_CHARSET;
         lf.lfPitchAndFamily = FF_SWISS;
         lf.lfWeight = FW_BOLD;
         strcpy (lf.lfFaceName, "Arial");
         hf = CreateFontIndirect (&lf);
         if (fmXac)
            lf.lfHeight  *= 2;
         hfBig = CreateFontIndirect (&lf);
         lf.lfHeight = (int) (lf.lfHeight / 2.0 * 1.2);
         lf.lfCharSet = FF_DECORATIVE;
         strcpy (lf.lfFaceName, "Symbol");
         hfSymbol = CreateFontIndirect (&lf);

         hfOld = (HFONT) SelectObject (hDC, hf);
         SetTextColor (hDC, RGB(255, 255, 255));
         SetBkMode (hDC, TRANSPARENT);

         // chapter
         r.right -= GetSystemMetrics (SM_CXSCREEN) / 50;
         rDraw = r;
         SelectObject (hDC, hf);
         DrawText(hDC, gszTitle, -1, &rDraw, DT_RIGHT | DT_TOP | DT_SINGLELINE);

         // want to italicize m, and center X?
         RECT  rSmall, rLarge, rSymbol;
         rSmall = r;
         rLarge = r;
         rSymbol = r;
         if (fmXac) {
            SelectObject (hDC, hf);
            DrawText(hDC, "ac", -1, &rSmall, DT_SINGLELINE | DT_CALCRECT);
            SelectObject (hDC, hfBig);
            DrawText(hDC, "X", -1, &rLarge, DT_SINGLELINE | DT_CALCRECT);
            SelectObject (hDC, hfSymbol);
            DrawText(hDC, "m", -1, &rSymbol, DT_SINGLELINE | DT_CALCRECT);
         }
         else {
            SelectObject (hDC, hfBig);
            DrawText(hDC, gszMfg, -1, &rLarge, DT_SINGLELINE | DT_CALCRECT);
            memset (&rSmall, 0, sizeof(rSmall));
            memset (&rSymbol, 0, sizeof(rSymbol));
         }

         // height
         int   iHeight, iXWidth;
         iHeight = rSmall.bottom - rSmall.top;
         iHeight = max(iHeight, rLarge.bottom - rLarge.top);
         iHeight = max(iHeight, rSymbol.bottom - rSymbol.top);
         iXWidth = rLarge.right - rLarge.left;

         SelectObject (hDC, hf);
         rDraw = r;
         rDraw.top = rDraw.bottom - iHeight;
         rDraw.bottom -= iHeight / 8;
         rDraw.left = rDraw.right - (rSmall.right - rSmall.left) - iXWidth / 8;
         if (fmXac) {
            DrawText(hDC, "ac", -1, &rDraw, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            rDraw.left += iXWidth / 8;
         }

         SelectObject (hDC, hfBig);
         rDraw.bottom = r.bottom;
         rDraw.right = rDraw.left;
         rDraw.left = rDraw.right - (rLarge.right - rLarge.left);
         if (fmXac)
            DrawText(hDC, "X", -1, &rDraw, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
         else {
            if (gfShowMfg)
               DrawText(hDC, gszMfg, -1, &rDraw, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
         }

         SelectObject (hDC, hfSymbol);
         rDraw.bottom -= (iHeight / 8 + iHeight / 16);
         rDraw.right = rDraw.left + iXWidth / 8;
         rDraw.left = rDraw.right - (rSymbol.right - rSymbol.left);
         if (fmXac)
            DrawText(hDC, "m", -1, &rDraw, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

         SelectObject (hDC, hfOld);
         DeleteObject (hf);
         DeleteObject (hfBig);
         DeleteObject (hfSymbol);
         


         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_DESTROY:
      if (gfUninstall)
         fclose (gfUninstall);
      PostQuitMessage (0);
      // that's all
      return 0;
   }

   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/****************************************************************************8
*/
void RegisterWindow (void)
{
   WNDCLASS wc;

   memset (&wc, 0, sizeof(wc));
   wc.lpfnWndProc = MainWndProc;
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hInstance = ghInstance;
   wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_APPICON));
   wc.hCursor = LoadCursor (NULL, IDC_NO);
   wc.hbrBackground = NULL;
   wc.lpszMenuName = NULL;
   wc.lpszClassName = gszMainClass;
   RegisterClass (&wc);
}


int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nShowCmd)
{
   ghInstance = hInstance;

   CoInitialize (NULL);
   // register class
   RegisterWindow ();

   // load in the compressed data
   HRSRC    hr;
   hr = FindResource (ghInstance, MAKEINTRESOURCE (IDR_DATA), "bin");

   HGLOBAL  hg;
   hg = LoadResource (ghInstance, hr);

   PVOID pMem;
   pMem = LockResource (hg);

   DWORD dwSize;
   dwSize = SizeofResource (ghInstance, hr);

   CEncFile cFile;
   cFile.OpenMem (pMem, dwSize);
   gChunk.Read (&cFile);

   // find the inf chunk
   CChunk   *pInf;
   pInf = gChunk.SubChunkGet (gChunk.SubChunkFind(1));
   gpszInf = (char*)pInf->DataGet ();
   gdwInfSize = pInf->DataSize();


   // get the name
   InfFind ("appname=", gszAppName);
   if (!InfFind ("title=", gszTitle))
      strcpy (gszTitle, gszAppName);
   if (!InfFind ("mfg=", gszMfg))
      strcpy (gszMfg, gpszmXac);
   char szShowMfg[256];
   szShowMfg[0] = 0;
   InfFind ("showmfg=", szShowMfg);
   if (szShowMfg[0] == '0')
      gfShowMfg = FALSE;


   // create the window
   // make sure the window is created in specific location
   RECT  r;
   SystemParametersInfo (SPI_GETWORKAREA, 0, (PVOID) &r, 0);   

   CreateWindowEx (
      WS_EX_APPWINDOW,
      gszMainClass, "Setup",
      /*WS_OVERLAPPEDWINDOW | */ WS_POPUP | WS_VISIBLE,
      r.left, r.top, (r.right - r.left), r.bottom - r.top, 
      NULL, NULL,
      ghInstance, NULL);

   // Message loop
   MSG msg;
   while (GetMessage (&msg, NULL, 0, 0)) {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
   }


   CoUninitialize ();


   return 0;
}
