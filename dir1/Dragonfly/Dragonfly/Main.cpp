/**********************************************************************
Main.cpp - Code that handles the main loop

begun 6/3/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"
#include "buildnum.h"
#include <crtdbg.h>

#define  WM_TASKBAR        (WM_USER+112)


PESCPAGECALLBACK DetermineCallback (DWORD dwID);


HINSTANCE   ghInstance;
char        gszAppPath[256];     // application path
char        gszAppDir[256];      // application directory
PCEscWindow gpWindow;            // main window object
PCDatabase  gpData;              // database
WCHAR       gszUserName[128];    // current user name
CEscSearch  gSearch;             // search
CListFixed  gListHistory;        // history list
HISTORY     gCurHistory;         // current location
BOOL        gfPrinting = FALSE;  // set to true
BOOL        gfFullColor = TRUE;  // set to true to use lots of color, FALSE to tend towards BnW
BOOL        gfMicroHelp = TRUE;  // if TRUE then show small help tips at top of page, else terse
BOOL        gfAskedAboutMicroHelp = FALSE;  // if TRUE then show small help tips at top of page, else terse
BOOL        gfMinimizeDragonflyToTaskbar = FALSE;  // if TRUE minimize dragonfly to a taskbar icon
DWORD       gdwSmallFont = 0;  // 0 for normal font, 1 for smaller, 2 for much smaller
BOOL        gfDisableSounds = FALSE;   // if true then disable sounds
BOOL        gfBugAboutWrap = TRUE;  // if true will bug about daily wrapup
DWORD       gdwLastLogin = 0;       // index of the last login
BOOL        gfAutoLogon = FALSE;
BOOL        gfDisableCursor = FALSE;   // if TRUE then disable cursor
BOOL        gfTemplateR = TRUE;  // if TRUE then use the right menu template
BOOL        gfPrintTwoColumns = TRUE;  // if true print with two columns
HWND        ghWndTaskbar = NULL; // taskbar window
BOOL        gfChangeWallpaper = TRUE;  // if true change the wallpaper from time to time
BOOL        gfFirstTime = TRUE;  // first time using dragonfly
BOOL        gfCheckForBackup = FALSE;  // if this is set checks to see if should backup

WCHAR       gszPowerPassword[64] = L"";   // password that must type to get access after power down
BOOL        gfPoweredDown = FALSE;; // set to true if just powered down
char        gaszJPEGRemap[JPEGREMAP][256]; // remap to file names
DWORD       gadwJPEGRemap[JPEGREMAP] = {
   236, 214, 219, 212, 222, 218, 215, 211, 217, 210, 209, 207, 216, 208, 213};

static char* gszTaskbarClass = "DragonflyTaskbarClass";
static WCHAR gszPageErrorID[] = L"r:298";

/****************************************************************************
TaskbarWndProc - Handles taskbar icon if minimize
*/
long CALLBACK TaskbarWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   HANGFUNCIN;
   
   switch (uMsg) {
   case WM_CREATE:
      {
         // turn on icon?
         NOTIFYICONDATA nid;
         memset (&nid, 0, sizeof(nid));
         nid.cbSize = sizeof(nid);
         nid.hWnd = hWnd;
         nid.uID = 42;
         nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
         nid.uCallbackMessage = WM_TASKBAR;
         nid.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_APPICON));
         strcpy (nid.szTip, "Click this to show Dragonfly.");
         Shell_NotifyIcon (NIM_ADD, &nid);

      }
      HANGFUNCOUT;
      return 0;

   case WM_DESTROY:
      {
         // kill icon
         NOTIFYICONDATA nid;
         memset (&nid, 0, sizeof(nid));
         nid.cbSize = sizeof(nid);
         nid.hWnd = hWnd;
         nid.uID = 42;
         Shell_NotifyIcon (NIM_DELETE, &nid);
      }
      HANGFUNCOUT;
      return 0;

   case WM_TASKBAR:
      switch (lParam) {
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
         {
            // show the main window
            ShowWindow (gpWindow->m_hWnd, SW_SHOW);
            ShowWindow (gpWindow->m_hWnd, SW_RESTORE);
         }
         break;

      default:
         break;
      }
      HANGFUNCOUT;
      return 0;

   }

   // else
   HANGFUNCOUT;
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


static void RegisterCaptureWindow (void)
{
   HANGFUNCIN;
   WNDCLASS wc;

   memset (&wc, 0, sizeof(wc));
   wc.lpfnWndProc = TaskbarWndProc;
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hInstance = ghInstance;
   wc.lpszClassName = gszTaskbarClass;
   RegisterClass (&wc);
   HANGFUNCOUT;
}



/***********************************************************************
GetList - Returns the pointer to the list for the sample FilteredList
controls displayed in help. This is a filler in case there's nothing
for the fitlered list to display.

inputs
returns
   PCListVariable - list
*/
static PCListVariable GetList (void)
{
   HANGFUNCIN;
   static BOOL fListFilled = FALSE;
   static CListVariable list;

   if (!fListFilled) {
      fListFilled = TRUE;

      WCHAR szFillList[] =
         L"Jenny Abernacky\0\0"
         L"Bill Bradburry\0Billy\0"
         L"Jim Briggs\0\0"
         L"Mike Rozak\0Michael\0"
         L"Fred Smith\0Freddie\0"
         L"John Smith\0\0"
         L"Samantha Walters\0\0"
         L"Harry Weatherspoon\0\0"
         L"Carry Wellsford\0\0"
         L"Jone Zeck\0\0"
         L"\0\0";

      PWSTR pCur;
      for (pCur = szFillList; pCur[0]; ) {
         DWORD dwLen;
         dwLen = wcslen(pCur)+1;
         dwLen += wcslen(pCur+dwLen)+1;

         list.Add (pCur, dwLen*2);

         pCur += dwLen;
      }
   }

   HANGFUNCOUT;
   return &list;
}




