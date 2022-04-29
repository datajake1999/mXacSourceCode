/**********************************************************************
Misc.cpp - For the misc page

begun 9/10/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <tapi.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"
#include "buildnum.h"

#define  ESCM_SHOWREGISTER       (ESCM_USER+342)

static WCHAR gszEmail[] = L"Email";
static WCHAR gszRegKey[] = L"RegKey";
static WCHAR gszFullColorButton[] = L"FullColor";
static WCHAR gszMicroHelpButton[] = L"MicroHelp";
static WCHAR gszSmallFontButton[] = L"SmallFont";
static WCHAR gszDisableSoundsButton[] = L"DisableSounds";
static WCHAR gszDisableCursorButton[] = L"DisableCursor";
static WCHAR gszTemplateRButton[] = L"TemplateR";
static WCHAR gszPrintTwoColumnsButton[] = L"PrintTwoColumns";
static WCHAR gszMinimizeButton[] = L"Minimize";
static WCHAR gszVersion[] = L"Version";
static WCHAR gszLastAsk[] = L"LastAsk";

/***********************************************************************
LastBackupGet - Get information about the last time backed up.

inputs
   PWSTR    pszVersion - Filled with the version number of last backup. 0 if not.
               Should be at least 32 chars.
   DFDATE   *pLastAsk - Last date that asked the user if wanted to backup. 0 if not
returns
   DFDATE - Date of last backup. 0 if not
*/
DFDATE LastBackupGet (PWSTR pszVersion, DFDATE *pLastAsk)
{
   HANGFUNCIN;
   pszVersion[0] = 0;
   *pLastAsk = 0;

   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszMiscNode);
   if (!pNode) {
      return 0;   // unexpected error
   }
   PWSTR psz;

   *pLastAsk = (DFDATE)NodeValueGetInt (pNode, gszLastAsk, 0);

   int iNumber;
   iNumber = 0;
   psz = NodeValueGet (pNode, gszVersion, &iNumber);
   if (psz)
      wcscpy (pszVersion, psz);
   gpData->NodeRelease(pNode);

   return pszVersion[0] ? (DFDATE) iNumber : 0;
}


/***********************************************************************
LastBackupSet - Set information about the last time backed up.

inputs
   PWSTR    pszVersion - The version number of last backup.
   DFDATE   dLastBackup - Last day that backed up
   DFDATE   dLastAsk - Last date that asked the user if wanted to backup. 0 if not
returns
*/
void LastBackupSet (PWSTR pszVersion, DFDATE dLastBackup, DFDATE dLastAsk)
{
   HANGFUNCIN;
   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszMiscNode);
   if (!pNode) {
      return;   // unexpected error
   }

   NodeValueSet (pNode, gszLastAsk, (int) dLastAsk);
   NodeValueSet (pNode, gszVersion, pszVersion, (int) dLastBackup);
   gpData->NodeRelease(pNode);
}

/***********************************************************************
BackupDirName - Fills a buffer in with the name of the backup directory.

inputs
   PWSTR    psz - Filled in with backup name. should be 256 chars
*/
void BackupDirName (PWSTR psz)
{
   wcscpy (psz, gpData->Dir());
   wcscat (psz, L" Backup");
}

/***********************************************************************
BackupDatabase - This backs up the database to the backup directory

inputs
   PCEscPage   pPage - If fail to backup then report error
   PWSTR       pszDir - Backup directory. If NULL uses default
returns
   BOOL - FALSE if error
*/
BOOL BackupDatabase (PCEscPage pPage, PWSTR pszDir)
{
   HANGFUNCIN;
   WCHAR szDir[256];
   BackupDirName(szDir);
   if (!pszDir)
      pszDir = szDir;

   if (!gpData->Backup(pszDir)) {
      MemZero (&gMemTemp);
      MemCat (&gMemTemp, L"The database could not be backed up to ");
      MemCatSanitize (&gMemTemp, pszDir);
      MemCat (&gMemTemp, L".");
      pPage->MBWarning ((PWSTR)gMemTemp.p, L"The hard disk may be full. Please investiage "
         L"the problem and try to back up again.");
      return FALSE;
   }

   pPage->MBSpeakInformation (L"The database has been backed up.");

   // note that it's backed up
   WCHAR szTemp[32];
   MultiByteToWideChar (CP_ACP, 0, BUILD_NUMBER, -1, szTemp, sizeof(szTemp)/2);
   LastBackupSet (szTemp, Today(), Today());

   return TRUE;
}

