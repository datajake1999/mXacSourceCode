/*************************************************************************
Photos.cpp - Archiving feature

begun July-7-2001 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

/* globals */
static DWORD       gdwEntryNode;
static DFDATE      gdateLog = Today();

static WCHAR *gpszAddFile;
static WCHAR *gpszAddName;
static char  *gpszaAddFile;
static DFDATE gdAddDate;
static DWORD gdwAddCategory;

static WCHAR gszWallpaper[] = L"wallpaper";
static WCHAR gszExtension[] = L"extension";

static DWORD   gdwLastPhotoTick = 0;   // last photo wallpapertick count
static DFDATE  gdLastPhotoDate = 0;    // last photo wallpaper date

/*****************************************************************************
GetPhotosLogNode - Given a date, this returns a month-specific node for the
call log.


inputs
   DFDATE         date - Date to look for
   BOOL           fCreateIfNotExit - if TRUE create the database node
                  if it doesn't exist. If FALSE, return NULL if it doesn't exist.
   DWORD          *pdwNode - Filled in with a node specific to the month/year.
returns
   PCMMLNode - Node (must be released) specific to the month/year. NULL if cant find/create.
*/
PCMMLNode GetPhotosLogNode (DFDATE date, BOOL fCreateIfNotExist, DWORD *pdwNode)
{
   HANGFUNCIN;
   PCMMLNode   pNew;

   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszPhotosNode);
   if (!pNode)
      return FALSE;   // unexpected error

   // take out the month because want photos by year
   pNew = MonthYearTree (pNode, TODFDATE(0, 0, YEARFROMDFDATE(date)), gszPhotos, fCreateIfNotExist, pdwNode);

   gpData->NodeRelease(pNode);

   return pNew;
}
/**********************************************************************
PhotosEntrySet - Sets the category node to view
*/
void PhotosEntrySet (DWORD dwNode)
{
   HANGFUNCIN;
   gdwEntryNode = dwNode;
}



#if 0
/***********************************************************************
PhotosViewPage - Page callback
*/
BOOL PhotosViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"ENTRYNAME")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszName);
            MemCat (&gMemTemp, psz ? psz : L"");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"JOURNALNOTES")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszSummary);
            if (psz)
               MemCat (&gMemTemp, psz);

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"TIME")) {
            PCMMLNode pNode;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwEntryNode);
            if (pNode) {
               WCHAR szTemp[64];
               DFDATEToString ((DFDATE) NodeValueGetInt (pNode, gszDate, 0), szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"<br/>");
               DFTIMEToString ((DFTIME) NodeValueGetInt (pNode, gszStart, -1), szTemp);
               MemCat (&gMemTemp, szTemp);
               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
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

         if (!_wcsicmp(p->pControl->m_pszName, L"edit")) {
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) gdwEntryNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
PhotosEditPage - Page callback
*/
BOOL PhotosEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"ENTRYNAME")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszName);
            MemCat (&gMemTemp, psz ? psz : L"");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"JOURNALNOTES")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszSummary);
            if (psz)
               MemCat (&gMemTemp, psz);

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

      }
      break;

   case ESCM_INITPAGE:
      {
         // save the information away
         PCMMLNode pNode;
         pNode = gpData->NodeGet (gdwEntryNode);
         if (!pNode)
            return FALSE;   // unexpected error

         // date
         DFDATE date;
         DFTIME time;
         date = (DFDATE) NodeValueGetInt (pNode, gszDate, 0);
         time = (DFTIME) NodeValueGetInt (pNode, gszStart, -1);

         DateControlSet (pPage, gszDate, date);
         TimeControlSet (pPage, gszStart, time);

         gpData->NodeRelease(pNode);
      }
      break;

   case ESCM_LINK:
      {
         // save the information away
         PCMMLNode pNode;
         pNode = gpData->NodeGet (gdwEntryNode);
         if (!pNode)
            return FALSE;   // unexpected error

         // get the text
         CMem mem;
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszSummary);
         if (pControl) {
            DWORD dwNeeded;
            dwNeeded = 0;
            pControl->AttribGet(gszText, NULL, 0, &dwNeeded);
            dwNeeded += 2;
            mem.Required (dwNeeded);
            pControl->AttribGet (gszText, (PWSTR) mem.p, dwNeeded, &dwNeeded);
            NodeValueSet (pNode, gszSummary, (PWSTR) mem.p);
         }


         // get the old date and remove it from the Photos list
         DFDATE olddate;
         DFTIME oldtime;
         olddate = (DFDATE) NodeValueGetInt (pNode, gszDate, 0);
         oldtime = (DFTIME) NodeValueGetInt (pNode, gszStart, -1);
         PCMMLNode pLog;
         DWORD dwLog;
         pLog = GetPhotosLogNode(olddate, TRUE, &dwLog);
         if (pLog) {
            NodeElemRemove (pLog, gszPhotos, gdwEntryNode);
            gpData->NodeRelease(pLog);
         }

         // date
         DFDATE date;
         DFTIME time;
         date = DateControlGet (pPage, gszDate);
         time = TimeControlGet (pPage, gszStart);
         NodeValueSet (pNode, gszDate, (int) date);
         NodeValueSet (pNode, gszStart, (int) time);


         // title
         WCHAR szTemp[256];
         pControl = pPage->ControlFind (gszName);
         if (pControl) {
            szTemp[0] = 0;
            DWORD dwNeeded;
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            NodeValueSet (pNode, gszName, szTemp);

            // change the title
            PCMMLNode pLog;
            DWORD dwLog;
            pLog = GetPhotosLogNode(date, TRUE, &dwLog);
            if (pLog) {
               NodeElemSet (pLog, gszPhotos, szTemp, (int) gdwEntryNode, TRUE,
                  date, time, -1);
               gpData->NodeRelease(pLog);
            }

         }

         gpData->NodeRelease(pNode);
         gpData->Flush();
      }
      break;   // default
   }

   return DefPage (pPage, dwMessage, pParam);
}