/*****************************************************************************
DefPage - Default page callback. It handles standard operations
*/
BOOL DefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      // if we have somplace to scroll to then do so
      if (gCurHistory.iVScroll >= 0) {
         pPage->VScroll (gCurHistory.iVScroll);

         // when bring up pop-up dialog often they're scrolled wrong because
         // iVScoll was left as valeu, and they use defpage
         gCurHistory.iVScroll = 0;

         // BUGBUG - 2.0 - putting this invalidate in to hopefully fix a refresh
         // problem when add or move a task in the ProjectView page
         pPage->Invalidate();
      }
      else {
         // if the current page string has a "#" in it then skip to that
         // section
         if (gCurHistory.iSection >= 0)
            pPage->VScrollToSection (gCurHistory.szLink + gCurHistory.iSection);
      }

      if (gfCheckForBackup) {
         gfCheckForBackup = FALSE;
         BackupBugUser (pPage);
      }

      // BUGFIX - ask about microhelp
      if (!gfAskedAboutMicroHelp && gpData && (gpData->Num() > 200) ) {
         if (gfMicroHelp && (IDYES == pPage->MBYesNo (L"Do you want to turn the small help blurbs at the top of the screen off?",
            L"This will allow you to fit more information on the screen."))) {
            gfMicroHelp = FALSE;
            KeySet (gszMicroHelp, gfMicroHelp);
         }
         gfAskedAboutMicroHelp = TRUE;
         KeySet (gszAskedAboutMicroHelp, gfAskedAboutMicroHelp);
      }

      }
      HANGFUNCOUT;
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"Template1")) {
            p->pszSubString = gfPrinting ? L"" :
               ((!gfTemplateR) ? L"<?Template resource=202?>" : L"<?Template resource=301?>");
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"Template2")) {
            p->pszSubString = gfPrinting ? L"" : L"<?Template resource=203?>";
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"Template3")) {
            p->pszSubString = gfPrinting ? L"" : L"<?Template resource=204?>";
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"FAVORITEMENU")) {
            p->pszSubString = FavoritesMenuSub ();
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"FAVORITEMENU2")) {
            p->pszSubString = FavoritesMenuSub (TRUE);
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFCOLOR")) {
            p->pszSubString = gfFullColor ? L"" : L"<comment>";
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFCOLOR")) {
            p->pszSubString = gfFullColor ? L"" : L"</comment>";
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFPCACTIVE")) {
            // BUGFIX - Special PCACTIVE edition
#ifdef PCACTIVE
            p->pszSubString = L"";
#else
            p->pszSubString = L"<comment>";
#endif
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFPCACTIVE")) {
            // BUGFIX - Special PCACTIVE edition
#ifdef PCACTIVE
            p->pszSubString = L"";
#else
            p->pszSubString = L"</comment>";
#endif
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFNOTPRINT")) {
            p->pszSubString = (!gfPrinting) ? L"" : L"<comment>";
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFNOTPRINT")) {
            p->pszSubString = (!gfPrinting) ? L"" : L"</comment>";
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFMICROHELP")) {
            p->pszSubString = gfMicroHelp ? L"" : L"<comment>";
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFMICROHELP")) {
            p->pszSubString = gfMicroHelp ? L"" : L"</comment>";
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WHITECOLOR")) {
            p->pszSubString = gfFullColor ? L"color=#ffffff" : L"";
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"JOURNALTIMER")) {
            p->pszSubString = JournalTimerSubst();
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PROJECTTIMER")) {
            p->pszSubString = ProjectTimerSubst();
            HANGFUNCOUT;
            return TRUE;
         }
      }
            HANGFUNCOUT;
      return FALSE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if ((p->psz[0] == L'w') && (p->psz[1] == L't') && (p->psz[2] == L':')) {
            // pressed a journal timer button, so should bring up UI
            if (JournalTimerEndUI (pPage, _wtoi(p->psz + 3)))
               pPage->Exit (gszRedoSamePage);
            HANGFUNCOUT;
            return TRUE;
         }
         else if ((p->psz[0] == L'p') && (p->psz[1] == L'x') && (p->psz[2] == L':')) {
            // pressed a project timer button, so should bring up UI
            if (ProjectTimerEndUI (pPage, _wtoi(p->psz + 3)))
               pPage->Exit (gszRedoSamePage);
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, gszPrint)) {
            // print dialog box
            PRINTDLG pd;
            memset (&pd, 0, sizeof(pd));
            pd.lStructSize = sizeof(pd);
            pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
            pd.hwndOwner = pPage->m_pWindow->m_hWnd;
            pd.nFromPage = 0;
            pd.nToPage = 0;
            pd.nCopies = 1;

            if (!PrintDlg (&pd)) {
               HANGFUNCOUT;
               return TRUE;  // cancelled or error
            }

            // Set gfPrinting to TRUE so that wont do templates.

            // user clicked the hello world sample
            CEscWindow cWindow;
            BOOL fColor;
            fColor = gfFullColor;
            gfFullColor = FALSE;
            gfPrinting = TRUE;
            cWindow.InitForPrint (pd.hDC, ghInstance, pPage->m_pWindow->m_hWnd);

            BOOL fTwoColumns;
            fTwoColumns = gfPrintTwoColumns;
            switch (gCurHistory.dwResource) {
            case IDR_MMLCALENDARCOMBO:
            case IDR_MMLCALENDAR:
            case IDR_MMLCALENDARYEARLY:
            case IDR_MMLCALENDARYEARLY2:
               // BUGFIX - If it's the combo-view then only print with one column
               fTwoColumns = FALSE;
            }

            if (fTwoColumns) {
               cWindow.m_PrintInfo.wOtherScale = 0x100 * 2 / 3;
               cWindow.m_PrintInfo.wFontScale = 0x100 * 2 / 3;
               cWindow.m_PrintInfo.wColumns = 2;
            }
            else {
               cWindow.m_PrintInfo.wOtherScale = 0x100;
               cWindow.m_PrintInfo.wFontScale = 0x100;
               cWindow.m_PrintInfo.wColumns = 1;
            }
            cWindow.PrintPageLoad (ghInstance, gCurHistory.dwResource,
               DetermineCallback(gCurHistory.dwResource));
            cWindow.Print ();
            gfPrinting = FALSE;
            gfFullColor = fColor;

            // delete the DC
            DeleteDC (pd.hDC);

            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"takenotes")) {
            DWORD dwNode;
            dwNode = PhoneCall (pPage, FALSE, FALSE, (DWORD)-1, L"");
            if (dwNode == (DWORD)-1)
               return TRUE;   // error

            // exit this, editing the new node
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwNode);
            pPage->Exit (szTemp);
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!wcsncmp(p->psz, L"qzaddmeeting", 12)) {
            if (SchedMeetingShowAddUI(pPage, _wtoi(p->psz+12))) {
               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Meeting added.");

               pPage->Exit (gszRedoSamePage);
            }
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!wcsncmp(p->psz, L"qzaddcall", 9)) {
            if (PhoneCallShowAddUI(pPage, _wtoi(p->psz+9))) {
               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Call scheduled.");

               pPage->Exit (gszRedoSamePage);
            }
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"qzaddperson")) {
            if (PeopleQuickAdd(pPage) != (DWORD)-1) {
               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Person added.");

               pPage->Exit (gszRedoSamePage);
            }
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"qzaddbusiness")) {
            if (BusinessQuickAdd(pPage) != (DWORD)-1) {
               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Business added.");

               pPage->Exit (gszRedoSamePage);
            }
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"qzaddpersonbusiness")) {
            if (PeopleBusinessQuickAdd(pPage) != (DWORD)-1) {
               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Person or business added.");

               pPage->Exit (gszRedoSamePage);
            }
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!wcsncmp(p->psz, L"qzaddtask", 9)) {
            if (WorkTaskShowAddUI(pPage, _wtoi(p->psz+9))) {
               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Task added.");

               pPage->Exit (gszRedoSamePage);
            }
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"qzaddnote")) {
            if (NotesQuickAddNote(pPage)) {
               pPage->Exit (gszRedoSamePage);
            }
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!wcsncmp(p->psz, L"qzaddreminder", 13)) {
            if (ReminderQuickAdd(pPage, _wtoi(p->psz+13))) {
               pPage->Exit (gszRedoSamePage);
            }
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"addtofavorites")) {
            if (FavoritesMenuAdd (pPage))
               pPage->Exit (gszRedoSamePage);
            HANGFUNCOUT;
            return TRUE;
         }
      }
            HANGFUNCOUT;
      return FALSE;

   case ESCN_FILTEREDLISTQUERY:
      {
         // this is a message sent by the filtered list controls
         // used as samples in help.

         PESCNFILTEREDLISTQUERY p = (PESCNFILTEREDLISTQUERY) pParam;

         if (!_wcsicmp(p->pszListName, gszProjectTask)) {
            p->pList = ProjectGetTaskList();
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszListName, gszSubProject)) {
            p->pList = ProjectGetSubProjectList();
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszListName, gszPerson)) {
            p->pList = PeopleFilteredList();
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszListName, gszBusiness)) {
            p->pList = BusinessFilteredList();
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszListName, gszPersonBusiness)) {
            p->pList = PeopleBusinessFilteredList();
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszListName, gszJournal)) {
            p->pList = JournalListVariable();
            HANGFUNCOUT;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszListName, gszNotes)) {
            p->pList = NotesFilteredList();
            HANGFUNCOUT;
            return TRUE;
         }

         // if all fails then make something up
         p->pList = GetList();
      }
            HANGFUNCOUT;
      return TRUE;

   case ESCN_FILTEREDLISTCHANGE:
      {
         PESCNFILTEREDLISTCHANGE p = (PESCNFILTEREDLISTCHANGE) pParam;

         // only care if selected add
         if (p->iCurSel != -2)
            break;

         // get the type of list
         WCHAR szListName[128];
         DWORD dwNeeded;
         szListName[0] = 0;
         p->pControl->AttribGet(L"ListName", szListName, sizeof(szListName), &dwNeeded);

         // person chose add so bring up the appropriate UI
         DWORD dwNew;
         if (!_wcsicmp(szListName, gszSubProject)) {
            dwNew = ProjectAddSubProject(pPage);
            p->pControl->AttribSetInt (gszCurSel, (int)dwNew);
         }
         else if (!_wcsicmp(szListName, gszPerson)) {
            dwNew = PeopleQuickAdd(pPage);
            p->pControl->AttribSetInt (gszCurSel, (int)dwNew);
         }
         else if (!_wcsicmp(szListName, gszBusiness)) {
            dwNew = BusinessQuickAdd(pPage);
            p->pControl->AttribSetInt (gszCurSel, (int)dwNew);
         }
         else if (!_wcsicmp(szListName, gszPersonBusiness)) {
            dwNew = PeopleBusinessQuickAdd(pPage);
            p->pControl->AttribSetInt (gszCurSel, (int)dwNew);
         }
         else if (!_wcsicmp(szListName, gszJournal)) {
            dwNew = JournalQuickAdd(pPage);
            p->pControl->AttribSetInt (gszCurSel, (int)dwNew);
         }
         else if (!_wcsicmp(szListName, gszNotes)) {
            dwNew = NotesQuickAdd(pPage);
            p->pControl->AttribSetInt (gszCurSel, (int)dwNew);
         }

      }
            HANGFUNCOUT;
      return TRUE;

   case ESCM_ACTIVATE:
      {
         PESCMACTIVATE p = (PESCMACTIVATE) pParam;

         if (p->fMinimized) {
            if (!ghWndTaskbar && (pPage->m_pWindow == gpWindow) && gfMinimizeDragonflyToTaskbar) {
               ghWndTaskbar = CreateWindow (
                  gszTaskbarClass, "Dragonfly Taskbar Window",
                  0,
                  0,0,0,0,
                  NULL, NULL,
                  ghInstance, NULL);
               ShowWindow (gpWindow->m_hWnd, SW_HIDE);
            }
         }
         else {   // not miniized
            if (ghWndTaskbar) {
               DestroyWindow (ghWndTaskbar);
               ShowWindow (gpWindow->m_hWnd, SW_SHOW);
               ghWndTaskbar = NULL;
            }
         }
      }
      break;

   case ESCM_POWERBROADCAST:
      {
         PESCMPOWERBROADCAST p = (PESCMPOWERBROADCAST) pParam;

         if (gszPowerPassword[0] && ((p->dwPowerEvent == PBT_APMSUSPEND) || (p->dwPowerEvent == PBT_APMRESUMESUSPEND)))
            gfPoweredDown = TRUE;
      }
      break;   // default behavious

   };


   HANGFUNCOUT;
   return FALSE;
}