/***********************************************************************
BackupBugUser - Bugs the user that they should backup IF the version
number changes, or if they haven't backed up for awhile.

inputs
   PCEscPage      pPage - page
returns
   nonne
*/
void BackupBugUser (PCEscPage pPage)
{
   HANGFUNCIN;
   // first of all, if user has only been using this for a short time then dont ask
   if (gpData->Num() < 100)
      return;

   // get last backup information
   DFDATE dLastBackup, dLastAsk;
   WCHAR  szOldVersion[32], szNewVersion[32];
   dLastBackup = LastBackupGet (szOldVersion, &dLastAsk);
   MultiByteToWideChar (CP_ACP, 0, BUILD_NUMBER, -1, szNewVersion, sizeof(szNewVersion)/2);

   // if there's no reason to backup then don't
   BOOL  fOutOfDate, fVersion;
   DFDATE today = Today();
   __int64 iToday = DFDATEToMinutes(today);
   fOutOfDate = (!dLastBackup || ((iToday - DFDATEToMinutes(dLastBackup)) > 60*24*32));
   fVersion = _wcsicmp(szNewVersion, szOldVersion) ? TRUE : FALSE;
   if (!fOutOfDate && !fVersion)
      return;

   // if it's out of date but just asked then quit
   if (fOutOfDate) {
      if (dLastAsk && ((iToday - DFDATEToMinutes(dLastAsk)) < 60*24*31))
         return;
   }

   // make up a string explaining where it copies to
   WCHAR szDir[256];
   BackupDirName(szDir);
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"If you press 'Yes', Dragonfly will backup your Dragonfly "
      L"files to ");
   MemCatSanitize (&gMemTemp, szDir);
   MemCat (&gMemTemp, L". If you want to back up to a different directory you can "
      L"press 'No' and visit the Backup page.");

   // what notification to ask
   WCHAR szMain[256];
   if (fOutOfDate) {
      if (dLastBackup) {
         wcscpy (szMain, L"You haven't backed up your Dragonfly files since ");
         DFDATEToString (dLastBackup, szMain + wcslen(szMain));
      }
      else
         wcscpy (szMain, L"You haven't backed up your Dragonfly files yet");
      wcscat (szMain, L". Do you want to back them up?");
   }
   else {
      wcscpy (szMain, L"You have just upgraded to a new version of Dragonfly. "
         L"Do you want to back up your Dragonfly files?");
   }

   // save away indicating that bugged
   LastBackupSet (szNewVersion, dLastBackup, today);

   if (IDYES == pPage->MBYesNo (szMain, (PWSTR) gMemTemp.p))
      BackupDatabase (pPage, szDir);
}

