/*************************************************************************************
Main.cpp - Entry code for the M3D wave.

begun 28/2/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <direct.h>
#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <commctrl.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "resource.h"
#include "..\..\m3d\M3D.h"
#include "..\buildnum.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "mifserver.h"


// POD - Info for project open dialog
typedef struct {
   WCHAR       szName[256];      // name of the project
   PCMIFLProj  pProj;            // filled with new project
   BOOL        fModify;          // set to TRUE if want tmodify
   BOOL        fRunCircumreality;          // if TRUE then user has selected Circumreality file to run
   HKEY        hKey;             // regitry key
   HKEY        hClientKey;       // for client registry
   BOOL        fEnableClientTools;  // if TRUE then enable client tools
} POD, *PPOD;



HINSTANCE      ghInstance;
char           gszAppDir[256];
char           gszAppPath[256];     // application path
WCHAR          gszProjectFile[256] = L""; // curent file that working on. Only if it's a Circumreality
CMem           gMemTemp; // temporary memoty
CMIFLSocketServer gMIFLSocket;  // callbakc
PCMIFLProj     gpMIFLProj = NULL;    // project
char *gpszServerKey = "Software\\mXac\\CircumrealityWorldSim";
char *gpszClientKey = "Software\\mXac\\Circumreality";
BOOL           gfM3DInit = FALSE;   // set if 3D initialized
DWORD          gdwCmdLineParam = 0; // what is sent into command line paramter
BOOL           gfRestart = FALSE;    // if TRUE then will restart the application on a shutdown
static PWSTR   gpszDiffToDelete = L"$$$TODELETE$$$";

/*************************************************************************
IsFileCircumreality - CHecks to see if the file is a Circumreality file.

inputs
   PWSTR       psz - String to check
returns
   BOOL - TRUE if ends in .crf
*/
BOOL IsFileCircumreality (PWSTR psz)
{
   PWSTR pszCircumreality = L".crf";
   DWORD dwLenCircumreality = (DWORD)wcslen(pszCircumreality);
   DWORD dwLen = (DWORD)wcslen(psz);
   if (dwLen < dwLenCircumreality)
      return FALSE;

   return !_wcsicmp (psz + (dwLen - dwLenCircumreality), pszCircumreality);
}

/*************************************************************************
IsFileMFP - CHecks to see if the file is a MFP file.

inputs
   PWSTR       psz - String to check
returns
   BOOL - TRUE if ends in .mfp
*/
BOOL IsFileMFP (PWSTR psz)
{
   PWSTR pszCircumreality = L".mfp";
   DWORD dwLenCircumreality = (DWORD)wcslen(pszCircumreality);
   DWORD dwLen = (DWORD)wcslen(psz);
   if (dwLen < dwLenCircumreality)
      return FALSE;

   return !_wcsicmp (psz + (dwLen - dwLenCircumreality), pszCircumreality);
}