/*****************************************************************************
AboutPage - Search page callback. It handles standard operations
*/
BOOL AboutPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"DFVERSION")) {
            static WCHAR szVersion[16];   // for about box
            MultiByteToWideChar (CP_ACP, 0, BUILD_NUMBER, -1, szVersion, sizeof(szVersion)/2);
            p->pszSubString = szVersion;
            HANGFUNCOUT;
            return TRUE;
         }
      }
      break;   // default behavior

   };


   HANGFUNCOUT;
   return DefPage (pPage, dwMessage, pParam);
}

/***************************************************************************
ViewNodeInfo - Given a node number, this loads it and determines what type
it is. From there it figures out what page to load, and calls any initialization
to the module.

inputs
   DWORD    dwNode - node into the database
   PHISTORY pHistory - history
   BOOL     fEdit - if TRUE then edit
returns
   BOOl - TRUE if succeded
*/
BOOL ViewNodeInfo (DWORD dwNode, PHISTORY pHistory, BOOL fEdit)
{
   HANGFUNCIN;
   // load it
   PCMMLNode   pNode;
   pNode = gpData->NodeGet(dwNode);
   if (!pNode) {
      HANGFUNCOUT;
      return FALSE;
   }

   // get the name
   WCHAR szTemp[256];
   wcscpy (szTemp, pNode->NameGet());

   // release
   gpData->NodeRelease(pNode);

   pHistory->dwData = dwNode;

   // based on the name
   if (!_wcsicmp(szTemp, gszProjectNode)) {
      // it's a project node.
      pHistory->dwResource = IDR_MMLPROJECTVIEW;

      // tell the project
      if (ProjectSetView(dwNode)) {
         HANGFUNCOUT;
         return TRUE;
      }
   }
   else if (!_wcsicmp(szTemp, gszPersonNode)) {
      // it's a project node.
      pHistory->dwResource = fEdit ? IDR_MMLPERSONEDIT : IDR_MMLPERSONVIEW;

      // tell the project
      if (PeopleSetView(dwNode)) {
         HANGFUNCOUT;
         return TRUE;
      }
   }
   else if (!_wcsicmp(szTemp, gszBusinessNode)) {
      // it's a project node.
      pHistory->dwResource = fEdit ? IDR_MMLBUSINESSEDIT : IDR_MMLBUSINESSVIEW;

      // tell the project
      if (PeopleSetView(dwNode)) {
         HANGFUNCOUT;
         return TRUE;
      }
   }
   else if (!_wcsicmp(szTemp, gszMeetingNotesNode)) {
      // it's a project node.
      pHistory->dwResource = fEdit ? IDR_MMLMEETINGNOTESEDIT : IDR_MMLMEETINGNOTESVIEW;

      // tell the project
      if (SchedSetView(dwNode)) {
         HANGFUNCOUT;
         return TRUE;
      }
   }
   else if (!_wcsicmp(szTemp, gszPhoneNotesNode)) {
      // it's a project node.
      pHistory->dwResource = fEdit ? IDR_MMLPHONENOTESEDIT : IDR_MMLPHONENOTESVIEW;

      // tell the project
      if (PhoneSetView(dwNode)) {
         HANGFUNCOUT;
         return TRUE;
      }
   }
   else if (!_wcsicmp(szTemp, gszCalendarLogDay)) {
      // it's a project node.
      pHistory->dwResource = IDR_MMLCALENDARLOG;

      // tell the project
      if (CalendarSetView(dwNode)) {
         HANGFUNCOUT;
         return TRUE;
      }
   }
   else if (!_wcsicmp(szTemp, gszJournalCategoryNode)) {
      // it's a project node.
      pHistory->dwResource = IDR_MMLJOURNALCAT;

      // tell the project
      JournalCatSet(dwNode);
         HANGFUNCOUT;
      return TRUE;
   }
   else if (!_wcsicmp(szTemp, gszJournalEntryNode)) {
      // it's a project node.
      pHistory->dwResource = fEdit ? IDR_MMLJOURNALENTRYEDIT : IDR_MMLJOURNALENTRYVIEW;

      // tell the project
      JournalEntrySet(dwNode);
         HANGFUNCOUT;
      return TRUE;
   }
   else if (!_wcsicmp(szTemp, gszPhotosEntryNode)) {
      // it's a project node.
      pHistory->dwResource = fEdit ? IDR_MMLPHOTOSENTRYEDIT : IDR_MMLPHOTOSENTRYVIEW;

      // tell the project
      PhotosEntrySet(dwNode);
         HANGFUNCOUT;
      return TRUE;
   }
   else if (!_wcsicmp(szTemp, gszMemoryEntryNode)) {
      // it's a project node.
      pHistory->dwResource = fEdit ? IDR_MMLMEMORYEDIT : IDR_MMLMEMORYVIEW;

      // tell the project
      MemoryEntrySet(dwNode);
         HANGFUNCOUT;
      return TRUE;
   }
   else if (!_wcsicmp(szTemp, gszMemoryListNode)) {
      // it's a project node.
      pHistory->dwResource = IDR_MMLMEMORYLIST;

      // tell the project
      MemoryListSet(dwNode);
         HANGFUNCOUT;
      return TRUE;
   }
   else if (!_wcsicmp(szTemp, gszWrapUpNode)) {
      // it's a project node.
      pHistory->dwResource = fEdit ? IDR_MMLWRAPUPEDIT : IDR_MMLWRAPUPVIEW;

      // tell the project
      WrapUpSet(dwNode);
         HANGFUNCOUT;
      return TRUE;
   }
   else if (!_wcsicmp(szTemp, gszArchiveEntryNode)) {
      // it's a project node.
      pHistory->dwResource = fEdit ? IDR_MMLARCHIVEEDIT : IDR_MMLARCHIVEVIEW;

      // tell the project
      ArchiveEntrySet(dwNode);
         HANGFUNCOUT;
      return TRUE;
   }
   // else
   pHistory->dwData = (DWORD)-1;
         HANGFUNCOUT;
   return FALSE;
}