#endif // 0


/***********************************************************************
PhotoEditFromControls - Read in controls and write to the photo entry.

inputs
   PCEscPage      pPage - page
   PCMMLNode      pNode - node
   DWORD          dwNode - node number
*/
void PhotoEditFromControls (PCEscPage pPage, PCMMLNode pNode, DWORD dwNode)
{
   HANGFUNCIN;
   // fill in details
   PCEscControl pControl;
   DWORD dwNeeded;
   WCHAR szTemp[10000], szName[128];

   pControl = pPage->ControlFind(gszName);
   szName[0] = 0;
   if (pControl)
      pControl->AttribGet (gszText, szName, sizeof(szName), &dwNeeded);
   if (!szName[0])
      wcscpy (szName, L"Unknown");
   NodeValueSet (pNode, gszName, szName);

   pControl = pPage->ControlFind(gszSummary);
   szTemp[0] = 0;
   if (pControl)
      pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
   NodeValueSet (pNode, gszSummary, szTemp);


   // other data
   DFDATE date;
   date = DateControlGet (pPage, gszMeetingDate);
   NodeValueSet (pNode, gszDate, (int) date);

   DWORD i;
   for (i = 0; i < MAXPEOPLE; i++) {
      pControl = pPage->ControlFind (gaszPerson[i]);
      if (!pControl)
         continue;
         // get the values
      int   iCurSel;
      iCurSel = pControl->AttribGetInt (gszCurSel);

      // find the node number for the person's data and the name
      DWORD dwPNode;
      dwPNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);

      NodeValueSet (pNode, gaszPerson[i], (int) dwPNode);

      // write out the people links
      PCMMLNode pPerson;
      pPerson = NULL;
      if (dwPNode != (DWORD)-1)
         pPerson = gpData->NodeGet (dwPNode);
      if (pPerson) {
         // overwrite if already exists
         NodeElemSet (pPerson, gszPhotos, szName, (int) dwNode, TRUE,
            date, Now(), -1);

         gpData->NodeRelease (pPerson);
      }
   }

   // remember wallpaper option
   pControl = pPage->ControlFind(gszWallpaper);
   if (pControl)
      NodeValueSet (pNode, gszWallpaper, (int) pControl->AttribGetBOOL(gszChecked));
   
   PCMMLNode pCategory;
   DWORD dwOldCategory;
   dwOldCategory = 0;
   pControl = pPage->ControlFind (gszCategory);
   DWORD dwNewCat;
   DWORD dwIndex;
   dwIndex = pControl ? pControl->AttribGetInt(gszCurSel) : -1;
   dwNewCat = JournalIndexToDatabase(dwIndex);
   dwOldCategory = dwNewCat;

   // write this to the log to the category
   if (dwOldCategory != (DWORD)-1) {
      NodeValueSet (pNode, gszCategory, (int) dwOldCategory);
      pCategory = gpData->NodeGet (dwOldCategory);
      if (pCategory) {
         NodeElemSet (pCategory, gszJournalEntryNode, szName, (int) dwNode,
            TRUE, Today(), Now(), -1);
         gpData->NodeRelease(pCategory);
      }
   }

   // write out the daily log
   swprintf (szTemp, L"Photo: %s", szName);
   CalendarLogAdd (Today(), Now(), -1, szTemp, dwNode);

   // write this into the entries for the year
   PCMMLNode pLog;
   DWORD dwLogNode;
   pLog = GetPhotosLogNode(date, TRUE, &dwLogNode);
   if (pLog) {
      NodeElemSet (pLog, gszPhotos, szName, (int)dwNode, TRUE,
         date, Now(), -1);
      gpData->NodeRelease (pLog);
   }
}