/*************************************************************************
OpenPage
*/
BOOL OpenPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPOD ppod = (PPOD)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"filename");
         if (pControl)
            pControl->AttribSet (Text(), ppod->szName);

         if (pControl = pPage->ControlFind (L"enableclient"))
            pControl->AttribSetBOOL (Checked(), ppod->fEnableClientTools);

         DoubleToControl (pPage, L"cmdlineparam", gdwCmdLineParam);

         pPage->Message (ESCM_USER+93);   // to update buttons
      }
      break;

   case ESCM_USER+93:   // to enable the buttons
      {
         PCEscControl pControl;
         if (pControl = pPage->ControlFind (L"openmodify"))
            pControl->Enable (IsFileMFP(ppod->szName));
         if (pControl = pPage->ControlFind (L"openrun"))
            pControl->Enable (IsFileMFP(ppod->szName) || IsFileCircumreality(ppod->szName));
      }
      return 0;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"filename")) {
            DWORD dwNeeded;
            p->pControl->AttribGet (Text(), ppod->szName, sizeof(ppod->szName), &dwNeeded);

            pPage->Message (ESCM_USER+93);   // to update buttons

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"cmdlineparam")) {
            gdwCmdLineParam = (DWORD)DoubleFromControl (pPage, psz);
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

         if (!_wcsicmp(psz, L"dialog")) {
            if (!MIFLProjOpenDialog (pPage->m_pWindow->m_hWnd, ppod->szName, sizeof(ppod->szName)/sizeof(WCHAR), FALSE, TRUE))
               return TRUE;

            // set the text
            PCEscControl pControl = pPage->ControlFind (L"filename");
            if (pControl)
               pControl->AttribSet (Text(), ppod->szName);

            pPage->Message (ESCM_USER+93);   // to update buttons

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"enableclient")) {
            ppod->fEnableClientTools = p->pControl->AttribGetBOOL (Checked());
            RegSetValueEx (ppod->hClientKey, "EnableClientTools", 0, REG_DWORD, (LPBYTE)&ppod->fEnableClientTools, sizeof(ppod->fEnableClientTools));
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"openmodify") || !_wcsicmp(psz, L"openrun")) {
            BOOL fModify = !_wcsicmp(psz, L"openmodify");
            BOOL fIsCircumreality = IsFileCircumreality (ppod->szName);

            if (fModify && fIsCircumreality) {
               pPage->MBWarning (L"You can't modify a compiled Circumreality file.",
                  L"You can only Open (and run) a compiled Circumreality file.");
               return TRUE;
            }

            if (fIsCircumreality) {
               // set flag so know to run
               ppod->fRunCircumreality = TRUE;
            }
            else {
               ppod->pProj = new CMIFLProj (&gMIFLSocket);
               if (!ppod->pProj)
                  return TRUE;
               if (!ppod->pProj->Open (ppod->szName, pPage->m_pWindow->m_hWnd)) {
                  delete ppod->pProj;
                  ppod->pProj = NULL;

                  pPage->MBWarning (L"The file couldn't be opened");

                  return TRUE;
               }
               ppod->pProj->m_fTransProsQuick = TRUE;
            }

            ppod->fModify = fModify;
            pPage->Exit (L"done");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"new")) {
            if (!MIFLProjOpenDialog (pPage->m_pWindow->m_hWnd, ppod->szName, sizeof(ppod->szName)/sizeof(WCHAR), TRUE))
               return TRUE;

            ppod->pProj = new CMIFLProj (&gMIFLSocket);
            if (!ppod->pProj)
               return TRUE;
            ppod->pProj->LibraryAddDefault();
            ppod->pProj->m_fTransProsQuick = TRUE;

            // save just in case
            wcscpy (ppod->pProj->m_szFile, ppod->szName);
            if (!ppod->pProj->Save (TRUE)) {
               delete ppod->pProj;
               ppod->pProj = NULL;
               pPage->MBWarning (L"The new project couldn't be created because it couldn't be saved.");
               return TRUE;
            }


            // edit
            ppod->fModify = TRUE;
            pPage->Exit (L"done");
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Circumreality World Simulator";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CircumrealityOpenDialog - Dialog box for opening a .crf file

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL CircumrealityOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
{
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
   memset (&ofn, 0, sizeof(ofn));
   
   // BUGFIX - Set directory
   char szInitial[256];
   strcpy (szInitial, gszAppDir);
   GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "Compiled Circumreality Project(*.crf)\0*.crf\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save the Circumreality File" :
      "Open Circumreality File";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = "crf";
   // nFileExtension 

   if (fSave) {
      if (!GetSaveFileName(&ofn))
         return FALSE;
   }
   else {
      if (!GetOpenFileName(&ofn))
         return FALSE;
   }

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // copy over
   MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, pszFile, dwChars);
   return TRUE;
}