/*****************************************************************************
ParseURL - Parses a URL into a HISTORY structure.

inputs
   PWSTR    pszURL - url
   PHISTORY pHistory - filled in
returns
   none
*/
void ParseURL (PWSTR pszURL, PHISTORY pHistory)
{
   HANGFUNCIN;
   memset (pHistory, 0, sizeof(*pHistory));

   if (!pszURL) {
      HANGFUNCOUT;
      return;
   }

   pHistory->fResolved = TRUE;

   // copy over the string
   if (wcslen(pszURL) < sizeof(pHistory->szLink)/2)
      wcscpy (pHistory->szLink, pszURL);
   else {
      memcpy (pHistory->szLink, pszURL, sizeof(pHistory->szLink)/2);
      pHistory->szLink[sizeof(pHistory->szLink)/2-1] = 0;
   }

   // see if there's a #
   pHistory->iVScroll = -1;
   pHistory->iSection = -1;
   PWSTR psz;
   psz = wcschr(pszURL, L'#');
   if (psz) {
      pHistory->iSection = ((int) (PBYTE) psz - (int) (PBYTE) pszURL) / 2 + 1;
   }

   // figure out what resource
   psz = pszURL;
   if (!_wcsnicmp (psz, L"r:", 2)) {
      // it's another page
      DWORD dwPage;
      dwPage = _wtoi (psz + 2);

      pHistory->dwResource = dwPage;
   }
   else if (!_wcsnicmp (psz, L"v:", 2) || !_wcsnicmp (psz, L"e:", 2) ) {
      // it's a view
      DWORD dwNode;
      dwNode = _wtoi (psz+2);

      if (!ViewNodeInfo (dwNode, pHistory, (psz[0] == L'e') || (psz[0] == L'E') ))
         pHistory->dwResource = IDR_MMLNYI;
      HANGFUNCOUT;
      return;
   }
   else if (!_wcsicmp (psz, gszBack)) {
      PHISTORY pOld;
      pOld = (PHISTORY) gListHistory.Get (0);
      if (!pOld) {
         // cant go back, so go to starting url
         ParseURL (gszStartingURL, pHistory);
         HANGFUNCOUT;
         return;
      }


      // else, use that
      *pHistory = *pOld;

      // must do some reparsing if v:
      psz = pHistory->szLink;
      if (!_wcsnicmp (psz, L"v:", 2) || !_wcsnicmp (psz, L"e:", 2) ) {
         // it's a view
         DWORD dwNode;
         dwNode = _wtoi (psz+2);

         if (!ViewNodeInfo (dwNode, pHistory, (psz[0] == L'e') || (psz[0] == L'E') ))
            pHistory->dwResource = IDR_MMLNYI;
      }

      // delete the back
      gListHistory.Remove (0);

      HANGFUNCOUT;
      return;
   }
   else {
      // quit
      pHistory->fResolved = FALSE;
      HANGFUNCOUT;
      return;
   }


   // assume data is -1
   pHistory->dwData = (DWORD)-1;
   HANGFUNCOUT;
}