/***********************************************************************
PhotosAddPage - Page callback
*/
BOOL PhotosAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"FILE")) {
            p->pszSubString = gpszAddFile;
            return TRUE;
         }


      }
      break;

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (gszName);
         if (pControl)
            pControl->AttribSet (gszText, gpszAddName);

         DateControlSet (pPage, gszMeetingDate, gdAddDate);

         // set journal dropdown
         // set the cateogyr
         pControl = pPage->ControlFind (gszCategory);
         DWORD dwIndex;
         dwIndex = JournalDatabaseToIndex (gdwAddCategory);
         if (pControl && (dwIndex != (DWORD)-1))
            pControl->AttribSetInt (gszCurSel, (int) dwIndex);
      }
      break;   // follow through

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // only want add
         if (_wcsicmp(p->pControl->m_pszName, gszAdd))
            break;

         // create the node to store the photo in
         PCMMLNode pNode;
         DWORD dwNode;
         pNode = gpData->NodeAdd (gszPhotosEntryNode, &dwNode);
         if (!pNode)
            return TRUE;   // error

         // copy the file
         WCHAR szDir[256], szwFile[256];
         char szaDir[256], szaFile[256];
         gpData->DirNode (dwNode, szDir);
         char *pszDot, *pszLastDot;
         for (pszDot = pszLastDot = strchr(gpszaAddFile, '.'); pszDot; pszDot = strchr(pszDot+1, '.'))
            pszLastDot = pszDot;
         WideCharToMultiByte (CP_ACP, 0, szDir, -1, szaDir, sizeof(szaDir), 0, 0);
         wsprintf (szaFile, "%s\\photo%d%s", szaDir, (int) dwNode, pszLastDot ? pszLastDot : "");
         MultiByteToWideChar (CP_ACP, 0, pszLastDot ? pszLastDot : ".jpg", -1, szwFile, sizeof(szwFile)/2);
         if (!CopyFile (gpszaAddFile, szaFile, FALSE)) {
            pPage->MBError (L"You are out of disk space in your Dragonfly directory.",
               L"Please clear out some space on your disk and try adding again.");
            gpData->NodeRelease (pNode);
            return TRUE;
            }
         NodeValueSet (pNode, gszExtension, szwFile);

         PhotoEditFromControls (pPage, pNode, dwNode);


         gpData->NodeRelease(pNode);
         gpData->Flush();

         pPage->Exit(gszOK);
         return TRUE;
      }
      break;   // default behavior

   };

   return DefPage (pPage, dwMessage, pParam);
}