/*************************************************************************************
DIFOpenDialog - Dialog box for opening a .dif file

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL DIFOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
{
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
   memset (&ofn, 0, sizeof(ofn));
   
   // BUGFIX - Set directory
   char szInitial[256];
   strcpy (szInitial, gszAppDir);
   GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "Difference file(*.dif)\0*.dif\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save the DIF File" :
      "Open DIF File";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = "dif";
   // nFileExtension 

   if (fSave) {
      if (!GetSaveFileName(&ofn))
         return FALSE;
   }
   else {
      if (!GetOpenFileName(&ofn))
         return FALSE;
   }

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // copy over
   MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, pszFile, dwChars);
   return TRUE;
}


/*************************************************************************
DiffPage
*/
BOOL DiffPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPOD ppod = (PPOD)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // read the strings from the registry
         WCHAR szTemp[256];
         PCEscControl pControl;
         DWORD dwSize, dwType;
         szTemp[0] = 0;
         dwSize = sizeof(szTemp);
         RegQueryValueExW (ppod->hKey, L"DiffNew", 0, &dwType, (LPBYTE)szTemp, &dwSize);
         if (pControl = pPage->ControlFind (L"new"))
            pControl->AttribSet (Text(), szTemp);

         szTemp[0] = 0;
         dwSize = sizeof(szTemp);
         RegQueryValueExW (ppod->hKey, L"DiffOld", 0, &dwType, (LPBYTE)szTemp, &dwSize);
         if (pControl = pPage->ControlFind (L"old"))
            pControl->AttribSet (Text(), szTemp);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"old") || !_wcsicmp(psz, L"new")) {
            DWORD dwOld = _wcsicmp(psz, L"old") ? 0 : 1;

            DWORD dwNeeded;
            WCHAR szTemp[256];
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);

            // write to registry
            RegSetValueExW (ppod->hKey, dwOld ? L"DiffOld" : L"DiffNew", 0, REG_SZ, (LPBYTE)szTemp, ((DWORD)wcslen(szTemp)+1)*sizeof(WCHAR));

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

         if (!_wcsicmp(psz, L"oldbrowse") || !_wcsicmp(psz, L"newbrowse")) {
            DWORD dwOld = _wcsicmp(psz, L"oldbrowse") ? 0 : 1;
            PWSTR pszControl = dwOld ? L"old" : L"new";
            WCHAR szTemp[256];
            PCEscControl pControl;
            DWORD dwNeeded;
            szTemp[0] = 0;
            if (pControl = pPage->ControlFind (pszControl))
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);

            if (!CircumrealityOpenDialog (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), FALSE))
               return TRUE;

            // set the text
            pControl->AttribSet (Text(), szTemp);

            // write the registry

            pPage->Message (ESCM_USER+93);   // to update buttons
            RegSetValueExW (ppod->hKey, dwOld ? L"DiffOld" : L"DiffNew", 0, REG_SZ, (LPBYTE)szTemp, ((DWORD)wcslen(szTemp)+1)*sizeof(WCHAR));

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"convert")) {
            // open the two files
            CMegaFile mfOld, mfNew, mfDiff;
            WCHAR szTemp[256];
            PCEscControl pControl;
            DWORD dwNeeded;

            // load in old
            szTemp[0] = 0;
            if (pControl = pPage->ControlFind (L"old"))
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            if (!mfOld.Init (szTemp, &GUID_MegaCircumreality, FALSE)) {
               pPage->MBWarning (
                  L"Couldn't open the \"old\" file.",
                  L"Make sure you have the correct path for the file.");
               return TRUE;
            }

            // load in new
            szTemp[0] = 0;
            if (pControl = pPage->ControlFind (L"new"))
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            if (!mfNew.Init (szTemp, &GUID_MegaCircumreality, FALSE)) {
               pPage->MBWarning (
                  L"Couldn't open the \"new\" file.",
                  L"Make sure you have the correct path for the file.");
               return TRUE;
            }

            // get the dif file to save it as
            if (!DIFOpenDialog (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), TRUE))
               return TRUE;

            // create it
            DeleteFileW (szTemp); // so its deleted first
            if (!mfDiff.Init (szTemp, &GUID_MegaCircumreality, TRUE)) {
               pPage->MBWarning (
                  L"Couldn't create the .DIF file.",
                  L"Make sure you have the correct path for the file.");
               return TRUE;
            }

            // enumerate all the files in the new one
            CListVariable lName;
            CListFixed lMFFILEINFO;
            MFFILEINFO mffiOld;
            lMFFILEINFO.Init (sizeof(MFFILEINFO));
            mfNew.Enum (&lName, &lMFFILEINFO);
            DWORD i;
            PMFFILEINFO pmf = (PMFFILEINFO) lMFFILEINFO.Get(0);
            for (i = 0; i < lName.Num(); i++, pmf++) {
               PWSTR psz = (PWSTR)lName.Get(i);

               // see if it exists in the old one, and if there's a difference
               BOOL fExists = mfOld.Exists (psz, &mffiOld);
               if (!fExists || (mffiOld.iDataSize != pmf->iDataSize) ||
                  CompareFileTime(&mffiOld.iTimeCreate, &pmf->iTimeCreate) || CompareFileTime(&mffiOld.iTimeModify, &pmf->iTimeModify)) {
                     // copy it
                     __int64 iSize, iSizeOld;
                     PVOID pLoad = mfNew.Load (psz, &iSize);
                     PVOID pLoadOld = NULL;
                     if (fExists && pLoad && (mffiOld.iDataSize == pmf->iDataSize)) {
                        pLoadOld = mfOld.Load(psz, &iSizeOld);
                        if (pLoadOld && !memcmp(pLoadOld, pLoad, iSizeOld)) {
                           // the two files have different dates, but the same size, so dont bother updating
                           if (pLoad)
                              MegaFileFree (pLoad);
                           pLoad = NULL;
                        }
                        if (pLoadOld)
                           MegaFileFree (pLoadOld);
                     }
                     if (pLoad) {
                        mfDiff.Save (psz, pLoad, iSize, &pmf->iTimeCreate, &pmf->iTimeModify, &pmf->iTimeAccess);
                        MegaFileFree (pLoad);

#ifdef _DEBUG
                        swprintf (szTemp, L"\r\n%s : %d %x-%x, %x-%x %x-%x",
                           psz, (int)iSize,
                           (int)pmf->iTimeCreate.dwHighDateTime, (int)pmf->iTimeCreate.dwLowDateTime,
                           (int)pmf->iTimeModify.dwHighDateTime, (int)pmf->iTimeModify.dwLowDateTime,
                           (int)pmf->iTimeAccess.dwHighDateTime, (int)pmf->iTimeAccess.dwLowDateTime
                           );
                        OutputDebugStringW (szTemp);
#endif
                     }
                  };
            } // i

            // enumerate all the old files and see if they're no longer on the new
            lName.Clear();
            lMFFILEINFO.Clear();
            mfOld.Enum (&lName, &lMFFILEINFO);
            for (i = 0; i < lName.Num(); i++) {
               PWSTR psz = (PWSTR)lName.Get(i);

               if (!mfNew.Exists (psz))
                  // and old file no longer exists, so mark as "to delete"
                  mfDiff.Save (psz, gpszDiffToDelete, (wcslen(gpszDiffToDelete)+1)*sizeof(WCHAR));
            } // i

            // done
            pPage->MBInformation (L"The .DIF file has been created.");

            pPage->Exit (Back());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Difference between two .crf files";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************