/*****************************************************************************
DetermineCallback - Given a resource ID, this returns the pagecallback
to be used.

inputs
   DWORD       dwID - MML resource iD
returns
   PCESCPAGECALLBACK - callback
*/
PESCPAGECALLBACK DetermineCallback (DWORD dwID)
{
   HANGFUNCIN;
   switch (dwID) {
   case IDR_MMLSEARCH:
      return SearchPage;
   case IDR_MMLABOUT:
      return AboutPage;
   case IDR_MMLPROJECT:
      return ProjectListPage;
   case IDR_MMLPROJECTADD:
      return ProjectAddPage;
   case IDR_MMLPROJECTREMOVE:
      return ProjectRemovePage;
   case IDR_MMLPROJECTVIEW:
      return ProjectViewPage;
   case IDR_MMLTODAY:
      return TodayPage;
   case IDR_MMLTOMORROW:
      return TomorrowPage;
   case IDR_MMLREMINDERS:
      return RemindersPage;
   case IDR_MMLTASKS:
      return WorkTasksPage;
   case IDR_MMLEVENT:
      return EventDaysPage;
   case IDR_MMLPEOPLEEXPORT:
      return PeopleExportPage;
   case IDR_MMLNEWPERSON:
      return PeopleNewPage;
   case IDR_MMLNEWBUSINESS:
      return BusinessNewPage;
   case IDR_MMLPEOPLELOOKUP:
      return PeopleLookUpPage;
   case IDR_MMLPEOPLEPRINT:
      return PeoplePrintPage;
   case IDR_MMLPERSONVIEW:
      return PeoplePersonViewPage;
   case IDR_MMLBUSINESSVIEW:
      return PeopleBusinessViewPage;
   case IDR_MMLPERSONEDIT:
      return PeoplePersonEditPage;
   case IDR_MMLBUSINESSEDIT:
      return PeopleBusinessEditPage;
   case IDR_MMLPEOPLELIST:
      return PeopleListPage;
   case IDR_MMLPEOPLE:
      return PeoplePage;
   case IDR_MMLMEETINGS:
      return SchedMeetingsPage;
   case IDR_MMLMEETINGNOTESEDIT:
      return SchedMeetingNotesEditPage;
   case IDR_MMLMEETINGNOTESVIEW:
      return SchedMeetingNotesViewPage;
   case IDR_MMLPHONENOTESEDIT:
      return PhoneNotesEditPage;
   case IDR_MMLPHONENOTESVIEW:
      return PhoneNotesViewPage;
   case IDR_MMLPHONEMAKECALL:
      return PhoneMakeCallPage;
   case IDR_MMLPHONESPEEDNEW:
      return PhoneSpeedNewPage;
   case IDR_MMLPHONESPEEDREMOVE:
      return PhoneSpeedRemovePage;
   case IDR_MMLPHONE:
      return PhonePage;
   case IDR_MMLPHONECALLLOG:
      return PhoneCallLogPage;
   case IDR_MMLCALENDAR:
      return CalendarPage;
   case IDR_MMLCALENDARCOMBO:
      return CalendarComboPage;
   case IDR_MMLCALENDARYEARLY:
   case IDR_MMLCALENDARYEARLY2:  // both use the same page
      return CalendarYearlyPage;
   case IDR_MMLCALENDARHOLIDAYS:
      return CalendarHolidaysPage;
   case IDR_MMLCALENDARLIST:
      return CalendarListPage;
   case IDR_MMLCALENDARSUMMARY:
      return CalendarSummaryPage;
   case IDR_MMLCALENDARLOG:
      return CalendarLogPage;
   case IDR_MMLNOTES:
      return NotesPage;
   case IDR_MMLQUOTES:
      return QuotesPage;
   case IDR_MMLPLANNER:
      return PlannerPage;
   case IDR_MMLPLANNERBREAKS:
      return PlannerBreaksPage;
   case IDR_MMLJOURNAL:
      return JournalPage;
   case IDR_MMLJOURNALCATADD:
      return JournalCatAddPage;
   case IDR_MMLJOURNALCATREMOVE:
      return JournalCatRemovePage;
   case IDR_MMLJOURNALCAT:
      return JournalCatPage;
   case IDR_MMLJOURNALENTRYEDIT:
      return JournalEntryEditPage;
   case IDR_MMLJOURNALENTRYVIEW:
      return JournalEntryViewPage;
   case IDR_MMLPHOTOSENTRYVIEW:
      return PhotosEntryViewPage;
   case IDR_MMLPHOTOSENTRYEDIT:
      return PhotosEntryEditPage;
   case IDR_MMLMEMORYLANE:
      return MemoryLanePage;
   case IDR_MMLMEMORYEDIT:
      return MemoryEditPage;
   case IDR_MMLMEMORYVIEW:
      return MemoryViewPage;
   case IDR_MMLMEMORYLIST:
      return MemoryListPage;
   case IDR_MMLMEMORYSUGGEST:
      return MemorySuggestPage;
   case IDR_MMLDEEPPEOPLE:
      return DeepPeoplePage;
   case IDR_MMLDEEPSHORT:
      return DeepShortPage;
   case IDR_MMLDEEPLONG:
      return DeepLongPage;
   case IDR_MMLDEEPCHANGE:
      return DeepChangePage;
   case IDR_MMLDEEPWORLD:
      return DeepWorldPage;
   case IDR_MMLWRAPUPEDIT:
      return WrapUpEditPage;
   case IDR_MMLWRAPUPVIEW:
      return WrapUpViewPage;
   case IDR_MMLWRAPUP:
      return WrapUpPage;
   case IDR_MMLARCHIVEOPTIONS:
      return ArchiveOptionsPage;
   case IDR_MMLARCHIVE:
      return ArchivePage;
   case IDR_MMLARCHIVEVIEW:
      return ArchiveViewPage;
   case IDR_MMLARCHIVEEDIT:
      return ArchiveEditPage;
   case IDR_MMLARCHIVELIST:
      return ArchiveListPage;
   case IDR_MMLREGISTER:
      return RegisterPage;
   case IDR_MMLBACKUP:
      return BackupPage;
   case IDR_MMLCHANGEPASSWORD:
      return ChangePasswordPage;
   case IDR_MMLPASSWORDSUSPEND:
      return PasswordSuspendPage;
   case IDR_MMLMISCUI:
      return MiscUIPage;
   case IDR_MMLFAVORITES:
      return FavoritesPage;
   case IDR_MMLPASSWORD:
      return PasswordPage;
   case IDR_MMLTIMEZONES:
      return TimeZonesPage;
   case IDR_MMLTIMEMOON:
      return TimeMoonPage;
   case IDR_MMLTIMESUNLIGHT:
      return TimeSunlightPage;
   case IDR_MMLPHOTOS:
      return PhotosPage;

   default:
      return DefPage;
   }
}