/***********************************************************************
RegisterPage - Page callback for new user (not quick-add though)
*/
BOOL RegisterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the text
         WCHAR szEmail[256];
         DWORD dwKey;
         dwKey = RegisterKeyGet (szEmail, sizeof(szEmail)/2);

         // show it
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszEmail);
         if (pControl)
            pControl->AttribSet (gszText, szEmail);
         pControl = pPage->ControlFind (gszRegKey);
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
         pControl = pPage->ControlFind (gszRegKey);
         if (pControl) {
            // BUGFIX - Didn't handle passwords larger than 2 billion
            WCHAR szTemp[64];
            szTemp[0] = 0;
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp) / 2, &dwNeeded);
            WCHAR *pEnd;
            dwKey = wcstoul (szTemp, &pEnd, 10);
         }

         // save it away
         RegisterKeySet (szEmail, dwKey);

         // change UI
         pPage->Message (ESCM_SHOWREGISTER);
      }
      break;

   case ESCM_SHOWREGISTER:
      {
         // set UI
         PWSTR psz;
         psz = !RegisterIsRegistered () ?
            L"<br><colorblend posn=background lcolor=#ffc000 rcolor=#ff0000/>You have <bold>not</bold> registered your copy of Dragonfly.</br>" :
            L"<br><colorblend posn=background lcolor=#40c040 rcolor=#40c0c0/><bold>Thank you.</bold><br/>You have <bold>registered</bold> your copy of Dragonfly.</br>";
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



/***********************************************************************
BackupPage - Page callback for new user (not quick-add though)
*/
BOOL BackupPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"backupdir");
         WCHAR szDir[256];
         BackupDirName(szDir);
         if (pControl)
            pControl->AttribSet (gszText, szDir);
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         if (!_wcsicmp(p->psz, L"showdir")) {
            WCHAR szw[256];
            char sz[256];
            gpData->DirParent (szw);
            WideCharToMultiByte (CP_ACP, 0, szw, -1, sz, sizeof(sz), 0, 0);
            ShellExecute (pPage->m_pWindow->m_hWnd, NULL, sz, NULL, NULL, SW_SHOWNORMAL);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"backup")) {
            WCHAR szw[256];
            DWORD dwNeeded = sizeof(szw);
            PCEscControl pControl = pPage->ControlFind (L"backupdir");
            szw[0] = 0;
            if (pControl)
               pControl->AttribGet (gszText, szw, sizeof(szw), &dwNeeded);

            BackupDatabase (pPage, szw);

            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         // only care about projectlist
         if (!_wcsicmp(p->pszSubName, L"DRAGONFLYDIRSUB")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            WCHAR szTemp[256];
            gpData->DirSub (szTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"BACKUPWARNING")) {
            DFDATE dLastBack, dLastAsk;
            WCHAR szVersion[32];
            dLastBack = LastBackupGet (szVersion, &dLastAsk);

            // zero memory and get the node
            MemZero (&gMemTemp);

            if (!dLastBack)
               MemCat (&gMemTemp, L"You have never backed up your Dragonfly files.");
            else {
               BOOL  fRed;
               fRed = ((DFDATEToMinutes(Today()) - DFDATEToMinutes(dLastBack)) > 60*24*31);

               if (fRed)
                  MemCat (&gMemTemp, L"<font color=#ff0000><big>");
               MemCat (&gMemTemp, L"You last backed up your Dragonfly files on <bold>");
               WCHAR szTemp[64];
               DFDATEToString (dLastBack, szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"</bold>.");
               if (fRed)
                  MemCat (&gMemTemp, L"</big></font>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"DRAGONFLYPARENT")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            WCHAR szTemp[256];
            gpData->DirParent (szTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"DRAGONFLYDIR")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, gpData->Dir());
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
ChangePasswordPage - Page callback for new user (not quick-add though)
*/
BOOL ChangePasswordPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"change")) {
            PCEscControl pControl;
            DWORD dwNeeded;
            WCHAR szOld[64], szNew1[64], szNew2[64];

            // get the passwords
            szOld[0] = szNew1[0] = szNew2[0] = 0;
            pControl = pPage->ControlFind (L"oldpassword");
            if (pControl)
               pControl->AttribGet (gszText, szOld, sizeof(szOld), &dwNeeded);
            pControl = pPage->ControlFind (L"password1");
            if (pControl)
               pControl->AttribGet (gszText, szNew1, sizeof(szOld), &dwNeeded);
            pControl = pPage->ControlFind (L"password2");
            if (pControl)
               pControl->AttribGet (gszText, szNew2, sizeof(szOld), &dwNeeded);

            // verify 2 new passwords are the same
            if (wcscmp (szNew1, szNew2)) {
               pPage->MBWarning (L"You've made a typing error on the new password.",
                  L"The two versions of the password are different. Please retype them.");
               return TRUE;
            }

            // verify that the old password is corect
            if (wcscmp (szOld, gpData->m_szPassword)) {
               pPage->MBWarning (L"Your old password isn't typed in properly.",
                  L"Please retype it. This is to make sure that someone doesn't change "
                  L"your password on you.");
               return TRUE;
            }

            // set cursor to wait
            pPage->SetCursor (IDC_NOCURSOR);

            // change it
            gpData->ChangePassword (szNew1);

            // alert user
            pPage->MBSpeakInformation (L"Password changed.");
            return TRUE;
         }
      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
MiscUIPage - Page callback

BUGFIX - Option that allows people to use color (or not) in Dragonfly
*/
BOOL MiscUIPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // check/uncheck based upon is archive on
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszFullColorButton);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gfFullColor);

         pControl = pPage->ControlFind (gszMicroHelpButton);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gfMicroHelp);

         pControl = pPage->ControlFind (gszMinimizeButton);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gfMinimizeDragonflyToTaskbar);

         pControl = pPage->ControlFind (gszSmallFontButton);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) gdwSmallFont);

         pControl = pPage->ControlFind (gszDisableSoundsButton);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gfDisableSounds);

         pControl = pPage->ControlFind (gszDisableCursorButton);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gfDisableCursor);

         pControl = pPage->ControlFind (gszTemplateRButton);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gfTemplateR);

         pControl = pPage->ControlFind (gszPrintTwoColumnsButton);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gfPrintTwoColumns);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         // if the date changes then refresh the page
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;    // default

         if (!_wcsicmp(psz, gszSmallFontButton)) {
            if (p->dwCurSel == gdwSmallFont)
               break;   // no change

            gdwSmallFont = p->dwCurSel;
            KeySet (gszSmallFont, gdwSmallFont);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR psz;
         psz = p->pControl->m_pszName;

         if ((psz[0] == L'c') && (psz[1] == L'i') && (psz[2] == L':')) {
            DWORD i = _wtoi(psz+3);
            if (i >= JPEGREMAP)
               return TRUE;   // out of bounds

            // first, file open dialog
            OPENFILENAME   ofn;
            HWND  hWnd = pPage->m_pWindow->m_hWnd;
            memset (&ofn, 0, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.hInstance = ghInstance;
            ofn.lpstrFilter =
               "Supported image files (*.jpg)\0*.jpg\0"
               "\0\0";
            ofn.lpstrFile = gaszJPEGRemap[i];
            ofn.nMaxFile = sizeof(gaszJPEGRemap[i]);
            ofn.lpstrTitle = "Select the image to use";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
               OFN_HIDEREADONLY | OFN_EXPLORER;
            ofn.lpstrDefExt = "jpg";
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               gaszJPEGRemap[i][0] = 0;

            char szTemp[64];
            sprintf (szTemp, "ImageRemap%d", i);
            KeySetString (szTemp, gaszJPEGRemap[i]);

            WCHAR szw[256];
            MultiByteToWideChar (CP_ACP, 0, gaszJPEGRemap[i], -1, szw, 256);
            HANGFUNCIN;
            EscRemapJPEG (gadwJPEGRemap[i], szw[0] ? szw : NULL);

            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

         if (!_wcsicmp(p->pControl->m_pszName, gszFullColorButton)) {
            gfFullColor = p->pControl->AttribGetBOOL (gszChecked);
            KeySet (gszFullColor, gfFullColor);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, gszMicroHelpButton)) {
            gfMicroHelp = p->pControl->AttribGetBOOL (gszChecked);
            KeySet (gszMicroHelp, gfMicroHelp);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, gszMinimizeButton)) {
            gfMinimizeDragonflyToTaskbar = p->pControl->AttribGetBOOL (gszChecked);
            KeySet (gszMinimizeDragonflyToTaskbar, gfMinimizeDragonflyToTaskbar);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, gszDisableSoundsButton)) {
            gfDisableSounds = p->pControl->AttribGetBOOL (gszChecked);
            KeySet (gszDisableSounds, gfDisableSounds);
            EscSoundsSet (gfDisableSounds ? 0 : ESCS_ALL);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, gszDisableCursorButton)) {
            gfDisableCursor = p->pControl->AttribGetBOOL (gszChecked);
            KeySet (gszDisableCursor, gfDisableCursor);
            EscTraditionalCursorSet (gfDisableCursor);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, gszTemplateRButton)) {
            gfTemplateR = p->pControl->AttribGetBOOL (gszChecked);
            KeySet (gszTemplateR, gfTemplateR);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, gszPrintTwoColumnsButton)) {
            gfPrintTwoColumns = p->pControl->AttribGetBOOL (gszChecked);
            KeySet (gszPrintTwoColumns, gfPrintTwoColumns);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}


