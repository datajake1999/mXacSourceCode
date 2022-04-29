/**********************************************************************************
Help.cpp - For help routines

begun 5/2/2002 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\buildnum.h"
#include "..\M3D.h"
#include "resource.h"


typedef struct {
   WCHAR    szLink[256];   // link name. controls all
   BOOL     fResolved;     // set to TRUE if resolved, FALSE if cant
   int      iVScroll;      // vertical scroll to use. -1 if use szLink
   int      iSection;      // index into szLink for character just after #. -1 if none
   DWORD    dwResource;    // MML resource using for this.
   DWORD    dwData;        // Database number using for this. -1 if none
} HISTORY, *PHISTORY;


CEscSearch  gSearch;             // search
CListFixed  gListHistory;        // history list
HISTORY     gCurHistory;         // current location
PCEscWindow gpHelpWindow = NULL;            // main window object
static DWORD gdwHelpTimer = 0;   // timer notification so can use help while dialog is up
static PWSTR gszPageErrorID = L"r:236";
static PWSTR gszStartingURL = L"r:236";
BOOL        gfSmallFont = FALSE;


/*****************************************************************************
HelpTimerFunc - Timer function used to make sure that if user clicks on button
for page-link then actually goes there, if if a dialog is running.
*/
void CALLBACK HelpTimerFunc (HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
   ASPHelpCheckPage();
}

/*****************************************************************************
HelpDefPage - Default page callback. It handles standard operations
*/
BOOL HelpDefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static BOOL fFirstTime = TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      pPage->m_pWindow->IconSet (LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_APPICON)));

      // if we have somplace to scroll to then do so
      if (gCurHistory.iVScroll >= 0) {
         pPage->VScroll (gCurHistory.iVScroll);

         // when bring up pop-up dialog often they're scrolled wrong because
         // iVScoll was left as valeu, and they use defpage
         gCurHistory.iVScroll = 0;

         // BUGFIX - putting this invalidate in to hopefully fix a refresh
         // problem when add or move a task in the ProjectView page
         pPage->Invalidate();
      }
      else {
         // if the current page string has a "#" in it then skip to that
         // section
         if (gCurHistory.iSection >= 0)
            pPage->VScrollToSection (gCurHistory.szLink + gCurHistory.iSection);
      }

      }
      return TRUE;


   };


   return FALSE;
}


/***********************************************************************
UpdateSearchList - If this is the search window then it fills the
search listbox with the search items

inputs
   PCEscPage      pPage
*/
void UpdateSearchList (PCEscPage pPage)
{
   PCEscControl pc;
   pc = pPage->ControlFind(L"searchlist");
   if (!pc)
      return;  // not the search window

   // clear out the current elemnts
   pc->Message (ESCM_LISTBOXRESETCONTENT);

   // add them in
   WCHAR szHuge[1024];
   WCHAR szTitle[512], szSection[512], szLink[512];
   size_t dwNeeded;

   DWORD i;
   PWSTR psz;
   for (i = 0; i < min(50,gSearch.m_listFound.Num()); i++) {
      psz = (PWSTR) gSearch.m_listFound.Get(i);
      if (!psz)
         continue;

      // the search info is packed, so unpack and convert so
      // none of the characters interfere with MML

      // scorew
      DWORD dwScore;
      dwScore = *((DWORD*) psz);
      psz += (sizeof(DWORD)/sizeof(WCHAR));

      // document title
      szTitle[0] = 0;
      StringToMMLString (psz, szTitle, sizeof(szTitle), &dwNeeded);
      psz += (wcslen(psz)+1);

      // section title
      szSection[0] = 0;
      StringToMMLString (psz, szSection, sizeof(szSection), &dwNeeded);
      psz += (wcslen(psz)+1);

      // link
      szLink[0] = 0;
      StringToMMLString (psz, szLink, sizeof(szLink), &dwNeeded);

      // combine this into 1 large mml string
      swprintf (szHuge,
         L"<elem name=\"%s\">"
            L"<br><bold>Document: %s</bold></br>"
            L"<br>&tab;Section: %s</br>"
            L"<br>&tab;Score: %g</br>"
         L"</elem>",
         szLink,
         szTitle[0] ? szTitle : L"Unknown",
         szSection[0] ? szSection : L"None",
         (double) (dwScore/100) / 10
         );

      // add to the list
      ESCMLISTBOXADD a;
      memset (&a, 0, sizeof(a));
      a.dwInsertBefore = (DWORD)-1;
      a.pszMML = szHuge;
      pc->Message (ESCM_LISTBOXADD, &a);
   }

   // done
}

