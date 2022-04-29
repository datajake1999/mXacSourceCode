/**********************************************************************
Search.cpp - Search functionality

begun 6/3/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


/* globals */
#define  NUMCATEGORIES     10
static DWORD      gdwDateFilter = 0;   //0=all,1=month,2=year,3=5 years,4=custom
static DFDATE     gdateOld = 0;        // oldest date acceptable
static DFDATE     gdateNew = 0;        // newest date acceptable
static BOOL       gabCat[NUMCATEGORIES] =
   {TRUE,TRUE,TRUE,TRUE,TRUE,TRUE,TRUE,TRUE,TRUE,TRUE}; // if they're on or not

static WCHAR gszAllDates[] = L"AllDates";
static WCHAR gszLastMonth[] = L"LastMonth";
static WCHAR gszLastYear[] = L"LastYear";
static WCHAR gszLast5Years[] = L"Last5Years";
static WCHAR gszCustomDates[] = L"CustomDates";
static WCHAR gszStartDate[] = L"StartDate";
static WCHAR gszEndDate[] = L"EndDate";

/***********************************************************************
GetSearchFilter - Given a page with the search on it, this gets
the search filter information and fills the globals.

inputs
   PCEscPage      pPage - page
*/
void GetSearchFilter (PCEscPage pPage)
{
   HANGFUNCIN;
   PCEscControl pControl;

   gdwDateFilter = 0;   // default

   pControl = pPage->ControlFind (gszAllDates);
   if (pControl && pControl->AttribGetBOOL(gszChecked))
      gdwDateFilter = 0;
   pControl = pPage->ControlFind (gszLastMonth);
   if (pControl && pControl->AttribGetBOOL(gszChecked))
      gdwDateFilter = 1;
   pControl = pPage->ControlFind (gszLastYear);
   if (pControl && pControl->AttribGetBOOL(gszChecked))
      gdwDateFilter = 2;
   pControl = pPage->ControlFind (gszLast5Years);
   if (pControl && pControl->AttribGetBOOL(gszChecked))
      gdwDateFilter = 3;
   pControl = pPage->ControlFind (gszCustomDates);
   if (pControl && pControl->AttribGetBOOL(gszChecked))
      gdwDateFilter = 4;

   // date dropdowns
   gdateOld = DateControlGet(pPage, gszStartDate);
   gdateNew = DateControlGet(pPage, gszEndDate);

   // categories
   WCHAR szTemp[16];
   DWORD i;
   for (i = 0; i < NUMCATEGORIES; i++) {
      swprintf (szTemp, L"cat:%d", i);
      gabCat[i] = TRUE;
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         gabCat[i] = pControl->AttribGetBOOL(gszChecked);
   }


   // done
}