/*****************************************************************************
AlarmTimerProc - Checks to see if any alarms are to go off once a minute.
*/
void CALLBACK AlarmTimerProc (HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
   HANGFUNCIN;
   DFDATE today = Today();
   DFTIME now = Now();

   // if the UI is visible then skip
   if (AlarmIsVisible ())
      return;

   // call it in
   RemindersCheckAlarm (today, now);
   SchedCheckAlarm (today, now);
   PhoneCheckAlarm (today, now);
   HANGFUNCOUT;
}


/*****************************************************************************
MainLoop - Does the main loop for the application, bringing up pages.

inputs
   none
returns
   none
*/
void MainLoop (void)
{
   HANGFUNCIN;
   // BUGFIX - Reset powered down
   gfPoweredDown = FALSE;

   // BUGFIX - set a timer for alarms
   DWORD dwID;
   dwID = SetTimer (NULL, 0, 60*1000, AlarmTimerProc);

   // BUGFIX - Get the date that first started to use. If no entry then write out
   // today. If PCACTIVE then allow special dispensation for six months
   PCMMLNode pNode;
   BOOL     fMoreThanSixMonths, fMoreThanThreeMonths;
   fMoreThanSixMonths = TRUE;
   fMoreThanThreeMonths = TRUE;
   pNode = FindMajorSection (gszMiscNode);
   if (pNode) {
      DFDATE df = (DFDATE) NodeValueGetInt (pNode, gszFirstDate, 0);
      if (!df) {
         df = Today();
         NodeValueSet (pNode, gszFirstDate, (int) df);
      }

#ifdef PCACTIVE
      if ( (DFDATEToMinutes(Today()) - DFDATEToMinutes (df)) / 60 / 24 <= 31 * 6)
         fMoreThanSixMonths = FALSE;
      if ( (DFDATEToMinutes(Today()) - DFDATEToMinutes (df)) / 60 / 24 <= 31 * 3)
         fMoreThanThreeMonths = FALSE;
#endif   // PCACTIVE
      gpData->NodeRelease (pNode);
   }

   // start out on "today"
   PWSTR pszStart;
   pszStart = FavoritesGetStartup();
   if (fMoreThanThreeMonths && !RegisterIsRegistered() && (gpData->Num() > 100)) {
      // if user is not registered and it's more than the 100th data entry, then
      // remind to register when first start
      pszStart = gszRegisterURL;
   }

   // BUGFIX: if it's the first time then start at overview
   if (gfFirstTime) {
      WCHAR pszOverview[] = L"r:131";
      gfFirstTime = FALSE;
      KeySet (gszFirstTime, gfFirstTime);
      pszStart = pszOverview;
   }
   gfCheckForBackup  = TRUE;

   // BUGFIX - if no start then go to page error
   if (!pszStart)
      pszStart = gszPageErrorID;

   ParseURL (pszStart, &gCurHistory);

   while (gCurHistory.fResolved) {
      PWSTR psz;
      
      // consider updating photos
      PhotosUpdateDesktop (FALSE);

      // set the font scale
      DWORD dwScale;
      dwScale = 0x100;
      switch (gdwSmallFont) {
      case 1:
         dwScale = 0xe0;
         break;
      case 2:
         dwScale = 0xc0;
         break;
      }
      EscFontScaleSet ((WORD) dwScale);
      psz = gpWindow->PageDialog (ghInstance, gCurHistory.dwResource, DetermineCallback(gCurHistory.dwResource), NULL);

      // BUGFIX: if suspended then verify password
      if (gfPoweredDown) {
         if (!PasswordSuspendVerify (gpWindow))
            psz = L"[close]";

         gfPoweredDown = FALSE;
      }

      // BUGFIX - If error in loading go to page error
      if (!psz) {
         psz = gszPageErrorID;
      }

#ifdef _DEBUG
      char  szTemp[512];
      if (psz) {
         OutputDebugString ("Page returned: ");
         WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0,0);
         strcat (szTemp, "\r\n");
         OutputDebugString (szTemp);
      }
#endif

      // intercept if not registered
      if (fMoreThanSixMonths && !RegisterIsRegistered() && (psz[0] == L'r')) {
         BOOL  fIntercept;
         DWORD dwNum = gpData->Num();
         if (dwNum > 1500) // BUGFIX - Was 3000
            fIntercept = TRUE;
         else if (dwNum > 1000)  // BUGFIX - Was 1500
            fIntercept = (rand() % 4) ? FALSE : TRUE;
         else if (dwNum > 500)   // BUGFIX - Was 750
            fIntercept = (rand() % 10) ? FALSE : TRUE;
         else
            fIntercept = FALSE;

         if (fIntercept)
            psz = gszRegisterURL;
      }

      // as long as we're not back, add the old one to current hisotry
      if (!_wcsicmp (psz, gszRedoSamePage)) {
         // page requested to redo itself
         gCurHistory.iVScroll = gpWindow->m_iExitVScroll;
         continue;
      }
      else if (_wcsicmp(psz, gszBack)) {
         gCurHistory.iVScroll = gpWindow->m_iExitVScroll;
         gListHistory.Insert (0, &gCurHistory);
      }

      // now partse
      ParseURL (psz, &gCurHistory);
   }

   // kill the timer
   KillTimer (NULL, dwID);
   HANGFUNCOUT;
}