/***********************************************************************
ImportPhotos - UI and code to import and addres book from a comma
separated file.

inputs
   PCEscPage      pPage - Page
   DWORD          dwJournal - Journal category node to add photos too. -1 = blank
returns
   none
*/
void ImportPhotos (PCEscPage pPage, DWORD dwJournal)
{
   HANGFUNCIN;
   // first, file open dialog
   OPENFILENAME   ofn;
   HWND  hWnd = pPage->m_pWindow->m_hWnd;
   char  szTemp[10000];
   szTemp[0] = 0;

   memset (&ofn, 0, sizeof(ofn));
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter =
      "Supported photo files (*.jpg;*.bmp)\0*.jpg;*.bmp\0"
      "\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = "Select the photos to load";
   ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
      OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
   ofn.lpstrDefExt = "jpg";
   // nFileExtension 

   if (!GetOpenFileName(&ofn))
      return;

   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;

   // work around wierd feature in file open where doesn't
   // separate file name with NULL if it's only a single selection,
   // but does if it's multile selection
   if (ofn.nFileOffset && (szTemp[ofn.nFileOffset-1]))
      szTemp[ofn.nFileOffset-1] = 0;

   // loop
   char  *pszName;
   for (pszName = szTemp + ofn.nFileOffset; pszName[0]; pszName += (strlen(pszName)+1)) {
      // create the full path
      char  szaFile[256];
      WCHAR szFile[256];
      WCHAR szName[256];
      strcpy (szaFile, szTemp);
      strcat (szaFile, "\\");
      strcat (szaFile, pszName);
      MultiByteToWideChar (CP_ACP, 0, szaFile, -1, szFile, sizeof(szFile)/2);

      // name without the .jpg extension
      MultiByteToWideChar (CP_ACP, 0, pszName, -1, szName, sizeof(szName)/2);
      PWSTR pszDot, pszLastDot;
      for (pszDot = pszLastDot = wcschr(szName, L'.'); pszDot; pszDot = wcschr(pszDot+1, L'.'))
         pszLastDot = pszDot;
      if (pszLastDot)
         pszLastDot[0] = 0;

      // set up some globals
      gpszAddFile = szFile;
      gpszaAddFile = szaFile;
      gpszAddName = szName;
      gdAddDate = Today();
      gdwAddCategory = dwJournal;
      FILETIME write;
      HFILE hFile;
      OFSTRUCT of;
      memset (&of, 0, sizeof(of));
      of.cBytes = sizeof(of);
      hFile = OpenFile (szaFile, &of, OF_READ | OF_SHARE_DENY_NONE);
      if (hFile != HFILE_ERROR) {
         GetFileTime ((HANDLE) hFile, NULL, NULL, &write);
         CloseHandle ((HANDLE) hFile);
         FILETIME local;
         SYSTEMTIME st;
         FileTimeToLocalFileTime (&write, &local);
         FileTimeToSystemTime (&local, &st);
         gdAddDate = TODFDATE (st.wDay, st.wMonth, st.wYear);
      }
      else {
         // file doesn't exist
         continue;
      }

      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPHOTOSADD, PhotosAddPage);
      if (pszRet && !_wcsicmp(pszRet, L"quit"))
         break;
   }

}