MonitorPage
*/
BOOL MonitorPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWORLDMONITOR pWM = (PWORLDMONITOR) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         WCHAR szTemp[256];
         DWORD i;
         for (i = 0; i < NUMWORLDSTOMONITOR; i++) {
            swprintf (szTemp, L"filename%d", (int)i);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSet (Text(), pWM->aszWorldFile[i]);

            swprintf (szTemp, L"shardnum%d", (int)i);
            DoubleToControl (pPage, szTemp, pWM->adwShardNum[i]);
         } // i

         if (pControl = pPage->ControlFind (L"domain"))
            pControl->AttribSet (Text(), pWM->szDomain);
         if (pControl = pPage->ControlFind (L"SMTPServer"))
            pControl->AttribSet (Text(), pWM->szSMTPServer);
         if (pControl = pPage->ControlFind (L"EmailTo"))
            pControl->AttribSet (Text(), pWM->szEMailTo);
         if (pControl = pPage->ControlFind (L"EmailFrom"))
            pControl->AttribSet (Text(), pWM->szEMailFrom);
         if (pControl = pPage->ControlFind (L"AuthUser"))
            pControl->AttribSet (Text(), pWM->szAuthUser);
         if (pControl = pPage->ControlFind (L"AuthPassword"))
            pControl->AttribSet (Text(), pWM->szAuthPassword);
         if (pControl = pPage->ControlFind (L"emailonsuccess"))
            pControl->AttribSetBOOL (Checked(), pWM->fEmailOnSuccess);

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR szShardNum = L"shardnum";
         DWORD dwShardNumLen = (DWORD)wcslen(szShardNum);
         DWORD dwNeeded;

         if (!_wcsnicmp(psz, szShardNum, dwShardNumLen)) {
            DWORD dwIndex = _wtoi(psz + dwShardNumLen);

            pWM->adwShardNum[dwIndex] = (DWORD) DoubleFromControl (pPage, psz);

            return TRUE;
         }


         if (!_wcsicmp(psz, L"domain")) {
            p->pControl->AttribGet (Text(), pWM->szDomain, sizeof(pWM->szDomain), &dwNeeded);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"SMTPServer")) {
            p->pControl->AttribGet (Text(), pWM->szSMTPServer, sizeof(pWM->szSMTPServer), &dwNeeded);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"EmailTo")) {
            p->pControl->AttribGet (Text(), pWM->szEMailTo, sizeof(pWM->szEMailTo), &dwNeeded);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"EmailFrom")) {
            p->pControl->AttribGet (Text(), pWM->szEMailFrom, sizeof(pWM->szEMailFrom), &dwNeeded);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"AuthUser")) {
            p->pControl->AttribGet (Text(), pWM->szAuthUser, sizeof(pWM->szAuthUser), &dwNeeded);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"AuthPassword")) {
            p->pControl->AttribGet (Text(), pWM->szAuthPassword, sizeof(pWM->szAuthPassword), &dwNeeded);
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

         PWSTR szDialog = L"dialog";
         DWORD dwDialogLen = (DWORD)wcslen(szDialog);

         if (!_wcsnicmp(psz, szDialog, dwDialogLen)) {
            DWORD dwIndex = _wtoi(psz + dwDialogLen);

            if (!CircumrealityOpenDialog(pPage->m_pWindow->m_hWnd, pWM->aszWorldFile[dwIndex],
               sizeof(pWM->aszWorldFile[dwIndex])/sizeof(WCHAR), FALSE))
               pWM->aszWorldFile[dwIndex][0] = 0;  // so cancel

            WCHAR szTemp[256];
            swprintf (szTemp, L"filename%d", (int)dwIndex);
            PCEscControl pControl;
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSet (Text(), pWM->aszWorldFile[dwIndex]);

            return TRUE;
         }

         if (!_wcsicmp(psz, L"emailonsuccess")) {
            pWM->fEmailOnSuccess = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Monitoring settings";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************
DiffIncPage
*/
BOOL DiffIncPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPOD ppod = (PPOD)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // read the strings from the registry
         WCHAR szTemp[256];
         PCEscControl pControl;
         DWORD dwSize, dwType;
         szTemp[0] = 0;
         dwSize = sizeof(szTemp);
         RegQueryValueExW (ppod->hKey, L"DiffIncCRF", 0, &dwType, (LPBYTE)szTemp, &dwSize);
         if (pControl = pPage->ControlFind (L"old"))
            pControl->AttribSet (Text(), szTemp);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"old")) {
            DWORD dwNeeded;
            WCHAR szTemp[256];
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);

            // write to registry
            RegSetValueExW (ppod->hKey, L"DiffIncCRF", 0, REG_SZ, (LPBYTE)szTemp, ((DWORD)wcslen(szTemp)+1)*sizeof(WCHAR));

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

         if (!_wcsicmp(psz, L"oldbrowse")) {
            PWSTR pszControl = L"old";
            WCHAR szTemp[256];
            PCEscControl pControl;
            DWORD dwNeeded;
            szTemp[0] = 0;
            if (pControl = pPage->ControlFind (pszControl))
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);

            if (!CircumrealityOpenDialog (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), FALSE))
               return TRUE;

            // set the text
            pControl->AttribSet (Text(), szTemp);

            // write the registry

            pPage->Message (ESCM_USER+93);   // to update buttons
            RegSetValueExW (ppod->hKey, L"DiffIncCRF", 0, REG_SZ, (LPBYTE)szTemp, ((DWORD)wcslen(szTemp)+1)*sizeof(WCHAR));

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"convert")) {
            // open the two files
            CMegaFile mfCircumreality, mfDiff;
            WCHAR szTemp[256];
            PCEscControl pControl;
            DWORD dwNeeded;

            // load in old
            szTemp[0] = 0;
            if (pControl = pPage->ControlFind (L"old"))
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            if (!mfCircumreality.Init (szTemp, &GUID_MegaCircumreality, FALSE)) {
               pPage->MBWarning (
                  L"Couldn't open the .crf file.",
                  L"Make sure you have the correct path for the file.");
               return TRUE;
            }

            // get the dif file to save it as
            if (!DIFOpenDialog (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), FALSE))
               return TRUE;

            // open
            if (!mfDiff.Init (szTemp, &GUID_MegaCircumreality, FALSE)) {
               pPage->MBWarning (
                  L"Couldn't open the .DIF file.",
                  L"Make sure you have the correct path for the file.");
               return TRUE;
            }

            // enumerate all the files in the diff
            CListVariable lName;
            CListFixed lMFFILEINFO;
            lMFFILEINFO.Init (sizeof(MFFILEINFO));
            mfDiff.Enum (&lName, &lMFFILEINFO);
            __int64 iDeleteSize = (wcslen(gpszDiffToDelete)+1)*sizeof(WCHAR);
            DWORD i;
            PMFFILEINFO pmf = (PMFFILEINFO) lMFFILEINFO.Get(0);
            for (i = 0; i < lName.Num(); i++, pmf++) {
               PWSTR psz = (PWSTR)lName.Get(i);

               // load this
               __int64 iSize;
               PVOID pLoad = mfDiff.Load (psz, &iSize);
               if (!pLoad)
                  continue;   // shouldnt happen

               // if it's delete then delete from original
               if ((iSize == iDeleteSize) && !wcscmp((PWSTR)pLoad, gpszDiffToDelete)) {
                  mfCircumreality.Delete (psz);
                  MegaFileFree (pLoad);
                  continue;
               }

               // else, write
               mfCircumreality.Save (psz, pLoad, iSize, &pmf->iTimeCreate, &pmf->iTimeModify, &pmf->iTimeAccess);

#ifdef _DEBUG
               swprintf (szTemp, L"\r\n%s : %d %x-%x, %x-%x %x-%x",
                  psz, (int)iSize,
                  (int)pmf->iTimeCreate.dwHighDateTime, (int)pmf->iTimeCreate.dwLowDateTime,
                  (int)pmf->iTimeModify.dwHighDateTime, (int)pmf->iTimeModify.dwLowDateTime,
                  (int)pmf->iTimeAccess.dwHighDateTime, (int)pmf->iTimeAccess.dwLowDateTime
                  );
               OutputDebugStringW (szTemp);
#endif

               MegaFileFree (pLoad);
            } // i

            // done
            pPage->MBInformation (L"The .DIF file has been merged into the .crf file.");

            pPage->Exit (Back());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Incorporate differences into a .crf file";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}