/*****************************************************************************
WinMain */
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
   EscOutputDebugStringAlwaysFlush (TRUE);
   HANGFUNCIN;

#ifdef _DEBUG
   // Get current flag
   int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

   // Turn on leak-checking bit
   tmpFlag |= _CRTDBG_LEAK_CHECK_DF; // | _CRTDBG_CHECK_ALWAYS_DF;

   // Set flag to the new value
   _CrtSetDbgFlag( tmpFlag );

   // test
   //char *p;
   //p = (char*)MYMALLOC (42);
   // p[43] = 0;
#endif // _DEBUG

   // BUGFIX - Only one instance of dragonfly running at once
   HANDLE hMutex;
   HANGFUNCIN;
   hMutex = CreateMutex (NULL, TRUE, "Dragonfly check for run once");
   HANGFUNCIN;
   if (GetLastError() == ERROR_ALREADY_EXISTS) {
      MessageBox (NULL, "Only one copy of Dragonfly can be run at a time.", "Dragonfly", MB_OK);
      return -1;  // already running
   }

   // initialize random
   srand(GetTickCount() + Today() + Now());

   // initialize class for taskbar window
   RegisterCaptureWindow ();

   // get registry valyes
   gfAskedAboutMicroHelp = KeyGet (gszAskedAboutMicroHelp, FALSE);
   gfFullColor = KeyGet (gszFullColor, TRUE);
   gfMicroHelp = KeyGet (gszMicroHelp, TRUE);
   gfMinimizeDragonflyToTaskbar = KeyGet (gszMinimizeDragonflyToTaskbar, FALSE);
   gdwSmallFont = KeyGet (gszSmallFont, FALSE);
   gdwLastLogin = KeyGet (gszLastLogin, 0);
   gfAutoLogon = KeyGet (gszAutoLogon, FALSE);
   gfDisableSounds = KeyGet (gszDisableSounds, FALSE);
   gfBugAboutWrap = KeyGet (gszBugAboutWrap, TRUE);
   gfDisableCursor = KeyGet (gszDisableCursor, FALSE);
   gfPrintTwoColumns = KeyGet (gszPrintTwoColumns, TRUE);
   gfChangeWallpaper = KeyGet (gszChangeWallpaper, TRUE);
   gfFirstTime = KeyGet (gszFirstTime, TRUE);
   gfTemplateR = KeyGet (gszTemplateR, TRUE);

   // remap
   char szTemp[64];
   DWORD i;
   for (i = 0; i < JPEGREMAP; i++) {
      HANGFUNCIN;
      gaszJPEGRemap[i][0] = 0;
      sprintf (szTemp, "ImageRemap%d", i);
      KeyGetString (szTemp, gaszJPEGRemap[i], sizeof(gaszJPEGRemap[i]));
      if (gaszJPEGRemap[i][0]) {
         WCHAR szw[256];
         MultiByteToWideChar (CP_ACP, 0, gaszJPEGRemap[i], -1, szw, 256);
         HANGFUNCIN;
         EscRemapJPEG (gadwJPEGRemap[i], szw);
      }
   }

   // store away some globals
   ghInstance = hInstance;
   // store the directory away
   GetModuleFileName (hInstance, gszAppPath, sizeof(gszAppPath));
   strcpy (gszAppDir, gszAppPath);
   char  *pCur;
   for (pCur = gszAppDir + strlen(gszAppDir); pCur >= gszAppDir; pCur--)
      if (*pCur == '\\') {
         pCur[1] = 0;
         break;
      }


   // initialize
   gListHistory.Init (sizeof(HISTORY));
   EscInitialize (L"mikerozak@bigpond.com", 2511603559, 0);
   if (gfDisableSounds)
      EscSoundsSet (0);
   if (gfDisableCursor)
      EscTraditionalCursorSet (TRUE);

   {
   RECT  r;
   r.left = GetSystemMetrics (SM_CXSCREEN) / 8;
   r.right = GetSystemMetrics (SM_CXSCREEN) - r.left;
   r.top = 32;
   r.bottom = GetSystemMetrics (SM_CYSCREEN) - r.top;
   CEscWindow  cWindow;
   
   cWindow.Init (hInstance, NULL, 0, &r);
   cWindow.IconSet (LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_APPICON)));

   gpWindow = &cWindow;

   if (!LogOnMain())
      goto done;

   // turn on archiving
   ArchiveInit();

   // BUGFIX - Get the power-down password
   // what's is supposed to be
   PCMMLNode pNode;
   pNode = FindMajorSection (gszMiscNode);
   if (pNode) {
      PWSTR psz = NodeValueGet (pNode, gszPP);
      if (psz)
         wcscpy (gszPowerPassword, psz);
      gpData->NodeRelease (pNode);
   }

   // main loop
   MainLoop ();
   }