/***********************************************************************
PhotosPage - Page callback
*/
BOOL PhotosPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set the dropdown to the right date
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszDate);
         if (pControl)
            pControl->AttribSetInt (gszText, YEARFROMDFDATE(gdateLog));

         pControl = pPage->ControlFind (gszWallpaper);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gfChangeWallpaper);
      }
      break;   // go to default handler

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (p->pControl == pPage->ControlFind(gszDate)) {
            // if type in a new date
            int   iYear;
            iYear = p->pControl->AttribGetInt (gszText);
            if ((iYear < 1000) || (iYear > 9999))
               break;   // assume years have 4 digits

            // new year
            gdateLog = TODFDATE(1,1,iYear);
            pPage->Exit(gszRedoSamePage);
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"LOG")) {
            // find the log
            PCMMLNode pNode;
            DWORD dwNode;
            BOOL fDisplayed = FALSE;
            PCListFixed pl = NULL;
            MemZero (&gMemTemp);
            pNode = GetPhotosLogNode (gdateLog, FALSE, &dwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszPhotos, FALSE);

            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               
               // write it out
               MemCat (&gMemTemp, L"<tr><xtdtask href=v:");
               MemCat (&gMemTemp, pnlg->iNumber);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pnlg->psz ? pnlg->psz : L"Unknown");
               MemCat (&gMemTemp, L"</xtdtask><xtdcompleted>");

               WCHAR szTemp[64];
               DFDATEToString (pnlg->date, szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"<br/>");
               DFTIMEToString (pnlg->start, szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"<br/>");
               
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // if no entries then say so
            if (!pl || !pl->Num())
               MemCat (&gMemTemp, L"<tr><td>No photos for this year.</td></tr>");

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);

            p->pszSubString = (PWSTR)gMemTemp.p;
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

         if (!_wcsicmp(p->pControl->m_pszName, L"import")) {
            ImportPhotos (pPage);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszWallpaper)) {
            gfChangeWallpaper = p->pControl->AttribGetBOOL (gszChecked);
            KeySet (gszChangeWallpaper, gfChangeWallpaper);
            if (gfChangeWallpaper)
               PhotosUpdateDesktop(TRUE);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
PhotosFileName - File name given node

inputs
   DWORD    dwNode - node
   PWSTR    pszFile - filled in
returns
   BOOL - TRUE if successuk
*/
BOOL PhotosFileName (DWORD dwNode, PWSTR pszFile)
{
   HANGFUNCIN;
   PCMMLNode pNode;
   pNode = gpData->NodeGet (dwNode);
   if (!pNode)
      return FALSE;   // unexpected error
   CMem mem;

   MemZero (&mem);

   PWSTR psz;
   psz = NodeValueGet (pNode, gszExtension);
   WCHAR szwTemp[256];
   gpData->DirNode (dwNode, szwTemp);
   MemCat (&mem, szwTemp);
   MemCat (&mem, L"\\photo");
   MemCat (&mem, (int) dwNode);
   MemCat (&mem, psz ? psz : L".jpg");
   gpData->NodeRelease(pNode);
   
   wcscpy (pszFile, (PWSTR) mem.p);
   return TRUE;
}

/***********************************************************************
PhotosSetAsDesktop - Set a photo as a desktop

inputs
   DWORD    dwNode - node
*/
void PhotosSetAsDesktop (DWORD dwNode)
{
   HANGFUNCIN;
   WCHAR szFileName[256];
   if (!PhotosFileName(dwNode, szFileName))
      return;

   // convert to ansi
   char szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, szFileName, -1, szTemp,sizeof(szTemp), 0,0);

   // find out if it's jpeg
   char *pszDot, *pszLastDot;
   for (pszDot = pszLastDot = strchr(szTemp, '.'); pszDot; pszDot = strchr(pszDot+1, '.'))
      pszLastDot = pszDot;
   BOOL fJPEG;
   fJPEG = pszLastDot && !_stricmp(pszLastDot, ".jpg");

   // get the windows directory, and hence the file name that want to write to
   char szWin[256];
   char *pszBitmap = "Dragonfly.bmp";
   GetWindowsDirectory (szWin, sizeof(szWin));
   strcat (szWin, "\\");
   strcat (szWin, pszBitmap);

   // copy or convert
   if (fJPEG)
      JPegToBitmapNoMegaFile (szTemp, szWin);
   else
      CopyFile (szTemp, szWin, FALSE);

   gdwLastPhotoTick = GetTickCount();
   gdLastPhotoDate = Today();

   // set it
   SystemParametersInfo (SPI_SETDESKWALLPAPER, 0, szWin, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}


/***********************************************************************
PhotoRandom - Randomly selects a photo.

returns
   DWORD - Photo node. -1 if can't find
*/
DWORD PhotoRandom (void)
{
   HANGFUNCIN;
   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszPhotosNode);
   if (!pNode)
      return (DWORD)-1;   // unexpected error

   // enumerate dates
   PCListFixed pl;
   pl = EnumMonthYearTree (pNode, gszPhotos);
   if (!pl) {
      // BUGFIX - Wasn't releasing photosnode
      gpData->NodeRelease (pNode);
      return (DWORD)-1;
   }
   if (!pl->Num()) {
      // BUGFIX - Wasn't releasing photosnode
      gpData->NodeRelease (pNode);
      delete pl;
      return (DWORD)-1;
   }

   // from that get everything in month
   DFDATE   date;
   date = *((DFDATE*)pl->Get( rand() % pl->Num() ));
   delete pl;
   gpData->NodeRelease (pNode);

   // now load in the month
   DWORD dwNode;
   pNode = GetPhotosLogNode (date, FALSE, &dwNode);
   if (!pNode)
      return (DWORD)-1;
   pl = NodeListGet (pNode, gszPhotos, TRUE);
   if (!pl) {
      gpData->NodeRelease(pNode);
      return (DWORD)-1;
   }
   if (!pl->Num()) {
      delete pl;
      gpData->NodeRelease(pNode);
      return (DWORD)-1;
   }


   NLG *pnlg;
   pnlg = (NLG*) pl->Get(rand() % pl->Num());
   DWORD dwRet;
   dwRet = (DWORD)pnlg->iNumber;
   delete pl;
   gpData->NodeRelease(pNode);
   return dwRet;
}