/*************************************************************
SetSearchFilter - Fills in the controls of the page with the
   appropriate search filter settings.
*/
void SetSearchFilter (PCEscPage pPage)
{
   HANGFUNCIN;
   PCEscControl pControl;

   pControl = pPage->ControlFind (gszAllDates);
   if (pControl)
      pControl->AttribSetBOOL(gszChecked, gdwDateFilter == 0);
   pControl = pPage->ControlFind (gszLastMonth);
   if (pControl)
      pControl->AttribSetBOOL(gszChecked, gdwDateFilter == 1);
   pControl = pPage->ControlFind (gszLastYear);
   if (pControl)
      pControl->AttribSetBOOL(gszChecked, gdwDateFilter == 2);
   pControl = pPage->ControlFind (gszLast5Years);
   if (pControl)
      pControl->AttribSetBOOL(gszChecked, gdwDateFilter == 3);
   pControl = pPage->ControlFind (gszCustomDates);
   if (pControl)
      pControl->AttribSetBOOL(gszChecked, gdwDateFilter == 4);

   // date dropdowns
   DateControlSet(pPage, gszStartDate, gdateOld);
   DateControlSet(pPage, gszEndDate, gdateNew);

   // categories
   WCHAR szTemp[16];
   DWORD i;
   for (i = 0; i < NUMCATEGORIES; i++) {
      swprintf (szTemp, L"cat:%d", i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->AttribSetBOOL(gszChecked, gabCat[i]);
   }


   // done
}

/***********************************************************************
UpdateSearchList - If this is the search window then it fills the
search listbox with the search items

inputs
   PCEscPage      pPage
*/
void UpdateSearchList (PCEscPage pPage)
{
   HANGFUNCIN;
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

/**************************************************************************************8
SearchIndexCallback - Called when indexing internal documents
*/
BOOL __cdecl SearchIndexCallback (CEscSearch *pSearch, DWORD dwDocument, PVOID pUserData)
{
   HANGFUNCIN;
   // load the page
   PCMMLNode pNode;
   pNode = gpData->NodeGet(dwDocument);
   if (!pNode)
      return TRUE;   // skip

   // depending upon the string figure out if going to parse
   BOOL fParse;
   PWSTR pszName;
   PWSTR pszTitle;
   PWSTR pszSection;
   PWSTR pszLink;
   WCHAR szLinkTemp[16];
   WCHAR szSectionTemp[512];
   DWORD dwDate = 0;
   DWORD dwCategory = 0;
   fParse = FALSE;
   swprintf (szLinkTemp, L"v:%d", (int) dwDocument);
   pszLink = szLinkTemp;
   pszTitle = NULL;
   pszSection = NodeValueGet (pNode, gszName);
   if (!pszSection) {
      szSectionTemp[64];
      DFDATE date;
      date = (DFDATE) NodeValueGetInt (pNode, gszDate, 0);
      szSectionTemp[0] = 0;
      if (date)
         DFDATEToString (date, szSectionTemp);
      pszSection = szSectionTemp;
   }

   // get the date
   dwDate = (DWORD) NodeValueGetInt (pNode, gszDate, 0);

   pszName = pNode->NameGet();
   if (!pszName) {
      // do nothing
   }
   else if (!_wcsicmp(pszName, gszProjectNode)) {
      fParse = TRUE;
      pszTitle = L"Project";
      dwCategory = 1;   // project category
   }
   else if (!_wcsicmp(pszName, gszPersonNode)) {
      fParse = TRUE;
      pszTitle = L"Person";

      PWSTR pszFirst, pszLast, pszNick;
      pszFirst = NodeValueGet (pNode, gszFirstName);
      pszLast = NodeValueGet (pNode, gszLastName);
      pszNick = NodeValueGet (pNode, gszNickName);
      szSectionTemp[0] = 0;

      PeopleFullName (pszLast ? pszLast : L"", pszFirst ? pszFirst : L"",
         pszNick ? pszNick : L"", szSectionTemp, sizeof(szSectionTemp));
      pszSection = szSectionTemp;

      dwCategory = 2;   // people category
   }
   else if (!_wcsicmp(pszName, gszBusinessNode)) {
      fParse = TRUE;
      pszTitle = L"Business";

      PWSTR pszLast;
      pszLast = NodeValueGet (pNode, gszLastName);
      szSectionTemp[0] = 0;
      if (pszLast)
         wcscpy (szSectionTemp, pszLast);
      pszSection = szSectionTemp;

      dwCategory = 2;   // people category
   }
   else if (!_wcsicmp(pszName, gszMeetingNotesNode)) {
      fParse = TRUE;
      pszTitle = L"Meeting notes";

      dwCategory = 3;   // meeting category
   }
   else if (!_wcsicmp(pszName, gszPhoneNotesNode)) {
      fParse = TRUE;
      pszTitle = L"Phone conversation notes";

      dwCategory = 4;   // phone calls
   }
   else if (!_wcsicmp(pszName, gszCalendarLogDay)) {
      fParse = TRUE;
      pszTitle = L"Daily journal";

      dwCategory = 5;   // misc
   }
   else if (!_wcsicmp(pszName, gszJournalCategoryNode)) {
      fParse = TRUE;
      pszTitle = L"Journal category";

      dwCategory = 6;   // journal
   }
   else if (!_wcsicmp(pszName, gszJournalEntryNode)) {
      fParse = TRUE;
      pszTitle = L"Journal entry";

      dwCategory = 6;   // journal
   }
   else if (!_wcsicmp(pszName, gszMemoryEntryNode)) {
      fParse = TRUE;
      pszTitle = L"Memory";

      dwCategory = 7;   // memory
   }
   else if (!_wcsicmp(pszName, gszMemoryListNode)) {
      fParse = TRUE;
      pszTitle = L"Memory list";

      dwCategory = 7;   // memory
   }
   else if (!_wcsicmp(pszName, gszWrapUpNode)) {
      fParse = TRUE;
      pszTitle = L"Daily wrap-up";

      dwCategory = 5;   // misc
   }
   else if (!_wcsicmp(pszName, gszArchiveEntryNode)) {
      fParse = TRUE;
      pszTitle = L"Archived document";

      dwCategory = 8;   // archived documents
   }
   else if (!_wcsicmp(pszName, gszProjectListNode)) {
      fParse = TRUE;
      pszTitle = L"Projects";
      pszSection = L"";
      pszLink = L"r:145";

      dwCategory = 1;   // projects
   }
   else if (!_wcsicmp(pszName, gszReminderListNode)) {
      fParse = TRUE;
      pszTitle = L"Reminders";
      pszSection = L"";
      pszLink = L"r:114";

      dwCategory = 5;   // misc
   }
   else if (!_wcsicmp(pszName, gszWorkListNode)) {
      fParse = TRUE;
      pszTitle = L"Tasks";
      pszSection = L"";
      pszLink = L"r:115";

      dwCategory = 5;   // misc
   }
   else if (!_wcsicmp(pszName, gszEventListNode)) {
      fParse = TRUE;
      pszTitle = L"Special days";
      pszSection = L"";
      pszLink = L"r:280";

      dwCategory = 5;   // misc
   }
   else if (!_wcsicmp(pszName, gszSchedListNode)) {
      fParse = TRUE;
      pszTitle = L"Meetings";
      pszSection = L"";
      pszLink = L"r:113";

      dwCategory = 3;   // meeting category
   }
   else if (!_wcsicmp(pszName, gszNoteListNode)) {
      fParse = TRUE;
      pszTitle = L"Notes";
      pszSection = L"";
      pszLink = L"r:116";

      dwCategory = 5;   // misc
   }
   else if (!_wcsicmp(pszName, gszPhotosEntryNode)) {
      fParse = TRUE;
      pszTitle = L"Photo";

      dwCategory = 9;   // photo
   }

   // if not indexing exit
   if (!fParse) {
#ifdef _DEBUG
      OutputDebugString ("Didnt index ");
      char szTemp[512];
      szTemp[0] = 0;
      WideCharToMultiByte (CP_ACP, 0, pszName, -1, szTemp, sizeof(szTemp), 0, 0);
      OutputDebugString (szTemp);
      OutputDebugString ("\r\n");
#endif
      gpData->NodeRelease(pNode);
      return TRUE;
   }

   // index
   pSearch->IndexNode (pNode, NULL, NULL);

   // if can finda  name add that as more important
   // PWSTR pszName;
   pszName = NodeValueGet (pNode, gszName);
   if (pszName) {
      pSearch->m_bCurRelevence = 128;
      pSearch->IndexText (pszName);
   }

   // add section
   pSearch->SectionFlush (pszTitle, pszSection, pszLink, dwDate, dwCategory);

   gpData->NodeRelease(pNode);
   return TRUE;
}

/*****************************************************************************
SearchIndex - Redoes the search index.

inputs
   PCEscPage pPage - page
*/
void SearchIndex (PCEscPage pPage)
{
   HANGFUNCIN;
   DWORD adwDontIndex[] = {IDR_MMLMACROS, IDR_MMLTEMPLATE, IDR_MMLTEMPLATER, IDR_MMLTEMPLATE2,
      IDR_MMLTEMPLATE3, IDR_MMLREOCCUR, IDR_MMLPERSONADDEDITINCLUDE, IDR_MMLNYI};

   ESCINDEX i;
   memset (&i, 0, sizeof(i));
   i.hWndUI = pPage->m_pWindow->m_hWnd;
   i.pdwMMLExclude = adwDontIndex;
   i.dwMMLExcludeCount = sizeof(adwDontIndex) / sizeof(DWORD);
   i.pIndexCallback = SearchIndexCallback;
   i.dwIndexDocuments = gpData->Num();
   gSearch.Index (&i);
}

/*****************************************************************************
SearchPage - Search page callback. It handles standard operations
*/
BOOL SearchPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
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

         // set the filter
         SetSearchFilter (pPage);

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

         // BUGFIX - for all links, even if don't trap, store away filter options
         GetSearchFilter (pPage);

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

            // get the search info
            GetSearchFilter (pPage);
            ESCADVANCEDSEARCH as;
            memset (&as, 0, sizeof(as));
            switch (gdwDateFilter) {
            default: // and 0, all
               // don't need to set anything
               break;
            case 1:  // last month
               as.dwMostRecent = Today();
               as.dwOldest = MinutesToDFDATE(DFDATEToMinutes(Today()) - 31*24*60);
               as.fRamp = TRUE;
               break;
            case 2:  // last year
               as.dwMostRecent = Today();
               as.dwOldest = MinutesToDFDATE(DFDATEToMinutes(as.dwMostRecent) - 365*24*60);
               as.fRamp = TRUE;
               break;
            case 3:  // last 5 years
               as.dwMostRecent = Today();
               as.dwOldest = MinutesToDFDATE(DFDATEToMinutes(as.dwMostRecent) - 5*365*24*60);
               as.fRamp = TRUE;
               break;
            case 4:  // custom dates
               as.dwMostRecent = gdateNew;
               as.dwOldest = gdateOld;
               break;
            }
            as.dwUseCategoryCount = NUMCATEGORIES;
            as.pafUseCategory = gabCat;

            // search
            gSearch.Search (szTemp, &as);

            // update the list
            UpdateSearchList(pPage);

            // inform user that search complete
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Search finished.");

            return TRUE;
         }
      }
      break;   // so default search behavior happens

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"rebuild")) {
            SearchIndex (pPage);
            return TRUE;
         }
      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}

// BUGBUG - 2.0 - advanced search (with filter for data types and dates) was
// removed from first release. Put back in eventually