/*****************************************************************************
ProjectOpen - Shows UI for opening the project.

inputs
   BOOL        *pfModify - Filled with TRUE if want to modify the project
   PWSTR       pszCircumrealityFile - This will be set to the Circumreality file name if the user
                  selects one.
returns
   PCMIFLProj - Project that's opened, or NULL if no project is opened. However,
                  pszMIFFile might be set to the MIF file name to run.
*/
static PCMIFLProj ProjectOpen (BOOL *pfModify, PWSTR pszCircumrealityFile)
{
   POD pod;
   char szTemp[256];
   memset (&pod, 0 ,sizeof(pod));

   // key to the registry
   HKEY hKey = NULL;
   RegCreateKey (HKEY_CURRENT_USER, gpszServerKey, &hKey);
   pod.hKey = hKey;
   szTemp[0] = 0;
   DWORD dwSize = sizeof(szTemp);
   DWORD dwType;
   RegQueryValueEx (hKey, "Project", 0, &dwType, (LPBYTE)szTemp, &dwSize);
   if (szTemp[0])
      MultiByteToWideChar (CP_ACP, 0, szTemp, -1, pod.szName, sizeof(pod.szName)/sizeof(WCHAR));

   // key for the lcient
   HKEY hClientKey = NULL;
   RegCreateKey (HKEY_CURRENT_USER, gpszClientKey, &hClientKey);
   pod.hClientKey = hClientKey;
   pod.fEnableClientTools = FALSE;
   dwSize = sizeof(pod.fEnableClientTools);
   RegQueryValueEx (hClientKey, "EnableClientTools", 0, &dwType, (LPBYTE)&pod.fEnableClientTools, &dwSize);

   // need to get and save gdwCmdLineParam
   dwSize = sizeof(gdwCmdLineParam);
   gdwCmdLineParam = 1; // BUGFIX - made default value 1, so live on internet
   RegQueryValueEx (hKey, "CmdLineParam", 0, &dwType, (LPBYTE)&gdwCmdLineParam, &dwSize);

   // create the dialog...
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, NULL, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLOPEN, OpenPage, &pod);

   if (psz && !_wcsicmp(psz, L"diff")) {
      psz = Window.PageDialog (ghInstance, IDR_MMLDIFF, DiffPage, &pod);

      if (psz && !_wcsicmp(psz, Back()))
         goto redo;
      // else fall through
   }
   else if (psz && !_wcsicmp(psz, L"diffinc")) {
      psz = Window.PageDialog (ghInstance, IDR_MMLDIFFINC, DiffIncPage, &pod);

      if (psz && !_wcsicmp(psz, Back()))
         goto redo;
      // else fall through
   }
   else if (psz && !_wcsicmp(psz, L"monitor")) {
      WORLDMONITOR WM;
      MonitorInfoFromReg (&WM);

      psz = Window.PageDialog (ghInstance, IDR_MMLMONITOR, MonitorPage, &WM);

      MonitorInfoToReg (&WM);

      if (psz && !_wcsicmp(psz, Back()))
         goto redo;
      // else fall through
   }

   *pfModify = pod.fModify;

   // free up
   if (hKey) {
      WideCharToMultiByte (CP_ACP, 0, pod.szName, -1, szTemp, sizeof(szTemp), 0, 0);
      RegSetValueEx (hKey, "Project", 0, REG_SZ, (LPBYTE)szTemp, (DWORD)strlen(szTemp)+1);
      RegSetValueEx (hKey, "CmdLineParam", 0, REG_DWORD, (LPBYTE)&gdwCmdLineParam, sizeof(gdwCmdLineParam));
      RegCloseKey (hKey);
   }
   if (hClientKey) {
      RegSetValueEx (hClientKey, "EnableClientTools", 0, REG_DWORD, (LPBYTE)&pod.fEnableClientTools, sizeof(pod.fEnableClientTools));
      RegCloseKey (hClientKey);
   }

   if (pod.fRunCircumreality)
      wcscpy (pszCircumrealityFile, pod.szName);

   return pod.pProj;
}