/*****************************************************************************
SearchIndex - Redoes the search index.

inputs
   PCEscPage pPage - page
*/
void SearchIndex (PCEscPage pPage)
{
   DWORD adwDontIndex[] = {IDR_MMLMACROS, IDR_MMLTEMPLATEBACK, IDR_MMLTEMPLATEHELP};

   ESCINDEX i;
   memset (&i, 0, sizeof(i));
   i.hWndUI = pPage->m_pWindow->m_hWnd;
   i.pdwMMLExclude = adwDontIndex;
   i.dwMMLExcludeCount = sizeof(adwDontIndex) / sizeof(DWORD);
   gSearch.Index (&i);
}

/*****************************************************************************
SearchPage - Search page callback. It handles standard operations
*/
BOOL SearchPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // if this is the seach window then set the search text and fill
         // in the list box
         PCEscControl   pc;
         pc = pPage->ControlFind (L"SearchString");
         if (pc && gSearch.m_pszLastSearch)
            pc->AttribSet (L"text", gSearch.m_pszLastSearch);
         UpdateSearchList(pPage);

      }
      break;   // so default init happens


      case ESCN_LISTBOXSELCHANGE:
         {
            PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;

            // only care about the search list control
            if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"searchlist"))
               return TRUE;

            // if no name then ignore
            if (!p->pszName)
               return TRUE;

            // use the name as a link
            pPage->Link(p->pszName);
         }
         return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!_wcsicmp(p->psz, L"searchbutton")) {
            // get the text to search for
            WCHAR szTemp[512];
            DWORD dwNeeded;
            szTemp[0] = 0;
            PCEscControl   pc;
            pc = pPage->ControlFind (L"SearchString");
            if (pc)
               pc->AttribGet (L"text", szTemp, sizeof(szTemp), &dwNeeded);

            // index the search if it need reindexing because of a new app version
            if (gSearch.NeedIndexing())
               SearchIndex (pPage);

            // search
            gSearch.Search (szTemp);

            // update the list
            UpdateSearchList(pPage);

            // inform user that search complete
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Search finished.");

            return TRUE;
         }
      }
      break;   // so default search behavior happens


   };


   return HelpDefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
AboutPage - Search page callback. It handles standard operations
*/
BOOL AboutPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"DFVERSION")) {
            static WCHAR szVersion[16];   // for about box
            MultiByteToWideChar (CP_ACP, 0, BUILD_NUMBER, -1, szVersion, sizeof(szVersion)/2);
            p->pszSubString = szVersion;
            return TRUE;
         }
      }
      break;   // default behavior

   };


   return HelpDefPage (pPage, dwMessage, pParam);
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
   memset (pHistory, 0, sizeof(*pHistory));

   if (!pszURL) {
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
      pHistory->iSection = (int)((size_t) (PBYTE) psz - (size_t) (PBYTE) pszURL) / 2 + 1;
   }

   // figure out what resource
   psz = pszURL;
   if (!_wcsnicmp (psz, L"r:", 2)) {
      // it's another page
      DWORD dwPage;
      dwPage = _wtoi (psz + 2);

      pHistory->dwResource = dwPage;
   }
   else if (!_wcsicmp (psz, Back())) {
      PHISTORY pOld;
      pOld = (PHISTORY) gListHistory.Get (0);
      if (!pOld) {
         // cant go back, so go to starting url
         ParseURL (gszStartingURL, pHistory);
         return;
      }


      // else, use that
      *pHistory = *pOld;

      // must do some reparsing if v:
      psz = pHistory->szLink;
      // delete the back
      gListHistory.Remove (0);

      return;
   }
   else {
      // quit
      pHistory->fResolved = FALSE;
      return;
   }


   // assume data is -1
   pHistory->dwData = (DWORD)-1;
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

   switch (dwID) {
   case IDR_MMLREGISTER:
      return RegisterPage;
   case IDR_HSEARCH:
      return SearchPage;
   case IDR_HABOUT:
      return AboutPage;

   default:
      return HelpDefPage;
   }
}