/***********************************************************************
PhotosUpdateDesktop - Consider updating desktop to a new photo. This
is called every time a new page pops up. However, for the most part
it looks at the last tick count that a desktop was changed, and if it's
less than 5 minutes then doesn't do anything. However, failnig that
it checks the date. If that's different then it chooses a random photo,
making sure it finds one that's allowed. Failing that it tries another
random photo up to 10 times. If it finds a photo that's used.

inputs
   BOOL     fForce - If TRUE checks date no matter what tick count. Else,
               only if > 5 minutes sine last tick count
returns
   none
*/
void PhotosUpdateDesktop (BOOL fForce)
{
   HANGFUNCIN;
   if (!gfChangeWallpaper)
      return;

   DWORD dwTick;
   dwTick = GetTickCount();
   if (!fForce && (dwTick < gdwLastPhotoTick + 1000*60*5))
      return;
   gdwLastPhotoTick = dwTick;
   if (Today() == gdLastPhotoDate)
      return;

   // it's a new day, find random
   DWORD i;
   for (i = 0; i < 10; i++) {
      DWORD dwNode = PhotoRandom();
      if (dwNode == (DWORD)-1)
         return;

      // make sure it's willing to be displayed
      PCMMLNode pNode;
      BOOL     fOK;
      pNode = gpData->NodeGet(dwNode);
      if (!pNode)
         continue;
      fOK = (BOOL) NodeValueGetInt (pNode, gszWallpaper, 0);
      gpData->NodeRelease (pNode);

      if (!fOK)
         continue;

      // else have it
      PhotosSetAsDesktop(dwNode);
      return;
   }

}