/*****************************************************************************
CircumrealityMainLoop - DOes the main display loop for 3DOB

inputs
   LPSTR       lpCmdLine - command line
   int         nShowCmd - Show
*/
int CircumrealityMainLoop (LPSTR lpCmdLine, int nShowCmd)
{
   // dissect the command line
   gszProjectFile[0] = 0;  // clear out
   BOOL fMonitoring = FALSE;
   if (lpCmdLine && !_stricmp(lpCmdLine, "-monitor"))
      fMonitoring = TRUE;
   else if (lpCmdLine && lpCmdLine[0]) {
      if ((lpCmdLine[0] == '-') || (lpCmdLine[0] == '/')) {
         gdwCmdLineParam = (DWORD)atoi(lpCmdLine+1);
         while (lpCmdLine[0] && !isspace(lpCmdLine[0]))
            lpCmdLine++;
      }

      // skip spaces
      while (isspace(lpCmdLine[0]))
         lpCmdLine++;
      
      if (strlen(lpCmdLine) < 250)
         MultiByteToWideChar (CP_ACP, 0, lpCmdLine, -1, gszProjectFile, sizeof(gszProjectFile)/sizeof(WCHAR));
   }
   DWORD dwLen = (DWORD)wcslen(gszProjectFile);
   DWORD dwCmdLineFile = 0;
   if (dwLen >= 4) {
      if (!_wcsicmp(gszProjectFile + (dwLen-4), L".mfp"))
         dwCmdLineFile = 1;
      else if (!_wcsicmp(gszProjectFile + (dwLen-4), L".crf"))
         dwCmdLineFile = 2;
   }

   // get the name
   GetModuleFileName (ghInstance, gszAppPath, sizeof(gszAppPath));
   strcpy (gszAppDir, gszAppPath);
   char  *pCur;
   for (pCur = gszAppDir + strlen(gszAppDir); pCur >= gszAppDir; pCur--)
      if (*pCur == '\\') {
         pCur[1] = 0;
         break;
      }
#ifdef _DEBUG
   // Get current flag
   int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

   // Turn on leak-checking bit
   tmpFlag |= _CRTDBG_LEAK_CHECK_DF; // | _CRTDBG_CHECK_ALWAYS_DF;
   //tmpFlag |= _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_DELAY_FREE_MEM_DF;

   // BUGFIX - Turn off the high values so dont check for leak every 10K
   // Do this to make things faster
   // tmpFlag &= ~(_CRTDBG_CHECK_EVERY_1024_DF);

   // Set flag to the new value
   _CrtSetDbgFlag( tmpFlag );

   // test
   //char *p;
   //p = (char*)MYMALLOC (42);
   // p[43] = 0;
#endif // _DEBUG


   // initialize
   EscInitialize (L"mikerozak@bigpond.com", 2511603559, 0);

   // add an imagegrid control that shows grid
   EscControlAdd (L"ImageDrag", ControlImageDrag);

   InitCommonControls ();

   // init beep window
   BeepWindowInit ();

   // if monitoring then special UI
   if (fMonitoring) {
      MonitorAll();
      goto done;
   }

   // get the file...
   BOOL fModify;

reopen:
   fModify = FALSE;
   switch (dwCmdLineFile) {
   default:
   case 0:  // unspecified
      gszProjectFile[0] = 0; // to clear
      gpMIFLProj = ProjectOpen (&fModify, gszProjectFile);
      if (!gpMIFLProj) {
         if (gszProjectFile[0]) {
            // selected Circumreality file, so redo
            dwCmdLineFile = 2;
            goto reopen;
         }
         goto done;
      }
      break;

   case 1: // command line file
      gpMIFLProj = new CMIFLProj (&gMIFLSocket);
      if (!gpMIFLProj) {
         dwCmdLineFile = 0;
         goto reopen;
      }
      if (!gpMIFLProj->Open (gszProjectFile, NULL)) {
         WCHAR szTemp[512];
         swprintf (szTemp, L"%s could not be opened.", gszProjectFile);
         goto done;
      }
      gpMIFLProj->m_fTransProsQuick = TRUE;
      break;

   case 2:  // Circumreality file
      {
         CMegaFile mf;
         if (!mf.Init (gszProjectFile, &GUID_MegaCircumreality, FALSE)) {
            WCHAR szTemp[512];
            swprintf (szTemp, L"%s could not be opened.", gszProjectFile);
            goto done;
         }

         // read the project from the megafile
         gpMIFLProj = new CMIFLProj (&gMIFLSocket);
         gpMIFLProj->m_fTransProsQuick = TRUE;
         if (!gpMIFLProj)
            goto done;
         MegaFileSet (&mf, NULL, NULL);
         BOOL fRet = gpMIFLProj->Open (gpszProjectFile, NULL);
         MegaFileSet (NULL, NULL, NULL);
         if (!fRet) {
            WCHAR szTemp[512];
            swprintf (szTemp, L"%s could not be opened.", gszProjectFile);
            goto done;
         }

      }
      break;
   }


   // if want to modify then something different
   if (fModify)   // pull up normal edit ui
      gpMIFLProj->DialogEdit ();
   else {
      WCHAR szCircumreality[256];
      if (dwCmdLineFile != 2) {
         // try generating megafile
         WCHAR szCRK[256];
         ProjectNameToCircumreality (gpMIFLProj->m_szFile, TRUE, szCircumreality);
         ProjectNameToCircumreality (gpMIFLProj->m_szFile, FALSE, szCRK);
         if (!MegaFileGenerateIfNecessary (szCircumreality, szCRK, gpMIFLProj, NULL))
            goto done;
      }
      else
         wcscpy (szCircumreality, gszProjectFile);

      // run without modification
      gpMainWindow = new CMainWindow;
      if (!gpMainWindow)
         goto done;
      gpMainWindow->m_fPostQuitMessage = TRUE;  // so that will shut down

      if (!gpMainWindow->Init (szCircumreality)) {
         delete gpMainWindow;
         gpMainWindow = NULL;
         goto done;
      }

      // compile
      BOOL fRet;
      {
         CProgress Progress;
         gpMIFLProj->Compile (&Progress);
         fRet = !gpMIFLProj->m_pErrors || gpMIFLProj->m_pErrors->m_dwNumError;
      }
      if (fRet) {
         EscMessageBox (NULL, L"Circumreality World Simulator", L"The project failed to compile.",
            L"You should modify and test compile it first.", MB_ICONWARNING | MB_OK);
         goto done;
      }

      // creat the VM
      PCMIFLVM pVM;
      pVM = gpMIFLProj->VMCreate();
         // BUGBUG - eventually reload saved version when call VMCreate()?
      if (!pVM) {
         EscMessageBox (NULL, L"Circumreality World Simulator", L"Circumreality World Simulator could not create the virtual machine.",
            L"You should modify and test compile it first.", MB_ICONWARNING | MB_OK);
         goto done;
      }

      // BUGFIX - Only do VMSet if no VM already, because may have had VMSet
      // called when accesse from VMCreate()
      if ((gpMainWindow->VMGet() != pVM) && !gpMainWindow->VMSet (pVM)) {
         delete pVM;
         EscMessageBox (NULL, L"Circumreality World Simulator", L"Circumreality World Simulator could not set the virtual machine.",
            L"You should modify and test compile it first.", MB_ICONWARNING | MB_OK);
         goto done;
      }

      // start the heartbeat timer
      MonitorHeartbeatTimerStart (szCircumreality);

      gpMainWindow->CPUMonitor(NULL);  // clear out user log
      // BUGFIX - Forgot to set the automatic timer
      pVM->TimerAutomatic (100);
      gpMainWindow->CPUMonitor(NULL);  // clear out user log

      // window loop
      MSG msg;
      while( GetMessage( &msg, NULL, 0, 0 ) ) {
         TranslateMessage( &msg );
         DispatchMessage( &msg );
      }

      // stop heartbeat
      MonitorHeartbeatTimerStop();

      // delete the VM
      gpMainWindow->VMSet(NULL);
      delete pVM;
   }

 
done:
   // free project
   // NOTE: Dont save because will auto save when exiting
   if (gpMIFLProj)
      delete gpMIFLProj;
   gpMIFLProj = NULL;

   // free the main window if it still exists
   if (gpMainWindow)
      delete gpMainWindow;

   if (gfM3DInit) {
      CProgress Progress;
      Progress.Start (NULL, "Shutting down...");
      MyCacheM3DFileEnd (DEFAULTRENDERSHARD);
      M3DEnd (DEFAULTRENDERSHARD, &Progress);
   }

   // free lexicon and tts
   TTSCacheShutDown();
   MLexiconCacheShutDown (TRUE);

   ButtonBitmapReleaseAll ();
   // FUTURERELEASE - end help ASPHelpEnd ();


   // dlete beep window
   BeepWindowEnd ();


   EscUninitialize();

#ifdef _DEBUG
   _CrtCheckMemory ();
#endif // DEBUG
   return 0;
}