/**************************************************************************************
ShowPage - Given a string, shows that page. NOTE: Only call this if know that no
page is currently running.

inputs
   PWSTR       psz - String
*/
static void ShowPage (PWSTR psz)
{
   // create the help window if not there
   if (!gpHelpWindow) {
      gpHelpWindow = new CEscWindow;
      if (!gpHelpWindow)
         return;

      // figure out where should appear
      RECT r;
      int iX, iY;
      iX = GetSystemMetrics (SM_CXSCREEN);
      iY = GetSystemMetrics (SM_CYSCREEN);
      r.left = iX / 6;
      r.top = iY / 6;
      r.right = r.left + iX / 3;
      r.bottom = r.top + iY / 3 * 2;

      if (!gpHelpWindow->Init (ghInstance, NULL, 0, &r)) {
         delete gpHelpWindow;
         gpHelpWindow =NULL;
         return;
      }

      // topmost
      SetWindowPos (gpHelpWindow->m_hWnd, (HWND)HWND_TOPMOST, 0, 0,
         0, 0, SWP_NOMOVE | SWP_NOSIZE);

      // create the timer
      if (!gdwHelpTimer) {
         gdwHelpTimer = SetTimer (NULL, NULL, 250, HelpTimerFunc);
      }
   }

   // BUGFIX - If error in loading go to page error
   if (!psz) {
      psz = gszPageErrorID;
   }

   // as long as we're not back, add the old one to current hisotry
   if (!_wcsicmp (psz, RedoSamePage())) {
      // page requested to redo itself
      gCurHistory.iVScroll = gpHelpWindow->m_iExitVScroll;
      goto showit;
   }
   else if (_wcsicmp(psz, Back()) && gCurHistory.szLink[0]) {
      gCurHistory.iVScroll = gpHelpWindow->m_iExitVScroll;
      gListHistory.Insert (0, &gCurHistory);
   }

   // now partse
   ParseURL (psz, &gCurHistory);

showit:
   if (!gCurHistory.fResolved) {
      // delete the window, unknown, so we're done
      delete gpHelpWindow;
      gpHelpWindow = NULL;
      if (gdwHelpTimer) {
         KillTimer (NULL, gdwHelpTimer);
         gdwHelpTimer = 0;
      }
      return;
   }

   EscFontScaleSet (gfSmallFont ? 0xc0 : 0x100);
   if (!gpHelpWindow->PageDisplay (ghInstance, gCurHistory.dwResource, DetermineCallback(gCurHistory.dwResource), NULL)) {
      // delete the window, unknown, so we're done
      delete gpHelpWindow;
      gpHelpWindow = NULL;
      if (gdwHelpTimer) {
         KillTimer (NULL, gdwHelpTimer);
         gdwHelpTimer = 0;
      }
      return;
   }

   // done
}


/**************************************************************************************
ASPHelpCheckPage - Called in the main message loop after every message and checks
to see that page links are parsed correctly.
*/
void ASPHelpCheckPage (void)
{
   if (!gpHelpWindow)
      return;

   if (!gpHelpWindow->m_pszExitCode)
      return;

   // else, and exit code
   PWSTR psz = gpHelpWindow->m_pszExitCode;

   // get rid of the page if its still there
   gpHelpWindow->PageClose ();

   ShowPage (psz);
}



/*****************************************************************************
ASPHelp - Does the main loop for help

inputs
   DWORD    dwRes - Resource ID to pull up
returns
   none
*/
void ASPHelp (DWORD dwRes)
{
   WCHAR szStart[32];
   swprintf (szStart, L"r:%d", dwRes);

   // show window if not visible
   if (gpHelpWindow && IsIconic(gpHelpWindow->m_hWnd))
      ShowWindow (gpHelpWindow->m_hWnd, SW_RESTORE);

   if (gpHelpWindow && gpHelpWindow->m_pPage) {
      gpHelpWindow->m_pPage->Exit (szStart);
   }
   else {
      gListHistory.Init (sizeof(HISTORY));
      memset (&gCurHistory, 0, sizeof(gCurHistory));

      ShowPage (szStart);
   }
}

/********************************************************************************
ASPInit - Sets up the globals for search
*/
void ASPHelpInit (void)
{
   // init the search
   WCHAR szFile[256];
   MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szFile, sizeof(szFile)/2);
   wcscat (szFile, APPSHORTNAMEW L".xsr");
   // initialize the search, use a hash of the date for the version
   char szDate[] = __DATE__;
   DWORD dwSum, i;
   dwSum = 1;
   for (i = 0; i < strlen(szDate); i++)
      dwSum += (i * 0x100 * szDate[i] + szDate[i]);
   gSearch.Init (ghInstance, dwSum, szFile);

}


/*****************************************************************************************
ASPHelpEnd - Shuts down help if it's still around.
*/
void ASPHelpEnd (void)
{
   if (gpHelpWindow) {
      gpHelpWindow->PageClose();
      delete gpHelpWindow;
   }
   gpHelpWindow = NULL;
   if (gdwHelpTimer) {
      KillTimer (NULL, gdwHelpTimer);
      gdwHelpTimer = 0;
   }
}


// FUTURERELEASE - Print functionality in help

// BUGBUG - Document that if put eyes in head, and then move head, need to
// make sure eyes are linked to head bone as rigid. That way CP for what focusing
// on will work properly