done:
   ProjectShutDown();
   LogOff();
   EscOutputDebugStringClose ();
   EscUninitialize();
   ArchiveShutDown();

   if (ghWndTaskbar)
      DestroyWindow (ghWndTaskbar);
   if (hMutex)
      ReleaseMutex (hMutex);

   // write reg values
   KeySet (gszFullColor, gfFullColor);
   KeySet (gszMicroHelp, gfMicroHelp);
   KeySet (gszSmallFont, gdwSmallFont);
   KeySet (gszLastLogin, gdwLastLogin);
   KeySet (gszAutoLogon, gfAutoLogon);
   KeySet (gszDisableSounds, gfDisableSounds);
   KeySet (gszBugAboutWrap, gfBugAboutWrap);
   KeySet (gszDisableCursor, gfDisableCursor);
   KeySet (gszTemplateR, gfTemplateR);
   KeySet (gszPrintTwoColumns, gfPrintTwoColumns);
   KeySet (gszMinimizeDragonflyToTaskbar, gfMinimizeDragonflyToTaskbar);
   KeySet (gszChangeWallpaper, gfChangeWallpaper);
   KeySet (gszFirstTime, gfFirstTime);
   for (i = 0; i < JPEGREMAP; i++) {
      sprintf (szTemp, "ImageRemap%d", i);
      KeySetString (szTemp, gaszJPEGRemap[i]);
   }

#ifdef _DEBUG
   _CrtCheckMemory ();
#endif // DEBUG
   return 0;
}

// BUGBUG - 2.0 - Under "to do" have a "Decisions" tab where people can write down
// decisions need to make and work out pros and cons.

// BUGBUG - 2.0 - at some point in the far future may want to allow people to record
// audio notes, and to record meetings/phone conversations. Could potentially
// hook to speech recognition and do transcription...

// BUGBUG - 2.0 - have gotten rid of photos for now. Eventually add them back in.

// BUGBUG - 2.0 - decisions to make section - eventually do this. Some feature to help
// people make decisions

// BUGBUG - 2.0 - Put in analysis section to analyze all the data that have collected


// BUGBUG - 2.0 - API/UI to show info such as directory, disk space used, number of entries

// BUGBUG - 2.0 - "to do" list for the next time in town, or whatever

// BUGBUG - 2.0 - backup to the printer is left for a 2.0 version

// BUGBUG - 2.0 - change directory is left for 2.0

// BUGBUG - 2.0 - statistics (file size, etc.) is left for 2.0


/* BUGBUG - feature request

Mark Oswood <moswood9087@charter.net>
By way of brief introduction, I’m a retired professor (biology,
University Alaska Fairbanks, now living in Washington state - USA) 
– still doing some remnants of academia, volunteer work and slithering
into second adult life in writing. I’ve been looking for a replacement
PIM for Outlook. I downloaded quite a number. Most were prosaic. I gave
both Dragonfly and Life Balance a week’s test flight and have settled on
Dragonfly. You’ve built a nice machine/human connection. I especially like
the way that tasks, tasks derived from projects, reminders, and meetings
can be viewed simultaneously yet remain separate – so that things that
have to occur on a particular day can be kept separate from things that
one hopes to get accomplished on that day.

 

I have two (hopefully brief) questions. If the answers are to be found
in the documentation, my apologies – must not have looked long or hard
enough. (1) Is it possible to edit the title of a project (one of mine
has irritating typo)? I can’t seem to find window or tool to edit out
this mistake. (2) Similarly, is it possible to delete bogusoid entries
in “completed activities?” I have some telephone calls made entries
that are confusing (didn’t really make them – not sure how they arose).
I’d like to know how to correct entries in completed activities so that
the record is accurate.

 

Finally, I have a (probably common) suggestion for a feature. It would
be useful to be able to mark tasks as “pending” or “waiting for” rather
than complete. It is sometimes nice to get something off the to-do list
yet not lose track of the task. It would also be good to be able to
attach a date for a “follow-up” to a pending/waiting for task. 

 

Again, thanks for a nice program.

*/


/* From Craig Moore <craigfm@bigpond.net.au>
I will try and detail the requests in dot points.

 

Can the menu on the RHS be made to float down as the task bar or the scroll wheel on the mouse moves a long page (Address book) downwards? (So you always have access to the menu and you don’t have to scroll up to the top of the page to perform menu functions) alternatively make the top/bottom menu function a fixed window at the top of screen and the page scrolls under it. 
 

When adding a recurring reminder (eg: to pay council rates in 4 monthly instalments) can an option be provided to add recurring payment for the next “x” number of months? 
 

Can up/down arrows be provided on the side of the “year” box on the calendar to allow selection of for example 2004 or 2005 or any other year depending how many times the up or down arrow is clicked. 
 

A function to allow the colour a day appears on the calendar to be changed. (so for example “paydays” can be marked) 
 

An alarm function for “tasks” that is selectable for the number of days to alarm before the task needs to be undertaken. 
 

And for the reminder alarm the ability to be able to select the number of days before as well as alarming on the day. 
 

An edit function for reminders that have been programmed to occur (so the reminder can be edited if needed) similar to the “task” edit provisions. 
 

In the search function it would be great if the function searched numbers as well as text. (I would like to type in parts of phone numbers in the “search box” to see who phone calls have been made to when checking the monthly acc).  Mobile and STD calls are the biggest problem as these are sometimes a claimable item and I need to know who the calls are being made to. I have a program that I use at the moment (Full Contact by Promsoft) but I wish to use Dragonfly for this task. 
 

*/

/*
Request by Sahil Sinh Gujarl

Dates - enter as string.

User form - Allow for different fields, or for hiding some fields.

Mass-data entry forms for lots of entries

URls and email addresses automatically parsed

Drag and drop local files into journal

Search - google desktop search extension

Customize UI with premade strings

Automatically query flight info based on what put in dragonfly

Request for automatically starting

Launch searches from google desktop within dragonfly
*/