/*****************************************************************************
WinMain */
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
   ghInstance = hInstance;

   // make sure NOT optimizing for memory usage, but to decrease fractures
   // BUGFIX - Temporarily had to TRUE, but got more memory on server and slow under debug
   EscMallocOptimizeMemory (FALSE);

   int iRet = CircumrealityMainLoop (lpCmdLine, nShowCmd);

   // if the restart flag is set then do that
   if (gfRestart) {
      char szDir[512];
      _getcwd (szDir, sizeof(szDir));

      // BUGFIX - Restart needs to make a new command line up
      // so that will restart with existing file
      char szCmdLine[512];
      if (gszProjectFile[0]) {
         sprintf (szCmdLine, "-%d ", (int)gdwCmdLineParam);
         WideCharToMultiByte (CP_ACP, 0, gszProjectFile, -1, szCmdLine + strlen(szCmdLine),
            sizeof(szCmdLine) - (DWORD)strlen(szCmdLine), 0, 0);
      }
      else
         szCmdLine[0] = 0; // since dont have anything to rerun
      HINSTANCE hInst = ShellExecute (GetDesktopWindow(), NULL, gszAppPath,
         szCmdLine, szDir, SW_SHOW);
#ifdef _DEBUG
      if ((DWORD)(LONG_PTR)hInst <= 32) {
         char szTemp[128];
         sprintf (szTemp, "ShellExecute() failed and returned %d", (int)(LONG_PTR)hInst);
         MessageBox (NULL, szTemp, NULL, MB_OK);
      }
#endif

   }

   return iRet;
}



// DOCUMENT: Will need to write tutorial for how to use the server along with MIFL


// BUGBUG - If start up in local-server mode (instead of itnernet mode) then
// hide the server window so that user won't see it

// BUGBUG - Default "shard" setting for first page should be "1" instead of
// "0"... makes easier for testing world.