/***********************************************************************
PhotosEntryViewPage - Page callback
*/
BOOL PhotosEntryViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // if user clicks on photo then shell execute edit
         if (p->psz && (p->psz[0] == L'!')) {
            char szTemp[256];
            WideCharToMultiByte (CP_ACP, 0, p->psz+1, -1, szTemp, sizeof(szTemp),0,0);
            ShellExecute (pPage->m_pWindow->m_hWnd, NULL, szTemp, NULL, NULL, SW_SHOWNORMAL);
            return TRUE;
         }
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"ENTRYNAME")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszName);
            MemCat (&gMemTemp, psz ? psz : L"");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"FILE")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszExtension);
            WCHAR szTemp[256];
            gpData->DirNode (gdwEntryNode, szTemp);
            MemCat (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"\\photo");
            MemCat (&gMemTemp, (int) gdwEntryNode);
            MemCat (&gMemTemp, psz ? psz : L".jpg");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"JOURNALNOTES")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszSummary);
            if (psz)
               MemCat (&gMemTemp, psz);

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"ATTENDEES")) {
            PCMMLNode pNode;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwEntryNode);
            if (pNode) {
               DWORD i;
               BOOL fAdded = FALSE;
               for (i = 0; i < MAXPEOPLE; i++) {
                  DWORD dwData;
                  dwData = (DWORD) NodeValueGetInt (pNode, gaszPerson[i], -1);
                  if (dwData == (DWORD)-1)
                     continue;

                  PWSTR psz;
                  psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(dwData));
                  if (!psz)
                     continue;

                  // make a link
                  MemCat (&gMemTemp, L"<a href=v:");
                  MemCat (&gMemTemp, (int) dwData);
                  MemCat (&gMemTemp, L">");
                  MemCatSanitize (&gMemTemp, psz);
                  MemCat (&gMemTemp, L"</a><br/>");
                  fAdded = TRUE;
               }

               if (!fAdded)
                  MemCat (&gMemTemp, L"None");

               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         };

         if (!_wcsicmp(p->pszSubName, L"TIME")) {
            PCMMLNode pNode;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwEntryNode);
            if (pNode) {
               WCHAR szTemp[64];
               DFDATEToString ((DFDATE) NodeValueGetInt (pNode, gszDate, 0), szTemp);
               MemCat (&gMemTemp, szTemp);
               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
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

         if (!_wcsicmp(p->pControl->m_pszName, L"edit")) {
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) gdwEntryNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszWallpaper)) {
            PhotosSetAsDesktop (gdwEntryNode);
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, L"save:", 5)) {
            OPENFILENAME   ofn;
            char  szTemp[256], szSrc[256];
            szTemp[0] = 0;
            WideCharToMultiByte (CP_ACP, 0, p->pControl->m_pszName + 5, -1,
               szTemp, sizeof(szTemp), 0, 0);
            strcpy (szSrc, szTemp);

            char *pszDot, *pszLastDot;
            for (pszDot = pszLastDot = strchr(szTemp, '.'); pszDot; pszDot = strchr(pszDot+1, '.'))
               pszLastDot = pszDot;
            BOOL fJPEG;
            fJPEG = pszLastDot && !_stricmp(pszLastDot, ".jpg");

            memset (&ofn, 0, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;
            if (fJPEG) {
               ofn.lpstrFilter = "JPEG image (*.jpg)\0*.jpg\0\0\0";
               ofn.lpstrDefExt = "jpg";
            }
            else {
               ofn.lpstrFilter = "Bitmap image (*.bmp)\0*.bmp\0\0\0";
               ofn.lpstrDefExt = "bmp";
            }
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Save the photo";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

            // nFileExtension 
            if (!GetSaveFileName(&ofn))
               return TRUE;

            // copy it
            if (!CopyFile (szSrc, szTemp, TRUE))
               pPage->MBWarning (L"The file couldn't be copied.");
            else
               pPage->MBSpeakInformation (L"The file was copied.");
            return TRUE;
         }
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
PhotosEntryEditPage - Page callback
*/
BOOL PhotosEntryEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"FILE")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszExtension);
            WCHAR szTemp[256];
            gpData->DirNode (gdwEntryNode, szTemp);
            MemCat (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"\\photo");
            MemCat (&gMemTemp, (int) gdwEntryNode);
            MemCat (&gMemTemp, psz ? psz : L".jpg");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }


      }
      break;

   case ESCM_INITPAGE:
      {
         PCMMLNode   pEntry;
         pEntry = gpData->NodeGet (gdwEntryNode);
         if (!pEntry)
            break; // error

         // show info
         PWSTR psz;
         PCEscControl pControl;

         // write some stuff into the entry
         psz = NodeValueGet (pEntry, gszSummary);
         pControl = pPage->ControlFind (gszSummary);
         if (psz && pControl)
            pControl->AttribSet(gszText, psz);

         psz = NodeValueGet (pEntry, gszName);
         pControl = pPage->ControlFind (gszName);
         if (psz && pControl)
            pControl->AttribSet(gszText, psz);

         DateControlSet (pPage, gszMeetingDate, (DFDATE) NodeValueGetInt (pEntry, gszDate, 0));

         // all the people
         DWORD i;
         for (i = 0; i < MAXPEOPLE; i++) {
            DWORD dwNum, dwIndex;
            pControl = pPage->ControlFind (gaszPerson[i]);
            dwNum = (DWORD)NodeValueGetInt (pEntry, gaszPerson[i]);
            dwIndex = PeopleBusinessDatabaseToIndex (dwNum);
            if (pControl && (dwIndex != (DWORD)-1))
               pControl->AttribSetInt (gszCurSel, (int) dwIndex);
         }

         // set the cateogyr
         pControl = pPage->ControlFind (gszCategory);
         int iCat;
         iCat = NodeValueGetInt (pEntry, gszCategory, -1);
         DWORD dwIndex;
         dwIndex = JournalDatabaseToIndex ((DWORD)iCat);
         if (pControl && (dwIndex != (DWORD)-1))
            pControl->AttribSetInt (gszCurSel, (int) dwIndex);

         // checkbox
         pControl = pPage->ControlFind(gszWallpaper);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, (BOOL)NodeValueGetInt(pEntry, gszWallpaper, TRUE));
         // release
         gpData->NodeRelease (pEntry);
            

      }
      break;   // follow through

   case ESCM_LINK:
      {
         // this doesn't actually trap and links; it just saves the ifnormation
         // to disk just in case it exits

         // IMPORTANT - if user closes window the changes dont get saved

         // fill in details
         PCMMLNode pNode;
         pNode = gpData->NodeGet (gdwEntryNode);
         if (!pNode)
            break;

         PhotoEditFromControls (pPage, pNode, gdwEntryNode);

         gpData->NodeRelease(pNode);
         gpData->Flush();
      }
      break;   // default behavior

   };

   return DefPage (pPage, dwMessage, pParam);
}


